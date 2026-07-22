#include "DeclarativeTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kContentPadding = 10;
constexpr int kMetadataGap = 4;
constexpr int kProgressHeight = 5;
constexpr int kModuleHeight = 48;
constexpr int kModuleGap = 6;
constexpr size_t kTextCapacity = 96;

void fitText(const GfxRenderer& renderer, const int fontId, const char* source, const int maxWidth, char* out,
             const size_t outSize, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (!out || outSize == 0) return;
  std::snprintf(out, outSize, "%s", source ? source : "");
  size_t length = std::strlen(out);
  while (length > 3 && renderer.getTextWidth(fontId, out, style) > maxWidth) {
    length = static_cast<size_t>(utf8SafeTruncateBuffer(out, static_cast<int>(length - 1)));
    out[length] = '\0';
  }
  const bool truncated = source && std::strlen(source) > length;
  while (truncated && length > 3 && length + 3 < outSize &&
         renderer.getTextWidth(fontId, out, style) + renderer.getTextWidth(fontId, "...", style) > maxWidth) {
    length = static_cast<size_t>(utf8SafeTruncateBuffer(out, static_cast<int>(length - 1)));
    out[length] = '\0';
  }
  if (truncated && length + 3 < outSize) {
    std::memcpy(out + length, "...", 4);
  }
}

void drawCover(const GfxRenderer& renderer, const RecentBook& book, const Rect target, const int radius,
               const int thumbHeight) {
  if (target.width <= 0 || target.height <= 0) return;
  renderer.fillRect(target.x, target.y, target.width, target.height, false);
  if (!book.coverBmpPath.empty()) {
    const std::string path = UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight);
    if (!path.empty()) {
      FsFile file;
      if (Storage.openFileForRead("THEME", path, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.drawBitmap(bitmap, target.x, target.y, target.width, target.height);
        }
        file.close();
      }
    }
  }
  if (radius > 0) {
    renderer.drawRoundedRect(target.x, target.y, target.width, target.height, 1, radius, true);
  } else {
    renderer.drawRect(target.x, target.y, target.width, target.height);
  }
}

void drawFittedText(const GfxRenderer& renderer, const int fontId, const int x, const int y, const int maxWidth,
                    const char* source, const bool bold = false) {
  char text[kTextCapacity];
  const EpdFontFamily::Style style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  fitText(renderer, fontId, source, maxWidth, text, sizeof(text), style);
  renderer.drawText(fontId, x, y, text, true, style);
}

void drawProgress(const GfxRenderer& renderer, const Rect rect, const float progressPercent) {
  if (rect.width <= 0 || rect.height <= 0) return;
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  if (progressPercent <= 0.0f) return;
  const int percent = std::clamp(static_cast<int>(progressPercent + 0.5f), 0, 100);
  const int fillWidth = std::max(0, (rect.width - 2) * percent / 100);
  if (fillWidth > 0) renderer.fillRect(rect.x + 1, rect.y + 1, fillWidth, std::max(1, rect.height - 2), true);
}

void drawStatCell(const GfxRenderer& renderer, const Rect rect, const char* value, const char* label,
                  const int radius) {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, radius, true);
  char visibleValue[kTextCapacity];
  char visibleLabel[kTextCapacity];
  fitText(renderer, UI_10_FONT_ID, value, rect.width - 10, visibleValue, sizeof(visibleValue), EpdFontFamily::BOLD);
  fitText(renderer, SMALL_FONT_ID, label, rect.width - 10, visibleLabel, sizeof(visibleLabel));
  const int valueWidth = renderer.getTextWidth(UI_10_FONT_ID, visibleValue, EpdFontFamily::BOLD);
  const int labelWidth = renderer.getTextWidth(SMALL_FONT_ID, visibleLabel);
  renderer.drawText(UI_10_FONT_ID, rect.x + (rect.width - valueWidth) / 2, rect.y + 5, visibleValue, true,
                    EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, rect.x + (rect.width - labelWidth) / 2,
                    rect.y + rect.height - renderer.getLineHeight(SMALL_FONT_ID) - 4, visibleLabel);
}

void drawBookStats(const GfxRenderer& renderer, const Rect rect, const BookReadingStats* stats,
                   const float progressPercent, const int gap, const int radius) {
  const BookReadingStats empty{};
  const BookReadingStats& value = stats ? *stats : empty;
  const int cellWidth = std::max(1, (rect.width - gap * 2) / 3);
  char readingTime[32];
  char progress[16];
  char pages[16];
  BookReadingStats::formatDuration(value.totalReadingSeconds, readingTime, sizeof(readingTime));
  if (progressPercent >= 0.0f) {
    std::snprintf(progress, sizeof(progress), "%d%%", std::clamp(static_cast<int>(progressPercent + 0.5f), 0, 100));
  } else {
    std::snprintf(progress, sizeof(progress), "-");
  }
  std::snprintf(pages, sizeof(pages), "%lu", static_cast<unsigned long>(value.totalPagesTurned));
  drawStatCell(renderer, Rect{rect.x, rect.y, cellWidth, rect.height}, readingTime, tr(STR_STATS_TIME_LBL), radius);
  drawStatCell(renderer, Rect{rect.x + cellWidth + gap, rect.y, cellWidth, rect.height}, progress,
               tr(STR_STATS_PROGRESS_LBL), radius);
  drawStatCell(renderer, Rect{rect.x + (cellWidth + gap) * 2, rect.y, cellWidth, rect.height}, pages,
               tr(STR_STATS_PAGES_LBL), radius);
}

void drawGlobalStats(const GfxRenderer& renderer, const Rect rect, const GlobalReadingStats* stats, const int gap,
                     const int radius) {
  const GlobalReadingStats empty{};
  const GlobalReadingStats& value = stats ? *stats : empty;
  const int cellWidth = std::max(1, (rect.width - gap * 2) / 3);
  char readingTime[32];
  char completed[16];
  char sessions[16];
  BookReadingStats::formatDuration(value.totalReadingSeconds, readingTime, sizeof(readingTime));
  std::snprintf(completed, sizeof(completed), "%lu", static_cast<unsigned long>(value.completedBooks));
  std::snprintf(sessions, sizeof(sessions), "%lu", static_cast<unsigned long>(value.totalSessions));
  drawStatCell(renderer, Rect{rect.x, rect.y, cellWidth, rect.height}, readingTime,
               tr(STR_STATS_TOTAL_READING_TIME_LBL), radius);
  drawStatCell(renderer, Rect{rect.x + cellWidth + gap, rect.y, cellWidth, rect.height}, completed,
               tr(STR_STATS_COMPLETED_LBL), radius);
  drawStatCell(renderer, Rect{rect.x + (cellWidth + gap) * 2, rect.y, cellWidth, rect.height}, sessions,
               tr(STR_STATS_SESSIONS_LBL), radius);
}

const char* homeActionLabel(const CustomThemeInfo::HomeAction action) {
  switch (action) {
    case CustomThemeInfo::HomeAction::BrowseFiles:
      return tr(STR_BROWSE_FILES);
    case CustomThemeInfo::HomeAction::RecentBooks:
      return tr(STR_MENU_RECENT_BOOKS);
    case CustomThemeInfo::HomeAction::OpdsBrowser:
      return tr(STR_OPDS_BROWSER);
    case CustomThemeInfo::HomeAction::ReadingStats:
      return tr(STR_READING_STATS);
    case CustomThemeInfo::HomeAction::SavedItems:
      return tr(STR_BOOKMARKS);
    case CustomThemeInfo::HomeAction::FileTransfer:
      return tr(STR_FILE_TRANSFER);
    case CustomThemeInfo::HomeAction::Settings:
      return tr(STR_SETTINGS_TITLE);
  }
  return "";
}

bool homeActionAvailable(const uint16_t mask, const CustomThemeInfo::HomeAction action) {
  return (mask & (1U << static_cast<uint8_t>(action))) != 0;
}

void drawCanvasRecentBooks(const GfxRenderer& renderer, const CustomThemeInfo& info, const Rect rect,
                           const std::vector<RecentBook>& books, const int selected, const int thumbHeight,
                           const float progressPercent,
                           const CustomThemeInfo::HomeCanvasBlockVariant variant) {
  const int count = std::min(static_cast<int>(books.size()), static_cast<int>(info.homeRecentBooks));
  if (count <= 0 || rect.width <= 0 || rect.height <= 0) return;
  const int selectedIndex = std::clamp(selected, 0, count - 1);
  const int gap = std::min(static_cast<int>(info.homeBookGap), std::max(0, rect.width / 8));

  if (info.homeLayout == CustomThemeInfo::HomeLayout::Shelf) {
    const int cardWidth = std::max(1, (rect.width - gap * (count - 1)) / count);
    const int titleHeight = info.homeShowTitle ? renderer.getLineHeight(UI_10_FONT_ID) + kMetadataGap : 0;
    const int authorHeight = info.homeShowAuthor ? renderer.getLineHeight(SMALL_FONT_ID) + kMetadataGap : 0;
    const int progressHeight = info.homeShowProgress ? kProgressHeight + kMetadataGap : 0;
    for (int i = 0; i < count; ++i) {
      const Rect card{rect.x + i * (cardWidth + gap), rect.y, cardWidth, rect.height};
      int contentY = card.y + 5;
      if (info.homeShowCover) {
        const int maxCoverHeight = std::max(1, card.height - titleHeight - authorHeight - progressHeight - 10);
        const int coverHeight = std::min(thumbHeight, maxCoverHeight);
        const int coverWidth = std::min(std::max(1, card.width - 10), std::max(1, coverHeight * 2 / 3));
        const Rect cover{card.x + (card.width - coverWidth) / 2, contentY, coverWidth, coverHeight};
        drawCover(renderer, books[i], cover, info.coverCornerRadius, thumbHeight);
        contentY += coverHeight + kMetadataGap;
      }
      if (info.homeShowTitle) {
        const char* title = books[i].title.empty() ? books[i].path.c_str() : books[i].title.c_str();
        drawFittedText(renderer, UI_10_FONT_ID, card.x + 5, contentY, card.width - 10, title, true);
        contentY += titleHeight;
      }
      if (info.homeShowAuthor) {
        drawFittedText(renderer, SMALL_FONT_ID, card.x + 5, contentY, card.width - 10, books[i].author.c_str());
        contentY += authorHeight;
      }
      if (info.homeShowProgress && i == selectedIndex) {
        drawProgress(renderer, Rect{card.x + 5, contentY, card.width - 10, kProgressHeight}, progressPercent);
      }
      if (variant == CustomThemeInfo::HomeCanvasBlockVariant::Cards) {
        renderer.drawRoundedRect(card.x, card.y, card.width, card.height, i == selectedIndex ? 2 : 1,
                                 info.coverCornerRadius, true);
      }
    }
    return;
  }

  const RecentBook& book = books[selectedIndex];
  const bool dashboard = info.homeLayout == CustomThemeInfo::HomeLayout::Dashboard;
  const int coverWidth = info.homeShowCover
                             ? std::min(dashboard ? rect.width * 2 / 5 : rect.width, std::max(1, thumbHeight * 2 / 3))
                             : 0;
  const int coverHeight = info.homeShowCover ? std::min(rect.height, thumbHeight) : 0;
  const int coverX = dashboard ? rect.x : rect.x + (rect.width - coverWidth) / 2;
  if (info.homeShowCover) {
    drawCover(renderer, book, Rect{coverX, rect.y, coverWidth, coverHeight}, info.coverCornerRadius, thumbHeight);
  }
  const int contentX = dashboard && info.homeShowCover ? coverX + coverWidth + gap : rect.x;
  const int contentWidth = std::max(1, rect.x + rect.width - contentX);
  int contentY = dashboard ? rect.y + 5 : rect.y + coverHeight + kMetadataGap;
  if (info.homeShowTitle) {
    const char* title = book.title.empty() ? book.path.c_str() : book.title.c_str();
    drawFittedText(renderer, UI_10_FONT_ID, contentX, contentY, contentWidth, title, true);
    contentY += renderer.getLineHeight(UI_10_FONT_ID) + kMetadataGap;
  }
  if (info.homeShowAuthor) {
    drawFittedText(renderer, SMALL_FONT_ID, contentX, contentY, contentWidth, book.author.c_str());
    contentY += renderer.getLineHeight(SMALL_FONT_ID) + kMetadataGap;
  }
  if (info.homeShowProgress) {
    drawProgress(renderer, Rect{contentX, contentY, contentWidth, kProgressHeight}, progressPercent);
  }
}
}  // namespace

Rect DeclarativeTheme::normalized(const Rect parent, const uint16_t x, const uint16_t y, const uint16_t width,
                                  const uint16_t height) const {
  if (parent.width <= 0 || parent.height <= 0) return Rect{};
  const int localX = std::min(parent.width - 1, parent.width * x / 1000);
  const int localY = std::min(parent.height - 1, parent.height * y / 1000);
  const int resolvedWidth = std::max(1, std::min(parent.width - localX, parent.width * width / 1000));
  const int resolvedHeight = std::max(1, std::min(parent.height - localY, parent.height * height / 1000));
  return Rect(parent.x + localX, parent.y + localY, resolvedWidth, resolvedHeight);
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
                                           const BookReadingStats* stats, const float progressPercent,
                                           const GlobalReadingStats* globalStats, const char*) const {
  (void)restored;
  (void)store;
  drawBackground(renderer, rect);
  if (info_.homeCanvasEnabled) {
    const int visibleCount = std::min(static_cast<int>(books.size()), static_cast<int>(info_.homeRecentBooks));
    const int selectedIndex = visibleCount > 0 ? std::clamp(selected, 0, visibleCount - 1) : -1;
    const int thumbHeight = std::max(1, static_cast<int>(UITheme::getInstance().getMetrics().homeCoverHeight));
    for (uint8_t blockIndex = 0; blockIndex < info_.homeCanvasBlockCount; ++blockIndex) {
      const CustomThemeInfo::HomeCanvasBlock& block = info_.homeCanvasBlocks[blockIndex];
      const Rect blockRect = normalized(rect, block.x, block.y, block.width, block.height);
      const bool focused = canvasFocusedBlock_ == blockIndex;
      switch (block.type) {
        case CustomThemeInfo::HomeCanvasBlockType::RecentBooks:
          drawCanvasRecentBooks(renderer, info_, blockRect, books, selected, thumbHeight, progressPercent,
                                block.variant);
          break;
        case CustomThemeInfo::HomeCanvasBlockType::BookProgress: {
          char progress[16];
          if (progressPercent >= 0.0f) {
            std::snprintf(progress, sizeof(progress), "%d%%",
                          std::clamp(static_cast<int>(progressPercent + 0.5f), 0, 100));
          } else {
            std::snprintf(progress, sizeof(progress), "-");
          }
          drawFittedText(renderer, UI_10_FONT_ID, blockRect.x, blockRect.y, blockRect.width, progress, true);
          drawProgress(renderer,
                       Rect{blockRect.x, blockRect.y + renderer.getLineHeight(UI_10_FONT_ID) + 4, blockRect.width,
                            kProgressHeight},
                       progressPercent);
          break;
        }
        case CustomThemeInfo::HomeCanvasBlockType::BookStats:
          drawBookStats(renderer, blockRect, stats, progressPercent, kModuleGap,
                        std::min(6, static_cast<int>(info_.coverCornerRadius)));
          break;
        case CustomThemeInfo::HomeCanvasBlockType::GlobalStats:
          drawGlobalStats(renderer, blockRect, globalStats, kModuleGap,
                          std::min(6, static_cast<int>(info_.coverCornerRadius)));
          break;
        case CustomThemeInfo::HomeCanvasBlockType::QuickActions: {
          const bool inlineActions =
              info_.homeMenuPresentation == CustomThemeInfo::HomeMenuPresentation::Inline;
          const uint8_t configuredCount =
              inlineActions ? info_.homeActionOrderCount : info_.homePinnedActionCount;
          const auto actionAt = [this, inlineActions](const uint8_t index) {
            return inlineActions ? info_.homeActionOrder[index] : info_.homePinnedActions[index];
          };
          int availableCount = 0;
          for (uint8_t i = 0; i < configuredCount; ++i) {
            if (homeActionAvailable(canvasAvailableActions_, actionAt(i))) ++availableCount;
          }
          if (availableCount == 0) break;
          const int gap = std::min(static_cast<int>(info_.homeBookGap), std::max(0, blockRect.width / 8));
          const int columns = inlineActions ? std::min(3, availableCount) : availableCount;
          const int rows = (availableCount + columns - 1) / columns;
          const int cellWidth = std::max(1, (blockRect.width - gap * (columns - 1)) / columns);
          const int cellHeight = std::max(1, (blockRect.height - gap * (rows - 1)) / rows);
          int visibleAction = 0;
          for (uint8_t i = 0; i < configuredCount; ++i) {
            const auto action = actionAt(i);
            if (!homeActionAvailable(canvasAvailableActions_, action)) continue;
            const int column = visibleAction % columns;
            const int row = visibleAction / columns;
            const Rect cell{blockRect.x + column * (cellWidth + gap), blockRect.y + row * (cellHeight + gap),
                            cellWidth, cellHeight};
            const bool selectedAction = focused && canvasQuickAction_ == visibleAction;
            if (selectedAction) renderer.fillRect(cell.x, cell.y, cell.width, cell.height, true);
            renderer.drawRoundedRect(cell.x, cell.y, cell.width, cell.height, 1, info_.coverCornerRadius,
                                     !selectedAction);
            char label[kTextCapacity];
            fitText(renderer, UI_10_FONT_ID, homeActionLabel(action), cell.width - 10, label, sizeof(label));
            renderer.drawText(UI_10_FONT_ID, cell.x + (cell.width - renderer.getTextWidth(UI_10_FONT_ID, label)) / 2,
                              cell.y + (cell.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label,
                              !selectedAction);
            ++visibleAction;
          }
          break;
        }
        case CustomThemeInfo::HomeCanvasBlockType::MenuTrigger:
          if (focused) renderer.fillRect(blockRect.x, blockRect.y, blockRect.width, blockRect.height, true);
          renderer.drawRoundedRect(blockRect.x, blockRect.y, blockRect.width, blockRect.height, 1,
                                   info_.coverCornerRadius, !focused);
          {
            char label[kTextCapacity];
            fitText(renderer, UI_10_FONT_ID, tr(STR_MENU), blockRect.width - 16, label, sizeof(label),
                    EpdFontFamily::BOLD);
            renderer.drawText(UI_10_FONT_ID, blockRect.x + (blockRect.width - renderer.getTextWidth(
                                                                                UI_10_FONT_ID, label,
                                                                                EpdFontFamily::BOLD)) /
                                                            2,
                              blockRect.y + (blockRect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label,
                              !focused, EpdFontFamily::BOLD);
          }
          break;
      }
      if (focused && block.type != CustomThemeInfo::HomeCanvasBlockType::QuickActions &&
          block.type != CustomThemeInfo::HomeCanvasBlockType::MenuTrigger) {
        renderer.drawRoundedRect(blockRect.x, blockRect.y, blockRect.width, blockRect.height, 2,
                                 info_.coverCornerRadius, true);
      }
    }
    rendered = false;
    stored = false;
    return;
  }
  const int padding = std::min(kContentPadding, std::max(0, rect.width / 20));
  const int moduleGap = std::min(kModuleGap, static_cast<int>(info_.homeBookGap));
  int moduleCount = info_.homeShowBookStats ? 1 : 0;
  moduleCount += info_.homeShowGlobalStats ? 1 : 0;
  const int modulesHeight = moduleCount * kModuleHeight + std::max(0, moduleCount - 1) * moduleGap;
  Rect booksRect{rect.x + padding, rect.y + padding, std::max(0, rect.width - padding * 2),
                 std::max(0, rect.height - padding * 2 - modulesHeight - (moduleCount > 0 ? moduleGap : 0))};

  const int visibleCount = std::min(static_cast<int>(books.size()), static_cast<int>(info_.homeRecentBooks));
  const int selectedIndex = visibleCount > 0 ? std::clamp(selected, 0, visibleCount - 1) : -1;
  const int thumbHeight = std::max(1, static_cast<int>(UITheme::getInstance().getMetrics().homeCoverHeight));

  if (selectedIndex >= 0 && info_.homeLayout == CustomThemeInfo::HomeLayout::Shelf) {
    const int gap = info_.homeBookGap;
    const int cardWidth = std::max(1, (booksRect.width - gap * (visibleCount - 1)) / visibleCount);
    const int titleHeight = info_.homeShowTitle ? renderer.getLineHeight(UI_10_FONT_ID) + kMetadataGap : 0;
    const int authorHeight = info_.homeShowAuthor ? renderer.getLineHeight(SMALL_FONT_ID) + kMetadataGap : 0;
    const int progressHeight = info_.homeShowProgress ? kProgressHeight + kMetadataGap : 0;
    const int metadataHeight = titleHeight + authorHeight + progressHeight;
    for (int i = 0; i < visibleCount; ++i) {
      const Rect card{booksRect.x + i * (cardWidth + gap), booksRect.y, cardWidth, booksRect.height};
      int contentY = card.y + 5;
      if (info_.homeShowCover) {
        const int maxCoverHeight = std::max(1, card.height - metadataHeight - 10);
        const int coverHeight = std::min(thumbHeight, maxCoverHeight);
        const int coverWidth = std::min(card.width - 10, std::max(1, coverHeight * 2 / 3));
        const Rect cover{card.x + (card.width - coverWidth) / 2, contentY, coverWidth, coverHeight};
        drawCover(renderer, books[i], cover, info_.coverCornerRadius, thumbHeight);
        contentY = cover.y + cover.height + kMetadataGap;
      }
      if (info_.homeShowTitle) {
        const char* title = books[i].title.empty() ? books[i].path.c_str() : books[i].title.c_str();
        drawFittedText(renderer, UI_10_FONT_ID, card.x + 5, contentY, card.width - 10, title, true);
        contentY += titleHeight;
      }
      if (info_.homeShowAuthor) {
        drawFittedText(renderer, SMALL_FONT_ID, card.x + 5, contentY, card.width - 10, books[i].author.c_str());
        contentY += authorHeight;
      }
      if (info_.homeShowProgress && i == selectedIndex) {
        drawProgress(renderer, Rect{card.x + 5, contentY, card.width - 10, kProgressHeight}, progressPercent);
      }
      renderer.drawRoundedRect(card.x, card.y, card.width, card.height, i == selectedIndex ? 2 : 1,
                               info_.coverCornerRadius, true);
    }
  } else if (selectedIndex >= 0 && info_.homeLayout == CustomThemeInfo::HomeLayout::Dashboard) {
    const RecentBook& book = books[selectedIndex];
    const int coverWidth = info_.homeShowCover ? std::min(booksRect.width * 2 / 5, thumbHeight * 2 / 3) : 0;
    const int coverHeight = info_.homeShowCover ? std::min(booksRect.height, thumbHeight) : 0;
    if (info_.homeShowCover) {
      drawCover(renderer, book, Rect{booksRect.x, booksRect.y, coverWidth, coverHeight}, info_.coverCornerRadius,
                thumbHeight);
    }
    const int contentX = booksRect.x + (info_.homeShowCover ? coverWidth + info_.homeBookGap : 0);
    const int contentWidth = std::max(1, booksRect.x + booksRect.width - contentX);
    int contentY = booksRect.y + 5;
    if (info_.homeShowTitle) {
      const char* title = book.title.empty() ? book.path.c_str() : book.title.c_str();
      drawFittedText(renderer, UI_12_FONT_ID, contentX, contentY, contentWidth, title, true);
      contentY += renderer.getLineHeight(UI_12_FONT_ID) + 8;
    }
    if (info_.homeShowAuthor) {
      drawFittedText(renderer, UI_10_FONT_ID, contentX, contentY, contentWidth, book.author.c_str());
      contentY += renderer.getLineHeight(UI_10_FONT_ID) + 10;
    }
    if (info_.homeShowProgress) {
      drawProgress(renderer, Rect{contentX, contentY, contentWidth, kProgressHeight}, progressPercent);
      contentY += kProgressHeight + 7;
      char progress[16];
      if (progressPercent >= 0.0f) {
        std::snprintf(progress, sizeof(progress), "%d%%",
                      std::clamp(static_cast<int>(progressPercent + 0.5f), 0, 100));
      } else {
        std::snprintf(progress, sizeof(progress), "-");
      }
      drawFittedText(renderer, SMALL_FONT_ID, contentX, contentY, contentWidth, progress);
    }
  } else if (selectedIndex >= 0) {
    const RecentBook& book = books[selectedIndex];
    Rect cover = normalized(booksRect, info_.coverX, info_.coverY, info_.coverWidth, info_.coverHeight);
    if (cover.width > cover.height * 3 / 2) {
      cover.width = cover.height * 3 / 5;
      cover.x = booksRect.x + (booksRect.width - cover.width) / 2;
    }
    if (info_.homeShowCover) drawCover(renderer, book, cover, info_.coverCornerRadius, thumbHeight);
    int contentY = info_.homeShowCover ? cover.y + cover.height + kMetadataGap : booksRect.y + 5;
    if (info_.homeShowTitle) {
      const char* title = book.title.empty() ? book.path.c_str() : book.title.c_str();
      drawFittedText(renderer, UI_10_FONT_ID, booksRect.x, contentY, booksRect.width, title, true);
      contentY += renderer.getLineHeight(UI_10_FONT_ID) + kMetadataGap;
    }
    if (info_.homeShowAuthor) {
      drawFittedText(renderer, SMALL_FONT_ID, booksRect.x, contentY, booksRect.width, book.author.c_str());
      contentY += renderer.getLineHeight(SMALL_FONT_ID) + kMetadataGap;
    }
    if (info_.homeShowProgress) {
      drawProgress(renderer, Rect{booksRect.x, contentY, booksRect.width, kProgressHeight}, progressPercent);
    }
  }

  int moduleY = booksRect.y + booksRect.height + (moduleCount > 0 ? moduleGap : 0);
  if (info_.homeShowBookStats) {
    drawBookStats(renderer, Rect{booksRect.x, moduleY, booksRect.width, kModuleHeight}, stats, progressPercent,
                  moduleGap, std::min(6, static_cast<int>(info_.coverCornerRadius)));
    moduleY += kModuleHeight + moduleGap;
  }
  if (info_.homeShowGlobalStats) {
    drawGlobalStats(renderer, Rect{booksRect.x, moduleY, booksRect.width, kModuleHeight}, globalStats, moduleGap,
                    std::min(6, static_cast<int>(info_.coverCornerRadius)));
  }

  // Declarative modules and selection state change together. Caching this
  // region would retain roughly 15 KB yet still require a full redraw, so the
  // theme deliberately keeps no secondary Home snapshot.
  rendered = false;
  stored = false;
}

void DeclarativeTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int count, const int selected,
                                      const std::function<const char*(int)>& label,
                                      const std::function<UIIcon(int)>&) const {
  if (count <= 0) return;
  const int columns = std::max(
      1, static_cast<int>(info_.homeCanvasEnabled ? info_.homePanelColumns : info_.menuColumns));
  const int rows = (count + columns - 1) / columns;
  const int gap = info_.menuGap;
  const int rowHeight =
      info_.menuRowHeight > 0 ? info_.menuRowHeight : std::max(1, (rect.height - gap * (rows - 1)) / rows);
  const int cellWidth = std::max(1, (rect.width - gap * (columns - 1)) / columns);
  for (int i = 0; i < count; ++i) {
    const int col = i % columns;
    const int row = i / columns;
    const Rect cell{rect.x + col * (cellWidth + gap), rect.y + row * (rowHeight + gap), cellWidth, rowHeight};
    const bool isSelected = i == selected;
    if (isSelected) renderer.fillRect(cell.x, cell.y, cell.width, cell.height, true);
    renderer.drawRoundedRect(cell.x, cell.y, cell.width, cell.height, 1, 8, !isSelected);
    char visible[kTextCapacity];
    fitText(renderer, UI_10_FONT_ID, label(i), cell.width - 12, visible, sizeof(visible));
    const int textW = renderer.getTextWidth(UI_10_FONT_ID, visible);
    renderer.drawText(UI_10_FONT_ID, cell.x + (cell.width - textW) / 2,
                      cell.y + (cell.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, visible, !isSelected);
  }
}
