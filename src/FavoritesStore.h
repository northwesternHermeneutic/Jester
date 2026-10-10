#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Jester (feature 1): books the user marked as favorites, shown in their own
// Home section. Stored in /.crosspoint/favorites.json and read on demand: no
// list stays in RAM between screens, and a file that exists but cannot be read
// is never overwritten (an add or remove fails instead).
struct FavoriteBook {
  std::string path;
  std::string title;
  std::string author;
  uint32_t addedSeq = 0;  // monotonic; larger = added later
};

namespace Favorites {

// Twice the recent-books capacity (RecentBooksStore::MAX_RECENT_BOOKS = 18).
constexpr size_t MAX_FAVORITES = 36;

enum class Sort : uint8_t { RecentlyAdded = 0, Title = 1, Author = 2, Count };

enum class ToggleResult : uint8_t { Added, Removed, Full, Failed };

const char* filePath();

// Loads every favorite. Returns false only when the file exists but cannot be
// read; a missing file is an empty list.
bool load(std::vector<FavoriteBook>& out, Sort* sortOut = nullptr);
bool contains(const std::string& path);
ToggleResult toggle(const std::string& path, const std::string& title, const std::string& author);
bool remove(const std::string& path);
bool setSort(Sort sort);
// Best effort, used by on-device renames and moves (BookMoveUtils).
void renamePath(const std::string& oldPath, const std::string& newPath);
// Refreshes title and author of a favorite when a book is opened; writes only on change.
void updateMetadata(const std::string& path, const std::string& title, const std::string& author);

void sortBooks(std::vector<FavoriteBook>& books, Sort sort);
// Title for a book that is not open: recent books first, then the file name.
std::string fallbackTitle(const std::string& path);

}  // namespace Favorites
