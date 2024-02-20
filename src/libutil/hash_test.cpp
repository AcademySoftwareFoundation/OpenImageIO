// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdio>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>

#include <OpenImageIO/detail/farmhash.h>



using namespace OIIO;
using OIIO::Strutil::print;

static int iterations = 100 << 20;
static int ntrials    = 1;
static bool verbose   = false;

static std::vector<uint32_t> data;

uint64_t
test_bjhash(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += bjhash::hashlittle(ptr, len);
    return a;
}

uint64_t
test_xxhash(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += xxhash::xxhash(ptr, len, 0);
    return a;
}

uint64_t
test_farmhash(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += farmhash::Hash(ptr, len);
    return a;
}

uint64_t
test_farmhash_inlined(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += farmhash::inlined::Hash(ptr, len);
    return a;
}

uint64_t
test_fasthash64(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += fasthash::fasthash64(ptr, len, 0);
    return a;
}

#ifdef __AES__

// https://github.com/gamozolabs/falkhash

// Licensed with the unlicense ( http://choosealicense.com/licenses/unlicense/ )

inline uint64_t
falkhash(const void* pbuf, uint64_t len, uint64_t pseed = 0)
{
    uint8_t* buf = (uint8_t*)pbuf;

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
        piece[0] = _mm_loadu_si128((__m128i*)(buf + 0 * 0x10));
        piece[1] = _mm_loadu_si128((__m128i*)(buf + 1 * 0x10));
        piece[2] = _mm_loadu_si128((__m128i*)(buf + 2 * 0x10));
        piece[3] = _mm_loadu_si128((__m128i*)(buf + 3 * 0x10));
        piece[4] = _mm_loadu_si128((__m128i*)(buf + 4 * 0x10));

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

uint64_t
test_falkhash(int len)
{
    char* ptr  = reinterpret_cast<char*>(data.data());
    uint64_t a = 0;
    for (int i = 0, e = iterations / len; i < e; i++, ptr += len)
        a += falkhash(ptr, len, 0);
    return a;
}
#endif



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("hash_test\n" OIIO_INTRO_STRING)
      .usage("hash_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    // fill data with random values so we can hash it a bunch of different ways
    std::mt19937 rng(42);
    data.resize(iterations / sizeof(data[0]), 0);
    for (uint32_t& d : data)
        d = rng();

    print("All times are seconds per {}\n", Strutil::memformat(iterations));

    // a sampling of sizes, both tiny and large-ish
    int hashlen[] = {
        1,         2,   4,   8,    12,
        16,        20,  24,  32,   64,  // small to medium
        3,         5,   6,   7,    13,
        15,        19,  23,  49,   67,    // small to medium - odd sizes
        128,       256, 512, 1024,        // large (even)
        95,        155, 243, 501,  1337,  // large (odd sizes)
        iterations                        // huge
    };

    // present results from smallest to largest
    std::sort(std::begin(hashlen), std::end(hashlen));

    auto candidates = {
        std::make_pair("BJ hash           ", test_bjhash),
        std::make_pair("XX hash           ", test_xxhash),
        std::make_pair("farmhash          ", test_farmhash),
        std::make_pair("farmhash::inlined ", test_farmhash_inlined),
        std::make_pair("fasthash64        ", test_fasthash64),
#ifdef __AES__
        std::make_pair("falkhash          ", test_falkhash),
#endif
    };

    for (int len : hashlen) {
        auto mem = Strutil::memformat(len, 2);
        print("\nHash benchmark for {} hashes\n", mem);

        double best_time = std::numeric_limits<double>::max();
        const char* best = "";
        for (auto&& c : candidates) {
            double range;
            double t = time_trial(std::bind(c.second, len), ntrials, 1, &range);
            print("  {} took {} (range {})\n", c.first,
                  Strutil::timeintervalformat(t, 3),
                  Strutil::timeintervalformat(range, 3));
            if (t < best_time) {
                best_time = t;
                best      = c.first;
            }
        }

        print("{} winner: {}\n", mem, best);
    }

    print("\nTesting correctness\n");
    using hashfn_t = uint64_t(string_view);
    auto hashes    = {
        std::make_pair<string_view, hashfn_t*>("BJ hash           ",
                                               [](string_view s) -> uint64_t {
                                                   return bjhash::strhash(s);
                                               }),
        std::make_pair<string_view, hashfn_t*>("XX hash           ",
                                               [](string_view s) -> uint64_t {
                                                   return xxhash::xxhash(s);
                                               }),
        std::make_pair<string_view, hashfn_t*>("farmhash          ",
                                               [](string_view s) -> uint64_t {
                                                   return farmhash::Hash(s);
                                               }),
        std::make_pair<string_view, hashfn_t*>(
            "farmhash::inlined ",
            [](string_view s) -> uint64_t {
                return farmhash::inlined::Hash(s.data(), s.size());
            }),
        std::make_pair<string_view, hashfn_t*>("fasthash64        ",
                                               [](string_view s) -> uint64_t {
                                                   return fasthash::fasthash64(
                                                       s.data(), s.size());
                                               }),
#ifdef __AES__
        std::make_pair<string_view, hashfn_t*>("falkhash          ",
                                               [](string_view s) -> uint64_t {
                                                   return falkhash(s.data(),
                                                                   s.size());
                                               }),
#endif
    };
    const char* teststrings[]
        = { "",                  // empty string
            "P",                 // one-char string
            "openimageio_2008",  // identifier-length string
            "/shots/abc/seq42/tex/my_texture/my_texture_acscg.0042.tx" };
    uint64_t expected[][4] = {
        {
            // bjhash
            0x0000000000000000,
            0x00000000b7656eb4,
            0x0000000055af8ab5,
            0x00000000c000c946,
        },
        {
            // xxhash
            0x03b139605dc5b187,
            0xa4820414c8aff387,
            0x4465cf017b51e76b,
            0x1c9ebf5ebae6e8ad,
        },
        {
            // farmhash
            0x9ae16a3b2f90404f,
            0x5b5dffc690bdcd30,
            0x0dd8ef814e8a4317,
            0x43ad136c828d5214,
        },
        {
            // farmhash::inlined
            0x9ae16a3b2f90404f,
            0x5b5dffc690bdcd30,
            0x0dd8ef814e8a4317,
            0x43ad136c828d5214,
        },
        {
            // fasthash64
            0x5b38e9e25863460c,
            0x38951d1ac28aad44,
            0x91271089669c4608,
            0xc66714c1deabacf0,
        },
        {
            // falkhash
            0xaa7f7a3188504dd7,
            0x8bae7d7501558eeb,
            0x0af667ed264008a1,
            0x25f0142ed7151208,
        },
    };

    int stringno = 0;
    for (string_view s : teststrings) {
        print(" Hash testing '{}'\n", s);
        int hashno = 0;
        for (auto&& h : hashes) {
            auto val = (*h.second)(s);
            print("  {}  {:016x}\n", h.first, val);
            OIIO_CHECK_EQUAL(val, expected[hashno][stringno]);
            ++hashno;
        }
        ++stringno;
    }

    return unit_test_failures;
}
