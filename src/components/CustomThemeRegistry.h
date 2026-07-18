#pragma once

#include <cstddef>
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
  enum class Kind : uint8_t { ComposedV1, DeclarativeV2 } kind = Kind::ComposedV1;
  uint8_t baseTheme = 0;
  uint8_t headerTheme = 0;
  uint8_t listTheme = 0;
  uint8_t menuTheme = 0;
  uint8_t popupTheme = 0;
  uint8_t inputTheme = 0;
  uint8_t hintsTheme = 0;
  uint8_t statusTheme = 0;

  // Declarative v2 values are normalized to 0..1000 within the Home cover
  // area. Keeping the compiled summary fixed-size avoids retaining JSON DOMs.
  char homeBackground[65] = {};
  uint16_t coverX = 0;
  uint16_t coverY = 0;
  uint16_t coverWidth = 1000;
  uint16_t coverHeight = 1000;
  uint16_t coverCornerRadius = 0;
  uint16_t homeTopPadding = 56;
  uint16_t homeCoverAreaHeight = 242;
  uint16_t homeMenuTopOffset = 16;
  uint8_t menuColumns = 1;
  uint16_t menuRowHeight = 0;
  uint16_t menuGap = 8;

  // Optional v2 component metrics. Defaults match Lyra so existing v2 themes
  // keep their current layout. Fixed-width values avoid retaining a JSON DOM.
  uint16_t headerHeight = 84;
  uint16_t topPadding = 5;
  uint16_t verticalSpacing = 16;
  uint16_t contentSidePadding = 20;
  uint16_t tabBarHeight = 40;
  uint16_t tabSpacing = 8;
  uint16_t listRowHeight = 36;
  uint16_t listSubtitleRowHeight = 60;
  uint16_t popupTopPermille = 165;
  uint16_t popupMarginX = 16;
  uint16_t popupMarginY = 12;
  uint16_t popupCornerRadius = 6;
  uint8_t popupFrameThickness = 2;
  uint8_t popupProgressHeight = 4;
  bool popupTextBold = false;
  bool popupTextInverted = false;
  uint16_t keyboardKeyWidth = 31;
  uint16_t keyboardKeyHeight = 40;
  uint16_t keyboardKeySpacing = 0;
  uint16_t keyboardCornerRadius = 6;
  uint16_t keyboardWidthPercent = 90;
  uint16_t textFieldPadding = 6;
  uint8_t textFieldThickness = 1;
  uint8_t textFieldCursorThickness = 3;
  bool keyboardFillUnselected = false;
  bool keyboardOutlineUnselected = false;
  uint16_t hintsHeight = 40;
  uint16_t sideHintsWidth = 30;
  uint16_t statusMarginX = 5;
  uint16_t statusMarginY = 19;
  uint16_t progressBarHeight = 16;
};

// The registry retains at most 16 summaries, keeping the persistent heap
// budget for all custom themes at or below 4 KB.
static_assert(sizeof(CustomThemeInfo) <= 256, "CustomThemeInfo exceeded its fixed registry budget");

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
