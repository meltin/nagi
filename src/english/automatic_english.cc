#include "english/automatic_english.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "base/embedded_file.h"
#include "composer/composer.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace english {
namespace {
// data/english_nagi/english_words.tsv as kEnglishWords.
#include "english/english_words.inc"
}  // namespace

int ConverterJudge::Cost(std::string_view reading) const {
  return converter_->GetReadingCost(reading);
}

bool ConverterJudge::IsPrefixOfWord(std::string_view reading) const {
  return converter_->IsReadingPrefixOfWord(reading);
}

std::vector<ReadingJudge::Segment> ConverterJudge::Segments(
    std::string_view reading) const {
  std::vector<size_t> ends;
  std::vector<int> costs;
  converter_->GetReadingCost(reading, &ends, &costs);
  std::vector<Segment> segments;
  for (size_t i = 0; i < ends.size() && i < costs.size(); ++i) {
    segments.push_back({ends[i], costs[i]});
  }
  return segments;
}

const EnglishWords& GetEnglishWords(const RomajiReader& reader) {
  static const EnglishWords* words = [&reader]() {
    auto* loaded = new EnglishWords();
    loaded->LoadFromString(LoadEmbeddedFile(kEnglishWords));
    loaded->RemoveRomajiNames(reader);
    return loaded;
  }();
  return *words;
}

void TypeWithRuns(std::string_view keys,
                  const std::vector<EnglishDetector::Run>& runs,
                  composer::Composer* composer) {
  size_t position = 0;
  const auto type_kana = [&](size_t end, bool before_run) {
    const std::string_view stretch = keys.substr(position, end - position);
    // An "n" still waiting for a vowel is "ん" before an English word. The
    // composer would keep it waiting. "nn" is "ん" already, so it waits when
    // the stretch ends in an odd number of them.
    const size_t last_n = stretch.find_last_not_of('n');
    const size_t n_count =
        stretch.size() - (last_n == std::string_view::npos ? 0 : last_n + 1);
    for (size_t i = 0; i < stretch.size(); ++i) {
      if (before_run && i + 1 == stretch.size() && n_count % 2 == 1) {
        composer->InsertCharacterKeyAndPreedit("n", "ん");
      } else {
        composer->InsertCharacter(std::string(1, stretch[i]));
      }
    }
    position = end;
  };
  for (const EnglishDetector::Run& run : runs) {
    type_kana(run.begin, /*before_run=*/true);
    composer->SetInputMode(transliteration::HALF_ASCII);
    composer->InsertCharacter(std::string(keys.substr(run.begin, run.end - run.begin)));
    composer->SetInputMode(transliteration::HIRAGANA);
    position = run.end;
  }
  type_kana(keys.size(), /*before_run=*/false);
}

}  // namespace english
}  // namespace mozc
