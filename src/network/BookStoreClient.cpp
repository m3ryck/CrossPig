#include "BookStoreClient.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <esp_http_client.h>

#include <cstdio>
#include <cstring>
#include <string_view>

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

#include "network/WifiPowerSaveGuard.h"

namespace {

// Let's Encrypt R13 intermediate — the CA that directly signed z-lib.fm's
// certificate.  We use this as the trust anchor instead of the ISRG Root X1
// (RSA 4096-bit) because the ESP32-C3 has no hardware RSA accelerator and
// 4096-bit RSA operations may exhaust the ~380 KB heap during the TLS
// handshake.  R13 uses a 2048-bit key which fits comfortably.
// Additionally, using an explicit PEM avoids the esp-x509-crt-bundle code
// which calls mbedtls_pk_verify_ext() for every intermediate cert and has
// issues with certain RSA signature schemes.
static constexpr const char* R13_PEM = R"(
-----BEGIN CERTIFICATE-----
MIIFBTCCAu2gAwIBAgIQWgDyEtjUtIDzkkFX6imDBTANBgkqhkiG9w0BAQsFADBP
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy
Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa
Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF
bmNyeXB0MQwwCgYDVQQDEwNSMTMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK
AoIBAQClZ3CN0FaBZBUXYc25BtStGZCMJlA3mBZjklTb2cyEBZPs0+wIG6BgUUNI
fSvHSJaetC3ancgnO1ehn6vw1g7UDjDKb5ux0daknTI+WE41b0VYaHEX/D7YXYKg
L7JRbLAaXbhZzjVlyIuhrxA3/+OcXcJJFzT/jCuLjfC8cSyTDB0FxLrHzarJXnzR
yQH3nAP2/Apd9Np75tt2QnDr9E0i2gB3b9bJXxf92nUupVcM9upctuBzpWjPoXTi
dYJ+EJ/B9aLrAek4sQpEzNPCifVJNYIKNLMc6YjCR06CDgo28EdPivEpBHXazeGa
XP9enZiVuppD0EqiFwUBBDDTMrOPAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG
MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/
AgEAMB0GA1UdDgQWBBTnq58PLDOgU9NeT3jIsoQOO9aSMzAfBgNVHSMEGDAWgBR5
tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG
Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD
VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B
AQsFAAOCAgEAUTdYUqEimzW7TbrOypLqCfL7VOwYf/Q79OH5cHLCZeggfQhDconl
k7Kgh8b0vi+/XuWu7CN8n/UPeg1vo3G+taXirrytthQinAHGwc/UdbOygJa9zuBc
VyqoH3CXTXDInT+8a+c3aEVMJ2St+pSn4ed+WkDp8ijsijvEyFwE47hulW0Ltzjg
9fOV5Pmrg/zxWbRuL+k0DBDHEJennCsAen7c35Pmx7jpmJ/HtgRhcnz0yjSBvyIw
6L1QIupkCv2SBODT/xDD3gfQQyKv6roV4G2EhfEyAsWpmojxjCUCGiyg97FvDtm/
NK2LSc9lybKxB73I2+P2G3CaWpvvpAiHCVu30jW8GCxKdfhsXtnIy2imskQqVZ2m
0Pmxobb28Tucr7xBK7CtwvPrb79os7u2XP3O5f9b/H66GNyRrglRXlrYjI1oGYL/
f4I1n/Sgusda6WvA6C190kxjU15Y12mHU4+BxyR9cx2hhGS9fAjMZKJss28qxvz6
Axu4CaDmRNZpK/pQrXF17yXCXkmEWgvSOEZy6Z9pcbLIVEGckV/iVeq0AOo2pkg9
p4QRIy0tK2diRENLSF2KysFwbY6B26BFeFs3v1sYVRhFW9nLkOrQVporCS0KyZmf
wVD89qSTlnctLcZnIavjKsKUu1nA1iU0yYMdYepKR7lWbnwhdx3ewok=
-----END CERTIFICATE-----
)";

// Percent-encode a value for use in an application/x-www-form-urlencoded body.
// RFC 3986 unreserved chars (A-Z a-z 0-9 - _ . ~) are left as-is; everything
// else is emitted as %XX.  Returns false if the output buffer would overflow.
bool urlEncode(const char* in, char* out, size_t outLen) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  size_t i = 0;
  for (const char* p = in; *p != '\0'; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                            c == '.' || c == '~';
    if (unreserved) {
      if (i + 1 >= outLen) return false;
      out[i++] = static_cast<char>(c);
    } else {
      if (i + 3 >= outLen) return false;
      out[i++] = '%';
      out[i++] = kHex[(c >> 4) & 0x0F];
      out[i++] = kHex[c & 0x0F];
    }
  }
  out[i] = '\0';
  return true;
}

// Shared helper: set common request headers (auth, user-agent, optional cookie).
static void setCommonHeaders(esp_http_client_handle_t client, const char* cookie,
                             const char* userIdHeader, const char* userKeyHeader) {
  esp_http_client_set_header(client, "User-Agent", "CrossInk-ESP32");
  // Z-Library eapi uses remix-userid / remix-userkey as HTTP headers (with hyphen).
  // Sending as cookies (with underscore) is kept as a fallback for older server configs.
  if (userIdHeader && userIdHeader[0] != '\0') {
    esp_http_client_set_header(client, "remix-userid", userIdHeader);
  }
  if (userKeyHeader && userKeyHeader[0] != '\0') {
    esp_http_client_set_header(client, "remix-userkey", userKeyHeader);
  }
  if (cookie && cookie[0] != '\0') {
    esp_http_client_set_header(client, "Cookie", cookie);
  }
}

// Shared response-reading helper used by both postForm and getRequest.
static bool readBufferedResponse(esp_http_client_handle_t client, std::string& outResponse,
                                 const char* logTag) {
  const int64_t contentLen = esp_http_client_fetch_headers(client);
  const int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    LOG_ERR(logTag, "HTTP status %d", status);
    return false;
  }
  // contentLen == -1 means Transfer-Encoding: chunked — valid, just unknown size.
  // Cap buffered responses at 64 KiB to protect the heap.
  if (contentLen > 65536) {
    LOG_ERR(logTag, "HTTP response too large: %lld", contentLen);
    return false;
  }

  // Pre-reserve to avoid repeated reallocations that fragment the heap.
  const size_t reserveHint = (contentLen > 0 && contentLen <= 65536)
                                 ? static_cast<size_t>(contentLen)
                                 : 2048;
  outResponse.reserve(reserveHint);

  char buffer[512];
  int readLen = 0;
  while ((readLen = esp_http_client_read(client, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[readLen] = '\0';
    outResponse.append(buffer);
    if (outResponse.size() > 65536) {
      LOG_ERR(logTag, "HTTP response exceeded buffer limit");
      outResponse.clear();
      return false;
    }
  }
  return !outResponse.empty();
}

bool postForm(const char* url, const char* body, std::string& outResponse, const char* cookie = nullptr,
              const char* userIdHeader = nullptr, const char* userKeyHeader = nullptr) {
  WifiPowerSaveGuard psGuard;

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 30000;
  config.buffer_size = 1024;
  config.buffer_size_tx = 1024;
  config.cert_pem = R13_PEM;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return false;

  esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
  setCommonHeaders(client, cookie, userIdHeader, userKeyHeader);
  esp_http_client_set_post_field(client, body, static_cast<int>(std::strlen(body)));

#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "POST heap: free=%u maxalloc=%u",
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  esp_err_t err = esp_http_client_open(client, static_cast<int>(std::strlen(body)));
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP POST open failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  const bool ok = readBufferedResponse(client, outResponse, "BOOKSTORE");
  esp_http_client_cleanup(client);
  return ok;
}

// GET request — used for endpoints that return JSON via HTTP GET (e.g. /eapi/book/.../file).
bool getRequest(const char* url, std::string& outResponse, const char* cookie = nullptr,
                const char* userIdHeader = nullptr, const char* userKeyHeader = nullptr) {
  WifiPowerSaveGuard psGuard;

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = 30000;
  config.buffer_size = 1024;
  config.buffer_size_tx = 512;
  config.cert_pem = R13_PEM;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return false;

  setCommonHeaders(client, cookie, userIdHeader, userKeyHeader);

#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "GET heap: free=%u maxalloc=%u",
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  esp_err_t err = esp_http_client_open(client, 0);  // 0 = no request body
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP GET open failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  const bool ok = readBufferedResponse(client, outResponse, "BOOKSTORE");
  esp_http_client_cleanup(client);
  return ok;
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
  // Percent-encode the query so special characters (spaces, &, +, etc.) are
  // transmitted correctly in the application/x-www-form-urlencoded body.
  char encodedQuery[384];
  if (!urlEncode(query, encodedQuery, sizeof(encodedQuery))) {
    return false;
  }
  const int n = std::snprintf(out, outLen, "message=%s&page=%u&limit=5&order=bestmatch", encodedQuery, page);
  return n > 0 && static_cast<size_t>(n) < outLen;
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

  // Percent-encode credentials in an inner scope so the encoding buffers are
  // released before postForm() — reducing peak stack usage for the background
  // network task (4 KiB stack).
  char body[448];
  {
    char encodedEmail[192];
    char encodedPassword[192];
    if (!urlEncode(email, encodedEmail, sizeof(encodedEmail)) ||
        !urlEncode(password, encodedPassword, sizeof(encodedPassword))) {
      setError(BookStoreError::Auth, "Credentials too long to encode");
      return BookStoreError::Auth;
    }
    const int bodyLen =
        std::snprintf(body, sizeof(body), "email=%s&password=%s", encodedEmail, encodedPassword);
    if (bodyLen <= 0 || static_cast<size_t>(bodyLen) >= sizeof(body)) {
      setError(BookStoreError::Auth, "Credentials too long");
      return BookStoreError::Auth;
    }
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

BookStoreError BookStoreClient::search(const char* query, uint32_t page, std::vector<BookStoreBook>& out) {
  out.clear();

  if (!isLoggedIn()) {
    const BookStoreError err = login();
    if (err != BookStoreError::Ok) return err;
  }

  char url[256];
  if (!buildUrl(url, sizeof(url), "/eapi/book/search")) {
    setError(BookStoreError::Network, "Failed to build search URL");
    return BookStoreError::Network;
  }

  char body[384];
  if (!buildSearchBody(body, sizeof(body), query, page)) {
    setError(BookStoreError::Network, "Failed to build search body");
    return BookStoreError::Network;
  }

  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

  std::string response;
  if (!postForm(url, body, response, cookie, userId, userKey)) {
    setError(BookStoreError::Network, "Search request failed");
    return BookStoreError::Network;
  }

  if (response.size() > 16384) {
    setError(BookStoreError::Network, "Search response too large");
    return BookStoreError::Network;
  }

  return parseSearchResponse(response.c_str(), response.size(), out);
}

BookStoreError BookStoreClient::parseSearchResponse(const char* json, size_t len, std::vector<BookStoreBook>& out) {
  if (!json || len == 0) {
    setError(BookStoreError::Parse, "Empty search response");
    return BookStoreError::Parse;
  }

  const std::string_view view(json, len);
  if (view.find("\"success\":1") == std::string_view::npos && view.find("\"success\": 1") == std::string_view::npos) {
    if (view.find("Please login") != std::string_view::npos) {
      userId[0] = '\0';
      userKey[0] = '\0';
      setError(BookStoreError::Auth, "Session expired");
      return BookStoreError::Auth;
    }
    setError(BookStoreError::Server, "Search request rejected");
    return BookStoreError::Server;
  }

  auto extractString = [](std::string_view v, const char* key, char* out, size_t outLen) {
    const std::string pattern = std::string("\"") + key + "\":\"";
    size_t pos = v.find(pattern);
    if (pos == std::string_view::npos) {
      out[0] = '\0';
      return;
    }
    pos += pattern.size();
    const size_t end = v.find('"', pos);
    if (end == std::string_view::npos) {
      out[0] = '\0';
      return;
    }
    const size_t copyLen = std::min(outLen - 1, end - pos);
    std::strncpy(out, v.data() + pos, copyLen);
    out[copyLen] = '\0';
  };

  auto extractUint = [](std::string_view v, const char* key) -> uint32_t {
    const std::string pattern = std::string("\"") + key + "\":";
    size_t pos = v.find(pattern);
    if (pos == std::string_view::npos) return 0;
    pos += pattern.size();
    // skip whitespace and quotes
    while (pos < v.size() && (v[pos] == ' ' || v[pos] == '"')) pos++;
    uint32_t value = 0;
    while (pos < v.size() && v[pos] >= '0' && v[pos] <= '9') {
      value = value * 10 + (v[pos] - '0');
      pos++;
    }
    return value;
  };

  // Find the books array. A simple approach: split the response by occurrences of {"id":
  out.reserve(5);
  size_t pos = 0;
  while (out.size() < 5) {
    pos = view.find("{\"id\":", pos);
    if (pos == std::string_view::npos) break;

    // Find matching closing brace at brace depth 0
    size_t end = pos + 1;
    int depth = 1;
    while (end < view.size() && depth > 0) {
      if (view[end] == '{') depth++;
      else if (view[end] == '}') depth--;
      end++;
    }

    const std::string_view bookView(view.data() + pos, end - pos);
    BookStoreBook book;
    extractString(bookView, "id", book.id, sizeof(book.id));
    extractString(bookView, "hash", book.hash, sizeof(book.hash));
    extractString(bookView, "title", book.title, sizeof(book.title));
    extractString(bookView, "author", book.author, sizeof(book.author));
    extractString(bookView, "extension", book.extension, sizeof(book.extension));
    extractString(bookView, "language", book.language, sizeof(book.language));
    extractString(bookView, "filesizeString", book.filesizeString, sizeof(book.filesizeString));
    book.year = extractUint(bookView, "year");

    if (book.id[0] != '\0') {
      out.push_back(book);
    }
    pos = end;
  }

  if (out.empty()) {
    setError(BookStoreError::NotFound, "No books found");
    return BookStoreError::NotFound;
  }

  return BookStoreError::Ok;
}

BookStoreError BookStoreClient::resolveDownloadUrl(const BookStoreBook& book, std::string& outUrl) {
  if (!isLoggedIn()) {
    const BookStoreError err = login();
    if (err != BookStoreError::Ok) return err;
  }

  char path[128];
  std::snprintf(path, sizeof(path), "/eapi/book/%s/%s/file", book.id, book.hash);

  char url[256];
  if (!buildUrl(url, sizeof(url), path)) {
    setError(BookStoreError::Network, "Failed to build download URL");
    return BookStoreError::Network;
  }

  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

  std::string response;
  // /eapi/book/<id>/<hash>/file is a GET endpoint — use getRequest, not postForm.
  if (!getRequest(url, response, cookie, userId, userKey)) {
    setError(BookStoreError::Network, "Failed to resolve download link");
    return BookStoreError::Network;
  }

  return parseDownloadLinkResponse(response.c_str(), response.size(), outUrl);
}

BookStoreError BookStoreClient::parseDownloadLinkResponse(const char* json, size_t len, std::string& outUrl) {
  if (!json || len == 0) {
    setError(BookStoreError::Parse, "Empty download link response");
    return BookStoreError::Parse;
  }

  const std::string_view view(json, len);
  if (view.find("\"success\":1") == std::string_view::npos && view.find("\"success\": 1") == std::string_view::npos) {
    if (view.find("Download limit reached") != std::string_view::npos) {
      setError(BookStoreError::Quota, "Download limit reached");
      return BookStoreError::Quota;
    }
    if (view.find("Please login") != std::string_view::npos) {
      userId[0] = '\0';
      userKey[0] = '\0';
      setError(BookStoreError::Auth, "Session expired");
      return BookStoreError::Auth;
    }
    setError(BookStoreError::Server, "Download link request rejected");
    return BookStoreError::Server;
  }

  const char* key = "\"downloadLink\":\"";
  size_t pos = view.find(key);
  if (pos == std::string_view::npos) {
    key = "\"downloadLink\": \"";
    pos = view.find(key);
  }
  if (pos == std::string_view::npos) {
    setError(BookStoreError::Parse, "No download link in response");
    return BookStoreError::Parse;
  }
  pos += std::strlen(key);
  const size_t end = view.find('"', pos);
  if (end == std::string_view::npos) {
    setError(BookStoreError::Parse, "Malformed download link");
    return BookStoreError::Parse;
  }

  outUrl.assign(view.data() + pos, end - pos);
  return BookStoreError::Ok;
}

BookStoreError BookStoreClient::downloadFile(const std::string& url, const std::string& destPath,
                                              ProgressCallback progress, const bool* cancelFlag) {
  WifiPowerSaveGuard psGuard;

  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

  // HttpDownloader does not expose a custom Cookie header, so download directly with esp_http_client.
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.timeout_ms = 120000;
  config.buffer_size = 2048;
  config.buffer_size_tx = 1024;
  config.cert_pem = R13_PEM;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    setError(BookStoreError::File, "Failed to init download client");
    return BookStoreError::File;
  }
  esp_http_client_set_header(client, "User-Agent", "CrossInk-ESP32-" CROSSINK_VERSION);
  // Send auth as both HTTP headers (primary) and Cookie (fallback for older configs).
  if (userId[0] != '\0') {
    esp_http_client_set_header(client, "remix-userid", userId);
  }
  if (userKey[0] != '\0') {
    esp_http_client_set_header(client, "remix-userkey", userKey);
  }
  esp_http_client_set_header(client, "Cookie", cookie);

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "Download open failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    setError(BookStoreError::Network, "Download request failed");
    return BookStoreError::Network;
  }

  const int64_t totalLen = esp_http_client_fetch_headers(client);
  const int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    LOG_ERR("BOOKSTORE", "Download status %d", status);
    
    esp_http_client_cleanup(client);
    setError(BookStoreError::File, "Download rejected by server");
    return BookStoreError::File;
  }

  FsFile file;
  if (!Storage.openFileForWrite("BOOKSTORE", destPath.c_str(), file)) {
    
    esp_http_client_cleanup(client);
    setError(BookStoreError::File, "Could not create destination file");
    return BookStoreError::File;
  }

  if (progress) {
    progress(0, totalLen > 0 ? static_cast<size_t>(totalLen) : 0);
  }

  char buffer[2048];
  int readLen = 0;
  size_t totalRead = 0;
  bool cancelled = false;
  while (!cancelled && (readLen = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
    if (file.write(buffer, readLen) != static_cast<size_t>(readLen)) {
      file.close();
      Storage.remove(destPath.c_str());
      
      esp_http_client_cleanup(client);
      setError(BookStoreError::File, "SD write failed");
      return BookStoreError::File;
    }
    totalRead += readLen;
    if (progress) {
      progress(totalRead, totalLen > 0 ? static_cast<size_t>(totalLen) : 0);
    }
    if (cancelFlag && *cancelFlag) {
      cancelled = true;
    }
  }

  file.close();
  esp_http_client_cleanup(client);

  if (cancelled) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::Cancelled, "Download cancelled");
    return BookStoreError::Cancelled;
  }

  // Verify the downloaded file is not an HTML error page
  FsFile verifyFile;
  if (Storage.openFileForRead("BOOKSTORE", destPath.c_str(), verifyFile)) {
    char header[16];
    const size_t read = verifyFile.read(header, sizeof(header));
    verifyFile.close();
    if (read >= 6 && std::strncmp(header, "<html", 5) == 0) {
      Storage.remove(destPath.c_str());
      setError(BookStoreError::Quota, "Download rejected by server");
      return BookStoreError::Quota;
    }
  }

  return BookStoreError::Ok;
}
