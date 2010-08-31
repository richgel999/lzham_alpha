// File: lzham_vector.cpp
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
#include "lzham_vector.h"

namespace lzham
{
   bool elemental_vector::increase_capacity(uint min_new_capacity, bool grow_hint, uint element_size, object_mover pMover, bool nofail)
   {
      LZHAM_ASSERT(m_size <= m_capacity);
#ifdef LZHAM_PLATFORM_PC_X64
      LZHAM_ASSERT(min_new_capacity < (0x400000000ULL / element_size));
#else      
      LZHAM_ASSERT(min_new_capacity < (0x7FFF0000U / element_size));
#endif      

      if (m_capacity >= min_new_capacity)
         return true;

      size_t new_capacity = min_new_capacity;
      if ((grow_hint) && (!math::is_power_of_2(new_capacity)))
         new_capacity = math::next_pow2(new_capacity);

      LZHAM_ASSERT(new_capacity && (new_capacity > m_capacity));

      const size_t desired_size = element_size * new_capacity;
      size_t actual_size;
      if (!pMover)
      {
         void* new_p = lzham_realloc(m_p, desired_size, &actual_size, true);
         if (!new_p)
         {
            if (nofail)
               return false;
               
            char buf[256];
            sprintf_s(buf, sizeof(buf), "vector: lzham_realloc() failed allocating %u bytes", desired_size);
            LZHAM_FAIL(buf);
         }
         m_p = new_p;
      }
      else
      {
         void* new_p = lzham_malloc(desired_size, &actual_size);
         if (!new_p)
         {
            if (nofail)
               return false;
               
            char buf[256];
            sprintf_s(buf, sizeof(buf), "vector: lzham_malloc() failed allocating %u bytes", desired_size);
            LZHAM_FAIL(buf);
         }
         
         (*pMover)(new_p, m_p, m_size);
         
         if (m_p)
            lzham_free(m_p);

         m_p = new_p;
      }            
      
      if (actual_size > desired_size)
         m_capacity = static_cast<uint>(actual_size / element_size);
      else
         m_capacity = static_cast<uint>(new_capacity);
    
      return true;
   }

} // namespace lzham
