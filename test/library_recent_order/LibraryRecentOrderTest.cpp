#include <LibraryRecentOrder.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <vector>

namespace {
std::vector<uint16_t> shelf(const uint16_t total, const std::vector<uint16_t>& recents, const bool descending) {
  std::vector<uint16_t> out;
  for (uint16_t row = 0; row < total; ++row) {
    out.push_back(library::recentShelfRow(row, total, recents.data(), recents.size(), descending));
  }
  return out;
}
}  // namespace

TEST(LibraryRecentOrder, ReadingHistoryIsSeparateFromDateAddedWithoutDuplicates) {
  EXPECT_EQ(shelf(7, {2, 5, 0}, true), (std::vector<uint16_t>{2, 5, 0, 6, 4, 3, 1}));
  EXPECT_EQ(shelf(7, {2, 5, 0}, false), (std::vector<uint16_t>{0, 5, 2, 1, 3, 4, 6}));
}

TEST(LibraryRecentOrder, NoHistoryFallsBackToArrivalOrder) {
  EXPECT_EQ(shelf(4, {}, true), (std::vector<uint16_t>{3, 2, 1, 0}));
  EXPECT_EQ(shelf(4, {}, false), (std::vector<uint16_t>{0, 1, 2, 3}));
}

TEST(LibraryRecentOrder, WholeShelfCanHaveReadingHistory) {
  EXPECT_EQ(shelf(3, {1, 2, 0}, true), (std::vector<uint16_t>{1, 2, 0}));
  EXPECT_EQ(shelf(3, {1, 2, 0}, false), (std::vector<uint16_t>{0, 2, 1}));
}

TEST(LibraryRecentOrder, EverySmallHistoryPermutationVisitsEachBookExactlyOnce) {
  std::vector<uint16_t> order{0, 1, 2, 3, 4};
  do {
    for (size_t size = 0; size <= order.size(); ++size) {
      const std::vector<uint16_t> recents(order.begin(), order.begin() + size);
      for (const bool descending : {false, true}) {
        auto rows = shelf(5, recents, descending);
        std::sort(rows.begin(), rows.end());
        EXPECT_EQ(rows, (std::vector<uint16_t>{0, 1, 2, 3, 4}));
      }
    }
  } while (std::next_permutation(order.begin(), order.end()));
}

TEST(LibraryRecentOrder, MaximumLibraryAndEighteenRecentBooks) {
  const std::vector<uint16_t> recents{4095, 4094, 4093, 4092, 4091, 4090, 4089, 4088, 4087,
                                      4086, 4085, 4084, 4083, 4082, 4081, 4080, 4079, 4078};
  auto rows = shelf(4096, recents, true);
  EXPECT_EQ(rows[18], 4077);
  std::sort(rows.begin(), rows.end());
  for (uint16_t i = 0; i < 4096; ++i) EXPECT_EQ(rows[i], i);
}

TEST(LibraryRecentOrder, EmptyAndOutOfRangeRowsAreRejected) {
  EXPECT_EQ(library::recentShelfRow(0, 0, nullptr, 0, true), UINT16_MAX);
  EXPECT_EQ(library::recentShelfRow(5, 5, nullptr, 0, false), UINT16_MAX);
}
