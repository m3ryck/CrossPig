#pragma once

#include "components/CustomThemeRegistry.h"
#include "components/themes/BaseTheme.h"

// Safe v2 renderer: layouts are data, never executable code. It deliberately
// owns no framebuffer and opens at most one BMP while drawing.
class DeclarativeTheme final : public BaseTheme {
 public:
  explicit DeclarativeTheme(const CustomThemeInfo& info) : info_(info) {}

  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& books, int selected,
                           bool& rendered, bool& stored, bool& restored,
                           const std::function<bool()>& store,
                           const BookReadingStats* stats = nullptr, float progress = -1.0f,
                           const GlobalReadingStats* global = nullptr, const char* chapter = nullptr) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int count, int selected,
                      const std::function<const char*(int)>& label,
                      const std::function<UIIcon(int)>& icon) const override;

 private:
  Rect normalized(Rect parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height) const;
  void drawBackground(GfxRenderer& renderer, Rect rect) const;
  CustomThemeInfo info_;
};
