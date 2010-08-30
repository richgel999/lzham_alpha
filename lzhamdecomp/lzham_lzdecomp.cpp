// File: lzham_lzdecomp.cpp
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
#include "lzham_decomp.h"
#include "lzham_symbol_codec.h"
#include "lzham_checksum.h"
#include "lzham_lzbase.h"

using namespace lzham;

namespace lzham
{
   const uint cFiberStackSize = 64 * 1024;

   struct lzham_decompress_state
   {
      symbol_codec m_codec;

      void *m_pPrimary_fiber;
      void *m_pWorker_fiber;

      uint8 *m_pRaw_decomp_buf;
      uint8 *m_pDecomp_buf;
      uint32 m_decomp_adler32;

      const uint8 *m_pIn_buf;
      size_t *m_pIn_buf_size;
      uint8 *m_pOut_buf;
      size_t *m_pOut_buf_size;
      bool m_no_more_input_bytes_flag;

      uint8 *m_pOrig_out_buf;
      size_t m_orig_out_buf_size;

      lzham_decompress_params m_params;

      lzham_decompress_status_t m_status;

      typedef quasi_adaptive_huffman_data_model sym_data_model;
      sym_data_model m_lit_table[1 << CLZBase::cNumLitPredBits];
      sym_data_model m_delta_lit_table[1 << CLZBase::cNumDeltaLitPredBits];
      sym_data_model m_main_table;
      sym_data_model m_rep_len_table[2];
      sym_data_model m_large_len_table[2];
      sym_data_model m_dist_lsb_table;
   };

   typedef void (*need_bytes_func_ptr)(uint num_bytes_consumed, void *pPrivate_data, const uint8* &pBuf, uint &buf_size, bool &eof_flag);

   static void lzham_return(lzham_decompress_state *pState, lzham_decompress_status_t status)
   {
      pState->m_status = status;

      for ( ; ; )
      {
         SwitchToFiber(pState->m_pPrimary_fiber);
         if (status < LZHAM_DECOMP_STATUS_INVALID_PARAMETER)
            break;
      }
   }

   static void lzham_decode_need_bytes_func(size_t num_bytes_consumed, void *p, const uint8* &pBuf, size_t &buf_size, bool &eof_flag)
   {
      lzham_decompress_state *pState = static_cast<lzham_decompress_state *>(p);
      for ( ; ; )
      {
         *pState->m_pIn_buf_size = num_bytes_consumed;
         *pState->m_pOut_buf_size = 0;

         lzham_return(pState, LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT);

         pBuf = pState->m_pIn_buf;
         buf_size = *pState->m_pIn_buf_size;
         eof_flag = pState->m_no_more_input_bytes_flag;

         if ((eof_flag) || (buf_size))
            break;
      }
   }

   static void lzham_flush_output_buf(lzham_decompress_state *pState, size_t total_bytes)
   {
      const uint8 *pDecomp_src = pState->m_pDecomp_buf;
      size_t num_bytes_remaining = total_bytes;
      while (num_bytes_remaining)
      {
         size_t n = LZHAM_MIN(num_bytes_remaining, *pState->m_pOut_buf_size);

         if (!pState->m_params.m_compute_adler32)
         {
            memcpy(pState->m_pOut_buf, pDecomp_src, n);
         }
         else
         {
            size_t copy_ofs = 0;
            while (copy_ofs < n)
            {
               const uint cBytesToMemCpyPerIteration = 8192U;
               size_t bytes_to_copy = LZHAM_MIN((size_t)(n - copy_ofs), cBytesToMemCpyPerIteration);
               memcpy(pState->m_pOut_buf + copy_ofs, pDecomp_src + copy_ofs, bytes_to_copy);
               pState->m_decomp_adler32 = adler32(pDecomp_src + copy_ofs, bytes_to_copy, pState->m_decomp_adler32);
               copy_ofs += bytes_to_copy;
            }
         }

         *pState->m_pIn_buf_size = static_cast<size_t>(pState->m_codec.decode_get_bytes_consumed());
         *pState->m_pOut_buf_size = n;

         lzham_return(pState, LZHAM_DECOMP_STATUS_NOT_FINISHED);

         pState->m_codec.decode_set_input_buffer(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_pIn_buf, pState->m_no_more_input_bytes_flag);

         pDecomp_src += n;
         num_bytes_remaining -= n;
      }
   }

   static void lzham_decompress_worker_fiber_buffered(void* p);
   static void lzham_decompress_worker_fiber_unbuffered(void* p);

   static const uint8 s_literal_next_state[24] =
   {
      0, 0, 0, 0,
      1, 2, 3, 4, 5, 6,
      4, 5, 7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10
   };

   //------------------------------------------------------------------------------------------------------------------
   // buffered output
   //------------------------------------------------------------------------------------------------------------------
   static void lzham_decompress_worker_fiber_buffered(void* p)
   {
      lzham_decompress_state *pState = static_cast<lzham_decompress_state *>(p);

      if (!pState->m_codec.start_decoding(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_no_more_input_bytes_flag, lzham_decode_need_bytes_func, pState))
      {
         lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED);
      }

      LZHAM_ASSUME(CLZBase::cNumStates <= 16);
      adaptive_bit_model m_is_match_model[(1 << CLZBase::cNumIsMatchContextBits) * 16];
      adaptive_bit_model m_is_rep_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep0_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep0_single_byte_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep1_model[CLZBase::cNumStates];

      CLZBase lzBase;
      lzBase.init_position_slots(pState->m_params.m_dict_size_log2);

      const uint dict_size = 1U << pState->m_params.m_dict_size_log2;
      const uint dict_size_mask = dict_size - 1;

      uint8* pDst = reinterpret_cast<uint8*>(pState->m_pDecomp_buf);
      uint8* pDst_end = pDst + dict_size;
      uint dst_ofs = 0;

      symbol_codec &codec = pState->m_codec;

      uint step = 0;

      lzham_decompress_status_t status = LZHAM_DECOMP_STATUS_NOT_FINISHED;

      bool huffman_decoders_initialized = false;

      do
      {
         codec.start_arith_decoding();

#ifdef LZDEBUG
         uint k = codec.decode_bits(12); LZHAM_VERIFY(k==166);
#endif

         if (!huffman_decoders_initialized)
         {
            huffman_decoders_initialized = true;

            bool fast_huffman_coding = (codec.decode_bits(1) != 0);
            bool use_polar_codes = (codec.decode_bits(1) != 0);

            for (uint i = 0; i < (1 << CLZBase::cNumLitPredBits); i++)
               pState->m_lit_table[i].init(false, 256, fast_huffman_coding, use_polar_codes);

            for (uint i = 0; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
               pState->m_delta_lit_table[i].init(false, 256, fast_huffman_coding, use_polar_codes);

            pState->m_main_table.init(false, CLZBase::cLZXNumSpecialLengths + (lzBase.m_num_lzx_slots - CLZBase::cLZXLowestUsableMatchSlot) * 8, fast_huffman_coding, use_polar_codes);
            for (uint i = 0; i < 2; i++)
            {
               pState->m_rep_len_table[i].init(false, CLZBase::cMaxMatchLen - CLZBase::cMinMatchLen + 1, fast_huffman_coding, use_polar_codes);
               pState->m_large_len_table[i].init(false, CLZBase::cLZXNumSecondaryLengths, fast_huffman_coding, use_polar_codes);
            }
            pState->m_dist_lsb_table.init(false, 16, fast_huffman_coding, use_polar_codes);
         }

         uint block_type = codec.decode_bits(2);
         switch (block_type)
         {
            case CLZBase::cRawBlock:
            {
               const uint raw_block_len = 1 + codec.decode_bits(24);

               codec.decode_align_to_byte();

               uint num_raw_bytes_remaining = raw_block_len;
               while (num_raw_bytes_remaining)
               {
                  int b = codec.decode_remove_byte_from_bit_buf();
                  if (b < 0)
                     break;

                  pDst[dst_ofs++] = static_cast<uint8>(b);
                  if (dst_ofs > dict_size_mask)
                  {
                     lzham_flush_output_buf(pState, dict_size);
                     dst_ofs = 0;
                  }

                  num_raw_bytes_remaining--;
               }

               while (num_raw_bytes_remaining)
               {
                  uint64 src_ofs = codec.decode_get_bytes_consumed();
                  uint64 src_bytes_remaining = *pState->m_pIn_buf_size - src_ofs;

                  while (!src_bytes_remaining)
                  {
                     if (pState->m_no_more_input_bytes_flag)
                     {
                        lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED);
                     }

                     *pState->m_pIn_buf_size = static_cast<size_t>(src_ofs);
                     *pState->m_pOut_buf_size = 0;

                     lzham_return(pState, LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT);
                     pState->m_codec.decode_set_input_buffer(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_pIn_buf, pState->m_no_more_input_bytes_flag);

                     src_ofs = 0;
                     src_bytes_remaining = *pState->m_pIn_buf_size;
                  }

                  uint num_bytes_to_copy = static_cast<uint>(LZHAM_MIN(num_raw_bytes_remaining, src_bytes_remaining));
                  num_bytes_to_copy = LZHAM_MIN(num_bytes_to_copy, dict_size - dst_ofs);

                  memcpy(pDst + dst_ofs, pState->m_pIn_buf + src_ofs, num_bytes_to_copy);

                  src_ofs += num_bytes_to_copy;
                  num_raw_bytes_remaining -= num_bytes_to_copy;

                  codec.decode_set_input_buffer(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_pIn_buf + src_ofs, pState->m_no_more_input_bytes_flag);

                  dst_ofs += num_bytes_to_copy;
                  if (dst_ofs > dict_size_mask)
                  {
                     LZHAM_ASSERT(dst_ofs == dict_size);
                     lzham_flush_output_buf(pState, dict_size);
                     dst_ofs = 0;
                  }
               }

               break;
            }
            case CLZBase::cCompBlock:
            {
               const uint initial_step = step;
               initial_step;

               int match_hist0 = 1;
               int match_hist1 = 2;
               int match_hist2 = 3;
               uint cur_state = 0;

               uint start_block_dst_ofs = dst_ofs;

               symbol_codec &codec = pState->m_codec;
               LZHAM_SYMBOL_CODEC_DECODE_DECLARE(codec);
               LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec);

               uint prev_char = 0;
               uint prev_prev_char = 0;

#ifdef LZDEBUG
               uint block_step = 0;
               for ( ; ; step++, block_step++)
#else
               for ( ; ; )
#endif
               {
#ifdef LZDEBUG
                  uint x; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, x, CLZBase::cLZHAMDebugSyncMarkerBits);
                  LZHAM_VERIFY(x == CLZBase::cLZHAMDebugSyncMarkerValue);
                  uint m; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, m, 1);
                  uint mlen; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, mlen, 9);
                  uint s; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, s, 4);
                  LZHAM_VERIFY(cur_state == s);
#endif
#ifdef _DEBUG
{
                  uint total_block_bytes = ((dst_ofs - start_block_dst_ofs) & dict_size_mask);
                  if (total_block_bytes > 0)
                  {
                     LZHAM_ASSERT(prev_char==pDst[(dst_ofs - 1) & dict_size_mask]);
                  }
                  else
                  {
                     LZHAM_ASSERT(prev_char==0);
                  }

                  if (total_block_bytes > 1)
                  {
                     LZHAM_ASSERT(prev_prev_char==pDst[(dst_ofs - 2) & dict_size_mask]);
                  }
                  else
                  {
                     LZHAM_ASSERT(prev_prev_char==0);
                  }
}
#endif
                  uint match_pred = prev_char >> (8 - CLZBase::cNumIsMatchContextBits);
                  uint match_model_index = cur_state + (match_pred << 4);
                  LZHAM_ASSERT(match_model_index < LZHAM_ARRAY_SIZE(m_is_match_model));
                  uint bit;
                  LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, bit, m_is_match_model[match_model_index]);

#ifdef LZDEBUG
                  LZHAM_VERIFY(bit == m);
#endif

                  if (!bit)
                  {
#ifdef LZDEBUG
                     LZHAM_VERIFY(mlen == 1);
#endif

#ifdef LZDEBUG
                     uint l; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, l, 8);
#endif

                     if (cur_state < CLZBase::cNumLitStates)
                     {
                        uint lit_pred = (prev_char >> (8 - CLZBase::cNumLitPredBits / 2)) |
                                        (prev_prev_char >> (8 - CLZBase::cNumLitPredBits / 2)) << (CLZBase::cNumLitPredBits / 2);

                        // literal
                        uint r;
                        LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, r, pState->m_lit_table[lit_pred]);
                        pDst[dst_ofs] = static_cast<uint8>(r);
                        prev_prev_char = prev_char;
                        prev_char = r;

#ifdef LZDEBUG
                        LZHAM_VERIFY(pDst[dst_ofs] == l);
#endif
                     }
                     else
                     {
                        // delta literal
                        uint rep_lit0 = 0;
                        uint rep_lit1 = 0;

                        int total_block_bytes = (dst_ofs - start_block_dst_ofs) & dict_size_mask;
                        if (total_block_bytes >= match_hist0)
                        {
                           rep_lit0 = pDst[(dst_ofs - match_hist0) & dict_size_mask];

                           if (total_block_bytes > match_hist0)
                           {
                              rep_lit1 = pDst[(dst_ofs - match_hist0 - 1) & dict_size_mask];
                           }
                        }

                        uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits / 2)) |
                           ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits / 2)) << CLZBase::cNumDeltaLitPredBits / 2);

#ifdef LZDEBUG
                        uint q; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, q, 8);
                        LZHAM_VERIFY(q == rep_lit0);
#endif

                        uint r; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, r, pState->m_delta_lit_table[lit_pred]);
                        r ^= rep_lit0;
                        pDst[dst_ofs] = static_cast<uint8>(r);
                        prev_prev_char = prev_char;
                        prev_char = r;

#ifdef LZDEBUG
                        LZHAM_VERIFY(pDst[dst_ofs] == l);
#endif
                     }

                     dst_ofs++;
                     if (dst_ofs > dict_size_mask)
                     {
                        LZHAM_SYMBOL_CODEC_DECODE_END(codec);
                        lzham_flush_output_buf(pState, dict_size);
                        LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec);
                        dst_ofs = 0;
                     }

                     cur_state = s_literal_next_state[cur_state];
                  }
                  else
                  {
                     uint match_len;

                     uint is_rep; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep, m_is_rep_model[cur_state]);
                     if (is_rep)
                     {
                        uint is_rep0; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep0, m_is_rep0_model[cur_state]);
                        if (is_rep0)
                        {
                           uint is_rep0_len1; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep0_len1, m_is_rep0_single_byte_model[cur_state]);
                           if (is_rep0_len1)
                           {
                              match_len = 1;

                              cur_state = (cur_state < CLZBase::cNumLitStates) ? 9 : 11;
                           }
                           else
                           {
                              LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, match_len, pState->m_rep_len_table[cur_state >= CLZBase::cNumLitStates]);
                              match_len += CLZBase::cMinMatchLen;

                              cur_state = (cur_state < CLZBase::cNumLitStates) ? 8 : 11;
                           }
                        }
                        else
                        {
                           uint is_rep1; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep1, m_is_rep1_model[cur_state]);

                           LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, match_len, pState->m_rep_len_table[cur_state >= CLZBase::cNumLitStates]);
                           match_len += CLZBase::cMinMatchLen;

                           if (is_rep1)
                           {
                              std::swap(match_hist0, match_hist1);
                           }
                           else
                           {
                              // rep2
                              uint temp = match_hist2;
                              match_hist2 = match_hist1;
                              match_hist1 = match_hist0;
                              match_hist0 = temp;
                           }

                           cur_state = (cur_state < CLZBase::cNumLitStates) ? 8 : 11;
                        }
                     }
                     else
                     {
                        uint sym; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, sym, pState->m_main_table);
                        sym -= CLZBase::cLZXNumSpecialLengths;
                        if (static_cast<int>(sym) < 0)
                        {
                           if (static_cast<int>(sym) == (CLZBase::cLZXSpecialCodeEndOfBlockCode - CLZBase::cLZXNumSpecialLengths))
                              break;
                           else
                           {
                              // reset state partial
                              match_hist0 = 1;
                              match_hist1 = 2;
                              match_hist2 = 3;
                              cur_state = 0;
                              continue;
                           }
                        }

                        match_len = (sym & 7) + 2;
                        uint match_slot = (sym >> 3) + CLZBase::cLZXLowestUsableMatchSlot;

                        if (match_len == 9)
                        {
                           uint e; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, e, pState->m_large_len_table[cur_state >= CLZBase::cNumLitStates]);
                           match_len += e;
                        }

                        const uint num_extra_bits = lzBase.m_lzx_position_extra_bits[match_slot];

                        uint extra_bits;
                        if (num_extra_bits < 3)
                        {
                           LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, extra_bits, num_extra_bits);
                        }
                        else
                        {
                           extra_bits = 0;
                           if (num_extra_bits > 4)
                           {
                              LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, extra_bits, num_extra_bits - 4);
                              extra_bits <<= 4;
                           }

                           uint j; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, j, pState->m_dist_lsb_table);
                           extra_bits += j;
                        }

                        match_hist2 = match_hist1;
                        match_hist1 = match_hist0;
                        match_hist0 = lzBase.m_lzx_position_base[match_slot] + extra_bits;

                        cur_state = (cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
                     }

#ifdef LZDEBUG
                     LZHAM_VERIFY(match_len == mlen);
                     uint d; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, d, 29);
                     LZHAM_VERIFY(match_hist0 == d);
#endif
                     uint src_ofs = (dst_ofs - match_hist0) & dict_size_mask;
                     const uint8* pCopy_src = pDst + src_ofs;

                     if ((LZHAM_MAX(src_ofs, dst_ofs) + match_len) > dict_size_mask)
                     {
                        for (int i = match_len; i > 0; i--)
                        {
                           uint8 c = *pCopy_src++;
                           prev_prev_char = prev_char;
                           prev_char = c;
                           pDst[dst_ofs++] = c;

                           if (pCopy_src == pDst_end)
                           {
                              pCopy_src = pDst;
                           }

                           if (dst_ofs > dict_size_mask)
                           {
                              LZHAM_SYMBOL_CODEC_DECODE_END(codec);
                              lzham_flush_output_buf(pState, dict_size);
                              LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec);
                              dst_ofs = 0;
                           }
                        }
                     }
                     else
                     {
                        uint8* pCopy_dst = pDst + dst_ofs;
                        if (match_hist0 == 1)
                        {
                           uint8 c = *pCopy_src;
                           if (match_len < 8)
                           {
                              for (int i = match_len; i > 0; i--)
                                 *pCopy_dst++ = c;
                              if (match_len == 1)
                                 prev_prev_char = prev_char;
                              else
                                 prev_prev_char = c;
                           }
                           else
                           {
                              memset(pCopy_dst, c, match_len);
                              prev_prev_char = c;
                           }
                           prev_char = c;
                        }
                        else if (match_len == 1)
                        {
                           prev_prev_char = prev_char;
                           prev_char = *pCopy_src;
                           *pCopy_dst = static_cast<uint8>(prev_char);
                        }
                        else
                        {
                           uint bytes_to_copy = match_len - 2;
                           if ((bytes_to_copy < 8) || ((int)bytes_to_copy > match_hist0))
                           {
                              for (int i = bytes_to_copy; i > 0; i--)
                                 *pCopy_dst++ = *pCopy_src++;
                           }
                           else
                           {
                              memcpy(pCopy_dst, pCopy_src, bytes_to_copy);
                              pCopy_dst += bytes_to_copy;
                              pCopy_src += bytes_to_copy;
                           }
                           prev_prev_char = *pCopy_src++;
                           *pCopy_dst++ = static_cast<uint8>(prev_prev_char);

                           prev_char = *pCopy_src++;
                           *pCopy_dst++ = static_cast<uint8>(prev_char);
                        }
                        dst_ofs += match_len;
                     }
                  }
               }

               LZHAM_SYMBOL_CODEC_DECODE_END(codec);

#ifdef LZDEBUG
               uint k = codec.decode_bits(12); LZHAM_VERIFY(k == 366);
#endif
               codec.decode_align_to_byte();

               break;
            }
            case CLZBase::cEOFBlock:
            {
               status = LZHAM_DECOMP_STATUS_SUCCESS;
               break;
            }
            default:
            {
               status = LZHAM_DECOMP_STATUS_FAILED_BAD_CODE;
               break;
            }
         }
      } while (status == LZHAM_DECOMP_STATUS_NOT_FINISHED);

      if (dst_ofs)
      {
         lzham_flush_output_buf(pState, dst_ofs);
      }

      if (status == LZHAM_DECOMP_STATUS_SUCCESS)
      {
         codec.decode_align_to_byte();

         uint src_file_adler32 = codec.decode_bits(32);

         if (pState->m_params.m_compute_adler32)
         {
            if (src_file_adler32 != pState->m_decomp_adler32)
            {
               status = LZHAM_DECOMP_STATUS_FAILED_ADLER32;
            }
         }
         else
         {
            pState->m_decomp_adler32 = src_file_adler32;
         }
      }

      *pState->m_pIn_buf_size = static_cast<size_t>(codec.stop_decoding());
      *pState->m_pOut_buf_size = 0;

      lzham_return(pState, status);
   }

   //------------------------------------------------------------------------------------------------------------------
   // unbuffered output
   //------------------------------------------------------------------------------------------------------------------
   static void lzham_decompress_worker_fiber_unbuffered(void* p)
   {
      lzham_decompress_state *pState = static_cast<lzham_decompress_state *>(p);

      if (!pState->m_codec.start_decoding(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_no_more_input_bytes_flag, lzham_decode_need_bytes_func, pState))
      {
         lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED);
      }

      LZHAM_ASSUME(CLZBase::cNumStates <= 16);
      adaptive_bit_model m_is_match_model[(1 << CLZBase::cNumIsMatchContextBits) * 16];
      adaptive_bit_model m_is_rep_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep0_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep0_single_byte_model[CLZBase::cNumStates];
      adaptive_bit_model m_is_rep1_model[CLZBase::cNumStates];

      CLZBase lzBase;
      lzBase.init_position_slots(pState->m_params.m_dict_size_log2);

      const size_t max_dst_ofs = *pState->m_pOut_buf_size - 1;

      uint8* pDst = reinterpret_cast<uint8*>(pState->m_pOut_buf);
      size_t dst_ofs = 0;

      symbol_codec &codec = pState->m_codec;

      uint step = 0;

      lzham_decompress_status_t status = LZHAM_DECOMP_STATUS_NOT_FINISHED;

      bool huffman_decoders_initialized = false;

      do
      {
         codec.start_arith_decoding();

         if (!huffman_decoders_initialized)
         {
            huffman_decoders_initialized = true;

            bool fast_huffman_coding = (codec.decode_bits(1) != 0);
            bool use_polar_codes = (codec.decode_bits(1) != 0);

            for (uint i = 0; i < (1 << CLZBase::cNumLitPredBits); i++)
               pState->m_lit_table[i].init(false, 256, fast_huffman_coding, use_polar_codes);

            for (uint i = 0; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
               pState->m_delta_lit_table[i].init(false, 256, fast_huffman_coding, use_polar_codes);

            pState->m_main_table.init(false, CLZBase::cLZXNumSpecialLengths + (lzBase.m_num_lzx_slots - CLZBase::cLZXLowestUsableMatchSlot) * 8, fast_huffman_coding, use_polar_codes);
            for (uint i = 0; i < 2; i++)
            {
               pState->m_rep_len_table[i].init(false, CLZBase::cMaxMatchLen - CLZBase::cMinMatchLen + 1, fast_huffman_coding, use_polar_codes);
               pState->m_large_len_table[i].init(false, CLZBase::cLZXNumSecondaryLengths, fast_huffman_coding, use_polar_codes);
            }
            pState->m_dist_lsb_table.init(false, 16, fast_huffman_coding, use_polar_codes);
         }

         uint block_type = codec.decode_bits(2);
         switch (block_type)
         {
            case CLZBase::cRawBlock:
            {
               const uint raw_block_len = 1 + codec.decode_bits(24);

               codec.decode_align_to_byte();

               size_t num_raw_bytes_remaining = raw_block_len;
               while (num_raw_bytes_remaining)
               {
                  int b = codec.decode_remove_byte_from_bit_buf();
                  if (b < 0)
                     break;

                  if (dst_ofs > max_dst_ofs)
                  {
                     lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL);
                  }

                  pDst[dst_ofs++] = static_cast<uint8>(b);

                  num_raw_bytes_remaining--;
               }

               while (num_raw_bytes_remaining)
               {
                  uint64 src_ofs = codec.decode_get_bytes_consumed();
                  uint64 src_bytes_remaining = *pState->m_pIn_buf_size - src_ofs;

                  while (!src_bytes_remaining)
                  {
                     if (pState->m_no_more_input_bytes_flag)
                     {
                        lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED);
                     }

                     *pState->m_pIn_buf_size = static_cast<size_t>(src_ofs);
                     *pState->m_pOut_buf_size = 0;

                     lzham_return(pState, LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT);
                     pState->m_codec.decode_set_input_buffer(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_pIn_buf, pState->m_no_more_input_bytes_flag);

                     src_ofs = 0;
                     src_bytes_remaining = *pState->m_pIn_buf_size;
                  }

                  if (dst_ofs > max_dst_ofs)
                  {
                     lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL);
                  }

                  size_t num_bytes_to_copy = static_cast<uint>(LZHAM_MIN(num_raw_bytes_remaining, src_bytes_remaining));
                  num_bytes_to_copy = LZHAM_MIN(num_bytes_to_copy, max_dst_ofs - dst_ofs + 1);
                  LZHAM_ASSERT(num_bytes_to_copy);

                  memcpy(pDst + dst_ofs, pState->m_pIn_buf + src_ofs, num_bytes_to_copy);

                  src_ofs += num_bytes_to_copy;
                  num_raw_bytes_remaining -= num_bytes_to_copy;

                  codec.decode_set_input_buffer(pState->m_pIn_buf, *pState->m_pIn_buf_size, pState->m_pIn_buf + src_ofs, pState->m_no_more_input_bytes_flag);

                  dst_ofs += num_bytes_to_copy;
               }

               break;
            }
            case CLZBase::cCompBlock:
            {
               const uint initial_step = step;
               initial_step;

               int match_hist0 = 1;
               int match_hist1 = 2;
               int match_hist2 = 3;
               uint cur_state = 0;

               size_t start_block_dst_ofs = dst_ofs;

               symbol_codec &codec = pState->m_codec;
               LZHAM_SYMBOL_CODEC_DECODE_DECLARE(codec);
               LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec);

               uint prev_char = 0;
               uint prev_prev_char = 0;

#ifdef LZDEBUG
               uint block_step = 0;
               for ( ; ; step++, block_step++)
#else
               for ( ; ; )
#endif
               {
                  uint match_pred = prev_char >> (8 - CLZBase::cNumIsMatchContextBits);
                  uint match_model_index = cur_state + (match_pred << 4);
                  LZHAM_ASSERT(match_model_index < LZHAM_ARRAY_SIZE(m_is_match_model));
                  uint bit;
                  LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, bit, m_is_match_model[match_model_index]);

                  if (!bit)
                  {
                     uint r;
                     if (cur_state < CLZBase::cNumLitStates)
                     {
                        uint lit_pred = (prev_char >> (8 - CLZBase::cNumLitPredBits / 2)) |
                                        (prev_prev_char >> (8 - CLZBase::cNumLitPredBits / 2)) << (CLZBase::cNumLitPredBits / 2);

                        // literal
                        LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, r, pState->m_lit_table[lit_pred]);
                     }
                     else
                     {
                        // delta literal
                        uint rep_lit0 = 0;
                        uint rep_lit1 = 0;

                        ptrdiff_t total_block_bytes = dst_ofs - start_block_dst_ofs;
                        if (total_block_bytes >= match_hist0)
                        {
                           rep_lit0 = pDst[dst_ofs - match_hist0];

                           if (total_block_bytes > match_hist0)
                           {
                              rep_lit1 = pDst[dst_ofs - match_hist0 - 1];
                           }
                        }

                        uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits / 2)) |
                           ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits / 2)) << CLZBase::cNumDeltaLitPredBits / 2);

                        LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, r, pState->m_delta_lit_table[lit_pred]);
                        r ^= rep_lit0;
                     }

                     if (dst_ofs > max_dst_ofs)
                     {
                        lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL);
                     }

                     pDst[dst_ofs++] = static_cast<uint8>(r);
                     prev_prev_char = prev_char;
                     prev_char = r;

                     cur_state = s_literal_next_state[cur_state];
                  }
                  else
                  {
                     uint match_len;

                     uint is_rep; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep, m_is_rep_model[cur_state]);
                     if (is_rep)
                     {
                        uint is_rep0; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep0, m_is_rep0_model[cur_state]);
                        if (is_rep0)
                        {
                           uint is_rep0_len1; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep0_len1, m_is_rep0_single_byte_model[cur_state]);
                           if (is_rep0_len1)
                           {
                              match_len = 1;

                              cur_state = (cur_state < CLZBase::cNumLitStates) ? 9 : 11;
                           }
                           else
                           {
                              LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, match_len, pState->m_rep_len_table[cur_state >= CLZBase::cNumLitStates]);
                              match_len += CLZBase::cMinMatchLen;

                              cur_state = (cur_state < CLZBase::cNumLitStates) ? 8 : 11;
                           }
                        }
                        else
                        {
                           uint is_rep1; LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, is_rep1, m_is_rep1_model[cur_state]);

                           LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, match_len, pState->m_rep_len_table[cur_state >= CLZBase::cNumLitStates]);
                           match_len += CLZBase::cMinMatchLen;

                           if (is_rep1)
                           {
                              std::swap(match_hist0, match_hist1);
                           }
                           else
                           {
                              // rep2
                              uint temp = match_hist2;
                              match_hist2 = match_hist1;
                              match_hist1 = match_hist0;
                              match_hist0 = temp;
                           }

                           cur_state = (cur_state < CLZBase::cNumLitStates) ? 8 : 11;
                        }
                     }
                     else
                     {
                        uint sym; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, sym, pState->m_main_table);
                        sym -= CLZBase::cLZXNumSpecialLengths;
                        if (static_cast<int>(sym) < 0)
                        {
                           if (static_cast<int>(sym) == (CLZBase::cLZXSpecialCodeEndOfBlockCode - CLZBase::cLZXNumSpecialLengths))
                              break;
                           else
                           {
                              // reset state partial
                              match_hist0 = 1;
                              match_hist1 = 2;
                              match_hist2 = 3;
                              cur_state = 0;
                              continue;
                           }
                        }

                        match_len = (sym & 7) + 2;
                        uint match_slot = (sym >> 3) + CLZBase::cLZXLowestUsableMatchSlot;

                        if (match_len == 9)
                        {
                           uint e; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, e, pState->m_large_len_table[cur_state >= CLZBase::cNumLitStates]);
                           match_len += e;
                        }

                        const uint num_extra_bits = lzBase.m_lzx_position_extra_bits[match_slot];

                        uint extra_bits;
                        if (num_extra_bits < 3)
                        {
                           LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, extra_bits, num_extra_bits);
                        }
                        else
                        {
                           extra_bits = 0;
                           if (num_extra_bits > 4)
                           {
                              LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, extra_bits, num_extra_bits - 4);
                              extra_bits <<= 4;
                           }

                           uint j; LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, j, pState->m_dist_lsb_table);
                           extra_bits += j;
                        }

                        match_hist2 = match_hist1;
                        match_hist1 = match_hist0;
                        match_hist0 = lzBase.m_lzx_position_base[match_slot] + extra_bits;

                        cur_state = (cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
                     }

                     if ( ((size_t)match_hist0 > dst_ofs) || ((dst_ofs + match_len - 1) > max_dst_ofs) )
                     {
                        lzham_return(pState, LZHAM_DECOMP_STATUS_FAILED_BAD_CODE);
                     }

                     ptrdiff_t src_ofs = dst_ofs - match_hist0;
                     const uint8* pCopy_src = pDst + src_ofs;

                     uint8* pCopy_dst = pDst + dst_ofs;
                     if (match_hist0 == 1)
                     {
                        uint8 c = *pCopy_src;
                        if (match_len < 8)
                        {
                           for (int i = match_len; i > 0; i--)
                              *pCopy_dst++ = c;
                           if (match_len == 1)
                              prev_prev_char = prev_char;
                           else
                              prev_prev_char = c;
                        }
                        else
                        {
                           memset(pCopy_dst, c, match_len);
                           prev_prev_char = c;
                        }
                        prev_char = c;
                     }
                     else if (match_len == 1)
                     {
                        prev_prev_char = prev_char;
                        prev_char = *pCopy_src;
                        *pCopy_dst = static_cast<uint8>(prev_char);
                     }
                     else
                     {
                        uint bytes_to_copy = match_len - 2;
                        if ((bytes_to_copy < 8) || ((int)bytes_to_copy > match_hist0))
                        {
                           for (int i = bytes_to_copy; i > 0; i--)
                              *pCopy_dst++ = *pCopy_src++;
                        }
                        else
                        {
                           memcpy(pCopy_dst, pCopy_src, bytes_to_copy);
                           pCopy_dst += bytes_to_copy;
                           pCopy_src += bytes_to_copy;
                        }
                        prev_prev_char = *pCopy_src++;
                        *pCopy_dst++ = static_cast<uint8>(prev_prev_char);

                        prev_char = *pCopy_src++;
                        *pCopy_dst++ = static_cast<uint8>(prev_char);
                     }
                     dst_ofs += match_len;
                  }
               }

               LZHAM_SYMBOL_CODEC_DECODE_END(codec);

               codec.decode_align_to_byte();

               break;
            }
            case CLZBase::cEOFBlock:
            {
               status = LZHAM_DECOMP_STATUS_SUCCESS;
               break;
            }
            default:
            {
               status = LZHAM_DECOMP_STATUS_FAILED_BAD_CODE;
               break;
            }
         }
      } while (status == LZHAM_DECOMP_STATUS_NOT_FINISHED);

      if (status == LZHAM_DECOMP_STATUS_SUCCESS)
      {
         codec.decode_align_to_byte();

         uint src_file_adler32 = codec.decode_bits(32);

         if (pState->m_params.m_compute_adler32)
         {
            pState->m_decomp_adler32 = adler32(pDst, dst_ofs, cInitAdler32);

            if (src_file_adler32 != pState->m_decomp_adler32)
            {
               status = LZHAM_DECOMP_STATUS_FAILED_ADLER32;
            }
         }
         else
         {
            pState->m_decomp_adler32 = src_file_adler32;
         }
      }

      *pState->m_pIn_buf_size = static_cast<size_t>(codec.stop_decoding());
      *pState->m_pOut_buf_size = dst_ofs;

      lzham_return(pState, status);
   }

   lzham_decompress_state_ptr lzham_lib_decompress_init(const lzham_decompress_params *pParams)
   {
      LZHAM_ASSUME(CLZBase::cMinDictSizeLog2 == LZHAM_MIN_DICT_SIZE_LOG2);
      LZHAM_ASSUME(CLZBase::cMaxDictSizeLog2 == LZHAM_MAX_DICT_SIZE_LOG2_X64);

      if ((!pParams) || (pParams->m_struct_size != sizeof(lzham_decompress_params)))
         return NULL;

      if ((pParams->m_dict_size_log2 < CLZBase::cMinDictSizeLog2) || (pParams->m_dict_size_log2 > CLZBase::cMaxDictSizeLog2))
         return NULL;

      // See http://www.crystalclearsoftware.com/soc/coroutine/coroutine/fibers.html
      // Try to detect if the caller has already converted the current thread to a fiber.
      LPVOID pPrimary_fiber = GetCurrentFiber();
      if ((!pPrimary_fiber) || (pPrimary_fiber == (LPVOID)0x1E00))
      {
         pPrimary_fiber = ConvertThreadToFiber(NULL);
      }
      if (!pPrimary_fiber)
         return NULL;

      lzham_decompress_state *pState = lzham_new<lzham_decompress_state>();
      if (!pState)
         return NULL;

      pState->m_params = *pParams;
      pState->m_pPrimary_fiber = pPrimary_fiber;

      if (pState->m_params.m_output_unbuffered)
      {
         pState->m_pRaw_decomp_buf = NULL;
         pState->m_pDecomp_buf = NULL;
      }
      else
      {
         pState->m_pRaw_decomp_buf = lzham_new_array<uint8>(static_cast<uint32>(1U << pState->m_params.m_dict_size_log2) + 15);
         if (!pState->m_pRaw_decomp_buf)
         {
            lzham_delete(pState);
            return NULL;
         }
         pState->m_pDecomp_buf = math::align_up_pointer(pState->m_pRaw_decomp_buf, 16);
      }

#ifdef LZHAM_PLATFORM_X360
      pState->m_pWorker_fiber = CreateFiber(cFiberStackSize,
         pState->m_params.m_output_unbuffered ? (LPFIBER_START_ROUTINE)lzham_decompress_worker_fiber_unbuffered : (LPFIBER_START_ROUTINE)lzham_decompress_worker_fiber_buffered,
         pState);
#else
      pState->m_pWorker_fiber = CreateFiberEx(cFiberStackSize, cFiberStackSize, 0,
         pState->m_params.m_output_unbuffered ? (LPFIBER_START_ROUTINE)lzham_decompress_worker_fiber_unbuffered : (LPFIBER_START_ROUTINE)lzham_decompress_worker_fiber_buffered,
         pState);
#endif

      if (!pState->m_pWorker_fiber)
      {
         lzham_delete_array(pState->m_pRaw_decomp_buf);
         lzham_delete(pState);
         return NULL;
      }

      pState->m_decomp_adler32 = cInitAdler32;

      pState->m_pIn_buf = NULL;
      pState->m_pIn_buf_size = NULL;
      pState->m_pOut_buf = NULL;
      pState->m_pOut_buf_size = NULL;
      pState->m_no_more_input_bytes_flag = false;
      pState->m_status = LZHAM_DECOMP_STATUS_NOT_FINISHED;
      pState->m_pOrig_out_buf = NULL;
      pState->m_orig_out_buf_size = 0;

      return pState;
   }

   uint32 lzham_lib_decompress_deinit(lzham_decompress_state_ptr p)
   {
      lzham_decompress_state *pState = static_cast<lzham_decompress_state *>(p);
      if (!pState)
         return 0;

      uint32 adler32 = pState->m_decomp_adler32;

      if (pState->m_pWorker_fiber)
      {
         DeleteFiber(pState->m_pWorker_fiber);
      }

      lzham_delete_array(pState->m_pRaw_decomp_buf);
      lzham_delete(pState);

      return adler32;
   }

   lzham_decompress_status_t lzham_lib_decompress(
      lzham_decompress_state_ptr p,
      const lzham_uint8 *pIn_buf, size_t *pIn_buf_size,
      lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
      lzham_bool no_more_input_bytes_flag)
   {
      lzham_decompress_state *pState = static_cast<lzham_decompress_state *>(p);

      if ((!pState) || (!pState->m_pPrimary_fiber) || (!pState->m_pWorker_fiber) || (!pState->m_params.m_dict_size_log2) ||
         (pState->m_status >= LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE) || (!pIn_buf_size) || (!pOut_buf_size))
      {
         return LZHAM_DECOMP_STATUS_INVALID_PARAMETER;
      }

      if ((*pIn_buf_size) && (!pIn_buf))
      {
         return LZHAM_DECOMP_STATUS_INVALID_PARAMETER;
      }

      if ((*pOut_buf_size) && (!pOut_buf))
      {
         return LZHAM_DECOMP_STATUS_INVALID_PARAMETER;
      }

      pState->m_pIn_buf = pIn_buf;
      pState->m_pIn_buf_size = pIn_buf_size;
      pState->m_pOut_buf = pOut_buf;
      pState->m_pOut_buf_size = pOut_buf_size;
      pState->m_no_more_input_bytes_flag = (no_more_input_bytes_flag != 0);

      if (pState->m_params.m_output_unbuffered)
      {
         if (!pState->m_pOrig_out_buf)
         {
            pState->m_pOrig_out_buf = pOut_buf;
            pState->m_orig_out_buf_size = *pOut_buf_size;
         }
         else
         {
            if ((pState->m_pOrig_out_buf != pOut_buf) || (pState->m_orig_out_buf_size != *pOut_buf_size))
            {
               return LZHAM_DECOMP_STATUS_INVALID_PARAMETER;
            }
         }
      }

      SwitchToFiber(pState->m_pWorker_fiber);

      return pState->m_status;
   }

   lzham_decompress_status_t lzham_lib_decompress_memory(const lzham_decompress_params *pParams, lzham_uint8* pDst_buf, size_t *pDst_len, const lzham_uint8* pSrc_buf, size_t src_len, lzham_uint32 *pAdler32)
   {
      if (!pParams)
         return LZHAM_DECOMP_STATUS_INVALID_PARAMETER;

      lzham_decompress_params params(*pParams);
      params.m_output_unbuffered = true;

      lzham_decompress_state_ptr pState = lzham_lib_decompress_init(&params);
      if (!pState)
         return LZHAM_DECOMP_STATUS_FAILED;

      lzham_decompress_status_t status = lzham_lib_decompress(pState, pSrc_buf, &src_len, pDst_buf, pDst_len, true);

      uint32 adler32 = lzham_lib_decompress_deinit(pState);
      if (pAdler32)
         *pAdler32 = adler32;

      return status;
   }

} // namespace lzham
