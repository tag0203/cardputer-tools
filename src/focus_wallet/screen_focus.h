#pragma once

void drawProgress(int y) {
  auto& lcd = canvas;
  uint8_t count = data.settings.requiredCount;
  int usable = 120;
  int gap = max(4, usable / max<int>(count, 1));
  int start = 8;
  for (uint8_t i = 0; i < count && i < 12; ++i) {
    lcd.fillCircle(start + i * gap, y, 4, i < data.cycleCount ? FOCUS : 0x4A69);
  }
  lcd.setTextDatum(middle_right);
  lcd.setTextFont(2);
  lcd.setTextColor(TEXT);
  lcd.drawString(String(data.cycleCount) + " / " + String(count), 231, y);
}

void drawHome() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  TimerMode shownMode = data.timerStatus == TimerStatus::IDLE ? TimerMode::FOCUS : data.timerMode;
  uint16_t accent = modeColor(shownMode);
  header(data.timerStatus == TimerStatus::IDLE ? "FOCUS WALLET" : modeName(data.timerMode), accent);
  String headerBalance = String("FREE ") + formatTime(data.freeBalanceSeconds,
                                                        data.freeBalanceSeconds >= 3600);
  drawFitText(headerBalance, 192, 10, 82, 1, BG, middle_right);

  const char* message = "A little progress today.";
  if (restoredWithoutClock) message = "Timer restored paused.";
  else if (data.timerStatus == TimerStatus::RUNNING) message = "You are doing great.";
  else if (data.timerStatus == TimerStatus::PAUSED) message = "Ready when you are.";
  drawCompanion(message, accent);

  uint32_t shownSeconds = data.timerStatus == TimerStatus::IDLE
                            ? data.settings.focusMinutes * 60UL
                            : data.remainingSeconds;
  drawFitText(formatTime(shownSeconds), 147, 73, 172, 7, accent);
  drawProgress(97);

  if (data.timerStatus == TimerStatus::IDLE) {
    drawFooter("[SPACE] START   [U] FREE TIME", "[H] HOME   [S] SET   [D] LOG");
  } else {
    const char* action = data.timerStatus == TimerStatus::RUNNING ? "Pause" : "Resume";
    drawFooter(String("[SPACE] ") + action, "[X] CANCEL   [H] HOME");
  }
}

void handleHomeKey(const Keyboard_Class::KeysState& keys) {
  if (keys.space) {
    restoredWithoutClock = false;
    if (data.timerStatus == TimerStatus::IDLE) startTimer(TimerMode::FOCUS);
    else if (data.timerStatus == TimerStatus::RUNNING) pauseTimer();
    else resumeTimer();
    return;
  }
  if (keyPressed(keys, 'x') && data.timerStatus != TimerStatus::IDLE) {
    cancelTimer();
    return;
  }
  if (keyPressed(keys, 'h')) {
    screen = Screen::LAUNCHER;
    dirty = true;
    beepClick();
    return;
  }
  if (data.timerStatus != TimerStatus::IDLE) return;
  if (keyPressed(keys, 'f')) startTimer(TimerMode::FOCUS);
  else if (keyPressed(keys, 'b')) startTimer(TimerMode::SHORT_BREAK);
  else if (keyPressed(keys, 'u') && data.freeBalanceSeconds > 0) startTimer(TimerMode::FREE_TIME);
  else if (keyPressed(keys, 's')) { screen = Screen::SETTINGS; dirty = true; beepClick(); }
  else if (keyPressed(keys, 'd')) { screen = Screen::HISTORY; dirty = true; beepClick(); }
}
