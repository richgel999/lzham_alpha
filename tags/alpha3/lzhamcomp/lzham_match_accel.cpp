// File: lzham_match_accel.cpp
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
#include "lzham_match_accel.h"

namespace lzham
{
   const uint cMaxSupportedProbes = 128;

   search_accelerator::search_accelerator() :
      m_pLZBase(NULL),
      m_pTask_pool(NULL),
      m_max_helper_threads(0),
      m_max_dict_size(0),
      m_max_dict_size_mask(0),
      m_lookahead_pos(0),
      m_lookahead_size(0),
      m_cur_dict_size(0),
      m_next_match_ref(0),
      m_fill_lookahead_pos(0),
      m_fill_lookahead_size(0),
      m_fill_dict_size(0),
      m_max_probes(0),
      m_max_matches(0),
      m_all_matches(false)
   {
   }

   bool search_accelerator::init(CLZBase* pLZBase, task_pool* pPool, uint max_helper_threads, uint max_dict_size, uint max_matches, bool all_matches, uint max_probes)
   {
      LZHAM_ASSERT(pLZBase);
      LZHAM_ASSERT(max_dict_size && math::is_power_of_2(max_dict_size));
      LZHAM_ASSERT(max_probes);

      m_max_probes = LZHAM_MIN(cMaxSupportedProbes, max_probes);

      m_pLZBase = pLZBase;
      m_pTask_pool = max_helper_threads ? pPool : NULL;
      m_max_helper_threads = m_pTask_pool ? max_helper_threads : 0;
      m_max_matches = LZHAM_MIN(m_max_probes, max_matches);
      m_all_matches = all_matches;

      m_max_dict_size = max_dict_size;
      m_max_dict_size_mask = m_max_dict_size - 1;
      m_cur_dict_size = 0;
      m_lookahead_size = 0;
      m_lookahead_pos = 0;
      m_fill_lookahead_pos = 0;
      m_fill_lookahead_size = 0;
      m_fill_dict_size = 0;

      if (!m_dict.try_resize_no_construct(max_dict_size + CLZBase::cMaxMatchLen))
         return false;

      if (!m_hash.try_resize_no_construct(cHashSize))
         return false;
            
      if (!m_nodes.try_resize_no_construct(max_dict_size))
         return false;

      memset(m_hash.get_ptr(), 0, m_hash.size_in_bytes());

      return true;
   }

   uint search_accelerator::get_max_add_bytes() const
   {
      uint add_pos = static_cast<uint>(m_lookahead_pos & (m_max_dict_size - 1));
      return m_max_dict_size - add_pos;
   }

   inline bool search_accelerator::is_better_match(uint bestMatchDist, uint compMatchDist) const
   {
      uint bestMatchSlot, bestMatchSlotOfs;
      m_pLZBase->compute_lzx_position_slot(bestMatchDist, bestMatchSlot, bestMatchSlotOfs);

      uint compMatchSlot, compMatchOfs;
      m_pLZBase->compute_lzx_position_slot(compMatchDist, compMatchSlot, compMatchOfs);

      // If both matches uses the same match slot, choose the one with the offset containing the lowest nibble as these bits separately entropy coded.
      // This could choose a match which is further away in the absolute sense, but closer in a coding sense.
      if ( (compMatchSlot < bestMatchSlot) ||
         ((compMatchSlot >= 8) && (compMatchSlot == bestMatchSlot) && ((compMatchOfs & 15) < (bestMatchSlotOfs & 15))) )
      {
         return true;
      }

      return false;
   }

   void search_accelerator::find_all_matches_callback(uint64 data, void* pData_ptr)
   {
      pData_ptr;
      const uint thread_index = (uint)data;

      dict_match temp_matches[cMaxSupportedProbes * 2];

      uint fill_lookahead_pos = m_fill_lookahead_pos;
      uint fill_dict_size = m_fill_dict_size;
      uint fill_lookahead_size = m_fill_lookahead_size;

      while (fill_lookahead_size)
      {
         const uint max_match_len = LZHAM_MIN(CLZBase::cMaxMatchLen, fill_lookahead_size);
         uint insert_pos = fill_lookahead_pos & m_max_dict_size_mask;
         if (max_match_len >= 2)
         {
            uint c0 = m_dict[insert_pos];
            uint c1 = m_dict[insert_pos + 1];

            uint h = (c1 << 8) | c0;

            if (m_pTask_pool)
            {
               LZHAM_ASSERT(m_hash_thread_index[h] != UINT8_MAX);
            }

            // Only process those strings that this worker thread was assigned to - this allows us to manipulate multiple trees in parallel with no worries about synchronization.
            if ((!m_pTask_pool) || (m_hash_thread_index[h] == thread_index))
            {
               dict_match* pDstMatch = temp_matches;

               uint cur_pos = m_hash[h];
               m_hash[h] = static_cast<uint>(fill_lookahead_pos);

               uint *pLeft = &m_nodes[insert_pos].m_left;
               uint *pRight = &m_nodes[insert_pos].m_right;

               uint best_match_len = 1;

               const uint8* pIns = &m_dict[insert_pos];

               uint n = m_max_probes;
               for ( ; ; )
               {
                  uint delta_pos = fill_lookahead_pos - cur_pos;
                  if ((n-- == 0) || (!delta_pos) || (delta_pos >= fill_dict_size))
                  {
                     *pLeft = 0;
                     *pRight = 0;
                     break;
                  }

                  uint pos = cur_pos & m_max_dict_size_mask;
                  node *pNode = &m_nodes[pos];

                  // Unfortunately, the initial compare match_len must be 2 because of the way we truncate matches at the end of each block.
                  uint match_len = 2;
                  const uint8* pComp = &m_dict[pos];

#if 0
                  for ( ; match_len < max_match_len; match_len++)
                     if (pComp[match_len] != pIns[match_len])
                        break;
#else
                  // Compare a qword at a time for a bit more efficiency.
                  const uint64* pComp_end = reinterpret_cast<const uint64*>(pComp + max_match_len - 7);
                  const uint64* pComp_cur = reinterpret_cast<const uint64*>(pComp + 2);
                  const uint64* pIns_cur = reinterpret_cast<const uint64*>(pIns + 2);
                  while (pComp_cur < pComp_end)
                  {
                     if (*pComp_cur != *pIns_cur) 
                        break;
                     pComp_cur++;
                     pIns_cur++;                  
                  }
                  uint alt_match_len = static_cast<uint>(reinterpret_cast<const uint8*>(pComp_cur) - reinterpret_cast<const uint8*>(pComp));
                  for ( ; alt_match_len < max_match_len; alt_match_len++)
                     if (pComp[alt_match_len] != pIns[alt_match_len])
                        break;
#ifdef LZVERIFY
                  for ( ; match_len < max_match_len; match_len++)
                     if (pComp[match_len] != pIns[match_len])
                        break;
                  LZHAM_VERIFY(alt_match_len == match_len);
#endif
                  match_len = alt_match_len;                  
#endif                        

                  if (match_len > best_match_len)
                  {
                     pDstMatch->m_len = static_cast<uint8>(match_len - 2);
                     pDstMatch->m_dist = delta_pos;
                     pDstMatch++;

                     best_match_len = match_len;

                     if (match_len == max_match_len)
                     {
                        *pLeft = pNode->m_left;
                        *pRight = pNode->m_right;
                        break;
                     }
                  }
                  else if (m_all_matches)
                  {
                     pDstMatch->m_len = static_cast<uint8>(match_len - 2);
                     pDstMatch->m_dist = delta_pos;
                     pDstMatch++;
                  }
                  else if ((best_match_len > 1) && (best_match_len == match_len))
                  {
                     if (is_better_match(pDstMatch[-1].m_dist, delta_pos))
                     {
                        LZHAM_ASSERT((pDstMatch[-1].m_len + 2U) == best_match_len);
                        pDstMatch[-1].m_dist = delta_pos;
                     }
                  }

                  if (pComp[match_len] < pIns[match_len])
                  {
                     *pLeft = cur_pos;
                     pLeft = &pNode->m_right;
                     cur_pos = pNode->m_right;
                  }
                  else
                  {
                     *pRight = cur_pos;
                     pRight = &pNode->m_left;
                     cur_pos = pNode->m_left;
                  }
               }

               const uint num_matches = (uint)(pDstMatch - temp_matches);

               if (num_matches)
               {
                  pDstMatch[-1].m_dist |= 0x80000000;

                  const uint num_matches_to_write = LZHAM_MIN(num_matches, m_max_matches);

                  const uint match_ref_ofs = InterlockedExchangeAdd(&m_next_match_ref, num_matches_to_write);

                  memcpy(&m_matches[match_ref_ofs],
                         temp_matches + (num_matches - num_matches_to_write),
                         sizeof(temp_matches[0]) * num_matches_to_write);

                  // FIXME: This is going to really hurt on platforms requiring export barriers.
                  LZHAM_MEMORY_EXPORT_BARRIER
                  
                  InterlockedExchange((LONG*)&m_match_refs[static_cast<uint>(fill_lookahead_pos - m_fill_lookahead_pos)], match_ref_ofs);
               }
               else
               {
                  InterlockedExchange((LONG*)&m_match_refs[static_cast<uint>(fill_lookahead_pos - m_fill_lookahead_pos)], -2);
               }
            }
         }
         else
         {
            m_nodes[insert_pos].m_left = 0;
            m_nodes[insert_pos].m_right = 0;

            InterlockedExchange((LONG*)&m_match_refs[static_cast<uint>(fill_lookahead_pos - m_fill_lookahead_pos)], -2);
         }

         fill_lookahead_pos++;
         fill_lookahead_size--;
         fill_dict_size++;
      }
   }

   bool search_accelerator::find_all_matches(uint num_bytes)
   {
      if (!m_matches.try_resize_no_construct(m_max_probes * num_bytes))
         return false;

      if (!m_match_refs.try_resize_no_construct(num_bytes))
         return false;
      
      memset(m_match_refs.get_ptr(), 0xFF, m_match_refs.size_in_bytes());

      m_fill_lookahead_pos = m_lookahead_pos;
      m_fill_lookahead_size = num_bytes;
      m_fill_dict_size = m_cur_dict_size;

      m_next_match_ref = 0;

      if (!m_pTask_pool)
      {
         find_all_matches_callback(0, NULL);
      }
      else
      {
         if (!m_hash_thread_index.try_resize_no_construct(0x10000))
            return false;
         
         memset(m_hash_thread_index.get_ptr(), 0xFF, m_hash_thread_index.size_in_bytes());

         uint next_thread_index = 0;
         const uint8* pDict = &m_dict[m_lookahead_pos & m_max_dict_size_mask];
         uint num_unique_digrams = 0;
         for (uint i = 0; i < (num_bytes - 1); i++)
         {
            const uint h = pDict[0] | (pDict[1] << 8);
            pDict++;

            if (m_hash_thread_index[h] == UINT8_MAX)
            {
               num_unique_digrams++;

               m_hash_thread_index[h] = static_cast<uint8>(next_thread_index);
               if (++next_thread_index == m_max_helper_threads)
                  next_thread_index = 0;
            }
         }

         for (uint i = 0; i < m_max_helper_threads; i++)
         {
            m_pTask_pool->queue_object_task(this, &search_accelerator::find_all_matches_callback, i);
         }
      }

      return true;
   }

   void search_accelerator::add_bytes_begin(uint num_bytes, const uint8* pBytes)
   {
      LZHAM_ASSERT(num_bytes <= m_max_dict_size);
      LZHAM_ASSERT(!m_lookahead_size);

      uint add_pos = m_lookahead_pos & m_max_dict_size_mask;
      LZHAM_ASSERT((add_pos + num_bytes) <= m_max_dict_size);

      memcpy(&m_dict[add_pos], pBytes, num_bytes);

      if (add_pos < CLZBase::cMaxMatchLen)
         memcpy(&m_dict[m_max_dict_size], &m_dict[0], CLZBase::cMaxMatchLen);

      m_lookahead_size += num_bytes;

      uint max_possible_dict_size = m_max_dict_size - num_bytes;
      m_cur_dict_size = LZHAM_MIN(m_cur_dict_size, max_possible_dict_size);

      m_next_match_ref = 0;

      find_all_matches(num_bytes);
   }

   void search_accelerator::add_bytes_end()
   {
      if (m_pTask_pool)
      {
         m_pTask_pool->join();
      }

      LZHAM_ASSERT((uint)m_next_match_ref <= m_matches.size());
   }

   dict_match* search_accelerator::find_matches(uint lookahead_ofs, bool spin)
   {
      LZHAM_ASSERT(lookahead_ofs < m_lookahead_size);

      const uint match_ref_ofs = static_cast<uint>(m_lookahead_pos - m_fill_lookahead_pos + lookahead_ofs);

      int match_ref;
      uint spin_count = 0;
      for ( ; ; )
      {
         match_ref = m_match_refs[match_ref_ofs];
         if (match_ref == -2)
            return NULL;
         else if (match_ref != -1)
            break;

         spin_count++;
         const uint cMaxSpinCount = 1000;
         if ((spin) && (spin_count < cMaxSpinCount))
         {
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            lzham_yield_processor();
            
            LZHAM_MEMORY_IMPORT_BARRIER
         }
         else
         {
            spin_count = cMaxSpinCount;
            
            Sleep(1);
         }
      }
      
      LZHAM_MEMORY_IMPORT_BARRIER

      return &m_matches[match_ref];
   }

   void search_accelerator::advance_bytes(uint num_bytes)
   {
      LZHAM_ASSERT(num_bytes <= m_lookahead_size);

      m_lookahead_pos += num_bytes;
      m_lookahead_size -= num_bytes;

      m_cur_dict_size += num_bytes;
      LZHAM_ASSERT(m_cur_dict_size <= m_max_dict_size);
   }

   uint search_accelerator::match(uint lookahead_ofs, int dist) const
   {
      LZHAM_ASSERT(lookahead_ofs < m_lookahead_size);

      const int find_dict_size = m_cur_dict_size + lookahead_ofs;
      if (dist > find_dict_size)
         return 0;

      const uint comp_pos = static_cast<uint>((m_lookahead_pos + lookahead_ofs - dist) & m_max_dict_size_mask);
      const uint lookahead_pos = static_cast<uint>((m_lookahead_pos + lookahead_ofs) & m_max_dict_size_mask);

      const uint max_match_len = LZHAM_MIN(CLZBase::cMaxMatchLen, m_lookahead_size - lookahead_ofs);

      uint match_len;
      for (match_len = 0; match_len < max_match_len; match_len++)
         if (m_dict[comp_pos + match_len] != m_dict[lookahead_pos + match_len])
            break;

      return match_len;
   }
}
