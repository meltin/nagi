// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Prints how natural each reading is as Japanese, for tuning the automatic
// input mode:  bazelisk run //converter:reading_cost_main -- ちめ まで timeをはかる
// Without arguments, reads one reading per line from the standard input.

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "converter/converter.h"
#include "converter/segments.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"

ABSL_FLAG(bool, reverse, false,
          "Print the reading of each line of text instead (reverse conversion), "
          "for turning ordinary sentences into test readings.");

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  auto engine = mozc::Engine::CreateEngine(
      std::make_unique<const mozc::oss::OssDataManager>());
  if (!engine.ok()) {
    std::cerr << "Cannot create the engine." << std::endl;
    return 1;
  }
  const mozc::converter::Converter& converter =
      (*engine)->GetNagiConverterForTesting();
  if (absl::GetFlag(FLAGS_reverse)) {
    std::string text;
    while (std::getline(std::cin, text)) {
      mozc::Segments segments;
      std::string reading;
      if (converter.StartReverseConversion(&segments, text)) {
        for (const mozc::Segment& segment : segments.conversion_segments()) {
          if (segment.candidates_size() > 0) {
            reading += segment.candidate(0).value;
          }
        }
      }
      std::cout << reading << "\n";
    }
    return 0;
  }
  // cost, whether a longer word starts with it, the reading, and the reading
  // cut where the converter cuts it.
  const auto print = [&converter](const std::string& reading) {
    std::vector<size_t> ends;
    const int cost = converter.GetReadingCost(reading, &ends);
    std::string cut;
    size_t begin = 0;
    for (const size_t end : ends) {
      cut += (cut.empty() ? "" : "|") + reading.substr(begin, end - begin);
      begin = end;
    }
    std::cout << cost << "\t"
              << (converter.IsReadingPrefixOfWord(reading) ? "prefix" : "-")
              << "\t" << reading << "\t" << cut << "\n";
  };
  for (int i = 1; i < argc; ++i) {
    print(argv[i]);
  }
  if (argc == 1) {
    // One reading per line, for measuring many at once.
    std::string reading;
    while (std::getline(std::cin, reading)) {
      print(reading);
    }
  }
  return 0;
}
