/*
Copyright (c) 2014 Larry Gritz et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
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
*/


#include <sstream>

#include "OpenImageIO/simd.h"
#include "OpenImageIO/unittest.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/strutil.h"



OIIO_NAMESPACE_USING;

using namespace OIIO::simd;


template<typename X, typename Y>
inline void
OIIO_CHECK_SIMD_EQUAL_impl (const X& x, const Y& y,
                            const char *xstr, const char *ystr,
                            const char *file, int line)
{
    if (! all(x == y)) {
        std::cout << __FILE__ << ":" << __LINE__ << ":\n"
                  << "FAILED: " << xstr << " == " << ystr << "\n"
                  << "\tvalues were '" << x << "' and '" << y << "'\n";
        ++unit_test_failures;
    }
}


#define xOIIO_CHECK_SIMD_EQUAL(x,y) \
            OIIO_CHECK_SIMD_EQUAL_impl(x,y,#x,#y,__FILE__,__LINE__)
#define OIIO_CHECK_SIMD_EQUAL(x,y)                                      \
    (all ((x) == (y)) ? ((void)0)                                       \
         : ((std::cout << __FILE__ << ":" << __LINE__ << ":\n"          \
             << "FAILED: " << #x << " == " << #y << "\n"                \
             << "\tvalues were '" << (x) << "' and '" << (y) << "'\n"), \
            (void)++unit_test_failures))



template<typename VEC>
void test_loadstore ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_loadstore " << VEC::type_name() << "\n";
    VEC C1234 (1, 2, 3, 4);
    // VEC C0 (0);
    ELEM partial[4] = { 101, 102, 103, 104 };
    for (int i = 1; i <= 4; ++i) {
        VEC a (ELEM(0));
        a.load (partial, i);
        for (int j = 0; j < 4; ++j)
            OIIO_CHECK_EQUAL (a[j], j<i ? partial[j] : ELEM(0));
        std::cout << "  partial load " << i << " : " << a << "\n";
        ELEM stored[4] = { 0, 0, 0, 0 };
        C1234.store (stored, i);
        for (int j = 0; j < 4; ++j)
            OIIO_CHECK_EQUAL (stored[j], j<i ? ELEM(j+1) : ELEM(0));
        std::cout << "  partial store " << i << " : " 
                  << stored[0] << ' ' << stored[1] << ' '
                  << stored[2] << ' ' << stored[3] << "\n";
    }

    {
    // Check load from integers
    VEC C1234 (1, 2, 3, 4);
    unsigned short us1234[] = {1, 2, 3, 4};
    short s1234[] = {1, 2, 3, 4};
    unsigned char uc1234[] = {1, 2, 3, 4};
    char c1234[] = {1, 2, 3, 4};
    OIIO_CHECK_SIMD_EQUAL (VEC(us1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( s1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(uc1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( c1234), C1234);
    }
}



template<typename VEC>
void test_component_access ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_component_access " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_EQUAL (a[0], 0);
    OIIO_CHECK_EQUAL (a[1], 1);
    OIIO_CHECK_EQUAL (a[2], 2);
    OIIO_CHECK_EQUAL (a[3], 3);
    OIIO_CHECK_EQUAL (extract<0>(a), 0);
    OIIO_CHECK_EQUAL (extract<1>(a), 1);
    OIIO_CHECK_EQUAL (extract<2>(a), 2);
    OIIO_CHECK_EQUAL (extract<3>(a), 3);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, ELEM(42)), VEC(42,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, ELEM(42)), VEC(0,42,2,3));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, ELEM(42)), VEC(0,1,42,3));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, ELEM(42)), VEC(0,1,2,42));

    const ELEM vals[4] = { 0, 1, 2, 3 };
    VEC b (vals);
    OIIO_CHECK_EQUAL (b[0], 0);
    OIIO_CHECK_EQUAL (b[1], 1);
    OIIO_CHECK_EQUAL (b[2], 2);
    OIIO_CHECK_EQUAL (b[3], 3);
    OIIO_CHECK_EQUAL (extract<0>(b), 0);
    OIIO_CHECK_EQUAL (extract<1>(b), 1);
    OIIO_CHECK_EQUAL (extract<2>(b), 2);
    OIIO_CHECK_EQUAL (extract<3>(b), 3);
}



template<typename VEC>
void test_arithmetic ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_arithmetic " << VEC::type_name() << "\n";

    VEC a (10, 11, 12, 13);
    VEC b (1, 2, 3, 4);
    OIIO_CHECK_SIMD_EQUAL (a+b, VEC(11,13,15,17));
    OIIO_CHECK_SIMD_EQUAL (a-b, VEC(9,9,9,9));
    OIIO_CHECK_SIMD_EQUAL (a*b, VEC(10,22,36,52));
    OIIO_CHECK_SIMD_EQUAL (a/b, VEC(a[0]/b[0],a[1]/b[1],a[2]/b[2],a[3]/b[3]));
    OIIO_CHECK_EQUAL (reduce_add(b), ELEM(10));
    OIIO_CHECK_SIMD_EQUAL (vreduce_add(b), VEC(ELEM(10)));
}



template<typename VEC>
void test_comparisons ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_comparisons " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_SIMD_EQUAL (a < 2, mask4(1,1,0,0));
    OIIO_CHECK_SIMD_EQUAL (a > 2, mask4(0,0,0,1));
    OIIO_CHECK_SIMD_EQUAL (a <= 2, mask4(1,1,1,0));
    OIIO_CHECK_SIMD_EQUAL (a >= 2, mask4(0,0,1,1));
    OIIO_CHECK_SIMD_EQUAL (a == 2, mask4(0,0,1,0));
    OIIO_CHECK_SIMD_EQUAL (a != 2, mask4(1,1,0,1));
}



template<typename VEC>
void test_shuffle ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_shuffle " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0>(a)), VEC(3,2,1,0));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,0,2,2>(a)), VEC(0,0,2,2));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<1,1,3,3>(a)), VEC(1,1,3,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,1,0,1>(a)), VEC(0,1,0,1));
}



template<typename VEC>
void test_swizzle ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_swizzle " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    VEC b (10, 11, 12, 13);
    OIIO_CHECK_SIMD_EQUAL (AxyBxy(a,b), VEC(0,1,10,11));
    OIIO_CHECK_SIMD_EQUAL (AxBxAyBy(a,b), VEC(0,10,1,11));
    OIIO_CHECK_SIMD_EQUAL (b.xyz0(), VEC(10,11,12,0));
}



template<typename VEC>
void test_blend ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_blend " << VEC::type_name() << "\n";

    VEC a (1, 2, 3, 4);
    VEC b (10, 11, 12, 13);

    OIIO_CHECK_SIMD_EQUAL (blend (a, b, mask4(false,false,false,false)), a);
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, mask4(true,true,true,true)), b);
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, mask4(true,false,true,false)), VEC (10, 2, 12, 4));

    OIIO_CHECK_SIMD_EQUAL (blend0 (a, mask4(false,false,false,false)), VEC(0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, mask4(true,true,true,true)), a);
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, mask4(true,false,true,false)), VEC (1, 0, 3, 0));

    OIIO_CHECK_SIMD_EQUAL (blend0not (a, mask4(false,false,false,false)), a);
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, mask4(true,true,true,true)), VEC(0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, mask4(true,false,true,false)), VEC (0, 2, 0, 4));
}



template<typename VEC>
void test_transpose ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_transpose " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    VEC b (4, 5, 6, 7);
    VEC c (8, 9, 10, 11);
    VEC d (12, 13, 14, 15);

    OIIO_CHECK_SIMD_EQUAL (AxBxCxDx(a,b,c,d), VEC(0,4,8,12));

    std::cout << " before transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    transpose (a, b, c, d);
    std::cout << " after transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    OIIO_CHECK_SIMD_EQUAL (a, VEC(0,4,8,12));
    OIIO_CHECK_SIMD_EQUAL (b, VEC(1,5,9,13));
    OIIO_CHECK_SIMD_EQUAL (c, VEC(2,6,10,14));
    OIIO_CHECK_SIMD_EQUAL (d, VEC(3,7,11,15));
}



void test_shift ()
{
    std::cout << "test_shift\n";
    int4 i (1, 2, 4, 8);
    OIIO_CHECK_SIMD_EQUAL (i << 2, int4(4, 8, 16, 32));

    int a = 1<<31, b = -1, c = 0xffff, d = 3;
    int4 hard (a, b, c, d);
    OIIO_CHECK_SIMD_EQUAL (hard >> 1, int4(a>>1, b>>1, c>>1, d>>1));
    OIIO_CHECK_SIMD_EQUAL (srl(hard,1), int4(unsigned(a)>>1, unsigned(b)>>1,
                                             unsigned(c)>>1, unsigned(d)>>1));
    std::cout << Strutil::format ("  [%x] >>  1 == [%x]\n", hard, hard>>1);
    std::cout << Strutil::format ("  [%x] srl 1 == [%x]\n", hard, srl(hard,4));
    OIIO_CHECK_SIMD_EQUAL (hard >> 4, int4(a>>4, b>>4, c>>4, d>>4));
    OIIO_CHECK_SIMD_EQUAL (srl(hard,4), int4(unsigned(a)>>4, unsigned(b)>>4,
                                             unsigned(c)>>4, unsigned(d)>>4));
    std::cout << Strutil::format ("  [%x] >>  4 == [%x]\n", hard, hard>>4);
    std::cout << Strutil::format ("  [%x] srl 4 == [%x]\n", hard, srl(hard,4));

    i = int4(1,2,4,8);
    i <<= 1;
    OIIO_CHECK_SIMD_EQUAL (i, int4(2,4,8,16));
    i = int4(1,2,4,8);
    i >>= 1;
    OIIO_CHECK_SIMD_EQUAL (i, int4(0,1,2,4));
}



template<typename VEC>
void test_vectorops ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_vectorops " << VEC::type_name() << "\n";

    VEC a (10, 11, 12, 13);
    VEC b (1, 2, 3, 4);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36+52));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36+52));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
}



// Miscellaneous one-off stuff not caught by other tests
void test_special ()
{
    {
        // Make sure a float4 constructed from saturated unsigned short,
        // short, unsigned char, or char values, then divided by the float
        // max, exactly equals 1.0.
        short s32767[] = {32767, 32767, 32767, 32767};
        unsigned short us65535[] = {65535, 65535, 65535, 65535};
        char c127[] = {127, 127, 127, 127};
        unsigned char uc255[] = {255, 255, 255, 255};
        OIIO_CHECK_SIMD_EQUAL (float4(us65535)/float4(65535.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(us65535)*float4(1.0f/65535.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(s32767)/float4(32767.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(s32767)*float4(1.0f/32767.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(uc255)/float4(255.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(uc255)*float4(1.0f/255.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(c127)/float4(127.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(c127)*float4(1.0f/127.0), float4(1.0f));
    }
}




int
main (int argc, char *argv[])
{
#ifdef OIIO_SIMD_SSE
    std::cout << "SIMD is " << OIIO_SIMD_SSE << "\n";
#else
    std::cout << "NO SIMD!!\n";
#endif

    std::cout << "\n";
    test_loadstore<float4> ();
    test_component_access<float4> ();
    test_arithmetic<float4> ();
    test_comparisons<float4> ();
    test_shuffle<float4> ();
    test_swizzle<float4> ();
    test_blend<float4> ();
    test_transpose<float4> ();
    test_vectorops<float4> ();

    std::cout << "\n";
    test_loadstore<int4> ();
    test_component_access<int4> ();
    test_arithmetic<int4> ();
    test_comparisons<int4> ();
    test_shuffle<int4> ();
    test_swizzle<float4> ();
    test_blend<int4> ();
    test_transpose<int4> ();
    test_shift ();

    std::cout << "\n";
    test_shuffle<mask4> ();

    test_special();

    return unit_test_failures;
}
