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


#include <functional>
#include <iostream>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>


using namespace OIIO;

// Test spin locks by creating a bunch of threads that all increment the
// accumulator many times, protected by spin locks.  If, at the end, the
// accumulated value is equal to iterations*threads, then the spin locks
// worked.

static int iterations = 40000000;
static int numthreads = 16;
static int ntrials    = 1;
static bool verbose   = false;
static bool wedge     = false;

static spin_mutex print_mutex;  // make the prints not clobber each other
volatile long long accum = 0;
float faccum             = 0;
spin_mutex mymutex;



static void
time_lock_cycle()
{
    // Find out how long it takes
    Benchmarker bench;
    std::cout << "Cost of lock/unlock cycle under no contention:\n";
    spin_mutex sm;
    std::mutex m;
    bench("spin_mutex", [&]() {
        sm.lock();
        sm.unlock();
    });
    bench("std::mutex", [&]() {
        m.lock();
        m.unlock();
    });
}



static void
do_accum(int iterations)
{
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "thread " << std::this_thread::get_id()
                  << ", accum = " << accum << "\n";
    }
#if 1
    for (int i = 0; i < iterations; ++i) {
        spin_lock lock(mymutex);
        accum += 1;
    }
#else
    // Alternate one that mixes in some math to make longer lock hold time,
    // and also more to do between locks.  Interesting contrast in timings.
    float last = 0.0f;
    for (int i = 0; i < iterations; ++i) {
        last = fmodf(sinf(last), 1.0f);
        spin_lock lock(mymutex);
        accum += 1;
        faccum = fmod(sinf(faccum + last), 1.0f);
    }
#endif
}



static void
getargs(int argc, char* argv[])
{
    bool help = false;
    ArgParse ap;
    // clang-format off
    ap.options(
        "spinlock_test\n" OIIO_INTRO_STRING "\n"
        "Usage:  spinlock_test [options]",
        // "%*", parse_files, "",
        "--help", &help, "Print help message",
        "-v", &verbose, "Verbose mode",
        "--threads %d", &numthreads,
                ustring::sprintf("Number of threads (default: %d)", numthreads).c_str(),
        "--iters %d", &iterations,
                ustring::sprintf("Number of iterations (default: %d)", iterations).c_str(),
        "--trials %d", &ntrials, "Number of trials", "--wedge", &wedge, "Do a wedge test",
        nullptr);
    // clang-format on
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        exit(EXIT_FAILURE);
    }
    if (help) {
        ap.usage();
        exit(EXIT_FAILURE);
    }
}



int
main(int argc, char* argv[])
{
    getargs(argc, argv);

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";

    time_lock_cycle();

    std::cout << "\nTiming thread contention for spin_mutex...\n";
    if (wedge)
        timed_thread_wedge(do_accum, numthreads, iterations, ntrials);
    else
        timed_thread_wedge(do_accum, numthreads, iterations, ntrials,
                           numthreads);

    return unit_test_failures;
}
