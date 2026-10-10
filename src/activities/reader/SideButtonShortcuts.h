#pragma once

#include <cstdint>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReaderUtils.h"
#include "util/QuickLockTrigger.h"

// A reader owns one tracker for each logical side button. Short actions fire
// on release; long actions fire once when the hold threshold is reached.
// Tracking only starts on a press edge, so a button already held while a
// reader activity opens cannot turn into an accidental shortcut.
class SideButtonShortcuts {
 public:
  struct Result {
    bool consumed = false;
    bool triggered = false;
    bool longPress = false;
    bool up = false;
    // Jester (feature 20): a front Left/Right long press (`up` = Right).
    bool front = false;
    uint8_t action = CrossPointSettings::IGNORE;
  };

  static QuickLockTrigger quickLockTrigger(const Result& result) {
    if (result.front) return QuickLockTrigger::None;  // Quick Lock is not unlockable from a front hold
    if (result.up) return result.longPress ? QuickLockTrigger::SideUpLong : QuickLockTrigger::SideUpShort;
    return result.longPress ? QuickLockTrigger::SideDownLong : QuickLockTrigger::SideDownShort;
  }

  static bool shouldConsume(const Result& result, const MappedInputManager& input) {
    // Chord Page Turn arrives as an injected front-button release while a side
    // button may still be held. Let that release reach the reader page handler.
    return result.consumed && !input.hasInjectedRelease(MappedInputManager::Button::Right);
  }

  Result update(const MappedInputManager& input, const unsigned long now) {
    Result upResult = updateOne(input, MappedInputManager::Button::Up, true, up_, now);
    Result downResult = updateOne(input, MappedInputManager::Button::Down, false, down_, now);
    if (upResult.triggered) return upResult;
    if (downResult.triggered) return downResult;
    // Jester (feature 20): front Left/Right long presses. Short presses are
    // never consumed here, so page turns and navigation keep working; only the
    // release that ends a fired long press is swallowed.
    Result leftResult = updateFront(input, MappedInputManager::Button::Left, SETTINGS.frontLeftLong, false, left_, now);
    Result rightResult =
        updateFront(input, MappedInputManager::Button::Right, SETTINGS.frontRightLong, true, right_, now);
    if (leftResult.triggered) return leftResult;
    if (rightResult.triggered) return rightResult;
    upResult.consumed = upResult.consumed || downResult.consumed || leftResult.consumed || rightResult.consumed;
    return upResult;
  }

 private:
  struct ButtonState {
    unsigned long pressedAt = 0;
    bool tracking = false;
    bool longFired = false;
  };
  ButtonState up_;
  ButtonState down_;
  ButtonState left_;
  ButtonState right_;

  static Result updateFront(const MappedInputManager& input, const MappedInputManager::Button button,
                            const uint8_t longAction, const bool right, ButtonState& state, const unsigned long now) {
    Result result;
    result.front = true;
    result.up = right;
    if (longAction == CrossPointSettings::IGNORE) {
      state.tracking = false;
      return result;
    }
    if (input.wasPressed(button)) {
      state.pressedAt = now;
      state.tracking = true;
      state.longFired = false;
    }
    if (!state.tracking) return result;
    const bool pressed = input.isPressed(button);
    if (!state.longFired && pressed && now - state.pressedAt >= ReaderUtils::SKIP_HOLD_MS) {
      state.longFired = true;
      result.triggered = true;
      result.longPress = true;
      result.consumed = true;
      result.action = longAction;
    } else if (state.longFired) {
      result.consumed = true;  // hold continues or ends: no page turn for it
    }
    if (!pressed) state.tracking = false;
    return result;
  }

  static Result updateOne(const MappedInputManager& input, const MappedInputManager::Button button, const bool up,
                          ButtonState& state, const unsigned long now) {
    Result result;
    result.up = up;
    if (input.wasPressed(button)) {
      state.pressedAt = now;
      state.tracking = true;
      state.longFired = false;
    }
    if (!state.tracking) return result;

    result.consumed = true;
    const bool pressed = input.isPressed(button);
    const bool released = input.wasReleased(button);
    const uint8_t longAction = up ? SETTINGS.sideButtonUpLong : SETTINGS.sideButtonDownLong;
    if (!state.longFired && longAction != CrossPointSettings::IGNORE &&
        now - state.pressedAt >= ReaderUtils::SKIP_HOLD_MS && (pressed || released)) {
      state.longFired = true;
      result.triggered = true;
      result.longPress = true;
      result.action = longAction;
    } else if (released) {
      if (!state.longFired) {
        result.triggered = true;
        result.action = up ? SETTINGS.sideButtonUpShort : SETTINGS.sideButtonDownShort;
      }
    }
    if (!pressed) state.tracking = false;
    return result;
  }
};
