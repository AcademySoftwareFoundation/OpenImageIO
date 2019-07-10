// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <cstdio>
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

// Test ustring's internal locks by creating a bunch of strings in many
// threads simultaneously.  Hopefully something will crash if the
// internal table is not being locked properly.

static int iterations = 1000000;
static int numthreads = 16;
static int ntrials    = 1;
static bool verbose   = false;
static bool wedge     = false;



static void
create_lotso_ustrings(int iterations)
{
    if (verbose)
        Strutil::printf("thread %d\n", std::this_thread::get_id());
    for (int i = 0; i < iterations; ++i) {
        char buf[20];
        sprintf(buf, "%d", i);
        ustring s(buf);
    }
}



static void
getargs(int argc, char* argv[])
{
    bool help = false;
    ArgParse ap;
    // clang-format off
    ap.options(
        "ustring_test\n" OIIO_INTRO_STRING "\n"
        "Usage:  ustring_test [options]",
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



int
main(int argc, char* argv[])
{
    getargs(argc, argv);

    OIIO_CHECK_ASSERT(ustring("foo") == ustring("foo"));
    OIIO_CHECK_ASSERT(ustring("bar") != ustring("foo"));
    ustring foo("foo");
    OIIO_CHECK_ASSERT(foo.string() == "foo");

    std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";

    if (wedge) {
        timed_thread_wedge(create_lotso_ustrings, numthreads, iterations,
                           ntrials);
    } else {
        timed_thread_wedge(create_lotso_ustrings, numthreads, iterations,
                           ntrials,
                           numthreads /* just this one thread count */);
    }
    OIIO_CHECK_ASSERT(true);  // If we make it here without crashing, pass

    if (verbose)
        std::cout << "\n" << ustring::getstats() << "\n";

    return unit_test_failures;
}
