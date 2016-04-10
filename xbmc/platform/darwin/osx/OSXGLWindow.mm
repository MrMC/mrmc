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

#import "Application.h"
#import "guilib/GUIWindowManager.h"
#import "messaging/ApplicationMessenger.h"
#import "settings/DisplaySettings.h"
#import "platform/darwin/osx/CocoaInterface.h"
#import "windowing/osx/WinEventsOSX.h"
#import "windowing/WindowingFactory.h"
#import "platform/MCRuntimeLib.h"

#import "OSXGLView.h"
#import "OSXGLWindow.h"
#import "platform/darwin/DarwinUtils.h"

NSString * const kOSXGLWindowPositionHeightWidth = @"OSXGLWindowPositionHeightWidth";

//------------------------------------------------------------------------------------------
@implementation OSXGLWindow

+(void) SetMenuBarVisible
{
  NSApplicationPresentationOptions options = NSApplicationPresentationDefault;
  [NSApp setPresentationOptions:options];
}

+(void) SetMenuBarInvisible
{
  NSApplicationPresentationOptions options = NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock;
  [NSApp setPresentationOptions:options];
}

-(id) initWithContentRect:(NSRect)box styleMask:(uint)style
{
  self = [super initWithContentRect:box styleMask:style backing:NSBackingStoreBuffered defer:YES];
  [self setDelegate:self];
  [self setAcceptsMouseMovedEvents:YES];
  // autosave the window position/size
  [[self windowController] setShouldCascadeWindows:NO]; // Tell the controller to not cascade its windows.
  [self setFrameAutosaveName:kOSXGLWindowPositionHeightWidth];  // Specify the autosave name for the window.
  [self setFrameUsingName:[self frameAutosaveName] force:YES];
  
  return self;
}

-(void) dealloc
{
  [self setDelegate:nil];
  [super dealloc];
}

- (BOOL)windowShouldClose:(id)sender
{
  if (!g_application.m_bStop)
    KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
  
  return NO;
}

- (void)windowDidExpose:(NSNotification *)notification
{
  //NSLog(@"windowDidExpose");
}

- (void)windowDidMove:(NSNotification *)notification
{
  //NSLog(@"windowDidMove");
  // will update from NSWindow bits
  if (MCRuntimeLib_Initialized())
    g_Windowing.OnMove(-1, -1);
}

- (void)windowDidResize:(NSNotification *)notification
{
  //NSLog(@"windowDidResize");
  if (!MCRuntimeLib_Initialized())
    return;

  NSRect rect = [self contentRectForFrameRect:[self frame]];
  if (!g_Windowing.IsFullScreen())
  {
    int RES_SCREEN = g_Windowing.DesktopResolution(g_Windowing.GetCurrentScreen());
    if(((int)rect.size.width == CDisplaySettings::GetInstance().GetResolutionInfo(RES_SCREEN).iWidth) &&
       ((int)rect.size.height == CDisplaySettings::GetInstance().GetResolutionInfo(RES_SCREEN).iHeight))
      return;
  }

  // send a message so that videoresolution (and refreshrate) is changed
  if (rect.size.width != 0 && rect.size.height != 0)
  {
    XBMC_Event newEvent;
    newEvent.type = XBMC_VIDEORESIZE;
    newEvent.resize.w = (int)rect.size.width;
    newEvent.resize.h = (int)rect.size.height;

    // check for valid sizes cause in some cases
    // we are hit during fullscreen transition from osx
    // and might be technically "zero" sized
    if (newEvent.resize.w != 0 && newEvent.resize.h != 0)
      g_application.OnEvent(newEvent);
  }

  g_windowManager.MarkDirty();
}

-(void)windowDidChangeScreen:(NSNotification *)notification
{
  //NSLog(@"windowDidChangeScreen");
  // user has moved the window to a different screen
  if (!g_Windowing.IsFullScreen())
    g_Windowing.SetMovedToOtherScreen(true);
}

-(NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
  //NSLog(@"windowWillResize");
  return frameSize;
}

-(void)windowWillStartLiveResize:(NSNotification *)notification
{
  //NSLog(@"windowWillStartLiveResize");
}

-(void)windowDidEndLiveResize:(NSNotification *)notification
{
  //NSLog(@"windowDidEndLiveResize");
  if (!MCRuntimeLib_Initialized())
    return;

  NSRect rect = [self contentRectForFrameRect:[self frame]];

  if(!g_Windowing.IsFullScreen())
  {
    int RES_SCREEN = g_Windowing.DesktopResolution(g_Windowing.GetCurrentScreen());
    if(((int)rect.size.width == CDisplaySettings::GetInstance().GetResolutionInfo(RES_SCREEN).iWidth) &&
       ((int)rect.size.height == CDisplaySettings::GetInstance().GetResolutionInfo(RES_SCREEN).iHeight))
      return;
  }

  // send a message so that videoresolution (and refreshrate) is changed
  if (rect.size.width != 0 && rect.size.height != 0)
  {
    XBMC_Event newEvent;
    newEvent.type = XBMC_VIDEORESIZE;
    newEvent.resize.w = (int)rect.size.width;
    newEvent.resize.h = (int)rect.size.height;

    // check for valid sizes cause in some cases
    // we are hit during fullscreen transition from osx
    // and might be technically "zero" sized
    if (newEvent.resize.w != 0 && newEvent.resize.h != 0)
      g_application.OnEvent(newEvent);
  }

  g_windowManager.MarkDirty();
}

-(void)windowDidEnterFullScreen: (NSNotification*)notification
{
}

-(void)windowWillEnterFullScreen: (NSNotification*)notification
{
  // if osx is the issuer of the toggle
  // call XBMCs toggle function
  if (!g_Windowing.GetFullscreenWillToggle())
  {
    // indicate that we are toggling
    // flag will be reset in SetFullscreen once its
    // called from XBMCs gui thread
    g_Windowing.SetFullscreenWillToggle(true);
    KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_TOGGLEFULLSCREEN);
  }
  else
  {
    // in this case we are just called because
    // of xbmc did a toggle - just reset the flag
    // we don't need to do anything else
    g_Windowing.SetFullscreenWillToggle(false);
  }
}

-(void)windowDidExitFullScreen: (NSNotification*)notification
{
  // if osx is the issuer of the toggle
  // call XBMCs toggle function
  if (!g_Windowing.GetFullscreenWillToggle())
  {
    // indicate that we are toggling
    // flag will be reset in SetFullscreen once its
    // called from XBMCs gui thread
    g_Windowing.SetFullscreenWillToggle(true);
    KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_TOGGLEFULLSCREEN);
    
  }
  else
  {
    // in this case we are just called because
    // of xbmc did a toggle - just reset the flag
    // we don't need to do anything else
    g_Windowing.SetFullscreenWillToggle(false);
  }
}

-(void)windowWillExitFullScreen: (NSNotification*)notification
{
  
}

- (NSApplicationPresentationOptions) window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
  return (proposedOptions| NSApplicationPresentationAutoHideToolbar);
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
  //NSLog(@"windowDidMiniaturize");
  MCRuntimeLib_SetRenderGUI(false);
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
  //NSLog(@"windowDidDeminiaturize");
  MCRuntimeLib_SetRenderGUI(true);
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  //NSLog(@"windowDidBecomeKey");
  CWinEventsOSXImp::EnableInput();
}

- (void)windowDidResignKey:(NSNotification *)notification
{
  //NSLog(@"windowDidResignKey");
  CWinEventsOSXImp::DisableInput();
}

-(void) mouseDown:(NSEvent *) theEvent
{
  //NSLog(@"mouseDown");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) rightMouseDown:(NSEvent *) theEvent
{
  //NSLog(@"rightMouseDown");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) otherMouseDown:(NSEvent *) theEvent
{
  //NSLog(@"otherMouseDown");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) mouseUp:(NSEvent *) theEvent
{
  //NSLog(@"mouseUp");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) rightMouseUp:(NSEvent *) theEvent
{
  //NSLog(@"rightMouseUp");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) otherMouseUp:(NSEvent *) theEvent
{
  //NSLog(@"otherMouseUp");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) mouseMoved:(NSEvent *) theEvent
{
  //NSLog(@"mouseMoved");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) mouseDragged:(NSEvent *) theEvent
{
  //NSLog(@"mouseDragged");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) rightMouseDragged:(NSEvent *) theEvent
{
  //NSLog(@"rightMouseDragged");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) otherMouseDragged:(NSEvent *) theEvent
{
  //NSLog(@"otherMouseDragged");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

-(void) scrollWheel:(NSEvent *) theEvent
{
  //NSLog(@"scrollWheel");
  // if it is hidden - mouse is belonging to us!
  if (Cocoa_IsMouseHidden())
    CWinEventsOSXImp::HandleInputEvent(theEvent);
}

- (BOOL) canBecomeKeyWindow
{
  return YES;
}
@end
