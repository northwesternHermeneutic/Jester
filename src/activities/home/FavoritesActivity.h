#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <vector>

#include "FavoritesStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Jester (feature 1): the Favorites Home section. Rows show book titles.
// Confirm opens a book, holding Confirm offers removal, holding Back cycles the
// sort mode (recently added, title, author), Back returns Home.
class FavoritesActivity final : public Activity {
 public:
  FavoritesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::vector<FavoriteBook> books;
  Favorites::Sort sort = Favorites::Sort::RecentlyAdded;
  bool loadFailed = false;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool confirmHoldHandled = false;
  bool backHoldHandled = false;
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);

  void reload();
  void cycleSort();
  void openBook(int index);
  void showActionMenu(int index, bool ignoreInitialConfirmRelease);
  const char* sortLabel() const;
};
