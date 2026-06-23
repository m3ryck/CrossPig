#pragma once

#include <HttpDownloader.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct BookStoreBook {
  char id[32] = {0};
  char hash[32] = {0};
  char title[128] = {0};
  char author[128] = {0};
  char extension[8] = {0};
  char language[16] = {0};
  char filesizeString[16] = {0};
  uint32_t year = 0;
};

enum class BookStoreError {
  Ok,
  Network,
  Auth,
  Quota,
  NotFound,
  Parse,
  Server,
  Cancelled,
  File,
};

class BookStoreClient {
 public:
  using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

  BookStoreClient();

  void setBaseUrl(const char* url);
  void setCredentials(const char* email, const char* password);

  BookStoreError login();
  BookStoreError search(const char* query, uint32_t page, std::vector<BookStoreBook>& out);
  BookStoreError resolveDownloadUrl(const BookStoreBook& book, std::string& outUrl);
  BookStoreError downloadFile(const std::string& url, const std::string& destPath, ProgressCallback progress = nullptr,
                              bool* cancelFlag = nullptr);

  const char* getLastErrorMessage() const { return lastErrorMessage; }

 private:
  char baseUrl[128] = {0};
  char email[64] = {0};
  char password[64] = {0};
  char userId[32] = {0};
  char userKey[64] = {0};
  char lastErrorMessage[128] = {0};

  bool isLoggedIn() const { return userId[0] != '\0' && userKey[0] != '\0'; }

  BookStoreError parseLoginResponse(const char* json, size_t len);
  BookStoreError parseSearchResponse(const char* json, size_t len, std::vector<BookStoreBook>& out);
  BookStoreError parseDownloadLinkResponse(const char* json, size_t len, std::string& outUrl);

  void setError(BookStoreError code, const char* msg);
  bool buildUrl(char* out, size_t outLen, const char* path) const;
  bool buildSearchBody(char* out, size_t outLen, const char* query, uint32_t page) const;
  bool isHtmlResponse(const char* contentType) const;
};
