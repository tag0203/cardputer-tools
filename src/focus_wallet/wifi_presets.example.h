#pragma once

#include <stddef.h>

// Copy the entries you need into wifi_presets.h and replace the sample values.
struct WifiPreset {
  const char* ssid;
  const char* password;
};

static const WifiPreset WIFI_PRESETS[] = {
    {"HomeWiFi", "replace-with-home-password"},
    {"MobileHotspot", "replace-with-hotspot-password"},
};

static constexpr size_t WIFI_PRESET_COUNT =
    sizeof(WIFI_PRESETS) / sizeof(WIFI_PRESETS[0]);
