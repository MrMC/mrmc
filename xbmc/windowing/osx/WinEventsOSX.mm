/*
 *      Copyright (C) 2011-2013 Team XBMC
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

#include "windowing/osx/WinEventsOSX.h"
#include "input/XBMC_vkeys.h"
#include "Application.h"
#include "windowing/WindowingFactory.h"
#include "threads/CriticalSection.h"
#include "guilib/GUIWindowManager.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/log.h"
#include "input/MouseStat.h"
#include "GUIUserMessages.h"
#include "platform/darwin/osx/CocoaInterface.h"

#import "platform/darwin/osx/OSXGLWindow.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <IOKit/hidsystem/ev_keymap.h>

#define NX_KEYSTATE_DOWN    0x0A
#define NX_KEYSTATE_UP      0x0B

bool ProcessOSXShortcuts(XBMC_Event& event)
{
  bool cmd = !!(event.key.keysym.mod & (XBMCKMOD_LMETA | XBMCKMOD_RMETA));
  if (cmd && event.key.type == XBMC_KEYDOWN)
  {
    switch(event.key.keysym.sym)
    {
      case XBMCK_q:  // CMD-q to quit
        if (!g_application.m_bStop)
          KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
        return true;
        
      case XBMCK_f: // CMD-f to toggle fullscreen
        //g_application.OnAction(CAction(ACTION_TOGGLE_FULLSCREEN));
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_TOGGLEFULLSCREEN);

        return true;
        
      case XBMCK_s: // CMD-s to take a screenshot
        g_application.OnAction(CAction(ACTION_TAKE_SCREENSHOT));
        return true;
        
      case XBMCK_h: // CMD-h to hide (but we minimize for now)
      case XBMCK_m: // CMD-m to minimize
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_MINIMIZE);
        return true;
        
      case XBMCK_v: // CMD-v to paste clipboard text
        if (g_Windowing.IsTextInputEnabled())
        {
          const char *szStr = Cocoa_Paste();
          if (szStr)
          {
            CGUIMessage msg(ACTION_INPUT_TEXT, 0, 0);
            msg.SetLabel(szStr);
            g_windowManager.SendMessage(msg, g_windowManager.GetFocusedWindow());
          }
        }
        return true;
        
      default:
        return false;
    }
  }
  
  return false;
}

UniChar OsxKey2XbmcKey(UniChar character)
{
  switch(character)
  {
    case 0x1c:
    case 0xf702:
      return XBMCK_LEFT;
    case 0x1d:
    case 0xf703:
      return XBMCK_RIGHT;
    case 0x1e:
    case 0xf700:
      return XBMCK_UP;
    case 0x1f:
    case 0xf701:
      return XBMCK_DOWN;
    case 0x7f:
      return XBMCK_BACKSPACE;
    default:
      return character;
  }
}

XBMCMod OsxMod2XbmcMod(CGEventFlags appleModifier)
{
  unsigned int xbmcModifier = XBMCKMOD_NONE;
  // shift left
  if (appleModifier & kCGEventFlagMaskAlphaShift)
  xbmcModifier |= XBMCKMOD_LSHIFT;
  // shift right
  if (appleModifier & kCGEventFlagMaskShift)
  xbmcModifier |= XBMCKMOD_RSHIFT;
  // left ctrl
  if (appleModifier & kCGEventFlagMaskControl)
  xbmcModifier |= XBMCKMOD_LCTRL;
  // left alt/option
  if (appleModifier & kCGEventFlagMaskAlternate)
  xbmcModifier |= XBMCKMOD_LALT;
  // left command
  if (appleModifier & kCGEventFlagMaskCommand)
  xbmcModifier |= XBMCKMOD_LMETA;

  return (XBMCMod)xbmcModifier;
}

// place holder for future native osx event handler
void toggleKey(XBMCKey key)
{
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  newEvent.key.keysym.sym = key;
  newEvent.type = XBMC_KEYDOWN;
  CWinEvents::MessagePush(&newEvent);
  newEvent.type = XBMC_KEYUP;
  CWinEvents::MessagePush(&newEvent);
}

//------------------------------------------------------------------------------------------
// former hotkeycontroller stuff is handled here
// WARNING: do not debugger breakpoint in this routine.
// It's a system level call back that taps ALL Events
// and you WILL lose all key control :)
CGEventRef HotKeyEventHandler(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
  bool passEvent = false;
  CWinEventsOSXImp *winEvents = (CWinEventsOSXImp*)refcon;
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  
  if (type == kCGEventTapDisabledByTimeout)
  {
    if (winEvents->GetEventTap() && winEvents->AreHotKeysEnabled())
      CGEventTapEnable((CFMachPortRef)winEvents->GetEventTap(), true);
    return NULL;
  }
  
  if (!g_application.m_renderGUI || !winEvents->AreHotKeysEnabled())
    return event;

 
  NSEvent *nsEvent = [NSEvent eventWithCGEvent:event];
  if (!nsEvent || [nsEvent subtype] != 8)
      return event;  
  
  int data = [nsEvent data1];
  int keyCode  = (data & 0xFFFF0000) >> 16;
  int keyFlags = (data & 0xFFFF);
  int keyState = (keyFlags & 0xFF00) >> 8;
  BOOL keyIsRepeat = (keyFlags & 0x1) > 0;
  
  // allow repeated keypresses for volume buttons
  // all other repeated keypresses are handled by the os (is this really good?)
  if (keyIsRepeat && keyCode != NX_KEYTYPE_SOUND_UP && keyCode != NX_KEYTYPE_SOUND_DOWN) 
    return event;
  
  // hotkeys mostly only fire a down event - we add the up event ourselves...
  switch (keyCode)
  {
    case NX_POWER_KEY:
      if (!winEvents->TapPowerKey() || (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN))
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_SLEEP);
      }
      break;
    case NX_KEYTYPE_MUTE:
      if (!winEvents->TapVolumeKeys() || (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN))
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_VOLUME_MUTE);
      }
      break;
    case NX_KEYTYPE_SOUND_UP:
      if (!winEvents->TapVolumeKeys() || (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN))
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_VOLUME_UP);
      }
      break;
    case NX_KEYTYPE_SOUND_DOWN:
      if (!winEvents->TapVolumeKeys() || (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN))
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_VOLUME_DOWN);
      }
      break;
    case NX_KEYTYPE_PLAY:
      if (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN)
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_MEDIA_PLAY_PAUSE);
      }
      break;
    case NX_KEYTYPE_FAST:
      if (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN)
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_FASTFORWARD);
      }
      break;
    case NX_KEYTYPE_REWIND:
      if (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN)
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_REWIND);
      }
      break;
    case NX_KEYTYPE_NEXT:
      if (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN)
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_MEDIA_NEXT_TRACK);
      }
      break;
    case NX_KEYTYPE_PREVIOUS:
      if (keyState != NX_KEYSTATE_UP && keyState != NX_KEYSTATE_DOWN)
      {
        passEvent = true;
      }
      else
      {
        if (keyState == NX_KEYSTATE_DOWN)
          toggleKey(XBMCK_MEDIA_PREV_TRACK);
      }
      break;
    default:
      passEvent = true;
  }
  
  if (passEvent)
    return event;
  else
    return NULL;
}

//------------------------------------------------------------------------------------------
NSEvent* InputEventHandler(NSEvent *nsevent)
{
  bool passEvent = true;
  CGEventRef event = [nsevent CGEvent];
  CGEventType type = CGEventGetType(event);

  NSWindow *targetWindowForEvent = [nsevent window];
  if (!targetWindowForEvent)
    return nsevent;

  // The incoming mouse position.
  NSPoint location = [nsevent locationInWindow];
  NSView  *view = [targetWindowForEvent contentView];
  location = [view convertPoint:location fromView:nil];
  if (NSPointInRect(location, [view frame]) == NO)
  {
    if (type != kCGEventKeyUp && type != kCGEventKeyDown)
      return nsevent;
  }

  // cocoa world is upside down ...
  location.y = g_Windowing.CocoaToNativeFlip(location.y);
  
  UniChar unicodeString[10];
  UniCharCount actualStringLength = 10;
  CGKeyCode keycode;
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  
  switch (type)
  {
    // handle mouse events and transform them into the xbmc event world
    case kCGEventLeftMouseUp:
      newEvent.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.button = XBMC_BUTTON_LEFT;
      newEvent.button.state = XBMC_RELEASED;
      newEvent.button.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventLeftMouseDown:
      newEvent.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.button = XBMC_BUTTON_LEFT;
      newEvent.button.state = XBMC_PRESSED;
      newEvent.button.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventRightMouseUp:
      newEvent.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.button = XBMC_BUTTON_RIGHT;
      newEvent.button.state = XBMC_RELEASED;
      newEvent.button.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventRightMouseDown:
      newEvent.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.button = XBMC_BUTTON_RIGHT;
      newEvent.button.state = XBMC_PRESSED;
      newEvent.button.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventOtherMouseUp:
      newEvent.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.button = XBMC_BUTTON_MIDDLE;
      newEvent.button.state = XBMC_RELEASED;
      newEvent.button.type = XBMC_MOUSEBUTTONUP;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventOtherMouseDown:
      newEvent.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.button = XBMC_BUTTON_MIDDLE;
      newEvent.button.state = XBMC_PRESSED;
      newEvent.button.type = XBMC_MOUSEBUTTONDOWN;
      newEvent.button.which = 0;
      newEvent.button.x = location.x;
      newEvent.button.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged:
    case kCGEventOtherMouseDragged:
      newEvent.type = XBMC_MOUSEMOTION;
      newEvent.motion.type = XBMC_MOUSEMOTION;
      newEvent.motion.xrel = CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
      newEvent.motion.yrel = CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);
      newEvent.motion.state = 0;
      newEvent.motion.which = 0;
      newEvent.motion.x = location.x;
      newEvent.motion.y = location.y;
      CWinEvents::MessagePush(&newEvent);
      break;
    case kCGEventScrollWheel:
      // very strange, real scrolls have non-zero deltaY followed by same number of events
      // with a zero deltaY. This reverses our scroll which is WTF? anoying. Trap them out here.
      if ([nsevent deltaY] != 0.0)
      {
        newEvent.type = XBMC_MOUSEBUTTONDOWN;
        newEvent.button.type = XBMC_MOUSEBUTTONDOWN;
        newEvent.button.state = XBMC_PRESSED;
        newEvent.button.x = location.x;
        newEvent.button.y = location.y;
        newEvent.button.which = 0;
        newEvent.button.button = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1) > 0 ? XBMC_BUTTON_WHEELUP : XBMC_BUTTON_WHEELDOWN;
        CWinEvents::MessagePush(&newEvent);

        newEvent.type = XBMC_MOUSEBUTTONUP;
        newEvent.button.type = XBMC_MOUSEBUTTONUP;
        newEvent.button.state = XBMC_RELEASED;
        CWinEvents::MessagePush(&newEvent);
      }
      break;
      
    // handle keyboard events and transform them into the xbmc event world
    case kCGEventKeyUp:
      keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
      CGEventKeyboardGetUnicodeString(event, sizeof(unicodeString) / sizeof(*unicodeString), &actualStringLength, unicodeString);
      if ( actualStringLength > 0 )
          unicodeString[0] = OsxKey2XbmcKey(unicodeString[0]);
      else
          unicodeString[0] = OsxKey2XbmcKey([[nsevent characters] characterAtIndex:0]);

      newEvent.type = XBMC_KEYUP;
      newEvent.key.keysym.scancode = keycode;
      newEvent.key.keysym.sym = (XBMCKey) unicodeString[0];
      newEvent.key.keysym.unicode = unicodeString[0];
      if (actualStringLength > 1)
        newEvent.key.keysym.unicode |= (unicodeString[1] << 8);
      newEvent.key.state = XBMC_RELEASED;
      newEvent.key.type = XBMC_KEYUP;
      newEvent.key.which = 0;
      newEvent.key.keysym.mod = OsxMod2XbmcMod(CGEventGetFlags(event));
      CWinEvents::MessagePush(&newEvent);
      passEvent = false;
      break;
    case kCGEventKeyDown:     
      keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
      CGEventKeyboardGetUnicodeString(event, sizeof(unicodeString) / sizeof(*unicodeString), &actualStringLength, unicodeString);
      if ( actualStringLength > 0 )
        unicodeString[0] = OsxKey2XbmcKey(unicodeString[0]);
      else
        unicodeString[0] = OsxKey2XbmcKey([[nsevent characters] characterAtIndex:0]);

      newEvent.type = XBMC_KEYDOWN;
      newEvent.key.keysym.scancode = keycode;
      newEvent.key.keysym.sym = (XBMCKey) unicodeString[0];
      newEvent.key.keysym.unicode = unicodeString[0];
      if (actualStringLength > 1)
        newEvent.key.keysym.unicode |= (unicodeString[1] << 8);
      newEvent.key.state = XBMC_PRESSED ;
      newEvent.key.type = XBMC_KEYDOWN;
      newEvent.key.which = 0;
      newEvent.key.keysym.mod = OsxMod2XbmcMod(CGEventGetFlags(event));
      
      if (!ProcessOSXShortcuts(newEvent))
        CWinEvents::MessagePush(&newEvent);
      passEvent = false;
      
      break;
    default:
      return nsevent;
  }
  // We must return the event for it to be useful if not already handled
  if (passEvent)
    return nsevent;
  else
    return NULL;
}

//------------------------------------------------------------------------------------------
void CWinEventsOSX::MessagePush(XBMC_Event *newEvent)
{
  CWinEventsOSXImp::MessagePush(newEvent);
}

bool CWinEventsOSX::MessagePump()
{
  return CWinEventsOSXImp::MessagePump();
}

size_t CWinEventsOSX::GetQueueSize()
{
  return CWinEventsOSXImp::GetQueueSize();
}

//------------------------------------------------------------------------------------------
static CCriticalSection g_inputCond;
static std::list<XBMC_Event> events;

CWinEventsOSXImp* CWinEventsOSXImp::WinEvents = 0;
CWinEventsOSXImp::CWinEventsOSXImp()
{
  mTapPowerKey   = true; // we tap the power key (but can't prevent the os from evaluating is aswell
  mTapVolumeKeys = false;// we don't tap the volume keys - they control system volume
  mHotKeysEnabled = false;
  mLocalMonitorId = nil;
  m_TapThread = new CThread(this, "OSXEventTapThread");
  //enableHotKeyTap();//disabled for now
}

CWinEventsOSXImp::~CWinEventsOSXImp()
{
  //disableHotKeyTap();
  disableInputEvents();
  delete m_TapThread;
}

void CWinEventsOSXImp::MessagePush(XBMC_Event *newEvent)
{
  if (!WinEvents)
    return;

  CSingleLock lock(g_inputCond);
  events.push_back(*newEvent);
}

bool CWinEventsOSXImp::MessagePump()
{
  if (!WinEvents)
    return false;
  
  bool ret = false;
  
  // Do not always loop, only pump the initial queued count events. else if ui keep pushing
  // events the loop won't finish then it will block xbmc main message loop.
  for (size_t pumpEventCount = GetQueueSize(); pumpEventCount > 0; --pumpEventCount)
  {
    // Pop up only one event per time since in App::OnEvent it may init modal dialog which init
    // deeper message loop and call the deeper MessagePump from there.
    XBMC_Event pumpEvent;
    {
      CSingleLock lock(g_inputCond);
      if (events.size() == 0)
        return ret;
      pumpEvent = events.front();
      events.pop_front();
    }
    ret |= g_application.OnEvent(pumpEvent);
  }
  return ret;
}

size_t CWinEventsOSXImp::GetQueueSize()
{
  if (!WinEvents)
    return 0;

  CSingleLock lock(g_inputCond);
  return events.size();
}

void CWinEventsOSXImp::EnableInput()
{
  WinEvents = new CWinEventsOSXImp();

  WinEvents->SetHotKeysEnabled(true);
  WinEvents->enableInputEvents();
}

void CWinEventsOSXImp::DisableInput()
{
  if (!WinEvents)
    return;

  WinEvents->SetHotKeysEnabled(false);
  WinEvents->disableInputEvents();

  delete WinEvents, WinEvents = NULL;
}

void CWinEventsOSXImp::HandleInputEvent(void *event)
{
  if (!WinEvents)
    return;

  if(event && WinEvents->mLocalMonitorId != nil)
    InputEventHandler((NSEvent*)event);
}

// from hotkeycontroller - tapping events in its own thread
void CWinEventsOSXImp::Run()
{
  mRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorSystemDefault, (CFMachPortRef)mEventTap, 0);
  mRunLoop = CFRunLoopGetCurrent();
  CFRunLoopAddSource((CFRunLoopRef)mRunLoop, (CFRunLoopSourceRef)mRunLoopSource, kCFRunLoopCommonModes);
  // Enable the event tap.
  CGEventTapEnable((CFMachPortRef)mEventTap, TRUE);

  CLog::Log(LOGDEBUG, "EventTap enabled...");
  CFRunLoopRun();
  CLog::Log(LOGDEBUG, "EventTap disabled...");

  SetHotKeysEnabled(false);

  if (mEventTap)
    CGEventTapEnable((CFMachPortRef)mEventTap, FALSE);

  if (mRunLoopSource)
  {
    CFRunLoopRemoveSource((CFRunLoopRef)mRunLoop, (CFRunLoopSourceRef)mRunLoopSource, kCFRunLoopCommonModes);
    CFRelease((CFRunLoopSourceRef)mRunLoopSource);
  }
  mRunLoopSource = NULL;

  if (mEventTap)
    CFRelease((CFMachPortRef)mEventTap);
  mEventTap = NULL;
}

void CWinEventsOSXImp::enableInputEvents()
{
  NSEventMask eventMask;
  disableInputEvents();// allow only one registration at a time

  // Create an event tap. We are interested in mouse and keyboard events.
  eventMask = NSLeftMouseDownMask |
              NSLeftMouseUpMask |
              NSRightMouseDownMask |
              NSRightMouseUpMask |
              NSLeftMouseDraggedMask |
              NSRightMouseDraggedMask |
              NSOtherMouseDownMask |
              NSOtherMouseUpMask |
              NSOtherMouseDraggedMask |
              NSMouseMovedMask |
              NSScrollWheelMask |
              NSKeyDownMask |
              NSKeyUpMask;

  mLocalMonitorId = [NSEvent addLocalMonitorForEventsMatchingMask:eventMask handler:^(NSEvent *event){
    return InputEventHandler(event);
  }];
}

void CWinEventsOSXImp::enableHotKeyTap()
{
  disableHotKeyTap();
  if (!m_TapThread->IsRunning())
  {
    // Create an event tap. We are interested hot key events and mouse move events.
    // tap former hotkeycontroller stuff   
    CGEventMask eventMask = CGEventMaskBit(NX_SYSDEFINED);
    mEventTap = CGEventTapCreate(kCGSessionEventTap,
                                   kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                   eventMask, HotKeyEventHandler, this);
    if (mEventTap != NULL)
    {
      m_TapThread->Create();
      SetHotKeysEnabled(true);
    }
  }
}

void CWinEventsOSXImp::disableInputEvents()
{
  // Disable the local Monitor
  if (mLocalMonitorId != nil)
    [NSEvent removeMonitor:(id)mLocalMonitorId];
  mLocalMonitorId = nil;
}

void CWinEventsOSXImp::disableHotKeyTap()
{  
  // Disable the event tap.
  if (mRunLoopSource)
  {
    CFRunLoopStop((CFRunLoopRef)mRunLoop);
    m_TapThread->StopThread(true);  
  }  
  SetHotKeysEnabled(false);
}



