#pragma once

constexpr uint8_t OMIKUJI_MAX_CHOICES = 20;
constexpr size_t OMIKUJI_MAX_MESSAGE = 48;
constexpr uint8_t OMIKUJI_SET_COUNT = 3;
uint8_t omikujiSet = 0;
uint8_t omikujiCount = 2;
String omikujiMessages[OMIKUJI_MAX_CHOICES] = {"yes", "no"};
uint8_t omikujiIndex = 0;
int omikujiResult = -1;
bool omikujiSettings = false;
bool omikujiEditing = false;
String omikujiInput;
constexpr uint32_t OMIKUJI_SPIN_MS = 3000;
bool omikujiSpinning = false;
uint8_t omikujiPreview = 0;
uint8_t omikujiTarget = 0;
uint32_t omikujiStartedMs = 0;
uint32_t omikujiNextStepMs = 0;

void updateOmikuji() {
  if (!omikujiSpinning) return;
  if (screen != Screen::OMIKUJI) {
    omikujiSpinning = false;
    return;
  }
  const uint32_t now = millis();
  const uint32_t elapsed = now - omikujiStartedMs;
  if (static_cast<int32_t>(now - omikujiNextStepMs) < 0) return;
  omikujiPreview = (omikujiPreview + 1) % omikujiCount;
  if (elapsed >= OMIKUJI_SPIN_MS && omikujiPreview == omikujiTarget) {
    // Stop on the actual roulette candidate; never jump to another result.
    omikujiResult = omikujiPreview;
    omikujiSpinning = false;
    if (!completionSoundPlaying) beepDone();
  } else {
    // Cap the slowdown after three seconds while advancing to the target.
    const uint32_t rampMs = min(elapsed, OMIKUJI_SPIN_MS);
    omikujiNextStepMs = now + 55 + 245 * rampMs * rampMs / (OMIKUJI_SPIN_MS * OMIKUJI_SPIN_MS);
    beepClick();
  }
  dirty = true;
}

void startOmikuji() {
  // Choose the final result before animating, preserving equal probabilities.
  uint32_t value;
  const uint32_t threshold = (uint32_t(0) - uint32_t(omikujiCount)) % omikujiCount;
  do { value = esp_random(); } while (value < threshold);
  omikujiTarget = value % omikujiCount;
  omikujiResult = -1;
  omikujiPreview = esp_random() % omikujiCount;
  omikujiSpinning = true;
  omikujiStartedMs = millis();
  omikujiNextStepMs = omikujiStartedMs + 55;
}

// Keep the original keys for set 1 so existing choices remain available.
String omikujiCountKey() {
  return omikujiSet == 0 ? String("lot_count") : String("lot_count") + omikujiSet;
}

String omikujiMessageKey(uint8_t index) {
  return omikujiSet == 0 ? String("lot_msg") + index
                         : String("lot_s") + omikujiSet + "m" + String(index);
}

void loadOmikujiSet() {
  omikujiCount = constrain(preferences.getUChar(omikujiCountKey().c_str(), 2), 2, OMIKUJI_MAX_CHOICES);
  for (uint8_t i = 0; i < OMIKUJI_MAX_CHOICES; ++i) {
    String key = omikujiMessageKey(i);
    omikujiMessages[i] = preferences.getString(key.c_str(), i == 0 ? "yes" : i == 1 ? "no" : "");
    omikujiMessages[i] = omikujiMessages[i].substring(0, OMIKUJI_MAX_MESSAGE);
  }
  omikujiIndex = 0;
  omikujiResult = -1;
}

void loadOmikuji() {
  omikujiSet = constrain(preferences.getUChar("lot_set", 0), 0, OMIKUJI_SET_COUNT - 1);
  loadOmikujiSet();
}

String omikujiLabel(uint8_t index) {
  String message = omikujiMessages[index];
  message.trim();
  return message.isEmpty() ? String(index + 1) : message;
}

void drawOmikuji() {
  canvas.fillScreen(BG);
  String title = String("OMIKUJI  SET ") + (omikujiSet + 1);
  header(title.c_str(), CLOCK_COLOR);
  if (omikujiEditing) {
    drawFitText(String("Message #") + (omikujiIndex + 1), 120, 34, 224, 2, TEXT);
    // Show the tail while typing so the insertion point remains visible.
    String tail = omikujiInput;
    canvas.setTextFont(2);
    while (!tail.isEmpty() && canvas.textWidth(tail + "_") > 220) tail.remove(0, 1);
    drawFitText(tail + "_", 10, 60, 220, 2, CLOCK_COLOR, middle_left);
    drawFitText(String(omikujiInput.length()) + "/48  Empty = choice number", 120, 88, 224, 1, MUTED);
    drawFooter("[ENTER] SAVE   [TAB] CANCEL", "[DEL] ERASE   [TYPE] MESSAGE");
  } else if (omikujiSettings) {
    drawFitText(String("Choices: ") + omikujiCount + " / 20", 120, 32, 224, 2, TEXT);
    drawFitText(String("Choice #") + (omikujiIndex + 1), 120, 54, 224, 1, MUTED);
    drawFitText(omikujiLabel(omikujiIndex), 120, 77, 224, 2, CLOCK_COLOR);
    drawFooter("[UP/DOWN] PICK  [LEFT/RIGHT] COUNT", "[ENTER] EDIT [1-3] SET [H] BACK");
  } else if (omikujiSpinning) {
    drawFitText("Drawing...", 120, 34, 224, 2, MUTED);
    drawFitText(omikujiLabel(omikujiPreview), 120, 64, 224, 4, CLOCK_COLOR);
    const uint32_t elapsed = min(uint32_t(millis() - omikujiStartedMs), OMIKUJI_SPIN_MS);
    canvas.fillRoundRect(20, 91, 200, 5, 2, PANEL);
    canvas.fillRoundRect(20, 91, max(2, int(200 * elapsed / OMIKUJI_SPIN_MS)), 5, 2, CLOCK_COLOR);
    drawFooter("ROLLING...", "[H] CANCEL / HOME");
  } else {
    drawFitText(String(omikujiCount) + " choices", 120, 34, 224, 2, MUTED);
    drawFitText(omikujiResult < 0 ? "Ready?" : omikujiLabel(omikujiResult), 120, 64, 224, 4, TEXT);
    if (omikujiResult >= 0) drawFitText(String("Choice #") + (omikujiResult + 1), 120, 91, 224, 1, CLOCK_COLOR);
    drawFooter("[SPACE/ENTER] DRAW   [1-3] SET", "[S] EDIT CHOICES   [H] HOME");
  }
}

void handleOmikujiKey(const Keyboard_Class::KeysState& keys) {
  if (omikujiSpinning) {
    if (keyPressed(keys, 'h')) {
      omikujiSpinning = false;
      screen = Screen::LAUNCHER;
      dirty = true;
    }
    return;
  }
  if (omikujiEditing) {
    if (keys.tab) {
      omikujiEditing = false;
    } else if (keys.enter) {
      omikujiMessages[omikujiIndex] = omikujiInput;
      String key = omikujiMessageKey(omikujiIndex);
      preferences.putString(key.c_str(), omikujiInput);
      omikujiEditing = false;
      omikujiResult = -1;
    } else {
      if (keys.del) removeLastUtf8Character(omikujiInput);
      for (char key : keys.word) {
        if (key >= 32 && key <= 126 && omikujiInput.length() < OMIKUJI_MAX_MESSAGE) omikujiInput += key;
      }
      if (keys.space && omikujiInput.length() < OMIKUJI_MAX_MESSAGE &&
          (keys.word.empty() || keys.word.back() != ' ')) omikujiInput += ' ';
    }
    dirty = true;
    return;
  }
  for (uint8_t i = 0; i < OMIKUJI_SET_COUNT; ++i) {
    if (keyPressed(keys, '1' + i)) {
      if (omikujiSet != i) {
        omikujiSet = i;
        loadOmikujiSet();
        preferences.putUChar("lot_set", omikujiSet);
        dirty = true;
        beepClick();
      }
      return;
    }
  }
  if (keyPressed(keys, 'h')) {
    if (omikujiSettings) omikujiSettings = false;
    else screen = Screen::LAUNCHER;
  } else if (omikujiSettings) {
    if (arrowUp(keys)) omikujiIndex = (omikujiIndex + omikujiCount - 1) % omikujiCount;
    else if (arrowDown(keys)) omikujiIndex = (omikujiIndex + 1) % omikujiCount;
    else if (arrowLeft(keys) || arrowRight(keys)) {
      uint8_t count = constrain(omikujiCount + (arrowRight(keys) ? 1 : -1), 2, OMIKUJI_MAX_CHOICES);
      if (count != omikujiCount) {
        omikujiCount = count;
        omikujiIndex = min(omikujiIndex, static_cast<uint8_t>(count - 1));
        omikujiResult = -1;
        preferences.putUChar(omikujiCountKey().c_str(), count);
      }
    } else if (keys.enter) {
      omikujiInput = omikujiMessages[omikujiIndex];
      omikujiEditing = true;
    } else return;
  } else if (keyPressed(keys, 's')) {
    omikujiSettings = true;
  } else if (keys.enter || keys.space) {
    startOmikuji();
  } else return;
  dirty = true;
  beepClick();
}
