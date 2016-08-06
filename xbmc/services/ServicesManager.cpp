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
#include "interfaces/AnnouncementManager.h"
#include "services/plex/PlexUtils.h"
#include "utils/JobManager.h"
#include "utils/log.h"
#include "utils/StringHasher.h"
#include "video/VideoInfoTag.h"

using namespace ANNOUNCEMENT;

class CServicesManagerJob: public CJob
{
public:
  CServicesManagerJob(CFileItem &item, double currentTime, std::string strFunction)
  :m_item(*new CFileItem(item)),
  m_function(strFunction),
  m_currentTime(currentTime)
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
      case "SetWatched"_mkhash:
        CPlexUtils::SetWatched(m_item);
        break;
      case "SetUnWatched"_mkhash:
        CPlexUtils::SetUnWatched(m_item);
        break;
      case "SetProgress"_mkhash:
        CPlexUtils::ReportProgress(m_item, m_currentTime);
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
  CFileItem      &m_item;
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
    if (g_application.CurrentFileItem().HasProperty("PlexItem"))
    {
      switch(mkhash(message))
      {
        case "OnPlay"_mkhash:
          CPlexUtils::SetPlayState(PlexUtilsPlayerState::playing);
          break;
        case "OnPause"_mkhash:
          CPlexUtils::SetPlayState(PlexUtilsPlayerState::paused);
          break;
        case "OnStop"_mkhash:
        {
          CPlexUtils::SetPlayState(PlexUtilsPlayerState::stopped);

          CFileItem item(g_application.CurrentFileItem());
          AddJob(new CServicesManagerJob(item, item.GetVideoInfoTag()->m_resumePoint.timeInSeconds, "SetProgress"));
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
  return CPlexUtils::HasClients();
}

bool CServicesManager::IsMediaServicesItem(const CFileItem &item)
{
  if (item.HasProperty("MediaServicesItem"))
    return item.GetProperty("MediaServicesItem").asBoolean();
  return false;
}

bool CServicesManager::UpdateMediaServicesLibraries(const CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
  {
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::Other, "plex", "UpdateLibrary");
  }
  return true;
}

void CServicesManager::SetItemWatched(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
  {
    AddJob(new CServicesManagerJob(item, 0, "SetWatched"));
  }
}

void CServicesManager::SetItemUnWatched(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
  {
    AddJob(new CServicesManagerJob(item, 0, "SetUnWatched"));
  }
}

void CServicesManager::UpdateItemState(CFileItem &item, double currentTime)
{
  if (item.HasProperty("PlexItem"))
  {
    AddJob(new CServicesManagerJob(item, currentTime, "SetProgress"));
  }
}

void CServicesManager::GetAllRecentlyAddedMovies(CFileItemList &recentlyAdded, int itemLimit)
{
  if (CPlexUtils::GetAllPlexRecentlyAddedMoviesAndShows(recentlyAdded, false))
  {
    CFileItemList temp;
    recentlyAdded.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < recentlyAdded.Size() && i < itemLimit; i++)
      temp.Add(recentlyAdded.Get(i));

    recentlyAdded.ClearItems();
    recentlyAdded.Append(temp);
  }
}

void CServicesManager::GetAllRecentlyAddedShows(CFileItemList &recentlyAdded, int itemLimit)
{
  if (CPlexUtils::GetAllPlexRecentlyAddedMoviesAndShows(recentlyAdded, true))
  {
    CFileItemList temp;
    recentlyAdded.Sort(SortByDateAdded, SortOrderDescending);
    for (int i = 0; i < recentlyAdded.Size() && i < itemLimit; i++)
      temp.Add(recentlyAdded.Get(i));

    recentlyAdded.ClearItems();
    recentlyAdded.Append(temp);
  }
}

void CServicesManager::GetSubtitles(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    CPlexUtils::GetItemSubtiles(item);
}

void CServicesManager::GetMoreInfo(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    CPlexUtils::GetMoreItemInfo(item);
}

bool CServicesManager::GetResolutions(CFileItem &item)
{
  if (item.HasProperty("PlexItem"))
    return CPlexUtils::GetMoreResolutions(item);
  return false;
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
