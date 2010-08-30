// File: lzham_task_pool.cpp
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
#include "lzham_task_pool.h"
#include <process.h>

namespace lzham
{
   task_pool::task_pool() :
      m_num_threads(0),
      m_exit_flag(false),
      m_num_outstanding_tasks(0)
   {
      utils::zero_object(m_threads);
   }
   
   task_pool::task_pool(uint num_threads) :
      m_num_threads(0),
      m_exit_flag(false),
      m_num_outstanding_tasks(0)
   {
      utils::zero_object(m_threads);
      bool status = init(num_threads);
      LZHAM_VERIFY(status);
   }
   
   task_pool::~task_pool()
   {
      deinit();
   }

   bool task_pool::init(uint num_threads)
   {
      LZHAM_ASSERT(num_threads <= cMaxThreads);
      num_threads = math::minimum<uint>(num_threads, cMaxThreads);
      
      deinit();
                  
      m_task_waitable_lock.lock();
      
      m_num_threads = num_threads;
            
      bool succeeded = true;
      for (uint i = 0; i < num_threads; i++)
      {
         m_threads[i] = (HANDLE)_beginthreadex(NULL, 32768, thread_func, this, 0, NULL);
         
         LZHAM_ASSERT(m_threads[i] != 0);
         if (!m_threads[i])
         {
            succeeded = false;
            break;
         }
      }
                  
      m_task_waitable_lock.unlock();
      
      if (!succeeded)
      {
         deinit();
         return false;
      }
      
      return true;
   }
   
   void task_pool::deinit()
   {
      if (m_num_threads)
      {
         m_task_waitable_lock.lock();
         
         m_exit_flag = true;
         
         m_task_waitable_lock.unlock();
         
         for (uint i = 0; i < m_num_threads; i++)
         {
            if (m_threads[i])
            {
               for ( ; ; )
               {
                  DWORD result = WaitForSingleObject(m_threads[i], 10000);
                  if (result == WAIT_OBJECT_0)
                     break;
               }
               
               CloseHandle(m_threads[i]);
               
               m_threads[i] = NULL;
            }
         }
         
         m_num_threads = 0;
         
         m_exit_flag = false;
      }
      
      m_tasks.clear();
      m_num_outstanding_tasks = 0;
   }

   uint task_pool::get_num_threads() const
   {
      return m_num_threads;
   }
   
   void task_pool::queue_task(task_callback_func pFunc, uint64 data, void* pData_ptr)
   {
      LZHAM_ASSERT(pFunc);
      
      m_task_waitable_lock.lock();
      
      task tsk;
      tsk.m_callback = pFunc;
      tsk.m_data = data;
      tsk.m_pData_ptr = pData_ptr;
      tsk.m_flags = 0;
      m_tasks.push_back(tsk);
      
      m_num_outstanding_tasks++;
      
      m_task_waitable_lock.unlock();
   }
   
   // It's the object's responsibility to delete pObj within the execute_task() method, if needed!
   void task_pool::queue_task(executable_task* pObj, uint64 data, void* pData_ptr)
   {
      LZHAM_ASSERT(pObj);

      m_task_waitable_lock.lock();

      task tsk;
      tsk.m_pObj = pObj;
      tsk.m_data = data;
      tsk.m_pData_ptr = pData_ptr;
      tsk.m_flags = cTaskFlagObject;
      m_tasks.push_back(tsk);

      m_num_outstanding_tasks++;

      m_task_waitable_lock.unlock();
   }
   
   BOOL task_pool::join_condition_func(void* pCallback_data_ptr, uint64 callback_data)
   {
      callback_data;
      
      task_pool* pPool = static_cast<task_pool*>(pCallback_data_ptr);
      
      return (!pPool->m_num_outstanding_tasks) || pPool->m_exit_flag;
   }
   
   void task_pool::process_task(task& tsk)
   {
      if (tsk.m_flags & cTaskFlagObject)
         tsk.m_pObj->execute_task(tsk.m_data, tsk.m_pData_ptr);
      else
         tsk.m_callback(tsk.m_data, tsk.m_pData_ptr);

      m_task_waitable_lock.lock();

      m_num_outstanding_tasks--;

      m_task_waitable_lock.unlock();
   }
   
   void task_pool::join()
   {
      for ( ; ; )
      {
         m_task_waitable_lock.lock();
         
         if (!m_tasks.empty())
         {
            task tsk(m_tasks.front());
            m_tasks.pop_front();

            m_task_waitable_lock.unlock();
            
            process_task(tsk);
         }
         else
         {
            int result = m_task_waitable_lock.wait(join_condition_func, this);
            result;
            LZHAM_ASSERT(result >= 0);
         
            m_task_waitable_lock.unlock();
            
            break;
         }
      }         
   }
   
   BOOL task_pool::wait_condition_func(void* pCallback_data_ptr, uint64 callback_data)
   {
      callback_data;

      task_pool* pPool = static_cast<task_pool*>(pCallback_data_ptr);

      // Wait condition is satisfied if there are tasks or if the task pool is being destroyed.
      return (!pPool->m_tasks.empty()) || pPool->m_exit_flag;
   }
         
   unsigned __stdcall task_pool::thread_func(void* pContext)
   {
      task_pool* pPool = static_cast<task_pool*>(pContext);
                  
      for ( ; ; )
      {
         pPool->m_task_waitable_lock.lock();
         
         int result = pPool->m_task_waitable_lock.wait(wait_condition_func, pPool);
         
         LZHAM_ASSERT(result >= 0);
         
         if ((result < 0) || (pPool->m_exit_flag))
         {
            pPool->m_task_waitable_lock.unlock();
            break;
         }
         
         if (pPool->m_tasks.empty())
            pPool->m_task_waitable_lock.unlock();
         else
         {
            task tsk(pPool->m_tasks.front());
            pPool->m_tasks.pop_front();
            
            pPool->m_task_waitable_lock.unlock();
                     
            pPool->process_task(tsk);
         }            
      }
      
      _endthreadex(0);
      return 0;
   }

} // namespace lzham
