// File: lzham_utils.h
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

#define LZHAM_GET_ALIGNMENT(v) ((!sizeof(v)) ? 1 : (__alignof(v) ? __alignof(v) : sizeof(uint32))) 

#define LZHAM_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LZHAM_MAX(a, b) (((a) < (b)) ? (b) : (a))

template<class T, size_t N> T decay_array_to_subtype(T (&a)[N]);
#define LZHAM_ARRAY_SIZE(X) (sizeof(X) / sizeof(decay_array_to_subtype(X)))

namespace lzham
{
   namespace utils
   {
      template<typename T> inline void swap(T& l, T& r)
      {
         T temp(l);
         l = r;
         r = temp;
      }
      
      template<typename T> inline void zero_object(T& obj)
      {
         memset(&obj, 0, sizeof(obj));
      }
                  
      static inline uint32 swap32(uint32 x) { return ((x << 24U) | ((x << 8U) & 0x00FF0000U) | ((x >> 8U) & 0x0000FF00U) | (x >> 24U)); }
      
      inline uint count_leading_zeros16(uint v)
      {
         LZHAM_ASSERT(v < 0x10000);
         
         uint temp;
         uint n = 16;
         
         temp = v >> 8;
         if (temp) { n -=  8; v = temp; }

         temp = v >> 4;
         if (temp) { n -=  4; v = temp; }

         temp = v >> 2;
         if (temp) { n -=  2; v = temp; }

         temp = v >> 1;
         if (temp) { n -=  1; v = temp; }

         if (v & 1) n--;

         return n;
      }
      
   }   // namespace utils
         
} // namespace lzham

