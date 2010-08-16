// File: lzham_waitable_lock.cpp
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
#include "lzham_waitable_lock.h"

namespace lzham
{
   waitable_lock::waitable_lock(uint spin_count) : 
      m_cur_age(0), 
      m_max_waiter_array_index(-1),
      m_waitable_lock_lock(1, 1)
   {
      m_waiters_array_lock.set_spin_count(spin_count);
      
      for (uint i = 0; i < cMaxWaitingThreads; i++)
         m_waiters[i].clear();
   }

   waitable_lock::~waitable_lock()
   {
   }

   void waitable_lock::lock() 
   { 
      m_waitable_lock_lock.wait();
   }

   void waitable_lock::unlock() 
   { 
      find_ready_waiter_or_unlock(-1, false); 
   }

   int waitable_lock::wait(
     pCondition_func pCallback, void* pCallback_data_ptr, uint64 callback_data, 
     uint num_wait_handles, const HANDLE* pWait_handles, DWORD max_time_to_wait)
   {
      LZHAM_ASSERT(pCallback);

      // First, see if the calling thread's condition function is satisfied. If so, there's no need to wait.
      if (pCallback(pCallback_data_ptr, callback_data))
         return 0;

      // Add this thread to the list of waiters.
      m_waiters_array_lock.lock();

      uint i;
      for (i = 0; i < cMaxWaitingThreads; i++)
         if (!m_waiters[i].m_occupied)
            break;
      
      LZHAM_ASSERT(i != cMaxWaitingThreads);
      if (i == cMaxWaitingThreads)
      {
         m_waiters_array_lock.unlock();
         return -1;
      }

      m_max_waiter_array_index = math::maximum<int>(m_max_waiter_array_index, i);
      
      waiting_thread& waiter_entry = m_waiters[i];

      waiter_entry.m_callback_func     = pCallback;
      waiter_entry.m_pCallback_ptr     = pCallback_data_ptr;
      waiter_entry.m_callback_data     = callback_data;
      waiter_entry.m_satisfied         = false;
      waiter_entry.m_occupied          = true;
      waiter_entry.m_age               = m_cur_age++;
            
      // Now leave the lock and scan to see if there are any satisfied waiters.
      find_ready_waiter_or_unlock(i, true);

      // Let's wait for this thread's condition to be satisfied, or until timeout, or until one of the user supplied handles is signaled.
      int return_index = 0;

      const uint cMaxWaitHandles = 64;
      LZHAM_ASSERT(num_wait_handles < cMaxWaitHandles);

      HANDLE handles[cMaxWaitHandles];
      
      handles[0] = waiter_entry.m_ready.get_handle();
      uint total_handles = 1;      

      if (num_wait_handles)
      {
         LZHAM_ASSERT(pWait_handles);
         memcpy(handles + total_handles, pWait_handles, sizeof(HANDLE) * num_wait_handles);
         total_handles += num_wait_handles;
      }

      DWORD result = WaitForMultipleObjects(total_handles, handles, FALSE, max_time_to_wait);
         
      if ((result == WAIT_ABANDONED) || (result == WAIT_TIMEOUT))
      {
         return_index = -1;
      }
      else
      { 
         return_index = result - WAIT_OBJECT_0;
      }

      // See if our condition was satisfied, and remove this thread from the waiter list.
      m_waiters_array_lock.lock();

      const bool was_satisfied = waiter_entry.m_satisfied;                          

      waiter_entry.m_occupied = false;

      m_waiters_array_lock.unlock();

      if (0 == return_index)
      {
         LZHAM_ASSERT(was_satisfied);  
      }
      else
      {
         // This is tricky. Re-enter the waitable_lock if a user supplied handle was signaled, or if we've timed out.
         // This guarantees that on exit of this function we're still inside the lock, 
         // no matter what happened during or immediately after the WaitForMultipleObjects() call (we may have been 
         // given ownership of the lock shortly after a user-provided handle became signaled).
         // Note: This codepath is not well tested in the LZHAM lib as of 7/25/10.
         if (!was_satisfied)
         {
            m_waitable_lock_lock.wait();
         }
         else
         {
            // This wait should immediately return.
            // It should be safe to wait here because we're inside the lock.
            waiter_entry.m_ready.wait();
         }
      }

      return return_index;                
   }

   void waitable_lock::find_ready_waiter_or_unlock(int index_to_ignore, bool already_inside_waiter_array_lock)
   {
      if (!already_inside_waiter_array_lock)
      {
         m_waiters_array_lock.lock();
      }
      
      uint best_age = 0;
      int best_index = -1;
      for (int i = 0; i <= m_max_waiter_array_index; i++)
      {
         waiting_thread& waiter_entry = m_waiters[i];

         if ((i != index_to_ignore) && (waiter_entry.m_occupied) && (!waiter_entry.m_satisfied))
         {
            uint age = m_cur_age - waiter_entry.m_age;
            
            if ((age > best_age) || (best_index < 0))
            {
               if (waiter_entry.m_callback_func(waiter_entry.m_pCallback_ptr, waiter_entry.m_callback_data))
               {
                  best_age = age;
                  best_index = i;
               }
            }               
         }            
      }
      
      if (best_index >= 0)
      {
         // We've found a waiter who's condition is satisfied, so wake it up.
         waiting_thread& waiter_entry = m_waiters[best_index];
         
         waiter_entry.m_satisfied = true;
         
         waiter_entry.m_ready.release();
         
         m_waiters_array_lock.unlock();
      }
      else
      {
         m_waiters_array_lock.unlock();

         // Couldn't find any waiting threads to wake up, so leave the lock.
         m_waitable_lock_lock.release();
      }         
   }

} // namespace lzham
