// File: lzhamtest.cpp
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "lzham_decomp.h"
#include "lzham_comp.h"

#ifdef _DEBUG
const bool g_is_debug = true;
#else
const bool g_is_debug = false;
#endif

typedef unsigned char uint8;
typedef unsigned int uint;
typedef unsigned int uint32;
typedef unsigned __int64 uint64;

int simple_test()
{
   printf("LZHAM simple memory to memory compression test\n");
   
   const int cDictSizeLog2 = 15;
   
   lzham_compress_params comp_params;
   memset(&comp_params, 0, sizeof(comp_params));
   comp_params.m_struct_size = sizeof(comp_params);
   comp_params.m_dict_size_log2 = cDictSizeLog2;
   comp_params.m_level = LZHAM_COMP_LEVEL_DEFAULT;
   comp_params.m_max_helper_threads = 0;
      
   lzham_uint8 cmp_buf[1024];
   size_t cmp_len = sizeof(cmp_buf);
   
   const char *p = "This is a test.This is a test.This is a test.1234567This is a test.This is a test.123456";
   size_t uncomp_len = strlen(p);
   
   lzham_uint32 comp_adler32 = 0;
   lzham_compress_status_t comp_status = lzham::lzham_lib_compress_memory(&comp_params, cmp_buf, &cmp_len, (const lzham_uint8 *)p, uncomp_len, &comp_adler32);
   if (comp_status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("Compression test failed with status %i!\n", comp_status);
      return EXIT_FAILURE;
   }
   
   printf("Uncompressed size: %u\nCompressed size: %u\n", uncomp_len, cmp_len);
   
   lzham_decompress_params decomp_params;
   memset(&decomp_params, 0, sizeof(decomp_params));
   decomp_params.m_struct_size = sizeof(decomp_params);
   decomp_params.m_dict_size_log2 = cDictSizeLog2;
   decomp_params.m_compute_adler32 = true;
   
   lzham_uint8 decomp_buf[1024];
   size_t decomp_size = sizeof(decomp_buf);
   lzham_uint32 decomp_adler32 = 0;
   lzham_decompress_status_t decomp_status = lzham::lzham_lib_decompress_memory(&decomp_params, decomp_buf, &decomp_size, cmp_buf, cmp_len, &decomp_adler32);
   if (decomp_status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("Compression test failed with status %i!\n", decomp_status);
      return EXIT_FAILURE;
   }
   
   if ((comp_adler32 != decomp_adler32) || (decomp_size != uncomp_len) || (memcmp(decomp_buf, p, uncomp_len)))
   {
      printf("Compression test failed!\n");
      return EXIT_FAILURE;
   }
         
   printf("Compression test succeeded.\n");

   return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
   argc;
   argv;
   
   return simple_test();
}

