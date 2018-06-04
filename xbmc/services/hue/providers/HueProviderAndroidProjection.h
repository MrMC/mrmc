#pragma once
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

#include <stdint.h>

#include "../IHueProvider.h"

class CHueProviderAndroidProjection : public IHueProvider
{
public:
  CHueProviderAndroidProjection();
  ~CHueProviderAndroidProjection() override;

  // IHueProvider interface
public:
  bool Initialize(unsigned int width, unsigned int height) override;
  bool Deinitialize() override;
  bool WaitMSec(unsigned int milliSeconds) override;
  HueProvideStatus GetStatus() override;
  unsigned char*GetBuffer() override;

private:
  uint8_t*         m_pixels;
  unsigned int     m_width;
  unsigned int     m_height;
  unsigned int     m_bufferSize;

  HueProvideStatus m_curstatus;
};
