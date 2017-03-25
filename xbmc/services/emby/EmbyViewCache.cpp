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

#include "EmbyViewCache.h"

#include "EmbyUtils.h"
#include "threads/SingleLock.h"
#include "utils/log.h"


CEmbyViewCache::CEmbyViewCache()
{
}

CEmbyViewCache::~CEmbyViewCache()
{
}

void CEmbyViewCache::Init(const EmbyViewContent &content)
{
  m_cache = content;
}

bool CEmbyViewCache::AppendItem(const CVariant &variant)
{
  bool exists = false;
  CSingleLock lock(m_cacheLock);
  const auto& variantItems = m_cache.items["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (variant["Id"] == (*variantItemIt)["Id"].asString())
    {
      exists = true;
      break;
    }
  }
  if (!exists)
    m_cache.items["Items"].push_back(variant);
        return true;

  return false;
}

bool CEmbyViewCache::UpdateItem(const CVariant &variant)
{
  CSingleLock lock(m_cacheLock);
  for (size_t k = 0; k < m_cache.items["Items"].size(); ++k)
  {
    if (variant["Id"] == m_cache.items["Items"][k]["Id"].asString())
    {
      m_cache.items["Items"][k] = variant;
      return true;
    }
  }
  return false;
}

bool CEmbyViewCache::RemoveItem(const CVariant &variant)
{
  std::string itemId = variant["Id"].asString();
  CSingleLock lock(m_cacheLock);
  for (size_t k = 0; k < m_cache.items["Items"].size(); ++k)
  {
    if (itemId == m_cache.items["Items"][k]["Id"].asString())
    {
      m_cache.items["Items"].erase(k);
      return true;
    }
  }
  return false;
}

const EmbyViewInfo CEmbyViewCache::GetInfo() const
{
  CSingleLock lock(m_cacheLock);
  EmbyViewInfo info;
  info.id = m_cache.id;
  info.name = m_cache.name;
  info.prefix = m_cache.prefix;
  info.mediaType = m_cache.mediaType;
  return info;
}

bool CEmbyViewCache::SetWatched(const std::string id, int playcount, double resumetime)
{
  CSingleLock lock(m_cacheLock);
  for (size_t k = 0; k < m_cache.items["Items"].size(); ++k)
  {
    if (id == m_cache.items["Items"][k]["Id"].asString())
    {
      // do it the long way or the value will not get updated
      m_cache.items["Items"][k]["UserData"]["Played"] = true;
      m_cache.items["Items"][k]["UserData"]["PlayCount"] = playcount;
      m_cache.items["Items"][k]["UserData"]["PlaybackPositionTicks"] = CEmbyUtils::SecondsToTicks(resumetime);
      return true;
    }
  }
  return false;
}

bool CEmbyViewCache::SetUnWatched(const std::string id)
{
  CSingleLock lock(m_cacheLock);
  for (size_t k = 0; k < m_cache.items["Items"].size(); ++k)
  {
    if (id == m_cache.items["Items"][k]["Id"].asString())
    {
      // do it the long way or the value will not get updated
      m_cache.items["Items"][k]["UserData"]["Played"] = false;
      m_cache.items["Items"][k]["UserData"]["PlayCount"] = 0;
      m_cache.items["Items"][k]["UserData"]["PlaybackPositionTicks"] = 0;
      return true;
    }
  }
  return false;
}
