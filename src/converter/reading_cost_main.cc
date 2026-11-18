// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Prints how natural each reading is as Japanese, for tuning the automatic
// input mode:  bazelisk run //converter:reading_cost_main -- ちめ まで timeをはかる
// Without arguments, reads one reading per line from the standard input.

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "base/init_mozc.h"
#include "converter/converter.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"

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
  const auto print = [&converter](const std::string& reading) {
    std::cout << converter.GetReadingCost(reading) << "\t"
              << (converter.IsReadingPrefixOfWord(reading) ? "prefix" : "-")
              << "\t" << reading << "\n";
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
