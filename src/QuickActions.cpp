#include "QuickActions.h"

#include <I18n.h>

#include <vector>

#include "GlobalActions.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"

namespace QuickActions {
namespace {

bool sleepScreenNeedsUnderlyingFrame() {
  // These modes deliberately keep some or all of the currently displayed
  // framebuffer. All other sleep screens paint an opaque replacement.
  return SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::OVERLAY ||
         SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME;
}

}  // namespace

void showConfiguredPopup(OptionPopup& popup, const std::function<void()>& requestUpdate, ActionHandler actionHandler,
                         ActionFilter actionFilter) {
  std::vector<std::string> labels;
  std::vector<uint8_t> actions;
  labels.reserve(std::size(SETTINGS.quickActionSlots));
  actions.reserve(std::size(SETTINGS.quickActionSlots));
  for (const uint8_t action : SETTINGS.quickActionSlots) {
    const auto shortcutAction = static_cast<CrossPointSettings::SHORT_PWRBTN>(action);
    if (action == CrossPointSettings::IGNORE || !isQuickActionSlotActionAvailable(action) ||
        (actionFilter && !actionFilter(shortcutAction))) {
      continue;
    }
    labels.emplace_back(I18N.get(actionLabel(action)));
    actions.push_back(action);
  }
  if (actions.empty()) return;
  popup.show(StrId::STR_QUICK_ACTIONS, labels, 0,
             [actions = std::move(actions), actionHandler = std::move(actionHandler)](const int selected) {
               if (selected >= 0 && static_cast<size_t>(selected) < actions.size()) {
                 const auto action = static_cast<CrossPointSettings::SHORT_PWRBTN>(actions[selected]);
                 // These actions read or write the current framebuffer immediately.
                 // Render the underlying screen after dismissing the popup so none of
                 // them captures or paints over the stale modal image. Other actions
                 // already schedule their own redraw and should not pay for an extra
                 // full-page render here.
                 // Quick Lock is never offered here (isQuickActionSlotActionAvailable
                 // filters it out) because unlocking needs a single physical shortcut.
                 // Jester: the Orientation Menu popup is drawn over the framebuffer
                 // too, which would still show this larger popup's frame around it.
                 if ((action == CrossPointSettings::SHORT_PWRBTN::SLEEP && sleepScreenNeedsUnderlyingFrame()) ||
                     (action == CrossPointSettings::SHORT_PWRBTN::SCREENSHOT &&
                      !activityManager.canSnapshotForSleepOverlay()) ||
                     action == CrossPointSettings::SHORT_PWRBTN::ORIENTATION_MENU) {
                   (void)activityManager.requestUpdateAndWait();
                 }
                 if (actionHandler) {
                   actionHandler(action);
                 } else {
                   dispatchShortcutAction(action);
                 }
               }
             });
  requestUpdate();
}

void showOrientationPopup(OptionPopup& popup, const uint8_t currentOrientation, std::function<void(uint8_t)> onPick) {
  static constexpr uint8_t orientations[] = {CrossPointSettings::PORTRAIT, CrossPointSettings::LANDSCAPE_CCW,
                                             CrossPointSettings::LANDSCAPE_CW, CrossPointSettings::INVERTED};
  static constexpr StrId labels[] = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CCW, StrId::STR_LANDSCAPE_CW,
                                     StrId::STR_ORIENTATION_INVERTED};
  int current = 0;
  for (int i = 0; i < static_cast<int>(std::size(orientations)); ++i) {
    if (orientations[i] == currentOrientation) current = i;
  }
  popup.show(StrId::STR_ORIENTATION, labels, static_cast<int>(std::size(labels)), current,
             [onPick = std::move(onPick)](const int selected) {
               if (onPick && selected >= 0 && selected < static_cast<int>(std::size(orientations))) {
                 onPick(orientations[selected]);
               }
             });
}
}  // namespace QuickActions
