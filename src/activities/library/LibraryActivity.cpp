#include "LibraryActivity.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryFileTypes.h>
#include <LibraryRecentOrder.h>
#include <LibraryText.h>
#include <Memory.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>

#include "activities/home/BookActions.h"
#include "activities/home/FileBrowserActionActivity.h"
#include "activities/library/LibrarySettingsActivity.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/libraryIcons.h"
#include "components/icons/listIcons.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
constexpr fui::ActionId ACTION_CONTROL = 2;
constexpr unsigned long LONG_PRESS_MS = 1000;
constexpr unsigned long ACTION_FEEDBACK_MS = 1000;
constexpr int HEADER_CONTROL_SIZE = 44;
constexpr int HEADER_CONTROL_GAP = 10;

int headerControlRightInset() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  // Inline battery themes need a separate lane for the options button.
  return metrics.headerBatteryDetached || metrics.headerBatterySide == 1 ? 10 : metrics.batteryWidth + 80;
}
}  // namespace

LibraryActivity::LibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Library", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void LibraryActivity::onEnter() {
  RenderLock lock;
  Activity::onEnter();
  if (RECENT_BOOKS.pruneMissing()) RECENT_BOOKS.saveToFile();
  applySharedUiTheme(app, uiTarget);
  sort = SETTINGS.librarySortMethod <= static_cast<uint8_t>(Sort::RecentlyRead)
             ? static_cast<Sort>(SETTINGS.librarySortMethod)
             : Sort::RecentlyRead;
  descending = SETTINGS.librarySortDescending != 0;
  app.on(ACTION_ROW, &LibraryActivity::onRowEvent, this);
  app.on(ACTION_CONTROL, &LibraryActivity::onControlEvent, this);
  app.setScreen(&LibraryActivity::listScreen, this);
  // Reconcile on entry as card contents may change through USB, Wi-Fi or an
  // external card reader. Unchanged books reuse the index's metadata.
  rebuildIndex(!Storage.exists(library::libraryIndexPath()));
  resetViewport();
  ignoreConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate();
}

void LibraryActivity::onExit() {
  index.close();
  filtered.reset();
  Activity::onExit();
}

bool LibraryActivity::rebuildIndex(const bool showScanning) {
  uiReady = false;
  index.close();
  if (showScanning) GUI.drawPopup(renderer, tr(STR_LIBRARY_SCANNING));
  library::BuildStats stats;
  scanFailed = !library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0);
  if (scanFailed) LOG_ERR("LIB", "Library scan failed; retaining the previous index");
  if (!index.open(library::libraryIndexPath())) {
    LOG_ERR("LIB", "Cannot open library index");
    scanFailed = true;
  }
  resolveRecents();
  applyFilter();
  return !scanFailed;
}

void LibraryActivity::resolveRecents() {
  recentCount = 0;
  if (!index.isOpen()) return;
  const auto& books = RECENT_BOOKS.getBooks();
  // Index lookup accepts up to 16 entries per pass; the history holds 18.
  // Two small batches keep stack use below 256 bytes.
  constexpr size_t BATCH = 8;
  library::BookIdentity identities[BATCH]{};
  uint16_t rows[BATCH]{};
  for (size_t offset = 0; offset < books.size(); offset += BATCH) {
    const size_t count = std::min(BATCH, books.size() - offset);
    for (size_t i = 0; i < count; ++i) {
      const auto& path = books[offset + i].path;
      identities[i] = {library::clixPathHash(path.data(), path.size()), 0};
    }
    if (!index.recentRowsFor(identities, count, rows)) {
      LOG_ERR("LIB", "Cannot match reading history to the index");
      recentCount = 0;
      scanFailed = true;
      return;
    }
    for (size_t i = 0; i < count && recentCount < RecentBooksStore::MAX_RECENT_BOOKS; ++i) {
      if (rows[i] == UINT16_MAX || std::find(recentRows, recentRows + recentCount, rows[i]) != recentRows + recentCount)
        continue;
      recentRows[recentCount++] = rows[i];
    }
  }
}

library::SortOrder LibraryActivity::indexOrder() const {
  switch (sort) {
    case Sort::Title:
      return descending ? library::SortOrder::TitleDesc : library::SortOrder::TitleAsc;
    case Sort::AuthorLast:
      return descending ? library::SortOrder::AuthorDesc : library::SortOrder::AuthorAsc;
    case Sort::AuthorFirst:
      return descending ? library::SortOrder::AuthorFirstDesc : library::SortOrder::AuthorFirstAsc;
    case Sort::RecentlyRead:
      return library::SortOrder::RecentAsc;
    case Sort::DateAdded:
      return descending ? library::SortOrder::RecentDesc : library::SortOrder::RecentAsc;
  }
  return library::SortOrder::RecentDesc;
}

const char* LibraryActivity::sortLabel() const {
  switch (sort) {
    case Sort::Title:
      return tr(STR_LIBRARY_TITLE);
    case Sort::AuthorLast:
      return tr(STR_LIBRARY_AUTHOR_LAST_NAME);
    case Sort::AuthorFirst:
      return tr(STR_LIBRARY_AUTHOR_FIRST_NAME);
    case Sort::RecentlyRead:
      return tr(STR_LIBRARY_RECENTLY_OPENED);
    case Sort::DateAdded:
      return tr(STR_LIBRARY_DATE_ADDED);
  }
  return tr(STR_LIBRARY_DATE_ADDED);
}

bool LibraryActivity::hasActiveFilter() const {
  return !query.empty() || !SETTINGS.libraryShowEpub || !SETTINGS.libraryShowXtc || !SETTINGS.libraryShowTxt ||
         !SETTINGS.libraryShowMarkdown;
}

int LibraryActivity::rowCount() const { return hasActiveFilter() ? filteredCount : index.bookCount(); }

uint16_t LibraryActivity::ordinalForRow(const int row) {
  if (row < 0 || row >= rowCount()) return UINT16_MAX;
  if (hasActiveFilter()) return filtered ? filtered[row] : UINT16_MAX;
  uint16_t indexRow = static_cast<uint16_t>(row);
  if (sort == Sort::RecentlyRead) {
    indexRow = library::recentShelfRow(indexRow, index.bookCount(), recentRows, recentCount, descending);
  }
  return index.ordinalForRow(indexOrder(), indexRow);
}

bool LibraryActivity::readBook(const int row, RecentBook& book, const bool fullPath) {
  library::ClixRecord record{};
  const auto ordinal = ordinalForRow(row);
  if (ordinal == UINT16_MAX || !index.readRecord(ordinal, record) ||
      !index.readDisplayText(record, book.title, book.author) ||
      !(fullPath ? index.readPath(record, book.path) : index.readName(record, book.path))) {
    LOG_ERR("LIB", "Cannot read book row %d", row);
    return false;
  }
  return true;
}

void LibraryActivity::applyFilter() {
  filteredCount = 0;
  filterFailed = false;
  filtered.reset();
  if (!hasActiveFilter() || !index.isOpen() || index.bookCount() == 0) return;
  filtered = makeUniqueNoThrow<uint16_t[]>(index.bookCount());
  if (!filtered) {
    LOG_ERR("LIB", "Cannot allocate Library search results");
    filterFailed = true;
    return;
  }
  const std::string needle = library::fold(query);
  const uint8_t visibleTypes =
      (SETTINGS.libraryShowEpub ? library::FileEpub : 0) | (SETTINGS.libraryShowXtc ? library::FileXtc : 0) |
      (SETTINGS.libraryShowTxt ? library::FileTxt : 0) | (SETTINGS.libraryShowMarkdown ? library::FileMarkdown : 0);
  std::string title;
  std::string author;
  std::string name;
  std::string combined;
  std::string folded;
  // Blob fields are byte-length-prefixed. Reserve once for the entire scan,
  // avoiding concat/fold allocations for each of up to 4,096 books.
  title.reserve(UINT8_MAX);
  author.reserve(UINT8_MAX);
  name.reserve(UINT8_MAX);
  combined.reserve(2 * UINT8_MAX + 1);
  folded.reserve(2 * UINT8_MAX + 1);
  for (uint16_t row = 0; row < index.bookCount(); ++row) {
    const uint16_t indexRow = sort == Sort::RecentlyRead
                                  ? library::recentShelfRow(row, index.bookCount(), recentRows, recentCount, descending)
                                  : row;
    const uint16_t ordinal = index.ordinalForRow(indexOrder(), indexRow);
    library::ClixRecord record{};
    if (ordinal == UINT16_MAX || !index.readRecord(ordinal, record) || !index.readName(record, name)) {
      LOG_ERR("LIB", "Cannot read Library search data");
      filterFailed = true;
      filteredCount = 0;
      break;
    }
    if ((library::fileTypeFor(name) & visibleTypes) == 0) continue;
    if (!needle.empty() && !index.readDisplayText(record, title, author)) {
      LOG_ERR("LIB", "Cannot read Library search text");
      filterFailed = true;
      filteredCount = 0;
      break;
    }
    if (!needle.empty()) {
      combined.assign(title);
      combined.push_back(' ');
      combined.append(author);
      library::foldInto(combined, folded);
    }
    if (needle.empty() || library::matchesQuery(folded, needle)) {
      filtered[filteredCount++] = ordinal;
    }
    if ((row & 31) == 31) delay(1);
  }
}

void LibraryActivity::resetViewport() {
  selection = rowCount() ? CONTROL_COUNT : 3;
  showSelection = !mappedInput.hasTouchHardware();
  topIndex = 0;
  listNav.reset(selection - CONTROL_COUNT);
  uiReady = false;
}

void LibraryActivity::reloadAfterBookAction() {
  rebuildIndex(false);
  selection = std::min(selection, std::max(CONTROL_COUNT, CONTROL_COUNT + rowCount() - 1));
  listNav.selected = selection - CONTROL_COUNT;
  listNav.top = topIndex;
  listNav.follow(rowCount());
  topIndex = listNav.top;
  requestUpdate();
}

void LibraryActivity::openDialog(std::unique_ptr<Activity>&& child, ActivityResultHandler handler) {
  if (!child) {
    LOG_ERR("LIB", "Cannot allocate Library dialog");
    return;
  }
  app.clearTapFlash();
  startActivityForResult(std::move(child), [this, handler = std::move(handler)](const ActivityResult& result) {
    RenderLock lock;
    ignoreConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
    longPressFired = false;
    uiReady = false;
    handler(result);
    requestUpdate();
  });
}

void LibraryActivity::openBook(const int row) {
  RecentBook book;
  if (!readBook(row, book)) return;
  app.clearTapFlash();
  index.close();
  onSelectBook(book.path);
}

void LibraryActivity::openSortPicker() {
  static constexpr StrId choices[] = {StrId::STR_LIBRARY_DATE_ADDED, StrId::STR_LIBRARY_TITLE,
                                      StrId::STR_LIBRARY_AUTHOR_LAST_NAME, StrId::STR_LIBRARY_AUTHOR_FIRST_NAME,
                                      StrId::STR_LIBRARY_RECENTLY_OPENED};
  sortPopup.setDismissOnOutsideTouchDown(true);
  sortPopup.show(StrId::STR_LIBRARY_SORT_BY, choices, 5, static_cast<int>(sort), [this](const int selected) {
    if (selected < 0 || selected > static_cast<int>(Sort::RecentlyRead)) return;
    sort = static_cast<Sort>(selected);
    descending = sort == Sort::DateAdded || sort == Sort::RecentlyRead;
    SETTINGS.librarySortMethod = static_cast<uint8_t>(sort);
    SETTINGS.librarySortDescending = descending;
    if (!SETTINGS.saveToFile()) LOG_ERR("LIB", "Cannot save Library sort");
    applyFilter();
    resetViewport();
  });
  requestUpdate();
}

void LibraryActivity::openSearch() {
  openDialog(
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SEARCH), query, 48, InputType::Text),
      [this](const ActivityResult& result) {
        const auto* text = std::get_if<KeyboardResult>(&result.data);
        if (result.isCancelled || !text) return;
        query = text->text;
        applyFilter();
        resetViewport();
      });
}

void LibraryActivity::refreshLibrary() {
  rebuildIndex(true);
  resetViewport();
  requestUpdate();
}

void LibraryActivity::openSettings() {
  openDialog(makeUniqueNoThrow<LibrarySettingsActivity>(renderer, mappedInput), [this](const ActivityResult&) {
    applyFilter();
    resetViewport();
  });
}

void LibraryActivity::activateControl(const int control) {
  app.clearTapFlash();
  if (control == 0) refreshLibrary();
  if (control == 1) openSearch();
  if (control == 2) openSettings();
  if (control == 3) openSortPicker();
  if (control == 4) {
    descending = !descending;
    SETTINGS.librarySortDescending = descending;
    if (!SETTINGS.saveToFile()) LOG_ERR("LIB", "Cannot save Library sort direction");
    applyFilter();
    topIndex = 0;
    requestUpdate();
  }
}

void LibraryActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<LibraryActivity*>(user);
  if (event.value < 0 || event.value >= self->rowCount()) return;
  self->selection = event.value + CONTROL_COUNT;
  self->showSelection = false;
  if (event.longPress)
    self->showBookActionMenu(event.value);
  else
    self->openBook(event.value);
}

void LibraryActivity::onControlEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<LibraryActivity*>(user);
  self->selection = event.value;
  self->showSelection = false;
  self->activateControl(event.value);
}

void LibraryActivity::loop() {
  RenderLock lock;
  if (sortPopup.isActive()) {
    sortPopup.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }
  if (pendingCacheDeletedFeedback && millis() - cacheDeletedFeedbackShowTime >= ACTION_FEEDBACK_MS) {
    pendingCacheDeletedFeedback = false;
    requestUpdate();
  }
  if (ignoreConfirmRelease || longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      ignoreConfirmRelease = false;
      longPressFired = false;
    }
    return;
  }
  int tapX = 0;
  int tapY = 0;
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.wasScreenTapped(tapX, tapY) && tapY < header.y + header.height &&
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    onGoHome();
    return;
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressFired = true;
    if (selection >= CONTROL_COUNT && rowCount() > 0)
      showBookActionMenu(selection - CONTROL_COUNT, true);
    else
      activateControl(selection);
    return;
  }
  if (uiReady) {
    const auto snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selection < CONTROL_COUNT)
      activateControl(selection);
    else
      openBook(selection - CONTROL_COUNT);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (!query.empty()) {
      query.clear();
      applyFilter();
      resetViewport();
      requestUpdate();
    } else
      onGoHome();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    if (mappedInput.hasTouchHardware()) showSelection = false;
    listNav.top = topIndex;
    const int page = listNav.pageRowsFor(rowCount());
    listNav.scrollBy(swipe == MappedInputManager::SwipeDir::Up ? page : -page, rowCount());
    topIndex = listNav.top;
    requestUpdate();
    return;
  }
  const int count = rowCount() + CONTROL_COUNT;
  const auto move = [this](const int next) {
    if (!showSelection) {
      showSelection = true;
      requestUpdate();
      return;
    }
    selection = next;
    if (selection >= CONTROL_COUNT) {
      listNav.selected = selection - CONTROL_COUNT;
      listNav.top = topIndex;
      listNav.follow(rowCount());
      topIndex = listNav.top;
    }
    requestUpdate();
  };
  buttonNavigator.onNextRelease([&] { move(ButtonNavigator::nextIndex(selection, count)); });
  buttonNavigator.onPreviousRelease([&] { move(ButtonNavigator::previousIndex(selection, count)); });
  buttonNavigator.onNextContinuous(
      [&] { move(ButtonNavigator::nextPageIndex(selection, count, listNav.pageRowsFor(rowCount()))); });
  buttonNavigator.onPreviousContinuous(
      [&] { move(ButtonNavigator::previousPageIndex(selection, count, listNav.pageRowsFor(rowCount()))); });
}

void LibraryActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<LibraryActivity*>(user)->buildListScreen(screen);
}

void LibraryActivity::provideRow(void* user, const uint16_t row, fui::ListItem& item) {
  auto* self = static_cast<LibraryActivity*>(user);
  if (!self->readBook(row, self->rowScratch, false)) {
    item.label = tr(STR_LIBRARY_UNAVAILABLE);
    item.enabled = false;
    return;
  }
  item.label = self->rowScratch.title.c_str();
  if (!self->rowScratch.author.empty()) item.subtitle = self->rowScratch.author.c_str();
  item.icon = listIconFor(UITheme::getFileIcon(self->rowScratch.path), 32);
  item.actionValue = static_cast<int16_t>(row);
  if (SETTINGS.libraryListExpanded && self->sort != Sort::DateAdded && self->sort != Sort::RecentlyRead) {
    const uint32_t initial = self->groupForRow(row);
    if (row == 0 || initial != self->groupForRow(row - 1)) {
      self->groupHeading.clear();
      utf8AppendCodepoint(initial ? (initial >= 'a' && initial <= 'z' ? initial - 'a' + 'A' : initial) : '#',
                          self->groupHeading);
      item.sectionHeading = self->groupHeading.c_str();
    }
  }
}

uint32_t LibraryActivity::groupForRow(const int row) {
  library::ClixRecord record{};
  if (!index.readRecord(ordinalForRow(row), record)) return 0;
  std::string key;
  if (sort == Sort::Title) {
    key.assign(record.fold, record.foldLen);
  } else {
    std::string author;
    if (!index.readAuthor(record, author)) return 0;
    key = library::fold(sort == Sort::AuthorLast ? library::surnameKey(author) : author);
  }
  return library::foldedGroupInitial(key);
}

void LibraryActivity::buildSortHeader(UiApp::ScreenType& screen) {
  const int16_t bandHeight = std::max<int16_t>(44, screen.theme().rowHeight);
  const auto band = screen.take(fui::LayoutAnchor::Top, bandHeight);
  const int16_t margin = 10;
  auto labelStyle = screen.theme().bodyText;
  labelStyle.bold = true;
  const int16_t labelWidth = std::min<int16_t>(
      band.width / 3, uiTarget.measureText(labelStyle.font, tr(STR_LIBRARY_SORT_BY), labelStyle).width);
  // The 32px glyph sits inside a wider selection box with even side padding.
  const int16_t iconWidth = 48;
  const int16_t methodWidth = std::min<int16_t>(
      band.width - labelWidth - iconWidth - 4 * margin,
      std::max<int16_t>(
          (sort == Sort::AuthorLast || sort == Sort::AuthorFirst) ? 220 : 0,
          uiTarget.measureText(screen.theme().bodyText.font, sortLabel(), screen.theme().bodyText).width + 20));
  const int16_t gap = std::max<int16_t>(0, (band.width - 2 * margin - labelWidth - methodWidth - iconWidth) / 2);
  const fui::Rect label{static_cast<int16_t>(band.x + margin), band.y, labelWidth, band.height};
  const fui::Rect method{static_cast<int16_t>(label.right() + gap), band.y, methodWidth, band.height};
  const fui::Rect direction{static_cast<int16_t>(band.right() - margin - iconWidth), band.y, iconWidth, band.height};
  uiTarget.text(label, tr(STR_LIBRARY_SORT_BY), labelStyle);
  // Each target owns half of the adjacent whitespace. SDK buttons provide
  // their draw/selection state; explicit hit regions avoid enlarged overlap.
  fui::ButtonProps button;
  button.label = sortLabel();
  button.text = screen.theme().bodyText;
  button.styles = fui::plainStyles();
  button.styles.selected = screen.theme().button.selected;
  button.state = showSelection && selection == 3 ? fui::StateSelected : fui::StateNormal;
  screen.button(button, method);
  button.label = nullptr;
  button.icon = fui::bitmapFromIcon(descending ? icon_arrow_down_wide_narrow_32 : icon_arrow_up_wide_narrow_32);
  button.state = showSelection && selection == 4 ? fui::StateSelected : fui::StateNormal;
  screen.button(button, direction);
  const int16_t split = static_cast<int16_t>((method.right() + direction.x) / 2);
  screen.frame().hit(fui::Rect{band.x, band.y, static_cast<int16_t>(split - band.x), band.height}, ACTION_CONTROL, 3,
                     fui::InputTouch);
  screen.frame().hit(fui::Rect{split, band.y, static_cast<int16_t>(band.right() - split), band.height}, ACTION_CONTROL,
                     4, fui::InputTouch);
  uiTarget.fill(fui::Rect{band.x, band.y, band.width, 1}, fui::Paint::solid(fui::Color::Black));
  uiTarget.fill(fui::Rect{band.x, static_cast<int16_t>(band.bottom() - 1), band.width, 1},
                fui::Paint::solid(fui::Color::Black));
}

void LibraryActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  int bounds[4]{};
  renderer.getOrientedViewableTRBL(&bounds[0], &bounds[1], &bounds[2], &bounds[3]);
  const int16_t headerBottom =
      static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput));
  screen.setContentMarginFromScreen(fui::Insets{headerBottom, static_cast<int16_t>(bounds[1]),
                                                static_cast<int16_t>(metrics.buttonHintsHeight + bounds[2]),
                                                static_cast<int16_t>(bounds[3])});
  // Every header icon owns a 44px touch box, with 10px of clearance.
  const int16_t controlSize = HEADER_CONTROL_SIZE;
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  const int16_t right = static_cast<int16_t>(renderer.getScreenWidth() - bounds[1] - headerControlRightInset());
  fui::ButtonProps action;
  action.action = ACTION_CONTROL;
  action.inputMask = fui::InputTouch;
  action.styles = fui::plainStyles();
  action.styles.selected = screen.theme().button.selected;
  action.icon = fui::bitmapFromIcon(icon_refresh_cw_32);
  action.value = 0;
  action.state = showSelection && selection == 0 ? fui::StateSelected : fui::StateNormal;
  screen.button(action,
                fui::Rect{static_cast<int16_t>(right - 3 * controlSize - 2 * HEADER_CONTROL_GAP),
                          static_cast<int16_t>(header.y + header.height - controlSize), controlSize, controlSize});
  action.icon = fui::bitmapFromIcon(icon_search_32);
  action.value = 1;
  action.state = showSelection && selection == 1 ? fui::StateSelected : fui::StateNormal;
  screen.button(action,
                fui::Rect{static_cast<int16_t>(right - 2 * controlSize - HEADER_CONTROL_GAP),
                          static_cast<int16_t>(header.y + header.height - controlSize), controlSize, controlSize});
  action.icon = fui::bitmapFromIcon(icon_ellipsis_vertical_32);
  action.value = 2;
  action.state = showSelection && selection == 2 ? fui::StateSelected : fui::StateNormal;
  screen.button(action,
                fui::Rect{static_cast<int16_t>(right - controlSize),
                          static_cast<int16_t>(header.y + header.height - controlSize), controlSize, controlSize});
  buildSortHeader(screen);
  if (scanFailed || index.ranksDegraded() || filterFailed) {
    const auto warning = screen.take(fui::LayoutAnchor::Top, uiTarget.lineHeight(screen.theme().smallText.font) + 8);
    uiTarget.text(warning,
                  filterFailed ? tr(STR_LIBRARY_SEARCH_FAILED)
                  : scanFailed ? tr(STR_LIBRARY_SCAN_FAILED)
                               : tr(STR_LIBRARY_UNSORTED),
                  screen.theme().smallText);
  }
  if (!query.empty()) {
    const auto search = screen.take(fui::LayoutAnchor::Top, uiTarget.lineHeight(screen.theme().smallText.font) + 8);
    uiTarget.text(search, query.c_str(), screen.theme().smallText);
  }
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (rowCount() == 0) {
    if (!scanFailed && !filterFailed) {
      screen.centeredText(hasActiveFilter() ? tr(STR_LIBRARY_NO_RESULTS) : tr(STR_LIBRARY_EMPTY),
                          screen.theme().bodyText);
    }
    return;
  }
  fui::ListProps props;
  props.rowProvider = &LibraryActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(rowCount());
  props.selectedIndex = showSelection ? static_cast<int16_t>(selection - CONTROL_COUNT) : -1;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.iconSize = 28;
  props.labelText = screen.theme().bodyText;
  props.labelText.bold = true;
  props.labelText.maxLines = 1;
  props.headerText = screen.theme().bodyText;
  props.headerText.bold = true;
  props.rtl = (I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::HE);
  configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  listNav.selected = showSelection ? selection - CONTROL_COUNT : -1;
  listNav.top = topIndex;
  listNav.syncToProps(screen.body(), props.rowHeight, props.rowGap, rowCount(), props);
  topIndex = listNav.top;
  screen.list(props);
}

void LibraryActivity::render(RenderLock&&) {
  uiReady = false;
  for (int pass = 0; pass < 8; ++pass) {
    renderer.clearScreen();
    const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
    if (mappedInput.hasTouchHardware())
      TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_LIBRARY), false,
                                  3 * HEADER_CONTROL_SIZE + 2 * HEADER_CONTROL_GAP + headerControlRightInset() + 10);
    else
      GUI.drawHeader(renderer, header, tr(STR_LIBRARY));
    app.render();
    topIndex = listNav.top;
    if (!listNav.consumeRebuildNeeded()) break;
  }
  uiReady = true;
  if (sortPopup.processRender(renderer, mappedInput)) return;
  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(query.empty() ? tr(STR_HOME) : tr(STR_BACK)),
                                            selection < CONTROL_COUNT ? tr(STR_SELECT) : tr(STR_OPEN), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (pendingCacheDeletedFeedback) GUI.drawPopup(renderer, tr(STR_BOOK_CACHE_DELETED));
  renderer.displayBuffer();
}

void LibraryActivity::promptDeleteBook(const RecentBook& book) {
  const std::string path = book.path;
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }

    BookActions::clearFileMetadata(path);
    if (!Storage.remove(path.c_str())) {
      LOG_ERR("LIB", "Failed to delete file: %s", path.c_str());
      return;
    }

    RECENT_BOOKS.removeByPath(path);
    reloadAfterBookAction();
  };

  const std::string heading = tr(STR_DELETE) + std::string("? ");
  openDialog(makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, book.title), std::move(handler));
}

void LibraryActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }
    if (RECENT_BOOKS.removeByPath(path)) {
      reloadAfterBookAction();
    }
  };

  openDialog(makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_RECENTS), title,
                                                     /*ignoreInitialConfirmRelease=*/false),
             std::move(handler));
}

void LibraryActivity::showBookActionMenu(const size_t bookIndex, const bool ignoreInitialConfirmRelease) {
  RecentBook book;
  if (!readBook(static_cast<int>(bookIndex), book)) return;
  const auto& recents = RECENT_BOOKS.getBooks();
  const bool isRecent = std::any_of(recents.begin(), recents.end(),
                                    [&book](const RecentBook& recent) { return recent.path == book.path; });
  std::vector<FileBrowserActionActivity::MenuItem> items =
      BookActions::buildBookActionItems(book.path, /*includeRemoveFromRecents=*/isRecent);
  if (BookActions::canSendNearby(book.path)) {
    items.push_back({FileBrowserAction::SendNearby, StrId::STR_SEND_NEARBY_BOOK});
  }

  openDialog(makeUniqueNoThrow<FileBrowserActionActivity>(renderer, mappedInput, book.title, std::move(items),
                                                          ignoreInitialConfirmRelease),
             [this, book](const ActivityResult& result) {
               longPressFired = false;
               if (result.isCancelled) {
                 return;
               }

               const auto* actionResult = std::get_if<FileBrowserActionResult>(&result.data);
               if (!actionResult) {
                 LOG_ERR("LIB", "Book action result missing");
                 return;
               }

               switch (static_cast<FileBrowserAction>(actionResult->action)) {
                 case FileBrowserAction::Delete:
                   promptDeleteBook(book);
                   return;
                 case FileBrowserAction::DeleteCache:
                   openDialog(makeUniqueNoThrow<ConfirmationActivity>(
                                  renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_CACHE),
                                  book.title),
                              [this, book](const ActivityResult& confirmation) {
                                if (!confirmation.isCancelled) {
                                  if (!BookActions::clearBookCache(book.path)) {
                                    LOG_ERR("LIB", "Failed to clear book cache for: %s", book.path.c_str());
                                  } else {
                                    pendingCacheDeletedFeedback = true;
                                    cacheDeletedFeedbackShowTime = millis();
                                  }
                                }
                                reloadAfterBookAction();
                              });
                   return;
                 case FileBrowserAction::DeleteStats:
                   openDialog(makeUniqueNoThrow<ConfirmationActivity>(
                                  renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_BOOK_STATS),
                                  book.title),
                              [this, book](const ActivityResult& confirmation) {
                                if (!confirmation.isCancelled) {
                                  if (!BookActions::deleteBookStats(book.path)) {
                                    LOG_ERR("LIB", "Failed to delete book stats for: %s", book.path.c_str());
                                  } else {
                                    BookActions::drawToast(renderer, tr(STR_BOOK_STATS_DELETED));
                                    delay(1000);
                                  }
                                }
                                reloadAfterBookAction();
                              });
                   return;
                 case FileBrowserAction::ResetReaderSettings:
                   openDialog(makeUniqueNoThrow<ConfirmationActivity>(
                                  renderer, mappedInput,
                                  BookActions::confirmationHeading(StrId::STR_RESET_BOOK_READER_SETTINGS), book.title),
                              [this, book](const ActivityResult& confirmation) {
                                if (!confirmation.isCancelled) {
                                  if (!BookActions::resetBookReaderSettings(book.path)) {
                                    LOG_ERR("LIB", "Failed to reset reader settings for: %s", book.path.c_str());
                                  } else {
                                    BookActions::drawToast(renderer, tr(STR_BOOK_READER_SETTINGS_RESET));
                                    delay(1000);
                                  }
                                }
                                reloadAfterBookAction();
                              });
                   return;
                 case FileBrowserAction::ToggleCompleted: {
                   bool completed = false;
                   if (BookActions::toggleBookCompleted(book.path, book.title, completed)) {
                     BookActions::drawToast(renderer, completed ? tr(STR_MARKED_FINISHED) : tr(STR_MARKED_UNFINISHED));
                     delay(1000);
                   }
                   reloadAfterBookAction();
                   return;
                 }
                 case FileBrowserAction::EpubRenderMode: {
                   const uint8_t currentIndex =
                       BookActions::epubRenderModeDisplayIndex(EpubReaderActivity::loadBookRenderMode(book.path));
                   openDialog(makeUniqueNoThrow<OptionSelectionActivity>(
                                  renderer, mappedInput, "LibraryEpubRenderModeSelect", StrId::STR_EPUB_RENDER_MODE,
                                  BookActions::epubRenderModeOptions(), currentIndex),
                              [this, book](const ActivityResult& selectionResult) {
                                if (!selectionResult.isCancelled) {
                                  const auto* selection = std::get_if<OptionSelectionResult>(&selectionResult.data);
                                  if (selection != nullptr &&
                                      !EpubReaderActivity::saveBookRenderMode(
                                          book.path, BookActions::epubRenderModeForDisplayIndex(selection->index))) {
                                    LOG_ERR("LIB", "Failed to save render mode for: %s", book.path.c_str());
                                  }
                                }
                                reloadAfterBookAction();
                              });
                   return;
                 }
                 case FileBrowserAction::RemoveFromRecents:
                   promptRemoveBook(book.path, book.title);
                   return;
                 case FileBrowserAction::SendNearby:
                   activityManager.goToNearbyBookSend(book.path, false);
                   return;
                 case FileBrowserAction::PinFavorite:
                 case FileBrowserAction::UnpinFavorite:
                 case FileBrowserAction::PinBootFavorite:
                 case FileBrowserAction::UnpinBootFavorite:
                 case FileBrowserAction::SetSleepFolder:
                 case FileBrowserAction::ClearSleepFolder:
                 case FileBrowserAction::ViewBookmarks:
                 case FileBrowserAction::ViewClippings:
                 case FileBrowserAction::DeleteBookmarks:
                 case FileBrowserAction::DeleteClippings:
                 case FileBrowserAction::Rename:
                   return;
               }
             });
}
