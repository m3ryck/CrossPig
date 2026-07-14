# AnnotationSync.koplugin — Reverse Engineering Document

> **Repository**: https://github.com/dani84bs/AnnotationSync.koplugin
> **Version analyzed**: v1.9.99 (latest, June 2026)
> **Language**: Lua (99.6%)
> **License**: MIT

---

## Table of Contents

1. [Architecture General](#1-architecture-general)
2. [Data Model](#2-data-model)
3. [Book Identification](#3-book-identification)
4. [Annotation Identification](#4-annotation-identification)
5. [Highlight Location System](#5-highlight-location)
6. [Sync Backend](#6-sync-backend)
7. [Sync Protocol](#7-sync-protocol)
8. [File Format](#8-file-format)
9. [KOReader Sidecar Interaction](#9-sidecar-interaction)
10. [KOReader API Dependencies](#10-koreader-api-dependencies)
11. [Reusable Layers](#11-reusable-layers)
12. [Coupling Analysis](#12-coupling-analysis)
13. [Conflict Resolution](#13-conflicts)
14. [Deletion Strategy](#14-deletion)
15. [Offline Support](#15-offline)
16. [Performance Analysis](#16-performance)
17. [Security Analysis](#17-security)
18. [Extensibility for CrossInk](#18-extensibility)
19. [Proposed Architecture](#19-proposed-architecture)
20. [Proposed Protocol](#20-proposed-protocol)
21. [Comparison Table](#21-comparison)
22. [Conclusion](#22-conclusion)

---

## 1. Architecture General

### 1.1 File Structure

```
AnnotationSync.koplugin/
├── _meta.lua                 # Plugin metadata (name, version, description)
├── main.lua                  # Entry point, KOReader plugin lifecycle, menu registration
├── annotations.lua           # Core annotation model + merge engine (position-based)
├── manager.lua               # SyncManager: orchestration, progress sync, settings sync
├── remote.lua                # Cloud provider abstraction layer
├── utils.lua                 # JSON reader, helper functions
├── menus.lua                 # UI: deleted annotations, jump to progress, pending docs
├── settings_selection.lua    # UI: changed settings comparison + selection
├── Makefile                  # msgfmt/xgettext for .mo translation files
├── run_tests.sh              # Test runner
├── defaults/
│   ├── settings.reader.lua   # Vanilla KOReader settings (for diff comparison)
│   └── defaults.custom.lua
├── l10n/
│   ├── annotation_sync.pot   # Translation template
│   ├── it_IT/
│   │   └── annotation_sync.mo
│   └── hu/
│       └── annotation_sync.mo
└── spec/unit/
    └── sync_integration_spec.lua  # Integration tests
```

### 1.2 Module Responsibilities

| File | Responsibility |
|------|---------------|
| `_meta.lua` | Static metadata: name, version, description, plugin_id |
| `main.lua` | KOReader plugin lifecycle (`init`, `addToMainMenu`), event registration, dispatcher actions, annotation restoration, settings persistence |
| `annotations.lua` | Pure data-layer: annotation key generation, list↔map conversion, position comparison, merge algorithm (`sync_callback`), deletion detection (`get_deleted_annotations`) |
| `manager.lua` | Orchestration: tracks dirty documents, syncs all changed docs, coordinates progress sync, handles settings push/pull, serializes state |
| `remote.lua` | Adapter for KOReader's cloud storage providers (`CloudStorage+` / `SyncService`); owns the `perform_sync` wrapper |
| `utils.lua` | JSON-safe file reading, `isPossiblyJson` guard, nested value access for settings |
| `menus.lua` | UI screens: deleted annotations restoration, device progress jump menu, settings pull menu, pending documents |
| `settings_selection.lua` | UI for diffing KOReader settings vs defaults; lets user pick which settings to sync |

### 1.3 Initialization Flow

```
KOReader starts
  ↓
Plugin loader discovers AnnotationSync.koplugin/
  ↓
main.lua:init() is called
  ↓
├── Registers to main menu (Tools → Annotation Sync)
├── Ensures self is in UI event chain (table.insert)
├── Calls utils.insert_after_statistics() to position menu after Statistics
├── Registers Dispatcher actions (gestures / profiles)
├── Reads settings from G_reader_settings (plugin_id key)
├── Falls back / migrates legacy cloud_server_object
├── Creates SyncManager instance
├── Loads translation .mo file for current locale
└── Calls registerEvents() — attaches NetworkConnected handler if auto-sync on
```

### 1.4 Event Flow (Annotations Modified)

```
User creates/edits/deletes highlight in KOReader
  ↓
KOReader fires Event: "AnnotationsModified"
  ↓
main.lua:onAnnotationsModified() catches it
  ↓
Extracts book_path from each annotation (falls back to ui.document.file)
  ↓
Calls manager:addToChangedDocumentsFile(file) for each unique file
  ↓
This writes to DataStorage:getDataDir()/changed_documents.lua
```

### 1.5 Sync Flow (Manual Sync for single document)

```
User taps "Manual Sync"
  ↓
main.lua:manualSync()
  ↓
manager:syncDocument(document, is_manual=true)
  ↓
├── _flushSettings() — broadcasts FlushSettings event
├── writeAnnotationsJSON(document)
│   ├── Gets sdr_dir from docsettings:getSidecarDir()
│   ├── Gets annotations from ui.annotation.annotations (or sidecar)
│   ├── annotations.list_to_map() converts list → keyed map
│   └── Writes JSON to SDR_DIR/<hash|filename>.json
│
├── remote.sync_annotations(plugin, document, json_path, callback, force)
│   ├── Calls cloudstorage:sync() or SyncService.sync()
│   │   └── This uploads the JSON, downloads remote copy
│   │       └── Cloud provider calls sync_callback(local_file, cached_file, income_file)
│   │           └── annotations.sync_callback(document, local_file, cached_file, income_file, force)
│   │               ├── Reads local JSON, last-sync JSON, remote JSON
│   │               ├── Validate remote schema
│   │               ├── get_deleted_annotations() — detects local deletions
│   │               ├── Merges local + remote sorted by position (position-based merge)
│   │               ├── Conflict resolution: timestamp comparison (last-write-wins)
│   │               ├── Writes merged result to local_file
│   │               └── Returns merged_list (annotations without deleted)
│   │
│   └── callback returns success + merged_list
│
└── _onSyncComplete(document, success, merged_list)
    ├── applySyncedAnnotations() — updates UI, re-renders document
    └── removeFromChangedDocumentsFile() — clears dirty flag
```

### 1.6 Complete Data Lifecycle of an Annotation

```
1. CREATION
   User selects text → KOReader creates annotation object in memory
   → ui.annotation.annotations table updated
   → KOReader broadcasts Event: "AnnotationsModified"
   → Plugin marks file as dirty in changed_documents.lua

2. LOCAL PERSISTENCE
   KOReader periodically saves annotations to sidecar SDR_DIR/*.lua
   (docsettings mechanism)

3. SYNC (on demand or automatic)
   Plugin reads annotations from ui.annotation (or sidecar)
   → Converts to map format (list_to_map)
   → Writes to SDR_DIR/<hash>.json
   → Uploads via CloudStorage+
   → Downloads remote copy
   → Merges (position-based + timestamp)
   → Writes merged JSON back
   → Applies merged annotations to UI via annotation:onSaveSettings()

4. DELETION
   User deletes highlight → KOReader removes from list
   → AnnotationSync detects deletion via last_sync comparison
   → Marks as deleted in the sync JSON (soft delete)
   → Syncs to remote

5. RESTORATION
   User restores from trash → deleted=false + new datetime_updated
   → Re-added to annotation list
   → Syncs to remote as revived

6. FINAL (not implemented)
   No hard purge mechanism for tombstones
```

---

## 2. Data Model

### 2.1 Annotation Object Structure

Annotations in KOReader follow the native highlight structure. AnnotationSync does not define its own schema — it uses what KOReader gives it:

```json
{
  "page": 42,
  "pos0": "//body/section[1]/p[3]/text()[1]/word[5]",
  "pos1": "//body/section[1]/p[3]/text()[1]/word[12]",
  "text": "the highlighted text content",
  "datetime": "2026-01-15 14:30:00",
  "chapter": "Chapter 3 - The Awakening",
  "note": "This reminds me of...",
  "deleted": false,
  "datetime_updated": "2026-01-20 09:15:00"
}
```

### 2.2 Fields Inventory

| Field | Type | Example | Origin | Optional? |
|-------|------|---------|--------|-----------|
| `page` | number/string | `42` or `"//body/..."` | KOReader native | No |
| `pos0` | string/table | XPath or `{x,y,zoom}` | KOReader native | No (annotation) |
| `pos1` | string/table | XPath or `{x,y,zoom}` | KOReader native | No (annotation) |
| `text` | string | `"the quick brown fox"` | KOReader extracted text | Yes |
| `datetime` | string (ISO) | `"2026-01-15 14:30:00"` | KOReader creation time | Yes |
| `datetime_updated` | string (ISO) | `"2026-01-20 09:15:00"` | AnnotationSync sync time | Yes (plugin sets it) |
| `chapter` | string | `"Chapter 3"` | KOReader detected chapter | Yes |
| `note` | string | `"My note text"` | User's note | Yes |
| `deleted` | boolean | `true` / `false` | AnnotationSync deletion marker | Yes (plugin sets it) |
| `draw` | table | Drawing data | KOReader drawing | Yes |
| `ink` | table | Ink data | KOReader ink | Yes |

### 2.3 Critical Findings

| Feature | Exists? | Details |
|---------|---------|---------|
| **UUID** | **NO** | There is no stable unique identifier. Keys are derived from position (`annotation_key()`). |
| **Versioning** | **NO** | No revision counter, no version vector, no sequence number. |
| **updated_at** | **YES** | `datetime_updated` added by plugin. Fallback to `datetime` if absent. |
| **created_at** | **YES** | `datetime` is creation time from KOReader. |
| **deleted_at** | **NO** | No field for when deletion happened. Only `datetime_updated` + `deleted=true`. |
| **Soft delete** | **YES** | `deleted` boolean flag. Tombstones remain in sync JSON indefinitely. |
| **Revision** | **NO** | No `revision` field. Total order impossible to determine without timestamps. |
| **device_id** | **NO** | Annotations don't carry a `device_id`. Only progress and settings do. |
| **book_hash** | **YES** | MD5 hash of file path (via `util.partialMD5`), used as filename. |

### 2.4 Key Generation (`annotation_key`)

```lua
-- annotations.lua:annotation_key()
-- For highlights (has pos0, pos1):
--   If pos0 is a table (PDF/scan):   "page|zoomx|zoomy||zoomx|zoomy"
--   If pos0 is a string (EPUB/XPath): "pos0_string||pos1_string"
--
-- For bookmarks (has page, no pos0):
--   "BOOKMARK|<page>"
--
-- Result is used as the key in the sync JSON map
```

**Consequence**: The same highlight on two devices generates the **same key** only if:
- Same book file
- Same position data
- Same page
- Same zoom level (PDF)

This is **position-addressed**, not identity-addressed.

---

## 3. Book Identification

### 3.1 How Books Are Identified

File path is the primary identifier. Two strategies for naming sync files:

1. **Hash mode (default)**: `util.partialMD5(file_path) .. ".json"`
   - `util.partialMD5` computes an MD5 hash of the file path string
   - `main.lua:347-348` — `self.settings.use_filename` toggles this

2. **Filename mode**: `filename_from_path .. ".json"`
   - Uses the last segment of the file path

```lua
-- manager.lua:_getAnnotationFilename()
function SyncManager:_getAnnotationFilename(file)
    if self.plugin.settings.use_filename then
        local filename = file:match("([^/]+)$") or file
        return filename .. ".json"
    end
    local hash = file and type(file) == "string" and util.partialMD5(file) or "No hash"
    return hash .. ".json"
end
```

### 3.2 Advantages and Limitations

| Strategy | Pros | Cons |
|----------|------|------|
| **MD5 of path** | Stable even if file is renamed externally (Calibre); collision-resistant | Breaks if file path changes; same book copied to different path = different hash |
| **Filename** | Human-readable in cloud storage; easy to identify files | Collisions if two books share same filename; breaks on rename |

### 3.3 Critical Issues

- **No content fingerprinting**: There is no checksum of the file *contents*. If the user has the same EPUB on two different devices at different file paths, they will have separate sync files → no cross-device sync.
- **No book metadata ID**: No ISBN, no EPUB UUID (`<dc:identifier>`), no Calibre ID. Book identity is purely filesystem-path-derived.
- **Renaming impacts**: If a user renames the book file, the hash changes, and annotations become orphaned from the new path.
- **Same book, different paths**: A book at `/books/moby.epub` on device A and `/ebooks/moby_dick.epub` on device B → two separate sync namespaces.

---

## 4. Annotation Identification

### 4.1 How Identities Are Established

There is **no stable UUID**. Annotation identity is **position-derived**:

```lua
-- annotations.lua:annotation_key()
-- EPUB: "//body/p[3]/text()[1]/word[5]||//body/p[3]/text()[1]/word[12]"
-- PDF:  "42|100|200||150|250"  (page|zoom-x|zoom-y||zoom-x|zoom-y)
-- Bookmark: "BOOKMARK|42"
```

After a merge, the key is what determines whether an incoming annotation is "the same" as an existing one.

### 4.2 Merge Identity Resolution

During merge (`annotations.lua:sync_callback`):

1. Both local and remote are sorted by position
2. Keys are compared as a two-pointer merge
3. If `positions_intersect(a, b)` → positions overlap → they are "the same"
4. If they intersect: `is_before()` decides which survives (timestamp comparison)
5. If they don't intersect: both survive, inserted in position order

### 4.3 What `positions_intersect` Does

```lua
function M.positions_intersect(a, b, document)
    -- 1. If keys match exactly → same annotation
    if M.annotation_key(a) == M.annotation_key(b) then return true end

    -- 2. If pos ranges overlap → same annotation (slightly moved)
    --    A.start <= B.start <= A.end
    --    B.start <= A.start <= B.end
    -- This catches cases where highlights were extended/trimmed
end
```

### 4.4 Critical Issues

- **No UUID**: If a highlight is deleted on device A and re-created at the same position, it will collide with device B's tombstone — the merge logic treats it as "same annotation" and whichever timestamp is newer wins. This is fragile.
- **Position drift**: If an EPUB is re-flowed with different font settings, XPath positions may still match (since they reference DOM nodes, not screen positions). But a reformatted EPUB with different DOM structure breaks all keys.
- **Editing a highlight text**: Changing only the text doesn't change position. The updated `datetime_updated` will be newer, so it wins via timestamp comparison.

---

## 5. Highlight Location

### 5.1 Location Systems Used

| Format | Location System | Type | Example |
|--------|----------------|------|---------|
| **EPUB** (crengine) | XPointer / XPath | String | `"//body/section/p[3]/text()[1]/word[5]"` |
| **PDF** (kopt) | Page + pixel coords with zoom | Table | `{page=42, x=100, y=200, zoom=1.0}` |
| **DJVU** | Page + pixel coords | Similar to PDF | `{page=5, x=50, y=75}` |
| **Picture** (mupdf) | Page + image coords | Table | Same structure as PDF |
| **Bookmark** | Just page number | Number | `42` |

### 5.2 Position Comparison

```lua
-- annotations.lua:compare_positions()
function M.compare_positions(a, b, document)
    -- Both numbers → numeric comparison (pages)
    -- Both strings → document:compareXPointers(a, b)  (EPUB xpointers)
    -- Both tables  → document:comparePositions(a, b)  (PDF coordinates)
    -- Mixed types  → return 0 (equal → unpredictable)
end
```

This delegates to the **KOReader document object** for comparison. Without `document:compareXPointers()` or `document:comparePositions()`, position comparison fails.

### 5.3 Consequences

- **Requires a document instance** to compare positions. The `sync_callback` receives a `document` parameter for precisely this reason.
- **Cross-format sync impossible**: An EPUB highlight cannot be matched with a PDF annotation.
- **For `Sync All`**: The manager opens temporary document instances (`DocumentRegistry:openDocument`) to get a valid document object for position comparison. This means:
  - Every unsynced EPUB must be rendered to compare XPointers → slow for many books
  - The document must be parsed and its TOC/structure loaded
- **EPUB re-rendering**: If a user changes font/size/margins between devices, XPointers still match because they're DOM-based, not layout-based.

---

## 6. Sync Backend

### 6.1 No Custom Server

AnnotationSync has **no backend server**. It piggybacks entirely on KOReader's existing cloud storage infrastructure:

```
AnnotationSync
  └── remote.lua
      ├── CloudStorage+ (KOReader dev/nightly) — preferred
      │   └── Supports: WebDAV, Dropbox, FTP, SFTP, S3
      └── SyncService (legacy, stable KOReader)
          └── Supports: WebDAV, Dropbox, FTP
```

### 6.2 How it Works

The plugin writes JSON files to the sidecar directory and uses the cloud storage plugin as a file sync mechanism:

```
Local filesystem                  Cloud storage
┌─────────────────────┐          ┌──────────────────┐
│ SDR_DIR/<hash>.json │  ─────→  │ /koreader/       │
│ SDR_DIR/<hash>.json.sync      │   <hash>.json     │
│ SDR_DIR/<hash>.json.temp      │   <hash>.json.sync│
│                     │  ←─────  │   <hash>.json.temp
│ SDR_DIR/<hash>.progress.json   │                  │
│ DataDir/settings_sync.json     │                  │
└─────────────────────┘          └──────────────────┘
```

The cloud storage sync function (`cloudstorage:sync()`) does:
1. Upload `local_file` → remote
2. Download remote → `income_file`
3. Download remote's previous local upload → `cached_file`
4. Call the callback with `(local_file, cached_file, income_file)`

The plugin receives these three files and runs the merge algorithm in the callback.

### 6.3 Provider Details

| Feature | CloudStorage+ | SyncService |
|---------|---------------|-------------|
| KOReader version | Dev/nightly (2024+) | Stable |
| Async | Yes (callback-based) | Synchronous |
| Mockable for tests | Yes (is_mock flag) | No |
| Progress sync | Yes | Limited |
| UI during sync | Configurable silent | Noisy |
| Supported providers | WebDAV, Dropbox, FTP, SFTP, S3 | WebDAV, Dropbox, FTP |

---

## 7. Sync Protocol

### 7.1 File-Level Protocol

The protocol is **file-based** using KOReader's cloud storage sync. There is no custom API, no HTTP endpoint, no database:

```
┌─────────┐    upload <hash>.json     ┌──────────┐
│ Device A │  ──────────────────────→  │  Cloud   │
│          │  ←──────────────────────  │ Storage  │
│          │    download remote copy   │          │
└─────────┘                           └──────────┘
                                               ↑
┌─────────┐    upload <hash>.json             │
│ Device B │  ─────────────────────────────────┘
│          │  ←─────────────────────────────────
└─────────┘    download remote copy
```

### 7.2 Three-File Merge Pattern

Each sync operation involves three files:

```
local_file    = SDR_DIR/<hash>.json       # Current local state (plugin writes before sync)
cached_file   = SDR_DIR/<hash>.json.sync  # Previous synced state (last local upload)
income_file   = SDR_DIR/<hash>.json.temp  # Remote state downloaded from cloud
```

The cloud storage provider creates `.sync` and `.temp` variants automatically.

### 7.3 Merge Algorithm (annotations.lua:sync_callback)

```
sync_callback(local_file, cached_file, income_file)
  │
  ├── 1. READ all three files
  │     local_map = read_json(local_file)
  │     last_sync_map = read_json(cached_file)
  │     income_map = read_json(income_file)
  │
  ├── 2. VALIDATE income_map structure
  │     - Must be a table
  │     - Entries must have annotation fields (datetime_updated, page, text, etc.)
  │     - If invalid → treat as 404 / empty remote
  │
  ├── 3. DETECT LOCAL DELETIONS
  │     get_deleted_annotations(local_map, last_sync_map, document, force)
  │     - Compares local vs last_sync: items in last_sync but not in local = deleted
  │     - Safety: if local is empty but last_sync had items → skip (protect against data loss)
  │
  ├── 4. TWO-POINTER MERGE (sorted by position)
  │     local_keys = sort_keys_by_position(local_map, document)
  │     income_keys = sort_keys_by_position(income_map, document)
  │
  │     while i <= #income_keys and l <= #local_keys:
  │       if positions_intersect(income_v, local_v):
  │         if is_before(income_v, local_v):  →  keep local (local is newer)
  │         else:                              →  keep income (income is newer)
  │       else:
  │         if local position < income position:  →  insert local
  │         else:                                  →  insert income
  │
  │     Append remaining local items
  │     Append remaining income items
  │
  ├── 5. FILTER OUT deleted annotations (map_to_list)
  │
  └── 6. WRITE merged map → local_file
      return true, merged_list (for UI update)
```

### 7.4 Sync Types

| Type | Trigger | What it syncs | Force flag |
|------|---------|---------------|------------|
| **Manual Sync** | User action (menu/gesture) | Current document only | `force=true` |
| **Sync All** | User action or auto-network | All dirty documents | `force=false` |
| **Progress Sync** | Page turn event | Current document progress | N/A |
| **Settings Push** | User action | Selected settings → cloud | N/A |
| **Settings Pull** | User action | Cloud → local selected values | N/A |

### 7.5 Sync All Flow

```
syncAllChangedDocuments()
  │
  ├── Read changed_documents.lua
  ├── For each dirty file:
  │   ├── Get or create document object
  │   ├── writeAnnotationsJSON(document)
  │   ├── remote.sync_annotations(...)
  │   ├── On complete:
  │   │   ├── If success: remove from changed_documents
  │   │   └── If fail: keep in changed_documents
  │   └── If temporary doc → close it
  ├── Show summary message
  └── Show error dialog for failed files
```

---

## 8. File Format

### 8.1 Annotation JSON (per book)

Location: `SDR_DIR/<hash|filename>.json`

```json
{
  "//body/section/p[1]/text()[1]/word[10]||//body/section/p[1]/text()[1]/word[20]": {
    "page": "//body/section/p[1]/text()[1]/word[10]",
    "pos0": "//body/section/p[1]/text()[1]/word[10]",
    "pos1": "//body/section/p[1]/text()[1]/word[20]",
    "text": "the highlighted passage",
    "datetime": "2026-01-15 14:30:00",
    "chapter": "Chapter 3",
    "note": "Important insight",
    "deleted": false,
    "datetime_updated": "2026-01-15 14:30:00"
  },
  "BOOKMARK|42": {
    "page": 42,
    "datetime": "2026-01-16 10:00:00",
    "datetime_updated": "2026-01-16 10:00:00",
    "deleted": false
  }
}
```

The keys are the annotation keys (position-derived). The values are the annotation objects with all fields.

### 8.2 Progress JSON (per book)

Location: `SDR_DIR/<hash|filename>.progress.json`

```json
{
  "Bedside Kobo": {
    "page": 142,
    "percentage": 0.45,
    "pos": "//body/section/p[5]/text()[1]/word[3]",
    "timestamp": "2026-01-20 22:30:00"
  },
  "Phone": {
    "page": 138,
    "percentage": 0.44,
    "pos": "//body/section/p[5]/text()[1]/word[1]",
    "timestamp": "2026-01-20 20:15:00"
  }
}
```

### 8.3 Settings Sync JSON

Location: `DataStorage:getDataDir()/settings_sync.json`

```json
{
  "Bedside Kobo": {
    "settings": {
      "reader:cre_header_page_number": 1,
      "reader:page_turns_tap_zones": "default"
    },
    "timestamp": "2026-01-20 22:30:00"
  }
}
```

### 8.4 Changed Documents

Location: `DataStorage:getDataDir()/changed_documents.lua`

Serialized as Lua code (not JSON):

```lua
{
  ["/books/moby.epub"] = true,
  ["/books/war_and_peace.epub"] = true,
}
```

---

## 9. Sidecar Interaction

### 9.1 Files Read

| File | How | When |
|------|-----|------|
| `SDR_DIR/*.lua` | `docsettings:open(file)` | Getting annotations for inactive documents |
| `SDR_DIR/*.lua` | `docsettings:readSetting("annotations")` | Getting annotation list from sidecar |
| `settings.reader.lua` | `dofile()` | Diffing changed vs vanilla settings |
| `defaults.custom.lua` | `dofile()` | Diffing custom defaults |
| `settings/*.lua` | `dofile()` | Reading individual plugin settings |
| `changed_documents.lua` | `dofile()` | Reading pending sync queue |

### 9.2 Files Written

| File | How | When |
|------|-----|------|
| `SDR_DIR/<hash>.json` | `util.writeToFile()` | Before every sync |
| `SDR_DIR/<hash>.json` (overwrite) | After merge callback | After sync completes |
| `changed_documents.lua` | `util.writeToFile()` | Annotations modified / sync complete |
| `settings_sync.json` | `util.writeToFile()` | Before settings push |
| `G_reader_settings` | `saveSetting()` | Persisting plugin settings |

### 9.3 Sidecar Files for Sync

The cloud storage plugin creates additional temporary files in the sidecar directory:
- `<hash>.json.sync` — last synced state (used as `cached_file`)
- `<hash>.json.temp` — downloaded remote copy (used as `income_file`)

---

## 10. KOReader API Dependencies

### 10.1 Complete API Inventory

| Module | Usage | Location | Replaceable? |
|--------|-------|----------|-------------|
| `frontend/docsettings` | Open sidecar, read/write annotations, get sidecar dir | manager.lua, annotations.lua | Medium — replace with FsFile |
| `ui/uimanager` | Show widgets, schedule, set dirty, broadcast events | All files | Hard — UI framework |
| `ui/event` | Create/broadcast events (AnnotationsModified, FlushSettings) | main.lua, manager.lua | Medium |
| `dispatcher` | Register gesture/profile actions | main.lua | Hard |
| `ui/widget/infomessage` | Show info popups | remote.lua, main.lua | Medium |
| `ui/widget/inputdialog` | Text input dialogs | main.lua | Medium |
| `ui/widget/confirmbox` | Confirmation dialogs | menus.lua | Medium |
| `ui/widget/menu` | Menu rendering for deleted/jump/pending | menus.lua, settings_selection.lua | Medium |
| `ui/network/manager` | Network connectivity check | manager.lua | Medium |
| `ffi/util` | Template strings, misc | All files | Easy — string formatting |
| `util` | fileExists, writeToFile, makePath, partialMD5, tableDeepCopy | All files | Medium — file I/O utils |
| `json` | JSON encode/decode | All files | Easy — swap library |
| `gettext` | i18n | All files | Easy — tr() in CrossInk |
| `logger` | Debug/error logging | All files | Easy — LOG_INF/LOG_ERR |
| `datastorage` | getDataDir, getSettingsDir | All files | Medium |
| `device` | model name, hardware info | manager.lua, main.lua | Hard — platform specific |
| `document/documentregistry` | Open documents by provider | manager.lua | Hard |
| `apps/cloudstorage/syncservice` | Legacy cloud sync provider | remote.lua, main.lua | Hard |
| `reader_order` (ui/elements) | Menu ordering | utils.lua | Easy |
| `ui/trapper` | Background subprocess | remote.lua | Hard |
| `libs/libkoreader-lfs` | File system attributes | manager.lua | Easy |
| `luasettings` | Open/save nested settings files | manager.lua | Medium |

### 10.2 UI Framework Coupling

The entire menu system, dialog infrastructure, and widget rendering depend on KOReader's `UIManager`. Every user interaction goes through:

```lua
UIManager:show(Menu:new{...})
UIManager:show(ConfirmBox:new{...})
UIManager:show(InfoMessage:new{...})
UIManager:setDirty(...)
```

This is completely tied to KOReader's widget system and cannot be reused.

### 10.3 Document Coupling

The merge algorithm requires a **live document object** to:
- `document:compareXPointers(a, b)` - compare EPUB positions
- `document:comparePositions(a, b)` - compare PDF positions
- `document.is_pdf` - format check
- `document:render()` - re-render after applying annotations

For `Sync All`, the manager creates temporary documents via `DocumentRegistry:openDocument()` just to get position comparison. This is **very** KOReader-specific.

---

## 11. Reusable Layers

### 11.1 Fully Reusable

| Module | Lines | Why |
|--------|-------|-----|
| **Merge algorithm** (`annotations.lua` logic except `compare_positions`) | ~100 | Pure data transformation: map/list conversion, key generation, two-pointer merge, deletion detection. No KOReader imports except `docsettings` for position comparison. |
| **Data model** (annotation key strategy) | ~30 | Position-based key generation. Portable concept, though the `annotation_key()` function itself needs adaptation. |
| **Timestamp comparison** (`is_before`) | ~5 | Pure function. |
| **Three-file merge pattern** (conceptual) | N/A | The local/cached/income pattern is transport-agnostic and could be reused with any sync mechanism. |

### 11.2 Partially Reusable

| Module | Reusable Part | KOReader-dependent Part |
|--------|---------------|------------------------|
| `remote.lua` | The three-file callback pattern | CloudStorage+ / SyncService API calls |
| `manager.lua` | Queue management, sync orchestration, settings push/pull | UIManager, Event, device, document objects |
| `utils.lua` | `read_json` (with JSON guard), `get_nested_value` | `insert_after_statistics` (menu ordering) |
| `annotations.lua:compare_positions` | The branching logic | Delegates to `document:compareXPointers/comparePositions` |

### 11.3 Not Reusable (CrossInk)

| Module | Why |
|--------|-----|
| `main.lua` | KOReader plugin lifecycle, menu registration, event system |
| `menus.lua` | KOReader widget/Menu/ConfirmBox UI |
| `settings_selection.lua` | KOReader settings file format, dofile-based diffing |
| `remote.lua` (entirely) | Direct CloudStorage+/SyncService calls |
| `manager.lua:_flushSettings` | KOReader-specific FlushSettings event |

---

## 12. Coupling Analysis

### 12.1 Assessment: **Strongly Coupled to KOReader**

The plugin is tightly integrated with KOReader in several dimensions:

1. **Plugin Framework**: Extends `WidgetContainer`, registers via `self.ui.menu:registerToMainMenu()`, hooks into KOReader's event system.

2. **Document Access**: Requires `self.ui.document` for the active document, and `DocumentRegistry:openDocument()` for inactive ones. Position comparison is delegated to the document object itself.

3. **UI System**: Every user interaction (menus, dialogs, confirmations) uses KOReader's `UIManager` widgets.

4. **Cloud Storage**: Direct dependency on `cloudstorage` or `SyncService` modules. The sync callback signature is defined by KOReader's cloud sync system.

5. **Settings System**: Reads/writes via `G_reader_settings`, `LuaSettings`, `docsettings`. All settings files are in KOReader's Lua format.

6. **File System**: Uses KOReader's `DataStorage` paths, `docsettings:getSidecarDir()`, and `util.writeToFile()` convenience wrappers.

7. **Event System**: Reacts to `AnnotationsModified`, `PageUpdate`, `PosUpdate`, `CloseDocument`, `Suspend`, `NetworkConnected`.

### 12.2 Quantitative Analysis

| Layer | Lines of Code | KOReader-dependent | Portability |
|-------|--------------|-------------------|-------------|
| `annotations.lua` | ~180 | ~20% (compare_positions) | ~80% portable |
| `manager.lua` | ~320 | ~60% | ~40% portable |
| `main.lua` | ~280 | ~95% | ~5% portable |
| `remote.lua` | ~210 | ~100% | 0% portable |
| `menus.lua` | ~220 | ~100% | 0% portable |
| `utils.lua` | ~45 | ~10% | ~90% portable |
| `settings_selection.lua` | ~280 | ~100% | 0% portable |

---

## 13. Conflict Resolution

### 13.1 Strategy: **Last Write Wins (Timestamp-Based)**

```lua
-- annotations.lua:is_before()
function M.is_before(a, b)
    local a_time = a.datetime_updated or a.datetime or 0
    local b_time = b.datetime_updated or b.datetime or 0
    return a_time <= b_time
end
```

### 13.2 Resolution Rules

| Scenario | Resolution |
|----------|------------|
| Same highlight, different note text | Version with newer `datetime_updated` wins (entire object replaced) |
| Same highlight, deleted on one device | If deletion timestamp is newer → deleted; if modification timestamp is newer → revived |
| Deleted on all devices → modified on one | Modification with newer timestamp wins → "zombie resurrection" (tested in spec) |
| Same position, different highlights | If positions_intersect → treated as same annotation → timestamp decides |
| First sync (no remote data) | Income is empty → local wins |
| Fresh device (local empty, remote has data) | **Safety guard**: if last_sync has data but local is empty → skip deletion propagation |
| Manual sync (`force=true`) | Bypasses the fresh-device safety guard |

### 13.3 Critical Weaknesses

- **No clock synchronization**: Timestamps are device-local. If device A's clock is 2 hours behind device B, the "wrong" version can win.
- **Complete object replacement**: There's no field-level merge. If device A changes the note and device B changes the text, one device loses its change entirely.
- **No conflict UI**: Users are never notified of conflicts. The merge is silent.
- **Timestamps are strings**: `os.date("%Y-%m-%d %H:%M:%S")` — no timezone info. Two devices in different timezones can produce confusing results.
- **No version vectors**: Impossible to detect true concurrent edits. Causal relationships between changes are lost.

---

## 14. Deletion Strategy

### 14.1 Mechanism: **Soft Delete with Tombstone**

When an annotation is deleted locally:

1. **Detection**: `get_deleted_annotations()` compares current local map vs last-synced map. Items in last-synced but absent from local are marked as deleted.

```lua
-- annotations.lua:get_deleted_annotations()
for _, uploaded_k in ipairs(uploaded_keys) do
    local uploaded_v = last_uploaded_map[uploaded_k]
    local found_in_local = false
    -- Check if this annotation still exists in local (by position intersection)
    for _, local_k in ipairs(local_keys) do
        if M.positions_intersect(uploaded_v, local_v, document) then
            found_in_local = true
            break
        end
    end
    if not found_in_local then
        uploaded_v.deleted = true  -- Mark as tombstone
        uploaded_v.datetime_updated = os.date("%Y-%m-%d %H:%M:%S")
        local_map[uploaded_k] = uploaded_v  -- Keep in map
    end
end
```

2. **Propagation**: Tombstone (with `deleted=true`) is synced to remote. Other devices will see the deletion.

3. **Trash bin**: `getDeletedAnnotations()` returns all annotations with `deleted=true` from the local sync JSON. User can restore them.

4. **Restoration**: `restoreAnnotation()` sets `deleted=false` and updates `datetime_updated`. This is synced as a normal annotation.

### 14.2 Issues

- **No hard delete**: Tombstones accumulate in the sync JSON forever. For a heavily annotated book with many deletions over years, the JSON file grows unboundedly.
- **No garbage collection**: No mechanism to purge tombstones older than X days.
- **No `deleted_at`**: The `datetime_updated` field is repurposed to track deletion time, conflating "last update" with "deletion time."
- **Safety guard can misfire**: If local annotations are wiped (factory reset, SD corruption), the safety guard prevents deletion propagation. But if the user intentionally cleared annotations and this is detected as "local=empty", the guard blocks the sync.

---

## 15. Offline Support

### 15.1 How Offline Works

```
Device has no network
  ↓
User creates highlights
  ↓
KOReader saves to sidecar (local persistence is KOReader's responsibility)
  ↓
AnnotationSync marks file as dirty in changed_documents.lua
  ↓
Network becomes available
  ↓
AnnotationSync._onNetworkConnected() fires
  ↓
Calls syncAllChangedDocuments()
  ↓
Processes all dirty documents
```

### 15.2 Architecture Points

| Aspect | Implementation |
|--------|---------------|
| **Queue** | `changed_documents.lua` — a Lua table stored on disk |
| **Retry** | Failed docs stay in changed_documents.lua → retry on next Sync All |
| **Persistence** | Changed documents survive device restart (stored on SD/data partition) |
| **Auto-detect network** | Listens for `NetworkConnected` event from KOReader |
| **Background sync** | Progress sync uses `Trapper:wrap()` with subprocess fallback |
| **Dirty flag management** | Cleared only on successful sync; failures remain dirty |

### 15.3 Limitations

- **No exponential backoff**: Failed syncs retry immediately on next network event, not with backoff.
- **No queue persistence of failed items**: The same `changed_documents.lua` mechanism handles both pending and failed. No separate retry queue.
- **No offline queue for progress**: Progress sync happens on page turns. If offline, the counter resets and the position is lost. Only the last page before network loss is recorded.
- **Sync All does not auto-retry**: After the initial attempt, Sync All only runs on explicit user action or next network event.

---

## 16. Performance Analysis

### 16.1 Scaling Characteristics

| Scenario | Expected Behavior |
|----------|-------------------|
| **100 highlights, 1 book** | Fast. Single JSON file < 50KB. Merge is O(n log n) for sorting + O(n) two-pointer merge. |
| **1000 highlights, 1 book** | Moderate. JSON ~500KB. Sorting takes milliseconds in LuaJIT. The real cost is opening documents for position comparison. |
| **10000 highlights, 1 book** | Slow. JSON ~5MB. Lua table operations on large maps consume memory. The O(n log n) sort of 10K items is ~140K comparisons with delegate calls to `document:compareXPointers()`. |
| **100 books, ~50 highlights each** | The bottleneck is Sync All: 100 documents must be opened and rendered. For EPUB books, each render call is expensive (crengine layout). |
| **100 books, Sync All** | Very slow. Each book must: open via `DocumentRegistry`, render (crengine), merge with position comparisons (requires rendered document), write JSON, close. Sequential processing (one at a time). |

### 16.2 Known Bottlenecks

1. **Document opening for Sync All**: `DocumentRegistry:openDocument()` + `document:render()` for every EPUB book is extremely slow. This is done just to get `compareXPointers()` working.

2. **Sequential processing**: Sync All processes documents one at a time in a recursive callback chain.

3. **No pagination/filtering**: The entire annotation list is loaded, sorted, and merged in memory.

4. **JSON parsing**: For every sync, the plugin reads and parses three JSON files (local, last_sync, income) entirely into memory.

5. **Memory churn**: Every sync operation creates multiple Lua table copies (list_to_map, map_to_list, sorted key lists, merged result).

### 16.3 Memory Analysis (Estimated)

| Operation | Memory | Notes |
|-----------|--------|-------|
| Load 10K annotations JSON | ~15-25 MB | Raw JSON + Lua table overhead |
| Merge 10K annotations | ~50-100 MB | Multiple table copies during merge |
| One EPUB document open+render | ~10-30 MB | crengine layout engine |
| Sync All (100 books × 50 anns) | Peak ~100 MB | Sequential, but documents stay in memory temporarily |

On ESP32-C3 with ~380 KB usable RAM, these numbers show that **the current architecture is not portable to CrossInk** without fundamental redesign.

---

## 17. Security Analysis

### 17.1 Current State

| Concern | Status |
|---------|--------|
| **Authentication** | Delegated to KOReader's CloudStorage+ / SyncService |
| **Authorization** | Delegated to cloud provider (OAuth tokens, passwords) |
| **Encryption (in transit)** | Delegated to cloud provider (HTTPS for WebDAV, OAuth for Dropbox) |
| **Encryption (at rest)** | **NONE** — JSON files are cleartext |
| **Credentials storage** | KOReader stores cloud credentials; plugin doesn't handle them |
| **Settings data exposure** | Settings sync JSON is cleartext — passwords, tokens, paths exposed |

### 17.2 Risks

1. **Settings sync leaks sensitive data**: The settings diff includes `terminal_shell`, `device_id`, `httpinspector.port`, and potentially other configuration that reveals system details. The exclusion list attempts to filter passwords, but it's a whitelist of exclusions, not a principled encryption.

2. **No annotation encryption**: All annotation text, notes, and positions are stored in plaintext JSON in the cloud.

3. **No end-to-end encryption**: Anyone with access to the cloud storage (Dropbox/WebDAV) can read all annotations.

4. **Developer's note in README**: "Synced settings are stored in cleartext (unencrypted JSON) on your cloud storage... Avoid syncing sensitive or private configuration values."

---

## 18. Extensibility for CrossInk

### 18.1 What Would Need to Change

| Layer | KOReader → CrossInk Adaptation |
|-------|-------------------------------|
| **Plugin entry** | Complete rewrite. CrossInk has `Activity` lifecycle, not `WidgetContainer`. |
| **Data model** | Reusable concept. Model data structure needs a C++ equivalent (struct + JSON). |
| **Annotation storage** | KOReader uses `docsettings` sidecar → CrossInk uses `crosspoint.db` or sidecar JSON. |
| **Annotation types** | KOReader's `pos0/pos1` (XPath or pixel) → CrossInk's EPUB CFI / position system. |
| **Book identification** | MD5 of path → Switch to content hash or UUID-based identification. |
| **Position comparison** | `document:compareXPointers()` → Need equivalent for EPUB on CrossInk (or CFI comparison). |
| **Cloud sync** | KOReader CloudStorage+ → Custom implementation using HTTP client on ESP32. |
| **Merge engine** | **Reusable** — the core algorithm is format-agnostic with an abstraction for position comparison. |
| **Conflict resolution** | Reusable concept. Timestamp comparison is simple but portable. |
| **UI** | Complete rewrite. KOReader widgets → CrossInk's GUI/UITheme system. |
| **Progress sync** | Reusable concept. Data format is simple JSON. |
| **Settings sync** | Not applicable. CrossInk settings are different from KOReader. |

### 18.2 What Can Be Reused as-is

- **Merge algorithm** (annotations.lua, ~80%): The two-pointer merge, list↔map conversion, key generation strategy, deletion detection. Requires porting from Lua to C++.
- **Three-file merge pattern**: The local/cached/income pattern is transport-agnostic.
- **Timestamp-based conflict resolution**: Portable concept.
- **Soft-delete with tombstone**: Portable concept.
- **Device-specific progress tracking**: The multi-device progress JSON format is simple and portable.

### 18.3 What Must Be Rebuilt

- **Cloud transport**: KOReader's cloud infrastructure → Custom HTTP/SD-sync mechanism
- **Position system**: KOReader XPath/Pixel coords → CrossInk EPUB CFI or CrossPig position system
- **Book identity**: MD5-of-path → Content-hash or UUID
- **UI**: All menus and dialogs
- **Event handling**: KOReader events → CrossInk's Activity lifecycle

---

## 19. Proposed Architecture

### 19.1 Overview

After analyzing AnnotationSync, I recommend **not forking** and **not partially reusing** the code directly. Instead, create a **new cross-reader sync ecosystem** inspired by AnnotationSync's merge algorithm but with a clean, layered architecture.

```
┌─────────────────────────────────────────────────────────────┐
│                     Sync Application                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ KOReader     │  │ CrossInk     │  │ Future Reader X  │   │
│  │ Adapter      │  │ Adapter      │  │ Adapter          │   │
│  └──────┬───────┘  └──────┬───────┘  └───────┬──────────┘   │
│         │                 │                   │              │
│         └─────────┬───────┴───────────────────┘              │
│                   │                                          │
│  ┌────────────────▼──────────────────────────────────┐       │
│  │           Canonical Annotation Model               │       │
│  │  ┌─────────────────────────────────────────────┐  │       │
│  │  │ Annotation {                               │  │       │
│  │  │   id: UUID (v7, time-sortable)             │  │       │
│  │  │   book_id: BookUUID                        │  │       │
│  │  │   device_id: string                        │  │       │
│  │  │   type: highlight|bookmark|note|drawing     │  │       │
│  │  │   location: Location (typed union)          │  │       │
│  │  │   text: string                             │  │       │
│  │  │   note: string                             │  │       │
│  │  │   created_at: UnixNanos                    │  │       │
│  │  │   updated_at: UnixNanos                    │  │       │
│  │  │   deleted: bool                            │  │       │
│  │  │   revision: u64 (device-local version)      │  │       │
│  │  │   tombstone_at: UnixNanos                  │  │       │
│  │  │   sync_state: Synced|Pending|Conflict      │  │       │
│  │  }                                            │  │       │
│  │  └─────────────────────────────────────────────┘  │       │
│  └────────────────────────────────────────────────────┘       │
│                   │                                          │
│  ┌────────────────▼──────────────────────────────────┐       │
│  │              Sync Engine                            │       │
│  │  ┌─────────────┐  ┌────────────┐  ┌────────────┐  │       │
│  │  │ Conflict     │  │ Merge      │  │ Version    │  │       │
│  │  │ Resolver     │  │ Algorithm  │  │ Tracker    │  │       │
│  │  └─────────────┘  └────────────┘  └────────────┘  │       │
│  └────────────────────────────────────────────────────┘       │
│                   │                                          │
│  ┌────────────────▼──────────────────────────────────┐       │
│  │            Storage Provider                         │       │
│  │  ┌─────────────┐  ┌────────────┐                    │       │
│  │  │ Local Store  │  │ Local DB   │                    │       │
│  │  │ (JSON/Lua)   │  │ (SQLite)   │                    │       │
│  │  └─────────────┘  └────────────┘                    │       │
│  └────────────────────────────────────────────────────┘       │
│                   │                                          │
│  ┌────────────────▼──────────────────────────────────┐       │
│  │           Remote Provider                           │       │
│  │  ┌─────────────┐  ┌────────────┐  ┌────────────┐  │       │
│  │  │ HTTP/Sync   │  │ WebDAV     │  │ Local FS   │  │       │
│  │  │ (REST API)  │  │ Adapter    │  │ (USB/SCP)  │  │       │
│  │  └─────────────┘  └────────────┘  └────────────┘  │       │
│  └────────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

### 19.2 Layer Responsibilities

#### Reader Adapter
- Translates reader-native annotation format ↔ Canonical Model
- Handles reader-specific position systems (XPath, CFI, pixel coords, page numbers)
- Provides position comparison for the reader's format
- Handles reader lifecycle events

#### Canonical Annotation Model
- Pure data structures (no dependencies on readers)
- UUID v7 for globally unique, time-sortable IDs
- Book identity via content hash + metadata UUID
- Typed location system (enum + format-specific payload)
- Revision counters for causal tracking

#### Sync Engine
- **Conflict Resolver**: Field-level merge + version vector comparison (not just timestamp)
- **Merge Algorithm**: Position-based + UUID-based two-pointer merge
- **Version Tracker**: Tracks per-device revisions for each annotation

#### Storage Provider
- **Local Store**: Device-local persistence (JSON files or SQLite)
- Handles the sync state machine (pending / synced / conflict)

#### Remote Provider
- Abstract transport layer for various backends
- HTTP REST API (recommended for new protocol)
- WebDAV adapter for backward compatibility
- Future: Dropbox, Google Drive, etc.

### 19.3 Data Flow

```
Reader creates highlight
  ↓
Adapter.onAnnotationCreated(reader_annotation)
  ↓
Converts to Canonical Annotation
  ├── UUID generated (v7)
  ├── Book resolved via content hash
  ├── Location converted to canonical format
  ├── device_id set
  └── revision = local_counter++
  ↓
Local Store.save(annotation)
  ↓
Sync Engine.markPending(annotation)
  ↓
[when network available]
Sync Engine.syncPending()
  ├── Fetch remote changes for same book
  ├── Merge with local changes
  ├── Resolve conflicts
  └── Push merged state back
```

---

## 20. Proposed Protocol

### 20.1 Entities

```jsonc
// Canonical Annotation
{
  "id": "0193fa2e-5b71-7f00-8000-000000000001",
  // UUID v7: time-sortable, globally unique

  "book_id": "urn:uuid:550e8400-e29b-41d4-a716-446655440000",
  // The book's canonical identity. Resolved from EPUB <dc:identifier>
  // or content hash, NOT file path.

  "device_id": "kobo-clara-hd-abc123",
  // Stable device identifier (hardware serial or random UUID)

  "type": "highlight",
  // Enum: highlight | bookmark | note | drawing

  "location": {
    "format": "epub-cfi",
    // Enum: epub-cfi | xpath | page-offset | pixel-box | unknown
    "value": "epubcfi(/6/10[chap03]!/4/2/28/2/1:200)",
    // Format-specific location string
    "text": "the selected passage text",
    "context": {
      "before": "some text before ",
      "after": " some text after"
    }
    // For fuzzy re-matching on different editions
  },

  "content": {
    "text": "the selected text",
    "note": "user's annotation note",
    "color": "#FFEB3B",
    "style": "underline"
    // reader-specific content
  },

  "timestamps": {
    "created_at": 1737124200000000000,
    // Unix nanoseconds — portable across timezones
    "updated_at": 1737210600000000000
  },

  "state": {
    "deleted": false,
    "deleted_at": null,
    "revision": 42,
    // Monotonic per-device counter for this annotation
    "synced": true
  },

  "metadata": {
    "chapter": "Chapter 3",
    "chapter_index": 3,
    "percentage": 0.42
  }
}
```

```jsonc
// Book Identity
{
  "book_id": "urn:uuid:550e8400-e29b-41d4-a716-446655440000",
  "content_hash": "sha256:abcdef...",
  // SHA-256 of normalized EPUB content (strip metadata that differs between stores)
  "metadata": {
    "title": "Moby Dick",
    "author": "Herman Melville",
    "isbn": "978-0142437247",
    "epub_uuid": "urn:uuid:original-publisher-uuid"
  },
  "files": {
    "/books/moby.epub": {
      "device_id": "kobo-clara-hd",
      "path": "/books/moby.epub"
    },
    "/ebooks/md/moby_dick.epub": {
      "device_id": "phone-android",
      "path": "/ebooks/md/moby_dick.epub"
    }
  }
}
```

```jsonc
// Sync Manifest (per book)
{
  "book_id": "urn:uuid:550e8400-...",
  "devices": {
    "kobo-clara-hd": {
      "last_sync": 1737210600000000000,
      "last_revision": 42,
      "last_purge": 1737000000000000000
    },
    "phone-android": {
      "last_sync": 1737200000000000000,
      "last_revision": 38
    }
  }
}
```

```jsonc
// Reading Progress (per book)
{
  "book_id": "urn:uuid:550e8400-...",
  "devices": {
    "kobo-clara-hd": {
      "progress": {
        "position": "epubcfi(/6/10!/4/2/28)",
        "page": 142,
        "percentage": 0.45,
        "last_word": "whale"
      },
      "updated_at": 1737210600000000000
    }
  }
}
```

### 20.2 REST API (Recommended)

```
Base URL: https://sync.example.com/api/v1

Authentication: Bearer token (JWT or API key)

Endpoints:
  POST   /sync/books                    — Register a book identity
  GET    /sync/books/:book_id           — Get book identity + device location map
  GET    /sync/books/:book_id/annotations?since=<revision>
                                       — Get annotations since revision
  POST   /sync/books/:book_id/annotations
                                       — Push local annotations
                                       — Body: { annotations: [...], device_id, revisions: {...} }
                                       — Response: { merged: [...], conflicts: [...] }
  GET    /sync/books/:book_id/progress  — Get all device progress
  POST   /sync/books/:book_id/progress  — Push device progress
  POST   /sync/purge                   — Purge tombstones older than X days

WebSocket:
  ws://sync.example.com/api/v1/events  — Real-time sync notifications
```

### 20.3 Sync Protocol Flow

```
DEVICE A                              SERVER                              DEVICE B
   │                                     │                                    │
   │  POST /sync/books/:id/annotations   │                                    │
   │  { annotations: [A1, A2] }         │                                    │
   │ ───────────────────────────────────→│                                    │
   │                                     │                                    │
   │                                     │  Check revisions                   │
   │                                     │  Merge: A1 (new), A2 (conflict)   │
   │                                     │                                    │
   │  Response:                          │                                    │
   │  { merged: [A1], conflicts: [A2] }  │                                    │
   │ ←───────────────────────────────────│                                    │
   │                                     │                                    │
   │  (User resolves conflict on A)      │                                    │
   │                                     │                                    │
   │  POST /sync/books/:id/annotations   │                                    │
   │  { annotations: [A2_resolved] }    │  GET /sync/books/:id/annotations   │
   │ ───────────────────────────────────→│  ?since=42                        │
   │                                     │ ←──────────────────────────────────│
   │                                     │                                    │
   │                                     │  Response:                         │
   │                                     │  { annotations: [A1, A2_resolved],│
   │                                     │    conflict: [] }                 │
   │                                     │ ──────────────────────────────────→│
   │                                     │                                    │
```

### 20.4 Merge Algorithm (Enhanced)

```
Input:
  - local_annotations: map[UUID]Annotation
  - remote_annotations: map[UUID]Annotation
  - device_id

Algorithm:
  1. For each annotation in remote but not in local → ADD to local
  2. For each annotation in local but not in remote → ADD to remote (push)
  3. For each annotation in BOTH:
     a. If remote.revision == local.revision → skip (no change)
     b. If local.updated_at > remote.updated_at → local wins
     c. If remote.updated_at > local.updated_at → remote wins
     d. If CONCURRENT (same updated_at or version vector conflict):
        - Field-level merge:
          - If local.note and remote.note are different → flag as conflict
          - If local.text and remote.text are different → flag as conflict
          - If local.deleted != remote.deleted:
            - Deletion with newer timestamp wins
        - If auto-merge not possible → mark annotation as CONFLICT
  4. Tombstone GC:
     - Remove annotations where deleted=true AND deleted_at < (now - 90 days)
     - Only if both sides have acknowledged the deletion
```

### 20.5 Version Vectors

Replace simple timestamps with per-device version vectors:

```jsonc
{
  "id": "0193fa2e-...",
  "revisions": {
    "kobo-clara-hd": 42,
    "phone-android": 35
  },
  // True if ANY device has unmerged changes
  "has_conflict": false
}
```

This allows detecting concurrent modifications without relying on wall clocks.

---

## 21. Comparison Table

| Criterion | AnnotationSync (current) | Proposed Architecture |
|-----------|-------------------------|---------------------|
| **Annotation identity** | Position-based (no UUID) | UUID v7 (globally unique) |
| **Book identity** | MD5 of file path | Content hash + EPUB UUID |
| **Location system** | XPath / pixel coords (KOReader-specific) | Abstract CFI/XPath/offset with context text |
| **Conflict resolution** | Last-write-wins (timestamp string) | Version vectors + field-level merge |
| **Conflict notification** | None (silent) | Conflict flag + user resolution UI |
| **Deletion** | Soft delete (tombstone, no GC) | Soft delete with time-based GC |
| **Sync transport** | KOReader CloudStorage+ / SyncService | REST API (or any transport adapter) |
| **Protocol** | File-based (3-file pattern) | API-based (annotation-level sync) |
| **Server requirement** | None (uses cloud storage files) | Optional (can be self-hosted or P2P) |
| **Offline support** | Queue + retry on network | Queue + exponential backoff |
| **Incremental sync** | No (full file upload each time) | Yes (since=<revision> parameter) |
| **Progress sync** | Per-device JSON | Canonical progress model |
| **Settings sync** | KOReader-specific Lua settings | Not in scope (reader-specific) |
| **Encryption** | None (cleartext JSON) | Optional E2EE (client-side encrypt payload) |
| **Performance** | Loads everything into memory | Pagination + streaming support |
| **Cross-reader** | KOReader only | KOReader, CrossInk, Calibre, Obsidian |
| **Portability** | ~20% code portable | ~90% code portable (reader adapters only) |
| **Implementation effort** | Existing (ready) | New (significant effort) |
| **Memory footprint** | 50-100 MB for large books | Configurable (adapts to ESP32 constraints) |
| **Revisions** | None | Per-device revision counters |

---

## 22. Conclusion

### 22.1 Question 1: Fork AnnotationSync?

**NO.** Forking does not make sense because:

- **Licensing risk**: The project is MIT-licensed, but forking a KOReader plugin doesn't help CrossInk — it's entirely Lua-based, KOReader-coupled code.
- **Different architecture**: CrossInk is C++ on ESP32-C3 with 380 KB RAM. KOReader is LuaJIT on Linux with 256+ MB RAM. The architectures are fundamentally incompatible.
- **Coupling is too deep**: ~80% of AnnotationSync's code is KOReader-specific. A fork would need to rewrite most of it for CrossInk anyway.
- **Maintenance burden**: Maintaining a fork against upstream changes to both KOReader and CrossInk would be excessive.

### 22.2 Question 2: Reuse Parts?

**LIMITED YES.** The following concepts/approaches are worth reusing:

| Concept | Why reuse |
|---------|-----------|
| **Three-file merge pattern** | Simple, well-tested, transport-agnostic pattern. The local/cached/income approach works regardless of protocol. |
| **Position-based merge with two-pointer traversal** | The merge algorithm in `annotations.lua` is clean and correct for annotations without UUIDs. It can be adapted to use UUIDs as primary identity with positions as fallback. |
| **Timestamp-based conflict resolution (with enhancements)** | Simple and predictable. Can be enhanced with version vectors. |
| **Soft delete with tombstone** | Proven pattern. Add GC timeout. |
| **Progress sync with device-specific entries** | The per-device key approach for progress is clean and works for multiple readers. |
| **Safety guard (empty local check)** | Critical protection against data loss. Must be preserved in any new implementation. |

**Do NOT reuse as code** — port the concepts to C++.

### 22.3 Question 3: Create New Project Inspired By It?

**YES — absolutely.** The approach is correct in concept (file-based sync with position-aware merge), but the implementation is too KOReader-specific. A new project should:

1. Define a **canonical annotation model** with UUIDs, content-based book identity, and typed locations
2. Build a **sync engine** protocol-agnostic and transport-agnostic
3. Implement **reader adapters** as thin layers that translate reader-native → canonical
4. Support **multiple transport backends**: REST API, WebDAV, direct file sync, p2p
5. Design for **constrained devices** from day one (ESP32-C3's 380 KB RAM)

### 22.4 Question 4: Recommended Architecture

```
                    ┌──────────────────────┐
                    │   AnnoSync Protocol  │
                    │   (New Project)      │
                    └──────────────────────┘
                              │
         ┌────────────────────┼────────────────────┐
         │                    │                    │
         ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ KOReader Plugin │  │ CrossInk Module │  │ CLI/Desktop App │
│ (Lua, thin)     │  │ (C++, thin)     │  │ (TypeScript)    │
└─────────────────┘  └─────────────────┘  └─────────────────┘
         │                    │                    │
         └────────────┬───────┴────────────────────┘
                      │
         ┌────────────▼────────────┐
         │   Canonical Protocol    │
         │   JSON + REST/File      │
         │   (Shared)              │
         └─────────────────────────┘
```

**Roadmap suggestion:**

| Phase | What | Duration |
|-------|------|----------|
| 1 | Define canonical annotation model + JSON serialization | 1-2 weeks |
| 2 | Implement sync engine (merge, conflict, version vectors) | 2-3 weeks |
| 3 | Implement REST backend (server) | 2-3 weeks |
| 4 | Build KOReader adapter (thin Lua plugin) | 1-2 weeks |
| 5 | Build CrossInk adapter (C++ module) | 2-3 weeks |
| 6 | Implement progress sync | 1 week |
| 7 | Implement settings sync (optional, reader-specific) | 1 week |
| 8 | Testing, hardening, E2EE | 2-3 weeks |

**Total estimate**: 12-18 weeks for a production-ready sync ecosystem.

---

## Appendix A: Key Files Reference (AnnotationSync)

| File | URL | Lines |
|------|-----|-------|
| `_meta.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/_meta.lua) | 10 |
| `main.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/main.lua) | ~280 |
| `annotations.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/annotations.lua) | ~180 |
| `manager.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/manager.lua) | ~320 |
| `remote.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/remote.lua) | ~210 |
| `utils.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/utils.lua) | ~45 |
| `menus.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/menus.lua) | ~220 |
| `settings_selection.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/settings_selection.lua) | ~280 |
| `sync_integration_spec.lua` | [link](https://github.com/dani84bs/AnnotationSync.koplugin/blob/main/spec/unit/sync_integration_spec.lua) | ~200 |

## Appendix B: Async Sync Flow Diagram

```
User taps "Manual Sync"
  │
  ├── manager:syncDocument(document, true, callback)
  │     │
  │     ├── writeAnnotationsJSON(document)
  │     │     └── Returns json_path (SDR_DIR/<hash>.json)
  │     │
  │     └── remote.sync_annotations(plugin, document, json_path, callback, force)
  │           │
  │           ├── If CloudStorage+:
  │           │     cloudstorage:sync(server, json_path, sync_cb, is_silent)
  │           │       │
  │           │       ├── Uploads json_path → remote
  │           │       ├── Downloads remote → json_path.temp (income_file)
  │           │       ├── Creates/reads json_path.sync (cached_file)
  │           │       └── Calls sync_cb(json_path, json_path.sync, json_path.temp)
  │           │             │
  │           │             └── annotations.sync_callback(document, local, cached, income, force)
  │           │                   │
  │           │                   └── Returns (success, merged_list)
  │           │
  │           └── On callback:
  │                 manager:_onSyncComplete(document, success, merged_list)
  │                   ├── If success + merged_list: applySyncedAnnotations()
  │                   └── removeFromChangedDocumentsFile()
  │
  └── manager:updateLastSync("Manual Sync")
```

## Appendix C: Annotation Key Examples

### EPUB Highlight
```
Key: "//body/section/p[3]/text()[1]/word[5]||//body/section/p[3]/text()[1]/word[12]"
      └─────── pos0 (XPath start) ───────┘||└─────── pos1 (XPath end) ───────┘
```

### PDF Highlight
```
Key: "3|100|200||150|250"
      └─ page|zoom-x|zoom-y ┘||└ zoom-x|zoom-y ┘
```

### Bookmark
```
Key: "BOOKMARK|42"
      └── type ──┘└ page ┘
```
