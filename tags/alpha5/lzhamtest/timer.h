// File: timer.h
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

typedef unsigned long long timer_ticks;

class timer
{
public:
   timer();
   timer(timer_ticks start_ticks);

   void start();
   void start(timer_ticks start_ticks);

   void stop();

   double get_elapsed_secs() const;
   inline double get_elapsed_ms() const { return get_elapsed_secs() * 1000.0f; }
   timer_ticks get_elapsed_us() const;

   static void init();
   static inline timer_ticks get_ticks_per_sec() { return g_freq; }
   static timer_ticks get_init_ticks();
   static timer_ticks get_ticks();
   static double ticks_to_secs(timer_ticks ticks);
   static inline double ticks_to_ms(timer_ticks ticks) { return ticks_to_secs(ticks) * 1000.0f; }
   static inline double get_secs() { return ticks_to_secs(get_ticks()); }
   static inline double get_ms() { return ticks_to_ms(get_ticks()); }

private:
   static timer_ticks g_init_ticks;
   static timer_ticks g_freq;
   static double g_inv_freq;

   timer_ticks m_start_time;
   timer_ticks m_stop_time;

   bool m_started : 1;
   bool m_stopped : 1;
};
