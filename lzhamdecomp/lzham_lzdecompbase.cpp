// File: lzham_lzdecompbase.cpp
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
#include "lzham_lzdecompbase.h"

namespace lzham
{
   void CLZDecompBase::init_position_slots(uint dict_size_log2)
   {
      m_dict_size_log2 = dict_size_log2;
      m_dict_size = 1U << dict_size_log2;
      
      int i, j;
      for (i = 0, j = 0; i < cLZXMaxPositionSlots; i += 2) 
      {
         m_lzx_position_extra_bits[i] = (uint8)j;
         m_lzx_position_extra_bits[i + 1] = (uint8)j; 

         if ((i != 0) && (j < 25))  
            j++; 
      }

      for (i = 0, j = 0; i < cLZXMaxPositionSlots; i++) 
      {
         m_lzx_position_base[i] = j;
         m_lzx_position_extra_mask[i] = (1 << m_lzx_position_extra_bits[i]) - 1;
         j += (1 << m_lzx_position_extra_bits[i]);
      }

      m_num_lzx_slots = 0;         
      
      const uint largest_dist = m_dict_size - 1;
      for (int i = 0; i < cLZXMaxPositionSlots; i++)
      {
         if ( (largest_dist >= m_lzx_position_base[i]) &&
              (largest_dist < (m_lzx_position_base[i] + (1 << m_lzx_position_extra_bits[i])) ) )
         {
            m_num_lzx_slots = i + 1;
            break;
         }              
      }
      
      LZHAM_VERIFY(m_num_lzx_slots);
   }
   
} //namespace lzham
