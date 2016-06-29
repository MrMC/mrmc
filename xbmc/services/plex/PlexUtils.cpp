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

#include "PlexUtils.h"
#include "PlexServices.h"
#include "Application.h"
#include "Util.h"
#include "URL.h"
#include "network/Network.h"
#include "utils/Base64.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "settings/Settings.h"
#include "cores/dvdplayer/DVDPlayer.h"
#include "guilib/LocalizeStrings.h"

#include "video/VideoInfoTag.h"

static int  g_progressSec = 0;
static PlexUtilsPlayerState g_playbackState = PlexUtilsPlayerState::stopped;

bool CPlexUtils::HasClients()
{
  return CPlexServices::GetInstance().HasClients();
}

bool CPlexUtils::GetIdentity(std::string url)
{
  // all (local and remote) plex server respond to identity
  // over http
  XFILE::CCurlFile plex;
  plex.SetTimeout(1);

  CURL curl(url);
  curl.SetFileName(curl.GetFileName() + "identity");
  curl.SetProtocol("http");
  std::string strResponse;
  if (plex.Get(curl.Get(), strResponse))
  {
    //CLog::Log(LOGDEBUG, "CPlexClient::GetIdentity() %s", strResponse.c_str());
    return true;
  }

  return false;
}

void CPlexUtils::GetDefaultHeaders(XFILE::CCurlFile &curl)
{
  curl.SetRequestHeader("Content-Type", "application/xml; charset=utf-8");
  curl.SetRequestHeader("Content-Length", "0");

  curl.SetUserAgent(CSysInfo::GetUserAgent());
  curl.SetRequestHeader("X-Plex-Client-Identifier", CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID));
  curl.SetRequestHeader("X-Plex-Product", "MrMC");
  curl.SetRequestHeader("X-Plex-Version", CSysInfo::GetVersionShort());
  std::string hostname;
  g_application.getNetwork().GetHostName(hostname);
  StringUtils::TrimRight(hostname, ".local");
  curl.SetRequestHeader("X-Plex-Model", CSysInfo::GetModelName());
  curl.SetRequestHeader("X-Plex-Device", CSysInfo::GetModelName());
  curl.SetRequestHeader("X-Plex-Device-Name", hostname);
  curl.SetRequestHeader("X-Plex-Platform", CSysInfo::GetOsName());
  curl.SetRequestHeader("X-Plex-Platform-Version", CSysInfo::GetOsVersion());
}

void CPlexUtils::SetPlexItemProperties(CFileItem &item, const std::string uuid)
{
  item.SetProperty("PlexItem", true);
  item.SetProperty("MediaServicesItem", true);
  item.SetProperty("MediaServicesClientID", uuid);
}

TiXmlDocument CPlexUtils::GetPlexXML(std::string url, std::string filter)
{
  std::string strXML;
  XFILE::CCurlFile http;
  http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");

  CURL url2(url);
  // this is key to get back gzip encoded content
  url2.SetProtocolOption("seekable", "0");
  if (!filter.empty())
    url2.SetFileName(url2.GetFileName() + filter);

  http.Get(url2.Get(), strXML);

  if (http.GetContentEncoding() == "gzip")
  {
    std::string buffer;
    if (XFILE::CZipFile::DecompressGzip(strXML, buffer))
      strXML = std::move(buffer);
    else
    {
      TiXmlDocument xml;
      return xml;
    }
  }

  //CLog::Log(LOGDEBUG, "CPlexUtils::GetPlexXML %s\n %s", url2.Get().c_str(), strXML.c_str());
  TiXmlDocument xml;
  xml.Parse(strXML.c_str());

  return xml;
}

void CPlexUtils::ReportToServer(std::string url, std::string filename)
{
  CURL url2(url);
  url2.SetFileName(filename.c_str());

  std::string strXML;
  XFILE::CCurlFile plex;
  CPlexUtils::GetDefaultHeaders(plex);
  plex.Get(url2.Get(), strXML);
}

void CPlexUtils::GetVideoDetails(CFileItem &item, const TiXmlElement* videoNode)
{
  // looks like plex is sending only one studio?
  std::vector<std::string> studios;
  studios.push_back(XMLUtils::GetAttribute(videoNode, "studio"));
  item.GetVideoInfoTag()->m_studio = studios;

  // get all genres
  std::vector<std::string> genres;
  const TiXmlElement* genreNode = videoNode->FirstChildElement("Genre");
  if (genreNode)
  {
    while (genreNode)
    {
      std::string genre = XMLUtils::GetAttribute(genreNode, "tag");
      genres.push_back(genre);
      genreNode = genreNode->NextSiblingElement("Genre");
    }
  }
  item.GetVideoInfoTag()->SetGenre(genres);

  // get all writers
  std::vector<std::string> writers;
  const TiXmlElement* writerNode = videoNode->FirstChildElement("Writer");
  if (writerNode)
  {
    while (writerNode)
    {
      std::string writer = XMLUtils::GetAttribute(writerNode, "tag");
      writers.push_back(writer);
      writerNode = writerNode->NextSiblingElement("Writer");
    }
  }
  item.GetVideoInfoTag()->SetWritingCredits(writers);

  // get all directors
  std::vector<std::string> directors;
  const TiXmlElement* directorNode = videoNode->FirstChildElement("Director");
  if (directorNode)
  {
    while (directorNode)
    {
      std::string director = XMLUtils::GetAttribute(directorNode, "tag");
      directors.push_back(director);
      directorNode = directorNode->NextSiblingElement("Director");
    }
  }
  item.GetVideoInfoTag()->SetDirector(directors);

  // get all countries
  std::vector<std::string> countries;
  const TiXmlElement* countryNode = videoNode->FirstChildElement("Country");
  if (countryNode)
  {
    while (countryNode)
    {
      std::string country = XMLUtils::GetAttribute(countryNode, "tag");
      countries.push_back(country);
      countryNode = countryNode->NextSiblingElement("Country");
    }
  }
  item.GetVideoInfoTag()->SetCountry(countries);

  // get all roles
  std::vector< SActorInfo > roles;
  const TiXmlElement* roleNode = videoNode->FirstChildElement("Role");
  if (roleNode)
  {
    while (roleNode)
    {
      SActorInfo role;
      role.strName = XMLUtils::GetAttribute(roleNode, "tag");
      roles.push_back(role);
      roleNode = roleNode->NextSiblingElement("Role");
    }
  }
  item.GetVideoInfoTag()->m_cast = roles;
}

void CPlexUtils::SetWatched(CFileItem &item)
{
  std::string id = item.GetVideoInfoTag()->m_strServiceId;
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "plex://"))
      url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

  std::string filename = StringUtils::Format(":/scrobble?identifier=com.plexapp.plugins.library&key=%s", id.c_str());
  ReportToServer(url, filename);
}

void CPlexUtils::SetUnWatched(CFileItem &item)
{
  std::string id = item.GetVideoInfoTag()->m_strServiceId;
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "plex://"))
    url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

  std::string filename = StringUtils::Format(":/unscrobble?identifier=com.plexapp.plugins.library&key=%s", id.c_str());
  ReportToServer(url, filename);
}

void CPlexUtils::ReportProgress(CFileItem &item, double currentSeconds)
{
  // we get called from Application.cpp every 500ms
  if ((g_playbackState == PlexUtilsPlayerState::stopped || g_progressSec == 0 || g_progressSec > 120))
  {
    g_progressSec = 0;

    std::string status;
    if (g_playbackState == PlexUtilsPlayerState::playing )
      status = "playing";
    else if (g_playbackState == PlexUtilsPlayerState::paused )
      status = "paused";
    else if (g_playbackState == PlexUtilsPlayerState::stopped)
      status = "stopped";

    if (!status.empty())
    {
      std::string url   = URIUtils::GetParentPath(item.GetPath());
      if (StringUtils::StartsWithNoCase(url, "plex://"))
        url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

      std::string id    = item.GetVideoInfoTag()->m_strServiceId;
      int totalSeconds  = item.GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds;

      std::string filename = StringUtils::Format(":/timeline?ratingKey=%s&",id.c_str());
      filename = filename + "key=%2Flibrary%2Fmetadata%2F" +
        StringUtils::Format("%s&state=%s&time=%i&duration=%i", id.c_str(), status.c_str(),
          (int)currentSeconds * 1000, totalSeconds * 1000);

      ReportToServer(url, filename);
      //CLog::Log(LOGDEBUG, "CPlexUtils::ReportProgress %s", filename.c_str());
    }
  }
  g_progressSec++;
}

void CPlexUtils::SetPlayState(PlexUtilsPlayerState state)
{
  g_progressSec = 0;
  g_playbackState = state;
}

bool CPlexUtils::GetVideoItems(CFileItemList &items, CURL url, TiXmlElement* rootXmlNode, std::string type, int season /* = -1 */)
{
  bool rtn = false;
  const TiXmlElement* videoNode = rootXmlNode->FirstChildElement("Video");
  while (videoNode)
  {
    rtn = true;
    CFileItemPtr plexItem(new CFileItem());
    plexItem->SetProperty("PlexItem", true);
    plexItem->SetProperty("MediaServicesItem", true);

    std::string fanart;
    std::string value;
    // if we have season means we are listing episodes, we need to get the fanart from rootXmlNode.
    // movies has it in videoNode
    if (season > -1)
    {
      value = XMLUtils::GetAttribute(videoNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      plexItem->SetArt("thumb", url.Get());
      plexItem->SetIconImage(url.Get());
      fanart = XMLUtils::GetAttribute(rootXmlNode, "art");
      plexItem->GetVideoInfoTag()->m_strShowTitle = XMLUtils::GetAttribute(rootXmlNode, "grandparentTitle");
      plexItem->GetVideoInfoTag()->m_iSeason = season;
      plexItem->GetVideoInfoTag()->m_iEpisode = atoi(XMLUtils::GetAttribute(videoNode, "index").c_str());
    }
    else if (((TiXmlElement*) videoNode)->Attribute("grandparentTitle")) // only recently added episodes have this
    {
      fanart = XMLUtils::GetAttribute(videoNode, "art");
      plexItem->GetVideoInfoTag()->m_strShowTitle = XMLUtils::GetAttribute(videoNode, "grandparentTitle");
      plexItem->GetVideoInfoTag()->m_iSeason = atoi(XMLUtils::GetAttribute(videoNode, "parentIndex").c_str());
      plexItem->GetVideoInfoTag()->m_iEpisode = atoi(XMLUtils::GetAttribute(videoNode, "index").c_str());

      value = XMLUtils::GetAttribute(videoNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      plexItem->SetArt("thumb", url.Get());

      value = XMLUtils::GetAttribute(videoNode, "parentThumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      plexItem->SetArt("tvshow.poster", url.Get());
      plexItem->SetArt("tvshow.thumb", url.Get());
      plexItem->SetIconImage(url.Get());
      std::string seasonEpisode = StringUtils::Format("S%02iE%02i", plexItem->GetVideoInfoTag()->m_iSeason, plexItem->GetVideoInfoTag()->m_iEpisode);
      plexItem->SetProperty("SeasonEpisode", seasonEpisode);
    }
    else
    {
      fanart = XMLUtils::GetAttribute(videoNode, "art");
      plexItem->SetLabel(XMLUtils::GetAttribute(videoNode, "title"));

      value = XMLUtils::GetAttribute(videoNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      plexItem->SetArt("thumb", url.Get());
      plexItem->SetIconImage(url.Get());
    }

    std::string title = XMLUtils::GetAttribute(videoNode, "title");
    plexItem->SetLabel(title);
    plexItem->GetVideoInfoTag()->m_strTitle = title;
    plexItem->GetVideoInfoTag()->m_strServiceId = XMLUtils::GetAttribute(videoNode, "ratingKey");
    plexItem->GetVideoInfoTag()->m_type = type;
    plexItem->GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(videoNode, "tagline"));
    plexItem->GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(videoNode, "summary"));

    CDateTime firstAired;
    firstAired.SetFromDBDate(XMLUtils::GetAttribute(videoNode, "originallyAvailableAt"));
    plexItem->GetVideoInfoTag()->m_firstAired = firstAired;
    
    time_t addedTime = atoi(XMLUtils::GetAttribute(videoNode, "addedAt").c_str());
    CDateTime aTime(addedTime);
    plexItem->GetVideoInfoTag()->m_dateAdded = aTime;

    if (!fanart.empty() && (fanart[0] == '/'))
      StringUtils::TrimLeft(fanart, "/");
    url.SetFileName(fanart);
    plexItem->SetArt("fanart", url.Get());

    plexItem->GetVideoInfoTag()->m_iYear = atoi(XMLUtils::GetAttribute(videoNode, "year").c_str());
    plexItem->GetVideoInfoTag()->m_fRating = atof(XMLUtils::GetAttribute(videoNode, "rating").c_str());
    plexItem->GetVideoInfoTag()->m_strMPAARating = XMLUtils::GetAttribute(videoNode, "contentRating");

    // lastViewedAt means that it was watched, if so we set m_playCount to 1 and set overlay
    if (((TiXmlElement*) videoNode)->Attribute("lastViewedAt"))
    {
      plexItem->GetVideoInfoTag()->m_playCount = 1;
    }
    plexItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, plexItem->HasVideoInfoTag() && plexItem->GetVideoInfoTag()->m_playCount > 0);

    GetVideoDetails(*plexItem, videoNode);

    const TiXmlElement* mediaNode = videoNode->FirstChildElement("Media");
    if (mediaNode)
    {
      CStreamDetails details;
      CStreamDetailVideo *p = new CStreamDetailVideo();
      p->m_strCodec = XMLUtils::GetAttribute(mediaNode, "videoCodec");
      p->m_fAspect = atof(XMLUtils::GetAttribute(mediaNode, "aspectRatio").c_str());
      p->m_iWidth = atoi(XMLUtils::GetAttribute(mediaNode, "width").c_str());
      p->m_iHeight = atoi(XMLUtils::GetAttribute(mediaNode, "height").c_str());
      p->m_iDuration = atoi(XMLUtils::GetAttribute(mediaNode, "videoCodec").c_str());
      details.AddStream(p);

      CStreamDetailAudio *a = new CStreamDetailAudio();
      a->m_strCodec = XMLUtils::GetAttribute(mediaNode, "audioCodec");
      a->m_iChannels = atoi(XMLUtils::GetAttribute(mediaNode, "audioChannels").c_str());
      a->m_strLanguage = XMLUtils::GetAttribute(mediaNode, "audioChannels");
      details.AddStream(a);

      plexItem->GetVideoInfoTag()->m_streamDetails = details;

      /// plex has duration in milliseconds
      plexItem->GetVideoInfoTag()->m_duration = atoi(XMLUtils::GetAttribute(mediaNode, "duration").c_str())/1000;

      CBookmark m_bookmark;
      m_bookmark.timeInSeconds = atoi(XMLUtils::GetAttribute(videoNode, "viewOffset").c_str())/1000;
      m_bookmark.totalTimeInSeconds = atoi(XMLUtils::GetAttribute(mediaNode, "duration").c_str())/1000;
      plexItem->GetVideoInfoTag()->m_resumePoint = m_bookmark;
      plexItem->m_lStartOffset = atoi(XMLUtils::GetAttribute(videoNode, "viewOffset").c_str())/1000;

      int part = 1;
      std::string filePath;
      const TiXmlElement* partNode = mediaNode->FirstChildElement("Part");
      while(partNode)
      {
        if (part == 2)
          filePath = "stack://" + filePath;
        std::string key = ((TiXmlElement*) partNode)->Attribute("key");
        if (!key.empty() && (key[0] == '/'))
          StringUtils::TrimLeft(key, "/");
        url.SetFileName(key);
        plexItem->GetVideoInfoTag()->m_strServiceFile = XMLUtils::GetAttribute(partNode, "file");
        std::string propertyKey = StringUtils::Format("stack:%i_time", part);
        plexItem->SetProperty(propertyKey, atoi(XMLUtils::GetAttribute(partNode, "duration").c_str())/1000 );
        if(part > 1)
          filePath = filePath + " , " + url.Get();
        else
          filePath = url.Get();
        part ++;
        partNode = partNode->NextSiblingElement("Part");
      }
      plexItem->SetPath(filePath);
      plexItem->GetVideoInfoTag()->m_strFileNameAndPath = filePath;
    }

    videoNode = videoNode->NextSiblingElement("Video");
    items.Add(plexItem);
  }
  // this is needed to display movies/episodes properly ... dont ask
  // good thing it didnt take 2 days to figure it out
  items.SetProperty("library.filter", "true");
  items.SetProperty("MediaServicesItem", true);
  items.SetProperty("PlexItem", true);

  return rtn;
}

bool CPlexUtils::GetPlexMovies(CFileItemList &items, std::string url, std::string filter)
{
  bool rtn = false;
  CURL url2(url);
  TiXmlDocument xml = GetPlexXML(url);

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeMovie);
  }

  return rtn;
}

bool CPlexUtils::GetPlexTvshows(CFileItemList &items, std::string url)
{
  bool rtn = false;
  std::string value;
  TiXmlDocument xml = GetPlexXML(url);

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* directoryNode = rootXmlNode->FirstChildElement("Directory");
    while (directoryNode)
    {
      rtn = true;
      CFileItemPtr plexItem(new CFileItem());
      // set m_bIsFolder to true to indicate we are tvshow list
      plexItem->m_bIsFolder = true;
      plexItem->SetProperty("PlexItem", true);
      plexItem->SetProperty("MediaServicesItem", true);
      plexItem->SetLabel(XMLUtils::GetAttribute(directoryNode, "title"));
      CURL url1(url);
      url1.SetFileName("library/metadata/" + XMLUtils::GetAttribute(directoryNode, "ratingKey") + "/children");
      plexItem->SetPath("plex://tvshows/shows/" + Base64::Encode(url1.Get()));
      plexItem->GetVideoInfoTag()->m_strServiceId = XMLUtils::GetAttribute(directoryNode, "ratingKey");
      plexItem->GetVideoInfoTag()->m_type = MediaTypeTvShow;
      plexItem->GetVideoInfoTag()->m_strTitle = XMLUtils::GetAttribute(directoryNode, "title");
      plexItem->GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(directoryNode, "tagline"));
      plexItem->GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(directoryNode, "summary"));
      value = XMLUtils::GetAttribute(directoryNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("thumb", url1.Get());

      value = XMLUtils::GetAttribute(directoryNode, "art");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("fanart", url1.Get());

      plexItem->GetVideoInfoTag()->m_iYear = atoi(XMLUtils::GetAttribute(directoryNode, "year").c_str());
      plexItem->GetVideoInfoTag()->m_fRating = atof(XMLUtils::GetAttribute(directoryNode, "rating").c_str());
      plexItem->GetVideoInfoTag()->m_strMPAARating = XMLUtils::GetAttribute(directoryNode, "contentRating");

      time_t addedTime = atoi(XMLUtils::GetAttribute(directoryNode, "addedAt").c_str());
      CDateTime aTime(addedTime);
      int watchedEpisodes = atoi(XMLUtils::GetAttribute(directoryNode, "viewedLeafCount").c_str());
      int iSeasons        = atoi(XMLUtils::GetAttribute(directoryNode, "childCount").c_str());
      plexItem->GetVideoInfoTag()->m_dateAdded = aTime;
      plexItem->GetVideoInfoTag()->m_iSeason = iSeasons;
      plexItem->GetVideoInfoTag()->m_iEpisode = atoi(XMLUtils::GetAttribute(directoryNode, "leafCount").c_str());
      plexItem->GetVideoInfoTag()->m_playCount = (int)watchedEpisodes >= plexItem->GetVideoInfoTag()->m_iEpisode;

      plexItem->SetProperty("totalseasons", iSeasons);
      plexItem->SetProperty("totalepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
      plexItem->SetProperty("numepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
      plexItem->SetProperty("watchedepisodes", watchedEpisodes);
      plexItem->SetProperty("unwatchedepisodes", plexItem->GetVideoInfoTag()->m_iEpisode - watchedEpisodes);

      plexItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, watchedEpisodes >= plexItem->GetVideoInfoTag()->m_iEpisode);

      CDateTime firstAired;
      firstAired.SetFromDBDate(XMLUtils::GetAttribute(directoryNode, "originallyAvailableAt"));
      plexItem->GetVideoInfoTag()->m_firstAired = firstAired;

      GetVideoDetails(*plexItem, directoryNode);

      items.Add(plexItem);
      directoryNode = directoryNode->NextSiblingElement("Directory");
    }
  }
  items.SetProperty("library.filter", "true");
  items.GetVideoInfoTag()->m_type = MediaTypeTvShow;

  return rtn;
}

bool CPlexUtils::GetPlexSeasons(CFileItemList &items, const std::string url)
{
  bool rtn = false;
  TiXmlDocument xml = GetPlexXML(url);
  std::string value;

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* directoryNode = rootXmlNode->FirstChildElement("Directory");
    while (directoryNode)
    {
      // only get the seasons listing, the one with "ratingKey"
      if (((TiXmlElement*) directoryNode)->Attribute("ratingKey"))
      {
        rtn = true;
        CFileItemPtr plexItem(new CFileItem());
        plexItem->m_bIsFolder = true;
        plexItem->SetProperty("PlexItem", true);
        plexItem->SetProperty("MediaServicesItem", true);
        plexItem->SetLabel(XMLUtils::GetAttribute(directoryNode, "title"));
        CURL url1(url);
        url1.SetFileName("library/metadata/" + XMLUtils::GetAttribute(directoryNode, "ratingKey") + "/children");
        plexItem->SetPath("plex://tvshows/seasons/" + Base64::Encode(url1.Get()));
        plexItem->GetVideoInfoTag()->m_strServiceId = XMLUtils::GetAttribute(directoryNode, "ratingKey");
        plexItem->GetVideoInfoTag()->m_type = MediaTypeTvShow;
        plexItem->GetVideoInfoTag()->m_strTitle = XMLUtils::GetAttribute(directoryNode, "title");
        // we get these from rootXmlNode, where all show info is
        plexItem->GetVideoInfoTag()->m_strShowTitle = XMLUtils::GetAttribute(rootXmlNode, "parentTitle");
        plexItem->GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(rootXmlNode, "tagline"));
        plexItem->GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(rootXmlNode, "summary"));
        plexItem->GetVideoInfoTag()->m_iYear = atoi(XMLUtils::GetAttribute(rootXmlNode, "parentYear").c_str());
        value = XMLUtils::GetAttribute(rootXmlNode, "art");
        if (!value.empty() && (value[0] == '/'))
          StringUtils::TrimLeft(value, "/");
        url1.SetFileName(value);
        plexItem->SetArt("fanart", url1.Get());
        /// -------
        value = XMLUtils::GetAttribute(directoryNode, "thumb");
        if (!value.empty() && (value[0] == '/'))
          StringUtils::TrimLeft(value, "/");
        url1.SetFileName(value);
        plexItem->SetArt("thumb", url1.Get());
        int watchedEpisodes = atoi(XMLUtils::GetAttribute(directoryNode, "viewedLeafCount").c_str());
        int iSeason = atoi(XMLUtils::GetAttribute(directoryNode, "index").c_str());
        plexItem->GetVideoInfoTag()->m_iSeason = iSeason;
        plexItem->GetVideoInfoTag()->m_iEpisode = atoi(XMLUtils::GetAttribute(directoryNode, "leafCount").c_str());
        plexItem->GetVideoInfoTag()->m_playCount = (int)watchedEpisodes >= plexItem->GetVideoInfoTag()->m_iEpisode;

//        plexItem->SetProperty("totalseasons", iSeason);
        plexItem->SetProperty("totalepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
        plexItem->SetProperty("numepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
        plexItem->SetProperty("watchedepisodes", watchedEpisodes);
        plexItem->SetProperty("unwatchedepisodes", plexItem->GetVideoInfoTag()->m_iEpisode - watchedEpisodes);

        plexItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, watchedEpisodes >= plexItem->GetVideoInfoTag()->m_iEpisode);

        items.Add(plexItem);
      }
      directoryNode = directoryNode->NextSiblingElement("Directory");
    }
  }

  items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
  items.SetProperty("library.filter", "true");
  items.SetProperty("PlexItem", true);
  items.SetProperty("MediaServicesItem", true);

  return rtn;
}

bool CPlexUtils::GetPlexEpisodes(CFileItemList &items, const std::string url)
{
  bool rtn = false;

  CURL url2(url);
  TiXmlDocument xml = GetPlexXML(url);

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    int season = atoi(XMLUtils::GetAttribute(rootXmlNode, "parentIndex").c_str());
    rtn = GetVideoItems(items,url2,rootXmlNode, MediaTypeEpisode, season);
    items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
  }

  return rtn;
}

bool CPlexUtils::GetPlexRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit)
{
  bool rtn = false;

  CURL url2(url);
  std::string strXML;
  XFILE::CCurlFile http;
  http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");

  url2.SetFileName(url2.GetFileName() + "recentlyAdded");
  url2.SetProtocolOptions(url2.GetProtocolOptions() + StringUtils::Format("&X-Plex-Container-Start=0&X-Plex-Container-Size=%i", limit));
  // this is key to get back gzip encoded content
  url2.SetProtocolOption("seekable", "0");

  http.Get(url2.Get(), strXML);
  if (http.GetContentEncoding() == "gzip")
  {
    std::string buffer;
    if (XFILE::CZipFile::DecompressGzip(strXML, buffer))
      strXML = std::move(buffer);
    else
      return false;
  }
  // remove the seakable option as we propigate the url
  url2.RemoveProtocolOption("seekable");

  TiXmlDocument xml;
  xml.Parse(strXML.c_str());

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeEpisode);
    items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
    items.Sort(SortByDateAdded, SortOrderDescending);
  }

  return rtn;
}

bool CPlexUtils::GetPlexRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit)
{
  bool rtn = false;

  CURL url2(url);
    url2.SetFileName(url2.GetFileName() + "recentlyAdded");
  url2.SetProtocolOptions(url2.GetProtocolOptions() + StringUtils::Format("&X-Plex-Container-Start=0&X-Plex-Container-Size=%i", limit));
  // this is key to get back gzip encoded content
  url2.SetProtocolOption("seekable", "0");

  std::string strXML;
  XFILE::CCurlFile http;
  http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");

  http.Get(url2.Get(), strXML);
  if (http.GetContentEncoding() == "gzip")
  {
    std::string buffer;
    if (XFILE::CZipFile::DecompressGzip(strXML, buffer))
      strXML = std::move(buffer);
    else
      return false;
  }
  // remove the seakable option as we propigate the url
  url2.RemoveProtocolOption("seekable");

  TiXmlDocument xml;
  xml.Parse(strXML.c_str());

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    rtn = GetVideoItems(items, url2,rootXmlNode, MediaTypeMovie);
    items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
    items.Sort(SortByDateAdded, SortOrderDescending);
  }

  return rtn;
}

bool CPlexUtils::GetAllPlexRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow)
{
  bool rtn = false;

  if (CPlexServices::GetInstance().HasClients())
  {
    //look through all plex clients and pull recently added for each library section
    std::vector<CPlexClientPtr> clients;
    CPlexServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      std::vector<PlexSectionsContent> contents;
      if (tvShow)
        contents = client->GetTvContent();
      else
        contents = client->GetMovieContent();
      for (const auto &content : contents)
      {
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetScheme());
        curl.SetFileName(curl.GetFileName() + content.section + "/");

        if (tvShow)
          rtn = GetPlexRecentlyAddedEpisodes(items, curl.Get(), 10);
        else
          rtn = GetPlexRecentlyAddedMovies(items, curl.Get(), 10);

        for (int item = 0; item < items.Size(); ++item)
          CPlexUtils::SetPlexItemProperties(*items[item], client->GetUuid());
      }
    }
    items.SetProperty("PlexItem", true);
    items.SetProperty("MediaServicesItem", true);
  }

  return rtn;
}

bool CPlexUtils::GetPlexFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter)
{
  bool rtn = false;

  TiXmlDocument xml = GetPlexXML(url,filter);

  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* directoryNode = rootXmlNode->FirstChildElement("Directory");
    while (directoryNode)
    {
      rtn = true;
      std::string title = XMLUtils::GetAttribute(directoryNode, "title");
      std::string key = XMLUtils::GetAttribute(directoryNode, "key");
      CFileItemPtr pItem(new CFileItem(title));
      pItem->m_bIsFolder = true;
      pItem->m_bIsShareOrDrive = false;
      pItem->SetProperty("PlexItem", true);
      pItem->SetProperty("MediaServicesItem", true);

      CURL plex(url);
      plex.SetFileName(plex.GetFileName() + "all?" + filter + "=" + key);
      pItem->SetPath(parentPath + Base64::Encode(plex.Get()));
      pItem->SetLabel(title);
      pItem->SetProperty("SkipLocalArt", true);
      items.Add(pItem);
      directoryNode = directoryNode->NextSiblingElement("Directory");
    }
  }

  return rtn;
}

bool CPlexUtils::GetItemSubtiles(CFileItem &item)
{
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "plex://"))
    url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));
  
  std::string id = item.GetVideoInfoTag()->m_strServiceId;
  std::string filename = StringUtils::Format("library/metadata/%s", id.c_str());
  
  CURL url2(url);
  std::string strXML;
  XFILE::CCurlFile http;
  http.SetRequestHeader("Accept-Encoding", "gzip");
  
  url2.SetFileName(filename);
  // this is key to get back gzip encoded content
  url2.SetProtocolOption("seekable", "0");
  
  http.Get(url2.Get(), strXML);
  if (http.GetContentEncoding() == "gzip")
  {
    std::string buffer;
    if (XFILE::CZipFile::DecompressGzip(strXML, buffer))
      strXML = std::move(buffer);
    else
      return false;
  }
  // remove the seakable option as we propigate the url
  url2.RemoveProtocolOption("seekable");
  
  TiXmlDocument xml;
  xml.Parse(strXML.c_str());
  
  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* videoNode = rootXmlNode->FirstChildElement("Video");
    if(videoNode)
    {
      const TiXmlElement* mediaNode = videoNode->FirstChildElement("Media");
      if (mediaNode)
      {
        const TiXmlElement* partNode = mediaNode->FirstChildElement("Part");
        if (partNode)
        {
          std::string subFile;
          const TiXmlElement* streamNode = partNode->FirstChildElement("Stream");
          while (streamNode)
          {
            if (XMLUtils::GetAttribute(streamNode, "streamType") == "3")
            {
              CURL plex(url);
              std::string filename = StringUtils::Format("library/streams/%s.%s",
                XMLUtils::GetAttribute(streamNode, "id").c_str(), XMLUtils::GetAttribute(streamNode, "format").c_str());
              plex.SetFileName(filename);
              SPlayerSubtitleStreamInfo s;
              s.file = plex.Get();
              s.name = g_localizeStrings.Get(21602);
              s.language = XMLUtils::GetAttribute(streamNode, "languageCode");
              if (g_application.m_pPlayer)
              {
                g_application.m_pPlayer->AddSubtitle(s);
              }
            }
            streamNode = streamNode->NextSiblingElement("Stream");
          }
        }
      }
    }
  }
  return true;
}
