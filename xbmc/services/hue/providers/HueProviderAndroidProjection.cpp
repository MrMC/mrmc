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

#include "HueProviderAndroidProjection.h"

#include <androidjni/Image.h>

#include "platform/android/activity/XBMCApp.h"
extern "C"
{
  #include "libswscale/swscale.h"
}

CHueProviderAndroidProjection::CHueProviderAndroidProjection()
  : m_pixels(nullptr)
  , m_width(0)
  , m_height(0)
  , m_bufferSize(0)
  , m_curstatus(HueProvideStatus::FAILED)
{
}

CHueProviderAndroidProjection::~CHueProviderAndroidProjection()
{
  Deinitialize();
}


bool CHueProviderAndroidProjection::Initialize(unsigned int width, unsigned int height)
{
  m_width = width; m_height = height;
  if (m_bufferSize != m_width * m_height * 4)
  {
    delete[] m_pixels;
    m_bufferSize = m_width * m_height * 4;
    m_pixels = new uint8_t[m_bufferSize];
  }

  CXBMCApp::get()->startCapture(m_width, m_height);
  return true;
}

bool CHueProviderAndroidProjection::Deinitialize()
{
  delete[] m_pixels;
  CXBMCApp::get()->StopCapture();
  return true;
}

bool CHueProviderAndroidProjection::WaitMSec(unsigned int milliSeconds)
{
  bool ret = CXBMCApp::get()->WaitForCapture(milliSeconds);
  m_curstatus = (ret ? HueProvideStatus::DONE : HueProvideStatus::FAILED);
  return ret;
}

HueProvideStatus CHueProviderAndroidProjection::GetStatus()
{
  return m_curstatus;
}

unsigned char* CHueProviderAndroidProjection::GetBuffer()
{
  jni::CJNIImage image;
  if (!CXBMCApp::get()->GetCapture(image))
    return nullptr;

  int iWidth = image.getWidth();
  int iHeight = image.getHeight();

  std::vector<jni::CJNIImagePlane> planes = image.getPlanes();
  if (planes.empty())
  {
    m_curstatus = HueProvideStatus::FAILED;
    return nullptr;
  }
  CJNIByteBuffer bytebuffer = planes[0].getBuffer();

  struct SwsContext *context = sws_getContext(iWidth, iHeight, AV_PIX_FMT_RGBA,
                                              m_width, m_height, AV_PIX_FMT_BGRA,
                                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (!context)
  {
    m_curstatus = HueProvideStatus::FAILED;
    return nullptr;
  }

  void *buf_ptr = xbmc_jnienv()->GetDirectBufferAddress(bytebuffer.get_raw());

  uint8_t *src[] = { (uint8_t*)buf_ptr, 0, 0, 0 };
  int     srcStride[] = { planes[0].getRowStride(), 0, 0, 0 };

  uint8_t *dst[] = { m_pixels, 0, 0, 0 };
  int     dstStride[] = { (int)m_width * 4, 0, 0, 0 };

  sws_scale(context, src, srcStride, 0, iHeight, dst, dstStride);
  sws_freeContext(context);

  image.close();

  return m_pixels;
}