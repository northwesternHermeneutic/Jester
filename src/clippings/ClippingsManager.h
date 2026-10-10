#pragma once

#include <HalStorage.h>

#include <string>

class ClippingsManager {
 public:
  static bool saveClipping(const std::string& bookTitle, const std::string& author, const std::string& chapterTitle,
                           int pageNumber, const std::string& selectedText);

  static constexpr const char* CLIPPINGS_PATH = "/My Clippings.txt";

  // Jester (feature 4): appends a dog ear record to /DogEars/dogears-<book file name>.txt
  // in the clippings record format, with the book percentage in the location line.
  // `writeText` streams the page text into the open file (no whole-page buffer).
  using TextWriter = bool (*)(void* context, FsFile& file);
  static bool saveDogEar(const std::string& bookFileName, const std::string& bookTitle, const std::string& author,
                         const std::string& chapterTitle, int pageNumber, const std::string& percentLabel,
                         TextWriter writeText, void* context);
  static constexpr const char* DOGEARS_TEXT_DIR = "/DogEars";
};
