#pragma once

#include <map>
#include <string>
#include <vector>

#include "HalStorage.h"

struct FakeMetadata {
  std::string title = "Title";
  std::string author = "Author";
  bool success = true;
};

inline std::map<std::string, FakeMetadata> bookMetadata;
inline std::map<std::string, FakeMetadata> cachedBookMetadata;
inline std::vector<bool> metadataCacheUse;

class Epub {
  std::string path;

 public:
  Epub(const std::string& path, const char*) : path(path) {}

  bool loadMetadata(std::string& title, std::string& author, const bool allowCachedMetadata = true) {
    ++fake::parses;
    metadataCacheUse.push_back(allowCachedMetadata);
    const auto cached = cachedBookMetadata.find(path);
    const auto& metadata =
        allowCachedMetadata && cached != cachedBookMetadata.end() ? cached->second : bookMetadata[path];
    if (!metadata.success) return false;
    title = metadata.title;
    author = metadata.author;
    return true;
  }
};
