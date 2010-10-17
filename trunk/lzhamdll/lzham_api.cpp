// File: lzham_api.cpp
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
#include "lzham_decomp.h"
#include "lzham_comp.h"

extern "C" LZHAM_DLL_EXPORT lzham_uint32 lzham_get_version(void)
{
   return LZHAM_DLL_VERSION;
}

extern "C" LZHAM_DLL_EXPORT void lzham_set_memory_callbacks(lzham_realloc_func pRealloc, lzham_msize_func pMSize, void* pUser_data)
{
   lzham::lzham_lib_set_memory_callbacks(pRealloc, pMSize, pUser_data);
}

extern "C" LZHAM_DLL_EXPORT lzham_decompress_state_ptr lzham_decompress_init(const lzham_decompress_params *pParams)
{
   return lzham::lzham_lib_decompress_init(pParams);
}

extern "C" LZHAM_DLL_EXPORT lzham_uint32 lzham_decompress_deinit(lzham_decompress_state_ptr p)
{
   return lzham::lzham_lib_decompress_deinit(p);
}

extern "C" LZHAM_DLL_EXPORT lzham_decompress_status_t lzham_decompress(
   lzham_decompress_state_ptr p,
   const lzham_uint8 *pIn_buf, size_t *pIn_buf_size, 
   lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
   lzham_bool no_more_input_bytes_flag)
{
   return lzham::lzham_lib_decompress(p, pIn_buf, pIn_buf_size, pOut_buf, pOut_buf_size, no_more_input_bytes_flag);
}   

extern "C" LZHAM_DLL_EXPORT lzham_decompress_status_t lzham_decompress_memory(const lzham_decompress_params *pParams, lzham_uint8* pDst_buf, size_t *pDst_len, const lzham_uint8* pSrc_buf, size_t src_len, lzham_uint32 *pAdler32)
{
   return lzham::lzham_lib_decompress_memory(pParams, pDst_buf, pDst_len, pSrc_buf, src_len, pAdler32);
}

extern "C" LZHAM_DLL_EXPORT lzham_compress_state_ptr lzham_compress_init(const lzham_compress_params *pParams)
{
   return lzham::lzham_lib_compress_init(pParams);
}

extern "C" LZHAM_DLL_EXPORT lzham_uint32 lzham_compress_deinit(lzham_compress_state_ptr p)
{
   return lzham::lzham_lib_compress_deinit(p);
}

extern "C" LZHAM_DLL_EXPORT lzham_compress_status_t lzham_compress(
   lzham_compress_state_ptr p,
   const lzham_uint8 *pIn_buf, size_t *pIn_buf_size, 
   lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
   lzham_bool no_more_input_bytes_flag)
{
   return lzham::lzham_lib_compress(p, pIn_buf, pIn_buf_size, pOut_buf, pOut_buf_size, no_more_input_bytes_flag);
}   

extern "C" LZHAM_DLL_EXPORT lzham_compress_status_t lzham_compress_memory(const lzham_compress_params *pParams, lzham_uint8* pDst_buf, size_t *pDst_len, const lzham_uint8* pSrc_buf, size_t src_len, lzham_uint32 *pAdler32)
{
   return lzham::lzham_lib_compress_memory(pParams, pDst_buf, pDst_len, pSrc_buf, src_len, pAdler32);
}

