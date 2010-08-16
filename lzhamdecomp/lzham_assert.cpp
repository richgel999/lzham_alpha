// File: lzham_assert.cpp
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
#include "lzham_core.h"

static bool g_fail_exceptions;
static bool g_exit_on_failure = true;

void lzham_enable_fail_exceptions(bool enabled)
{
   g_fail_exceptions = enabled;
}

void lzham_assert(const char* pExp, const char* pFile, unsigned line)
{
   char buf[512];

#ifdef WIN32
   sprintf_s(buf, sizeof(buf), "%s(%u): Assertion failed: \"%s\"\n", pFile, line, pExp);
#else   
   sprintf(buf, "%s(%u): Assertion failed: \"%s\"\n", pFile, line, pExp);
#endif   
   
   lzham_output_debug_string(buf);
   
   printf(buf);
   
   if (lzham_is_debugger_present())   
      lzham_debug_break();
}

void lzham_fail(const char* pExp, const char* pFile, unsigned line)
{
   char buf[512];

#ifdef WIN32
   sprintf_s(buf, sizeof(buf), "%s(%u): Failure: \"%s\"\n", pFile, line, pExp);
#else
   sprintf(buf, "%s(%u): Failure: \"%s\"\n", pFile, line, pExp);
#endif   

   lzham_output_debug_string(buf);

   printf(buf);

   if (lzham_is_debugger_present())   
      lzham_debug_break();

   if (g_fail_exceptions)
      RaiseException(LZHAM_FAIL_EXCEPTION_CODE, 0, 0, NULL);
   else if (g_exit_on_failure)
      exit(EXIT_FAILURE);
}

void trace(const char* pFmt, va_list args)
{
   if (lzham_is_debugger_present())
   {
      char buf[512];
#ifdef WIN32      
      vsprintf_s(buf, sizeof(buf), pFmt, args);
#else
      vsprintf(buf, pFmt, args);
#endif      
   
      lzham_output_debug_string(buf);   
   }      
};

void trace(const char* pFmt, ...)
{
   va_list args;
   va_start(args, pFmt);     
   trace(pFmt, args);
   va_end(args);
};
