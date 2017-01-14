/*
 * This file is part of libbluray/libdvd
 * Copyright (C) 2010  hpi1
 * Copyright (C) 2017 Rootcoder, LLC (adapted for libdvd)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDVD_ATTRIBUTES_H_
#define LIBDVD_ATTRIBUTES_H_

#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3 ))
#    if defined(_WIN32)
#        define DVD_ATTR_FORMAT_PRINTF(format,var) __attribute__((__format__(__gnu_printf__,format,var)))
#    else
#        define DVD_ATTR_FORMAT_PRINTF(format,var) __attribute__((__format__(__printf__,format,var)))
#    endif
#    define DVD_ATTR_MALLOC                    __attribute__((__malloc__))
#    define DVD_ATTR_PACKED                    __attribute__((packed))
#else
#    define DVD_ATTR_FORMAT_PRINTF(format,var)
#    define DVD_ATTR_MALLOC
#    define DVD_ATTR_PACKED
#endif

#if defined(_WIN32)
#    if defined(__GNUC__)
#        define DVD_PUBLIC  __attribute__((dllexport))
#        define DVD_PRIVATE
#    else
#        define DVD_PUBLIC  __declspec(dllexport)
#        define DVD_PRIVATE
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define DVD_PUBLIC  __attribute__((visibility("default")))
#    define DVD_PRIVATE __attribute__((visibility("hidden")))
#else
#    define DVD_PUBLIC
#    define DVD_PRIVATE
#endif

#if !defined(__GNUC__) || __GNUC__ < 3
#  define DVD_LIKELY(x)   (x)
#  define DVD_UNLIKELY(x) (x)
#else
#  define DVD_LIKELY(x)   __builtin_expect((x),1)
#  define DVD_UNLIKELY(x) __builtin_expect((x),0)
#endif

#endif /* LIBDVD_ATTRIBUTES_H_ */
