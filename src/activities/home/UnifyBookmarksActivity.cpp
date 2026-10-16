#include "UnifyBookmarksActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryIndexFile.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "BookmarkStore.h"
#include "ClippingStore.h"
#include "FavoritesStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

// Case-insensitive, trimmed comparison key for titles and authors.
std::string matchKey(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  std::string key = text.substr(begin, end - begin);
  std::transform(key.begin(), key.end(), key.begin(), [](const unsigned char c) { return std::tolower(c); });
  return key;
}

std::string typeForPath(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) return "epub";
  if (FsHelpers::hasXtcExtension(path)) return "xtc";
  if (FsHelpers::hasTxtExtension(path)) return "txt";
  return "";
}

}  // namespace

UnifyBookmarksActivity::UnifyBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("UnifyBookmarks", renderer, mappedInput) {}

void UnifyBookmarksActivity::onEnter() {
  Activity::onEnter();
  state = State::Searching;
  searchStarted = false;
  candidates.clear();
  nextCandidate = 0;
  merged = 0;
  ignored = 0;
  indexMissing = false;
  requestUpdate();
}

void UnifyBookmarksActivity::findCandidates() {
  // Missing books that still own saved items, one entry per old path.
  std::vector<Candidate> missing;
  const auto addMissing = [&missing](const std::string& path, const std::string& title, const std::string& author,
                                     const std::string& type) {
    if (path.empty() || title.empty() || type.empty() || Storage.exists(path.c_str())) return;
    for (const auto& existing : missing) {
      if (existing.oldPath == path) return;
    }
    missing.push_back({path, "", title, author, type});
  };
  {
    std::vector<BookmarkedBookEntry> books;
    BookmarkStore::getAllBookmarkedBooks(books);
    BookmarkStore::getAllBookmarkedBooks(books, DOGEARS_STORE_DIR);
    for (const auto& book : books) addMissing(book.bookPath, book.bookTitle, book.bookAuthor, book.bookType);
  }
  {
    std::vector<ClippedBookEntry> books;
    ClippingStore::getAllClippedBooks(books);
    for (const auto& book : books) addMissing(book.bookPath, book.bookTitle, book.bookAuthor, book.bookType);
  }
  {
    std::vector<FavoriteBook> books;
    Favorites::load(books);
    for (const auto& book : books) addMissing(book.path, book.title, book.author, typeForPath(book.path));
  }
  if (missing.empty()) return;

  // One pass over the whole-card Library index (library 0).
  library::LibraryIndexFile index;
  if (!index.open("/.crosspoint/library.idx")) {
    indexMissing = true;
    LOG_ERR("UNIFY", "No Library index to match against");
    return;
  }
  std::vector<std::string> titleKeys;
  std::vector<std::string> authorKeys;
  titleKeys.reserve(missing.size());
  authorKeys.reserve(missing.size());
  for (const auto& candidate : missing) {
    titleKeys.push_back(matchKey(candidate.title));
    authorKeys.push_back(matchKey(candidate.author));
  }
  std::string title;
  std::string displayAuthor;
  std::string author;
  std::string path;
  for (uint16_t ordinal = 0; ordinal < index.bookCount(); ++ordinal) {
    library::ClixRecord record{};
    // Display text falls back to the file name when the book has no metadata
    // title (TXT, XTC, EPUBs without dc:title, or metadata turned off).
    if (!index.readRecord(ordinal, record) || !index.readDisplayText(record, title, displayAuthor)) continue;
    const std::string titleKey = matchKey(title);
    for (size_t i = 0; i < missing.size(); ++i) {
      if (!missing[i].newPath.empty() || titleKeys[i] != titleKey) continue;
      if (!index.readSourceAuthor(record, author) || matchKey(author) != authorKeys[i]) continue;
      if (!index.readPath(record, path) || !Storage.exists(path.c_str()) ||
          typeForPath(path) != missing[i].bookType) {
        continue;
      }
      missing[i].newPath = path;
    }
  }
  index.close();
  for (auto& candidate : missing) {
    if (!candidate.newPath.empty()) candidates.push_back(std::move(candidate));
  }
}

bool UnifyBookmarksActivity::merge(const Candidate& candidate) {
  bool ok = true;
  ok &= BookmarkStore::migrateForFilePath(candidate.oldPath, candidate.newPath, candidate.title, candidate.author,
                                          candidate.bookType);
  ok &= BookmarkStore::migrateForFilePath(candidate.oldPath, candidate.newPath, candidate.title, candidate.author,
                                          candidate.bookType, DOGEARS_STORE_DIR);
  if (candidate.bookType == "epub") {
    ok &= ClippingStore::migrateForFilePath(candidate.oldPath, candidate.newPath, candidate.title, candidate.author,
                                            "epub");
    // Reading progress lives in the book cache, keyed by path: move it when
    // the new location has none yet.
    const std::string oldCache = Epub::cachePathForFilePath(candidate.oldPath, "/.crosspoint");
    const std::string newCache = Epub::cachePathForFilePath(candidate.newPath, "/.crosspoint");
    if (Storage.exists(oldCache.c_str()) && !Storage.exists(newCache.c_str())) {
      if (!Storage.rename(oldCache.c_str(), newCache.c_str())) {
        LOG_ERR("UNIFY", "Cache not moved: %s", oldCache.c_str());
      } else {
        (void)RECENT_BOOKS.updatePath(candidate.oldPath, candidate.newPath, oldCache, newCache);
      }
    }
  }
  Favorites::renamePath(candidate.oldPath, candidate.newPath);
  if (!ok) LOG_ERR("UNIFY", "Merge incomplete: %s -> %s", candidate.oldPath.c_str(), candidate.newPath.c_str());
  return ok;
}

void UnifyBookmarksActivity::promptNext() {
  if (nextCandidate >= candidates.size()) {
    state = State::Done;
    requestUpdate();
    return;
  }
  state = State::Prompting;
  const Candidate& candidate = candidates[nextCandidate];
  const std::string body = candidate.title + "\n" + candidate.oldPath + "\n-> " + candidate.newPath;
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_UNIFY_MERGE_QUESTION), body);
  if (!confirmation) {
    LOG_ERR("UNIFY", "OOM: confirmation");
    state = State::Done;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
    if (nextCandidate < candidates.size()) {
      if (result.isCancelled) {
        ++ignored;
      } else if (merge(candidates[nextCandidate])) {
        ++merged;
      }
      ++nextCandidate;
    }
    promptNext();
  });
}

void UnifyBookmarksActivity::loop() {
  if (state == State::Searching) {
    if (!searchStarted) {
      // Let the "Looking for moved books" screen paint before the scan.
      searchStarted = true;
      return;
    }
    findCandidates();
    promptNext();
    return;
  }
  if (state == State::Done && (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
                               mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    finish();
  }
}

void UnifyBookmarksActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_UNIFY_BOOKMARKS));
  const int y = renderer.getScreenHeight() / 2;
  if (state == State::Searching || state == State::Prompting) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_UNIFY_SEARCHING));
  } else if (indexMissing) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_UNIFY_NO_INDEX));
  } else if (candidates.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_UNIFY_NOTHING));
  } else {
    char line[64];
    snprintf(line, sizeof(line), tr(STR_UNIFY_RESULT), merged, ignored);
    renderer.drawCenteredText(UI_10_FONT_ID, y, line);
  }
  if (state == State::Done) {
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_DONE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
