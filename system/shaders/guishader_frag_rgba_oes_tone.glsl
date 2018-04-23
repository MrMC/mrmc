/*
 *      Copyright (C) 2010-2013 Team XBMC
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

#extension GL_OES_EGL_image_external : require

precision mediump float;
uniform samplerExternalOES m_samp0;
varying vec4      m_cord0;

uniform float     m_brightness;
uniform float     m_contrast;

uniform float     m_toneP1;

// https://github.com/armory3d/armory/blob/master/Shaders/std/tonemap.glsl

vec3 uncharted2Tonemap(const vec3 x)
{
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(const vec3 color)
{
	const float W = 11.2;
	const float exposureBias = 2.0;
	vec3 curr = uncharted2Tonemap(exposureBias * color);
	vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
	return curr * whiteScale;
}

// Based on Filmic Tonemapping Operators http://filmicgames.com/archives/75
vec3 tonemapFilmic(const vec3 color)
{
	vec3 x = max(vec3(0.0), color - 0.004);
	return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 acesFilm(const vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d ) + e), 0.0, 1.0);
}

vec3 tonemapReinhard(const vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 tonemapReinhard_luma(const vec3 color)
{
  const vec3 coefsDst709 = vec3(0.2126, 0.7152, 0.0722);  //BT709

  float luma = dot(color, coefsDst709);
  float val = luma * (1.0 + luma/(m_toneP1*m_toneP1))/(1.0 + luma);  // Reinhard Complex
  return color * val / luma;
}


void main ()
{
    vec4 color = texture2D(m_samp0, m_cord0.xy).rgba;
    color.rgb = tonemapReinhard_luma(color.rgb);
    color = color * m_contrast;
    color = color + m_brightness;

    gl_FragColor.rgba = color;
}
