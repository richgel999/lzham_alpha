// File: lzham_lzcomp_internal.cpp
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
#include "lzham_lzcomp_internal.h"
#include "lzham_checksum.h"

#define LZHAM_UPDATE_STATS 0

namespace lzham
{
   static comp_settings s_settings[cCompressionLevelCount] = 
   {
      // cCompressionLevelFastest
      { 
         true,                   // m_fast_adaptive_huffman_updating
         true,                   // m_use_polar_codes
         true,                   // m_non_optimal_parse_if_hist_match
         0,                      // m_non_optimal_parse_match_len_thresh
         8,                      // m_match_truncation_disable_match_len_thresh
         512,                    // m_match_truncation_disable_match_dist_thresh
         false,                  // m_match_truncation_disable_if_hist_match
         2,                      // m_hist_match_len_disable_full_match_find_thresh
         1,                      // m_match_accel_max_matches_per_probe
         8,                      // m_match_accel_max_probes
         CLZBase::cMaxMatchLen + 1,  // m_max_parse_graph_nodes
      },
      // cCompressionLevelFaster
      { 
         true,                   // m_fast_adaptive_huffman_updating
         true,                   // m_use_polar_codes
         false,                  // m_non_optimal_parse_if_hist_match
         8,                      // m_non_optimal_parse_match_len_thresh
         8,                      // m_match_truncation_disable_match_len_thresh
         256,                    // m_match_truncation_disable_match_dist_thresh
         false,                  // m_match_truncation_disable_if_hist_match
         2,                      // m_hist_match_len_disable_full_match_find_thresh
         6,                      // m_match_accel_max_matches_per_probe
         16,                     // m_match_accel_max_probes
         CLZBase::cMaxMatchLen + 1, // m_max_parse_graph_nodes
      },
      // cCompressionLevelDefault
      { 
         false,                  // m_fast_adaptive_huffman_updating             
         true,                   // m_use_polar_codes
         false,                  // m_non_optimal_parse_if_hist_match              
         16,                     // m_non_optimal_parse_match_len_thresh           
         CLZBase::cMaxMatchLen,  // m_match_truncation_disable_match_len_thresh    
         0,                      // m_match_truncation_disable_match_dist_thresh   
         false,                  // m_match_truncation_disable_if_hist_match       
         16,                     // m_hist_match_len_disable_full_match_find_thresh  
         UINT_MAX,               // m_match_accel_max_matches_per_probe            
         32,                     // m_match_accel_max_probes                     
         512,                    // m_max_parse_graph_nodes
      },
      // cCompressionLevelBetter
      { 
         false,                  // m_fast_adaptive_huffman_updating             
         false,                  // m_use_polar_codes
         false,                  // m_non_optimal_parse_if_hist_match              
         28,                     // m_non_optimal_parse_match_len_thresh           
         CLZBase::cMaxMatchLen,  // m_match_truncation_disable_match_len_thresh    
         0,                      // m_match_truncation_disable_match_dist_thresh   
         false,                  // m_match_truncation_disable_if_hist_match       
         32,                     // m_hist_match_len_disable_full_match_find_thresh  
         UINT_MAX,               // m_match_accel_max_matches_per_probe            
         48,                     // m_match_accel_max_probes       
         768,                    // m_max_parse_graph_nodes              
      },
      // cCompressionLevelUber
      { 
         false,                  // m_fast_adaptive_huffman_updating             
         false,                  // m_use_polar_codes
         false,                  // m_non_optimal_parse_if_hist_match              
         CLZBase::cMaxMatchLen,  // m_non_optimal_parse_match_len_thresh           
         CLZBase::cMaxMatchLen,  // m_match_truncation_disable_match_len_thresh    
         0,                      // m_match_truncation_disable_match_dist_thresh   
         false,                  // m_match_truncation_disable_if_hist_match       
         CLZBase::cMaxMatchLen,  // m_hist_match_len_disable_full_match_find_thresh  
         UINT_MAX,               // m_match_accel_max_matches_per_probe            
         128,                    // m_match_accel_max_probes                     
         cMaxParseGraphNodes,    // m_max_parse_graph_nodes
      }
   };
   
   cpucache::cpucache() :
      m_num_cachelines(0),
      m_cacheline_size(0),
      m_cacheline_size_shift(0),
      m_num_lines_present(0)
   {
   }

   bool cpucache::init(uint num_cachelines, uint cacheline_size)
   {
      clear();
      
      LZHAM_ASSERT(math::is_power_of_2(cacheline_size));
      
      cacheline_size = LZHAM_MAX(32, cacheline_size);
                  
      m_num_cachelines = num_cachelines;
      m_cacheline_size = cacheline_size;
      m_cacheline_size_shift = math::floor_log2i(cacheline_size);
      
      if (!m_cacheline_hash.try_resize( 1U << math::ceil_log2i(num_cachelines * 3) ))
      {
         clear();
         return false;
      }
    
      m_head.m_line = -2;
      m_head.m_pPrev = NULL;
      m_head.m_pNext = &m_tail;
      
      m_tail.m_line = -1;
      m_tail.m_pPrev = &m_head;
      m_tail.m_pNext = NULL;
      
      m_num_lines_present = 0;
      
      return true;
   }
   
   void cpucache::clear()
   {
      m_num_cachelines = 0;
      m_cacheline_size = 0;
      m_cacheline_size_shift = 0;
      m_cacheline_hash.clear();
      m_num_lines_present = 0;
   }
   
   void cpucache::check() const
   {
      for (uint i = 0; i < m_cacheline_hash.size(); i++)
      {
         if (m_cacheline_hash[i].m_line != -1)
         {
            LZHAM_VERIFY(is_present(m_cacheline_hash[i].m_line << m_cacheline_size_shift));
         }
         else
         {
            LZHAM_VERIFY(m_cacheline_hash[i].m_pPrev == NULL);
            LZHAM_VERIFY(m_cacheline_hash[i].m_pNext == NULL);
         }
      }
   }
   
   void cpucache::erase_lru()
   {
      // cache is full - delete least recently touched
      cacheline *pLast = m_tail.m_pPrev;
      cacheline *pPrev = pLast->m_pPrev;

      LZHAM_ASSERT(pLast->m_pNext == &m_tail);
      LZHAM_ASSERT(pPrev->m_pNext == pLast);

      pPrev->m_pNext = &m_tail;
      m_tail.m_pPrev = pPrev;
      
      m_num_lines_present--;
           
      int i = static_cast<int>(pLast - m_cacheline_hash.get_ptr());
      
      // Hash erase algorithm from Knuth's "Sorting and Searching".
      for ( ; ; )
      {
         const int j = i;
         cacheline* pDst = &m_cacheline_hash[j];

         cacheline* pSrc;
         for ( ; ; )
         {
            if (--i < 0)
               i = m_cacheline_hash.size() - 1;

            pSrc = &m_cacheline_hash[i];
            
            if (pSrc->m_line == -1)
            {
               pDst->m_line = -1;
               pDst->m_pPrev = NULL;
               pDst->m_pNext = NULL;
               return;   
            }
            
            int r = pSrc->hash() & (m_cacheline_hash.size() - 1);
            
            if ( ((i <= r) && (r <  j)) ||
                 ((r <  j) && (j <  i)) ||
                 ((j <  i) && (i <= r)) )
               continue;
            else
               break;
         }
                                    
         cacheline* pPrev = pSrc->m_pPrev;
         cacheline* pNext = pSrc->m_pNext;
         
         LZHAM_ASSERT(pPrev->m_pNext == pSrc);
         LZHAM_ASSERT(pNext->m_pPrev == pSrc);
         
         pPrev->m_pNext = pDst;
         pNext->m_pPrev = pDst;
                  
         *pDst = *pSrc;
      }
   }
      
   void cpucache::touch(uint address)
   {
      int line = static_cast<int>(address >> m_cacheline_size_shift);
      LZHAM_ASSERT(line >= 0);
      
      int index = bitmix32(line) & (m_cacheline_hash.size() - 1);
         
      int first_erased_index = -1;
      
      for ( ; ; )
      {
         cacheline& c = m_cacheline_hash[index];
         
         if (c.m_line == -1)
         {
            // not present
            cacheline& ins = m_cacheline_hash[(first_erased_index >= 0) ? first_erased_index : index];
                        
            ins.m_line = line;
            
            cacheline *pFirst = m_head.m_pNext;
            
            LZHAM_ASSERT(m_head.m_pNext == pFirst);
            LZHAM_ASSERT(pFirst->m_pPrev == &m_head);

            ins.m_pPrev = &m_head;
            ins.m_pNext = pFirst;
            
            m_head.m_pNext = &ins;
            pFirst->m_pPrev = &ins;               
         
            m_num_lines_present++;
            if (m_num_lines_present > m_num_cachelines)
               erase_lru();
                              
            break;  
         }
         else if (c.m_line == line)
         {
            // present
            if (c.m_pPrev != &m_head)
            {
               // move to head of list
               cacheline *pPrev = c.m_pPrev;
               cacheline *pNext = c.m_pNext;
                              
               pPrev->m_pNext = pNext;
               pNext->m_pPrev = pPrev;
               
               cacheline *pFirst = m_head.m_pNext;
               
               c.m_pPrev = &m_head;
               c.m_pNext = pFirst;
               
               m_head.m_pNext = &c;               
               pFirst->m_pPrev = &c;               
            }
               
            break;
         }
         
         index--;
         if (index < 0)
            index = m_cacheline_hash.size() - 1;
      }
   }
   
   void cpucache::touch(uint address, uint len) 
   {
      LZHAM_ASSERT(len > 0);
      const uint64 start_address = address & (~(m_cacheline_size - 1));
      const uint64 end_address = (static_cast<uint64>(address) + len - 1) & (~(m_cacheline_size - 1));

      for (uint64 i = start_address; i <= end_address; i += m_cacheline_size)
         touch(static_cast<uint>(i));
   }
   
   bool cpucache::is_present(uint address) const
   {
      int line = static_cast<int>(address >> m_cacheline_size_shift);
      LZHAM_ASSERT(line >= 0);

      int index = bitmix32(line) & (m_cacheline_hash.size() - 1);

      for ( ; ; )
      {
         const cacheline& c = m_cacheline_hash[index];

         if (c.m_line == -1)
         {
            // not present
            return false;
         }
         else if (c.m_line == line)
         {
            return true;
         }
         
         index--;
         if (index < 0)
            index = m_cacheline_hash.size() - 1;
      }
   }
   
   bool cpucache::is_present(uint address, uint len) const
   {
      LZHAM_ASSERT(len > 0);
      const uint64 start_address = address & (~(m_cacheline_size - 1));
      const uint64 end_address = (static_cast<uint64>(address) + len - 1) & (~(m_cacheline_size - 1));
      
      for (uint64 i = start_address; i <= end_address; i += m_cacheline_size)
         if (!is_present(static_cast<uint>(i)))
            return false;
     
      return true;
   }

   uint lzcompressor::lzdecision::get_match_dist(const state& cur_state) const
   {
      if (!is_match())
         return 0;
      else if (is_rep())
      {
         int index = -m_dist - 1;
         LZHAM_ASSERT(index < CLZBase::cMatchHistSize);
         return cur_state.m_match_hist[index];
      }
      else
         return m_dist;
   }
   
   lzcompressor::state::state()
   {
      clear();
   }
   
   void lzcompressor::state::clear()
   {
      m_cur_ofs = 0;
      m_cur_state = 0;
      m_block_ofs = 0;

      for (uint i = 0; i < 2; i++)
      {
         m_rep_len_table[i].clear();
         m_large_len_table[i].clear();
      }
      m_main_table.clear();
      m_dist_lsb_table.clear();
      
      for (uint i = 0; i < (1 << CLZBase::cNumLitPredBits); i++)
         m_lit_table[i].clear();
      
      for (uint i = 0; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
         m_delta_lit_table[i].clear();
                        
      m_match_hist[0] = 1;
      m_match_hist[1] = 2;
      m_match_hist[2] = 3;
   }

   bool lzcompressor::state::init(CLZBase& lzbase, bool fast_adaptive_huffman_updating, bool use_polar_codes)
   {
      m_cur_ofs = 0;
      m_cur_state = 0;

      for (uint i = 0; i < 2; i++)
      {
         if (!m_rep_len_table[i].init(true, CLZBase::cMaxMatchLen - CLZBase::cMinMatchLen + 1, fast_adaptive_huffman_updating, use_polar_codes)) return false;
         if (!m_large_len_table[i].init(true, CLZBase::cLZXNumSecondaryLengths, fast_adaptive_huffman_updating, use_polar_codes)) return false;
      }
      if (!m_main_table.init(true, CLZBase::cLZXNumSpecialLengths + (lzbase.m_num_lzx_slots - CLZBase::cLowestUsableMatchSlot) * 8, fast_adaptive_huffman_updating, use_polar_codes)) return false;
      if (!m_dist_lsb_table.init(true, 16, fast_adaptive_huffman_updating, use_polar_codes)) return false;
      
      for (uint i = 0; i < (1 << CLZBase::cNumLitPredBits); i++)
      {
         if (!m_lit_table[i].init(true, 256, fast_adaptive_huffman_updating, use_polar_codes)) return false;
      }
      
      for (uint i = 0; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
      {
         if (!m_delta_lit_table[i].init(true, 256, fast_adaptive_huffman_updating, use_polar_codes)) return false;
      }
            
      m_match_hist[0] = 1;
      m_match_hist[1] = 2;
      m_match_hist[2] = 3;
            
      m_cur_ofs = 0;
      
      return true;
   }

   void lzcompressor::state::save_partial_state(lzcompressor::state::saved_state& dst)
   {
      dst.m_cur_ofs = m_cur_ofs;
      dst.m_cur_state = m_cur_state;
      memcpy(dst.m_match_hist, m_match_hist, sizeof(m_match_hist));
   }

   void lzcompressor::state::restore_partial_state(const lzcompressor::state::saved_state& src)
   {
      m_cur_ofs = src.m_cur_ofs;
      m_cur_state = src.m_cur_state;
      memcpy(m_match_hist, src.m_match_hist, sizeof(m_match_hist));
   }

   void lzcompressor::state::partial_advance(const lzdecision& lzdec)
   {
      if (lzdec.m_len == 0)
      {
         if (m_cur_state < 4) m_cur_state = 0; else if (m_cur_state < 10) m_cur_state -= 3; else m_cur_state -= 6;
      }
      else 
      {
         if (lzdec.m_dist < 0)
         {
            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               if (lzdec.m_len == 1)
               {
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 9 : 11;
               }
               else
               {
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
               }
            }
            else 
            {
               if (match_hist_index == 1)
               {
                  std::swap(m_match_hist[0], m_match_hist[1]);
               }
               else
               {
                  int dist = m_match_hist[2];
                  m_match_hist[2] = m_match_hist[1];
                  m_match_hist[1] = m_match_hist[0];
                  m_match_hist[0] = dist;
               }

               m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
            }           
         }
         else 
         {
            // full 
            
            update_match_hist(lzdec.get_match_dist(*this));
            
            m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
         }            
      }
                  
      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
   }
   
   uint lzcompressor::state::get_pred_char(const search_accelerator& dict, int pos, int backward_ofs) const
   {
      LZHAM_ASSERT(pos >= (int)m_block_ofs);
      int limit = pos - m_block_ofs;
      if (backward_ofs > limit)
         return 0;
      return dict[pos - backward_ofs];
   }
         
   float lzcompressor::state::get_cost(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec) const
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);
      const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);
      
      uint match_pred = lit_pred0 >> (8 - CLZBase::cNumIsMatchContextBits);
      uint match_model_index = m_cur_state + (match_pred << 4);
      float cost = m_is_match_model[match_model_index].get_cost(lzdec.is_match());
      
      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];
         
         if (m_cur_state < CLZBase::cNumLitStates)
         {
            uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
                            (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));
               
            // literal
            cost += m_lit_table[lit_pred].get_cost(lit);
         }
         else
         {
            // delta literal
            const uint rep_lit0 = get_pred_char(dict, lzdec.m_pos, m_match_hist[0]);
            const uint rep_lit1 = get_pred_char(dict, lzdec.m_pos, m_match_hist[0] + 1);
            
            uint delta_lit = rep_lit0 ^ lit;
            
            uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
                            ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);
            
            cost += m_delta_lit_table[lit_pred].get_cost(delta_lit);         
         }
      
      }
      else 
      {
         // match
         if (lzdec.m_dist < 0)
         {
            // rep match
            cost += m_is_rep_model[m_cur_state].get_cost(1);
            
            int match_hist_index = -lzdec.m_dist - 1;
                        
            if (!match_hist_index)
            {
               // rep0 match
               cost += m_is_rep0_model[m_cur_state].get_cost(1);

               if (lzdec.m_len == 1)
               {
                  // single byte rep0
                  cost += m_is_rep0_single_byte_model[m_cur_state].get_cost(1);
               }
               else
               {
                  // normal rep0
                  cost += m_is_rep0_single_byte_model[m_cur_state].get_cost(0);
                  
                  cost += m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - cMinMatchLen);
               }
            }
            else 
            {
               // rep1 or rep2 match
               cost += m_is_rep0_model[m_cur_state].get_cost(0);

               if (match_hist_index == 1)
               {
                  // rep1
                  cost += m_is_rep1_model[m_cur_state].get_cost(1);
                  
                  cost += m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - cMinMatchLen);
               }
               else
               {
                  // rep2
                  cost += m_is_rep1_model[m_cur_state].get_cost(0);
                  
                  cost += m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - cMinMatchLen);
               }
            }           
         }
         else 
         {
            cost += m_is_rep_model[m_cur_state].get_cost(0);
            
            LZHAM_ASSERT(lzdec.m_len >= cMinMatchLen);
            
            // full match
            uint match_slot, match_extra;
            lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);
            
            uint match_low_sym = 0;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;
               cost += m_large_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - 9);
            }
            else
               match_low_sym = lzdec.m_len - 2;

            uint match_high_sym = 0;

            LZHAM_ASSERT(match_slot >= CLZBase::cLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLowestUsableMatchSlot;
                                    
            uint main_sym = match_low_sym | (match_high_sym << 3);
            
            cost += m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);
            
            uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
            if (num_extra_bits < 3)
               cost += num_extra_bits;
            else
            {
               if (num_extra_bits > 4)
                  cost += (num_extra_bits - 4);

               cost += m_dist_lsb_table.get_cost(match_extra & 15);
            }
      
         }            
      }         
      
      return cost;
   }
         
   bool lzcompressor::state::encode(symbol_codec& codec, CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec)
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);
      const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);
      
      uint match_pred = lit_pred0 >> (8 - CLZBase::cNumIsMatchContextBits);
      uint match_model_index = m_cur_state + (match_pred << 4);
      if (!codec.encode(lzdec.is_match(), m_is_match_model[match_model_index])) return false;
      
      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];

#ifdef LZDEBUG
         if (!codec.encode_bits(lit, 8)) return false;
#endif

         if (m_cur_state < CLZBase::cNumLitStates)
         {
            uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
               (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));
               
            // literal
            if (!codec.encode(lit, m_lit_table[lit_pred])) return false;
         }
         else
         {
            // delta literal
            const uint rep_lit0 = get_pred_char(dict, lzdec.m_pos, m_match_hist[0]);
            const uint rep_lit1 = get_pred_char(dict, lzdec.m_pos, m_match_hist[0] + 1);

            uint delta_lit = rep_lit0 ^ lit;

            uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
                            ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);
            
#ifdef LZDEBUG
            if (!codec.encode_bits(rep_lit0, 8)) return false;
#endif
            
            if (!codec.encode(delta_lit, m_delta_lit_table[lit_pred])) return false;
         }
         
         if (m_cur_state < 4) m_cur_state = 0; else if (m_cur_state < 10) m_cur_state -= 3; else m_cur_state -= 6;
      }
      else 
      {
         // match
         if (lzdec.m_dist < 0)
         {
            // rep match
            if (!codec.encode(1, m_is_rep_model[m_cur_state])) return false;

            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               // rep0 match
               if (!codec.encode(1, m_is_rep0_model[m_cur_state])) return false;

               if (lzdec.m_len == 1)
               {
                  // single byte rep0
                  if (!codec.encode(1, m_is_rep0_single_byte_model[m_cur_state])) return false;
                  
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 9 : 11;
               }
               else
               {
                  // normal rep0
                  if (!codec.encode(0, m_is_rep0_single_byte_model[m_cur_state])) return false;
                  
                  if (!codec.encode(lzdec.m_len - cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
               }
            }
            else 
            {
               // rep1 or rep2 match
               if (!codec.encode(0, m_is_rep0_model[m_cur_state])) return false;
               
               if (match_hist_index == 1)
               {
                  // rep1
                  if (!codec.encode(1, m_is_rep1_model[m_cur_state])) return false;

                  if (!codec.encode(lzdec.m_len - cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  
                  std::swap(m_match_hist[0], m_match_hist[1]);
               }
               else
               {
                  // rep2
                  if (!codec.encode(0, m_is_rep1_model[m_cur_state])) return false;

                  if (!codec.encode(lzdec.m_len - cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  
                  int dist = m_match_hist[2];
                  m_match_hist[2] = m_match_hist[1];
                  m_match_hist[1] = m_match_hist[0];
                  m_match_hist[0] = dist;
               }
               
               m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
            }           
         }
         else
         {
            if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;
                                                
            LZHAM_ASSERT(lzdec.m_len >= cMinMatchLen);
            
            // full match
            uint match_slot, match_extra;
            lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

            uint match_low_sym = 0;
            int large_len_sym = -1;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;
               
               large_len_sym = lzdec.m_len - 9;
            }
            else
               match_low_sym = lzdec.m_len - 2;
                        
            uint match_high_sym = 0;

            LZHAM_ASSERT(match_slot >= CLZBase::cLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLowestUsableMatchSlot;

            uint main_sym = match_low_sym | (match_high_sym << 3);

            if (!codec.encode(CLZBase::cLZXNumSpecialLengths + main_sym, m_main_table)) return false;
            
            if (large_len_sym >= 0)
            {
               if (!codec.encode(large_len_sym, m_large_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
            }
               
            uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
            if (num_extra_bits < 3)
            {
               if (!codec.encode_bits(match_extra, num_extra_bits)) return false;
            }
            else
            {
               if (num_extra_bits > 4)
               {
                  if (!codec.encode_bits((match_extra >> 4), num_extra_bits - 4)) return false;
               }

               if (!codec.encode(match_extra & 15, m_dist_lsb_table)) return false;
            }
            
            update_match_hist(lzdec.m_dist);
                                    
            m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
         }
         
#ifdef LZDEBUG         
         if (!codec.encode_bits(m_match_hist[0], 23)) return false;
#endif
      }
                  
      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
      return true;
   }
   
   bool lzcompressor::state::encode_eob(symbol_codec& codec, const search_accelerator& dict)
   {
      uint match_pred = dict.get_cur_dict_size() ? (dict.get_char(-1) >> (8 - CLZBase::cNumIsMatchContextBits)) : 0;
      uint match_model_index = m_cur_state + (match_pred << 4);
      if (!codec.encode(1, m_is_match_model[match_model_index])) return false;

      // full match
      if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;
            
      return codec.encode(0, m_main_table);
   }   
   
   void lzcompressor::state::update_match_hist(uint match_dist) 
   {
      m_match_hist[2] = m_match_hist[1];
      m_match_hist[1] = m_match_hist[0];
      m_match_hist[0] = match_dist;
   }

   int lzcompressor::state::find_match_dist(uint match_dist) const
   {
      for (uint match_hist_index = 0; match_hist_index < CLZBase::cMatchHistSize; match_hist_index++)
         if (match_dist == m_match_hist[match_hist_index])
            return match_hist_index;
            
      return -1;
   }
   
   void lzcompressor::state::reset_match_hist()
   {
      m_match_hist[0] = 1;
      m_match_hist[1] = 2;
      m_match_hist[2] = 3;
      m_cur_state = 0;
   }
   
   void lzcompressor::state::start_of_block(const search_accelerator& dict, uint cur_ofs)
   {
      dict;
      reset_match_hist();
      
      m_cur_ofs = cur_ofs;
      m_block_ofs = cur_ofs;
   }
   
   void lzcompressor::coding_stats::clear()
   {
      m_total_bytes = 0;
      m_total_contexts = 0;
      m_total_match_bits_cost = 0;
      m_worst_match_bits_cost = 0;
      m_total_match0_bits_cost = 0;
      m_total_match1_bits_cost = 0;
      m_total_nonmatches = 0;
      m_total_matches = 0;
      m_total_cost = 0.0f;
      m_total_lits = 0;
      m_total_lit_cost = 0;
      m_worst_lit_cost = 0;
      m_total_delta_lits = 0;
      m_total_delta_lit_cost = 0;
      m_worst_delta_lit_cost = 0;
      m_total_rep0_len1_matches = 0;
      m_total_reps = 0;
      m_total_rep0_len1_cost = 0;
      m_worst_rep0_len1_cost = 0;
      utils::zero_object(m_total_rep_matches);
      utils::zero_object(m_total_rep_cost);
      utils::zero_object(m_total_full_matches);
      utils::zero_object(m_total_full_match_cost);
      utils::zero_object(m_worst_rep_cost);
      utils::zero_object(m_worst_full_match_cost);
   }
   
   void lzcompressor::coding_stats::print()
   {
      if (!m_total_contexts)
         return;
         
      printf("-----------\n");
      printf("Coding statistics:\n");
      printf("Total Bytes: %u, Total Contexts: %u, Total Cost: %f bits (%f bytes), Ave context cost: %f\n", m_total_bytes, m_total_contexts, m_total_cost, m_total_cost / 8.0f, m_total_cost / m_total_contexts);
      printf("Ave bytes per context: %f\n", m_total_bytes / (float)m_total_contexts);
      
      printf("IsMatch:\n");
      printf("  Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", 
         m_total_contexts, m_total_match_bits_cost, m_total_match_bits_cost / 8.0f, m_total_match_bits_cost / math::maximum<uint>(1, m_total_contexts), m_worst_match_bits_cost);
         
      printf("  IsMatch0: %u, Cost: %f (%f bytes), Ave. Cost: %f\n", 
         m_total_nonmatches, m_total_match0_bits_cost, m_total_match0_bits_cost / 8.0f, m_total_match0_bits_cost / math::maximum<uint>(1, m_total_nonmatches));
      printf("  IsMatch1: %u, Cost: %f (%f bytes), Ave. Cost: %f\n", 
         m_total_matches, m_total_match1_bits_cost, m_total_match1_bits_cost / 8.0f, m_total_match1_bits_cost / math::maximum<uint>(1, m_total_matches));
      
      printf("Literal stats:\n");
      printf("  Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", m_total_lits, m_total_lit_cost, m_total_lit_cost / 8.0f, m_total_lit_cost / math::maximum<uint>(1, m_total_lits), m_worst_lit_cost);
      
      printf("Delta literal stats:\n");
      printf("  Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", m_total_delta_lits, m_total_delta_lit_cost, m_total_delta_lit_cost / 8.0f, m_total_delta_lit_cost / math::maximum<uint>(1, m_total_delta_lits), m_worst_delta_lit_cost);
      
      printf("Rep0 Len1 stats:\n");
      printf("  Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", m_total_rep0_len1_matches, m_total_rep0_len1_cost, m_total_rep0_len1_cost / 8.0f, m_total_rep0_len1_cost / math::maximum<uint>(1, m_total_rep0_len1_matches), m_worst_rep0_len1_cost);
      
      for (uint i = 0; i < CLZBase::cMatchHistSize; i++)
      {
         printf("Rep %u stats:\n", i);
         printf("  Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", m_total_rep_matches[i], m_total_rep_cost[i], m_total_rep_cost[i] / 8.0f, m_total_rep_cost[i] / math::maximum<uint>(1, m_total_rep_matches[i]), m_worst_rep_cost[i]);
      }
                              
      for (uint i = CLZBase::cMinMatchLen; i <= CLZBase::cMaxMatchLen; i++)
      {
         printf("Match %u: Total: %u, Cost: %f (%f bytes), Ave. Cost: %f, Worst Cost: %f\n", i, 
            m_total_full_matches[i], m_total_full_match_cost[i], m_total_full_match_cost[i] / 8.0f, m_total_full_match_cost[i] / math::maximum<uint>(1, m_total_full_matches[i]), m_worst_full_match_cost[i]);
      }
   }
   
   void lzcompressor::coding_stats::update(const lzdecision& lzdec, const state& cur_state, const search_accelerator& dict)
   {
      float cost = lzdec.m_cost;
      m_total_bytes += lzdec.get_len();
      m_total_contexts++;

      m_total_cost += cost;
      
      uint match_pred = lzdec.m_pos ? (dict[lzdec.m_pos - 1] >> (8 - CLZBase::cNumIsMatchContextBits)) : 0;
      uint match_model_index = cur_state.m_cur_state + (match_pred << 4);
                  
      if (lzdec.m_len == 0)
      {
         float match_bit_cost = cur_state.m_is_match_model[match_model_index].get_cost(0);
         m_total_match0_bits_cost += match_bit_cost;
         m_total_match_bits_cost += match_bit_cost;
         m_worst_match_bits_cost = math::maximum<double>(m_worst_match_bits_cost, match_bit_cost);
         m_total_nonmatches++;
         
         if (cur_state.m_cur_state < CLZBase::cNumLitStates)
         {
            m_total_lits++;
            m_total_lit_cost += cost;
            m_worst_lit_cost = math::maximum<double>(m_worst_lit_cost, cost);
         }
         else
         {
            m_total_delta_lits++;
            m_total_delta_lit_cost += cost;
            m_worst_delta_lit_cost = math::maximum<double>(m_worst_delta_lit_cost, cost);
         }
      }
      else
      {
         float match_bit_cost = cur_state.m_is_match_model[match_model_index].get_cost(1);
         m_total_match1_bits_cost += match_bit_cost;
         m_total_match_bits_cost += match_bit_cost;
         m_worst_match_bits_cost = math::maximum<double>(m_worst_match_bits_cost, match_bit_cost);
         m_total_matches++;
         
         if (lzdec.m_dist < 0)
         {
            m_total_reps++;

            // rep match

            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               // rep0 match
               if (lzdec.m_len == 1)
               {
                  m_total_rep0_len1_matches++;
                  m_total_rep0_len1_cost += cost;
                  m_worst_rep0_len1_cost = math::maximum<double>(m_worst_rep0_len1_cost, cost);
               }
               else
               {
                  m_total_rep_matches[0]++;
                  m_total_rep_cost[0] += cost;
                  m_worst_rep_cost[0] = math::maximum<double>(m_worst_rep_cost[0], cost);
               }
            }
            else 
            {
               LZHAM_ASSERT(match_hist_index < CLZBase::cMatchHistSize);
               m_total_rep_matches[match_hist_index]++;
               m_total_rep_cost[match_hist_index] += cost;
               m_worst_rep_cost[match_hist_index] = math::maximum<double>(m_worst_rep_cost[match_hist_index], cost);
            }           
         }
         else
         {
            m_total_full_matches[lzdec.get_len()]++;
            m_total_full_match_cost[lzdec.get_len()] += cost;
            m_worst_full_match_cost[lzdec.get_len()] = math::maximum<double>(m_worst_full_match_cost[lzdec.get_len()], cost);
         }
      }
   }

   lzcompressor::lzcompressor() :
      m_src_size(-1),
      m_src_adler32(0),
      m_step(0),
      m_finished(false),
      m_start_dict_ofs(0),
      m_block_index(0)
   {
      LZHAM_VERIFY( ((uint32_ptr)this & (LZHAM_GET_ALIGNMENT(lzcompressor) - 1)) == 0);
   }

   bool lzcompressor::init(const init_params& params)
   {
      clear();
      
      if ((params.m_dict_size_log2 < CLZBase::cMinDictSizeLog2) || (params.m_dict_size_log2 > CLZBase::cMaxDictSizeLog2))
         return false;
      if ((params.m_compression_level < 0) || (params.m_compression_level > cCompressionLevelCount))
         return false;
                     
      m_params = params;
      m_settings = s_settings[params.m_compression_level];
                              
      const uint dict_size = 1U << m_params.m_dict_size_log2;
      
      uint max_block_size = dict_size / 8;
      if (m_params.m_block_size > max_block_size)
      {
         m_params.m_block_size = max_block_size;
      }
      
      uint max_helper_threads = params.m_max_helper_threads;
      if (m_params.m_block_size < 16384)
      {
         max_helper_threads = 0;
      }
      
      if (!m_accel.init(this, params.m_pTask_pool, max_helper_threads, dict_size, m_settings.m_match_accel_max_matches_per_probe, params.m_num_cachelines > 0, m_settings.m_match_accel_max_probes))
         return false;
                                    
      init_position_slots(params.m_dict_size_log2);
      
      if (!m_state.init(*this, m_settings.m_fast_adaptive_huffman_updating, m_settings.m_use_polar_codes)) 
         return false;
                  
      if (!m_block_buf.try_reserve(m_params.m_block_size))
         return false;
         
      if (!m_comp_buf.try_reserve(m_params.m_block_size*2))
         return false;
      
      if (m_params.m_num_cachelines)
      {
         if (!m_cpucache.init(m_params.m_num_cachelines, m_params.m_cacheline_size))
            return false;
      }
                  
      return true;
   }
   
   void lzcompressor::clear()
   {
      m_codec.clear();
      m_state.clear();
      m_src_size = 0;
      m_src_adler32 = cInitAdler32;
      m_block_buf.clear();
      m_comp_buf.clear();
      m_best_decisions.clear();
      m_step = 0;
      m_finished = false;
      m_start_dict_ofs = 0;
      for (uint i = 0; i < cMaxParseGraphNodes; i++)
         m_nodes[i].clear();      
      m_cpucache.clear();
      m_block_index = 0;
   }
   
   bool lzcompressor::code_decision(lzdecision lzdec, uint& cur_ofs, uint& bytes_to_match, bool recompute_cost, float *pActual_cost)
   {
      if (pActual_cost)
      {
         if (recompute_cost)
            lzdec.m_cost = m_state.get_cost(*this, m_accel, lzdec);
         else
         {
            LZHAM_ASSERT(lzdec.m_cost == m_state.get_cost(*this, m_accel, lzdec));
         }

         m_stats.update(lzdec, m_state, m_accel);
         
         *pActual_cost = lzdec.m_cost;
      }            

#ifdef LZDEBUG      
      if (!m_codec.encode_bits(666, 12)) return false;
      if (!m_codec.encode_bits(lzdec.is_match(), 1)) return false;
      if (!m_codec.encode_bits(lzdec.get_len(), 9)) return false;
      if (!m_codec.encode_bits(m_state.m_cur_state, 4)) return false;
#endif         

#ifdef LZVERIFY
      if (lzdec.is_match())
      {
         uint match_dist = lzdec.get_match_dist(m_state);

         LZHAM_VERIFY(m_accel[cur_ofs] == m_accel[(cur_ofs - match_dist) & (m_accel.get_max_dict_size() - 1)]);
      }
#endif               

      const uint len = lzdec.get_len();
      
      if (m_params.m_num_cachelines)
      {
         const uint abs_ofs = m_accel.get_lookahead_pos();
         
         m_cpucache.touch(abs_ofs, len);

         if (lzdec.is_match())
         {
            uint match_dist = lzdec.get_match_dist(m_state);
            m_cpucache.touch(abs_ofs - match_dist, len);
         }
         else if (m_state.will_reference_last_match(lzdec))
         {
            uint match_dist = m_state.m_match_hist[0];
            if (abs_ofs >= match_dist)
               m_cpucache.touch(abs_ofs - match_dist, 1);
         }
      }

      if (!m_state.encode(m_codec, *this, m_accel, lzdec)) return false;
      m_step++;
      
      cur_ofs += len;
      LZHAM_ASSERT(bytes_to_match >= len);
      bytes_to_match -= len;  
      
      m_accel.advance_bytes(len);             
      return true;
   }
   
   bool lzcompressor::put_bytes(const void* pBuf, uint buf_len)
   {
      LZHAM_ASSERT(!m_finished);
      if (m_finished)
         return false;
         
      bool status = true;
      
      if (!pBuf)
      {
         if (m_block_buf.size())
         {
            status = compress_block(m_block_buf.get_ptr(), m_block_buf.size());
                        
            m_block_buf.try_resize(0);
         }
         
         if (status)
         {
            if (!send_final_block())
            {
               status = false;
            }
         }
         
         m_finished = true;
      }
      else
      {
         const uint8 *pSrcBuf = static_cast<const uint8*>(pBuf);
         uint num_src_bytes_remaining = buf_len;
         
         while (num_src_bytes_remaining)
         {
            const uint num_bytes_to_copy = LZHAM_MIN(num_src_bytes_remaining, m_params.m_block_size - m_block_buf.size());
            
            if (num_bytes_to_copy == m_params.m_block_size)
            {
               LZHAM_ASSERT(!m_block_buf.size());
               
               status = compress_block(pSrcBuf, num_bytes_to_copy);
            }
            else
            {
               if (!m_block_buf.append(static_cast<const uint8 *>(pSrcBuf), num_bytes_to_copy)) return false;
               
               LZHAM_ASSERT(m_block_buf.size() <= m_params.m_block_size);
               
               if (m_block_buf.size() == m_params.m_block_size)
               {
                  status = compress_block(m_block_buf.get_ptr(), m_block_buf.size());
                              
                  m_block_buf.try_resize(0);
               }
            }               
            
            pSrcBuf += num_bytes_to_copy;
            num_src_bytes_remaining -= num_bytes_to_copy;
         }            
      }
      
      return status;
   }
   
   bool lzcompressor::send_final_block()
   {
      //m_codec.clear();         

      if (!m_codec.start_encoding(16)) 
         return false;

#ifdef LZDEBUG              
      if (!m_codec.encode_bits(166, 12)) 
         return false;
#endif

      if (!m_block_index)
      {
         if (!send_configuration())
            return false;
      }

      if (!m_codec.encode_bits(cEOFBlock, cBlockHeaderBits)) 
         return false;
      
      if (!m_codec.encode_align_to_byte()) 
         return false;

      if (!m_codec.encode_bits(m_src_adler32, 32)) 
         return false;

      if (!m_codec.stop_encoding(true)) 
         return false;

      if (!m_comp_buf.append(m_codec.get_encoding_buf())) 
         return false;
      
      m_block_index++;
      
      //m_stats.print();
      return true;
   }
   
   bool lzcompressor::send_configuration()
   {
      if (!m_codec.encode_bits(m_settings.m_fast_adaptive_huffman_updating, 1)) 
         return false;
      if (!m_codec.encode_bits(m_settings.m_use_polar_codes, 1)) 
         return false;
         
      return true;   
   }
 
   bool lzcompressor::compress_block(const void* pBuf, uint buf_len)
   {
      LZHAM_ASSERT(pBuf);
      LZHAM_ASSERT(buf_len <= m_params.m_block_size);
            
      LZHAM_ASSERT(m_src_size >= 0);
      if (m_src_size < 0)
         return false;
            
      m_src_size += buf_len;
      m_src_adler32 = adler32(pBuf, buf_len, m_src_adler32);

      m_accel.add_bytes_begin(buf_len, static_cast<const uint8*>(pBuf));
            
      m_start_dict_ofs = m_accel.get_lookahead_pos() & (m_accel.get_max_dict_size() - 1); 

      uint cur_ofs = m_start_dict_ofs;
            
      uint bytes_to_match = buf_len;
      
      lzham::vector<lzdecision> lzdecisions0;
      if (!lzdecisions0.try_reserve(64))
         return false;
      
      lzham::vector<lzdecision> lzdecisions1;
      if (!lzdecisions1.try_reserve(64))
         return false;
      
      m_codec.start_encoding((buf_len * 9) / 8);

#ifdef LZDEBUG              
      m_codec.encode_bits(166, 12); 
#endif

      if (!m_block_index)
      {
         if (!send_configuration())
            return false;
      }
      
      if (!m_codec.encode_bits(cCompBlock, cBlockHeaderBits))
         return false;
                  
      m_state.start_of_block(m_accel, cur_ofs);
            
      state initial_state(m_state);
                  
      state::saved_state temp_state;
      coding_stats initial_stats(m_stats);
      
      uint initial_step = m_step;
                  
#ifdef LZVERIFY
      for (uint i = 0; i < bytes_to_match; i++)
      {
         uint cur_ofs = m_start_dict_ofs + i;
         uint largest_match_index = enumerate_lz_decisions(cur_ofs, m_state, lzdecisions0);
         if (lzdecisions0.empty()) return false;
         
         float largest_match_cost = lzdecisions0[largest_match_index].m_cost;
         uint largest_match_len = lzdecisions0[largest_match_index].get_len();
         
         for (uint j = 0; j < lzdecisions0.size(); j++)
         {
            const lzdecision& lzdec = lzdecisions0[j];
            
            if (lzdec.is_match())
            {
               uint match_dist = lzdec.get_match_dist(m_state);

               for (uint k = 0; k < lzdec.get_len(); k++)
               {
                  LZHAM_VERIFY(m_accel[cur_ofs+k] == m_accel[(cur_ofs+k - match_dist) & (m_accel.get_max_dict_size() - 1)]);
               }
            }
         }
      }
#endif      
      
      while (bytes_to_match)   
      {
         uint largest_match_index = enumerate_lz_decisions(cur_ofs, m_state, lzdecisions0);
         if (lzdecisions0.empty()) 
            return false;
         
         uint largest_match_len = lzdecisions0[largest_match_index].get_len();
                  
         if ( (largest_match_len == 1) || (largest_match_len >= m_settings.m_non_optimal_parse_match_len_thresh) ||
              ((m_settings.m_non_optimal_parse_if_hist_match) && (lzdecisions0[largest_match_index].is_rep())) 
            )
         {
            if (!code_decision(lzdecisions0[largest_match_index], cur_ofs, bytes_to_match, false, NULL)) 
               return false;
         }
         else
         {
            // Dijkstra's algorithm
            m_state.save_partial_state(temp_state);
            
            m_nodes[0].m_visited = false;
            m_nodes[0].m_parent = -1;
            m_nodes[0].m_total_cost = 0.0f;
            m_nodes[0].m_total_complexity = 0;
            
#ifdef LZHAM_BUILD_DEBUG            
            for (uint i = 1; i < m_settings.m_max_parse_graph_nodes; i++)
            {
               LZHAM_ASSERT(!m_nodes[i].m_visited);
               LZHAM_ASSERT(m_nodes[i].m_total_cost == math::cNearlyInfinite);
               LZHAM_ASSERT(m_nodes[i].m_total_complexity == UINT_MAX);
            }
#endif
            
            uint unvisted_nodes[cMaxParseGraphNodes];
            unvisted_nodes[0] = 0;
            uint num_unvisted_nodes = 1;
            
            int root_node_index = -1;
            
            const uint max_lookahead_bytes = math::minimum<uint>(bytes_to_match - 1, (m_settings.m_max_parse_graph_nodes - 1U));
            
            uint highest_unvisted_node_index = 0;

            for ( ; ; )
            {
               float lowest_cost = math::cNearlyInfinite;
               uint lowest_complexity = UINT_MAX;
               int cur_node_index = -1;

               uint unvisited_node_index = 0;
               for (uint j = 0; j < num_unvisted_nodes; j++)
               {
                  const uint i = unvisted_nodes[j];

                  LZHAM_ASSERT(!m_nodes[i].m_visited);
                  LZHAM_ASSERT(m_nodes[i].m_total_cost < math::cNearlyInfinite);

                  if ( (m_nodes[i].m_total_cost < lowest_cost) ||
                       ((m_nodes[i].m_total_cost == lowest_cost) && (m_nodes[i].m_total_complexity < lowest_complexity)) )
                  {
                     cur_node_index = i;
                     unvisited_node_index = j;

                     lowest_cost = m_nodes[i].m_total_cost;
                     lowest_complexity = m_nodes[i].m_total_complexity;
                  }
               }

               LZHAM_ASSERT(cur_node_index >= 0);

               node& cur_node = m_nodes[cur_node_index];
               cur_node.m_visited = true;

               if (cur_node_index == (int)max_lookahead_bytes)
               {
                  root_node_index = cur_node_index;
                  break;
               }

               utils::swap(unvisted_nodes[num_unvisted_nodes - 1], unvisted_nodes[unvisited_node_index]);
               num_unvisted_nodes--;

               const lzham::vector<lzdecision>* pLZDecisions = &lzdecisions0;
               if (cur_node.m_parent >= 0)
               {
                  m_state.restore_partial_state(cur_node.m_parent_state);
                  m_state.partial_advance(cur_node.m_lzdec);

                  enumerate_lz_decisions(cur_ofs + cur_node_index, m_state, m_temp_lzdecisions);
                  if (m_temp_lzdecisions.empty()) return false;

                  pLZDecisions = &m_temp_lzdecisions;
               }
               
               const uint max_len = max_lookahead_bytes - cur_node_index;

               for (uint i = 0; i < pLZDecisions->size(); i++)
               {
                  const lzdecision& lzdec = (*pLZDecisions)[i];

                  const uint max_match_len = math::minimum<uint>(max_len, lzdec.get_len());
                  
                  const uint min_match_len = (lzdec.m_dist == -1) ? 1 : cMinMatchLen;
                  uint cur_match_len = math::minimum<uint>(min_match_len, max_match_len);
                                                      
                  if ( (max_match_len > m_settings.m_match_truncation_disable_match_len_thresh) || 
                       (lzdec.get_match_dist(m_state) < m_settings.m_match_truncation_disable_match_dist_thresh) ||
                       ((m_settings.m_match_truncation_disable_if_hist_match) && (lzdec.is_rep())) 
                      )
                  {
                     // Don't even try truncating the match if it's long and/or close enough.
                     cur_match_len = max_match_len;
                  }
                                       
                  if (lzdec.get_len() > max_len)
                     cur_match_len = max_match_len;
                  
                  for ( ; cur_match_len <= max_match_len; cur_match_len++)
                  {
                     int child_node_index = cur_node_index + cur_match_len;
                     LZHAM_ASSERT((child_node_index >= 0) && (child_node_index < (int)m_settings.m_max_parse_graph_nodes));
                     node& child_node = m_nodes[child_node_index];

                     if (child_node.m_visited)
                        continue;
                                          
                     lzdecision actual_lzdec(lzdec);
                     
                     const bool is_truncated = ((int)cur_match_len != lzdec.get_len()) && (child_node_index != (int)max_lookahead_bytes);
                     if (is_truncated)
                     {
                        actual_lzdec.m_len = cur_match_len;
                        actual_lzdec.m_cost = m_state.get_cost(*this, m_accel, actual_lzdec);
                        
                        if (cur_match_len <= 4)
                        {
                           if (actual_lzdec.m_cost > (8.2f * cur_match_len))
                              continue;
                        }
                        
                        if ((m_params.m_num_cachelines) && (actual_lzdec.is_match()) && (!actual_lzdec.is_rep()))
                        {
                           // If the full-length match was present in the cache, the truncated version must also be present.
                           // Otherwise, we need to check again in case the truncated match is fully present.
                           if (!actual_lzdec.m_present_in_cpucache)
                           {
                              uint abs_ofs = m_accel.get_lookahead_pos() + cur_node_index;
                              actual_lzdec.m_present_in_cpucache = m_cpucache.is_present(abs_ofs - actual_lzdec.m_dist, actual_lzdec.get_len());
                           }                              
                        }
                     }                           
                                          
                     float child_total_cost = cur_node.m_total_cost + actual_lzdec.get_cost_plus_penalty();
                     uint child_total_complexity = cur_node.m_total_complexity + actual_lzdec.get_complexity();

                     if ( (child_total_cost < child_node.m_total_cost) ||
                          ((child_total_cost == child_node.m_total_cost) && (child_total_complexity < child_node.m_total_complexity)) )
                     {
                        if (child_node.m_total_complexity == UINT_MAX)
                        {
                           highest_unvisted_node_index = math::maximum<uint>(highest_unvisted_node_index, child_node_index);

                           unvisted_nodes[num_unvisted_nodes] = child_node_index;
                           num_unvisted_nodes++;
                        }

                        child_node.m_total_cost = child_total_cost;
                        child_node.m_total_complexity = child_total_complexity;
                        child_node.m_parent = (int16)cur_node_index;
                        m_state.save_partial_state(child_node.m_parent_state);
                        child_node.m_lzdec = actual_lzdec;
                     }
                  }                     
               }

               // The graph search is now going through a single node - it's better to stop the search now and let all of the 
               // adaptive models to update their statistics.
               if (num_unvisted_nodes == 1)
               {
                  root_node_index = unvisted_nodes[0];
                  break;
               }
            }
            
            m_state.restore_partial_state(temp_state);
                        
            int cur_node_index = root_node_index;
            
            m_best_decisions.try_resize(0);
            
            float total_projected_cost[cMaxParseGraphNodes];
            while (cur_node_index > 0)
            {
               LZHAM_ASSERT((cur_node_index >= 0) && (cur_node_index < (int)m_settings.m_max_parse_graph_nodes));
               node& cur_node = m_nodes[cur_node_index];

               if ( (cur_node_index != ((int)m_settings.m_max_parse_graph_nodes - 1)) || (m_nodes[cur_node_index].m_parent == 0) )
               {
                  total_projected_cost[m_best_decisions.size()] = cur_node.m_total_cost;
                     
                  if (!m_best_decisions.try_push_back(cur_node.m_lzdec))
                     return false;
               }
                                                   
               cur_node_index = m_nodes[cur_node_index].m_parent;
            }
                           
            bool recompute_cost = false;
                                    
            for (int i = m_best_decisions.size() - 1; i >= 0; --i)
            {
               LZHAM_ASSERT(m_best_decisions[i].m_pos == (int)cur_ofs);
               
#if LZHAM_UPDATE_STATS               
               float actual_cost;
               if (!code_decision(m_best_decisions[i], cur_ofs, bytes_to_match, recompute_cost, &actual_cost)) 
#else
               if (!code_decision(m_best_decisions[i], cur_ofs, bytes_to_match, recompute_cost, NULL)) 
#endif               
                  return false;
                  
               recompute_cost = true;
            }
                                    
            for (int i = 1; i <= (int)highest_unvisted_node_index; i++)
               m_nodes[i].clear();
         }  
      }
                  
      m_accel.add_bytes_end();
      
#ifdef LZDEBUG      
      if (!m_codec.encode_bits(666, 12)) return false;
      if (!m_codec.encode_bits(1, 1)) return false;
      if (!m_codec.encode_bits(0, 9)) return false;
      if (!m_codec.encode_bits(m_state.m_cur_state, 4)) return false;
#endif      
                  
      if (!m_state.encode_eob(m_codec, m_accel))
         return false;
      
#ifdef LZDEBUG              
      if (!m_codec.encode_bits(366, 12)) return false;
#endif
      
      if (!m_codec.stop_encoding(true)) return false;
      
      uint compressed_size = m_codec.get_encoding_buf().size();
#ifdef LZDEBUG            
      //if (0)
      if (compressed_size & 1)
#else      
      if (compressed_size >= buf_len)
#endif
      {
         m_state = initial_state;
         m_step = initial_step;
         m_stats = initial_stats;
         
         m_codec.clear();
         
         if (!m_codec.start_encoding(buf_len + 16)) return false;
         
#ifdef LZDEBUG              
         if (!m_codec.encode_bits(166, 12)) return false; 
#endif
         if (!m_block_index)
         {
            if (!send_configuration())
               return false;
         }
                           
         if (!m_codec.encode_bits(cRawBlock, cBlockHeaderBits)) return false;
         
         LZHAM_ASSERT(buf_len <= 0x1000000);
         if (!m_codec.encode_bits(buf_len - 1, 24)) return false;
         if (!m_codec.encode_align_to_byte()) return false;

         const uint8* pSrc = m_accel.get_ptr(m_start_dict_ofs);

         for (uint i = 0; i < buf_len; i++)
         {
            if (!m_codec.encode_bits(*pSrc++, 8)) return false;
         }
         
         if (!m_codec.stop_encoding(true)) return false;
      }  
      
      if (!m_comp_buf.append(m_codec.get_encoding_buf()))
         return false;

      m_block_index++;
                                   
      return true;
   }
         
   uint lzcompressor::enumerate_lz_decisions(uint ofs, const state& cur_state, lzham::vector<lzdecision>& decisions)
   {  
      uint start_ofs = m_accel.get_lookahead_pos() & m_accel.get_max_dict_size_mask();
      LZHAM_ASSERT(ofs >= start_ofs);
      const uint lookahead_ofs = ofs - start_ofs;
      
      uint largest_index = 0;
      uint largest_len = 1;
      
      if (!decisions.try_resize(1))
         return 0;
      lzdecision& lit_dec = decisions[0];
      lit_dec.init(ofs, 0, 0);
      float largest_cost = cur_state.get_cost(*this, m_accel, lit_dec);
      lit_dec.m_cost = largest_cost;
                        
      uint match_hist_max_len = 0;

      for (uint i = 0; i < cMatchHistSize; i++)
      {
         uint hist_match_len = m_accel.match(lookahead_ofs, cur_state.m_match_hist[i]);
         if (hist_match_len)
         {
            match_hist_max_len = math::maximum(match_hist_max_len, hist_match_len);
            if ( ((hist_match_len == 1) && (i == 0)) || (hist_match_len >= CLZBase::cMinMatchLen) )
            {
               lzdecision dec(ofs, hist_match_len, -((int)i + 1));
               dec.m_cost = cur_state.get_cost(*this, m_accel, dec);
               if (!decisions.try_push_back(dec))
                  return false;
               
               if ( (hist_match_len > largest_len) || ((hist_match_len == largest_len) && (dec.m_cost < largest_cost)) )
               {
                  largest_index = decisions.size() - 1;
                  largest_len = hist_match_len;
                  largest_cost = dec.m_cost;
               }
            }
         }               
      }
               
      if (match_hist_max_len < m_settings.m_hist_match_len_disable_full_match_find_thresh)
      {
         const search_accelerator::dict_match* pMatches = m_accel.find_matches(lookahead_ofs);

         if (pMatches)
         {
            for ( ; ; )
            {  
               uint match_len = pMatches->get_len();
               LZHAM_ASSERT((pMatches->get_dist() > 0) && (pMatches->get_dist() <= m_dict_size));
               
               if (match_len > match_hist_max_len)
               {
                  lzdecision dec(ofs, match_len, pMatches->get_dist());
                  dec.m_cost = cur_state.get_cost(*this, m_accel, dec);
                  // Immediately reject small matches that get expanded too much.
                  if ((match_len > 4) || (dec.m_cost <= match_len * 8.2f))
                  {
                     if ((m_params.m_num_cachelines) && (dec.is_match()))
                     {
                        // Only check if the match points back far enough (before the lookahead), because
                        // we only update the cpucache during encoding after all matches are decided.
                        int highest_ofs = lookahead_ofs - dec.m_dist + dec.get_len() - 1;
                        if (highest_ofs < 0)
                        {
                           dec.m_present_in_cpucache = m_cpucache.is_present(
                              (lookahead_ofs + m_accel.get_lookahead_pos()) - dec.m_dist, dec.get_len());
                        }
                        else
                        {
                           dec.m_present_in_cpucache = true;
                        }
                     }
                     
                     if (!decisions.try_push_back(dec))
                        return false;
                     
                     if ( (match_len > largest_len) || ((match_len == largest_len) && (dec.get_cost_plus_penalty() < largest_cost)) )
                     {
                        largest_index = decisions.size() - 1;
                        largest_len = match_len;
                        largest_cost = dec.get_cost_plus_penalty();
                     }
                  }
               }
               if (pMatches->is_last())
                  break;
               pMatches++;
            }
         }            
      }         
      
      return largest_index;
   }
   
} // namespace lzham
   
