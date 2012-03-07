// File: lzham.h
// This is the main header file which defines all the publically available API's, structs, and types used by the LZHAM codec.
// See Copyright Notice and license at the end of this file.

#pragma once

// Upper byte = major version
// Lower byte = minor version
#define LZHAM_DLL_VERSION        0x1007

#if defined(_MSC_VER)
   #define LZHAM_CDECL __cdecl
#else
   #define LZHAM_CDECL
#endif

#ifdef LZHAM_EXPORTS
   #define LZHAM_DLL_EXPORT __declspec(dllexport)
#else
   #define LZHAM_DLL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

   typedef unsigned char   lzham_uint8;
   typedef unsigned int    lzham_uint32;
   typedef unsigned int    lzham_bool;

   // Returns DLL version (LZHAM_DLL_VERSION).
   LZHAM_DLL_EXPORT lzham_uint32 LZHAM_CDECL lzham_get_version(void);

   // User provided memory allocation

   // Custom allocation function must return pointers with LZHAM_MIN_ALLOC_ALIGNMENT (or better).
   #define LZHAM_MIN_ALLOC_ALIGNMENT sizeof(size_t) * 2

   typedef void*  (LZHAM_CDECL *lzham_realloc_func)(void* p, size_t size, size_t* pActual_size, bool movable, void* pUser_data);
   typedef size_t (LZHAM_CDECL *lzham_msize_func)(void* p, void* pUser_data);

   // Call this function to force LZHAM to use custom memory malloc(), realloc(), free() and msize functions.
   LZHAM_DLL_EXPORT void LZHAM_CDECL lzham_set_memory_callbacks(lzham_realloc_func pRealloc, lzham_msize_func pMSize, void* pUser_data);

   // Compression
   #define LZHAM_MIN_DICT_SIZE_LOG2 15
   #define LZHAM_MAX_DICT_SIZE_LOG2_X86 26
   #define LZHAM_MAX_DICT_SIZE_LOG2_X64 29

   #define LZHAM_MAX_HELPER_THREADS 16

   enum lzham_compress_status_t
   {
      LZHAM_COMP_STATUS_NOT_FINISHED = 0,
      LZHAM_COMP_STATUS_NEEDS_MORE_INPUT,

      // All the following enums must indicate failure/success.

      LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      LZHAM_COMP_STATUS_SUCCESS = LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,
      LZHAM_COMP_STATUS_FAILED,
      LZHAM_COMP_STATUS_FAILED_INITIALIZING,
      LZHAM_COMP_STATUS_INVALID_PARAMETER,
      LZHAM_COMP_STATUS_OUTPUT_BUF_TOO_SMALL,

      LZHAM_COMP_STATUS_FORCE_DWORD = 0xFFFFFFFF
   };

   enum lzham_compress_level
   {
      LZHAM_COMP_LEVEL_FASTEST = 0,
      LZHAM_COMP_LEVEL_FASTER,
      LZHAM_COMP_LEVEL_DEFAULT,
      LZHAM_COMP_LEVEL_BETTER,
      LZHAM_COMP_LEVEL_UBER,

      LZHAM_TOTAL_COMP_LEVELS,

      LZHAM_COMP_LEVEL_FORCE_DWORD = 0xFFFFFFFF
   };

   // streaming (zlib-like) interface
   typedef void *lzham_compress_state_ptr;
   enum lzham_compress_flags
   {
      LZHAM_COMP_FLAG_FORCE_POLAR_CODING = 1,      // Forces Polar codes vs. Huffman, for a slight increase in decompression speed.
      LZHAM_COMP_FLAG_EXTREME_PARSING = 2,         // Improves ratio by allowing the compressor's parse graph to grow "higher" (up to 4 parent nodes per output node), but is much slower.
      LZHAM_COMP_FLAG_DETERMINISTIC_PARSING = 4,   // Guarantees that the compressed output will always be the same given the same input and parameters (no variation between runs due to kernel threading scheduling).

      // If enabled, the compressor is free to use any optimizations which could lower the decompression rate (such
      // as adaptively resetting the Huffman table update rate to maximum frequency, which is costly for the decompressor).
      LZHAM_COMP_FLAG_TRADEOFF_DECOMPRESSION_RATE_FOR_COMP_RATIO = 16,
   };

   struct lzham_compress_params
   {
      lzham_uint32 m_struct_size;            // set to sizeof(lzham_compress_params)
      lzham_uint32 m_dict_size_log2;         // set to the log2(dictionary_size), must range between [LZHAM_MIN_DICT_SIZE_LOG2, LZHAM_MAX_DICT_SIZE_LOG2_X86] for x86 LZHAM_MAX_DICT_SIZE_LOG2_X64 for x64
      lzham_compress_level m_level;          // set to LZHAM_COMP_LEVEL_FASTEST, etc.
      lzham_uint32 m_max_helper_threads;     // max # of additional "helper" threads to create, must range between [0,LZHAM_MAX_HELPER_THREADS]
      lzham_uint32 m_cpucache_total_lines;   // set to 0 (optimize compressed stream to avoid L1/L2 cache misses - not currently supported)
      lzham_uint32 m_cpucache_line_size;     // set to 0
      lzham_uint32 m_compress_flags;         // optional compression flags (see lzham_compress_flags enum)
      lzham_uint32 m_num_seed_bytes;         // for delta compression (optional) - number of seed bytes pointed to by m_pSeed_bytes
      const void *m_pSeed_bytes;             // for delta compression (optional) - pointer to seed bytes buffer, must be at least m_num_seed_bytes long
   };
   
   // Initializes a compressor. Returns a pointer to the compressor's internal state, or NULL on failure.
   // pParams cannot be NULL. Be sure to initialize the pParams->m_struct_size member to sizeof(lzham_compress_params) (along with the other members to reasonable values) before calling this function.
   // TODO: With large dictionaries this function could take a while (due to memory allocation). I need to add a reinit() API for compression (decompression already has one).
   LZHAM_DLL_EXPORT lzham_compress_state_ptr LZHAM_CDECL lzham_compress_init(const lzham_compress_params *pParams);

   // Deinitializes a compressor, releasing all allocated memory.
   // returns adler32 of source data (valid only on success).
   LZHAM_DLL_EXPORT lzham_uint32 LZHAM_CDECL lzham_compress_deinit(lzham_compress_state_ptr pState);

   // Compresses an arbitrarily sized block of data, writing as much available compressed data as possible to the output buffer. 
   // This method may be called as many times as needed, but for best perf. try not to call it with tiny buffers.
   // pState - Pointer to internal compression state, created by lzham_compress_init.
   // pIn_buf, pIn_buf_size - Pointer to input data buffer, and pointer to a size_t containing the number of bytes available in this buffer. 
   //                         On return, *pIn_buf_size will be set to the number of bytes read from the buffer.
   // pOut_buf, pOut_buf_size - Pointer to the output data buffer, and a pointer to a size_t containing the max number of bytes that can be written to this buffer.
   //                         On return, *pOut_buf_size will be set to the number of bytes written to this buffer.
   // no_more_input_bytes_flag - Set to true to indicate that no more input bytes are available to compress (EOF). Once you call this function with this param set to true, it must stay set to true in all future calls.
   //
   // Normal return status codes:
   //    LZHAM_COMP_STATUS_NOT_FINISHED - Compression can continue, but the compressor needs more input, or it needs more room in the output buffer.
   //    LZHAM_COMP_STATUS_NEEDS_MORE_INPUT - Compression can contintue, but the compressor has no more output, and has no input but we're not at EOF. Supply more input to continue.
   // Success/failure return status codes:
   //    LZHAM_COMP_STATUS_SUCCESS - Compression has completed successfully.
   //    LZHAM_COMP_STATUS_FAILED, LZHAM_COMP_STATUS_FAILED_INITIALIZING, LZHAM_COMP_STATUS_INVALID_PARAMETER - Something went wrong.
   LZHAM_DLL_EXPORT lzham_compress_status_t LZHAM_CDECL lzham_compress(
      lzham_compress_state_ptr pState,
      const lzham_uint8 *pIn_buf, size_t *pIn_buf_size,
      lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
      lzham_bool no_more_input_bytes_flag);

   // Single function call compression interface.
   // Same return codes as lzham_compress, except this function can also return LZHAM_COMP_STATUS_OUTPUT_BUF_TOO_SMALL.
   LZHAM_DLL_EXPORT lzham_compress_status_t LZHAM_CDECL lzham_compress_memory(
      const lzham_compress_params *pParams,
      lzham_uint8* pDst_buf,
      size_t *pDst_len,
      const lzham_uint8* pSrc_buf,
      size_t src_len,
      lzham_uint32 *pAdler32);

   // Decompression
   enum lzham_decompress_status_t
   {
      // LZHAM_DECOMP_STATUS_NOT_FINISHED indicates that the decompressor is flushing its output buffer (and there may be more bytes available to decompress).
      LZHAM_DECOMP_STATUS_NOT_FINISHED = 0,

      // LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT indicates that the decompressor has consumed all input bytes and has not encountered an "end of stream" code, so it's expecting more input to proceed.
      LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT,

      // All the following enums always (and MUST) indicate failure/success.
      LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      // LZHAM_DECOMP_STATUS_SUCCESS indicates decompression has successfully completed.
      LZHAM_DECOMP_STATUS_SUCCESS = LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      // The remaining status codes indicate a failure of some sort. Most failures are unrecoverable. TODO: Document which codes are recoverable.
      LZHAM_DECOMP_STATUS_FIRST_FAILURE_CODE,

      LZHAM_DECOMP_STATUS_FAILED_INITIALIZING = LZHAM_DECOMP_STATUS_FIRST_FAILURE_CODE,
      LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL,
      LZHAM_DECOMP_STATUS_FAILED_HAVE_MORE_OUTPUT,
      LZHAM_DECOMP_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES,
      LZHAM_DECOMP_STATUS_FAILED_BAD_CODE,
      LZHAM_DECOMP_STATUS_FAILED_ADLER32,
      LZHAM_DECOMP_STATUS_FAILED_BAD_RAW_BLOCK,
      LZHAM_DECOMP_STATUS_FAILED_BAD_COMP_BLOCK_SYNC_CHECK,
      LZHAM_DECOMP_STATUS_INVALID_PARAMETER,
   };

   // streaming (zlib-like) interface
   typedef void *lzham_decompress_state_ptr;

   // Decompression parameters structure.
   // Notes: 
   // m_dict_size_log2 MUST match the value used during compression!
   // If m_num_seed_bytes != 0, m_output_unbuffered MUST be false (i.e. static "seed" dictionaries are not compatible with unbuffered decompression).
   // The seed buffer's contents and size must match the seed buffer used during compression.
   struct lzham_decompress_params
   {
      lzham_uint32 m_struct_size;            // set to sizeof(lzham_decompress_params)
      lzham_uint32 m_dict_size_log2;         // set to the log2(dictionary_size), must range between [LZHAM_MIN_DICT_SIZE_LOG2, LZHAM_MAX_DICT_SIZE_LOG2_X86] for x86 LZHAM_MAX_DICT_SIZE_LOG2_X64 for x64
      lzham_bool m_output_unbuffered;        // true if the output buffer is guaranteed to be large enough to hold the entire output stream (a bit faster)
      lzham_bool m_compute_adler32;          // true to enable adler32 checking during decompression (slightly slower)
      lzham_uint32 m_num_seed_bytes;         // for delta compression (optional) - number of seed bytes pointed to by m_pSeed_bytes
      const void *m_pSeed_bytes;             // for delta compression (optional) - pointer to seed bytes buffer, must be at least m_num_seed_bytes long
   };
   
   // Initializes a decompressor.
   // pParams cannot be NULL. Be sure to initialize the pParams->m_struct_size member to sizeof(lzham_decompress_params) (along with the other members to reasonable values) before calling this function.
   // Note: With large dictionaries this function could take a while (due to memory allocation). To serially decompress multiple streams, it's faster to init a compressor once and 
   // reuse it using by calling lzham_decompress_reinit().
   LZHAM_DLL_EXPORT lzham_decompress_state_ptr LZHAM_CDECL lzham_decompress_init(const lzham_decompress_params *pParams);

   // Quickly re-initializes the decompressor to its initial state given an already allocated/initialized state (doesn't do any memory alloc unless necessary).
   LZHAM_DLL_EXPORT lzham_decompress_state_ptr LZHAM_CDECL lzham_decompress_reinit(lzham_decompress_state_ptr pState, const lzham_decompress_params *pParams);

   // Deinitializes a decompressor.
   // returns adler32 of decompressed data if compute_adler32 was true, otherwise it returns the adler32 from the compressed stream.
   LZHAM_DLL_EXPORT lzham_uint32 LZHAM_CDECL lzham_decompress_deinit(lzham_decompress_state_ptr pState);

   // Decompresses an arbitrarily sized block of compressed data, writing as much available decompressed data as possible to the output buffer. 
   // This method is implemented as a coroutine so it may be called as many times as needed. However, for best perf. try not to call it with tiny buffers.
   // pState - Pointer to internal decompression state, originally created by lzham_decompress_init.
   // pIn_buf, pIn_buf_size - Pointer to input data buffer, and pointer to a size_t containing the number of bytes available in this buffer. 
   //                         On return, *pIn_buf_size will be set to the number of bytes read from the buffer.
   // pOut_buf, pOut_buf_size - Pointer to the output data buffer, and a pointer to a size_t containing the max number of bytes that can be written to this buffer.
   //                         On return, *pOut_buf_size will be set to the number of bytes written to this buffer.
   // no_more_input_bytes_flag - Set to true to indicate that no more input bytes are available to compress (EOF). Once you call this function with this param set to true, it must stay set to true in all future calls.
   // Notes:
   // In unbuffered mode, the output buffer MUST be large enough to hold the entire decompressed stream. Otherwise, you'll receive the
   //  LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL error (which is currently unrecoverable during unbuffered decompression).
   // In buffered mode, if the output buffer's size is 0 bytes, the caller is indicating that no more output bytes are expected from the
   //  decompressor. In this case, if the decompressor actually has more bytes you'll receive the LZHAM_DECOMP_STATUS_FAILED_HAVE_MORE_OUTPUT
   //  error (which is recoverable in the buffered case - just call lzham_decompress() again with a non-zero size output buffer).
   LZHAM_DLL_EXPORT lzham_decompress_status_t LZHAM_CDECL lzham_decompress(
      lzham_decompress_state_ptr pState,
      const lzham_uint8 *pIn_buf, size_t *pIn_buf_size,
      lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
      lzham_bool no_more_input_bytes_flag);

   // Single function call interface.
   LZHAM_DLL_EXPORT lzham_decompress_status_t LZHAM_CDECL lzham_decompress_memory(
      const lzham_decompress_params *pParams,
      lzham_uint8* pDst_buf,
      size_t *pDst_len,
      const lzham_uint8* pSrc_buf,
      size_t src_len,
      lzham_uint32 *pAdler32);

   // Exported function typedefs, to simplify loading the LZHAM DLL dynamically.
   typedef lzham_uint32 (LZHAM_CDECL *lzham_get_version_func)(void);
   typedef void (LZHAM_CDECL *lzham_set_memory_callbacks_func)(lzham_realloc_func pRealloc, lzham_msize_func pMSize, void* pUser_data);
   typedef lzham_compress_state_ptr (LZHAM_CDECL *lzham_compress_init_func)(const lzham_compress_params *pParams);
   typedef lzham_uint32 (LZHAM_CDECL *lzham_compress_deinit_func)(lzham_compress_state_ptr pState);
   typedef lzham_compress_status_t (LZHAM_CDECL *lzham_compress_func)(lzham_compress_state_ptr pState, const lzham_uint8 *pIn_buf, size_t *pIn_buf_size, lzham_uint8 *pOut_buf, size_t *pOut_buf_size, lzham_bool no_more_input_bytes_flag);
   typedef lzham_compress_status_t (LZHAM_CDECL *lzham_compress_memory_func)(const lzham_compress_params *pParams, lzham_uint8* pDst_buf, size_t *pDst_len, const lzham_uint8* pSrc_buf, size_t src_len, lzham_uint32 *pAdler32);
   typedef lzham_decompress_state_ptr (LZHAM_CDECL *lzham_decompress_init_func)(const lzham_decompress_params *pParams);
   typedef lzham_decompress_state_ptr (LZHAM_CDECL *lzham_decompress_reinit_func)(lzham_compress_state_ptr pState, const lzham_decompress_params *pParams);
   typedef lzham_uint32 (LZHAM_CDECL *lzham_decompress_deinit_func)(lzham_decompress_state_ptr pState);
   typedef lzham_decompress_status_t (LZHAM_CDECL *lzham_decompress_func)(lzham_decompress_state_ptr pState, const lzham_uint8 *pIn_buf, size_t *pIn_buf_size, lzham_uint8 *pOut_buf, size_t *pOut_buf_size, lzham_bool no_more_input_bytes_flag);
   typedef lzham_decompress_status_t (LZHAM_CDECL *lzham_decompress_memory_func)(const lzham_decompress_params *pParams, lzham_uint8* pDst_buf, size_t *pDst_len, const lzham_uint8* pSrc_buf, size_t src_len, lzham_uint32 *pAdler32);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// This optional interface is used by the dynamic/static link helpers defined in lzham_dynamic_lib.h and lzham_static_lib.h.
// It allows code to always call LZHAM the same way, independent of how it was linked into the app (statically or dynamically).
class ilzham
{
   ilzham(const ilzham &other);
   ilzham& operator= (const ilzham &rhs);

public:
   ilzham() { clear(); }

   virtual ~ilzham() { }
   virtual bool load() = 0;
   virtual void unload() = 0;
   virtual bool is_loaded() = 0;

   void clear()
   {
      lzham_get_version = NULL;
      lzham_set_memory_callbacks = NULL;
      lzham_compress_init = NULL;
      lzham_compress_deinit = NULL;
      lzham_compress = NULL;
      lzham_compress_memory = NULL;
      lzham_decompress_init = NULL;
      lzham_decompress_reinit = NULL;
      lzham_decompress_deinit = NULL;
      lzham_decompress = NULL;
      lzham_decompress_memory = NULL;
   }

   lzham_get_version_func           lzham_get_version;
   lzham_set_memory_callbacks_func  lzham_set_memory_callbacks;
   lzham_compress_init_func         lzham_compress_init;
   lzham_compress_deinit_func       lzham_compress_deinit;
   lzham_compress_func              lzham_compress;
   lzham_compress_memory_func       lzham_compress_memory;
   lzham_decompress_init_func       lzham_decompress_init;
   lzham_decompress_reinit_func     lzham_decompress_reinit;
   lzham_decompress_deinit_func     lzham_decompress_deinit;
   lzham_decompress_func            lzham_decompress;
   lzham_decompress_memory_func     lzham_decompress_memory;
};
#endif

// Copyright (c) 2009-2011 Richard Geldreich, Jr. <richgel99@gmail.com>
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
