#pragma once
/*
 *      Copyright (C) 2017 Team MrMC
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

#include <atomic>

#include "filesystem/CurlFile.h"
#include "services/ServicesManager.h"
#include "settings/lib/ISettingCallback.h"
#include "interfaces/IAnnouncer.h"
#include "utils/JobManager.h"

#define TRAKT_DEBUG_VERBOSE

typedef struct TraktPlayState
{
  std::string path;
  MediaServicesPlayerState state = MediaServicesPlayerState::stopped;
} TraktPlayState;

class CTraktServices
: public CJobQueue
, public ISettingCallback
, public ANNOUNCEMENT::IAnnouncer
{
  friend class CTraktServiceJob;

public:
  CTraktServices();
  CTraktServices(const CTraktServices&);
  virtual ~CTraktServices();
  static CTraktServices &GetInstance();

  bool              IsEnabled();
  // IAnnouncer callbacks
  virtual void      Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) override;
  // ISettingCallback
  virtual void      OnSettingAction(const CSetting *setting) override;
  virtual void      OnSettingChanged(const CSetting *setting) override;

  void              SetItemWatched(CFileItem &item);
  void              SetItemUnWatched(CFileItem &item);
  void              SaveFileState(CFileItem &item, double currentTime, double totalTime);

protected:
  static void       ReportProgress(CFileItem &item, const std::string &status, double percentage);

private:
  // private construction, and no assignements; use the provided singleton methods
  void              SetUserSettings();
  void              SetButtonText(std::string text, std::string button);
  void              GetUserSettings();
  bool              MyTraktSignedIn();

  bool              GetSignInPinCode();
  bool              GetSignInByPinReply();
  const MediaServicesPlayerState GetPlayState(CFileItem &item);
  void              SetPlayState(CFileItem &item, const MediaServicesPlayerState &state);
  static CVariant   ParseIds(const std::map<std::string, std::string> &Ids, const std::string &type);
  static void       PullWatchedStatus();  
  static void       PushWatchedStatus();
  static bool       CompareMovieToHistory(CFileItem &mItem, const CVariant &watchedMovies);
  static bool       CompareEpisodeToHistory(CFileItem &eItem, const CVariant &watchedShows, CFileItem &sItem);
  static CVariant   GetTraktCVariant(const std::string &url);
  static void       ServerChat(const std::string &url, const CVariant &data);
  static void       SetItemWatchedJob(CFileItem &item, bool watched, bool setLastWatched = false);
  void              RefreshAccessToken();
  void              CheckAccessToken();

  std::atomic<bool> m_active;
  std::atomic<bool> m_canSync;
  CCriticalSection  m_critical;
  CEvent            m_processSleep;

  std::string       m_authToken;
  int               m_authTokenValidity;
  std::string       m_refreshAuthToken;
  std::string       m_deviceCode;
  XFILE::CCurlFile  m_Trakttv;
  CCriticalSection  m_playStatesLock;
  // needed to track play/pause via "OnPlay" Announce msgs
  std::vector<TraktPlayState> m_playStates;
};
