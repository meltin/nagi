// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "zenz/zenz_feedback.h"

#include <fstream>
#include <string>

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace zenz {
namespace {

class ZenzFeedbackTest : public testing::TestWithTempUserProfile {};

TEST_F(ZenzFeedbackTest, NothingIsKnownAtFirst) {
  EXPECT_FALSE(AcceptedZenzCorrection("きしゃした").has_value());
  EXPECT_FALSE(IsRejectedZenzCorrection("きしゃした", "記者した"));
}

TEST_F(ZenzFeedbackTest, RemembersByTheWholeReading) {
  AcceptZenzCorrection("きかいをつくる", "機械を作る");
  RejectZenzCorrection("きしゃした", "記者した");

  EXPECT_EQ(AcceptedZenzCorrection("きかいをつくる"), "機械を作る");
  EXPECT_TRUE(IsRejectedZenzCorrection("きしゃした", "記者した"));
  // Other readings and other texts are not affected.
  EXPECT_FALSE(AcceptedZenzCorrection("きかい").has_value());
  EXPECT_FALSE(IsRejectedZenzCorrection("きしゃした", "帰社した"));
  EXPECT_FALSE(IsRejectedZenzCorrection("きしゃ", "記者"));
}

TEST_F(ZenzFeedbackTest, TheLatestVerdictWins) {
  AcceptZenzCorrection("きしゃした", "記者した");
  RejectZenzCorrection("きしゃした", "記者した");
  EXPECT_FALSE(AcceptedZenzCorrection("きしゃした").has_value());
  EXPECT_TRUE(IsRejectedZenzCorrection("きしゃした", "記者した"));

  AcceptZenzCorrection("きしゃした", "記者した");
  EXPECT_EQ(AcceptedZenzCorrection("きしゃした"), "記者した");
  EXPECT_FALSE(IsRejectedZenzCorrection("きしゃした", "記者した"));

  // A reading has one accepted text.
  AcceptZenzCorrection("きしゃした", "帰社した");
  EXPECT_EQ(AcceptedZenzCorrection("きしゃした"), "帰社した");
}

TEST_F(ZenzFeedbackTest, IsStoredInTheProfileAndReadBack) {
  AcceptZenzCorrection("きかいをつくる", "機械を作る");
  const std::string path = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "zenz_feedback.tsv");
  std::ifstream file(path);
  std::string line;
  ASSERT_TRUE(std::getline(file, line));
  EXPECT_EQ(line, "A\tきかいをつくる\t機械を作る");

  // Another profile knows nothing, and coming back reads the file again.
  const std::string profile = SystemUtil::GetUserProfileDirectory();
  const TempDirectory other = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(other.path());
  EXPECT_FALSE(AcceptedZenzCorrection("きかいをつくる").has_value());
  SystemUtil::SetUserProfileDirectory(profile);
  EXPECT_EQ(AcceptedZenzCorrection("きかいをつくる"), "機械を作る");
}

TEST_F(ZenzFeedbackTest, DoesNotStoreWhatWouldBreakTheFile) {
  AcceptZenzCorrection("きかい", "機\t械");
  AcceptZenzCorrection("", "機械");
  EXPECT_FALSE(AcceptedZenzCorrection("きかい").has_value());
}

}  // namespace
}  // namespace zenz
}  // namespace mozc
