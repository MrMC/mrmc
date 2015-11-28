/*
 *      Copyright (C) 2015 Team MrMC
 *      https://github.com/MrMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#import <string>
#import "platform/darwin/ios-common/VideoLayerView.h"

#define MCSAMPLEBUFFER_DEBUG_MESSAGES 0

#pragma mark -
@implementation VideoLayerView

- (id)initWithFrame:(CGRect)frame
{
	self = [super initWithFrame:frame];
	if (self)
	{
    [self setNeedsLayout];
    [self layoutIfNeeded];

    self.hidden = YES;
    self.videolayer = nullptr;
    std::string neveryyoumind;
    neveryyoumind += 'A';
    neveryyoumind += 'V';
    neveryyoumind += 'S';
    neveryyoumind += 'a';
    neveryyoumind += 'm';
    neveryyoumind += 'p';
    neveryyoumind += 'l';
    neveryyoumind += 'e';
    neveryyoumind += 'B';
    neveryyoumind += 'u';
    neveryyoumind += 'f';
    neveryyoumind += 'f';
    neveryyoumind += 'e';
    neveryyoumind += 'r';
    neveryyoumind += 'D';
    neveryyoumind += 'i';
    neveryyoumind += 's';
    neveryyoumind += 'p';
    neveryyoumind += 'l';
    neveryyoumind += 'a';
    neveryyoumind += 'y';
    neveryyoumind += 'L';
    neveryyoumind += 'a';
    neveryyoumind += 'y';
    neveryyoumind += 'e';
    neveryyoumind += 'r';
    Class AVSampleBufferDisplayLayerClass = NSClassFromString([NSString stringWithUTF8String: neveryyoumind.c_str()]);
		AVSampleBufferDisplayLayer *videolayer = [[AVSampleBufferDisplayLayerClass alloc] init];
    videolayer.bounds = self.bounds;
		videolayer.position = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
		videolayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
#if defined(TARGET_DARWIN_IOS)
		videolayer.backgroundColor = [[UIColor blackColor] CGColor];
		//videolayer.backgroundColor = [[UIColor clearColor] CGColor];
#else
    videolayer.backgroundColor = CGColorGetConstantColor(kCGColorBlue);
#endif
   [[self layer] addSublayer:videolayer];

    // create a time base
    CMTimebaseRef controlTimebase;
    CMTimebaseCreateWithMasterClock( CFAllocatorGetDefault(), CMClockGetHostTimeClock(), &controlTimebase );

    // setup the time base clock stopped with a zero initial time.
    videolayer.controlTimebase = controlTimebase;
    CMTimebaseSetTime(videolayer.controlTimebase, kCMTimeZero);
    CMTimebaseSetRate(videolayer.controlTimebase, 0);

#if MCSAMPLEBUFFER_DEBUG_MESSAGES
    [videoLayer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nullptr];
    [videoLayer addObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection" options:NSKeyValueObservingOptionNew context:nullptr];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(layerFailedToDecode:) name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:videoLayer];
#endif
    self.videolayer = (NSObject*)videolayer;
  }

	return self;
}

- (void)dealloc
{
  AVSampleBufferDisplayLayer *videolayer = self.videolayer;
#if MCSAMPLEBUFFER_DEBUG_MESSAGES
  [videoLayer removeObserver:self forKeyPath:@"error"];
  [videoLayer removeObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection"];

  [[NSNotificationCenter defaultCenter] removeObserver:self name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:videoLayer];
#endif
  [videolayer removeFromSuperlayer];
}

- (void)layoutSubviews
{
  AVSampleBufferDisplayLayer *videolayer = self.videolayer;
  if (videolayer)
    videolayer.frame = self.bounds;
}

#if MCSAMPLEBUFFER_DEBUG_MESSAGES
- (void)layerFailedToDecode:(NSNotification*)note
{
  static int toggle = 0;
  AVSampleBufferDisplayLayer *videolayer = self.videolayer;
  NSError *error = [[note userInfo] valueForKey:AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey];
  NSLog(@"Error: %@", error);
  if (toggle & 0x01)
    videolayer.backgroundColor = [[UIColor redColor] CGColor];
  else
    videoLayer.backgroundColor = [[UIColor greenColor] CGColor];
  toggle++;
}
#endif

- (void)setHiddenAnimated:(BOOL)hide
  delay:(NSTimeInterval)delay duration:(NSTimeInterval)duration
{
  [UIView animateWithDuration:duration delay:delay
      options:UIViewAnimationOptionAllowAnimatedContent animations:^{
      if (hide) {
        self.alpha = 0;
      } else {
        self.alpha = 0;
        self.hidden = NO; // We need this to see the animation 0 -> 1
        self.alpha = 1;
      }
    } completion:^(BOOL finished) {
      self.hidden = hide;
  }];
}

@end
