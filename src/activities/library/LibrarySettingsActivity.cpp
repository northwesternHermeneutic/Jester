#include "LibrarySettingsActivity.h"

#include <I18n.h>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
constexpr int ROW_COUNT = 5;
}  // namespace

LibrarySettingsActivity::LibrarySettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Library Settings", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void LibrarySettingsActivity::onEnter() {
  Activity::onEnter();
  showSelection = !mappedInput.hasTouchHardware();
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &LibrarySettingsActivity::onRow, this);
  app.setScreen(&LibrarySettingsActivity::screen, this);
  ignoreConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate();
}

void LibrarySettingsActivity::toggle(const int row) {
  switch (row) {
    case 0:
      SETTINGS.libraryListExpanded = !SETTINGS.libraryListExpanded;
      break;
    case 1:
      SETTINGS.libraryShowEpub = !SETTINGS.libraryShowEpub;
      break;
    case 2:
      SETTINGS.libraryShowXtc = !SETTINGS.libraryShowXtc;
      break;
    case 3:
      SETTINGS.libraryShowTxt = !SETTINGS.libraryShowTxt;
      break;
    case 4:
      SETTINGS.libraryShowMarkdown = !SETTINGS.libraryShowMarkdown;
      break;
    default:
      return;
  }
  if (!SETTINGS.saveToFile()) LOG_ERR("LIB", "Cannot save Library settings");
  requestUpdate();
}

void LibrarySettingsActivity::onRow(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<LibrarySettingsActivity*>(user);
  if (event.value < 0 || event.value >= ROW_COUNT) return;
  self->selection = event.value;
  self->showSelection = false;
  self->app.clearTapFlash();
  self->toggle(event.value);
}

void LibrarySettingsActivity::loop() {
  RenderLock lock;
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finishAfterBackPress();
    return;
  }
  if (ignoreConfirmRelease) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) ignoreConfirmRelease = false;
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
    toggle(selection);
    return;
  }
  const auto move = [this](int next) {
    if (!showSelection) {
      showSelection = true;
      requestUpdate();
      return;
    }
    selection = next;
    requestUpdate();
  };
  buttonNavigator.onNextRelease([&] { move(ButtonNavigator::nextIndex(selection, ROW_COUNT)); });
  buttonNavigator.onPreviousRelease([&] { move(ButtonNavigator::previousIndex(selection, ROW_COUNT)); });
}

void LibrarySettingsActivity::screen(UiApp::ScreenType& screen, void* user) {
  static_cast<LibrarySettingsActivity*>(user)->buildScreen(screen);
}

void LibrarySettingsActivity::buildScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  int bounds[4]{};
  renderer.getOrientedViewableTRBL(&bounds[0], &bounds[1], &bounds[2], &bounds[3]);
  const int16_t headerBottom =
      static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput));
  const int16_t sidePadding = static_cast<int16_t>(metrics.contentSidePadding);
  screen.setContentMarginFromScreen(fui::Insets{headerBottom, static_cast<int16_t>(bounds[1] + sidePadding),
                                                static_cast<int16_t>(metrics.buttonHintsHeight + bounds[2]),
                                                static_cast<int16_t>(bounds[3] + sidePadding)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  auto rowStyles = screen.theme().listRow;
  rowStyles.selected.background = fui::Paint::dither(fui::Color::LightGray);
  rowStyles.selected.foreground = fui::Paint::solid(fui::Color::Black);
  rowStyles.active = rowStyles.selected;
  fui::SettingRowProps row;
  row.label = tr(STR_LIBRARY_LIST_VIEW);
  row.value = SETTINGS.libraryListExpanded ? tr(STR_LIBRARY_EXPANDED) : tr(STR_COMPACT);
  row.action = ACTION_ROW;
  row.valueId = 0;
  row.labelText = row.valueText = screen.theme().bodyText;
  row.styles = rowStyles;
  row.state = showSelection && selection == 0 ? fui::StateSelected : fui::StateNormal;
  screen.settingRow(row, 44);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  const auto heading = screen.take(fui::LayoutAnchor::Top, 44);
  auto headingText = screen.theme().bodyText;
  headingText.bold = true;
  uiTarget.text(heading, tr(STR_LIBRARY_SHOW_FILES), headingText);
  const StrId labels[] = {StrId::STR_LIBRARY_EPUBS, StrId::STR_LIBRARY_XTC_XTCH, StrId::STR_LIBRARY_TXT,
                          StrId::STR_LIBRARY_MARKDOWN};
  const bool checks[] = {SETTINGS.libraryShowEpub != 0, SETTINGS.libraryShowXtc != 0, SETTINGS.libraryShowTxt != 0,
                         SETTINGS.libraryShowMarkdown != 0};
  for (int i = 0; i < 4; ++i) {
    fui::ToggleRowProps toggle;
    toggle.row.label = I18N.get(labels[i]);
    toggle.row.action = ACTION_ROW;
    toggle.row.valueId = static_cast<int16_t>(i + 1);
    toggle.row.labelText = screen.theme().bodyText;
    toggle.row.styles = rowStyles;
    toggle.row.state = showSelection && selection == i + 1 ? fui::StateSelected : fui::StateNormal;
    toggle.checked = checks[i];
    screen.toggleRow(toggle, 44);
  }
}

void LibrarySettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_LIBRARY_SETTINGS), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_LIBRARY_SETTINGS));
  uiReady = false;
  app.render();
  uiReady = true;
  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
