#pragma once

#include <string>

#include "CrossSyncTypes.h"

namespace CrossSyncHighlightJson {

const char* matchTypeToString(CrossSyncMatchType matchType);
bool matchTypeFromString(const char* value, CrossSyncMatchType& out);

bool serialize(const CrossSyncHighlightDocument& document, std::string& outJson);
bool parse(const char* json, CrossSyncHighlightDocument& outDocument);

}  // namespace CrossSyncHighlightJson
