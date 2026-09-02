#pragma once

void drawLauncher() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  header("APP HOME", CLOCK_COLOR);

  const char* title = "FOCUS WALLET";
  String detail1 = "Focus to earn rewards";
  String detail2 = String("Free-time balance: ") + formatMinutes(data.freeBalanceSeconds);
  uint16_t accent = FOCUS;
  if (launcherIndex == 0 && data.timerStatus != TimerStatus::IDLE) {
    detail1 = String(modeName(data.timerMode)) + "  " + formatTime(data.remainingSeconds);
  } else if (launcherIndex == 1) {
    title = "WI-FI SETTINGS";
    accent = WIFI_COLOR;
    if (WiFi.status() == WL_CONNECTED) {
      detail1 = "CONNECTED";
      detail2 = WiFi.SSID();
    } else {
      detail1 = "DISCONNECTED";
      detail2 = "Scan and manage networks";
    }
  }

  lcd.fillRoundRect(8, 27, 224, 70, 9, PANEL);
  lcd.fillRoundRect(16, 36, 42, 42, 8, accent);
  lcd.setTextDatum(middle_center);
  lcd.setTextFont(2);
  lcd.setTextColor(BG);
  lcd.drawString(String(launcherIndex + 1), 37, 57);
  drawFitText(title, 68, 43, 154, 2, TEXT, middle_left);
  drawFitText(detail1, 68, 63, 154, 1, MUTED, middle_left);
  drawFitText(detail2, 16, 87, 206, 1, MUTED, middle_left);

  for (uint8_t i = 0; i < 2; ++i) {
    lcd.fillCircle(114 + i * 12, 101, i == launcherIndex ? 4 : 2,
                   i == launcherIndex ? accent : MUTED);
  }
  drawFooter(String("APP ") + (launcherIndex + 1) + " / 2",
             "[ENTER] OPEN   [H] LOCK   [1] [2]");
}

void openLauncherApp(uint8_t index) {
  launcherIndex = constrain(index, 0, 1);
  if (launcherIndex == 0) screen = Screen::POMODORO;
  else screen = Screen::WIFI_SETTINGS;
  dirty = true;
  beepClick();
}

void handleLauncherKey(const Keyboard_Class::KeysState& keys) {
  if (keyPressed(keys, 'h')) {
    screen = Screen::LOCK_SCREEN;
    dirty = true;
    beepClick();
    return;
  }
  if (arrowLeft(keys) || arrowUp(keys)) {
    launcherIndex = launcherIndex == 0 ? 1 : launcherIndex - 1;
    dirty = true;
    beepClick();
  } else if (arrowRight(keys) || arrowDown(keys)) {
    launcherIndex = (launcherIndex + 1) % 2;
    dirty = true;
    beepClick();
  } else if (keys.enter) openLauncherApp(launcherIndex);
  else if (keyPressed(keys, '1')) openLauncherApp(0);
  else if (keyPressed(keys, '2')) openLauncherApp(1);
}
