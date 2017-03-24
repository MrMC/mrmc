/*
 *      Copyright (C) 2017 Team MrMC
 *      https://github.com/MrMC
 *      based from EmbyMediaImporter.cpp
 *      Copyright (C) 2016 Team XBMC
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

#include "EmbyClientSync.h"

#include "EmbyServices.h"
#include "EmbyClient.h"
#include "EmbyUtils.h"

#include "contrib/easywsclient/easywsclient.hpp"
#include "filesystem/File.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/JSONVariantParser.h"
#include "video/VideoInfoTag.h"

typedef enum MediaImportChangesetType
{
  MediaImportChangesetTypeNone = 0,
  MediaImportChangesetTypeAdded,
  MediaImportChangesetTypeChanged,
  MediaImportChangesetTypeRemoved
} MediaImportChangesetType;

CEmbyClientSync::CEmbyClientSync(const std::string &name, const std::string &address, const std::string &deviceId, const std::string &accessToken)
  : CThread(StringUtils::Format("EmbyClientSync[%s]", name.c_str()).c_str())
  , m_address(address)
  , m_name(name)
  , m_websocket(nullptr)
  , m_stop(true)
{
  // ws://server.local:32400/:/websockets/notifications
  CURL curl(address);
  if (curl.GetProtocol() == "http")
    curl.SetProtocol("ws");
  else if (curl.GetProtocol() == "https")
    curl.SetProtocol("wss");
  curl.SetOption("api_key", accessToken);
  curl.SetOption("deviceId", deviceId);

  m_address = curl.Get();
}

CEmbyClientSync::~CEmbyClientSync()
{
  Stop();
}

void CEmbyClientSync::Start()
{
  if (!m_stop)
    return;

  m_stop = false;
  CThread::Create();
}

void CEmbyClientSync::Stop()
{
  if (m_stop)
    return;

  m_stop = true;
  CThread::StopThread();
}

void CEmbyClientSync::Process()
{
  static const int WebSocketTimeoutMs = 100;

  static const std::string NotificationMessageType = "MessageType";
  static const std::string NotificationData = "Data";
  static const std::string NotificationMessageTypeUserUpdated = "UserUpdated";
  static const std::string NotificationMessageTypeSessionEnded = "SessionEnded";
  static const std::string NotificationMessageTypeLibraryChanged = "LibraryChanged";
  static const std::string NotificationMessageTypeUserDataChanged = "UserDataChanged";
  static const std::string NotificationMessageTypePlaybackStart = "PlaybackStart";
  static const std::string NotificationMessageTypePlaybackStopped = "PlaybackStopped";
  static const std::string NotificationMessageTypeScheduledTaskEnded = "ScheduledTaskEnded";
  static const std::string NotificationLibraryChangedItemsAdded = "ItemsAdded";
  static const std::string NotificationLibraryChangedItemsUpdated = "ItemsUpdated";
  static const std::string NotificationLibraryChangedItemsRemoved = "ItemsRemoved";
  static const std::string NotificationUserDataChangedUserDataList = "UserDataList";
  static const std::string NotificationUserDataChangedUserDataItemId = "ItemId";

  struct ChangedLibraryItem
  {
    std::string itemId;
    MediaImportChangesetType changesetType;
  };

  m_websocket = easywsclient::WebSocket::from_url(m_address /* TODO: , origin */);
  if (!m_websocket)
  {
    CLog::Log(LOGERROR, "CEmbyClientSync: websocket connection failed from %s", m_name.c_str());
    m_stop = true;
  }
  else
    CLog::Log(LOGDEBUG, "CEmbyClientSync: websocket connected to %s", m_name.c_str());

  while (!m_stop && m_websocket->getReadyState() != easywsclient::WebSocket::CLOSED)
  {
    m_websocket->poll(WebSocketTimeoutMs);
    m_websocket->dispatch(
      [this](const std::string& msg)
      {
        const auto msgObject = CJSONVariantParser::Parse(msg);
        if (!msgObject.isObject() || !msgObject.isMember(NotificationMessageType) || !msgObject.isMember(NotificationData))
        {
          CLog::Log(LOGERROR, "CEmbyClientSync: invalid websocket notification from %s", m_name.c_str());
          return;
        }

        const std::string msgType = msgObject[NotificationMessageType].asString();
        // ignore SessionEnded (server spew)
        if (msgType == NotificationMessageTypeSessionEnded)
          return;
        // ignore UserUpdated (server spew)
        if (msgType == NotificationMessageTypeUserUpdated)
          return;
        // ignore PlaybackStart (server spew)
        if (msgType == NotificationMessageTypePlaybackStart)
          return;
        // ignore PlaybackStopped (server spew)
        if (msgType == NotificationMessageTypePlaybackStopped)
          return;
        // ignore ScheduledTaskEnded (server spew)
        if (msgType == NotificationMessageTypeScheduledTaskEnded)
          return;


        CLog::Log(LOGDEBUG, "[%s] %s: %s", this->m_name.c_str(), msgType.c_str(), msg.c_str());

        const auto msgData = msgObject[NotificationData];
        if (!msgData.isObject())
        {
          CLog::Log(LOGDEBUG, "CEmbyClientSync: ignoring websocket notification of type \"%s\" from %s", msgType.c_str(), m_name.c_str());
          return;
        }

        if (msgType == NotificationMessageTypeLibraryChanged)
        {
          const auto itemsAdded = msgData[NotificationLibraryChangedItemsAdded];
          const auto itemsUpdated = msgData[NotificationLibraryChangedItemsUpdated];
          const auto itemsRemoved = msgData[NotificationLibraryChangedItemsRemoved];

          std::vector<ChangedLibraryItem> changedLibraryItems;

          if (itemsAdded.isArray())
          {
            for (auto item = itemsAdded.begin_array(); item != itemsAdded.end_array(); ++item)
            {
              if (item->isString() && !item->empty())
                changedLibraryItems.emplace_back(ChangedLibraryItem { item->asString(), MediaImportChangesetTypeAdded });
            }
          }

          if (itemsUpdated.isArray())
          {
            for (auto item = itemsUpdated.begin_array(); item != itemsUpdated.end_array(); ++item)
            {
              if (item->isString() && !item->empty())
                changedLibraryItems.emplace_back(ChangedLibraryItem{ item->asString(), MediaImportChangesetTypeChanged });
            }
          }

          if (itemsRemoved.isArray())
          {
            for (auto item = itemsRemoved.begin_array(); item != itemsRemoved.end_array(); ++item)
            {
              if (item->isString() && !item->empty())
                changedLibraryItems.emplace_back(ChangedLibraryItem{ item->asString(), MediaImportChangesetTypeRemoved });
            }
          }

          for (const auto& changedLibraryItem : changedLibraryItems)
          {
            CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(m_address);
            if (client && client->GetPresence())
            {
              CLog::Log(LOGDEBUG, "CEmbyClientSync: processing changed item with id \"%s\" ", changedLibraryItem.itemId.c_str());
              switch(changedLibraryItem.changesetType)
              {
                default:
                  break;
                case MediaImportChangesetTypeAdded:
                  client->AddNewViewItem(changedLibraryItem.itemId);
                  break;
                case MediaImportChangesetTypeChanged:
                  client->UpdateViewItem(changedLibraryItem.itemId);
                  break;
                case MediaImportChangesetTypeRemoved:
                  client->RemoveViewItem(changedLibraryItem.itemId);
                  break;
              }
            }
          }
        }
        else if (msgType == NotificationMessageTypeUserDataChanged)
        {
          /*
          UserDataChanged: {"MessageType":"UserDataChanged","Data":{"UserId":"c0234e3b7f364e5da6ded482cde90f62","UserDataList":[{"PlayedPercentage":11.0171244559219,"PlaybackPositionTicks":7680000000,"PlayCount":2,"IsFavorite":false,"LastPlayedDate":"2017-03-19T17:55:21.9907250Z","Played":true,"Key":"274870","ItemId":"f822f61be1862484f5a2e4c854d244ac"},{"UnplayedItemCount":751,"PlaybackPositionTicks":0,"PlayCount":0,"IsFavorite":false,"Played":false,"Key":"207cf78d-67ba-ae9a-9328-4169cb204f16","ItemId":"207cf78d67baae9a93284169cb204f16"}]}}
          */
          // we see these from us or some other emby client causes changes (seems like during playback)
          CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(m_address);
          if (client && client->GetPresence())
          {
            if (msgData.isArray())
            {
              const auto userDataList = msgData[NotificationUserDataChangedUserDataList];
              for (auto userData = userDataList.begin_array(); userData != userDataList.end_array(); ++userData)
              {
                if (!userData->isObject() || !userData->isMember(NotificationUserDataChangedUserDataItemId))
                  continue;

                const std::string itemId = (*userData)[NotificationUserDataChangedUserDataItemId].asString();
                client->UpdateViewItem(itemId);
              }
            }
            else
            {
              client->UpdateViewItem(msgData[NotificationUserDataChangedUserDataItemId].asString());
            }
          }
        }
      });
  }

  if (m_websocket)
  {
    m_websocket->close();
    SAFE_DELETE(m_websocket);
  }
  m_stop = true;
}
