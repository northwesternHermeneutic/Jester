#pragma once

#include <string>

#include "Epub.h"

namespace serialization {

template <typename T>
void writePod(HalFile&, const T&) {}

template <typename T>
bool tryReadPod(HalFile&, T&) {
  return false;
}

inline void writeString(HalFile&, const std::string&) {}
inline bool tryReadString(HalFile&, std::string& out) {
  out.clear();
  return false;
}

}  // namespace serialization
