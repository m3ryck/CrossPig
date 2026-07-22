#include "ThemeInstaller.h"

#include <ArduinoJson.h>
#include <Bitmap.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "components/CustomThemeRegistry.h"

namespace {
constexpr char kStagingPrefix[] = "__install-";

void setError(char* out, size_t size, const char* message) {
  if (size == 0) return;
  std::snprintf(out, size, "%s", message);
}

bool stagingPath(const char* id, char* out, const size_t outSize) {
  const int written = std::snprintf(out, outSize, "%s/%s%s", CustomThemeRegistry::kThemesDir, kStagingPrefix, id);
  return written >= 0 && static_cast<size_t>(written) < outSize;
}

bool safeAssetName(const char* name) {
  if (!name || std::strncmp(name, "assets/", 7) != 0 || std::strstr(name, "..")) return false;
  const size_t n = std::strlen(name);
  if (n <= 7 || n >= 64 || std::strcmp(name + n - 4, ".bmp")) return false;
  for (const char* p = name; *p; ++p)
    if (!(std::isalnum(static_cast<unsigned char>(*p)) || *p == '/' || *p == '.' || *p == '_' || *p == '-')) return false;
  return true;
}

bool validateAssets(const char* root, size_t& total, char* error, const size_t errorSize) {
  char assetsPath[96];
  const int written = std::snprintf(assetsPath, sizeof(assetsPath), "%s/assets", root);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(assetsPath)) return false;
  if (!Storage.exists(assetsPath)) return true;

  HalFile assets = Storage.open(assetsPath);
  if (!assets || !assets.isDirectory()) {
    if (assets) assets.close();
    setError(error, errorSize, "Invalid assets directory");
    return false;
  }

  int count = 0;
  char name[65];
  while (HalFile entry = assets.openNextFile()) {
    const bool isDirectory = entry.isDirectory();
    const size_t size = isDirectory ? 0 : entry.fileSize();
    entry.getName(name, sizeof(name));
    entry.close();
    char relative[73];
    std::snprintf(relative, sizeof(relative), "assets/%s", name);
    if (isDirectory || !safeAssetName(relative) || ++count > 16 || size == 0 ||
        (total += size) > ThemeInstaller::kMaxPackageBytes) {
      assets.close();
      setError(error, errorSize, "Invalid package asset");
      return false;
    }
    char assetPath[144];
    std::snprintf(assetPath, sizeof(assetPath), "%s/%s", root, relative);
    HalFile asset;
    if (!Storage.openFileForRead("THEME", assetPath, asset)) {
      assets.close();
      setError(error, errorSize, "Could not read package asset");
      return false;
    }
    Bitmap bitmap(asset);
    const bool validBitmap = bitmap.parseHeaders() == BmpReaderError::Ok && bitmap.is1Bit();
    asset.close();
    if (!validBitmap) {
      assets.close();
      setError(error, errorSize, "Assets must be 1-bit BMP files");
      return false;
    }
  }
  assets.close();
  return true;
}

bool validateStagingEntries(const char* root, char* error, const size_t errorSize) {
  HalFile directory = Storage.open(root);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    setError(error, errorSize, "Invalid package directory");
    return false;
  }
  bool hasManifest = false;
  char name[65];
  while (HalFile entry = directory.openNextFile()) {
    const bool isDirectory = entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    const bool manifest = !isDirectory && std::strcmp(name, "theme.json") == 0;
    const bool assets = isDirectory && std::strcmp(name, "assets") == 0;
    if (!manifest && !assets) {
      directory.close();
      setError(error, errorSize, "Package contains unsupported files");
      return false;
    }
    if (manifest) hasManifest = true;
  }
  directory.close();
  if (!hasManifest) {
    setError(error, errorSize, "Missing manifest");
    return false;
  }
  return true;
}
}  // namespace

bool ThemeInstaller::isSafeThemeId(const char* id) {
  if (!id || !*id || std::strlen(id) >= CustomThemeInfo::kIdCapacity) return false;
  for (const char* p = id; *p; ++p)
    if (!(std::islower(static_cast<unsigned char>(*p)) || std::isdigit(static_cast<unsigned char>(*p)) || *p == '-')) return false;
  return true;
}

bool ThemeInstaller::prepareStaging(const char* id, char* error, size_t errorSize) {
  if (!isSafeThemeId(id)) {
    setError(error, errorSize, "Invalid theme ID");
    return false;
  }
  char root[80];
  if (!stagingPath(id, root, sizeof(root))) {
    setError(error, errorSize, "Theme path is too long");
    return false;
  }
  if (!Storage.ensureDirectoryExists(CustomThemeRegistry::kThemesDir)) {
    LOG_ERR("THEME", "Could not create %s", CustomThemeRegistry::kThemesDir);
    setError(error, errorSize, "Could not create themes directory");
    return false;
  }
  if (Storage.exists(root) && !Storage.removeDir(root)) {
    LOG_ERR("THEME", "Could not clear staging directory %s", root);
    setError(error, errorSize, "Could not clear previous upload");
    return false;
  }
  char assets[96];
  std::snprintf(assets, sizeof(assets), "%s/assets", root);
  if (!Storage.mkdir(root) || !Storage.mkdir(assets)) {
    LOG_ERR("THEME", "Could not create staging directory %s", root);
    Storage.removeDir(root);
    setError(error, errorSize, "Could not prepare upload");
    return false;
  }
  setError(error, errorSize, "");
  return true;
}

bool ThemeInstaller::validateStaging(const char* id, char* error, size_t errorSize) {
  if (!isSafeThemeId(id)) { setError(error, errorSize, "Invalid theme ID"); return false; }
  char root[80], manifest[96];
  if (!stagingPath(id, root, sizeof(root))) { setError(error, errorSize, "Theme path is too long"); return false; }
  std::snprintf(manifest, sizeof(manifest), "%s/theme.json", root);
  if (!validateStagingEntries(root, error, errorSize)) return false;
  HalFile file;
  if (!Storage.openFileForRead("THEME", manifest, file) || file.fileSize() > 16 * 1024) { setError(error, errorSize, "Missing manifest"); return false; }
  const size_t manifestSize = file.fileSize();
  JsonDocument doc;
  const auto parse = deserializeJson(doc, file); file.close();
  const int schemaVersion = doc["schemaVersion"] | 0;
  if (parse || (schemaVersion != 2 && schemaVersion != 3 && schemaVersion != 4) ||
      std::strcmp(doc["engine"] | "", "declarative") != 0 ||
      std::strcmp(doc["id"] | "", id) != 0) { setError(error, errorSize, "Invalid declarative manifest"); return false; }
  size_t total = manifestSize;
  if (!validateAssets(root, total, error, errorSize)) return false;
  const char* bg = doc["home"]["background"] | "";
  if (bg[0]) {
    if (!safeAssetName(bg)) { setError(error, errorSize, "Unsafe asset path"); return false; }
    char asset[144]; std::snprintf(asset, sizeof(asset), "%s/%s", root, bg);
    if (!Storage.exists(asset)) { setError(error, errorSize, "Missing background asset"); return false; }
  }
  setError(error, errorSize, ""); return true;
}

bool ThemeInstaller::publish(const char* id, bool replace, char* error, size_t errorSize) {
  if (!validateStaging(id, error, errorSize)) return false;
  char staging[80], target[80], backup[88];
  if (!stagingPath(id, staging, sizeof(staging))) { setError(error, errorSize, "Theme path is too long"); return false; }
  std::snprintf(target, sizeof(target), "%s/%s", CustomThemeRegistry::kThemesDir, id);
  std::snprintf(backup, sizeof(backup), "%s/.backup-%s", CustomThemeRegistry::kThemesDir, id);
  const bool exists = Storage.exists(target);
  if (exists && !replace) { setError(error, errorSize, "Theme already exists"); return false; }
  if (Storage.exists(backup)) Storage.removeDir(backup);
  if (exists && !Storage.rename(target, backup)) { setError(error, errorSize, "Could not back up existing theme"); return false; }
  if (!Storage.rename(staging, target)) {
    if (exists) Storage.rename(backup, target);
    setError(error, errorSize, "Could not publish theme"); return false;
  }
  CUSTOM_THEMES.discover();
  if (!CUSTOM_THEMES.find(id)) {
    LOG_ERR("THEME", "Published package did not pass registry validation: %s", id);
    Storage.removeDir(target);
    if (exists) Storage.rename(backup, target);
    CUSTOM_THEMES.discover();
    setError(error, errorSize, "Manifest is incompatible with this firmware");
    return false;
  }
  if (exists) Storage.removeDir(backup);
  setError(error, errorSize, ""); return true;
}

bool ThemeInstaller::remove(const char* id, char* error, size_t errorSize) {
  if (!isSafeThemeId(id)) { setError(error, errorSize, "Invalid theme ID"); return false; }
  char target[80]; std::snprintf(target, sizeof(target), "%s/%s", CustomThemeRegistry::kThemesDir, id);
  if (!Storage.exists(target) || !Storage.removeDir(target)) { setError(error, errorSize, "Delete failed"); return false; }
  if (SETTINGS.uiTheme == CrossPointSettings::CUSTOM_THEME && std::strcmp(SETTINGS.customThemeId, id) == 0) {
    SETTINGS.uiTheme = CrossPointSettings::LYRA; SETTINGS.customThemeId[0] = '\0'; SETTINGS.saveToFile();
  }
  CUSTOM_THEMES.discover(); setError(error, errorSize, ""); return true;
}
