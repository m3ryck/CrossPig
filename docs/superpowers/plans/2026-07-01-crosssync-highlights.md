# CrossSync Highlights Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add CrossSync highlight synchronization to CrossInk, driven by the existing Progress Sync direction prompt and backed by separate WebDAV credentials.

**Architecture:** Progress Sync remains the user-facing orchestrator. CrossSync adds a separate settings store, a bounded highlight JSON document, a small WebDAV client, and a highlight service that either replaces local highlights from remote JSON or replaces remote JSON from local highlights. The current KOReader document matching strategy is reused for remote path identity.

**Tech Stack:** C++20, PlatformIO, ArduinoJson 7, ESP32 `HTTPClient`, SdFat/HAL storage, existing CrossInk activity/settings/i18n patterns, host CMake/gtest for pure serialization/path tests.

---

## Files

- Create: `lib/CrossSync/CrossSyncTypes.h`
- Create: `lib/CrossSync/CrossSyncHighlightDocument.h`
- Create: `lib/CrossSync/CrossSyncHighlightDocument.cpp`
- Create: `lib/CrossSync/CrossSyncCredentialStore.h`
- Create: `lib/CrossSync/CrossSyncCredentialStore.cpp`
- Create: `lib/CrossSync/CrossSyncJsonIO.h`
- Create: `lib/CrossSync/CrossSyncJsonIO.cpp`
- Create: `lib/CrossSync/CrossSyncWebDavClient.h`
- Create: `lib/CrossSync/CrossSyncWebDavClient.cpp`
- Create: `lib/CrossSync/CrossSyncHighlightService.h`
- Create: `lib/CrossSync/CrossSyncHighlightService.cpp`
- Create: `test/crosssync_highlight_document/CMakeLists.txt`
- Create: `test/crosssync_highlight_document/CrossSyncHighlightDocumentTest.cpp`
- Modify: `test/CMakeLists.txt`
- Modify: `src/ClippingStore.h`
- Modify: `src/ClippingStore.cpp`
- Modify: `src/activities/reader/KOReaderSyncActivity.h`
- Modify: `src/activities/reader/KOReaderSyncActivity.cpp`
- Modify: `src/activities/settings/KOReaderSettingsActivity.cpp` or add a dedicated CrossSync settings activity if the existing list becomes unclear
- Modify: `src/SettingsList.h`
- Modify: `src/main.cpp`
- Modify: `lib/I18n/translations/*.yaml`
- Modify generated i18n files through `scripts/gen_i18n.py`
- Modify: `CHANGELOG.md`
- Modify: `docs/file-formats.md`
- Modify: `docs/data-cache.md`

## Task 1: CrossSync Highlight JSON Model

**Files:**
- Create: `lib/CrossSync/CrossSyncTypes.h`
- Create: `lib/CrossSync/CrossSyncHighlightDocument.h`
- Create: `lib/CrossSync/CrossSyncHighlightDocument.cpp`
- Create: `test/crosssync_highlight_document/CMakeLists.txt`
- Create: `test/crosssync_highlight_document/CrossSyncHighlightDocumentTest.cpp`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Create tests for these behaviors:

```cpp
TEST(CrossSyncHighlightDocument, SerializesBoundedHighlightDocument) {
  CrossSyncHighlightDocument doc;
  doc.book = "abc123";
  doc.matchType = CrossSyncMatchType::Filename;
  CrossSyncHighlight h;
  h.spine = 2;
  h.startPage = 3;
  h.endPage = 4;
  h.pageCount = 20;
  h.startWord = 5;
  h.endWord = 8;
  h.wordCount = 4;
  h.paragraph = 9;
  h.createdAt = 123456;
  h.chapter = "Chapter";
  h.text = "Selected text";
  doc.highlights.push_back(h);

  std::string json;
  ASSERT_TRUE(CrossSyncHighlightJson::serialize(doc, json));

  EXPECT_NE(json.find("\"format\":\"crosssync.highlights\""), std::string::npos);
  EXPECT_NE(json.find("\"match_type\":\"filename\""), std::string::npos);
  EXPECT_NE(json.find("\"text\":\"Selected text\""), std::string::npos);
}

TEST(CrossSyncHighlightDocument, ParsesValidDocument) {
  const char* json = R"JSON({
    "format":"crosssync.highlights",
    "version":1,
    "book":"abc123",
    "match_type":"binary",
    "source":"koreader",
    "highlights":[{"spine":1,"start_page":2,"end_page":2,"page_count":10,"start_word":3,"end_word":5,"word_count":3,"paragraph":7,"chapter":"C","text":"T","created_at":99}]
  })JSON";

  CrossSyncHighlightDocument doc;
  ASSERT_TRUE(CrossSyncHighlightJson::parse(json, doc));
  EXPECT_EQ(doc.book, "abc123");
  EXPECT_EQ(doc.matchType, CrossSyncMatchType::Binary);
  ASSERT_EQ(doc.highlights.size(), 1u);
  EXPECT_EQ(doc.highlights[0].text, "T");
}

TEST(CrossSyncHighlightDocument, RejectsWrongVersionAndTooManyHighlights) {
  std::string json = R"JSON({"format":"crosssync.highlights","version":2,"book":"abc","match_type":"filename","highlights":[]})JSON";
  CrossSyncHighlightDocument doc;
  EXPECT_FALSE(CrossSyncHighlightJson::parse(json, doc));

  json = R"JSON({"format":"crosssync.highlights","version":1,"book":"abc","match_type":"filename","highlights":[)JSON";
  for (int i = 0; i < CLIPPING_MAX_PER_BOOK + 1; ++i) {
    if (i > 0) json += ",";
    json += R"JSON({"text":"x"})JSON";
  }
  json += "]}";
  EXPECT_FALSE(CrossSyncHighlightJson::parse(json, doc));
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake -S test -B /tmp/crosspig-tests
cmake --build /tmp/crosspig-tests --target crosssync_highlight_document_test
/tmp/crosspig-tests/crosssync_highlight_document/crosssync_highlight_document_test
```

Expected: compile failure because `CrossSyncHighlightDocument` does not exist.

- [ ] **Step 3: Implement minimal model and parser**

Add bounded structs, match-type conversion, `serialize()`, and `parse()`. Validation must reject unknown format, version other than `1`, unknown match type, missing book, and highlight counts above `CLIPPING_MAX_PER_BOOK`. Truncate parsed `chapter` and `text` to existing clipping limits.

- [ ] **Step 4: Run tests to verify pass**

Run the same CMake commands. Expected: all CrossSync JSON tests pass.

## Task 2: CrossSync Settings Store

**Files:**
- Create: `lib/CrossSync/CrossSyncCredentialStore.h`
- Create: `lib/CrossSync/CrossSyncCredentialStore.cpp`
- Create: `lib/CrossSync/CrossSyncJsonIO.h`
- Create: `lib/CrossSync/CrossSyncJsonIO.cpp`

- [ ] **Step 1: Write failing tests or compile-only smoke**

If host stubs for HAL storage are not available, use a simulator compile as the first failing check after adding includes in Task 4. The desired API is:

```cpp
CrossSyncCredentialStore& store = CROSSSYNC_STORE;
store.setEnabled(true);
store.setServerUrl("https://example.com/dav");
store.setRootPath("/CrossSync");
store.setCredentials("user", "pass");
EXPECT_TRUE(store.hasConfig());
EXPECT_EQ(store.getBaseUrl(), "https://example.com/dav");
```

- [ ] **Step 2: Implement store**

Store fields:

```cpp
bool enabled = false;
std::string serverUrl;
std::string rootPath = "/CrossSync";
std::string username;
std::string password;
```

Persist to `/.crosspoint/crosssync.json`. Use existing obfuscation helpers for the password, matching current settings patterns.

- [ ] **Step 3: Add load at boot**

Include the store in `src/main.cpp` and call `CROSSSYNC_STORE.loadFromFile()` near the current `KOREADER_STORE.loadFromFile()` boot path.

## Task 3: ClippingStore Replace-All API

**Files:**
- Modify: `src/ClippingStore.h`
- Modify: `src/ClippingStore.cpp`

- [ ] **Step 1: Write failing compile usage**

Add service code in Task 5 expecting:

```cpp
bool replaceAllForSync(const std::vector<Clipping>& incoming);
```

- [ ] **Step 2: Implement API**

Rules:

- If `incoming.size() > CLIPPING_MAX_PER_BOOK`, log and return false.
- Clear current `clippings`.
- Reserve once if needed.
- Copy only bounded fields.
- Mark dirty.
- Save immediately with existing `saveToFile()`.

## Task 4: WebDAV Client

**Files:**
- Create: `lib/CrossSync/CrossSyncWebDavClient.h`
- Create: `lib/CrossSync/CrossSyncWebDavClient.cpp`

- [ ] **Step 1: Define API**

```cpp
class CrossSyncWebDavClient {
 public:
  enum class Error : uint8_t { OK, NotConfigured, NotFound, NetworkError, AuthFailed, ServerError, LowMemory };
  static Error ensureCollectionPath(const std::string& path);
  static Error getTextFile(const std::string& remotePath, std::string& out);
  static Error putTextFile(const std::string& remotePath, const std::string& body);
  static const char* errorString(Error error);
};
```

- [ ] **Step 2: Implement with HTTPClient**

Use transient `HTTPClient`. Set Basic auth from CrossSync credentials. Implement:

- `MKCOL` for each path segment in `<rootPath>/v1/highlights/<match>`.
- `GET` remote JSON; `404` maps to `NotFound`.
- `PUT` JSON body; `2xx` maps to `OK`.

Use small HTTP timeouts and log before returning failures.

## Task 5: Highlight Service

**Files:**
- Create: `lib/CrossSync/CrossSyncHighlightService.h`
- Create: `lib/CrossSync/CrossSyncHighlightService.cpp`

- [ ] **Step 1: Define direction API**

```cpp
enum class CrossSyncDirection : uint8_t {
  PullRemote,
  UploadLocal,
};

class CrossSyncHighlightService {
 public:
  enum class Result : uint8_t { Skipped, OK, NotConfigured, RemoteMissingAppliedAsEmpty, ParseError, NetworkError, LocalStoreError };
  static Result sync(CrossSyncDirection direction, const std::string& documentHash, DocumentMatchMethod matchMethod);
  static const char* resultString(Result result);
};
```

- [ ] **Step 2: Implement upload local**

Read `CLIPPINGS.getClippings()`, build `CrossSyncHighlightDocument`, serialize, ensure remote directory, and `PUT` the JSON file.

- [ ] **Step 3: Implement pull remote**

`GET` remote JSON. If missing, call `CLIPPINGS.replaceAllForSync({})` and return `RemoteMissingAppliedAsEmpty`. If present, parse, convert highlights to `Clipping`, and replace local store.

## Task 6: Progress Sync Integration

**Files:**
- Modify: `src/activities/reader/KOReaderSyncActivity.h`
- Modify: `src/activities/reader/KOReaderSyncActivity.cpp`

- [ ] **Step 1: Add shared direction hooks**

Change confirm handling:

- `selectedOption == 0`: call `performCrossSync(PullRemote)` before or after `saveProgressAndReturn(remotePosition)`.
- `selectedOption == 1`: after progress upload success, call `performCrossSync(UploadLocal)` before showing upload complete.

For no remote progress, upload local highlights after progress upload success.

- [ ] **Step 2: Error behavior**

If CrossSync is disabled, do nothing. If enabled and fails, show a translated CrossSync warning but do not roll back completed progress sync.

## Task 7: Settings UI And Web Settings

**Files:**
- Modify: `src/activities/settings/KOReaderSettingsActivity.cpp`
- Modify: `src/SettingsList.h`
- Modify: `lib/I18n/translations/*.yaml`

- [ ] **Step 1: Add settings**

Add these settings under the sync category:

- Enable CrossSync Highlights
- CrossSync WebDAV URL
- CrossSync Root Path
- CrossSync Username
- CrossSync Password

Do not add a separate provider picker until there is a second provider.

- [ ] **Step 2: Generate i18n**

Run:

```bash
python3 scripts/gen_i18n.py
```

Expected: generated i18n files update from YAML.

## Task 8: Docs And Changelog

**Files:**
- Modify: `docs/file-formats.md`
- Modify: `docs/data-cache.md`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Document CrossSync JSON**

Add the V1 JSON schema and remote path layout.

- [ ] **Step 2: Changelog**

Add a user-facing entry under `Added` or `Changed`, depending on current unreleased structure.

## Task 9: Verification

- [ ] **Step 1: Host tests**

Run:

```bash
cmake -S test -B /tmp/crosspig-tests
cmake --build /tmp/crosspig-tests
ctest --test-dir /tmp/crosspig-tests --output-on-failure
```

- [ ] **Step 2: Simulator build**

Run:

```bash
pio run -e simulator
```

- [ ] **Step 3: Firmware build**

Run:

```bash
pio run -e default
```

- [ ] **Step 4: Hardware verification**

On Xteink X4:

1. Configure Progress Sync.
2. Configure CrossSync WebDAV.
3. Open an EPUB and create a highlight.
4. Trigger Progress Sync and choose Upload local.
5. Confirm the WebDAV JSON file is replaced.
6. Edit or replace the WebDAV JSON from another client.
7. Trigger Progress Sync and choose Apply remote.
8. Confirm CrossInk highlights are replaced in the reader and clipping list.

## Notes For `crossync.koplugin`

The plugin is a separate repository. Its phase-1 structure should be:

```text
crossync.koplugin/
  _meta.lua
  main.lua
  settings.lua
  document_id.lua
  schema.lua
  webdav.lua
  reader.lua
  writer.lua
  sync.lua
  README.md
```

It should implement explicit `Pull remote` and `Upload local` actions with the same overwrite semantics. It should not depend on AnnotationSync.
