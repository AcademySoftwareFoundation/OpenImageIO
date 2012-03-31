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

#include "fmath.h"
#include "unittest.h"

OIIO_NAMESPACE_USING;


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


void test_bit_range_convert ()
{
    OIIO_CHECK_EQUAL ((bit_range_convert<10,16>(1023)), 65535);
    OIIO_CHECK_EQUAL ((bit_range_convert<2,8>(3)), 255);
    OIIO_CHECK_EQUAL ((bit_range_convert<8,8>(255)), 255);
    OIIO_CHECK_EQUAL ((bit_range_convert<16,10>(65535)), 1023);
    OIIO_CHECK_EQUAL ((bit_range_convert<2,20>(3)), 1048575);
    OIIO_CHECK_EQUAL ((bit_range_convert<20,2>(1048575)), 3);
    OIIO_CHECK_EQUAL ((bit_range_convert<20,21>(1048575)), 2097151);
    OIIO_CHECK_EQUAL ((bit_range_convert<32,32>(4294967295)), 4294967295);
    OIIO_CHECK_EQUAL ((bit_range_convert<32,16>(4294967295)), 65535);
// These are not expected to work, since bit_range_convert only takes a
// regular 'unsigned int' as parameter.  If we need >32 bit conversion,
// we need to add a uint64_t version of bit_range_convert.
//    OIIO_CHECK_EQUAL ((bit_range_convert<33,16>(8589934591)), 65535);
//    OIIO_CHECK_EQUAL ((bit_range_convert<33,33>(8589934591)), 8589934591);
//    OIIO_CHECK_EQUAL ((bit_range_convert<64,32>(18446744073709551615)), 4294967295);
}



int main (int argc, char *argv[])
{
    std::cout << "round trip convert char/float/char\n";
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
