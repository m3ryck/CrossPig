#pragma once

#include <cstddef>

class ThemeInstaller {
 public:
  static constexpr size_t kMaxPackageBytes = 2U * 1024U * 1024U;
  static bool isSafeThemeId(const char* id);
  // Clears and creates the reserved upload directory and its assets folder.
  static bool prepareStaging(const char* id, char* error, size_t errorSize);
  static bool validateStaging(const char* id, char* error, size_t errorSize);
  static bool publish(const char* id, bool replace, char* error, size_t errorSize);
  static bool remove(const char* id, char* error, size_t errorSize);
};
