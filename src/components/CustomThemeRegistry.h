#pragma once

#include <cstdint>
#include <vector>

// Theme manifests live on the SD card under /themes/<id>/theme.json.  The
// registry intentionally retains only this small summary; manifests and image
// assets stay on the card so installed themes do not consume framebuffer RAM.
struct CustomThemeInfo {
  static constexpr size_t kIdCapacity = 33;
  static constexpr size_t kNameCapacity = 49;

  char id[kIdCapacity] = {};
  char name[kNameCapacity] = {};
  uint8_t baseTheme = 0;
  uint8_t headerTheme = 0;
  uint8_t listTheme = 0;
  uint8_t menuTheme = 0;
  uint8_t popupTheme = 0;
  uint8_t inputTheme = 0;
  uint8_t hintsTheme = 0;
  uint8_t statusTheme = 0;
};

class CustomThemeRegistry {
 public:
  static constexpr const char* kThemesDir = "/themes";
  static constexpr const char* kComposerId = "__composer__";
  static constexpr int kMaxThemes = 16;

  static CustomThemeRegistry& getInstance();

  // Re-scan only on boot or when Settings opens. The vector capacity is kept
  // across scans, avoiding allocation churn while the UI is rendering.
  void discover();
  const std::vector<CustomThemeInfo>& getThemes() const { return themes_; }
  const CustomThemeInfo* find(const char* id) const;

 private:
  std::vector<CustomThemeInfo> themes_;

  bool loadManifest(const char* directoryName, CustomThemeInfo& out) const;
};

#define CUSTOM_THEMES CustomThemeRegistry::getInstance()
