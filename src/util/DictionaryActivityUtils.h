#pragma once
#include <GfxRenderer.h>
#include <I18n.h>
#include <MappedInputManager.h>

#include "CrossPointSettings.h"
#include "activities/Activity.h"
#include "components/UITheme.h"
#include "util/WordSelectNavigator.h"

namespace DictUtils {

// D-006: Back-cancel pattern — sets isCancelled=true and finishes the activity.
inline void cancelAndFinish(Activity& act) {
  ActivityResult r;
  r.isCancelled = true;
  act.setResult(std::move(r));
  act.finish();
}

// Dictionary screens follow configured short page actions. When both side
// buttons turn forward, reserve Up for previous navigation in the picker.
inline bool dictionaryPageButtonTriggered(MappedInputManager& input, const bool previous) {
  const MappedInputManager::Button button =
      SETTINGS.sideButtonUpShort == CrossPointSettings::PAGE_TURN &&
              SETTINGS.sideButtonDownShort == CrossPointSettings::PAGE_TURN
          ? (previous ? MappedInputManager::Button::Up : MappedInputManager::Button::Down)
          : (previous ? MappedInputManager::Button::PageBack : MappedInputManager::Button::PageForward);
  return input.wasReleased(button);
}

// Jester: the buttons that move a word cursor, per reader orientation. Look Up
// Word defined this mapping; Create Clipping uses it too so both modes match.
struct WordCursorButtons {
  MappedInputManager::Button rowPrev;
  MappedInputManager::Button rowNext;
  MappedInputManager::Button wordPrev;
  MappedInputManager::Button wordNext;
};

inline WordCursorButtons wordCursorButtons(const MappedInputManager& input, const GfxRenderer& renderer) {
  using Button = MappedInputManager::Button;
  switch (renderer.getOrientation()) {
    case GfxRenderer::Orientation::LandscapeClockwise:
      return {Button::Left, Button::Right, Button::Down, Button::Up};
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      if (input.isFrontNavButtonSwapActive()) return {Button::Left, Button::Right, Button::Up, Button::Down};
      return {Button::Right, Button::Left, Button::Up, Button::Down};
    case GfxRenderer::Orientation::PortraitInverted:
      return {Button::Down, Button::Up, Button::Right, Button::Left};
    case GfxRenderer::Orientation::Portrait:
    default:
      return {Button::Up, Button::Down, Button::Left, Button::Right};
  }
}

// Blank the front button hint strip so hints stay readable over page text.
inline void clearFrontButtonHintArea(GfxRenderer& renderer) {
  const int hintSize = UITheme::getInstance().getMetrics().buttonHintsHeight;
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  switch (renderer.getOrientation()) {
    case GfxRenderer::Orientation::Portrait:
      renderer.fillRect(0, screenHeight - hintSize, screenWidth, hintSize, false);
      break;
    case GfxRenderer::Orientation::LandscapeClockwise:
      renderer.fillRect(0, 0, hintSize, screenHeight, false);
      break;
    case GfxRenderer::Orientation::PortraitInverted:
      renderer.fillRect(0, 0, screenWidth, hintSize, false);
      break;
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      renderer.fillRect(screenWidth - hintSize, 0, hintSize, screenHeight, false);
      break;
  }
}

inline void drawWordCursorButtonHints(GfxRenderer& renderer, const MappedInputManager& mappedInput,
                                      const char* confirmLabel) {
  const auto orientation = renderer.getOrientation();
  const bool isLandscape = orientation == GfxRenderer::Orientation::LandscapeClockwise ||
                           orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const char* previousLabel = isLandscape ? tr(STR_DIR_UP) : tr(STR_DIR_LEFT);
  const char* nextLabel = isLandscape ? tr(STR_DIR_DOWN) : tr(STR_DIR_RIGHT);
  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), confirmLabel, previousLabel, nextLabel);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

inline void drawWordSelectButtonHints(GfxRenderer& renderer, const MappedInputManager& mappedInput,
                                      const WordSelectNavigator& navigator) {
  drawWordCursorButtonHints(renderer, mappedInput,
                            navigator.isMultiSelecting() ? tr(STR_DONE) : tr(STR_LOOKUP_SHORT));
}

}  // namespace DictUtils
