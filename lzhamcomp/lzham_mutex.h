// File: lzham_mutex.h
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
   class mutex
   {
      mutex(const mutex&);
      mutex& operator= (const mutex&);
      
   public:
      inline mutex(unsigned int spin_count = 0)
      {
         BOOL status = true;
#ifdef LZHAM_PLATFORM_X360
         InitializeCriticalSectionAndSpinCount(&m_cs, spin_count); 
#else
         status = InitializeCriticalSectionAndSpinCount(&m_cs, spin_count); 
#endif   
         if (!status)
            lzham_fail("mutex::mutex: InitializeCriticalSectionAndSpinCount failed", __FILE__, __LINE__);       

#ifdef LZHAM_BUILD_DEBUG
         m_lock_count = 0;
#endif         
      }
      
      inline ~mutex()
      {
#ifdef LZHAM_BUILD_DEBUG         
         if (m_lock_count)
            lzham_assert("mutex::~mutex: mutex is still locked", __FILE__, __LINE__);
#endif         
         DeleteCriticalSection(&m_cs);
      }
      
      inline void lock()
      {
         EnterCriticalSection(&m_cs);
#ifdef LZHAM_BUILD_DEBUG         
         m_lock_count++;
#endif         
      }
      
      inline void unlock()
      {
#ifdef LZHAM_BUILD_DEBUG
         if (!m_lock_count)
            lzham_assert("mutex::unlock: mutex is not locked", __FILE__, __LINE__);
         m_lock_count--;
#endif      
         LeaveCriticalSection(&m_cs);
      }
      
      inline void set_spin_count(unsigned int count) 
      { 
         SetCriticalSectionSpinCount(&m_cs, count);
      }
   
   private:
      CRITICAL_SECTION m_cs;

#ifdef LZHAM_BUILD_DEBUG
      unsigned int m_lock_count;
#endif      
   };
   
} // namespace lzham   
