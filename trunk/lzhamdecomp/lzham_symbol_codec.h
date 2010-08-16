// File: lzham_symbol_codec.h
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
#include "lzham_prefix_coding.h"

namespace lzham
{
   class symbol_codec;
   class adaptive_arith_data_model;
   
   const uint cSymbolCodecArithMinLen = 0x01000000U;   
   const uint cSymbolCodecArithMaxLen = 0xFFFFFFFFU;      

   const uint cSymbolCodecArithProbBits = 11;
   const uint cSymbolCodecArithProbScale = 1 << cSymbolCodecArithProbBits;
   const uint cSymbolCodecArithProbMoveBits = 5;

   const uint cSymbolCodecArithProbMulBits    = 8;
   const uint cSymbolCodecArithProbMulScale   = 1 << cSymbolCodecArithProbMulBits;
               
   class quasi_adaptive_huffman_data_model
   {  
   public:
      quasi_adaptive_huffman_data_model(bool encoding = true, uint total_syms = 0, bool fast_encoding = false, bool use_polar_codes = false);
      quasi_adaptive_huffman_data_model(const quasi_adaptive_huffman_data_model& other);
      ~quasi_adaptive_huffman_data_model();
      
      quasi_adaptive_huffman_data_model& operator= (const quasi_adaptive_huffman_data_model& rhs);
                  
      void clear();
      
      bool init(bool encoding, uint total_syms, bool fast_encoding, bool use_polar_codes);
      bool reset();
      
      void rescale();
                              
      uint get_total_syms() const { return m_total_syms; }
      uint get_cost(uint sym) const { return m_code_sizes[sym]; }
            
   public:
      uint                             m_total_syms;
            
      uint                             m_update_cycle;
      uint                             m_symbols_until_update;
      
      uint                             m_total_count;
                  
      lzham::vector<uint16>            m_sym_freq;
                        
      lzham::vector<uint16>            m_codes;
      lzham::vector<uint8>             m_code_sizes;
            
      prefix_coding::decoder_tables*   m_pDecode_tables;
      
      uint8                            m_decoder_table_bits;
      bool                             m_encoding;
      bool                             m_fast_updating;
      bool                             m_use_polar_codes;
      
      bool update();
      
      friend class symbol_codec;
   };
      
   class adaptive_bit_model
   {
   public:
      adaptive_bit_model();
      adaptive_bit_model(float prob0);
      adaptive_bit_model(const adaptive_bit_model& other);
      
      adaptive_bit_model& operator= (const adaptive_bit_model& rhs);

      void clear();
      void set_probability_0(float prob0);
      void update(uint bit);
      
      float get_cost(uint bit) const;

   public:
      uint16 m_bit_0_prob;
      
      friend class symbol_codec;
      friend class adaptive_arith_data_model;
   };
   
   class adaptive_arith_data_model
   {
   public:
      adaptive_arith_data_model(bool encoding = true, uint total_syms = 0);
      adaptive_arith_data_model(const adaptive_arith_data_model& other);
      ~adaptive_arith_data_model();

      adaptive_arith_data_model& operator= (const adaptive_arith_data_model& rhs);

      void clear();

      bool init(bool encoding, uint total_syms);
      void reset();
      
      uint get_total_syms() const { return m_total_syms; }
      float get_cost(uint sym) const;
      
   private:
      uint m_total_syms;
      typedef lzham::vector<adaptive_bit_model> adaptive_bit_model_vector;
      adaptive_bit_model_vector m_probs;
   
      friend class symbol_codec;
   };

#if (defined(LZHAM_PLATFORM_X360) || defined(LZHAM_PLATFORM_PC_X64))
   #define LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER 1
#else
   #define LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER 0
#endif   
          
   class symbol_codec
   {
   public:
      symbol_codec();
      
      void clear();
      
      // Encoding
      bool start_encoding(uint expected_file_size);
      bool encode_bits(uint bits, uint num_bits);
      bool encode_align_to_byte();
      bool encode(uint sym, quasi_adaptive_huffman_data_model& model);
      bool encode_truncated_binary(uint v, uint n);
      static uint encode_truncated_binary_cost(uint v, uint n);
      bool encode_golomb(uint v, uint m);
      bool encode_rice(uint v, uint m);
      static uint encode_rice_get_cost(uint v, uint m);
      bool encode(uint bit, adaptive_bit_model& model, bool update_model = true);
      bool encode(uint sym, adaptive_arith_data_model& model);
            
      inline void encode_enable_simulation(bool enabled) { m_simulate_encoding = enabled; }
      inline bool encode_get_simulation() { return m_simulate_encoding; }
      inline uint encode_get_total_bits_written() const { return m_total_bits_written; }
      
      bool stop_encoding(bool support_arith);
      
      const lzham::vector<uint8>& get_encoding_buf() const  { return m_output_buf; }     
            lzham::vector<uint8>& get_encoding_buf()        { return m_output_buf; }     
                  
      // Decoding
      
      typedef void (*need_bytes_func_ptr)(size_t num_bytes_consumed, void *pPrivate_data, const uint8* &pBuf, size_t &buf_size, bool &eof_flag);
      
      bool start_decoding(const uint8* pBuf, size_t buf_size, bool eof_flag = true, need_bytes_func_ptr pNeed_bytes_func = NULL, void *pPrivate_data = NULL);
      void decode_set_input_buffer(const uint8* pBuf, size_t buf_size, const uint8* pBuf_next, bool eof_flag = true);
      inline uint64 decode_get_bytes_consumed() const { return m_pDecode_buf_next - m_pDecode_buf; }
      inline uint64 decode_get_bits_remaining() const { return ((m_pDecode_buf_end - m_pDecode_buf_next) << 3) + m_bit_count; }
      void start_arith_decoding();
      uint decode_bits(uint num_bits);
      uint decode_peek_bits(uint num_bits);
      void decode_remove_bits(uint num_bits);
      void decode_align_to_byte();
      int decode_remove_byte_from_bit_buf();
      uint decode(quasi_adaptive_huffman_data_model& model);
      uint decode_truncated_binary(uint n);
      uint decode_golomb(uint m);
      uint decode_rice(uint m);
      uint decode(adaptive_bit_model& model, bool update_model = true);
      uint decode(adaptive_arith_data_model& model);
      uint64 stop_decoding();
      
      uint get_total_model_updates() const { return m_total_model_updates; }
               
   public:
      const uint8*            m_pDecode_buf;
      const uint8*            m_pDecode_buf_next;
      const uint8*            m_pDecode_buf_end;
      size_t                  m_decode_buf_size;
      bool                    m_decode_buf_eof;
      
      need_bytes_func_ptr     m_pDecode_need_bytes_func;
      void*                   m_pDecode_private_data;

#if LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER
      typedef uint64 bit_buf_t;
      enum { cBitBufSize = 64 };
#else
      typedef uint32 bit_buf_t;
      enum { cBitBufSize = 32 };
#endif      
      
      bit_buf_t               m_bit_buf;
      int                     m_bit_count;

      uint                    m_total_model_updates;   
                     
      lzham::vector<uint8>    m_output_buf;
      lzham::vector<uint8>    m_arith_output_buf;
      
      struct output_symbol
      {
         uint m_bits;
         
         enum { cArithSym = -1, cAlignToByteSym = -2 };
         int16 m_num_bits;
         
         uint16 m_arith_prob0;
      };
      lzham::vector<output_symbol> m_output_syms;
      
      uint                    m_total_bits_written;
      bool                    m_simulate_encoding;
      
      uint                    m_arith_base;
      uint                    m_arith_value;
      uint                    m_arith_length;
      uint                    m_arith_total_bits;
      
      bool                    m_support_arith;
                        
      bool put_bits_init(uint expected_size);
      bool record_put_bits(uint bits, uint num_bits);
            
      void arith_propagate_carry();
      bool arith_renorm_enc_interval();
      void arith_start_encoding();
      bool arith_stop_encoding();
      
      bool put_bits(uint bits, uint num_bits);
      bool put_bits_align_to_byte();
      bool flush_bits();
      bool assemble_output_buf(bool support_arith);
                        
      void get_bits_init();
      uint get_bits(uint num_bits);
      void remove_bits(uint num_bits);
      
      void decode_need_bytes();
            
      enum 
      {  
         cNull,
         cEncoding,
         cDecoding
      } m_mode;
   };
   
#define LZHAM_SYMBOL_CODEC_USE_MACROS 1

#ifdef LZHAM_PLATFORM_X360
   #define LZHAM_READ_BIG_ENDIAN_UINT32(p) *reinterpret_cast<const uint32*>(p)
#elif defined(LZHAM_PLATFORM_PC) && defined(_MSC_VER)
   #define LZHAM_READ_BIG_ENDIAN_UINT32(p) _byteswap_ulong(*reinterpret_cast<const uint32*>(p))
#else   
   #define LZHAM_READ_BIG_ENDIAN_UINT32(p) utils::swap32(*reinterpret_cast<const uint32*>(p))
#endif
   
#if LZHAM_SYMBOL_CODEC_USE_MACROS
   #define LZHAM_SYMBOL_CODEC_DECODE_DECLARE(codec) \
      uint arith_value; \
      uint arith_length; \
      symbol_codec::bit_buf_t bit_buf; \
      int bit_count; \
      const uint8* pDecode_buf_next;
   
   #define LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
      arith_value = codec.m_arith_value; \
      arith_length = codec.m_arith_length; \
      bit_buf = codec.m_bit_buf; \
      bit_count = codec.m_bit_count; \
      pDecode_buf_next = codec.m_pDecode_buf_next;
   
   #define LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
      codec.m_arith_value = arith_value; \
      codec.m_arith_length = arith_length; \
      codec.m_bit_buf = bit_buf; \
      codec.m_bit_count = bit_count; \
      codec.m_pDecode_buf_next = pDecode_buf_next;

   #define LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, result, num_bits) \
   { \
      while (bit_count < (int)(num_bits)) \
      { \
         uint c = 0; \
         if (pDecode_buf_next == codec.m_pDecode_buf_end) \
         { \
            LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
            codec.decode_need_bytes(); \
            LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
            if (pDecode_buf_next < codec.m_pDecode_buf_end) c = *pDecode_buf_next++; \
         } \
         else \
            c = *pDecode_buf_next++; \
         bit_count += 8; \
         bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
      } \
      result = num_bits ? static_cast<uint>(bit_buf >> (symbol_codec::cBitBufSize - (num_bits))) : 0; \
      bit_buf <<= (num_bits); \
      bit_count -= (num_bits); \
   }
   
   #define LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, result, model) \
   { \
      if (arith_length < cSymbolCodecArithMinLen) \
      { \
         uint c; \
         LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, c, 8); \
         arith_value = (arith_value << 8) | c; \
         arith_length <<= 8; \
      } \
      uint x = model.m_bit_0_prob * (arith_length >> cSymbolCodecArithProbBits); \
      result = (arith_value >= x); \
      if (!result) \
      { \
         model.m_bit_0_prob += ((cSymbolCodecArithProbScale - model.m_bit_0_prob) >> cSymbolCodecArithProbMoveBits); \
         arith_length = x; \
      } \
      else \
      { \
         model.m_bit_0_prob -= (model.m_bit_0_prob >> cSymbolCodecArithProbMoveBits); \
         arith_value  -= x; \
         arith_length -= x; \
      } \
   }

#if LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER
   #define LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, result, model) \
   { \
      const prefix_coding::decoder_tables* pTables = model.m_pDecode_tables; \
      if (bit_count < 24) \
      { \
         uint c = 0; \
         pDecode_buf_next += sizeof(uint32); \
         if (pDecode_buf_next >= codec.m_pDecode_buf_end) \
         { \
            pDecode_buf_next -= sizeof(uint32); \
            while (bit_count < 24) \
            { \
               LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
               codec.decode_need_bytes(); \
               LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
               if (pDecode_buf_next < codec.m_pDecode_buf_end) c = *pDecode_buf_next++; \
               bit_count += 8; \
               bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
            } \
         } \
         else \
         { \
            c = LZHAM_READ_BIG_ENDIAN_UINT32(pDecode_buf_next - sizeof(uint32)); \
            bit_count += 32; \
            bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
         } \
      } \
      uint k = static_cast<uint>((bit_buf >> (symbol_codec::cBitBufSize - 16)) + 1); \
      uint len; \
      if (k <= pTables->m_table_max_code) \
      { \
         uint32 t = pTables->m_lookup[bit_buf >> (symbol_codec::cBitBufSize - pTables->m_table_bits)]; \
         result = t & UINT16_MAX; \
         len = t >> 16; \
      } \
      else \
      { \
         len = pTables->m_decode_start_code_size; \
         for ( ; ; ) \
         { \
            if (k <= pTables->m_max_codes[len - 1]) \
               break; \
            len++; \
         } \
         int val_ptr = pTables->m_val_ptrs[len - 1] + static_cast<int>(bit_buf >> (symbol_codec::cBitBufSize - len)); \
         if (((uint)val_ptr >= model.m_total_syms)) val_ptr = 0; \
         result = pTables->m_sorted_symbol_order[val_ptr]; \
      }  \
      bit_buf <<= len; \
      bit_count -= len; \
      uint freq = model.m_sym_freq[result]; \
      freq++; \
      model.m_sym_freq[result] = static_cast<uint16>(freq); \
      if (freq == UINT16_MAX) model.rescale(); \
      if (--model.m_symbols_until_update == 0) \
      { \
         model.update(); \
      } \
   }
#else
   #define LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, result, model) \
   { \
      const prefix_coding::decoder_tables* pTables = model.m_pDecode_tables; \
      while (bit_count < (symbol_codec::cBitBufSize - 8)) \
      { \
         uint c = 0; \
         if (pDecode_buf_next == codec.m_pDecode_buf_end) \
         { \
            LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
            codec.decode_need_bytes(); \
            LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
            if (pDecode_buf_next < codec.m_pDecode_buf_end) c = *pDecode_buf_next++; \
         } \
         else \
            c = *pDecode_buf_next++; \
         bit_count += 8; \
         bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
      } \
      uint k = static_cast<uint>((bit_buf >> (symbol_codec::cBitBufSize - 16)) + 1); \
      uint len; \
      if (k <= pTables->m_table_max_code) \
      { \
         uint32 t = pTables->m_lookup[bit_buf >> (symbol_codec::cBitBufSize - pTables->m_table_bits)]; \
         result = t & UINT16_MAX; \
         len = t >> 16; \
      } \
      else \
      { \
         len = pTables->m_decode_start_code_size; \
         for ( ; ; ) \
         { \
            if (k <= pTables->m_max_codes[len - 1]) \
               break; \
            len++; \
         } \
         int val_ptr = pTables->m_val_ptrs[len - 1] + static_cast<int>(bit_buf >> (symbol_codec::cBitBufSize - len)); \
         if (((uint)val_ptr >= model.m_total_syms)) val_ptr = 0; \
         result = pTables->m_sorted_symbol_order[val_ptr]; \
      }  \
      bit_buf <<= len; \
      bit_count -= len; \
      uint freq = model.m_sym_freq[result]; \
      freq++; \
      model.m_sym_freq[result] = static_cast<uint16>(freq); \
      if (freq == UINT16_MAX) model.rescale(); \
      if (--model.m_symbols_until_update == 0) \
      { \
         model.update(); \
      } \
   }
#endif   
   
#else
   #define LZHAM_SYMBOL_CODEC_DECODE_DECLARE(codec)
   #define LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec)
   #define LZHAM_SYMBOL_CODEC_DECODE_END(codec)

   #define LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, result, num_bits) result = codec.decode_bits(num_bits);
   #define LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, result, model) result = codec.decode(model);
   #define LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, result, model) result = codec.decode(model);
#endif   

} // namespace lzham

