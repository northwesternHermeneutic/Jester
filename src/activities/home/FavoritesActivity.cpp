#include "FavoritesActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <string>

#include "BookActions.h"
#include "FileBrowserActionActivity.h"
#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "util/TwoLineName.h"

namespace fui = freeink::ui;

namespace {
constexpr unsigned long HOLD_MS = 1000;
constexpr fui::ActionId ACTION_ROW = 1;
}  // namespace

FavoritesActivity::FavoritesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Favorites", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void FavoritesActivity::reload() {
  loadFailed = !Favorites::load(books, &sort);
  if (books.empty()) {
    selectedIndex = 0;
  } else if (selectedIndex >= static_cast<int>(books.size())) {
    selectedIndex = static_cast<int>(books.size()) - 1;
  }
}

void FavoritesActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  uiReady = false;
  // A hold that opened this screen must not also count here.
  confirmHoldHandled = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  backHoldHandled = mappedInput.isPressed(MappedInputManager::Button::Back);
  reload();
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &FavoritesActivity::onRowEvent, this);
  app.setScreen(&FavoritesActivity::listScreen, this);
  requestUpdate();
}

void FavoritesActivity::onExit() {
  std::vector<FavoriteBook>().swap(books);
  Activity::onExit();
}

const char* FavoritesActivity::sortLabel() const {
  switch (sort) {
    case Favorites::Sort::Title:
      return tr(STR_LIBRARY_TITLE);
    case Favorites::Sort::Author:
      return tr(STR_LIBRARY_AUTHOR);
    case Favorites::Sort::RecentlyAdded:
    default:
      return tr(STR_LIBRARY_DATE_ADDED);
  }
}

void FavoritesActivity::cycleSort() {
  const auto next =
      static_cast<Favorites::Sort>((static_cast<uint8_t>(sort) + 1) % static_cast<uint8_t>(Favorites::Sort::Count));
  if (!Favorites::setSort(next)) {
    BookActions::drawToast(renderer, tr(STR_FAVORITES_SAVE_FAILED));
  }
  sort = next;
  Favorites::sortBooks(books, sort);
  selectedIndex = 0;
  topIndex = 0;
  requestUpdate();
}

void FavoritesActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, TouchHeaderBackButton::compactHeaderRect(renderer))) {
    onGoHome();
    return;
  }

  // Back: hold cycles the sort mode, a short press returns Home.
  if (!backHoldHandled && mappedInput.isPressed(MappedInputManager::Button::Back) &&
      mappedInput.getHeldTime() >= HOLD_MS) {
    backHoldHandled = true;
    cycleSort();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (backHoldHandled) {
      backHoldHandled = false;
      return;
    }
    onGoHome();
    return;
  }
  if (backHoldHandled && !mappedInput.isPressed(MappedInputManager::Button::Back)) backHoldHandled = false;

  if (!books.empty() && !confirmHoldHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= HOLD_MS) {
    confirmHoldHandled = true;
    showActionMenu(selectedIndex, true);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (confirmHoldHandled) {
      confirmHoldHandled = false;
      return;
    }
    openBook(selectedIndex);
    return;
  }
  if (confirmHoldHandled && !mappedInput.isPressed(MappedInputManager::Button::Confirm)) confirmHoldHandled = false;

  const int listSize = static_cast<int>(books.size());
  if (listSize == 0) return;

  if (uiReady) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }

  const auto moveSelection = [this, listSize](const int next) {
    selectedIndex = next;
    topIndex = followListSelection(selectedIndex, topIndex, visibleRows, listSize);
    requestUpdate();
  };
  buttonNavigator.onNextRelease(
      [this, listSize, &moveSelection] { moveSelection(ButtonNavigator::nextIndex(selectedIndex, listSize)); });
  buttonNavigator.onPreviousRelease(
      [this, listSize, &moveSelection] { moveSelection(ButtonNavigator::previousIndex(selectedIndex, listSize)); });
  buttonNavigator.onNextContinuous([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::nextPageIndex(selectedIndex, listSize, visibleRows));
  });
  buttonNavigator.onPreviousContinuous([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::previousPageIndex(selectedIndex, listSize, visibleRows));
  });
}

void FavoritesActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FavoritesActivity*>(user);
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->books.size())) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  if (event.longPress) {
    self->showActionMenu(self->selectedIndex, false);
    return;
  }
  self->openBook(self->selectedIndex);
}

void FavoritesActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<FavoritesActivity*>(user)->buildListScreen(screen);
}

void FavoritesActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(CompactHeader::contentTop(metrics)), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (books.empty()) {
    screen.centeredText(loadFailed ? tr(STR_FAVORITES_SAVE_FAILED) : tr(STR_NO_FAVORITES), screen.theme().bodyText);
    return;
  }
  const std::string sortLine = std::string(tr(STR_SORT)) + ": " + sortLabel();
  const auto sortRow = screen.take(fui::LayoutAnchor::Top, uiTarget.lineHeight(screen.theme().smallText.font) + 8)
                           .inset(fui::Insets{0, static_cast<int16_t>(metrics.contentSidePadding), 0,
                                              static_cast<int16_t>(metrics.contentSidePadding)});
  uiTarget.text(sortRow, sortLine.c_str(), screen.theme().smallText);

  // Jester (feature 18): two-line titles break long words at a hyphenation point.
  fui::TextStyle labelStyle = screen.theme().bodyText;
  labelStyle.bold = true;
  labelStyle.maxLines = 2;
  TwoLineName::UiMeasure measure{&uiTarget, labelStyle};
  const int labelWidth = TwoLineName::listLabelWidth(screen.body().width, -1, 0, 10, 0, 0);
  std::vector<std::string> titles(books.size());
  std::vector<fui::ListItem> items;
  items.reserve(books.size());
  for (size_t i = 0; i < books.size(); ++i) {
    titles[i] = TwoLineName::prebreak(books[i].title, labelWidth, &TwoLineName::measureUiText, &measure);
    fui::ListItem item;
    item.label = titles[i].c_str();
    if (!books[i].author.empty()) item.subtitle = books[i].author.c_str();
    item.actionValue = static_cast<int16_t>(i);
    items.push_back(item);
  }
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.action = ACTION_ROW;
  props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);
  props.labelText = screen.theme().bodyText;
  props.labelText.bold = true;
  props.labelText.maxLines = 2;  // feature 18
  const fui::Rect bounds = screen.body();
  const auto rows = configureUiList(props, screen.theme(), bounds, UiListRowType::WithSubtitle);
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(books.size()));
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void FavoritesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_FAVORITES));
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_FAVORITES));
  }
  uiReady = false;
  app.render();
  uiReady = true;

  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_HOME)),
                                            books.empty() ? "" : tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void FavoritesActivity::openBook(const int index) {
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  const std::string path = books[index].path;
  if (!Storage.exists(path.c_str())) {
    BookActions::drawToast(renderer, tr(STR_FAVORITE_MISSING));
    requestUpdate();
    return;
  }
  onSelectBook(path);
}

void FavoritesActivity::showActionMenu(const int index, const bool ignoreInitialConfirmRelease) {
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  const FavoriteBook entry = books[index];
  std::vector<FileBrowserActionActivity::MenuItem> items;
  items.push_back({FileBrowserAction::RemoveFromFavorites, StrId::STR_REMOVE_FROM_FAVORITES});
  auto menu = makeUniqueNoThrow<FileBrowserActionActivity>(renderer, mappedInput, entry.title, std::move(items),
                                                           ignoreInitialConfirmRelease);
  if (!menu) {
    LOG_ERR("FAV", "OOM: favorites action menu");
    return;
  }
  startActivityForResult(std::move(menu), [this, entry](const ActivityResult& result) {
    confirmHoldHandled = false;
    const auto* actionResult = std::get_if<FileBrowserActionResult>(&result.data);
    if (!result.isCancelled && actionResult &&
        static_cast<FileBrowserAction>(actionResult->action) == FileBrowserAction::RemoveFromFavorites) {
      if (!Favorites::remove(entry.path)) BookActions::drawToast(renderer, tr(STR_FAVORITES_SAVE_FAILED));
    }
    reload();
    requestUpdate();
  });
}
