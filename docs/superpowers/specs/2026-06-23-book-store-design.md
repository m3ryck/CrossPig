# Book Store — Design Specification

**Date:** 2026-06-23  
**Status:** Approved  
**Target:** Xteink X3 / X4 (ESP32-C3)  
**Scope:** Personal fork only; not suitable for upstream CrossInk

---

## 1. Summary

Add a **Book Store** feature to the firmware that lets the user search and download books directly to the SD card. The first implementation targets the Z-library internal API (as used by the KOReader `zlibrary.koplugin`), but the UI and settings are named generically so the feature is not branded to Z-library.

The feature is intentionally simplified compared to the KOReader plugin:

- No book covers.
- No favorites, "My books", recommendations, or "most popular".
- No multiple simultaneous downloads.
- Search results are paginated and limited to 5 books per page to fit ESP32-C3 RAM constraints.

---

## 2. User Flow

1. From the home screen, select **File Transfer**.
2. In the transfer menu, select the new **Book Store** option.
3. On the Book Store main screen, choose:
   - **Search** — enter a query.
   - **Settings** — configure connection and download options.
4. In **Settings**, configure:
   - Base URL
   - Account email
   - Account password (visible while typing)
   - Download folder (default: `/downloads`)
5. After searching, a paginated list of up to 5 books is shown.
   - Text input uses the firmware's existing on-screen keyboard / text input mechanism.
6. Selecting a book opens a detail view.
7. Confirming download shows progress and saves the file to the configured folder.
8. On completion, the user may open the book or return.

The physical **Back** button is used to navigate back at any time.

---

## 3. Architecture

```
+----------------------------------+
| BookStoreActivity                |
|  - UI state machine              |
|  - Input handling                |
|  - Rendering via UITheme/GUI     |
+----------------------------------+
                |
                v
+----------------------------------+
| BookStoreClient                  |
|  - URL building                  |
|  - Login (cookie session)        |
|  - Search (POST, limit=5)        |
|  - Download link resolution      |
|  - Streaming download            |
+----------------------------------+
                |
                v
+----------------------------------+
| HttpDownloader / esp_http_client |
|  - TLS via crt_bundle_attach     |
|  - Chunked download to SD        |
+----------------------------------+
```

### Components

| Component | File | Responsibility |
|---|---|---|
| `BookStoreClient` | `src/network/BookStoreClient.h`, `.cpp` | Network client for the Z-library API. No UI dependencies. |
| `BookStoreActivity` | `src/activities/network/BookStoreActivity.h`, `.cpp` | Activity state machine, input, rendering. |
| Settings fields | `src/CrossPointSettings.h`, `src/SettingsList.h` | Persist URL, email, password, download path. |
| Transfer menu | `src/activities/network/NetworkModeSelectionActivity.cpp` | Adds the Book Store entry. |
| Activity launcher | `src/activities/ActivityManager.h`, `.cpp` | Adds `goToBookStore()`. |
| I18n strings | `lib/I18n/translations/en.yaml` | User-facing strings; other languages fall back to English. |

---

## 4. Network API

The client targets the same internal JSON endpoints used by the KOReader plugin:

| Operation | Method | Path |
|---|---|---|
| Health check | GET | `<base>/eapi/info/ok` |
| Login | POST | `<base>/eapi/user/login` |
| Search | POST | `<base>/eapi/book/search` |
| Resolve download | GET | `<base>/eapi/book/<id>/<hash>/file` |
| Download file | GET | URL returned by resolve step |

### Authentication

- Login POSTs `email` and `password` as `application/x-www-form-urlencoded`.
- On success, the response contains `user.id` and `user.remix_userkey`.
- Subsequent requests include the cookie header:
  ```
  Cookie: remix_userid=<id>; remix_userkey=<key>
  ```
- The session is held in memory only; it is rebuilt on each activity entry.

### Search request body

```
message=<query>
page=<page>
limit=5
order=bestmatch
```

Optional filters (extensions, languages) are out of scope for v1.

### Response handling

- Responses are JSON objects with `data.success == 1` on success.
- Search results are in `data.books` or `data.exactMatch.books`.
- Each book is normalized into a fixed-size `BookStoreBook` struct.
- JSON is parsed in chunks using the project's existing streaming parser infrastructure to avoid large `ArduinoJson` documents.

---

## 5. Data Structures

```cpp
struct BookStoreBook {
  char id[32];
  char hash[32];
  char title[128];
  char author[128];
  char extension[8];
  char language[16];
  char filesizeString[16];
  uint32_t year;
};
```

Per-book size is approximately 360 bytes. With a page size of 5, the result list consumes ~1.8 KB of heap.

---

## 6. Activity States

```cpp
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
```

### Input Mapping

| Button | Behavior |
|---|---|
| `Back` | Go back / cancel current operation. |
| `Confirm` | Select item / confirm action. |
| `Up` / `Down` | Navigate lists. |
| `PageForward` / `PageBack` | Next / previous search page. |

---

## 7. Settings and Persistence

Four new fields are added to `CrossPointSettings`:

```cpp
char bookStoreBaseUrl[128] = {0};
char bookStoreEmail[64] = {0};
char bookStorePassword[64] = {0};
char bookStoreDownloadPath[64] = "/downloads";
```

All are registered in `SettingsList.h` as strings and persisted to `/settings.json` on the SD card. The password is stored in plaintext, matching the behavior of the KOReader plugin and fitting the constraints of a device without a secure enclave.

---

## 8. Memory Budget and Constraints

| Allocation | Size | Strategy |
|---|---|---|
| JSON parse buffer | 4 KB | Allocate once in `onEnter`, reuse. |
| Search result vector | ~2 KB (5 books) | `reserve(5)` before populating. |
| HTTP download buffer | 2 KB | Reuse `HttpDownloader` default. |
| TLS connection | 30–50 KB | Managed by `esp_http_client`; only one connection active at a time. |
| Session / credentials | ~300 B | Fixed `char[]` members in client. |

### Decisions to contain RAM usage

- Limit search page size to 5 books.
- Parse JSON incrementally; never materialize the full HTTP body.
- No covers, no parallel downloads, no large metadata caching.
- Free the search vector in `onExit` or when leaving the results list.

---

## 9. Error Handling

| Condition | User-Facing Message | Behavior |
|---|---|---|
| Wi-Fi not connected | `STR_BOOK_STORE_NO_WIFI` | Show error; suggest opening network settings. |
| Login failed | `STR_BOOK_STORE_LOGIN_FAILED` | Return to settings. |
| Search returned no results | `STR_BOOK_STORE_NO_RESULTS` | Stay on search input. |
| Download quota reached | `STR_BOOK_STORE_QUOTA_REACHED` | Return to results. |
| Download returned HTML error page | `STR_BOOK_STORE_DOWNLOAD_FAILED` | Delete partial file. |
| SD write error | `STR_BOOK_STORE_SAVE_FAILED` | Delete partial file. |
| Timeout / cancelled | `STR_BOOK_STORE_CANCELLED` | Return to results. |

All errors are logged with `LOG_ERR` before returning failure.

---

## 10. Out of Scope (v1)

The following features are explicitly deferred to keep RAM usage and complexity low:

- Book covers and other images.
- Favorites, "My books", recommendations, "most popular".
- Multiple downloads at once.
- Extension/language filters.
- In-book reading progress sync with the store.
- Auto-discovery of working mirrors.
- Proxy or generic API support.
- Encrypted credential storage.

---

## 11. Files Modified and Added

### New files

- `src/network/BookStoreClient.h`
- `src/network/BookStoreClient.cpp`
- `src/activities/network/BookStoreActivity.h`
- `src/activities/network/BookStoreActivity.cpp`

### Modified files

- `src/activities/ActivityManager.h`
- `src/activities/ActivityManager.cpp`
- `src/activities/network/NetworkModeSelectionActivity.cpp`
- `src/CrossPointSettings.h`
- `src/SettingsList.h`
- `lib/I18n/translations/en.yaml`

---

## 12. Verification Plan

### Simulator

1. `pio run -e simulator` builds without errors.
2. Open Book Store from File Transfer menu.
3. Configure base URL, email, password, and download path.
4. Search for a known book.
5. Confirm 5 results appear and pagination works.
6. Download a small EPUB and verify it appears in the configured folder.

### Hardware (Xteink X4/X3)

1. `pio run -e tiny --target upload` flashes successfully.
2. Connect to Wi-Fi.
3. Repeat simulator steps 3–6.
4. Check serial logs for login success, search response, and download completion.
5. Check `ESP.getFreeHeap()` after search to confirm remaining heap is healthy.
6. Run `./bin/clang-format-fix` and `pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high`.

---

## 13. Scope and Governance Note

This feature implements active client connectivity to a third-party, undocumented API. It is appropriate for a personal fork but is **not aligned with the upstream CrossInk `SCOPE.md` guidelines** regarding simple, pull-based local transfer and avoidance of active connectivity. It should not be submitted as a pull request to the upstream CrossInk repository without prior discussion.
