#pragma once

String maskedPassword() {
  String masked;
  masked.reserve(passwordInput.length());
  for (size_t i = 0; i < passwordInput.length(); ++i) masked += '*';
  return masked;
}

void drawWifiSettings() {
  auto& lcd = canvas;
  lcd.fillScreen(BG);
  header("WI-FI SETTINGS", WIFI_COLOR);

  if (wifiView == WifiView::NETWORKS) {
    int first = wifiNetworkIndex > 1 ? wifiNetworkIndex - 1 : 0;
    if (first + 4 > wifiScanCount) first = max(0, wifiScanCount - 4);
    for (int row = 0; row < 4 && first + row < wifiScanCount; ++row) {
      int index = first + row;
      int y = 23 + row * 20;
      bool selected = index == wifiNetworkIndex;
      if (selected) lcd.fillRoundRect(5, y, 230, 18, 4, PANEL);
      String lock = WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? " " : "*";
      drawFitText((selected ? "> " : "  ") + lock + WiFi.SSID(index),
                  8, y + 9, 176, 2, selected ? TEXT : MUTED, middle_left);
      drawFitText(String(WiFi.RSSI(index)) + "dB", 231, y + 9, 43, 1,
                  selected ? WIFI_COLOR : MUTED, middle_right);
    }
    drawFooter("[ENTER] CHOOSE NETWORK", "[*] LOCKED   [H] HOME");
    return;
  }

  if (wifiView == WifiView::PASSWORD) {
    drawFitText("ENTER WI-FI PASSWORD", 120, 31, 226, 2, TEXT);
    drawFitText(selectedSsid, 120, 49, 226, 1, WIFI_COLOR);
    lcd.fillRoundRect(8, 60, 224, 29, 5, PANEL);
    String masked = maskedPassword();
    if (masked.isEmpty()) masked = "Type password...";
    drawFitText(masked, 15, 74, 210, 2, passwordInput.isEmpty() ? MUTED : TEXT, middle_left);
    drawFitText(String(passwordInput.length()) + " / 63 characters", 228, 96, 150, 1,
                MUTED, middle_right);
    drawFooter("[ENTER] CONNECT   [DEL] ERASE", "TYPE PASSWORD   [TAB] CANCEL");
    return;
  }

  if (wifiView == WifiView::SCANNING || wifiView == WifiView::CONNECTING) {
    const char* action = wifiView == WifiView::SCANNING ? "SCANNING..." : "CONNECTING...";
    drawFitText(action, 120, 49, 226, 4, WIFI_COLOR);
    drawFitText(wifiMessage, 120, 75, 226, 2, TEXT);
    drawFitText("Please wait", 120, 94, 226, 1, MUTED);
    drawFooter("[X] CANCEL", "[H] HOME");
    return;
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  drawFitText(connected ? "CONNECTED" : "DISCONNECTED", 120, 31, 226, 2,
              connected ? FREE_COLOR : DANGER);
  drawFitText(connected ? WiFi.SSID() : (wifiSsid.isEmpty() ? "No saved network" : wifiSsid),
              120, 47, 226, 1, TEXT);
  drawFitText(wifiAlwaysOn ? "MODE: ALWAYS CONNECTED" : "MODE: HOURLY TIME SYNC",
              120, 62, 226, 1, wifiAlwaysOn ? WIFI_COLOR : MUTED);
  tm now;
  if (getJstTime(now)) {
    char synced[28];
    snprintf(synced, sizeof(synced), "JST  %02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
    drawFitText(synced, 120, 78, 226, 2, CLOCK_COLOR);
  } else {
    drawFitText("JST not synchronized", 120, 78, 226, 2, MUTED);
  }
  drawFitText(wifiMessage, 120, 96, 226, 1, MUTED);
  drawFooter("[A] MODE   [S] SCAN   [C] SYNC", "[X] OFF   [F] FORGET   [H] HOME");
}

void leaveWifiOperation() {
  if (wifiView == WifiView::SCANNING) WiFi.scanDelete();
  if (wifiView == WifiView::CONNECTING) {
    stopWifiRadio("Operation cancelled");
  } else if (!wifiAlwaysOn) {
    stopWifiRadio("Wi-Fi off until next sync");
  }
  wifiView = WifiView::STATUS;
  if (wifiAlwaysOn) wifiMessage = "Operation cancelled";
  dirty = true;
}

void chooseWifiNetwork() {
  if (wifiScanCount <= 0 || wifiNetworkIndex < 0 || wifiNetworkIndex >= wifiScanCount) return;
  selectedSsid = WiFi.SSID(wifiNetworkIndex);
  if (WiFi.encryptionType(wifiNetworkIndex) == WIFI_AUTH_OPEN) {
    wifiSsid = selectedSsid;
    wifiPassword = "";
    saveWifiCredentials();
    WiFi.scanDelete();
    beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
  } else {
    String presetPassword;
    if (findWifiPreset(selectedSsid, presetPassword)) {
      wifiSsid = selectedSsid;
      wifiPassword = presetPassword;
      saveWifiCredentials();
      WiFi.scanDelete();
      beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
    } else {
      passwordInput = selectedSsid == wifiSsid ? wifiPassword : "";
      wifiView = WifiView::PASSWORD;
      dirty = true;
    }
  }
}

void handleWifiKey(const Keyboard_Class::KeysState& keys) {
  if (wifiView == WifiView::PASSWORD) {
    if (keys.tab) {
      wifiView = wifiScanCount > 0 ? WifiView::NETWORKS : WifiView::STATUS;
      passwordInput = "";
      dirty = true;
      return;
    }
    if (keys.del && !passwordInput.isEmpty()) {
      passwordInput.remove(passwordInput.length() - 1);
      dirty = true;
    }
    for (char key : keys.word) {
      if (passwordInput.length() < 63 && key >= 32 && key <= 126) passwordInput += key;
    }
    if (keys.space && passwordInput.length() < 63 &&
        (keys.word.empty() || keys.word.back() != ' ')) {
      passwordInput += ' ';
    }
    if (keys.enter) {
      wifiSsid = selectedSsid;
      wifiPassword = passwordInput;
      saveWifiCredentials();
      WiFi.scanDelete();
      beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
    }
    dirty = true;
    return;
  }

  if (keyPressed(keys, 'h')) {
    if (wifiView == WifiView::SCANNING || wifiView == WifiView::CONNECTING ||
        wifiView == WifiView::NETWORKS) {
      leaveWifiOperation();
    }
    screen = Screen::LAUNCHER;
    dirty = true;
    beepClick();
    return;
  }
  if (keyPressed(keys, 'x') &&
      (wifiView == WifiView::SCANNING || wifiView == WifiView::CONNECTING)) {
    leaveWifiOperation();
    return;
  }
  if (wifiView == WifiView::NETWORKS) {
    if (arrowUp(keys)) {
      wifiNetworkIndex = wifiNetworkIndex <= 0 ? wifiScanCount - 1 : wifiNetworkIndex - 1;
      dirty = true;
    } else if (arrowDown(keys)) {
      wifiNetworkIndex = (wifiNetworkIndex + 1) % wifiScanCount;
      dirty = true;
    } else if (keys.enter) chooseWifiNetwork();
    return;
  }
  if (wifiView != WifiView::STATUS) return;

  if (keyPressed(keys, 'a')) {
    wifiAlwaysOn = !wifiAlwaysOn;
    preferences.putBool("wifi_always", wifiAlwaysOn);
    if (wifiAlwaysOn) {
      wifiMessage = "Always-connected mode enabled";
      nextWifiAttemptMs = 0;
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.setAutoReconnect(true);
        dirty = true;
      } else {
        beginWifiConnection(wifiSsid, wifiPassword, false);
      }
    } else {
      nextWifiAttemptMs = millis() + WIFI_SYNC_INTERVAL_MS;
      stopWifiRadio("Hourly sync mode - Wi-Fi off");
    }
  } else if (keyPressed(keys, 's')) startWifiScan();
  else if (keyPressed(keys, 'c')) beginWifiConnection(wifiSsid, wifiPassword, !wifiAlwaysOn);
  else if (keyPressed(keys, 'x')) {
    nextWifiAttemptMs = millis() + (wifiAlwaysOn ? WIFI_RETRY_INTERVAL_MS
                                                 : WIFI_SYNC_INTERVAL_MS);
    stopWifiRadio(wifiAlwaysOn ? "Disconnected - auto retry later"
                               : "Wi-Fi off until next sync");
  } else if (keyPressed(keys, 'f')) {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    wifiSsid = "";
    wifiPassword = "";
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    nextWifiAttemptMs = UINT32_MAX;
    ntpConfigured = false;
    wifiMessage = "Saved network forgotten";
    dirty = true;
  }
}
