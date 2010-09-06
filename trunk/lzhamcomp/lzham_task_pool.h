// File: task_pool.h
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
#include "lzham_semaphore.h"
#include "lzham_mutex.h"

namespace lzham
{
   inline long lzham_interlocked_add(LONG volatile *addend, LONG value)
   {
      return InterlockedExchangeAdd(addend, value) + value;
   }
      
   template<typename T>  
   class tsstack 
   {
   public:
      inline tsstack(bool use_freelist = true) : 
         m_use_freelist(use_freelist)
      {
         LZHAM_VERIFY(((ptr_bits_t)this & (LZHAM_GET_ALIGNMENT(tsstack) - 1)) == 0);
         InitializeSListHead(&m_stack_head);
         InitializeSListHead(&m_freelist_head);
      }

      inline ~tsstack()
      {
         clear();
      }

      inline void clear()
      {
         for ( ; ; )
         {
            node* pNode = (node*)InterlockedPopEntrySList(&m_stack_head);
            if (!pNode)
               break;

            LZHAM_MEMORY_IMPORT_BARRIER

            helpers::destruct(&pNode->m_obj);

            lzham_free(pNode);
         }

         flush_freelist();
      }

      inline void flush_freelist()
      {
         if (!m_use_freelist)
            return;

         for ( ; ; )
         {
            node* pNode = (node*)InterlockedPopEntrySList(&m_freelist_head);
            if (!pNode)
               break;

            LZHAM_MEMORY_IMPORT_BARRIER

            lzham_free(pNode);
         }
      }

      inline bool try_push(const T& obj)
      {
         node* pNode = alloc_node();
         if (!pNode)
            return false;

         helpers::construct(&pNode->m_obj, obj);
         
         LZHAM_MEMORY_EXPORT_BARRIER

         InterlockedPushEntrySList(&m_stack_head, &pNode->m_slist_entry);
         
         return true;
      }

      inline bool pop(T& obj)
      {
         node* pNode = (node*)InterlockedPopEntrySList(&m_stack_head);
         if (!pNode)
            return false;

         LZHAM_MEMORY_IMPORT_BARRIER

         obj = pNode->m_obj;

         helpers::destruct(&pNode->m_obj);

         free_node(pNode);

         return true;
      }

   private:
      SLIST_HEADER m_stack_head;
      SLIST_HEADER m_freelist_head;

      struct node
      {
         SLIST_ENTRY m_slist_entry;
         T m_obj;
      };

      bool m_use_freelist;

      inline node* alloc_node()
      {
         node* pNode = m_use_freelist ? (node*)InterlockedPopEntrySList(&m_freelist_head) : NULL;

         if (!pNode)
            pNode = (node*)lzham_malloc(sizeof(node));

         return pNode;
      }

      inline void free_node(node* pNode)
      {
         if (m_use_freelist)
            InterlockedPushEntrySList(&m_freelist_head, &pNode->m_slist_entry);
         else
            lzham_free(pNode);         
      }
   };

   class task_pool
   {
   public:
      task_pool();
      task_pool(uint num_threads);
      ~task_pool();
      
      enum { cMaxThreads = 16 };
      bool init(uint num_threads);
      void deinit();
      
      inline uint get_num_threads() const { return m_num_threads; }
      inline uint get_num_outstanding_tasks() const { return m_num_outstanding_tasks; }
      
      // C-style task callback
      typedef void (*task_callback_func)(uint64 data, void* pData_ptr);
      bool queue_task(task_callback_func pFunc, uint64 data = 0, void* pData_ptr = NULL);
                  
      class executable_task
      {
      public:
         virtual void execute_task(uint64 data, void* pData_ptr) = 0;
      };
      
      // It's the caller's responsibility to delete pObj within the execute_task() method, if needed!
      bool queue_task(executable_task* pObj, uint64 data = 0, void* pData_ptr = NULL);
            
      template<typename S, typename T>
      inline bool queue_object_task(S* pObject, T pObject_method, uint64 data = 0, void* pData_ptr = NULL);
      
      template<typename S, typename T>
      inline bool queue_multiple_object_tasks(S* pObject, T pObject_method, uint64 first_data, uint num_tasks, void* pData_ptr = NULL);
            
      void join();

   private:
      struct task
      {
         uint64 m_data;
         void* m_pData_ptr;

         union
         {
            task_callback_func m_callback;
            executable_task* m_pObj;
         };

         uint m_flags;
      };    
      
      tsstack<task> m_task_stack;
      
      uint m_num_threads;
      HANDLE m_threads[cMaxThreads];
                              
      semaphore m_tasks_available;
      
      enum task_flags
      {
         cTaskFlagObject = 1
      };
            
      volatile LONG m_num_outstanding_tasks;                  
      volatile LONG m_exit_flag;

      void process_task(task& tsk);
      
      static BOOL join_condition_func(void* pCallback_data_ptr, uint64 callback_data);
      static BOOL wait_condition_func(void* pCallback_data_ptr, uint64 callback_data);
      static unsigned __stdcall thread_func(void* pContext);
   };
   
   enum object_task_flags
   {
      cObjectTaskFlagDefault = 0,
      cObjectTaskFlagDeleteAfterExecution = 1
   };
   
   template<typename T>
   class object_task : public task_pool::executable_task
   {
   public:
      object_task(uint flags = cObjectTaskFlagDefault) :
         m_pObject(NULL),
         m_pMethod(NULL),
         m_flags(flags)
      {
      }
      
      typedef void (T::*object_method_ptr)(uint64 data, void* pData_ptr);
                  
      object_task(T* pObject, object_method_ptr pMethod, uint flags = cObjectTaskFlagDefault) :
         m_pObject(pObject),
         m_pMethod(pMethod),
         m_flags(flags)
      {
         LZHAM_ASSERT(pObject && pMethod);
      }
      
      void init(T* pObject, object_method_ptr pMethod, uint flags = cObjectTaskFlagDefault)
      {
         LZHAM_ASSERT(pObject && pMethod);
         
         m_pObject = pObject;
         m_pMethod = pMethod;
         m_flags = flags;
      }
      
      T* get_object() const { return m_pObject; }
      object_method_ptr get_method() const { return m_pMethod; }
            
      virtual void execute_task(uint64 data, void* pData_ptr)
      {
         (m_pObject->*m_pMethod)(data, pData_ptr);
         
         if (m_flags & cObjectTaskFlagDeleteAfterExecution)
            lzham_delete(this);
      }
   
   protected:
      T* m_pObject;        
      
      object_method_ptr m_pMethod;
      
      uint m_flags;
   };
   
   template<typename S, typename T>
   inline bool task_pool::queue_object_task(S* pObject, T pObject_method, uint64 data, void* pData_ptr)
   {
      object_task<S> *pTask = lzham_new< object_task<S> >(pObject, pObject_method, cObjectTaskFlagDeleteAfterExecution);
      if (!pTask)
         return false;
      return queue_task(pTask, data, pData_ptr);
   }
   
   template<typename S, typename T>
   inline bool task_pool::queue_multiple_object_tasks(S* pObject, T pObject_method, uint64 first_data, uint num_tasks, void* pData_ptr)
   {
      LZHAM_ASSERT(m_num_threads);
      LZHAM_ASSERT(pObject);
      LZHAM_ASSERT(num_tasks);
      if (!num_tasks)
         return true;
      
      bool status = true;   
      
      uint i;
      for (i = 0; i < num_tasks; i++)
      {
         task tsk;
         
         tsk.m_pObj = lzham_new< object_task<S> >(pObject, pObject_method, cObjectTaskFlagDeleteAfterExecution);
         if (!tsk.m_pObj)
         {
            status = false;
            break;
         }
         
         tsk.m_data = first_data + i;
         tsk.m_pData_ptr = pData_ptr;
         tsk.m_flags = cTaskFlagObject;
         
         if (!m_task_stack.try_push(tsk))
         {
            status = false;
            break;
         }
      }
           
      if (i)
      {
         lzham_interlocked_add(&m_num_outstanding_tasks, i);
         
         m_tasks_available.release(i);
      }
      
      return status;
   }
 
} // namespace lzham

