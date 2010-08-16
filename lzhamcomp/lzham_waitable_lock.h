// File: lzham_waitable_lock.h
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
//
// Inspired by the "monitor" class in "Win32 Multithreaded Programming" by Cohen and Woodring.
// This class utilizes the Win32 critical section and semaphore objects.
// Also see: http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
#pragma once

#include "lzham_mutex.h"
#include "lzham_semaphore.h"

namespace lzham
{
   class waitable_lock
   {
      LZHAM_NO_COPY_OR_ASSIGNMENT_OP(waitable_lock);
            
   public:
      waitable_lock(uint spin_count = 512U);
      ~waitable_lock();

      // Locks the waitable_lock. 
      // Recursive locking is NOT supported.
      void lock();
      
      // Returns TRUE if the thread owning this condition function should stop waiting. 
      // This function will always be called from within the waitable_lock, but it may be called from several different threads!
      typedef BOOL (*pCondition_func)(void* pCallback_data_ptr, uint64 callback_data);
            
      // Temporarily leaves the lock and waits for a condition to be satisfied.
      // The calling thread will efficiently sleep (via a call to WaitForMultipleObjects()) while waiting for the condition.
      // When this method returns, the calling thread is guaranteed to have ownership of the lock.
      // Returns -1 on timeout or error, 0 if the wait was satisfied, or 1 or higher if one of the extra wait handles became signaled.
      int wait(
         pCondition_func pCallback = NULL, void* pCallback_data_ptr = NULL, uint64 callback_data = 0, 
         uint num_wait_handles = 0, const HANDLE* pWait_handles = NULL, DWORD max_time_to_wait = INFINITE);
      
      // Releases this thread's ownership of the waitable_lock. Another thread may be woken up and receive lock ownership 
      // if its condition function is now satisfied.
      void unlock();
            
   private:
      enum 
      { 
         // Must be a power of two
         cMaxWaitingThreads = 32, 
         cMaxWaitingThreadsMask = cMaxWaitingThreads - 1 
      };

      struct waiting_thread
      {
         uint64            m_callback_data;
         void*             m_pCallback_ptr;
         pCondition_func   m_callback_func;
         uint              m_age;
         bool              m_satisfied;
         bool              m_occupied;

         semaphore         m_ready;
         
         void clear()
         {
            m_callback_data = 0;
            m_pCallback_ptr = NULL;
            m_callback_func = NULL;
            m_age = 0;
            m_satisfied = false;
            m_occupied = false;
         }
      };

      semaphore      m_waitable_lock_lock;
      mutex          m_waiters_array_lock;
      
      uint           m_cur_age;
      waiting_thread m_waiters[cMaxWaitingThreads];
      int            m_max_waiter_array_index;
            
      void find_ready_waiter_or_unlock(int index_to_ignore, bool already_inside_waiter_array_lock);
   };
   
   class scoped_waitable_lock
   {
      scoped_waitable_lock(const scoped_waitable_lock&);
      scoped_waitable_lock& operator= (const scoped_waitable_lock&);

      waitable_lock& m_waitable_lock;

   public:
      inline scoped_waitable_lock(waitable_lock& m) : m_waitable_lock(m) 
      { 
         m_waitable_lock.lock();
      }

      inline ~scoped_waitable_lock()
      {
         m_waitable_lock.unlock();
      }
   };

} // namespace lzham
