// File: polar_codes.cpp
// Andrew Polar's prefix code algorithm: http://ezcodesample.com/prefixer/prefixer_article.html
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
#include "lzham_polar_codes.h"

namespace lzham
{
   struct sym_freq
   {
      uint16 m_freq;
      uint16 m_sym;
   };

   static inline sym_freq* radix_sort_syms(uint num_syms, sym_freq* syms0, sym_freq* syms1)
   {  
      const uint cMaxPasses = 2;
      uint hist[256 * cMaxPasses];

      memset(hist, 0, sizeof(hist[0]) * 256 * cMaxPasses);

      sym_freq* p = syms0;
      sym_freq* q = syms0 + (num_syms >> 1) * 2;

      for ( ; p != q; p += 2)
      {
         const uint freq0 = p[0].m_freq;
         const uint freq1 = p[1].m_freq;

         hist[        freq0         & 0xFF]++;
         hist[256 + ((freq0 >>  8) & 0xFF)]++;

         hist[        freq1        & 0xFF]++;
         hist[256 + ((freq1 >>  8) & 0xFF)]++;
      }

      if (num_syms & 1)
      {
         const uint freq = p->m_freq;

         hist[        freq        & 0xFF]++;
         hist[256 + ((freq >>  8) & 0xFF)]++;
      }

      sym_freq* pCur_syms = syms0;
      sym_freq* pNew_syms = syms1;
      
      const uint total_passes = (hist[256] == num_syms) ? 1 : cMaxPasses;
               
      for (uint pass = 0; pass < total_passes; pass++)
      {
         const uint* pHist = &hist[pass << 8];

         uint offsets[256];

         uint cur_ofs = 0;
         for (uint i = 0; i < 256; i += 2)
         {
            offsets[i] = cur_ofs;
            cur_ofs += pHist[i];

            offsets[i+1] = cur_ofs;
            cur_ofs += pHist[i+1];
         }

         const uint pass_shift = pass << 3;

         sym_freq* p = pCur_syms;
         sym_freq* q = pCur_syms + (num_syms >> 1) * 2;

         for ( ; p != q; p += 2)
         {
            uint c0 = p[0].m_freq;
            uint c1 = p[1].m_freq;

            if (pass)
            {
               c0 >>= 8;
               c1 >>= 8;
            }

            c0 &= 0xFF;
            c1 &= 0xFF;

            // Cut down on LHS's on console platforms by processing two at a time.
            if (c0 == c1)
            {
               uint dst_offset0 = offsets[c0];

               offsets[c0] = dst_offset0 + 2;

               pNew_syms[dst_offset0] = p[0];
               pNew_syms[dst_offset0 + 1] = p[1];
            }
            else
            {
               uint dst_offset0 = offsets[c0]++;
               uint dst_offset1 = offsets[c1]++;

               pNew_syms[dst_offset0] = p[0];
               pNew_syms[dst_offset1] = p[1];
            }
         }

         if (num_syms & 1)
         {
            uint c = ((p->m_freq) >> pass_shift) & 0xFF;

            uint dst_offset = offsets[c];
            offsets[c] = dst_offset + 1;

            pNew_syms[dst_offset] = *p;
         }

         sym_freq* t = pCur_syms;
         pCur_syms = pNew_syms;
         pNew_syms = t;
      }            

#ifdef LZHAM_ASSERTS_ENABLED
      uint prev_freq = 0;
      for (uint i = 0; i < num_syms; i++)
      {
         LZHAM_ASSERT(!(pCur_syms[i].m_freq < prev_freq));
         prev_freq = pCur_syms[i].m_freq;
      }
#endif

      return pCur_syms;
   }

   struct polar_work_tables
   {
      sym_freq syms0[cPolarMaxSupportedSyms];
      sym_freq syms1[cPolarMaxSupportedSyms];
   };

   uint get_generate_polar_codes_table_size()
   {
      return sizeof(polar_work_tables);
   }
   
   void generate_polar_codes(uint num_syms, sym_freq* pSF, uint8* pCodesizes, uint& max_code_size_ret)
   {
      int tmp_freq[cPolarMaxSupportedSyms];

      uint orig_total_freq = 0;
      uint cur_total = 0;
      for (uint i = 0; i < num_syms; i++) 
      {
         uint sym_freq = pSF[num_syms - 1 - i].m_freq;
         orig_total_freq += sym_freq;

         uint sym_len = math::total_bits(sym_freq);
         uint adjusted_sym_freq = 1 << (sym_len - 1);
         tmp_freq[i] = adjusted_sym_freq;
         cur_total += adjusted_sym_freq;
      }

      uint tree_total = 1 << (math::total_bits(orig_total_freq) - 1);
      if (tree_total < orig_total_freq)
         tree_total <<= 1;

      uint start_index = 0;
      while ((cur_total < tree_total) && (start_index < num_syms))
      {
         for (uint i = start_index; i < num_syms; i++) 
         {
            uint freq = tmp_freq[i];
            if ((cur_total + freq) <= tree_total) 
            {
               tmp_freq[i] += freq;
               if ((cur_total += freq) == tree_total)
                  break;
            }
            else 
            {
               start_index = i + 1;
            }
         }
      }         

      LZHAM_ASSERT(cur_total == tree_total);

      uint max_code_size = 0;
      const uint tree_total_bits = math::total_bits(tree_total);
      for (uint i = 0; i < num_syms; i++) 
      {
         uint codesize = (tree_total_bits - math::total_bits(tmp_freq[i]));
         max_code_size = LZHAM_MAX(codesize, max_code_size);
         pCodesizes[pSF[num_syms-1-i].m_sym] = static_cast<uint8>(codesize);
      }
      max_code_size_ret = max_code_size;
   }

   bool generate_polar_codes(void* pContext, uint num_syms, const uint16* pFreq, uint8* pCodesizes, uint& max_code_size, uint& total_freq_ret)
   {
      if ((!num_syms) || (num_syms > cPolarMaxSupportedSyms))
         return false;

      polar_work_tables& state = *static_cast<polar_work_tables*>(pContext);;

      uint max_freq = 0;
      uint total_freq = 0;

      uint num_used_syms = 0;
      for (uint i = 0; i < num_syms; i++)
      {
         uint freq = pFreq[i];

         if (!freq)
            pCodesizes[i] = 0;
         else
         {
            total_freq += freq;
            max_freq = math::maximum(max_freq, freq);

            sym_freq& sf = state.syms0[num_used_syms];
            sf.m_sym = static_cast<uint16>(i);
            sf.m_freq = static_cast<uint16>(freq);
            num_used_syms++;
         }            
      }

      total_freq_ret = total_freq;

      if (num_used_syms == 1)
      {
         pCodesizes[state.syms0[0].m_sym] = 1;
      }
      else
      {
         sym_freq* syms = radix_sort_syms(num_used_syms, state.syms0, state.syms1);

         generate_polar_codes(num_syms, syms, pCodesizes, max_code_size);
      }

      return true;
   }

} // namespace lzham

