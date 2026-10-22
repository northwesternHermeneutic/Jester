#pragma once

// Tracks one-shot button-release suppression across an activity transition.
// A suppression remains armed for the release frame that follows the shortcut
// press, then expires automatically. That prevents a child activity that does
// not read that particular button from accidentally consuming a later, real
// user action.
class ReleaseSuppression {
 public:
  struct FrameState {
    bool backHeld = false;
    bool backReleased = false;
    bool confirmHeld = false;
    bool confirmReleased = false;
    bool powerHeld = false;
    bool powerReleased = false;
  };

  void suppressBack() { suppressBackRelease_ = true; }
  void suppressConfirm() { suppressConfirmRelease_ = true; }
  void suppressPower() { suppressPowerRelease_ = true; }
  void suppressPowerConfirm() { suppressPowerConfirmRelease_ = true; }

  // Jester: a swallowed release stays swallowed for every query in the same
  // input frame. Before, only the first wasReleased() call saw the
  // suppression, so a second reader of the same button in that frame got the
  // release (Mark as Finished's Confirm also opened the End of Book screen's
  // first suggestion, found in eink-qemu).
  bool consumeBackRelease() {
    if (backSwallowedThisFrame_) return true;
    if (!suppressBackRelease_) return false;
    suppressBackRelease_ = false;
    backSwallowedThisFrame_ = true;
    return true;
  }

  bool consumeConfirmRelease() {
    if (confirmSwallowedThisFrame_) return true;
    if (!suppressConfirmRelease_) return false;
    suppressConfirmRelease_ = false;
    // Confirm can be delivered through the Power fallback. Keep both paths in
    // sync when that release has already been claimed by the popup selection.
    suppressPowerConfirmRelease_ = false;
    confirmSwallowedThisFrame_ = true;
    return true;
  }

  bool consumePowerRelease() {
    if (powerSwallowedThisFrame_) return true;
    if (!suppressPowerRelease_) return false;
    suppressPowerRelease_ = false;
    powerSwallowedThisFrame_ = true;
    return true;
  }

  bool consumePowerConfirmRelease() {
    if (powerConfirmSwallowedThisFrame_) return true;
    if (!suppressPowerConfirmRelease_) return false;
    suppressPowerConfirmRelease_ = false;
    powerConfirmSwallowedThisFrame_ = true;
    return true;
  }

  bool isPowerReleaseSuppressed() const { return suppressPowerRelease_ || powerSwallowedThisFrame_; }

  void expireAfterReleaseFrame(const FrameState& state) {
    // A new input frame: releases swallowed in the previous frame are over.
    backSwallowedThisFrame_ = false;
    confirmSwallowedThisFrame_ = false;
    powerSwallowedThisFrame_ = false;
    powerConfirmSwallowedThisFrame_ = false;
    if (!state.backHeld && !state.backReleased) suppressBackRelease_ = false;

    // Confirm may be a physical Confirm button or Power's Confirm fallback.
    if (!state.confirmHeld && !state.confirmReleased && !state.powerHeld && !state.powerReleased) {
      suppressConfirmRelease_ = false;
    }

    if (!state.powerHeld && !state.powerReleased) {
      suppressPowerRelease_ = false;
      suppressPowerConfirmRelease_ = false;
    }
  }

 private:
  bool suppressBackRelease_ = false;
  bool backSwallowedThisFrame_ = false;
  bool confirmSwallowedThisFrame_ = false;
  bool powerSwallowedThisFrame_ = false;
  bool powerConfirmSwallowedThisFrame_ = false;
  bool suppressConfirmRelease_ = false;
  bool suppressPowerRelease_ = false;
  bool suppressPowerConfirmRelease_ = false;
};
