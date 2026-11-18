// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Types romaji through a real session with the real dictionary and the Zenz
// runtime of the Nagi profile, the way the macOS client does: keys first, then
// each delayed callback after its delay. Prints every change of the preview
// with the time since the last key, so live conversion and Zenz correction can
// be checked without installing the input method.
//
//   bazelisk run //session:nagi_typing_main -- kyouhaiitenkidesu

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler.h"
#include "zenz/zenz_client.h"

ABSL_FLAG(bool, zenz, true, "Correct the preview with Zenz.");
ABSL_FLAG(bool, live, true, "Convert while typing.");
ABSL_FLAG(bool, english, false, "Automatic input mode.");
ABSL_FLAG(bool, quiet, false,
          "Print only the keys and what Enter commits, and read more keys "
          "from the standard input.");
ABSL_FLAG(uint32_t, live_delay_msec, 228, "Delay of live conversion.");
ABSL_FLAG(uint32_t, zenz_delay_msec, 500, "Delay before asking Zenz.");
ABSL_FLAG(std::string, profile, "",
          "Profile directory for history and settings. A temporary directory "
          "when empty, so that the real profile is not touched.");

namespace {

using mozc::commands::Command;
using mozc::commands::Input;
using mozc::commands::KeyEvent;
using mozc::commands::Output;

std::string PreeditText(const Output& output) {
  std::string text;
  for (const auto& segment : output.preedit().segment()) {
    text += segment.value();
  }
  return text;
}

const char* StatusName(const Output& output) {
  switch (output.conversion_status()) {
    case Output::CONVERSION_STATUS_READING:
      return "入力中";
    case Output::CONVERSION_STATUS_ENGINE:
      return "Mozc";
    case Output::CONVERSION_STATUS_ZENZ_PENDING:
      return "Zenz…";
    case Output::CONVERSION_STATUS_ZENZ:
      return "Zenz";
    default:
      return "-";
  }
}

class Typist {
 public:
  explicit Typist(std::unique_ptr<mozc::EngineInterface> engine)
      : handler_(std::move(engine)) {
    Command command;
    command.mutable_input()->set_type(Input::CREATE_SESSION);
    handler_.EvalCommand(&command);
    id_ = command.output().id();
  }

  Output SetConfig(const mozc::config::Config& config) {
    Input input;
    input.set_type(Input::SET_CONFIG);
    *input.mutable_config() = config;
    return Eval(std::move(input));
  }

  Output SendKey(const KeyEvent& key) {
    Input input;
    input.set_type(Input::SEND_KEY);
    *input.mutable_key() = key;
    return Eval(std::move(input));
  }

  Output SendCallback(const Output& previous) {
    Input input;
    input.set_type(Input::SEND_COMMAND);
    *input.mutable_command() = previous.callback().session_command();
    return Eval(std::move(input));
  }

 private:
  Output Eval(Input input) {
    input.set_id(id_);
    Command command;
    *command.mutable_input() = std::move(input);
    handler_.EvalCommand(&command);
    return command.output();
  }

  mozc::SessionHandler handler_;
  uint64_t id_ = 0;
};

KeyEvent SpecialKey(KeyEvent::SpecialKey special_key) {
  KeyEvent key;
  key.set_special_key(special_key);
  return key;
}

}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  // The Zenz runtime lives in the real profile. Find it before the profile is
  // switched to a scratch directory.
  mozc::zenz::GetZenzClient();
  std::string profile = absl::GetFlag(FLAGS_profile);
  if (profile.empty()) {
    const char* tmpdir = std::getenv("TMPDIR");
    profile = mozc::FileUtil::JoinPath(tmpdir != nullptr ? tmpdir : "/tmp",
                                       "nagi_typing_main_profile");
  }
  if (!mozc::FileUtil::CreateDirectory(profile).ok() &&
      !mozc::FileUtil::DirectoryExists(profile).ok()) {
    std::cerr << "Cannot create the profile directory: " << profile << std::endl;
    return 1;
  }
  mozc::SystemUtil::SetUserProfileDirectory(profile);

  auto engine = mozc::Engine::CreateEngine(
      std::make_unique<const mozc::oss::OssDataManager>());
  if (!engine.ok()) {
    std::cerr << "Cannot create the engine." << std::endl;
    return 1;
  }
  Typist typist(*std::move(engine));

  mozc::config::Config config = mozc::config::ConfigHandler::DefaultConfig();
  config.set_use_live_conversion(absl::GetFlag(FLAGS_live));
  config.set_use_automatic_english(absl::GetFlag(FLAGS_english));
  config.set_live_conversion_delay_msec(absl::GetFlag(FLAGS_live_delay_msec));
  config.set_use_conversion_status_indicator(true);
  config.set_use_zenz_correction(absl::GetFlag(FLAGS_zenz));
  config.set_zenz_delay_msec(absl::GetFlag(FLAGS_zenz_delay_msec));
  typist.SetConfig(config);
  typist.SendKey(SpecialKey(KeyEvent::ON));

  const bool quiet = absl::GetFlag(FLAGS_quiet);
  std::vector<std::string> inputs(argv + 1, argv + argc);
  if (quiet) {
    std::string line;
    while (std::getline(std::cin, line)) {
      inputs.push_back(line);
    }
  }
  for (const std::string& keys : inputs) {
    if (keys.starts_with("--")) {
      continue;  // A flag that InitMozc left in argv.
    }
    if (!quiet) {
      std::cout << "== " << keys << std::endl;
    }
    Output output;
    int callbacks = 0;
    // "/" is a pause: the delayed callbacks run before the typing goes on.
    for (const absl::string_view chunk : absl::StrSplit(keys, '/')) {
      // "{F10}", "{F6}", "{BS}" and "{SPACE}" are keys; the rest is typed.
      for (size_t i = 0; i < chunk.size(); ++i) {
        KeyEvent key;
        const size_t close = chunk.find('}', i);
        if (chunk[i] == '{' && close != absl::string_view::npos) {
          const absl::string_view name = chunk.substr(i + 1, close - i - 1);
          key = SpecialKey(name == "F10"     ? KeyEvent::F10
                           : name == "F6"    ? KeyEvent::F6
                           : name == "SPACE" ? KeyEvent::SPACE
                                             : KeyEvent::BACKSPACE);
          i = close;
        } else {
          key.set_key_code(chunk[i]);
        }
        output = typist.SendKey(key);
        if (!quiet) {
          std::cout << "     key  " << StatusName(output) << "\t"
                    << PreeditText(output) << std::endl;
        }
      }
      const absl::Time last_key = absl::Now();
      std::string shown = PreeditText(output);
      std::string status = StatusName(output);

      // Follow the delayed callbacks until the session stops asking.
      while (output.has_callback() && callbacks < 1000) {
        absl::SleepFor(absl::Milliseconds(output.callback().delay_millisec()));
        output = typist.SendCallback(output);
        ++callbacks;
        const std::string now_shown = PreeditText(output);
        const std::string now_status = StatusName(output);
        if (now_shown != shown || now_status != status) {
          shown = now_shown;
          status = now_status;
          const int64_t msec =
              absl::ToInt64Milliseconds(absl::Now() - last_key);
          if (!quiet) {
            std::cout.width(6);
            std::cout << msec << "ms  " << status << "\t" << shown
                      << std::endl;
          }
        }
      }
    }

    output = typist.SendKey(SpecialKey(KeyEvent::ENTER));
    if (quiet) {
      std::cout << keys << "\t" << output.result().value() << std::endl;
      continue;
    }
    std::cout << "  commit  " << output.result().value() << "  (" << callbacks
              << " callbacks)" << std::endl;
  }
  return 0;
}
