// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#ifndef MOZC_ZENZ_ZENZ_CLIENT_H_
#define MOZC_ZENZ_ZENZ_CLIENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace zenz {

struct ZenzResult {
  enum State {
    PENDING,  // Still working.
    READY,    // |text| holds the conversion.
    FAILED,   // No result will come for this request.
  };
  State state = FAILED;
  std::string text;
};

// Converts a reading with the Zenz neural model, off the caller's thread.
// Only the newest request is kept: a new Request() abandons the older one.
class ZenzClientInterface {
 public:
  virtual ~ZenzClientInterface() = default;

  // Starts converting |reading| (hiragana). |left_context| is the text before
  // the composition and may be empty. Returns immediately.
  virtual void Request(uint32_t id, std::string_view reading,
                       std::string_view left_context) = 0;

  // Returns the state of request |id|. Never blocks.
  virtual ZenzResult Take(uint32_t id) = 0;

  // Abandons the current request, if any.
  virtual void Cancel() = 0;

  // Gets ready for the first request, which otherwise waits for the model to
  // load. Returns immediately.
  virtual void WarmUp() = 0;
};

// Returns the process-wide client. Never null.
ZenzClientInterface* GetZenzClient();

// Replaces the client GetZenzClient() returns. nullptr restores the default.
void SetZenzClientForTesting(ZenzClientInterface* client);

// Creates the default client of this platform. Defined per platform.
ZenzClientInterface* CreateDefaultZenzClient();

// Returns true when |reading| is something Zenz can be asked to convert:
// hiragana, the long vowel mark and Japanese punctuation only.
bool IsZenzReading(std::string_view reading);

// Builds the prompt of the zenz-v3 format:
//   [U+EE02 left_context] U+EE00 reading_in_katakana U+EE01
// Only the tail of |left_context| is used.
std::string BuildZenzPrompt(std::string_view reading,
                            std::string_view left_context);

// Returns true when |output| is safe to show as a conversion of |reading|.
// The kana in |output| must appear in |reading| in the same order, which
// rejects outputs that add or change words.
bool IsAcceptableZenzOutput(std::string_view reading, std::string_view output);

// Returns true when |output| may replace |engine_text|, the engine's conversion
// of the same reading. Checks what the reading alone cannot show:
// - |output| is not just the beginning or the end of |engine_text|, which
//   would drop characters the user typed,
// - it has the same runs of ASCII letters and digits and the same emoji,
//   which the model must not invent or rewrite,
// - it still contains every word of |protected_words|, the user dictionary
//   words the engine chose.
// The same rules as Mozkey's, without its repairs. Which kanji is right for a
// homophone is deliberately not judged here.
bool KeepsEngineText(std::string_view engine_text, std::string_view output,
                     const std::vector<std::string>& protected_words);

// JSON helpers for the llama-server completion API.
std::string EscapeJsonString(std::string_view text);
// Returns the value of the first string field named |field| in |json|.
std::optional<std::string> ExtractJsonStringField(std::string_view json,
                                                  std::string_view field);

}  // namespace zenz
}  // namespace mozc

#endif  // MOZC_ZENZ_ZENZ_CLIENT_H_
