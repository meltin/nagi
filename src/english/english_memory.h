// Nagi: the words the user taught the automatic input mode by correcting it,
// kept in english_words.tsv in the profile directory. Nothing leaves the Mac.

#ifndef MOZC_ENGLISH_ENGLISH_MEMORY_H_
#define MOZC_ENGLISH_ENGLISH_MEMORY_H_

#include <string_view>

#include "english/english_detector.h"

namespace mozc {
namespace english {

// Everything learned so far.
LearnedWords GetLearnedEnglishWords();

// Remembers that |word| is English, or is not. The newest verdict wins. Only
// words of 3 to 24 lower-case letters are kept.
void LearnEnglishWord(std::string_view word, bool english);

// Forgets everything, with the rest of the user's history.
void ClearLearnedEnglishWords();

}  // namespace english
}  // namespace mozc

#endif  // MOZC_ENGLISH_ENGLISH_MEMORY_H_
