#pragma once

#include "components/themes/lyra/LyraTheme.h"

// Jester (feature 5): Belacqua, a modified Lyra and Jester's default UI.
// Same components and small/large scale as Lyra; the Home hero tile has no
// cover: the most recent book's title, author and progress (at the user's
// decimal level), at least 1.5x a menu row high. Home order (HomeActivity):
// hero, Favorites, Files, Library, Bookmarks & Clippings, File Transfer,
// Settings.
namespace BelacquaMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics metrics = LyraMetrics::values;
  // 120 px hero against 56 px menu rows (> 1.5x), leaving room for the menu.
  metrics.homeCoverHeight = 104;
  metrics.homeCoverTileHeight = 120;
  return metrics;
}
constexpr ThemeMetrics values = makeValues();
}  // namespace BelacquaMetrics

class BelacquaTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats,
                           float progressPercent, const GlobalReadingStats* globalStats,
                           const char* currentChapterTitle) const override;
};
