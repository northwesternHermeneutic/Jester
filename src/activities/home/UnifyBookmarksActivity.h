#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Jester (feature 6): re-attaches bookmarks, dog ears, clippings, favorites
// and reading progress to a book that was moved or renamed on a computer.
// Stored items whose book file is missing are matched against the whole-card
// Library index by exact title and author (v0.1 policy); the user approves or
// ignores each proposed merge.
class UnifyBookmarksActivity final : public Activity {
 public:
  UnifyBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Candidate {
    std::string oldPath;
    std::string newPath;
    std::string title;
    std::string author;
    std::string bookType;
  };

  enum class State : uint8_t { Searching, Prompting, Done };

  State state = State::Searching;
  std::vector<Candidate> candidates;
  size_t nextCandidate = 0;
  int merged = 0;
  int ignored = 0;
  bool indexMissing = false;
  bool searchStarted = false;

  void findCandidates();
  void promptNext();
  bool merge(const Candidate& candidate);
};
