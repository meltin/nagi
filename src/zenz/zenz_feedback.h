// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#ifndef MOZC_ZENZ_ZENZ_FEEDBACK_H_
#define MOZC_ZENZ_ZENZ_FEEDBACK_H_

#include <optional>
#include <string>
#include <string_view>

namespace mozc {
namespace zenz {

// What the user did with Zenz corrections, remembered across restarts.
//
// A correction committed as shown is "accepted": the next time the same
// reading is converted, it is shown at once, without asking Zenz.
// A correction the user turned back to the engine's conversion is "rejected":
// it is not shown for that reading again.
//
// Both are matched by the whole reading, so what is remembered for one
// sentence does not leak into others.
//
// Stored as plain text in the user profile directory (zenz_feedback.tsv),
// next to the engine's own history. Nothing leaves the machine.

// The correction to show for |reading| right away, if one was accepted.
std::optional<std::string> AcceptedZenzCorrection(std::string_view reading);

// True when the user turned |text| back for |reading|.
bool IsRejectedZenzCorrection(std::string_view reading, std::string_view text);

void AcceptZenzCorrection(std::string_view reading, std::string_view text);
void RejectZenzCorrection(std::string_view reading, std::string_view text);

}  // namespace zenz
}  // namespace mozc

#endif  // MOZC_ZENZ_ZENZ_FEEDBACK_H_
