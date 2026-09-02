#pragma once

void drawLockScreen() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);

  drawFitText("FOCUS WALLET", 7, 10, 150, 1, MUTED, middle_left);
  drawBattery(TEXT);

  tm now;
  if (getJstTime(now)) {
    char timeText[8];
    char secondsText[6];
    char dateText[32];
    static const char* weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    snprintf(timeText, sizeof(timeText), "%02d:%02d", now.tm_hour, now.tm_min);
    snprintf(secondsText, sizeof(secondsText), ":%02d", now.tm_sec);
    snprintf(dateText, sizeof(dateText), "%04d/%02d/%02d  %s  JST",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, weekdays[now.tm_wday]);
    drawFitText(timeText, 113, 48, 190, 7, CLOCK_COLOR);
    drawFitText(secondsText, 218, 61, 38, 2, MUTED, middle_right);
    drawFitText(dateText, 120, 80, 226, 2, TEXT);
  } else {
    drawFitText("--:--", 120, 48, 190, 7, MUTED);
    drawFitText("Connect Wi-Fi to set JST", 120, 80, 226, 2, TEXT);
  }

  String status;
  uint16_t statusColor = MUTED;
  if (data.timerStatus != TimerStatus::IDLE) {
    status = String(modeName(data.timerMode)) + "  " + formatTime(data.remainingSeconds);
    statusColor = modeColor(data.timerMode);
  } else if (WiFi.status() == WL_CONNECTED) {
    status = wifiAlwaysOn ? "Wi-Fi always connected" : "Synchronizing time...";
    statusColor = WIFI_COLOR;
  } else {
    status = wifiAlwaysOn ? "Wi-Fi always-on enabled" : "Wi-Fi syncs every hour";
  }
  drawFitText(status, 120, 98, 226, 1, statusColor);
  drawFooter("[ENTER] OPEN HOME", "Display dims after 2 minutes");
}

void handleLockScreenKey(const Keyboard_Class::KeysState& keys) {
  if (keys.enter || keys.space) {
    screen = Screen::LAUNCHER;
    dirty = true;
    beepClick();
  }
}
