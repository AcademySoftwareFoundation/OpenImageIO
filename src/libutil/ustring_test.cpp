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
#include <cstdio>
#include <functional>

#include <OpenImageIO/thread.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;

// Test ustring's internal locks by creating a bunch of strings in many
// threads simultaneously.  Hopefully something will crash if the 
// internal table is not being locked properly.

static int iterations = 1000000;
static int numthreads = 16;
static int ntrials = 1;
static bool verbose = false;
static bool wedge = false;



static void
create_lotso_ustrings (int iterations)
{
    if (verbose)
        Strutil::printf ("thread %d\n", std::this_thread::get_id());
    for (int i = 0;  i < iterations;  ++i) {
        char buf[20];
        sprintf (buf, "%d", i);
        ustring s (buf);
    }
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("ustring_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  ustring_test [options]",
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
    getargs (argc, argv);

    OIIO_CHECK_ASSERT(ustring("foo") == ustring("foo"));
    OIIO_CHECK_ASSERT(ustring("bar") != ustring("foo"));
    ustring foo ("foo");
    OIIO_CHECK_ASSERT (foo.string() == "foo");

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";

    if (wedge) {
        timed_thread_wedge (create_lotso_ustrings, numthreads, iterations, ntrials);
    } else {
        timed_thread_wedge (create_lotso_ustrings, numthreads, iterations, ntrials,
                            numthreads /* just this one thread count */);
    }
    OIIO_CHECK_ASSERT (true);  // If we make it here without crashing, pass

    if (verbose)
        std::cout << "\n" << ustring::getstats() << "\n";

    return unit_test_failures;
}
