// Nagi: what the session needs to show English words as typed in a romaji
// composition (the automatic input mode).

#ifndef MOZC_ENGLISH_AUTOMATIC_ENGLISH_H_
#define MOZC_ENGLISH_AUTOMATIC_ENGLISH_H_

#include <string_view>
#include <vector>

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "english/english_detector.h"

namespace mozc {
namespace english {

// Judges readings with the converter.
class ConverterJudge : public ReadingJudge {
 public:
  explicit ConverterJudge(const ConverterInterface* converter)
      : converter_(converter) {}
  int Cost(std::string_view reading) const override;
  bool IsPrefixOfWord(std::string_view reading) const override;
  std::vector<Segment> Segments(std::string_view reading) const override;

 private:
  const ConverterInterface* converter_;
};

// The word list built into the binary, read on the first call. Romanized
// names are removed with |reader|. Empty when Nagi was built without the list.
const EnglishWords& GetEnglishWords(const RomajiReader& reader);

// Types |keys| into the empty |composer| again: the |runs| as typed in half
// width, the rest as romaji.
void TypeWithRuns(std::string_view keys,
                  const std::vector<EnglishDetector::Run>& runs,
                  composer::Composer* composer);

}  // namespace english
}  // namespace mozc

#endif  // MOZC_ENGLISH_AUTOMATIC_ENGLISH_H_
