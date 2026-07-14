#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <Epub.h>
#include <KOReaderCredentialStore.h>

#include "CrossSyncTypes.h"

class CrossSyncHighlightService {
 public:
  enum class Result : uint8_t {
    Skipped,
    OK,
    NotConfigured,
    RemoteMissingAppliedAsEmpty,
    ParseError,
    NetworkError,
    LocalStoreError,
  };

  static Result sync(CrossSyncDirection direction, const std::string& documentHash, DocumentMatchMethod matchMethod,
                     const std::shared_ptr<Epub>& epub, int currentSpineIndex, int totalPagesInCurrentSpine);
  static const char* resultString(Result result);
  static std::string resultDetail(Result result);
};
