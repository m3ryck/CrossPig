#include "CustomThemeRegistry.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <HalDisplay.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"

namespace {
constexpr size_t kManifestPathCapacity = 96;
constexpr size_t kMaxManifestBytes = 4096;

bool isSafeThemeId(const char* id) {
  if (!id || id[0] == '\0' || std::strlen(id) >= CustomThemeInfo::kIdCapacity) return false;
  for (const char* p = id; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (!(std::islower(c) || std::isdigit(c) || c == '-')) return false;
  }
  return true;
}

bool baseThemeForName(const char* name, uint8_t& out) {
  if (!name) return false;
  if (std::strcmp(name, "classic") == 0) out = CrossPointSettings::CLASSIC;
  else if (std::strcmp(name, "lyra") == 0) out = CrossPointSettings::LYRA;
  else if (std::strcmp(name, "lyra-extended") == 0) out = CrossPointSettings::LYRA_3_COVERS;
  else if (std::strcmp(name, "roundedraff") == 0) out = CrossPointSettings::ROUNDEDRAFF;
  else if (std::strcmp(name, "lyra-carousel") == 0) out = CrossPointSettings::LYRA_CAROUSEL;
  else if (std::strcmp(name, "minimal") == 0) out = CrossPointSettings::MINIMAL;
  else if (std::strcmp(name, "dashboard") == 0) out = CrossPointSettings::DASHBOARD;
  else return false;
  return true;
}

bool safeAssetPath(const char* path) {
  if (!path || std::strncmp(path, "assets/", 7) != 0 || std::strstr(path, "..") != nullptr) return false;
  for (const char* p = path; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (!(std::isalnum(c) || c == '/' || c == '.' || c == '_' || c == '-')) return false;
  }
  const size_t length = std::strlen(path);
  return length > 7 && length < 64 && std::strcmp(path + length - 4, ".bmp") == 0;
}

bool normalizedValue(const JsonVariantConst value, uint16_t fallback, uint16_t& out) {
  if (value.isNull()) {
    out = fallback;
    return true;
  }
  const int raw = value.as<int>();
  if (raw < 0 || raw > 1000) return false;
  out = static_cast<uint16_t>(raw);
  return true;
}

bool boundedValue(const JsonVariantConst value, const int fallback, const int minimum, const int maximum,
                  uint16_t& out) {
  const int raw = value.isNull() ? fallback : value.as<int>();
  if (raw < minimum || raw > maximum) return false;
  out = static_cast<uint16_t>(raw);
  return true;
}

bool boundedByte(const JsonVariantConst value, const int fallback, const int minimum, const int maximum,
                 uint8_t& out) {
  uint16_t parsed = 0;
  if (!boundedValue(value, fallback, minimum, maximum, parsed)) return false;
  out = static_cast<uint8_t>(parsed);
  return true;
}

void loadComponent(const JsonObjectConst components, const char* key, uint8_t fallback, uint8_t& out) {
  out = fallback;
  const char* value = components[key] | "";
  if (value[0] != '\0' && !baseThemeForName(value, out)) out = fallback;
}
}  // namespace

CustomThemeRegistry& CustomThemeRegistry::getInstance() {
  static CustomThemeRegistry instance;
  return instance;
}

const CustomThemeInfo* CustomThemeRegistry::find(const char* id) const {
  if (!id || id[0] == '\0') return nullptr;
  for (const auto& theme : themes_) {
    if (std::strcmp(theme.id, id) == 0) return &theme;
  }
  return nullptr;
}

bool CustomThemeRegistry::loadManifest(const char* directoryName, CustomThemeInfo& out) const {
  if (!isSafeThemeId(directoryName)) return false;

  char manifestPath[kManifestPathCapacity];
  const int written = std::snprintf(manifestPath, sizeof(manifestPath), "%s/%s/theme.json", kThemesDir, directoryName);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(manifestPath)) return false;

  FsFile file;
  if (!Storage.openFileForRead("THEME", manifestPath, file)) return false;
  if (file.fileSize() > kMaxManifestBytes) {
    LOG_ERR("THEME", "Manifest too large: %s", manifestPath);
    file.close();
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    LOG_ERR("THEME", "Invalid manifest %s: %s", manifestPath, error.c_str());
    return false;
  }
  const int schemaVersion = doc["schemaVersion"] | 0;
  if (schemaVersion != 1 && schemaVersion != 2) {
    LOG_ERR("THEME", "Unsupported schema in %s", manifestPath);
    return false;
  }

  const char* id = doc["id"] | "";
  const char* name = doc["name"] | "";
  if (!isSafeThemeId(id) || std::strcmp(id, directoryName) != 0 || name[0] == '\0' ||
      std::strlen(name) >= CustomThemeInfo::kNameCapacity) {
    LOG_ERR("THEME", "Invalid metadata in %s", manifestPath);
    return false;
  }

  std::strncpy(out.id, id, sizeof(out.id) - 1);
  std::strncpy(out.name, name, sizeof(out.name) - 1);
  if (schemaVersion == 2) {
    if (std::strcmp(doc["engine"] | "", "declarative") != 0) return false;
    out.kind = CustomThemeInfo::Kind::DeclarativeV2;
    out.baseTheme = CrossPointSettings::LYRA;  // neutral metrics for legacy activities
    const JsonObjectConst home = doc["home"].as<JsonObjectConst>();
    const JsonObjectConst cover = home["cover"].as<JsonObjectConst>();
    const JsonObjectConst menu = doc["menu"].as<JsonObjectConst>();
    if (home.isNull() || menu.isNull() || !normalizedValue(cover["x"], 0, out.coverX) ||
        !normalizedValue(cover["y"], 0, out.coverY) || !normalizedValue(cover["width"], 1000, out.coverWidth) ||
        !normalizedValue(cover["height"], 1000, out.coverHeight) ||
        !boundedValue(home["topPadding"], out.homeTopPadding, 0, 200, out.homeTopPadding) ||
        !boundedValue(home["coverAreaHeight"], out.homeCoverAreaHeight, 100, 600, out.homeCoverAreaHeight) ||
        !boundedValue(home["menuTopOffset"], out.homeMenuTopOffset, 0, 100, out.homeMenuTopOffset)) return false;
    const int radius = cover["cornerRadius"] | 0;
    const int columns = menu["columns"] | 1;
    const int rowHeight = menu["rowHeight"] | 0;
    const int gap = menu["gap"] | 8;
    if (radius < 0 || radius > 100 || columns < 1 || columns > 4 || rowHeight < 0 || rowHeight > 300 || gap < 0 || gap > 100) return false;
    out.coverCornerRadius = static_cast<uint16_t>(radius);
    out.menuColumns = static_cast<uint8_t>(columns);
    out.menuRowHeight = static_cast<uint16_t>(rowHeight);
    out.menuGap = static_cast<uint16_t>(gap);
    const JsonObjectConst header = doc["header"].as<JsonObjectConst>();
    const JsonObjectConst list = doc["list"].as<JsonObjectConst>();
    const JsonObjectConst popup = doc["popup"].as<JsonObjectConst>();
    const JsonObjectConst input = doc["input"].as<JsonObjectConst>();
    const JsonObjectConst hints = doc["hints"].as<JsonObjectConst>();
    const JsonObjectConst status = doc["status"].as<JsonObjectConst>();
    if (!boundedValue(header["height"], out.headerHeight, 24, 160, out.headerHeight) ||
        !boundedValue(header["topPadding"], out.topPadding, 0, 80, out.topPadding) ||
        !boundedValue(header["spacing"], out.verticalSpacing, 0, 80, out.verticalSpacing) ||
        !boundedValue(header["sidePadding"], out.contentSidePadding, 0, 120, out.contentSidePadding) ||
        !boundedValue(header["tabHeight"], out.tabBarHeight, 20, 120, out.tabBarHeight) ||
        !boundedValue(header["tabSpacing"], out.tabSpacing, 0, 80, out.tabSpacing) ||
        !boundedValue(list["rowHeight"], out.listRowHeight, 20, 120, out.listRowHeight) ||
        !boundedValue(list["subtitleRowHeight"], out.listSubtitleRowHeight, 30, 160,
                      out.listSubtitleRowHeight) ||
        !boundedValue(popup["top"], out.popupTopPermille, 0, 800, out.popupTopPermille) ||
        !boundedValue(popup["marginX"], out.popupMarginX, 4, 80, out.popupMarginX) ||
        !boundedValue(popup["marginY"], out.popupMarginY, 4, 80, out.popupMarginY) ||
        !boundedValue(popup["cornerRadius"], out.popupCornerRadius, 0, 80, out.popupCornerRadius) ||
        !boundedByte(popup["frameThickness"], out.popupFrameThickness, 1, 6, out.popupFrameThickness) ||
        !boundedByte(popup["progressHeight"], out.popupProgressHeight, 1, 20, out.popupProgressHeight) ||
        !boundedValue(input["keyWidth"], out.keyboardKeyWidth, 16, 80, out.keyboardKeyWidth) ||
        !boundedValue(input["keyHeight"], out.keyboardKeyHeight, 20, 80, out.keyboardKeyHeight) ||
        !boundedValue(input["keySpacing"], out.keyboardKeySpacing, 0, 24, out.keyboardKeySpacing) ||
        !boundedValue(input["cornerRadius"], out.keyboardCornerRadius, 0, 40, out.keyboardCornerRadius) ||
        !boundedValue(input["widthPercent"], out.keyboardWidthPercent, 40, 100, out.keyboardWidthPercent) ||
        !boundedValue(input["textFieldPadding"], out.textFieldPadding, 0, 30, out.textFieldPadding) ||
        !boundedByte(input["textFieldThickness"], out.textFieldThickness, 1, 6, out.textFieldThickness) ||
        !boundedByte(input["cursorThickness"], out.textFieldCursorThickness, 1, 8,
                     out.textFieldCursorThickness) ||
        !boundedValue(hints["height"], out.hintsHeight, 20, 100, out.hintsHeight) ||
        !boundedValue(hints["sideWidth"], out.sideHintsWidth, 16, 80, out.sideHintsWidth) ||
        !boundedValue(status["marginX"], out.statusMarginX, 0, 80, out.statusMarginX) ||
        !boundedValue(status["marginY"], out.statusMarginY, 0, 80, out.statusMarginY) ||
        !boundedValue(status["progressHeight"], out.progressBarHeight, 2, 40, out.progressBarHeight)) return false;
    out.popupTextBold = popup["bold"] | out.popupTextBold;
    out.popupTextInverted = popup["inverted"] | out.popupTextInverted;
    out.keyboardFillUnselected = input["fillUnselected"] | out.keyboardFillUnselected;
    out.keyboardOutlineUnselected = input["outlineUnselected"] | out.keyboardOutlineUnselected;
    const char* background = home["background"] | "";
    if (background[0] != '\0') {
      if (!safeAssetPath(background)) return false;
      const int assetPathWritten = std::snprintf(out.homeBackground, sizeof(out.homeBackground), "%s/%s/%s", kThemesDir,
                                                 directoryName, background);
      if (assetPathWritten < 0 || static_cast<size_t>(assetPathWritten) >= sizeof(out.homeBackground) ||
          !Storage.exists(out.homeBackground)) return false;
    }
    return true;
  }
  const char* base = doc["base"] | "";
  if (!baseThemeForName(base, out.baseTheme)) return false;
  const JsonObjectConst components = doc["components"].as<JsonObjectConst>();
  // Home controls the runtime layout metrics and interaction model. Other
  // component choices only change their renderer, keeping layout bounded.
  loadComponent(components, "home", out.baseTheme, out.baseTheme);
  loadComponent(components, "header", out.baseTheme, out.headerTheme);
  loadComponent(components, "list", out.baseTheme, out.listTheme);
  loadComponent(components, "menu", out.baseTheme, out.menuTheme);
  loadComponent(components, "popup", out.baseTheme, out.popupTheme);
  loadComponent(components, "input", out.baseTheme, out.inputTheme);
  loadComponent(components, "hints", out.baseTheme, out.hintsTheme);
  loadComponent(components, "status", out.baseTheme, out.statusTheme);
  return true;
}

void CustomThemeRegistry::discover() {
  if (themes_.capacity() == 0) themes_.reserve(kMaxThemes);  // <= 16 fixed summaries, allocated once.
  themes_.clear();

  HalFile root = Storage.open(kThemesDir);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  char directoryName[CustomThemeInfo::kIdCapacity];
  while (themes_.size() < kMaxThemes) {
    HalFile entry = root.openNextFile();
    if (!entry) break;
    const bool isDirectory = entry.isDirectory();
    const size_t nameLength = isDirectory ? entry.getName(directoryName, sizeof(directoryName)) : 0;
    entry.close();
    if (!isDirectory || nameLength == 0 || nameLength >= sizeof(directoryName)) continue;

    CustomThemeInfo theme;
    if (loadManifest(directoryName, theme)) themes_.push_back(theme);
  }
  root.close();

  std::sort(themes_.begin(), themes_.end(), [](const CustomThemeInfo& a, const CustomThemeInfo& b) {
    return std::strcmp(a.name, b.name) < 0;
  });
  LOG_DBG("THEME", "Discovered %d custom theme(s)", static_cast<int>(themes_.size()));
}
