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

// Small (<= 4) matches that cost more than match_len*LZHAM_SMALL_MATCH_EXPANSION_BITS_PER_BYTE_THRESH bits are immediately discarded.
// This results in a small win on binary files, and a small loss on text, but I'm not sure exactly why.
#define LZHAM_SMALL_MATCH_EXPANSION_BITS_PER_BYTE_THRESH 8.2f
#define LZHAM_DISCARD_SMALL_EXPANDED_MATCHES 1

// Update and print high-level coding statistics if set to 1.
// TODO: Add match distance coding statistics.
#define LZHAM_UPDATE_STATS 0

#define LZHAM_FORCE_SINGLE_THREADED_PARSING 0

#define LZHAM_VERIFY_MATCH_COSTS 0

namespace lzham
{
   // TODO: This table is now outdated since alpha3.
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
         cMaxParseGraphNodes,    // m_max_parse_graph_nodes
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
         cMaxParseGraphNodes,    // m_max_parse_graph_nodes
      },
      // cCompressionLevelDefault
      {
         false,                  // m_fast_adaptive_huffman_updating
         true,                   // m_use_polar_codes
         false,                  // m_non_optimal_parse_if_hist_match
         16,                     // m_non_optimal_parse_match_len_thresh
         32,                     // m_match_truncation_disable_match_len_thresh
         0,                      // m_match_truncation_disable_match_dist_thresh
         false,                  // m_match_truncation_disable_if_hist_match
         16,                     // m_hist_match_len_disable_full_match_find_thresh
         UINT_MAX,               // m_match_accel_max_matches_per_probe
         32,                     // m_match_accel_max_probes
         cMaxParseGraphNodes,    // m_max_parse_graph_nodes
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
         cMaxParseGraphNodes,    // m_max_parse_graph_nodes
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
      if (!m_main_table.init(true, CLZBase::cLZXNumSpecialLengths + (lzbase.m_num_lzx_slots - CLZBase::cLZXLowestUsableMatchSlot) * 8, fast_adaptive_huffman_updating, use_polar_codes)) return false;
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

      uint match_pred = lit_pred0 >> (8 - CLZBase::cNumIsMatchContextBits);
      uint match_model_index = m_cur_state + (match_pred << 4);
      float cost = m_is_match_model[match_model_index].get_cost(lzdec.is_match());

      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];

         if (m_cur_state < CLZBase::cNumLitStates)
         {
            const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);

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

            LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

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

   float lzcompressor::state::get_match_base_cost(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec, float &base_cost_res) const
   {
      LZHAM_ASSERT(!lzdec.is_lit());

      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);

      uint match_pred = lit_pred0 >> (8 - CLZBase::cNumIsMatchContextBits);
      uint match_model_index = m_cur_state + (match_pred << 4);
      float base_cost = m_is_match_model[match_model_index].get_cost(lzdec.is_match());

      // match
      const sym_data_model &rep_len_table = m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates];

      float actual_cost;

      if (lzdec.m_dist < 0)
      {
         // rep match
         base_cost += m_is_rep_model[m_cur_state].get_cost(1);

         int match_hist_index = -lzdec.m_dist - 1;

         if (!match_hist_index)
         {
            // rep0 match
            base_cost += m_is_rep0_model[m_cur_state].get_cost(1);

            if (lzdec.get_len() == 1)
            {
               // single byte rep0
               actual_cost = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(1);
            }
            else
            {
               actual_cost = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(0) + rep_len_table.get_cost(lzdec.get_len() - cMinMatchLen);
            }
         }
         else
         {
            // rep1 or rep2 match
            base_cost += m_is_rep0_model[m_cur_state].get_cost(0);

            if (match_hist_index == 1)
            {
               // rep1
               base_cost += m_is_rep1_model[m_cur_state].get_cost(1);

               actual_cost = base_cost + rep_len_table.get_cost(lzdec.m_len - cMinMatchLen);
            }
            else
            {
               // rep2
               base_cost += m_is_rep1_model[m_cur_state].get_cost(0);

               actual_cost = base_cost + rep_len_table.get_cost(lzdec.m_len - cMinMatchLen);
            }
         }
      }
      else
      {
         base_cost += m_is_rep_model[m_cur_state].get_cost(0);

         // full match
         uint match_slot, match_extra;
         lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

         uint match_high_sym = 0;

         LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
         match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

         uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];

         if (num_extra_bits < 3)
            base_cost += num_extra_bits;
         else
         {
            if (num_extra_bits > 4)
               base_cost += (num_extra_bits - 4);

            base_cost += m_dist_lsb_table.get_cost(match_extra & 15);
         }

         const sym_data_model &large_len_table = m_large_len_table[m_cur_state >= CLZBase::cNumLitStates];

         // compute actual_cost
         {
            uint match_low_sym = 0;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;
               actual_cost = base_cost + large_len_table.get_cost(lzdec.m_len - 9);
            }
            else
            {
               match_low_sym = lzdec.m_len - 2;
               actual_cost = base_cost;
            }

            uint main_sym = match_low_sym | (match_high_sym << 3);
            actual_cost += m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);
         }
      }

      base_cost_res = base_cost;

      return actual_cost;
   }

   void lzcompressor::state::get_match_costs(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec, float base_cost, float *pBitcosts, int min_len, int max_len) const
   {
      dict;
      LZHAM_ASSERT(!lzdec.is_lit());

      // match
      const sym_data_model &rep_len_table = m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates];

      if (lzdec.m_dist < 0)
      {
         // rep match
         int match_hist_index = -lzdec.m_dist - 1;

         if (!match_hist_index)
         {
            if (min_len == 1)
            {
               // single byte rep0
               pBitcosts[1] = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(1);
               min_len++;
            }

            float rep0_match_base_cost = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(0);
            for (int match_len = min_len; match_len <= max_len; match_len++)
            {
               // normal rep0
               pBitcosts[match_len] = rep0_match_base_cost + rep_len_table.get_cost(match_len - cMinMatchLen);
            }
         }
         else
         {
            // rep1 or rep2 match
            if (match_hist_index == 1)
            {
               // rep1
               for (int match_len = min_len; match_len <= max_len; match_len++)
               {
                  pBitcosts[match_len] = base_cost + rep_len_table.get_cost(match_len - cMinMatchLen);
               }
            }
            else
            {
               // rep2
               for (int match_len = min_len; match_len <= max_len; match_len++)
               {
                  pBitcosts[match_len] = base_cost + rep_len_table.get_cost(match_len - cMinMatchLen);
               }
            }
         }
      }
      else
      {
         LZHAM_ASSERT(min_len >= cMinMatchLen);

         // full match
         uint match_slot, match_extra;
         lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

         LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
         uint match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

         const sym_data_model &large_len_table = m_large_len_table[m_cur_state >= CLZBase::cNumLitStates];

         for (int match_len = min_len; match_len <= max_len; match_len++)
         {
            float len_cost = base_cost;

            uint match_low_sym = 0;
            if (match_len >= 9)
            {
               match_low_sym = 7;
               len_cost += large_len_table.get_cost(match_len - 9);
            }
            else
               match_low_sym = match_len - 2;

            uint main_sym = match_low_sym | (match_high_sym << 3);

            pBitcosts[match_len] = len_cost + m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);
         }
      }
   }

   bool lzcompressor::state::encode(symbol_codec& codec, CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec)
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);

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
            const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);

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

            LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

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
         if (!codec.encode_bits(m_match_hist[0], 29)) return false;
#endif
      }

      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
      return true;
   }

   bool lzcompressor::state::encode_eob(symbol_codec& codec, const search_accelerator& dict)
   {
#ifdef LZDEBUG
      if (!codec.encode_bits(CLZBase::cLZHAMDebugSyncMarkerValue, CLZBase::cLZHAMDebugSyncMarkerBits)) return false;
      if (!codec.encode_bits(1, 1)) return false;
      if (!codec.encode_bits(0, 9)) return false;
      if (!codec.encode_bits(m_cur_state, 4)) return false;
#endif

      uint match_pred = dict.get_cur_dict_size() ? (dict.get_char(-1) >> (8 - CLZBase::cNumIsMatchContextBits)) : 0;
      uint match_model_index = m_cur_state + (match_pred << 4);
      if (!codec.encode(1, m_is_match_model[match_model_index])) return false;

      // full match
      if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;

      return codec.encode(CLZBase::cLZXSpecialCodeEndOfBlockCode, m_main_table);
   }

   bool lzcompressor::state::encode_reset_state_partial(symbol_codec& codec, const search_accelerator& dict)
   {
#ifdef LZDEBUG
      if (!codec.encode_bits(CLZBase::cLZHAMDebugSyncMarkerValue, CLZBase::cLZHAMDebugSyncMarkerBits)) return false;
      if (!codec.encode_bits(1, 1)) return false;
      if (!codec.encode_bits(0, 9)) return false;
      if (!codec.encode_bits(m_cur_state, 4)) return false;
#endif

      uint match_pred = dict.get_cur_dict_size() ? (dict.get_char(-1) >> (8 - CLZBase::cNumIsMatchContextBits)) : 0;
      uint match_model_index = m_cur_state + (match_pred << 4);
      if (!codec.encode(1, m_is_match_model[match_model_index])) return false;

      // full match
      if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;

      if (!codec.encode(CLZBase::cLZXSpecialCodeResetStatePartial, m_main_table))
         return false;

      reset_state_partial();
      return true;
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

   void lzcompressor::state::reset_state_partial()
   {
      m_match_hist[0] = 1;
      m_match_hist[1] = 2;
      m_match_hist[2] = 3;
      m_cur_state = 0;
   }

   void lzcompressor::state::start_of_block(const search_accelerator& dict, uint cur_ofs)
   {
      dict;
      reset_state_partial();

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
      m_start_dict_ofs(0),
      m_block_index(0),
      m_finished(false),
      m_num_parse_threads(0),
      m_parse_jobs_remaining(0)
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

      m_num_parse_threads = 1;
      uint match_accel_helper_threads = 0;

      if (params.m_max_helper_threads > 0)
      {
         LZHAM_ASSUME(cMaxParseThreads == 4);

         if (m_params.m_block_size < 16384)
         {
            m_num_parse_threads = LZHAM_MIN(cMaxParseThreads, params.m_max_helper_threads + 1);
         }
         else
         {
            if (m_params.m_compression_level == cCompressionLevelFastest)
            {
               m_num_parse_threads = 1;
            }
            else
            {
               if (params.m_max_helper_threads == 1)
               {
                  m_num_parse_threads = 1;
               }
               else if (params.m_max_helper_threads <= 3)
               {
                  m_num_parse_threads = 2;
               }
               else if (params.m_max_helper_threads <= 7)
               {
                  m_num_parse_threads = 3;
               }
               else
               {
                  // 8-16
                  m_num_parse_threads = 4;
               }
            }
            int num_parse_jobs = m_num_parse_threads - 1;
            match_accel_helper_threads = LZHAM_MAX(0, (int)params.m_max_helper_threads - num_parse_jobs);
         }
      }

#if LZHAM_FORCE_SINGLE_THREADED_PARSING
      m_num_parse_threads = 1;
#endif
      LZHAM_ASSERT(m_num_parse_threads >= 1);
      LZHAM_ASSERT(m_num_parse_threads <= cMaxParseThreads);

      if (!params.m_pTask_pool)
      {
         LZHAM_ASSERT(!match_accel_helper_threads && (m_num_parse_threads == 1));
      }
      else
      {
         LZHAM_ASSERT((match_accel_helper_threads + (m_num_parse_threads - 1)) <= params.m_max_helper_threads);
      }

      if (!m_accel.init(this, params.m_pTask_pool, match_accel_helper_threads, dict_size, m_settings.m_match_accel_max_matches_per_probe, false, m_settings.m_match_accel_max_probes))
         return false;

      init_position_slots(params.m_dict_size_log2);

      if (!m_state.init(*this, m_settings.m_fast_adaptive_huffman_updating, m_settings.m_use_polar_codes))
         return false;

      if (!m_block_buf.try_reserve(m_params.m_block_size))
         return false;

      if (!m_comp_buf.try_reserve(m_params.m_block_size*2))
         return false;

      for (uint i = 0; i < m_num_parse_threads; i++)
      {
         if (!m_parse_thread_state[i].m_approx_state.init(*this, m_settings.m_fast_adaptive_huffman_updating, m_settings.m_use_polar_codes))
            return false;
      }

      return true;
   }

   void lzcompressor::clear()
   {
      m_codec.clear();
      m_src_size = 0;
      m_src_adler32 = cInitAdler32;
      m_block_buf.clear();
      m_comp_buf.clear();

      m_step = 0;
      m_finished = false;
      m_start_dict_ofs = 0;
      m_block_index = 0;
      m_state.clear();
      m_num_parse_threads = 0;
      m_parse_jobs_remaining = 0;

      for (uint i = 0; i < cMaxParseThreads; i++)
      {
         parse_thread_state &parse_state = m_parse_thread_state[i];
         parse_state.m_approx_state.clear();

         for (uint j = 0; j <= cMaxParseGraphNodes; j++)
            parse_state.m_nodes[j].clear();

         parse_state.m_start_ofs = 0;
         parse_state.m_bytes_to_match = 0;
         parse_state.m_best_decisions.clear();
         parse_state.m_issued_reset_state_partial = false;
         parse_state.m_emit_decisions_backwards = false;
         parse_state.m_failed = false;
      }
   }

   bool lzcompressor::code_decision(lzdecision lzdec, uint& cur_ofs, uint& bytes_to_match, float *pActual_cost)
   {
      if (pActual_cost)
      {
         lzdec.m_cost = m_state.get_cost(*this, m_accel, lzdec);
         *pActual_cost = lzdec.m_cost;
      }

#ifdef LZDEBUG
      if (!m_codec.encode_bits(CLZBase::cLZHAMDebugSyncMarkerValue, CLZBase::cLZHAMDebugSyncMarkerBits)) return false;
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

      if (!m_state.encode(m_codec, *this, m_accel, lzdec))
         return false;

      cur_ofs += len;
      LZHAM_ASSERT(bytes_to_match >= len);
      bytes_to_match -= len;

      m_accel.advance_bytes(len);

      m_step++;

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

#if LZHAM_UPDATE_STATS
      m_stats.print();
#endif

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

   // TODO: greedy_parse() currently sucks - it should at least try to use some form of flexible parsing.
   bool lzcompressor::greedy_parse(parse_thread_state &parse_state)
   {
      LZHAM_ASSERT(parse_state.m_bytes_to_match <= cMaxParseGraphNodes);

      parse_state.m_failed = false;
      parse_state.m_emit_decisions_backwards = false;

      parse_state.m_best_decisions.try_resize(0);

      uint cur_ofs = 0;
      while (cur_ofs < parse_state.m_bytes_to_match)
      {
         LZHAM_ASSERT((parse_state.m_start_ofs + cur_ofs) == parse_state.m_approx_state.m_cur_ofs);

         uint bytes_remaining = parse_state.m_bytes_to_match - cur_ofs;

         int largest_match_index = enumerate_lz_decisions(parse_state.m_start_ofs + cur_ofs, parse_state.m_approx_state, parse_state.m_temp_lzdecisions, 1);
         if (largest_match_index < 0)
         {
            parse_state.m_failed = true;
            return false;
         }

         lzdecision *pLZDec = &parse_state.m_temp_lzdecisions[largest_match_index];

         if (pLZDec->get_len() > bytes_remaining)
         {
            LZHAM_ASSERT(!pLZDec->is_lit());

            // Match is too long - either truncate it or switch to a literal.
            const uint min_codable_match_len = pLZDec->is_rep0() ? 1 : cMinMatchLen;
            if (min_codable_match_len > bytes_remaining)
               pLZDec = &parse_state.m_temp_lzdecisions[0];
            else
            {
               pLZDec->m_len = bytes_remaining;
               // Not updating cost here.
            }
         }

         LZHAM_ASSERT(pLZDec->get_len() <= bytes_remaining);

         if (!parse_state.m_best_decisions.try_push_back(*pLZDec))
         {
            parse_state.m_failed = true;
            return false;
         }

         parse_state.m_approx_state.partial_advance(*pLZDec);

         cur_ofs += pLZDec->get_len();
      }

      return true;
   }

   bool lzcompressor::optimal_parse(parse_thread_state &parse_state)
   {
      LZHAM_ASSERT(parse_state.m_bytes_to_match <= cMaxParseGraphNodes);

      parse_state.m_failed = false;
      parse_state.m_emit_decisions_backwards = true;

      // Dijkstra's algorithm. The graph nodes are positions in the lookahead buffer, and the edges are LZ "decisions" (and always point forward/towards the end of the file).
      // The edge weights are the LZ decision coding cost in bits. Ties are resolved by favoring LZ decisions that are simpler to code.

      parse_state.m_nodes[0].m_visited = false;
      parse_state.m_nodes[0].m_parent = -1;
      parse_state.m_nodes[0].m_total_cost = 0.0f;
      parse_state.m_nodes[0].m_total_complexity = 0;

#ifdef LZHAM_BUILD_DEBUG
      for (uint i = 1; i <= cMaxParseGraphNodes; i++)
      {
         LZHAM_ASSERT(!parse_state.m_nodes[i].m_visited);
         LZHAM_ASSERT(parse_state.m_nodes[i].m_total_cost == math::cNearlyInfinite);
         LZHAM_ASSERT(parse_state.m_nodes[i].m_total_complexity == UINT_MAX);
      }
#endif

      uint unvisted_nodes[cMaxParseGraphNodes];
      unvisted_nodes[0] = 0;
      uint num_unvisted_nodes = 1;

      int root_node_index = -1;

      uint highest_unvisted_node_index = 0;

      lzham::vector<lzdecision>* pLZDecisions = &parse_state.m_temp_lzdecisions;

      float lzdec_bitcosts[CLZBase::cMaxMatchLen + 1];

      for ( ; ; )
      {
         int cur_node_index = -1;
         float lowest_cost = math::cNearlyInfinite;
         uint unvisited_node_index = 0;

         uint lowest_complexity = UINT_MAX;

         // Brute force search to find next cheapest unvisited node. I have a version that uses a heap based priority queue, but it was a wash.
         for (uint j = 0; j < num_unvisted_nodes; j++)
         {
            const uint i = unvisted_nodes[j];

            LZHAM_ASSERT(!parse_state.m_nodes[i].m_visited);
            LZHAM_ASSERT(parse_state.m_nodes[i].m_total_cost < math::cNearlyInfinite);

            if ( (parse_state.m_nodes[i].m_total_cost < lowest_cost) ||
                ((parse_state.m_nodes[i].m_total_cost == lowest_cost) && (parse_state.m_nodes[i].m_total_complexity < lowest_complexity)) )
            {
               cur_node_index = i;
               unvisited_node_index = j;

               lowest_cost = parse_state.m_nodes[i].m_total_cost;
               lowest_complexity = parse_state.m_nodes[i].m_total_complexity;
            }
         }

         LZHAM_ASSERT((cur_node_index >= 0) && (cur_node_index <= (int)parse_state.m_bytes_to_match));

         node& cur_node = parse_state.m_nodes[cur_node_index];
         cur_node.m_visited = true;

         if (cur_node_index == (int)parse_state.m_bytes_to_match)
         {
            root_node_index = cur_node_index;
            break;
         }

         // Remove this node from unvisted_nodes list.
         utils::swap(unvisted_nodes[num_unvisted_nodes - 1], unvisted_nodes[unvisited_node_index]);
         num_unvisted_nodes--;

         if (cur_node.m_parent >= 0)
         {
            // Move to this node's state.
            parse_state.m_approx_state.restore_partial_state(cur_node.m_parent_state);
            parse_state.m_approx_state.partial_advance(cur_node.m_lzdec);
         }

         uint min_useful_match_len = 1;

         do
         {
            if (!parse_state.m_nodes[cur_node_index + min_useful_match_len].m_visited)
               break;
            min_useful_match_len++;
         } while ((min_useful_match_len < cMaxMatchLen) && ((cur_node_index + min_useful_match_len) < (int)parse_state.m_bytes_to_match));

         // Get all possible LZ decisions beginning at this node.
         int largest_match_index = enumerate_lz_decisions(parse_state.m_start_ofs + cur_node_index, parse_state.m_approx_state, *pLZDecisions, min_useful_match_len);
         if (largest_match_index < 0)
         {
            parse_state.m_failed = true;
            return false;
         }

         if (pLZDecisions->empty())
            continue;

         const uint max_len = parse_state.m_bytes_to_match - cur_node_index;

         lzdecision &largestLZDec = pLZDecisions->at(largest_match_index);

         if ((largestLZDec.get_len() >= m_settings.m_non_optimal_parse_match_len_thresh) && (max_len >= 2))
         {
            // Match is pretty long, so immediately accept it as the only edge.
            uint match_len = LZHAM_MIN(largestLZDec.get_len(), parse_state.m_bytes_to_match - cur_node_index);

            int child_node_index = cur_node_index + match_len;
            LZHAM_ASSERT((child_node_index >= 0) && (child_node_index <= (int)cMaxParseGraphNodes));
            node& child_node = parse_state.m_nodes[child_node_index];

            if (!child_node.m_visited)
            {
               if (match_len < largestLZDec.get_len())
               {
                  largestLZDec.m_len = match_len;
                  largestLZDec.m_cost = parse_state.m_approx_state.get_cost(*this, m_accel, largestLZDec);
               }

               float child_total_cost = cur_node.m_total_cost + largestLZDec.get_cost();
               uint child_total_complexity = cur_node.m_total_complexity + largestLZDec.get_complexity();

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
                  parse_state.m_approx_state.save_partial_state(child_node.m_parent_state);
                  child_node.m_lzdec = largestLZDec;
               }
            }

            continue;
         }

         uint rep_match_max_len = 0;

         const uint first_match_lzdec_index = pLZDecisions->at(0).is_lit() ? 1 : 0;
         for (uint i = first_match_lzdec_index; i < pLZDecisions->size(); i++)
         {
            const lzdecision& lzdec = (*pLZDecisions)[i];
            LZHAM_ASSERT(!lzdec.is_lit());

            const uint min_codable_match_len = lzdec.is_rep0() ? 1 : cMinMatchLen;

            if (min_codable_match_len > max_len)
            {
               // We can't use this decision - it can't be truncated to the max. possible size so we can't code it.
               continue;
            }

            const uint max_possible_match_len = math::minimum<uint>(max_len, lzdec.get_len());

            uint start_match_len = LZHAM_MAX(min_useful_match_len, min_codable_match_len);

            LZHAM_ASSERT(start_match_len <= max_possible_match_len);

            if ( (max_possible_match_len > m_settings.m_match_truncation_disable_match_len_thresh) ||
               (lzdec.get_match_dist(parse_state.m_approx_state) < m_settings.m_match_truncation_disable_match_dist_thresh) ||
               ((m_settings.m_match_truncation_disable_if_hist_match) && (lzdec.is_rep()))
               )
            {
               // Don't even try truncating the match if it's long and/or close enough.
               start_match_len = max_possible_match_len;
            }

            if (lzdec.is_rep())
            {
               // track the largest rep match
               rep_match_max_len = LZHAM_MAX(rep_match_max_len, max_possible_match_len);
            }
            else if ((max_possible_match_len > rep_match_max_len) && (start_match_len < rep_match_max_len))
            {
               // We have a full match longer than the largest rep match, so it's worth evaluating the large full match.
               // However, it's not worth evaluating the portion that overlaps with the likely cheaper rep match.
               start_match_len = LZHAM_MAX(start_match_len, rep_match_max_len + 1);
               LZHAM_ASSERT(start_match_len <= max_possible_match_len);
            }

            // See if it's worth evaluating this decision (and truncated versions of it) at all by comparing the decision's base cost against what we've encountered so far.
            float child_base_cost = cur_node.m_total_cost + lzdec.get_base_cost();
            while (start_match_len <= max_possible_match_len)
            {
               int child_node_index = cur_node_index + start_match_len;
               LZHAM_ASSERT((child_node_index >= 0) && (child_node_index <= (int)cMaxParseGraphNodes));
               node& child_node = parse_state.m_nodes[child_node_index];

               if ((child_node.m_visited) || (child_base_cost > child_node.m_total_cost))
                  start_match_len++;
               else
                  break;
            }

            if (start_match_len > max_possible_match_len)
               continue;

            if ((start_match_len == max_possible_match_len) && (max_possible_match_len == lzdec.get_len()))
            {
               // Not truncating
               uint cur_match_len = start_match_len;

               LZHAM_ASSERT((cur_match_len >= min_codable_match_len) && (cur_match_len <= max_possible_match_len));

               int child_node_index = cur_node_index + cur_match_len;
               LZHAM_ASSERT((child_node_index >= 0) && (child_node_index <= (int)cMaxParseGraphNodes));
               node& child_node = parse_state.m_nodes[child_node_index];

               if (!child_node.m_visited)
               {
                  float child_total_cost = cur_node.m_total_cost + lzdec.get_cost();
                  uint child_total_complexity = cur_node.m_total_complexity + lzdec.get_complexity();

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
                     parse_state.m_approx_state.save_partial_state(child_node.m_parent_state);
                     child_node.m_lzdec = lzdec;
                  }
               }
            }
            else
            {
               // Generate children nodes by trying this decision and all truncated versions.

               // Get bitcost of each possible truncated decision, and the original decision.
               // We've already computed the decisions base cost so this only factors in the difference due to match len truncation.
               parse_state.m_approx_state.get_match_costs(*this, m_accel, lzdec, lzdec.m_base_cost, lzdec_bitcosts, start_match_len, max_possible_match_len);

               for (uint cur_match_len = start_match_len; cur_match_len <= max_possible_match_len; cur_match_len++ )
               {
#if LZHAM_VERIFY_MATCH_COSTS
                  {
                     lzdecision actual_dec(lzdec);
                     actual_dec.m_len = cur_match_len;
                     float actual_cost = parse_state.m_approx_state.get_cost(*this, m_accel, actual_dec);
                     LZHAM_ASSERT(fabs(actual_cost - lzdec_bitcosts[cur_match_len]) < .000125f);
                  }
#endif

                  LZHAM_ASSERT((cur_match_len >= min_codable_match_len) && (cur_match_len <= max_possible_match_len));

                  int child_node_index = cur_node_index + cur_match_len;
                  LZHAM_ASSERT((child_node_index >= 0) && (child_node_index <= (int)cMaxParseGraphNodes));
                  node& child_node = parse_state.m_nodes[child_node_index];

                  if (child_node.m_visited)
                     continue;

                  float cost = lzdec_bitcosts[cur_match_len];

                  float child_total_cost = cur_node.m_total_cost + cost;
                  if (child_total_cost > child_node.m_total_cost)
                     continue;

#if LZHAM_DISCARD_SMALL_EXPANDED_MATCHES
                  const bool is_truncated = (int)cur_match_len != lzdec.get_len();
                  if ((is_truncated) && (cur_match_len <= 4) && (cost > (LZHAM_SMALL_MATCH_EXPANSION_BITS_PER_BYTE_THRESH * cur_match_len)))
                     continue;
#endif

                  uint child_total_complexity = cur_node.m_total_complexity;
                  if (lzdec.is_rep())
                     child_total_complexity += -lzdec.m_dist;  // 2, 3, 4
                  else if (cur_match_len >= 9)
                     child_total_complexity += 5;
                  else
                     child_total_complexity += 6;

                  if (child_total_cost == child_node.m_total_cost)
                  {
                     // Child has same cost as the best found so far - so choose the one with lowest overall coding complexity.
                     if (child_total_complexity >= child_node.m_total_complexity)
                        continue;
                  }

                  if (child_node.m_total_complexity == UINT_MAX)
                  {
                     highest_unvisted_node_index = math::maximum<uint>(highest_unvisted_node_index, child_node_index);

                     unvisted_nodes[num_unvisted_nodes] = child_node_index;
                     num_unvisted_nodes++;
                  }

                  child_node.m_total_cost = child_total_cost;
                  child_node.m_total_complexity = child_total_complexity;
                  child_node.m_parent = (int16)cur_node_index;
                  parse_state.m_approx_state.save_partial_state(child_node.m_parent_state);

                  child_node.m_lzdec = lzdec;
                  child_node.m_lzdec.m_len = cur_match_len;
                  child_node.m_lzdec.m_cost = cost;

               } // cur_match_len
            } // truncation check

         } // pLZDecisions iteration

         if (pLZDecisions->at(0).is_lit())
         {
            const lzdecision& lzdec = pLZDecisions->at(0);

            int child_node_index = cur_node_index + 1;
            LZHAM_ASSERT((child_node_index >= 0) && (child_node_index <= (int)cMaxParseGraphNodes));
            node& child_node = parse_state.m_nodes[child_node_index];

            if (!child_node.m_visited)
            {
               float child_total_cost = cur_node.m_total_cost + lzdec.get_cost();
               uint child_total_complexity = cur_node.m_total_complexity + lzdec.get_complexity();

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
                  parse_state.m_approx_state.save_partial_state(child_node.m_parent_state);
                  child_node.m_lzdec = lzdec;
               }
            }
         } // literal

         LZHAM_ASSERT(num_unvisted_nodes);

      } // graph search

      for (int i = 1; i <= (int)highest_unvisted_node_index; i++)
         parse_state.m_nodes[i].clear();

      // Now get the optimal decisions by starting from the goal node. m_best_decisions will be filled backwards.
      parse_state.m_best_decisions.try_resize(0);

      int cur_node_index = root_node_index;
      do
      {
         LZHAM_ASSERT((cur_node_index >= 0) && (cur_node_index <= (int)cMaxParseGraphNodes));
         node& cur_node = parse_state.m_nodes[cur_node_index];

         if (!parse_state.m_best_decisions.try_push_back(cur_node.m_lzdec))
         {
            parse_state.m_failed = true;
            return false;
         }

         cur_node_index = parse_state.m_nodes[cur_node_index].m_parent;
      } while (cur_node_index > 0);

      return true;
   }

   void lzcompressor::parse_job_callback(uint64 data, void* pData_ptr)
   {
      (void)pData_ptr;
      const uint parse_job_index = (uint)data;

      parse_thread_state &parse_state = m_parse_thread_state[parse_job_index];

      if (m_params.m_compression_level == cCompressionLevelFastest)
         greedy_parse(parse_state);
      else
         optimal_parse(parse_state);

      LZHAM_MEMORY_EXPORT_BARRIER

      if (InterlockedDecrement(&m_parse_jobs_remaining) == 0)
      {
         m_parse_jobs_complete.release();
      }
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

      if (!m_codec.start_encoding((buf_len * 9) / 8))
         return false;

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

      m_initial_state = m_state;

      coding_stats initial_stats(m_stats);

      uint initial_step = m_step;

#ifdef LZVERIFY
      for (uint i = 0; i < bytes_to_match; i++)
      {
         uint cur_ofs = m_start_dict_ofs + i;
         int largest_match_index = enumerate_lz_decisions(cur_ofs, m_state, lzdecisions0, 1);
         if (largest_match_index < 0)
            return false;

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
         uint num_parse_jobs = LZHAM_MIN(m_num_parse_threads, (bytes_to_match + cMaxParseGraphNodes - 1) / cMaxParseGraphNodes);

         uint parse_thread_start_ofs = cur_ofs;
         uint parse_thread_total_size = LZHAM_MIN(bytes_to_match, cMaxParseGraphNodes * num_parse_jobs);
         uint parse_thread_remaining = parse_thread_total_size;
         for (uint parse_thread_index = 0; parse_thread_index < num_parse_jobs; parse_thread_index++)
         {
            parse_thread_state &parse_thread = m_parse_thread_state[parse_thread_index];

            parse_thread.m_approx_state = m_state;
            parse_thread.m_approx_state.m_cur_ofs = parse_thread_start_ofs;

            if (parse_thread_index > 0)
            {
               parse_thread.m_approx_state.reset_state_partial();
               parse_thread.m_issued_reset_state_partial = true;
            }
            else
            {
               parse_thread.m_issued_reset_state_partial = false;
            }

            parse_thread.m_start_ofs = parse_thread_start_ofs;
            if (parse_thread_index == (num_parse_jobs - 1))
               parse_thread.m_bytes_to_match = parse_thread_remaining;
            else
               parse_thread.m_bytes_to_match = parse_thread_total_size / num_parse_jobs;

            parse_thread.m_bytes_to_match = LZHAM_MIN(parse_thread.m_bytes_to_match, cMaxParseGraphNodes);
            LZHAM_ASSERT(parse_thread.m_bytes_to_match > 0);

            parse_thread_start_ofs += parse_thread.m_bytes_to_match;
            parse_thread_remaining -= parse_thread.m_bytes_to_match;
         }

         if ((m_params.m_pTask_pool) && (num_parse_jobs > 1))
         {
            m_parse_jobs_remaining = num_parse_jobs;

            for (uint parse_thread_index = 1; parse_thread_index < num_parse_jobs; parse_thread_index++)
            {
               m_params.m_pTask_pool->queue_object_task(this, &lzcompressor::parse_job_callback, parse_thread_index);
            }

            parse_job_callback(0, NULL);

            m_parse_jobs_complete.wait();
         }
         else
         {
            m_parse_jobs_remaining = INT_MAX;
            for (uint parse_thread_index = 0; parse_thread_index < num_parse_jobs; parse_thread_index++)
            {
               parse_job_callback(parse_thread_index, NULL);
            }
         }

         for (uint parse_thread_index = 0; parse_thread_index < num_parse_jobs; parse_thread_index++)
         {
            parse_thread_state &parse_thread = m_parse_thread_state[parse_thread_index];
            if (parse_thread.m_failed)
               return false;

            const lzham::vector<lzdecision> &best_decisions = parse_thread.m_best_decisions;

            if (parse_thread.m_issued_reset_state_partial)
            {
               if (!m_state.encode_reset_state_partial(m_codec, m_accel))
                  return false;
               m_step++;
            }

            if (parse_thread.m_emit_decisions_backwards)
            {
               LZHAM_ASSERT(best_decisions.back().m_pos == (int)parse_thread.m_start_ofs);

               for (int i = best_decisions.size() - 1; i >= 0; --i)
               {
                  LZHAM_ASSERT(best_decisions[i].m_pos == (int)cur_ofs);

   #if LZHAM_UPDATE_STATS
                  float actual_cost;
                  if (!code_decision(best_decisions[i], cur_ofs, bytes_to_match, &actual_cost))
                     return false;

                  m_stats.update(best_decisions[i], m_state, m_accel);
   #else
                  if (!code_decision(best_decisions[i], cur_ofs, bytes_to_match, NULL))
                     return false;
   #endif
               }
            }
            else
            {
               LZHAM_ASSERT(best_decisions.front().m_pos == (int)parse_thread.m_start_ofs);

               for (uint i = 0; i < best_decisions.size(); i++)
               {
                  LZHAM_ASSERT(best_decisions[i].m_pos == (int)cur_ofs);

#if LZHAM_UPDATE_STATS
                  float actual_cost;
                  if (!code_decision(best_decisions[i], cur_ofs, bytes_to_match, &actual_cost))
                     return false;

                  m_stats.update(best_decisions[i], m_state, m_accel);
#else
                  if (!code_decision(best_decisions[i], cur_ofs, bytes_to_match, NULL))
                     return false;
#endif
               }
            }

            LZHAM_ASSERT(cur_ofs == parse_thread.m_start_ofs + parse_thread.m_bytes_to_match);

         } // parse_thread_index
      }

      m_accel.add_bytes_end();

      if (!m_state.encode_eob(m_codec, m_accel))
         return false;

#ifdef LZDEBUG
      if (!m_codec.encode_bits(366, 12)) return false;
#endif

      if (!m_codec.stop_encoding(true)) return false;

      uint compressed_size = m_codec.get_encoding_buf().size();

#if defined(LZDISABLE_RAW_BLOCKS) || defined(LZDEBUG)
      if (0)
#else
      if (compressed_size >= buf_len)
#endif
      {
         m_state = m_initial_state;
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

   int lzcompressor::enumerate_lz_decisions(uint ofs, const state& cur_state, lzham::vector<lzdecision>& decisions, uint min_match_len)
   {
      LZHAM_ASSERT(min_match_len >= 1);

      uint start_ofs = m_accel.get_lookahead_pos() & m_accel.get_max_dict_size_mask();
      LZHAM_ASSERT(ofs >= start_ofs);
      const uint lookahead_ofs = ofs - start_ofs;

      uint largest_index = 0;
      uint largest_len;
      float largest_cost;

      if (min_match_len <= 1)
      {
         if (!decisions.try_resize(1))
            return -1;

         lzdecision& lit_dec = decisions[0];
         lit_dec.init(ofs, 0, 0);
         largest_cost = cur_state.get_cost(*this, m_accel, lit_dec);
         lit_dec.m_cost = largest_cost;
         lit_dec.m_base_cost = 0.0f;

         largest_len = 1;
      }
      else
      {
         if (!decisions.try_resize(0))
            return -1;

         largest_len = 0;
         largest_cost = math::cNearlyInfinite;
      }

      uint match_hist_max_len = 0;

      // Add rep matches.
      for (uint i = 0; i < cMatchHistSize; i++)
      {
         uint hist_match_len = m_accel.match(lookahead_ofs, cur_state.m_match_hist[i]);
         if (hist_match_len < min_match_len)
            continue;

         if ( ((hist_match_len == 1) && (i == 0)) || (hist_match_len >= CLZBase::cMinMatchLen) )
         {
            match_hist_max_len = math::maximum(match_hist_max_len, hist_match_len);

            lzdecision dec(ofs, hist_match_len, -((int)i + 1));
            dec.m_cost = cur_state.get_match_base_cost(*this, m_accel, dec, dec.m_base_cost);

#if LZHAM_VERIFY_MATCH_COSTS
            {
               float actual_cost = cur_state.get_cost(*this, m_accel, dec);
               LZHAM_ASSERT(fabs(actual_cost - dec.m_cost) < .000125f);
            }
#endif
            if (!decisions.try_push_back(dec))
               return -1;

            if ( (hist_match_len > largest_len) || ((hist_match_len == largest_len) && (dec.m_cost < largest_cost)) )
            {
               largest_index = decisions.size() - 1;
               largest_len = hist_match_len;
               largest_cost = dec.m_cost;
            }
         }
      }

      // Now add full matches.
      if (match_hist_max_len < m_settings.m_hist_match_len_disable_full_match_find_thresh)
      {
         const dict_match* pMatches = m_accel.find_matches(lookahead_ofs);

         if (pMatches)
         {
            for ( ; ; )
            {
               uint match_len = pMatches->get_len();
               LZHAM_ASSERT((pMatches->get_dist() > 0) && (pMatches->get_dist() <= m_dict_size));

               // Full matches are very likely to be more expensive than rep matches of the same length, so don't bother evaluating them.
               if ((match_len >= min_match_len) && (match_len > match_hist_max_len))
               {
                  lzdecision dec(ofs, match_len, pMatches->get_dist());
                  dec.m_cost = cur_state.get_match_base_cost(*this, m_accel, dec, dec.m_base_cost);

#if LZHAM_VERIFY_MATCH_COSTS
                  {
                     float actual_cost = cur_state.get_cost(*this, m_accel, dec);
                     LZHAM_ASSERT(fabs(actual_cost - dec.m_cost) < .000125f);
                  }
#endif

#if LZHAM_DISCARD_SMALL_EXPANDED_MATCHES
                  // Immediately reject small matches that get expanded too much.
                  if ((match_len > 4) || (dec.m_cost <= match_len * LZHAM_SMALL_MATCH_EXPANSION_BITS_PER_BYTE_THRESH))
#endif
                  {
                     if (!decisions.try_push_back(dec))
                        return -1;

                     if ( (match_len > largest_len) || ((match_len == largest_len) && (dec.get_cost() < largest_cost)) )
                     {
                        largest_index = decisions.size() - 1;
                        largest_len = match_len;
                        largest_cost = dec.get_cost();
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

