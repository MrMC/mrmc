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
#import <objc/runtime.h>

#import "platform/darwin/tvos/MainApplication.h"

#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/MainController.h"
#import "platform/darwin/tvos/TVOSTopShelf.h"
#import "platform/darwin/tvos/PreflightHandler.h"

@implementation MainApplicationDelegate
MainController* m_xbmcController;

- (void)applicationWillResignActive:(UIApplication *)application
{
  PRINT_SIGNATURE();

  [[UIApplication sharedApplication] ignoreSnapshotOnNextApplicationLaunch];
  [m_xbmcController pauseAnimation];
  [m_xbmcController becomeInactive];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
  PRINT_SIGNATURE();

  [m_xbmcController resumeAnimation];
  [m_xbmcController becomeActive];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
  PRINT_SIGNATURE();

  if (application.applicationState == UIApplicationStateBackground)
  {
    // the app is turn into background, not in by screen lock which has app state inactive.
    [m_xbmcController enterBackground];
  }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
  PRINT_SIGNATURE();
  [m_xbmcController stopAnimation];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
  PRINT_SIGNATURE();
  [m_xbmcController enterForeground];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application 
{
  PRINT_SIGNATURE();
  // applicationDidFinishLaunching is the very first callback that we get

  // This needs to run before anything does any CLog::Log calls
  // as they will directly cause guitsetting to get accessed/created
  // via debug log settings.
  CPreflightHandler::MigrateUserdataXMLToNSUserDefaults();

  NSError *err = nullptr;
  if (![[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback error:&err])
  {
    NSLog(@"AVAudioSession setCategory failed: %ld", (long)err.code);
  }
  err = nil;
  if (![[AVAudioSession sharedInstance] setMode:AVAudioSessionModeMoviePlayback error:&err])
  {
    NSLog(@"AVAudioSession setMode failed: %ld", (long)err.code);
  }
  err = nil;
  if (![[AVAudioSession sharedInstance] setActive: YES error: &err])
  {
    NSLog(@"AVAudioSession setActive YES failed: %ld", (long)err.code);
  }

  UIScreen *currentScreen = [UIScreen mainScreen];
  m_xbmcController = [[MainController alloc] initWithFrame: [currentScreen bounds] withScreen:currentScreen];
  [m_xbmcController startAnimation];

  [self registerScreenNotifications];
  [self registerAudioRouteNotifications];
}

- (BOOL)application:(UIApplication *)app
  openURL:(NSURL *)url options:(NSDictionary<NSString *, id> *)options
{
  PRINT_SIGNATURE();
  NSArray *urlComponents = [[url absoluteString] componentsSeparatedByString:@"/"];
  NSString *action = urlComponents[2];
  if ([action isEqualToString:@"display"] || [action isEqualToString:@"play"])
  {
    std::string cleanURL = *new std::string([[url absoluteString] UTF8String]);
    CTVOSTopShelf::GetInstance().HandleTopShelfUrl(cleanURL, true);
  }
  return YES;
}

-(BOOL)application:(UIApplication *)application shouldRestoreApplicationState:(NSCoder *)coder
{
  PRINT_SIGNATURE();
  return YES;
}

-(BOOL)application:(UIApplication *)application shouldSaveApplicationState:(NSCoder *)coder
{
  PRINT_SIGNATURE();
  return YES;
}

- (void)dealloc
{
  [self unregisterScreenNotifications];
  [self unregisterAudioRouteNotifications];
  [m_xbmcController stopAnimation];
  m_xbmcController = nil;
}

#pragma mark private methods
- (void)handleAudioRouteChange:(NSNotification *)notification
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
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonNewDeviceAvailable");
        [m_xbmcController audioRouteChanged];
        CDarwinUtils::DumpAudioDescriptions("AVAudioSessionRouteChangeReasonNewDeviceAvailable");
        break;
    case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
        // a audio device was removed
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonOldDeviceUnavailable");
        [m_xbmcController audioRouteChanged];
        CDarwinUtils::DumpAudioDescriptions("AVAudioSessionRouteChangeReasonOldDeviceUnavailable");
        break;
    case AVAudioSessionRouteChangeReasonCategoryChange:
        // called at start - also when other audio wants to play
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonCategoryChange");
        CDarwinUtils::DumpAudioDescriptions("AVAudioSessionRouteChangeReasonCategoryChange");
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
        NSLog(@"routeChangeReason : AVAudioSessionRouteChangeReasonRouteConfigurationChange");
        CDarwinUtils::DumpAudioDescriptions("AVAudioSessionRouteChangeReasonRouteConfigurationChange");
        break;
    default:
        NSLog(@"routeChangeReason : unknown notification %ld", (long)routeChangeReason);
        break;
  }
}
- (void)handleAudioInterrupted:(NSNotification *)notification
{
  PRINT_SIGNATURE();
  NSNumber *interruptionType = notification.userInfo[AVAudioSessionInterruptionTypeKey];
  switch (interruptionType.integerValue)
  {
    case AVAudioSessionInterruptionTypeBegan:
      // • Audio has stopped, already inactive
      // • Change state of UI, etc., to reflect non-playing state
      NSLog(@"audioInterrupted : AVAudioSessionInterruptionTypeBegan");
      // pausedForAudioSessionInterruption = YES;
      break;
    case AVAudioSessionInterruptionTypeEnded:
      {
        // • Make session active
        // • Update user interface
        NSNumber *interruptionOption = notification.userInfo[AVAudioSessionInterruptionOptionKey];
        BOOL shouldResume = interruptionOption.integerValue == AVAudioSessionInterruptionOptionShouldResume;
        if (shouldResume == YES)
        {
          // if shouldResume you should continue playback.
          NSLog(@"audioInterrupted : AVAudioSessionInterruptionTypeEnded: resume=yes");
        }
        else
        {
          NSLog(@"audioInterrupted : AVAudioSessionInterruptionTypeEnded: resume=no");
        }
        // pausedForAudioSessionInterruption = NO;
      }
      break;
    default:
      break;
  }
}
- (void)handleMediaServicesReset:(NSNotification *)notification
{
  PRINT_SIGNATURE();
  // Dispose orphaned audio objects and create new audio objects
  // Reset any internal audio state being tracked, including all properties of AVAudioSession
  // When appropriate, reactivate the AVAudioSession using the setActive:error: method
  // test by choosing the "Reset Media Services" selection in the Settings app
}

- (void)registerAudioRouteNotifications
{
  PRINT_SIGNATURE();
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  //register to audio route notifications
  [nc addObserver:self selector:@selector(handleAudioRouteChange:) name:AVAudioSessionRouteChangeNotification object:nil];
  [nc addObserver:self selector:@selector(handleAudioInterrupted:) name:AVAudioSessionInterruptionNotification object:nil];
  [nc addObserver:self selector:@selector(handleMediaServicesReset:) name:AVAudioSessionMediaServicesWereResetNotification object:nil];
}

- (void)unregisterAudioRouteNotifications
{
  PRINT_SIGNATURE();
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  //unregister faudio route notifications
  [nc removeObserver:self name:AVAudioSessionRouteChangeNotification object:nil];
  [nc removeObserver:self name:AVAudioSessionInterruptionNotification object:nil];
  [nc removeObserver:self name:AVAudioSessionMediaServicesWereResetNotification object:nil];
}

- (void)screenDidConnect:(NSNotification *)aNotification
{
  PRINT_SIGNATURE();
}

- (void)screenDidDisconnect:(NSNotification *)aNotification
{
  PRINT_SIGNATURE();
}

- (void)screenModeDidChange:(NSNotification *)aNotification
{
  PRINT_SIGNATURE();
  UIScreen *someScreen = [aNotification object];
  NSLog(@"The screen mode for a screen did change: %@", [someScreen currentMode]);
}

- (void)registerScreenNotifications
{
  PRINT_SIGNATURE();
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  //register to screen notifications
  [nc addObserver:self selector:@selector(screenDidConnect:) name:UIScreenDidConnectNotification object:nil];
  [nc addObserver:self selector:@selector(screenDidDisconnect:) name:UIScreenDidDisconnectNotification object:nil]; 
  [nc addObserver:self selector:@selector(screenModeDidChange:) name:UIScreenModeDidChangeNotification object:nil];
}

- (void)unregisterScreenNotifications
{
  PRINT_SIGNATURE();
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  //unregister from screen notifications
  [nc removeObserver:self name:UIScreenDidConnectNotification object:nil];
  [nc removeObserver:self name:UIScreenDidDisconnectNotification object:nil];
}

@end

int main(int argc, char *argv[])
{
  signal(SIGPIPE, SIG_IGN);
  
  int retVal = 0;
  @try
  {
    retVal = UIApplicationMain(argc,argv,@"UIApplication",@"MainApplicationDelegate");
  }
  @catch (id theException) 
  {
    ELOG(@"%@", theException);
  }
  @finally 
  {
    ILOG(@"This always happens.");
  }
	
  return retVal;

}
