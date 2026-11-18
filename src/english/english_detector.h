// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#ifndef MOZC_ENGLISH_ENGLISH_DETECTOR_H_
#define MOZC_ENGLISH_ENGLISH_DETECTOR_H_

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "composer/table.h"

namespace mozc {
namespace english {

class RomajiReader;

// The English word list: tools/english/prepare_english_words.sh.
class EnglishWords {
 public:
  struct Info {
    int scowl_size = 0;  // 35 (most common) to 70. 0: not an ordinary word.
    bool in_cspell = false;
    bool in_dictionary = false;
  };

  // Reads "word <TAB> scowl_size <TAB> sources" lines. False if unreadable.
  bool LoadFromFile(const std::string& path);
  void LoadFromString(std::string_view text);
  // Removes romanized Japanese names that only the conversion dictionary
  // lists ("ayaka"): they read as kana and are not English.
  void RemoveRomajiNames(const RomajiReader& reader);
  void Add(std::string_view word, const Info& info);
  void Remove(std::string_view word);

  const Info* Find(std::string_view word) const;
  // True if a word starts with |prefix|.
  bool HasWordStartingWith(std::string_view prefix) const;
  size_t size() const { return words_.size(); }
  const std::map<std::string, Info, std::less<>>& words() const {
    return words_;
  }

 private:
  std::map<std::string, Info, std::less<>> words_;
};

// Reads keys as romaji, the way the composer does.
class RomajiReader {
 public:
  explicit RomajiReader(const composer::Table* table) : table_(table) {}

  // What |keys| read as. Letters that do not read stay as they are. With
  // |end_of_input|, an "n" at the end is "ん", as it is when converting.
  std::string ToKana(std::string_view keys, bool end_of_input) const;
  // True if no key is left unread.
  bool ReadsAsKana(std::string_view keys, bool end_of_input = true) const;
  // The number of keys at the end of |keys| that are consonants still waiting
  // for a vowel ("k", "ky", "tt"). 0 when nothing waits, or when a key does
  // not read at all.
  size_t WaitingLength(std::string_view keys) const;

 private:
  bool IsUnfinished(std::string_view keys) const;

  const composer::Table* table_;
};

// How natural a reading is as Japanese. Implemented with the converter.
class ReadingJudge {
 public:
  virtual ~ReadingJudge() = default;
  // The cost of the best conversion of |reading| (hiragana with ASCII words).
  virtual int Cost(std::string_view reading) const = 0;
  // True if a longer dictionary word starts with |reading|.
  virtual bool IsPrefixOfWord(std::string_view reading) const = 0;
  // The segments of the best conversion of |reading|: where each ends, in
  // bytes, and what it costs.
  struct Segment {
    size_t end = 0;
    int cost = 0;
  };
  virtual std::vector<Segment> Segments(std::string_view reading) const = 0;
};

// What the user taught by correcting: true for a word that is English, false
// for one that is not.
using LearnedWords = std::map<std::string, bool, std::less<>>;

// Finds the English words in keys typed in a romaji composition.
class EnglishDetector {
 public:
  struct Run {
    size_t begin = 0;
    size_t end = 0;
    bool operator==(const Run&) const = default;
  };

  // The thresholds. Chosen with tools/english/measure; see docs/nagi/decisions.md.
  struct Options {
    // A word that does not read as kana is English when showing it as English
    // costs at most this much more than reading the keys as kana.
    int unreadable_margin = 5500;
    // A word that also reads as kana needs more. Any entry that matches makes
    // it English: {minimum letters, minimum cost of the word alone as kana,
    // margin}.
    struct ReadableRule {
      size_t letters;
      int word_cost;
      int margin;
    };
    std::vector<ReadableRule> readable_rules = {{4, 12000, 10000},
                                                {4, 9000, 6000}};
    // A readable word that both the spell checkers for code and the
    // conversion dictionary know ("time", "code", "token") is English more
    // readily: its margin grows and the cost its kana must exceed shrinks.
    int known_word_margin_bonus = 4500;
    int known_word_cost_discount = 2000;
    // What a taught word that reads as kana may cost in the middle of the
    // keys.
    int taught_margin = 12000;
    // A segment of the kana reading that costs less than this is ordinary
    // Japanese. A word that only touches such segments is not English.
    int natural_segment_cost = 10000;
    // Added for every letter that is neither read as kana nor part of a run.
    int unread_letter_cost = 20000;
    // Added to a candidate's score for each of its letters.
    int letter_bonus = 2000;
    size_t max_runs = 4;
    // With the previous result, only this many keys at the end are looked at.
    size_t window = 40;
    // A run that ended this many keys ago is not looked at again.
    size_t settled = 12;
  };

  EnglishDetector(const EnglishWords* words, const RomajiReader* reader,
                  const ReadingJudge* judge)
      : words_(words), reader_(reader), judge_(judge) {}

  void set_options(const Options& options) { options_ = options; }

  // The English runs in |keys|, in order. Only keys that are all lower-case
  // letters are looked at. While the user is typing, consonants at the end may
  // still become a syllable ("git" may go on to "gita"). With |end_of_input|,
  // before converting or committing, they cannot, and are English or unread.
  //
  // |previous| is what Detect() returned one key earlier. With it, only the
  // last keys are looked at again and runs well before the end stay as they
  // are, which keeps a long composition fast and its beginning stable.
  std::vector<Run> Detect(std::string_view keys, bool end_of_input,
                          const std::vector<Run>* previous = nullptr) const;

  // A word learned as English is English wherever a Japanese word could end
  // before it, also when it is not in the list. One learned as not English
  // never is. |learned| must outlive the detector.
  void set_learned(const LearnedWords* learned) { learned_ = learned; }

  // What the composition shows for |keys| with |runs|: kana, the runs as
  // typed, and letters that are not read yet in full width.
  std::string Display(std::string_view keys, const std::vector<Run>& runs,
                      bool end_of_input) const;
  // The same with the unread letters as typed: what is given to the converter.
  std::string Reading(std::string_view keys, const std::vector<Run>& runs,
                      bool end_of_input) const;

 private:
  // |begins_fresh| is false when |keys| were cut out of a longer composition
  // at a place that may be inside a word: nothing English begins at offset 0.
  std::vector<Run> DetectAll(std::string_view keys, bool end_of_input,
                             bool begins_fresh = true) const;
  int Cost(const std::string& reading) const;

  const EnglishWords* words_;
  const LearnedWords* learned_ = nullptr;
  const RomajiReader* reader_;
  const ReadingJudge* judge_;
  Options options_;
  // Costs asked before. Typing one more key asks for many of the same again.
  mutable absl::flat_hash_map<std::string, int> cost_cache_;
};

}  // namespace english
}  // namespace mozc

#endif  // MOZC_ENGLISH_ENGLISH_DETECTOR_H_
