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
#include <vector>

#include "utils/Variant.h"
#include "threads/CriticalSection.h"

typedef struct EmbyViewInfo
{
  std::string id;
  std::string name;
  std::string prefix;
  std::string mediaType;
} EmbyViewInfo;

typedef struct EmbyViewContent
{
  std::string id;
  std::string name;
  std::string etag;
  std::string prefix;
  std::string serverId;
  std::string mediaType;
  std::string iconId;
  CVariant items;
} EmbyViewContent;

class CEmbyViewCache
{
public:
  CEmbyViewCache();
 ~CEmbyViewCache();

  void  Init(const EmbyViewContent &content);
  const std::string GetId() const { return m_cache.id; };
  const std::string GetName() const { return m_cache.name; };
  void  SetItems(CVariant &variant) {m_cache.items = std::move(variant);};
  CVariant &GetItems() { return m_cache.items; };
  bool  AppendItem(const CVariant &variant);
  bool  UpdateItem(const CVariant &variant);
  bool  RemoveItem(const CVariant &variant);

  bool  SetWatched(const std::string id, int playcount, double resumetime);
  bool  SetUnWatched(const std::string id);

  const EmbyViewInfo GetInfo() const;

private:
  EmbyViewContent m_cache;
  CCriticalSection m_cacheLock;
};
