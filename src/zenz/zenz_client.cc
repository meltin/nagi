// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#include "zenz/zenz_client.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"

namespace mozc {
namespace zenz {
namespace {

constexpr char kReadingBegin[] = "\xEE\xB8\x80";  // U+EE00
constexpr char kOutputBegin[] = "\xEE\xB8\x81";   // U+EE01
constexpr char kContextBegin[] = "\xEE\xB8\x82";  // U+EE02

// How many characters of the left context are given to the model.
constexpr size_t kMaxContextChars = 40;

ZenzClientInterface* g_client_for_testing = nullptr;

bool IsHiragana(char32_t c) { return 0x3041 <= c && c <= 0x3096; }
bool IsKatakana(char32_t c) { return 0x30A1 <= c && c <= 0x30F6; }
// Characters that stay as they are in both the reading and the output.
bool IsReadingMark(char32_t c) {
  return c == 0x30FC ||  // ー
         c == 0x3001 ||  // 、
         c == 0x3002 ||  // 。
         c == 0xFF01 ||  // ！
         c == 0xFF1F;    // ？
}

std::vector<char32_t> ToChars32(std::string_view text) {
  std::vector<char32_t> chars;
  for (const char32_t c : Utf8AsChars32(text)) {
    chars.push_back(c);
  }
  return chars;
}

void AppendUtf8(char32_t c, std::string* out) {
  if (c < 0x80) {
    out->push_back(static_cast<char>(c));
  } else if (c < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (c >> 6)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  } else if (c < 0x10000) {
    out->push_back(static_cast<char>(0xE0 | (c >> 12)));
    out->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (c >> 18)));
    out->push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  }
}

bool ParseHex4(std::string_view text, uint32_t* value) {
  if (text.size() < 4) {
    return false;
  }
  *value = 0;
  for (size_t i = 0; i < 4; ++i) {
    const char c = text[i];
    *value <<= 4;
    if ('0' <= c && c <= '9') {
      *value |= c - '0';
    } else if ('a' <= c && c <= 'f') {
      *value |= c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
      *value |= c - 'A' + 10;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

ZenzClientInterface* GetZenzClient() {
  if (g_client_for_testing != nullptr) {
    return g_client_for_testing;
  }
  static ZenzClientInterface* client = CreateDefaultZenzClient();
  return client;
}

void SetZenzClientForTesting(ZenzClientInterface* client) {
  g_client_for_testing = client;
}

bool IsZenzReading(std::string_view reading) {
  if (reading.empty()) {
    return false;
  }
  for (const char32_t c : Utf8AsChars32(reading)) {
    if (!IsHiragana(c) && !IsReadingMark(c)) {
      return false;
    }
  }
  return true;
}

std::string BuildZenzPrompt(std::string_view reading,
                            std::string_view left_context) {
  std::string prompt;
  const std::vector<char32_t> context = ToChars32(left_context);
  if (!context.empty()) {
    prompt.append(kContextBegin);
    const size_t begin =
        context.size() > kMaxContextChars ? context.size() - kMaxContextChars : 0;
    for (size_t i = begin; i < context.size(); ++i) {
      // The markers of the prompt format must not come from the context.
      if (0xEE00 <= context[i] && context[i] <= 0xEE0F) {
        continue;
      }
      AppendUtf8(context[i], &prompt);
    }
  }
  absl::StrAppend(&prompt, kReadingBegin, japanese::HiraganaToKatakana(reading),
                  kOutputBegin);
  return prompt;
}

bool IsAcceptableZenzOutput(std::string_view reading, std::string_view output) {
  const std::vector<char32_t> reading_chars = ToChars32(reading);
  const std::vector<char32_t> output_chars = ToChars32(output);
  if (output_chars.empty() || output_chars.size() > reading_chars.size() + 2) {
    return false;
  }
  size_t reading_index = 0;
  for (char32_t c : output_chars) {
    if (c < 0x20 || c == 0x7F || c == 0xFFFD || (0xE000 <= c && c <= 0xF8FF)) {
      return false;
    }
    if (IsKatakana(c)) {
      c -= 0x60;  // To hiragana.
    }
    if (!IsHiragana(c) && !IsReadingMark(c)) {
      continue;  // Kanji and so on cannot be checked against the reading.
    }
    while (reading_index < reading_chars.size() &&
           reading_chars[reading_index] != c) {
      ++reading_index;
    }
    if (reading_index == reading_chars.size()) {
      return false;
    }
    ++reading_index;
  }
  return true;
}

std::string EscapeJsonString(std::string_view text) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string escaped;
  escaped.reserve(text.size() + 2);
  for (const char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      escaped.push_back('\\');
      escaped.push_back(c);
    } else if (u < 0x20) {
      escaped.append("\\u00");
      escaped.push_back(kHex[u >> 4]);
      escaped.push_back(kHex[u & 0xF]);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::optional<std::string> ExtractJsonStringField(std::string_view json,
                                                  std::string_view field) {
  const std::string key = absl::StrCat("\"", field, "\"");
  size_t pos = json.find(key);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  pos += key.size();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return std::nullopt;
  }
  ++pos;
  std::string value;
  while (pos < json.size()) {
    const char c = json[pos++];
    if (c == '"') {
      return value;
    }
    if (c != '\\') {
      value.push_back(c);
      continue;
    }
    if (pos >= json.size()) {
      break;
    }
    const char escape = json[pos++];
    switch (escape) {
      case 'n':
        value.push_back('\n');
        break;
      case 't':
        value.push_back('\t');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 'b':
        value.push_back('\b');
        break;
      case 'f':
        value.push_back('\f');
        break;
      case 'u': {
        uint32_t code = 0;
        if (!ParseHex4(json.substr(pos), &code)) {
          return std::nullopt;
        }
        pos += 4;
        // A surrogate pair is two \u escapes.
        if (0xD800 <= code && code <= 0xDBFF && json.substr(pos, 2) == "\\u") {
          uint32_t low = 0;
          if (ParseHex4(json.substr(pos + 2), &low) && 0xDC00 <= low &&
              low <= 0xDFFF) {
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
            pos += 6;
          }
        }
        AppendUtf8(code, &value);
        break;
      }
      default:  // \" \\ \/
        value.push_back(escape);
        break;
    }
  }
  return std::nullopt;  // Unterminated string.
}

}  // namespace zenz
}  // namespace mozc
