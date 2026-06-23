#include "BookStoreClient.h"

#include <I18n.h>
#include <Logging.h>

#include <cstring>
#include <cstdio>

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
  if (msg) {
    std::strncpy(lastErrorMessage, msg, sizeof(lastErrorMessage) - 1);
    lastErrorMessage[sizeof(lastErrorMessage) - 1] = '\0';
  } else {
    lastErrorMessage[0] = '\0';
  }
  LOG_ERR("BOOKSTORE", "%s", lastErrorMessage);
}
