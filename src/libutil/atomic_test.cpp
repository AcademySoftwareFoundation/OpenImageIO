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

#include "OpenImageIO/thread.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/sysutil.h"
#include "OpenImageIO/timer.h"
#include "OpenImageIO/argparse.h"
#include "OpenImageIO/ustring.h"

#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include "OpenImageIO/unittest.h"


OIIO_NAMESPACE_USING;

// How do we test atomics?  Run a whole bunch of threads, incrementing
// and decrementing the crap out of it, and make sure it has the right
// value at the end.

static int iterations = 40000000;
static int numthreads = 16;
static int ntrials = 1;
static bool verbose = false;
static bool wedge = false;

static spin_mutex print_mutex;  // make the prints not clobber each other
atomic_int ai;
atomic_ll all;


static void
do_int_math (int iterations)
{
    if (verbose) {
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



void test_atomic_int (int numthreads, int iterations)
{
    ai = 42;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (boost::bind (do_int_math, iterations));
    }
    ASSERT ((int)threads.size() == numthreads);
    threads.join_all ();
    OIIO_CHECK_EQUAL (ai, 42);
}



static void
do_int64_math (int iterations)
{
    if (verbose) {
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



void test_atomic_int64 (int numthreads, int iterations)
{
    all = 0;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (boost::bind (do_int64_math, iterations));
    }
    threads.join_all ();
    OIIO_CHECK_EQUAL (all, 0);
}



void test_atomics (int numthreads, int iterations)
{
    test_atomic_int (numthreads, iterations);
    test_atomic_int64 (numthreads, iterations);
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("atomic_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  atomic_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--threads %d", &numthreads, 
                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iters %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                "--wedge", &wedge, "Do a wedge test",
                NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



int main (int argc, char *argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODECOV)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs (argc, argv);

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";
    std::cout << "threads\ttime (best of " << ntrials << ")\n";
    std::cout << "-------\t----------\n";

    static int threadcounts[] = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 64, 128, 1024, 1<<30 };
    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt = wedge ? threadcounts[i] : numthreads;
        int its = iterations/nt;

        double range;
        double t = time_trial (boost::bind(test_atomics,nt,its),
                               ntrials, &range);

        std::cout << Strutil::format ("%2d\t%5.1f   range %.2f\t(%d iters/thread)\n",
                                      nt, t, range, its);
        if (! wedge)
            break;    // don't loop if we're not wedging
    }

    return unit_test_failures;
}
