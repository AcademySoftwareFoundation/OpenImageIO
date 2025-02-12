// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


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

// Test spin_rw_mutex by creating a bunch of threads usually just check
// the accumulator value (requiring a read lock), but occasionally
// (1/100 of the time) increment the accumulator, requiring a write
// lock.  If, at the end, the accumulated value is equal to
// iterations/read_to_write_ratio*threads, then the locks worked.

static int read_write_ratio = 99;
static int iterations       = 16000000;
static int numthreads       = 16;
static int ntrials          = 1;
static bool verbose         = false;
static bool wedge           = false;

long long accum = 0;
spin_rw_mutex mymutex;



static void
do_accum(int iterations)
{
    for (int i = 0; i < iterations; ++i) {
        if ((i % (read_write_ratio + 1)) == read_write_ratio) {
            spin_rw_write_lock lock(mymutex);
            accum += 1;
        } else {
            spin_rw_read_lock lock(mymutex);
            // meaningless test to force examination of the variable
            if (accum < 0)
                break;
        }
    }
}



void
test_spin_rw(int numthreads, int iterations)
{
    accum = 0;
    thread_group threads;
    for (int i = 0; i < numthreads; ++i) {
        threads.create_thread(do_accum, iterations);
    }
    if (verbose)
        std::cout << "Created " << threads.size() << " threads\n";
    threads.join_all();
    OIIO_CHECK_EQUAL(accum, (((long long)iterations / (read_write_ratio + 1))
                             * (long long)numthreads));
    if (verbose)
        std::cout << "it " << iterations << ", r::w = " << read_write_ratio
                  << ", accum = " << accum << "\n";
}



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("spin_rw_test\n" OIIO_INTRO_STRING)
      .usage("spin_rw_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--threads %d", &numthreads)
      .help(Strutil::fmt::format("Number of threads (default: {})",
                                 numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})",
                                 iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--rwratio %d", &read_write_ratio)
       .help(Strutil::fmt::format("Reader::writer ratio (default: {})",
                                 read_write_ratio));
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
    std::cout << "reader:writer ratio = " << read_write_ratio << ":1\n";
    std::cout << "threads\ttime (best of " << ntrials << ")\n";
    std::cout << "-------\t----------\n";

    static int threadcounts[] = { 1,  2,  4,  8,  12,  16,   20,
                                  24, 28, 32, 64, 128, 1024, 1 << 30 };
    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt  = threadcounts[i];
        int its = iterations / nt;

        double range;
        double t = time_trial(std::bind(test_spin_rw, nt, its), ntrials, 1,
                              &range);

        Strutil::printf("%2d\t%s\t%5.1fs, range %.1f\t(%d iters/thread)\n", nt,
                        Strutil::timeintervalformat(t), t, range, its);
        if (!wedge)
            break;  // don't loop if we're not wedging
    }

    return unit_test_failures;
}
