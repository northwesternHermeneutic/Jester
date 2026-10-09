#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Jester (feature 2): the book percentage is shown at one user-chosen decimal
// level (Settings > Reader > Percentage Format) everywhere it appears: status
// bars, bookmark and dog ear locations, the in-book menu and Home. Short books
// always show whole numbers, because tenths only add jitter there.
namespace BookPercentFormat {

// Total spine XHTML bytes (Epub::getBookSize) below which tenths are hidden.
// Roughly 200 printed pages at about 3,000 markup bytes per page. Tune by testing.
constexpr size_t SHORT_BOOK_MAX_BYTES = 600 * 1024;

constexpr bool isShortBook(const size_t bookSizeBytes) {
  return bookSizeBytes > 0 && bookSizeBytes < SHORT_BOOK_MAX_BYTES;
}

// Decimal places for the user's setting, or 0 for a short book.
uint8_t decimals(bool shortBook);

// Writes e.g. "37.4%" (percent is 0..100).
void format(char* buffer, size_t bufferSize, float percent, bool shortBook);
std::string format(float percent, bool shortBook);

// Book-size sidecar in the book's cache directory, so screens without an open
// book (Home, Library) can apply the same short-book rule. Writes only when the
// stored value differs.
void saveBookSize(const std::string& cachePath, size_t bookSizeBytes);
// False when the size is unknown (no sidecar yet): the user's level is used.
bool loadIsShortBook(const std::string& cachePath);

}  // namespace BookPercentFormat
