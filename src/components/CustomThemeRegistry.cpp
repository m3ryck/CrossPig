#include "CustomThemeRegistry.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
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
  if ((doc["schemaVersion"] | 0) != 1) {
    LOG_ERR("THEME", "Unsupported schema in %s", manifestPath);
    return false;
  }

  const char* id = doc["id"] | "";
  const char* name = doc["name"] | "";
  const char* base = doc["base"] | "";
  if (!isSafeThemeId(id) || std::strcmp(id, directoryName) != 0 || name[0] == '\0' ||
      std::strlen(name) >= CustomThemeInfo::kNameCapacity || !baseThemeForName(base, out.baseTheme)) {
    LOG_ERR("THEME", "Invalid metadata in %s", manifestPath);
    return false;
  }

  std::strncpy(out.id, id, sizeof(out.id) - 1);
  std::strncpy(out.name, name, sizeof(out.name) - 1);
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
