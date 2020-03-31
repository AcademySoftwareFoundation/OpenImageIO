// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <iostream>
#include <thread>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>


using namespace OIIO;

// How do we test atomics?  Run a whole bunch of threads, incrementing
// and decrementing the crap out of it, and make sure it has the right
// value at the end.

static int iterations = 2000000;
static int numthreads = clamp((int)Sysutil::physical_concurrency(), 2, 16);
static int ntrials    = 5;
static bool verbose   = false;
static bool wedge     = false;

static spin_mutex print_mutex;  // make the prints not clobber each other
atomic_int ai;
atomic_ll all;
std::atomic<float> af(0.0f);
std::atomic<double> ad(0.0);


static void
do_int_math(int iterations)
{
    for (int i = 0; i < iterations; ++i) {
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



void
test_atomic_int()
{
    // Test and, or, xor
    ai = 42;
    ai &= 15;
    OIIO_CHECK_EQUAL(ai, 10);
    ai |= 6;
    OIIO_CHECK_EQUAL(ai, 14);
    ai ^= 31;
    OIIO_CHECK_EQUAL(ai, 17);
    ai = 42;
    int tmp;
    tmp = ai.fetch_and(15);
    OIIO_CHECK_EQUAL(tmp, 42);
    OIIO_CHECK_EQUAL(ai, 10);
    tmp = ai.fetch_or(6);
    OIIO_CHECK_EQUAL(tmp, 10);
    OIIO_CHECK_EQUAL(ai, 14);
    tmp = ai.fetch_xor(31);
    OIIO_CHECK_EQUAL(tmp, 14);
    OIIO_CHECK_EQUAL(ai, 17);
}



static void
do_int64_math(int iterations)
{
    for (int i = 0; i < iterations; ++i) {
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



void
test_atomic_int64()
{
    // Test and, or, xor
    all = 42;
    all &= 15;
    OIIO_CHECK_EQUAL(all, 10);
    all |= 6;
    OIIO_CHECK_EQUAL(all, 14);
    all ^= 31;
    OIIO_CHECK_EQUAL(all, 17);
    all = 42;
    long long tmp;
    tmp = all.fetch_and(15);
    OIIO_CHECK_EQUAL(tmp, 42);
    OIIO_CHECK_EQUAL(all, 10);
    tmp = all.fetch_or(6);
    OIIO_CHECK_EQUAL(tmp, 10);
    OIIO_CHECK_EQUAL(all, 14);
    tmp = all.fetch_xor(31);
    OIIO_CHECK_EQUAL(tmp, 14);
    OIIO_CHECK_EQUAL(all, 17);
}



static void
do_float_math(int iterations)
{
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "thread " << std::this_thread::get_id()
                  << ", all = " << all << "\n";
    }
    for (int i = 0; i < iterations; ++i) {
        atomic_fetch_add(af, 1.0f);
        atomic_fetch_add(af, 3.0f);
        atomic_fetch_add(af, -1.0f);
        atomic_fetch_add(af, 1.0f);
        atomic_fetch_add(af, -3.0f);
        atomic_fetch_add(af, -1.0f);
        // That should have a net change of 0, but since other threads
        // are doing operations simultaneously, it's only after all
        // threads have finished that we can be sure it's back to the
        // initial value.
    }
}



static void
do_double_math(int iterations)
{
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "thread " << std::this_thread::get_id()
                  << ", all = " << all << "\n";
    }
    for (int i = 0; i < iterations; ++i) {
        atomic_fetch_add(ad, 1.0);
        atomic_fetch_add(ad, 3.0);
        atomic_fetch_add(ad, -1.0);
        atomic_fetch_add(ad, 1.0);
        atomic_fetch_add(ad, -3.0);
        atomic_fetch_add(ad, -1.0);
        // That should have a net change of 0, but since other threads
        // are doing operations simultaneously, it's only after all
        // threads have finished that we can be sure it's back to the
        // initial value.
    }
}



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("atomic_test\n" OIIO_INTRO_STRING)
      .usage("atomic_test [options]");

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
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";

    std::cout << "\natomic int:\n";
    test_atomic_int();
    ai = 0;
    if (wedge)
        timed_thread_wedge(do_int_math, numthreads, iterations, ntrials);
    else
        timed_thread_wedge(do_int_math, numthreads, iterations, ntrials,
                           numthreads);
    OIIO_CHECK_EQUAL(ai, 0);

    std::cout << "\natomic int64:\n";
    test_atomic_int64();
    all = 0;
    if (wedge)
        timed_thread_wedge(do_int64_math, numthreads, iterations, ntrials);
    else
        timed_thread_wedge(do_int64_math, numthreads, iterations, ntrials,
                           numthreads);
    OIIO_CHECK_EQUAL(all, 0);

    std::cout << "\natomic floats:\n";
    af = 0.0f;
    if (wedge)
        timed_thread_wedge(do_float_math, numthreads, iterations, ntrials);
    else
        timed_thread_wedge(do_float_math, numthreads, iterations, ntrials,
                           numthreads);
    OIIO_CHECK_EQUAL(af, 0.0f);

    std::cout << "\natomic doubles:\n";
    ad = 0.0;
    if (wedge)
        timed_thread_wedge(do_double_math, numthreads, iterations, ntrials);
    else
        timed_thread_wedge(do_double_math, numthreads, iterations, ntrials,
                           numthreads);
    OIIO_CHECK_EQUAL(ad, 0.0);

    return unit_test_failures;
}
