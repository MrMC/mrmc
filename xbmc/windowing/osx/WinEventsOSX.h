#pragma once

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

#include "windowing/WinEvents.h"
#include "threads/Thread.h"

class CWinEventsOSX : public IWinEvents
{
public:
  void MessagePush(XBMC_Event *newEvent);
  bool MessagePump();
  virtual size_t  GetQueueSize();
};

class CWinEventsOSXImp: public IRunnable
{
public:
  CWinEventsOSXImp();
  virtual ~CWinEventsOSXImp();
  static void MessagePush(XBMC_Event *newEvent);
  static bool MessagePump();
  static size_t GetQueueSize();

  static void EnableInput();
  static void DisableInput();
  static void HandleInputEvent(void *event);

  void *GetEventTap(){return mEventTap;}
  bool TapVolumeKeys(){return mTapVolumeKeys;}
  bool TapPowerKey(){return mTapPowerKey;}
  void SetHotKeysEnabled(bool enable){mHotKeysEnabled = enable;}
  bool AreHotKeysEnabled(){return mHotKeysEnabled;}

  virtual void Run();
  
private:
  static CWinEventsOSXImp *WinEvents;

  void *mRunLoopSource;
  void *mRunLoop;
  void *mEventTap;
  void *mLocalMonitorId;
  bool mHotKeysEnabled;
  bool mTapVolumeKeys;
  bool mTapPowerKey;
  CThread *m_TapThread;
  
  void enableHotKeyTap();
  void disableHotKeyTap();
  void enableInputEvents();
  void disableInputEvents();
};
