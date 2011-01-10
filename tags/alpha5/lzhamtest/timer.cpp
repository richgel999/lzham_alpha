// File: timer.cpp
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "timer.h"

#if defined(WIN32)
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

unsigned long long timer::g_init_ticks;
unsigned long long timer::g_freq;
double timer::g_inv_freq;

#if defined(WIN32) || defined(_XBOX)
inline void query_counter(timer_ticks *pTicks)
{
   QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(pTicks));
}
inline void query_counter_frequency(timer_ticks *pTicks)
{
   QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(pTicks));
}
#elif defined(__GNUC__)
#include <sys/timex.h>
inline void query_counter(timer_ticks *pTicks)
{
   struct timeval cur_time;
   gettimeofday(&cur_time, NULL);
   *pTicks = static_cast<unsigned long long>(cur_time.tv_sec)*1000000ULL + static_cast<unsigned long long>(cur_time.tv_usec);
}
inline void query_counter_frequency(timer_ticks *pTicks)
{
   *pTicks = 1000000;
}
#endif

timer::timer() :
   m_start_time(0),
   m_stop_time(0),
   m_started(false),
   m_stopped(false)
{
   if (!g_inv_freq)
      init();
}

timer::timer(timer_ticks start_ticks)
{
   if (!g_inv_freq)
      init();

   m_start_time = start_ticks;

   m_started = true;
   m_stopped = false;
}

void timer::start(timer_ticks start_ticks)
{
   m_start_time = start_ticks;

   m_started = true;
   m_stopped = false;
}

void timer::start()
{
   query_counter(&m_start_time);

   m_started = true;
   m_stopped = false;
}

void timer::stop()
{
   assert(m_started);

   query_counter(&m_stop_time);

   m_stopped = true;
}

double timer::get_elapsed_secs() const
{
   assert(m_started);
   if (!m_started)
      return 0;

   timer_ticks stop_time = m_stop_time;
   if (!m_stopped)
      query_counter(&stop_time);

   timer_ticks delta = stop_time - m_start_time;
   return delta * g_inv_freq;
}

timer_ticks timer::get_elapsed_us() const
{
   assert(m_started);
   if (!m_started)
      return 0;

   timer_ticks stop_time = m_stop_time;
   if (!m_stopped)
      query_counter(&stop_time);

   timer_ticks delta = stop_time - m_start_time;
   return (delta * 1000000ULL + (g_freq >> 1U)) / g_freq;
}

void timer::init()
{
   if (!g_inv_freq)
   {
      query_counter_frequency(&g_freq);
      g_inv_freq = 1.0f / g_freq;

      query_counter(&g_init_ticks);
   }
}

timer_ticks timer::get_init_ticks()
{
   if (!g_inv_freq)
      init();

   return g_init_ticks;
}

timer_ticks timer::get_ticks()
{
   if (!g_inv_freq)
      init();

   timer_ticks ticks;
   query_counter(&ticks);
   return ticks - g_init_ticks;
}

double timer::ticks_to_secs(timer_ticks ticks)
{
   if (!g_inv_freq)
      init();

   return ticks * g_inv_freq;
}

