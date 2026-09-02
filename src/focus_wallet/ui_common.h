#pragma once

// Shared display primitives used by every screen.
void drawBattery(uint16_t normalColor = BG) {
  auto& lcd = canvas;
  constexpr int x = 197;
  constexpr int y = 5;
  constexpr int width = 13;
  constexpr int height = 10;
  uint16_t batteryColor = batteryLevel >= 0 && batteryLevel <= 15 ? DANGER : normalColor;

  lcd.drawRoundRect(x, y, width, height, 2, batteryColor);
  lcd.fillRect(x + width, y + 3, 2, 4, batteryColor);
  if (batteryLevel >= 0) {
    int fillWidth = (width - 4) * batteryLevel / 100;
    if (batteryLevel > 0 && fillWidth == 0) fillWidth = 1;
    if (fillWidth > 0) lcd.fillRect(x + 2, y + 2, fillWidth, height - 4, batteryColor);
  }

  char percentage[6];
  if (batteryLevel >= 0) snprintf(percentage, sizeof(percentage), "%ld%%", batteryLevel);
  else snprintf(percentage, sizeof(percentage), "--");
  lcd.setTextDatum(middle_right);
  lcd.setTextFont(1);
  lcd.setTextSize(1.0f);
  lcd.setTextColor(batteryColor);
  lcd.drawString(percentage, 239, 10);
}

void header(const char* title, uint16_t color) {
  auto& lcd = canvas;
  lcd.fillRect(0, 0, lcd.width(), 20, color);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(BG);
  lcd.setTextFont(2);
  lcd.drawString(title, 7, 10);
  drawBattery();
}

void removeLastUtf8Character(String& value) {
  if (value.isEmpty()) return;
  int index = value.length() - 1;
  while (index > 0 && (static_cast<uint8_t>(value[index]) & 0xC0) == 0x80) --index;
  value.remove(index);
}

void drawFitText(const String& value, int x, int y, int maxWidth,
                 uint8_t font, uint16_t color, textdatum_t datum = middle_center) {
  auto& lcd = canvas;
  lcd.setTextFont(font);
  lcd.setTextSize(1.0f);
  String fitted = value;
  if (lcd.textWidth(fitted) > maxWidth) {
    const String ellipsis = "...";
    while (!fitted.isEmpty() && lcd.textWidth(fitted + ellipsis) > maxWidth) {
      removeLastUtf8Character(fitted);
    }
    fitted += ellipsis;
  }
  lcd.setTextDatum(datum);
  lcd.setTextColor(color);
  lcd.drawString(fitted, x, y);
}

void drawFooter(const String& firstLine, const String& secondLine) {
  auto& lcd = canvas;
  lcd.fillRect(0, 105, lcd.width(), 30, PANEL);
  lcd.drawFastHLine(0, 105, lcd.width(), MUTED);
  drawFitText(firstLine, 120, 113, 228, 1, TEXT);
  drawFitText(secondLine, 120, 127, 228, 1, TEXT);
}

void drawCompanion(const char* message, uint16_t accent) {
  auto& lcd = canvas;
  lcd.drawPng(character_png, character_png_len, 2, 21);
  lcd.fillRoundRect(59, 24, 176, 23, 6, PANEL);
  lcd.fillTriangle(59, 34, 53, 40, 63, 39, PANEL);
  drawFitText(message, 147, 35, 164, 1, TEXT);
}

// Cardputer ADV arrow positions work as arrows without holding Fn in menus.
bool keyPressed(const Keyboard_Class::KeysState& keys, char target) {
  for (char key : keys.word) {
    if (key == target || key == target - ('a' - 'A')) return true;
  }
  return false;
}

bool hidKeyPressed(const Keyboard_Class::KeysState& keys, uint8_t target) {
  for (uint8_t key : keys.hid_keys) {
    if (key == target) return true;
  }
  return false;
}

bool arrowUp(const Keyboard_Class::KeysState& keys) {
  return hidKeyPressed(keys, 0x52) || keyPressed(keys, ';');
}

bool arrowLeft(const Keyboard_Class::KeysState& keys) {
  return hidKeyPressed(keys, 0x50) || keyPressed(keys, ',');
}

bool arrowDown(const Keyboard_Class::KeysState& keys) {
  return hidKeyPressed(keys, 0x51) || keyPressed(keys, '.');
}

bool arrowRight(const Keyboard_Class::KeysState& keys) {
  return hidKeyPressed(keys, 0x4F) || keyPressed(keys, '/');
}
