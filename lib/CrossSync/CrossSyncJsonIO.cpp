#include "CrossSyncJsonIO.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include "CrossSyncCredentialStore.h"

namespace CrossSyncJsonIO {

bool save(const CrossSyncCredentialStore& store, const char* path) {
  JsonDocument doc;
  doc["enabled"] = store.isEnabled();
  doc["provider"] = "webdav";
  doc["username"] = store.getUsername();
  doc["password_obf"] = obfuscation::obfuscateToBase64(store.getPassword());
  doc["serverUrl"] = store.getServerUrl();
  doc["rootPath"] = store.getRootPath();

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool load(CrossSyncCredentialStore& store, const char* json, bool* needsResave) {
  if (needsResave) *needsResave = false;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json ? json : "");
  if (error) {
    LOG_ERR("CrossSync", "Config JSON parse error: %s", error.c_str());
    return false;
  }

  store.setEnabled(doc["enabled"] | false);
  store.setServerUrl(doc["serverUrl"] | std::string(""));
  store.setRootPath(doc["rootPath"] | std::string("/CrossSync"));

  const std::string user = doc["username"] | std::string("");
  obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", &status);
  if (status == obfuscation::DecodeStatus::LEGACY && !pass.empty() && needsResave) {
    *needsResave = true;
  }
  if (status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY || pass.empty()) {
    pass = doc["password"] | std::string("");
    if (!pass.empty() && needsResave) *needsResave = true;
  }
  if (status == obfuscation::DecodeStatus::INVALID && pass.empty()) {
    LOG_ERR("CrossSync", "Ignoring unreadable CrossSync password");
  }

  store.setCredentials(user, pass);
  return true;
}

}  // namespace CrossSyncJsonIO
