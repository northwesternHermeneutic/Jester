#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <LibraryIndexFile.h>

#include <memory>
#include <string>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

class LibraryActivity final : public Activity {
 public:
  LibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool blocksGlobalInput() const override { return sortPopup.isActive(); }

 private:
  enum class Sort : uint8_t { DateAdded, Title, AuthorLast, AuthorFirst, RecentlyRead };
  // Ring: refresh, search, settings, sort method, direction, then the book rows. All controls are
  // reachable on button-only devices as well as through SDK touch routing.
  static constexpr int CONTROL_COUNT = 5;
  using UiApp = freeink::ui::FreeInkApp<32, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  ButtonNavigator buttonNavigator;
  freeink::ui::ListNav listNav;
  OptionPopup sortPopup;
  library::LibraryIndexFile index;
  Sort sort = Sort::RecentlyRead;
  bool descending = true;
  int selection = CONTROL_COUNT;
  bool showSelection = true;
  int topIndex = 0;
  bool uiReady = false;
  bool longPressFired = false;
  bool ignoreConfirmRelease = false;
  bool scanFailed = false;
  bool filterFailed = false;
  bool pendingCacheDeletedFeedback = false;
  unsigned long cacheDeletedFeedbackShowTime = 0;
  std::string query;
  // Only searches allocate one u16 per indexed book (at most 8 KiB), fallibly.
  std::unique_ptr<uint16_t[]> filtered;
  uint16_t filteredCount = 0;
  uint16_t recentRows[RecentBooksStore::MAX_RECENT_BOOKS]{};
  size_t recentCount = 0;
  // SDK rowProvider consumes the strings before asking for the next row.
  // Reuse one row instead of retaining every title/author in the library.
  RecentBook rowScratch;
  std::string groupHeading;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onControlEvent(const freeink::ui::ActionEvent& event, void* user);
  static void provideRow(void* user, uint16_t row, freeink::ui::ListItem& item);
  void buildListScreen(UiApp::ScreenType& screen);
  void buildSortHeader(UiApp::ScreenType& screen);
  const char* sortLabel() const;
  library::SortOrder indexOrder() const;
  int rowCount() const;
  uint16_t ordinalForRow(int row);
  bool readBook(int row, RecentBook& book, bool fullPath = true);
  uint32_t groupForRow(int row);
  bool hasActiveFilter() const;
  bool rebuildIndex(bool showScanning);
  void resolveRecents();
  void applyFilter();
  void resetViewport();
  void reloadAfterBookAction();
  void openBook(int row);
  void openSortPicker();
  void refreshLibrary();
  void openSearch();
  void openSettings();
  void activateControl(int control);
  // Owns the allocation-failure check and render lock for every child result.
  void openDialog(std::unique_ptr<Activity>&& activity, ActivityResultHandler handler);
  void promptDeleteBook(const RecentBook& book);
  void promptRemoveBook(const std::string& path, const std::string& title);
  void showBookActionMenu(size_t bookIndex, bool ignoreInitialConfirmRelease = false);
};
