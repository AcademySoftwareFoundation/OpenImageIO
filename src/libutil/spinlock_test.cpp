// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


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
    ArgParse ap;
    // clang-format off
    ap.intro("spinlock_test\n" OIIO_INTRO_STRING)
      .usage("spinlock_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--threads %d", &numthreads)
      .help(Strutil::sprintf("Number of threads (default: %d)", numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::sprintf("Number of iterations (default: %d)", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--wedge", &wedge)
      .help("Do a wedge test");
    // clang-format on

    ap.parse(argc, (const char**)argv);
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
