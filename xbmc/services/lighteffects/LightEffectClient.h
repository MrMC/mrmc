#pragma once
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

#include <string>
#include <vector>
#include "network/TCPClient.h"

class CLightEffectClient
{
public:
  CLightEffectClient();

  bool Connect(const char *ip, int port, int timeout);
  int  SetPriority(int prio);
  bool SetOption(const char *option);
  void SetScanRange(int width, int height);
  void SetPixel(const int rgb[], int x, int y);
  int  SendLights(bool sync);
  void SendLights(const int rgb[], bool sync);
  
private:
  struct CLight
  {
    int   count = 0;
    float rgb[3] = {0, 0, 0};
    float hscan[2] = {-1.0f, -1.0f};
    float vscan[2] = {-1.0f, -1.0f};
    int   hscanscaled[2] = {0, 0};
    int   vscanscaled[2] = {0, 0};
    std::string name = "";
  };

  std::string  ReadReply();
  bool         ParseGetLights(std::string &message);
  bool         ParseWord(std::string &message, std::string wordtocmp);
  bool         GetWord(std::string &data, std::string &word);
  void         ConvertLocale(std::string &strfloat);
  void         AddPixelToLight(CLight &light, const int rgb[]);
  void         GetRGBFromLight(CLight &light, float rgb[]);

  CTCPClient   m_socket;

  float        m_speed;
  int          m_threshold;
  bool         m_interpolation;
  float        m_value;
  float        m_saturation;
  std::vector <CLight> m_lights;
};
