#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class CrossSyncMatchType : uint8_t {
  Filename = 0,
  Binary = 1,
};

struct CrossSyncHighlight {
  uint16_t spine = 0;
  uint16_t startPage = 0;
  uint16_t endPage = 0;
  uint16_t pageCount = 1;
  uint16_t startWord = 0;
  uint16_t endWord = 0;
  uint16_t wordCount = 0;
  uint16_t paragraph = UINT16_MAX;
  uint32_t createdAt = 0;
  std::string chapter;
  std::string text;
  std::string pos0;
  std::string pos1;
};

struct CrossSyncHighlightDocument {
  std::string book;
  CrossSyncMatchType matchType = CrossSyncMatchType::Filename;
  std::string source = "crossink";
  std::vector<CrossSyncHighlight> highlights;
};

enum class CrossSyncDirection : uint8_t {
  PullRemote,
  UploadLocal,
};
