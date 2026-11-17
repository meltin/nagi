// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#import "mac/conversion_status_indicator.h"

#include <cstdlib>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace {

using commands::Output;

TEST(ConversionStatusIndicatorTest, Labels) {
  EXPECT_TRUE([[ConversionStatusView labelForStatus:Output::CONVERSION_STATUS_READING]
      isEqualToString:@"入力中"]);
  EXPECT_TRUE([[ConversionStatusView labelForStatus:Output::CONVERSION_STATUS_ENGINE]
      isEqualToString:@"Mozc"]);
  EXPECT_TRUE([[ConversionStatusView labelForStatus:Output::CONVERSION_STATUS_ZENZ_PENDING]
      isEqualToString:@"Zenz…"]);
  EXPECT_TRUE([[ConversionStatusView labelForStatus:Output::CONVERSION_STATUS_ZENZ]
      isEqualToString:@"Zenz"]);
  EXPECT_EQ([[ConversionStatusView labelForStatus:Output::CONVERSION_STATUS_NONE] length], 0);
}

TEST(ConversionStatusIndicatorTest, PlacedAboveTheLine) {
  const NSRect screen = NSMakeRect(0, 0, 1000, 800);
  const NSRect line = NSMakeRect(100, 400, 10, 20);
  const NSPoint origin = [ConversionStatusIndicator originForLineRect:line
                                                                 size:NSMakeSize(60, 20)
                                                         visibleFrame:screen];
  EXPECT_EQ(origin.x, 100);
  EXPECT_GT(origin.y, NSMaxY(line));
}

TEST(ConversionStatusIndicatorTest, PlacedBelowWhenNoRoomAbove) {
  const NSRect screen = NSMakeRect(0, 0, 1000, 800);
  const NSRect line = NSMakeRect(100, 775, 10, 20);
  const NSSize size = NSMakeSize(60, 20);
  const NSPoint origin = [ConversionStatusIndicator originForLineRect:line
                                                                 size:size
                                                         visibleFrame:screen];
  EXPECT_LT(origin.y + size.height, NSMinY(line));
}

TEST(ConversionStatusIndicatorTest, KeptInsideTheScreen) {
  const NSRect screen = NSMakeRect(0, 0, 1000, 800);
  const NSSize size = NSMakeSize(60, 20);
  const NSPoint origin = [ConversionStatusIndicator originForLineRect:NSMakeRect(990, 400, 10, 20)
                                                                 size:size
                                                         visibleFrame:screen];
  EXPECT_LE(origin.x + size.width, NSMaxX(screen));
}

// Draws each status in both appearances. When NAGI_INDICATOR_SAMPLE_DIR is
// set, the result is also saved there as PNG files, so the look can be checked
// without installing the IME.
TEST(ConversionStatusIndicatorTest, RenderSamples) {
  const char *output_dir = std::getenv("NAGI_INDICATOR_SAMPLE_DIR");
  const NSSize size = [ConversionStatusView preferredSize];
  EXPECT_GT(size.width, 0);
  EXPECT_GT(size.height, 0);

  // std::vector, because a block cannot capture a C array.
  const std::vector<Output::ConversionStatus> statuses = {
      Output::CONVERSION_STATUS_READING,
      Output::CONVERSION_STATUS_ENGINE,
      Output::CONVERSION_STATUS_ZENZ_PENDING,
      Output::CONVERSION_STATUS_ZENZ,
  };
  NSArray<NSAppearanceName> *appearances = @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ];
  const CGFloat margin = 12;
  const CGFloat scale = 3;
  const NSSize canvas = NSMakeSize(size.width + margin * 2,
                                   (size.height + margin) * statuses.size() + margin);

  for (NSAppearanceName name in appearances) {
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr
                      pixelsWide:canvas.width * scale
                      pixelsHigh:canvas.height * scale
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                     bytesPerRow:0
                    bitsPerPixel:0];
    bitmap.size = canvas;
    NSAppearance *appearance = [NSAppearance appearanceNamed:name];
    [appearance performAsCurrentDrawingAppearance:^{
      [NSGraphicsContext saveGraphicsState];
      NSGraphicsContext.currentContext =
          [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
      [NSColor.textBackgroundColor setFill];
      NSRectFill(NSMakeRect(0, 0, canvas.width, canvas.height));
      CGFloat y = canvas.height - margin - size.height;
      for (const Output::ConversionStatus status : statuses) {
        ConversionStatusView *view = [[ConversionStatusView alloc]
            initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
        view.status = status;
        NSAffineTransform *transform = [NSAffineTransform transform];
        [transform translateXBy:margin yBy:y];
        [transform concat];
        [view drawRect:view.bounds];
        [transform invert];
        [transform concat];
        y -= size.height + margin;
      }
      [NSGraphicsContext restoreGraphicsState];
    }];
    NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    EXPECT_GT(png.length, 0);
    if (output_dir != nullptr) {
      NSString *path = [NSString stringWithFormat:@"%s/indicator_%@.png", output_dir,
                                                  [name isEqualToString:NSAppearanceNameAqua]
                                                      ? @"light"
                                                      : @"dark"];
      EXPECT_TRUE([png writeToFile:path atomically:YES]);
    }
  }
}

}  // namespace
}  // namespace mozc
