#pragma once

#include <FreeInkUICore.h>

#include <string>

// Jester (feature 18): names longer than one line wrap to two lines regardless
// of spaces, breaking a long word at a hyphenation point.
//
// The FreeInkUI list wraps greedily by word and honours a hard '\n'. This
// helper decides where the first line ends and inserts that '\n' (plus a '-'
// when the hyphenation break needs one); the list then draws the second line
// and ellipsizes it if the rest is still too long.
namespace TwoLineName {

// Width in pixels of `text` in the font the label is drawn with.
using MeasureFn = int (*)(void* context, const char* text);

std::string prebreak(const std::string& text, int maxWidth, MeasureFn measure, void* context);

// Measures with a FreeInkUI draw target and the label's text style.
struct UiMeasure {
  freeink::ui::DrawTarget* target = nullptr;
  freeink::ui::TextStyle style{};
};
int measureUiText(void* context, const char* text);

// Label width of a FreeInkUI list row: row width minus side padding, the icon
// and its gap, and the value column (FreeInkUI list.h row layout). A small
// safety margin keeps the first line from re-wrapping.
int listLabelWidth(int rowWidth, int sidePadding, int iconSize, int textGap, int valueWidth, int valueInset);

}  // namespace TwoLineName
