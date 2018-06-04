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

#include "HueProviderRenderCapture.h"

#include "cores/VideoRenderers/RenderManager.h"
#include "cores/VideoRenderers/RenderCapture.h"

CHueProviderRenderCapture::CHueProviderRenderCapture()
{
  m_capture = g_renderManager.AllocRenderCapture();
}

CHueProviderRenderCapture::~CHueProviderRenderCapture()
{
  if (m_capture)
    Deinitialize();
}

bool CHueProviderRenderCapture::Initialize(unsigned int width, unsigned int height)
{
  g_renderManager.Capture(m_capture, width, height, CAPTUREFLAG_CONTINUOUS);
  return true;
}

bool CHueProviderRenderCapture::Deinitialize()
{
  g_renderManager.ReleaseRenderCapture(m_capture);
  m_capture = nullptr;
  return true;
}

bool CHueProviderRenderCapture::WaitMSec(unsigned int milliSeconds)
{
  return m_capture->GetEvent().WaitMSec(milliSeconds);
}

HueProvideStatus CHueProviderRenderCapture::GetStatus()
{
  if (m_capture->GetUserState() == CAPTURESTATE_DONE)
    return HueProvideStatus::DONE;

  return HueProvideStatus::FAILED;
}

unsigned char*CHueProviderRenderCapture::GetBuffer()
{
  return m_capture->GetPixels();
}