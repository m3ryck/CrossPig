#include "CrossSyncCredentialStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include "CrossSyncJsonIO.h"

CrossSyncCredentialStore CrossSyncCredentialStore::instance;

namespace {
constexpr char CROSSSYNC_FILE_JSON[] = "/.crosspoint/crosssync.json";
constexpr char DEFAULT_ROOT_PATH[] = "/CrossSync";

std::string normalizeBaseUrl(const std::string& raw) {
  std::string url = raw;
  if (!url.empty() && url.find("://") == std::string::npos) {
    url = "https://" + url;
  }
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }
  return url;
}

std::string normalizeRootPath(const std::string& raw) {
  std::string path = raw.empty() ? DEFAULT_ROOT_PATH : raw;
  if (path[0] != '/') {
    path.insert(path.begin(), '/');
  }
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();
  }
  return path;
}
}  // namespace

bool CrossSyncCredentialStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  return CrossSyncJsonIO::save(*this, CROSSSYNC_FILE_JSON);
}

bool CrossSyncCredentialStore::loadFromFile() {
  if (!Storage.exists(CROSSSYNC_FILE_JSON)) {
    LOG_DBG("CrossSync", "No CrossSync config file found");
    return false;
  }

  String json = Storage.readFile(CROSSSYNC_FILE_JSON);
  if (json.isEmpty()) {
    LOG_ERR("CrossSync", "CrossSync config file is empty");
    return false;
  }

  bool resave = false;
  const bool result = CrossSyncJsonIO::load(*this, json.c_str(), &resave);
  if (result && resave) {
    saveToFile();
  }
  return result;
}

void CrossSyncCredentialStore::setEnabled(const bool value) {
  enabled = value;
  LOG_DBG("CrossSync", "Set enabled: %s", enabled ? "true" : "false");
}

void CrossSyncCredentialStore::setCredentials(const std::string& user, const std::string& pass) {
  username = user;
  password = pass;
  LOG_DBG("CrossSync", "Set credentials for user: %s", username.c_str());
}

bool CrossSyncCredentialStore::hasCredentials() const { return !username.empty() && !password.empty(); }

void CrossSyncCredentialStore::setServerUrl(const std::string& url) {
  serverUrl = url;
  LOG_DBG("CrossSync", "Set WebDAV URL: %s", serverUrl.c_str());
}

std::string CrossSyncCredentialStore::getBaseUrl() const { return normalizeBaseUrl(serverUrl); }

void CrossSyncCredentialStore::setRootPath(const std::string& path) {
  rootPath = normalizeRootPath(path);
  LOG_DBG("CrossSync", "Set root path: %s", rootPath.c_str());
}

bool CrossSyncCredentialStore::hasConfig() const { return enabled && !getBaseUrl().empty() && hasCredentials(); }
