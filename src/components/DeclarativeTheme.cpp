#include "DeclarativeTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

Rect DeclarativeTheme::normalized(const Rect parent, const uint16_t x, const uint16_t y, const uint16_t width,
                                  const uint16_t height) const {
  return Rect(parent.x + parent.width * x / 1000, parent.y + parent.height * y / 1000,
              std::max(1, parent.width * width / 1000), std::max(1, parent.height * height / 1000));
}

void DeclarativeTheme::drawBackground(GfxRenderer& renderer, const Rect rect) const {
  if (info_.homeBackground[0] == '\0') return;
  FsFile file;
  if (!Storage.openFileForRead("THEME", info_.homeBackground, file)) return;
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() == BmpReaderError::Ok && bitmap.is1Bit()) {
    renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
  }
  file.close();
}

void DeclarativeTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect,
                                           const std::vector<RecentBook>& books, const int selected, bool& rendered,
                                           bool& stored, bool& restored, const std::function<bool()>& store,
                                           const BookReadingStats*, float, const GlobalReadingStats*, const char*) const {
  (void)restored;
  drawBackground(renderer, rect);
  if (books.empty()) return;

  Rect cover = normalized(rect, info_.coverX, info_.coverY, info_.coverWidth, info_.coverHeight);
  if (cover.width > cover.height * 3 / 2) {
    cover.width = cover.height * 3 / 5;
    cover.x = rect.x + (rect.width - cover.width) / 2;
  }
  const RecentBook& book = books[0];
  bool hasCover = false;
  const std::string path = UITheme::getCoverThumbPath(book.coverBmpPath, cover.height);
  if (!path.empty()) {
    FsFile file;
    if (Storage.openFileForRead("THEME", path, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        renderer.drawBitmap(bitmap, cover.x, cover.y, cover.width, cover.height);
        hasCover = true;
      }
      file.close();
    }
  }
  if (!hasCover) renderer.fillRect(cover.x, cover.y, cover.width, cover.height, false);
  if (info_.coverCornerRadius > 0) {
    renderer.drawRoundedRect(cover.x, cover.y, cover.width, cover.height, 1, info_.coverCornerRadius, true);
  } else {
    renderer.drawRect(cover.x, cover.y, cover.width, cover.height);
  }
  if (selected == 0) renderer.drawRect(cover.x + 2, cover.y + 2, cover.width - 4, cover.height - 4);
  if (!book.title.empty()) {
    const int textY = std::min(rect.y + rect.height - renderer.getLineHeight(UI_10_FONT_ID), cover.y + cover.height + 4);
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, book.title.c_str(), rect.width - 20);
    renderer.drawCenteredText(UI_10_FONT_ID, textY, title.c_str());
  }
  if (!rendered) {
    stored = store();
    rendered = stored;
  }
}

void DeclarativeTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int count, const int selected,
                                      const std::function<const char*(int)>& label,
                                      const std::function<UIIcon(int)>&) const {
  if (count <= 0) return;
  const int columns = std::max(1, static_cast<int>(info_.menuColumns));
  const int rows = (count + columns - 1) / columns;
  const int gap = info_.menuGap;
  const int rowHeight = info_.menuRowHeight > 0 ? info_.menuRowHeight : std::max(1, (rect.height - gap * (rows - 1)) / rows);
  const int cellWidth = std::max(1, (rect.width - gap * (columns - 1)) / columns);
  for (int i = 0; i < count; ++i) {
    const int col = i % columns;
    const int row = i / columns;
    const Rect cell{rect.x + col * (cellWidth + gap), rect.y + row * (rowHeight + gap), cellWidth, rowHeight};
    const bool isSelected = i == selected;
    if (isSelected) renderer.fillRect(cell.x, cell.y, cell.width, cell.height, true);
    renderer.drawRoundedRect(cell.x, cell.y, cell.width, cell.height, 1, 8, !isSelected);
    const char* text = label(i);
    const std::string visible = renderer.truncatedText(UI_10_FONT_ID, text ? text : "", cell.width - 12);
    const int textW = renderer.getTextWidth(UI_10_FONT_ID, visible.c_str());
    renderer.drawText(UI_10_FONT_ID, cell.x + (cell.width - textW) / 2,
                      cell.y + (cell.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, visible.c_str(), !isSelected);
  }
}
