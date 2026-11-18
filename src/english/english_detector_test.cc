// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "english/english_detector.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "composer/table.h"
#include "testing/gunit.h"

namespace mozc {
namespace english {
namespace {

// Costs given by the test. A reading that is not listed costs 3000 for each
// byte, so that a longer reading costs more; ASCII letters cost a lot, as they
// do in the converter.
class FakeJudge : public ReadingJudge {
 public:
  int Cost(std::string_view reading) const override {
    const auto it = costs_.find(reading);
    if (it != costs_.end()) {
      return it->second;
    }
    int cost = 0;
    for (const char c : reading) {
      cost += static_cast<unsigned char>(c) < 0x80 ? 4000 : 1000;
    }
    return cost;
  }
  bool IsPrefixOfWord(std::string_view reading) const override {
    return prefixes_.contains(std::string(reading));
  }
  // The whole reading is one costly segment, unless the test says otherwise.
  std::vector<Segment> Segments(std::string_view reading) const override {
    const auto it = segments_.find(reading);
    if (it != segments_.end()) {
      return it->second;
    }
    return {{reading.size(), 20000}};
  }
  std::map<std::string, std::vector<Segment>, std::less<>> segments_;

  std::map<std::string, int, std::less<>> costs_;
  std::map<std::string, bool> prefixes_;
};

class EnglishDetectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(table_.LoadFromFile("system://romanji-hiragana.tsv"));
    for (const char* word : {"google", "git", "time", "made", "sur", "advance",
                             "dva", "accept", "github", "push"}) {
      words_.Add(word, {.scowl_size = 35, .in_cspell = true, .in_dictionary = true});
    }
  }

  std::string Show(std::string_view keys, bool end_of_input = false) {
    const RomajiReader reader(&table_);
    const EnglishDetector detector(&words_, &reader, &judge_);
    return detector.Display(keys, detector.Detect(keys, end_of_input),
                            end_of_input);
  }

  composer::Table table_;
  EnglishWords words_;
  FakeJudge judge_;
};

TEST_F(EnglishDetectorTest, RomajiReader) {
  const RomajiReader reader(&table_);
  EXPECT_EQ(reader.ToKana("kyouha", true), "きょうは");
  EXPECT_EQ(reader.ToKana("kan", true), "かん");
  EXPECT_EQ(reader.ToKana("googl", true), "ごおgl");
  EXPECT_TRUE(reader.ReadsAsKana("timewohakaru"));
  EXPECT_FALSE(reader.ReadsAsKana("google"));
  // Consonants that can still become a syllable.
  EXPECT_EQ(reader.WaitingLength("kak"), 1);
  EXPECT_EQ(reader.WaitingLength("kaky"), 2);
  EXPECT_EQ(reader.WaitingLength("itt"), 2);
  EXPECT_EQ(reader.WaitingLength("kaka"), 0);
  // "gl" is not the beginning of a syllable.
  EXPECT_EQ(reader.WaitingLength("googl"), 0);
  // Letters before that do not read are not its business.
  EXPECT_EQ(reader.WaitingLength("acceptnisur"), 1);
}

TEST_F(EnglishDetectorTest, WordList) {
  EXPECT_NE(words_.Find("google"), nullptr);
  EXPECT_EQ(words_.Find("goog"), nullptr);
  EXPECT_TRUE(words_.HasWordStartingWith("goog"));
  EXPECT_FALSE(words_.HasWordStartingWith("gooq"));
}

TEST_F(EnglishDetectorTest, WordThatDoesNotReadAsKanaIsEnglish) {
  EXPECT_EQ(Show("kyouhagoogledekensaku", true), "きょうはgoogleでけんさく");
  // Two words in one input.
  EXPECT_EQ(Show("githubnipushsita", true), "githubにpushした");
}

TEST_F(EnglishDetectorTest, HalfTypedWordIsShownAsTyped) {
  // "gl" cannot be Japanese, and "googl" begins a word.
  EXPECT_EQ(Show("kyouhagoogl"), "きょうはgoogl");
  // From where the word begins, not a shorter word inside it.
  EXPECT_EQ(Show("adva"), "adva");
}

TEST_F(EnglishDetectorTest, JapaneseStaysKana) {
  EXPECT_EQ(Show("kyoumadeniyaru", true), "きょうまでにやる");
  // A consonant at the end is the next syllable while typing.
  EXPECT_EQ(Show("kyouhasur"), "きょうはすｒ");
  EXPECT_EQ(Show("git"), "ぎｔ");
}

TEST_F(EnglishDetectorTest, WordDoesNotBeginInsideASyllable) {
  words_.Add("amer", {.scowl_size = 35});
  // "na" is one syllable: "amer" does not begin after its "n".
  EXPECT_EQ(Show("namerakana", true), "なめらかな");
}

TEST_F(EnglishDetectorTest, WordDoesNotBeginInsideAJapaneseWord) {
  words_.Add("rase", {.scowl_size = 35});
  judge_.costs_["らせ"] = 20000;
  // The converter keeps "しらせが" in one piece, which is ordinary Japanese.
  judge_.segments_["しらせがとどいた"] = {{12, 5000}, {24, 5000}};
  EXPECT_EQ(Show("sirasegatodoita", true), "しらせがとどいた");
}

TEST_F(EnglishDetectorTest, EndOfInputDecidesWaitingConsonants) {
  // Nothing more is coming: "git" cannot end in a syllable.
  EXPECT_EQ(Show("git", true), "git");
}

TEST_F(EnglishDetectorTest, ReadableWordNeedsMoreThanBeingInTheList) {
  // "made" reads as "まで", which is ordinary Japanese.
  judge_.costs_["まで"] = 3500;
  judge_.costs_["までいく"] = 9000;
  judge_.segments_["までいく"] = {{6, 3500}, {12, 4500}};
  EXPECT_EQ(Show("madeiku", true), "までいく");
  // "time" reads as "ちめ", which is hardly a word, and "timeをはかる" is not
  // much worse than "ちめをはかる".
  judge_.costs_["ちめ"] = 13000;
  judge_.costs_["ちめをはかる"] = 15000;
  judge_.costs_["timeをはかる"] = 24000;
  EXPECT_EQ(Show("timewohakaru", true), "timeをはかる");
  // Not while a longer Japanese word may be coming.
  judge_.prefixes_["ちめ"] = true;
  EXPECT_EQ(Show("time"), "ちめ");
}

TEST_F(EnglishDetectorTest, LongCompositionKeepsEarlierRuns) {
  const RomajiReader reader(&table_);
  const EnglishDetector detector(&words_, &reader, &judge_);
  std::string keys = "kyouhagoogledekensaku";
  std::vector<EnglishDetector::Run> runs;
  // Type a long tail of Japanese after the English word, key by key.
  const std::string tail = "siteatodekakuninsitekarakaerimasu"
                           "sorekaramouitidokakuninsimasu";
  runs = detector.Detect(keys, false, nullptr);
  for (const char c : tail) {
    keys.push_back(c);
    const std::vector<EnglishDetector::Run> previous = runs;
    runs = detector.Detect(keys, false, &previous);
    // The word stays through every key, also while the window passes it.
    ASSERT_FALSE(runs.empty()) << keys;
    ASSERT_EQ(runs.front(), (EnglishDetector::Run{6, 12})) << keys;
  }
  ASSERT_GT(keys.size(), 60);
  ASSERT_FALSE(runs.empty());
  // "google" is far outside the window by now, and still English.
  EXPECT_EQ(runs.front(), (EnglishDetector::Run{6, 12}));
}

TEST_F(EnglishDetectorTest, LongCompositionKeepsARunAfterN) {
  const RomajiReader reader(&table_);
  const EnglishDetector detector(&words_, &reader, &judge_);
  // "nn" is "ん", and the word begins right after it.
  std::string keys = "jikanngoogledekensaku";
  std::vector<EnglishDetector::Run> runs = detector.Detect(keys, false);
  ASSERT_FALSE(runs.empty());
  const EnglishDetector::Run word = runs.front();
  for (const char c : std::string("siteatodekakuninsitekarakaerimasu"
                                  "sorekaramouitidokakuninsimasu")) {
    keys.push_back(c);
    const std::vector<EnglishDetector::Run> previous = runs;
    runs = detector.Detect(keys, false, &previous);
    ASSERT_FALSE(runs.empty()) << keys;
    ASSERT_EQ(runs.front(), word) << keys;
  }
}

TEST_F(EnglishDetectorTest, OnlyLowerCaseLetters) {
  // Capitals and digits are the session's business: the composer already
  // switches to ASCII on a capital.
  const RomajiReader reader(&table_);
  const EnglishDetector detector(&words_, &reader, &judge_);
  EXPECT_TRUE(detector.Detect("kyouha1google", true).empty());
  EXPECT_TRUE(detector.Detect("kyouhaGoogle", true).empty());
}

}  // namespace
}  // namespace english
}  // namespace mozc
