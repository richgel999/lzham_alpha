// File: lzham_lzbase.cpp
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
#include "lzham_lzbase.h"

namespace lzham
{
   void CLZBase::init_slot_tabs()
   {
      for (uint i = 0; i < m_num_lzx_slots; i++)
      {
         //printf("%u: 0x%08X - 0x%08X, %u\n", i, m_lzx_position_base[i], m_lzx_position_base[i] + (1 << m_lzx_position_extra_bits[i]) - 1, m_lzx_position_extra_bits[i]);

         uint lo = m_lzx_position_base[i];
         uint hi = lo + m_lzx_position_extra_mask[i];

         uint8* pTab;
         uint shift;
         uint n;

         if (hi < 0x1000)
         {
            pTab = m_slot_tab0;
            shift = 0;
            n = sizeof(m_slot_tab0);
         }
         else if (hi < 0x100000)
         {
            pTab = m_slot_tab1;
            shift = 11;
            n = sizeof(m_slot_tab1);
         }
         else if (hi < 0x1000000)
         {
            pTab = m_slot_tab2;
            shift = 16;
            n = sizeof(m_slot_tab2);
         }
         else
            break;

         lo >>= shift;
         hi >>= shift;

         LZHAM_ASSERT(hi < n);
         memset(pTab + lo, (uint8)i, hi - lo + 1);
      }

#ifdef LZHAM_BUILD_DEBUG      
      uint slot, ofs;
      for (uint i = 1; i < m_num_lzx_slots; i++) 
      {
         compute_lzx_position_slot(m_lzx_position_base[i], slot, ofs);
         LZHAM_ASSERT(slot == i);

         compute_lzx_position_slot(m_lzx_position_base[i] + m_lzx_position_extra_mask[i], slot, ofs);
         LZHAM_ASSERT(slot == i);
      }

      for (uint i = 1; i <= (m_dict_size-1); i += 512U*1024U)
      {
         compute_lzx_position_slot(i, slot, ofs);
         LZHAM_ASSERT(i == m_lzx_position_base[slot] + ofs);
      }

      compute_lzx_position_slot(m_dict_size - 1, slot, ofs);
      LZHAM_ASSERT((m_dict_size - 1) == m_lzx_position_base[slot] + ofs);
#endif      
   }
} //namespace lzham

