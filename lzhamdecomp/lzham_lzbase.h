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

namespace lzham
{
   class CLZBase
   {
   public:
      enum 
      {
         cMinMatchLen = 2U,
         cMaxMatchLen = 257U,

         cMinDictSizeLog2 = 15,
         cMaxDictSizeLog2 = 29,
                  
         cMatchHistSize = 3
      };

      enum 
      {
         cLZXNumSecondaryLengths = 250,
         cLZXNumSpecialLengths = 2,
         
         cLZXLowestUsableMatchSlot = 1,
         cLZXMaxPositionSlots = 128
      };
      
      enum
      {
         cLZXSpecialCodeEndOfBlockCode = 0,
         cLZXSpecialCodeResetStatePartial = 1
      };
      
      enum
      {  
         cLZHAMDebugSyncMarkerValue = 666,
         cLZHAMDebugSyncMarkerBits = 12
      };

      enum
      {
         cBlockHeaderBits = 2,
         
         cCompBlock = 1,
         cRawBlock = 2,
         cEOFBlock = 3
      };
      
      enum
      {
         cNumStates = 12,
         cNumLitStates = 7,
         
         cNumLitPredBits = 6,
         cNumDeltaLitPredBits = 6,
         
         cNumIsMatchContextBits = 4
      };
      
      uint m_dict_size_log2;
      uint m_dict_size;
      
      uint m_num_lzx_slots;
      uint m_lzx_position_base[cLZXMaxPositionSlots];
      uint8 m_lzx_position_extra_bits[cLZXMaxPositionSlots];
      uint m_lzx_position_extra_mask[cLZXMaxPositionSlots];
      
      uint8 m_slot_tab0[4096];
      uint8 m_slot_tab1[512];
      uint8 m_slot_tab2[256];

      void init_position_slots(uint dict_size_log2);
      
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
         
   //#define LZVERIFY
   
   //#define LZDEBUG
   //#define LZDISABLE_RAW_BLOCKS

} // namespace lzham
