#pragma once

#include <Epub.h>

#include <cstdint>
#include <memory>
#include <string>

class ChapterXPathResolver {
 public:
  /**
   * Resolve the Nth paragraph in a spine item to its real XHTML ancestry path.
   *
   * Returns a KOReader-compatible path like:
   * /body/DocFragment[8]/body/div[2]/section[1]/p[4]
   *
   * An empty string means parsing failed or the paragraph index was not found.
   */
  static std::string findXPathForParagraph(const std::shared_ptr<Epub>& epub, int spineIndex, uint16_t paragraphIndex);

  /**
   * Resolve intra-spine progress to a real XHTML ancestry path plus text offset.
   *
   * Returns a KOReader-compatible path like:
   * /body/DocFragment[8]/body/div[2]/section[1]/p[4]/text().96
   *
   * An empty string means parsing failed or the location could not be resolved.
   */
  static std::string findXPathForProgress(const std::shared_ptr<Epub>& epub, int spineIndex, float intraSpineProgress);

  /**
   * Resolve highlighted text inside a paragraph to a KOReader-compatible xpointer range.
   *
   * Returns true when the text was found in the target paragraph and fills pos0/pos1
   * with text-node offsets, for example:
   * /body/DocFragment[8]/body/div[2]/p[4]/text().12
   */
  static bool findXPathRangeForText(const std::shared_ptr<Epub>& epub, int spineIndex, uint16_t paragraphIndex,
                                    const std::string& text, std::string& pos0, std::string& pos1);
};
