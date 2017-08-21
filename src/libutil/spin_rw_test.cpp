/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
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

#include <OpenImageIO/thread.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;

// Test spin_rw_mutex by creating a bunch of threads usually just check
// the accumulator value (requiring a read lock), but occasionally
// (1/100 of the time) increment the accumulator, requiring a write
// lock.  If, at the end, the accumulated value is equal to
// iterations/read_to_write_ratio*threads, then the locks worked.

static int read_write_ratio = 99;
static int iterations = 16000000;
static int numthreads = 16;
static int ntrials = 1;
static bool verbose = false;
static bool wedge = false;

volatile long long accum = 0;
spin_rw_mutex mymutex;




static void
do_accum (int iterations)
{
    for (int i = 0;  i < iterations;  ++i) {
        if ((i % (read_write_ratio+1)) == read_write_ratio) {
            spin_rw_write_lock lock (mymutex);
            accum += 1;
        } else {
            spin_rw_read_lock lock (mymutex);
            // meaningless test to force examination of the variable
            if (accum < 0)
                break;
        }
    }
}



void test_spin_rw (int numthreads, int iterations)
{
    accum = 0;
    thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (do_accum, iterations);
    }
    if (verbose)
        std::cout << "Created " << threads.size() << " threads\n";
    threads.join_all ();
    OIIO_CHECK_EQUAL (accum, (((long long)iterations/(read_write_ratio+1)) * (long long)numthreads));
    if (verbose)
        std::cout << "it " << iterations << ", r::w = " << read_write_ratio
                  << ", accum = " << accum << "\n";
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("spin_rw_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  spin_rw_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--threads %d", &numthreads, 
                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iters %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                "--rwratio %d", &read_write_ratio, 
                    ustring::format("Reader::writer ratio (default: %d)", read_write_ratio).c_str(),
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
    getargs (argc, argv);

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";
    std::cout << "reader:writer ratio = " << read_write_ratio << ":1\n";
    std::cout << "threads\ttime (best of " << ntrials << ")\n";
    std::cout << "-------\t----------\n";

    static int threadcounts[] = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 64, 128, 1024, 1<<30 };
    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt = threadcounts[i];
        int its = iterations/nt;

        double range;
        double t = time_trial (std::bind(test_spin_rw,nt,its),
                               ntrials, &range);

        std::cout << Strutil::format ("%2d\t%s\t%5.1fs, range %.1f\t(%d iters/thread)\n",
                                      nt, Strutil::timeintervalformat(t),
                                      t, range, its);
        if (! wedge)
            break;    // don't loop if we're not wedging
    }

    return unit_test_failures;
}
