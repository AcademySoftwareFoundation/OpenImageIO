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

#include <OpenEXR/half.h>
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathMatrix.h>

#include <OpenImageIO/simd.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/fmath.h>



OIIO_NAMESPACE_USING;

using namespace OIIO::simd;


static int iterations = 10;
static int ntrials = 5;
static bool verbose = false;


static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("simd_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  simd_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
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


#define OIIO_CHECK_SIMD_EQUAL_THRESH(x,y,eps)                           \
    (all (abs((x)-(y)) < (eps)) ? ((void)0)                             \
         : ((std::cout << __FILE__ << ":" << __LINE__ << ":\n"          \
             << "FAILED: " << #x << " == " << #y << "\n"                \
             << "\tvalues were '" << (x) << "' and '" << (y) << "'\n"), \
            (void)++unit_test_failures))



template<typename VEC>
inline VEC mkvec (typename VEC::value_t a, typename VEC::value_t b,
                  typename VEC::value_t c, typename VEC::value_t d=0)
{
    return VEC(a,b,c,d);
}


template<>
inline float3 mkvec (float a, float b, float c, float d)
{
    return float3(a,b,c);
}



inline Imath::V3f
norm_imath (const Imath::V3f &a) {
    return a.normalized();
}

inline Imath::V3f
norm_imath_simd (float3 a) {
    return a.normalized().V3f();
}

inline Imath::V3f
norm_imath_simd_fast (float3 a) {
    return a.normalized_fast().V3f();
}

inline float3
norm_simd_fast (float3 a) {
    return a.normalized_fast();
}

inline float3
norm_simd (float3 a) {
    return a.normalized();
}


inline Imath::M44f inverse_imath (const Imath::M44f &M)
{
    return M.inverse();
}


inline matrix44 inverse_simd (const matrix44 &M)
{
    return M.inverse();
}



template<typename VEC>
void test_loadstore ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_loadstore " << VEC::type_name() << "\n";
    VEC C1234 = mkvec<VEC>(1, 2, 3, 4);
    // VEC C0 (0);
    ELEM partial[4] = { 101, 102, 103, 104 };
    for (int i = 1; i <= VEC::elements; ++i) {
        VEC a (ELEM(0));
        a.load (partial, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL (a[j], j<i ? partial[j] : ELEM(0));
        std::cout << "  partial load " << i << " : " << a << "\n";
        ELEM stored[4] = { 0, 0, 0, 0 };
        C1234.store (stored, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL (stored[j], j<i ? ELEM(j+1) : ELEM(0));
        std::cout << "  partial store " << i << " :";
        for (int c = 0; c < VEC::elements; ++c)
            std::cout << ' ' << stored[c];
        std::cout << std::endl;
    }

    {
    // Check load from integers
    // VEC C1234 (1, 2, 3, 4);
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



void
test_int4_to_uint16s ()
{
    int4 i (0xffff0001, 0xffff0002, 0xffff0003, 0xffff0004);
    unsigned short s[4];
    i.store (s);
    OIIO_CHECK_EQUAL (s[0], 1);
    OIIO_CHECK_EQUAL (s[1], 2);
    OIIO_CHECK_EQUAL (s[2], 3);
    OIIO_CHECK_EQUAL (s[3], 4);
}



void
test_int4_to_uint8s ()
{
    int4 i (0xffffff01, 0xffffff02, 0xffffff03, 0xffffff04);
    unsigned char c[4];
    i.store (c);
    OIIO_CHECK_EQUAL (int(c[0]), 1);
    OIIO_CHECK_EQUAL (int(c[1]), 2);
    OIIO_CHECK_EQUAL (int(c[2]), 3);
    OIIO_CHECK_EQUAL (int(c[3]), 4);
}



template<typename VEC>
void test_component_access ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_component_access " << VEC::type_name() << "\n";

    VEC a = mkvec<VEC> (0, 1, 2, 3);
    OIIO_CHECK_EQUAL (a[0], 0);
    OIIO_CHECK_EQUAL (a[1], 1);
    OIIO_CHECK_EQUAL (a[2], 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (a[3], 3);
    OIIO_CHECK_EQUAL (a.x(), 0);
    OIIO_CHECK_EQUAL (a.y(), 1);
    OIIO_CHECK_EQUAL (a.z(), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (a.w(), 3);
    OIIO_CHECK_EQUAL (extract<0>(a), 0);
    OIIO_CHECK_EQUAL (extract<1>(a), 1);
    OIIO_CHECK_EQUAL (extract<2>(a), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (extract<3>(a), 3);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, ELEM(42)), mkvec<VEC>(42,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, ELEM(42)), mkvec<VEC>(0,42,2,3));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, ELEM(42)), mkvec<VEC>(0,1,42,3));
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_SIMD_EQUAL (insert<3>(a, ELEM(42)), mkvec<VEC>(0,1,2,42));
    VEC t;
    t = a; t.set_x(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(42,1,2,3));
    t = a; t.set_y(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,42,2,3));
    t = a; t.set_z(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,1,42,3));
    if (SimdElements<VEC>::size > 3) {
        t = a; t.set_w(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,1,2,42));
    }

    const ELEM vals[4] = { 0, 1, 2, 3 };
    VEC b (vals);
    OIIO_CHECK_EQUAL (b[0], 0);
    OIIO_CHECK_EQUAL (b[1], 1);
    OIIO_CHECK_EQUAL (b[2], 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (b[3], 3);
    OIIO_CHECK_EQUAL (extract<0>(b), 0);
    OIIO_CHECK_EQUAL (extract<1>(b), 1);
    OIIO_CHECK_EQUAL (extract<2>(b), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (extract<3>(b), 3);
}



template<>
void test_component_access<mask4> ()
{
    typedef mask4 VEC;
    typedef VEC::value_t ELEM;
    std::cout << "test_component_access " << VEC::type_name() << "\n";

    VEC a (false, true, true, true);
    OIIO_CHECK_EQUAL (bool(a[0]), false);
    OIIO_CHECK_EQUAL (bool(a[1]), true);
    OIIO_CHECK_EQUAL (bool(a[2]), true);
    OIIO_CHECK_EQUAL (bool(a[3]), true);
    OIIO_CHECK_EQUAL (extract<0>(a), false);
    OIIO_CHECK_EQUAL (extract<1>(a), true);
    OIIO_CHECK_EQUAL (extract<2>(a), true);
    OIIO_CHECK_EQUAL (extract<3>(a), true);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, ELEM(true)), VEC(true,true,true,true));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, ELEM(false)), VEC(false,false,true,true));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, ELEM(false)), VEC(false,true,false,true));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, ELEM(false)), VEC(false,true,true,false));
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
    if (is_same<VEC,float3>::value)
        OIIO_CHECK_EQUAL (reduce_add(b), ELEM(6));
    else
        OIIO_CHECK_EQUAL (reduce_add(b), ELEM(10));
    OIIO_CHECK_SIMD_EQUAL (vreduce_add(b), VEC(ELEM(10)));
    OIIO_CHECK_EQUAL (reduce_add(VEC(1.0f)), SimdElements<VEC>::size);
}



template<>
void test_arithmetic<float3> ()
{
    typedef float3 VEC;
    typedef typename VEC::value_t ELEM;
    std::cout << "test_arithmetic " << VEC::type_name() << "\n";

    VEC a (10, 11, 12);
    VEC b (1, 2, 3);
    OIIO_CHECK_SIMD_EQUAL (a+b, VEC(11,13,15));
    OIIO_CHECK_SIMD_EQUAL (a-b, VEC(9,9,9));
    OIIO_CHECK_SIMD_EQUAL (a*b, VEC(10,22,36));
    OIIO_CHECK_SIMD_EQUAL (a/b, VEC(a[0]/b[0],a[1]/b[1],a[2]/b[2]));
    OIIO_CHECK_EQUAL (reduce_add(b), ELEM(6));
    OIIO_CHECK_SIMD_EQUAL (vreduce_add(b), VEC(ELEM(6)));
}



template<typename VEC>
void test_fused ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_fused " << VEC::type_name() << "\n";

    VEC a (10, 11, 12, 13);
    VEC b (1, 2, 3, 4);
    VEC c (0.5, 1.5, 2.5, 3.5);
    OIIO_CHECK_SIMD_EQUAL (madd (a, b, c), a*b+c);
    OIIO_CHECK_SIMD_EQUAL (msub (a, b, c), a*b-c);
    OIIO_CHECK_SIMD_EQUAL (nmadd (a, b, c), -(a*b)+c);
    OIIO_CHECK_SIMD_EQUAL (nmsub (a, b, c), -(a*b)-c);
}



void test_bitwise_int4 ()
{
    typedef int4 VEC;
    typedef int ELEM;
    std::cout << "test_bitwise " << VEC::type_name() << "\n";

    OIIO_CHECK_SIMD_EQUAL (VEC(0x12341234) & VEC(0x11111111), VEC(0x10101010));
    OIIO_CHECK_SIMD_EQUAL (VEC(0x12341234) | VEC(0x11111111), VEC(0x13351335));
    OIIO_CHECK_SIMD_EQUAL (VEC(0x12341234) ^ VEC(0x11111111), VEC(0x03250325));
    OIIO_CHECK_SIMD_EQUAL (~(VEC(0x12341234)), VEC(0xedcbedcb));
    OIIO_CHECK_SIMD_EQUAL (andnot (VEC(0x11111111), VEC(0x12341234)),
                           (~(VEC(0x11111111))) & VEC(0x12341234));
    OIIO_CHECK_SIMD_EQUAL (andnot (VEC(0x11111111), VEC(0x12341234)), VEC(0x02240224));
}



void test_bitwise_mask4 ()
{
    typedef mask4 VEC;
    typedef int ELEM;
    std::cout << "test_bitwise " << VEC::type_name() << "\n";

    OIIO_CHECK_SIMD_EQUAL (VEC(true, true, false, false) & VEC(true, false, true, false),
                           VEC(true, false, false, false));
    OIIO_CHECK_SIMD_EQUAL (VEC(true, true, false, false) | VEC(true, false, true, false),
                           VEC(true, true, true, false));
    OIIO_CHECK_SIMD_EQUAL (VEC(true, true, false, false) ^ VEC(true, false, true, false),
                           VEC(false, true, true, false));
    OIIO_CHECK_SIMD_EQUAL (~(VEC(true, true, false, false)), VEC(false, false, true, true));
}



template<typename VEC>
void test_comparisons ()
{
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
    std::cout << "test_swizzle " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    VEC b (10, 11, 12, 13);
    OIIO_CHECK_SIMD_EQUAL (AxyBxy(a,b), VEC(0,1,10,11));
    OIIO_CHECK_SIMD_EQUAL (AxBxAyBy(a,b), VEC(0,10,1,11));
    OIIO_CHECK_SIMD_EQUAL (b.xyz0(), VEC(10,11,12,0));
    OIIO_CHECK_SIMD_EQUAL (b.xyz1(), VEC(10,11,12,1));
}



template<typename VEC>
void test_blend ()
{
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

    VEC a = mkvec<VEC> (10, 11, 12, 13);
    VEC b = mkvec<VEC> (1, 2, 3, 4);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36+52));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36+52));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
}



template<>
void test_vectorops<float3> ()
{
    typedef float3 VEC;
    typedef typename VEC::value_t ELEM;
    std::cout << "test_vectorops " << VEC::type_name() << "\n";

    VEC a = mkvec<VEC> (10, 11, 12);
    VEC b = mkvec<VEC> (1, 2, 3);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
}



void test_constants ()
{
    std::cout << "test_constants\n";

    OIIO_CHECK_SIMD_EQUAL (mask4::False(), mask4(false));
    OIIO_CHECK_SIMD_EQUAL (mask4::True(), mask4(true));

    OIIO_CHECK_SIMD_EQUAL (int4::Zero(), int4(0));
    OIIO_CHECK_SIMD_EQUAL (int4::One(), int4(1));
    OIIO_CHECK_SIMD_EQUAL (int4::NegOne(), int4(-1));

    OIIO_CHECK_SIMD_EQUAL (float4::Zero(), float4(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float4::One(), float4(1.0f));

    OIIO_CHECK_SIMD_EQUAL (float3::Zero(), float3(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float3::One(), float3(1.0f));
}



// Miscellaneous one-off stuff not caught by other tests
void test_special ()
{
    std::cout << "test_special\n";
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



void test_mathfuncs ()
{
    std::cout << "test_mathfuncs\n";
    float4 A (-1.0f, 0.0f, 1.0f, 4.5f);
    float4 expA (0.367879441171442f, 1.0f, 2.718281828459045f, 90.0171313005218f);
    OIIO_CHECK_SIMD_EQUAL (exp(A), expA);
    OIIO_CHECK_SIMD_EQUAL_THRESH (log(expA), A, 1e-6f);
    OIIO_CHECK_SIMD_EQUAL (fast_exp(A),
                float4(fast_exp(A[0]), fast_exp(A[1]), fast_exp(A[2]), fast_exp(A[3])));
    OIIO_CHECK_SIMD_EQUAL (fast_log(expA),
                float4(fast_log(expA[0]), fast_log(expA[1]), fast_log(expA[2]), fast_log(expA[3])));
    OIIO_CHECK_SIMD_EQUAL_THRESH (fast_pow_pos(float4(2.0f), A),
                           float4(0.5f, 1.0f, 2.0f, 22.62741699796952f), 0.0001f);

    OIIO_CHECK_SIMD_EQUAL (safe_div(float4(1.0f,2.0f,3.0f,4.0f), float4(2.0f,0.0f,2.0f,0.0f)),
                           float4(0.5f,0.0f,1.5f,0.0f));
    OIIO_CHECK_SIMD_EQUAL (hdiv(float4(1.0f,2.0f,3.0f,2.0f)), float3(0.5f,1.0f,1.5f));
    OIIO_CHECK_SIMD_EQUAL (sqrt(float4(1.0f,4.0f,9.0f,16.0f)), float4(1.0f,2.0f,3.0f,4.0f));
    OIIO_CHECK_SIMD_EQUAL (rsqrt(float4(1.0f,4.0f,9.0f,16.0f)), float4(1.0f)/float4(1.0f,2.0f,3.0f,4.0f));
    OIIO_CHECK_SIMD_EQUAL_THRESH (rsqrt_fast(float4(1.0f,4.0f,9.0f,16.0f)),
                                  float4(1.0f)/float4(1.0f,2.0f,3.0f,4.0f), 0.0005f);
    OIIO_CHECK_SIMD_EQUAL (float3(1.0f,2.0f,3.0f).normalized(),
                           float3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))));
    OIIO_CHECK_SIMD_EQUAL_THRESH (float3(1.0f,2.0f,3.0f).normalized_fast(),
                                  float3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))), 0.0005);
}



void test_metaprogramming ()
{
    std::cout << "test_metaprogramming\n";
    OIIO_CHECK_EQUAL (SimdSize<float4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<float3>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<int4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<mask4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdSize<int>::size, 1);

    OIIO_CHECK_EQUAL (SimdElements<float4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<float3>::size, 3);
    OIIO_CHECK_EQUAL (SimdElements<int4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<mask4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdElements<int>::size, 1);

    OIIO_CHECK_EQUAL (float4::elements, 4);
    OIIO_CHECK_EQUAL (float3::elements, 3);
    OIIO_CHECK_EQUAL (int4::elements, 4);
    OIIO_CHECK_EQUAL (mask4::elements, 4);
    // OIIO_CHECK_EQUAL (is_same<float4::value_t,float>::value, true);
    // OIIO_CHECK_EQUAL (is_same<float3::value_t,float>::value, true);
    // OIIO_CHECK_EQUAL (is_same<int4::value_t,int>::value, true);
    // OIIO_CHECK_EQUAL (is_same<mask4::value_t,int>::value, true);
}



// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformp_imath (const Imath::V3f &v, const Imath::M44f &m)
{
    Imath::V3f r;
    m.multVecMatrix (v, r);
    return r;
}

// Transform a point by a matrix using simd ops on Imath types.
inline Imath::V3f
transformp_imath_simd (const Imath::V3f &v, const Imath::M44f &m)
{
    return simd::transformp(m,v).V3f();
}

// Transform a simd point by an Imath matrix using SIMD
inline float3
transformp_simd (const float3 &v, const Imath::M44f &m)
{
    return simd::transformp (m, v);
}

// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformv_imath (const Imath::V3f &v, const Imath::M44f &m)
{
    Imath::V3f r;
    m.multDirMatrix (v, r);
    return r;
}



inline bool
mx_equal_thresh (const matrix44 &a, const matrix44 &b, float thresh)
{
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i)
            if (fabsf(a[j][i] - b[j][i]) > thresh)
                return false;
    return true;
}



void test_matrix ()
{
    Imath::V3f P (1.0f, 0.0f, 0.0f);
    Imath::M44f Mtrans (1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  10, 11, 12, 1);
    Imath::M44f Mrot = Imath::M44f().rotate(Imath::V3f(0.0f, M_PI_2, 0.0f));

    std::cout << "Testing matrix ops:\n";
    std::cout << "  P = " << P << "\n";
    std::cout << "  Mtrans = " << Mtrans << "\n";
    std::cout << "  Mrot   = " << Mrot << "\n";
    OIIO_CHECK_EQUAL (simd::transformp(Mtrans, P).V3f(),
                      transformp_imath(P, Mtrans));
    std::cout << "  P translated = " << simd::transformp(Mtrans,P) << "\n";
    OIIO_CHECK_EQUAL (simd::transformv(Mtrans,P).V3f(), P);
    OIIO_CHECK_EQUAL (simd::transformp(Mrot, P).V3f(),
                      transformp_imath(P, Mrot));
    std::cout << "  P rotated = " << simd::transformp(Mrot,P) << "\n";
    OIIO_CHECK_EQUAL (simd::transformvT(Mrot, P).V3f(),
                      transformv_imath(P, Mrot.transposed()));
    std::cout << "  P rotated by the transpose = " << simd::transformv(Mrot,P) << "\n";
    OIIO_CHECK_EQUAL (matrix44(Mrot).transposed().M44f(),
                      Mrot.transposed());
    std::cout << "  Mrot transposed = " << matrix44(Mrot).transposed().M44f() << "\n";
    {
        matrix44 mt (Mtrans), mr (Mrot);
        OIIO_CHECK_EQUAL (mt, mt);
        OIIO_CHECK_EQUAL (mt, Mtrans);
        OIIO_CHECK_EQUAL (Mtrans, mt);
        OIIO_CHECK_NE (mt, mr);
        OIIO_CHECK_NE (mr, Mtrans);
        OIIO_CHECK_NE (Mtrans, mr);
    }
    OIIO_CHECK_ASSERT (mx_equal_thresh (Mtrans.inverse(),
                       matrix44(Mtrans).inverse(), 1.0e-6f));
    OIIO_CHECK_ASSERT (mx_equal_thresh (Mrot.inverse(),
                       matrix44(Mrot).inverse(), 1.0e-6f));
}



#if OIIO_CPLUSPLUS_VERSION >= 11  /* So easy with lambdas */

template <typename FUNC, typename T>
void benchmark_function (string_view funcname, size_t n, FUNC func, T x,
                         size_t work=SimdElements<T>::size)
{
    auto repeat_func = [&](){
        // Unroll the loop 8 times
        for (size_t i = 0; i < n; i += work*8) {
            auto r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
            r = func(x); DoNotOptimize (r);
        }
    };
    float time = time_trial (repeat_func, ntrials, iterations) / iterations;
    std::cout << Strutil::format (" %s: %7.1f Mvals/sec, (%.1f Mcalls/sec)\n",
                                  funcname, (n/1.0e6)/time,
                                  ((n/work)/1.0e6)/time);
}


template <typename FUNC, typename T, typename U>
void benchmark_function2 (string_view funcname, size_t n, FUNC func, T x, U y,
                          size_t work=SimdElements<T>::size)
{
    auto repeat_func = [&](){
        // Unroll the loop 8 times
        for (size_t i = 0; i < n; i += work*8) {
            auto r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
            r = func(x, y); DoNotOptimize (r);
        }
    };
    float time = time_trial (repeat_func, ntrials, iterations) / iterations;
    std::cout << Strutil::format (" %s: %7.1f Mvals/sec, (%.1f Mcalls/sec)\n",
                                  funcname, (n/1.0e6)/time,
                                  ((n/work)/1.0e6)/time);
}


// Wrappers to resolve the return type ambiguity
inline float fast_exp_float (float x) { return fast_exp(x); }
inline float4 fast_exp_float4 (const float4& x) { return fast_exp(x); }
inline float fast_log_float (float x) { return fast_log(x); }
inline float4 fast_log_float4 (const float4& x) { return fast_log(x); }

float dummy_float[16];
float dummy_float2[16];
float dummy_int[16];

template<typename VEC>
inline int loadstore_vec (int x) {
    VEC v;
    v.load (dummy_float);
    v.store (dummy_float2);
    return 0;
}

template<typename VEC, int N>
inline int loadstore_vec_N (int x) {
    VEC v;
    v.load (dummy_float, N);
    v.store (dummy_float2, N);
    return 0;
}

template<typename VEC>
inline VEC add_vec (const VEC &a, const VEC &b) {
    return a+b;
}

template<typename VEC>
inline VEC mul_vec (const VEC &a, const VEC &b) {
    return a*b;
}

template<typename VEC>
inline VEC div_vec (const VEC &a, const VEC &b) {
    return a/b;
}

// Add Imath 3-vectors using simd underneath
inline Imath::V3f
add_vec_simd (const Imath::V3f &a, const Imath::V3f &b) {
    return (float3(a)*float3(b)).V3f();
}

inline float dot_imath (const Imath::V3f &v) {
    return v.dot(v);
}
inline float dot_imath_simd (const Imath::V3f &v_) {
    float3 v (v_);
    return simd::dot(v,v);
}
inline float dot_simd (const simd::float3 v) {
    return dot(v,v);
}

inline Imath::M44f
mat_transpose (const Imath::M44f &m) {
    return m.transposed();
}

inline Imath::M44f
mat_transpose_simd (const Imath::M44f &m) {
    return matrix44(m).transposed().M44f();
}


inline float rsqrtf (float f) { return 1.0f / sqrtf(f); }

#endif


void test_timing ()
{
#if OIIO_CPLUSPLUS_VERSION >= 11  /* So easy with lambdas */
    const size_t size = 1000000;
    for (int i = 0; i < 16; ++i) {
        dummy_float[i] = 1.0f;
        dummy_int[i] = 1;
    }
    benchmark_function ("load/store float4", size, loadstore_vec<float4>, 0);
    benchmark_function ("load/store float4, 4 comps", size, loadstore_vec_N<float4, 4>, 0);
    benchmark_function ("load/store float4, 3 comps", size, loadstore_vec_N<float4, 3>, 0);
    benchmark_function ("load/store float4, 2 comps", size, loadstore_vec_N<float4, 2>, 0);
    benchmark_function ("load/store float4, 1 comps", size, loadstore_vec_N<float4, 1>, 0);
    benchmark_function ("load/store float3", size, loadstore_vec<float3>, 0);

    benchmark_function2 ("add float", size, add_vec<float>, float(2.51f), float(3.1f));
    benchmark_function2 ("add float4", size, add_vec<float4>, float4(2.51f), float4(3.1f));
    benchmark_function2 ("add float3", size, add_vec<float3>, float3(2.51f), float3(3.1f));
    benchmark_function2 ("add Imath::V3f", size, add_vec<Imath::V3f>, Imath::V3f(2.51f,1.0f,1.0f), Imath::V3f(3.1f,1.0f,1.0f));
    benchmark_function2 ("add Imath::V3f with simd", size, add_vec_simd, Imath::V3f(2.51f,1.0f,1.0f), Imath::V3f(3.1f,1.0f,1.0f));
    benchmark_function2 ("add int", size, add_vec<int>, int(2), int(3));
    benchmark_function2 ("add int4", size, add_vec<int4>, int4(2), int4(3));
    benchmark_function2 ("mul float", size, mul_vec<float>, float(2.51f), float(3.1f));
    benchmark_function2 ("mul float4", size, mul_vec<float4>, float4(2.51f), float4(3.1f));
    benchmark_function2 ("mul float3", size, mul_vec<float3>, float3(2.51f), float3(3.1f));
    benchmark_function2 ("mul Imath::V3f", size, mul_vec<Imath::V3f>, Imath::V3f(2.51f,0.0f,0.0f), Imath::V3f(3.1f,0.0f,0.0f));
    benchmark_function2 ("div float", size, div_vec<float>, float(2.51f), float(3.1f));
    benchmark_function2 ("div float4", size, div_vec<float4>, float4(2.51f), float4(3.1f));
    benchmark_function2 ("div float3", size, div_vec<float3>, float3(2.51f), float3(3.1f));
    benchmark_function2 ("div int", size, div_vec<int>, int(2), int(3));
    benchmark_function2 ("div int4", size, div_vec<int4>, int4(2), int4(3));
    benchmark_function ("dot Imath::V3f", size, dot_imath, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark_function ("dot Imath::V3f with simd", size, dot_imath_simd, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark_function ("dot float3", size, dot_simd, float3(2.0f,1.0f,0.0f), 1);

    Imath::V3f vx (2.51f,1.0f,1.0f);
    Imath::M44f mx (1,0,0,0, 0,1,0,0, 0,0,1,0, 10,11,12,1);
    benchmark_function2 ("transformp Imath", size, transformp_imath, vx, mx, 1);
    benchmark_function2 ("transformp Imath with simd", size, transformp_imath_simd, vx, mx, 1);
    benchmark_function2 ("transformp simd", size, transformp_simd, float3(vx), mx, 1);
    benchmark_function ("transpose m44", size, mat_transpose, mx, 1);
    benchmark_function ("transpose m44 with simd", size, mat_transpose_simd, mx, 1);

    benchmark_function ("expf", size, expf, 0.67f);
    benchmark_function ("fast_exp", size, fast_exp_float, 0.67f);
    benchmark_function ("simd::exp", size, simd::exp, float4(0.67f));
    benchmark_function ("simd::fast_exp", size, fast_exp_float4, float4(0.67f));

    benchmark_function ("logf", size, logf, 0.67f);
    benchmark_function ("fast_log", size, fast_log_float, 0.67f);
    benchmark_function ("simd::log", size, simd::log, float4(0.67f));
    benchmark_function ("simd::fast_log", size, fast_log_float4, float4(0.67f));
    benchmark_function2 ("powf", size, powf, 0.67f, 0.67f);
    benchmark_function2 ("simd fast_pow_pos", size, fast_pow_pos, float4(0.67f), float4(0.67f));
    benchmark_function ("sqrt", size, sqrtf, 4.0f);
    benchmark_function ("simd::sqrt", size, simd::sqrt, float4(1.0f,4.0f,9.0f,16.0f));
    benchmark_function ("rsqrt", size, rsqrtf, 4.0f);
    benchmark_function ("simd::rsqrt", size, simd::rsqrt, float4(1.0f,4.0f,9.0f,16.0f));
    benchmark_function ("simd::rsqrt_fast", size, simd::rsqrt_fast, float4(1.0f,4.0f,9.0f,16.0f));
    benchmark_function ("normalize Imath", size, norm_imath, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark_function ("normalize Imath with simd", size, norm_imath_simd, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark_function ("normalize Imath with simd fast", size, norm_imath_simd_fast, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark_function ("normalize simd", size, norm_simd, float3(1.0f,4.0f,9.0f));
    benchmark_function ("normalize simd fast", size, norm_simd_fast, float3(1.0f,4.0f,9.0f));
    benchmark_function ("m44 inverse Imath", size/8, inverse_imath, mx, 1);
    // std::cout << "inv " << matrix44(inverse_imath(mx)) << "\n";
    benchmark_function ("m44 inverse_simd", size/8, inverse_simd, mx, 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
    benchmark_function ("m44 inverse_simd native simd", size/8, inverse_simd, matrix44(mx), 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
#endif
}



int
main (int argc, char *argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODECOV)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs (argc, argv);

#if defined(OIIO_SIMD_AVX)
    std::cout << "SIMD is AVX " << OIIO_SIMD_AVX << "\n";
#elif defined(OIIO_SIMD_SSE)
    std::cout << "SIMD is SSE " << OIIO_SIMD_SSE << "\n";
#elif defined(OIIO_SIMD_NEON)
    std::cout << "SIMD is NEON " << OIIO_SIMD_NEON << "\n";
#elif defined(OIIO_SIMD_SSE)
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
    test_fused<float4> ();

    std::cout << "\n";
    test_loadstore<float3> ();
    test_component_access<float3> ();
    test_arithmetic<float3> ();
    // Unnecessary to test these, they just use the float4 ops.
    // test_comparisons<float3> ();
    // test_shuffle<float3> ();
    // test_swizzle<float3> ();
    // test_blend<float3> ();
    // test_transpose<float3> ();
    test_vectorops<float3> ();
    // test_fused<float3> ();

    std::cout << "\n";
    test_loadstore<int4> ();
    test_component_access<int4> ();
    test_arithmetic<int4> ();
    test_bitwise_int4 ();
    test_comparisons<int4> ();
    test_shuffle<int4> ();
    test_swizzle<float4> ();
    test_blend<int4> ();
    test_transpose<int4> ();
    test_int4_to_uint16s ();
    test_int4_to_uint8s ();
    test_shift ();

    std::cout << "\n";
    test_shuffle<mask4> ();
    test_component_access<mask4> ();
    test_bitwise_mask4 ();

    test_constants();
    test_special();
    test_mathfuncs();
    test_metaprogramming();
    test_matrix();

    std::cout << "\nTiming tests:\n";
    test_timing();

    if (unit_test_failures)
        std::cout << "\nERRORS!\n";
    else
        std::cout << "\nOK\n";
    return unit_test_failures;
}
