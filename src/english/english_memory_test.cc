#include "english/english_memory.h"

#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace english {
namespace {

class EnglishMemoryTest : public testing::TestWithTempUserProfile {};

TEST_F(EnglishMemoryTest, LearnsAndForgets) {
  EXPECT_TRUE(GetLearnedEnglishWords().empty());
  LearnEnglishWord("made", true);
  LearnEnglishWord("git", false);
  LearnEnglishWord("Made", true);  // Not lower case.
  LearnEnglishWord("ab", true);    // Too short.
  EXPECT_EQ(GetLearnedEnglishWords(),
            (LearnedWords{{"made", true}, {"git", false}}));
  // The newest verdict wins.
  LearnEnglishWord("made", false);
  EXPECT_EQ(GetLearnedEnglishWords(),
            (LearnedWords{{"made", false}, {"git", false}}));
  ClearLearnedEnglishWords();
  EXPECT_TRUE(GetLearnedEnglishWords().empty());
}

}  // namespace
}  // namespace english
}  // namespace mozc
