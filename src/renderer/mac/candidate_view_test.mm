// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#import "renderer/mac/CandidateView.h"
#import "renderer/mac/InfolistView.h"

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

// Draws |view| after its layout. Saves a PNG when NAGI_CANDIDATE_SAMPLE_DIR is set, so the
// look can be checked without installing the IME. Returns the drawn bitmap.
NSBitmapImageRep *Draw(NSView *view, NSSize size, const char *name) {
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
  return bitmap;
}

NSSize Render(const commands::CandidateWindow &window, const char *name) {
  CandidateView *view = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
  [view setCandidateWindow:&window];
  const NSSize size = [view updateLayout];
  Draw(view, size, name);
  return size;
}

// Brightness of the background, sampled in the last row where no text is drawn.
CGFloat BackgroundBrightness(NSView *view, NSSize size, const char *name) {
  NSBitmapImageRep *bitmap = Draw(view, size, name);
  NSColor *color = [[bitmap colorAtX:bitmap.pixelsWide - 8 y:bitmap.pixelsHigh * 4 / 5]
      colorUsingColorSpace:NSColorSpace.genericGrayColorSpace];
  return color.whiteComponent;
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

TEST(CandidateViewTest, FollowsTheDarkAppearance) {
  const commands::CandidateWindow window =
      MakeWindow(commands::SUGGESTION, {"聞いていないのか", "効いていないのか"});
  CGFloat brightness[2];
  int i = 0;
  for (NSAppearanceName name in @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]) {
    CandidateView *view = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
    view.appearance = [NSAppearance appearanceNamed:name];
    [view setCandidateWindow:&window];
    const NSSize size = [view updateLayout];
    brightness[i] = BackgroundBrightness(view, size, i == 0 ? "light" : "dark");
    ++i;
  }
  EXPECT_GT(brightness[0], 0.8);
  EXPECT_LT(brightness[1], 0.3);
}

TEST(InfolistViewTest, RoundedAndFollowsTheDarkAppearance) {
  commands::CandidateWindow window = MakeWindow(commands::CONVERSION, {"聞く", "聴く"});
  commands::InformationList *usages = window.mutable_usages();
  usages->set_focused_index(0);
  for (const char *title : {"聞く", "聴く"}) {
    commands::Information *information = usages->add_information();
    information->set_title(title);
    information->set_description("音を耳で感じる。「話を聞く」");
  }
  CGFloat brightness[2];
  int i = 0;
  for (NSAppearanceName name in @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]) {
    InfolistView *view = [[InfolistView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
    view.appearance = [NSAppearance appearanceNamed:name];
    [view setCandidateWindow:&window];
    const NSSize size = [view updateLayout];
    // Draw() also checks that the corner is outside the shape.
    brightness[i] = BackgroundBrightness(view, size, i == 0 ? "usage_light" : "usage_dark");
    ++i;
  }
  EXPECT_GT(brightness[0], 0.8);
  EXPECT_LT(brightness[1], 0.3);
}

}  // namespace
}  // namespace mozc
