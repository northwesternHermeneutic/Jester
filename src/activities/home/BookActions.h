#pragma once

#include <string>
#include <vector>

#include "FileBrowserActionActivity.h"

class GfxRenderer;

namespace BookActions {

std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string& fullPath,
                                                                      bool includeRemoveFromRecents);
bool hasClearableBookCache(const std::string& path);
bool canSendNearby(const std::string& path);
void clearFileMetadata(const std::string& fullPath);
bool clearBookCache(const std::string& fullPath);
bool deleteBookStats(const std::string& fullPath);
bool resetBookReaderSettings(const std::string& fullPath);
std::vector<std::string> epubRenderModeOptions();
uint8_t epubRenderModeDisplayIndex(uint8_t renderMode);
uint8_t epubRenderModeForDisplayIndex(uint8_t displayIndex);
std::string confirmationHeading(StrId actionLabelId);
bool isBookCompleted(const std::string& fullPath);
bool toggleBookCompleted(const std::string& fullPath, const std::string& displayName, bool& completed);
void drawToast(const GfxRenderer& renderer, const char* msg);
// Jester (feature 1): books that can be favorites (EPUB, TXT, XTC).
bool canBeFavorite(const std::string& path);
// Adds or removes a favorite and shows the outcome as a toast. An empty title
// falls back to the recent-books title or the file name.
void toggleFavorite(const GfxRenderer& renderer, const std::string& path, const std::string& title,
                    const std::string& author);

}  // namespace BookActions
