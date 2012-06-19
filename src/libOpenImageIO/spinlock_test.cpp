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
#include "argparse.h"
#include "ustring.h"

#include <boost/thread/thread.hpp>

#include "unittest.h"


OIIO_NAMESPACE_USING;

// Test spin locks by creating a bunch of threads that all increment the
// accumulator many times, protected by spin locks.  If, at the end, the
// accumulated value is equal to iterations*threads, then the spin locks
// worked.

static int iterations = 100000000;
static int numthreads = 16;
static bool verbose = false;

static spin_mutex print_mutex;  // make the prints not clobber each other
volatile long long accum = 0;
spin_mutex mymutex;



static void
do_accum ()
{
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "thread " << boost::this_thread::get_id() 
                  << ", accum = " << accum << "\n";
    }
    for (int i = 0;  i < iterations;  ++i) {
        spin_lock lock (mymutex);
        accum += 1;
    }
}



void test_spinlock ()
{
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "hw threads = " << boost::thread::hardware_concurrency() << "\n";
    }

    accum = 0;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (&do_accum);
    }
    if (verbose) {
        spin_lock lock(print_mutex);
        std::cout << "Created " << threads.size() << " threads\n";
    }
    threads.join_all ();
    int a = (int) accum;
    OIIO_CHECK_EQUAL (a, (int)(numthreads * iterations));
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("spinlock_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  spinlock_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--threads %d", &numthreads, 
                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iters %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
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
    std::cout << "Running " << iterations << " on " << numthreads << "\n";

    Timer timer;
    test_spinlock ();
    std::cout << "accum = " << accum << ", expect " 
              << ((long long)iterations * (long long)numthreads) << "\n";
    std::cout << "Time: " << Strutil::timeintervalformat (timer()) << "\n";
    OIIO_CHECK_EQUAL (accum, ((long long)iterations * (long long)numthreads));

    return unit_test_failures;
}
