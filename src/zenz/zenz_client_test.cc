// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "zenz/zenz_client.h"

#include <optional>
#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace zenz {
namespace {

TEST(ZenzClientTest, IsZenzReading) {
  EXPECT_TRUE(IsZenzReading("きょうはいいてんきです"));
  EXPECT_TRUE(IsZenzReading("こーひー、のむ？"));
  EXPECT_FALSE(IsZenzReading(""));
  EXPECT_FALSE(IsZenzReading("きょうはgoogle"));
  EXPECT_FALSE(IsZenzReading("きょう1にち"));
  EXPECT_FALSE(IsZenzReading("今日"));
}

TEST(ZenzClientTest, BuildZenzPrompt) {
  EXPECT_EQ(BuildZenzPrompt("きょう", ""), "キョウ");
  EXPECT_EQ(BuildZenzPrompt("はし", "川の"), "川のハシ");
}

TEST(ZenzClientTest, BuildZenzPromptUsesTheTailOfTheContext) {
  const std::string context = std::string(100, 'a') + "末尾";
  const std::string prompt = BuildZenzPrompt("はし", context);
  EXPECT_EQ(prompt, "" + std::string(38, 'a') + "末尾ハシ");
}

TEST(ZenzClientTest, BuildZenzPromptDropsMarkersFromTheContext) {
  EXPECT_EQ(BuildZenzPrompt("はし", "川の"), "川のハシ");
}

TEST(ZenzClientTest, AcceptsOutputsThatKeepTheReading) {
  EXPECT_TRUE(IsAcceptableZenzOutput("きょうはいいてんきです", "今日はいい天気です"));
  EXPECT_TRUE(IsAcceptableZenzOutput("こーひーをのむ", "コーヒーを飲む"));
  EXPECT_TRUE(IsAcceptableZenzOutput("はしをわたる", "橋を渡る"));
  EXPECT_TRUE(IsAcceptableZenzOutput("そうですね。", "そうですね。"));
}

TEST(ZenzClientTest, RejectsOutputsThatChangeTheReading) {
  // Kana that is not in the reading.
  EXPECT_FALSE(IsAcceptableZenzOutput("きょうはいいてんきです", "今日はいい天気でした"));
  // Kana in a different order.
  EXPECT_FALSE(IsAcceptableZenzOutput("はしをわたる", "をはし渡る"));
  // Far longer than the reading.
  EXPECT_FALSE(IsAcceptableZenzOutput("はし", "橋橋橋橋橋"));
  EXPECT_FALSE(IsAcceptableZenzOutput("はし", ""));
  // Control characters and prompt markers.
  EXPECT_FALSE(IsAcceptableZenzOutput("はし", "橋\n"));
  EXPECT_FALSE(IsAcceptableZenzOutput("はし", "橋"));
}

TEST(ZenzClientTest, ReadingAloneDoesNotCatchTheseOutputs) {
  // Why KeepsEngineText() exists: the kana of these outputs are all in the
  // reading, so the reading check passes them.
  EXPECT_TRUE(IsAcceptableZenzOutput("いっしょにいって", "行って"));
  EXPECT_TRUE(IsAcceptableZenzOutput("とうきょうにいく", "Tokyoに行く"));
}

TEST(ZenzClientTest, KeepsEngineText) {
  // A different choice of kanji or kana is what Zenz is for.
  EXPECT_TRUE(KeepsEngineText("機会を作る", "機械を作る", {}));
  EXPECT_TRUE(KeepsEngineText("橋をわたる", "橋を渡る", {}));
  EXPECT_TRUE(KeepsEngineText("iPhoneを勝った", "iPhoneを買った", {}));
  EXPECT_TRUE(KeepsEngineText("3日目に合う", "3日目に会う", {}));

  // Only the beginning or the end of the engine's text: typed characters
  // would be lost.
  EXPECT_FALSE(KeepsEngineText("一緒に行って", "行って", {}));
  EXPECT_FALSE(KeepsEngineText("一緒に行って", "一緒に", {}));

  // Latin text, digits and emoji are not Zenz's to invent or change.
  EXPECT_FALSE(KeepsEngineText("東京に行く", "Tokyoに行く", {}));
  EXPECT_FALSE(KeepsEngineText("GitHubを使う", "GitLabを使う", {}));
  EXPECT_FALSE(KeepsEngineText("3日目に会う", "5日目に会う", {}));
  EXPECT_FALSE(KeepsEngineText("楽しい", "楽しい😀", {}));
  EXPECT_TRUE(KeepsEngineText("楽しい😀", "愉しい😀", {}));
}

TEST(ZenzClientTest, KeepsEngineTextProtectsUserDictionaryWords) {
  EXPECT_TRUE(KeepsEngineText("凪を仕様する", "凪を使用する", {"凪"}));
  EXPECT_FALSE(KeepsEngineText("凪を使用する", "薙を使用する", {"凪"}));
}

TEST(ZenzClientTest, EscapeJsonString) {
  EXPECT_EQ(EscapeJsonString("a\"b\\c\nd"), "a\\\"b\\\\c\\u000ad");
  EXPECT_EQ(EscapeJsonString("キョウ"), "キョウ");
}

TEST(ZenzClientTest, ExtractJsonStringField) {
  EXPECT_EQ(ExtractJsonStringField(R"({"index":0,"content":"今日は","stop":true})",
                                   "content"),
            "今日は");
  EXPECT_EQ(ExtractJsonStringField(R"({"content": "a\"b\\c\n"})", "content"),
            "a\"b\\c\n");
  EXPECT_EQ(ExtractJsonStringField(R"({"content":"今日"})", "content"),
            "今日");
  // A surrogate pair.
  EXPECT_EQ(ExtractJsonStringField(R"({"content":"😀"})", "content"),
            "\U0001F600");
  EXPECT_EQ(ExtractJsonStringField(R"({"other":"x"})", "content"), std::nullopt);
  EXPECT_EQ(ExtractJsonStringField(R"({"content":"unterminated)", "content"),
            std::nullopt);
  EXPECT_EQ(ExtractJsonStringField(R"({"content":12})", "content"), std::nullopt);
}

}  // namespace
}  // namespace zenz
}  // namespace mozc
