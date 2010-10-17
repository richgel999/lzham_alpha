// File: lzham_assert.h
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

const unsigned int LZHAM_FAIL_EXCEPTION_CODE = 256U;
void lzham_enable_fail_exceptions(bool enabled);

void lzham_assert(const char* pExp, const char* pFile, unsigned line);
void lzham_fail(const char* pExp, const char* pFile, unsigned line);

#ifdef NDEBUG
   #define LZHAM_ASSERT(x) ((void)0)
#else
   #define LZHAM_ASSERT(_exp) (void)( (!!(_exp)) || (lzham_assert(#_exp, __FILE__, __LINE__), 0) )
   #define LZHAM_ASSERTS_ENABLED 1
#endif

#define LZHAM_VERIFY(_exp) (void)( (!!(_exp)) || (lzham_assert(#_exp, __FILE__, __LINE__), 0) )

#define LZHAM_FAIL(msg) do { lzham_fail(#msg, __FILE__, __LINE__); } while(0)

#define LZHAM_ASSERT_OPEN_RANGE(x, l, h) LZHAM_ASSERT((x >= l) && (x < h))
#define LZHAM_ASSERT_CLOSED_RANGE(x, l, h) LZHAM_ASSERT((x >= l) && (x <= h))

void lzham_trace(const char* pFmt, va_list args);
void lzham_trace(const char* pFmt, ...);

// Borrowed from boost libraries.
template <bool x>  struct assume_failure;
template <> struct assume_failure<true> { enum { blah = 1 }; };
template<int x> struct assume_try { };

#define LZHAM_JOINER_FINAL(a, b) a##b
#define LZHAM_JOINER(a, b) LZHAM_JOINER_FINAL(a, b)
#define LZHAM_JOIN(a, b) LZHAM_JOINER(a, b)
#define LZHAM_ASSUME(p) typedef assume_try < sizeof(assume_failure< (bool)(p) > ) > LZHAM_JOIN(assume_typedef, __COUNTER__)
