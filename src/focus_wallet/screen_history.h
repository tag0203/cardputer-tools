#pragma once

void drawHistoryRow(const DayStats& day, int y, bool today) {
  auto& lcd = canvas;
  char date[14];
  if (today) snprintf(date, sizeof(date), "TODAY");
  else if (day.dateKey > 0) snprintf(date, sizeof(date), "%02ld/%02ld",
                                     (day.dateKey / 100) % 100, day.dateKey % 100);
  else snprintf(date, sizeof(date), "SESSION");
  lcd.setTextDatum(middle_left);
  lcd.setTextFont(1);
  lcd.setTextColor(today ? FOCUS : TEXT);
  lcd.drawString(date, 7, y);
  char values[42];
  snprintf(values, sizeof(values), "%2u  %3lum  +%3lum  -%3lum", day.completed,
           day.focusSeconds / 60UL, day.earnedSeconds / 60UL, day.usedSeconds / 60UL);
  lcd.setTextDatum(middle_right);
  lcd.setTextColor(TEXT);
  lcd.drawString(values, 233, y);
}

void drawHistory() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  header("DAILY LOG", FREE_COLOR);
  lcd.setTextDatum(middle_right);
  lcd.setTextFont(1);
  lcd.setTextColor(MUTED);
  lcd.drawString("done focus earned used", 233, 29);
  drawHistoryRow(data.today, 44, true);
  int y = 59;
  for (int i = 0; i < 3; ++i) {
    if (data.history[i].dateKey != 0 || data.history[i].completed != 0 ||
        data.history[i].usedSeconds != 0) {
      drawHistoryRow(data.history[i], y, false);
      y += 15;
    }
  }
  drawFooter("DAILY FOCUS / REWARD SUMMARY", "[H] RETURN HOME");
}

void handleHistoryKey(const Keyboard_Class::KeysState& keys) {
  if (keyPressed(keys, 'h')) {
    screen = Screen::POMODORO;
    dirty = true;
    beepClick();
  }
}
