// File: lzham_platform.h
//
// Copyright (c) 2009-2010 Richard Geldreich, Jr. <richgel99@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#pragma once

#ifdef LZHAM_PLATFORM_PC
   const bool c_lzham_little_endian_platform = true;
   #define LZHAM_LITTLE_ENDIAN_CPU 1
#else
   const bool c_lzham_little_endian_platform = false;
   #define LZHAM_BIG_ENDIAN_CPU 1
#endif

const bool c_lzham_big_endian_platform = !c_lzham_little_endian_platform;

inline bool lzham_is_little_endian() { return c_lzham_little_endian_platform; }
inline bool lzham_is_big_endian() { return c_lzham_big_endian_platform; }

inline bool lzham_is_xbox()
{
#ifdef LZHAM_PLATFORM_X360
   return true;
#else
   return false;
#endif
}

inline bool lzham_is_pc()
{
#ifdef LZHAM_PLATFORM_PC
   return true;
#else
   return false;
#endif
}

inline bool lzham_is_x86()
{
#ifdef LZHAM_PLATFORM_PC_X86
   return true;
#else
   return false;
#endif
}

inline bool lzham_is_x64()
{
#ifdef LZHAM_PLATFORM_PC_X64
   return true;
#else
   return false;
#endif
}
#define RESTRICT __restrict

bool lzham_is_debugger_present(void);
void lzham_debug_break(void);
void lzham_output_debug_string(const char* p);

// actually in lzham_assert.cpp
void lzham_assert(const char* pExp, const char* pFile, unsigned line);
void lzham_fail(const char* pExp, const char* pFile, unsigned line);

inline void lzham_yield_processor()
{
   #if defined ( LZHAM_PLATFORM_PC_X64 )
      _mm_pause();
   #elif defined( LZHAM_PLATFORM_PC_X86 )
      YieldProcessor();
   #elif defined( LZHAM_PLATFORM_X360 )
      YieldProcessor();
      __asm { or r0, r0, r0 }
      YieldProcessor();
      __asm { or r1, r1, r1 }
   #else
      #error Unimplemented!
   #endif
}

#ifdef __GNUC__
int sprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...);
#endif

#if ( defined(_MSC_VER) && !defined(LZHAM_PLATFORM_X360) ) || defined(__MINGW32__) || defined(__MINGW64__)
   #define LZHAM_USE_X86_INTRINSICS 1
#endif

#define LZHAM_FORCE_INLINE __forceinline

#ifdef PLATFORM_X360
   #define LZHAM_MEMORY_EXPORT_BARRIER MemoryBarrier();
#else   
   // Barriers shouldn't be necessary on x86/x64.
   #define LZHAM_MEMORY_EXPORT_BARRIER
#endif

#ifdef PLATFORM_X360
   #define LZHAM_MEMORY_IMPORT_BARRIER MemoryBarrier();
#else   
   // Barriers shouldn't be necessary on x86/x64.
   #define LZHAM_MEMORY_IMPORT_BARRIER
#endif
