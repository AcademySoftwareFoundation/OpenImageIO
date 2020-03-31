// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <iostream>
#include <vector>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;

// Aid for things that are too short to benchmark accurately
#define REP10(x) x, x, x, x, x, x, x, x, x, x

static int iterations = 1000000;
static int ntrials    = 5;
static bool verbose   = false;



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("fmath_test\n" OIIO_INTRO_STRING)
      .usage("fmath_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--iters %d", &iterations)
      .help(Strutil::sprintf("Number of iterations (default: %d)", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



void
test_int_helpers()
{
    std::cout << "\ntest_int_helpers\n";

    // ispow2
    for (int i = 1; i < (1 << 30); i *= 2) {
        OIIO_CHECK_ASSERT(ispow2(i));
        if (i > 1)
            OIIO_CHECK_ASSERT(!ispow2(i + 1));
    }
    OIIO_CHECK_ASSERT(ispow2(int(0)));
    OIIO_CHECK_ASSERT(!ispow2(-1));
    OIIO_CHECK_ASSERT(!ispow2(-2));

    // ispow2, try size_t, which is unsigned
    for (size_t i = 1; i < (1 << 30); i *= 2) {
        OIIO_CHECK_ASSERT(ispow2(i));
        if (i > 1)
            OIIO_CHECK_ASSERT(!ispow2(i + 1));
    }
    OIIO_CHECK_ASSERT(ispow2((unsigned int)0));

    // ceil2
    OIIO_CHECK_EQUAL(ceil2(4), 4);
    OIIO_CHECK_EQUAL(ceil2(5), 8);
    OIIO_CHECK_EQUAL(ceil2(6), 8);
    OIIO_CHECK_EQUAL(ceil2(7), 8);
    OIIO_CHECK_EQUAL(ceil2(8), 8);

    // floor2
    OIIO_CHECK_EQUAL(floor2(4), 4);
    OIIO_CHECK_EQUAL(floor2(5), 4);
    OIIO_CHECK_EQUAL(floor2(6), 4);
    OIIO_CHECK_EQUAL(floor2(7), 4);
    OIIO_CHECK_EQUAL(floor2(8), 8);

    // round_to_multiple
    OIIO_CHECK_EQUAL(round_to_multiple(0, 5), 0);
    OIIO_CHECK_EQUAL(round_to_multiple(1, 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(2, 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(3, 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(4, 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(5, 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(6, 5), 10);
    OIIO_CHECK_EQUAL(round_to_multiple(size_t(5), 5), 5);
    OIIO_CHECK_EQUAL(round_to_multiple(size_t(6), 5), 10);

    // round_to_multiple_of_pow2
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(int(1), 4), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(int(2), 4), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(int(3), 4), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(int(4), 4), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(int(5), 4), 8);

    // round_to_multiple_of_pow2
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(size_t(1), size_t(4)), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(size_t(2), size_t(4)), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(size_t(3), size_t(4)), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(size_t(4), size_t(4)), 4);
    OIIO_CHECK_EQUAL(round_to_multiple_of_pow2(size_t(5), size_t(4)), 8);

    OIIO_CHECK_EQUAL(rotl(uint32_t(0x12345678), 4), uint32_t(0x23456781));
    OIIO_CHECK_EQUAL(rotl(uint64_t(0x123456789abcdef0ULL), 4),
                     uint64_t(0x23456789abcdef01ULL));
}



void
test_math_functions()
{
    std::cout << "Testing math functions\n";
    Benchmarker bench;

    OIIO_CHECK_EQUAL(ifloor(0.0f), 0);
    OIIO_CHECK_EQUAL(ifloor(-0.999f), -1);
    OIIO_CHECK_EQUAL(ifloor(-1.0f), -1);
    OIIO_CHECK_EQUAL(ifloor(-1.001f), -2);
    OIIO_CHECK_EQUAL(ifloor(0.999f), 0);
    OIIO_CHECK_EQUAL(ifloor(1.0f), 1);
    OIIO_CHECK_EQUAL(ifloor(1.001f), 1);
    float fval = 1.1f;
    clobber(fval);
    bench("ifloor", [&]() { return DoNotOptimize(ifloor(fval)); });
    fval = -1.1f;
    clobber(fval);
    bench("ifloor (neg)", [&]() { return DoNotOptimize(ifloor(fval)); });

    int ival;
    OIIO_CHECK_EQUAL_APPROX(floorfrac(0.0f, &ival), 0.0f);
    OIIO_CHECK_EQUAL(ival, 0);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(-0.999f, &ival), 0.001f);
    OIIO_CHECK_EQUAL(ival, -1);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(-1.0f, &ival), 0.0f);
    OIIO_CHECK_EQUAL(ival, -1);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(-1.001f, &ival), 0.999f);
    OIIO_CHECK_EQUAL(ival, -2);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(0.999f, &ival), 0.999f);
    OIIO_CHECK_EQUAL(ival, 0);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(1.0f, &ival), 0.0f);
    OIIO_CHECK_EQUAL(ival, 1);
    OIIO_CHECK_EQUAL_APPROX(floorfrac(1.001f, &ival), 0.001f);
    OIIO_CHECK_EQUAL(ival, 1);
    bench(
        "floorfrac",
        [&](float x) { return DoNotOptimize(floorfrac(x, &ival)); }, fval);

    OIIO_CHECK_EQUAL(sign(3.1f), 1.0f);
    OIIO_CHECK_EQUAL(sign(-3.1f), -1.0f);
    OIIO_CHECK_EQUAL(sign(0.0f), 0.0f);

    {
        OIIO_CHECK_EQUAL(fast_neg(1.5f), -1.5f);
        OIIO_CHECK_EQUAL(fast_neg(-1.5f), 1.5f);
        OIIO_CHECK_EQUAL(fast_neg(0.0f), 0.0f);
        OIIO_CHECK_EQUAL(fast_neg(-0.0f), 0.0f);
        float x = -3.5f;
        clobber(x);
        bench("-float x10", [&]() { return REP10(DoNotOptimize(-x)); });
        bench("fast_neg(float) x10",
              [&]() { return REP10(DoNotOptimize(fast_neg(x))); });
    }

    {
        float a = 2.5f, b = 1.5f, c = 8.5f;
        clobber(a);
        clobber(b);
        clobber(c);
        bench("madd fake a*b+c", [&]() { return DoNotOptimize(a * b + c); });
        bench("madd(a,b,c)",
              [&]() { return DoNotOptimize(OIIO::madd(a, b, c)); });
        bench("std::fma(a,b,c)",
              [&]() { return DoNotOptimize(std::fma(a, b, c)); });
    }
    {
        float a = 2.5f, b = 1.5f, c = 8.5f;
        OIIO_CHECK_EQUAL(clamp(2.5f, 1.5f, 8.5f), 2.5f);
        OIIO_CHECK_EQUAL(clamp(1.5f, 2.5f, 8.5f), 2.5f);
        OIIO_CHECK_EQUAL(clamp(8.5f, 1.5f, 2.5f), 2.5f);
        clobber(a);
        clobber(b);
        clobber(c);
        bench("clamp(f,f,f) middle",
              [&]() { return DoNotOptimize(clamp(a, b, c)); });
        bench("clamp(f,f,f) low",
              [&]() { return DoNotOptimize(clamp(b, a, c)); });
        bench("clamp(f,f,f) high",
              [&]() { return DoNotOptimize(clamp(c, b, a)); });
    }

    {
        float x = 1.3f, y = 2.5f;
        clobber(x, y);
        bench("std::cos", [&]() { return DoNotOptimize(std::cos(x)); });
        bench("fast_cos", [&]() { return DoNotOptimize(fast_cos(x)); });
        bench("fast_cospi", [&]() { return DoNotOptimize(fast_cospi(x)); });
        bench("std::sin", [&]() { return DoNotOptimize(std::sin(x)); });
        bench("fast_sin", [&]() { return DoNotOptimize(fast_sin(x)); });
        bench("fast_sinpi", [&]() { return DoNotOptimize(fast_sinpi(x)); });
        bench("std::tan", [&]() { return DoNotOptimize(std::tan(x)); });
        bench("fast_tan", [&]() { return DoNotOptimize(fast_tan(x)); });
        bench("std::acos", [&]() { return DoNotOptimize(std::acos(x)); });
        bench("fast_acos", [&]() { return DoNotOptimize(fast_acos(x)); });
        bench("std::asin", [&]() { return DoNotOptimize(std::asin(x)); });
        bench("fast_asin", [&]() { return DoNotOptimize(fast_asin(x)); });
        bench("std::atan2", [&]() { return DoNotOptimize(std::atan2(y, x)); });
        bench("fast_atan2", [&]() { return DoNotOptimize(fast_atan2(y, x)); });

        bench("std::log2", [&]() { return DoNotOptimize(std::log2(x)); });
        bench("fast_log2", [&]() { return DoNotOptimize(fast_log2(x)); });
        bench("std::log", [&]() { return DoNotOptimize(std::log(x)); });
        bench("fast_log", [&]() { return DoNotOptimize(fast_log(x)); });
        bench("std::log10", [&]() { return DoNotOptimize(std::log10(x)); });
        bench("fast_log10", [&]() { return DoNotOptimize(fast_log10(x)); });
        bench("std::exp", [&]() { return DoNotOptimize(std::exp(x)); });
        bench("fast_exp", [&]() { return DoNotOptimize(fast_exp(x)); });
        bench("fast_correct_exp",
              [&]() { return DoNotOptimize(fast_correct_exp(x)); });
        bench("std::exp2", [&]() { return DoNotOptimize(std::exp2(x)); });
        bench("fast_exp2", [&]() { return DoNotOptimize(fast_exp2(x)); });

        OIIO_CHECK_EQUAL(safe_fmod(5.0f, 2.5f), 0.0f);
        OIIO_CHECK_EQUAL(safe_fmod(-5.0f, 2.5f), 0.0f);
        OIIO_CHECK_EQUAL(safe_fmod(-5.0f, -2.5f), 0.0f);
        OIIO_CHECK_EQUAL(safe_fmod(5.5f, 2.5f), 0.5f);
        OIIO_CHECK_EQUAL(safe_fmod(-5.5f, 2.5f), -0.5f);
        OIIO_CHECK_EQUAL(safe_fmod(-5.5f, -2.5f), -0.5f);
        OIIO_CHECK_EQUAL(safe_fmod(5.5f, 0.0f), 0.0f);
        bench("std::fmod", [&]() { return DoNotOptimize(std::fmod(y, x)); });
        bench("safe_fmod", [&]() { return DoNotOptimize(safe_fmod(y, x)); });
    }

    {
        OIIO_CHECK_EQUAL(fast_rint(0.0f), 0);
        OIIO_CHECK_EQUAL(fast_rint(-1.0f), -1);
        OIIO_CHECK_EQUAL(fast_rint(-1.2f), -1);
        OIIO_CHECK_EQUAL(fast_rint(-0.8f), -1);
        OIIO_CHECK_EQUAL(fast_rint(-1.49f), -1);
        OIIO_CHECK_EQUAL(fast_rint(-1.50f), -2);
        OIIO_CHECK_EQUAL(fast_rint(-1.51f), -2);
        OIIO_CHECK_EQUAL(fast_rint(1.0f), 1);
        OIIO_CHECK_EQUAL(fast_rint(1.2f), 1);
        OIIO_CHECK_EQUAL(fast_rint(0.8f), 1);
        OIIO_CHECK_EQUAL(fast_rint(1.49f), 1);
        OIIO_CHECK_EQUAL(fast_rint(1.50f), 2);
        OIIO_CHECK_EQUAL(fast_rint(1.51f), 2);
        float a = 1.5f;
        clobber(a);
        bench("fast_rint", [&]() { return DoNotOptimize(fast_rint(a)); });
        bench("std::lrint", [&]() { return DoNotOptimize(std::lrint(a)); });
        bench("int(std::rint)",
              [&]() { return DoNotOptimize(static_cast<int>(std::rint(a))); });
        bench("int(x+copysignf(0.5f,x))", [&]() {
            return DoNotOptimize(static_cast<int>(a + copysignf(0.5f, a)));
        });
    }
}



// Convert T to F to T, make sure value are preserved round trip
template<typename T, typename F>
void
test_convert_type(double tolerance = 1e-6)
{
    if (std::numeric_limits<T>::is_integer) {
        for (long long i = std::numeric_limits<T>::min();
             i <= std::numeric_limits<T>::max(); ++i) {
            T in  = (T)i;
            F f   = convert_type<T, F>(in);
            T out = convert_type<F, T>(f);
            if (out != in) {
                std::cout << "  convert " << (long long)in << " -> " << f
                          << " -> " << (long long)out << "\n";
                ++unit_test_failures;
            }
        }
    } else {
        for (float i = 0.0f; i <= 1.0f; i += 0.001) {  // NOLINT
            T in  = (T)i;
            F f   = convert_type<T, F>(in);
            T out = convert_type<F, T>(f);
            if (fabs(double(out - in)) > tolerance) {
                std::cout << "  convert " << in << " -> " << f << " -> " << out
                          << " (diff = " << (out - in) << ")\n";
                ++unit_test_failures;
            }
        }
    }
}



template<typename S, typename D>
void
do_convert_type(const std::vector<S>& svec, std::vector<D>& dvec)
{
    convert_type(&svec[0], &dvec[0], svec.size());
    DoNotOptimize(dvec[0]);  // Be sure nothing is optimized away
}


template<typename S, typename D>
void
benchmark_convert_type()
{
    const size_t repeats = 10;
    const size_t size    = iterations;
    const S testval(1.0);
    std::vector<S> svec(size, testval);
    std::vector<D> dvec(size);
    Strutil::printf("Benchmark conversion of %6s -> %6s : ",
                    TypeDesc(BaseTypeFromC<S>::value),
                    TypeDesc(BaseTypeFromC<D>::value));
    float time = time_trial(bind(do_convert_type<S, D>, std::cref(svec),
                                 std::ref(dvec)),
                            ntrials, repeats)
                 / repeats;
    Strutil::printf("%7.1f Mvals/sec\n", (size / 1.0e6) / time);
    D r = convert_type<S, D>(testval);
    OIIO_CHECK_EQUAL(dvec[size - 1], r);
}



void
test_bit_range_convert()
{
    OIIO_CHECK_EQUAL((bit_range_convert<10, 16>(1023)), 65535);
    OIIO_CHECK_EQUAL((bit_range_convert<2, 8>(3)), 255);
    OIIO_CHECK_EQUAL((bit_range_convert<8, 8>(255)), 255);
    OIIO_CHECK_EQUAL((bit_range_convert<16, 10>(65535)), 1023);
    OIIO_CHECK_EQUAL((bit_range_convert<2, 20>(3)), 1048575);
    OIIO_CHECK_EQUAL((bit_range_convert<20, 2>(1048575)), 3);
    OIIO_CHECK_EQUAL((bit_range_convert<20, 21>(1048575)), 2097151);
    OIIO_CHECK_EQUAL((bit_range_convert<32, 32>(4294967295U)), 4294967295U);
    OIIO_CHECK_EQUAL((bit_range_convert<32, 16>(4294967295U)), 65535);
    // These are not expected to work, since bit_range_convert only takes a
    // regular 'unsigned int' as parameter.  If we need >32 bit conversion,
    // we need to add a uint64_t version of bit_range_convert.
    //    OIIO_CHECK_EQUAL ((bit_range_convert<33,16>(8589934591)), 65535);
    //    OIIO_CHECK_EQUAL ((bit_range_convert<33,33>(8589934591)), 8589934591);
    //    OIIO_CHECK_EQUAL ((bit_range_convert<64,32>(18446744073709551615)), 4294967295);
}



void
test_packbits()
{
    std::cout << "test_convert_pack_bits\n";

    {
        unsigned char foo[3] = { 0, 0, 0 };
        unsigned char* fp    = foo;
        int fpf              = 0;
        bitstring_add_n_bits(fp, fpf, 1, 4);
        bitstring_add_n_bits(fp, fpf, 2, 8);
        bitstring_add_n_bits(fp, fpf, 0xffff, 10);
        // result should be 0x10 0x2f 0xfc
        Strutil::printf("  bitstring_add_n_bits results %02x %02x %02x\n",
                        foo[0], foo[1], foo[2]);
        OIIO_CHECK_EQUAL(foo[0], 0x10);
        OIIO_CHECK_EQUAL(foo[1], 0x2f);
        OIIO_CHECK_EQUAL(foo[2], 0xfc);
    }
    {
        unsigned char foo[4] = { 0, 0, 0, 0 };
        unsigned char* fp    = foo;
        int fpf              = 0;
        bitstring_add_n_bits(fp, fpf, 1023, 10);
        bitstring_add_n_bits(fp, fpf, 0, 10);
        bitstring_add_n_bits(fp, fpf, 1023, 10);
        // result should be 1111111111 0000000000 1111111111 00
        //                     f   f    c   0   0    f   f    c
        Strutil::printf("  bitstring_add_n_bits results %02x %02x %02x %02x\n",
                        foo[0], foo[1], foo[2], foo[3]);
        OIIO_CHECK_EQUAL(foo[0], 0xff);
        OIIO_CHECK_EQUAL(foo[1], 0xc0);
        OIIO_CHECK_EQUAL(foo[2], 0x0f);
        OIIO_CHECK_EQUAL(foo[3], 0xfc);
    }

    const uint16_t u16vals[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    uint16_t u10[5]           = { 255, 255, 255, 255, 255 };
    Strutil::printf(
        " in 16 bit values: %04x %04x %04x %04x %04x %04x %04x %04x\n",
        u16vals[0], u16vals[1], u16vals[2], u16vals[3], u16vals[4], u16vals[5],
        u16vals[6], u16vals[7]);
    bit_pack(cspan<uint16_t>(u16vals, 8), u10, 10);
    Strutil::printf(
        " packed to 10 bits, as 16 bit values: %04x %04x %04x %04x %04x\n",
        u10[0], u10[1], u10[2], u10[3], u10[4]);
    uint16_t u16[8];
    bit_unpack(8, (const unsigned char*)u10, 10, u16);
    Strutil::printf(
        " unpacked back to 16 bits: %04x %04x %04x %04x %04x %04x %04x %04x\n",
        u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]);
    // Before: 00000000 00000001  00000000 00000001  00000000 00000001...
    // After:  00000000 01000000  00010000 00000100  00000001 00000000  01000000 00010000  00000100 00000001
    //       =  00 40  10 04  01 00  40 10  04 01
    // as little endian 16 bit:  4000 0410 0001 1040 0104
    for (size_t i = 0; i < 8; ++i)
        OIIO_CHECK_EQUAL(u16vals[i], u16[i]);
}



static void
test_interpolate_linear()
{
    std::cout << "\nTesting interpolate_linear\n";

    // Test simple case of 2 knots
    float knots2[] = { 1.0f, 2.0f };
    OIIO_CHECK_EQUAL(interpolate_linear(0.0f, knots2), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.25f, knots2), 1.25f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.0f, knots2), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.0f, knots2), 2.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(-0.1f, knots2), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.1f, knots2), 2.0f);
    float inf = std::numeric_limits<float>::infinity();
    float nan = std::numeric_limits<float>::quiet_NaN();
    OIIO_CHECK_EQUAL(interpolate_linear(-inf, knots2), 1.0f);  // Test -inf
    OIIO_CHECK_EQUAL(interpolate_linear(inf, knots2), 2.0f);   // Test inf
    OIIO_CHECK_EQUAL(interpolate_linear(nan, knots2), 1.0f);   // Test nan

    // More complex case of many knots
    float knots4[] = { 1.0f, 2.0f, 4.0f, 6.0f };
    OIIO_CHECK_EQUAL(interpolate_linear(-0.1f, knots4), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.0f, knots4), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.0f / 3.0f, knots4), 2.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.5f, knots4), 3.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(5.0f / 6.0f, knots4), 5.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.0f, knots4), 6.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.1f, knots4), 6.0f);

    // Make sure it all works for strided arrays, too
    float knots4_strided[] = { 1.0f, 0.0f, 2.0f, 0.0f, 4.0f, 0.0f, 6.0f, 0.0f };
    span_strided<const float> a(knots4_strided, 4, 2);
    OIIO_CHECK_EQUAL(interpolate_linear(-0.1f, a), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.0f, a), 1.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.0f / 3.0f, a), 2.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(0.5f, a), 3.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(5.0f / 6.0f, a), 5.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.0f, a), 6.0f);
    OIIO_CHECK_EQUAL(interpolate_linear(1.1f, a), 6.0f);
}



inline std::string
bin16(int i)
{
    std::string out;
    for (int b = 15; b >= 0; --b) {
        out += (1 << b) & i ? '1' : '0';
        if (b == 15 || b == 10)
            out += '\'';
    }
    return out;
}



void
test_half_convert_accuracy()
{
    // Enumerate every half value
    const int nhalfs = 1 << 16;
    std::vector<half> H(nhalfs, 0.0f);
    for (auto i = 0; i < nhalfs; ++i)
        H[i] = bit_cast<unsigned short, half>((unsigned short)i);

    // Convert the whole array to float equivalents in one shot (which will
    // use SIMD ops if available).
    std::vector<float> F(nhalfs);
    convert_type(H.data(), F.data(), nhalfs);
    // And convert back in a batch as well (using SIMD if available)
    std::vector<float> H2(nhalfs);
    convert_type(F.data(), H2.data(), nhalfs);

    // Compare the round trip as well as all the values to the result we get
    // if we convert individually, which will use the table-based method
    // from Imath. They should match!
    int nwrong = 0;
    for (auto i = 0; i < nhalfs; ++i) {
        float f = H[i];  // single assignment uses table from Imath
        half h  = (half)f;
        if ((f != F[i] || f != H2[i] || f != h || H[i] != H2[i]
             || bit_cast<half, unsigned short>(h)
                    != bit_cast<half, unsigned short>(H[i])
             || bit_cast<half, unsigned short>(h) != i)
            && Imath::finitef(H[i])) {
            ++nwrong;
            Strutil::printf("wrong %d 0b%s  h=%g, f=%g %s\n", i, bin16(i), H[i],
                            F[i], isnan(f) ? "(nan)" : "");
        }
    }

    Sysutil::Term term(std::cout);
    if (nwrong)
        std::cout << term.ansi("red");
    std::cout << "test_half_convert_accuracy: " << nwrong << " mismatches\n";
    std::cout << term.ansi("default");
    OIIO_CHECK_ASSERT(nwrong == 0);
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

    test_int_helpers();

    test_math_functions();

    std::cout << "\nround trip convert char/float/char\n";
    test_convert_type<char, float>();
    std::cout << "round trip convert unsigned char/float/unsigned char\n";
    test_convert_type<unsigned char, float>();
    std::cout
        << "round trip convert unsigned char/unsigned short/unsigned char\n";
    test_convert_type<unsigned char, unsigned short>();
    std::cout << "round trip convert short/float/short\n";
    test_convert_type<short, float>();
    std::cout << "round trip convert unsigned short/float/unsigned short\n";
    test_convert_type<unsigned short, float>();
    std::cout << "round trip convert float/int/float \n";
    test_convert_type<float, int>();
    std::cout << "round trip convert double/float/double\n";
    test_convert_type<double, float>();
    std::cout << "round trip convert double/long/double\n";
    test_convert_type<double, long>();
    std::cout << "round trip convert float/unsigned int/float\n";
    test_convert_type<float, unsigned int>();

    test_half_convert_accuracy();

    benchmark_convert_type<unsigned char, float>();
    benchmark_convert_type<float, unsigned char>();
    benchmark_convert_type<unsigned short, float>();
    benchmark_convert_type<float, unsigned short>();
    benchmark_convert_type<half, float>();
    benchmark_convert_type<float, half>();
    benchmark_convert_type<float, float>();
    // convertion to a type smaller in bytes causes error
    //    std::cout << "round trip convert float/short/float\n";
    //    test_convert_type<float,short> ();
    //    std::cout << "round trip convert unsigned float/char/float\n";
    //    test_convert_type<float,char> ();
    //    std::cout << "round trip convert unsigned float/unsigned char/float\n";
    //    test_convert_type<float,unsigned char> ();
    //    std::cout << "round trip convert unsigned short/unsigned char/unsigned short\n";
    //    test_convert_type<unsigned short,unsigned char> ();
    //    std::cout << "round trip convert float/unsigned short/float\n";
    //    test_convert_type<float,unsigned short> ();

    test_bit_range_convert();
    test_packbits();

    test_interpolate_linear();

    return unit_test_failures != 0;
}
