#pragma once

#include <StreamingJsonParser.h>

#include "network/HttpDownloader.h"

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

class BookStoreSearchParser final {
 public:
  static constexpr size_t MAX_RESULTS = 5;

  BookStoreSearchParser();

  void reset(std::vector<BookStoreBook>& output);
  void resetDownload(std::string& outputUrl);
  bool feed(const char* data, size_t len);
  bool hasError() const { return parser.hasError(); }
  bool hasAuthenticationError() const { return authenticationError; }
  bool hasQuotaError() const { return quotaError; }
  bool sawBooks() const { return booksArraySeen; }
  bool sawDownloadLink() const { return downloadLinkSeen; }

 private:
  enum class LastKey : uint8_t {
    None,
    Success,
    Books,
    Error,
    Message,
    Id,
    Hash,
    Title,
    Author,
    Extension,
    Language,
    FilesizeString,
    Year,
    DownloadLink,
  };

  static void onKey(void* ctx, const char* key, size_t len);
  static void onString(void* ctx, const char* value, size_t len);
  static void onNumber(void* ctx, const char* value, size_t len);
  static void onBool(void* ctx, bool value);
  static void onNull(void* ctx);
  static void onObjectStart(void* ctx);
  static void onObjectEnd(void* ctx);
  static void onArrayStart(void* ctx);
  static void onArrayEnd(void* ctx);

  void copyValue(char* dest, size_t destLen, const char* value, size_t valueLen);
  void applyString(const char* value, size_t len);
  void commitBook();

  StreamingJsonParser parser;
  std::vector<BookStoreBook>* books = nullptr;
  std::string* downloadUrl = nullptr;
  BookStoreBook currentBook{};
  LastKey lastKey = LastKey::None;
  uint8_t depth = 0;
  uint8_t booksArrayDepth = 0;
  bool insideBooksArray = false;
  bool insideBook = false;
  bool booksArraySeen = false;
  bool downloadLinkSeen = false;
  bool authenticationError = false;
  bool quotaError = false;
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
  ~BookStoreClient();
  BookStoreClient(const BookStoreClient&) = delete;
  BookStoreClient& operator=(const BookStoreClient&) = delete;

  void setBaseUrl(const char* url);
  void setCredentials(const char* email, const char* password);

  BookStoreError login();
  BookStoreError search(const char* query, uint32_t page, std::vector<BookStoreBook>& out);
  BookStoreError resolveDownloadUrl(const BookStoreBook& book, std::string& outUrl);
  BookStoreError downloadFile(const std::string& url, const std::string& destPath, ProgressCallback progress = nullptr,
                              const bool* cancelFlag = nullptr);

  const char* getLastErrorMessage() const { return lastErrorMessage; }

 private:
  char baseUrl[128] = {0};
  char email[64] = {0};
  char password[64] = {0};
  char userId[32] = {0};
  char userKey[64] = {0};
  char lastErrorMessage[128] = {0};

  // One ESP-IDF HTTP session is retained for the activity lifetime so EAPI
  // requests can reuse their TLS connection instead of fragmenting the heap
  // with a new handshake for every operation.
  void* httpClient = nullptr;

  // Allocated with the client once and reused between requests. This avoids a
  // growing response string during TLS and keeps small EAPI responses bounded.
  char responseBuffer[1024] = {0};
  size_t responseLength = 0;
  BookStoreSearchParser searchParser;

  bool isLoggedIn() const { return userId[0] != '\0' && userKey[0] != '\0'; }

  BookStoreError parseLoginResponse(const char* json, size_t len);

  void setError(BookStoreError code, const char* msg);
  bool buildUrl(char* out, size_t outLen, const char* path) const;
  bool buildSearchBody(char* out, size_t outLen, const char* query, uint32_t page) const;
};
