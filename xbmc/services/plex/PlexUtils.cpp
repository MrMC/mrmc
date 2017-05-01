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
#include "ContextMenuManager.h"
#include "Util.h"
#include "URL.h"
#include "filesystem/StackDirectory.h"
#include "network/Network.h"
#include "utils/Base64URL.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "settings/Settings.h"

#include "video/VideoInfoTag.h"
#include "video/windows/GUIWindowVideoBase.h"

#include "music/tags/MusicInfoTag.h"
#include "music/dialogs/GUIDialogSongInfo.h"
#include "music/dialogs/GUIDialogMusicInfo.h"
#include "guilib/GUIWindowManager.h"

static int  g_progressSec = 0;
static CFileItem m_curItem;
static MediaServicesPlayerState g_playbackState = MediaServicesPlayerState::stopped;

bool CPlexUtils::HasClients()
{
  return CPlexServices::GetInstance().HasClients();
}

bool CPlexUtils::GetIdentity(CURL url, int timeout)
{
  // all (local and remote) plex server respond to identity
  XFILE::CCurlFile plex;
  plex.SetTimeout(timeout);
  plex.SetSilent(true);

  url.SetFileName(url.GetFileName() + "identity");
  std::string strResponse;
  if (plex.Get(url.Get(), strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CPlexClient::GetIdentity() %s", strResponse.c_str());
#endif
    return true;
  }

  return false;
}

void CPlexUtils::GetDefaultHeaders(XFILE::CCurlFile &curl)
{
  curl.SetRequestHeader("Content-Type", "application/xml; charset=utf-8");
  curl.SetRequestHeader("Content-Length", "0");
  curl.SetRequestHeader("Connection", "Keep-Alive");
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
  curl.SetRequestHeader("Cache-Control", "no-cache");
  curl.SetRequestHeader("Pragma", "no-cache");
  curl.SetRequestHeader("Expires", "Sat, 26 Jul 1997 05:00:00 GMT");
}

void CPlexUtils::SetPlexItemProperties(CFileItem &item)
{
  CPlexClientPtr client = CPlexServices::GetInstance().FindClient(item.GetPath());
  SetPlexItemProperties(item, client);
}

void CPlexUtils::SetPlexItemProperties(CFileItem &item, const CPlexClientPtr &client)
{
  item.SetProperty("PlexItem", true);
  item.SetProperty("MediaServicesItem", true);
  if (!client)
    return;
  if (client->IsCloud())
    item.SetProperty("MediaServicesCloudItem", true);
  item.SetProperty("MediaServicesClientID", client->GetUuid());
}

TiXmlDocument CPlexUtils::GetPlexXML(std::string url, std::string filter)
{
  std::string strXML;
  XFILE::CCurlFile http;
  //http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");
  GetDefaultHeaders(http);

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

int CPlexUtils::ParsePlexMediaXML(TiXmlDocument xml)
{
  int result = 0;
  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    result = atoi(XMLUtils::GetAttribute(rootXmlNode, "totalSize").c_str());
  }
  
  return result;
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
  if(item.GetVideoInfoTag()->m_strPlotOutline.empty())
    item.GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(videoNode, "tagline"));
  if(item.GetVideoInfoTag()->m_strPlot.empty())
    item.GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(videoNode, "summary"));
  
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
      role.strRole = XMLUtils::GetAttribute(roleNode, "role");
      role.thumb = XMLUtils::GetAttribute(roleNode, "thumb");
      roles.push_back(role);
      roleNode = roleNode->NextSiblingElement("Role");
    }
  }
  item.GetVideoInfoTag()->m_cast = roles;
}

void CPlexUtils::GetMusicDetails(CFileItem &item, const TiXmlElement* videoNode)
{
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
  item.GetMusicInfoTag()->SetGenre(genres);
  
  // get all countries
//  std::vector<std::string> countries;
//  const TiXmlElement* countryNode = videoNode->FirstChildElement("Country");
//  if (countryNode)
//  {
//    while (countryNode)
//    {
//      std::string country = XMLUtils::GetAttribute(countryNode, "tag");
//      countries.push_back(country);
//      countryNode = countryNode->NextSiblingElement("Country");
//    }
//  }
//  item.GetMusicInfoTag()->SetCountry(countries);
}

void CPlexUtils::GetMediaDetals(CFileItem &item, CURL url, const TiXmlElement* mediaNode, std::string id)
{
  if (mediaNode && (id == "0" || XMLUtils::GetAttribute(mediaNode, "id") == id))
  {
    CStreamDetails details;
    CStreamDetailVideo *p = new CStreamDetailVideo();
    p->m_strCodec = XMLUtils::GetAttribute(mediaNode, "videoCodec");
    p->m_fAspect = atof(XMLUtils::GetAttribute(mediaNode, "aspectRatio").c_str());
    p->m_iWidth = atoi(XMLUtils::GetAttribute(mediaNode, "width").c_str());
    p->m_iHeight = atoi(XMLUtils::GetAttribute(mediaNode, "height").c_str());
    p->m_iDuration = atoi(XMLUtils::GetAttribute(mediaNode, "duration").c_str())/1000;
    details.AddStream(p);
    
    CStreamDetailAudio *a = new CStreamDetailAudio();
    a->m_strCodec = XMLUtils::GetAttribute(mediaNode, "audioCodec");
    a->m_iChannels = atoi(XMLUtils::GetAttribute(mediaNode, "audioChannels").c_str());
    a->m_strLanguage = XMLUtils::GetAttribute(mediaNode, "audioChannels");
    details.AddStream(a);

    std::string label;
    std::string resolution = XMLUtils::GetAttribute(mediaNode, "videoResolution");
    StringUtils::ToUpper(resolution);
    float bitrate = atof(XMLUtils::GetAttribute(mediaNode, "bitrate").c_str())/1000;
    if(resolution.empty())
      label = StringUtils::Format("%.2f Mbps", bitrate);
    else
    {
      if (bitrate > 0)
        label = StringUtils::Format("%s, %.2f Mbps",resolution.c_str(),bitrate);
      else
        label = resolution;
    }
    item.SetProperty("PlexResolutionChoice", label);
    item.SetProperty("PlexMediaID", XMLUtils::GetAttribute(mediaNode, "id"));
    item.GetVideoInfoTag()->m_streamDetails = details;
    
    /// plex has duration in milliseconds
    item.GetVideoInfoTag()->m_duration = atoi(XMLUtils::GetAttribute(mediaNode, "duration").c_str())/1000;
    
    int part = 1;
    std::string filePath;
    const TiXmlElement* partNode = mediaNode->FirstChildElement("Part");
    while(partNode)
    {
      int iPart = 1;
      std::string subFile;
      const TiXmlElement* streamNode = partNode->FirstChildElement("Stream");
      while (streamNode)
      {
        // "codecID" indicates that subtitle file is internal, we ignore it as our player will pick that up anyway
        bool internalSubtitle = streamNode->Attribute("codecID");
        
        if (!internalSubtitle && XMLUtils::GetAttribute(streamNode, "streamType") == "3")
        {
          CURL plex(url);
          std::string filename;
          std::string id    = XMLUtils::GetAttribute(streamNode, "id");
          std::string ext   = XMLUtils::GetAttribute(streamNode, "format");
          std::string codec = XMLUtils::GetAttribute(streamNode, "codec");
          
          if(ext.empty() && codec.empty())
            filename = StringUtils::Format("library/streams/%s",id.c_str());
          else
            filename = StringUtils::Format("library/streams/%s.%s",id.c_str(), ext.empty()? codec.c_str():ext.c_str());
          plex.SetFileName(filename);
          std::string propertyKey = StringUtils::Format("subtitle:%i", iPart);
          std::string propertyLangKey = StringUtils::Format("subtitle:%i_language", iPart);
          item.SetProperty(propertyKey, plex.Get());
          item.SetProperty(propertyLangKey, XMLUtils::GetAttribute(streamNode, "languageCode"));
          iPart ++;
        }
        streamNode = streamNode->NextSiblingElement("Stream");
      }
      
      if (part == 2)
        filePath = "stack://" + filePath;
      std::string key = ((TiXmlElement*) partNode)->Attribute("key");
      if (!key.empty() && (key[0] == '/'))
        StringUtils::TrimLeft(key, "/");
      url.SetFileName(key);
      item.SetMediaServiceFile(XMLUtils::GetAttribute(partNode, "file"));
      std::string propertyKey = StringUtils::Format("stack:%i_time", part);
      item.SetProperty(propertyKey, atoi(XMLUtils::GetAttribute(partNode, "duration").c_str())/1000 );
      if(part > 1)
        filePath = filePath + " , " + url.Get();
      else
        filePath = url.Get();
      part ++;
      partNode = partNode->NextSiblingElement("Part");
    }
    item.SetPath(filePath);
    item.GetVideoInfoTag()->m_strFileNameAndPath = filePath;
  }

}

void CPlexUtils::SetWatched(CFileItem &item)
{
  // if we are Plex music, do not report
  if (item.IsAudio())
    return;

  std::string id = item.GetMediaServiceId();
  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url   = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "plex://"))
      url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

  std::string filename = StringUtils::Format(":/scrobble?identifier=com.plexapp.plugins.library&key=%s", id.c_str());
  ReportToServer(url, filename);
}

void CPlexUtils::SetUnWatched(CFileItem &item)
{
  // if we are Plex music, do not report
  if (item.IsAudio())
    return;

  std::string id = item.GetMediaServiceId();
  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url   = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "plex://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

  std::string filename = StringUtils::Format(":/unscrobble?identifier=com.plexapp.plugins.library&key=%s", id.c_str());
  ReportToServer(url, filename);
}

void CPlexUtils::ReportProgress(CFileItem &item, double currentSeconds)
{
  // if we are Plex music, do not report
  if (item.IsAudio())
    return;
  
  // we get called from Application.cpp every 500ms
  if ((g_playbackState == MediaServicesPlayerState::stopped || g_progressSec == 0 || g_progressSec > 120))
  {
    g_progressSec = 0;

    std::string status;
    if (g_playbackState == MediaServicesPlayerState::playing )
      status = "playing";
    else if (g_playbackState == MediaServicesPlayerState::paused )
      status = "paused";
    else if (g_playbackState == MediaServicesPlayerState::stopped)
      status = "stopped";

    if (!status.empty())
    {
      std::string url = item.GetPath();
      if (URIUtils::IsStack(url))
        url = XFILE::CStackDirectory::GetFirstStackedFile(url);
      else
      {
        CURL url1(item.GetPath());
        CURL url2(URIUtils::GetParentPath(url));
        CURL url3(url2.GetWithoutFilename());
        url3.SetProtocolOptions(url1.GetProtocolOptions());
        url = url3.Get();
      }
      if (StringUtils::StartsWithNoCase(url, "plex://"))
        url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

      std::string id    = item.GetMediaServiceId();
      int totalSeconds  = item.GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds;

      std::string filename = StringUtils::Format(":/timeline?ratingKey=%s&",id.c_str());
      filename = filename + "key=%2Flibrary%2Fmetadata%2F" +
        StringUtils::Format("%s&state=%s&time=%i&duration=%i", id.c_str(), status.c_str(),
          (int)currentSeconds * 1000, totalSeconds * 1000);

      ReportToServer(url, filename);
      //CLog::Log(LOGDEBUG, "CPlexUtils::ReportProgress %s", filename.c_str());
    }
    if (g_playbackState == MediaServicesPlayerState::stopped &&
        item.GetProperty("PlexTranscoder").asBoolean())
    {
      StopTranscode(item);
    }
    
  }
  g_progressSec++;
}

void CPlexUtils::SetPlayState(MediaServicesPlayerState state)
{
  g_progressSec = 0;
  g_playbackState = state;
}

bool CPlexUtils::GetVideoItems(CFileItemList &items, CURL url, TiXmlElement* rootXmlNode, std::string type, bool formatLabel, int season /* = -1 */)
{
  bool rtn = false;
  const TiXmlElement* videoNode = rootXmlNode->FirstChildElement("Video");
  while (videoNode)
  {
    rtn = true;
    CFileItemPtr plexItem(new CFileItem());

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
      plexItem->SetArt("tvshow.thumb", url.Get());
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
      if (value.empty())
        value = XMLUtils::GetAttribute(videoNode, "grandparentThumb");
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
    plexItem->SetMediaServiceId(XMLUtils::GetAttribute(videoNode, "ratingKey"));
    plexItem->SetProperty("PlexShowKey", XMLUtils::GetAttribute(rootXmlNode, "grandparentRatingKey"));
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

    plexItem->GetVideoInfoTag()->SetYear(atoi(XMLUtils::GetAttribute(videoNode, "year").c_str()));
    plexItem->GetVideoInfoTag()->SetRating(atof(XMLUtils::GetAttribute(videoNode, "rating").c_str()));
    plexItem->GetVideoInfoTag()->m_strMPAARating = XMLUtils::GetAttribute(videoNode, "contentRating");

    // lastViewedAt means that it was watched, if so we set m_playCount to 1 and set overlay.
    // If we have "viewOffset" that means we are partially watched and shoudl not set m_playCount to 1
    if (((TiXmlElement*) videoNode)->Attribute("lastViewedAt") && !((TiXmlElement*) videoNode)->Attribute("viewOffset"))
    {
      plexItem->GetVideoInfoTag()->m_playCount = 1;
    }
    plexItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, plexItem->HasVideoInfoTag() && plexItem->GetVideoInfoTag()->m_playCount > 0);

    GetVideoDetails(*plexItem, videoNode);

    CBookmark m_bookmark;
    m_bookmark.timeInSeconds = atoi(XMLUtils::GetAttribute(videoNode, "viewOffset").c_str())/1000;
    m_bookmark.totalTimeInSeconds = atoi(XMLUtils::GetAttribute(videoNode, "duration").c_str())/1000;
    plexItem->GetVideoInfoTag()->m_resumePoint = m_bookmark;
    plexItem->m_lStartOffset = atoi(XMLUtils::GetAttribute(videoNode, "viewOffset").c_str())/1000;

    const TiXmlElement* mediaNode = videoNode->FirstChildElement("Media");
    GetMediaDetals(*plexItem, url, mediaNode);

    if (formatLabel)
    {
      CLabelFormatter formatter("%H. %T", "");
      formatter.FormatLabel(plexItem.get());
      plexItem->SetLabelPreformated(true);
    }
    SetPlexItemProperties(*plexItem);
    items.Add(plexItem);
    videoNode = videoNode->NextSiblingElement("Video");
  }
  // this is needed to display movies/episodes properly ... dont ask
  // good thing it didnt take 2 days to figure it out
  items.SetProperty("library.filter", "true");
  SetPlexItemProperties(items);

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
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeMovie, false);
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
      plexItem->SetLabel(XMLUtils::GetAttribute(directoryNode, "title"));
      CURL url1(url);
      url1.SetFileName("library/metadata/" + XMLUtils::GetAttribute(directoryNode, "ratingKey") + "/children");
      plexItem->SetPath("plex://tvshows/shows/" + Base64URL::Encode(url1.Get()));
      plexItem->SetMediaServiceId(XMLUtils::GetAttribute(directoryNode, "ratingKey"));
      plexItem->SetProperty("PlexShowKey", XMLUtils::GetAttribute(directoryNode, "ratingKey"));
      plexItem->GetVideoInfoTag()->m_type = MediaTypeTvShow;
      plexItem->GetVideoInfoTag()->m_strTitle = XMLUtils::GetAttribute(directoryNode, "title");
      plexItem->GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(directoryNode, "tagline"));
      plexItem->GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(directoryNode, "summary"));
      value = XMLUtils::GetAttribute(directoryNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("thumb", url1.Get());

      value = XMLUtils::GetAttribute(directoryNode, "banner");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("banner", url1.Get());
      
      value = XMLUtils::GetAttribute(directoryNode, "art");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("fanart", url1.Get());

      plexItem->GetVideoInfoTag()->SetYear(atoi(XMLUtils::GetAttribute(directoryNode, "year").c_str()));
      plexItem->GetVideoInfoTag()->SetRating(atof(XMLUtils::GetAttribute(directoryNode, "rating").c_str()));
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
      SetPlexItemProperties(*plexItem);
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
        plexItem->SetLabel(XMLUtils::GetAttribute(directoryNode, "title"));
        CURL url1(url);
        url1.SetFileName("library/metadata/" + XMLUtils::GetAttribute(directoryNode, "ratingKey") + "/children");
        plexItem->SetPath("plex://tvshows/seasons/" + Base64URL::Encode(url1.Get()));
        plexItem->SetMediaServiceId(XMLUtils::GetAttribute(directoryNode, "ratingKey"));
        plexItem->GetVideoInfoTag()->m_type = MediaTypeSeason;
        plexItem->GetVideoInfoTag()->m_strTitle = XMLUtils::GetAttribute(directoryNode, "title");
        // we get these from rootXmlNode, where all show info is
        plexItem->GetVideoInfoTag()->m_strShowTitle = XMLUtils::GetAttribute(rootXmlNode, "parentTitle");
        plexItem->GetVideoInfoTag()->SetPlotOutline(XMLUtils::GetAttribute(rootXmlNode, "tagline"));
        plexItem->GetVideoInfoTag()->SetPlot(XMLUtils::GetAttribute(rootXmlNode, "summary"));
        plexItem->GetVideoInfoTag()->SetYear(atoi(XMLUtils::GetAttribute(rootXmlNode, "parentYear").c_str()));
        plexItem->SetProperty("PlexShowKey", XMLUtils::GetAttribute(rootXmlNode, "key"));
        value = XMLUtils::GetAttribute(rootXmlNode, "art");
        if (!value.empty() && (value[0] == '/'))
          StringUtils::TrimLeft(value, "/");
        url1.SetFileName(value);
        plexItem->SetArt("fanart", url1.Get());
        
        value = XMLUtils::GetAttribute(rootXmlNode, "banner");
        if (!value.empty() && (value[0] == '/'))
          StringUtils::TrimLeft(value, "/");
        url1.SetFileName(value);
        plexItem->SetArt("banner", url1.Get());
        
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

        plexItem->SetProperty("totalepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
        plexItem->SetProperty("numepisodes", plexItem->GetVideoInfoTag()->m_iEpisode);
        plexItem->SetProperty("watchedepisodes", watchedEpisodes);
        plexItem->SetProperty("unwatchedepisodes", plexItem->GetVideoInfoTag()->m_iEpisode - watchedEpisodes);

        plexItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, watchedEpisodes >= plexItem->GetVideoInfoTag()->m_iEpisode);

        SetPlexItemProperties(*plexItem);
        items.Add(plexItem);
      }
      directoryNode = directoryNode->NextSiblingElement("Directory");
    }
  }

  items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
  items.SetProperty("library.filter", "true");
  SetPlexItemProperties(items);

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
    rtn = GetVideoItems(items,url2,rootXmlNode, MediaTypeEpisode, true, season);
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
  //http.SetBufferSize(32768*10);
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
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeEpisode, false);
    items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
    items.Sort(SortByDateAdded, SortOrderDescending);
  }

  return rtn;
}

bool CPlexUtils::GetPlexInProgressShows(CFileItemList &items, const std::string url, int limit)
{
  bool rtn = false;
  
  CURL url2(url);
  std::string strXML;
  XFILE::CCurlFile http;
  //http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");
  
  url2.SetFileName(url2.GetFileName() + "onDeck");
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
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeEpisode, false);
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
  //http.SetBufferSize(32768*10);
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
    rtn = GetVideoItems(items, url2,rootXmlNode, MediaTypeMovie, false);
    items.SetLabel(XMLUtils::GetAttribute(rootXmlNode, "title2"));
    items.Sort(SortByDateAdded, SortOrderDescending);
  }

  return rtn;
}

bool CPlexUtils::GetPlexInProgressMovies(CFileItemList &items, const std::string url, int limit)
{
  bool rtn = false;
  
  CURL url2(url);
  std::string strXML;
  XFILE::CCurlFile http;
  //http.SetBufferSize(32768*10);
  http.SetRequestHeader("Accept-Encoding", "gzip");
  
  url2.SetFileName(url2.GetFileName() + "onDeck");
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
    rtn = GetVideoItems(items, url2, rootXmlNode, MediaTypeMovie, false);
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
    CFileItemList plexItems;
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all plex clients and pull recently added for each library section
    std::vector<CPlexClientPtr> clients;
    CPlexServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;
      
      std::vector<PlexSectionsContent> contents;
      if (tvShow)
        contents = client->GetTvContent();
      else
        contents = client->GetMovieContent();
      for (const auto &content : contents)
      {
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(curl.GetFileName() + content.section + "/");

        if (tvShow)
          rtn = GetPlexRecentlyAddedEpisodes(plexItems, curl.Get(), 10);
        else
          rtn = GetPlexRecentlyAddedMovies(plexItems, curl.Get(), 10);

        for (int item = 0; item < plexItems.Size(); ++item)
          CPlexUtils::SetPlexItemProperties(*plexItems[item], client);
      }
      SetPlexItemProperties(plexItems);
      items.Append(plexItems);
      plexItems.ClearItems();
    }

  }

  return rtn;
}

bool CPlexUtils::GetAllPlexInProgress(CFileItemList &items, bool tvShow)
{
  if (CPlexServices::GetInstance().HasClients())
  {
    CFileItemList plexItems;
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all plex clients and pull recently added for each library section
    std::vector<CPlexClientPtr> clients;
    CPlexServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;
      
      std::vector<PlexSectionsContent> contents;
      if (tvShow)
        contents = client->GetTvContent();
      else
        contents = client->GetMovieContent();
      for (const auto &content : contents)
      {
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(curl.GetFileName() + content.section + "/");
        
        if (tvShow)
          GetPlexInProgressShows(plexItems, curl.Get(), 10);
        else
          GetPlexInProgressMovies(plexItems, curl.Get(), 10);
        
        for (int item = 0; item < plexItems.Size(); ++item)
          CPlexUtils::SetPlexItemProperties(*plexItems[item], client);
      }
      SetPlexItemProperties(plexItems);
      items.Append(plexItems);
      plexItems.ClearItems();
    }

  }
  
  return items.Size() > 0;
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
      CURL plex(url);
      plex.SetFileName(plex.GetFileName() + "all?" + filter + "=" + key);
      pItem->SetPath(parentPath + Base64URL::Encode(plex.Get()));
      pItem->SetLabel(title);
      pItem->SetProperty("SkipLocalArt", true);
      SetPlexItemProperties(*pItem);
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
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  
  std::string id = item.GetMediaServiceId();
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
      while (mediaNode)
      {
        GetMediaDetals(item, url2, mediaNode, item.GetProperty("PlexMediaID").asString());
        mediaNode = mediaNode->NextSiblingElement("Media");
      }
    }
  }
  return true;
}

bool CPlexUtils::GetMoreItemInfo(CFileItem &item)
{
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "plex://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  
  std::string id = item.GetMediaServiceId();
  std::string childElement = "Video";
  if (item.HasProperty("PlexShowKey") && item.GetVideoInfoTag()->m_type != MediaTypeMovie)
  {
    id = item.GetProperty("PlexShowKey").asString();
    childElement = "Directory";
  }
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
    const TiXmlElement* videoNode = rootXmlNode->FirstChildElement(childElement);
    if(videoNode)
    {
      GetVideoDetails(item, videoNode);
    }
  }
  return true;
}

bool CPlexUtils::GetMoreResolutions(CFileItem &item)
{
  std::string id  = item.GetMediaServiceId();
  std::string url = item.GetVideoInfoTag()->m_strFileNameAndPath;
  
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url   = URIUtils::GetParentPath(url);

  CURL url2(url);
  url2.SetFileName("library/metadata/" + id);
  CContextButtons choices;
  std::vector<CFileItem> resolutionList;
  TiXmlDocument xml = GetPlexXML(url2.Get());
  
  RemoveSubtitleProperties(item);
  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* videoNode = rootXmlNode->FirstChildElement("Video");
    if (videoNode)
    {
      const TiXmlElement* mediaNode = videoNode->FirstChildElement("Media");
      while (mediaNode)
      {
        CFileItem mediaItem(item);
        GetMediaDetals(mediaItem, url2, mediaNode);
        resolutionList.push_back(mediaItem);
        choices.Add(resolutionList.size(), mediaItem.GetProperty("PlexResolutionChoice").c_str());
        mediaNode = mediaNode->NextSiblingElement("Media");
      }
    }
    if (resolutionList.size() < 2)
      return true;
    
    int button = CGUIDialogContextMenu::ShowAndGetChoice(choices);
    if (button > -1)
    {
      m_curItem = resolutionList[button-1];
      item.UpdateInfo(m_curItem, false);
      item.SetPath(m_curItem.GetPath());
      return true;
    }
  }
  return false;
}

void CPlexUtils::RemoveSubtitleProperties(CFileItem &item)
{
  std::string key("subtitle:1");
  for(unsigned s = 1; item.HasProperty(key); key = StringUtils::Format("subtitle:%u", ++s))
  {
    item.ClearProperty(key);
    item.ClearProperty(StringUtils::Format("subtitle:%i_language", s));
  }
}

bool CPlexUtils::SearchPlex(CFileItemList &items, std::string strSearchString)
{
  if (CPlexServices::GetInstance().HasClients())
  {
    StringUtils::Replace(strSearchString, " ","%20");
    CFileItemList plexItems;
    std::vector<CPlexClientPtr> clients;
    CPlexServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      CURL url(client->GetUrl());
      url.SetFileName("hubs/search?sectionId=&query=" + strSearchString);
      TiXmlDocument xml = GetPlexXML(url.Get());
      TiXmlElement* rootXmlNode = xml.RootElement();
      if (rootXmlNode)
      {
        TiXmlElement* hubNode = rootXmlNode->FirstChildElement("Hub");
        while (hubNode)
        {
          if(((TiXmlElement*) hubNode)->Attribute("type"))
          {
            std::string type = XMLUtils::GetAttribute(hubNode, "type");
            if (type == "show")
            {
              CFileItemList plexShow;
              const TiXmlElement* directoryNode = hubNode->FirstChildElement("Directory");
              std::string ratingKey = XMLUtils::GetAttribute(directoryNode, "ratingKey");
              url.SetFileName("library/metadata/"+ ratingKey + "/allLeaves");
              TiXmlDocument xml = GetPlexXML(url.Get());
              TiXmlElement* rootXmlNode = xml.RootElement();
              if(rootXmlNode)
                GetVideoItems(plexShow, url, rootXmlNode, MediaTypeEpisode, false);
              
              for (int i = 0; i < plexShow.Size(); ++i)
              {
                std::string label = plexShow[i]->GetVideoInfoTag()->m_strTitle + " (" +  plexShow[i]->GetVideoInfoTag()->m_strShowTitle + ")";
                plexShow[i]->SetLabel(label);
              }
              CGUIWindowVideoBase::AppendAndClearSearchItems(plexShow, "[" + g_localizeStrings.Get(20359) + "] ", plexItems);
            }
            else if (type == "movie")
            {
              CFileItemList plexMovies;
              GetVideoItems(plexMovies, url, hubNode, MediaTypeMovie, false);
              for (int i = 0; i < plexMovies.Size(); ++i)
              {
                std::string label = plexMovies[i]->GetVideoInfoTag()->m_strTitle;
                if (plexMovies[i]->GetVideoInfoTag()->GetYear() > 0)
                  label += StringUtils::Format(" (%i)", plexMovies[i]->GetVideoInfoTag()->GetYear());
                plexMovies[i]->SetLabel(label);
              }
              CGUIWindowVideoBase::AppendAndClearSearchItems(plexMovies, "[" + g_localizeStrings.Get(20338) + "] ", plexItems);
            }
          }
          hubNode = hubNode->NextSiblingElement("Hub");
        }
      }
      
      for (int item = 0; item < plexItems.Size(); ++item)
        CPlexUtils::SetPlexItemProperties(*plexItems[item], client);
      
      SetPlexItemProperties(plexItems);
      items.Append(plexItems);
      plexItems.ClearItems();
    }
  }
  
  return items.Size() > 0;
}

/// Plex Music Below

bool CPlexUtils::GetPlexArtistsOrAlbum(CFileItemList &items, std::string url, bool album)
{
  bool rtn = false;
  std::string value;
  TiXmlDocument xml = GetPlexXML(url);
  
  std::string strMediaType = album ? MediaTypeAlbum : MediaTypeArtist;
  std::string strMediaTypeUrl = album ? "plex://music/songs/" : "plex://music/albums/";
  
  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    /*
    <Directory
       ratingKey="5453"
       key="/library/metadata/5453/children"
       guid="com.plexapp.agents.plexmusic://gracenote/artist/05CCBFDB0877C16F?lang=en"
       type="artist"
       title="Amy MacDonald"
       summary="Amy Macdonald (born 25 August 1987 in Bishopbriggs, East Dunbartonshire) is a Scottish recording artist. Macdonald rose to fame in 2007 with her debut album, This Is the Life (2007) and its fourth single, &#34;This Is the Life&#34;. The single charted at number one in six countries, while reaching the top ten in another eleven countries. Her third album, Life in a Beautiful Light, was released on June 11, 2012."
       index="1"
       thumb="/library/metadata/5453/thumb/1473727903"
       art="/library/metadata/5453/art/1473727903"
       addedAt="1355585491"
       updatedAt="1473727903">
     
    <Genre tag="Adult Alternative Rock" />
    <Genre tag="Pop" />
    <Country tag="United Kingdom" />
    </Directory>
     
     /// Album
     
     <Directory ratingKey="5752" key="/library/metadata/5752/children" guid="local://5752" parentRatingKey="5508" type="album" title="Atomska trilogija" parentKey="/library/metadata/5508" parentTitle="Atomsko skloniste" summary="" index="1" year="1980" thumb="/library/metadata/5752/thumb/1467464126" originallyAvailableAt="1980-01-01" addedAt="1355587058" updatedAt="1467464126">
     <Genre tag="Other" />
     </Directory>
    */
    const TiXmlElement* directoryNode = rootXmlNode->FirstChildElement("Directory");
    while (directoryNode)
    {
      /*
       void SetURL(const std::string& strURL);
       void SetTitle(const std::string& strTitle);
       void SetArtist(const std::string& strArtist);
       void SetArtist(const std::vector<std::string>& artists, bool FillDesc = false);
       void SetArtistDesc(const std::string& strArtistDesc);
       void SetDateAdded(const std::string& strDateAdded);
       void SetDateAdded(const CDateTime& strDateAdded);
       */
      rtn = true;
      CFileItemPtr plexItem(new CFileItem());
      // set m_bIsFolder to true to indicate we are artist list
      plexItem->m_bIsFolder = true;
      plexItem->SetLabel(XMLUtils::GetAttribute(directoryNode, "title"));
      CURL url1(url);
      url1.SetFileName("library/metadata/" + XMLUtils::GetAttribute(directoryNode, "ratingKey") + "/children");
      plexItem->SetPath(strMediaTypeUrl + Base64URL::Encode(url1.Get()));
      plexItem->SetMediaServiceId(XMLUtils::GetAttribute(directoryNode, "ratingKey"));
      
      plexItem->GetMusicInfoTag()->m_type = strMediaType;
      plexItem->GetMusicInfoTag()->SetTitle(XMLUtils::GetAttribute(directoryNode, "title"));
      if (album)
      {
        plexItem->GetMusicInfoTag()->SetArtistDesc(XMLUtils::GetAttribute(directoryNode, "parentTitle"));
        plexItem->SetProperty("artist", XMLUtils::GetAttribute(directoryNode, "parentTitle"));
        plexItem->SetProperty("PlexAlbumKey", XMLUtils::GetAttribute(directoryNode, "ratingKey"));
      }
      else
      {
        plexItem->GetMusicInfoTag()->SetArtistDesc(XMLUtils::GetAttribute(directoryNode, "title"));
        plexItem->SetProperty("PlexArtistKey", XMLUtils::GetAttribute(directoryNode, "ratingKey"));
      }
      plexItem->GetMusicInfoTag()->SetAlbum(XMLUtils::GetAttribute(directoryNode, "title"));
      plexItem->GetMusicInfoTag()->SetYear(atoi(XMLUtils::GetAttribute(directoryNode, "title").c_str()));

      
      value = XMLUtils::GetAttribute(directoryNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("thumb", url1.Get());
      plexItem->SetProperty("thumb", url1.Get());
      
      value = XMLUtils::GetAttribute(directoryNode, "art");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url1.SetFileName(value);
      plexItem->SetArt("fanart", url1.Get());
      plexItem->SetProperty("fanart", url1.Get());
      
      time_t addedTime = atoi(XMLUtils::GetAttribute(directoryNode, "addedAt").c_str());
      CDateTime aTime(addedTime);
      plexItem->GetMusicInfoTag()->SetDateAdded(aTime);
      
      GetMusicDetails(*plexItem, directoryNode);
      SetPlexItemProperties(*plexItem);
      items.Add(plexItem);
      directoryNode = directoryNode->NextSiblingElement("Directory");
    }
  }
  items.SetProperty("library.filter", "true");
  items.GetVideoInfoTag()->m_type = strMediaType;
  SetPlexItemProperties(items);
  
  return rtn;
}

bool CPlexUtils::GetPlexSongs(CFileItemList &items, std::string url)
{
  /*
   song.strTitle,
   song.strMusicBrainzTrackID,
   song.strFileName,
   song.strComment,
   song.strMood,
   song.strThumb,
   song.GetArtistString(), // NOTE: Don't call this function internally!!!
   song.genre,
   song.iTrack,
   song.iDuration,
   song.iYear,
   song.iTimesPlayed,
   song.iStartOffset,
   song.iEndOffset,
   song.lastPlayed,
   song.rating
   
   void SetURL(const std::string& strURL);
   void SetTitle(const std::string& strTitle);
   void SetArtist(const std::string& strArtist);
   void SetArtist(const std::vector<std::string>& artists, bool FillDesc = false);
   void SetArtistDesc(const std::string& strArtistDesc);
   void SetAlbum(const std::string& strAlbum);
   void SetAlbumId(const int iAlbumId);
   void SetAlbumArtist(const std::string& strAlbumArtist);
   void SetAlbumArtist(const std::vector<std::string>& albumArtists, bool FillDesc = false);
   void SetAlbumArtistDesc(const std::string& strAlbumArtistDesc);
   void SetGenre(const std::string& strGenre);
   void SetGenre(const std::vector<std::string>& genres);
   void SetYear(int year);
   void SetDatabaseId(long id, const std::string &type);
   void SetReleaseDate(SYSTEMTIME& dateTime);
   void SetTrackNumber(int iTrack);
   void SetDiscNumber(int iDiscNumber);
   void SetTrackAndDiscNumber(int iTrackAndDisc);
   void SetDuration(int iSec);
   void SetLoaded(bool bOnOff = true);
   void SetArtist(const CArtist& artist);
   void SetAlbum(const CAlbum& album);
   void SetSong(const CSong& song);
   
   <Track
   ratingKey="5455"
   key="/library/metadata/5455"
   guid="com.plexapp.agents.plexmusic://gracenote/track/142066031-CCBFDBA3584154C8D3B9AC5A5DB18C2C/142066032-861BDFE03C20AA0E1189DCD803EA6F27?lang=en"
   parentRatingKey="5454"
   grandparentRatingKey="5453"
   type="track"
   title="Mr. Rock &amp; Roll"
   grandparentKey="/library/metadata/5453"
   parentKey="/library/metadata/5454"
   grandparentTitle="Amy MacDonald"
   parentTitle="This Is The Life"
   summary=""
   index="1"
   parentIndex="1"
   ratingCount="210254"
   year="2007"
   thumb="/library/metadata/5454/thumb/1467464063"
   art="/library/metadata/5453/art/1474411502"
   parentThumb="/library/metadata/5454/thumb/1467464063"
   grandparentThumb="/library/metadata/5453/thumb/1474411502"
   grandparentArt="/library/metadata/5453/art/1474411502"
   duration="213856"
   addedAt="1355585491"
   updatedAt="1467464062"
   chapterSource="">
     <Media id="5058" duration="213856" bitrate="284" audioChannels="2" audioCodec="aac" container="mp4" optimizedForStreaming="1" audioProfile="lc" has64bitOffsets="0">
     <Part
   id="5062"
   key="/library/parts/5062/1355585491/file.m4a"
   duration="213856"
   file="/share/CACHEDEV1_DATA/Music/Amy MacDonald/This Is The Life/01 Mr. Rock &amp; Roll.m4a"
   size="7604634"
   audioProfile="lc"
   container="mp4"
   has64bitOffsets="0"
   hasThumbnail="1"
   optimizedForStreaming="1" />
     </Media>
   </Track>
   */
  bool rtn = false;
  std::string value;
  TiXmlDocument xml = GetPlexXML(url);
  TiXmlElement* rootXmlNode = xml.RootElement();
  if (rootXmlNode)
  {
    const TiXmlElement* trackNode = rootXmlNode->FirstChildElement("Track");
    while (trackNode)
    {
      rtn = true;
      CFileItemPtr plexItem(new CFileItem());
      plexItem->SetLabel(XMLUtils::GetAttribute(trackNode, "title"));
      
      CURL url1(url);
      const TiXmlElement* mediaNode = trackNode->FirstChildElement("Media");
      if(mediaNode)
      {
        const TiXmlElement* partNode = mediaNode->FirstChildElement("Part");
        if(partNode)
        {
          XMLUtils::GetAttribute(partNode, "id");
          std::string key = ((TiXmlElement*) partNode)->Attribute("key");
          if (!key.empty() && (key[0] == '/'))
            StringUtils::TrimLeft(key, "/");
          url1.SetFileName(key);
          plexItem->SetPath(url1.Get());
          plexItem->SetMediaServiceId(XMLUtils::GetAttribute(trackNode, "ratingKey"));
          plexItem->SetProperty("PlexSongKey", XMLUtils::GetAttribute(trackNode, "ratingKey"));
          plexItem->GetMusicInfoTag()->m_type = MediaTypeSong;
          plexItem->GetMusicInfoTag()->SetTitle(XMLUtils::GetAttribute(trackNode, "title"));
//          plexItem->GetMusicInfoTag()->SetArtistDesc(XMLUtils::GetAttribute(mediaNode, "summary"));
          plexItem->SetLabel(XMLUtils::GetAttribute(trackNode, "title"));
          plexItem->GetMusicInfoTag()->SetArtist(XMLUtils::GetAttribute(trackNode, "grandparentTitle"));
          plexItem->GetMusicInfoTag()->SetAlbum(XMLUtils::GetAttribute(trackNode, "parentTitle"));
          int year = atoi(XMLUtils::GetAttribute(trackNode, "year").c_str());
          plexItem->GetMusicInfoTag()->SetYear(year);
          plexItem->GetMusicInfoTag()->SetTrackNumber(atoi(XMLUtils::GetAttribute(trackNode, "index").c_str()));
          plexItem->GetMusicInfoTag()->SetDuration(atoi(XMLUtils::GetAttribute(trackNode, "duration").c_str())/1000);
          
          value = XMLUtils::GetAttribute(trackNode, "thumb");
          if (!value.empty() && (value[0] == '/'))
            StringUtils::TrimLeft(value, "/");
          url1.SetFileName(value);
          plexItem->SetArt("thumb", url1.Get());
          
          value = XMLUtils::GetAttribute(trackNode, "art");
          if (!value.empty() && (value[0] == '/'))
            StringUtils::TrimLeft(value, "/");
          url1.SetFileName(value);
          plexItem->SetArt("fanart", url1.Get());
          
          
          time_t addedTime = atoi(XMLUtils::GetAttribute(trackNode, "addedAt").c_str());
          CDateTime aTime(addedTime);
          plexItem->GetMusicInfoTag()->SetDateAdded(aTime);
          plexItem->GetMusicInfoTag()->SetLoaded(true);
          SetPlexItemProperties(*plexItem);
          items.Add(plexItem);
        }
      }
      trackNode = trackNode->NextSiblingElement("Track");
    }
  }
  items.SetProperty("library.filter", "true");
  items.GetMusicInfoTag()->m_type = MediaTypeSong;
  SetPlexItemProperties(items);
  return rtn;
}

bool CPlexUtils::ShowMusicInfo(CFileItem item)
{
  std::string type = item.GetMusicInfoTag()->m_type;
  if (type == MediaTypeSong)
  {
    CGUIDialogSongInfo *dialog = (CGUIDialogSongInfo *)g_windowManager.GetWindow(WINDOW_DIALOG_SONG_INFO);
    if (dialog)
    {
      dialog->SetSong(&item);
      dialog->Open();
    }
  }
  else if (type == MediaTypeAlbum)
  {
    CGUIDialogMusicInfo *pDlgAlbumInfo = (CGUIDialogMusicInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_MUSIC_INFO);
    if (pDlgAlbumInfo)
    {
      pDlgAlbumInfo->SetAlbum(item);
      pDlgAlbumInfo->Open();
    }
  }
  else if (type == MediaTypeArtist)
  {
    CGUIDialogMusicInfo *pDlgArtistInfo = (CGUIDialogMusicInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_MUSIC_INFO);
    if (pDlgArtistInfo)
    {
      pDlgArtistInfo->SetArtist(item);
      pDlgArtistInfo->Open();
    }
  }
  return true;
}

bool CPlexUtils::GetPlexRecentlyAddedAlbums(CFileItemList &items, int limit)
{
  if (CPlexServices::GetInstance().HasClients())
  {
    CFileItemList plexItems;
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all plex clients and pull recently added for each library section
    std::vector<CPlexClientPtr> clients;
    CPlexServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;
      
      std::vector<PlexSectionsContent> contents;
      contents = client->GetArtistContent();
      for (const auto &content : contents)
      {
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(curl.GetFileName() + content.section + "/recentlyAdded");
//        curl.SetFileName(curl.GetFileName() + "recentlyAdded");
        curl.SetProtocolOptions(curl.GetProtocolOptions() + StringUtils::Format("&X-Plex-Container-Start=0&X-Plex-Container-Size=%i", limit));
        
        GetPlexArtistsOrAlbum(items, curl.Get(), true);
        
        for (int item = 0; item < plexItems.Size(); ++item)
          CPlexUtils::SetPlexItemProperties(*plexItems[item], client);
      }
      SetPlexItemProperties(plexItems);
      items.Append(plexItems);
      items.SetLabel("Recently Added Albums");
      items.Sort(SortByDateAdded, SortOrderDescending);
      plexItems.ClearItems();
    }
    
  }
  return true;
}

bool CPlexUtils::GetPlexAlbumSongs(CFileItem item, CFileItemList &items)
{
  std::string url = item.GetPath();
  if (StringUtils::StartsWithNoCase(url, "plex://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  CURL url1(url);
  url1.SetFileName("library/metadata/" + item.GetProperty("PlexAlbumKey").asString() + "/children");
  url1.RemoveProtocolOption("X-Plex-Container-Start");
  url1.RemoveProtocolOption("X-Plex-Container-Size");
  return GetPlexSongs(items, url1.Get());
  
}

bool CPlexUtils::GetPlexMediaTotals(MediaServicesMediaCount &totals)
{
  // totals might or might not be reset to zero so add to it
  std::vector<CPlexClientPtr> clients;
  CPlexServices::GetInstance().GetClients(clients);
  for (const auto &client : clients)
  {
    /// Only do this for "owned" servers, getting totals from large servers can take up to 1 minute. setting for this maybe?
    if (client->GetOwned() == "1")
    {
      std::vector<PlexSectionsContent> contents;
      // movie totals
      contents = client->GetMovieContent();
      for (const auto &content : contents)
      {
        TiXmlDocument xml;
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(content.section + "/all?type=1&unwatched=1");
        curl.SetProtocolOption("X-Plex-Container-Start", "0");
        curl.SetProtocolOption("X-Plex-Container-Size", "0");
        // get movie unwatched totals
        xml = GetPlexXML(curl.Get());
        totals.iMovieUnwatched += ParsePlexMediaXML(xml);
        // get movie totals
        curl.SetFileName(content.section + "/all?type=1");
        xml = GetPlexXML(curl.Get());
        totals.iMovieTotal +=ParsePlexMediaXML(xml);
      }
      // Show Totals
      contents = client->GetTvContent();
      for (const auto &content : contents)
      {
        TiXmlDocument xml;
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(content.section + "/all?type=4&unwatched=1");
        curl.SetProtocolOption("X-Plex-Container-Start", "0");
        curl.SetProtocolOption("X-Plex-Container-Size", "0");
        // get episode unwatched totals
        xml = GetPlexXML(curl.Get());
        totals.iEpisodeUnwatched += ParsePlexMediaXML(xml);
        // get episode totals
        curl.SetFileName(content.section + "/all?type=4");
        xml = GetPlexXML(curl.Get());
        totals.iEpisodeTotal += ParsePlexMediaXML(xml);
        // get show totals
        curl.SetFileName(content.section + "/all?type=2");
        xml = GetPlexXML(curl.Get());
        totals.iShowTotal += ParsePlexMediaXML(xml);
        // get show unwatched totals
        curl.SetFileName(content.section + "/all?type=2&unwatched=1");
        xml = GetPlexXML(curl.Get());
        totals.iShowUnwatched += ParsePlexMediaXML(xml);
      }
      // Music Totals
      contents = client->GetArtistContent();
      for (const auto &content : contents)
      {
        TiXmlDocument xml;
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetFileName(content.section + "/all?type=8");
        curl.SetProtocolOption("X-Plex-Container-Start", "0");
        curl.SetProtocolOption("X-Plex-Container-Size", "0");
        // get artist totals
        xml = GetPlexXML(curl.Get());
        totals.iMusicArtist += ParsePlexMediaXML(xml);
        // get Album totals
        curl.SetFileName(content.section + "/all?type=9");
        xml = GetPlexXML(curl.Get());
        totals.iMusicAlbums += ParsePlexMediaXML(xml);
        // get Song totals
        curl.SetFileName(content.section + "/all?type=10");
        xml = GetPlexXML(curl.Get());
        totals.iMusicSongs += ParsePlexMediaXML(xml);
      }
    }
  }
  return true;
}
/// End of Plex Music

bool CPlexUtils::GetURL(CFileItem &item)
{
  
  if (!CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXTRANSCODE))
    return true;
  
  CURL url(item.GetPath());
  std::string cleanUrl = url.GetWithoutFilename();
  CURL curl(cleanUrl);
  
  if ((!CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXTRANSCODELOCAL)) &&
        CPlexServices::GetInstance().ClientIsLocal(cleanUrl))
  {
    /// always transcode h265 and VC1
    if (!(CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXTRANSCODELOCALEXCLUSION) &&
          (item.GetVideoInfoTag()->m_streamDetails.GetVideoCodec() == "hevc" ||
          item.GetVideoInfoTag()->m_streamDetails.GetVideoCodec() == "vc1")))
        return true;
  }
  
  
  int res = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXQUALITY);
  
  std::string maxBitrate;
  std::string resolution;
  
  switch (res)
  {
    case  1:
      maxBitrate = "320";
      resolution = "576x320";
      break;
    case  2:
      maxBitrate = "720";
      resolution = "720x480";
      break;
    case  3:
      maxBitrate = "1500";
      resolution = "1024x768";
      break;
    case  4:
      maxBitrate = "2000";
      resolution = "1280x720";
      break;
    case  5:
      maxBitrate = "3000";
      resolution = "1280x720";
      break;
    case  6:
      maxBitrate = "4000";
      resolution = "1920x1080";
      break;
    case  7:
      maxBitrate = "8000";
      resolution = "1920x1080";
      break;
    case  8:
      maxBitrate = "10000";
      resolution = "1920x1080";
      break;
    case  9:
      maxBitrate = "12000";
      resolution = "1920x1080";
      break;
    case  10:
      maxBitrate = "20000";
      resolution = "1920x1080";
      break;
    case  11:
      maxBitrate = "40000";
      resolution = "1920x1080";
      break;
    default:
      maxBitrate = "2000";
      resolution = "1280x720";
      break;
  }
  
  CLog::Log(LOGDEBUG, "CPlexUtils::GetURL - bitrate [%s] res [%s]", maxBitrate.c_str(), resolution.c_str());
  
  std::string plexID = item.GetMediaServiceId();
  std::string uuidStr = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID);
  
#if 1
  curl.SetFileName("video/:/transcode/universal/start.m3u8");
  curl.SetOption("hasMDE", "1");
  curl.SetOption("maxVideoBitrate", maxBitrate);
  curl.SetOption("protocol", "hls");
  curl.SetOption("secondsPerSegment", "10");
  curl.SetOption("session", uuidStr);
  curl.SetOption("offset", "0");
  curl.SetOption("videoQuality", "100");
  curl.SetOption("videoResolution", resolution);
  curl.SetOption("directStream", "1");
  curl.SetOption("directPlay", "0");
  curl.SetOption("audioBoost", "0");
  curl.SetOption("fastSeek", "1");
  curl.SetOption("subtitleSize", "100");
  curl.SetOption("location", CPlexServices::GetInstance().ClientIsLocal(cleanUrl) ? "lan" : "wan");
  curl.SetOption("path", "library/metadata/" + plexID);
#else
  curl.SetFileName("video/:/transcode/universal/start.mkv");
  curl.SetOption("hasMDE", "1");
  curl.SetOption("maxVideoBitrate", maxBitrate);
  curl.SetOption("session", uuidStr);
  std::string resumeTime = StringUtils::Format("%f", item.GetVideoInfoTag()->m_resumePoint.timeInSeconds);
  CLog::Log(LOGNOTICE, "resumeTime: %s seconds", resumeTime.c_str());
  curl.SetOption("offset", resumeTime);
  curl.SetOption("videoQuality", "100");
  curl.SetOption("videoResolution", resolution);
  curl.SetOption("directStream", "1");
  curl.SetOption("directPlay", "0");
  curl.SetOption("audioBoost", "0");
  curl.SetOption("fastSeek", "1");
  curl.SetOption("subtitleSize", "100");
  curl.SetOption("copyts", "1");
  curl.SetOption("partIndex", "0");
  curl.SetOption("mediaIndex", "0");
  int bufferSize = 8 * 1024 * 124;
  curl.SetOption("mediaBufferSize", StringUtils::Format("%d", bufferSize));
  // plex has duration in milliseconds
  std::string duration = StringUtils::Format("%i", item.GetVideoInfoTag()->m_duration * 1000);
  curl.SetOption("duration", duration);
  curl.SetOption("location", CPlexServices::GetInstance().ClientIsLocal(cleanUrl) ? "lan" : "wan");
  curl.SetOption("path", "library/metadata/" + plexID);
#endif
  
  // do we want audio transcoded?
  if (!CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXTRANSCODEAUDIO))
  {
    curl.SetOption("X-Plex-Client-Profile-Extra", "append-transcode-target-audio-codec(type=videoProfile&context=streaming&codec=h264&protocol=hls&container=mpegts&audioCodec=dts,ac3,dca,mp3,eac3,truehd)");
    curl.SetOption("X-Plex-Client-Capabilities","protocols=http-live-streaming,http-mp4-streaming,http-mp4-video,http-mp4-video-720p,http-mp4-video-1080p,http-streaming-video,http-streaming-video-720p,http-streaming-video-1080p;videoDecoders=mpeg1video,mpeg2video,mpeg4,h264{profile:high&resolution:1080&level:51};audioDecoders=*");
  }
  //+add-limitation(scope=videoAudioCodec&scopeName=dca&type=upperBound&name=audio.channels&value=6&isRequired=false)
 // +add-direct-play-profile(type=videoProfile&container=mkv&codec=mpeg1video,mpeg2video,mpeg4,h264&audioCodec=pcm,mp3,ac3,dts,dca,eac3,mp2,flac,truehd,lpcm)
  else
  {
    curl.SetOption("X-Plex-Client-Capabilities","protocols=http-live-streaming,http-mp4-streaming,http-mp4-video,http-mp4-video-720p,http-mp4-video-1080p,http-streaming-video,http-streaming-video-720p,http-streaming-video-1080p;videoDecoders=mpeg4,h264{profile:high&resolution:1080&level:51};audioDecoders=aac");
    curl.SetOption("X-Plex-Client-Profile-Extra", "append-transcode-target-audio-codec(type=videoProfile&context=streaming&protocol=hls&container=mpegts&audioCodec=aac)");
  }
  curl.SetProtocolOption("X-Plex-Token", url.GetProtocolOption("X-Plex-Token"));
  curl.SetProtocolOption("X-Plex-Platform", "MrMC");
  curl.SetProtocolOption("X-Plex-Device","Plex Home Theater");
  curl.SetProtocolOption("X-Plex-Client-Identifier", uuidStr);

  item.SetPath(curl.Get());
  item.SetProperty("PlexTranscoder", true);
  return true;
}

void CPlexUtils::StopTranscode(CFileItem &item)
{
  CURL url(item.GetPath());
  std::string cleanUrl = url.Get();
  CURL curl(cleanUrl);
  
  std::string uuidStr = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID);
  
  CURL url1(item.GetPath());
  CURL url2(URIUtils::GetParentPath(cleanUrl));
  CURL url3(url2.GetWithoutFilename());
  url3.SetProtocolOption("X-Plex-Token",url1.GetProtocolOption("X-Plex-Token"));
  cleanUrl = url3.Get();
  
  if (StringUtils::StartsWithNoCase(cleanUrl, "plex://"))
    cleanUrl = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  
  std::string filename = StringUtils::Format("video/:/transcode/universal/stop?session=%s",uuidStr.c_str());
  ReportToServer(cleanUrl, filename);
}
