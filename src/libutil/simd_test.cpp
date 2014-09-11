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



OIIO_NAMESPACE_USING;

using namespace OIIO::simd;


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

    const ELEM vals[4] = { 0, 1, 2, 3 };
    VEC b (vals);
    OIIO_CHECK_EQUAL (b[0], 0);
    OIIO_CHECK_EQUAL (b[1], 1);
    OIIO_CHECK_EQUAL (b[2], 2);
    OIIO_CHECK_EQUAL (b[3], 3);
}



template<typename VEC>
void test_arithmetic ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_arithmetic " << VEC::type_name() << "\n";

    VEC a (10, 11, 12, 13);
    VEC b (1, 2, 3, 4);
    OIIO_CHECK_ASSERT (all((a+b) == VEC(11,13,15,17)));
    OIIO_CHECK_ASSERT (all((a-b) == VEC(9,9,9,9)));
    OIIO_CHECK_ASSERT (all((a*b) == VEC(10,22,36,52)));
    OIIO_CHECK_ASSERT (all((a/b) == VEC(a[0]/b[0],a[1]/b[1],a[2]/b[2],a[3]/b[3])));
}



template<typename VEC>
void test_comparisons ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_comparisons " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_ASSERT (all((a < 2) == mask4(1,1,0,0)));
    OIIO_CHECK_ASSERT (all((a > 2) == mask4(0,0,0,1)));
    OIIO_CHECK_ASSERT (all((a <= 2) == mask4(1,1,1,0)));
    OIIO_CHECK_ASSERT (all((a >= 2) == mask4(0,0,1,1)));
    OIIO_CHECK_ASSERT (all((a == 2) == mask4(0,0,1,0)));
    OIIO_CHECK_ASSERT (all((a != 2) == mask4(1,1,0,1)));
}



template<typename VEC>
void test_shuffle ()
{
    typedef typename VEC::value_t ELEM;
    std::cout << "test_shuffle " << VEC::type_name() << "\n";

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_ASSERT (all(shuffle<3,2,1,0>(a) == VEC(3,2,1,0)));
    OIIO_CHECK_ASSERT (all(shuffle<0,0,2,2>(a) == VEC(0,0,2,2)));
    OIIO_CHECK_ASSERT (all(shuffle<1,1,3,3>(a) == VEC(1,1,3,3)));
    OIIO_CHECK_ASSERT (all(shuffle<0,1,0,1>(a) == VEC(0,1,0,1)));
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
    test_component_access<float4> ();
    test_arithmetic<float4> ();
    // FIXME - implement float comparisons
    test_shuffle<float4> ();

    std::cout << "\n";
    test_component_access<int4> ();
    test_arithmetic<int4> ();
    test_comparisons<int4> ();
    test_shuffle<int4> ();

    std::cout << "\n";
    test_shuffle<mask4> ();

    return unit_test_failures;
}
