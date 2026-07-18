#include "CompositeTheme.h"

#include <utility>

#include "CrossPointSettings.h"

const BaseTheme& CompositeTheme::source(uint8_t type) const {
  switch (static_cast<CrossPointSettings::UI_THEME>(type)) {
    case CrossPointSettings::LYRA: return lyra_;
    case CrossPointSettings::LYRA_3_COVERS: return lyraExtended_;
    case CrossPointSettings::LYRA_CAROUSEL: return carousel_;
    case CrossPointSettings::MINIMAL: return minimal_;
    case CrossPointSettings::DASHBOARD: return dashboard_;
    case CrossPointSettings::ROUNDEDRAFF: return roundedRaff_;
    case CrossPointSettings::CLASSIC:
    default: return classic_;
  }
}

void CompositeTheme::fillBatteryIcon(const GfxRenderer& r, Rect x, uint16_t p, bool b) const { source(info_.headerTheme).fillBatteryIcon(r, x, p, b); }
void CompositeTheme::drawButtonHints(GfxRenderer& r, const char* a, const char* b, const char* c, const char* d, bool i) const { source(info_.hintsTheme).drawButtonHints(r, a, b, c, d, i); }
void CompositeTheme::drawSideButtonHints(const GfxRenderer& r, const char* a, const char* b) const { source(info_.hintsTheme).drawSideButtonHints(r, a, b); }
void CompositeTheme::drawList(const GfxRenderer& r, Rect x, int n, int s, const std::function<std::string(int)>& a, const std::function<std::string(int)>& b, const std::function<UIIcon(int)>& c, const std::function<std::string(int)>& d, bool e, const std::function<bool(int)>& f, const std::function<bool(int)>& g) const { source(info_.listTheme).drawList(r, x, n, s, a, b, c, d, e, f, g); }
void CompositeTheme::drawHeader(const GfxRenderer& r, Rect x, const char* a, const char* b, bool c) const { source(info_.headerTheme).drawHeader(r, x, a, b, c); }
void CompositeTheme::drawSubHeader(const GfxRenderer& r, Rect x, const char* a, const char* b) const { source(info_.headerTheme).drawSubHeader(r, x, a, b); }
void CompositeTheme::drawTabBar(const GfxRenderer& r, Rect x, const std::vector<TabInfo>& a, bool b) const { source(info_.headerTheme).drawTabBar(r, x, a, b); }
void CompositeTheme::drawRecentBookCover(GfxRenderer& r, Rect x, const std::vector<RecentBook>& a, int b, bool& c, bool& d, bool& e, const std::function<bool()>& f, const BookReadingStats* g, float h, const GlobalReadingStats* i, const char* j) const { source(info_.baseTheme).drawRecentBookCover(r, x, a, b, c, d, e, f, g, h, i, j); }
void CompositeTheme::drawButtonMenu(GfxRenderer& r, Rect x, int a, int b, const std::function<const char*(int)>& c, const std::function<UIIcon(int)>& d) const { source(info_.menuTheme).drawButtonMenu(r, x, a, b, c, d); }
Rect CompositeTheme::drawPopup(const GfxRenderer& r, const char* a) const { return source(info_.popupTheme).drawPopup(r, a); }
void CompositeTheme::drawOptionPopup(const GfxRenderer& r, const char* a, const std::vector<std::string>& b, int c) const { source(info_.popupTheme).drawOptionPopup(r, a, b, c); }
void CompositeTheme::fillPopupProgress(const GfxRenderer& r, const Rect& a, int b) const { source(info_.popupTheme).fillPopupProgress(r, a, b); }
void CompositeTheme::drawStatusBar(GfxRenderer& r, float a, int b, int c, std::string d, int e, int f, bool g, const char* h, bool i, float j, int k, int l, bool m) const { source(info_.statusTheme).drawStatusBar(r, a, b, c, std::move(d), e, f, g, h, i, j, k, l, m); }
void CompositeTheme::drawTopStatusBarClock(const GfxRenderer& r, int a, const char* b, bool c, int d, bool e) const { source(info_.statusTheme).drawTopStatusBarClock(r, a, b, c, d, e); }
void CompositeTheme::drawHelpText(const GfxRenderer& r, Rect a, const char* b) const { source(info_.statusTheme).drawHelpText(r, a, b); }
void CompositeTheme::drawTextField(const GfxRenderer& r, Rect a, int b, bool c, int d, int e) const { source(info_.inputTheme).drawTextField(r, a, b, c, d, e); }
void CompositeTheme::drawKeyboardKey(const GfxRenderer& r, Rect a, const char* b, bool c, const char* d, KeyboardKeyType e, bool f) const { source(info_.inputTheme).drawKeyboardKey(r, a, b, c, d, e, f); }
bool CompositeTheme::showsFileIcons() const { return source(info_.listTheme).showsFileIcons(); }
bool CompositeTheme::usesCompactFileBrowserRows() const { return source(info_.listTheme).usesCompactFileBrowserRows(); }
int CompositeTheme::compactFileBrowserRowHeight(const GfxRenderer& r) const { return source(info_.listTheme).compactFileBrowserRowHeight(r); }
