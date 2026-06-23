#include "BookStoreClient.h"

#include <I18n.h>
#include <Logging.h>

#include <esp_http_client.h>

#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
bool postForm(const char* url, const char* body, std::string& outResponse, const char* cookie = nullptr) {
  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 30000;
  config.buffer_size = 2048;
  config.buffer_size_tx = 1024;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return false;

  esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
  if (cookie && cookie[0] != '\0') {
    esp_http_client_set_header(client, "Cookie", cookie);
  }
  esp_http_client_set_post_field(client, body, static_cast<int>(std::strlen(body)));

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP POST failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  const int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    LOG_ERR("BOOKSTORE", "HTTP POST status %d", status);
    esp_http_client_cleanup(client);
    return false;
  }

  char buffer[512];
  int readLen = 0;
  while ((readLen = esp_http_client_read(client, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[readLen] = '\0';
    outResponse.append(buffer);
  }

  esp_http_client_cleanup(client);
  return !outResponse.empty();
}
}  // namespace

BookStoreClient::BookStoreClient() { lastErrorMessage[0] = '\0'; }

void BookStoreClient::setBaseUrl(const char* url) {
  if (!url) {
    baseUrl[0] = '\0';
    return;
  }
  // Ensure https:// prefix
  if (std::strncmp(url, "http://", 7) == 0 || std::strncmp(url, "https://", 8) == 0) {
    std::strncpy(baseUrl, url, sizeof(baseUrl) - 1);
  } else {
    std::snprintf(baseUrl, sizeof(baseUrl), "https://%s", url);
  }
  baseUrl[sizeof(baseUrl) - 1] = '\0';
  // Strip trailing slash
  const size_t len = std::strlen(baseUrl);
  if (len > 0 && baseUrl[len - 1] == '/') {
    baseUrl[len - 1] = '\0';
  }
}

void BookStoreClient::setCredentials(const char* e, const char* p) {
  if (e) {
    std::strncpy(email, e, sizeof(email) - 1);
    email[sizeof(email) - 1] = '\0';
  }
  if (p) {
    std::strncpy(password, p, sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
  }
}

bool BookStoreClient::buildUrl(char* out, size_t outLen, const char* path) const {
  if (baseUrl[0] == '\0' || !path) return false;
  const int n = std::snprintf(out, outLen, "%s%s", baseUrl, path);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool BookStoreClient::buildSearchBody(char* out, size_t outLen, const char* query, uint32_t page) const {
  if (!query) return false;
  const int n = std::snprintf(out, outLen, "message=%s&page=%lu&limit=5&order=bestmatch", query, page);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool BookStoreClient::isHtmlResponse(const char* contentType) const {
  if (!contentType) return false;
  return std::strstr(contentType, "text/html") != nullptr;
}

void BookStoreClient::setError(BookStoreError code, const char* msg) {
  (void)code;
  if (msg) {
    std::strncpy(lastErrorMessage, msg, sizeof(lastErrorMessage) - 1);
    lastErrorMessage[sizeof(lastErrorMessage) - 1] = '\0';
  } else {
    lastErrorMessage[0] = '\0';
  }
  LOG_ERR("BOOKSTORE", "%s", lastErrorMessage);
}

BookStoreError BookStoreClient::login() {
  userId[0] = '\0';
  userKey[0] = '\0';

  if (baseUrl[0] == '\0') {
    setError(BookStoreError::Network, "Base URL not configured");
    return BookStoreError::Network;
  }
  if (email[0] == '\0' || password[0] == '\0') {
    setError(BookStoreError::Auth, "Email or password not configured");
    return BookStoreError::Auth;
  }

  char url[192];
  if (!buildUrl(url, sizeof(url), "/eapi/user/login")) {
    setError(BookStoreError::Network, "Failed to build login URL");
    return BookStoreError::Network;
  }

  char body[256];
  const int bodyLen = std::snprintf(body, sizeof(body), "email=%s&password=%s", email, password);
  if (bodyLen <= 0 || static_cast<size_t>(bodyLen) >= sizeof(body)) {
    setError(BookStoreError::Auth, "Credentials too long");
    return BookStoreError::Auth;
  }

  std::string response;
  if (!postForm(url, body, response)) {
    setError(BookStoreError::Network, "Login request failed");
    return BookStoreError::Network;
  }

  return parseLoginResponse(response.c_str(), response.size());
}

BookStoreError BookStoreClient::parseLoginResponse(const char* json, size_t len) {
  // Minimal manual parse: look for "success":1 and extract user.id / remix_userkey
  if (!json || len == 0) {
    setError(BookStoreError::Parse, "Empty login response");
    return BookStoreError::Parse;
  }

  const std::string_view view(json, len);
  if (view.find("\"success\":1") == std::string_view::npos && view.find("\"success\": 1") == std::string_view::npos) {
    if (view.find("Incorrect email or password") != std::string_view::npos ||
        view.find("Please login") != std::string_view::npos) {
      setError(BookStoreError::Auth, "Incorrect email or password");
      return BookStoreError::Auth;
    }
    setError(BookStoreError::Auth, "Login rejected by server");
    return BookStoreError::Auth;
  }

  auto extract = [](std::string_view v, const char* key, char* out, size_t outLen) {
    const std::string pattern = std::string("\"") + key + "\":\"";
    size_t pos = v.find(pattern);
    if (pos == std::string_view::npos) {
      // Try unquoted numeric id
      const std::string altPattern = std::string("\"") + key + "\":";
      pos = v.find(altPattern);
      if (pos == std::string_view::npos) return false;
      pos += altPattern.size();
      const size_t end = v.find_first_of(",}", pos);
      const size_t len = end == std::string_view::npos ? outLen - 1 : std::min(outLen - 1, end - pos);
      std::strncpy(out, v.data() + pos, len);
      out[len] = '\0';
      return true;
    }
    pos += pattern.size();
    const size_t end = v.find('"', pos);
    if (end == std::string_view::npos) return false;
    const size_t len = std::min(outLen - 1, end - pos);
    std::strncpy(out, v.data() + pos, len);
    out[len] = '\0';
    return true;
  };

  if (!extract(view, "id", userId, sizeof(userId)) || !extract(view, "remix_userkey", userKey, sizeof(userKey))) {
    setError(BookStoreError::Parse, "Failed to parse login session");
    return BookStoreError::Parse;
  }

  return BookStoreError::Ok;
}
