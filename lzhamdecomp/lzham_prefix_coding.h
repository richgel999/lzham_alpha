// File: lzham_prefix_coding.h
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
   namespace prefix_coding
   {
      const uint cMaxExpectedCodeSize = 16;
      const uint cMaxSupportedSyms = 8192;
      const uint cMaxTableBits = 11;

      bool limit_max_code_size(uint num_syms, uint8* pCodesizes, uint max_code_size);

      bool generate_codes(uint num_syms, const uint8* pCodesizes, uint16* pCodes);

      class decoder_tables
      {
      public:
         inline decoder_tables() :
            m_table_shift(0), m_table_max_code(0), m_decode_start_code_size(0), m_cur_lookup_size(0), m_lookup(NULL), m_cur_sorted_symbol_order_size(0), m_sorted_symbol_order(NULL)
         {
         }

         inline decoder_tables(const decoder_tables& other) :
            m_table_shift(0), m_table_max_code(0), m_decode_start_code_size(0), m_cur_lookup_size(0), m_lookup(NULL), m_cur_sorted_symbol_order_size(0), m_sorted_symbol_order(NULL)
         {
            *this = other;
         }

         decoder_tables& operator= (const decoder_tables& other)
         {
            if (this == &other)
               return *this;

            clear();

            memcpy(this, &other, sizeof(*this));

            if (other.m_lookup)
            {
               m_lookup = lzham_new_array<uint32>(m_cur_lookup_size);
               memcpy(m_lookup, other.m_lookup, sizeof(m_lookup[0]) * m_cur_lookup_size);
            }

            if (other.m_sorted_symbol_order)
            {
               m_sorted_symbol_order = lzham_new_array<uint16>(m_cur_sorted_symbol_order_size);
               memcpy(m_sorted_symbol_order, other.m_sorted_symbol_order, sizeof(m_sorted_symbol_order[0]) * m_cur_sorted_symbol_order_size);
            }

            return *this;
         }

         inline void clear()
         {
            if (m_lookup)
            {
               lzham_delete_array(m_lookup);
               m_lookup = 0;
               m_cur_lookup_size = 0;
            }

            if (m_sorted_symbol_order)
            {
               lzham_delete_array(m_sorted_symbol_order);
               m_sorted_symbol_order = NULL;
               m_cur_sorted_symbol_order_size = 0;
            }
         }

         inline ~decoder_tables()
         {
            if (m_lookup)
               lzham_delete_array(m_lookup);

            if (m_sorted_symbol_order)
               lzham_delete_array(m_sorted_symbol_order);
         }

         // DO NOT use any complex classes here - it is bitwise copied.

         uint                 m_num_syms;
         uint                 m_total_used_syms;
         uint                 m_table_bits;
         uint                 m_table_shift;
         uint                 m_table_max_code;
         uint                 m_decode_start_code_size;

         uint8                m_min_code_size;
         uint8                m_max_code_size;

         uint                 m_max_codes[cMaxExpectedCodeSize + 1];
         int                  m_val_ptrs[cMaxExpectedCodeSize + 1];

         uint                 m_cur_lookup_size;
         uint32*              m_lookup;

         uint                 m_cur_sorted_symbol_order_size;
         uint16*              m_sorted_symbol_order;

         inline uint get_unshifted_max_code(uint len) const
         {
            LZHAM_ASSERT( (len >= 1) && (len <= cMaxExpectedCodeSize) );
            uint k = m_max_codes[len - 1];
            if (!k)
               return UINT_MAX;
            return (k - 1) >> (16 - len);
         }
      };

      bool generate_decoder_tables(uint num_syms, const uint8* pCodesizes, decoder_tables* pTables, uint table_bits);

   } // namespace prefix_coding

} // namespace lzham
