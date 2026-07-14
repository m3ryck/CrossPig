#pragma once

#include <cstdint>
#include <string>

class CrossSyncWebDavClient {
 public:
  enum class Error : uint8_t {
    OK,
    NotConfigured,
    NotFound,
    NetworkError,
    AuthFailed,
    ServerError,
    LowMemory,
  };

  static Error ensureCollectionPath(const std::string& path);
  static Error getTextFile(const std::string& remotePath, std::string& out);
  static Error putTextFile(const std::string& remotePath, const std::string& body);
  static const char* errorString(Error error);
  static std::string lastErrorDetail();

  static int lastHttpCode;
  static int lastTransportError;
};
