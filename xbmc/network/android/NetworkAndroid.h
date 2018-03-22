#pragma once
/*
 *      Copyright (C) 2016 Christian Browet
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
#include "threads/CriticalSection.h"

#include <androidjni/Network.h>
#include <androidjni/NetworkInfo.h>
#include <androidjni/LinkProperties.h>
#include <androidjni/RouteInfo.h>
#include <androidjni/NetworkInterface.h>

class CNetworkAndroid;

class CNetworkInterfaceAndroid : public CNetworkInterface
{
public:
  CNetworkInterfaceAndroid(CJNINetwork network, const CJNILinkProperties& lp, const CJNINetworkInterface& intf);
  std::vector<std::string> GetNameServers();
  
  // CNetworkInterface interface
public:
  virtual std::string& GetName();
  virtual bool IsEnabled();
  virtual bool IsConnected();
  virtual bool IsWireless();
  virtual std::string GetMacAddress();
  virtual void GetMacAddressRaw(char rawMac[6]);
  virtual bool GetHostMacAddress(in_addr_t host, std::string& mac);
  virtual std::string GetCurrentIPAddress();
  virtual std::string GetCurrentNetmask();
  virtual std::string GetCurrentDefaultGateway();
  virtual std::string GetCurrentWirelessEssId();
  virtual std::vector<NetworkAccessPoint> GetAccessPoints();
  virtual void GetSettings(NetworkAssignment& assignment, std::string& ipAddress, std::string& networkMask, std::string& defaultGateway, std::string& essId, std::string& key, EncMode& encryptionMode);
  virtual void SetSettings(NetworkAssignment& assignment, std::string& ipAddress, std::string& networkMask, std::string& defaultGateway, std::string& essId, std::string& key, EncMode& encryptionMode);
  
protected:
  std::string m_name;
  CJNINetwork m_network;
  CJNILinkProperties m_lp;
  CJNINetworkInterface m_intf;
};


class CNetworkAndroid : public CNetwork
{
friend class CXBMCApp;

public:
  CNetworkAndroid();
 ~CNetworkAndroid();

  // CNetwork interface
public:
  virtual bool GetHostName(std::string& hostname);
  virtual std::vector<CNetworkInterface*>& GetInterfaceList();
  virtual CNetworkInterface* GetFirstConnectedInterface();
  virtual bool PingHost(in_addr_t remote_ip, unsigned int timeout_ms = 2000);
  virtual std::vector<std::string> GetNameServers();
  virtual void SetNameServers(const std::vector<std::string>& nameServers);
  
protected:
  void RetrieveInterfaces();
  std::vector<CNetworkInterface*> m_interfaces;  
  std::vector<CNetworkInterface*> m_oldInterfaces;
  CCriticalSection m_refreshMutex;
};

