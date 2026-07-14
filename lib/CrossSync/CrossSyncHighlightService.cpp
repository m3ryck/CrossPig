#include "CrossSyncHighlightService.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "../../src/ClippingStore.h"
#include "ChapterXPathResolver.h"
#include "CrossSyncCredentialStore.h"
#include "CrossSyncHighlightDocument.h"
#include "CrossSyncWebDavClient.h"
#include "ProgressMapper.h"

namespace {
CrossSyncMatchType toCrossSyncMatchType(const DocumentMatchMethod method) {
  return method == DocumentMatchMethod::BINARY ? CrossSyncMatchType::Binary : CrossSyncMatchType::Filename;
}

std::string collectionPath(const CrossSyncMatchType matchType) {
  return CROSSSYNC_STORE.getRootPath() + "/v1/highlights/" + CrossSyncHighlightJson::matchTypeToString(matchType);
}

std::string remotePath(const std::string& documentHash, const CrossSyncMatchType matchType) {
  return collectionPath(matchType) + "/" + documentHash + ".json";
}

CrossSyncHighlight toSyncHighlight(const Clipping& clipping, const std::shared_ptr<Epub>& epub) {
  CrossSyncHighlight highlight;
  highlight.spine = clipping.spineIndex;
  highlight.startPage = clipping.startPage;
  highlight.endPage = clipping.endPage;
  highlight.pageCount = clipping.pageCount;
  highlight.startWord = clipping.startWordIndex;
  highlight.endWord = clipping.endWordIndex;
  highlight.wordCount = clipping.wordCount;
  highlight.paragraph = clipping.paragraphIndex;
  highlight.createdAt = clipping.timestamp;
  highlight.chapter = clipping.chapterTitle;
  highlight.text = clipping.text;
  if (epub) {
    ChapterXPathResolver::findXPathRangeForText(epub, clipping.spineIndex, clipping.paragraphIndex, clipping.text,
                                                highlight.pos0, highlight.pos1);
  }
  return highlight;
}

Clipping toClipping(const CrossSyncHighlight& highlight) {
  Clipping clipping;
  clipping.spineIndex = highlight.spine;
  clipping.startPage = highlight.startPage;
  clipping.endPage = highlight.endPage;
  clipping.pageCount = std::max<uint16_t>(1, highlight.pageCount);
  clipping.startWordIndex = highlight.startWord;
  clipping.endWordIndex = highlight.endWord;
  clipping.wordCount = highlight.wordCount;
  clipping.paragraphIndex = highlight.paragraph;
  clipping.timestamp = highlight.createdAt;
  strncpy(clipping.chapterTitle, highlight.chapter.c_str(), sizeof(clipping.chapterTitle) - 1);
  clipping.chapterTitle[sizeof(clipping.chapterTitle) - 1] = '\0';
  clipping.text.assign(highlight.text.data(), std::min(highlight.text.size(), CLIPPING_TEXT_MAX));
  return clipping;
}

bool hasUsableCrossInkAnchor(const CrossSyncHighlight& highlight) {
  return !highlight.text.empty() &&
         (highlight.wordCount > 0 || highlight.paragraph != UINT16_MAX || highlight.startWord != highlight.endWord ||
          highlight.spine > 0 || highlight.startPage > 0 || highlight.pageCount > 1);
}

bool hasKOReaderAnchor(const CrossSyncHighlight& highlight) { return !highlight.pos0.empty(); }

bool fillCrossInkAnchorFromKOReader(CrossSyncHighlight& highlight, const std::shared_ptr<Epub>& epub,
                                    const int currentSpineIndex, const int totalPagesInCurrentSpine) {
  if (hasUsableCrossInkAnchor(highlight) || !hasKOReaderAnchor(highlight) || !epub) {
    return hasUsableCrossInkAnchor(highlight);
  }

  const KOReaderPosition koPos{highlight.pos0, 0.0f};
  const CrossPointPosition pos = ProgressMapper::toCrossPoint(epub, koPos, currentSpineIndex, totalPagesInCurrentSpine);
  if (pos.totalPages <= 0) {
    LOG_ERR("CrossSync", "Could not map KOReader highlight position to CrossInk anchor: %s", highlight.pos0.c_str());
    return false;
  }

  highlight.spine = static_cast<uint16_t>(std::max(0, pos.spineIndex));
  highlight.startPage = static_cast<uint16_t>(std::max(0, pos.pageNumber));
  highlight.endPage = highlight.startPage;
  highlight.pageCount = static_cast<uint16_t>(std::max(1, pos.totalPages));
  if (pos.hasParagraphIndex) {
    highlight.paragraph = pos.paragraphIndex;
  }
  LOG_DBG("CrossSync", "Mapped KOReader pos %s -> spine=%u page=%u/%u paragraph=%u", highlight.pos0.c_str(),
          highlight.spine, highlight.startPage, highlight.pageCount, highlight.paragraph);
  return true;
}

bool hasUsableCrossInkAnchor(const Clipping& clipping) {
  return !clipping.text.empty() &&
         (clipping.wordCount > 0 || clipping.paragraphIndex != UINT16_MAX ||
          clipping.startWordIndex != clipping.endWordIndex || clipping.spineIndex > 0 || clipping.startPage > 0 ||
          clipping.pageCount > 1);
}

bool isSameHighlight(const CrossSyncHighlight& a, const CrossSyncHighlight& b) {
  if (a.text != b.text) return false;
  if (a.spine != b.spine || a.startPage != b.startPage || a.endPage != b.endPage) return false;
  if (a.paragraph != UINT16_MAX && b.paragraph != UINT16_MAX && a.paragraph != b.paragraph) return false;
  return true;
}

bool isBetterHighlight(const CrossSyncHighlight& candidate, const CrossSyncHighlight& current) {
  if (candidate.wordCount != current.wordCount) return candidate.wordCount > current.wordCount;
  const bool candidateHasKOReaderPos = !candidate.pos0.empty() && !candidate.pos1.empty();
  const bool currentHasKOReaderPos = !current.pos0.empty() && !current.pos1.empty();
  if (candidateHasKOReaderPos != currentHasKOReaderPos) return candidateHasKOReaderPos;
  return candidate.createdAt > current.createdAt;
}

void addDedupedHighlight(std::vector<CrossSyncHighlight>& highlights, CrossSyncHighlight&& highlight) {
  for (auto& existing : highlights) {
    if (!isSameHighlight(existing, highlight)) continue;
    if (isBetterHighlight(highlight, existing)) {
      existing = std::move(highlight);
    }
    return;
  }
  highlights.push_back(std::move(highlight));
}

CrossSyncHighlightService::Result uploadLocal(const std::string& documentHash, const CrossSyncMatchType matchType,
                                              const std::shared_ptr<Epub>& epub) {
  CrossSyncHighlightDocument document;
  document.book = documentHash;
  document.matchType = matchType;
  document.source = "crossink";
  const auto& clippings = CLIPPINGS.getClippings();
  document.highlights.reserve(clippings.size());
  for (const auto& clipping : clippings) {
    if (!hasUsableCrossInkAnchor(clipping)) {
      LOG_ERR("CrossSync", "Skipping local highlight without usable CrossInk anchor: %.48s", clipping.text.c_str());
      continue;
    }
    addDedupedHighlight(document.highlights, toSyncHighlight(clipping, epub));
  }

  std::string json;
  if (!CrossSyncHighlightJson::serialize(document, json)) {
    return CrossSyncHighlightService::Result::ParseError;
  }

  auto result = CrossSyncWebDavClient::ensureCollectionPath(collectionPath(matchType));
  if (result != CrossSyncWebDavClient::Error::OK) {
    LOG_ERR("CrossSync", "Failed to ensure remote collection: %s", CrossSyncWebDavClient::errorString(result));
    return result == CrossSyncWebDavClient::Error::NotConfigured ? CrossSyncHighlightService::Result::NotConfigured
                                                                 : CrossSyncHighlightService::Result::NetworkError;
  }

  result = CrossSyncWebDavClient::putTextFile(remotePath(documentHash, matchType), json);
  if (result != CrossSyncWebDavClient::Error::OK) {
    LOG_ERR("CrossSync", "Failed to upload highlights: %s", CrossSyncWebDavClient::errorString(result));
    return result == CrossSyncWebDavClient::Error::NotConfigured ? CrossSyncHighlightService::Result::NotConfigured
                                                                 : CrossSyncHighlightService::Result::NetworkError;
  }

  return CrossSyncHighlightService::Result::OK;
}

CrossSyncHighlightService::Result pullRemote(const std::string& documentHash, const CrossSyncMatchType matchType,
                                             const std::shared_ptr<Epub>& epub, const int currentSpineIndex,
                                             const int totalPagesInCurrentSpine) {
  std::string json;
  const auto result = CrossSyncWebDavClient::getTextFile(remotePath(documentHash, matchType), json);
  if (result == CrossSyncWebDavClient::Error::NotFound) {
    if (!CLIPPINGS.replaceAllForSync({})) {
      return CrossSyncHighlightService::Result::LocalStoreError;
    }
    return CrossSyncHighlightService::Result::RemoteMissingAppliedAsEmpty;
  }
  if (result != CrossSyncWebDavClient::Error::OK) {
    LOG_ERR("CrossSync", "Failed to download highlights: %s", CrossSyncWebDavClient::errorString(result));
    return result == CrossSyncWebDavClient::Error::NotConfigured ? CrossSyncHighlightService::Result::NotConfigured
                                                                 : CrossSyncHighlightService::Result::NetworkError;
  }

  CrossSyncHighlightDocument document;
  if (!CrossSyncHighlightJson::parse(json.c_str(), document)) {
    return CrossSyncHighlightService::Result::ParseError;
  }
  if (document.book != documentHash || document.matchType != matchType) {
    LOG_ERR("CrossSync", "Remote highlight document identity mismatch");
    return CrossSyncHighlightService::Result::ParseError;
  }

  std::vector<Clipping> clippings;
  clippings.reserve(document.highlights.size());
  for (auto& highlight : document.highlights) {
    fillCrossInkAnchorFromKOReader(highlight, epub, currentSpineIndex, totalPagesInCurrentSpine);
    if (!hasUsableCrossInkAnchor(highlight)) {
      LOG_ERR("CrossSync", "Skipping remote highlight without usable CrossInk anchor: %.48s", highlight.text.c_str());
      continue;
    }
    clippings.push_back(toClipping(highlight));
  }

  if (!CLIPPINGS.replaceAllForSync(clippings)) {
    return CrossSyncHighlightService::Result::LocalStoreError;
  }
  return CrossSyncHighlightService::Result::OK;
}
}  // namespace

CrossSyncHighlightService::Result CrossSyncHighlightService::sync(const CrossSyncDirection direction,
                                                                  const std::string& documentHash,
                                                                  const DocumentMatchMethod matchMethod,
                                                                  const std::shared_ptr<Epub>& epub,
                                                                  const int currentSpineIndex,
                                                                  const int totalPagesInCurrentSpine) {
  if (!CROSSSYNC_STORE.isEnabled()) return Result::Skipped;
  if (documentHash.empty()) return Result::NotConfigured;
  if (!CROSSSYNC_STORE.hasConfig()) return Result::NotConfigured;

  const CrossSyncMatchType matchType = toCrossSyncMatchType(matchMethod);
  LOG_DBG("CrossSync", "%s highlights for %s (%s)",
          direction == CrossSyncDirection::PullRemote ? "Pulling" : "Uploading", documentHash.c_str(),
          CrossSyncHighlightJson::matchTypeToString(matchType));

  return direction == CrossSyncDirection::PullRemote
             ? pullRemote(documentHash, matchType, epub, currentSpineIndex, totalPagesInCurrentSpine)
             : uploadLocal(documentHash, matchType, epub);
}

const char* CrossSyncHighlightService::resultString(const Result result) {
  switch (result) {
    case Result::Skipped:
      return "CrossSync skipped";
    case Result::OK:
      return "CrossSync highlights synced";
    case Result::NotConfigured:
      return "CrossSync is not configured";
    case Result::RemoteMissingAppliedAsEmpty:
      return "No remote CrossSync highlights; local highlights cleared";
    case Result::ParseError:
      return "CrossSync highlight data is invalid";
    case Result::NetworkError:
      return "CrossSync network error";
    case Result::LocalStoreError:
      return "CrossSync could not update local highlights";
  }
  return "CrossSync error";
}

std::string CrossSyncHighlightService::resultDetail(const Result result) {
  std::string detail = resultString(result);
  const std::string webDavDetail = CrossSyncWebDavClient::lastErrorDetail();
  if (!webDavDetail.empty()) {
    detail += ": ";
    detail += webDavDetail;
  }
  return detail;
}
