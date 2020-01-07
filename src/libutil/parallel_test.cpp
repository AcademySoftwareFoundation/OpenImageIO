// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <algorithm>
#include <functional>
#include <iostream>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>

using namespace OIIO;

static int iterations     = 100000;
static int numthreads     = 16;
static int ntrials        = 1;
static bool verbose       = false;
static bool wedge         = false;
static int threadcounts[] = { 1,  2,  4,  8,  12,  16,   20,
                              24, 28, 32, 64, 128, 1024, 1 << 30 };



static void
getargs(int argc, char* argv[])
{
    bool help = false;
    ArgParse ap;
    // clang-format off
    ap.options(
        "parallel_test\n" OIIO_INTRO_STRING "\n"
        "Usage:  parallel_test [options]",
        // "%*", parse_files, "",
        "--help", &help, "Print help message",
        "-v", &verbose, "Verbose mode",
        "--threads %d", &numthreads,
            ustring::sprintf("Number of threads (default: %d)", numthreads).c_str(),
        "--iters %d", &iterations,
            ustring::sprintf("Number of iterations (default: %d)", iterations).c_str(),
        "--trials %d", &ntrials, "Number of trials",
        "--wedge", &wedge, "Do a wedge test",
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



void
time_parallel_for()
{
    std::cout << "\nTiming how long it takes to run parallel_for:\n";
    std::cout << "threads\ttime (best of " << ntrials << ")\n";
    std::cout << "-------\t----------\n";
    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt  = wedge ? threadcounts[i] : numthreads;
        int its = iterations / nt;

        // make a lambda function that spawns a bunch of threads, calls a
        // trivial function, then waits for them to finish and tears down
        // the group.
        auto func = [=]() { parallel_for(0, nt, [](int64_t i) { /*empty*/ }); };

        double range;
        double t = time_trial(func, ntrials, its, &range);

        Strutil::printf("%2d\t%5.1f   launch %8.1f threads/sec\n", nt, t,
                        (nt * its) / t);
        if (!wedge)
            break;  // don't loop if we're not wedging
    }
}



void
test_parallel_for()
{
    // vector of ints, initialized to zero
    const int length = 1000;
    std::vector<int> vals(length, 0);

    // Increment all the integers via parallel_for
    parallel_for(0, length, [&](uint64_t i) { vals[i] += 1; });

    // Verify that all elements are exactly 1
    bool all_one = std::all_of(vals.cbegin(), vals.cend(),
                               [&](int i) { return vals[i] == 1; });
    OIIO_CHECK_ASSERT(all_one);
}



void
test_parallel_for_2D()
{
    // vector of ints, initialized to zero
    const int size = 100;
    std::vector<int> vals(size * size, 0);

    // Increment all the integers via parallel_for
    parallel_for_2D(0, size, 0, size,
                    [&](uint64_t i, uint64_t j) { vals[j * size + i] += 1; });

    // Verify that all elements are exactly 1
    bool all_one = std::all_of(vals.cbegin(), vals.cend(),
                               [&](int i) { return vals[i] == 1; });
    OIIO_CHECK_ASSERT(all_one);
}



void
test_thread_pool_recursion()
{
    std::cout << "\nTesting thread pool recursion" << std::endl;
    static spin_mutex print_mutex;
    thread_pool* pool(default_thread_pool());
    pool->resize(2);
    parallel_for(0, 10, [&](int id, int64_t i) {
        // sleep long enough that we can push all the jobs before any get
        // done.
        Sysutil::usleep(10);
        // then run something else that itself will push jobs onto the
        // thread pool queue.
        parallel_for(0, 10, [&](int id, int64_t i) {
            Sysutil::usleep(2);
            spin_lock lock(print_mutex);
            // std::cout << "  recursive running thread " << id << std::endl;
        });
    });
}



void
test_empty_thread_pool()
{
    std::cout << "\nTesting that pool size 0 makes all jobs run by caller"
              << std::endl;
    thread_pool* pool(default_thread_pool());
    pool->resize(0);
    OIIO_CHECK_EQUAL(pool->size(), 0);
    atomic_int count(0);
    const int ntasks = 100;
    task_set ts(pool);
    for (int i = 0; i < ntasks; ++i)
        ts.push(pool->push([&](int id) {
            OIIO_ASSERT(id == -1 && "Must be run by calling thread");
            count += 1;
        }));
    ts.wait();
    OIIO_CHECK_EQUAL(count, ntasks);
}



int
main(int argc, char** argv)
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

    test_parallel_for();
    test_parallel_for_2D();
    time_parallel_for();
    test_thread_pool_recursion();
    test_empty_thread_pool();

    return unit_test_failures;
}
