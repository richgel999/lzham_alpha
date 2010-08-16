// File: lzham_lzcomp_internal.h
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
#include "lzham_match_accel.h"
#include "lzham_symbol_codec.h"
#include "lzham_task_pool.h"
#include "lzham_lzbase.h"

namespace lzham
{
   typedef lzham::vector<uint8> byte_vec;
   
   const uint cMaxParseGraphNodes = 1024;
   
   enum compression_level
   {
      cCompressionLevelFastest,
      cCompressionLevelFaster,
      cCompressionLevelDefault,
      cCompressionLevelBetter,
      cCompressionLevelUber,
      
      cCompressionLevelCount
   };
   
   struct comp_settings
   {
      bool m_fast_adaptive_huffman_updating;
      bool m_use_polar_codes;
      bool m_non_optimal_parse_if_hist_match;
      uint m_non_optimal_parse_match_len_thresh;
      uint m_match_truncation_disable_match_len_thresh;
      uint m_match_truncation_disable_match_dist_thresh;
      uint m_match_truncation_disable_if_hist_match;
      uint m_hist_match_len_disable_full_match_find_thresh;
      uint m_match_accel_max_matches_per_probe;
      uint m_match_accel_max_probes;
      uint m_max_parse_graph_nodes;
   };
   
   const float cMatchNotPresentCostPenalty = 3.0f;
   
   // 4-byte integer hash, full avalanche, no constants
   inline uint32 bitmix32(uint32 a)
   {
      a -= (a << 6);
      a ^= (a >> 17);
      a -= (a << 9);
      a ^= (a << 4);
      a -= (a << 3);
      a ^= (a << 10);
      a ^= (a >> 15);
      return a;
   }
   
   class cpucache
   {
   public:
      cpucache();
      
      bool init(uint num_cachelines, uint cachline_size);
      void clear();
      
      void touch(uint address);
      void touch(uint address, uint len);
      
      bool is_present(uint address) const; 
      bool is_present(uint address, uint len) const; 
      
      inline uint get_num_cachelines() const { return m_num_cachelines; }
      inline uint get_cacheline_size() const { return m_cacheline_size; }
      inline uint get_cacheline_size_shift() const { return m_cacheline_size_shift; }
      
      inline bool is_initialized() const { return m_num_cachelines > 0; }
               
   private:
      uint m_num_cachelines;
      uint m_cacheline_size;
      uint m_cacheline_size_shift;
      
      struct cacheline
      {
         cacheline() : m_line(-1), m_pPrev(NULL), m_pNext(NULL) { }
         
         inline uint hash() const { return bitmix32(m_line); }
                           
         int m_line;
         
         cacheline *m_pPrev;
         cacheline *m_pNext;
      };
      
      cacheline m_head;
      cacheline m_tail;
                  
      lzham::vector<cacheline> m_cacheline_hash;
      uint m_num_lines_present;
      
      void check() const;
      void erase_lru();
   };
   
   class lzcompressor : public CLZBase
   {
   public:
      lzcompressor();
      
      struct init_params
      {
         enum 
         { 
            cMinDictSizeLog2 = CLZBase::cMinDictSizeLog2,
            cMaxDictSizeLog2 = CLZBase::cMaxDictSizeLog2,
            cDefaultBlockSize = 1024U*512U
         };
         
         init_params() : 
            m_pTask_pool(NULL),
            m_max_helper_threads(0),
            m_compression_level(cCompressionLevelDefault),
            m_dict_size_log2(22),
            m_block_size(cDefaultBlockSize),
            m_num_cachelines(0),
            m_cacheline_size(0)
         {
         }
         
         task_pool* m_pTask_pool;
         uint m_max_helper_threads;
         
         compression_level m_compression_level;
         uint m_dict_size_log2;
         
         uint m_block_size;
         
         uint m_num_cachelines;
         uint m_cacheline_size;
      };
      
      bool init(const init_params& params);
      void clear();
      
      bool put_bytes(const void* pBuf, uint buf_len);
      
      const byte_vec& get_compressed_data() const   { return m_comp_buf; }
            byte_vec& get_compressed_data()         { return m_comp_buf; }

      uint32 get_src_adler32() const { return m_src_adler32; }
                  
   private:
      class state;
      
      struct lzdecision
      {
         int m_pos;  // dict position
         int m_len;  // 0 if literal, 1+ if match
         int m_dist; // <0 if match rep, else >=1 is match dist
         float m_cost;
         bool m_present_in_cpucache;
                           
         inline lzdecision() { }
         inline lzdecision(int pos, int len, int dist) : m_pos(pos), m_len(len), m_dist(dist), m_cost(0.0f), m_present_in_cpucache(true) { LZHAM_ASSERT(len <= CLZBase::cMaxMatchLen); }
         
         // does not init m_cost
         inline void init(int pos, int len, int dist) { m_pos = pos; m_len = len; m_dist = dist; LZHAM_ASSERT(len <= CLZBase::cMaxMatchLen); }
         
         inline bool is_lit() const { return !m_len; }
         inline bool is_match() const { return m_len > 0; }
         inline uint get_len() const { return math::maximum<uint>(m_len, 1); }
         inline bool is_rep() const { return m_dist < 0; }
                  
         uint get_match_dist(const state& s) const;
         
         inline uint get_complexity() const
         {
            if (is_lit())
               return 1;
            else if (is_rep())
               return 1 + -m_dist;  // 2, 3, 4
            else if (get_len() >= 9)
               return 5;
            else
               return 6;
         }
         
         inline float get_cost_plus_penalty() const 
         { 
            return m_cost + (m_present_in_cpucache ? 0.0f : cMatchNotPresentCostPenalty); 
         }
      };
                        
      class state
      {
      public:
         state();
         
         void clear();
         
         bool init(CLZBase& lzbase, bool fast_adaptive_huffman_updating, bool use_polar_codes);
         
         struct saved_state
         {
            uint m_cur_ofs;
            uint m_cur_state;
            uint m_match_hist[CLZBase::cMatchHistSize];
         };
         void save_partial_state(saved_state& dst);
         void restore_partial_state(const saved_state& src);
         
         void partial_advance(const lzdecision& lzdec);
         
         float get_cost(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec) const;
         
         float update_stats(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec);
                  
         bool encode(symbol_codec& codec, CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec);
         bool encode_eob(symbol_codec& codec, const search_accelerator& dict);

         void update_match_hist(uint match_dist);
         int find_match_dist(uint match_hist) const;
         
         void reset_match_hist();
         void start_of_block(const search_accelerator& dict, uint cur_ofs);
         
         uint get_pred_char(const search_accelerator& dict, int pos, int backward_ofs) const;
         
         inline bool will_reference_last_match(const lzdecision& lzdec) const 
         {
            return (!lzdec.is_match()) &&  (m_cur_state >= CLZBase::cNumLitStates);
         }
         
         uint m_cur_ofs;
         uint m_cur_state;
         uint m_match_hist[CLZBase::cMatchHistSize];
         uint m_block_ofs;
                  
         adaptive_bit_model m_is_match_model[(1 << CLZBase::cNumIsMatchContextBits) * 16];//CLZBase::cNumStates];
                                                               
         adaptive_bit_model m_is_rep_model[CLZBase::cNumStates];
         adaptive_bit_model m_is_rep0_model[CLZBase::cNumStates];
         adaptive_bit_model m_is_rep0_single_byte_model[CLZBase::cNumStates];      
         adaptive_bit_model m_is_rep1_model[CLZBase::cNumStates];

         typedef quasi_adaptive_huffman_data_model sym_data_model;
         sym_data_model m_lit_table[1 << CLZBase::cNumLitPredBits];
         sym_data_model m_delta_lit_table[1 << CLZBase::cNumDeltaLitPredBits];
         
         sym_data_model m_main_table;
         sym_data_model m_rep_len_table[2];
         sym_data_model m_large_len_table[2];
         sym_data_model m_dist_lsb_table;
      };   
      
      struct coding_stats
      {
         coding_stats() { clear(); }

         void clear();

         void update(const lzdecision& lzdec, const state& cur_state, const search_accelerator& dict);
         void print();

         uint m_total_bytes;
         uint m_total_contexts;
         double m_total_cost;
         
         double m_total_match_bits_cost;
         double m_worst_match_bits_cost;
         double m_total_match0_bits_cost;
         double m_total_match1_bits_cost;
         uint m_total_nonmatches;
         uint m_total_matches;
                  
         uint m_total_lits;
         double m_total_lit_cost;
         double m_worst_lit_cost;

         uint m_total_delta_lits;
         double m_total_delta_lit_cost;
         double m_worst_delta_lit_cost;

         uint m_total_reps;
         uint m_total_rep0_len1_matches;
         double m_total_rep0_len1_cost;
         double m_worst_rep0_len1_cost;

         uint m_total_rep_matches[CLZBase::cMatchHistSize];
         double m_total_rep_cost[CLZBase::cMatchHistSize];
         double m_worst_rep_cost[CLZBase::cMatchHistSize];

         uint m_total_full_matches[cMaxMatchLen + 1];
         double m_total_full_match_cost[cMaxMatchLen + 1];
         double m_worst_full_match_cost[cMaxMatchLen + 1];
      };
      
      init_params m_params;
      comp_settings m_settings;
            
      int64 m_src_size;
      uint32 m_src_adler32;
            
      search_accelerator m_accel;
                  
      symbol_codec m_codec;
            
      state m_state;
      coding_stats m_stats;
      
      byte_vec m_block_buf;
      byte_vec m_comp_buf;
            
      lzham::vector<lzdecision> m_best_decisions;
            
      struct node
      {
         bool                 m_visited;
         int16                m_parent;         // parent cell
         float                m_total_cost; 
         uint                 m_total_complexity;
         lzdecision           m_lzdec;          // the lzdecision that led from parent to this cell
         state::saved_state   m_parent_state;   // the parent's state
         
         void clear()
         {
            m_visited = false;
            m_total_cost = math::cNearlyInfinite;
            m_total_complexity = UINT_MAX;
         }
      };
      
      node m_nodes[cMaxParseGraphNodes];
                  
      lzham::vector<lzdecision> m_temp_lzdecisions;
      
      uint m_step;
      
      uint m_start_dict_ofs;
      
      cpucache m_cpucache;
      
      uint m_block_index;
      
      bool m_finished;

      bool send_final_block();
      bool send_configuration();
      bool compress_block(const void* pBuf, uint buf_len);
      bool code_decision(lzdecision lzdec, uint& cur_ofs, uint& bytes_to_match, bool recompute_cost, float *pActual_cost);
      uint enumerate_lz_decisions(uint ofs, const state& cur_state, lzham::vector<lzdecision>& decisions);
   };
      
} // namespace lzham



