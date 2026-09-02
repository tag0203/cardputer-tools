#pragma once

void drawScreen() {
  switch (screen) {
    case Screen::LOCK_SCREEN: drawLockScreen(); break;
    case Screen::LAUNCHER: drawLauncher(); break;
    case Screen::POMODORO: drawHome(); break;
    case Screen::SETTINGS: drawSettings(); break;
    case Screen::HISTORY: drawHistory(); break;
    case Screen::RESULT: drawResult(); break;
    case Screen::WIFI_SETTINGS: drawWifiSettings(); break;
  }
  canvas.pushSprite(0, 0);
  dirty = false;
  lastDrawSecond = data.remainingSeconds;
}

void handleKeyboard() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
  // The first key after dimming only restores brightness, preventing an
  // unintended menu action while the screen is hard to see.
  if (registerUserActivity()) return;
  const auto& keys = M5Cardputer.Keyboard.keysState();
  switch (screen) {
    case Screen::LOCK_SCREEN: handleLockScreenKey(keys); break;
    case Screen::LAUNCHER: handleLauncherKey(keys); break;
    case Screen::POMODORO: handleHomeKey(keys); break;
    case Screen::SETTINGS: handleSettingsKey(keys); break;
    case Screen::HISTORY: handleHistoryKey(keys); break;
    case Screen::RESULT: handleResultKey(keys); break;
    case Screen::WIFI_SETTINGS: handleWifiKey(keys); break;
  }
}
