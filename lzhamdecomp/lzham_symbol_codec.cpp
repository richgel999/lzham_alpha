// File: lzham_symbol_codec.cpp
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
#include "lzham_symbol_codec.h"
#include "lzham_huffman_codes.h"
#include "lzham_polar_codes.h"

namespace lzham
{
   static float gProbCost[cSymbolCodecArithProbScale];
   
   //const uint cArithProbMulLenSigBits  = 8;
   //const uint cArithProbMulLenSigScale  = 1 << cArithProbMulLenSigBits;
   
   class arith_prob_cost_initializer
   {
   public:
      arith_prob_cost_initializer()
      {
         const float cInvLn2 = 1.0f / 0.69314718f;

         for (uint i = 0; i < cSymbolCodecArithProbScale; i++)
            gProbCost[i] = -logf(i * (1.0f / cSymbolCodecArithProbScale)) * cInvLn2;
      }
   };
   
   static arith_prob_cost_initializer g_prob_cost_initializer;
   
   quasi_adaptive_huffman_data_model::quasi_adaptive_huffman_data_model(bool encoding, uint total_syms, bool fast_updating, bool use_polar_codes) :
      m_total_syms(0),
      m_update_cycle(0),
      m_symbols_until_update(0),
      m_encoding(encoding),
      m_pDecode_tables(NULL),
      m_decoder_table_bits(0),
      m_total_count(0),
      m_fast_updating(false),
      m_use_polar_codes(false)
   {
      if (total_syms)
      {
         init(encoding, total_syms, fast_updating, use_polar_codes);
      }
   }
   
   quasi_adaptive_huffman_data_model::quasi_adaptive_huffman_data_model(const quasi_adaptive_huffman_data_model& other) :
      m_total_syms(0),
      m_update_cycle(0),
      m_symbols_until_update(0),
      m_encoding(false),
      m_pDecode_tables(NULL),
      m_decoder_table_bits(0),
      m_total_count(0),
      m_fast_updating(false),
      m_use_polar_codes(false)
   {
      *this = other;
   }
   
   quasi_adaptive_huffman_data_model::~quasi_adaptive_huffman_data_model()
   {
      if (m_pDecode_tables)
         lzham_delete(m_pDecode_tables);
   }
   
   quasi_adaptive_huffman_data_model& quasi_adaptive_huffman_data_model::operator= (const quasi_adaptive_huffman_data_model& rhs)
   {
      if (this == &rhs)
         return *this;
               
      m_total_syms = rhs.m_total_syms;

      m_update_cycle = rhs.m_update_cycle;
      m_symbols_until_update = rhs.m_symbols_until_update;

      m_total_count = rhs.m_total_count;

      m_sym_freq = rhs.m_sym_freq;

      m_codes = rhs.m_codes;
      m_code_sizes = rhs.m_code_sizes;

      if (rhs.m_pDecode_tables)
      {
         if (m_pDecode_tables)
            *m_pDecode_tables = *rhs.m_pDecode_tables;
         else
         {
            m_pDecode_tables = lzham_new<prefix_coding::decoder_tables>(*rhs.m_pDecode_tables);
            if (!m_pDecode_tables)
            {
               clear();
               return *this;
            }
         }
      }
      else
      {
         lzham_delete(m_pDecode_tables);
         m_pDecode_tables = NULL;
      }

      m_decoder_table_bits = rhs.m_decoder_table_bits;
      m_encoding = rhs.m_encoding;
      m_fast_updating = rhs.m_fast_updating;
      m_use_polar_codes = rhs.m_use_polar_codes;
      
      return *this;
   }
   
   void quasi_adaptive_huffman_data_model::clear()
   {
      m_sym_freq.clear();
      m_codes.clear();
      m_code_sizes.clear();
      
      m_total_syms = 0;
      m_update_cycle = 0;
      m_symbols_until_update = 0;
      m_decoder_table_bits = 0;
      m_total_count = 0;
         
      if (m_pDecode_tables)
      {
         lzham_delete(m_pDecode_tables);
         m_pDecode_tables = NULL;
      }
      
      m_fast_updating = false;
      m_use_polar_codes = false;
   }
   
   bool quasi_adaptive_huffman_data_model::init(bool encoding, uint total_syms, bool fast_updating, bool use_polar_codes)
   {
      clear();
      
      m_encoding = encoding;
      m_fast_updating = fast_updating;
      m_use_polar_codes = use_polar_codes;
      
      if (!m_sym_freq.try_resize(total_syms))
      {
         clear();
         return false;
      }
      if (!m_code_sizes.try_resize(total_syms))
      {
         clear();
         return false;
      }
      
      m_total_syms = total_syms;
      
      if (m_total_syms <= 16)
         m_decoder_table_bits = 0;
      else
         m_decoder_table_bits = static_cast<uint8>(math::minimum(1 + math::ceil_log2i(m_total_syms), prefix_coding::cMaxTableBits));
      
      if (m_encoding)
      {
         if (!m_codes.try_resize(total_syms))
            return false;
      }
      else
      {
         m_pDecode_tables = lzham_new<prefix_coding::decoder_tables>();
         if (!m_pDecode_tables)
         {
            clear();
            return false;
         }
      }
      
      reset();
      
      return true;
   }
   
   bool quasi_adaptive_huffman_data_model::reset()
   {
      if (!m_total_syms)
         return true;
      
      for (uint i = 0; i < m_total_syms; i++)
         m_sym_freq[i] = 1;
    
      m_total_count = 0;
      m_update_cycle = m_total_syms;
      
      if (!update())
         return false;
      
      m_symbols_until_update = m_update_cycle = 8;//(m_total_syms + 6) >> 1;
      return true;
   }
   
   void quasi_adaptive_huffman_data_model::rescale()
   {
      uint total_freq = 0;

      for (uint i = 0; i < m_total_syms; i++)
      {
         uint freq = (m_sym_freq[i] + 1) >> 1;
         total_freq += freq;
         m_sym_freq[i] = static_cast<uint16>(freq);
      }

      m_total_count = total_freq;
   }
         
   bool quasi_adaptive_huffman_data_model::update()
   {
      m_total_count += m_update_cycle;
      
      if (m_total_count >= 32768)
         rescale();
         
      uint table_size = m_use_polar_codes ? get_generate_polar_codes_table_size() : get_generate_huffman_codes_table_size();
      void *pTables = _alloca(table_size);
            
      uint max_code_size, total_freq;
      bool status;
      if (m_use_polar_codes)
         status = generate_polar_codes(pTables, m_total_syms, &m_sym_freq[0], &m_code_sizes[0], max_code_size, total_freq);
      else
         status = generate_huffman_codes(pTables, m_total_syms, &m_sym_freq[0], &m_code_sizes[0], max_code_size, total_freq);
      LZHAM_ASSERT(status);
      LZHAM_ASSERT(total_freq == m_total_count);
      if ((!status) || (total_freq != m_total_count))
         return false;
    
      if (max_code_size > prefix_coding::cMaxExpectedCodeSize)
      {
         bool status = prefix_coding::limit_max_code_size(m_total_syms, &m_code_sizes[0], prefix_coding::cMaxExpectedCodeSize);
         LZHAM_ASSERT(status);
         if (!status)
            return false;
      }

      if (m_encoding)           
         status = prefix_coding::generate_codes(m_total_syms, &m_code_sizes[0], &m_codes[0]);
      else
         status = prefix_coding::generate_decoder_tables(m_total_syms, &m_code_sizes[0], m_pDecode_tables, m_decoder_table_bits);
       
      LZHAM_ASSERT(status);
      if (!status)
         return false;
      
      uint max_cycle;
      if (m_fast_updating)
      {
         m_update_cycle = 2 * m_update_cycle;
         max_cycle = (LZHAM_MAX(64, m_total_syms) + 6) << 5; 
      }
      else
      {
         m_update_cycle = (5 * m_update_cycle) >> 2;
         max_cycle = (LZHAM_MAX(32, m_total_syms) + 6) << 3; // this was << 2 - which decompresses ~12% slower 
      }
                        
      if (m_update_cycle > max_cycle) 
         m_update_cycle = max_cycle;
                     
      m_symbols_until_update = m_update_cycle;
      
      return true;
   }
            
   adaptive_bit_model::adaptive_bit_model()
   {
      clear();
   }
         
   adaptive_bit_model::adaptive_bit_model(float prob0)
   {
      set_probability_0(prob0);
   }
   
   adaptive_bit_model::adaptive_bit_model(const adaptive_bit_model& other) :
      m_bit_0_prob(other.m_bit_0_prob)
   {
   }

   adaptive_bit_model& adaptive_bit_model::operator= (const adaptive_bit_model& rhs)
   {
      m_bit_0_prob = rhs.m_bit_0_prob;
      return *this;
   }

   void adaptive_bit_model::clear()
   {
      m_bit_0_prob  = 1U << (cSymbolCodecArithProbBits - 1);
   }
   
   void adaptive_bit_model::set_probability_0(float prob0)
   {
      m_bit_0_prob = static_cast<uint16>(math::clamp<uint>((uint)(prob0 * cSymbolCodecArithProbScale), 1, cSymbolCodecArithProbScale - 1));
   }
   
   float adaptive_bit_model::get_cost(uint bit) const
   {
      return gProbCost[bit ? (cSymbolCodecArithProbScale - m_bit_0_prob) : m_bit_0_prob];
   }
   
   void adaptive_bit_model::update(uint bit)
   {
      if (!bit)
         m_bit_0_prob += ((cSymbolCodecArithProbScale - m_bit_0_prob) >> cSymbolCodecArithProbMoveBits);
      else
         m_bit_0_prob -= (m_bit_0_prob >> cSymbolCodecArithProbMoveBits);
      LZHAM_ASSERT(m_bit_0_prob >= 1);
      LZHAM_ASSERT(m_bit_0_prob < cSymbolCodecArithProbScale);
   }
   
   adaptive_arith_data_model::adaptive_arith_data_model(bool encoding, uint total_syms) 
   {
      init(encoding, total_syms);
   }
   
   adaptive_arith_data_model::adaptive_arith_data_model(const adaptive_arith_data_model& other)
   {
      m_total_syms = other.m_total_syms;
      m_probs = other.m_probs;
   }   
   
   adaptive_arith_data_model::~adaptive_arith_data_model()
   {
   }

   adaptive_arith_data_model& adaptive_arith_data_model::operator= (const adaptive_arith_data_model& rhs)
   {
      m_total_syms = rhs.m_total_syms;
      m_probs = rhs.m_probs;
      return *this;
   }

   void adaptive_arith_data_model::clear()
   {
      m_total_syms = 0;
      m_probs.clear();
   }

   bool adaptive_arith_data_model::init(bool encoding, uint total_syms)
   {
      encoding;
      if (!total_syms)
      {
         clear();
         return true;
      }
      
      if ((total_syms < 2) || (!math::is_power_of_2(total_syms)))
         total_syms = math::next_pow2(total_syms);
      
      m_total_syms = total_syms;
      
      if (!m_probs.try_resize(m_total_syms))
         return false;
      
      return true;
   }   
   
   void adaptive_arith_data_model::reset()
   {  
      for (uint i = 0; i < m_probs.size(); i++)
         m_probs[i].clear();
   }
   
   float adaptive_arith_data_model::get_cost(uint sym) const
   {
      uint node = 1;
      
      uint bitmask = m_total_syms;
      
      float cost = 0.0f;
      do 
      {
         bitmask >>= 1;
         
         uint bit = (sym & bitmask) ? 1 : 0;
         cost += m_probs[node].get_cost(bit);
         node = (node << 1) + bit;
         
      } while (bitmask > 1);
      
      return cost;
   }
   
   symbol_codec::symbol_codec() 
   {
      clear();
   }
   
   void symbol_codec::clear()
   {
      m_pDecode_buf = NULL;
      m_pDecode_buf_next = NULL;
      m_pDecode_buf_end = NULL;
      m_decode_buf_size = 0;
      
      m_bit_buf = 0;
      m_bit_count = 0;
      m_total_model_updates = 0;
      m_mode = cNull;
      m_simulate_encoding = false;
      m_total_bits_written = 0;
      
      m_arith_base = 0;
      m_arith_value = 0;
      m_arith_length = 0;
      m_arith_total_bits = 0;
      
      m_output_buf.clear();
      m_arith_output_buf.clear();
      m_output_syms.clear();
   }
   
   bool symbol_codec::start_encoding(uint expected_file_size)
   {
      m_mode = cEncoding;
      
      m_total_model_updates = 0;
      m_total_bits_written = 0;
            
      put_bits_init(expected_file_size);
            
      if (!m_output_syms.try_resize(0))
         return false;
            
      arith_start_encoding();
      
      return true;
   }
               
   bool symbol_codec::encode_bits(uint bits, uint num_bits)
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      
      if (!num_bits)
         return true;
      
      LZHAM_ASSERT((num_bits == 32) || (bits <= ((1U << num_bits) - 1)));
      
      if (num_bits > 16)
      {
         if (!record_put_bits(bits >> 16, num_bits - 16)) 
            return false;
         if (!record_put_bits(bits & 0xFFFF, 16)) 
            return false;
      }
      else
      {
         if (!record_put_bits(bits, num_bits)) 
            return false;
      }
      return true;
   }
   
   bool symbol_codec::encode_align_to_byte()
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      
      if (!m_simulate_encoding)
      {
         output_symbol sym;
         sym.m_bits = 0;
         sym.m_num_bits = output_symbol::cAlignToByteSym;
         sym.m_arith_prob0 = 0;
         if (!m_output_syms.try_push_back(sym))
            return false;
      }
      else
      {
         // We really don't know how many we're going to write, so just be conservative.
         m_total_bits_written += 7;
      }         
      return true;
   }
   
   bool symbol_codec::encode(uint sym, quasi_adaptive_huffman_data_model& model)
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      LZHAM_ASSERT(model.m_encoding);
            
      if (!record_put_bits(model.m_codes[sym], model.m_code_sizes[sym]))
         return false;
      
      uint freq = model.m_sym_freq[sym];
      freq++;
      model.m_sym_freq[sym] = static_cast<uint16>(freq);
      
      if (freq == UINT16_MAX)
         model.rescale();
         
      if (--model.m_symbols_until_update == 0)
      {
         m_total_model_updates++;
         if (!model.update()) 
            return false;
      }
      return true;
   }
         
   bool symbol_codec::encode_truncated_binary(uint v, uint n)
   {
      LZHAM_ASSERT((n >= 2) && (v < n));
      
      uint k = math::floor_log2i(n);
      uint u = (1 << (k + 1)) - n;
      
      if (v < u)
         return encode_bits(v, k);  
      else
         return encode_bits(v + u, k + 1);
   }
   
   uint symbol_codec::encode_truncated_binary_cost(uint v, uint n)
   {
      LZHAM_ASSERT((n >= 2) && (v < n));

      uint k = math::floor_log2i(n);
      uint u = (1 << (k + 1)) - n;

      if (v < u)
         return k;
      else
         return k + 1;
   }
   
   bool symbol_codec::encode_golomb(uint v, uint m)
   {
      LZHAM_ASSERT(m > 0);
      
      uint q = v / m;
      uint r = v % m;
            
      while (q > 16)
      {
         if (!encode_bits(0xFFFF, 16)) 
            return false;
         q -= 16;
      }
      
      if (q)
      {
         if (!encode_bits( (1 << q) - 1, q)) 
            return false;
      }
         
      if (!encode_bits(0, 1)) 
         return false;
      
      return encode_truncated_binary(r, m);
   }
   
   bool symbol_codec::encode_rice(uint v, uint m)
   {
      LZHAM_ASSERT(m > 0);
                  
      uint q = v >> m;
      uint r = v & ((1 << m) - 1);

      while (q > 16)
      {
         if (!encode_bits(0xFFFF, 16)) 
            return false;
         q -= 16;
      }

      if (q)
      {
         if (!encode_bits( (1 << q) - 1, q)) 
            return false;
      }

      if (!encode_bits(0, 1)) 
         return false;
      if (!encode_bits(r, m)) 
         return false;
      
      return true;
   }
   
   uint symbol_codec::encode_rice_get_cost(uint v, uint m)
   {
      LZHAM_ASSERT(m > 0);
      
      uint q = v >> m;
      //uint r = v & ((1 << m) - 1);

      return q + 1 + m;
   }
   
   void symbol_codec::arith_propagate_carry()
   {
      int index = m_arith_output_buf.size() - 1;
      while (index >= 0)
      {
         uint c = m_arith_output_buf[index];
                  
         if (c == 0xFF)
            m_arith_output_buf[index] = 0;
         else
         {
            m_arith_output_buf[index]++;
            break;
         }
         
         index--;
      }
   }

   bool symbol_codec::arith_renorm_enc_interval()
   {
      do 
      {                    
         if (!m_arith_output_buf.try_push_back( (m_arith_base >> 24) & 0xFF ))
            return false;
         m_total_bits_written += 8;
                  
         m_arith_base <<= 8;
      } while ((m_arith_length <<= 8) < cSymbolCodecArithMinLen);        
      return true;
   }
      
   void symbol_codec::arith_start_encoding()
   {
      m_arith_output_buf.try_resize(0);
      
      m_arith_base = 0;
      m_arith_value = 0;
      m_arith_length = cSymbolCodecArithMaxLen;
      m_arith_total_bits = 0;
   }
            
   bool symbol_codec::encode(uint bit, adaptive_bit_model& model, bool update_model)
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      
      m_arith_total_bits++;
            
      if (!m_simulate_encoding)
      {
         output_symbol sym;
         sym.m_bits = bit;
         sym.m_num_bits = -1;
         sym.m_arith_prob0 = model.m_bit_0_prob;
         if (!m_output_syms.try_push_back(sym)) 
            return false;
      }
               
      //uint x = gArithProbMulTab[model.m_bit_0_prob >> (cSymbolCodecArithProbBits - cSymbolCodecArithProbMulBits)][m_arith_length >> (32 - cSymbolCodecArithProbMulLenSigBits)] << 16;
      uint x = model.m_bit_0_prob * (m_arith_length >> cSymbolCodecArithProbBits);
      
      if (!bit)
      {
         if (update_model)
            model.m_bit_0_prob += ((cSymbolCodecArithProbScale - model.m_bit_0_prob) >> cSymbolCodecArithProbMoveBits);
         
         m_arith_length = x;
      }
      else 
      {
         if (update_model)
            model.m_bit_0_prob -= (model.m_bit_0_prob >> cSymbolCodecArithProbMoveBits);
         
         uint orig_base = m_arith_base;
         m_arith_base   += x;
         m_arith_length -= x;
         if (orig_base > m_arith_base) 
            arith_propagate_carry();               
      }

      if (m_arith_length < cSymbolCodecArithMinLen) 
      {
         if (!arith_renorm_enc_interval()) 
            return false;
      }
      return true;
   }
   
   bool symbol_codec::encode(uint sym, adaptive_arith_data_model& model)
   {
      uint node = 1;

      uint bitmask = model.m_total_syms;

      do 
      {
         bitmask >>= 1;

         uint bit = (sym & bitmask) ? 1 : 0;
         if (!encode(bit, model.m_probs[node])) 
            return false;
         node = (node << 1) + bit;

      } while (bitmask > 1);
      return true;
   }
   
   bool symbol_codec::arith_stop_encoding()
   {
      if (!m_arith_total_bits)
         return true;

      uint orig_base = m_arith_base;            

      if (m_arith_length > 2 * cSymbolCodecArithMinLen) 
      {
         m_arith_base  += cSymbolCodecArithMinLen;                 
         m_arith_length = (cSymbolCodecArithMinLen >> 1);
      }
      else 
      {
         m_arith_base  += (cSymbolCodecArithMinLen >> 1);            
         m_arith_length = (cSymbolCodecArithMinLen >> 9);            
      }

      if (orig_base > m_arith_base) 
         arith_propagate_carry();                 

      if (!arith_renorm_enc_interval()) 
         return false;
      
      while (m_arith_output_buf.size() < 4)
      {
         if (!m_arith_output_buf.try_push_back(0)) 
            return false;
         m_total_bits_written += 8;
      }             
      return true;
   }
         
   bool symbol_codec::stop_encoding(bool support_arith)
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      
      if (!arith_stop_encoding()) 
         return false;
         
      if (!m_simulate_encoding)
      {
         if (!assemble_output_buf(support_arith)) 
            return false;
      }
                  
      m_mode = cNull;
      return true;
   }
   
   bool symbol_codec::record_put_bits(uint bits, uint num_bits)
   {
      LZHAM_ASSERT(m_mode == cEncoding);
      
      LZHAM_ASSERT(num_bits <= 25);
      LZHAM_ASSERT(m_bit_count >= 25);

      if (!num_bits)
         return true;
      
      m_total_bits_written += num_bits;

      if (!m_simulate_encoding)
      {
         output_symbol sym;
         sym.m_bits = bits;
         sym.m_num_bits = (uint16)num_bits;
         sym.m_arith_prob0 = 0;
         if (!m_output_syms.try_push_back(sym))
            return false;
      }
      return true;
   }
   
   bool symbol_codec::put_bits_init(uint expected_size)
   {
      m_bit_buf = 0;
      m_bit_count = cBitBufSize;
            
      m_output_buf.try_resize(0);
      if (!m_output_buf.try_reserve(expected_size))
         return false;
      
      return true;
   }
            
   bool symbol_codec::put_bits(uint bits, uint num_bits)
   {
      LZHAM_ASSERT(num_bits <= 25);
      LZHAM_ASSERT(m_bit_count >= 25);

      if (!num_bits)
         return true;
                  
      m_bit_count -= num_bits;
      m_bit_buf |= (static_cast<bit_buf_t>(bits) << m_bit_count);
      
      m_total_bits_written += num_bits;
      
      while (m_bit_count <= (cBitBufSize - 8))
      {
         if (!m_output_buf.try_push_back(static_cast<uint8>(m_bit_buf >> (cBitBufSize - 8))))
            return false;
                 
         m_bit_buf <<= 8;
         m_bit_count += 8;
      }
      
      return true;
   }
   
   bool symbol_codec::put_bits_align_to_byte()
   {
      uint num_bits_in = cBitBufSize - m_bit_count;
      if (num_bits_in & 7)
      {
         if (!put_bits(0, 8 - (num_bits_in & 7)))
            return false;
      }
      return true;
   }
   
   bool symbol_codec::flush_bits()
   {
      return put_bits(0, 7); // to ensure the last bits are flushed
   }
   
   bool symbol_codec::assemble_output_buf(bool support_arith)
   {
      m_total_bits_written = 0;
      
      uint arith_buf_ofs = 0;
      
      if (support_arith)
      {
         if (m_arith_output_buf.size())
         {
            if (!put_bits(1, 1)) 
               return false;
            
            m_arith_length = cSymbolCodecArithMaxLen;
            m_arith_value = 0;
            for (uint i = 0; i < 4; i++)
            {
               const uint c = m_arith_output_buf[arith_buf_ofs++];
               m_arith_value = (m_arith_value << 8) | c;
               if (!put_bits(c, 8)) 
                  return false;
            }
         }
         else
         {
            if (!put_bits(0, 1)) 
               return false;
         }
      }         
      
      for (uint sym_index = 0; sym_index < m_output_syms.size(); sym_index++)
      {
         const output_symbol& sym = m_output_syms[sym_index];
         
         if (sym.m_num_bits == output_symbol::cAlignToByteSym)
         {
            if (!put_bits_align_to_byte()) 
               return false;
         }
         else if (sym.m_num_bits == output_symbol::cArithSym)
         {
            if (m_arith_length < cSymbolCodecArithMinLen) 
            {
               do 
               {                                          
                  const uint c = (arith_buf_ofs < m_arith_output_buf.size()) ? m_arith_output_buf[arith_buf_ofs++] : 0;
                  if (!put_bits(c, 8)) 
                     return false;
                  m_arith_value = (m_arith_value << 8) | c;
               } while ((m_arith_length <<= 8) < cSymbolCodecArithMinLen);
            }
            
            //uint x = gArithProbMulTab[sym.m_arith_prob0 >> (cSymbolCodecArithProbBits - cSymbolCodecArithProbMulBits)][m_arith_length >> (32 - cSymbolCodecArithProbMulLenSigBits)] << 16;
            uint x = sym.m_arith_prob0 * (m_arith_length >> cSymbolCodecArithProbBits);
            uint bit = (m_arith_value >= x);

            if (bit == 0) 
            {
               m_arith_length = x;
            }
            else 
            {
               m_arith_value  -= x;
               m_arith_length -= x;
            }

            LZHAM_VERIFY(bit == sym.m_bits);
         }
         else
         {
            if (!put_bits(sym.m_bits, sym.m_num_bits)) 
               return false;
         }
      }
      
      return flush_bits();
   }
      
   //------------------------------------------------------------------------------------------------------------------
   // Decoding
   //------------------------------------------------------------------------------------------------------------------
   
   bool symbol_codec::start_decoding(const uint8* pBuf, size_t buf_size, bool eof_flag, need_bytes_func_ptr pNeed_bytes_func, void *pPrivate_data)
   {  
      if (!buf_size)
         return false;
                  
      m_total_model_updates = 0;                  
      
      m_pDecode_buf = pBuf;
      m_pDecode_buf_next = pBuf;
      m_decode_buf_size = buf_size;
      m_pDecode_buf_end = pBuf + buf_size;
      
      m_pDecode_need_bytes_func = pNeed_bytes_func;
      m_pDecode_private_data = pPrivate_data;
      m_decode_buf_eof = eof_flag;
      if (!pNeed_bytes_func)
      {
         m_decode_buf_eof = true;
      }
      
      m_mode = cDecoding;
            
      get_bits_init();
                  
      return true;
   }
            
   uint symbol_codec::decode_bits(uint num_bits)
   {
      LZHAM_ASSERT(m_mode == cDecoding);
      
      if (!num_bits)
         return 0;
      
      if (num_bits > 16)
      {
         uint a = get_bits(num_bits - 16);
         uint b = get_bits(16);
         
         return (a << 16) | b;
      }
      else
         return get_bits(num_bits);
   }
   
   void symbol_codec::decode_remove_bits(uint num_bits)
   {
      LZHAM_ASSERT(m_mode == cDecoding);

      while (num_bits > 16)
      {
         remove_bits(16);
         num_bits -= 16;
      }
      
      remove_bits(num_bits);
   }
         
   uint symbol_codec::decode_peek_bits(uint num_bits)
   {
      LZHAM_ASSERT(m_mode == cDecoding);
      LZHAM_ASSERT(num_bits <= 25);

      if (!num_bits)
         return 0;

      while (m_bit_count < (int)num_bits)
      {
         uint c = 0;
         if (m_pDecode_buf_next == m_pDecode_buf_end)
         {
            if (!m_decode_buf_eof)
            {
               m_pDecode_need_bytes_func(m_pDecode_buf_next - m_pDecode_buf, m_pDecode_private_data, m_pDecode_buf, m_decode_buf_size, m_decode_buf_eof);
               m_pDecode_buf_end = m_pDecode_buf + m_decode_buf_size;
               m_pDecode_buf_next = m_pDecode_buf;
               if (m_pDecode_buf_next < m_pDecode_buf_end) c = *m_pDecode_buf_next++;
            }
         }
         else
            c = *m_pDecode_buf_next++;

         m_bit_count += 8;
         LZHAM_ASSERT(m_bit_count <= cBitBufSize);

         m_bit_buf |= (static_cast<bit_buf_t>(c) << (cBitBufSize - m_bit_count));
      }

      return static_cast<uint>(m_bit_buf >> (cBitBufSize - num_bits));
   }
   
   uint symbol_codec::decode(quasi_adaptive_huffman_data_model& model)
   {
      LZHAM_ASSERT(m_mode == cDecoding);
      LZHAM_ASSERT(!model.m_encoding);
      
      const prefix_coding::decoder_tables* pTables = model.m_pDecode_tables;
                  
      while (m_bit_count < (cBitBufSize - 8))
      {
         uint c = 0;
         if (m_pDecode_buf_next == m_pDecode_buf_end)
         {
            if (!m_decode_buf_eof)
            {
               m_pDecode_need_bytes_func(m_pDecode_buf_next - m_pDecode_buf, m_pDecode_private_data, m_pDecode_buf, m_decode_buf_size, m_decode_buf_eof);
               m_pDecode_buf_end = m_pDecode_buf + m_decode_buf_size;
               m_pDecode_buf_next = m_pDecode_buf;
               if (m_pDecode_buf_next < m_pDecode_buf_end) c = *m_pDecode_buf_next++;
            }
         }
         else
            c = *m_pDecode_buf_next++;
         
         m_bit_count += 8;
         m_bit_buf |= (static_cast<bit_buf_t>(c) << (cBitBufSize - m_bit_count));
      }

      uint k = static_cast<uint>((m_bit_buf >> (cBitBufSize - 16)) + 1);
      uint sym, len;

      if (k <= pTables->m_table_max_code)
      {
         uint32 t = pTables->m_lookup[m_bit_buf >> (cBitBufSize - pTables->m_table_bits)];

         LZHAM_ASSERT(t != UINT32_MAX);
         sym = t & UINT16_MAX;
         len = t >> 16;
         
         LZHAM_ASSERT(model.m_code_sizes[sym] == len);
      }
      else
      {
         len = pTables->m_decode_start_code_size;

         for ( ; ; )
         {
            if (k <= pTables->m_max_codes[len - 1])
               break;
            len++;
         }
         
         int val_ptr = pTables->m_val_ptrs[len - 1] + static_cast<int>((m_bit_buf >> (cBitBufSize - len)));

         if (((uint)val_ptr >= model.m_total_syms))
         {  
            // corrupted stream, or a bug
            LZHAM_ASSERT(0); 
            return 0;
         }

         sym = pTables->m_sorted_symbol_order[val_ptr];
      }  

      m_bit_buf <<= len;
      m_bit_count -= len;
      
      uint freq = model.m_sym_freq[sym];
      freq++;
      model.m_sym_freq[sym] = static_cast<uint16>(freq);
      
      if (freq == UINT16_MAX)
         model.rescale();
         
      if (--model.m_symbols_until_update == 0)
      {
         m_total_model_updates++;
         model.update();
      }
      
      return sym;
   }
   
   void symbol_codec::decode_set_input_buffer(const uint8* pBuf, size_t buf_size, const uint8* pBuf_next, bool eof_flag)
   {
      LZHAM_ASSERT(m_mode == cDecoding);
      
      m_pDecode_buf = pBuf;
      m_pDecode_buf_next = pBuf_next;
      m_decode_buf_size = buf_size;
      m_pDecode_buf_end = pBuf + buf_size;

      if (!m_pDecode_need_bytes_func)
         m_decode_buf_eof = true;
      else
         m_decode_buf_eof = eof_flag;
   }
   
   uint symbol_codec::decode_truncated_binary(uint n)
   {
      LZHAM_ASSERT(n >= 2);

      uint k = math::floor_log2i(n);
      uint u = (1 << (k + 1)) - n;
      
      uint i = decode_bits(k);
                  
      if (i >= u)
         i = ((i << 1) | decode_bits(1)) - u;

      return i;
   }
   
   uint symbol_codec::decode_golomb(uint m)
   {
      LZHAM_ASSERT(m > 1);
      
      uint q = 0;
      
      for ( ; ; )
      {
         uint k = decode_peek_bits(16);
                  
         uint l = utils::count_leading_zeros16((~k) & 0xFFFF);
         q += l;
         if (l < 16)
            break;
      }
      
      decode_remove_bits(q + 1);
      
      uint r = decode_truncated_binary(m);
      
      return (q * m) + r;
   }
   
   uint symbol_codec::decode_rice(uint m)
   {
      LZHAM_ASSERT(m > 0);
      
      uint q = 0;

      for ( ; ; )
      {
         uint k = decode_peek_bits(16);

         uint l = utils::count_leading_zeros16((~k) & 0xFFFF);
         
         q += l;
         
         decode_remove_bits(l);
         
         if (l < 16)
            break;
      }

      decode_remove_bits(1);

      uint r = decode_bits(m);

      return (q << m) + r;
   }
   
   uint64 symbol_codec::stop_decoding()
   {
      LZHAM_ASSERT(m_mode == cDecoding);
          
      uint64 n = m_pDecode_buf_next - m_pDecode_buf;
      
      m_mode = cNull;
      
      return n;
   }
   
   void symbol_codec::get_bits_init()
   {
      m_bit_buf = 0;
      m_bit_count = 0;
   }
   
   uint symbol_codec::get_bits(uint num_bits)
   {
      LZHAM_ASSERT(num_bits <= 25);
      
      if (!num_bits)
         return 0;

      while (m_bit_count < (int)num_bits)
      {
         uint c = 0;
         if (m_pDecode_buf_next == m_pDecode_buf_end)
         {
            if (!m_decode_buf_eof)
            {
               m_pDecode_need_bytes_func(m_pDecode_buf_next - m_pDecode_buf, m_pDecode_private_data, m_pDecode_buf, m_decode_buf_size, m_decode_buf_eof);
               m_pDecode_buf_end = m_pDecode_buf + m_decode_buf_size;
               m_pDecode_buf_next = m_pDecode_buf;
               if (m_pDecode_buf_next < m_pDecode_buf_end) c = *m_pDecode_buf_next++;
            }
         }
         else
            c = *m_pDecode_buf_next++;
         
         m_bit_count += 8;
         LZHAM_ASSERT(m_bit_count <= cBitBufSize);
         
         m_bit_buf |= (static_cast<bit_buf_t>(c) << (cBitBufSize - m_bit_count));
      }
      
      uint result = static_cast<uint>(m_bit_buf >> (cBitBufSize - num_bits));
      
      m_bit_buf <<= num_bits;
      m_bit_count -= num_bits;

      return result;	
   }
   
   void symbol_codec::remove_bits(uint num_bits)
   {
      LZHAM_ASSERT(num_bits <= 25);

      if (!num_bits)
         return;

      while (m_bit_count < (int)num_bits)
      {
         uint c = 0;
         if (m_pDecode_buf_next == m_pDecode_buf_end)
         {
            if (!m_decode_buf_eof)
            {
               m_pDecode_need_bytes_func(m_pDecode_buf_next - m_pDecode_buf, m_pDecode_private_data, m_pDecode_buf, m_decode_buf_size, m_decode_buf_eof);
               m_pDecode_buf_end = m_pDecode_buf + m_decode_buf_size;
               m_pDecode_buf_next = m_pDecode_buf;
               if (m_pDecode_buf_next < m_pDecode_buf_end) c = *m_pDecode_buf_next++;
            }
         }
         else
            c = *m_pDecode_buf_next++;

         m_bit_count += 8;
         LZHAM_ASSERT(m_bit_count <= cBitBufSize);

         m_bit_buf |= (static_cast<bit_buf_t>(c) << (cBitBufSize - m_bit_count));
      }
      
      m_bit_buf <<= num_bits;
      m_bit_count -= num_bits;
   }
   
   void symbol_codec::decode_align_to_byte()
   {  
      LZHAM_ASSERT(m_mode == cDecoding);
      
      if (m_bit_count & 7)
      {
         remove_bits(m_bit_count & 7);
      }
   }
   
   int symbol_codec::decode_remove_byte_from_bit_buf()
   {
      if (m_bit_count < 8)
         return -1;
      int result = static_cast<int>(m_bit_buf >> (cBitBufSize - 8));
      m_bit_buf <<= 8;
      m_bit_count -= 8;
      return result;
   }
         
   uint symbol_codec::decode(adaptive_bit_model& model, bool update_model)
   {
      if (m_arith_length < cSymbolCodecArithMinLen) 
      {
         uint c = get_bits(8);
         m_arith_value = (m_arith_value << 8) | c;
         
         m_arith_length <<= 8;
         LZHAM_ASSERT(m_arith_length >= cSymbolCodecArithMinLen);
      }
      
      LZHAM_ASSERT(m_arith_length >= cSymbolCodecArithMinLen);
      
      //uint x = gArithProbMulTab[model.m_bit_0_prob >> (cSymbolCodecArithProbBits - cSymbolCodecArithProbMulBits)][m_arith_length >> (32 - cSymbolCodecArithProbMulLenSigBits)] << 16;
      uint x = model.m_bit_0_prob * (m_arith_length >> cSymbolCodecArithProbBits);
      uint bit = (m_arith_value >= x);
      
      if (!bit) 
      {
         if (update_model)
            model.m_bit_0_prob += ((cSymbolCodecArithProbScale - model.m_bit_0_prob) >> cSymbolCodecArithProbMoveBits);
         
         m_arith_length = x;
      }
      else 
      {
         if (update_model)
            model.m_bit_0_prob -= (model.m_bit_0_prob >> cSymbolCodecArithProbMoveBits);
         
         m_arith_value  -= x;
         m_arith_length -= x;
      }
      
      return bit;                
   }
   
   uint symbol_codec::decode(adaptive_arith_data_model& model)
   {
      uint node = 1;

      do 
      {
         uint bit = decode(model.m_probs[node]);
         
         node = (node << 1) + bit;

      } while (node < model.m_total_syms);

      return node - model.m_total_syms;
   }
   
   void symbol_codec::start_arith_decoding()
   {
      LZHAM_ASSERT(m_mode == cDecoding);
      
      m_arith_length = cSymbolCodecArithMaxLen;
      m_arith_value = 0;
      
      if (get_bits(1))
      {
         m_arith_value = (get_bits(8) << 24);
         m_arith_value |= (get_bits(8) << 16);
         m_arith_value |= (get_bits(8) << 8);
         m_arith_value |= get_bits(8);
      }
   }
   
   void symbol_codec::decode_need_bytes()
   {
      if (!m_decode_buf_eof)
      {
         m_pDecode_need_bytes_func(m_pDecode_buf_next - m_pDecode_buf, m_pDecode_private_data, m_pDecode_buf, m_decode_buf_size, m_decode_buf_eof);
         m_pDecode_buf_end = m_pDecode_buf + m_decode_buf_size;
         m_pDecode_buf_next = m_pDecode_buf;
      }
   }
                     
} // namespace lzham
