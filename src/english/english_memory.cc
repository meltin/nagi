#include "english/english_memory.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace english {
namespace {

constexpr char kFileName[] = "english_words.tsv";
constexpr char kEnglish[] = "E";
constexpr char kNotEnglish[] = "J";
// The oldest words are dropped beyond this.
constexpr size_t kMaxWords = 3000;

bool IsStorable(std::string_view word) {
  return word.size() >= 3 && word.size() <= 24 &&
         std::all_of(word.begin(), word.end(), [](char c) {
           return absl::ascii_islower(static_cast<unsigned char>(c));
         });
}

class Store {
 public:
  LearnedWords Get() {
    absl::MutexLock lock(&mutex_);
    Load();
    return LearnedWords(words_.begin(), words_.end());
  }

  void Learn(std::string_view word, bool english) {
    if (!IsStorable(word)) {
      return;
    }
    absl::MutexLock lock(&mutex_);
    Load();
    std::erase_if(words_, [&](const auto& entry) { return entry.first == word; });
    words_.emplace_back(word, english);
    while (words_.size() > kMaxWords) {
      words_.pop_front();
    }
    Save();
  }

  void Clear() {
    absl::MutexLock lock(&mutex_);
    Load();
    words_.clear();
    Save();
  }

 private:
  // Reads the file of the current profile. The profile directory can change
  // in tests, so the path is checked on every use.
  void Load() {
    std::string path =
        FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kFileName);
    if (path == path_) {
      return;
    }
    path_ = std::move(path);
    words_.clear();
    std::ifstream file(path_);
    std::string line;
    while (std::getline(file, line)) {
      const std::vector<std::string_view> fields = absl::StrSplit(line, '\t');
      if (fields.size() != 2 || !IsStorable(fields[0]) ||
          (fields[1] != kEnglish && fields[1] != kNotEnglish)) {
        continue;  // Not ours, or damaged. Dropped on the next save.
      }
      words_.emplace_back(fields[0], fields[1] == kEnglish);
    }
  }

  // Writes to a temporary file first, so that a crash leaves the old file.
  void Save() const {
    const std::string temporary = path_ + ".tmp";
    {
      std::ofstream file(temporary, std::ios::trunc);
      for (const auto& [word, english] : words_) {
        file << word << '\t' << (english ? kEnglish : kNotEnglish) << '\n';
      }
      if (!file.good()) {
        return;
      }
    }
    FileUtil::AtomicRename(temporary, path_).IgnoreError();
  }

  absl::Mutex mutex_;
  std::string path_;
  std::deque<std::pair<std::string, bool>> words_;
};

Store& GetStore() {
  static absl::NoDestructor<Store> store;
  return *store;
}

}  // namespace

LearnedWords GetLearnedEnglishWords() { return GetStore().Get(); }

void LearnEnglishWord(std::string_view word, bool english) {
  GetStore().Learn(word, english);
}

void ClearLearnedEnglishWords() { GetStore().Clear(); }

}  // namespace english
}  // namespace mozc
