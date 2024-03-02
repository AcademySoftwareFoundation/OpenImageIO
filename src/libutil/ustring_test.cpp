// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdio>
#include <functional>
#include <iostream>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>

#if FMT_VERSION >= 90000
#    include <OpenImageIO/detail/fmt/std.h>
#endif


using namespace OIIO;

// Test ustring's internal locks by creating a bunch of strings in many
// threads simultaneously.  Hopefully something will crash if the
// internal table is not being locked properly.

static int iterations = 1000000;
static int numthreads = 16;
static int ntrials    = 1;
static bool verbose   = false;
static bool wedge     = false;
static int collide    = 1;  // Millions
static std::vector<std::array<char, 16>> strings;


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
      .help(Strutil::fmt::format("Number of threads (default: {})", numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--wedge", &wedge)
      .help("Do a wedge test");
    ap.arg("--collide %d", &collide)
      .help("Strings (x 1M) to create to make hash collisions");
    // clang-format on

    ap.parse(argc, (const char**)argv);

    const int nhw_threads = Sysutil::hardware_concurrency();
    std::cout << "hw threads = " << nhw_threads << "\n";

    // user wants to max out the number of threads
    if (numthreads <= 0)
        numthreads = nhw_threads;
}


void
test_ustring()
{
    ustring foo("foo"), bar("bar"), empty(""), uninit;
    ustring foobarbaz("foobarbaz");

    OIIO_CHECK_ASSERT(std::is_default_constructible<OIIO::ustring>());
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<OIIO::ustring>());
    OIIO_CHECK_ASSERT(std::is_trivially_destructible<OIIO::ustring>());
    OIIO_CHECK_ASSERT(std::is_trivially_move_constructible<OIIO::ustring>());
    OIIO_CHECK_ASSERT(std::is_trivially_copy_constructible<OIIO::ustring>());
    OIIO_CHECK_ASSERT(std::is_trivially_move_assignable<OIIO::ustring>());

    // Size of a ustring is just a pointer
    OIIO_CHECK_EQUAL(sizeof(ustring), sizeof(const char*));

    // Test constructors
    OIIO_CHECK_ASSERT(uninit.c_str() == nullptr);
    OIIO_CHECK_EQUAL(foo, ustring(string_view("foo")));
    OIIO_CHECK_EQUAL(foo, ustring(std::string("foo")));
    OIIO_CHECK_EQUAL(ustring("hoobarfoo123", 6, 3), ustring("foo"));
    OIIO_CHECK_EQUAL(ustring("hoobarfoo123", 3), ustring("hoo"));
    OIIO_CHECK_EQUAL(ustring(3, 'x'), ustring("xxx"));
    OIIO_CHECK_EQUAL(ustring(foo), foo);
    OIIO_CHECK_EQUAL(ustring(foo, 2, 1), ustring("o"));

    // Conversion to char*, string_view, string
    OIIO_CHECK_ASSERT(!strcmp(foo.c_str(), "foo"));
    OIIO_CHECK_EQUAL(foo.string(), "foo");
    OIIO_CHECK_EQUAL(string_view(foo), "foo");
    OIIO_CHECK_EQUAL(std::string(foo), "foo");

    // assignment, clear
    ustring foo2;
    foo2 = foo;
    OIIO_CHECK_EQUAL(foo2, foo);
    foo2.clear();
    OIIO_CHECK_EQUAL(foo2, uninit);

    // length/size, empty
    OIIO_CHECK_EQUAL(foo.length(), 3);
    OIIO_CHECK_EQUAL(foo.size(), 3);
    OIIO_CHECK_EQUAL(empty.size(), 0);
    OIIO_CHECK_EQUAL(uninit.size(), 0);
    OIIO_CHECK_ASSERT(empty.empty());
    OIIO_CHECK_ASSERT(uninit.empty());
    OIIO_CHECK_ASSERT(!foo.empty());

    // Characters
    OIIO_CHECK_EQUAL(foo[0], 'f');
    OIIO_CHECK_EQUAL(bar[1], 'a');

    // copy
    char buf[10];
    foo.copy(buf, sizeof(buf) - 1);
    OIIO_CHECK_ASSERT(!strcmp(buf, "foo"));
    ustring("foobarbaz").copy(buf, 4, 3);
    OIIO_CHECK_ASSERT(!strcmp(buf, "barb"));

    // substr
    OIIO_CHECK_ASSERT(foobarbaz.substr(3, 4) == ustring("barb"));

    // find
    OIIO_CHECK_ASSERT(foobarbaz.find("ba") == 3);
    OIIO_CHECK_ASSERT(foobarbaz.find("ba", 4) == 6);
    OIIO_CHECK_ASSERT(foobarbaz.rfind("ba") == 6);
    OIIO_CHECK_ASSERT(foobarbaz.rfind("ba") == 6);
    // FIXME: there are lots of find_* permutations to test, but I'm lazy

    // concat
    OIIO_CHECK_EQUAL(ustring::concat(foo, bar), "foobar");
    OIIO_CHECK_EQUAL(ustring::concat(foo, "bar"), "foobar");
    OIIO_CHECK_EQUAL(ustring::concat(foo, ""), "foo");
    OIIO_CHECK_EQUAL(ustring::concat("", foo), "foo");
    ustring longstring(Strutil::repeat("01234567890", 100));
    OIIO_CHECK_EQUAL(ustring::concat(longstring, longstring),
                     ustring::fmtformat("{}{}", longstring, longstring));

    // from_hash
    OIIO_CHECK_EQUAL(ustring::from_hash(foo.hash()), foo);
    OIIO_CHECK_EQUAL(empty.hash(), 0);
    OIIO_CHECK_EQUAL(ustring().hash(), 0);

    // make_unique, is_unique, from_unique
    const char* foostr = foo.c_str();
    OIIO_CHECK_EQUAL(ustring::make_unique("foo"), foostr);
    OIIO_CHECK_EQUAL(ustring::make_unique(string_view()), ustring());
    OIIO_CHECK_EQUAL(ustring::is_unique(foostr), true);
    OIIO_CHECK_EQUAL(ustring::is_unique("foo"), false);
    OIIO_CHECK_EQUAL(ustring::from_unique(foostr), foo);

    // std::hash
    OIIO_CHECK_EQUAL(std::hash<ustring> {}(foo), foo.hash());

    // string literals
    auto whichtype = "foo"_us;
    OIIO_CHECK_EQUAL(whichtype, ustring("foo"));
    OIIO_CHECK_ASSERT((std::is_same<decltype(whichtype), ustring>::value));
    OIIO_CHECK_ASSERT(!(std::is_same<decltype(whichtype), const char*>::value));
}



void
test_ustringhash()
{
    // Two ustrings
    ustring foo("foo"), bar("bar");

    OIIO_CHECK_ASSERT(std::is_default_constructible<OIIO::ustringhash>());
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<OIIO::ustringhash>());
    OIIO_CHECK_ASSERT(std::is_trivially_destructible<OIIO::ustringhash>());
    OIIO_CHECK_ASSERT(
        std::is_trivially_move_constructible<OIIO::ustringhash>());
    OIIO_CHECK_ASSERT(
        std::is_trivially_copy_constructible<OIIO::ustringhash>());
    OIIO_CHECK_ASSERT(std::is_trivially_move_assignable<OIIO::ustringhash>());

    OIIO_CHECK_EQUAL(sizeof(ustringhash), sizeof(size_t));

    // Make two ustringhash's from strings
    ustringhash hfoo("foo"), hbar("bar");
    OIIO_CHECK_EQUAL(hfoo.hash(), foo.hash());
    OIIO_CHECK_EQUAL(hbar.hash(), bar.hash());
    OIIO_CHECK_NE(hfoo.hash(), hbar.hash());

    // Check copy construction, assignment, ==, !=
    ustringhash hfoo_copy(hfoo);
    OIIO_CHECK_EQUAL(hfoo_copy, hfoo);
    OIIO_CHECK_NE(hfoo, hbar);
    ustringhash hfoo_copy2;
    hfoo_copy2 = hfoo;
    OIIO_CHECK_EQUAL(hfoo_copy2, hfoo);

    // Assignment from a ustring
    ustringhash hfoo_from_foo = foo;
    OIIO_CHECK_EQUAL(hfoo_from_foo, hfoo);

    // Ask a ustring for its ustringhash
    OIIO_CHECK_EQUAL(hfoo, foo.uhash());

    // ustring constructed from a ustringhash
    OIIO_CHECK_EQUAL(hfoo_from_foo, foo);

    // string_view and string from ustringhash
    string_view foo_sv = hfoo;
    OIIO_CHECK_EQUAL(foo_sv, "foo");
    OIIO_CHECK_EQUAL(std::string(foo_sv), "foo");

    // clear and empty()
    OIIO_CHECK_ASSERT(!hfoo_copy2.empty());
    hfoo_copy2.clear();
    OIIO_CHECK_ASSERT(hfoo_copy2.empty());

    // Can we get to the characters?
    OIIO_CHECK_EQUAL(hfoo.c_str(), foo.c_str());
    OIIO_CHECK_EQUAL(hfoo.length(), foo.length());
    OIIO_CHECK_EQUAL(hfoo.size(), foo.size());

    // Check ==, != with strings, and with ustring's
    OIIO_CHECK_EQUAL(hfoo, "foo");
    OIIO_CHECK_NE(hbar, "foo");
    // OIIO_CHECK_EQUAL(hfoo, std::string("foo"));
    // OIIO_CHECK_NE(hbar, std::string("foo"));
    OIIO_CHECK_EQUAL(hfoo, foo);
    OIIO_CHECK_NE(hbar, foo);

    // Conversion to string
    OIIO_CHECK_EQUAL(Strutil::to_string(hfoo), "foo");

    // from_hash
    OIIO_CHECK_EQUAL(ustringhash::from_hash(hfoo.hash()), hfoo);
    OIIO_CHECK_EQUAL(ustringhash("").hash(), 0);
    OIIO_CHECK_EQUAL(ustringhash().hash(), 0);

    // std::hash
    OIIO_CHECK_EQUAL(std::hash<ustringhash> {}(hfoo), hfoo.hash());

    // formatting string
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", hfoo), "foo");

    // string literals
    auto whichtype = "foo"_ush;
    OIIO_CHECK_EQUAL(whichtype, ustringhash("foo"));
    OIIO_CHECK_ASSERT((std::is_same<decltype(whichtype), ustringhash>::value));
    OIIO_CHECK_ASSERT(!(std::is_same<decltype(whichtype), const char*>::value));
}



static void
create_lotso_ustrings(int iterations)
{
    OIIO_DASSERT(size_t(iterations) <= strings.size());
    if (verbose)
        Strutil::print("thread {}\n", std::this_thread::get_id());
    size_t h = 0;
    for (int i = 0; i < iterations; ++i) {
        ustring s(strings[i].data());
        h += s.hash();
    }
    if (verbose)
        Strutil::printf("checksum %08x\n", unsigned(h));
}



void
benchmark_threaded_ustring_creation()
{
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
}



void
verify_no_collisions()
{
    // Try to force a hash collision
    parallel_for(int64_t(0), int64_t(1000000LL * int64_t(collide)),
                 [](int64_t i) { (void)ustring::fmtformat("{:x}", i); });
    std::vector<ustring> collisions;
    size_t ncollisions = ustring::hash_collisions(&collisions);
    OIIO_CHECK_ASSERT(ncollisions == 0);
    if (ncollisions) {
        Strutil::print("  Hash collisions: {}\n", ncollisions);
        for (auto c : collisions)
            Strutil::print("    \"{}\" (orig {:08x} rehashed {:08x})\n", c,
                           Strutil::strhash(c), c.hash());
    }
}



int
main(int argc, char* argv[])
{
    getargs(argc, argv);

    test_ustring();
    test_ustringhash();
    verify_no_collisions();
    benchmark_threaded_ustring_creation();
    verify_no_collisions();

    std::cout << "\n" << ustring::getstats(true) << "\n";

    return unit_test_failures;
}
