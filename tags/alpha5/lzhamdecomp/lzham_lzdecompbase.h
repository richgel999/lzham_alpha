// File: lzham_lzdecompbase.h
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

//#define LZHAM_LZDEBUG

#define LZHAM_COMPUTE_IS_MATCH_MODEL_INDEX(prev_char, cur_state) ((prev_char) >> (8 - CLZDecompBase::cNumIsMatchContextBits)) + ((cur_state) << CLZDecompBase::cNumIsMatchContextBits)

namespace lzham
{
   struct CLZDecompBase
   {
      enum 
      {
         cMinMatchLen = 2U,
         cMaxMatchLen = 257U,

         cMinDictSizeLog2 = 15,
         cMaxDictSizeLog2 = 29,
                  
         cMatchHistSize = 4,
         cMaxLen2MatchDist = 2047
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
         cLZXSpecialCodePartialStateReset = 1
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

         cNumLitPredBits = 6,          // must be even
         cNumDeltaLitPredBits = 6,     // must be even

         cNumIsMatchContextBits = 6
      };
      
      uint m_dict_size_log2;
      uint m_dict_size;
      
      uint m_num_lzx_slots;
      uint m_lzx_position_base[cLZXMaxPositionSlots];
      uint m_lzx_position_extra_mask[cLZXMaxPositionSlots];
      uint8 m_lzx_position_extra_bits[cLZXMaxPositionSlots];
      
      void init_position_slots(uint dict_size_log2);
   };
   
} // namespace lzham
