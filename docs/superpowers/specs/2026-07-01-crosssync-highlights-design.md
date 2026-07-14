# CrossSync Highlights Design

## Goal

Implement CrossSync highlight synchronization as an extension of the existing Progress Sync user flow. Progress Sync remains responsible for asking the user which direction wins. CrossSync applies the same decision to highlights with absolute replacement semantics.

## Scope

This feature syncs highlights only. It does not sync notes, bookmarks, reading progress, statistics, history, tombstones, incremental deltas, or multiple users.

CrossSync credentials and storage are separate from KOReader Progress Sync credentials. The KOReader progress server cannot be used for highlights because its protocol only exposes auth and reading-progress endpoints. CrossSync v1 uses a drive-like storage backend, starting with WebDAV.

The KOReader plugin lives in a separate repository, `crossync.koplugin`. This repository implements the CrossInk side and documents the shared protocol. Plugin source is not created in this firmware repository unless a workspace for that repository is provided.

## User Flow

The existing Progress Sync screen remains the single synchronization UI.

When Progress Sync finds remote progress, it presents the existing options:

- Apply remote
- Upload local

If CrossSync is enabled and configured:

- Apply remote also downloads the remote CrossSync highlight JSON and replaces local CrossInk highlights for the current book.
- Upload local also serializes local CrossInk highlights and replaces the remote CrossSync highlight JSON for the current book.

If there is no remote progress and the user chooses upload, CrossSync uploads local highlights. If the user backs out, CrossSync does nothing.

No conflict resolution exists. The chosen side wins absolutely.

## Matching

CrossSync reuses the current Progress Sync document matching strategy:

- Filename: `KOReaderDocumentId::calculateFromFilename()`
- Binary: `KOReaderDocumentId::calculate()`

The selected match method decides the remote path:

```text
<rootPath>/v1/highlights/filename/<document_hash>.json
<rootPath>/v1/highlights/binary/<document_hash>.json
```

## Local Model

CrossInk already models highlights as `Clipping` records in `ClippingStore`. CrossSync does not add a second local highlight cache. It exports and imports the current book's `ClippingStore`.

The existing limits remain authoritative:

- Maximum highlights per book: `CLIPPING_MAX_PER_BOOK`
- Maximum in-app highlight text: `CLIPPING_TEXT_MAX`
- Maximum chapter title: `CLIPPING_CHAPTER_TITLE_MAX`

## CrossSync JSON V1

```json
{
  "format": "crosssync.highlights",
  "version": 1,
  "book": "document-md5",
  "match_type": "filename",
  "source": "crossink",
  "highlights": [
    {
      "spine": 0,
      "start_page": 3,
      "end_page": 3,
      "page_count": 22,
      "start_word": 4,
      "end_word": 17,
      "word_count": 14,
      "paragraph": 12,
      "chapter": "Chapter title",
      "text": "Highlighted text",
      "created_at": 1712345678
    }
  ]
}
```

`created_at` is best-effort in CrossInk v1 because existing clipping timestamps are seconds since firmware boot. The value must not be used for merge or conflict handling.

## WebDAV Backend

CrossSync v1 implements a small WebDAV client in firmware:

- `MKCOL` to ensure directories exist.
- `GET` to download the current book JSON.
- `PUT` to replace the current book JSON.

It does not implement PROPFIND browsing, locking, ETags, OAuth, Dropbox, or Google Drive in phase 1.

WebDAV credentials are stored separately in `/.crosspoint/crosssync.json`.

## Settings

CrossSync gets a small settings surface:

- Enable CrossSync highlights
- WebDAV server URL
- WebDAV root path
- Username
- Password

These settings are separate from KOReader Progress Sync settings. The document matching setting remains shared indirectly because CrossSync reads the current Progress Sync match method.

## Error Handling

Progress Sync must not fail solely because CrossSync is disabled or unconfigured.

If CrossSync is enabled but fails:

- Log the failure.
- Show the CrossSync error in the result state.
- Keep any already completed Progress Sync operation.

Remote missing behavior:

- Pull remote: missing highlight JSON means remote has no highlights, so local highlights are cleared.
- Upload local: creates or replaces the remote file.

## RAM Discipline

CrossSync performs work only during explicit sync. It must not create background tasks or persistent buffers.

JSON parse and serialize must stay bounded by the current highlight limits. WebDAV transfer buffers should be small and transient. No second framebuffer, no long-lived heap cache, and no repeated allocations in loops.

## Verification

Host tests cover JSON validation, serialization, path generation, and absolute pull/upload decision mapping. Firmware validation uses `pio run -e simulator` and `pio run -e default`.

Hardware verification uses an Xteink X4:

1. Configure Progress Sync and CrossSync WebDAV.
2. Open an EPUB.
3. Create a highlight.
4. Choose Upload local in Progress Sync.
5. Confirm `<rootPath>/v1/highlights/<match>/<hash>.json` is replaced on WebDAV.
6. Change the remote JSON or sync from KOReader.
7. Choose Apply remote in Progress Sync.
8. Confirm local highlights are replaced in the reader and clipping list.
