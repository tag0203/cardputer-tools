#pragma once

void drawResult() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  uint16_t accent = FOCUS;
  const char* title = "FOCUS COMPLETE!";
  const char* message = "Nice work. Take a breath.";
  if (resultType == ResultType::REWARD_EARNED) {
    title = "GOAL COMPLETE!";
    message = "You earned free time!";
    accent = FREE_COLOR;
  } else if (resultType == ResultType::BREAK_DONE) {
    title = "BREAK COMPLETE";
    message = "Let's make progress again.";
    accent = BREAK_COLOR;
  } else if (resultType == ResultType::FREE_DONE) {
    title = "FREE TIME COMPLETE";
    message = "Ready for the next focus?";
    accent = FREE_COLOR;
  }
  header(title, accent);
  drawCompanion(message, accent);

  lcd.setTextDatum(middle_center);
  lcd.setTextFont(2);
  lcd.setTextColor(TEXT);
  if (resultType == ResultType::REWARD_EARNED) {
    lcd.drawString(String("Reward +") + formatMinutes(resultRewardSeconds), 120, 66);
    lcd.setTextColor(FREE_COLOR);
    lcd.drawString(String("Balance ") + formatMinutes(data.freeBalanceSeconds), 120, 85);
  } else if (resultType == ResultType::FOCUS_DONE) {
    lcd.drawString(String("Progress ") + data.cycleCount + " / " + data.settings.requiredCount, 120, 72);
  } else {
    lcd.drawString(String("Balance ") + formatMinutes(data.freeBalanceSeconds), 120, 72);
  }

  if (resultType == ResultType::REWARD_EARNED) {
    drawFooter("[U] USE FREE TIME       [F] FOCUS", "[B] START LONG BREAK    [H] HOME");
  } else if (resultType == ResultType::BREAK_DONE || resultType == ResultType::FREE_DONE) {
    drawFooter("[F] START NEXT FOCUS", "[H] RETURN HOME");
  } else {
    drawFooter("[B] START SHORT BREAK   [F] FOCUS", "[H] RETURN HOME");
  }
}

void handleResultKey(const Keyboard_Class::KeysState& keys) {
  if (keyPressed(keys, 'h')) {
    screen = Screen::POMODORO;
    resultType = ResultType::NONE;
    dirty = true;
  } else if (keyPressed(keys, 'f')) startTimer(TimerMode::FOCUS);
  else if (keyPressed(keys, 'b')) {
    startTimer(resultType == ResultType::REWARD_EARNED ? TimerMode::LONG_BREAK
                                                       : TimerMode::SHORT_BREAK);
  } else if (keyPressed(keys, 'u') && data.freeBalanceSeconds > 0) {
    startTimer(TimerMode::FREE_TIME);
  }
}
