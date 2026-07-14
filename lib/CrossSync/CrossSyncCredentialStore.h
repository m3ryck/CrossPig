#pragma once

#include <string>

class CrossSyncCredentialStore {
 private:
  static CrossSyncCredentialStore instance;

  bool enabled = false;
  std::string username;
  std::string password;
  std::string serverUrl;
  std::string rootPath = "/CrossSync";

  CrossSyncCredentialStore() = default;

 public:
  CrossSyncCredentialStore(const CrossSyncCredentialStore&) = delete;
  CrossSyncCredentialStore& operator=(const CrossSyncCredentialStore&) = delete;

  static CrossSyncCredentialStore& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();

  void setEnabled(bool value);
  bool isEnabled() const { return enabled; }

  void setCredentials(const std::string& user, const std::string& pass);
  const std::string& getUsername() const { return username; }
  const std::string& getPassword() const { return password; }
  bool hasCredentials() const;

  void setServerUrl(const std::string& url);
  const std::string& getServerUrl() const { return serverUrl; }
  std::string getBaseUrl() const;

  void setRootPath(const std::string& path);
  const std::string& getRootPath() const { return rootPath; }

  bool hasConfig() const;
};

#define CROSSSYNC_STORE CrossSyncCredentialStore::getInstance()
