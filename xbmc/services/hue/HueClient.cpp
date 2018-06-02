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

#include "HueClient.h"
#include "HueUtils.h"

#include <chrono>
#include <arpa/inet.h>
#include <regex>

#include <openssl/err.h>

#include "contrib/libmicrossdp/ssdp.h"
#include "filesystem/CurlFile.h"
#include "utils/StringUtils.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogProgress.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/StringUtils.h"
#include "guilib/LocalizeStrings.h"
#include "utils/log.h"

CHueBridge* CHueBridge::m_sslbridge = nullptr;

CHueBridge::CHueBridge()
  : m_useStreaming(false)
  , m_streamingbuffer(nullptr)
  , m_streamingbuffersize(0)
{
}

CHueBridge::CHueBridge(std::string ip, std::string mac)
  : m_useStreaming(false)
  , m_ip(ip)
  , m_mac(mac)
  , m_streamingbuffer(nullptr)
  , m_streamingbuffersize(0)
{
}

CHueBridge::CHueBridge(std::string ip, std::string username, std::string clientkey)
  : m_useStreaming(false)
  , m_ip(ip)
  , m_username(username)
  , m_clientkey(clientkey)
  , m_streamingbuffer(nullptr)
  , m_streamingbuffersize(0)
{
  logConfig();
  refreshLightsState();
  refreshGroupsState();
  m_sslbridge = this;
}

CHueBridge::~CHueBridge()
{
}

std::vector<CHueBridge> CHueBridge::discover()
{
  XFILE::CCurlFile curlf;

  // SSDP
  std::vector<CHueBridge> bridges;
  struct upnp_device devices[SSDP_MAX];
  int found = ssdp_discovery(AF_INET, SSDP_CAT_BASIC, devices);
  std::regex serialRegex("<serialNumber>(\\w+)</serialNumber>");

  // filter out devices
  for (int i=0;  i<found; ++i)
  {
    if (strstr(devices[i].response, "IpBridge"))
    {
      std::string location(devices[i].location);
      size_t start = location.find("//") + 2;
      size_t length = location.find(":", start) - start;
      std::string ip = location.substr(start, length);
      if (ip.empty())
        continue;

      if(std::find_if(bridges.begin(), bridges.end(), [&](const CHueBridge &item){return item.getIp() == ip;}) != bridges.end())
        continue;

      std::string response;
      curlf.Get("http://" + ip + "/description.xml", response);

      std::smatch matchResult;
      if (std::regex_search(response, matchResult, serialRegex))
        bridges.push_back(CHueBridge(ip, matchResult[1].str()));
    }
  }

  if (!bridges.empty())
    return bridges;

  //N-UPNP
  std::string sanswer;
  CVariant tmpV;
  if (!curlf.Get("https://www.meethue.com/api/nupnp", sanswer))
    return bridges;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return bridges;
  if (!tmpV.isArray())
    return bridges;

  for (CVariant::iterator_array it = tmpV.begin_array(); it != tmpV.end_array(); ++it)
  {
    if (!it->isObject())
      continue;

    if (!it->isMember("internalipaddress"))
      continue;

    std::string ip = (*it)["internalipaddress"].asString();
    std::string mac;
    if (!it->isMember("macaddress"))
      mac = (*it)["macaddress"].asString();

    bridges.push_back(CHueBridge(ip, mac));
  }

  return bridges;
}

bool CHueBridge::pair()
{
  bool ret = true;

  CGUIDialogProgress *waitPinDialog;
  waitPinDialog = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
  waitPinDialog->SetHeading(g_localizeStrings.Get(14201));
  waitPinDialog->SetLine(0, g_localizeStrings.Get(14204));

  waitPinDialog->Open();
  waitPinDialog->ShowProgressBar(true);

  bool hasStreaming = false;
  XFILE::CCurlFile curlf;
  std::string sanswer;
  CVariant answer;

  if (!curlf.Get(getUrl() + "/config", sanswer))
    return false;
  if (!CJSONVariantParser::Parse(sanswer, answer))
    return false;

  std::string ver = answer["apiversion"].asString();
  std::vector<std::string> vec = StringUtils::Split(ver, '.');
  int iver = 0;
  for (unsigned int i=1; i<=3; ++i)
  {
    if (vec.size() >= i)
    {
      iver += std::stoi(vec[i-1]) * pow(100, 3-i);
    }
  }
  if (iver > 12200)
    hasStreaming = true;

  CVariant request;
  request["devicetype"] = "MrMC#User";
  if (hasStreaming)
    request["generateclientkey"] = true;

  std::string srequest;
  CJSONVariantWriter::Write(request, srequest, true);

  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lastCheck;
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(35) && !waitPinDialog->IsCanceled())
  {
    waitPinDialog->SetPercentage(int(double(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count()) / 35.0 * 100.0));

    if (std::chrono::steady_clock::now() - lastCheck > std::chrono::seconds(1))
    {
      lastCheck = std::chrono::steady_clock::now();
      XFILE::CCurlFile curlf;
      if (!curlf.Post(getUrl(), srequest, sanswer))
        break;
      answer.clear();
      if (!CJSONVariantParser::Parse(sanswer, answer))
        break;

      if (!answer[0]["success"].isNull())
      {
        // [{"success":{"username": "<username>"}}]
        m_username = answer[0]["success"]["username"].asString();
        if (hasStreaming)
          m_clientkey = answer[0]["success"]["clientkey"].asString();;
        CLog::Log(LOGINFO, "Hue - Pairing complete: u: %s; k:%s", m_username.c_str(), m_clientkey.c_str());
        break;
      }
      if (!answer[0]["error"].isNull())
      {
        CLog::Log(LOGDEBUG, "Hue - Waiting for link button press");
      }
      waitPinDialog->Progress();
    }
  }
  if (std::chrono::steady_clock::now() - start > std::chrono::seconds(35) || waitPinDialog->IsCanceled())
    ret = false;
  waitPinDialog->Close();

  return ret;
}

void CHueBridge::logConfig()
{
  if (m_username.empty())
    return;

  std::string sanswer;
  CVariant tmpV;
  XFILE::CCurlFile curlf;

  if (!curlf.Get(getUsernameUrl() + "/config", sanswer))
    return;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return;

  CLog::Log(LOGINFO, "Hue - Bridge connection: model (%s) sw(%s) api(%s)",
      tmpV["modelid"].asString().c_str(),
      tmpV["swversion"].asString().c_str(),
      tmpV["apiversion"].asString().c_str()
      );
}

void CHueBridge::refreshGroupsState()
{
  if (m_username.empty())
    return;

  std::string sanswer;
  CVariant tmpV;
  XFILE::CCurlFile curlf;

  if (!curlf.Get(getUsernameUrl() + "/groups", sanswer))
    return;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return;

  if (tmpV.isObject())
    m_groupsState = tmpV;
  else
    CLog::Log(LOGERROR, "Hue - Error refreshing groups state: %s, ", sanswer.c_str());

  m_groups.clear();
  for (CVariant::iterator_map it = m_groupsState.begin_map(); it != m_groupsState.end_map(); ++it)
  {
    int id = std::stoi(it->first);
    m_groups[id].reset(new CHueGroup(it->first, this, it->second["state"]));
  }
}

void CHueBridge::refreshLightsState()
{
  if (m_username.empty())
    return;

  std::string sanswer;
  CVariant tmpV;
  XFILE::CCurlFile curlf;

  if (!curlf.Get(getUsernameUrl() + "/lights", sanswer))
    return;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return;

  if (tmpV.isObject())
    m_lightsState = tmpV;
  else
    CLog::Log(LOGERROR, "Hue - Error refreshing lights state: %s", sanswer.c_str());

  m_lights.clear();
  for (CVariant::iterator_map it = m_lightsState.begin_map(); it != m_lightsState.end_map(); ++it)
  {
    int id = std::stoi(it->first);
    m_lights[id].reset(new CHueLight(it->first, this, it->second["state"]));
  }
}

bool CHueBridge::checkReply(std::string id, std::string request, std::string reply)
{
  CVariant answer;
  CJSONVariantParser::Parse(reply, answer);

  if (!answer[0]["success"].isObject())
  {
    CLog::Log(LOGERROR, "Hue - Error: id:%s, request: %s, reply: %s", id.c_str(), request.c_str(), reply.c_str());
    return false;
  }
  return true;
}

static inline int cval(char c)
{
  if (c>='a') return c-'a'+0x0a;
  if (c>='A') return c-'A'+0x0a;
  return c-'0';
}

static int hex2bin(const char *str, unsigned char *out)
{
  int i;
  for(i = 0; str[i] && str[i+1]; i+=2)
  {
    if (!isxdigit(str[i])&& !isxdigit(str[i+1]))
      return -1;
    out[i/2] = (cval(str[i])<<4) + cval(str[i+1]);
  }
  return i/2;
}

unsigned int CHueBridge::psk_client_cb(SSL *ssl, const char *hint, char *identity,
                                  unsigned int max_identity_len, unsigned char *psk, unsigned int max_psk_len)
{
  int ret;

  (void)(ssl); //unused; prevent gcc warning;

  if (!hint)
    CLog::Log(LOGWARNING, "NULL received PSK identity hint, continuing anyway\n");
  else
    CLog::Log(LOGINFO, "Received PSK identity hint '%s'\n", hint);

  ret = snprintf(identity, max_identity_len, "%s", m_sslbridge->getUsername().c_str());
  if (ret < 0 || (unsigned int)ret > max_identity_len)
  {
    CLog::Log(LOGERROR, "Error, psk_identify too long\n");
    return 0;
  }

  std::string psk_key = m_sslbridge->getClientkey();
  if (psk_key.size() >= (max_psk_len*2))
  {
    CLog::Log(LOGERROR, "Error, psk_key too long\n");
    return 0;
  }

  /* convert the PSK key to binary */
  ret = hex2bin(psk_key.c_str(), psk);
  if (ret<=0)
  {
    CLog::Log(LOGERROR, "Error, Could not convert PSK key '%s' to binary key\n", psk_key.c_str());
    return 0;
  }
  return ret;
}


int CHueBridge::dtls_InitContext(SDTLSParams* params)
{
  int result = 0;

  // Register the error strings for libcrypto & libssl
  SSL_load_error_strings ();
  // Register the available ciphers and digests
  SSL_library_init ();

  // Create a new context using DTLS
  params->ctx = SSL_CTX_new(DTLSv1_2_client_method());
  if (!params->ctx)
  {
    ERR_print_errors_fp (stdout);
    CLog::Log(LOGERROR, "Hue %s - Error: cannot create SSL_CTX", __FUNCTION__);
    return -1;
  }

  // Set our supported ciphers
  result = SSL_CTX_set_cipher_list(params->ctx, "PSK-AES128-CBC-SHA");
  if (result != 1)
  {
    ERR_print_errors_fp (stdout);
    CLog::Log(LOGERROR, "Hue %s - Error: cannot set the cipher list", __FUNCTION__);
    return -2;
  }

  SSL_CTX_set_psk_client_callback(params->ctx, CHueBridge::psk_client_cb);

  return 0;
}

int CHueBridge::dtls_InitClient(SDTLSParams* params, const char *address)
{
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(2100);
  int sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    CLog::Log(LOGERROR, "%s - error creating socket", __FUNCTION__);
    return -1;
  }

  params->bio = BIO_new_dgram(sock, BIO_NOCLOSE);
  if (!params->bio)
  {
    ERR_print_errors_fp (stdout);
    CLog::Log(LOGERROR, "%s - error creating BIO", __FUNCTION__);
    return -1;
  }

  struct sockaddr_in d;
  memset(&d, 0, sizeof(d));
  d.sin_family = AF_INET;
  d.sin_addr.s_addr = inet_addr(address);
  d.sin_port = htons(2100);

  BIO_dgram_set_peer(params->bio, &d);

  params->ssl = SSL_new(params->ctx);
  SSL_set_bio(params->ssl, params->bio, params->bio);
  SSL_set_connect_state(params->ssl);

  return 0;
}

bool CHueBridge::initDTLSConnection()
{
  if (!m_sslbridge)
    return false;

  if (m_clientkey.empty())
    return false;

  // Initialize the DTLS context from the keystore and then create the server
  // SSL state.
  if (dtls_InitContext(&m_dtls_client) < 0)
      return false;

  if (dtls_InitClient(&m_dtls_client, m_ip.c_str()) < 0)
    return false;

  // Attempt to connect to the server and complete the handshake.
  int result = SSL_connect(m_dtls_client.ssl);
  if (result != 1)
  {
      CLog::Log(LOGERROR, "%s - Unable to connect to the DTLS server.", __FUNCTION__);
      return false;
  }
  return true;
}

bool CHueBridge::closeDTLSConnection()
{
  if (m_dtls_client.ssl == nullptr)
    return false;

  SSL_shutdown(m_dtls_client.ssl);
  close(SSL_get_fd(m_dtls_client.ssl));

  if (m_dtls_client.ssl != nullptr)
    SSL_free(m_dtls_client.ssl);
  if (m_dtls_client.ctx != nullptr)
    SSL_CTX_free(m_dtls_client.ctx);

  return true;
}

std::vector<std::pair<std::string, int> > CHueBridge::getStreamGroupsNames()
{
  std::vector<std::pair<std::string, int>> result;

  for (CVariant::iterator_map it = m_groupsState.begin_map(); it != m_groupsState.end_map(); ++it)
  {
    if (m_groupsState[it->first]["type"] == "Entertainment")
    {
      int id = std::stoi(it->first);
      std::string name = m_groupsState[it->first]["name"].asString();
      result.push_back(std::make_pair(name, id));
    }
  }

  return result;
}

std::vector<std::pair<std::string, int> > CHueBridge::getLightsNames()
{
  std::vector<std::pair<std::string, int>> result;

  for (CVariant::iterator_map it = m_lightsState.begin_map(); it != m_lightsState.end_map(); ++it)
  {
    int id = std::stoi(it->first);
    std::string name = m_lightsState[it->first]["name"].asString();
    result.push_back(std::make_pair(name, id));
  }

  return result;
}

std::map<int, std::shared_ptr<CHueLight>>& CHueBridge::getLights()
{
  return m_lights;
}

std::shared_ptr<CHueLight> CHueBridge::getLight(int lightid)
{
  return m_lights[lightid];
}

std::map<int, std::shared_ptr<CHueGroup>>& CHueBridge::getGroups()
{
  return m_groups;
}

std::shared_ptr<CHueGroup> CHueBridge::getGroup(int gid)
{
  return m_groups[gid];
}

std::string CHueBridge::getIp() const
{
  return m_ip;
}

std::string CHueBridge::getMac() const
{
  return m_mac;
}

std::string CHueBridge::getUsername() const
{
  return m_username;
}

void CHueBridge::setUsername(const std::string& value)
{
  m_username = value;
}

std::string CHueBridge::getClientkey() const
{
  return m_clientkey;
}

void CHueBridge::setClientkey(const std::string& value)
{
  m_clientkey = value;
}

bool CHueBridge::startStreaming(int streamgroupid)
{
  m_streaminglights = m_groupsState[std::to_string((streamgroupid))]["lights"];
  if (m_streaminglights.isNull())
    return false;

  m_streamgroupid = streamgroupid;

  CVariant request;
  CVariant streamObj(CVariant::VariantTypeObject);

  streamObj["active"] = true;
  request["stream"] = streamObj;

  std::string srequest, sanswer;
  CJSONVariantWriter::Write(request, srequest, true);

  XFILE::CCurlFile curlf;
  if (!curlf.Put(getUsernameUrl() + "/groups/" + std::to_string(m_streamgroupid), srequest, sanswer))
  {
    CLog::Log(LOGERROR, "Hue - Error in %s", __FUNCTION__);
    return false;
  }

  m_useStreaming = initDTLSConnection();
  if (!m_useStreaming)
    return false;

  char header[] = {'H', 'u', 'e', 'S', 't', 'r', 'e', 'a', 'm', //protocol
                   0x01, 0x00, //version 1.0
                   0x01, //sequence number 1
                   0x00, 0x00, //reserved
                   0x01, //color mode XYB
                   0x00, //reserved
                  };

  m_streamingbuffersize = sizeof (SStreamPacketHeader) + m_streaminglights.size() * sizeof(SStreamLight);
  m_streamingbuffer = (char *)malloc(m_streamingbuffersize);
  memcpy(m_streamingbuffer, header, sizeof (SStreamPacketHeader));

  CLog::Log(LOGINFO, "Hue - Ready to stream on group %d", m_streamgroupid);

  return true;
}

void CHueBridge::stopStreaming()
{
  if (!m_useStreaming)
    return;

  closeDTLSConnection();
  m_useStreaming = false;
  free(m_streamingbuffer);
  m_streamingbuffer = nullptr;

  CVariant request;
  CVariant streamObj(CVariant::VariantTypeObject);

  streamObj["active"] = false;
  request["stream"] = streamObj;

  std::string srequest, sanswer;
  CJSONVariantWriter::Write(request, srequest, true);

  XFILE::CCurlFile curlf;
  if (!curlf.Put(getUsernameUrl() + "/groups/" + std::to_string(m_streamgroupid), srequest, sanswer))
  {
    CLog::Log(LOGERROR, "Hue - Error in %s", __FUNCTION__);
  }
}

bool CHueBridge::isStreaming()
{
  return m_useStreaming;
}

bool CHueBridge::streamXYB(float x, float y, float B)
{
  if (!m_useStreaming)
    return false;

  uint16_t ix = uint16_t(x * 0xffff);
  uint16_t iy = uint16_t(y * 0xffff);
  uint16_t ib = uint16_t(B * 0xffff);

  int i=0;
  for (CVariant::iterator_array it = m_streaminglights.begin_array(); it != m_streaminglights.end_array(); ++it)
  {
    SStreamLight ltpck;
    ltpck.type = 0x00;
    ltpck.id[0] = 0x00;
    ltpck.id[1] = uint8_t(std::stoi(it->asString()));
    ltpck.val[0] = static_cast<uint8_t>((ix >> 8) & 0xff);
    ltpck.val[1] = static_cast<uint8_t>(ix & 0xff);
    ltpck.val[2] = static_cast<uint8_t>((iy >> 8) & 0xff);
    ltpck.val[3] = static_cast<uint8_t>(iy & 0xff);
    ltpck.val[4] = static_cast<uint8_t>((ib >> 8) & 0xff);
    ltpck.val[5] = static_cast<uint8_t>(ib & 0xff);

    memcpy(&m_streamingbuffer[sizeof(SStreamPacketHeader) + i*sizeof(SStreamLight)], &ltpck, sizeof (SStreamLight));
    ++i;
  }

  int written = SSL_write(m_dtls_client.ssl, m_streamingbuffer, m_streamingbuffersize);
  if (written != m_streamingbuffersize)
  {
    CLog::Log(LOGERROR, "Hue - Error writing stream (%d)", written);
    return false;
  }
  return true;
}

bool CHueBridge::streamRGBL(float r, float g, float b, float l)
{
  if (!m_useStreaming)
    return false;

  float x, y;
  CHueUtils::rgb2xy(r, g, b, x, y);
  return streamXYB(x, y, l);
}

std::string CHueBridge::getUrl()
{
  return ("http://" + m_ip + "/api");
}

std::string CHueBridge::getUsernameUrl()
{
  return (getUrl() + "/" + m_username);
}

bool CHueBridge::putLightStateRequest(std::string sid, const CVariant& request)
{
  std::string srequest, sanswer;
  CJSONVariantWriter::Write(request, srequest, true);

  XFILE::CCurlFile curlf;
  if (!curlf.Put(getUsernameUrl() + "/lights/" + sid + "/state", srequest, sanswer))
  {
    CLog::Log(LOGERROR, "Hue - Error in %s: %s", __FUNCTION__, sanswer.c_str());
    return false;
  }

  return checkReply(sid, srequest, sanswer);
}

bool CHueBridge::putGroupStateRequest(std::string sid, const CVariant& request)
{
  std::string srequest, sanswer;
  CJSONVariantWriter::Write(request, srequest, true);

  XFILE::CCurlFile curlf;
  if (!curlf.Put(getUsernameUrl() + "/groups/" + sid + "/action", srequest, sanswer))
  {
    CLog::Log(LOGERROR, "Hue - Error in %s: %s", __FUNCTION__, sanswer.c_str());
    return false;
  }

  return checkReply(sid, srequest, sanswer);
}

CHueGroup::CHueGroup(std::string gid, CHueBridge* bridge, CVariant state)
  : m_gid(gid)
  , m_bridge(bridge)
  , m_state(state)
{
}

bool CHueGroup::isAnyOn()
{
  return m_state["any_on"].asBoolean();
}

bool CHueGroup::setOn(bool val)
{
  CVariant request;
  request["on"] = val;

  bool ret;
  if((ret = m_bridge->putGroupStateRequest(m_gid, request)))
  {
    CLog::Log(LOGINFO, "Hue - Group (%s) on (%s)", val ? "true" : "false");
    m_state["on"] = val;
  }
  return ret;
}

void CHueGroup::refreshState()
{
  if (m_bridge->getUsername().empty())
    return;

  std::string sanswer;
  CVariant tmpV;
  XFILE::CCurlFile curlf;

  if (!curlf.Get(m_bridge->getUsernameUrl() + "/groups/" + m_gid, sanswer))
    return;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return;

  if (tmpV.isObject())
  {
    m_state = tmpV["state"];
    m_lights = tmpV["lights"];
  }
  else
    CLog::Log(LOGERROR, "Hue: Error refreshing group state (%s): %s", m_gid.c_str(), sanswer.c_str());
}

CHueLight::CHueLight(std::string sid, CHueBridge* bridge, CVariant state)
  : m_sid(sid)
  , m_mode(0)
  , m_bridge(bridge)
  , m_state(state)
{
}

bool CHueLight::isOn() const
{
  return m_state["on"].asBoolean();
}

bool CHueLight::setOn(bool val)
{
  CVariant request;
  request["on"] = val;

  bool ret;
  if((ret = m_bridge->putLightStateRequest(m_sid, request)))
  {
    CLog::Log(LOGINFO, "Hue - Light (%s) on (%s)", val ? "true" : "false");
    m_state["on"] = val;
  }
  return ret;
}

std::pair<uint16_t, uint8_t> CHueLight::getColorHueSaturation()
{
  return std::make_pair(m_state["hue"].asUnsignedInteger(), m_state["sat"].asUnsignedInteger());
}

uint8_t CHueLight::getBrightness()
{
  return m_state["bri"].asUnsignedInteger();
}

bool CHueLight::setColorHSV(uint16_t hue, uint8_t sat, uint8_t bri, uint32_t duration)
{
  CVariant request;
  request["on"] = (bri > 0 ? true : false);
  request["hue"] = hue;
  request["sat"] = sat;
  request["bri"] = bri;
  request["transitiontime"] = int(duration/100);

  bool ret;
  if((ret = m_bridge->putLightStateRequest(m_sid, request)))
  {
    m_state["hue"] = hue;
    m_state["sat"] = sat;
    m_state["bri"] = bri;
  }
  return ret;
}

bool CHueLight::setColorXYB(float x, float y, uint8_t bri, uint32_t duration)
{
  CVariant request;
  CVariant xyArray(CVariant::VariantTypeArray);

  xyArray.push_back(x);
  xyArray.push_back(y);

  request["on"] = (bri > 0 ? true : false);
  request["xy"] = xyArray;
  request["bri"] = bri;
  request["transitiontime"] = int(duration/100);

  bool ret;
  if((ret = m_bridge->putLightStateRequest(m_sid, request)))
  {
    m_state["xy"][0] = x;
    m_state["xy"][1] = y;
    m_state["bri"] = bri;
  }
  return ret;
}

bool CHueLight::setColorRGBL(float r, float g, float b, uint8_t l, uint32_t duration)
{
  float x, y;
  CHueUtils::rgb2xy(r, g, b, x, y);
  return setColorXYB(x, y, l, duration);
}

bool CHueLight::setBrightness(uint8_t bri, uint32_t duration)
{
  CVariant request;
  request["on"] = (bri > 0 ? true : false);
  request["bri"] = bri;
  request["transitiontime"] = int(duration/100);

  bool ret;
  if((ret = m_bridge->putLightStateRequest(m_sid, request)))
  {
    m_state["bri"] = bri;
  }
  return ret;
}

void CHueLight::saveState()
{
  m_savstate["on"] = m_state["on"];
  m_savstate["bri"] = m_state["bri"];
  m_savstate["hue"] = m_state["hue"];
  m_savstate["sat"] = m_state["sat"];

  CLog::Log(LOGINFO, "Hue - Light (%s) savestate: on (%s) h(%lld) s(%lld) b(%lld)", m_sid.c_str(),
      m_savstate["on"].asBoolean() ? "true" : "false",
      m_savstate["hue"].asInteger(),
      m_savstate["sat"].asInteger(),
      m_savstate["bri"].asInteger()
      );
}

bool CHueLight::restoreState(uint32_t dur)
{
  CVariant request;
  request["transitiontime"] = int(dur/100);
  request["hue"] = m_savstate["hue"];
  request["sat"] =  m_savstate["sat"];
  request["bri"] =  m_savstate["bri"];
  request["on"] = m_savstate["on"];

  bool ret;
  if((ret = m_bridge->putLightStateRequest(m_sid, request)))
  {
    m_state["on"] = m_savstate["on"];
    m_state["bri"] = m_savstate["bri"];
    m_state["hue"] = m_savstate["hue"];
    m_state["sat"] = m_savstate["sat"];

    CLog::Log(LOGINFO, "Hue - Light (%s) restorestate: on (%s) h(%lld) s(%lld) b(%lld)", m_sid.c_str(),
        m_savstate["on"].asBoolean() ? "true" : "false",
        m_savstate["hue"].asInteger(),
        m_savstate["sat"].asInteger(),
        m_savstate["bri"].asInteger()
        );
  }
  return ret;
}

int CHueLight::getMode() const
{
  return m_mode;
}

void CHueLight::setMode(int mode)
{
  m_mode = mode;
}

void CHueLight::refreshState()
{
  if (m_bridge->getUsername().empty())
    return;

  std::string sanswer;
  CVariant tmpV;
  XFILE::CCurlFile curlf;

  if (!curlf.Get(m_bridge->getUsernameUrl() + "/lights/" + m_sid, sanswer))
    return;
  if (!CJSONVariantParser::Parse(sanswer, tmpV))
    return;

  if (tmpV.isObject())
    m_state = tmpV["state"];
  else
    CLog::Log(LOGERROR, "Hue: Error refreshing light state (%s): %s", m_sid.c_str(), sanswer.c_str());
}
