// File: lzham_mem.h
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

namespace lzham
{
   void     lzham_mem_init();
   
   void*    lzham_malloc(size_t size, size_t* pActual_size = NULL);
   void*    lzham_realloc(void* p, size_t size, size_t* pActual_size = NULL, bool movable = true);
   void     lzham_free(void* p);
   size_t   lzham_msize(void* p);

   template<typename T>
   inline T* lzham_new()
   {
      T* p = static_cast<T*>(lzham_malloc(sizeof(T)));
      if (!p) return NULL;
      return helpers::construct(p);
   }
     
   template<typename T, typename A>
   inline T* lzham_new(const A& init0)
   {
      T* p = static_cast<T*>(lzham_malloc(sizeof(T)));
      if (!p) return NULL;
      return new (static_cast<void*>(p)) T(init0); 
   }
   
   template<typename T, typename A, typename B>
   inline T* lzham_new(const A& init0, const B& init1)
   {
      T* p = static_cast<T*>(lzham_malloc(sizeof(T)));
      if (!p) return NULL;
      return new (static_cast<void*>(p)) T(init0, init1); 
   }
   
   template<typename T, typename A, typename B, typename C>
   inline T* lzham_new(const A& init0, const B& init1, const C& init2)
   {
      T* p = static_cast<T*>(lzham_malloc(sizeof(T)));
      if (!p) return NULL;
      return new (static_cast<void*>(p)) T(init0, init1, init2); 
   }
   
   template<typename T, typename A, typename B, typename C, typename D>
   inline T* lzham_new(const A& init0, const B& init1, const C& init2, const D& init3)
   {
      T* p = static_cast<T*>(lzham_malloc(sizeof(T)));
      if (!p) return NULL;
      return new (static_cast<void*>(p)) T(init0, init1, init2, init3); 
   }

   template<typename T>
   inline T* lzham_new_array(uint32 num)
   {
      if (!num) num = 1;

      uint8* q = static_cast<uint8*>(lzham_malloc(LZHAM_MIN_ALLOC_ALIGNMENT + sizeof(T) * num));
      if (!q)
         return NULL;

      T* p = reinterpret_cast<T*>(q + LZHAM_MIN_ALLOC_ALIGNMENT);

      reinterpret_cast<uint32*>(p)[-1] = num;
      reinterpret_cast<uint32*>(p)[-2] = ~num;

      helpers::construct_array(p, num);
      return p;
   }

   template<typename T> 
   inline void lzham_delete(T* p)
   {
      if (p) 
      {
         helpers::destruct(p);
         lzham_free(p);
      }         
   }

   template<typename T> 
   inline void lzham_delete_array(T* p)
   {
      if (p)
      {
         const uint32 num = reinterpret_cast<uint32*>(p)[-1];
         const uint32 num_check = reinterpret_cast<uint32*>(p)[-2];
         LZHAM_ASSERT(num && (num == ~num_check));
         if (num == ~num_check)
         {
            helpers::destruct_array(p, num);

            lzham_free(reinterpret_cast<uint8*>(p) - LZHAM_MIN_ALLOC_ALIGNMENT);
         }
      }
   }   

} // namespace lzham
