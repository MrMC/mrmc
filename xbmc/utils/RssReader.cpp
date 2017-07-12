/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "network/Network.h"
#include "threads/SystemClock.h"
#include "RssReader.h"
#include "utils/HTMLUtil.h"
#include "Application.h"
#include "CharsetConverter.h"
#include "URL.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#if defined(TARGET_DARWIN)
#include "platform/darwin/osx/CocoaInterface.h"
#endif
#include "settings/AdvancedSettings.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIRSSControl.h"
#include "threads/SingleLock.h"
#include "log.h"

#define RSS_COLOR_BODY      0
#define RSS_COLOR_HEADLINE  1
#define RSS_COLOR_CHANNEL   2

using namespace XFILE;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRssReader::CRssReader() : CThread("RSSReader")
{
  m_pObserver = NULL;
  m_spacesBetweenFeeds = 0;
  m_bIsRunning = false;
  m_savedScrollPixelPos = 0;
  m_rtlText = false;
  m_requestRefresh = false;
}

CRssReader::~CRssReader()
{
  m_pObserver = NULL;
  StopThread();
  delete m_vecTimeStamp;
}

void CRssReader::Create(IRssObserver* aObserver, const std::string& aUrl, const int &time, int spacesBetweenFeeds, bool rtl)
{
  CSingleLock lock(m_critical);

  m_pObserver = aObserver;
  m_spacesBetweenFeeds = spacesBetweenFeeds;
  m_vecUrl = aUrl;
  m_strFeed = L"";
  m_strColor = L"";
  // set update times
  m_vecUpdateTime = time;
  m_rtlText = rtl;
  m_requestRefresh = false;

  m_vecTimeStamp = new SYSTEMTIME;
  GetLocalTime(m_vecTimeStamp);
  Start();
}

void CRssReader::requestRefresh()
{
  m_requestRefresh = true;
}

void CRssReader::Start()
{
  CSingleLock lock(m_critical);
  if (!m_bIsRunning)
  {
    StopThread();
    m_bIsRunning = true;
    CThread::Create(false, THREAD_MINSTACKSIZE);
  }
}

void CRssReader::OnExit()
{
  m_bIsRunning = false;
}

void CRssReader::Process()
{

  CSingleLock lock(m_critical);

  m_strFeed.clear();
  m_strColor.clear();

  CCurlFile http;
  http.SetUserAgent(g_advancedSettings.m_userAgent);
  http.SetTimeout(2);
  std::string strXML;
  lock.Leave();

  int nRetries = 3;
  CURL url(m_vecUrl);
  url.SetProtocolOption("seekable", "0");
  std::string fileCharset;

  // we wait for the network to come up
  if ((url.IsProtocol("http") || url.IsProtocol("https")) &&
      !g_application.getNetwork().IsAvailable())
  {
    CLog::Log(LOGWARNING, "RSS: No network connection");
    strXML = "<rss><item><title>"+g_localizeStrings.Get(15301)+"</title></item></rss>";
  }
  else
  {
    XbmcThreads::EndTime timeout(15000);
    while (!m_bStop && nRetries > 0)
    {
      if (timeout.IsTimePast())
      {
        CLog::Log(LOGERROR, "Timeout while retrieving rss feed: %s", m_vecUrl.c_str());
        break;
      }
      nRetries--;

      if (!url.IsProtocol("http") && !url.IsProtocol("https"))
      {
        CFile file;
        auto_buffer buffer;
        if (file.LoadFile(m_vecUrl, buffer) > 0)
        {
          strXML.assign(buffer.get(), buffer.length());
          break;
        }
      }
      else
      {
        if (http.Get(url.Get(), strXML))
        {
          fileCharset = http.GetServerReportedCharset();
          CLog::Log(LOGDEBUG, "Got rss feed: %s", m_vecUrl.c_str());
          break;
        }
        else if (nRetries > 0)
          Sleep(5000); // Network problems? Retry, but not immediately.
        else
          CLog::Log(LOGERROR, "Unable to obtain rss feed: %s", m_vecUrl.c_str());
      }
    }
    http.Cancel();
  }
  if (!strXML.empty() && m_pObserver)
  {
    // erase any <content:encoded> tags (also unsupported by tinyxml)
    size_t iStart = strXML.find("<content:encoded>");
    size_t iEnd = 0;
    while (iStart != std::string::npos)
    {
      // get <content:encoded> end position
      iEnd = strXML.find("</content:encoded>", iStart) + 18;

      // erase the section
      strXML = strXML.erase(iStart, iEnd - iStart);

      iStart = strXML.find("<content:encoded>");
    }

    if (Parse(strXML, fileCharset))
      CLog::Log(LOGDEBUG, "Parsed rss feed: %s", m_vecUrl.c_str());
  }
  UpdateObserver();
}

void CRssReader::getFeed(vecText &text)
{
  text.clear();
  // double the spaces at the start of the set
  for (int j = 0; j < m_spacesBetweenFeeds; j++)
    text.push_back(L' ');

  for (int j = 0; j < m_spacesBetweenFeeds; j++)
    text.push_back(L' ');

  for (size_t j = 0; j < m_strFeed.size(); j++)
  {
    character_t letter = m_strFeed[j] | ((m_strColor[j] - 48) << 16);
    text.push_back(letter);
  }
}

void CRssReader::AddTag(const std::string &aString)
{
  m_tagSet.push_back(aString);
}

void CRssReader::AddString(std::wstring aString, int aColour)
{
  if (m_rtlText)
    m_strFeed = aString + m_strFeed;
  else
    m_strFeed += aString;

  size_t nStringLength = aString.size();

  for (size_t i = 0;i < nStringLength;i++)
    aString[i] = (char)(48 + aColour);

  if (m_rtlText)
    m_strColor = aString + m_strColor;
  else
    m_strColor += aString;
}

void CRssReader::GetNewsItems(TiXmlElement* channelXmlNode)
{
  HTML::CHTMLUtil html;

  TiXmlElement * itemNode = channelXmlNode->FirstChildElement("item");
  std::map<std::string, std::wstring> mTagElements;
  typedef std::pair<std::string, std::wstring> StrPair;
  std::list<std::string>::iterator i;

  // Add the title tag in if we didn't pass any tags in at all
  // Represents default behaviour before configurability
  
  if (m_tagSet.empty())
    AddTag("title");
  
  while (itemNode > 0)
  {
    TiXmlNode* childNode = itemNode->FirstChild();
    mTagElements.clear();
    while (childNode > 0)
    {
      std::string strName = childNode->ValueStr();
      
      for (i = m_tagSet.begin(); i != m_tagSet.end(); ++i)
      {
        if (!childNode->NoChildren() && *i == strName)
        {
          std::string htmlText = childNode->FirstChild()->ValueStr();
          
          // This usually happens in right-to-left languages where they want to
          // specify in the RSS body that the text should be RTL.
          // <title>
          //  <div dir="RTL">��� ����: ���� �� �����</div>
          // </title>
          if (htmlText == "div" || htmlText == "span")
            htmlText = childNode->FirstChild()->FirstChild()->ValueStr();
          
          std::wstring unicodeText, unicodeText2;
          
          g_charsetConverter.utf8ToW(htmlText, unicodeText2, m_rtlText);
          html.ConvertHTMLToW(unicodeText2, unicodeText);
          
          mTagElements.insert(StrPair(*i, unicodeText));
        }
      }
      childNode = childNode->NextSibling();
    }
    
    int rsscolour = RSS_COLOR_HEADLINE;
    for (i = m_tagSet.begin(); i != m_tagSet.end(); ++i)
    {
      std::map <std::string, std::wstring>::iterator j = mTagElements.find(*i);
      
      if (j == mTagElements.end())
        continue;
      
      std::wstring& text = j->second;
      AddString(text, rsscolour);
      rsscolour = RSS_COLOR_CHANNEL;
      text = L" - ";
      AddString(text, rsscolour);
    }
    itemNode = itemNode->NextSiblingElement("item");
  }
}

void CRssReader::GetAtomItems(TiXmlElement* channelXmlNode)
{
  
  TiXmlElement* titleNode = channelXmlNode->FirstChildElement("title");
  if (titleNode && !titleNode->NoChildren())
  {
    std::string strChannel = titleNode->FirstChild()->Value();
    std::wstring strChannelUnicode;
    g_charsetConverter.utf8ToW(strChannel, strChannelUnicode, m_rtlText);
    AddString(strChannelUnicode, RSS_COLOR_CHANNEL);
    
    AddString(L":", RSS_COLOR_CHANNEL);
    AddString(L" ", RSS_COLOR_CHANNEL);
  }
  
  HTML::CHTMLUtil html;

  TiXmlElement * itemNode = channelXmlNode->FirstChildElement("entry");
  std::map <std::string, std::wstring> mTagElements;
  typedef std::pair <std::string, std::wstring> StrPair;
  std::list <std::string>::iterator i;

  // Add the title tag in if we didn't pass any tags in at all
  // Represents default behaviour before configurability

  if (m_tagSet.empty())
    AddTag("title");

  while (itemNode != nullptr)
  {
    TiXmlNode* childNode = itemNode->FirstChild();
    mTagElements.clear();
    while (childNode != nullptr)
    {
      std::string strName = childNode->ValueStr();

      for (i = m_tagSet.begin(); i != m_tagSet.end(); ++i)
      {
        if (!childNode->NoChildren() && *i == strName)
        {
          std::string htmlText = childNode->FirstChild()->ValueStr();

          // This usually happens in right-to-left languages where they want to
          // specify in the RSS body that the text should be RTL.
          // <title>
          //  <div dir="RTL">��� ����: ���� �� �����</div>
          // </title>
          if (htmlText == "div" || htmlText == "span")
            htmlText = childNode->FirstChild()->FirstChild()->ValueStr();

          std::wstring unicodeText, unicodeText2;

          g_charsetConverter.utf8ToW(htmlText, unicodeText2, m_rtlText);
          html.ConvertHTMLToW(unicodeText2, unicodeText);

          mTagElements.insert(StrPair(*i, unicodeText));
        }
      }
      childNode = childNode->NextSibling();
    }

    int rsscolour = RSS_COLOR_HEADLINE;
    for (i = m_tagSet.begin(); i != m_tagSet.end(); ++i)
    {
      std::map<std::string, std::wstring>::iterator j = mTagElements.find(*i);

      if (j == mTagElements.end())
        continue;

      std::wstring& text = j->second;
      AddString(text, rsscolour);
      rsscolour = RSS_COLOR_BODY;
      text = L" - ";
      AddString(text, rsscolour);
    }
    itemNode = itemNode->NextSiblingElement("entry");
  }
}

bool CRssReader::Parse(const std::string& data, const std::string& charset)
{
  m_xml.Clear();
  m_xml.Parse(data, charset);

  CLog::Log(LOGDEBUG, "RSS feed encoding: %s", m_xml.GetUsedCharset().c_str());

  return Parse();
}

bool CRssReader::Parse()
{
  TiXmlElement* rootXmlNode = m_xml.RootElement();

  if (!rootXmlNode)
    return false;

  TiXmlElement* rssXmlNode = NULL;

  std::string strValue = rootXmlNode->ValueStr();
  if (strValue.find("rss") != std::string::npos ||
      strValue.find("rdf") != std::string::npos)
    rssXmlNode = rootXmlNode;
  else if (strValue.find("feed") != std::string::npos)
  {
    // atom feed
    GetAtomItems(rootXmlNode);
    return true;
  }
  else
  {
    // Unable to find root <rss> or <rdf> node
    return false;
  }

  TiXmlElement* channelXmlNode = rssXmlNode->FirstChildElement("channel");
  if (channelXmlNode)
  {
    TiXmlElement* titleNode = channelXmlNode->FirstChildElement("title");
    if (titleNode && !titleNode->NoChildren())
    {
      std::string strChannel = titleNode->FirstChild()->Value();
      std::wstring strChannelUnicode;
      g_charsetConverter.utf8ToW(strChannel, strChannelUnicode, m_rtlText);
      AddString(strChannelUnicode, RSS_COLOR_CHANNEL);

      AddString(L":", RSS_COLOR_CHANNEL);
      AddString(L" ", RSS_COLOR_CHANNEL);
    }

    GetNewsItems(channelXmlNode);
  }

  GetNewsItems(rssXmlNode);

  // avoid trailing ' - '
  if (m_strFeed.size() > 3 && m_strFeed.substr(m_strFeed.size() - 3) == L" - ")
  {
    if (m_rtlText)
    {
      m_strFeed.erase(0, 3);
      m_strColor.erase(0, 3);
    }
    else
    {
      m_strFeed.erase(m_strFeed.length() - 3);
      m_strColor.erase(m_strColor.length() - 3);
    }
  }
  return true;
}

void CRssReader::SetObserver(IRssObserver *observer)
{
  m_pObserver = observer;
}

void CRssReader::UpdateObserver()
{
  if (!m_pObserver)
    return;

  vecText feed;
  getFeed(feed);
  if (!feed.empty())
  {
    CSingleLock lock(g_graphicsContext);
    if (m_pObserver) // need to check again when locked to make sure observer wasnt removed
      m_pObserver->OnFeedUpdate(feed);
  }
}

void CRssReader::CheckForUpdates()
{
  SYSTEMTIME time;
  GetLocalTime(&time);


  if (m_requestRefresh ||
     ((time.wDay * 24 * 60) + (time.wHour * 60) + time.wMinute) - ((m_vecTimeStamp->wDay * 24 * 60) + (m_vecTimeStamp->wHour * 60) + m_vecTimeStamp->wMinute) > m_vecUpdateTime)
  {
    CLog::Log(LOGDEBUG, "Updating RSS");
    GetLocalTime(m_vecTimeStamp);
  }

  m_requestRefresh = false;
}
