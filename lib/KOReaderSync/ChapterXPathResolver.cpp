#include "ChapterXPathResolver.h"

#include <Logging.h>
#include <Print.h>
#include <Utf8.h>
#include <XmlParserUtils.h>
#include <expat.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string stripPrefix(const XML_Char* name) {
  if (!name) {
    return "";
  }

  const char* local = std::strrchr(name, ':');
  return local ? std::string(local + 1) : std::string(name);
}

struct NameCounter {
  std::string name;
  int count;
};

struct ParentState {
  std::vector<NameCounter> children;

  int nextIndex(const std::string& name) {
    for (auto& child : children) {
      if (child.name == name) {
        child.count++;
        return child.count;
      }
    }

    children.push_back({name, 1});
    return 1;
  }
};

struct PathSegment {
  std::string name;
  int index;
};

std::string buildParagraphXPath(const int spineIndex, const std::vector<PathSegment>& path, const int textNodeIndex,
                                const size_t charOffset) {
  std::string xpath = "/body/DocFragment[" + std::to_string(spineIndex + 1) + "]/body";
  for (const auto& segment : path) {
    xpath += "/" + segment.name + "[" + std::to_string(segment.index) + "]";
  }
  if (textNodeIndex > 0 && charOffset > 0) {
    xpath += "/text()[" + std::to_string(textNodeIndex) + "]." + std::to_string(charOffset);
  }
  return xpath;
}

std::string buildTextXPath(const int spineIndex, const std::vector<PathSegment>& path, const int textNodeIndex,
                           const size_t charOffset) {
  std::string xpath = "/body/DocFragment[" + std::to_string(spineIndex + 1) + "]/body";
  for (const auto& segment : path) {
    xpath += "/" + segment.name + "[" + std::to_string(segment.index) + "]";
  }
  xpath += "/text()";
  if (textNodeIndex > 1) {
    xpath += "[" + std::to_string(textNodeIndex) + "]";
  }
  xpath += "." + std::to_string(charOffset);
  return xpath;
}

size_t countUtf8Codepoints(const XML_Char* data, const int len) {
  if (!data || len <= 0) {
    return 0;
  }

  size_t count = 0;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
  const unsigned char* end = ptr + len;
  while (ptr < end) {
    utf8NextCodepoint(&ptr);
    count++;
  }

  return count;
}

size_t countUtf8CodepointsInBytes(const std::string& text, const size_t byteStart, const size_t byteEnd) {
  if (byteStart >= byteEnd || byteStart >= text.size()) {
    return 0;
  }
  const size_t clampedEnd = std::min(byteEnd, text.size());
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(text.data() + byteStart);
  const unsigned char* end = reinterpret_cast<const unsigned char*>(text.data() + clampedEnd);
  size_t count = 0;
  while (ptr < end) {
    utf8NextCodepoint(&ptr);
    count++;
  }
  return count;
}

struct TextSegment {
  std::vector<PathSegment> path;
  int textNodeIndex;
  size_t startChar;
  size_t charCount;
};

bool locateTextRange(const std::string& haystack, const std::string& needle, size_t& startChar, size_t& endChar) {
  if (haystack.empty() || needle.empty()) {
    return false;
  }

  const size_t found = haystack.find(needle);
  if (found != std::string::npos) {
    startChar = countUtf8CodepointsInBytes(haystack, 0, found);
    endChar = startChar + countUtf8CodepointsInBytes(needle, 0, needle.size());
    return endChar > startChar;
  }

  return false;
}

const TextSegment* findSegmentForOffset(const std::vector<TextSegment>& segments, const size_t charOffset,
                                        const bool preferPreviousAtBoundary) {
  for (const auto& segment : segments) {
    const size_t segmentEnd = segment.startChar + segment.charCount;
    if (charOffset > segment.startChar && charOffset < segmentEnd) {
      return &segment;
    }
    if (charOffset == segment.startChar && !preferPreviousAtBoundary && segment.charCount > 0) {
      return &segment;
    }
    if (charOffset == segmentEnd && preferPreviousAtBoundary && segment.charCount > 0) {
      return &segment;
    }
  }
  return segments.empty() ? nullptr : &segments.back();
}

class ParagraphTextCounter final : public Print {
 public:
  ParagraphTextCounter() {
    parser = XML_ParserCreate(nullptr);
    if (!parser) {
      LOG_ERR("KOX", "Failed to create XML parser");
      return;
    }

    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, &ParagraphTextCounter::startElement, &ParagraphTextCounter::endElement);
    XML_SetCharacterDataHandler(parser, &ParagraphTextCounter::characterData);
  }

  ~ParagraphTextCounter() override { destroyXmlParser(parser); }

  bool ok() const { return parser != nullptr && parseOk; }

  bool finish() {
    if (!parser || !parseOk || stopped) {
      return parseOk;
    }

    if (XML_Parse(parser, "", 0, XML_TRUE) == XML_STATUS_ERROR) {
      LOG_ERR("KOX", "Final XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
      parseOk = false;
    }
    return parseOk;
  }

  size_t write(uint8_t c) override { return write(&c, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!parser || !parseOk || stopped) {
      return size;
    }

    if (XML_Parse(parser, reinterpret_cast<const char*>(buffer), static_cast<int>(size), XML_FALSE) != XML_STATUS_OK) {
      const enum XML_Error error = XML_GetErrorCode(parser);
      if (error != XML_ERROR_ABORTED) {
        LOG_ERR("KOX", "XML parse error: %s", XML_ErrorString(error));
        parseOk = false;
      }
    }

    return size;
  }

  size_t totalVisibleChars() const { return visibleChars; }

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char**) {
    auto* self = static_cast<ParagraphTextCounter*>(userData);
    self->onStartElement(name);
  }

  static void XMLCALL endElement(void* userData, const XML_Char* name) {
    auto* self = static_cast<ParagraphTextCounter*>(userData);
    self->onEndElement(name);
  }

  static void XMLCALL characterData(void* userData, const XML_Char* data, const int len) {
    auto* self = static_cast<ParagraphTextCounter*>(userData);
    self->onCharacterData(data, len);
  }

  void onStartElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    if (!insideBody) {
      if (name == "body") {
        insideBody = true;
        bodyDepth = depth;
      }
      depth++;
      return;
    }

    if (name == "p") {
      paragraphDepth++;
    }
    depth++;
  }

  void onEndElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    depth--;
    if (!insideBody) {
      return;
    }

    if (depth == bodyDepth && name == "body") {
      insideBody = false;
      return;
    }

    if (name == "p" && paragraphDepth > 0) {
      paragraphDepth--;
    }
  }

  void onCharacterData(const XML_Char* data, const int len) {
    if (!insideBody || paragraphDepth <= 0 || len <= 0) {
      return;
    }

    visibleChars += countUtf8Codepoints(data, len);
  }

 private:
  XML_Parser parser = nullptr;
  bool parseOk = true;
  bool insideBody = false;
  bool stopped = false;
  int depth = 0;
  int bodyDepth = -1;
  int paragraphDepth = 0;
  size_t visibleChars = 0;
};

class XPathParagraphResolver final : public Print {
 public:
  explicit XPathParagraphResolver(const int targetParagraph) : targetParagraph(targetParagraph) {
    parser = XML_ParserCreate(nullptr);
    if (!parser) {
      LOG_ERR("KOX", "Failed to create XML parser");
      return;
    }

    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, &XPathParagraphResolver::startElement, &XPathParagraphResolver::endElement);
  }

  ~XPathParagraphResolver() override { destroyXmlParser(parser); }

  bool ok() const { return parser != nullptr && parseOk; }

  bool finish() {
    if (!parser || !parseOk || stopped) {
      return parseOk;
    }

    if (XML_Parse(parser, "", 0, XML_TRUE) == XML_STATUS_ERROR) {
      LOG_ERR("KOX", "Final XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
      parseOk = false;
    }
    return parseOk;
  }

  bool hasMatch() const { return !xpath.empty(); }
  const std::string& getXPath() const { return xpath; }

  size_t write(uint8_t c) override { return write(&c, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!parser || !parseOk || stopped) {
      return size;
    }

    if (XML_Parse(parser, reinterpret_cast<const char*>(buffer), static_cast<int>(size), XML_FALSE) != XML_STATUS_OK) {
      const enum XML_Error error = XML_GetErrorCode(parser);
      if (error != XML_ERROR_ABORTED) {
        LOG_ERR("KOX", "XML parse error: %s", XML_ErrorString(error));
        parseOk = false;
      }
    }

    return size;
  }

  int spineIndex = 0;

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char**) {
    auto* self = static_cast<XPathParagraphResolver*>(userData);
    self->onStartElement(name);
  }

  static void XMLCALL endElement(void* userData, const XML_Char* name) {
    auto* self = static_cast<XPathParagraphResolver*>(userData);
    self->onEndElement(name);
  }

  void onStartElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    if (!insideBody) {
      if (name == "body") {
        insideBody = true;
        bodyDepth = depth;
        parentStates.emplace_back();
      }
      depth++;
      return;
    }

    const int siblingIndex = parentStates.back().nextIndex(name);
    path.push_back({name, siblingIndex});
    parentStates.emplace_back();

    if (name == "p") {
      paragraphCount++;
      if (paragraphCount == targetParagraph) {
        xpath = buildParagraphXPath(spineIndex, path, 0, 0);
        stopped = true;
        XML_StopParser(parser, XML_FALSE);
      }
    }

    depth++;
  }

  void onEndElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    depth--;
    if (!insideBody) {
      return;
    }

    if (depth == bodyDepth && name == "body") {
      insideBody = false;
      parentStates.clear();
      path.clear();
      return;
    }

    if (!path.empty()) {
      path.pop_back();
    }
    if (!parentStates.empty()) {
      parentStates.pop_back();
    }
  }

  XML_Parser parser = nullptr;
  const int targetParagraph;
  bool parseOk = true;
  bool insideBody = false;
  bool stopped = false;
  int depth = 0;
  int bodyDepth = -1;
  int paragraphCount = 0;
  std::vector<ParentState> parentStates;
  std::vector<PathSegment> path;
  std::string xpath;
};

class XPathProgressResolver final : public Print {
 public:
  explicit XPathProgressResolver(const size_t targetVisibleChar) : targetVisibleChar(targetVisibleChar) {
    parser = XML_ParserCreate(nullptr);
    if (!parser) {
      LOG_ERR("KOX", "Failed to create XML parser");
      return;
    }

    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, &XPathProgressResolver::startElement, &XPathProgressResolver::endElement);
    XML_SetCharacterDataHandler(parser, &XPathProgressResolver::characterData);
  }

  ~XPathProgressResolver() override { destroyXmlParser(parser); }

  bool ok() const { return parser != nullptr && parseOk; }

  bool finish() {
    if (!parser || !parseOk || stopped) {
      return parseOk;
    }

    if (XML_Parse(parser, "", 0, XML_TRUE) == XML_STATUS_ERROR) {
      LOG_ERR("KOX", "Final XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
      parseOk = false;
    }
    return parseOk;
  }

  bool hasMatch() const { return !xpath.empty(); }
  const std::string& getXPath() const { return xpath; }

  size_t write(uint8_t c) override { return write(&c, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!parser || !parseOk || stopped) {
      return size;
    }

    if (XML_Parse(parser, reinterpret_cast<const char*>(buffer), static_cast<int>(size), XML_FALSE) != XML_STATUS_OK) {
      const enum XML_Error error = XML_GetErrorCode(parser);
      if (error != XML_ERROR_ABORTED) {
        LOG_ERR("KOX", "XML parse error: %s", XML_ErrorString(error));
        parseOk = false;
      }
    }

    return size;
  }

  int spineIndex = 0;

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char**) {
    auto* self = static_cast<XPathProgressResolver*>(userData);
    self->onStartElement(name);
  }

  static void XMLCALL endElement(void* userData, const XML_Char* name) {
    auto* self = static_cast<XPathProgressResolver*>(userData);
    self->onEndElement(name);
  }

  static void XMLCALL characterData(void* userData, const XML_Char* data, const int len) {
    auto* self = static_cast<XPathProgressResolver*>(userData);
    self->onCharacterData(data, len);
  }

  void onStartElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    if (!insideBody) {
      if (name == "body") {
        insideBody = true;
        bodyDepth = depth;
        parentStates.emplace_back();
      }
      depth++;
      return;
    }

    const int siblingIndex = parentStates.back().nextIndex(name);
    path.push_back({name, siblingIndex});
    parentStates.emplace_back();
    textNodeIndexStack.push_back(0);
    pendingTextNode = true;

    if (name == "p") {
      paragraphDepth++;
    }
    if (name == "li") {
      liDepth++;
    }

    depth++;
  }

  void onEndElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    depth--;
    if (!insideBody) {
      return;
    }

    if (depth == bodyDepth && name == "body") {
      insideBody = false;
      parentStates.clear();
      path.clear();
      textNodeIndexStack.clear();
      return;
    }

    if (name == "p" && paragraphDepth > 0) {
      paragraphDepth--;
    }
    if (name == "li" && liDepth > 0) {
      liDepth--;
    }

    if (!textNodeIndexStack.empty()) {
      textNodeIndexStack.pop_back();
    }
    if (paragraphDepth > 0 || liDepth > 0) {
      pendingTextNode = true;
    }
    if (!path.empty()) {
      path.pop_back();
    }
    if (!parentStates.empty()) {
      parentStates.pop_back();
    }
  }

  void onCharacterData(const XML_Char* data, const int len) {
    if (!insideBody || (paragraphDepth <= 0 && liDepth <= 0) || len <= 0 || stopped) {
      return;
    }

    const size_t codepointCount = countUtf8Codepoints(data, len);
    if (codepointCount == 0) {
      return;
    }

    // Start a new text node on first non-empty content after any element boundary.
    // Only counting non-empty nodes matches KOReader's text()[N] indexing behavior,
    // which skips empty text nodes created by bare <a id="anchor"/> anchors.
    if (pendingTextNode) {
      if (!textNodeIndexStack.empty()) {
        textNodeIndexStack.back()++;
      }
      textNodeStartChars = visibleChars;
      pendingTextNode = false;
    }

    const size_t nextVisibleChars = visibleChars + codepointCount;
    if (targetVisibleChar <= nextVisibleChars) {
      const size_t delta = targetVisibleChar - visibleChars;
      const int texNode = textNodeIndexStack.empty() ? 0 : textNodeIndexStack.back();
      const size_t charOff = visibleChars - textNodeStartChars + delta;
      xpath = buildParagraphXPath(spineIndex, path, texNode, charOff);
      stopped = true;
      XML_StopParser(parser, XML_FALSE);
      return;
    }

    visibleChars = nextVisibleChars;
  }

  XML_Parser parser = nullptr;
  const size_t targetVisibleChar;
  bool parseOk = true;
  bool insideBody = false;
  bool stopped = false;
  bool pendingTextNode = true;
  int depth = 0;
  int bodyDepth = -1;
  int paragraphDepth = 0;
  int liDepth = 0;
  size_t visibleChars = 0;
  size_t textNodeStartChars = 0;
  std::vector<int> textNodeIndexStack;
  std::vector<ParentState> parentStates;
  std::vector<PathSegment> path;
  std::string xpath;
};

class XPathHighlightTextResolver final : public Print {
 public:
  explicit XPathHighlightTextResolver(const uint16_t targetParagraph, const std::string& targetText)
      : targetParagraph(targetParagraph), targetText(targetText) {
    parser = XML_ParserCreate(nullptr);
    if (!parser) {
      LOG_ERR("KOX", "Failed to create XML parser");
      return;
    }

    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, &XPathHighlightTextResolver::startElement, &XPathHighlightTextResolver::endElement);
    XML_SetCharacterDataHandler(parser, &XPathHighlightTextResolver::characterData);
    paragraphText.reserve(std::min<size_t>(targetText.size() + 256, MAX_PARAGRAPH_TEXT));
    segments.reserve(8);
  }

  ~XPathHighlightTextResolver() override { destroyXmlParser(parser); }

  bool ok() const { return parser != nullptr && parseOk; }

  bool finish() {
    if (!parser || !parseOk || stopped) {
      return parseOk;
    }

    if (XML_Parse(parser, "", 0, XML_TRUE) == XML_STATUS_ERROR) {
      LOG_ERR("KOX", "Final XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
      parseOk = false;
    }
    return parseOk;
  }

  bool hasMatch() const { return !pos0.empty() && !pos1.empty() && pos0 != pos1; }
  const std::string& startXPath() const { return pos0; }
  const std::string& endXPath() const { return pos1; }

  size_t write(uint8_t c) override { return write(&c, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!parser || !parseOk || stopped) {
      return size;
    }

    if (XML_Parse(parser, reinterpret_cast<const char*>(buffer), static_cast<int>(size), XML_FALSE) != XML_STATUS_OK) {
      const enum XML_Error error = XML_GetErrorCode(parser);
      if (error != XML_ERROR_ABORTED) {
        LOG_ERR("KOX", "XML parse error: %s", XML_ErrorString(error));
        parseOk = false;
      }
    }

    return size;
  }

  int spineIndex = 0;

 private:
  static constexpr size_t MAX_PARAGRAPH_TEXT = 4096;

  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char**) {
    auto* self = static_cast<XPathHighlightTextResolver*>(userData);
    self->onStartElement(name);
  }

  static void XMLCALL endElement(void* userData, const XML_Char* name) {
    auto* self = static_cast<XPathHighlightTextResolver*>(userData);
    self->onEndElement(name);
  }

  static void XMLCALL characterData(void* userData, const XML_Char* data, const int len) {
    auto* self = static_cast<XPathHighlightTextResolver*>(userData);
    self->onCharacterData(data, len);
  }

  void onStartElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    if (!insideBody) {
      if (name == "body") {
        insideBody = true;
        bodyDepth = depth;
        parentStates.emplace_back();
      }
      depth++;
      return;
    }

    const int siblingIndex = parentStates.back().nextIndex(name);
    path.push_back({name, siblingIndex});
    parentStates.emplace_back();
    textNodeIndexStack.push_back(0);
    pendingTextNode = true;

    if (name == "p") {
      paragraphCount++;
      if (targetParagraph == 0 || paragraphCount == targetParagraph) {
        resetParagraphCapture();
        insideTargetParagraph = true;
        targetParagraphDepth = depth;
      }
    }

    depth++;
  }

  void onEndElement(const XML_Char* rawName) {
    const std::string name = stripPrefix(rawName);

    depth--;
    if (!insideBody) {
      return;
    }

    if (insideTargetParagraph && depth == targetParagraphDepth && name == "p") {
      resolveRange();
      insideTargetParagraph = false;
      if (hasMatch()) {
        stopped = true;
        XML_StopParser(parser, XML_FALSE);
      }
    }

    if (depth == bodyDepth && name == "body") {
      insideBody = false;
      parentStates.clear();
      path.clear();
      textNodeIndexStack.clear();
      return;
    }

    if (!textNodeIndexStack.empty()) {
      textNodeIndexStack.pop_back();
    }
    pendingTextNode = true;
    if (!path.empty()) {
      path.pop_back();
    }
    if (!parentStates.empty()) {
      parentStates.pop_back();
    }
  }

  void onCharacterData(const XML_Char* data, const int len) {
    if (!insideTargetParagraph || len <= 0 || stopped || paragraphText.size() >= MAX_PARAGRAPH_TEXT) {
      return;
    }

    if (pendingTextNode) {
      if (!textNodeIndexStack.empty()) {
        textNodeIndexStack.back()++;
      }
      pendingTextNode = false;
    }

    const size_t remaining = MAX_PARAGRAPH_TEXT - paragraphText.size();
    const size_t appendBytes = std::min(remaining, static_cast<size_t>(len));
    const size_t charCount = countUtf8Codepoints(reinterpret_cast<const XML_Char*>(data), static_cast<int>(appendBytes));
    if (charCount == 0) {
      return;
    }

    const int textNodeIndex = textNodeIndexStack.empty() ? 1 : textNodeIndexStack.back();
    segments.push_back({path, textNodeIndex, paragraphChars, charCount});
    paragraphText.append(data, appendBytes);
    paragraphChars += charCount;
  }

  void resolveRange() {
    size_t startChar = 0;
    size_t endChar = 0;
    if (!locateTextRange(paragraphText, targetText, startChar, endChar)) {
      return;
    }

    const TextSegment* startSegment = findSegmentForOffset(segments, startChar, false);
    const TextSegment* endSegment = findSegmentForOffset(segments, endChar, true);
    if (!startSegment || !endSegment) {
      return;
    }

    pos0 = buildTextXPath(spineIndex, startSegment->path, startSegment->textNodeIndex,
                          startChar - startSegment->startChar);
    pos1 = buildTextXPath(spineIndex, endSegment->path, endSegment->textNodeIndex, endChar - endSegment->startChar);
  }

  void resetParagraphCapture() {
    paragraphChars = 0;
    paragraphText.clear();
    segments.clear();
    pendingTextNode = true;
  }

  XML_Parser parser = nullptr;
  const uint16_t targetParagraph;
  const std::string& targetText;
  bool parseOk = true;
  bool insideBody = false;
  bool insideTargetParagraph = false;
  bool stopped = false;
  bool pendingTextNode = true;
  int depth = 0;
  int bodyDepth = -1;
  int targetParagraphDepth = -1;
  uint16_t paragraphCount = 0;
  size_t paragraphChars = 0;
  std::vector<int> textNodeIndexStack;
  std::vector<ParentState> parentStates;
  std::vector<PathSegment> path;
  std::vector<TextSegment> segments;
  std::string paragraphText;
  std::string pos0;
  std::string pos1;
};
}  // namespace

std::string ChapterXPathResolver::findXPathForParagraph(const std::shared_ptr<Epub>& epub, const int spineIndex,
                                                        const uint16_t paragraphIndex) {
  if (!epub || paragraphIndex == 0 || spineIndex < 0 || spineIndex >= epub->getSpineItemsCount()) {
    return "";
  }

  const auto href = epub->getSpineItem(spineIndex).href;
  if (href.empty()) {
    return "";
  }

  XPathParagraphResolver resolver(paragraphIndex);
  if (!resolver.ok()) {
    return "";
  }

  resolver.spineIndex = spineIndex;
  if (!epub->readItemContentsToStream(href, resolver, 1024) || !resolver.finish()) {
    return "";
  }

  if (resolver.hasMatch()) {
    LOG_DBG("KOX", "Resolved paragraph %u in spine %d -> %s", paragraphIndex, spineIndex, resolver.getXPath().c_str());
    return resolver.getXPath();
  }

  LOG_DBG("KOX", "Paragraph %u not found in spine %d", paragraphIndex, spineIndex);
  return "";
}

std::string ChapterXPathResolver::findXPathForProgress(const std::shared_ptr<Epub>& epub, const int spineIndex,
                                                       const float intraSpineProgress) {
  if (!epub || spineIndex < 0 || spineIndex >= epub->getSpineItemsCount()) {
    return "";
  }

  const auto href = epub->getSpineItem(spineIndex).href;
  if (href.empty()) {
    return "";
  }

  if (!(intraSpineProgress > 0.0f)) {
    return "/body/DocFragment[" + std::to_string(spineIndex + 1) + "]/body";
  }

  ParagraphTextCounter counter;
  if (!counter.ok() || !epub->readItemContentsToStream(href, counter, 1024) || !counter.finish()) {
    return "";
  }

  const size_t totalVisibleChars = counter.totalVisibleChars();
  if (totalVisibleChars == 0) {
    return "";
  }

  const float clamped = std::max(0.0f, std::min(1.0f, intraSpineProgress));
  const size_t targetVisibleChar =
      std::max<size_t>(1, std::min(totalVisibleChars, static_cast<size_t>(std::ceil(clamped * totalVisibleChars))));

  XPathProgressResolver resolver(targetVisibleChar);
  if (!resolver.ok()) {
    return "";
  }

  resolver.spineIndex = spineIndex;
  if (!epub->readItemContentsToStream(href, resolver, 1024) || !resolver.finish()) {
    return "";
  }

  if (resolver.hasMatch()) {
    LOG_DBG("KOX", "Resolved progress %.3f in spine %d -> %s", intraSpineProgress, spineIndex,
            resolver.getXPath().c_str());
    return resolver.getXPath();
  }

  LOG_DBG("KOX", "Could not resolve progress %.3f in spine %d", intraSpineProgress, spineIndex);
  return "";
}

bool ChapterXPathResolver::findXPathRangeForText(const std::shared_ptr<Epub>& epub, const int spineIndex,
                                                 const uint16_t paragraphIndex, const std::string& text,
                                                 std::string& pos0, std::string& pos1) {
  pos0.clear();
  pos1.clear();
  if (!epub || spineIndex < 0 || spineIndex >= epub->getSpineItemsCount() || text.empty()) {
    return false;
  }

  const auto href = epub->getSpineItem(spineIndex).href;
  if (href.empty()) {
    return false;
  }

  const auto resolveWithParagraph = [&](const uint16_t targetParagraph, const char* scope) {
    XPathHighlightTextResolver resolver(targetParagraph, text);
    if (!resolver.ok()) {
      return false;
    }

    resolver.spineIndex = spineIndex;
    if (!epub->readItemContentsToStream(href, resolver, 1024) || !resolver.finish() || !resolver.hasMatch()) {
      LOG_DBG("KOX", "Could not resolve highlight text in spine=%d %s=%u", spineIndex, scope, targetParagraph);
      return false;
    }

    pos0 = resolver.startXPath();
    pos1 = resolver.endXPath();
    LOG_DBG("KOX", "Resolved highlight text in spine=%d %s=%u -> %s .. %s", spineIndex, scope, targetParagraph,
            pos0.c_str(), pos1.c_str());
    return true;
  };

  if (paragraphIndex != UINT16_MAX && paragraphIndex > 0 && resolveWithParagraph(paragraphIndex, "paragraph")) {
    return true;
  }

  if (!resolveWithParagraph(0, "any_paragraph")) {
    return false;
  }
  return true;
}
