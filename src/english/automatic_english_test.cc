#include "english/automatic_english.h"

#include <memory>
#include <string>
#include <vector>

#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "english/english_detector.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace english {
namespace {

TEST(AutomaticEnglishTest, TypeWithRuns) {
  auto table = std::make_shared<composer::Table>();
  ASSERT_TRUE(table->LoadFromFile("system://romanji-hiragana.tsv"));
  auto request = std::make_shared<commands::Request>();
  auto config = std::make_shared<config::Config>(
      config::ConfigHandler::DefaultConfig());
  composer::Composer composer(table, request, config);

  // "n" before the word is "ん", and "nn" is one "ん".
  const struct {
    std::string keys;
    std::vector<EnglishDetector::Run> runs;
    std::string shown;
  } cases[] = {
      {"kyouhagoogledekensaku", {{6, 12}}, "きょうはgoogleでけんさく"},
      {"hangooglede", {{3, 9}}, "はんgoogleで"},
      {"hanngooglede", {{4, 10}}, "はんgoogleで"},
      {"gitdepushsuru", {{0, 3}, {5, 9}}, "gitでpushする"},
      {"kyouhagoog", {{6, 10}}, "きょうはgoog"},
      {"kyouha", {}, "きょうは"},
  };
  for (const auto& c : cases) {
    composer.EditErase();
    TypeWithRuns(c.keys, c.runs, &composer);
    EXPECT_EQ(composer.GetStringForPreedit(), c.shown);
    EXPECT_EQ(composer.GetQueryForConversion(), c.shown);
    EXPECT_EQ(composer.GetRawString(), c.keys);
    EXPECT_EQ(composer.GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(composer.GetCursor(), composer.GetLength());
  }
  // An "n" at the end waits, as it does without English words, and is "ん"
  // for the converter.
  composer.EditErase();
  TypeWithRuns("stringnihenkan", {{0, 6}}, &composer);
  EXPECT_EQ(composer.GetStringForPreedit(), "stringにへんかｎ");
  EXPECT_EQ(composer.GetQueryForConversion(), "stringにへんかん");
}

}  // namespace
}  // namespace english
}  // namespace mozc
