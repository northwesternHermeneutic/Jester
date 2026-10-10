#include "TwoLineName.h"

#include <Epub/hyphenation/Hyphenator.h>

#include <vector>

namespace TwoLineName {
namespace {

bool isContinuationByte(const char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

// Byte length of the longest UTF-8 prefix of `text` that fits in maxWidth.
size_t fittingPrefix(const std::string& text, const int maxWidth, const MeasureFn measure, void* context) {
  std::vector<size_t> boundaries;
  boundaries.reserve(text.size());
  for (size_t i = 1; i <= text.size(); ++i) {
    if (i == text.size() || !isContinuationByte(text[i])) boundaries.push_back(i);
  }
  size_t lo = 0;
  size_t hi = boundaries.size();
  size_t best = 0;
  std::string probe;
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    probe.assign(text, 0, boundaries[mid]);
    if (measure(context, probe.c_str()) <= maxWidth) {
      best = boundaries[mid];
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return best;
}

}  // namespace

std::string prebreak(const std::string& text, const int maxWidth, const MeasureFn measure, void* context) {
  if (text.empty() || maxWidth <= 0 || !measure || text.find('\n') != std::string::npos) return text;
  if (measure(context, text.c_str()) <= maxWidth) return text;

  const size_t fit = fittingPrefix(text, maxWidth, measure, context);
  if (fit == 0 || fit >= text.size()) return text;

  // The word that crosses the line edge.
  const size_t lastSpace = text.rfind(' ', fit);
  const size_t wordStart = lastSpace == std::string::npos ? 0 : lastSpace + 1;
  size_t wordEnd = text.find(' ', wordStart);
  if (wordEnd == std::string::npos) wordEnd = text.size();

  if (wordStart < fit) {
    const std::string word = text.substr(wordStart, wordEnd - wordStart);
    const auto breaks = Hyphenator::breakOffsets(word, /*includeFallback=*/false);
    std::string candidate;
    for (auto it = breaks.rbegin(); it != breaks.rend(); ++it) {
      const size_t split = wordStart + it->byteOffset;
      // Keep at least two bytes of the word on each side of the break.
      if (it->byteOffset < 2 || it->byteOffset + 2 > word.size() || split > fit) continue;
      candidate.assign(text, 0, split);
      if (it->requiresInsertedHyphen) candidate.push_back('-');
      if (measure(context, candidate.c_str()) > maxWidth) continue;
      candidate.push_back('\n');
      candidate.append(text, split, std::string::npos);
      return candidate;
    }
  }
  if (lastSpace != std::string::npos && lastSpace > 0) {
    // No usable hyphenation point: break at the space (what word wrap does).
    std::string result = text.substr(0, lastSpace);
    result.push_back('\n');
    result.append(text, lastSpace + 1, std::string::npos);
    return result;
  }
  // One long word without a hyphenation point: break where the line ends.
  std::string result = text.substr(0, fit);
  result.push_back('\n');
  result.append(text, fit, std::string::npos);
  return result;
}

int measureUiText(void* context, const char* text) {
  const auto* measure = static_cast<const UiMeasure*>(context);
  if (!measure || !measure->target || !text) return 0;
  return measure->target->measureText(measure->style.font, text, measure->style).width;
}

int listLabelWidth(const int rowWidth, const int sidePadding, const int iconSize, const int textGap,
                   const int valueWidth, const int valueInset) {
  constexpr int SAFETY = 12;  // scrollbar and rounding
  const int pad = sidePadding < 0 ? 8 : sidePadding;
  int width = rowWidth - 2 * pad - SAFETY;
  if (iconSize > 0) width -= iconSize + textGap;
  if (valueWidth > 0) width -= valueWidth + valueInset + textGap;
  return width;
}

}  // namespace TwoLineName
