// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Converts the readings given as arguments with the Zenz runtime in the Nagi
// profile, and prints each result with the time it took.

#include <cstdint>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/init_mozc.h"
#include "zenz/zenz_client.h"

ABSL_FLAG(std::string, context, "", "Text before the reading.");

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  mozc::zenz::ZenzClientInterface* client = mozc::zenz::GetZenzClient();
  int failures = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string reading = argv[i];
    if (reading.starts_with("--")) {
      continue;  // A flag that InitMozc left in argv.
    }
    const uint32_t id = i;
    const absl::Time start = absl::Now();
    client->Request(id, reading, absl::GetFlag(FLAGS_context));
    mozc::zenz::ZenzResult result = client->Take(id);
    while (result.state == mozc::zenz::ZenzResult::PENDING &&
           absl::Now() - start < absl::Seconds(20)) {
      absl::SleepFor(absl::Milliseconds(5));
      result = client->Take(id);
    }
    const int64_t msec = absl::ToInt64Milliseconds(absl::Now() - start);
    if (result.state != mozc::zenz::ZenzResult::READY) {
      std::cout << reading << "\tFAILED\t" << msec << "ms" << std::endl;
      ++failures;
      continue;
    }
    std::cout << reading << "\t" << result.text << "\t" << msec << "ms\t"
              << (mozc::zenz::IsAcceptableZenzOutput(reading, result.text)
                      ? "accepted"
                      : "rejected")
              << std::endl;
  }
  return failures == 0 ? 0 : 1;
}
