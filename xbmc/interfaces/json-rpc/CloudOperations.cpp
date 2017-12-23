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

#include "CloudOperations.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/Variant.h"
#include "powermanagement/PowerManager.h"
#include "filesystem/CloudUtils.h"

using namespace JSONRPC;
using namespace KODI::MESSAGING;

JSONRPC_STATUS CCloudOperations::GetDropboxPrelogin(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  std::string service = parameterObject["service"].asString();
  
  if (service == "dropbox")
  {
    result["appkey"] = CCloudUtils::GetDropboxAppKey();
    result["csrf"] = CCloudUtils::GetDropboxCSRF();
  }
  else if (service == "google")
  {
    result["appkey"] = CCloudUtils::GetGoogleAppKey();
  }

  return OK;
}

JSONRPC_STATUS CCloudOperations::CloudAuthorize(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  std::vector<std::string> info;

  std::string service = parameterObject["service"].asString();
  std::string authToken = parameterObject["auth_token"].asString();
  
  result = CCloudUtils::AuthorizeCloud(service, authToken);

  return OK;
}
