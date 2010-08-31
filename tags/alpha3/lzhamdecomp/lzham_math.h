// File: lzham_math.h
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

#if defined(LZHAM_USE_X86_INTRINSICS) && !defined(__MINGW32__)
   #include <intrin.h>
   #if defined(_MSC_VER)
      #pragma intrinsic(_BitScanReverse)
   #endif
#endif

namespace lzham
{
   namespace math
   {
   	const float cNearlyInfinite = 1.0e+37f;

      // Yes I know these should probably be pass by ref, not val:
      // http://www.stepanovpapers.com/notes.pdf
      // Just don't use them on non-simple (non built-in) types!
      template<typename T> inline T minimum(T a, T b) { return (a < b) ? a : b; }

      template<typename T> inline T minimum(T a, T b, T c) { return minimum(minimum(a, b), c); }

      template<typename T> inline T maximum(T a, T b) { return (a > b) ? a : b; }

      template<typename T> inline T maximum(T a, T b, T c) { return maximum(maximum(a, b), c); }

      template<typename T> inline T clamp(T value, T low, T high) { return (value < low) ? low : ((value > high) ? high : value); }

      inline bool is_power_of_2(uint32 x) { return x && ((x & (x - 1U)) == 0U); }
      inline bool is_power_of_2(uint64 x) { return x && ((x & (x - 1U)) == 0U); }

      template<typename T> inline T align_up_pointer(T p, uint alignment)
      {
         LZHAM_ASSERT(is_power_of_2(alignment));
         ptr_bits_t q = reinterpret_cast<ptr_bits_t>(p);
         q = (q + alignment - 1) & (~((uint_ptr)alignment - 1));
         return reinterpret_cast<T>(q);
      }

		// From "Hackers Delight"
		// val remains unchanged if it is already a power of 2.
      inline uint32 next_pow2(uint32 val)
      {
         val--;
         val |= val >> 16;
         val |= val >> 8;
         val |= val >> 4;
         val |= val >> 2;
         val |= val >> 1;
         return val + 1;
      }

      // val remains unchanged if it is already a power of 2.
      inline uint64 next_pow2(uint64 val)
      {
         val--;
         val |= val >> 32;
         val |= val >> 16;
         val |= val >> 8;
         val |= val >> 4;
         val |= val >> 2;
         val |= val >> 1;
         return val + 1;
      }

      inline uint floor_log2i(uint v)
      {
         uint l = 0;
         while (v > 1U)
         {
            v >>= 1;
            l++;
         }
         return l;
      }

      inline uint ceil_log2i(uint v)
      {
         uint l = floor_log2i(v);
         if ((l != cIntBits) && (v > (1U << l)))
            l++;
         return l;
      }

      // Returns the total number of bits needed to encode v.
      // This needs to be fast - it's used heavily when determining Polar codelengths.
      inline uint total_bits(uint v)
      {
         unsigned long l = 0;
#if defined(__MINGW32__)
         if (v)
         {
            l = 32 -__builtin_clz(v);
         }
#elif defined(LZHAM_USE_X86_INTRINSICS)
         if (_BitScanReverse(&l, v))
         {
            l++;
         }
#else
         while (v > 0U)
         {
            v >>= 1;
            l++;
         }
#endif
         return l;
      }

   }

} // namespace lzham

