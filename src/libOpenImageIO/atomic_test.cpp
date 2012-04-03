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
#include "strutil.h"
#include "timer.h"

#include <boost/thread/thread.hpp>

#include "unittest.h"


OIIO_NAMESPACE_USING;

// How do we test atomics?  Run a whole bunch of threads, incrementing
// and decrementing the crap out of it, and make sure it has the right
// value at the end.

const int iterations = 10000000;
const int numthreads = 16;

static spin_mutex print_mutex;  // make the prints not clobber each other
atomic_int ai;
atomic_ll all;


static void
do_int_math ()
{
    {
        spin_lock lock(print_mutex);
        std::cout << "thread " << boost::this_thread::get_id()
              << ", ai = " << ai << "\n";
    }
    for (int i = 0;  i < iterations;  ++i) {
        ++ai;
        ai += 3;
        --ai;
        ai++;
        ai -= 3;
        --ai;
        // That should have a net change of 0, but since other threads
        // are doing operations simultaneously, it's only after all
        // threads have finished that we can be sure it's back to the
        // initial value.
    }
}



void test_atomic_int ()
{
    {
        spin_lock lock(print_mutex);
        std::cout << "hw threads = " << boost::thread::hardware_concurrency() << "\n";
    }

    ai = 42;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (&do_int_math);
    }
    {
        spin_lock lock(print_mutex);
        std::cout << "Created " << threads.size() << " threads\n";
    }
    threads.join_all ();
    OIIO_CHECK_EQUAL (ai, 42);
}



static void
do_int64_math ()
{
    {
        spin_lock lock(print_mutex);
        std::cout << "thread " << boost::this_thread::get_id()
                  << ", all = " << all << "\n";
    }
    for (int i = 0;  i < iterations;  ++i) {
        ++all;
        all += 3;
        --all;
        all++;
        all -= 3;
        --all;
        // That should have a net change of 0, but since other threads
        // are doing operations simultaneously, it's only after all
        // threads have finished that we can be sure it's back to the
        // initial value.
    }
}



void test_atomic_int64 ()
{
    all = 0;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (&do_int64_math);
    }
    threads.join_all ();
    do_int64_math ();
    OIIO_CHECK_EQUAL (all, 0);
}



int main (int argc, char *argv[])
{
    Timer timer;
    test_atomic_int ();
    test_atomic_int64 ();
    std::cout << "Time: " << Strutil::timeintervalformat (timer()) << "\n";
    return unit_test_failures;
}
