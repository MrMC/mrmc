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

#include "LightEffectClient.h"
#include "utils/StringUtils.h"
#include <stdlib.h>
#include <sstream>
#include <cmath>
#include <cstdlib>

// replace these when we hit c++17
static inline int ClampValue(int value, int min, int max)
{
  return std::min(std::max(value, min), max);
}
static inline float ClampValue(float value, float min, float max)
{
  return std::min(std::max(value, min), max);
}

CLightEffectClient::CLightEffectClient()
: m_speed(100.0f)
, m_threshold(0)
, m_interpolation(false)
, m_value(1.0f)
, m_saturation(1.0f)
{
}

bool CLightEffectClient::Connect(const char *ip, int port, int timeout)
{
  if (m_socket.Open(ip, port, timeout, true) != CTCPClient::SUCCESS)
    return false;

  const char hello[] = "hello\n";
  if (m_socket.Write(hello, strlen(hello)) != CTCPClient::SUCCESS)
    return false;

  if (ReadReply() != hello)
    return false;

  const char get_version[] = "get version\n";
  if (m_socket.Write(get_version, strlen(get_version)) != CTCPClient::SUCCESS)
    return false;

  if (ReadReply() != "version 5\n")
    return false;

  const char get_lights[] = "get lights\n";
  if (m_socket.Write(get_lights, strlen(get_lights)) != CTCPClient::SUCCESS)
    return false;

  std::string reply = ReadReply();
  if (!ParseGetLights(reply))
    return false;

  return true;
}

int CLightEffectClient::SetPriority(int prio)
{
  std::string data = StringUtils::Format("set priority %i\n", prio);
  return m_socket.Write(data.c_str(), data.length());
}

bool CLightEffectClient::SetOption(const char *option)
{
  std::string data;
  for (size_t i = 0; i < m_lights.size(); ++i)
  {
    std::string strname;
    std::string stroption = option;
    if (!GetWord(stroption, strname))
      return false;

    bool send = false;
    if (strname == "interpolation")
    {
      // eat the blank space
      StringUtils::Replace(stroption, " ", "");
      m_interpolation = (stroption == "1");
      send = true;
    }
    else
    {
     float value = strtod(stroption.c_str(), NULL);
      if (strname == "speed")
      {
        m_speed = value;
        m_speed = ClampValue(m_speed, 0.0f, 100.0f);
        send = true;
      }
      else if (strname == "threshold")
      {
        m_threshold = value;
        m_threshold = ClampValue(m_threshold, 0, 255);
      }
      else if (strname == "value")
      {
        m_value = value;
        m_value = std::max(m_value, 0.0f);
      }
      else if (strname == "saturation")
      {
        m_saturation = value;
        m_saturation = std::max(m_saturation, 0.0f);
      }
    }

    if (send)
      data += StringUtils::Format("set light %s %s\n", m_lights[i].name.c_str(), option);
  }

  if (!data.empty())
  {
    if (m_socket.Write(data.c_str(), data.length()) != CTCPClient::SUCCESS)
      return false;
  }

  return true;
}

void CLightEffectClient::SetScanRange(int width, int height)
{
  float fwidth = (float)(width  - 1);
  float fheight = (float)(height  - 1);
  
  for (size_t i = 0; i < m_lights.size(); ++i)
  {
    // m_hscan, m_vscan come ParseLights when we
    // connect to a device. They are paired for each light.
    m_lights[i].hscanscaled[0] = lround(m_lights[i].hscan[0] / 100.0f * fwidth);
    m_lights[i].hscanscaled[1] = lround(m_lights[i].hscan[1] / 100.0f * fwidth);
    m_lights[i].vscanscaled[0] = lround(m_lights[i].vscan[0] / 100.0f * fheight);
    m_lights[i].vscanscaled[1] = lround(m_lights[i].vscan[1] / 100.0f * fheight);
  }
}

void CLightEffectClient::SetPixel(const int rgb[], int x, int y)
{
  for (size_t i = 0; i < m_lights.size(); ++i)
  {
    if (x >= m_lights[i].hscanscaled[0] && x <= m_lights[i].hscanscaled[1] &&
        y >= m_lights[i].vscanscaled[0] && y <= m_lights[i].vscanscaled[1])
    {
      AddPixelToLight(m_lights[i], rgb);
    }
  }
}

int CLightEffectClient::SendLights(bool sync)
{
  std::string data;

  for (size_t i = 0; i < m_lights.size(); ++i)
  {
    // convert from 0 to 255 range rgb -> 0.0 to 1.0 range rgb
    float rgb[3];
    GetRGBFromLight(m_lights[i], rgb);
    data += StringUtils::Format("set light %s rgb %f %f %f\n",
      m_lights[i].name.c_str(), rgb[0], rgb[1], rgb[2]);
  }

  if (sync)
    data += "sync\n";

  if (m_socket.Write(data.c_str(), data.length()) != CTCPClient::SUCCESS)
    return 0;

  return 1;
}

void CLightEffectClient::SendLights(const int rgb[], bool sync)
{
  for (size_t i = 0; i < m_lights.size(); ++i)
    AddPixelToLight(m_lights[i], rgb);
  SendLights(sync);
}

std::string CLightEffectClient::ReadReply()
{
  std::string data;
  if (m_socket.Read(data) == CTCPClient::SUCCESS)
    return data;

  return "error";
}

bool CLightEffectClient::ParseGetLights(std::string &message)
{
  std::string word;
  if (!ParseWord(message, "lights") || !GetWord(message, word))
    return false;

  int nrlights = strtol(word.c_str(), NULL, 10);
  if (nrlights < 1)
    return false;

  for (int i = 0; i < nrlights; ++i)
  {
    CLight light;
    if (!ParseWord(message, "light") || !GetWord(message, light.name))
      return false;

    if (!ParseWord(message, "scan"))
      return false;

    std::string scanarea;
    for (int i = 0; i < 4; ++i)
    {
      if (!GetWord(message, word))
        return false;
      
      scanarea += word + " ";
    }

    ConvertLocale(scanarea);

    if (sscanf(scanarea.c_str(), "%f %f %f %f", light.vscan, light.vscan + 1, light.hscan, light.hscan + 1) != 4)
      return false;
    
    m_lights.push_back(light);
  }    
  return true;
}

bool CLightEffectClient::ParseWord(std::string &message, std::string wordtocmp)
{
  std::string readword;
  if (!GetWord(message, readword) || readword != wordtocmp)
    return false;

  return true;
}

bool CLightEffectClient::GetWord(std::string &data, std::string &word)
{
  std::stringstream datastream(data);
  
  datastream >> word;
  if (datastream.fail())
  {
    data.clear();
    return false;
  }
  
  size_t pos = data.find(word) + word.length();
  if (pos >= data.length())
  {
    data.clear();
    return true;
  }
  
  data = data.substr(pos);
  
  datastream.clear();
  datastream.str(data);
  
  std::string end;
  datastream >> end;
  if (datastream.fail())
    data.clear();
  
  return true;
}

void CLightEffectClient::ConvertLocale(std::string &strfloat)
{
  static struct lconv *locale = localeconv();

  size_t pos = strfloat.find_first_of(",.");
  while (pos != std::string::npos)
  {
    strfloat.replace(pos, 1, 1, *locale->decimal_point);
    pos++;
    
    if (pos >= strfloat.size())
      break;
    
    pos = strfloat.find_first_of(",.", pos);
  }
}

void CLightEffectClient::AddPixelToLight(CLight &light, const int rgb[])
{
  // anything below the threshold retains
  // the default value of zero (for black)
  if (rgb[0] >= m_threshold ||
      rgb[1] >= m_threshold ||
      rgb[2] >= m_threshold)
  {
    // m_rgb is sum of all pixels as defined by SetScanRange.
    light.rgb[0] += ClampValue(rgb[0], 0, 255);
    light.rgb[1] += ClampValue(rgb[1], 0, 255);
    light.rgb[2] += ClampValue(rgb[2], 0, 255);
  }
  light.count++;
}

void CLightEffectClient::GetRGBFromLight(CLight &light, float rgb[])
{
  // SetPixel/AddPixelToLight never called, quick exit
  if (light.count == 0)
  {
    for (int i = 0; i < 3; ++i)
    {
      rgb[i] = 0.0f;
      light.rgb[i] = 0.0f;
    }
    return;
  }

  // 0 to 255 rgb convert to 0.0 to 1.0 rgb
  // this also clears the internal light rgb value to black.
  for (int i = 0; i < 3; ++i)
  {
    rgb[i] = ClampValue(light.rgb[i] / (float)light.count / 255.0f, 0.0f, 1.0f);
    light.rgb[i] = 0.0f;
  }
  light.count = 0;

  // apply value/saturation if different from default
  if (m_value != 1.0 || m_saturation != 1.0)
  {
    // convert to hsv
    float hsv[3];
    float max = std::max(std::max(rgb[0], rgb[1]), rgb[2]);
    float min = std::min(std::min(rgb[0], rgb[1]), rgb[2]);
    
    if (min == max)
    {
      hsv[0] = -1.0f;
      hsv[1] = 0.0;
      hsv[2] = min;
    }
    else
    {
      float span = max - min;
      if (max == rgb[0])
      {
        hsv[0] = 60.0f * ((rgb[1] - rgb[2]) / span) + 360.0f;
        while (hsv[0] >= 360.0f)
          hsv[0] -= 360.0f;
      }
      else if (max == rgb[1])
      {
        hsv[0] = 60.0f * ((rgb[2] - rgb[0]) / span) + 120.0f;
      }
      else if (max == rgb[2])
      {
        hsv[0] = 60.0f * ((rgb[0] - rgb[1]) / span) + 240.0f;
      }

      hsv[1] = span / max;
      hsv[2] = max;
    }

    // apply the value/saturation
    hsv[1] = ClampValue(hsv[1] * m_saturation, 0.0f, 1.0f);
    hsv[2] = ClampValue(hsv[2] * m_value,      0.0f, 1.0f);

    // convert back to rgb
    if (hsv[0] == -1.0f)
    {
      for (int i = 0; i < 3; ++i)
        rgb[i] = hsv[2];
    }
    else
    {
      int hi = (int)(hsv[0] / 60.0f) % 6;
      float f = (hsv[0] / 60.0f) - (float)(int)(hsv[0] / 60.0f);

      float s = hsv[1];
      float v = hsv[2];
      float p = v * (1.0f - s);
      float q = v * (1.0f - f * s);
      float t = v * (1.0f - (1.0f - f) * s);

      switch(hi)
      {
        case 0:
          rgb[0] = v; rgb[1] = t; rgb[2] = p;
          break;
        case 1:
          rgb[0] = q; rgb[1] = v; rgb[2] = p;
          break;
        case 2:
          rgb[0] = p; rgb[1] = v; rgb[2] = t;
          break;
        case 3:
          rgb[0] = p; rgb[1] = q; rgb[2] = v;
          break;
        case 4:
          rgb[0] = t; rgb[1] = p; rgb[2] = v;
          break;
        case 5:
          rgb[0] = v; rgb[1] = p; rgb[2] = q;
          break;
      }
    }

    // always rgb clamp calculated values
    for (int i = 0; i < 3; ++i)
      rgb[i] = ClampValue(rgb[i], 0.0f, 1.0f);
  }
}
