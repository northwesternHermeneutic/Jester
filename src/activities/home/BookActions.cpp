#include "BookActions.h"

#include <Epub.h>
#include <Epub/EpubRenderMode.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Xtc.h>

#include <cstdio>

#include "BookmarkStore.h"
#include "ClippingStore.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/reader/GlobalReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookCacheUtils.h"
#include "util/BookMoveUtils.h"
#include "FavoritesStore.h"

namespace BookActions {
namespace {

bool hasReadingStats(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path);
}

std::string bookStatsCachePath(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) {
    return Epub(path, "/.crosspoint").getCachePath();
  }
  if (FsHelpers::hasXtcExtension(path)) {
    return Xtc(path, "/.crosspoint").getCachePath();
  }
  return "";
}

}  // namespace

std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string& fullPath,
                                                                      const bool includeRemoveFromRecents) {
  std::vector<FileBrowserActionActivity::MenuItem> items;
  items.reserve(includeRemoveFromRecents ? 8 : 7);
  if (canBeFavorite(fullPath)) {
    // Jester (feature 1): first, so the long-press menu leads with it.
    const bool favorite = Favorites::contains(fullPath);
    items.push_back({favorite ? FileBrowserAction::RemoveFromFavorites : FileBrowserAction::AddToFavorites,
                     favorite ? StrId::STR_REMOVE_FROM_FAVORITES : StrId::STR_ADD_TO_FAVORITES});
  }
  items.push_back({FileBrowserAction::Delete, StrId::STR_DELETE});
  if (hasClearableBookCache(fullPath)) {
    items.push_back({FileBrowserAction::DeleteCache, StrId::STR_DELETE_CACHE});
  }
  if (FsHelpers::hasEpubExtension(fullPath)) {
    items.push_back({FileBrowserAction::EpubRenderMode, StrId::STR_EPUB_RENDER_MODE});
    items.push_back({FileBrowserAction::ResetReaderSettings, StrId::STR_RESET_BOOK_READER_SETTINGS});
  }
  if (hasReadingStats(fullPath)) {
    // Jester (feature 17): no Delete Stats entry; Mark Finished stays.
    items.push_back({FileBrowserAction::ToggleCompleted,
                     isBookCompleted(fullPath) ? StrId::STR_MARK_UNFINISHED : StrId::STR_MARK_FINISHED});
  }
  if (includeRemoveFromRecents) {
    items.push_back({FileBrowserAction::RemoveFromRecents, StrId::STR_REMOVE_FROM_RECENTS_ACTION});
  }
  return items;
}

bool hasClearableBookCache(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path);
}

bool canBeFavorite(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasTxtExtension(path) || FsHelpers::hasXtcExtension(path);
}

void toggleFavorite(const GfxRenderer& renderer, const std::string& path, const std::string& title,
                    const std::string& author) {
  switch (Favorites::toggle(path, title, author)) {
    case Favorites::ToggleResult::Added:
      drawToast(renderer, tr(STR_ADDED_TO_FAVORITES));
      break;
    case Favorites::ToggleResult::Removed:
      drawToast(renderer, tr(STR_REMOVED_FROM_FAVORITES));
      break;
    case Favorites::ToggleResult::Full:
      drawToast(renderer, tr(STR_FAVORITES_FULL));
      break;
    case Favorites::ToggleResult::Failed:
    default:
      drawToast(renderer, tr(STR_FAVORITES_SAVE_FAILED));
      break;
  }
}

bool canSendNearby(const std::string& path) {
  // Jester (feature 16): Nearby transfer is removed.
  (void)path;
  return false;
}

void clearFileMetadata(const std::string& fullPath) {
  if (FsHelpers::hasEpubExtension(fullPath)) {
    Epub(fullPath, "/.crosspoint").clearCache();
    BookmarkStore::deleteForFilePath(fullPath, "epub");
    BookmarkStore::deleteForFilePath(fullPath, "epub", DOGEARS_STORE_DIR);
    ClippingStore::deleteForFilePath(fullPath, "epub");
  } else if (FsHelpers::hasXtcExtension(fullPath)) {
    BookmarkStore::deleteForFilePath(fullPath, "xtc");
    BookmarkStore::deleteForFilePath(fullPath, "xtc", DOGEARS_STORE_DIR);
  } else if (FsHelpers::hasTxtExtension(fullPath) || FsHelpers::hasMarkdownExtension(fullPath)) {
    BookmarkStore::deleteForFilePath(fullPath, "txt");
    BookmarkStore::deleteForFilePath(fullPath, "txt", DOGEARS_STORE_DIR);
  }
}

bool clearBookCache(const std::string& fullPath) {
  if (FsHelpers::hasEpubExtension(fullPath) || FsHelpers::hasXtcExtension(fullPath)) {
    return clearBookCachePreservingUserState(fullPath);
  }
  return false;
}

bool deleteBookStats(const std::string& fullPath) {
  const std::string cachePath = bookStatsCachePath(fullPath);
  if (cachePath.empty()) {
    return false;
  }
  return BookReadingStats::remove(cachePath);
}

bool resetBookReaderSettings(const std::string& fullPath) {
  if (!FsHelpers::hasEpubExtension(fullPath)) {
    return false;
  }
  return EpubReaderActivity::resetBookReaderSettings(fullPath);
}

std::vector<std::string> epubRenderModeOptions() {
  return {I18N.get(StrId::STR_RENDER_MODE_CROSSINK_DEFAULT), I18N.get(StrId::STR_RENDER_MODE_BALANCED),
          I18N.get(StrId::STR_RENDER_MODE_LIGHT)};
}

uint8_t epubRenderModeDisplayIndex(const uint8_t renderMode) {
  switch (static_cast<EpubRenderMode>(renderMode)) {
    case EpubRenderMode::Balanced:
      return 1;
    case EpubRenderMode::Light:
      return 2;
    case EpubRenderMode::CrossInkDefault:
    default:
      return 0;
  }
}

uint8_t epubRenderModeForDisplayIndex(const uint8_t displayIndex) {
  switch (displayIndex) {
    case 1:
      return static_cast<uint8_t>(EpubRenderMode::Balanced);
    case 2:
      return static_cast<uint8_t>(EpubRenderMode::Light);
    case 0:
    default:
      return static_cast<uint8_t>(EpubRenderMode::CrossInkDefault);
  }
}

std::string confirmationHeading(const StrId actionLabelId) {
  return std::string(tr(STR_CONFIRM)) + ": " + std::string(I18N.get(actionLabelId));
}

bool isBookCompleted(const std::string& fullPath) {
  const std::string cachePath = bookStatsCachePath(fullPath);
  return !cachePath.empty() && BookReadingStats::load(cachePath).isCompleted;
}

bool toggleBookCompleted(const std::string& fullPath, const std::string& displayName, bool& completed) {
  const bool isEpub = FsHelpers::hasEpubExtension(fullPath);
  const bool isXtc = FsHelpers::hasXtcExtension(fullPath);
  if (!isEpub && !isXtc) {
    return false;
  }

  Epub epub(fullPath, "/.crosspoint");
  Xtc xtc(fullPath, "/.crosspoint");
  std::string cachePath;
  std::string title;
  std::string author;
  if (isEpub) {
    epub.setupCacheDir();
    cachePath = epub.getCachePath();
    title = epub.getTitle();
    author = epub.getAuthor();
  } else {
    if (!xtc.load()) {
      return false;
    }
    xtc.setupCacheDir();
    cachePath = xtc.getCachePath();
    title = xtc.getTitle();
    author = xtc.getAuthor();
  }

  BookReadingStats stats = BookReadingStats::load(cachePath);
  completed = !stats.isCompleted;
  stats.isCompleted = completed;
  if (completed && !stats.finishedDateManual) {
    ReadingStatsDateTime now;
    if (getCurrentLocalReadingStatsDateTime(now)) {
      stats.finishedDate = now.date;
    }
  }

  GlobalReadingStats globalStats = GlobalReadingStats::load();
  if (completed) {
    globalStats.completedBooks++;
  } else if (globalStats.completedBooks > 0) {
    globalStats.completedBooks--;
  }

  stats.save(cachePath);
  globalStats.save();

  // Changing completion status does not open a book. The reader adds it to
  // recents if it is opened again after being marked unfinished.
  RECENT_BOOKS.setFinished(fullPath, completed);  // Jester: Home tile skips finished books
  if (SETTINGS.removeReadBooksFromRecents && completed) RECENT_BOOKS.removeByPath(fullPath);

  if (isEpub && completed && SETTINGS.moveFinishedToReadFolder && fullPath.rfind("/Read/", 0) != 0) {
    const std::string oldCachePath = epub.getCachePath();
    const std::string dstPath = BookMoveUtils::buildReadFolderDestination(fullPath);
    LOG_INF("BookActions", "Moving completed epub: %s -> %s", fullPath.c_str(), dstPath.c_str());
    if (!Storage.rename(fullPath.c_str(), dstPath.c_str())) {
      LOG_ERR("BookActions", "Failed to move book to 'Read' folder");
      snprintf(APP_STATE.pendingAlertTitle, sizeof(APP_STATE.pendingAlertTitle), "%s",
               tr(STR_MOVE_TO_READ_FAILED_TITLE));
      snprintf(APP_STATE.pendingAlertBody, sizeof(APP_STATE.pendingAlertBody), tr(STR_MOVE_TO_READ_FAILED_BODY),
               displayName.c_str());
      APP_STATE.pendingAlertGoHomeOnBack.store(false, std::memory_order_relaxed);
      APP_STATE.hasPendingAlert.store(true, std::memory_order_release);
      return true;
    }

    BookMoveUtils::migrateMovedEpubState(fullPath, dstPath, oldCachePath, title, author,
                                         !SETTINGS.removeReadBooksFromRecents);
  }

  return true;
}

void drawToast(const GfxRenderer& renderer, const char* msg) {
  constexpr int toastPadX = 20;
  constexpr int toastPadY = 12;
  const int msgW = renderer.getTextWidth(UI_10_FONT_ID, msg);
  const int msgH = renderer.getLineHeight(UI_10_FONT_ID);
  const int toastW = msgW + toastPadX * 2;
  const int toastH = msgH + toastPadY * 2;
  const int toastX = (renderer.getScreenWidth() - toastW) / 2;
  const int toastY = (renderer.getScreenHeight() - toastH) / 2;
  renderer.fillRect(toastX, toastY, toastW, toastH, true);
  renderer.drawText(UI_10_FONT_ID, toastX + toastPadX, toastY + toastPadY, msg, false);
  renderer.displayBuffer();
}

}  // namespace BookActions
