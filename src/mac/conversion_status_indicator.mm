// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#import "mac/conversion_status_indicator.h"

#include <algorithm>
#include <cmath>

using mozc::commands::Output;

namespace {

constexpr CGFloat kFontSize = 11;
constexpr CGFloat kDotSize = 6;
constexpr CGFloat kPaddingX = 8;
constexpr CGFloat kPaddingY = 3;
constexpr CGFloat kDotLabelGap = 5;
// Distance between the composition line and the indicator.
constexpr CGFloat kLineGap = 4;

NSDictionary<NSAttributedStringKey, id> *LabelAttributes() {
  return @{
    NSFontAttributeName : [NSFont systemFontOfSize:kFontSize weight:NSFontWeightMedium],
    NSForegroundColorAttributeName : NSColor.whiteColor,
  };
}

NSColor *ChipColor(Output::ConversionStatus status) {
  switch (status) {
    case Output::CONVERSION_STATUS_ENGINE:
      return NSColor.systemBlueColor;
    case Output::CONVERSION_STATUS_ZENZ_PENDING:
      return NSColor.systemOrangeColor;
    case Output::CONVERSION_STATUS_ZENZ:
      return NSColor.systemGreenColor;
    default:
      return NSColor.systemGrayColor;
  }
}

}  // namespace

@implementation ConversionStatusView

+ (NSString *)labelForStatus:(Output::ConversionStatus)status {
  switch (status) {
    case Output::CONVERSION_STATUS_READING:
      return @"入力中";
    case Output::CONVERSION_STATUS_ENGINE:
      return @"Mozc";
    case Output::CONVERSION_STATUS_ZENZ_PENDING:
      return @"Zenz…";
    case Output::CONVERSION_STATUS_ZENZ:
      return @"Zenz";
    default:
      return @"";
  }
}

+ (NSSize)sizeForStatus:(Output::ConversionStatus)status {
  const NSSize label = [[self labelForStatus:status] sizeWithAttributes:LabelAttributes()];
  return NSMakeSize(std::ceil(kPaddingX * 2 + kDotSize + kDotLabelGap + label.width),
                    std::ceil(kPaddingY * 2 + label.height));
}

- (void)setStatus:(Output::ConversionStatus)status {
  if (_status != status) {
    _status = status;
    [self setNeedsDisplay:YES];
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  // A pill: the corners are half the height.
  const CGFloat radius = NSHeight(self.bounds) / 2;
  [ChipColor(_status) setFill];
  [[NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:radius yRadius:radius] fill];

  const NSRect dot = NSMakeRect(kPaddingX, NSMidY(self.bounds) - kDotSize / 2, kDotSize, kDotSize);
  [NSColor.whiteColor setFill];
  [[NSBezierPath bezierPathWithOvalInRect:dot] fill];

  NSString *label = [ConversionStatusView labelForStatus:_status];
  const NSSize labelSize = [label sizeWithAttributes:LabelAttributes()];
  [label drawAtPoint:NSMakePoint(NSMaxX(dot) + kDotLabelGap,
                                 NSMidY(self.bounds) - labelSize.height / 2)
      withAttributes:LabelAttributes()];
}

@end

@implementation ConversionStatusIndicator {
  NSPanel *panel_;
  ConversionStatusView *view_;
}

+ (instancetype)sharedIndicator {
  static ConversionStatusIndicator *indicator = nil;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    indicator = [[ConversionStatusIndicator alloc] init];
  });
  return indicator;
}

+ (NSPoint)originForLineRect:(NSRect)lineRect size:(NSSize)size visibleFrame:(NSRect)visibleFrame {
  CGFloat y = NSMaxY(lineRect) + kLineGap;
  if (y + size.height > NSMaxY(visibleFrame)) {
    y = NSMinY(lineRect) - kLineGap - size.height;
  }
  const CGFloat maxX = std::max(NSMinX(visibleFrame), NSMaxX(visibleFrame) - size.width);
  const CGFloat x = std::clamp(NSMinX(lineRect), NSMinX(visibleFrame), maxX);
  return NSMakePoint(x, y);
}

- (void)createPanelIfNeeded {
  if (panel_) {
    return;
  }
  // Resized for each status in showStatus:.
  const NSRect frame = NSMakeRect(0, 0, 1, 1);
  panel_ = [[NSPanel alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                  backing:NSBackingStoreBuffered
                    defer:YES];
  view_ = [[ConversionStatusView alloc] initWithFrame:frame];
  panel_.contentView = view_;
  panel_.opaque = NO;
  panel_.backgroundColor = NSColor.clearColor;
  panel_.hasShadow = YES;
  panel_.floatingPanel = YES;
  panel_.worksWhenModal = YES;
  panel_.ignoresMouseEvents = YES;
  panel_.releasedWhenClosed = NO;
  panel_.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                              NSWindowCollectionBehaviorFullScreenAuxiliary |
                              NSWindowCollectionBehaviorTransient;
}

- (void)showStatus:(Output::ConversionStatus)status
      nearLineRect:(NSRect)lineRect
       windowLevel:(NSInteger)windowLevel {
  if (status == Output::CONVERSION_STATUS_NONE) {
    [self hide];
    return;
  }
  [self createPanelIfNeeded];
  view_.status = status;

  NSRect visibleFrame = NSScreen.mainScreen.visibleFrame;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(lineRect.origin, screen.frame)) {
      visibleFrame = screen.visibleFrame;
      break;
    }
  }
  const NSSize size = [ConversionStatusView sizeForStatus:status];
  const NSPoint origin = [ConversionStatusIndicator originForLineRect:lineRect
                                                                 size:size
                                                         visibleFrame:visibleFrame];
  [panel_ setFrame:NSMakeRect(origin.x, origin.y, size.width, size.height) display:YES];
  // Above the client window, like the candidate window.
  panel_.level = std::max<NSInteger>(windowLevel + 1, NSPopUpMenuWindowLevel);
  [panel_ orderFront:nil];
}

- (void)hide {
  [panel_ orderOut:nil];
}

@end
