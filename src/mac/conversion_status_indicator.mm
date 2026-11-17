// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#import "mac/conversion_status_indicator.h"

#include <algorithm>
#include <cmath>

using mozc::commands::Output;

namespace {

constexpr CGFloat kFontSize = 11;
constexpr CGFloat kDotSize = 6;
constexpr CGFloat kPaddingX = 7;
constexpr CGFloat kPaddingY = 3;
constexpr CGFloat kDotLabelGap = 5;
constexpr CGFloat kCornerRadius = 6;
// Distance between the composition line and the indicator.
constexpr CGFloat kLineGap = 4;

constexpr Output::ConversionStatus kVisibleStatuses[] = {
    Output::CONVERSION_STATUS_READING,
    Output::CONVERSION_STATUS_ENGINE,
    Output::CONVERSION_STATUS_ZENZ_PENDING,
    Output::CONVERSION_STATUS_ZENZ,
};

NSDictionary<NSAttributedStringKey, id> *LabelAttributes() {
  return @{
    NSFontAttributeName : [NSFont systemFontOfSize:kFontSize weight:NSFontWeightMedium],
    NSForegroundColorAttributeName : NSColor.labelColor,
  };
}

NSColor *DotColor(Output::ConversionStatus status) {
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

+ (NSSize)preferredSize {
  CGFloat labelWidth = 0;
  CGFloat labelHeight = 0;
  for (const Output::ConversionStatus status : kVisibleStatuses) {
    const NSSize size = [[self labelForStatus:status] sizeWithAttributes:LabelAttributes()];
    labelWidth = std::max(labelWidth, size.width);
    labelHeight = std::max(labelHeight, size.height);
  }
  return NSMakeSize(std::ceil(kPaddingX * 2 + kDotSize + kDotLabelGap + labelWidth),
                    std::ceil(kPaddingY * 2 + labelHeight));
}

- (void)setStatus:(Output::ConversionStatus)status {
  if (_status != status) {
    _status = status;
    [self setNeedsDisplay:YES];
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  const NSRect bounds = NSInsetRect(self.bounds, 0.5, 0.5);
  NSBezierPath *background = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                             xRadius:kCornerRadius
                                                             yRadius:kCornerRadius];
  [[NSColor.windowBackgroundColor colorWithAlphaComponent:0.95] setFill];
  [background fill];
  [NSColor.separatorColor setStroke];
  [background stroke];

  const NSRect dot = NSMakeRect(kPaddingX, NSMidY(self.bounds) - kDotSize / 2, kDotSize, kDotSize);
  [DotColor(_status) setFill];
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
  const NSSize size = [ConversionStatusView preferredSize];
  const NSRect frame = NSMakeRect(0, 0, size.width, size.height);
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
  const NSSize size = panel_.frame.size;
  const NSPoint origin = [ConversionStatusIndicator originForLineRect:lineRect
                                                                 size:size
                                                         visibleFrame:visibleFrame];
  [panel_ setFrameOrigin:origin];
  // Above the client window, like the candidate window.
  panel_.level = std::max<NSInteger>(windowLevel + 1, NSPopUpMenuWindowLevel);
  [panel_ orderFront:nil];
}

- (void)hide {
  [panel_ orderOut:nil];
}

@end
