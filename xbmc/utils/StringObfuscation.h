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


#include <cstdint>
#include <array>

// based on http://stackoverflow.com/a/17426611/410767 by Xeo
namespace std14replacement
{
  template <size_t... Ints>
  struct index_sequence
  {
    using type = index_sequence;
    using value_type = size_t;
    static constexpr std::size_t size() noexcept { return sizeof...(Ints); }
  };

  // --------------------------------------------------------------

  template <class Sequence1, class Sequence2>
  struct _merge_and_renumber;

  template <size_t... I1, size_t... I2>
  struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
    : index_sequence<I1..., (sizeof...(I1)+I2)...>
  { };

  // --------------------------------------------------------------

  template <size_t N>
  struct make_index_sequence
    : _merge_and_renumber<typename make_index_sequence<N/2>::type,
                          typename make_index_sequence<N - N/2>::type>
  { };

  template<> struct make_index_sequence<0> : index_sequence<> { };
  template<> struct make_index_sequence<1> : index_sequence<0> { };
}

namespace StringObfuscation
{
  //-------------------------------------------------------------//
  // "Malware related compile-time hacks with C++11" by LeFF   //
  // You can use this code however you like, I just don't really //
  // give a shit, but if you feel some respect for me, please //
  // don't cut off this comment when copy-pasting... ;-)       //
  //-------------------------------------------------------------//
   
  ////////////////////////////////////////////////////////////////////
  template <int X> struct EnsureCompileTime {
    enum : int {
      Value = X
    };
  };
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  //Use Compile-Time as seed
  #define Seed ((__TIME__[7] - '0') * 1  + (__TIME__[6] - '0') * 10  + \
                (__TIME__[4] - '0') * 60   + (__TIME__[3] - '0') * 600 + \
                (__TIME__[1] - '0') * 3600 + (__TIME__[0] - '0') * 36000)
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  constexpr inline __attribute__((always_inline)) int LinearCongruentGenerator(int Rounds) {
    return 1013904223 + 1664525 * ((Rounds> 0) ? LinearCongruentGenerator(Rounds - 1) : Seed & 0xFFFFFFFF);
  }
  #define Random() EnsureCompileTime<LinearCongruentGenerator(10)>::Value //10 Rounds
  #define RandomNumber(Min, Max) (Min + (Random() % (Max - Min + 1)))
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  template <int... Pack> struct IndexList {};
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  template <typename IndexList, int Right> struct Append;
  template <int... Left, int Right> struct Append<IndexList<Left...>, Right> {
    typedef IndexList<Left..., Right> Result;
  };
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  template <int N> struct ConstructIndexList {
    typedef typename Append<typename ConstructIndexList<N - 1>::Result, N - 1>::Result Result;
  };
  template <> struct ConstructIndexList<0> {
    typedef IndexList<> Result;
  };
  ////////////////////////////////////////////////////////////////////
   
   
  ////////////////////////////////////////////////////////////////////
  const char XORKEY = static_cast<char>(RandomNumber(0, 0xFF));
  constexpr inline __attribute__((always_inline)) char EncryptCharacter(const char Character, int Index) {
    return Character ^ (XORKEY + Index);
  }
   
  template <typename IndexList> class CXorString;
  template <int... Index> class CXorString<IndexList<Index...> > {
  private:
      char Value[sizeof...(Index) + 1];
  public:
      constexpr inline __attribute__((always_inline)) CXorString(const char* const String)
      : Value{ EncryptCharacter(String[Index], Index)... } {}
   
      inline __attribute__((always_inline)) char* decrypt() {
        for(int t = 0; t < (int)sizeof...(Index); t++) {
            Value[t] = Value[t] ^ (XORKEY + t);
          }
          Value[sizeof...(Index)] = '\0';
          return Value;
      }
   
      char* get() {
        return Value;
      }
  };
  // note: building debug will not compile time obfuscate strings and they will show up
  // in the binary. In release builds, strings will be obfuscated. Compilers will ignore all sorts of magic needed.
  //#define ObfuscateS(X, String) CXorString<ConstructIndexList<sizeof( String) - 1>::Result> X(String)
  #define ObfuscateString( String ) ( CXorString<ConstructIndexList<sizeof( String ) - 1>::Result> ( String ).decrypt() )
}
