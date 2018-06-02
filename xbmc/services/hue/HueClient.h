#pragma once
/*
 *      Copyright (C) 2018 Team MrMC
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

#include <memory>
#include <vector>
#include <map>
#include <string>

#include <openssl/ssl.h>

#include "utils/Variant.h"

class CHueBridge;

class CHueLight
{
public:
  CHueLight(std::string sid, CHueBridge* bridge, CVariant state);

  bool isOn() const;
  bool setOn(bool val);

  std::pair<uint16_t, uint8_t> getColorHueSaturation();
  uint8_t getBrightness();

  bool setColorHSV(uint16_t hue, uint8_t sat, uint8_t bri, uint32_t duration);
  bool setColorRGBL(float r, float g, float b, uint8_t l, uint32_t duration);
  bool setColorXYB(float x, float y, uint8_t bri, uint32_t duration);
  bool setBrightness(uint8_t bri, uint32_t duration);

  void saveState();
  bool restoreState(uint32_t dur);

  int getMode() const;
  void setMode(int mode);

protected:
  std::string m_sid;
  int m_mode;
  CHueBridge* m_bridge;
  CVariant m_state;
  CVariant m_savstate;

  void refreshState();
};

class CHueGroup
{
public:
  CHueGroup(std::string gid, CHueBridge* bridge, CVariant state);

  bool isAnyOn();
  bool setOn(bool val);

protected:
  std::string m_gid;
  CHueBridge* m_bridge;
  CVariant m_state;
  CVariant m_lights;

  void refreshState();
};

class CHueBridge
{
  friend class CHueLight;
  friend class CHueGroup;

public:
  CHueBridge();
  CHueBridge(std::string ip, std::string mac);
  CHueBridge(std::string ip, std::string username, std::string clientkey);
  ~CHueBridge();

  static std::vector<CHueBridge> discover();

  bool pair();
  void logConfig();
  std::vector< std::pair<std::string, int> > getStreamGroupsNames();
  std::vector< std::pair<std::string, int> > getLightsNames();
  std::map<int, std::shared_ptr<CHueLight>>& getLights();
  std::shared_ptr<CHueLight> getLight(int lightid);
  std::map<int, std::shared_ptr<CHueGroup>>& getGroups();
  std::shared_ptr<CHueGroup> getGroup(int lightid);

  std::string getIp() const;
  std::string getMac() const;

  std::string getUsername() const;
  void setUsername(const std::string& value);

  std::string getClientkey() const;
  void setClientkey(const std::string& value);

  bool startStreaming(int streamgroupid);
  void stopStreaming();
  bool isStreaming();
  CVariant& getStreamingLights() { return m_streaminglights; }
  bool streamXYB(float x, float y, float B);
  bool streamRGBL(float r, float g, float b, float l);

protected:
  struct SDTLSParams
  {
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *bio;
  };
  struct SStreamPacketHeader
  {
    char protocol[9];      // Array of 9 characters{‘H’, ‘u’, ‘e’, ‘S’, ‘t’, ‘r’, ‘e’, ‘a’, ‘m’}
    char version[2];       // Streaming API version 1 byte major version (0x01), 1 byte minor version (0x00)
    uint8_t seq;           // Can be used to indicate the message sequence number. Currently it is ignored by the bridge.
    uint16_t reserved;     // Reserved, all zeros should be sent.
    uint8_t colorspace;    // 0x00 = RGB; 0x01 = XY Brightness
    uint8_t reserved2;     // Reserved, all zeros should be sent.
  };

  struct SStreamLight
  {
    uint8_t type;          // Type of device 0x00 = Light
    uint8_t id[2];         // Unique ID of light
    uint8_t val[6];        // RGB or XY+Brightness with 16 bit resolution per element
  };

  static CHueBridge* m_sslbridge;
  bool m_useStreaming;
  std::string m_ip;
  std::string m_mac;
  std::string m_username;
  std::string m_clientkey;
  CVariant m_groupsState;
  CVariant m_lightsState;
  std::map<int, std::shared_ptr<CHueLight>> m_lights;
  std::map<int, std::shared_ptr<CHueGroup>> m_groups;
  SDTLSParams m_dtls_client;
  int m_streamgroupid;
  SStreamPacketHeader m_streamheader;
  CVariant m_streaminglights;
  char* m_streamingbuffer;
  int m_streamingbuffersize;

  void refreshGroupsState();
  void refreshLightsState();
  int dtls_InitContext(SDTLSParams* params);
  int dtls_InitClient(SDTLSParams* params, const char* address);
  static unsigned int psk_client_cb(SSL* ssl, const char* hint, char* identity, unsigned int max_identity_len, unsigned char* psk, unsigned int max_psk_len);
  bool initDTLSConnection();
  bool closeDTLSConnection();
  bool checkReply(std::string id, std::string request, std::string reply);
  std::string getUrl();
  std::string getUsernameUrl();
  bool putLightStateRequest(std::string sid, const CVariant& request);
  bool putGroupStateRequest(std::string sid, const CVariant& request);
};
