//File: lzham_semaphore.h
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
   class semaphore
   {
      LZHAM_NO_COPY_OR_ASSIGNMENT_OP(semaphore);
      
   public:
      semaphore(LONG initialCount = 0, LONG maximumCount = 1, const char* pName = NULL)
      {
         m_handle = CreateSemaphoreA(NULL, initialCount, maximumCount, pName);
         if (NULL == m_handle)
         {
            LZHAM_FAIL("semaphore: CreateSemaphore() failed");
         }
      }

      ~semaphore()
      {
         if (m_handle)
         {
            CloseHandle(m_handle);
            m_handle = NULL;
         }
      }

      inline HANDLE get_handle(void) const { return m_handle; }   

      void release(LONG releaseCount = 1, LPLONG pPreviousCount = NULL)
      {
         if (0 == ReleaseSemaphore(m_handle, releaseCount, pPreviousCount))
         {
            LZHAM_FAIL("semaphore: ReleaseSemaphore() failed");
         }
      }

      bool wait(DWORD milliseconds = INFINITE) 
      {
         DWORD result = WaitForSingleObject(m_handle, milliseconds);

         if (WAIT_FAILED == result)
         {
            LZHAM_FAIL("semaphore: WaitForSingleObject() failed");
         }

         return WAIT_OBJECT_0 == result;
      }      

   private:   
      HANDLE m_handle;
   };

} // namespace lzham
