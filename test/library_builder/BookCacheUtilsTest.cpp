#include <gtest/gtest.h>

#include <string>

#include "../../src/util/BookCacheUtils.h"
#include "Epub.h"
#include "HalStorage.h"

namespace {

class BookCacheUtilsTest : public ::testing::Test {
 protected:
  std::string cachePath;

  void SetUp() override {
    fake::reset();
    cachePath = Epub("/a.epub", "/.crosspoint").getCachePath();
    fake::add(cachePath + "/progress.bin", "progress");
    fake::add(cachePath + "/stats_v5.bin", "stats");
    fake::add(cachePath + "/reader_settings.bin", "settings");
    fake::add(cachePath + "/book.bin", "derived");
  }

  void expectUserStateIntact() {
    const auto expectFile = [](const std::string& path, const std::string& expected) {
      ASSERT_TRUE(Storage.exists(path.c_str())) << path;
      const auto& bytes = fake::files.at(path)->bytes;
      EXPECT_EQ(std::string(bytes.begin(), bytes.end()), expected);
    };
    expectFile(cachePath + "/progress.bin", "progress");
    expectFile(cachePath + "/stats_v5.bin", "stats");
    expectFile(cachePath + "/reader_settings.bin", "settings");
  }
};

TEST_F(BookCacheUtilsTest, ClearsDerivedFilesWhileKeepingReadingState) {
  ASSERT_TRUE(clearBookCachePreservingUserState("/a.epub"));
  expectUserStateIntact();
  EXPECT_FALSE(Storage.exists((cachePath + "/book.bin").c_str()));
  EXPECT_EQ(fake::directoryEntriesByPath[cachePath], 0u);
}

TEST_F(BookCacheUtilsTest, RetryRecoversStatsAfterFailedRestore) {
  fake::failRenameToPath = cachePath + "/stats_v5.bin";
  ASSERT_FALSE(clearBookCachePreservingUserState("/a.epub"));
  ASSERT_TRUE(Storage.exists((cachePath + ".upload_preserve_stats_v5.bin").c_str()));
  ASSERT_TRUE(Storage.exists((cachePath + ".upload_preserve_stats_pending").c_str()));

  ASSERT_TRUE(clearBookCachePreservingUserState("/a.epub"));
  expectUserStateIntact();
  EXPECT_FALSE(Storage.exists((cachePath + ".upload_preserve_stats_v5.bin").c_str()));
  EXPECT_FALSE(Storage.exists((cachePath + ".upload_preserve_stats_pending").c_str()));
}

TEST_F(BookCacheUtilsTest, RetryRecoversProgressAfterFailedRestore) {
  fake::failRenameToPath = cachePath + "/progress.bin";
  ASSERT_FALSE(clearBookCachePreservingUserState("/a.epub"));
  ASSERT_TRUE(Storage.exists((cachePath + ".upload_preserve_progress.bin").c_str()));

  ASSERT_TRUE(clearBookCachePreservingUserState("/a.epub"));
  expectUserStateIntact();
  EXPECT_FALSE(Storage.exists((cachePath + ".upload_preserve_progress.bin").c_str()));
}

}  // namespace
