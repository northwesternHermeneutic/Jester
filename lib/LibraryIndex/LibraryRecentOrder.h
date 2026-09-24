#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace library {

// Translate a shelf row to RecentAsc index space. Known reading history stays
// together, followed by books without history; neither section duplicates a
// book. The caller supplies unique, valid recent rows in newest-read order.
inline uint16_t recentShelfRow(const uint16_t row, const uint16_t total, const uint16_t* recentRows,
                               const size_t recentCount, const bool descending) {
  if (row >= total || recentCount > total) return UINT16_MAX;
  if (row < recentCount) return recentRows[descending ? row : recentCount - 1 - row];
  const uint16_t unpinned = static_cast<uint16_t>(row - recentCount);
  uint16_t candidate = unpinned;
  // Skip known reads in direction-space. At most recentCount + 1 passes;
  // work is bounded by history size, even on a 4,096-book card.
  for (;;) {
    uint16_t skipped = 0;
    for (size_t i = 0; i < recentCount; ++i) {
      const uint16_t position = descending ? static_cast<uint16_t>(total - 1 - recentRows[i]) : recentRows[i];
      if (position <= candidate) ++skipped;
    }
    const uint16_t next = static_cast<uint16_t>(unpinned + skipped);
    if (next == candidate) return descending ? static_cast<uint16_t>(total - 1 - candidate) : candidate;
    candidate = next;
  }
}

}  // namespace library
