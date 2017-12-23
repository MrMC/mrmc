#pragma once
/*
 *      Copyright (C) 2017 Team MrMC
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
#include "FileItem.h"

class CCloudUtils
{
public:
  static CCloudUtils &GetInstance();
  static void        ParseAuth2();
  static std::string GetGoogleAppKey();
  static std::string GetDropboxAppKey();
  static std::string GetDropboxCSRF();
  static bool        AuthorizeCloud(std::string service, std::string authCode);
  static std::string GetAccessToken(std::string service);
  bool               GetURL(CFileItem &item);
private:
  static std::string GenerateRandom16Byte();
  static void        CheckGoogleTokenValidity();
  static bool        RefreshGoogleToken();
  
  static std::string m_dropboxAppID;
  static std::string m_dropboxAppSecret;
  static std::string m_dropboxAccessToken;
  static std::string m_googleAppID;
  static std::string m_googleAppSecret;
  static std::string m_googleAccessToken;
  static std::string m_googleRefreshToken;
};
