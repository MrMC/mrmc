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

+ (Class) layerClass
{
  return [AVSampleBufferDisplayLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
	self = [super initWithFrame:frame];
	if (self)
	{
    [self setNeedsLayout];
    [self layoutIfNeeded];

    // create a time base
    CMTimebaseRef controlTimebase;
    CMTimebaseCreateWithMasterClock(CFAllocatorGetDefault(), CMClockGetHostTimeClock(), &controlTimebase);

    // setup the time base clock stopped with a zero initial time.
    AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)[self layer];
    videolayer.controlTimebase = controlTimebase;
    CMTimebaseSetTime(videolayer.controlTimebase, kCMTimeZero);
    CMTimebaseSetRate(videolayer.controlTimebase, 0);

#if MCSAMPLEBUFFER_DEBUG_MESSAGES
    [videoLayer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nullptr];
    [videoLayer addObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection" options:NSKeyValueObservingOptionNew context:nullptr];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(layerFailedToDecode:) name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:videoLayer];
#endif
  }

	return self;
}

- (void)dealloc
{
#if MCSAMPLEBUFFER_DEBUG_MESSAGES
  AVSampleBufferDisplayLayer *videolayer = self.videolayer;
  [videoLayer removeObserver:self forKeyPath:@"error"];
  [videoLayer removeObserver:self forKeyPath:@"outputObscuredDueToInsufficientExternalProtection"];

  [[NSNotificationCenter defaultCenter] removeObserver:self name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:videoLayer];
#endif
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

@end
