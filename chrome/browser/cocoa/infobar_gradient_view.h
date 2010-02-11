// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_INFOBAR_GRADIENT_VIEW_H_
#define CHROME_BROWSER_COCOA_INFOBAR_GRADIENT_VIEW_H_

#import <Cocoa/Cocoa.h>

// A custom view that draws the background gradient for an infobar.
// The default is a yellow gradient, but a custom gradient can also be set.
@interface InfoBarGradientView : NSView {
 @private
  // The gradient to draw.
  NSGradient* gradient_;
}

// Set a custom gradient for the view.
- (void)setGradient:(NSGradient*)gradient;

@end

#endif  // CHROME_BROWSER_COCOA_INFOBAR_GRADIENT_VIEW_H_
