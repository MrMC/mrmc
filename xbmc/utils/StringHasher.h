#pragma once
/*
 *      Copyright (C) 2016 Team MrMC
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


#include <cstdint>

namespace StringHasher
{
  typedef std::uint64_t hash_t;
  constexpr hash_t prime = 0x100000001B3ull;
  constexpr hash_t basis = 0xCBF29CE484222325ull;
  constexpr hash_t hash_compile_time(char const *str, hash_t last_value = basis)
  {
    return *str ? hash_compile_time(str + 1, (*str ^ last_value) * prime) : last_value;
  }

  static hash_t mkhash(char const *str)
  {
    hash_t ret{basis};
    while(*str)
    {
      ret ^= *str++;
      ret *= prime;
    }
    return ret;
  }

  constexpr hash_t operator "" _mkhash(char const *p, size_t)
  {
    return hash_compile_time(p);
  }
}
