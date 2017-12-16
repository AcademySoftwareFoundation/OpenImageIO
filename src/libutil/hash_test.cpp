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


#include <cstdio>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <OpenImageIO/hash.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unittest.h>



using namespace OIIO;

static int iterations = 100 << 20;
static int ntrials = 1;
static bool verbose = false;

static std::vector<uint32_t> data;

size_t
test_bjhash (int len)
{
    char* ptr = reinterpret_cast<char*>(data.data());
    size_t a = 0;
    for (int i = 0, e = iterations / len;  i < e; i++, ptr += len)
        a += bjhash::hashlittle(ptr, len);
    return a;
}

size_t
test_xxhash (int len)
{
    char* ptr = reinterpret_cast<char*>(data.data());
    size_t a = 0;
    for (int i = 0, e = iterations / len;  i < e; i++, ptr += len)
        a += xxhash::xxhash (ptr, len, 0);
    return a;
}

size_t
test_farmhash (int len)
{
    char* ptr = reinterpret_cast<char*>(data.data());
    size_t a = 0;
    for (int i = 0, e = iterations / len;  i < e; i++, ptr += len)
        a += farmhash::Hash (ptr, len);
    return a;
}

size_t
test_fasthash64 (int len)
{
    char* ptr = reinterpret_cast<char*>(data.data());
    size_t a = 0;
    for (int i = 0, e = iterations / len;  i < e; i++, ptr += len)
        a += fasthash::fasthash64 (ptr, len, 0);
    return a;
}

#ifdef __AES__

// https://github.com/gamozolabs/falkhash

// Licensed with the unlicense ( http://choosealicense.com/licenses/unlicense/ )

inline uint64_t falkhash(
                void     *pbuf,
                uint64_t  len,
                uint64_t  pseed = 0)
{
    uint8_t *buf = (uint8_t*)pbuf;

    uint64_t iv[2];

    __m128i hash, seed;

    /* Create the 128-bit seed. Low 64-bits gets seed, high 64-bits gets
     * seed + len + 1. The +1 ensures that both 64-bits values will never be
     * the same (with the exception of a length of -1. If you have that much
     * ram, send me some).
     */
    iv[0] = pseed;
    iv[1] = pseed + len + 1;

    /* Load the IV into a __m128i */
    seed = _mm_loadu_si128((__m128i*)iv);

    /* Hash starts out with the seed */
    hash = seed;

    while (len) {
        uint8_t tmp[0x50];

        __m128i piece[5];

        /* If the data is smaller than one chunk, pad it with zeros */
        if (len < 0x50) {
            memset(tmp, 0, 0x50);
            for (int i = 0; i < int(len); i++)
                tmp[i] = buf[i];
            buf = tmp;
            len = 0x50;
        }

        /* Load up the data into __m128is */
        piece[0] = _mm_loadu_si128((__m128i*)(buf + 0*0x10));
        piece[1] = _mm_loadu_si128((__m128i*)(buf + 1*0x10));
        piece[2] = _mm_loadu_si128((__m128i*)(buf + 2*0x10));
        piece[3] = _mm_loadu_si128((__m128i*)(buf + 3*0x10));
        piece[4] = _mm_loadu_si128((__m128i*)(buf + 4*0x10));

        /* xor each piece against the seed */
        piece[0] = _mm_xor_si128(piece[0], seed);
        piece[1] = _mm_xor_si128(piece[1], seed);
        piece[2] = _mm_xor_si128(piece[2], seed);
        piece[3] = _mm_xor_si128(piece[3], seed);
        piece[4] = _mm_xor_si128(piece[4], seed);

        /* aesenc all into piece[0] */
        piece[0] = _mm_aesenc_si128(piece[0], piece[1]);
        piece[0] = _mm_aesenc_si128(piece[0], piece[2]);
        piece[0] = _mm_aesenc_si128(piece[0], piece[3]);
        piece[0] = _mm_aesenc_si128(piece[0], piece[4]);

        /* Finalize piece[0] by aesencing against seed */
        piece[0] = _mm_aesenc_si128(piece[0], seed);

        /* aesenc the piece into the hash */
        hash = _mm_aesenc_si128(hash, piece[0]);

        buf += 0x50;
        len -= 0x50;
    }

    /* Finalize hash by aesencing against seed four times */
    hash = _mm_aesenc_si128(hash, seed);
    hash = _mm_aesenc_si128(hash, seed);
    hash = _mm_aesenc_si128(hash, seed);
    hash = _mm_aesenc_si128(hash, seed);

    return _mm_cvtsi128_si64(hash);
}

size_t
test_falkhash (int len)
{
    char* ptr = reinterpret_cast<char*>(data.data());
    size_t a = 0;
    for (int i = 0, e = iterations / len;  i < e; i++, ptr += len)
        a += falkhash (ptr, len, 0);
    return a;
}
#endif

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
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs (argc, argv);

    // fill data with random values so we can hash it a bunch of different ways
    std::mt19937 rng(42);
    data.resize(iterations / sizeof(data[0]), 0);
    for (uint32_t& d : data)
        d = rng();

    printf("All times are seconds per %s\n", Strutil::memformat(iterations).c_str());

    // a sampling of sizes, both tiny and large-ish
    int hashlen[] = { 1, 2, 4, 8, 12, 16, 20, 24, 32, 64, // small to medium
                      3, 5, 6, 7, 13, 15, 19, 23, 49, 67, // small to medium - odd sizes
                         128, 256, 512, 1024,             // large (even)
                     95, 155, 243, 501, 1337,             // large (odd sizes)
                         iterations                       // huge
    };

    // present results from smallest to largest
    std::sort(std::begin(hashlen), std::end(hashlen));

    auto candidates = {
        std::make_pair(test_bjhash       , "BJ hash      "),
        std::make_pair(test_xxhash       , "XX hash      "),
        std::make_pair(test_farmhash     , "farmhash     "),
        std::make_pair(test_fasthash64   , "fasthash64   "),
#ifdef __AES__
        std::make_pair(test_falkhash     , "falkhash     "),
#endif
    };

    for (int len : hashlen) {
        auto mem = Strutil::memformat(len, 2);
        printf("\nHash benchmark for %s hashes\n", mem.c_str());

        double best_time = std::numeric_limits<double>::max();
        const char* best = "";
        for (auto&& c : candidates) {
            double range;
            double t = time_trial(std::bind(c.first, len), ntrials, 1, &range);
            printf("  %s took %s (range %s)\n", c.second, Strutil::timeintervalformat(t, 3).c_str(), Strutil::timeintervalformat(range, 3).c_str());
            if (t < best_time) {
                best_time = t;
                best = c.second;
            }
        }

        printf("%s winner: %s\n", mem.c_str(), best);
        fflush(stdout);
    }

    return unit_test_failures;
}
