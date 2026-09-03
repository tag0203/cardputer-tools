#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <time.h>
#include "character_image.h"
#include "wifi_presets.h"

// Focus Wallet for M5Cardputer
// ------------------------------------------------------------
// Keyboard
//   SPACE : start the next session / pause / resume
//   X     : cancel the current timer
//   F/B/U : focus / break / use free time
//   S/D   : settings / daily history
//   ADV arrow positions (; , . /) : navigate without holding Fn
//   H     : home
//
// The UI intentionally uses an isolated drawCompanion() function. Replace that
// function with pushImage()/drawPng() later to add character artwork without
// touching any of the timer and reward rules.

namespace {

constexpr uint32_t DATA_MAGIC = 0x46574C54;  // "FWLT"
constexpr uint16_t DATA_VERSION = 2;
constexpr uint32_t SAVE_INTERVAL_MS = 10000;
constexpr uint16_t BG = 0x1082;
constexpr uint16_t PANEL = 0x2124;
constexpr uint16_t TEXT = 0xFFFF;
constexpr uint16_t MUTED = 0x9CF3;
constexpr uint16_t FOCUS = 0xFD20;
constexpr uint16_t BREAK_COLOR = 0x4E79;
constexpr uint16_t FREE_COLOR = 0x5D7F;
constexpr uint16_t DANGER = 0xF986;
constexpr uint16_t WIFI_COLOR = 0x4D9F;
constexpr uint16_t CLOCK_COLOR = 0x867D;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t DISPLAY_DIM_TIMEOUT_MS = 30UL * 1000UL;
constexpr uint32_t DISPLAY_SLEEP_TIMEOUT_MS = 2UL * 60UL * 1000UL;
constexpr uint32_t LIGHT_SLEEP_IDLE_WAKE_INTERVAL_MS = 30UL * 1000UL;
constexpr gpio_num_t ADV_KEYBOARD_INTERRUPT_PIN = GPIO_NUM_11;
constexpr uint8_t ADV_AUDIO_CODEC_ADDRESS = 0x18;
constexpr uint32_t ADV_AUDIO_CODEC_I2C_FREQUENCY = 100000;
constexpr uint32_t ADV_AUDIO_MUTE_SETTLE_MS = 10;
constexpr uint32_t SPEAKER_COLD_START_SETTLE_MS = 30;
constexpr uint8_t DISPLAY_BRIGHTNESS_NORMAL = 120;
constexpr uint8_t DISPLAY_BRIGHTNESS_DIM = 20;

enum class Screen : uint8_t {
  LOCK_SCREEN, LAUNCHER, POMODORO, SETTINGS, HISTORY, RESULT, WIFI_SETTINGS
};
enum class TimerMode : uint8_t { NONE, FOCUS, SHORT_BREAK, LONG_BREAK, FREE_TIME };
enum class TimerStatus : uint8_t { IDLE, RUNNING, PAUSED };
enum class ResultType : uint8_t { NONE, FOCUS_DONE, REWARD_EARNED, BREAK_DONE, FREE_DONE };
enum class WifiView : uint8_t { STATUS, SCANNING, NETWORKS, PASSWORD, CONNECTING };
enum class DisplayPowerState : uint8_t { ACTIVE, DIMMED, SLEEPING };

struct Settings {
  uint16_t focusMinutes = 25;
  uint16_t shortBreakMinutes = 5;
  uint16_t longBreakMinutes = 15;
  uint8_t requiredCount = 4;
  uint16_t rewardMinutes = 60;
  uint16_t maximumFreeMinutes = 180;
  uint16_t dailyRewardLimitMinutes = 0;  // 0 = unlimited
  bool carryOver = false;
  bool autoBreak = false;
  bool autoFocus = false;
  bool sound = true;
};

struct DayStats {
  int32_t dateKey = 0;  // YYYYMMDD. 0 means RTC is unavailable.
  uint16_t completed = 0;
  uint32_t focusSeconds = 0;
  uint32_t earnedSeconds = 0;
  uint32_t usedSeconds = 0;
};

struct PersistentData {
  uint32_t magic = DATA_MAGIC;
  uint16_t version = DATA_VERSION;
  uint16_t size = sizeof(PersistentData);
  Settings settings;
  uint8_t cycleCount = 0;
  uint32_t freeBalanceSeconds = 0;
  DayStats today;
  DayStats history[7];
  TimerMode timerMode = TimerMode::NONE;
  TimerStatus timerStatus = TimerStatus::IDLE;
  uint32_t remainingSeconds = 0;
  uint32_t plannedSeconds = 0;
  int64_t savedEpoch = 0;
};

Preferences preferences;
M5Canvas canvas(&M5Cardputer.Display);
PersistentData data;
Screen screen = Screen::LOCK_SCREEN;
ResultType resultType = ResultType::NONE;
uint32_t resultRewardSeconds = 0;
uint32_t lastTickMs = 0;
uint32_t lastSaveMs = 0;
uint32_t lastDrawSecond = UINT32_MAX;
uint32_t lastBatteryPollMs = 0;
int32_t batteryLevel = -1;
bool dirty = true;
bool rtcAvailable = false;
bool restoredWithoutClock = false;
bool ntpConfigured = false;
volatile bool ntpSyncReceived = false;
bool timerRestorePending = false;
uint32_t timerRestoreDeadlineMs = 0;
bool wifiAlwaysOn = false;
uint32_t nextWifiAttemptMs = 0;
uint32_t lastUserInputMs = 0;
DisplayPowerState displayPowerState = DisplayPowerState::ACTIVE;
uint8_t settingsIndex = 0;
uint8_t launcherIndex = 0;
WifiView wifiView = WifiView::STATUS;
String wifiSsid;
String wifiPassword;
String selectedSsid;
String passwordInput;
String wifiMessage = "Not connected";
int16_t wifiScanCount = 0;
int16_t wifiNetworkIndex = 0;
uint32_t wifiConnectStartedMs = 0;
uint32_t lastClockEpoch = 0;

void restoreRunningTimer();
void wakeDisplayForTimerCompletion();

const char* modeName(TimerMode mode) {
  switch (mode) {
    case TimerMode::FOCUS: return "FOCUS";
    case TimerMode::SHORT_BREAK: return "SHORT BREAK";
    case TimerMode::LONG_BREAK: return "LONG BREAK";
    case TimerMode::FREE_TIME: return "FREE TIME";
    default: return "READY";
  }
}

uint16_t modeColor(TimerMode mode) {
  switch (mode) {
    case TimerMode::FOCUS: return FOCUS;
    case TimerMode::SHORT_BREAK:
    case TimerMode::LONG_BREAK: return BREAK_COLOR;
    case TimerMode::FREE_TIME: return FREE_COLOR;
    default: return TEXT;
  }
}

void saveData() {
  data.magic = DATA_MAGIC;
  data.version = DATA_VERSION;
  data.size = sizeof(PersistentData);
  preferences.putBytes("state", &data, sizeof(data));
  lastSaveMs = millis();
}

bool loadData() {
  if (preferences.getBytesLength("state") != sizeof(PersistentData)) return false;
  PersistentData loaded;
  preferences.getBytes("state", &loaded, sizeof(loaded));
  if (loaded.magic != DATA_MAGIC || loaded.version != DATA_VERSION ||
      loaded.size != sizeof(PersistentData)) return false;
  data = loaded;
  return true;
}

bool systemTimeValid() {
  return time(nullptr) >= 1704067200;  // 2024-01-01
}

bool getJstTime(tm& value) {
  if (systemTimeValid()) {
    time_t now = time(nullptr);
    localtime_r(&now, &value);
    return true;
  }
  if (M5.Rtc.isEnabled()) {
    auto dt = M5.Rtc.getDateTime();
    if (dt.date.year >= 2024 && dt.date.year <= 2099) {
      value = dt.get_tm();
      return true;
    }
  }
  memset(&value, 0, sizeof(value));
  return false;
}

// Returns local RTC time as an epoch-like value. It is used only for elapsed
// duration comparisons on the same device, so UTC/local timezone is irrelevant.
int64_t rtcEpoch() {
  if (systemTimeValid()) return static_cast<int64_t>(time(nullptr));
  if (!M5.Rtc.isEnabled()) return 0;
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year < 2024 || dt.date.year > 2099) return 0;
  tm value = dt.get_tm();
  value.tm_isdst = -1;
  time_t result = mktime(&value);
  return result > 0 ? static_cast<int64_t>(result) : 0;
}

int32_t currentDateKey() {
  tm now;
  if (!getJstTime(now)) return 0;
  return (now.tm_year + 1900) * 10000L + (now.tm_mon + 1) * 100L + now.tm_mday;
}

void saveWifiCredentials() {
  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_pass", wifiPassword);
}

bool findWifiPreset(const String& ssid, String& password) {
  for (size_t i = 0; i < WIFI_PRESET_COUNT; ++i) {
    const char* presetSsid = WIFI_PRESETS[i].ssid;
    if (presetSsid == nullptr || presetSsid[0] == '\0') continue;
    if (ssid == presetSsid) {
      password = WIFI_PRESETS[i].password == nullptr ? "" : WIFI_PRESETS[i].password;
      return true;
    }
  }
  return false;
}

bool loadFirstWifiPreset(String& ssid, String& password) {
  for (size_t i = 0; i < WIFI_PRESET_COUNT; ++i) {
    const char* presetSsid = WIFI_PRESETS[i].ssid;
    if (presetSsid == nullptr || presetSsid[0] == '\0') continue;
    ssid = presetSsid;
    password = WIFI_PRESETS[i].password == nullptr ? "" : WIFI_PRESETS[i].password;
    return true;
  }
  return false;
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void onNtpTimeAvailable(struct timeval*) {
  ntpSyncReceived = true;
}

void stopWifiRadio(const String& message) {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true, false);
  esp_sntp_stop();
  ntpConfigured = false;
  if (wifiView == WifiView::CONNECTING) wifiView = WifiView::STATUS;
  wifiMessage = message;
  dirty = true;
}

void configureNtp() {
  if (ntpConfigured) return;
  // POSIX TZ notation has an inverted sign: JST-9 means UTC+9.
  sntp_set_time_sync_notification_cb(onNtpTimeAvailable);
  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp", "pool.ntp.org");
  ntpConfigured = true;
  wifiMessage = "Connected - syncing JST";
  dirty = true;
}

void beginWifiConnection(const String& ssid, const String& password,
                         bool syncOnly = true) {
  if (ssid.isEmpty()) {
    wifiMessage = "No saved network";
    wifiView = WifiView::STATUS;
    dirty = true;
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(!syncOnly);
  WiFi.disconnect();
  delay(20);
  WiFi.begin(ssid.c_str(), password.c_str());
  wifiConnectStartedMs = millis();
  nextWifiAttemptMs = wifiConnectStartedMs + WIFI_RETRY_INTERVAL_MS;
  wifiView = WifiView::CONNECTING;
  wifiMessage = String("Connecting to ") + ssid;
  ntpConfigured = false;
  ntpSyncReceived = false;
  dirty = true;
}

void startWifiScan() {
  esp_sntp_stop();
  ntpConfigured = false;
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
  wifiScanCount = 0;
  wifiNetworkIndex = 0;
  wifiView = WifiView::SCANNING;
  wifiMessage = "Scanning nearby networks";
  dirty = true;
}

void updateWifiAndTime() {
  uint32_t nowMs = millis();

  if (wifiView == WifiView::SCANNING) {
    int16_t result = WiFi.scanComplete();
    if (result >= 0) {
      wifiScanCount = result;
      wifiNetworkIndex = 0;
      wifiView = result > 0 ? WifiView::NETWORKS : WifiView::STATUS;
      wifiMessage = result > 0 ? String(result) + " networks found" : "No networks found";
      if (result == 0 && !wifiAlwaysOn) stopWifiRadio("No networks - Wi-Fi off");
      dirty = true;
    } else if (result == WIFI_SCAN_FAILED) {
      wifiView = WifiView::STATUS;
      wifiMessage = "Wi-Fi scan failed";
      if (!wifiAlwaysOn) stopWifiRadio("Scan failed - Wi-Fi off");
      dirty = true;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    configureNtp();
    if (wifiView == WifiView::CONNECTING) {
      wifiView = WifiView::STATUS;
      wifiMessage = "Wi-Fi connected";
      dirty = true;
    }
  } else if (wifiView == WifiView::CONNECTING &&
             nowMs - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    nextWifiAttemptMs = nowMs + WIFI_RETRY_INTERVAL_MS;
    if (wifiAlwaysOn) {
      WiFi.disconnect();
      wifiView = WifiView::STATUS;
      wifiMessage = "Connection timed out - retry later";
      dirty = true;
    } else {
      stopWifiRadio("Sync failed - retry in 5 min");
    }
  }

  if (ntpSyncReceived) {
    ntpSyncReceived = false;
    tm now;
    if (getJstTime(now) && M5.Rtc.isEnabled()) {
      M5.Rtc.setDateTime(&now);
      rtcAvailable = true;
    }
    nextWifiAttemptMs = nowMs + (wifiAlwaysOn ? WIFI_RETRY_INTERVAL_MS
                                              : WIFI_SYNC_INTERVAL_MS);
    wifiView = WifiView::STATUS;
    if (wifiAlwaysOn) {
      WiFi.setAutoReconnect(true);
      wifiMessage = "JST synced - always connected";
      dirty = true;
    } else {
      stopWifiRadio("JST synced - Wi-Fi off");
    }
  }

  if (wifiView == WifiView::STATUS && WiFi.status() != WL_CONNECTED &&
      !wifiSsid.isEmpty() && deadlineReached(nowMs, nextWifiAttemptMs)) {
    beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
  }

  if (timerRestorePending &&
      (systemTimeValid() || nowMs >= timerRestoreDeadlineMs)) {
    timerRestorePending = false;
    restoreRunningTimer();
  }

  if (screen == Screen::LOCK_SCREEN || screen == Screen::LAUNCHER) {
    uint32_t now = systemTimeValid() ? static_cast<uint32_t>(time(nullptr)) : millis() / 1000UL;
    if (now != lastClockEpoch) {
      lastClockEpoch = now;
      dirty = true;
    }
  }
}

void pushHistory(const DayStats& day) {
  if (day.completed == 0 && day.earnedSeconds == 0 && day.usedSeconds == 0) return;
  for (int i = 6; i > 0; --i) data.history[i] = data.history[i - 1];
  data.history[0] = day;
}

void checkDayChange() {
  int32_t key = currentDateKey();
  if (key == 0) return;
  if (data.today.dateKey == 0) {
    data.today.dateKey = key;
    saveData();
    return;
  }
  if (key == data.today.dateKey) return;

  pushHistory(data.today);
  data.today = DayStats();
  data.today.dateKey = key;
  data.cycleCount = 0;
  // A running free-time session is allowed to finish across midnight.
  if (!data.settings.carryOver && data.timerMode != TimerMode::FREE_TIME) {
    data.freeBalanceSeconds = 0;
  }
  saveData();
  dirty = true;
}

void setAdvAudioMuted(bool muted) {
  if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return;

  uint8_t dacControl = M5Cardputer.In_I2C.readRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x31, ADV_AUDIO_CODEC_I2C_FREQUENCY);
  dacControl &= 0x9F;
  if (muted) dacControl |= 0x60;
  M5Cardputer.In_I2C.writeRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x31, dacControl, ADV_AUDIO_CODEC_I2C_FREQUENCY);
}

void powerDownAdvAudioCodec() {
  if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return;

  setAdvAudioMuted(true);
  M5Cardputer.In_I2C.writeRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x32, 0x00, ADV_AUDIO_CODEC_I2C_FREQUENCY);
  delay(ADV_AUDIO_MUTE_SETTLE_MS);

  M5Cardputer.In_I2C.writeRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x0D, 0xFC, ADV_AUDIO_CODEC_I2C_FREQUENCY);
  M5Cardputer.In_I2C.writeRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x0E, 0x6A, ADV_AUDIO_CODEC_I2C_FREQUENCY);
  M5Cardputer.In_I2C.writeRegister8(
      ADV_AUDIO_CODEC_ADDRESS, 0x00, 0x00, ADV_AUDIO_CODEC_I2C_FREQUENCY);
}

void prepareSpeakerForSound() {
  bool coldStart = !M5Cardputer.Speaker.isRunning();
  M5Cardputer.Speaker.begin();
  if (coldStart) {
    // The ADV callback powers the codec up during begin(). Mute it again while
    // its analog path and I2S clocks settle, without occupying a sound channel.
    setAdvAudioMuted(true);
    delay(SPEAKER_COLD_START_SETTLE_MS);
  }
  setAdvAudioMuted(false);
}

void beepDone() {
  if (!data.settings.sound) return;
  prepareSpeakerForSound();
  M5Cardputer.Speaker.tone(1200, 100);
  delay(120);
  M5Cardputer.Speaker.tone(1700, 100);
  delay(120);
  M5Cardputer.Speaker.tone(2200, 180);
}

void beepClick() {
  if (!data.settings.sound) return;
  prepareSpeakerForSound();
  M5Cardputer.Speaker.tone(3500, 18);
}

void completeTimer();

void startTimer(TimerMode mode) {
  uint32_t seconds = 0;
  switch (mode) {
    case TimerMode::FOCUS: seconds = data.settings.focusMinutes * 60UL; break;
    case TimerMode::SHORT_BREAK: seconds = data.settings.shortBreakMinutes * 60UL; break;
    case TimerMode::LONG_BREAK: seconds = data.settings.longBreakMinutes * 60UL; break;
    case TimerMode::FREE_TIME: seconds = data.freeBalanceSeconds; break;
    default: return;
  }
  if (seconds == 0) return;
  data.timerMode = mode;
  data.timerStatus = TimerStatus::RUNNING;
  data.remainingSeconds = seconds;
  data.plannedSeconds = seconds;
  data.savedEpoch = rtcEpoch();
  lastTickMs = millis();
  screen = Screen::POMODORO;
  resultType = ResultType::NONE;
  saveData();
  dirty = true;
  beepClick();
}

void pauseTimer() {
  if (data.timerStatus != TimerStatus::RUNNING) return;
  data.timerStatus = TimerStatus::PAUSED;
  data.savedEpoch = rtcEpoch();
  saveData();
  dirty = true;
  beepClick();
}

void resumeTimer() {
  if (data.timerStatus != TimerStatus::PAUSED) return;
  data.timerStatus = TimerStatus::RUNNING;
  data.savedEpoch = rtcEpoch();
  lastTickMs = millis();
  saveData();
  dirty = true;
  beepClick();
}

void cancelTimer() {
  if (data.timerStatus == TimerStatus::IDLE) return;
  data.timerMode = TimerMode::NONE;
  data.timerStatus = TimerStatus::IDLE;
  data.remainingSeconds = 0;
  data.plannedSeconds = 0;
  data.savedEpoch = 0;
  screen = Screen::POMODORO;
  saveData();
  dirty = true;
  beepClick();
}

void completeTimer() {
  TimerMode completedMode = data.timerMode;
  uint32_t planned = data.plannedSeconds;
  data.timerMode = TimerMode::NONE;
  data.timerStatus = TimerStatus::IDLE;
  data.remainingSeconds = 0;
  data.savedEpoch = 0;
  resultRewardSeconds = 0;

  if (completedMode == TimerMode::FOCUS) {
    data.today.completed++;
    data.today.focusSeconds += planned;
    data.cycleCount++;
    resultType = ResultType::FOCUS_DONE;

    if (data.cycleCount >= data.settings.requiredCount) {
      uint32_t reward = data.settings.rewardMinutes * 60UL;
      if (data.settings.dailyRewardLimitMinutes > 0) {
        uint32_t dailyCap = data.settings.dailyRewardLimitMinutes * 60UL;
        reward = data.today.earnedSeconds >= dailyCap
                   ? 0
                   : min(reward, dailyCap - data.today.earnedSeconds);
      }
      uint32_t walletCap = data.settings.maximumFreeMinutes * 60UL;
      uint32_t room = data.freeBalanceSeconds >= walletCap
                        ? 0
                        : walletCap - data.freeBalanceSeconds;
      resultRewardSeconds = min(reward, room);
      data.freeBalanceSeconds += resultRewardSeconds;
      data.today.earnedSeconds += resultRewardSeconds;
      data.cycleCount = 0;
      resultType = ResultType::REWARD_EARNED;
    }
    screen = Screen::RESULT;
  } else if (completedMode == TimerMode::FREE_TIME) {
    data.freeBalanceSeconds = 0;
    resultType = ResultType::FREE_DONE;
    screen = Screen::RESULT;
  } else {
    resultType = ResultType::BREAK_DONE;
    screen = Screen::RESULT;
  }

  wakeDisplayForTimerCompletion();
  saveData();
  beepDone();

  if (completedMode == TimerMode::FOCUS && data.settings.autoBreak &&
      resultType != ResultType::REWARD_EARNED) {
    startTimer(TimerMode::SHORT_BREAK);
  } else if ((completedMode == TimerMode::SHORT_BREAK || completedMode == TimerMode::LONG_BREAK) &&
             data.settings.autoFocus) {
    startTimer(TimerMode::FOCUS);
  }
}

void restoreRunningTimer() {
  if (data.timerStatus != TimerStatus::RUNNING) return;
  int64_t now = rtcEpoch();
  if (now > 0 && data.savedEpoch > 0 && now >= data.savedEpoch) {
    uint64_t elapsed = static_cast<uint64_t>(now - data.savedEpoch);
    // A jump larger than seven days is treated as a clock change, not progress.
    if (elapsed <= 7UL * 24UL * 60UL * 60UL) {
      if (data.timerMode == TimerMode::FREE_TIME) {
        uint32_t consumed = min<uint64_t>(elapsed, data.freeBalanceSeconds);
        data.freeBalanceSeconds -= consumed;
        data.today.usedSeconds += consumed;
      }
      if (elapsed >= data.remainingSeconds) {
        data.remainingSeconds = 0;
        completeTimer();
        return;
      }
      data.remainingSeconds -= elapsed;
    } else {
      data.timerStatus = TimerStatus::PAUSED;
      restoredWithoutClock = true;
    }
  } else {
    // Original Cardputer models have no battery-backed RTC. Never award focus
    // progress from an unverifiable wall clock after reboot; restore as paused.
    data.timerStatus = TimerStatus::PAUSED;
    restoredWithoutClock = true;
  }
  data.savedEpoch = now;
  lastTickMs = millis();
  saveData();
}

void tickTimer() {
  if (data.timerStatus != TimerStatus::RUNNING) return;
  uint32_t now = millis();
  uint32_t elapsed = (now - lastTickMs) / 1000UL;
  if (elapsed == 0) return;
  lastTickMs += elapsed * 1000UL;

  uint32_t consumed = min(elapsed, data.remainingSeconds);
  if (data.timerMode == TimerMode::FREE_TIME) {
    consumed = min(consumed, data.freeBalanceSeconds);
    data.freeBalanceSeconds -= consumed;
    data.today.usedSeconds += consumed;
  }
  data.remainingSeconds -= consumed;
  dirty = true;

  if (data.remainingSeconds == 0 ||
      (data.timerMode == TimerMode::FREE_TIME && data.freeBalanceSeconds == 0)) {
    completeTimer();
    return;
  }
  if (now - lastSaveMs >= SAVE_INTERVAL_MS) {
    data.savedEpoch = rtcEpoch();
    saveData();
  }
}

String formatTime(uint32_t seconds, bool includeHours = false) {
  char value[16];
  if (includeHours || seconds >= 3600) {
    snprintf(value, sizeof(value), "%02lu:%02lu:%02lu",
             seconds / 3600UL, (seconds / 60UL) % 60UL, seconds % 60UL);
  } else {
    snprintf(value, sizeof(value), "%02lu:%02lu", seconds / 60UL, seconds % 60UL);
  }
  return String(value);
}

String formatMinutes(uint32_t seconds) {
  char value[20];
  snprintf(value, sizeof(value), "%lum", seconds / 60UL);
  return String(value);
}

void updateBatteryLevel(bool force = false) {
  uint32_t now = millis();
  if (!force && now - lastBatteryPollMs < 30000UL) return;
  lastBatteryPollMs = now;
  int32_t measured = M5Cardputer.Power.getBatteryLevel();
  if (measured >= 0) measured = constrain(measured, 0, 100);
  if (measured != batteryLevel) {
    batteryLevel = measured;
    dirty = true;
  }
}

bool registerUserActivity() {
  lastUserInputMs = millis();

  if (displayPowerState == DisplayPowerState::ACTIVE) return false;

  bool consumeKey = displayPowerState == DisplayPowerState::SLEEPING;
  if (consumeKey) M5Cardputer.Display.wakeup();
  displayPowerState = DisplayPowerState::ACTIVE;
  M5Cardputer.Display.setBrightness(DISPLAY_BRIGHTNESS_NORMAL);
  dirty = true;
  return consumeKey;
}

void wakeDisplayForTimerCompletion() {
  lastUserInputMs = millis();
  if (displayPowerState == DisplayPowerState::SLEEPING) {
    M5Cardputer.Display.wakeup();
  }
  displayPowerState = DisplayPowerState::ACTIVE;
  M5Cardputer.Display.setBrightness(DISPLAY_BRIGHTNESS_NORMAL);
  dirty = true;
}

void suspendSpeakerForDisplaySleep() {
  if (M5.getBoard() == m5::board_t::board_M5CardputerADV &&
      M5Cardputer.Speaker.isRunning()) {
    // M5Unified 0.2.21 does not power down the ADV ES8311 in Speaker.end().
    powerDownAdvAudioCodec();
  }

  // tone() lazily calls begin() the next time audio is needed, so keep the
  // speaker stopped across timer-only Light Sleep wakeups.
  M5Cardputer.Speaker.end();
}

void updateDisplayPower() {
  uint32_t idleMs = millis() - lastUserInputMs;
  if (idleMs >= DISPLAY_SLEEP_TIMEOUT_MS) {
    if (displayPowerState != DisplayPowerState::SLEEPING) {
      suspendSpeakerForDisplaySleep();
      displayPowerState = DisplayPowerState::SLEEPING;
      M5Cardputer.Display.sleep();
    }
    return;
  }
  if (idleMs >= DISPLAY_DIM_TIMEOUT_MS &&
      displayPowerState == DisplayPowerState::ACTIVE) {
    displayPowerState = DisplayPowerState::DIMMED;
    M5Cardputer.Display.setBrightness(DISPLAY_BRIGHTNESS_DIM);
  }
}

uint32_t lightSleepWakeIntervalMs() {
  uint32_t now = millis();
  uint32_t intervalMs = data.timerStatus == TimerStatus::RUNNING
                          ? SAVE_INTERVAL_MS
                          : LIGHT_SLEEP_IDLE_WAKE_INTERVAL_MS;

  if (data.timerStatus == TimerStatus::RUNNING && data.remainingSeconds > 0) {
    uint64_t remainingMs = static_cast<uint64_t>(data.remainingSeconds) * 1000ULL;
    uint32_t elapsedSinceTickMs = now - lastTickMs;
    uint32_t untilTimerCompletionMs = remainingMs > elapsedSinceTickMs
                                        ? static_cast<uint32_t>(remainingMs - elapsedSinceTickMs)
                                        : 1;
    intervalMs = min(intervalMs, untilTimerCompletionMs);
  }

  if (!wifiSsid.isEmpty() && nextWifiAttemptMs != 0 &&
      !deadlineReached(now, nextWifiAttemptMs)) {
    intervalMs = min(intervalMs, nextWifiAttemptMs - now);
  }

  return max(intervalMs, 1UL);
}

bool enterLightSleepIfPossible() {
  if (displayPowerState != DisplayPowerState::SLEEPING) return false;
  if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return false;

  // Explicit Light Sleep powers down the Wi-Fi peripheral, so defer it while
  // Wi-Fi is connecting, scanning, synchronizing, or intentionally always on.
  if (WiFi.getMode() != WIFI_OFF) return false;

  // The TCA8418 interrupt output is active-low. If it is already asserted,
  // allow the next normal loop iteration to drain the pending key event first.
  if (digitalRead(ADV_KEYBOARD_INTERRUPT_PIN) == LOW) return false;

  uint64_t wakeAfterUs = static_cast<uint64_t>(lightSleepWakeIntervalMs()) * 1000ULL;
  if (gpio_wakeup_enable(ADV_KEYBOARD_INTERRUPT_PIN, GPIO_INTR_LOW_LEVEL) != ESP_OK) {
    return false;
  }
  if (esp_sleep_enable_gpio_wakeup() != ESP_OK ||
      esp_sleep_enable_timer_wakeup(wakeAfterUs) != ESP_OK) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    gpio_wakeup_disable(ADV_KEYBOARD_INTERRUPT_PIN);
    gpio_set_intr_type(ADV_KEYBOARD_INTERRUPT_PIN, GPIO_INTR_ANYEDGE);
    return false;
  }

  M5Cardputer.Display.waitDisplay();
  esp_err_t result = esp_light_sleep_start();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  gpio_wakeup_disable(ADV_KEYBOARD_INTERRUPT_PIN);
  // gpio_wakeup_enable() changes the interrupt to level-low. Restore the
  // edge mode installed by the M5Cardputer TCA8418 keyboard driver.
  gpio_set_intr_type(ADV_KEYBOARD_INTERRUPT_PIN, GPIO_INTR_ANYEDGE);
  return result == ESP_OK;
}

// Screen modules are included here so they share the application state above
// while remaining readable as independent Arduino IDE tabs.
#include "ui_common.h"
#include "screen_focus.h"
#include "screen_settings.h"
#include "screen_history.h"
#include "screen_result.h"
#include "screen_launcher.h"
#include "screen_lock.h"
#include "screen_wifi.h"
#include "screen_router.h"

}  // namespace

void setup() {
  auto config = M5.config();
  config.output_power = true;
  M5Cardputer.begin(config, true);
  // Put the ADV codec into a known silent state at the earliest point where
  // its internal I2C bus is available. This does not start the speaker task.
  powerDownAdvAudioCodec();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(DISPLAY_BRIGHTNESS_NORMAL);
  M5Cardputer.Display.setTextWrap(false);
  canvas.setPsram(false);
  canvas.setColorDepth(16);
  canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  canvas.setTextWrap(false);
  M5Cardputer.Speaker.setVolume(80);
  updateBatteryLevel(true);

  preferences.begin("focuswallet", false);
  if (!loadData()) saveData();
  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_pass", "");
  wifiAlwaysOn = preferences.getBool("wifi_always", false);
  if (wifiSsid.isEmpty()) loadFirstWifiPreset(wifiSsid, wifiPassword);

  rtcAvailable = M5.Rtc.isEnabled() && currentDateKey() != 0;
  checkDayChange();
  if (!wifiSsid.isEmpty()) beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
  else WiFi.mode(WIFI_OFF);

  if (data.timerStatus == TimerStatus::RUNNING) {
    if (rtcEpoch() > 0 || wifiSsid.isEmpty()) {
      restoreRunningTimer();
    } else {
      timerRestorePending = true;
      timerRestoreDeadlineMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
    }
  }
  lastTickMs = millis();
  lastUserInputMs = millis();
  dirty = true;
}

void loop() {
  M5Cardputer.update();
  updateBatteryLevel();
  updateWifiAndTime();
  handleKeyboard();
  updateDisplayPower();
  checkDayChange();
  if (!timerRestorePending) tickTimer();

  if (displayPowerState != DisplayPowerState::SLEEPING &&
      (dirty || (data.timerStatus == TimerStatus::RUNNING &&
                 data.remainingSeconds != lastDrawSecond))) {
    drawScreen();
  }
  if (!enterLightSleepIfPossible()) delay(8);
}
