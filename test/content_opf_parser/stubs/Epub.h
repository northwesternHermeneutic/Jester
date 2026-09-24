#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Minimal stand-in for the HalFile surface ContentOpfParser actually calls:
// the manifest-index temp-file read/write path and the RAII bool check in its
// destructor. Real seek/read semantics are irrelevant here because every test
// in this suite either passes cache=nullptr (manifest indexing never runs) or
// exercises the metadataOnly early-stop, which returns before the manifest is
// ever opened.
class HalFile {
 public:
  explicit operator bool() const { return open_; }
  void close() { open_ = false; }
  bool seek(size_t position) {
    position_ = position;
    return true;
  }
  bool seekCur(int64_t offset) {
    position_ += static_cast<size_t>(offset);
    return true;
  }
  size_t position() const { return position_; }
  size_t size() const { return 0; }
  int available() const { return 0; }
  int read(void*, size_t) { return 0; }
  void markOpen() { open_ = true; }

 private:
  bool open_ = false;
  size_t position_ = 0;
};

struct TestStorage {
  int writeOpens = 0;
  int readOpens = 0;

  bool openFileForWrite(const char*, const std::string&, HalFile& file) {
    writeOpens++;
    file.markOpen();
    return true;
  }
  bool openFileForRead(const char*, const std::string&, HalFile& file) {
    readOpens++;
    file.markOpen();
    return true;
  }
  bool exists(const char*) const { return false; }
  bool remove(const char*) { return true; }
};

inline TestStorage Storage;
