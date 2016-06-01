/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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

#include <vector>

#include <OpenEXR/half.h>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/argparse.h>

OIIO_NAMESPACE_USING;

static int iterations = 10;
static int ntrials = 5;
static bool verbose = false;



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("fmath_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  fmath_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                // "--threads %d", &numthreads,
                //     ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iterations %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
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



void
test_int_helpers ()
{
    std::cout << "\ntest_int_helpers\n";
 
    // ispow2
    for (int i = 1; i < (1<<30); i *= 2) {
        OIIO_CHECK_ASSERT (ispow2(i));
        if (i > 1)
            OIIO_CHECK_ASSERT (! ispow2(i+1));
    }
    OIIO_CHECK_ASSERT (ispow2(int(0)));
    OIIO_CHECK_ASSERT (! ispow2(-1));
    OIIO_CHECK_ASSERT (! ispow2(-2));

    // ispow2, try size_t, which is unsigned
    for (size_t i = 1; i < (1<<30); i *= 2) {
        OIIO_CHECK_ASSERT (ispow2(i));
        if (i > 1)
            OIIO_CHECK_ASSERT (! ispow2(i+1));
    }
    OIIO_CHECK_ASSERT (ispow2((unsigned int)0));

    // pow2roundup
    OIIO_CHECK_EQUAL (pow2roundup(4), 4);
    OIIO_CHECK_EQUAL (pow2roundup(5), 8);
    OIIO_CHECK_EQUAL (pow2roundup(6), 8);
    OIIO_CHECK_EQUAL (pow2roundup(7), 8);
    OIIO_CHECK_EQUAL (pow2roundup(8), 8);

    // pow2rounddown
    OIIO_CHECK_EQUAL (pow2rounddown(4), 4);
    OIIO_CHECK_EQUAL (pow2rounddown(5), 4);
    OIIO_CHECK_EQUAL (pow2rounddown(6), 4);
    OIIO_CHECK_EQUAL (pow2rounddown(7), 4);
    OIIO_CHECK_EQUAL (pow2rounddown(8), 8);

    // round_to_multiple
    OIIO_CHECK_EQUAL (round_to_multiple(1, 5), 5);
    OIIO_CHECK_EQUAL (round_to_multiple(2, 5), 5);
    OIIO_CHECK_EQUAL (round_to_multiple(3, 5), 5);
    OIIO_CHECK_EQUAL (round_to_multiple(4, 5), 5);
    OIIO_CHECK_EQUAL (round_to_multiple(5, 5), 5);
    OIIO_CHECK_EQUAL (round_to_multiple(6, 5), 10);

    // round_to_multiple_of_pow2
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(int(1), 4), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(int(2), 4), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(int(3), 4), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(int(4), 4), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(int(5), 4), 8);

    // round_to_multiple_of_pow2
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(size_t(1), size_t(4)), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(size_t(2), size_t(4)), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(size_t(3), size_t(4)), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(size_t(4), size_t(4)), 4);
    OIIO_CHECK_EQUAL (round_to_multiple_of_pow2(size_t(5), size_t(4)), 8);
}




// Convert T to F to T, make sure value are preserved round trip
template<typename T, typename F>
void test_convert_type (double tolerance = 1e-6)
{
    if (std::numeric_limits<T>::is_integer) {
        for (long long i = std::numeric_limits<T>::min();
                 i <= std::numeric_limits<T>::max();  ++i) {
            T in = (T)i;
            F f = convert_type<T,F> (in);
            T out = convert_type<F,T> (f);
            if (out != in) {
                std::cout << "  convert " << (long long)in << " -> " << f << " -> " << (long long)out << "\n";
                ++unit_test_failures;
            }
        }
    } else {
        for (float i = 0.0f; i <= 1.0f;  i += 0.001) {
            T in = (T)i;
            F f = convert_type<T,F> (in);
            T out = convert_type<F,T> (f);
            if (fabs(double(out-in)) > tolerance) {
                std::cout << "  convert " << in << " -> " << f << " -> " << out << " (diff = " << (out-in) << ")\n";
                ++unit_test_failures;
            }
        }
    }
}



template<typename S, typename D>
void do_convert_type (const std::vector<S> &svec, std::vector<D> &dvec)
{
    convert_type (&svec[0], &dvec[0], svec.size());
    DoNotOptimize (dvec[0]);  // Be sure nothing is optimized away
}


template<typename S, typename D>
void benchmark_convert_type ()
{
    const size_t size = 10000000;
    const S testval(1.0);
    std::vector<S> svec (size, testval);
    std::vector<D> dvec (size);
    std::cout << Strutil::format("Benchmark conversion of %6s -> %6s : ",
                                 TypeDesc(BaseTypeFromC<S>::value),
                                 TypeDesc(BaseTypeFromC<D>::value));
    float time = time_trial (bind (do_convert_type<S,D>, OIIO::cref(svec), OIIO::ref(dvec)),
                             ntrials, iterations) / iterations;
    std::cout << Strutil::format ("%7.1f Mvals/sec", (size/1.0e6)/time) << std::endl;
    D r = convert_type<S,D>(testval);
    OIIO_CHECK_EQUAL (dvec[size-1], r);
}



void test_bit_range_convert ()
{
    OIIO_CHECK_EQUAL ((bit_range_convert<10,16>(1023)), 65535);
    OIIO_CHECK_EQUAL ((bit_range_convert<2,8>(3)), 255);
    OIIO_CHECK_EQUAL ((bit_range_convert<8,8>(255)), 255);
    OIIO_CHECK_EQUAL ((bit_range_convert<16,10>(65535)), 1023);
    OIIO_CHECK_EQUAL ((bit_range_convert<2,20>(3)), 1048575);
    OIIO_CHECK_EQUAL ((bit_range_convert<20,2>(1048575)), 3);
    OIIO_CHECK_EQUAL ((bit_range_convert<20,21>(1048575)), 2097151);
    OIIO_CHECK_EQUAL ((bit_range_convert<32,32>(4294967295U)), 4294967295U);
    OIIO_CHECK_EQUAL ((bit_range_convert<32,16>(4294967295U)), 65535);
// These are not expected to work, since bit_range_convert only takes a
// regular 'unsigned int' as parameter.  If we need >32 bit conversion,
// we need to add a uint64_t version of bit_range_convert.
//    OIIO_CHECK_EQUAL ((bit_range_convert<33,16>(8589934591)), 65535);
//    OIIO_CHECK_EQUAL ((bit_range_convert<33,33>(8589934591)), 8589934591);
//    OIIO_CHECK_EQUAL ((bit_range_convert<64,32>(18446744073709551615)), 4294967295);
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

    test_int_helpers ();

    std::cout << "\nround trip convert char/float/char\n";
    test_convert_type<char,float> ();
    std::cout << "round trip convert unsigned char/float/unsigned char\n";
    test_convert_type<unsigned char,float> ();
    std::cout << "round trip convert unsigned char/unsigned short/unsigned char\n";
    test_convert_type<unsigned char,unsigned short> ();
    std::cout << "round trip convert short/float/short\n";
    test_convert_type<short,float> ();
    std::cout << "round trip convert unsigned short/float/unsigned short\n";
    test_convert_type<unsigned short,float> ();
    std::cout << "round trip convert float/int/float \n";
    test_convert_type<float,int> ();
    std::cout << "round trip convert double/float/double\n";
    test_convert_type<double,float> ();
    std::cout << "round trip convert double/long/double\n";
    test_convert_type<double,long> ();
    std::cout << "round trip convert float/unsigned int/float\n";
    test_convert_type<float, unsigned int> ();

    benchmark_convert_type<unsigned char, float> ();
    benchmark_convert_type<float, unsigned char> ();
    benchmark_convert_type<unsigned short, float> ();
    benchmark_convert_type<float, unsigned short> ();
    benchmark_convert_type<half, float> ();
    benchmark_convert_type<float, half> ();
    benchmark_convert_type<float, float> ();
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

    return unit_test_failures != 0;
}
