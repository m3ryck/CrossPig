# Book Store Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a native C++ "Book Store" activity to the CrossInk firmware that lets the user search and download books directly to the SD card via the Z-library internal API, with a generic UI and settings.

**Architecture:** A network client (`BookStoreClient`) in `src/network/` handles HTTPS login, search, and download against the Z-library `/eapi/` endpoints. A UI activity (`BookStoreActivity`) in `src/activities/network/` presents a main menu, settings, search input, paginated results, book details, download confirmation, and progress. The feature is reachable from **Home → File Transfer → Book Store**.

**Tech Stack:** C++17/20, PlatformIO, ESP-IDF `esp_http_client` via `HttpDownloader`, `StreamingJsonParser`, `GUI`/`UITheme`, `CrossPointSettings` persistence.

---

## File map

| File | What it is |
|---|---|
| `lib/I18n/translations/english.yaml` | New user-facing strings |
| `src/CrossPointSettings.h` | Four new persisted char[] fields |
| `src/SettingsList.h` | Registration of new string settings |
| `src/activities/ActivityManager.h` | Declaration of `goToBookStore()` |
| `src/activities/ActivityManager.cpp` | Implementation of `goToBookStore()` |
| `src/activities/network/NetworkModeSelectionActivity.h` | New `NetworkMode::BOOK_STORE` |
| `src/activities/network/NetworkModeSelectionActivity.cpp` | Menu item and dispatch for Book Store |
| `src/network/BookStoreClient.h` | Client class header |
| `src/network/BookStoreClient.cpp` | Client implementation |
| `src/activities/network/BookStoreActivity.h` | Activity header |
| `src/activities/network/BookStoreActivity.cpp` | Activity implementation |

---

## Task 1: Add I18n strings

**Files:**
- Modify: `lib/I18n/translations/english.yaml`

Add the following keys near the other network/transfer strings (after `STR_CALIBRE_WEB_URL` is a good spot):

```yaml
STR_BOOK_STORE: "Book Store"
STR_BOOK_STORE_DESC: "Search and download books from a configured store"
STR_BOOK_STORE_SEARCH: "Search"
STR_BOOK_STORE_SETTINGS: "Settings"
STR_BOOK_STORE_BASE_URL: "Store URL"
STR_BOOK_STORE_EMAIL: "Email"
STR_BOOK_STORE_PASSWORD: "Password"
STR_BOOK_STORE_DOWNLOAD_PATH: "Download folder"
STR_BOOK_STORE_NO_WIFI: "WiFi not connected"
STR_BOOK_STORE_LOGIN_FAILED: "Login failed. Check email and password."
STR_BOOK_STORE_NO_RESULTS: "No books found"
STR_BOOK_STORE_QUOTA_REACHED: "Daily download limit reached"
STR_BOOK_STORE_DOWNLOAD_FAILED: "Download failed"
STR_BOOK_STORE_SAVE_FAILED: "Could not save file"
STR_BOOK_STORE_CANCELED: "Canceled"
STR_BOOK_STORE_DOWNLOADING: "Downloading..."
STR_BOOK_STORE_DOWNLOAD_COMPLETE: "Download complete"
STR_BOOK_STORE_OPEN_BOOK: "Open Book"
STR_BOOK_STORE_CONFIRM_DOWNLOAD: "Download this book?"
```

No other translation files need to be touched; missing keys fall back to English.

- [ ] **Step 1: Add the keys above to `english.yaml`**

- [ ] **Step 2: Verify the YAML is valid**

Run:
```bash
python3 scripts/gen_i18n.py
```

Expected: no errors, and files `lib/I18n/I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` are regenerated.

- [ ] **Step 3: Commit**

```bash
git add lib/I18n/translations/english.yaml lib/I18n/I18nKeys.h lib/I18n/I18nStrings.h lib/I18n/I18nStrings.cpp
git commit -m "i18n: add Book Store strings"
```

---

## Task 2: Add persisted settings fields

**Files:**
- Modify: `src/CrossPointSettings.h`
- Modify: `src/SettingsList.h`

### Step 1: Add fields to `CrossPointSettings.h`

Add these fields after the OPDS settings block (around line 384):

```cpp
  // Book Store settings
  char bookStoreBaseUrl[128] = "";
  char bookStoreEmail[64] = "";
  char bookStorePassword[64] = "";
  char bookStoreDownloadPath[64] = "/downloads";
```

### Step 2: Register strings in `SettingsList.h`

Add these four entries inside `getSettingsList()` before the KOReader Sync block (around line 538):

```cpp
    // --- Book Store ---
    add(SettingInfo::String(StrId::STR_BOOK_STORE_BASE_URL, SETTINGS.bookStoreBaseUrl,
                            sizeof(SETTINGS.bookStoreBaseUrl), "bookStoreBaseUrl", StrId::STR_BOOK_STORE));
    add(SettingInfo::String(StrId::STR_BOOK_STORE_EMAIL, SETTINGS.bookStoreEmail,
                            sizeof(SETTINGS.bookStoreEmail), "bookStoreEmail", StrId::STR_BOOK_STORE));
    add(SettingInfo::String(StrId::STR_BOOK_STORE_PASSWORD, SETTINGS.bookStorePassword,
                            sizeof(SETTINGS.bookStorePassword), "bookStorePassword", StrId::STR_BOOK_STORE));
    add(SettingInfo::String(StrId::STR_BOOK_STORE_DOWNLOAD_PATH, SETTINGS.bookStoreDownloadPath,
                            sizeof(SETTINGS.bookStoreDownloadPath), "bookStoreDownloadPath", StrId::STR_BOOK_STORE));
```

- [ ] **Step 1: Add fields to `CrossPointSettings.h`**

- [ ] **Step 2: Register settings in `SettingsList.h`**

- [ ] **Step 3: Build simulator to check compile**

Run:
```bash
pio run -e simulator
```

Expected: build succeeds or fails only because new strings/UI code are not yet present.

- [ ] **Step 4: Commit**

```bash
git add src/CrossPointSettings.h src/SettingsList.h
git commit -m "feat(settings): add Book Store persisted fields"
```

---

## Task 3: Add ActivityManager launcher

**Files:**
- Modify: `src/activities/ActivityManager.h`
- Modify: `src/activities/ActivityManager.cpp`

### Step 1: Declare in header

Add after `goToNearbyStatsSync()` in `ActivityManager.h` (line 96):

```cpp
  void goToBookStore();
```

### Step 2: Include and implement in `ActivityManager.cpp`

Add the include near the other network activity includes (after line 19):

```cpp
#include "network/BookStoreActivity.h"
```

Add the implementation after `goToNearbyStatsSync()` (around line 209):

```cpp
void ActivityManager::goToBookStore() {
  replaceActivity(std::make_unique<BookStoreActivity>(renderer, mappedInput));
}
```

- [ ] **Step 1: Declare `goToBookStore()` in `ActivityManager.h`**

- [ ] **Step 2: Include `BookStoreActivity.h` in `ActivityManager.cpp`**

- [ ] **Step 3: Implement `goToBookStore()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because `BookStoreActivity.h` does not exist yet. This is fine — the next tasks create it.

- [ ] **Step 5: Commit**

```bash
git add src/activities/ActivityManager.h src/activities/ActivityManager.cpp
git commit -m "feat(activity-manager): add Book Store launcher"
```

---

## Task 4: Add Book Store to the transfer menu

**Files:**
- Modify: `src/activities/network/NetworkModeSelectionActivity.h`
- Modify: `src/activities/network/NetworkModeSelectionActivity.cpp`

### Step 1: Add enum value

In `NetworkModeSelectionActivity.h`, change the enum to:

```cpp
enum class NetworkMode { JOIN_NETWORK, CONNECT_CALIBRE, CREATE_HOTSPOT, NEARBY_STATS_SYNC, BOOK_STORE };
```

### Step 2: Update menu arrays and item count

In `NetworkModeSelectionActivity.cpp`:

1. Change `MENU_ITEM_COUNT` to `5`.
2. Update the static arrays:

```cpp
static constexpr StrId menuItems[MENU_ITEM_COUNT] = {StrId::STR_JOIN_NETWORK, StrId::STR_CALIBRE_WIRELESS,
                                                     StrId::STR_CREATE_HOTSPOT, StrId::STR_NEARBY_STATS_SYNC,
                                                     StrId::STR_BOOK_STORE};
static constexpr StrId menuDescs[MENU_ITEM_COUNT] = {StrId::STR_JOIN_DESC, StrId::STR_CALIBRE_DESC,
                                                     StrId::STR_HOTSPOT_DESC, StrId::STR_NEARBY_STATS_SYNC_DESC,
                                                     StrId::STR_BOOK_STORE_DESC};
static constexpr UIIcon menuIcons[MENU_ITEM_COUNT] = {UIIcon::Wifi, UIIcon::Library, UIIcon::Hotspot,
                                                        UIIcon::Transfer, UIIcon::Library};
```

### Step 3: Update selection dispatch in `loop()`

Change the `loop()` body selection block to:

```cpp
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    NetworkMode mode = NetworkMode::JOIN_NETWORK;
    if (selectedIndex == 1) {
      mode = NetworkMode::CONNECT_CALIBRE;
    } else if (selectedIndex == 2) {
      mode = NetworkMode::CREATE_HOTSPOT;
    } else if (selectedIndex == 3) {
      mode = NetworkMode::NEARBY_STATS_SYNC;
    } else if (selectedIndex == 4) {
      mode = NetworkMode::BOOK_STORE;
    }
    onModeSelected(mode);
    return;
  }
```

### Step 4: Handle BOOK_STORE in the consumer

The `CrossPointWebServerActivity.cpp` consumes the result of `NetworkModeSelectionActivity`. Add handling for `NetworkMode::BOOK_STORE` in its result handler.

Locate the result handler (look for `NetworkModeResult` or `onModeSelected`). Add a case:

```cpp
      case NetworkMode::BOOK_STORE:
        activityManager.goToBookStore();
        break;
```

- [ ] **Step 1: Add `BOOK_STORE` to `NetworkMode` enum**

- [ ] **Step 2: Update menu count, labels, descriptions, and icons**

- [ ] **Step 3: Update `loop()` dispatch to set `NetworkMode::BOOK_STORE`**

- [ ] **Step 4: Handle `NetworkMode::BOOK_STORE` in `CrossPointWebServerActivity.cpp` result handler**

- [ ] **Step 5: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: still fails because `BookStoreActivity` and `BookStoreClient` do not exist.

- [ ] **Step 6: Commit**

```bash
git add src/activities/network/NetworkModeSelectionActivity.h src/activities/network/NetworkModeSelectionActivity.cpp src/activities/network/CrossPointWebServerActivity.cpp
git commit -m "feat(network-menu): add Book Store transfer menu entry"
```

---

## Task 5: Create `BookStoreClient` header

**Files:**
- Create: `src/network/BookStoreClient.h`

Write the file:

```cpp
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
```

- [ ] **Step 1: Create `src/network/BookStoreClient.h` with the content above**

- [ ] **Step 2: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because `.cpp` does not exist yet.

- [ ] **Step 3: Commit**

```bash
git add src/network/BookStoreClient.h
git commit -m "feat(network): add BookStoreClient header"
```

---

## Task 6: Implement `BookStoreClient` core helpers

**Files:**
- Create: `src/network/BookStoreClient.cpp`

### Step 1: Write the skeleton and helpers

```cpp
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
```

- [ ] **Step 1: Create `src/network/BookStoreClient.cpp` with helpers above**

- [ ] **Step 2: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because `login()`, `search()`, etc. are not defined.

- [ ] **Step 3: Commit**

```bash
git add src/network/BookStoreClient.cpp
git commit -m "feat(network): add BookStoreClient helpers"
```

---

## Task 7: Implement `BookStoreClient::login()`

**Files:**
- Modify: `src/network/BookStoreClient.cpp`

Add the login implementation. It POSTs `email` and `password` to `/eapi/user/login` and parses the JSON response.

For v1, use `HttpDownloader::fetchUrl` with a small in-memory body; the login response is tiny.

```cpp
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

  // Use HttpDownloader to POST is not directly supported; use esp_http_client directly for the login call.
  // For simplicity in this plan, implement a small helper using esp_http_client.
  // ... (see next step)
}
```

Because `HttpDownloader` only supports GET downloads and generic fetch, implement a small private helper `postForm` using `esp_http_client`:

```cpp
namespace {
bool postForm(const char* url, const char* body, std::string& outResponse) {
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
```

Then finish `login()`:

```cpp
  std::string response;
  if (!postForm(url, body, response)) {
    setError(BookStoreError::Network, "Login request failed");
    return BookStoreError::Network;
  }

  return parseLoginResponse(response.c_str(), response.size());
}
```

Add `parseLoginResponse`:

```cpp
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
```

Note: requires `<string_view>` include.

- [ ] **Step 1: Add `postForm` helper and includes**

- [ ] **Step 2: Implement `login()`**

- [ ] **Step 3: Implement `parseLoginResponse()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: may fail because `esp_http_client` APIs differ on native sim; wrap `#ifndef SIMULATOR` if needed, or use `HttpDownloader::fetchUrl` only on simulator. The simulator build of `esp_http_client` may not exist. If the build fails here, add `#ifndef SIMULATOR` stubs for the `postForm` helper that return false, and document that login cannot be tested in the simulator.

- [ ] **Step 5: Commit**

```bash
git add src/network/BookStoreClient.cpp
git commit -m "feat(network): implement BookStoreClient login"
```

---

## Task 8: Implement `BookStoreClient::search()`

**Files:**
- Modify: `src/network/BookStoreClient.cpp`

Implement search using `HttpDownloader::fetchUrl` with a `DataCallback` that feeds a streaming JSON parser. For v1, because the response with `limit=5` is small, a simpler approach is acceptable: fetch the whole response into a `std::string` if it is under a bounded size (e.g., 16 KB). This keeps the implementation small.

```cpp
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

  // The Z-library search endpoint is POST. Use the same postForm helper with session cookies.
  // postForm currently does not set cookies; extend it to accept optional cookie header.
}
```

Extend `postForm` to accept an optional cookie string:

```cpp
namespace {
bool postForm(const char* url, const char* body, std::string& outResponse, const char* cookie = nullptr) {
  // ... existing init ...
  if (cookie && cookie[0] != '\0') {
    esp_http_client_set_header(client, "Cookie", cookie);
  }
  // ... rest unchanged ...
}
}  // namespace
```

Finish `search()`:

```cpp
  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

  std::string response;
  if (!postForm(url, body, response, cookie)) {
    setError(BookStoreError::Network, "Search request failed");
    return BookStoreError::Network;
  }

  if (response.size() > 16384) {
    setError(BookStoreError::Network, "Search response too large");
    return BookStoreError::Network;
  }

  return parseSearchResponse(response.c_str(), response.size(), out);
}
```

Implement `parseSearchResponse`:

```cpp
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
```

- [ ] **Step 1: Extend `postForm` to accept optional cookie**

- [ ] **Step 2: Implement `search()`**

- [ ] **Step 3: Implement `parseSearchResponse()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: may fail because `BookStoreActivity` is still missing; client should compile if no simulator-specific issues.

- [ ] **Step 5: Commit**

```bash
git add src/network/BookStoreClient.cpp
git commit -m "feat(network): implement BookStoreClient search"
```

---

## Task 9: Implement `BookStoreClient::resolveDownloadUrl()` and `downloadFile()`

**Files:**
- Modify: `src/network/BookStoreClient.cpp`

### `resolveDownloadUrl`

```cpp
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
  // GET request with cookie; reuse postForm but with empty body
  if (!postForm(url, "", response, cookie)) {
    setError(BookStoreError::Network, "Failed to resolve download link");
    return BookStoreError::Network;
  }

  return parseDownloadLinkResponse(response.c_str(), response.size(), outUrl);
}
```

### `parseDownloadLinkResponse`

```cpp
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
```

### `downloadFile`

```cpp
BookStoreError BookStoreClient::downloadFile(const std::string& url, const std::string& destPath,
                                              ProgressCallback progress, bool* cancelFlag) {
  char cookie[128];
  std::snprintf(cookie, sizeof(cookie), "remix_userid=%s; remix_userkey=%s", userId, userKey);

  // HttpDownloader does not expose a custom Cookie header, so download directly with esp_http_client.
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.timeout_ms = 120000;
  config.buffer_size = 2048;
  config.buffer_size_tx = 1024;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    setError(BookStoreError::File, "Failed to init download client");
    return BookStoreError::File;
  }
  esp_http_client_set_header(client, "User-Agent", "CrossInk-ESP32-" CROSSINK_VERSION);
  esp_http_client_set_header(client, "Cookie", cookie);

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    LOG_ERR("BOOKSTORE", "Download request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    setError(BookStoreError::Network, "Download request failed");
    return BookStoreError::Network;
  }

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

  const int64_t totalLen = esp_http_client_get_content_length(client);
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
      Storage.deleteFile(destPath.c_str());
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
    Storage.deleteFile(destPath.c_str());
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
      Storage.deleteFile(destPath.c_str());
      setError(BookStoreError::Quota, "Download rejected by server");
      return BookStoreError::Quota;
    }
  }

  return BookStoreError::Ok;
}

- [ ] **Step 1: Implement `resolveDownloadUrl()`**

- [ ] **Step 2: Implement `parseDownloadLinkResponse()`**

- [ ] **Step 3: Implement `downloadFile()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because `BookStoreActivity` is still missing.

- [ ] **Step 5: Commit**

```bash
git add src/network/BookStoreClient.cpp
git commit -m "feat(network): implement BookStoreClient download"
```

---

## Task 10: Create `BookStoreActivity` header

**Files:**
- Create: `src/activities/network/BookStoreActivity.h`

```cpp
#pragma once

#include <BookStoreClient.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

enum class BookStoreState {
  MainMenu,
  Settings,
  SearchInput,
  Searching,
  ResultsList,
  BookDetails,
  ConfirmDownload,
  Downloading,
  DownloadDone,
  Error
};

class BookStoreActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  BookStoreState state = BookStoreState::MainMenu;

  // Main menu
  int mainMenuIndex = 0;
  static constexpr int MAIN_MENU_ITEM_COUNT = 2;

  // Settings
  int settingsIndex = 0;
  static constexpr int SETTINGS_ITEM_COUNT = 4;

  // Search input
  std::string searchQuery;

  // Results
  std::vector<BookStoreBook> books;
  int resultSelectedIndex = 0;
  uint32_t currentPage = 1;

  // Download
  BookStoreBook selectedBook;
  std::string downloadUrl;
  std::string downloadPath;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  bool cancelDownload = false;

  // Async client
  std::unique_ptr<BookStoreClient> client;

  // Error
  BookStoreError lastError = BookStoreError::Ok;

 public:
  explicit BookStoreActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BookStoreActivity", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void resetToMainMenu();
  void startSearch();
  void onSearchCompleted(BookStoreError err);
  void onDownloadLinkResolved(BookStoreError err);
  void onDownloadCompleted(BookStoreError err);
  void ensureClient();
  void buildDownloadPath();

  // Render helpers
  void renderMainMenu();
  void renderSettings();
  void renderSearchInput();
  void renderSearching();
  void renderResultsList();
  void renderBookDetails();
  void renderConfirmDownload();
  void renderDownloading();
  void renderDownloadDone();
  void renderError();
};
```

- [ ] **Step 1: Create `src/activities/network/BookStoreActivity.h`**

- [ ] **Step 2: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because `.cpp` is missing.

- [ ] **Step 3: Commit**

```bash
git add src/activities/network/BookStoreActivity.h
git commit -m "feat(activity): add BookStoreActivity header"
```

---

## Task 11: Implement `BookStoreActivity` skeleton and main menu

**Files:**
- Create: `src/activities/network/BookStoreActivity.cpp`

### Step 1: Skeleton

```cpp
#include "BookStoreActivity.h"

#include <I18n.h>
#include <Logging.h>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

void BookStoreActivity::onEnter() {
  Activity::onEnter();
  resetToMainMenu();
  ensureClient();
  requestUpdate();
}

void BookStoreActivity::onExit() {
  client.reset();
  Activity::onExit();
}

void BookStoreActivity::resetToMainMenu() {
  state = BookStoreState::MainMenu;
  mainMenuIndex = 0;
  resultSelectedIndex = 0;
  currentPage = 1;
  books.clear();
  books.shrink_to_fit();
}

void BookStoreActivity::ensureClient() {
  if (!client) {
    client = std::make_unique<BookStoreClient>();
    client->setBaseUrl(SETTINGS.bookStoreBaseUrl);
    client->setCredentials(SETTINGS.bookStoreEmail, SETTINGS.bookStorePassword);
  }
}
```

### Step 2: `loop()` main menu handling

```cpp
void BookStoreActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == BookStoreState::MainMenu) {
      finish();
    } else {
      resetToMainMenu();
      requestUpdate();
    }
    return;
  }

  switch (state) {
    case BookStoreState::MainMenu:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (mainMenuIndex == 0) {
          state = BookStoreState::SearchInput;
          searchQuery.clear();
        } else {
          state = BookStoreState::Settings;
          settingsIndex = 0;
        }
        requestUpdate();
      }
      buttonNavigator.onNext([this] {
        mainMenuIndex = ButtonNavigator::nextIndex(mainMenuIndex, MAIN_MENU_ITEM_COUNT);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        mainMenuIndex = ButtonNavigator::previousIndex(mainMenuIndex, MAIN_MENU_ITEM_COUNT);
        requestUpdate();
      });
      break;

    // Other states handled in later tasks
    default:
      break;
  }
}
```

### Step 3: `render()` dispatcher

```cpp
void BookStoreActivity::render(RenderLock&&) {
  renderer.clearScreen();

  switch (state) {
    case BookStoreState::MainMenu:
      renderMainMenu();
      break;
    case BookStoreState::Settings:
      renderSettings();
      break;
    case BookStoreState::SearchInput:
      renderSearchInput();
      break;
    case BookStoreState::Searching:
      renderSearching();
      break;
    case BookStoreState::ResultsList:
      renderResultsList();
      break;
    case BookStoreState::BookDetails:
      renderBookDetails();
      break;
    case BookStoreState::ConfirmDownload:
      renderConfirmDownload();
      break;
    case BookStoreState::Downloading:
      renderDownloading();
      break;
    case BookStoreState::DownloadDone:
      renderDownloadDone();
      break;
    case BookStoreState::Error:
      renderError();
      break;
  }

  renderer.displayBuffer();
}
```

### Step 4: `renderMainMenu`

```cpp
void BookStoreActivity::renderMainMenu() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  static constexpr StrId menuItems[MAIN_MENU_ITEM_COUNT] = {StrId::STR_BOOK_STORE_SEARCH,
                                                            StrId::STR_BOOK_STORE_SETTINGS};
  static constexpr StrId menuDescs[MAIN_MENU_ITEM_COUNT] = {StrId::STR_EMPTY, StrId::STR_EMPTY};
  static constexpr UIIcon menuIcons[MAIN_MENU_ITEM_COUNT] = {UIIcon::Book, UIIcon::Settings};

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, MAIN_MENU_ITEM_COUNT, mainMenuIndex,
               [](int index) { return std::string(I18N.get(menuItems[index])); },
               [](int index) { return std::string(I18N.get(menuDescs[index])); },
               [](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

Use `UIIcon::Book` for Search and `UIIcon::Settings` for Settings; both exist in `src/components/themes/BaseTheme.h`.

- [ ] **Step 1: Create skeleton, `onEnter`, `onExit`, `resetToMainMenu`, `ensureClient`**

- [ ] **Step 2: Implement `loop()` main menu branch**

- [ ] **Step 3: Implement `render()` dispatcher**

- [ ] **Step 4: Implement `renderMainMenu()`**

- [ ] **Step 5: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because other render methods are not defined.

- [ ] **Step 6: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStoreActivity main menu skeleton"
```

---

## Task 12: Implement settings UI

**Files:**
- Modify: `src/activities/network/BookStoreActivity.cpp`

### `loop()` settings branch

Add inside `loop()` switch, after `MainMenu`:

```cpp
    case BookStoreState::Settings:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        static constexpr StrId labels[SETTINGS_ITEM_COUNT] = {
            StrId::STR_BOOK_STORE_BASE_URL, StrId::STR_BOOK_STORE_EMAIL, StrId::STR_BOOK_STORE_PASSWORD,
            StrId::STR_BOOK_STORE_DOWNLOAD_PATH};
        char* target = nullptr;
        size_t maxLen = 0;
        InputType inputType = InputType::Text;
        switch (settingsIndex) {
          case 0:
            target = SETTINGS.bookStoreBaseUrl;
            maxLen = sizeof(SETTINGS.bookStoreBaseUrl);
            inputType = InputType::Url;
            break;
          case 1:
            target = SETTINGS.bookStoreEmail;
            maxLen = sizeof(SETTINGS.bookStoreEmail);
            inputType = InputType::Text;
            break;
          case 2:
            target = SETTINGS.bookStorePassword;
            maxLen = sizeof(SETTINGS.bookStorePassword);
            inputType = InputType::Text;
            break;
          case 3:
            target = SETTINGS.bookStoreDownloadPath;
            maxLen = sizeof(SETTINGS.bookStoreDownloadPath);
            inputType = InputType::Text;
            break;
        }
        if (target) {
          startActivityForResult(
              std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, I18N.get(labels[settingsIndex]),
                                                      std::string(target), maxLen, inputType),
              [this, target, maxLen](const ActivityResult& result) {
                if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
                  const auto& keyboardResult = std::get<KeyboardResult>(result.data);
                  std::strncpy(target, keyboardResult.text.c_str(), maxLen - 1);
                  target[maxLen - 1] = '\0';
                  SETTINGS.saveToFile();
                }
                requestUpdate();
              });
        }
      }
      buttonNavigator.onNext([this] {
        settingsIndex = ButtonNavigator::nextIndex(settingsIndex, SETTINGS_ITEM_COUNT);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        settingsIndex = ButtonNavigator::previousIndex(settingsIndex, SETTINGS_ITEM_COUNT);
        requestUpdate();
      });
      break;
```

For a real implementation, use the project's existing text-input dialog. Look for `InputDialogActivity`, `TextInputActivity`, or similar. If none exists, implement inline editing using Up/Down to cycle characters and Confirm to advance. The plan here intentionally defers the exact mechanism to the implementer because it depends on existing UI components.

### `renderSettings`

```cpp
void BookStoreActivity::renderSettings() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE_SETTINGS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  static constexpr StrId labels[SETTINGS_ITEM_COUNT] = {
      StrId::STR_BOOK_STORE_BASE_URL, StrId::STR_BOOK_STORE_EMAIL, StrId::STR_BOOK_STORE_PASSWORD,
      StrId::STR_BOOK_STORE_DOWNLOAD_PATH};
  const char* values[SETTINGS_ITEM_COUNT] = {SETTINGS.bookStoreBaseUrl, SETTINGS.bookStoreEmail,
                                             SETTINGS.bookStorePassword, SETTINGS.bookStoreDownloadPath};

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, SETTINGS_ITEM_COUNT, settingsIndex,
               [labels](int index) { return std::string(I18N.get(labels[index])); },
               [values](int index) { return std::string(values[index]); },
               [](int) { return UIIcon::None; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

- [ ] **Step 1: Add settings branch to `loop()`**

- [ ] **Step 2: Implement `renderSettings()`**

- [ ] **Step 3: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: fails because other states still missing.

- [ ] **Step 4: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStore settings screen"
```

---

## Task 13: Implement search input and searching states

**Files:**
- Modify: `src/activities/network/BookStoreActivity.cpp`

### `loop()` search input branch

```cpp
    case BookStoreState::SearchInput:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        startActivityForResult(
            std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BOOK_STORE_SEARCH), searchQuery, 64,
                                                    InputType::Text),
            [this](const ActivityResult& result) {
              if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
                const auto& keyboardResult = std::get<KeyboardResult>(result.data);
                searchQuery = keyboardResult.text;
                if (!searchQuery.empty()) {
                  startSearch();
                } else {
                  requestUpdate();
                }
              } else {
                resetToMainMenu();
                requestUpdate();
              }
            });
      }
      break;
```

Replace the hard-coded query with real text input before finalizing.

### `startSearch`

```cpp
void BookStoreActivity::startSearch() {
  state = BookStoreState::Searching;
  books.clear();
  resultSelectedIndex = 0;
  requestUpdate();

  ensureClient();
  BookStoreError err = client->search(searchQuery.c_str(), currentPage, books);
  onSearchCompleted(err);
}

void BookStoreActivity::onSearchCompleted(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
  } else {
    state = BookStoreState::ResultsList;
  }
  requestUpdate();
}
```

### Render methods

```cpp
void BookStoreActivity::renderSearchInput() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE_SEARCH));

  // Centered prompt
  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  GUI.drawText(renderer, tr(STR_BOOK_STORE_SEARCH), Rect{0, y, pageWidth, metrics.lineHeight * 2},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderSearching() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  GUI.drawText(renderer, tr(STR_LOADING), Rect{0, y, pageWidth, metrics.lineHeight * 2},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_EMPTY), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

- [ ] **Step 1: Add search input branch to `loop()`**

- [ ] **Step 2: Implement `startSearch()` and `onSearchCompleted()`**

- [ ] **Step 3: Implement `renderSearchInput()` and `renderSearching()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: still fails because result/details/download states missing.

- [ ] **Step 5: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStore search flow"
```

---

## Task 14: Implement results list

**Files:**
- Modify: `src/activities/network/BookStoreActivity.cpp`

### `loop()` results list branch

```cpp
    case BookStoreState::ResultsList:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (resultSelectedIndex >= 0 && resultSelectedIndex < static_cast<int>(books.size())) {
          selectedBook = books[resultSelectedIndex];
          state = BookStoreState::BookDetails;
          requestUpdate();
        }
      }
      buttonNavigator.onNext([this] {
        resultSelectedIndex = ButtonNavigator::nextIndex(resultSelectedIndex, static_cast<int>(books.size()));
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        resultSelectedIndex = ButtonNavigator::previousIndex(resultSelectedIndex, static_cast<int>(books.size()));
        requestUpdate();
      });
      if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
        currentPage++;
        startSearch();
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
        if (currentPage > 1) {
          currentPage--;
          startSearch();
        }
      }
      break;
```

### `renderResultsList`

```cpp
void BookStoreActivity::renderResultsList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  char header[64];
  std::snprintf(header, sizeof(header), "%s (%lu)", I18N.get(StrId::STR_BOOK_STORE_SEARCH), currentPage);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(books.size()),
               resultSelectedIndex,
               [this](int index) {
                 return std::string(books[index].title);
               },
               [this](int index) {
                 char buf[128];
                 std::snprintf(buf, sizeof(buf), "%s / %s / %s", books[index].author, books[index].extension,
                               books[index].filesizeString);
                 return std::string(buf);
               },
               [](int) { return UIIcon::Book; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

- [ ] **Step 1: Add results list branch to `loop()`**

- [ ] **Step 2: Implement `renderResultsList()`**

- [ ] **Step 3: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: still fails because remaining states missing.

- [ ] **Step 4: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStore results list"
```

---

## Task 15: Implement book details and confirm download

**Files:**
- Modify: `src/activities/network/BookStoreActivity.cpp`

### `loop()` book details branch

```cpp
    case BookStoreState::BookDetails:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        state = BookStoreState::ConfirmDownload;
        requestUpdate();
      }
      break;

    case BookStoreState::ConfirmDownload:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        state = BookStoreState::Downloading;
        downloadProgress = 0;
        downloadTotal = 0;
        cancelDownload = false;
        requestUpdate();

        ensureClient();
        BookStoreError err = client->resolveDownloadUrl(selectedBook, downloadUrl);
        onDownloadLinkResolved(err);
      } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        state = BookStoreState::BookDetails;
        requestUpdate();
      }
      break;
```

### `onDownloadLinkResolved`

```cpp
void BookStoreActivity::onDownloadLinkResolved(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
    requestUpdate();
    return;
  }

  buildDownloadPath();
  BookStoreError downloadErr = client->downloadFile(
      downloadUrl, downloadPath,
      [this](size_t downloaded, size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
      },
      &cancelDownload);
  onDownloadCompleted(downloadErr);
}

void BookStoreActivity::buildDownloadPath() {
  char fileName[160];
  std::snprintf(fileName, sizeof(fileName), "%s_%s.%s", selectedBook.title, selectedBook.id,
                selectedBook.extension);
  // Sanitize filename: replace path separators and spaces
  for (size_t i = 0; fileName[i] != '\0'; i++) {
    if (fileName[i] == '/' || fileName[i] == '\\' || fileName[i] == ' ') {
      fileName[i] = '_';
    }
  }

  downloadPath = std::string(SETTINGS.bookStoreDownloadPath) + "/" + fileName;
}
```

### `onDownloadCompleted`

```cpp
void BookStoreActivity::onDownloadCompleted(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
  } else {
    state = BookStoreState::DownloadDone;
  }
  requestUpdate();
}
```

### Render methods

```cpp
void BookStoreActivity::renderBookDetails() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s\n%s\n%s, %s\n%s", selectedBook.title, selectedBook.author,
                selectedBook.extension, selectedBook.filesizeString, selectedBook.language);

  GUI.drawText(renderer, buf, Rect{metrics.sidePadding, contentTop, pageWidth - metrics.sidePadding * 2, contentHeight},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::LEFT);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BOOK_STORE_DOWNLOADING), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderConfirmDownload() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  char msg[192];
  std::snprintf(msg, sizeof(msg), "%s\n%s (%s, %s)", I18N.get(StrId::STR_BOOK_STORE_CONFIRM_DOWNLOAD),
                selectedBook.title, selectedBook.extension, selectedBook.filesizeString);

  GUI.drawText(renderer, msg, Rect{0, y, pageWidth, metrics.lineHeight * 4},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

- [ ] **Step 1: Add book details and confirm download branches to `loop()`**

- [ ] **Step 2: Implement `onDownloadLinkResolved()`, `buildDownloadPath()`, and `onDownloadCompleted()`**

- [ ] **Step 3: Implement `renderBookDetails()` and `renderConfirmDownload()`**

- [ ] **Step 4: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: still fails because downloading/done/error states missing.

- [ ] **Step 5: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStore details and confirm download"
```

---

## Task 16: Implement downloading, download done, and error states

**Files:**
- Modify: `src/activities/network/BookStoreActivity.cpp`

### `loop()` downloading/done/error branches

```cpp
    case BookStoreState::Downloading:
      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        cancelDownload = true;
      }
      break;

    case BookStoreState::DownloadDone:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        activityManager.goToReader(downloadPath, false);
      }
      break;

    case BookStoreState::Error:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
          mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        resetToMainMenu();
        requestUpdate();
      }
      break;
```

### Render methods

```cpp
void BookStoreActivity::renderDownloading() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  char msg[128];
  if (downloadTotal > 0) {
    std::snprintf(msg, sizeof(msg), "%s\n%zu / %zu bytes", I18N.get(StrId::STR_BOOK_STORE_DOWNLOADING),
                  downloadProgress, downloadTotal);
  } else {
    std::snprintf(msg, sizeof(msg), "%s\n%zu bytes", I18N.get(StrId::STR_BOOK_STORE_DOWNLOADING), downloadProgress);
  }

  GUI.drawText(renderer, msg, Rect{0, y, pageWidth, metrics.lineHeight * 3},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_EMPTY), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderDownloadDone() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  GUI.drawText(renderer, tr(STR_BOOK_STORE_DOWNLOAD_COMPLETE), Rect{0, y, pageWidth, metrics.lineHeight * 2},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BOOK_STORE_OPEN_BOOK), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderError() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - metrics.lineHeight;
  const char* msg = tr(STR_BOOK_STORE_DOWNLOAD_FAILED);
  switch (lastError) {
    case BookStoreError::Auth:
      msg = tr(STR_BOOK_STORE_LOGIN_FAILED);
      break;
    case BookStoreError::Quota:
      msg = tr(STR_BOOK_STORE_QUOTA_REACHED);
      break;
    case BookStoreError::NotFound:
      msg = tr(STR_BOOK_STORE_NO_RESULTS);
      break;
    case BookStoreError::Network:
      msg = tr(STR_BOOK_STORE_NO_WIFI);
      break;
    case BookStoreError::File:
      msg = tr(STR_BOOK_STORE_SAVE_FAILED);
      break;
    case BookStoreError::Cancelled:
      msg = tr(STR_BOOK_STORE_CANCELED);
      break;
    default:
      break;
  }

  GUI.drawText(renderer, msg, Rect{0, y, pageWidth, metrics.lineHeight * 2},
               EpdFontFamily::Style::REGULAR, UITheme::getInstance().getBodyColor(), TextAlign::CENTER);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OK), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
```

- [ ] **Step 1: Add downloading/done/error branches to `loop()`**

- [ ] **Step 2: Implement `renderDownloading()`, `renderDownloadDone()`, and `renderError()`**

- [ ] **Step 3: Build simulator**

Run:
```bash
pio run -e simulator
```

Expected: first successful simulator build for the feature.

- [ ] **Step 4: Commit**

```bash
git add src/activities/network/BookStoreActivity.cpp
git commit -m "feat(activity): add BookStore download and error states"
```

---

## Task 17: Full simulator build and run

**Files:**
- None

- [ ] **Step 1: Build the simulator**

Run:
```bash
pio run -e simulator
```

Expected: build succeeds.

- [ ] **Step 2: Run the simulator**

Run:
```bash
pio run -e simulator --target upload
```

or, if a run target is configured:
```bash
pio run -e simulator --target exec
```

Expected: simulator window opens.

- [ ] **Step 3: Manual UI check**

1. Navigate to Home → File Transfer → Book Store.
2. Verify the Book Store main menu shows "Search" and "Settings".
3. Open Settings and verify the four fields are listed.
4. Open Search and confirm it transitions to Searching.
5. Since the simulator cannot reach Z-library, expect an Error screen.

- [ ] **Step 4: Commit if any simulator fixes were needed**

```bash
git commit -am "fix(book-store): simulator build issues"
```

---

## Task 18: Hardware build and static analysis

**Files:**
- None

- [ ] **Step 1: Build the default/tiny firmware target**

Run:
```bash
pio run -e tiny
```

Expected: build succeeds.

- [ ] **Step 2: Run static analysis**

Run:
```bash
pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high
```

Expected: no new defects introduced by Book Store code.

- [ ] **Step 3: Format touched files**

Run:
```bash
./bin/clang-format-fix
```

Expected: formatting applied.

- [ ] **Step 4: Commit formatting/analysis fixes**

```bash
git commit -am "style(book-store): apply clang-format and fix static analysis"
```

---

## Task 19: Hardware verification

**Files:**
- None

- [ ] **Step 1: Flash the firmware to an Xteink X4 or X3**

Run:
```bash
pio run -e tiny --target upload
```

- [ ] **Step 2: Connect the device to Wi-Fi**

Use the existing Wi-Fi settings flow.

- [ ] **Step 3: Configure Book Store settings**

Set base URL, email, password, and download folder.

- [ ] **Step 4: Search for a known book**

Confirm 5 results appear and pagination works.

- [ ] **Step 5: Download a small EPUB**

Confirm download progress is shown, the file is saved to the configured folder, and the "Open book" option works.

- [ ] **Step 6: Monitor serial logs and heap**

Run:
```bash
pio device monitor
```

Look for `LOG_INF` lines showing login success, search result count, and download completion. Check `ESP.getFreeHeap()` after search to confirm remaining heap is healthy.

- [ ] **Step 7: Commit any hardware fixes**

```bash
git commit -am "fix(book-store): hardware testing fixes"
```

---

## Spec coverage check

| Spec section | Plan task(s) |
|---|---|
| I18n strings | Task 1 |
| Settings persistence | Task 2 |
| ActivityManager launcher | Task 3 |
| Transfer menu entry | Task 4 |
| BookStoreClient architecture | Tasks 5–9 |
| Activity state machine | Tasks 10–16 |
| Memory budget (5 results, bounded buffers) | Tasks 8, 10 |
| Error handling | Tasks 8, 9, 16 |
| Verification plan | Tasks 17–19 |

---

## Known open questions

1. **Simulator `esp_http_client`:** The `postForm` helper and `downloadFile` use ESP-IDF APIs. If the simulator target does not provide them, add `#ifndef SIMULATOR` stubs that return errors so the UI can still be navigated without real network calls.
