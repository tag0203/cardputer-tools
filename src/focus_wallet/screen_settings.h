#pragma once

constexpr uint8_t SETTINGS_COUNT = 11;

String settingLabel(uint8_t index) {
  switch (index) {
    case 0: return "Focus";
    case 1: return "Short break";
    case 2: return "Long break";
    case 3: return "Pomodoros";
    case 4: return "Reward";
    case 5: return "Wallet cap";
    case 6: return "Daily cap";
    case 7: return "Carry over";
    case 8: return "Auto break";
    case 9: return "Auto focus";
    default: return "Sound";
  }
}

String settingValue(uint8_t index) {
  switch (index) {
    case 0: return String(data.settings.focusMinutes) + " min";
    case 1: return String(data.settings.shortBreakMinutes) + " min";
    case 2: return String(data.settings.longBreakMinutes) + " min";
    case 3: return String(data.settings.requiredCount) + " times";
    case 4: return String(data.settings.rewardMinutes) + " min";
    case 5: return String(data.settings.maximumFreeMinutes) + " min";
    case 6: return data.settings.dailyRewardLimitMinutes == 0
                     ? "Unlimited"
                     : String(data.settings.dailyRewardLimitMinutes) + " min";
    case 7: return data.settings.carryOver ? "ON" : "OFF";
    case 8: return data.settings.autoBreak ? "ON" : "OFF";
    case 9: return data.settings.autoFocus ? "ON" : "OFF";
    default: return data.settings.sound ? "ON" : "OFF";
  }
}

void drawSettings() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  header("SETTINGS", FOCUS);
  uint8_t first = settingsIndex > 1 ? settingsIndex - 1 : 0;
  if (first + 4 > SETTINGS_COUNT) first = SETTINGS_COUNT - 4;
  for (uint8_t row = 0; row < 4; ++row) {
    uint8_t index = first + row;
    int y = 23 + row * 20;
    bool selected = index == settingsIndex;
    if (selected) lcd.fillRoundRect(5, y, 230, 18, 4, PANEL);
    lcd.setTextDatum(middle_left);
    lcd.setTextFont(2);
    lcd.setTextColor(selected ? TEXT : MUTED);
    lcd.drawString((selected ? "> " : "  ") + settingLabel(index), 8, y + 8);
    lcd.setTextDatum(middle_right);
    lcd.setTextColor(selected ? FOCUS : TEXT);
    lcd.drawString(settingValue(index), 230, y + 8);
  }
  drawFooter("POMODORO SETTINGS", "[H] RETURN TO FOCUS WALLET");
}

void adjustSetting(int direction) {
  Settings& s = data.settings;
  switch (settingsIndex) {
    case 0: s.focusMinutes = constrain((int)s.focusMinutes + direction, 1, 120); break;
    case 1: s.shortBreakMinutes = constrain((int)s.shortBreakMinutes + direction, 1, 60); break;
    case 2: s.longBreakMinutes = constrain((int)s.longBreakMinutes + direction, 1, 120); break;
    case 3: s.requiredCount = constrain((int)s.requiredCount + direction, 1, 12); break;
    case 4: s.rewardMinutes = constrain((int)s.rewardMinutes + direction * 5, 5, 240); break;
    case 5:
      s.maximumFreeMinutes = constrain((int)s.maximumFreeMinutes + direction * 15, 15, 1440);
      data.freeBalanceSeconds = min(data.freeBalanceSeconds, s.maximumFreeMinutes * 60UL);
      break;
    case 6: {
      int value = s.dailyRewardLimitMinutes + direction * 30;
      s.dailyRewardLimitMinutes = constrain(value, 0, 1440);
      break;
    }
    case 7: s.carryOver = !s.carryOver; break;
    case 8: s.autoBreak = !s.autoBreak; break;
    case 9: s.autoFocus = !s.autoFocus; break;
    case 10: s.sound = !s.sound; break;
  }
  saveData();
  dirty = true;
  beepClick();
}

void handleSettingsKey(const Keyboard_Class::KeysState& keys) {
  if (keyPressed(keys, 'h')) { screen = Screen::POMODORO; dirty = true; beepClick(); return; }
  if (arrowUp(keys)) {
    settingsIndex = settingsIndex == 0 ? SETTINGS_COUNT - 1 : settingsIndex - 1;
    dirty = true;
    beepClick();
  } else if (arrowDown(keys)) {
    settingsIndex = (settingsIndex + 1) % SETTINGS_COUNT;
    dirty = true;
    beepClick();
  } else if (arrowLeft(keys)) adjustSetting(-1);
  else if (arrowRight(keys)) adjustSetting(1);
}
