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
static std::vector<std::array<char, 16>> strings;


static void
create_lotso_ustrings(int iterations)
{
    OIIO_DASSERT(size_t(iterations) <= strings.size());
    if (verbose)
        Strutil::printf("thread %d\n", std::this_thread::get_id());
    size_t h = 0;
    for (int i = 0; i < iterations; ++i) {
        ustring s(strings[i].data());
        h += s.hash();
    }
    if (verbose)
        Strutil::printf("checksum %08x\n", unsigned(h));
}



static void
getargs(int argc, char* argv[])
{
    // clang-format off
    ArgParse ap;
    ap.intro("ustring_test\n" OIIO_INTRO_STRING)
      .usage("ustring_test [options]");

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

    OIIO_CHECK_ASSERT(ustring("foo") == ustring("foo"));
    OIIO_CHECK_ASSERT(ustring("bar") != ustring("foo"));
    ustring foo("foo");
    OIIO_CHECK_ASSERT(foo.string() == "foo");
    ustring bar("bar");
    OIIO_CHECK_EQUAL(ustring::concat(foo, bar), "foobar");
    OIIO_CHECK_EQUAL(ustring::concat(foo, "bar"), "foobar");
    OIIO_CHECK_EQUAL(ustring::concat(foo, ""), "foo");
    OIIO_CHECK_EQUAL(ustring::concat("", foo), "foo");
    ustring longstring(Strutil::repeat("01234567890", 100));
    OIIO_CHECK_EQUAL(ustring::concat(longstring, longstring),
                     ustring::sprintf("%s%s", longstring, longstring));

    const int nhw_threads = Sysutil::hardware_concurrency();
    std::cout << "hw threads = " << nhw_threads << "\n";

    // user wants to max out the number of threads
    if (numthreads <= 0)
        numthreads = nhw_threads;

    // prepare the strings we will turn into ustrings to avoid
    // including snprintf in the benchmark
    strings.resize(wedge ? iterations : iterations / numthreads);
    int i = 0;
    for (auto& s : strings)
        snprintf(s.data(), s.size(), "%d", i++);

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
