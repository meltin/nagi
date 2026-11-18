// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Types keys one at a time and prints what the automatic input mode would
// show after each key, for measuring it (tools/english/measure).
//   echo kyouhagoogledekensaku | bazel-bin/english/english_detector_main --words=PATH
// Output: keys, then one display per key, separated by tabs.

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "composer/table.h"
#include "converter/converter.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"
#include "english/english_detector.h"

ABSL_FLAG(std::string, words, "", "Path of english_words.tsv.");
ABSL_FLAG(int32_t, unreadable_margin, -1, "Overrides the option when >= 0.");
ABSL_FLAG(int32_t, known_word_margin_bonus, -1, "Overrides the option when >= 0.");
ABSL_FLAG(int32_t, natural_segment_cost, -1, "Overrides the option when >= 0.");
ABSL_FLAG(int32_t, window, -1, "Overrides the option when >= 0.");
ABSL_FLAG(int32_t, settled, -1, "Overrides the option when >= 0.");

namespace {
class ConverterJudge : public mozc::english::ReadingJudge {
 public:
  explicit ConverterJudge(const mozc::converter::Converter* converter)
      : converter_(converter) {}
  int Cost(std::string_view reading) const override {
    return converter_->GetReadingCost(reading);
  }
  bool IsPrefixOfWord(std::string_view reading) const override {
    return converter_->IsReadingPrefixOfWord(reading);
  }
  std::vector<Segment> Segments(std::string_view reading) const override {
    std::vector<size_t> ends;
    std::vector<int> costs;
    converter_->GetReadingCost(reading, &ends, &costs);
    std::vector<Segment> segments;
    for (size_t i = 0; i < ends.size(); ++i) {
      segments.push_back({ends[i], costs[i]});
    }
    return segments;
  }

 private:
  const mozc::converter::Converter* converter_;
};
}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  auto engine = mozc::Engine::CreateEngine(
      std::make_unique<const mozc::oss::OssDataManager>());
  mozc::composer::Table table;
  if (!engine.ok() || !table.LoadFromFile("system://romanji-hiragana.tsv")) {
    std::cerr << "Cannot create the engine or load the romaji table." << std::endl;
    return 1;
  }
  const mozc::english::RomajiReader reader(&table);
  mozc::english::EnglishWords words;
  if (!words.LoadFromFile(absl::GetFlag(FLAGS_words))) {
    std::cerr << "Cannot read --words." << std::endl;
    return 1;
  }
  words.RemoveRomajiNames(reader);

  const ConverterJudge judge(&(*engine)->GetNagiConverterForTesting());
  mozc::english::EnglishDetector detector(&words, &reader, &judge);
  mozc::english::EnglishDetector::Options options;
  if (absl::GetFlag(FLAGS_unreadable_margin) >= 0) {
    options.unreadable_margin = absl::GetFlag(FLAGS_unreadable_margin);
  }
  if (absl::GetFlag(FLAGS_known_word_margin_bonus) >= 0) {
    options.known_word_margin_bonus =
        absl::GetFlag(FLAGS_known_word_margin_bonus);
  }
  if (absl::GetFlag(FLAGS_natural_segment_cost) >= 0) {
    options.natural_segment_cost = absl::GetFlag(FLAGS_natural_segment_cost);
  }
  if (absl::GetFlag(FLAGS_settled) >= 0) {
    options.settled = absl::GetFlag(FLAGS_settled);
  }
  if (absl::GetFlag(FLAGS_window) >= 0) {
    options.window = absl::GetFlag(FLAGS_window);
  }
  detector.set_options(options);

  std::string keys;
  int64_t total_micros = 0;
  int64_t max_micros = 0;
  int64_t key_count = 0;
  while (std::getline(std::cin, keys)) {
    std::cout << keys;
    std::vector<mozc::english::EnglishDetector::Run> runs;
    for (size_t typed = 1; typed <= keys.size(); ++typed) {
      const std::string_view prefix = std::string_view(keys).substr(0, typed);
      const auto start = std::chrono::steady_clock::now();
      // The last display is what is converted or committed.
      // As the session does: every key continues from the previous result.
      const std::vector<mozc::english::EnglishDetector::Run> previous = runs;
      runs = detector.Detect(prefix, typed == keys.size(), &previous);
      const int64_t micros =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - start)
              .count();
      total_micros += micros;
      max_micros = std::max(max_micros, micros);
      ++key_count;
      std::cout << '\t' << detector.Display(prefix, runs, typed == keys.size());
    }
    std::cout << '\n';
  }
  std::cerr << "keys " << key_count << "  mean "
            << (key_count ? total_micros / key_count : 0) << " us  max "
            << max_micros << " us" << std::endl;
  return 0;
}
