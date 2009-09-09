/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <iostream>

#include "thread.h"

#include <boost/thread/thread.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>


// Test spin locks by creating a bunch of threads that all increment the
// accumulator many times, protected by spin locks.  If, at the end, the
// accumulated value is equal to iterations*threads, then the spin locks
// worked.

const int iterations = 1000000;
const int numthreads = 16;

volatile int accum = 0;
spin_mutex mymutex;



static void
do_int_math ()
{
    std::cout << "thread " 
#if (BOOST_VERSION >= 103500)
              << boost::this_thread::get_id() 
#endif
              << ", accum = " << accum << "\n";
    for (int i = 0;  i < iterations;  ++i) {
        spin_lock lock (mymutex);
        accum += 1;
    }
}



BOOST_AUTO_TEST_CASE (test_atomic_int)
{
#if (BOOST_VERSION >= 103500)
    std::cout << "hw threads = " << boost::thread::hardware_concurrency() << "\n";
#endif

    accum = 0;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (&do_int_math);
    }
    std::cout << "Created " << threads.size() << " threads\n";
    threads.join_all ();
    BOOST_CHECK_EQUAL ((int)accum, (int)(numthreads * iterations));
}
