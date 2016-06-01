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


#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include <boost/bind.hpp>

#include "OpenImageIO/hash.h"
#include "OpenImageIO/timer.h"
#include "OpenImageIO/argparse.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/unittest.h"



OIIO_NAMESPACE_USING;

static int iterations = 100000000;
static int ntrials = 1;
static bool verbose = false;

static const int shortlength = 5;
static const int medlength = 50;
static const int longlength = 501;  // Purposely not multiple of 4
static std::string shortstring (shortlength,char('a'));
static std::string medstring (medlength,char('a'));
static std::string longstring (longlength,char('a'));
static std::vector<uint32_t> data (1000,42);
size_t dummy = 0;


size_t
test_bjhash_small ()
{
    void *ptr = &data[0];
    int len = 2*sizeof(int);
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashlittle (ptr, len);
    return a;
}



size_t
test_bjhash_big ()
{
    void *ptr = &data[0];
    int len = (int)(data.size() * sizeof(int));
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashlittle (ptr, len);
    return a;
}



size_t
test_bjhash_small_words ()
{
    uint32_t *ptr = &data[0];
    int len = 2*sizeof(int);
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashword (ptr, len/sizeof(int));
    return a;
}



size_t
test_bjhash_big_words ()
{
    uint32_t *ptr = &data[0];
    int len = (int)(data.size() * sizeof(int));
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashword (ptr, len/sizeof(int));
    return a;
}



size_t
test_xxhash (const void *ptr, size_t len)
{
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += xxhash::xxhash (ptr, len);
    return a;
}



size_t
test_bjstrhash (const std::string &str)
{
    int len = int(str.length());
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i) {
        a += bjhash::strhash (str);
        dummy = a;
    }
    return a;
}



size_t
test_farmhashstr (const std::string &str)
{
    int len = int(str.length());
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i) {
        a += farmhash::Hash (str);
        // dummy = a;
    }
    return a;
}



size_t
test_farmhashchar (const char *str)
{
    int len = strlen(str);
    size_t a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i) {
        a += farmhash::Hash (str, strlen(str));
        // dummy = a;
    }
    return a;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("hash_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  hash_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--iters %d", &iterations,
                    Strutil::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
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

    double t;

    std::cout << "All times are seconds per " << iterations << " bytes.\n\n";

    t = time_trial (test_bjhash_small, ntrials);
    std::cout << "BJ hash of small data as bytes: " 
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (test_bjhash_small_words, ntrials);
    std::cout << "BJ hash of small data as words: " 
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (test_bjhash_big, ntrials);
    std::cout << "BJ hash of big data: " 
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (test_bjhash_big_words, ntrials);
    std::cout << "BJ hash of big data as words: " 
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (boost::bind(test_xxhash, &data[0], 2*sizeof(data[0])), ntrials);
    std::cout << "XX hash of small data: " 
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_xxhash, &data[0], data.size()*sizeof(data[0])), ntrials);
    std::cout << "XX hash of big data: " 
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (boost::bind(test_bjstrhash, shortstring), ntrials);
    std::cout << "BJ strhash hash of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_bjstrhash, medstring), ntrials);
    std::cout << "BJ strhash hash of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_bjstrhash, longstring), ntrials);
    std::cout << "BJ strhash hash of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (boost::bind(test_farmhashstr, shortstring), ntrials);
    std::cout << "farmhash of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_farmhashstr, medstring), ntrials);
    std::cout << "farmhash of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_farmhashstr, longstring), ntrials);
    std::cout << "farmhash of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (boost::bind(test_farmhashchar, shortstring.c_str()), ntrials);
    std::cout << "farmhash of short char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_farmhashchar, medstring.c_str()), ntrials);
    std::cout << "farmhash of medium char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_farmhashchar, longstring.c_str()), ntrials);
    std::cout << "farmhash of long char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (boost::bind(test_xxhash, shortstring.c_str(), shortstring.length()), ntrials);
    std::cout << "xxhash XH64 of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_xxhash, medstring.c_str(), medstring.length()), ntrials);
    std::cout << "xxhash XH64 of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (boost::bind(test_xxhash, longstring.c_str(), longstring.length()), ntrials);
    std::cout << "xxhash XH64 of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    return unit_test_failures;
}
