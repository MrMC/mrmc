/*
 *      Copyright (C) 2016 Team MrMC
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

#include <algorithm>

#include "services/ServicesManager.h"
#include "Application.h"
#include "guilib/LocalizeStrings.h"
#include "interfaces/AnnouncementManager.h"
#include "utils/JobManager.h"
#include "utils/log.h"
#include "utils/StringHasher.h"
#include "video/VideoInfoTag.h"
#include "services/plex/PlexUtils.h"
#include "services/emby/EmbyUtils.h"


using namespace ANNOUNCEMENT;

class CServicesManagerJob: public CJob
{
public:
  CServicesManagerJob(CFileItem &item, double currentTime, std::string strFunction)
  : m_item(item)
  , m_function(strFunction)
  , m_currentTime(currentTime)
  {
  }
  virtual ~CServicesManagerJob()
  {
  }
  virtual bool DoWork()
  {
    using namespace StringHasher;
    switch(mkhash(m_function.c_str()))
    {
      case "PlexSetWatched"_mkhash:
        CPlexUtils::SetWatched(m_item);
        break;
      case "PlexSetUnWatched"_mkhash:
        CPlexUtils::SetUnWatched(m_item);
        break;
      case "PlexSetProgress"_mkhash:
        CPlexUtils::ReportProgress(m_item, m_currentTime);
        break;
      case "EmbySetWatched"_mkhash:
        CEmbyUtils::SetWatched(m_item);
        break;
      case "EmbySetUnWatched"_mkhash:
        CEmbyUtils::SetUnWatched(m_item);
        break;
      case "EmbySetProgress"_mkhash:
        CEmbyUtils::ReportProgress(m_item, m_currentTime);
        break;
      default:
        return false;
    }
    return true;
  }
  virtual bool operator==(const CJob *job) const
  {
    return true;
  }
private:
  CFileItem      m_item;
  std::string    m_function;
  double         m_currentTime;
};


CServicesManager::CServicesManager()
{
  CAnnouncementManager::GetInstance().AddAnnouncer(this);
}

CServicesManager::~CServicesManager()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
}

CServicesManager& CServicesManager::GetInstance()
{
  static CServicesManager sServicesManager;
  return sServicesManager;
}

void CServicesManager::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  using namespace StringHasher;
  //CLog::Log(LOGDEBUG, "CServicesManager::Announce [%s], [%s], [%s]", ANNOUNCEMENT::AnnouncementFlagToString(flag), sender, message);
  if ((flag & AnnouncementFlag::Player) && strcmp(sender, "xbmc") == 0)
  {
    if (g_application.CurrentFileItem().HasProperty("MediaServicesItem"))
    {
     bool isPlex = g_application.CurrentFileItem().HasProperty("PlexItem");
     bool isEmby = g_application.CurrentFileItem().HasProperty("EmbyItem");

     switch(mkhash(message))
      {
        case "OnPlay"_mkhash:
          if (isPlex)
            CPlexUtils::SetPlayState(MediaServicesPlayerState::playing);
          else if (isEmby)
            CEmbyUtils::SetPlayState(MediaServicesPlayerState::playing);
          break;
        case "OnPause"_mkhash:
          if (isPlex)
            CPlexUtils::SetPlayState(MediaServicesPlayerState::paused);
          else if (isEmby)
            CEmbyUtils::SetPlayState(MediaServicesPlayerState::paused);
          break;
        case "OnStop"_mkhash:
        {
          std::string msg;
          if (isPlex)
          {
            msg = "PlexSetProgress";
            CPlexUtils::SetPlayState(MediaServicesPlayerState::stopped);
          }
          else if (isEmby)
          {
            msg = "EmbySetProgress";
            CEmbyUtils::SetPlayState(MediaServicesPlayerState::stopped);
          }
          if (!msg.empty())
          {
            CFileItem &item = g_application.CurrentFileItem();
            AddJob(new CServicesManagerJob(g_application.CurrentFileItem(), item.GetVideoInfoTag()->m_resumePoint.timeInSeconds, msg));
          }
          break;
        }
        default:
          break;
      }
    }
  }
}

bool CServicesManager::HasServices()
{
  bool rtn = CPlexUtils::HasClients() ||
             CEmbyUtils::HasClients();
  return rtn;
}

bool CServicesManager::GetStartFolder(std::string &path)
{
  bool rtn = false;
  // prefer plex over emby if both are enabled.
  // TODO, get more clever later
  if (CPlexUtils::HasClients())
  {
    path = "plex://" + path;
    rtn = true;
  }
  else if (CEmbyUtils::HasClients())
  {
    path = "emby://" + path;
    rtn = true;
  }

  return rtn;
}

bool CServicesManager::IsMediaServicesItem(const CFileItem &item)
{
  if (item.HasProperty("MediaServicesItem"))
    return item.GetProperty("MediaServicesItem").asBoolean();
  return false;
}

bool CServicesManager::IsMediaServicesCloudItem(const CFileItem &item)
{
  if (item.HasProperty("MediaServicesCloudItem"))
    return item.GetProperty("MediaServicesCloudItem").asBoolean();
  return false;
}

bool CServicesManager::UpdateMediaServicesLibraries(const CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::Other, "plex", "UpdateLibrary");
  if (item.HasProperty("EmbyItem"))
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::Other, "emby", "UpdateLibrary");
  return true;
}

bool CServicesManager::ReloadProfiles()
{
  if (CPlexUtils::HasClients())
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::Other, "plex", "ReloadProfiles");
  if (CEmbyUtils::HasClients())
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::Other, "emby", "ReloadProfiles");
  return true;
}

void CServicesManager::SetItemWatched(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    AddJob(new CServicesManagerJob(item, 0, "PlexSetWatched"));
  else if (item.HasProperty("EmbyItem"))
    AddJob(new CServicesManagerJob(item, 0, "EmbySetWatched"));
}

void CServicesManager::SetItemUnWatched(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    AddJob(new CServicesManagerJob(item, 0, "PlexSetUnWatched"));
  else if (item.HasProperty("EmbyItem"))
    AddJob(new CServicesManagerJob(item, 0, "EmbySetUnWatched"));
}

void CServicesManager::UpdateItemState(CFileItem &item, double currentTime)
{
  if (item.HasProperty("PlexItem"))
    AddJob(new CServicesManagerJob(item, currentTime, "PlexSetProgress"));
  else if (item.HasProperty("EmbyItem"))
    AddJob(new CServicesManagerJob(item, currentTime, "EmbySetProgress"));
}

void CServicesManager::ShowMusicInfo(CFileItem item)
{
  if (item.HasProperty("PlexItem"))
    CPlexUtils::ShowMusicInfo(item);
  else if (item.HasProperty("EmbyItem"))
    CEmbyUtils::ShowMusicInfo(item);
}

void CServicesManager::GetAllRecentlyAddedMovies(CFileItemList &recentlyAdded, int itemLimit)
{
  bool hasRecentlyAdded = false;
  if (CPlexUtils::HasClients())
    hasRecentlyAdded |= CPlexUtils::GetAllPlexRecentlyAddedMoviesAndShows(recentlyAdded, false);
  if (CEmbyUtils::HasClients())
    hasRecentlyAdded |= CEmbyUtils::GetAllEmbyRecentlyAddedMoviesAndShows(recentlyAdded, false);

  if (hasRecentlyAdded)
  {
    CFileItemList temp;
    recentlyAdded.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < recentlyAdded.Size() && i < itemLimit; i++)
    {
      CFileItemPtr item = recentlyAdded.Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(20386));
      temp.Add(item);
    }

    recentlyAdded.ClearItems();
    recentlyAdded.Append(temp);
  }
}

void CServicesManager::GetAllRecentlyAddedShows(CFileItemList &recentlyAdded, int itemLimit)
{
  bool hasRecentlyAdded = false;
  if (CPlexUtils::HasClients())
    hasRecentlyAdded |= CPlexUtils::GetAllPlexRecentlyAddedMoviesAndShows(recentlyAdded, true);
  if (CEmbyUtils::HasClients())
    hasRecentlyAdded |= CEmbyUtils::GetAllEmbyRecentlyAddedMoviesAndShows(recentlyAdded, true);

  if (hasRecentlyAdded)
  {
    CFileItemList temp;
    recentlyAdded.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < recentlyAdded.Size() && i < itemLimit; i++)
    {
      CFileItemPtr item = recentlyAdded.Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(20387));
      temp.Add(item);
    }

    recentlyAdded.ClearItems();
    recentlyAdded.Append(temp);
  }
}

void CServicesManager::GetAllRecentlyAddedAlbums(CFileItemList &recentlyAdded, int itemLimit)
{
  bool hasRecentlyAdded = false;
  if (CPlexUtils::HasClients())
    hasRecentlyAdded |= CPlexUtils::GetPlexRecentlyAddedAlbums(recentlyAdded, itemLimit);
  if (CEmbyUtils::HasClients())
    hasRecentlyAdded |= CEmbyUtils::GetEmbyRecentlyAddedAlbums(recentlyAdded, itemLimit);

  if (hasRecentlyAdded)
  {
    CFileItemList temp;
    recentlyAdded.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < recentlyAdded.Size() && i < itemLimit; i++)
    {
      CFileItemPtr item = recentlyAdded.Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(359));
      temp.Add(item);
    }
    
    recentlyAdded.ClearItems();
    recentlyAdded.Append(temp);
  }
}

void CServicesManager::GetAllInProgressShows(CFileItemList &inProgress, int itemLimit)
{
  bool hasInProgress= false;
  if (CPlexUtils::HasClients())
    hasInProgress |= CPlexUtils::GetAllPlexInProgress(inProgress, true);
  if (CEmbyUtils::HasClients())
    hasInProgress |= CEmbyUtils::GetAllEmbyInProgress(inProgress, true);

  if (hasInProgress)
  {
    CFileItemList temp;
    inProgress.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < inProgress.Size() && i < itemLimit; i++)
    {
      CFileItemPtr item = inProgress.Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(626));
      temp.Add(item);
    }
    
    inProgress.ClearItems();
    inProgress.Append(temp);
  }
}

void CServicesManager::GetAllInProgressMovies(CFileItemList &inProgress, int itemLimit)
{
  bool hasInProgress= false;
  if (CPlexUtils::HasClients())
    hasInProgress |= CPlexUtils::GetAllPlexInProgress(inProgress, false);
  if (CEmbyUtils::HasClients())
    hasInProgress |= CEmbyUtils::GetAllEmbyInProgress(inProgress, false);

  if (hasInProgress)
  {
    CFileItemList temp;
    inProgress.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < inProgress.Size() && i < itemLimit; i++)
    {
      CFileItemPtr item = inProgress.Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(627));
      temp.Add(item);
    }
    
    inProgress.ClearItems();
    inProgress.Append(temp);
  }
}

void CServicesManager::GetSubtitles(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    CPlexUtils::GetItemSubtiles(item);
  else if (item.HasProperty("EmbyItem"))
    CEmbyUtils::GetItemSubtiles(item);
}

void CServicesManager::GetMoreInfo(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    CPlexUtils::GetMoreItemInfo(item);
  else if (item.HasProperty("EmbyItem"))
    CEmbyUtils::GetMoreItemInfo(item);
}

bool CServicesManager::GetResolutions(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    return CPlexUtils::GetMoreResolutions(item);
  else if (item.HasProperty("EmbyItem"))
    return CEmbyUtils::GetMoreResolutions(item);
  return false;
}

bool CServicesManager::GetURL(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    return CPlexUtils::GetURL(item);
  else if (item.HasProperty("EmbyItem"))
    return CEmbyUtils::GetURL(item);
  return false;
}

void CServicesManager::SearchService(CFileItemList &items, std::string strSearchString)
{
  if (CPlexUtils::HasClients())
    CPlexUtils::SearchPlex(items, strSearchString);
  if (CEmbyUtils::HasClients())
    CEmbyUtils::SearchEmby(items, strSearchString);
}

bool CServicesManager::GetAlbumSongs(CFileItem item, CFileItemList &items)
{
  if (item.HasProperty("PlexItem"))
    return CPlexUtils::GetPlexAlbumSongs(item, items);
  else if (item.HasProperty("EmbyItem"))
    return CEmbyUtils::GetEmbyAlbumSongs(item, items);
  return false;
}

bool CServicesManager::GetMediaTotals(MediaServicesMediaCount &totals)
{
  bool rtn = false;
  totals.reset();
  if (HasServices())
  {
    if (CPlexUtils::HasClients())
      rtn |= CPlexUtils::GetPlexMediaTotals(totals);
    if (CEmbyUtils::HasClients())
      rtn |= CEmbyUtils::GetEmbyMediaTotals(totals);
  }
  return rtn;
}

void CServicesManager::RegisterMediaServicesHandler(IMediaServicesHandler *mediaServicesHandler)
{
  if (mediaServicesHandler == nullptr)
    return;

  CExclusiveLock lock(m_mediaServicesCritical);
  if (std::find(m_mediaServicesHandlers.begin(), m_mediaServicesHandlers.end(), mediaServicesHandler) == m_mediaServicesHandlers.end())
    m_mediaServicesHandlers.push_back(mediaServicesHandler);
}

void CServicesManager::UnregisterSettingsHandler(IMediaServicesHandler *mediaServicesHandler)
{
  if (mediaServicesHandler == NULL)
    return;

  CExclusiveLock lock(m_mediaServicesCritical);
  MediaServicesHandlers::iterator it = std::find(m_mediaServicesHandlers.begin(), m_mediaServicesHandlers.end(), mediaServicesHandler);
  if (it != m_mediaServicesHandlers.end())
    m_mediaServicesHandlers.erase(it);
}
