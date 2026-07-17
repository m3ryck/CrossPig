#include "BookStoreClient.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <esp_http_client.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <time.h>

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

#include "network/WifiPowerSaveGuard.h"

namespace {

// z-lib.fm currently uses the Let's Encrypt YR1 RSA-2048 intermediate. Trust
// it directly so mbedTLS parses one small CA instead of YR1 plus the inactive
// R13 chain (or the RSA-4096 ISRG root) during its peak heap phase.
static constexpr const char* BOOKSTORE_CA_PEM = R"(
-----BEGIN CERTIFICATE-----
MIIE2zCCAsOgAwIBAgIRAKICU/FfJpHAXcHOE7m8yk4wDQYJKoZIhvcNAQELBQAw
LjELMAkGA1UEBhMCVVMxDTALBgNVBAoTBElTUkcxEDAOBgNVBAMTB1Jvb3QgWVIw
HhcNMjUwOTAzMDAwMDAwWhcNMjgwOTAyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEW
MBQGA1UEChMNTGV0J3MgRW5jcnlwdDEMMAoGA1UEAxMDWVIxMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoVi8X2xCYgMXvJxNPKp/oF13UMgmPABB07VC
LNDtoXmt9luEZNJSBV10VyT1Pz6LD8Zq1d2gc43WNl1AdRrj4sEnazbOiz0nPpmG
Bp2hui49oZtDIY6wdKeZAi5BbNU20CH6RSBBMLSQ9cXrH8dxdv4PAJ45ssGML68U
SE3BsjC2a6cAN9L5CgXVIQi5tfNiTPoFZZ3S0OlXqLmmtdV95udWAb5b6e/F49Di
CsH0Y00Ag72BVIb1hzynmKe+X0mERBTtsb3BwmpV9ipeBjMLoR/D9cHxHQCWoi5l
TmXwY015J5rGelz1nZjJuxc2kioaX29XJBnhMkP531rSdG5uMwIDAQABo4HuMIHr
MA4GA1UdDwEB/wQEAwIBhjATBgNVHSUEDDAKBggrBgEFBQcDATASBgNVHRMBAf8E
CDAGAQH/AgEAMB0GA1UdDgQWBBQfLzW+RhSCzUCxrnksVXj699Ro+zAfBgNVHSME
GDAWgBTe51tg0CJtQCh9Pw0B/qS1UrRRlDAyBggrBgEFBQcBAQQmMCQwIgYIKwYB
BQUHMAKGFmh0dHA6Ly95ci5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwB
AgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3lyLmMubGVuY3Iub3JnLzANBgkq
hkiG9w0BAQsFAAOCAgEA0+zvMq3kHig1ddTmmm+RibTr9/RpX7k4buanMMRqbV/y
IvP82zAHN3mvaw+cASuVsdpd0ikjhr4hnhJQLQOzOp2ccKrsdGOAgo0vddeISFAq
EWEV4lmUM3vFF796up+bSgmJ1u6RupDCMxDgF8M3eLvGuj6L0lu3zkQ0KuQLnKxL
tB0oQqn1Idg5CuuGpMvQzk29Pa3D/qHurc0EIM9SxukQuJqq63lxsYyRQFU8yMBO
hq1w5LbfaWNRrz1uklOfI/pYkAb2E2MTZrAMQkBIE2S8Jt1F8gRc96o/xOsrgvSk
a84AisX6xq1lz1Z7jGvrnXc4TMcjxZTjiTaihcYI1JIXZiLtEMSCa5l3cu8YWd6z
dLRQlqRdclVjuQfNHawRJ6GWlkK0QJosivTKwdBw3KxEtzGo8yMHERbsy57gP1UX
HOMcmZYQC0gtyR3SxfenIM/MxC3Ia2Ypab/kQ/CTnlIn2KQ5JUC6NYrGCbhFN9bp
5lKJStEwCUnLpntcrXk5XVDCNv/5RyWpRThkGOV7GetKkQ0qAY8hCzWK6oqnAhDZ
cjlYVdWfqOw3DIOX6EDNBgAqHarRVxyF9QZdOaXSyPJ0ueD2BYJEBgaCGQ8rAaU/
Qc123V5LTXDZW4CcsPBDyhy4v+c8hClAyw/IkJlfBqxB9D+/wvIMHgECZ4ptP6o=
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
  const auto setOptionalHeader = [client](const char* name, const char* value) {
    if (value && value[0] != '\0') {
      esp_http_client_set_header(client, name, value);
#ifdef ESP_PLATFORM
    } else {
      esp_http_client_delete_header(client, name);
#endif
    }
  };
  setOptionalHeader("remix-userid", userIdHeader);
  setOptionalHeader("remix-userkey", userKeyHeader);
  setOptionalHeader("Cookie", cookie);
}

void logTlsFailure(esp_http_client_handle_t client, const char* phase) {
  int tlsError = 0;
  int tlsFlags = 0;
  const esp_err_t diagnostic = esp_http_client_get_and_clear_last_tls_error(client, &tlsError, &tlsFlags);
  if (diagnostic != ESP_OK || tlsError != 0 || tlsFlags != 0) {
    LOG_ERR("BOOKSTORE", "%s TLS diagnostic: err=%s mbedtls=0x%x flags=0x%x", phase,
            esp_err_to_name(diagnostic), tlsError < 0 ? -tlsError : tlsError, tlsFlags);
  }
}

void cleanupHttpClient(esp_http_client_handle_t client, bool wasOpened, const char* requestType) {
  if (wasOpened) {
    const esp_err_t closeErr = esp_http_client_close(client);
    if (closeErr != ESP_OK) {
      LOG_ERR("BOOKSTORE", "%s close failed: %s", requestType, esp_err_to_name(closeErr));
    }
  }
  esp_http_client_cleanup(client);
#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "%s cleanup heap: free=%u maxalloc=%u", requestType,
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}

void logTlsClock(const char* requestType) {
  const time_t now = time(nullptr);
  struct tm utcTime = {};
  if (!gmtime_r(&now, &utcTime)) {
    LOG_ERR("BOOKSTORE", "%s TLS clock unavailable", requestType);
    return;
  }
  LOG_INF("BOOKSTORE", "%s TLS clock: %04d-%02d-%02d %02d:%02d:%02d UTC", requestType, utcTime.tm_year + 1900,
          utcTime.tm_mon + 1, utcTime.tm_mday, utcTime.tm_hour, utcTime.tm_min, utcTime.tm_sec);
}

using ResponseSink = bool (*)(void* ctx, const char* data, size_t len);
using ResponseStart = void (*)(void* ctx, size_t total);

struct HttpRequestResult {
  bool ok = false;
  int status = -1;
  size_t bytesRead = 0;
};

struct FixedBufferSink {
  char* data;
  size_t capacity;
  size_t length;
};

bool appendFixedResponse(void* ctx, const char* data, size_t len) {
  auto* sink = static_cast<FixedBufferSink*>(ctx);
  if (!sink || !sink->data || sink->capacity == 0 || len > sink->capacity - 1 - sink->length) {
    return false;
  }
  std::memcpy(sink->data + sink->length, data, len);
  sink->length += len;
  sink->data[sink->length] = '\0';
  return true;
}

bool feedSearchResponse(void* ctx, const char* data, size_t len) {
  auto* parser = static_cast<BookStoreSearchParser*>(ctx);
  return parser && parser->feed(data, len);
}

#ifdef ESP_PLATFORM

struct DownloadResponseSink {
  FsFile* file = nullptr;
  BookStoreClient::ProgressCallback* progress = nullptr;
  const bool* cancelFlag = nullptr;
  size_t total = 0;
  size_t downloaded = 0;
  bool writeFailed = false;
  bool cancelled = false;
};

void startDownloadResponse(void* ctx, size_t total) {
  auto* sink = static_cast<DownloadResponseSink*>(ctx);
  if (!sink) return;
  sink->total = total;
  if (sink->progress && *sink->progress) {
    (*sink->progress)(0, total);
  }
}

bool writeDownloadResponse(void* ctx, const char* data, size_t len) {
  auto* sink = static_cast<DownloadResponseSink*>(ctx);
  if (!sink || !sink->file) return false;
  if (sink->cancelFlag && *sink->cancelFlag) {
    sink->cancelled = true;
    return false;
  }
  if (sink->file->write(data, len) != len) {
    sink->writeFailed = true;
    return false;
  }
  sink->downloaded += len;
  if (sink->progress && *sink->progress) {
    (*sink->progress)(sink->downloaded, sink->total);
  }
  return true;
}

struct ResponseEventContext {
  ResponseSink sink = nullptr;
  ResponseStart start = nullptr;
  void* sinkCtx = nullptr;
  size_t bytesRead = 0;
  int requiredStatus = 0;
  bool started = false;
  bool sinkOk = true;
};

esp_err_t onHttpEvent(esp_http_client_event_t* event) {
  if (!event || event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;

  auto* context = static_cast<ResponseEventContext*>(event->user_data);
  if (!context || !context->sink || !event->data || event->data_len <= 0) return ESP_OK;
  if (!context->sinkOk) return ESP_FAIL;

  if (!context->started) {
    const int64_t contentLength = esp_http_client_get_content_length(event->client);
    if (context->start) {
      context->start(context->sinkCtx, contentLength > 0 ? static_cast<size_t>(contentLength) : 0);
    }
    context->started = true;
  }

  if (context->requiredStatus != 0 &&
      esp_http_client_get_status_code(event->client) != context->requiredStatus) {
    return ESP_OK;
  }

  if (!context->sink(context->sinkCtx, static_cast<const char*>(event->data),
                     static_cast<size_t>(event->data_len))) {
    context->sinkOk = false;
    return ESP_FAIL;
  }
  context->bytesRead += static_cast<size_t>(event->data_len);
  return ESP_OK;
}

esp_http_client_handle_t ensureHttpClient(void*& storedClient, const char* url,
                                          esp_http_client_method_t method, int timeoutMs,
                                          void* userData) {
  if (storedClient) return static_cast<esp_http_client_handle_t>(storedClient);

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = method;
  config.timeout_ms = timeoutMs;
  // Responses are streamed in SD-sector-sized chunks, so a larger HTTP RX
  // buffer only competes with the TLS handshake on this no-PSRAM target.
  config.buffer_size = 512;
  config.buffer_size_tx = 512;
  config.cert_pem = BOOKSTORE_CA_PEM;
  config.event_handler = onHttpEvent;
  config.user_data = userData;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    LOG_ERR("BOOKSTORE", "Failed to allocate retained HTTP client");
    return nullptr;
  }
  storedClient = client;
  return client;
}

void destroyHttpClient(void*& storedClient) {
  if (!storedClient) return;

  esp_http_client_handle_t client = static_cast<esp_http_client_handle_t>(storedClient);
  esp_http_client_cleanup(client);
  storedClient = nullptr;
#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "HTTP session cleanup heap: free=%u maxalloc=%u",
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}

HttpRequestResult performRequest(void*& storedClient, const char* url, esp_http_client_method_t method,
                                 const char* body, ResponseSink sink, void* sinkCtx, const char* requestType,
                                 const char* cookie, const char* userIdHeader, const char* userKeyHeader,
                                 int timeoutMs = 30000, int requiredStatus = 0,
                                 ResponseStart start = nullptr) {
  WifiPowerSaveGuard psGuard;
  HttpRequestResult result;

  const bool reusedHandle = storedClient != nullptr;
  ResponseEventContext context{sink, start, sinkCtx, 0, requiredStatus, false, true};
  LOG_INF("BOOKSTORE", "%s pre-init heap: free=%u maxalloc=%u", requestType,
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  esp_http_client_handle_t client = ensureHttpClient(storedClient, url, method, timeoutMs, &context);
  if (!client) return result;

  if (reusedHandle &&
      (esp_http_client_set_url(client, url) != ESP_OK ||
       esp_http_client_set_method(client, method) != ESP_OK ||
       esp_http_client_set_timeout_ms(client, timeoutMs) != ESP_OK ||
       esp_http_client_set_user_data(client, &context) != ESP_OK)) {
    LOG_ERR("BOOKSTORE", "%s request configuration failed", requestType);
    return result;
  }

  const size_t bodyLen = body ? std::strlen(body) : 0;
  if (esp_http_client_set_post_field(client, body, static_cast<int>(bodyLen)) != ESP_OK) {
    LOG_ERR("BOOKSTORE", "%s request body configuration failed", requestType);
    esp_http_client_set_user_data(client, nullptr);
    return result;
  }

  if (method == HTTP_METHOD_POST) {
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    esp_http_client_set_header(client, "Accept", "application/json, text/javascript, */*; q=0.01");
    esp_http_client_set_header(client, "X-Requested-With", "XMLHttpRequest");
  }
  setCommonHeaders(client, cookie, userIdHeader, userKeyHeader);
  logTlsClock(requestType);

  LOG_INF("BOOKSTORE", "%s heap: free=%u maxalloc=%u", requestType,
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  LOG_INF("BOOKSTORE", "%s HTTP handle: %s", requestType, reusedHandle ? "reused" : "new");

  const esp_err_t err = esp_http_client_perform(client);
  result.status = esp_http_client_get_status_code(client);
  result.bytesRead = context.bytesRead;
  result.ok = err == ESP_OK && context.sinkOk && result.bytesRead > 0 &&
              (requiredStatus == 0 || result.status == requiredStatus);

  // These point to caller-owned stack data and must not survive the request.
  esp_http_client_set_user_data(client, nullptr);
  esp_http_client_set_post_field(client, nullptr, 0);

  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP %s failed: %s", requestType, esp_err_to_name(err));
    logTlsFailure(client, requestType);
    // Preserve the allocated handle and its buffers, but discard a broken
    // transport so the next request can reconnect without another init cycle.
    esp_http_client_close(client);
  } else if (!context.sinkOk) {
    LOG_ERR("BOOKSTORE", "HTTP %s response sink rejected data", requestType);
    esp_http_client_close(client);
  }

  if (result.status != 200) {
    LOG_ERR("BOOKSTORE", "HTTP status %d", result.status);
  }
  LOG_INF("BOOKSTORE", "HTTP response: status=%d bytes=%u", result.status,
          static_cast<unsigned>(result.bytesRead));
  LOG_INF("BOOKSTORE", "%s retained heap: free=%u maxalloc=%u", requestType,
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  return result;
}

#endif

HttpRequestResult readResponse(esp_http_client_handle_t client, ResponseSink sink, void* sinkCtx,
                               const char* logTag) {
  HttpRequestResult result;
  const int64_t contentLen = esp_http_client_fetch_headers(client);
  result.status = esp_http_client_get_status_code(client);
  if (contentLen < 0) {
    LOG_ERR(logTag, "HTTP header fetch failed: %lld status=%d", static_cast<long long>(contentLen), result.status);
    return result;
  }
  if (result.status != 200) {
    LOG_ERR(logTag, "HTTP status %d", result.status);
  }

  char buffer[512];
  int readLen = 0;
  while ((readLen = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
    if (!sink(sinkCtx, buffer, static_cast<size_t>(readLen))) {
      LOG_ERR(logTag, "HTTP response exceeded parser or buffer limit");
      return result;
    }
    result.bytesRead += static_cast<size_t>(readLen);
  }
  if (readLen < 0) {
    LOG_ERR(logTag, "HTTP response read failed");
    return result;
  }

  result.ok = result.bytesRead > 0;
  LOG_INF(logTag, "HTTP response: status=%d bytes=%u", result.status,
          static_cast<unsigned>(result.bytesRead));
  return result;
}

HttpRequestResult postForm(void*& storedClient, const char* url, const char* body, ResponseSink sink, void* sinkCtx,
                           const char* cookie = nullptr, const char* userIdHeader = nullptr,
                           const char* userKeyHeader = nullptr) {
#ifdef ESP_PLATFORM
  return performRequest(storedClient, url, HTTP_METHOD_POST, body, sink, sinkCtx, "POST", cookie,
                        userIdHeader, userKeyHeader);
#else
  (void)storedClient;
  WifiPowerSaveGuard psGuard;
  HttpRequestResult result;

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 30000;
  config.buffer_size = 1024;
  config.buffer_size_tx = 512;
  config.cert_pem = BOOKSTORE_CA_PEM;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return result;

  esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
  esp_http_client_set_header(client, "Accept", "application/json, text/javascript, */*; q=0.01");
  esp_http_client_set_header(client, "X-Requested-With", "XMLHttpRequest");
  setCommonHeaders(client, cookie, userIdHeader, userKeyHeader);
  logTlsClock("POST");

#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "POST heap: free=%u maxalloc=%u",
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  const size_t bodyLen = std::strlen(body);
  esp_err_t err = esp_http_client_open(client, static_cast<int>(bodyLen));
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP POST open failed: %s", esp_err_to_name(err));
    logTlsFailure(client, "HTTP POST open failed");
    cleanupHttpClient(client, false, "POST");
    return result;
  }

  size_t totalWritten = 0;
  while (totalWritten < bodyLen) {
    const int written = esp_http_client_write(client, body + totalWritten,
                                              static_cast<int>(bodyLen - totalWritten));
    if (written <= 0) {
      LOG_ERR("BOOKSTORE", "HTTP POST body write failed: written=%d sent=%u total=%u", written,
              static_cast<unsigned>(totalWritten), static_cast<unsigned>(bodyLen));
      cleanupHttpClient(client, true, "POST");
      return result;
    }
    totalWritten += static_cast<size_t>(written);
  }

  result = readResponse(client, sink, sinkCtx, "BOOKSTORE");
  cleanupHttpClient(client, true, "POST");
  return result;
#endif
}

// GET request — used for endpoints that return JSON via HTTP GET (e.g. /eapi/book/.../file).
HttpRequestResult getRequest(void*& storedClient, const char* url, ResponseSink sink, void* sinkCtx,
                             const char* cookie = nullptr,
                             const char* userIdHeader = nullptr, const char* userKeyHeader = nullptr) {
#ifdef ESP_PLATFORM
  return performRequest(storedClient, url, HTTP_METHOD_GET, nullptr, sink, sinkCtx, "GET", cookie,
                        userIdHeader, userKeyHeader);
#else
  (void)storedClient;
  WifiPowerSaveGuard psGuard;
  HttpRequestResult result;

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = 30000;
  config.buffer_size = 1024;
  config.buffer_size_tx = 512;
  config.cert_pem = BOOKSTORE_CA_PEM;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return result;

  setCommonHeaders(client, cookie, userIdHeader, userKeyHeader);
  logTlsClock("GET");

#ifdef ESP_PLATFORM
  LOG_INF("BOOKSTORE", "GET heap: free=%u maxalloc=%u",
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  esp_err_t err = esp_http_client_open(client, 0);  // 0 = no request body
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "HTTP GET open failed: %s", esp_err_to_name(err));
    logTlsFailure(client, "HTTP GET open failed");
    cleanupHttpClient(client, false, "GET");
    return result;
  }

  result = readResponse(client, sink, sinkCtx, "BOOKSTORE");
  cleanupHttpClient(client, true, "GET");
  return result;
#endif
}

}  // namespace

BookStoreSearchParser::BookStoreSearchParser()
    : parser(JsonCallbacks{this, &BookStoreSearchParser::onKey, &BookStoreSearchParser::onString,
                           &BookStoreSearchParser::onNumber, &BookStoreSearchParser::onBool,
                           &BookStoreSearchParser::onNull, &BookStoreSearchParser::onObjectStart,
                           &BookStoreSearchParser::onObjectEnd, &BookStoreSearchParser::onArrayStart,
                           &BookStoreSearchParser::onArrayEnd}) {}

void BookStoreSearchParser::reset(std::vector<BookStoreBook>& output) {
  books = &output;
  downloadUrl = nullptr;
  currentBook = {};
  lastKey = LastKey::None;
  depth = 0;
  booksArrayDepth = 0;
  insideBooksArray = false;
  insideBook = false;
  booksArraySeen = false;
  downloadLinkSeen = false;
  authenticationError = false;
  quotaError = false;
  parser.reset();
}

void BookStoreSearchParser::resetDownload(std::string& outputUrl) {
  books = nullptr;
  downloadUrl = &outputUrl;
  downloadUrl->clear();
  if (downloadUrl->capacity() < StreamingJsonParser::TOKEN_BUF_SIZE) {
    downloadUrl->reserve(StreamingJsonParser::TOKEN_BUF_SIZE);
  }
  currentBook = {};
  lastKey = LastKey::None;
  depth = 0;
  booksArrayDepth = 0;
  insideBooksArray = false;
  insideBook = false;
  booksArraySeen = false;
  downloadLinkSeen = false;
  authenticationError = false;
  quotaError = false;
  parser.reset();
}

bool BookStoreSearchParser::feed(const char* data, size_t len) {
  parser.feed(data, len);
  return !parser.hasError();
}

void BookStoreSearchParser::copyValue(char* dest, size_t destLen, const char* value, size_t valueLen) {
  if (!dest || destLen == 0) return;
  const size_t copyLen = std::min(destLen - 1, valueLen);
  std::memcpy(dest, value, copyLen);
  dest[copyLen] = '\0';
}

void BookStoreSearchParser::applyString(const char* value, size_t len) {
  if (lastKey == LastKey::Error || lastKey == LastKey::Message) {
    const std::string_view message(value, len);
    authenticationError = authenticationError || message.find("Please login") != std::string_view::npos ||
                          message.find("Incorrect email or password") != std::string_view::npos;
    quotaError = quotaError || message.find("Download limit reached") != std::string_view::npos;
    return;
  }
  if (lastKey == LastKey::DownloadLink && downloadUrl) {
    downloadUrl->assign(value, len);
    downloadLinkSeen = true;
    return;
  }
  if (!insideBook || depth != booksArrayDepth + 1) return;

  switch (lastKey) {
    case LastKey::Id:
      copyValue(currentBook.id, sizeof(currentBook.id), value, len);
      break;
    case LastKey::Hash:
      copyValue(currentBook.hash, sizeof(currentBook.hash), value, len);
      break;
    case LastKey::Title:
      copyValue(currentBook.title, sizeof(currentBook.title), value, len);
      break;
    case LastKey::Author:
      copyValue(currentBook.author, sizeof(currentBook.author), value, len);
      break;
    case LastKey::Extension:
      copyValue(currentBook.extension, sizeof(currentBook.extension), value, len);
      break;
    case LastKey::Language:
      copyValue(currentBook.language, sizeof(currentBook.language), value, len);
      break;
    case LastKey::FilesizeString:
      copyValue(currentBook.filesizeString, sizeof(currentBook.filesizeString), value, len);
      break;
    case LastKey::Year:
      currentBook.year = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
      break;
    default:
      break;
  }
}

void BookStoreSearchParser::commitBook() {
  if (books && books->size() < MAX_RESULTS && currentBook.id[0] != '\0') {
    books->push_back(currentBook);
  }
  currentBook = {};
}

void BookStoreSearchParser::onKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  self->lastKey = LastKey::None;

  if (!self->insideBook) {
    if (len == 7 && std::memcmp(key, "success", 7) == 0)
      self->lastKey = LastKey::Success;
    else if (len == 5 && std::memcmp(key, "books", 5) == 0)
      self->lastKey = LastKey::Books;
    else if (len == 5 && std::memcmp(key, "error", 5) == 0)
      self->lastKey = LastKey::Error;
    else if (len == 7 && std::memcmp(key, "message", 7) == 0)
      self->lastKey = LastKey::Message;
    else if (len == 12 && std::memcmp(key, "downloadLink", 12) == 0)
      self->lastKey = LastKey::DownloadLink;
    return;
  }

  if (self->depth != self->booksArrayDepth + 1) return;
  if (len == 2 && std::memcmp(key, "id", 2) == 0)
    self->lastKey = LastKey::Id;
  else if (len == 4 && std::memcmp(key, "hash", 4) == 0)
    self->lastKey = LastKey::Hash;
  else if (len == 5 && std::memcmp(key, "title", 5) == 0)
    self->lastKey = LastKey::Title;
  else if (len == 6 && std::memcmp(key, "author", 6) == 0)
    self->lastKey = LastKey::Author;
  else if (len == 9 && std::memcmp(key, "extension", 9) == 0)
    self->lastKey = LastKey::Extension;
  else if (len == 8 && std::memcmp(key, "language", 8) == 0)
    self->lastKey = LastKey::Language;
  else if (len == 14 && std::memcmp(key, "filesizeString", 14) == 0)
    self->lastKey = LastKey::FilesizeString;
  else if (len == 4 && std::memcmp(key, "year", 4) == 0)
    self->lastKey = LastKey::Year;
}

void BookStoreSearchParser::onString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  self->applyString(value, len);
  self->lastKey = LastKey::None;
}

void BookStoreSearchParser::onNumber(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  if (self->lastKey != LastKey::Success) self->applyString(value, len);
  self->lastKey = LastKey::None;
}

void BookStoreSearchParser::onBool(void* ctx, bool /*value*/) {
  static_cast<BookStoreSearchParser*>(ctx)->lastKey = LastKey::None;
}

void BookStoreSearchParser::onNull(void* ctx) {
  static_cast<BookStoreSearchParser*>(ctx)->lastKey = LastKey::None;
}

void BookStoreSearchParser::onObjectStart(void* ctx) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  self->depth++;
  if (self->insideBooksArray && !self->insideBook && self->depth == self->booksArrayDepth + 1) {
    self->insideBook = true;
    self->currentBook = {};
  }
  self->lastKey = LastKey::None;
}

void BookStoreSearchParser::onObjectEnd(void* ctx) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  if (self->insideBook && self->depth == self->booksArrayDepth + 1) {
    self->commitBook();
    self->insideBook = false;
  }
  if (self->depth > 0) self->depth--;
  self->lastKey = LastKey::None;
}

void BookStoreSearchParser::onArrayStart(void* ctx) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  const bool startsBooks = self->lastKey == LastKey::Books && !self->insideBooksArray;
  self->depth++;
  if (startsBooks) {
    self->insideBooksArray = true;
    self->booksArraySeen = true;
    self->booksArrayDepth = self->depth;
  }
  self->lastKey = LastKey::None;
}

void BookStoreSearchParser::onArrayEnd(void* ctx) {
  auto* self = static_cast<BookStoreSearchParser*>(ctx);
  if (self->insideBooksArray && self->depth == self->booksArrayDepth) {
    self->insideBooksArray = false;
    self->insideBook = false;
  }
  if (self->depth > 0) self->depth--;
  self->lastKey = LastKey::None;
}

BookStoreClient::BookStoreClient() { lastErrorMessage[0] = '\0'; }

BookStoreClient::~BookStoreClient() {
#ifdef ESP_PLATFORM
  destroyHttpClient(httpClient);
#endif
}

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

  // Keep the encoded credentials scoped so they are released before the TLS
  // request. The final EAPI form remains bounded by the credential field sizes.
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

  responseBuffer[0] = '\0';
  FixedBufferSink sink{responseBuffer, sizeof(responseBuffer), 0};
  const HttpRequestResult response = postForm(httpClient, url, body, appendFixedResponse, &sink);
  responseLength = sink.length;
  if (!response.ok) {
    setError(BookStoreError::Network, "Login request failed");
    return BookStoreError::Network;
  }
  if (response.status >= 500) {
    setError(BookStoreError::Server, "Login server unavailable");
    return BookStoreError::Server;
  }

  return parseLoginResponse(responseBuffer, responseLength);
}

BookStoreError BookStoreClient::parseLoginResponse(const char* json, size_t len) {
  if (!json || len == 0) {
    setError(BookStoreError::Parse, "Empty login response");
    return BookStoreError::Parse;
  }

  const std::string_view view(json, len);
  auto extract = [](std::string_view v, const char* key, char* out, size_t outLen) {
    if (outLen == 0) return false;

    char pattern[48];
    const int patternLen = std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (patternLen <= 0 || static_cast<size_t>(patternLen) >= sizeof(pattern)) return false;
    size_t pos = v.find(std::string_view(pattern, static_cast<size_t>(patternLen)));
    if (pos == std::string_view::npos) return false;
    pos += static_cast<size_t>(patternLen);
    while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t' || v[pos] == '\r' || v[pos] == '\n')) pos++;
    if (pos >= v.size() || v[pos++] != ':') return false;
    while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t' || v[pos] == '\r' || v[pos] == '\n')) pos++;
    if (pos >= v.size()) return false;

    const bool quoted = v[pos] == '"';
    if (quoted) pos++;
    size_t end = quoted ? v.find('"', pos) : v.find_first_of(",}", pos);
    if (end == std::string_view::npos || end == pos) return false;
    while (!quoted && end > pos && (v[end - 1] == ' ' || v[end - 1] == '\t' || v[end - 1] == '\r' ||
                                    v[end - 1] == '\n')) {
      end--;
    }

    const size_t copyLen = std::min(outLen - 1, end - pos);
    std::memcpy(out, v.data() + pos, copyLen);
    out[copyLen] = '\0';
    return true;
  };

  // EAPI mirrors use either user_id/user_key or id/remix_userkey.
  if (extract(view, "user_id", userId, sizeof(userId)) && extract(view, "user_key", userKey, sizeof(userKey))) {
    LOG_INF("BOOKSTORE", "EAPI login succeeded");
    return BookStoreError::Ok;
  }
  if (extract(view, "id", userId, sizeof(userId)) && extract(view, "remix_userkey", userKey, sizeof(userKey))) {
    LOG_INF("BOOKSTORE", "EAPI login succeeded");
    return BookStoreError::Ok;
  }

  if (view.find("Incorrect email or password") != std::string_view::npos ||
      view.find("Please login") != std::string_view::npos) {
    setError(BookStoreError::Auth, "Incorrect email or password");
    return BookStoreError::Auth;
  }

  setError(BookStoreError::Parse, "Failed to parse login session");
  return BookStoreError::Parse;
}

BookStoreError BookStoreClient::search(const char* query, uint32_t page, std::vector<BookStoreBook>& out) {
  out.clear();

  if (!isLoggedIn()) {
    const BookStoreError err = login();
    if (err != BookStoreError::Ok) return err;
  }

  // Five fixed-size records cost 1,820 bytes. Reserve only after login so this
  // allocation does not compete with the first TLS handshake.
  if (out.capacity() < BookStoreSearchParser::MAX_RESULTS) {
    out.reserve(BookStoreSearchParser::MAX_RESULTS);
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

  searchParser.reset(out);
  const HttpRequestResult response =
      postForm(httpClient, url, body, feedSearchResponse, &searchParser, cookie, userId, userKey);
  if (!response.ok) {
    setError(BookStoreError::Network, "Search request failed");
    return BookStoreError::Network;
  }
  if (response.status == 401 || response.status == 403 || searchParser.hasAuthenticationError()) {
    userId[0] = '\0';
    userKey[0] = '\0';
    setError(BookStoreError::Auth, "Session expired");
    return BookStoreError::Auth;
  }
  if (response.status != 200) {
    setError(BookStoreError::Server, "Search request rejected");
    return BookStoreError::Server;
  }
  if (searchParser.hasError() || !searchParser.sawBooks()) {
    setError(BookStoreError::Parse, "Failed to parse search response");
    return BookStoreError::Parse;
  }

  if (out.empty()) {
    setError(BookStoreError::NotFound, "No books found");
    return BookStoreError::NotFound;
  }

  LOG_INF("BOOKSTORE", "Search parsed %u result(s)", static_cast<unsigned>(out.size()));
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

  searchParser.resetDownload(outUrl);
  const HttpRequestResult response =
      getRequest(httpClient, url, feedSearchResponse, &searchParser, cookie, userId, userKey);
  if (!response.ok) {
    setError(BookStoreError::Network, "Failed to resolve download link");
    return BookStoreError::Network;
  }
  if (response.status == 401 || response.status == 403 || searchParser.hasAuthenticationError()) {
    userId[0] = '\0';
    userKey[0] = '\0';
    setError(BookStoreError::Auth, "Session expired");
    return BookStoreError::Auth;
  }
  if (searchParser.hasQuotaError()) {
    setError(BookStoreError::Quota, "Download limit reached");
    return BookStoreError::Quota;
  }
  if (response.status != 200) {
    setError(BookStoreError::Server, "Download link request rejected");
    return BookStoreError::Server;
  }
  if (searchParser.hasError() || !searchParser.sawDownloadLink()) {
    setError(BookStoreError::Parse, "No download link in response");
    return BookStoreError::Parse;
  }
  return BookStoreError::Ok;
}

BookStoreError BookStoreClient::downloadFile(const std::string& url, const std::string& destPath,
                                              ProgressCallback progress, const bool* cancelFlag) {
  WifiPowerSaveGuard psGuard;

  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

#ifdef ESP_PLATFORM
  FsFile file;
  if (!Storage.openFileForWrite("BOOKSTORE", destPath.c_str(), file)) {
    setError(BookStoreError::File, "Could not create destination file");
    return BookStoreError::File;
  }

  DownloadResponseSink sink{&file, &progress, cancelFlag};
  const HttpRequestResult response =
      performRequest(httpClient, url.c_str(), HTTP_METHOD_GET, nullptr, writeDownloadResponse, &sink,
                     "DOWNLOAD", cookie, userId, userKey, 120000, 200, startDownloadResponse);
  file.close();

  if (sink.cancelled) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::Cancelled, "Download cancelled");
    return BookStoreError::Cancelled;
  }
  if (sink.writeFailed) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::File, "SD write failed");
    return BookStoreError::File;
  }
  if (response.status != 200) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::File, "Download rejected by server");
    return BookStoreError::File;
  }
  if (!response.ok) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::Network, "Download interrupted");
    return BookStoreError::Network;
  }
#else
  // HttpDownloader does not expose a custom Cookie header, so download directly with esp_http_client.
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.timeout_ms = 120000;
  config.buffer_size = 1024;
  config.buffer_size_tx = 512;
  config.cert_pem = BOOKSTORE_CA_PEM;

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
  logTlsClock("DOWNLOAD");

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "Download open failed: %s", esp_err_to_name(err));
    logTlsFailure(client, "Download open failed");
    cleanupHttpClient(client, false, "DOWNLOAD");
    setError(BookStoreError::Network, "Download request failed");
    return BookStoreError::Network;
  }

  const int64_t totalLen = esp_http_client_fetch_headers(client);
  const int status = esp_http_client_get_status_code(client);
  if (totalLen < 0 || status != 200) {
    LOG_ERR("BOOKSTORE", "Download header fetch failed: len=%lld status=%d", static_cast<long long>(totalLen),
            status);
    cleanupHttpClient(client, true, "DOWNLOAD");
    setError(BookStoreError::File, "Download rejected by server");
    return BookStoreError::File;
  }

  FsFile file;
  if (!Storage.openFileForWrite("BOOKSTORE", destPath.c_str(), file)) {
    cleanupHttpClient(client, true, "DOWNLOAD");
    setError(BookStoreError::File, "Could not create destination file");
    return BookStoreError::File;
  }

  if (progress) {
    progress(0, totalLen > 0 ? static_cast<size_t>(totalLen) : 0);
  }

  // One SD-sector buffer keeps stack use bounded while avoiding partial-sector writes.
  char buffer[512];
  int readLen = 0;
  size_t totalRead = 0;
  bool cancelled = false;
  while (!cancelled && (readLen = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
    if (file.write(buffer, readLen) != static_cast<size_t>(readLen)) {
      file.close();
      Storage.remove(destPath.c_str());
      cleanupHttpClient(client, true, "DOWNLOAD");
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
  cleanupHttpClient(client, true, "DOWNLOAD");

  if (readLen < 0) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::Network, "Download interrupted");
    return BookStoreError::Network;
  }

  if (cancelled) {
    Storage.remove(destPath.c_str());
    setError(BookStoreError::Cancelled, "Download cancelled");
    return BookStoreError::Cancelled;
  }
#endif

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
