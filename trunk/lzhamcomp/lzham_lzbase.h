// File: lzham_lzbase.h
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

#include "lzham_lzdecompbase.h"

//#define LZHAM_LZVERIFY
//#define LZHAM_DISABLE_RAW_BLOCKS

namespace lzham
{
   struct CLZBase : CLZDecompBase
   {
      uint8 m_slot_tab0[4096];
      uint8 m_slot_tab1[512];
      uint8 m_slot_tab2[256];

      void init_slot_tabs();

      inline void compute_lzx_position_slot(uint dist, uint& slot, uint& ofs)
      {
         uint s;
         if (dist < 0x1000)
            s = m_slot_tab0[dist];
         else if (dist < 0x100000)
            s = m_slot_tab1[dist >> 11];
         else if (dist < 0x1000000)
            s = m_slot_tab2[dist >> 16];
         else if (dist < 0x2000000)
            s = 48 + ((dist - 0x1000000) >> 23);
         else if (dist < 0x4000000)
            s = 50 + ((dist - 0x2000000) >> 24);
         else 
            s = 52 + ((dist - 0x4000000) >> 25);

         ofs = (dist - m_lzx_position_base[s]) & m_lzx_position_extra_mask[s];
         slot = s;

         LZHAM_ASSERT(s < m_num_lzx_slots);
         LZHAM_ASSERT((m_lzx_position_base[slot] + ofs) == dist);
         LZHAM_ASSERT(ofs < (1U << m_lzx_position_extra_bits[slot]));
      }
   };

} // namespace lzham
