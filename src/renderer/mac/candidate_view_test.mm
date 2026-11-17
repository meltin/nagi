// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#import "renderer/mac/CandidateView.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "protocol/candidate_window.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

commands::CandidateWindow MakeWindow(commands::Category category,
                                     const std::vector<std::string> &values) {
  commands::CandidateWindow window;
  window.set_category(category);
  window.set_size(values.size());
  window.set_position(0);
  for (size_t i = 0; i < values.size(); ++i) {
    commands::CandidateWindow::Candidate *candidate = window.add_candidate();
    candidate->set_index(i);
    candidate->set_id(i);
    candidate->set_value(values[i]);
    if (category == commands::CONVERSION) {
      candidate->mutable_annotation()->set_shortcut(std::to_string(i + 1));
    }
  }
  if (category == commands::CONVERSION) {
    window.set_focused_index(0);
    window.mutable_footer()->set_index_visible(true);
  } else {
    window.mutable_footer()->set_label("Tabキーで選択");
  }
  return window;
}

// Lays out |window| and draws it. Saves a PNG when NAGI_CANDIDATE_SAMPLE_DIR is set, so the
// look can be checked without installing the IME.
NSSize Render(const commands::CandidateWindow &window, const char *name) {
  CandidateView *view = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
  [view setCandidateWindow:&window];
  const NSSize size = [view updateLayout];
  [view setFrame:NSMakeRect(0, 0, size.width, size.height)];
  NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
  [view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];
  if (const char *dir = std::getenv("NAGI_CANDIDATE_SAMPLE_DIR")) {
    NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    NSString *path = [NSString stringWithFormat:@"%s/%s.png", dir, name];
    EXPECT_TRUE([png writeToFile:path atomically:YES]);
  }
  // The corner pixel is outside the rounded shape.
  EXPECT_EQ([bitmap colorAtX:0 y:0].alphaComponent, 0);
  return size;
}

TEST(CandidateViewTest, SuggestionHasNoTabHintFooter) {
  const NSSize one = Render(MakeWindow(commands::SUGGESTION, {"聞いていないのか"}), "suggestion");
  const NSSize two =
      Render(MakeWindow(commands::SUGGESTION, {"聞いていないのか", "効いていないのか"}),
             "suggestions");
  // One row is half of two rows only when no footer is added below them.
  EXPECT_LT(one.height * 2, two.height + 4);
}

TEST(CandidateViewTest, ConversionKeepsTheIndexFooter) {
  const NSSize suggestion =
      Render(MakeWindow(commands::SUGGESTION, {"聞いてる", "聴いてる"}), "suggestion_pair");
  const NSSize conversion =
      Render(MakeWindow(commands::CONVERSION, {"聞いてる", "聴いてる"}), "conversion");
  EXPECT_GT(conversion.height, suggestion.height);
}

}  // namespace
}  // namespace mozc
