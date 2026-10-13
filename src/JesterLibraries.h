#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Jester (feature 19): user-made libraries and Library exclusions.
//
// Library slot 0 is the whole card ("/"). "Make Library" on a folder adds a
// library rooted there, named after the folder, in a free slot 1..7; each slot
// has its own index file (LibraryBuilder setActiveLibrary). Exclusions are full
// folder paths skipped by every library whose root contains them; a library
// rooted inside an excluded folder still indexes it (LibraryBuilder walk()).
// Stored in /.crosspoint/jester-libraries.json, read on demand.
namespace JesterLibraries {

struct Library {
  uint8_t slot = 0;
  std::string name;
  std::string root;
};

// Index Limit choices (Settings > System > Library Index Limit); 0 = All Books.
inline constexpr uint16_t INDEX_LIMIT_VALUES[] = {10, 25, 50, 75, 100, 200, 300, 400, 500, 0};
inline constexpr uint8_t INDEX_LIMIT_COUNT = sizeof(INDEX_LIMIT_VALUES) / sizeof(INDEX_LIMIT_VALUES[0]);

// All libraries, slot 0 first.
std::vector<Library> list();
Library active();
bool setActive(uint8_t slot);
// Adds a library for `root` (or returns the existing one). False when all
// slots are taken or the file cannot be written.
bool makeLibrary(const std::string& root, uint8_t& slotOut);
bool removeLibrary(uint8_t slot);
bool isLibraryRoot(const std::string& root);

bool isExcluded(const std::string& folder);
bool setExcluded(const std::string& folder, bool excluded);

// Pushes the active slot, exclusions and the Index Limit setting into the
// Library builder; marks indexes dirty when the limit changed.
void applyToBuilder();

// Library index state across the X4's sleep. On battery the X4's sleep is a
// power-off (GPIO13 low cuts the battery), so RAM and RTC memory are lost and
// every wake looked like a cold boot that must rescan. saveIndexStateForSleep()
// writes the dirty bits to the card as the last step before sleeping;
// restoreIndexStateAtBoot() reads and deletes that file on every boot, so only
// the boot right after an orderly sleep finds it. A reset, crash or battery
// pull leaves no file and still rescans. Books added with a computer while the
// device sleeps need Library > Refresh.
void saveIndexStateForSleep();
bool restoreIndexStateAtBoot(uint32_t& maskOut);

}  // namespace JesterLibraries
