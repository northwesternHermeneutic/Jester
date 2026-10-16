#include "DictionaryRegistry.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "Dictionary.h"

static constexpr size_t DICT_FILENAME_BUFFER_SIZE = 256;  // FAT LFN max (255) plus terminator.

// Candidate SD card root directories for dictionaries. Jester scans every one
// that exists (hidden first) and merges their dictionaries, so a card with
// both /.dictionaries and /dictionaries shows all of them.
static constexpr const char* DICT_ROOT_CANDIDATES[] = {
    "/.dictionaries",
    "/dictionaries",
};

void DictionaryRegistry::scanRoot(const std::string& root) {
  auto rootDir = Storage.open(root.c_str());
  if (!rootDir || !rootDir.isDirectory()) {
    LOG_ERR("DREG", "Could not open dictionary directory: %s", root.c_str());
    if (rootDir) rootDir.close();
    return;
  }

  rootDir.rewindDirectory();

  char name[DICT_FILENAME_BUFFER_SIZE];
  for (auto entry = rootDir.openNextFile(); entry; entry = rootDir.openNextFile()) {
    entry.getName(name, sizeof(name));

    if (!entry.isDirectory() || name[0] == '.') {
      entry.close();
      continue;
    }

    // Scan the subdirectory for .idx and .ifo files.
    // Folders with multiple .idx or multiple .ifo files are ambiguous and skipped.
    std::string subPath = root + "/" + name;
    entry.close();

    auto subDir = Storage.open(subPath.c_str());
    if (!subDir || !subDir.isDirectory()) {
      LOG_ERR("DREG", "Could not open dictionary folder: %s", subPath.c_str());
      if (subDir) subDir.close();
      continue;
    }

    subDir.rewindDirectory();
    char subName[DICT_FILENAME_BUFFER_SIZE];
    char foundStem[DICT_FILENAME_BUFFER_SIZE];
    foundStem[0] = '\0';
    bool ambiguous = false;
    bool foundDict = false;
    bool foundNestedDirectory = false;
    int ifoCount = 0;
    for (auto subEntry = subDir.openNextFile(); subEntry; subEntry = subDir.openNextFile()) {
      subEntry.getName(subName, sizeof(subName));

      // Skip macOS metadata files (AppleDouble resource forks, .DS_Store)
      if (strncmp(subName, "._", 2) == 0 || strcasecmp(subName, ".DS_Store") == 0) {
        subEntry.close();
        continue;
      }

      const size_t subLen = strlen(subName);
      const bool isDirectory = subEntry.isDirectory();
      const bool isIdx = !isDirectory && subLen > 4 && strcmp(subName + subLen - 4, ".idx") == 0;
      const bool isIfo = !isDirectory && subLen > 4 && strcmp(subName + subLen - 4, ".ifo") == 0;
      const bool isDict = !isDirectory && subLen > 5 && strcmp(subName + subLen - 5, ".dict") == 0;
      subEntry.close();

      if (isDirectory) foundNestedDirectory = true;
      if (isIfo) ifoCount++;
      if (isDict) foundDict = true;
      if (isIdx) {
        if (foundStem[0] != '\0') {
          // Second .idx found — folder is ambiguous, skip it.
          ambiguous = true;
          LOG_DBG("DREG", "Skipping %s: multiple .idx files found", name);
          break;
        }
        subName[subLen - 4] = '\0';  // strip ".idx" to get stem
        strncpy(foundStem, subName, sizeof(foundStem) - 1);
      }
    }
    subDir.close();

    if (!ambiguous && ifoCount > 1) {
      ambiguous = true;
      LOG_DBG("DREG", "Skipping %s: multiple .ifo files found", name);
    }

    if (!ambiguous && foundStem[0] != '\0' && foundDict) {
      // Jester: the same folder in both roots is one dictionary; the hidden
      // root is scanned first and wins.
      if (std::any_of(entries_.begin(), entries_.end(), [&](const DictionaryEntry& d) { return d.name == name; })) {
        LOG_DBG("DREG", "Skipping %s/%s: same folder already found in another root", root.c_str(), name);
        continue;
      }
      DictionaryEntry e;
      e.name = name;
      e.stem = foundStem;
      e.basePath = root + "/" + e.name + "/" + e.stem;
      entries_.push_back(std::move(e));
      LOG_DBG("DREG", "Found dictionary: %s/%s", name, foundStem);
    } else if (!ambiguous) {
      if (foundNestedDirectory) {
        LOG_DBG("DREG", "Skipping %s: missing%s%s at folder root; nested directories are not scanned", name,
                foundStem[0] == '\0' ? " .idx" : "", foundDict ? "" : " .dict");
      } else {
        LOG_DBG("DREG", "Skipping %s: missing%s%s at folder root", name, foundStem[0] == '\0' ? " .idx" : "",
                foundDict ? "" : " .dict");
      }
    }
  }

  rootDir.close();
}

bool DictionaryRegistry::discover() {
  entries_.clear();
  entries_.reserve(16);

  bool foundRoot = false;
  for (const char* candidate : DICT_ROOT_CANDIDATES) {
    char resolvedRoot[32];
    if (!FsHelpers::resolveRootDirectoryIgnoreCase(candidate, resolvedRoot, sizeof(resolvedRoot))) continue;
    foundRoot = true;
    scanRoot(resolvedRoot);
  }
  if (!foundRoot) {
    LOG_DBG("DREG", "No dictionary directory found on SD card");
    return false;
  }
  LOG_DBG("DREG", "Discovery complete: %d dictionaries", static_cast<int>(entries_.size()));

  // Sort alphabetically by folder name (case-insensitive — matches FileBrowserActivity).
  std::sort(entries_.begin(), entries_.end(), [](const DictionaryEntry& a, const DictionaryEntry& b) {
    const char* s1 = a.name.c_str();
    const char* s2 = b.name.c_str();
    while (*s1 && *s2) {
      char c1 = static_cast<char>(tolower(static_cast<unsigned char>(*s1)));
      char c2 = static_cast<char>(tolower(static_cast<unsigned char>(*s2)));
      if (c1 != c2) return c1 < c2;
      s1++;
      s2++;
    }
    return *s1 == '\0' && *s2 != '\0';
  });

  maybeAutoSelectDefaultDictionary();

  return !entries_.empty();
}

void DictionaryRegistry::clear() { std::vector<DictionaryEntry>().swap(entries_); }

int DictionaryRegistry::indexOf(const std::string& basePath) const {
  if (basePath.empty()) return -1;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].basePath == basePath) return static_cast<int>(i);
  }
  return -1;
}

void DictionaryRegistry::maybeAutoSelectDefaultDictionary() const {
  if (entries_.empty()) return;

  const std::string activePath = Dictionary::readConfiguredDictPath();
  if (activePath == entries_[0].basePath) return;
  if (activePath.empty() && Dictionary::hasGlobalDictPathFile()) return;
  if (!activePath.empty() && indexOf(activePath) >= 0) return;

  LOG_INF("DREG", "Auto-selecting first discovered dictionary: %s", entries_[0].basePath.c_str());
  Dictionary::saveGlobalDictPath(entries_[0].basePath.c_str());
}
