// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "zenz/zenz_feedback.h"

#include <cstddef>
#include <deque>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace zenz {
namespace {

constexpr char kFileName[] = "zenz_feedback.tsv";
constexpr char kAccepted[] = "A";
constexpr char kRejected[] = "R";
// The oldest entries are dropped beyond this. A linear scan of this many short
// strings takes microseconds.
constexpr size_t kMaxEntries = 5000;

struct Entry {
  bool accepted;
  std::string reading;
  std::string text;
};

// Tabs and newlines would break the file. Neither appears in a reading or in
// an accepted Zenz output, so such a pair is simply not remembered.
bool IsStorable(std::string_view reading, std::string_view text) {
  constexpr std::string_view kSeparators = "\t\r\n";
  return !reading.empty() && !text.empty() &&
         reading.find_first_of(kSeparators) == std::string_view::npos &&
         text.find_first_of(kSeparators) == std::string_view::npos;
}

class Store {
 public:
  std::optional<std::string> Accepted(std::string_view reading) {
    absl::MutexLock lock(&mutex_);
    Load();
    // The newest verdict wins.
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
      if (it->accepted && it->reading == reading) {
        return it->text;
      }
    }
    return std::nullopt;
  }

  bool IsRejected(std::string_view reading, std::string_view text) {
    absl::MutexLock lock(&mutex_);
    Load();
    for (const Entry& entry : entries_) {
      if (!entry.accepted && entry.reading == reading && entry.text == text) {
        return true;
      }
    }
    return false;
  }

  void Record(bool accepted, std::string_view reading, std::string_view text) {
    if (!IsStorable(reading, text)) {
      return;
    }
    absl::MutexLock lock(&mutex_);
    Load();
    // One verdict per pair, and one accepted text per reading.
    std::erase_if(entries_, [&](const Entry& entry) {
      return entry.reading == reading &&
             (entry.text == text || (accepted && entry.accepted));
    });
    entries_.push_back({accepted, std::string(reading), std::string(text)});
    while (entries_.size() > kMaxEntries) {
      entries_.pop_front();
    }
    Save();
  }

 private:
  // Reads the file of the current profile. The profile directory can change
  // in tests, so the path is checked on every use.
  void Load() {
    std::string path = FileUtil::JoinPath(
        SystemUtil::GetUserProfileDirectory(), kFileName);
    if (path == path_) {
      return;
    }
    path_ = std::move(path);
    entries_.clear();
    std::ifstream file(path_);
    std::string line;
    while (std::getline(file, line)) {
      const std::vector<std::string_view> fields =
          absl::StrSplit(line, '\t');
      if (fields.size() != 3 ||
          (fields[0] != kAccepted && fields[0] != kRejected) ||
          !IsStorable(fields[1], fields[2])) {
        continue;  // Not ours, or damaged. Dropped on the next save.
      }
      entries_.push_back({fields[0] == kAccepted, std::string(fields[1]),
                          std::string(fields[2])});
    }
  }

  // Writes to a temporary file first, so that a crash leaves the old file.
  void Save() const {
    const std::string temporary = path_ + ".tmp";
    {
      std::ofstream file(temporary, std::ios::trunc);
      for (const Entry& entry : entries_) {
        file << (entry.accepted ? kAccepted : kRejected) << '\t'
             << entry.reading << '\t' << entry.text << '\n';
      }
      if (!file.good()) {
        return;
      }
    }
    FileUtil::AtomicRename(temporary, path_).IgnoreError();
  }

  absl::Mutex mutex_;
  std::string path_;
  std::deque<Entry> entries_;
};

Store& GetStore() {
  static absl::NoDestructor<Store> store;
  return *store;
}

}  // namespace

std::optional<std::string> AcceptedZenzCorrection(std::string_view reading) {
  return GetStore().Accepted(reading);
}

bool IsRejectedZenzCorrection(std::string_view reading, std::string_view text) {
  return GetStore().IsRejected(reading, text);
}

void AcceptZenzCorrection(std::string_view reading, std::string_view text) {
  GetStore().Record(true, reading, text);
}

void RejectZenzCorrection(std::string_view reading, std::string_view text) {
  GetStore().Record(false, reading, text);
}

}  // namespace zenz
}  // namespace mozc
