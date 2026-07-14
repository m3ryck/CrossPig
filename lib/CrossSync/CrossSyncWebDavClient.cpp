#include "CrossSyncWebDavClient.h"

#include <HTTPClient.h>
#include <Logging.h>
#ifdef SIMULATOR
#include <WiFi.h>
#include <WiFiClientSecure.h>
#else
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>

#include "CrossSyncCredentialStore.h"

int CrossSyncWebDavClient::lastHttpCode = 0;
int CrossSyncWebDavClient::lastTransportError = 0;

namespace {
constexpr uint32_t MIN_HEAP_FOR_TLS = 55000;
constexpr size_t MAX_HIGHLIGHT_JSON_BYTES = 48 * 1024;

bool isHttpsUrl(const std::string& url) { return url.rfind("https://", 0) == 0; }

std::string encodePath(const std::string& path) {
  std::string out;
  out.reserve(path.size());
  char encoded[4];
  for (const unsigned char c : path) {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      snprintf(encoded, sizeof(encoded), "%%%02X", c);
      out += encoded;
    }
  }
  return out;
}

std::string joinUrl(const std::string& remotePath) {
  std::string base = CROSSSYNC_STORE.getBaseUrl();
  std::string path = remotePath.empty() ? "/" : remotePath;
  if (path[0] != '/') {
    path.insert(path.begin(), '/');
  }
  return base + encodePath(path);
}

CrossSyncWebDavClient::Error classifyStatus(const int code) {
  if (code >= 200 && code < 300) return CrossSyncWebDavClient::Error::OK;
  if (code == 401 || code == 403) return CrossSyncWebDavClient::Error::AuthFailed;
  if (code == 404) return CrossSyncWebDavClient::Error::NotFound;
  if (code < 0) return CrossSyncWebDavClient::Error::NetworkError;
  return CrossSyncWebDavClient::Error::ServerError;
}

void recordHttpResult(const char* method, const std::string& url, const int code) {
  CrossSyncWebDavClient::lastHttpCode = code > 0 ? code : 0;
  CrossSyncWebDavClient::lastTransportError = code < 0 ? code : 0;

  if (code >= 200 && code < 300) {
    LOG_DBG("CrossSync", "%s %s -> HTTP %d", method, url.c_str(), code);
    return;
  }

  if (code < 0) {
    LOG_ERR("CrossSync", "%s %s transport error %d (%s), WiFi=%d ip=%s", method, url.c_str(), code,
            HTTPClient::errorToString(code).c_str(), static_cast<int>(WiFi.status()), WiFi.localIP().toString().c_str());
    return;
  }

  LOG_ERR("CrossSync", "%s %s -> HTTP %d, WiFi=%d ip=%s", method, url.c_str(), code,
          static_cast<int>(WiFi.status()), WiFi.localIP().toString().c_str());
}

bool beginHttp(HTTPClient& http, WiFiClient& plainClient, std::unique_ptr<WiFiClientSecure>& secureClient,
               const std::string& url) {
  if (isHttpsUrl(url)) {
    const uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < MIN_HEAP_FOR_TLS) {
      LOG_ERR("CrossSync", "Insufficient heap for WebDAV TLS: %u", static_cast<unsigned>(freeHeap));
      return false;
    }
    secureClient.reset(new (std::nothrow) WiFiClientSecure);
    if (!secureClient) {
      LOG_ERR("CrossSync", "Failed to allocate secure client");
      return false;
    }
    secureClient->setInsecure();
    http.begin(*secureClient, url.c_str());
    return true;
  }
  http.begin(plainClient, url.c_str());
  return true;
}

void addAuth(HTTPClient& http) {
  http.setTimeout(15000);
  http.setAuthorization(CROSSSYNC_STORE.getUsername().c_str(), CROSSSYNC_STORE.getPassword().c_str());
}

CrossSyncWebDavClient::Error requestNoBody(const char* method, const std::string& remotePath) {
  if (!CROSSSYNC_STORE.hasConfig()) return CrossSyncWebDavClient::Error::NotConfigured;

#ifdef SIMULATOR
  (void)method;
  (void)remotePath;
  return CrossSyncWebDavClient::Error::OK;
#else
  const std::string url = joinUrl(remotePath);
  HTTPClient http;
  WiFiClient plainClient;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!beginHttp(http, plainClient, secureClient, url)) return CrossSyncWebDavClient::Error::LowMemory;
  addAuth(http);

  const int code = http.sendRequest(method);
  http.end();
  recordHttpResult(method, url, code);
  if (strcmp(method, "MKCOL") == 0 && (code == 301 || code == 302 || code == 307 || code == 308 || code == 405)) {
    return CrossSyncWebDavClient::Error::OK;
  }
  return classifyStatus(code);
#endif
}
}  // namespace

CrossSyncWebDavClient::Error CrossSyncWebDavClient::ensureCollectionPath(const std::string& path) {
  if (path.empty() || path == "/") return Error::OK;

  std::string normalized = path;
  if (normalized[0] != '/') normalized.insert(normalized.begin(), '/');
  while (normalized.size() > 1 && normalized.back() == '/') normalized.pop_back();

  size_t pos = 1;
  while (pos != std::string::npos) {
    pos = normalized.find('/', pos);
    const std::string current = normalized.substr(0, pos);
    Error result = requestNoBody("MKCOL", current);
    if (result != Error::OK) {
      return result;
    }
    if (pos != std::string::npos) ++pos;
  }

  return Error::OK;
}

CrossSyncWebDavClient::Error CrossSyncWebDavClient::getTextFile(const std::string& remotePath, std::string& out) {
  if (!CROSSSYNC_STORE.hasConfig()) return Error::NotConfigured;

  const std::string url = joinUrl(remotePath);
  HTTPClient http;
  WiFiClient plainClient;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!beginHttp(http, plainClient, secureClient, url)) return Error::LowMemory;
  addAuth(http);

  const int code = http.GET();
  const Error status = classifyStatus(code);
  if (status == Error::OK) {
    const int size = http.getSize();
    if (size > static_cast<int>(MAX_HIGHLIGHT_JSON_BYTES)) {
      LOG_ERR("CrossSync", "Remote highlight JSON too large: %d", size);
      http.end();
      return Error::ServerError;
    }
    String body = http.getString();
    if (body.length() > MAX_HIGHLIGHT_JSON_BYTES) {
      LOG_ERR("CrossSync", "Remote highlight JSON exceeded limit after read: %u", body.length());
      http.end();
      return Error::ServerError;
    }
    out.assign(body.c_str(), body.length());
  }
  http.end();
  recordHttpResult("GET", url, code);
  return status;
}

CrossSyncWebDavClient::Error CrossSyncWebDavClient::putTextFile(const std::string& remotePath, const std::string& body) {
  if (!CROSSSYNC_STORE.hasConfig()) return Error::NotConfigured;
  if (body.size() > MAX_HIGHLIGHT_JSON_BYTES) {
    LOG_ERR("CrossSync", "Local highlight JSON too large: %zu", body.size());
    return Error::ServerError;
  }

  const std::string url = joinUrl(remotePath);
  HTTPClient http;
  WiFiClient plainClient;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!beginHttp(http, plainClient, secureClient, url)) return Error::LowMemory;
  addAuth(http);
  http.addHeader("Content-Type", "application/json");

#ifdef SIMULATOR
  const int code = http.PUT(String(body.c_str()));
#else
  const int code = http.PUT(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())), body.size());
#endif
  http.end();
  recordHttpResult("PUT", url, code);
  return classifyStatus(code);
}

const char* CrossSyncWebDavClient::errorString(const Error error) {
  switch (error) {
    case Error::OK:
      return "OK";
    case Error::NotConfigured:
      return "CrossSync is not configured";
    case Error::NotFound:
      return "Remote CrossSync file not found";
    case Error::NetworkError:
      return "CrossSync network error";
    case Error::AuthFailed:
      return "CrossSync credentials were rejected";
    case Error::ServerError:
      return "CrossSync server error";
    case Error::LowMemory:
      return "Not enough memory for CrossSync";
  }
  return "CrossSync error";
}

std::string CrossSyncWebDavClient::lastErrorDetail() {
  if (lastTransportError != 0) {
    return std::string("transport ") + std::to_string(lastTransportError) + " (" +
           HTTPClient::errorToString(lastTransportError).c_str() + ")";
  }
  if (lastHttpCode != 0) {
    return std::string("HTTP ") + std::to_string(lastHttpCode);
  }
  return "";
}
