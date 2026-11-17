// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

#ifndef MOZC_MAC_CONVERSION_STATUS_INDICATOR_H_
#define MOZC_MAC_CONVERSION_STATUS_INDICATOR_H_

#import <Cocoa/Cocoa.h>

#include "protocol/commands.pb.h"

/** Draws one status as a colored dot and a short label. */
@interface ConversionStatusView : NSView
@property(nonatomic) mozc::commands::Output::ConversionStatus status;

/** Label shown for |status|. Empty for CONVERSION_STATUS_NONE. */
+ (NSString *)labelForStatus:(mozc::commands::Output::ConversionStatus)status;

/** Size that fits the label of |status|. */
+ (NSSize)sizeForStatus:(mozc::commands::Output::ConversionStatus)status;
@end

/** Small floating panel that tells what produced the text in the composition. */
@interface ConversionStatusIndicator : NSObject

+ (instancetype)sharedIndicator;

/** Shows |status| next to |lineRect|, the screen rectangle of the first line of the
 * composition. |windowLevel| is the level of the client window. */
- (void)showStatus:(mozc::commands::Output::ConversionStatus)status
      nearLineRect:(NSRect)lineRect
       windowLevel:(NSInteger)windowLevel;

- (void)hide;

/** Where to put an indicator of |size| for |lineRect|: above the line, or below it when the
 * screen has no room above. Kept inside |visibleFrame|. */
+ (NSPoint)originForLineRect:(NSRect)lineRect size:(NSSize)size visibleFrame:(NSRect)visibleFrame;

@end

#endif  // MOZC_MAC_CONVERSION_STATUS_INDICATOR_H_
