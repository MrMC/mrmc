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

#include "HueUtils.h"

#include <math.h>

// CIE 1931 coordinates of CIE Standard Illuminant D65
constexpr float D65_x = 0.31271f;
constexpr float D65_y = 0.32902f;

// Normalized XYZ coordinates of D65 (normalized s.t. Y = 100)
//constexpr float D65_Xn = 95.047f;
//constexpr float D65_Yn = 100.0f;
//constexpr float D65_Zn = 108.883f;


CHueUtils::CHueUtils()
{
}

void CHueUtils::rgb2xy(float red, float green, float blue, float& x, float& y)
{
  // https://developers.meethue.com/documentation/color-conversions-rgb-xy

  // gamma correction
  const float redCorrected = (red > 0.04045f) ? pow((red + 0.055f) / (1.0f + 0.055f), 2.4f) : (red / 12.92f);
  const float greenCorrected = (green > 0.04045f) ? pow((green + 0.055f) / (1.0f + 0.055f), 2.4f) : (green / 12.92f);
  const float blueCorrected = (blue > 0.04045f) ? pow((blue + 0.055f) / (1.0f + 0.055f), 2.4f) : (blue / 12.92f);

  const float X = redCorrected * 0.664511f + greenCorrected * 0.154324f + blueCorrected * 0.162028f;
  const float Y = redCorrected * 0.283881f + greenCorrected * 0.668433f + blueCorrected * 0.047685f;
  const float Z = redCorrected * 0.000088f + greenCorrected * 0.072310f + blueCorrected * 0.986039f;

  x = D65_x;
  y = D65_y;
  if (X + Y + Z > 0)
  {
    x = X / (X + Y + Z);
    y = Y / (X + Y + Z);
  }
}

// http://cs.haifa.ac.il/hagit/courses/ist/Lectures/Demos/ColorApplet2/t_convert.html

void CHueUtils::xy2uv(float x, float y, float& u, float& v)
{
  u = 4*x / ( -2*x + 12*y + 3 );
  v = 9*y / ( -2*x + 12*y + 3 );
}

void CHueUtils::uv2xy(float u, float v, float& x, float& y)
{
  x = 27*u / ( 18*u - 48*v + 36 );
  y = 12*v / ( 18*u - 48*v + 36 );
}
