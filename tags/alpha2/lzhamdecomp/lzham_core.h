// File: lzham_core.h
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

#if defined (WIN32) || defined(_XBOX)
   #pragma warning (disable: 4127) // conditional expression is constant
#endif

#if defined(_XBOX)
   #include <xtl.h>
   #define _HAS_EXCEPTIONS 0
   #define NOMINMAX
   #ifndef XBOX
      #define XBOX
   #endif
   #define LZHAM_PLATFORM_X360 1
#elif defined(WIN32) 
   #ifdef NDEBUG
      // Ensure checked iterators are disabled.
      #define _SECURE_SCL 0   
      #define _HAS_ITERATOR_DEBUGGING 0
   #endif      
   #ifndef _DLL
      // If we're using the DLL form of the run-time libs, we're also going to be enabling exceptions because we'll be building CLR apps.
      // Otherwise, we disable exceptions for a small (up to 5%) speed boost.
      #define _HAS_EXCEPTIONS 0
   #endif
   #define NOMINMAX
      
   #define LZHAM_PLATFORM_PC 1
   
   #ifdef _WIN64
      #define LZHAM_PLATFORM_PC_X64 1
   #else
      #define LZHAM_PLATFORM_PC_X86 1
   #endif
   
   #ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x500
   #endif
   #ifndef WIN32_LEAN_AND_MEAN
      #define WIN32_LEAN_AND_MEAN
   #endif
   #include "windows.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>

#include "lzham.h"

#include "lzham_config.h"
#include "lzham_platform.h"
#include "lzham_assert.h"
#include "lzham_types.h"
#include "lzham_traits.h"
#include "lzham_mem.h"
#include "lzham_math.h"
#include "lzham_helpers.h"
#include "lzham_utils.h"
#include "lzham_vector.h"

