#include "FavoritesStore.h"

#include <ArduinoJson.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <algorithm>
#include <cctype>

#include "RecentBooksStore.h"

namespace Favorites {
namespace {

constexpr uint8_t FILE_VERSION = 1;

struct FileState {
  std::vector<FavoriteBook> books;
  uint32_t nextSeq = 1;
  Sort sort = Sort::RecentlyAdded;
};

bool fileExists() {
  const std::string backup = std::string(filePath()) + ".bak";
  return Storage.exists(filePath()) || Storage.exists(backup.c_str());
}

// False when an existing file cannot be read, so callers never overwrite it.
bool readState(FileState& state) {
  state = FileState{};
  if (!fileExists()) return true;
  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(filePath(), doc)) {
    LOG_ERR("FAV", "Cannot read %s", filePath());
    return false;
  }
  if ((doc["version"] | 0) != FILE_VERSION) {
    LOG_ERR("FAV", "Unsupported favorites version");
    return false;
  }
  state.nextSeq = doc["nextSeq"] | 1u;
  const uint8_t sort = doc["sort"] | 0;
  state.sort = sort < static_cast<uint8_t>(Sort::Count) ? static_cast<Sort>(sort) : Sort::RecentlyAdded;
  const JsonArrayConst books = doc["books"].as<JsonArrayConst>();
  state.books.reserve(std::min<size_t>(books.size(), MAX_FAVORITES));
  for (const JsonObjectConst book : books) {
    if (state.books.size() >= MAX_FAVORITES) break;
    FavoriteBook entry;
    entry.path = book["p"] | "";
    if (entry.path.empty()) continue;
    entry.title = book["t"] | "";
    entry.author = book["a"] | "";
    entry.addedSeq = book["s"] | 0u;
    state.books.push_back(std::move(entry));
  }
  return true;
}

bool writeState(const FileState& state) {
  JsonDocument doc;
  doc["version"] = FILE_VERSION;
  doc["nextSeq"] = state.nextSeq;
  doc["sort"] = static_cast<uint8_t>(state.sort);
  JsonArray books = doc["books"].to<JsonArray>();
  for (const auto& entry : state.books) {
    JsonObject book = books.add<JsonObject>();
    book["p"] = entry.path;
    book["t"] = entry.title;
    book["a"] = entry.author;
    book["s"] = entry.addedSeq;
  }
  if (!PersistableStoreBase::writeDocToFileAtomically(filePath(), doc)) {
    LOG_ERR("FAV", "Cannot write %s", filePath());
    return false;
  }
  return true;
}

std::string lowerCopy(const std::string& text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(), [](const unsigned char c) { return std::tolower(c); });
  return out;
}

}  // namespace

const char* filePath() { return "/.crosspoint/favorites.json"; }

bool load(std::vector<FavoriteBook>& out, Sort* sortOut) {
  FileState state;
  if (!readState(state)) {
    out.clear();
    return false;
  }
  if (sortOut) *sortOut = state.sort;
  sortBooks(state.books, state.sort);
  out = std::move(state.books);
  return true;
}

bool contains(const std::string& path) {
  FileState state;
  if (!readState(state)) return false;
  return std::any_of(state.books.begin(), state.books.end(), [&](const FavoriteBook& b) { return b.path == path; });
}

ToggleResult toggle(const std::string& path, const std::string& title, const std::string& author) {
  FileState state;
  if (path.empty() || !readState(state)) return ToggleResult::Failed;
  const auto it =
      std::find_if(state.books.begin(), state.books.end(), [&](const FavoriteBook& b) { return b.path == path; });
  if (it != state.books.end()) {
    state.books.erase(it);
    return writeState(state) ? ToggleResult::Removed : ToggleResult::Failed;
  }
  if (state.books.size() >= MAX_FAVORITES) return ToggleResult::Full;
  FavoriteBook entry;
  entry.path = path;
  entry.title = title.empty() ? fallbackTitle(path) : title;
  entry.author = author;
  entry.addedSeq = state.nextSeq++;
  state.books.push_back(std::move(entry));
  return writeState(state) ? ToggleResult::Added : ToggleResult::Failed;
}

bool remove(const std::string& path) {
  if (!fileExists()) return true;
  FileState state;
  if (!readState(state)) return false;
  const auto before = state.books.size();
  state.books.erase(std::remove_if(state.books.begin(), state.books.end(),
                                   [&](const FavoriteBook& b) { return b.path == path; }),
                    state.books.end());
  if (state.books.size() == before) return true;
  return writeState(state);
}

bool removeUnder(const std::string& folder) {
  if (folder.empty() || !fileExists()) return true;
  const std::string prefix = folder.back() == '/' ? folder : folder + "/";
  FileState state;
  if (!readState(state)) return false;
  const auto before = state.books.size();
  state.books.erase(std::remove_if(state.books.begin(), state.books.end(),
                                   [&](const FavoriteBook& b) { return b.path.rfind(prefix, 0) == 0; }),
                    state.books.end());
  if (state.books.size() == before) return true;
  return writeState(state);
}

bool setSort(const Sort sort) {
  FileState state;
  if (!readState(state)) return false;
  if (state.sort == sort) return true;
  state.sort = sort;
  return writeState(state);
}

void renamePath(const std::string& oldPath, const std::string& newPath) {
  if (!fileExists()) return;
  FileState state;
  if (!readState(state)) return;
  bool changed = false;
  for (auto& book : state.books) {
    if (book.path == oldPath) {
      book.path = newPath;
      changed = true;
    }
  }
  if (changed && !writeState(state)) LOG_ERR("FAV", "Favorite not repointed: %s", oldPath.c_str());
}

void updateMetadata(const std::string& path, const std::string& title, const std::string& author) {
  if (!fileExists() || title.empty()) return;
  FileState state;
  if (!readState(state)) return;
  for (auto& book : state.books) {
    if (book.path != path) continue;
    if (book.title == title && book.author == author) return;
    book.title = title;
    book.author = author;
    (void)writeState(state);
    return;
  }
}

void sortBooks(std::vector<FavoriteBook>& books, const Sort sort) {
  switch (sort) {
    case Sort::Title:
      std::stable_sort(books.begin(), books.end(), [](const FavoriteBook& a, const FavoriteBook& b) {
        return lowerCopy(a.title) < lowerCopy(b.title);
      });
      break;
    case Sort::Author:
      std::stable_sort(books.begin(), books.end(), [](const FavoriteBook& a, const FavoriteBook& b) {
        const std::string authorA = lowerCopy(a.author);
        const std::string authorB = lowerCopy(b.author);
        if (authorA != authorB) return authorA < authorB;
        return lowerCopy(a.title) < lowerCopy(b.title);
      });
      break;
    case Sort::RecentlyAdded:
    default:
      std::stable_sort(books.begin(), books.end(),
                       [](const FavoriteBook& a, const FavoriteBook& b) { return a.addedSeq > b.addedSeq; });
      break;
  }
}

std::string fallbackTitle(const std::string& path) {
  for (const auto& book : RECENT_BOOKS.getBooks()) {
    if (book.path == path && !book.title.empty()) return book.title;
  }
  const size_t slash = path.find_last_of('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0) name.resize(dot);
  return name;
}

}  // namespace Favorites
