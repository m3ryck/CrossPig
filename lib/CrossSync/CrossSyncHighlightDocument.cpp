#include "CrossSyncHighlightDocument.h"

#include <ArduinoJson.h>
#ifdef SIMULATOR
#include <ArduinoJsonStringCompat.h>
#endif
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "../../src/ClippingStore.h"

namespace {
constexpr const char* FORMAT = "crosssync.highlights";
constexpr uint8_t VERSION = 1;

std::string boundedString(const char* value, const size_t maxLen) {
  if (!value) return "";
  return std::string(value, std::min(strlen(value), maxLen));
}

uint16_t boundedU16(JsonVariantConst value, const uint16_t fallback = 0) {
  const uint32_t raw = value | static_cast<uint32_t>(fallback);
  return static_cast<uint16_t>(std::min<uint32_t>(raw, UINT16_MAX));
}
}  // namespace

namespace CrossSyncHighlightJson {

const char* matchTypeToString(const CrossSyncMatchType matchType) {
  return matchType == CrossSyncMatchType::Binary ? "binary" : "filename";
}

bool matchTypeFromString(const char* value, CrossSyncMatchType& out) {
  if (!value) return false;
  if (strcmp(value, "filename") == 0) {
    out = CrossSyncMatchType::Filename;
    return true;
  }
  if (strcmp(value, "binary") == 0) {
    out = CrossSyncMatchType::Binary;
    return true;
  }
  return false;
}

bool serialize(const CrossSyncHighlightDocument& document, std::string& outJson) {
  if (document.book.empty() || document.highlights.size() > CLIPPING_MAX_PER_BOOK) {
    LOG_ERR("CrossSync", "Invalid highlight document for serialization");
    return false;
  }

  JsonDocument doc;
  doc["format"] = FORMAT;
  doc["version"] = VERSION;
  doc["book"] = document.book;
  doc["match_type"] = matchTypeToString(document.matchType);
  doc["source"] = document.source.empty() ? "crossink" : document.source.c_str();

  JsonArray highlights = doc["highlights"].to<JsonArray>();
  for (const auto& highlight : document.highlights) {
    JsonObject item = highlights.add<JsonObject>();
    item["spine"] = highlight.spine;
    item["start_page"] = highlight.startPage;
    item["end_page"] = highlight.endPage;
    item["page_count"] = std::max<uint16_t>(1, highlight.pageCount);
    item["start_word"] = highlight.startWord;
    item["end_word"] = highlight.endWord;
    item["word_count"] = highlight.wordCount;
    item["paragraph"] = highlight.paragraph;
    item["chapter"] = highlight.chapter.substr(0, CLIPPING_CHAPTER_TITLE_MAX - 1);
    item["text"] = highlight.text.substr(0, CLIPPING_TEXT_MAX);
    if (!highlight.pos0.empty()) {
      item["pos0"] = highlight.pos0.substr(0, 255);
    }
    if (!highlight.pos1.empty()) {
      item["pos1"] = highlight.pos1.substr(0, 255);
    }
    if (highlight.createdAt > 0) {
      item["created_at"] = highlight.createdAt;
    }
  }

  String json;
  serializeJson(doc, json);
  outJson.assign(json.c_str(), json.length());
  return true;
}

bool parse(const char* json, CrossSyncHighlightDocument& outDocument) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json ? json : "");
  if (err) {
    LOG_ERR("CrossSync", "Highlight JSON parse failed: %s", err.c_str());
    return false;
  }

  if (!doc.is<JsonObject>()) {
    LOG_ERR("CrossSync", "Highlight JSON root is not an object");
    return false;
  }
  if (strcmp(doc["format"] | "", FORMAT) != 0 || (doc["version"] | 0) != VERSION) {
    LOG_ERR("CrossSync", "Unsupported highlight JSON format/version");
    return false;
  }

  const char* book = doc["book"] | "";
  if (book[0] == '\0') {
    LOG_ERR("CrossSync", "Highlight JSON missing book id");
    return false;
  }

  CrossSyncMatchType matchType;
  if (!matchTypeFromString(doc["match_type"] | "", matchType)) {
    LOG_ERR("CrossSync", "Highlight JSON has invalid match type");
    return false;
  }

  JsonArrayConst highlights = doc["highlights"].as<JsonArrayConst>();
  if (highlights.size() > CLIPPING_MAX_PER_BOOK) {
    LOG_ERR("CrossSync", "Remote highlight count exceeds local limit");
    return false;
  }

  CrossSyncHighlightDocument parsed;
  parsed.book = book;
  parsed.matchType = matchType;
  parsed.source = boundedString(doc["source"] | "unknown", 32);
  parsed.highlights.reserve(highlights.size());

  for (JsonObjectConst item : highlights) {
    CrossSyncHighlight highlight;
    highlight.spine = boundedU16(item["spine"]);
    highlight.startPage = boundedU16(item["start_page"]);
    highlight.endPage = boundedU16(item["end_page"], highlight.startPage);
    highlight.pageCount = std::max<uint16_t>(1, boundedU16(item["page_count"], 1));
    highlight.startWord = boundedU16(item["start_word"]);
    highlight.endWord = boundedU16(item["end_word"], highlight.startWord);
    highlight.wordCount = boundedU16(item["word_count"]);
    highlight.paragraph = boundedU16(item["paragraph"], UINT16_MAX);
    highlight.createdAt = item["created_at"] | static_cast<uint32_t>(0);
    highlight.chapter = boundedString(item["chapter"] | "", CLIPPING_CHAPTER_TITLE_MAX - 1);
    highlight.text = boundedString(item["text"] | "", CLIPPING_TEXT_MAX);
    highlight.pos0 = boundedString(item["pos0"] | "", 255);
    highlight.pos1 = boundedString(item["pos1"] | "", 255);
    parsed.highlights.push_back(std::move(highlight));
  }

  outDocument = std::move(parsed);
  return true;
}

}  // namespace CrossSyncHighlightJson
