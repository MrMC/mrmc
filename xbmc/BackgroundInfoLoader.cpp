/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "BackgroundInfoLoader.h"
#include "FileItem.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "URL.h"

#include <algorithm>

CBackgroundInfoLoader::CBackgroundInfoLoader() : m_thread (nullptr)
{
  m_bStop = true;
  m_pObserver=NULL;
  m_pProgressCallback=NULL;
  m_pVecItems = NULL;
  m_bIsLoading = false;
}

CBackgroundInfoLoader::~CBackgroundInfoLoader()
{
  StopThread();
}

void CBackgroundInfoLoader::Run()
{
  CSingleLock lock(m_lock);
  size_t focus = m_focus;
  lock.Leave();

  try
  {
    if (!m_stage1.empty())
    {
      OnLoaderStart();

      // Stage 1: All "fast" stuff we have already cached
      while (!m_stage1.empty())
      {
        // Ask the callback if we should abort
        if ((m_pProgressCallback && m_pProgressCallback->Abort()) || m_bStop)
          break;

        // check if we need to rotate the order to
        // align front (next processed) to focused item
        lock.Enter();
        if (focus != m_focus)
        {
          // update m_focus so we do this once
          focus = m_focus;
          // find start index in stage1 list
          auto focusIt = std::find_if( m_stage1.begin(), m_stage1.end(),
            [&focus](const std::pair<size_t, CFileItemPtr>& element){ return element.first == focus;} );
          if (focusIt != m_stage1.end())
          {
            // focus index is in list, rotate the vector contents
            // so that focus index is in front and will get processed next.
            std::rotate(m_stage1.begin(), focusIt, m_stage1.end());
          }
        }
        lock.Leave();

        CFileItemPtr pItem = m_stage1.front().second;
        try
        {
          // process the item
          if (LoadItemCached(pItem.get()) && m_pObserver)
            m_pObserver->OnItemLoaded(pItem.get());
          // once stage1 is processed, move item to stage2
          m_stage2.push_back(m_stage1.front().second);
          // no pop in std:vector, erase the front.
          m_stage1.erase(m_stage1.begin());
         }
        catch (...)
        {
          CLog::Log(LOGERROR, "CBackgroundInfoLoader::LoadItemCached - Unhandled exception for item %s", CURL::GetRedacted(
            pItem->GetPath()).c_str());
        }
      }

      // Stage 2: All "slow" stuff that we need to lookup
      for (std::vector<CFileItemPtr>::const_iterator iter = m_stage2.begin(); iter != m_stage2.end(); ++iter)
      {
        CFileItemPtr pItem = *iter;

        // Ask the callback if we should abort
        if ((m_pProgressCallback && m_pProgressCallback->Abort()) || m_bStop)
          break;

        try
        {
          if (LoadItemLookup(pItem.get()) && m_pObserver)
            m_pObserver->OnItemLoaded(pItem.get());
        }
        catch (...)
        {
          CLog::Log(LOGERROR, "CBackgroundInfoLoader::LoadItemLookup - Unhandled exception for item %s",
            CURL::GetRedacted(pItem->GetPath()).c_str());
        }
      }
    }

    OnLoaderFinish();
    m_bIsLoading = false;
  }
  catch (...)
  {
    m_bIsLoading = false;
    CLog::Log(LOGERROR, "%s - Unhandled exception", __FUNCTION__);
  }
}

void CBackgroundInfoLoader::SetFocus(size_t focus)
{
  CSingleLock lock(m_lock);
  if (m_focus != focus)
    m_focus = focus;
}

void CBackgroundInfoLoader::Load(CFileItemList& items)
{
  StopThread();

  if (items.Size() == 0)
    return;

  CSingleLock lock(m_lock);

  m_focus = 0;
  for (size_t nItem = 0; nItem < (size_t)items.Size(); ++nItem)
    m_stage1.push_back(std::make_pair(nItem, items[nItem]));

  m_pVecItems = &items;
  m_bStop = false;
  m_bIsLoading = true;

  m_thread = new CThread(this, "BackgroundLoader");
  m_thread->Create();
  m_thread->SetPriority(THREAD_PRIORITY_BELOW_NORMAL);
}

void CBackgroundInfoLoader::StopAsync()
{
  m_bStop = true;
}


void CBackgroundInfoLoader::StopThread()
{
  StopAsync();

  if (m_thread)
  {
    m_thread->StopThread();
    delete m_thread;
    m_thread = NULL;
  }
  m_focus = 0;
  m_stage1.clear();
  m_stage2.clear();
  m_pVecItems = NULL;
  m_bIsLoading = false;
}

bool CBackgroundInfoLoader::IsLoading()
{
  return m_bIsLoading;
}

void CBackgroundInfoLoader::SetObserver(IBackgroundLoaderObserver* pObserver)
{
  m_pObserver = pObserver;
}

void CBackgroundInfoLoader::SetProgressCallback(IProgressCallback* pCallback)
{
  m_pProgressCallback = pCallback;
}

