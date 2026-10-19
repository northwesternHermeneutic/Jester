#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <I18n.h>

#include <atomic>
#include <string>
#include <vector>

#include "../Activity.h"
#include "BookmarkStore.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

class EpubReaderBookmarkListActivity final : public Activity {
 public:
  EpubReaderBookmarkListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::vector<Bookmark>& bookmarks, bool shortBook = false,
                                 BookmarkStore* store = nullptr, StrId titleId = StrId::STR_BOOKMARKS,
                                 StrId emptyId = StrId::STR_NO_BOOKMARKS, std::string textFilePath = {});

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }

 private:
  bool shortBook = false;  // Jester (feature 2): whole-number percentages
  // Jester (feature 4): the same list shows dog ears (DOGEARS store, own titles).
  BookmarkStore* store = nullptr;
  StrId titleId = StrId::STR_BOOKMARKS;
  StrId emptyId = StrId::STR_NO_BOOKMARKS;
  std::vector<Bookmark> bookmarks;
  // Jester (feature 4): when the dog ears text file exists it is row 0, ahead
  // of the marks, and selecting it asks the caller to open it as text.
  std::string textFilePath;
  std::string textFileName;
  int fileRows = 0;
  int selectedIndex = 0;  // row; the mark is bookmarks[selectedIndex - fileRows]
  bool confirmingDelete = false;
  ButtonNavigator buttonNavigator;
  OptionPopup confirmPopup;
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  int rowCount() const { return static_cast<int>(bookmarks.size()) + fileRows; }
  // Index into bookmarks for the selected row, or -1 for the text file row.
  int selectedMark() const;
  void selectBookmark();

  void deleteSelectedBookmark();
  void showBookmarkDeletePopup();
};
