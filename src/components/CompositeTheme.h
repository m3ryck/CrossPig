#pragma once

#include "components/CustomThemeRegistry.h"
#include "components/themes/BaseTheme.h"
#include "components/themes/dashboard/DashboardTheme.h"
#include "components/themes/lyra/Lyra3CoversTheme.h"
#include "components/themes/lyra/LyraCarouselTheme.h"
#include "components/themes/lyra/LyraTheme.h"
#include "components/themes/minimal/MinimalTheme.h"
#include "components/themes/roundedraff/RoundedRaffTheme.h"

// Stateless built-in renderers can be composed safely. The selected theme owns
// no framebuffer and delegates every drawing call to an existing implementation.
class CompositeTheme final : public BaseTheme {
 public:
  explicit CompositeTheme(const CustomThemeInfo& info) : info_(info) {}

  void fillBatteryIcon(const GfxRenderer& r, Rect rect, uint16_t pct, bool black = true) const override;
  void drawButtonHints(GfxRenderer& r, const char* a, const char* b, const char* c, const char* d,
                       bool inverted = false) const override;
  void drawSideButtonHints(const GfxRenderer& r, const char* top, const char* bottom) const override;
  void drawList(const GfxRenderer& r, Rect rect, int count, int selected, const std::function<std::string(int)>& title,
                const std::function<std::string(int)>& subtitle = nullptr,
                const std::function<UIIcon(int)>& icon = nullptr,
                const std::function<std::string(int)>& value = nullptr, bool highlight = false,
                const std::function<bool(int)>& dimmed = nullptr,
                const std::function<bool(int)>& header = nullptr) const override;
  void drawHeader(const GfxRenderer& r, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool reader = false) const override;
  void drawSubHeader(const GfxRenderer& r, Rect rect, const char* label, const char* right = nullptr) const override;
  void drawTabBar(const GfxRenderer& r, Rect rect, const std::vector<TabInfo>& tabs, bool selected) const override;
  void drawRecentBookCover(GfxRenderer& r, Rect rect, const std::vector<RecentBook>& books, int selected,
                           bool& rendered, bool& stored, bool& restored, const std::function<bool()>& store,
                           const BookReadingStats* stats = nullptr, float progress = -1.0f,
                           const GlobalReadingStats* global = nullptr, const char* chapter = nullptr) const override;
  void drawButtonMenu(GfxRenderer& r, Rect rect, int count, int selected,
                      const std::function<const char*(int)>& label,
                      const std::function<UIIcon(int)>& icon) const override;
  Rect drawPopup(const GfxRenderer& r, const char* message) const override;
  void drawOptionPopup(const GfxRenderer& r, const char* title, const std::vector<std::string>& options,
                       int selected) const override;
  void fillPopupProgress(const GfxRenderer& r, const Rect& layout, int progress) const override;
  void drawStatusBar(GfxRenderer& r, float bookProgress, int currentPage, int pageCount, std::string title,
                     int paddingBottom = 0, int textYOffset = 0, bool bookmarked = false, const char* timeLeft = nullptr,
                     bool dark = false, float chapterProgress = -1.0f, int stableCurrent = 0, int stableCount = 0,
                     bool showProgress = true) const override;
  void drawTopStatusBarClock(const GfxRenderer& r, int topY = -1, const char* preview = nullptr,
                             bool reader = true, int yOffset = 0, bool dark = false) const override;
  void drawHelpText(const GfxRenderer& r, Rect rect, const char* label) const override;
  void drawTextField(const GfxRenderer& r, Rect rect, int textWidth, bool cursor = false, int start = 0,
                     int width = 0) const override;
  void drawKeyboardKey(const GfxRenderer& r, Rect rect, const char* label, bool selected,
                       const char* secondary = nullptr, KeyboardKeyType type = KeyboardKeyType::Normal,
                       bool inactive = false) const override;
  bool showsFileIcons() const override;
  bool usesCompactFileBrowserRows() const override;
  int compactFileBrowserRowHeight(const GfxRenderer& r) const override;

 private:
  const BaseTheme& source(uint8_t type) const;
  CustomThemeInfo info_;
  BaseTheme classic_;
  LyraTheme lyra_;
  Lyra3CoversTheme lyraExtended_;
  LyraCarouselTheme carousel_;
  MinimalTheme minimal_;
  DashboardTheme dashboard_;
  RoundedRaffTheme roundedRaff_;
};
