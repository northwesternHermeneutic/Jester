#pragma once

#include <map>
#include <string>
#include <vector>

#include "HalStorage.h"

struct FakeMetadata {
  std::string title = "Title";
  std::string author = "Author";
  std::string series;
  std::string genre;
  bool success = true;
};

inline std::map<std::string, FakeMetadata> bookMetadata;
inline std::map<std::string, FakeMetadata> cachedBookMetadata;
inline std::vector<bool> metadataCacheUse;

class Epub {
  std::string path;

 public:
  Epub(const std::string& path, const char*) : path(path) {}

  bool loadMetadata(std::string& title, std::string& author, const bool allowCachedMetadata = true,
                    std::string* series = nullptr, std::string* genre = nullptr) {
    ++fake::parses;
    metadataCacheUse.push_back(allowCachedMetadata);
    const auto cached = cachedBookMetadata.find(path);
    const auto& metadata = allowCachedMetadata && !series && !genre && cached != cachedBookMetadata.end()
                               ? cached->second
                               : bookMetadata[path];
    if (!metadata.success) return false;
    title = metadata.title;
    author = metadata.author;
    if (series) *series = metadata.series;
    if (genre) *genre = metadata.genre;
    return true;
  }
};
