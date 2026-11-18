// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "english/english_detector.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "composer/table.h"

namespace mozc {
namespace english {
namespace {

constexpr size_t kMinWordLength = 3;
constexpr size_t kMaxWordLength = 24;
constexpr size_t kMaxCachedCosts = 20000;

// Lower-case letters, and "-", which types the long vowel mark.
bool IsLowerLetters(std::string_view keys) {
  return std::all_of(keys.begin(), keys.end(), [](char c) {
    return absl::ascii_islower(static_cast<unsigned char>(c)) || c == '-';
  });
}

}  // namespace

bool EnglishWords::LoadFromFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }
  std::stringstream text;
  text << file.rdbuf();
  LoadFromString(text.str());
  return true;
}

void EnglishWords::LoadFromString(std::string_view text) {
  for (const std::string_view line : absl::StrSplit(text, '\n')) {
    const std::vector<std::string_view> fields = absl::StrSplit(line, '\t');
    Info info;
    if (fields.size() != 3 || !absl::SimpleAtoi(fields[1], &info.scowl_size)) {
      continue;
    }
    info.in_cspell = fields[2].find('c') != std::string_view::npos;
    info.in_dictionary = fields[2].find('d') != std::string_view::npos;
    Add(fields[0], info);
  }
}

void EnglishWords::RemoveRomajiNames(const RomajiReader& reader) {
  std::erase_if(words_, [&reader](const auto& entry) {
    return entry.second.scowl_size == 0 && !entry.second.in_cspell &&
           reader.ReadsAsKana(entry.first);
  });
}

void EnglishWords::Add(std::string_view word, const Info& info) {
  words_[std::string(word)] = info;
}

void EnglishWords::Remove(std::string_view word) {
  const auto it = words_.find(word);
  if (it != words_.end()) {
    words_.erase(it);
  }
}

const EnglishWords::Info* EnglishWords::Find(std::string_view word) const {
  const auto it = words_.find(word);
  return it == words_.end() ? nullptr : &it->second;
}

bool EnglishWords::HasWordStartingWith(std::string_view prefix) const {
  const auto it = words_.lower_bound(prefix);
  return it != words_.end() && it->first.starts_with(prefix);
}

std::string RomajiReader::ToKana(std::string_view keys,
                                 bool end_of_input) const {
  std::string kana;
  std::string rest(keys);
  while (!rest.empty()) {
    size_t key_length = 0;
    bool fixed = false;
    const composer::Entry* entry =
        table_->LookUpPrefix(rest, &key_length, &fixed);
    if (entry == nullptr || key_length == 0) {
      kana.push_back(rest.front());
      rest.erase(0, 1);
      continue;
    }
    kana.append(entry->result());
    rest = std::string(entry->pending()) + rest.substr(key_length);
  }
  if (end_of_input && !kana.empty() && kana.back() == 'n') {
    kana.pop_back();
    kana.append("ん");
  }
  return kana;
}

bool RomajiReader::ReadsAsKana(std::string_view keys, bool end_of_input) const {
  const std::string kana = ToKana(keys, end_of_input);
  return std::none_of(kana.begin(), kana.end(), [](char c) {
    return absl::ascii_isalpha(static_cast<unsigned char>(c));
  });
}

bool RomajiReader::IsUnfinished(std::string_view keys) const {
  std::vector<const composer::Entry*> entries;
  table_->LookUpPredictiveAll(keys, &entries);
  return std::any_of(entries.begin(), entries.end(),
                     [keys](const composer::Entry* entry) {
                       return entry->input().size() > keys.size() ||
                              !entry->pending().empty();
                     });
}

size_t RomajiReader::WaitingLength(std::string_view keys) const {
  const std::string kana = ToKana(keys, /*end_of_input=*/false);
  size_t waiting = 0;
  while (waiting < kana.size() &&
         absl::ascii_isalpha(
             static_cast<unsigned char>(kana[kana.size() - 1 - waiting]))) {
    ++waiting;
  }
  if (waiting == 0 ||
      !IsUnfinished(std::string_view(kana).substr(kana.size() - waiting))) {
    return 0;
  }
  // The kana may be shorter than the keys ("tt" is a small tsu and a "t").
  // Earlier letters that do not read are not this function's business.
  for (size_t n = waiting; n <= std::min<size_t>(4, keys.size()); ++n) {
    const std::string head =
        ToKana(keys.substr(0, keys.size() - n), /*end_of_input=*/false);
    if (head.empty() ||
        !absl::ascii_isalpha(static_cast<unsigned char>(head.back()))) {
      return n;
    }
  }
  return 0;
}

int EnglishDetector::Cost(const std::string& reading) const {
  const auto it = cost_cache_.find(reading);
  if (it != cost_cache_.end()) {
    return it->second;
  }
  if (cost_cache_.size() >= kMaxCachedCosts) {
    cost_cache_.clear();
  }
  const int cost = judge_->Cost(reading);
  cost_cache_.emplace(reading, cost);
  return cost;
}

namespace {
// Letters that are not read yet are shown full-width, as the composer shows
// them, which also tells them from the English runs in a display.
std::string WidenLetters(std::string_view kana) {
  std::string out;
  for (const char c : kana) {
    if (absl::ascii_islower(static_cast<unsigned char>(c))) {
      const char32_t wide = 0xFF41 + (c - 'a');  // U+FF41 is "ａ".
      out.push_back(static_cast<char>(0xE0 | (wide >> 12)));
      out.push_back(static_cast<char>(0x80 | ((wide >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (wide & 0x3F)));
    } else {
      out.push_back(c);
    }
  }
  return out;
}
}  // namespace

std::string EnglishDetector::Reading(std::string_view keys,
                                     const std::vector<Run>& runs,
                                     bool end_of_input) const {
  std::string reading;
  size_t position = 0;
  for (const Run& run : runs) {
    // Kana before an English word is complete: an "n" there is "ん".
    reading += reader_->ToKana(keys.substr(position, run.begin - position),
                               /*end_of_input=*/true);
    reading += keys.substr(run.begin, run.end - run.begin);
    position = run.end;
  }
  reading += reader_->ToKana(keys.substr(position), end_of_input);
  return reading;
}

std::string EnglishDetector::Display(std::string_view keys,
                                     const std::vector<Run>& runs,
                                     bool end_of_input) const {
  std::string display;
  size_t position = 0;
  for (const Run& run : runs) {
    display += WidenLetters(reader_->ToKana(
        keys.substr(position, run.begin - position), /*end_of_input=*/true));
    display += keys.substr(run.begin, run.end - run.begin);
    position = run.end;
  }
  display += WidenLetters(reader_->ToKana(keys.substr(position), end_of_input));
  return display;
}

std::vector<EnglishDetector::Run> EnglishDetector::Detect(
    std::string_view keys, bool end_of_input,
    const std::vector<Run>* previous) const {
  if (previous == nullptr || keys.size() <= options_.window) {
    return DetectAll(keys, end_of_input);
  }
  // Keep the runs that ended before the window, and look again at the rest,
  // starting where a syllable starts.
  size_t target = keys.size() - options_.window;
  std::vector<Run> runs;
  // True if the window starts where a word is known to begin.
  bool starts_at_word = false;
  for (const Run& run : *previous) {
    // A run the user has typed well past is settled. Looking at it again with
    // less in front of it every key only risks losing it.
    if (run.end <= target || run.end + options_.settled <= keys.size()) {
      runs.push_back(run);
    } else {
      // A run across the edge of the window is looked at again as a whole.
      if (run.begin <= target) {
        target = run.begin;
      }
      break;
    }
  }
  const size_t origin = runs.empty() ? 0 : runs.back().end;
  size_t begin = std::max(target, origin);
  while (begin < keys.size()) {
    const std::string head =
        reader_->ToKana(keys.substr(origin, begin - origin), false);
    // Not after an "n" that belongs to the next syllable ("na").
    const bool splits_n =
        begin > origin && keys[begin - 1] == 'n' &&
        std::string_view("aiueoyn").find(keys[begin]) != std::string_view::npos;
    if (!splits_n &&
        (head.empty() ||
         !absl::ascii_isalpha(static_cast<unsigned char>(head.back())))) {
      break;
    }
    ++begin;
  }
  // The window starts at a syllable, which may be in the middle of a word,
  // unless it starts right after a run or where a run began before.
  for (const Run& run : *previous) {
    starts_at_word = starts_at_word || run.begin == begin;
  }
  for (const Run& run : DetectAll(keys.substr(begin), end_of_input,
                                  /*begins_fresh=*/begin == origin ||
                                      starts_at_word)) {
    runs.push_back(Run{run.begin + begin, run.end + begin});
  }
  return runs;
}

std::vector<EnglishDetector::Run> EnglishDetector::DetectAll(
    std::string_view keys, bool end_of_input, bool begins_fresh) const {
  std::vector<Run> runs;
  if (keys.size() < kMinWordLength || !IsLowerLetters(keys)) {
    return runs;
  }
  // The consonants at the end that may become a syllable are left out of the
  // kana reading, unless an English word takes them.
  const size_t waiting = end_of_input ? 0 : reader_->WaitingLength(keys);
  const size_t body_end = keys.size() - waiting;

  // The cost of showing |chosen| as English and the rest as kana. The waiting
  // consonants are dropped when no run takes them. A letter that is neither
  // read as kana nor part of a run makes the reading impossible as Japanese,
  // which the converter does not charge enough for by itself.
  const auto cost_of = [&](const std::vector<Run>& chosen) {
    const bool takes_end = !chosen.empty() && chosen.back().end == keys.size();
    const std::string reading =
        Reading(takes_end ? keys : keys.substr(0, body_end), chosen,
                /*end_of_input=*/true);
    size_t letters = 0;
    for (const char c : reading) {
      letters += absl::ascii_isalpha(static_cast<unsigned char>(c)) ? 1 : 0;
    }
    for (const Run& run : chosen) {
      letters -= run.end - run.begin;
    }
    return Cost(reading) + static_cast<int>(letters) * options_.unread_letter_cost;
  };

  // Where the words taught not to be English are. Nothing English begins
  // inside one: not "tests" or "est" where "test" is kana.
  std::vector<Run> kana_spans;
  if (learned_ != nullptr) {
    for (const auto& [word, english] : *learned_) {
      for (size_t at = english ? std::string_view::npos : keys.find(word);
           at != std::string_view::npos; at = keys.find(word, at + 1)) {
        kana_spans.push_back(Run{at, at + word.size()});
      }
    }
  }
  const auto untaught = [&kana_spans](size_t begin) {
    return std::any_of(kana_spans.begin(), kana_spans.end(),
                       [begin](const Run& span) {
                         return span.begin <= begin && begin < span.end;
                       });
  };

  while (runs.size() < options_.max_runs) {
    const int cost_now = cost_of(runs);
    // Where the converter cuts the reading as it is now. A word of Japanese
    // is not cut by it, so an English word does not begin inside a segment:
    // "rase" is not a word in "しらせ".
    // A run may have taken the consonants waiting at the end.
    const bool run_takes_end = !runs.empty() && runs.back().end == keys.size();
    const std::string reading_now =
        Reading(run_takes_end ? keys : keys.substr(0, body_end), runs,
                /*end_of_input=*/true);
    const std::vector<ReadingJudge::Segment> segments =
        judge_->Segments(reading_now);
    // The offset in |reading_now| of what comes before |key_offset| keys.
    const auto reading_offset = [&](size_t key_offset) {
      std::vector<Run> before;
      for (const Run& run : runs) {
        if (run.end <= key_offset) {
          before.push_back(run);
        }
      }
      return Reading(keys.substr(0, std::min(key_offset, keys.size())), before,
                     /*end_of_input=*/true)
          .size();
    };
    const auto at_segment_edge = [&](size_t key_offset) {
      const size_t offset = reading_offset(key_offset);
      return offset == 0 || offset >= reading_now.size() ||
             std::any_of(segments.begin(), segments.end(),
                         [offset](const ReadingJudge::Segment& segment) {
                           return segment.end == offset;
                         });
    };
    // True if every segment the keys [begin, end) fall in is ordinary Japanese.
    const auto in_natural_segments = [&](size_t begin, size_t end) {
      const size_t from = reading_offset(begin);
      const size_t to = reading_offset(end);
      size_t segment_begin = 0;
      for (const ReadingJudge::Segment& segment : segments) {
        if (segment_begin < to && segment.end > from &&
            segment.cost >= options_.natural_segment_cost) {
          return false;
        }
        segment_begin = segment.end;
      }
      return true;
    };
    int best_delta = 0;
    Run best;
    bool found = false;
    for (size_t index = 0; index <= runs.size(); ++index) {
      // The stretch of kana between two runs, or at either end.
      const size_t stretch_begin = index == 0 ? 0 : runs[index - 1].end;
      const bool last = index == runs.size();
      const size_t stretch_end = last ? keys.size() : runs[index].begin;
      for (size_t begin = stretch_begin; begin + kMinWordLength <= stretch_end;
           ++begin) {
        if (begin == 0 && !begins_fresh) {
          continue;
        }
        if (!reader_->ReadsAsKana(
                keys.substr(stretch_begin, begin - stretch_begin))) {
          continue;
        }
        // An "n" just before is "ん" only if a vowel does not follow: "na" in
        // "nameraka" is "な", not "ん" and a word that begins with "a".
        if (begin > stretch_begin && keys[begin - 1] == 'n' &&
            (begin - stretch_begin < 2 || keys[begin - 2] != 'n') &&
            std::string_view("aiueoy").find(keys[begin]) !=
                std::string_view::npos) {
          continue;
        }
        const size_t max_end = std::min(stretch_end, begin + kMaxWordLength);
        for (size_t end = begin + kMinWordLength; end <= max_end; ++end) {
          const std::string_view word = keys.substr(begin, end - begin);
          const EnglishWords::Info* info = words_->Find(word);
          bool taught = false;
          if (learned_ != nullptr) {
            const auto it = learned_->find(word);
            taught = it != learned_->end() && it->second;
          }
          if (!taught && untaught(begin)) {
            continue;
          }
          static const EnglishWords::Info kTaughtInfo;
          if (info == nullptr) {
            if (!taught) {
              continue;
            }
            info = &kTaughtInfo;
          }
          // The word may take the consonants waiting at the end, but not
          // stop in the middle of them. What follows the word need not read
          // as kana: another English word may be there, and letters left
          // unread are charged for in the cost.
          const bool takes_waiting = last && end == keys.size() && waiting > 0;
          if (last && end > body_end && !takes_waiting) {
            continue;
          }
          // While the keys from the word's beginning still read as kana, a
          // consonant at the end is the next syllable, not the end of an
          // English word ("sur" in "suru").
          if (takes_waiting &&
              reader_->ReadsAsKana(keys.substr(begin, body_end - begin))) {
            continue;
          }
          // Two runs side by side would be one unknown word to the converter,
          // which makes any pair look cheap.
          if ((index > 0 && begin == stretch_begin) ||
              (!last && end == stretch_end)) {
            continue;
          }
          std::vector<Run> with = runs;
          with.insert(with.begin() + index, Run{begin, end});
          const int delta = cost_now - cost_of(with);
          const bool readable = reader_->ReadsAsKana(word);
          // While the keys around the word read as kana, Japanese is
          // possible. The word must begin where a Japanese word could end,
          // and what it would replace must not be ordinary Japanese already:
          // the converter cuts "ちめを|はかる" and finds "ちめを" costly, but
          // "まだ|めに" is fine as it is and hides no "madame".
          const size_t kana_end = last ? body_end : stretch_end;
          const bool stretch_reads =
              kana_end >= stretch_begin &&
              reader_->ReadsAsKana(
                  keys.substr(stretch_begin, kana_end - stretch_begin));
          if (stretch_reads &&
              (!at_segment_edge(begin) ||
               // A taught word is judged by its cost below instead.
               (!taught &&
                in_natural_segments(begin, std::min(end, body_end))))) {
            continue;
          }
          bool english = false;
          if (taught) {
            // In the middle of the keys a taught word that reads as kana
            // must still not cost much: "todo" is not in "とどかなかった".
            english = !readable || (last && end >= body_end) ||
                      delta > -options_.taught_margin;
          } else if (!readable) {
            english = delta > -options_.unreadable_margin;
          } else {
            const std::string word_kana =
                reader_->ToKana(word, /*end_of_input=*/true);
            const bool at_end = last && end == body_end && waiting == 0;
            if (!(at_end && judge_->IsPrefixOfWord(word_kana))) {
              const bool known = info->in_cspell && info->in_dictionary;
              const int word_cost =
                  Cost(word_kana) +
                  (known ? options_.known_word_cost_discount : 0);
              const int bonus = known ? options_.known_word_margin_bonus : 0;
              for (const Options::ReadableRule& rule :
                   options_.readable_rules) {
                english = english || (word.size() >= rule.letters &&
                                      word_cost > rule.word_cost &&
                                      delta > -(rule.margin + bonus));
              }
            }
          }
          // Among the words that qualify, a longer one explains more of
          // the keys: "accept" rather than "ptm" after "acce" read as kana.
          const int score =
              delta + static_cast<int>(word.size()) * options_.letter_bonus;
          if (english && (!found || score > best_delta)) {
            found = true;
            best_delta = score;
            best = Run{begin, end};
          }
        }
      }
    }
    if (!found) {
      break;
    }
    runs.insert(std::upper_bound(runs.begin(), runs.end(), best,
                                 [](const Run& a, const Run& b) {
                                   return a.begin < b.begin;
                                 }),
                best);
  }

  // What is left at the end and cannot be Japanese as it stands may be an
  // English word half typed: show it as typed from where it begins. It may
  // begin before runs chosen above, which were then words inside it ("dva"
  // while typing "advance").
  for (size_t index = 0; index <= runs.size(); ++index) {
    // A run followed by kana or by a consonant in progress is settled:
    // "absolute" and then "n" is not the beginning of "absoluteness".
    if (index < runs.size() && runs.back().end != keys.size()) {
      continue;
    }
    const size_t tail_begin = index == 0 ? 0 : runs[index - 1].end;
    const std::string_view tail = keys.substr(tail_begin);
    if (tail.size() < kMinWordLength ||
        reader_->ReadsAsKana(tail.substr(
            0,
            tail.size() - (end_of_input ? 0 : reader_->WaitingLength(tail))))) {
      continue;
    }
    for (size_t begin = 0; begin + kMinWordLength <= tail.size(); ++begin) {
      if (tail_begin + begin == 0 && !begins_fresh) {
        continue;
      }
      if (index < runs.size() && tail_begin + begin > runs[index].begin) {
        break;  // It would start inside a run that explains more.
      }
      if (untaught(tail_begin + begin)) {
        continue;
      }
      if (words_->HasWordStartingWith(tail.substr(begin)) &&
          reader_->ReadsAsKana(tail.substr(0, begin))) {
        runs.resize(index);
        runs.push_back(Run{tail_begin + begin, keys.size()});
        return runs;
      }
    }
  }
  return runs;
}

}  // namespace english
}  // namespace mozc
