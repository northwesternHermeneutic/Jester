#include "BookPercentFormat.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "util/FileContentEquals.h"

namespace BookPercentFormat {
namespace {
constexpr char SIDECAR_NAME[] = "/jester_size.bin";
}  // namespace

uint8_t decimals(const bool shortBook) {
  if (shortBook) return 0;
  constexpr uint8_t maxDecimals = CrossPointSettings::BOOK_PERCENTAGE_FORMAT_COUNT - 1;
  return std::min<uint8_t>(SETTINGS.statusBarBookPercentageFormat, maxDecimals);
}

void format(char* buffer, const size_t bufferSize, const float percent, const bool shortBook) {
  if (!buffer || bufferSize == 0) return;
  const float clamped = std::clamp(percent, 0.0f, 100.0f);
  snprintf(buffer, bufferSize, "%.*f%%", static_cast<int>(decimals(shortBook)), clamped);
}

std::string format(const float percent, const bool shortBook) {
  char buffer[16];
  format(buffer, sizeof(buffer), percent, shortBook);
  return buffer;
}

void saveBookSize(const std::string& cachePath, const size_t bookSizeBytes) {
  if (cachePath.empty()) return;
  const uint32_t value = static_cast<uint32_t>(bookSizeBytes);
  uint8_t data[sizeof(value)];
  for (size_t i = 0; i < sizeof(value); ++i) data[i] = static_cast<uint8_t>(value >> (8 * i));
  const std::string path = cachePath + SIDECAR_NAME;
  if (fileContentEquals("BPF", path.c_str(), data, sizeof(data))) return;
  HalFile file;
  if (!Storage.openFileForWrite("BPF", path.c_str(), file)) {
    LOG_ERR("BPF", "Cannot write %s", path.c_str());
    return;
  }
  if (file.write(data, sizeof(data)) != sizeof(data)) LOG_ERR("BPF", "Short write to %s", path.c_str());
  file.close();
}

bool loadIsShortBook(const std::string& cachePath) {
  if (cachePath.empty()) return false;
  const std::string path = cachePath + SIDECAR_NAME;
  if (!Storage.exists(path.c_str())) return false;
  HalFile file;
  if (!Storage.openFileForRead("BPF", path.c_str(), file)) return false;
  uint8_t data[4] = {};
  const bool ok = file.read(data, sizeof(data)) == static_cast<int>(sizeof(data));
  file.close();
  if (!ok) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < sizeof(data); ++i) value |= static_cast<uint32_t>(data[i]) << (8 * i);
  return isShortBook(value);
}

}  // namespace BookPercentFormat
