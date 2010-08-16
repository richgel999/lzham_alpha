// File: lzham_mem.cpp
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
#include <malloc.h>

using namespace lzham;

namespace lzham
{
#ifdef LZHAM_PLATFORM_PC_X64
   const uint64 MAX_POSSIBLE_BLOCK_SIZE = 0x400000000ULL;
#else
   const uint32 MAX_POSSIBLE_BLOCK_SIZE = 0x7FFF0000U;
#endif   
      
   static void* lzham_default_realloc(void* p, size_t size, size_t* pActual_size, bool movable, void* pUser_data)
   {
      pUser_data;

      void* p_new;

      if (!p)
      {
         p_new = malloc(size);
         LZHAM_ASSERT( (reinterpret_cast<ptr_bits_t>(p_new) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1)) == 0 );

         if (pActual_size)
            *pActual_size = p_new ? _msize(p_new) : 0;
      }
      else if (!size)
      {
         free(p);
         p_new = NULL;

         if (pActual_size)
            *pActual_size = 0;
      }
      else
      {
         void* p_final_block = p;
         p_new = _expand(p, size);

         if (p_new)
         {
            LZHAM_ASSERT( (reinterpret_cast<ptr_bits_t>(p_new) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1)) == 0 );
            p_final_block = p_new;
         }
         else if (movable)
         {
            p_new = realloc(p, size);

            if (p_new)
            {
               LZHAM_ASSERT( (reinterpret_cast<ptr_bits_t>(p_new) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1)) == 0 );
               p_final_block = p_new;
            }
         }

         if (pActual_size)
            *pActual_size = _msize(p_final_block);
      }

      return p_new;
   }

   static size_t lzham_default_msize(void* p, void* pUser_data)
   {
      pUser_data;
      return p ? _msize(p) : 0;
   }

   static lzham_realloc_func        g_pRealloc = lzham_default_realloc;
   static lzham_msize_func          g_pMSize   = lzham_default_msize;
   static void*                     g_pUser_data;
   
   static inline void lzham_mem_error(const char* p_msg)
   {
      lzham_assert(p_msg, __FILE__, __LINE__);
   }

   void* lzham_malloc(size_t size, size_t* pActual_size)
   {
      size = (size + sizeof(uint32) - 1U) & ~(sizeof(uint32) - 1U);
      if (!size)
         size = sizeof(uint32);

      if (size > MAX_POSSIBLE_BLOCK_SIZE)
      {
         lzham_mem_error("lzham_malloc: size too big");
         return NULL;
      }

      size_t actual_size = size;
      uint8* p_new = static_cast<uint8*>((*g_pRealloc)(NULL, size, &actual_size, true, g_pUser_data));

      if (pActual_size)
         *pActual_size = actual_size;

      if ((!p_new) || (actual_size < size))
      {
         lzham_mem_error("lzham_malloc: out of memory");
         return NULL;
      }

      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(p_new) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1)) == 0);

      return p_new;
   }

   void* lzham_realloc(void* p, size_t size, size_t* pActual_size, bool movable)
   {
      if ((uint32)p & (LZHAM_MIN_ALLOC_ALIGNMENT - 1))
      {
         lzham_mem_error("lzham_realloc: bad ptr");
         return NULL;
      }

      if (size > MAX_POSSIBLE_BLOCK_SIZE)
      {
         lzham_mem_error("lzham_malloc: size too big");
         return NULL;
      }

      size_t actual_size = size;
      void* p_new = (*g_pRealloc)(p, size, &actual_size, movable, g_pUser_data);

      if (pActual_size)
         *pActual_size = actual_size;

      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(p_new) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1)) == 0);

      return p_new;
   }

   void lzham_free(void* p)
   {
      if (!p)
         return;

      if (reinterpret_cast<ptr_bits_t>(p) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1))
      {
         lzham_mem_error("lzham_free: bad ptr");
         return;
      }

      (*g_pRealloc)(p, 0, NULL, true, g_pUser_data);
   }

   size_t lzham_msize(void* p)
   {
      if (!p)
         return 0;

      if (reinterpret_cast<ptr_bits_t>(p) & (LZHAM_MIN_ALLOC_ALIGNMENT - 1))
      {
         lzham_mem_error("lzham_msize: bad ptr");
         return 0;
      }

      return (*g_pMSize)(p, g_pUser_data);
   }   
   
   void lzham_lib_set_memory_callbacks(lzham_realloc_func pRealloc, lzham_msize_func pMSize, void* pUser_data)
   {
      if ((!pRealloc) || (!pMSize))
      {
         g_pRealloc = lzham_default_realloc;
         g_pMSize = lzham_default_msize;
         g_pUser_data = NULL;
      }
      else
      {
         g_pRealloc = pRealloc;
         g_pMSize = pMSize;
         g_pUser_data = pUser_data;
      }
   }

} // namespace lzham

