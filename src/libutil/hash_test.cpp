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

#include "hash.h"
#include "timer.h"
#include "argparse.h"
#include "strutil.h"
#include "unittest.h"



OIIO_NAMESPACE_USING;

static int iterations = 1000000000;
static int ntrials = 1;
static bool verbose = false;

static std::string shortstring ("abcde");
static std::string longstring (1000,char('a'));
static std::vector<uint32_t> data (1000,42);

int
test_bjhash_small ()
{
    void *ptr = &data[0];
    int len = 2*sizeof(int);
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashlittle (ptr, len);
    return a;
}



int
test_bjhash_big ()
{
    void *ptr = &data[0];
    int len = (int)(data.size() * sizeof(int));
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashlittle (ptr, len);
    return a;
}



int
test_bjhash_small_words ()
{
    uint32_t *ptr = &data[0];
    int len = 2*sizeof(int);
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashword (ptr, len/sizeof(int));
    return a;
}



int
test_bjhash_big_words ()
{
    uint32_t *ptr = &data[0];
    int len = (int)(data.size() * sizeof(int));
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += bjhash::hashword (ptr, len/sizeof(int));
    return a;
}



int
test_xxhash_small ()
{
    void *ptr = &data[0];
    int len = 2*sizeof(int);
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += xxhash::XXH_fast32 (ptr, len);
    return a;
}



int
test_xxhash_big ()
{
    void *ptr = &data[0];
    int len = (int)(data.size() * sizeof(int));
    int a = 0;
    for (int i = 0, e = iterations/len;  i < e;  ++i)
        a += xxhash::XXH_fast32 (ptr, len);
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
    getargs (argc, argv);

    double t;

    t = time_trial (test_bjhash_small, ntrials);
    std::cout << "BJ hash of small data as bytes: " 
              << Strutil::timeintervalformat(t, 3) << "\n";
    t = time_trial (test_bjhash_small_words, ntrials);
    std::cout << "BJ hash of small data as words: " 
              << Strutil::timeintervalformat(t, 3) << "\n";
    t = time_trial (test_bjhash_big, ntrials);
    std::cout << "BJ hash of big data: " 
              << Strutil::timeintervalformat(t, 3) << "\n";
    t = time_trial (test_bjhash_big_words, ntrials);
    std::cout << "BJ hash of big data as words: " 
              << Strutil::timeintervalformat(t, 3) << "\n";

    t = time_trial (test_xxhash_small, ntrials);
    std::cout << "XX hash of small data: " 
              << Strutil::timeintervalformat(t, 3) << "\n";
    t = time_trial (test_xxhash_big, ntrials);
    std::cout << "XX hash of big data: " 
              << Strutil::timeintervalformat(t, 3) << "\n";

    return unit_test_failures;
}
