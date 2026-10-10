#include "BelacquaTheme.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <string>

#include "RecentBooksStore.h"
#include "activities/home/RecentBookProgress.h"
#include "components/TouchRegistry.h"
#include "fontIds.h"

namespace {
constexpr int TILE_PADDING = 12;
constexpr int CORNER_RADIUS = 6;
}  // namespace

void BelacquaTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect,
                                        const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                        bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                        const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats,
                                        const float progressPercent, const GlobalReadingStats* globalStats,
                                        const char* currentChapterTitle) const {
  (void)bufferRestored;
  (void)storeCoverBuffer;
  (void)stats;
  (void)globalStats;
  (void)currentChapterTitle;
  // No cover: nothing to cache between renders.
  coverRendered = false;
  coverBufferStored = false;

  const auto& metrics = BelacquaMetrics::values;
  const int tileX = metrics.contentSidePadding;
  const int tileWidth = rect.width - 2 * metrics.contentSidePadding;
  const int tileY = rect.y;
  const int tileHeight = rect.height;
  if (recentBooks.empty()) {
    drawEmptyRecents(renderer, rect);
    return;
  }

  const RecentBook& book = recentBooks[0];
  TouchRegistry::getInstance().add(Rect{tileX, tileY, tileWidth, tileHeight}, 0, TouchRegistry::Cover);
  if (selectorIndex == 0) {
    renderer.fillRoundedRect(tileX, tileY, tileWidth, tileHeight, CORNER_RADIUS, Color::LightGray);
  } else {
    renderer.drawRoundedRect(tileX, tileY, tileWidth, tileHeight, 1, CORNER_RADIUS, true);
  }

  const int textX = tileX + TILE_PADDING;
  const int textWidth = tileWidth - 2 * TILE_PADDING;
  const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, book.title.c_str(), textWidth, 2, EpdFontFamily::BOLD);
  const std::string author = renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), textWidth);
  const bool hasProgress = progressPercent >= 0.0f;
  const std::string progress = hasProgress ? RecentBookProgress::formatPercent(progressPercent, book.path) : "";

  const int titleLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  constexpr int barHeight = 4;
  const int blockHeight = titleLineHeight * static_cast<int>(titleLines.size()) +
                          (book.author.empty() ? 0 : smallLineHeight) +
                          (hasProgress ? smallLineHeight + barHeight + 4 : 0);
  int y = tileY + std::max(TILE_PADDING / 2, (tileHeight - blockHeight) / 2);
  for (const auto& line : titleLines) {
    renderer.drawText(UI_12_FONT_ID, textX, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += titleLineHeight;
  }
  if (!book.author.empty()) {
    renderer.drawText(UI_10_FONT_ID, textX, y, author.c_str(), true);
    y += smallLineHeight;
  }
  if (hasProgress) {
    renderer.drawText(UI_10_FONT_ID, textX, y, progress.c_str(), true, EpdFontFamily::BOLD);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, progress.c_str(), EpdFontFamily::BOLD);
    const int barX = textX + labelWidth + 10;
    const int barWidth = std::max(0, textX + textWidth - barX);
    const int barY = y + (smallLineHeight - barHeight) / 2;
    if (barWidth > 0) {
      renderer.drawRect(barX, barY, barWidth, barHeight, true);
      const int filled = std::clamp(static_cast<int>(progressPercent / 100.0f * barWidth), 0, barWidth);
      if (filled > 2) renderer.fillRect(barX + 1, barY + 1, filled - 2, barHeight - 2);
    }
  }
}
