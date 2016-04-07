/*
 *      Copyright (C) 2010-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
 
#import <UIKit/UIKit.h>

#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/ios/XBMCApplication.h"
#import "platform/darwin/ios/XBMCController.h"
#import "platform/darwin/ios/IOSScreenManager.h"

@implementation XBMCApplicationDelegate

XBMCController *m_xbmcController;

// - iOS6 rotation API - will be called on iOS7 runtime!--------
// - on iOS7 first application is asked for supported orientation
// - then the controller of the current view is asked for supported orientation
// - if both say OK - rotation is allowed
- (UIInterfaceOrientationMask)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow *)window
{
  if ([[window rootViewController] respondsToSelector:@selector(supportedInterfaceOrientations)])
    return [[window rootViewController] supportedInterfaceOrientations];
  else
    return (1 << UIInterfaceOrientationLandscapeRight) | (1 << UIInterfaceOrientationLandscapeLeft);
}

- (void)applicationWillResignActive:(UIApplication *)application
{
  [m_xbmcController pauseAnimation];
  [m_xbmcController becomeInactive];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
  [m_xbmcController resumeAnimation];
  [m_xbmcController enterForeground];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
  if (application.applicationState == UIApplicationStateBackground)
  {
    // the app is turn into background, not in by screen lock which has app state inactive.
    [m_xbmcController enterBackground];
  }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
  [m_xbmcController stopAnimation];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)screenDidConnect:(NSNotification *)aNotification
{
  [IOSScreenManager updateResolutions];
}

- (void)screenDidDisconnect:(NSNotification *)aNotification
{
  [IOSScreenManager updateResolutions];
}

- (void)registerScreenNotifications:(BOOL)bRegister
{
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];  
  
  if( bRegister )
  {
    //register to screen notifications
    [nc addObserver:self selector:@selector(screenDidConnect:) name:UIScreenDidConnectNotification object:nil]; 
    [nc addObserver:self selector:@selector(screenDidDisconnect:) name:UIScreenDidDisconnectNotification object:nil]; 
  }
  else
  {
    //deregister from screen notifications
    [nc removeObserver:self name:UIScreenDidConnectNotification object:nil];
    [nc removeObserver:self name:UIScreenDidDisconnectNotification object:nil];
  }
}

- (void)audioRouteChanged:(NSNotification *)notification
{
  // Your tests on the Audio Output changes will go here
  NSInteger routeChangeReason = [notification.userInfo[AVAudioSessionRouteChangeReasonKey] integerValue];
  switch (routeChangeReason)
  {
    case AVAudioSessionRouteChangeReasonUnknown:
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonUnknown");
        break;
    case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
        // an audio device was added
        {
          AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];

          NSArray *currentInputs = myAudioSession.currentRoute.inputs;
          int count_in = [currentInputs count];
          for (int k = 0; k < count_in; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentInputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonNewDeviceAvailable, input count = %d", count_in);

          NSArray *currentOutputs = myAudioSession.currentRoute.outputs;
          int count_out = [currentOutputs count];
          for (int k = 0; k < count_out; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentOutputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonNewDeviceAvailable, output count = %d", count_out);
        }
        [m_xbmcController audioRouteChanged];
        break;
    case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
        // a audio device was removed
        {
          AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];

          NSArray *currentInputs = myAudioSession.currentRoute.inputs;
          int count_in = [currentInputs count];
          for (int k = 0; k < count_in; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentInputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonOldDeviceUnavailable, input count = %d", count_in);

          NSArray *currentOutputs = myAudioSession.currentRoute.outputs;
          int count_out = [currentOutputs count];
          for (int k = 0; k < count_out; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentOutputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonOldDeviceUnavailable, output count = %d", count_out);
        }
        [m_xbmcController audioRouteChanged];
        break;
    case AVAudioSessionRouteChangeReasonCategoryChange:
        // called at start - also when other audio wants to play
        {
          AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];

          NSArray *currentInputs = myAudioSession.currentRoute.inputs;
          int count_in = [currentInputs count];
          for (int k = 0; k < count_in; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentInputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonCategoryChange, input count = %d", count_in);

          NSArray *currentOutputs = myAudioSession.currentRoute.outputs;
          int count_out = [currentOutputs count];
          for (int k = 0; k < count_out; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentOutputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonCategoryChange, output count = %d", count_out);
        }
        break;
    case AVAudioSessionRouteChangeReasonOverride:
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonOverride");
        break;
    case AVAudioSessionRouteChangeReasonWakeFromSleep:
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonWakeFromSleep");
        break;
    case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory");
        break;
    case AVAudioSessionRouteChangeReasonRouteConfigurationChange:
        {
          AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];

          NSArray *currentInputs = myAudioSession.currentRoute.inputs;
          int count_in = [currentInputs count];
          for (int k = 0; k < count_in; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentInputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonRouteConfigurationChange, input count = %d", count_in);

          NSArray *currentOutputs = myAudioSession.currentRoute.outputs;
          int count_out = [currentOutputs count];
          for (int k = 0; k < count_out; k++)
          {
            AVAudioSessionPortDescription *portDesc = [currentOutputs objectAtIndex:k];
            NSLog(@"routeChangeReason : AVAudioSessionPortDescription, %@", portDesc);
          }
          NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonRouteConfigurationChange, output count = %d", count_out);
        }
        break;
    default:
        NSLog(@"routeChangeReason : unknown notification %ld", (long)routeChangeReason);
        break;
  }
}

- (void)registerAudioRouteNotifications:(BOOL)bRegister
{
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  if (bRegister)
  {
    //register to audio route notifications
    [nc addObserver:self selector:@selector(audioRouteChanged:) name:AVAudioSessionRouteChangeNotification object:nil];
  }
  else
  {
    //unregister faudio route notifications
    [nc removeObserver:self name:AVAudioSessionRouteChangeNotification object:nil];
  }
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  NSError *err = nullptr;
  if (![[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback error:&err])
  {
    NSLog(@"AVAudioSession setCategory failed: %ld", (long)err.code);
  }
  err = nil;
  if (![[AVAudioSession sharedInstance] setActive: YES error: &err])
  {
    NSLog(@"AVAudioSession setActive YES failed: %ld", (long)err.code);
  }

  [[UIDevice currentDevice] setBatteryMonitoringEnabled:YES];
  UIScreen *currentScreen = [UIScreen mainScreen];

  m_xbmcController = [[XBMCController alloc] initWithFrame: [currentScreen bounds] withScreen:currentScreen];  
  [m_xbmcController startAnimation];
  [self registerScreenNotifications:YES];
}

- (void)dealloc
{
  [self registerScreenNotifications:NO];
  [m_xbmcController stopAnimation];
  [m_xbmcController release];

  [super dealloc];
}
@end

static void SigPipeHandler(int s)
{
  NSLog(@"We Got a Pipe Single :%d____________", s);
}

int main(int argc, char *argv[]) {
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];	
  int retVal = 0;
  
  signal(SIGPIPE, SigPipeHandler);
  
  @try
  {
    retVal = UIApplicationMain(argc,argv,@"UIApplication",@"XBMCApplicationDelegate");
  }
  @catch (id theException) 
  {
    ELOG(@"%@", theException);
  }
  @finally 
  {
    ILOG(@"This always happens.");
  }
    
  [pool release];
	
  return retVal;

}
