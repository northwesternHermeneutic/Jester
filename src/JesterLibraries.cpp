#include "JesterLibraries.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <algorithm>

#include "CrossPointSettings.h"

namespace JesterLibraries {
namespace {

constexpr char FILE_PATH[] = "/.crosspoint/jester-libraries.json";
constexpr uint8_t FILE_VERSION = 1;

struct State {
  uint8_t active = 0;
  std::vector<Library> libraries;  // user libraries only (slots 1..)
  std::vector<std::string> excluded;
};

bool fileExists() {
  const std::string backup = std::string(FILE_PATH) + ".bak";
  return Storage.exists(FILE_PATH) || Storage.exists(backup.c_str());
}

bool readState(State& state) {
  state = State{};
  if (!fileExists()) return true;
  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(FILE_PATH, doc) || (doc["version"] | 0) != FILE_VERSION) {
    LOG_ERR("JLIB", "Cannot read %s", FILE_PATH);
    return false;
  }
  for (const JsonObjectConst entry : doc["libraries"].as<JsonArrayConst>()) {
    Library library;
    library.slot = entry["s"] | 0;
    library.name = entry["n"] | "";
    library.root = entry["r"] | "";
    if (library.slot == 0 || library.slot >= library::MAX_LIBRARIES || library.root.empty()) continue;
    state.libraries.push_back(std::move(library));
  }
  for (const JsonVariantConst folder : doc["excluded"].as<JsonArrayConst>()) {
    const char* path = folder | "";
    if (path[0] != '\0') state.excluded.emplace_back(path);
  }
  const uint8_t active = doc["active"] | 0;
  const bool activeExists = active == 0 || std::any_of(state.libraries.begin(), state.libraries.end(),
                                                       [active](const Library& l) { return l.slot == active; });
  state.active = activeExists ? active : 0;
  return true;
}

bool writeState(const State& state) {
  JsonDocument doc;
  doc["version"] = FILE_VERSION;
  doc["active"] = state.active;
  JsonArray libraries = doc["libraries"].to<JsonArray>();
  for (const auto& library : state.libraries) {
    JsonObject entry = libraries.add<JsonObject>();
    entry["s"] = library.slot;
    entry["n"] = library.name;
    entry["r"] = library.root;
  }
  JsonArray excluded = doc["excluded"].to<JsonArray>();
  for (const auto& folder : state.excluded) excluded.add(folder);
  if (!PersistableStoreBase::writeDocToFileAtomically(FILE_PATH, doc)) {
    LOG_ERR("JLIB", "Cannot write %s", FILE_PATH);
    return false;
  }
  return true;
}

Library cardLibrary() {
  Library library;
  library.slot = 0;
  library.name = tr(STR_LIBRARY);
  library.root = "/";
  return library;
}

std::string folderName(const std::string& root) {
  const size_t slash = root.find_last_of('/');
  const std::string name = slash == std::string::npos ? root : root.substr(slash + 1);
  return name.empty() ? root : name;
}

}  // namespace

std::vector<Library> list() {
  State state;
  readState(state);
  std::vector<Library> out;
  out.reserve(state.libraries.size() + 1);
  out.push_back(cardLibrary());
  for (auto& library : state.libraries) out.push_back(std::move(library));
  return out;
}

Library active() {
  State state;
  if (readState(state)) {
    for (const auto& library : state.libraries) {
      if (library.slot == state.active) return library;
    }
  }
  return cardLibrary();
}

bool setActive(const uint8_t slot) {
  State state;
  if (!readState(state)) return false;
  if (slot != 0 && std::none_of(state.libraries.begin(), state.libraries.end(),
                                [slot](const Library& l) { return l.slot == slot; })) {
    return false;
  }
  if (state.active == slot) return true;
  state.active = slot;
  return writeState(state);
}

bool makeLibrary(const std::string& root, uint8_t& slotOut) {
  if (root.empty() || root == "/") {
    slotOut = 0;
    return true;
  }
  State state;
  if (!readState(state)) return false;
  for (const auto& library : state.libraries) {
    if (library.root == root) {
      slotOut = library.slot;
      return true;
    }
  }
  for (uint8_t slot = 1; slot < library::MAX_LIBRARIES; ++slot) {
    if (std::any_of(state.libraries.begin(), state.libraries.end(),
                    [slot](const Library& l) { return l.slot == slot; })) {
      continue;
    }
    // A stale index from an earlier library in this slot must not be reused.
    char path[40];
    snprintf(path, sizeof(path), "/.crosspoint/library%u.idx", static_cast<unsigned>(slot));
    if (Storage.exists(path)) Storage.remove(path);
    state.libraries.push_back({slot, folderName(root), root});
    slotOut = slot;
    return writeState(state);
  }
  return false;
}

bool removeLibrary(const uint8_t slot) {
  if (slot == 0) return false;
  State state;
  if (!readState(state)) return false;
  const auto before = state.libraries.size();
  state.libraries.erase(std::remove_if(state.libraries.begin(), state.libraries.end(),
                                       [slot](const Library& l) { return l.slot == slot; }),
                        state.libraries.end());
  if (state.libraries.size() == before) return true;
  if (state.active == slot) state.active = 0;
  char path[40];
  snprintf(path, sizeof(path), "/.crosspoint/library%u.idx", static_cast<unsigned>(slot));
  if (Storage.exists(path)) Storage.remove(path);
  return writeState(state);
}

bool isLibraryRoot(const std::string& root) {
  State state;
  if (!readState(state)) return false;
  return std::any_of(state.libraries.begin(), state.libraries.end(),
                     [&](const Library& l) { return l.root == root; });
}

bool isExcluded(const std::string& folder) {
  State state;
  if (!readState(state)) return false;
  return std::find(state.excluded.begin(), state.excluded.end(), folder) != state.excluded.end();
}

bool setExcluded(const std::string& folder, const bool excluded) {
  if (folder.empty() || folder == "/") return false;
  State state;
  if (!readState(state)) return false;
  const auto it = std::find(state.excluded.begin(), state.excluded.end(), folder);
  if (excluded == (it != state.excluded.end())) return true;
  if (excluded) {
    state.excluded.push_back(folder);
  } else {
    state.excluded.erase(it);
  }
  if (!writeState(state)) return false;
  library::setExcludedDirectories(state.excluded);
  library::invalidateLibraryIndex();
  return true;
}

void applyToBuilder() {
  State state;
  readState(state);
  library::setActiveLibrary(state.active);
  library::setExcludedDirectories(std::move(state.excluded));
  const uint8_t choice = SETTINGS.libraryIndexLimit < INDEX_LIMIT_COUNT ? SETTINGS.libraryIndexLimit
                                                                         : INDEX_LIMIT_COUNT - 1;
  const uint16_t limit = INDEX_LIMIT_VALUES[choice];
  static int lastLimit = -1;
  if (lastLimit >= 0 && lastLimit != limit) library::invalidateLibraryIndex();
  lastLimit = limit;
  library::setIndexLimit(limit);
}

}  // namespace JesterLibraries
