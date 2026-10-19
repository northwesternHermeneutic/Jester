#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <array>
#include <atomic>
#include <string>
#include <vector>

#include "ClippingStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderClippingListActivity final : public Activity {
 public:
  // textFilePath (Jester, feature 4): the clippings text file, listed first when it exists.
  EpubReaderClippingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 std::string textFilePath = {});

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string detailText;
  std::vector<std::string> detailLines;
  ButtonNavigator buttonNavigator;
  // Jester (feature 4): when the text file exists it is row 0, ahead of the
  // clippings, and selecting it asks the caller to open it as text.
  std::string textFilePath;
  std::string textFileName;
  int fileRows = 0;
  int selectedIndex = 0;  // row; the clipping is selectedIndex - fileRows
  int detailPage = 0;
  int detailLayoutWidth = 0;
  int detailLinesPerPage = 0;
  bool longPressConfirmHandled = false;
  bool detailMode = false;
  bool initialListRender = true;
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  // Caps the rendered row window: the label buffers below are fixed size, so the
  // visible-row count from the theme geometry must never exceed them.
  static constexpr size_t CLIPPING_WINDOW_SIZE = 20;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;
  std::vector<freeink::ui::ListItem> uiItems;
  std::array<std::string, CLIPPING_WINDOW_SIZE> uiLabels;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  int rowCount() const { return static_cast<int>(CLIPPINGS.clippingCount()) + fileRows; }
  bool fileRowSelected() const { return fileRows > 0 && selectedIndex == 0; }
  // Clipping index for the selected row, or -1 for the text file row.
  int selectedClippingIndex() const;
  void openTextFile();
  int getDetailTextWidth() const;
  int getDetailLinesPerPage() const;
  int getDetailPageCount() const;
  void deleteSelectedClipping();
  void closeDetail();
  void jumpToSelectedClipping();
  void openSelectedDetail();
  void rebuildDetailLayoutIfNeeded();
  void showClippingActionMenu(bool ignoreInitialConfirmRelease);
  void renderDetail();
};
