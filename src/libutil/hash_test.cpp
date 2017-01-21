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
#include <functional>
#include <set>

#include "OpenImageIO/hash.h"
#include "OpenImageIO/timer.h"
#include "OpenImageIO/argparse.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/ustring.h"
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



template <typename V, typename T> void
test_xxpod ( T (*hash_data)(const void* input, size_t length, T seed),
             unsigned seed, V value) {
    xxhash::Hash<T> hash(seed);
    hash(value);
    OIIO_CHECK_EQUAL(hash_data(&value, sizeof(value), seed), hash.value());
    // Make sure XXH_digest doesn't change the hash state
    OIIO_CHECK_EQUAL(hash.value(), hash.value());
}



template <typename T> void
test_xxwrappers ( T (*hash_data)(const void* input, size_t length, T seed),
                  unsigned seed = sizeof(T)) {

    test_xxpod<size_t>(hash_data, seed++, 0xdeadbeef);
    test_xxpod<bool>(hash_data, seed++, true);
    test_xxpod<char>(hash_data, seed++, 'a');
    test_xxpod<int>(hash_data, seed++, -45678);

    {
        unsigned lvec[5] = { 100, 201, 302, 403, 504 };
        std::vector<unsigned> vec(&lvec[0], &lvec[0]+5);
        xxhash::Hash<T> vhash(seed);
        vhash(vec);
        OIIO_CHECK_EQUAL(hash_data(&lvec, sizeof(lvec), seed++), vhash.value());

        xxhash::Hash<T> lhash(seed);
        lhash(lvec);
        OIIO_CHECK_EQUAL(hash_data(&lvec, sizeof(lvec), seed++), lhash.value());
    }

    {
        const char lstr[] = "C_STRINGV_STRINGSTL_STRING";
        xxhash::Hash<T> shash(seed);
        shash("C_STRING");
        shash(string_view("V_STRING"));
        shash(std::string("STL_STRING"));
        OIIO_CHECK_EQUAL(hash_data(lstr, strlen(lstr), seed++), shash.value());
        
        shash.reset(++seed);
        shash(lstr);
        OIIO_CHECK_EQUAL(hash_data(lstr, strlen(lstr), seed), shash.value());
    }

    {
        const char ustr[] = "U_STRING";
        xxhash::Hash<T> uhash(seed);
        uhash(ustring("U_STRING"));
        // Optimized ustring::hash variant, not equal
        OIIO_CHECK_NE(hash_data(ustr, strlen(ustr), seed++), uhash.value());
    }

    {
        const char mstr[] = "A_KEY->VALUE0B_KEY->VALUE1";
        std::map<std::string, std::string> mpng;
        mpng["A_KEY"] = "->VALUE0";
        mpng["B_KEY"] = "->VALUE1";
        xxhash::Hash<T> mhash(seed);
        mhash.append(mpng.begin(), mpng.end());
        OIIO_CHECK_EQUAL(hash_data(mstr, strlen(mstr), seed++), mhash.value());
    }

    {
#pragma pack(push, 0)
        struct HashData {
            size_t szv[5];
            unsigned uiv1, uiv2;
            char c[8];
        };
#pragma pack(pop)
        HashData cpmplx = {
            {1, 2, 3, 4, 5},
            903894, 102837,
            {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}
        };
        std::set<size_t> lset(&cpmplx.szv[0], &cpmplx.szv[0]+5);
        xxhash::Hash<T> chash(seed);
        chash.append(lset);//.begin(), lset.end());
        chash(cpmplx.uiv1);
        chash(cpmplx.uiv2);
        chash("abcdefgh");
        OIIO_CHECK_EQUAL(hash_data(&cpmplx, sizeof(cpmplx), seed++), chash.value());

        chash.reset(++seed);
        chash(cpmplx);
        OIIO_CHECK_EQUAL(hash_data(&cpmplx, sizeof(cpmplx), seed++), chash.value());

        OIIO_CHECK_EQUAL(hash_data(&cpmplx.szv, sizeof(cpmplx.szv), seed), xxhash::Hash<T>(lset, seed).value());

    }
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

    t = time_trial (std::bind(test_xxhash, &data[0], 2*sizeof(data[0])), ntrials);
    std::cout << "XX hash of small data: " 
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_xxhash, &data[0], data.size()*sizeof(data[0])), ntrials);
    std::cout << "XX hash of big data: " 
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (std::bind(test_bjstrhash, shortstring), ntrials);
    std::cout << "BJ strhash hash of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_bjstrhash, medstring), ntrials);
    std::cout << "BJ strhash hash of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_bjstrhash, longstring), ntrials);
    std::cout << "BJ strhash hash of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (std::bind(test_farmhashstr, shortstring), ntrials);
    std::cout << "farmhash of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_farmhashstr, medstring), ntrials);
    std::cout << "farmhash of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_farmhashstr, longstring), ntrials);
    std::cout << "farmhash of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (std::bind(test_farmhashchar, shortstring.c_str()), ntrials);
    std::cout << "farmhash of short char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_farmhashchar, medstring.c_str()), ntrials);
    std::cout << "farmhash of medium char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_farmhashchar, longstring.c_str()), ntrials);
    std::cout << "farmhash of long char*: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    t = time_trial (std::bind(test_xxhash, shortstring.c_str(), shortstring.length()), ntrials);
    std::cout << "xxhash XH64 of short string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_xxhash, medstring.c_str(), medstring.length()), ntrials);
    std::cout << "xxhash XH64 of medium string: "
              << Strutil::timeintervalformat(t, 2) << "\n";
    t = time_trial (std::bind(test_xxhash, longstring.c_str(), longstring.length()), ntrials);
    std::cout << "xxhash XH64 of long string: "
              << Strutil::timeintervalformat(t, 2) << "\n";

    test_xxwrappers(xxhash::XXH32);
    test_xxwrappers(xxhash::XXH64);

    return unit_test_failures;
}
