/*
Copyright 2008-2017 Larry Gritz et al. All Rights Reserved.

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


#include <limits>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;


// Helper: create a single ParamValue, store data in it, make sure we can
// extract each element again, make sure we can convert to the appropriate
// int or float, and also return a string representation.
template<typename T>
static std::string
test_numeric (T* data, int num_elements, TypeDesc type)
{
    ParamValue p ("name", type, num_elements, data);
    int n = type.numelements() * num_elements;
    for (int i = 0; i < n; ++i)
        OIIO_CHECK_EQUAL (p.get<T>(i), ((const T *)data)[i]);
    if (std::numeric_limits<T>::is_integer) {
        OIIO_CHECK_EQUAL (p.get_int(), int(data[0]));
        for (int i = 0; i < n; ++i)
            OIIO_CHECK_EQUAL (p.get_int_indexed(i), int(data[i]));
    } else {
        OIIO_CHECK_EQUAL (p.get_float(), float(data[0]));
        for (int i = 0; i < n; ++i)
            OIIO_CHECK_EQUAL (p.get_float_indexed(i), float(data[i]));
    }
    return p.get_string();
}



// Create ParamValue of various types and make sure it copies the data in
// and out.
void test_value_types ()
{
    std::cout << "test_value_types\n";
    std::string ret;

    {
        int val = 42;
        ret = test_numeric (&val, 1, TypeDesc::INT);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        unsigned int val = 42;
        ret = test_numeric (&val, 1, TypeDesc::UINT);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        short val = 42;
        ret = test_numeric (&val, 1, TypeDesc::INT16);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        unsigned short val = 42;
        ret = test_numeric (&val, 1, TypeDesc::UINT16);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        char val = 42;
        ret = test_numeric (&val, 1, TypeDesc::INT8);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        unsigned char val = 42;
        ret = test_numeric (&val, 1, TypeDesc::UINT8);
        OIIO_CHECK_EQUAL (ret, "42");
    }

    {
        float val = 2.25;
        ret = test_numeric (&val, 1, TypeDesc::FLOAT);
        OIIO_CHECK_EQUAL (ret, "2.25");
    }

    {
        const char *val = "hello";
        ParamValue p ("name", val);
        OIIO_CHECK_EQUAL (p.get<ustring>(), "hello");
        OIIO_CHECK_EQUAL (p.get_string(), "hello");
    }

    {
        int imatrix[] = {100, 200, 300, 400};
        ret = test_numeric (&imatrix[0], 1, TypeInt);
        OIIO_CHECK_EQUAL (ret, "100");
        ret = test_numeric (imatrix, sizeof (imatrix)/sizeof(int), TypeInt);
        OIIO_CHECK_EQUAL (ret, "100, 200, 300, 400");
        OIIO_CHECK_NE (ret, "100, 200, 300, 400,");
        // Test it as an array as well
        ret = test_numeric (&imatrix[0], 1, TypeDesc(TypeDesc::INT,4));
        OIIO_CHECK_EQUAL (ret, "100, 200, 300, 400");
    }

    {
        float fmatrix[] = {10.12f, 200.34f, 300.11f, 400.9f};
        ret = test_numeric (&fmatrix[0], 1, TypeFloat);
        OIIO_CHECK_EQUAL (ret, "10.12");
        ret = test_numeric (fmatrix, sizeof (fmatrix) / sizeof (float), TypeFloat);
        OIIO_CHECK_EQUAL (ret, "10.12, 200.34, 300.11, 400.9");
        OIIO_CHECK_NE (ret, "10, 200, 300, 400");
        OIIO_CHECK_NE (ret, "10.12, 200.34, 300.11, 400.9,");
        ret = test_numeric (&fmatrix[0], 1, TypeDesc(TypeDesc::FLOAT,4));
        OIIO_CHECK_EQUAL (ret, "10.12, 200.34, 300.11, 400.9");
    }

    {
        unsigned long long ullmatrix[] = {0xffffffffffffffffLL, 0xffffffffffffffffLL};
        ret = test_numeric (&ullmatrix[0], 1, TypeDesc::UINT64);
        OIIO_CHECK_EQUAL (ret, "18446744073709551615");
        ret = test_numeric (ullmatrix, sizeof (ullmatrix) / sizeof (unsigned long long), TypeDesc::UINT64);
        OIIO_CHECK_EQUAL (ret, "18446744073709551615, 18446744073709551615");
        OIIO_CHECK_NE (ret, "-1, -1");
        OIIO_CHECK_NE (ret, "18446744073709551615, 18446744073709551615,");
    }

    {
        const char* smatrix[] = {"this is \"a test\"", "this is another test"};

        ParamValue p ("name", smatrix[0]);
        OIIO_CHECK_EQUAL (p.get<ustring>(), smatrix[0]);
        OIIO_CHECK_EQUAL (p.get_string(), smatrix[0]);

        ParamValue q ("name", TypeString, sizeof (smatrix) / sizeof (char *), &smatrix);
        OIIO_CHECK_EQUAL (q.get_string(), "\"this is \\\"a test\\\"\", \"this is another test\"");
    }

    {
        float matrix16[2][16] = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
        {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25}};
        ParamValue p ("name", TypeMatrix, 1, matrix16);
        std::string s = p.get_string();
        OIIO_CHECK_EQUAL (s, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16");
        OIIO_CHECK_NE (s, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16,");
        ParamValue q ("name", TypeMatrix, sizeof (matrix16) / (16 * sizeof (float)), matrix16);
        OIIO_CHECK_EQUAL (q.get_string(), "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16, 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25");
    }

    // Test rational
    {
        int rat[2] = { 1, 2 };
        ParamValue p ("name", TypeRational, 1, rat);
        // make sure we can retrieve it as int[2] (numerator, denominator)
        OIIO_CHECK_EQUAL (p.get<int>(0), rat[0]);
        OIIO_CHECK_EQUAL (p.get<int>(1), rat[1]);
        // make sure we can retrieve rational as float, with conversion
        OIIO_CHECK_EQUAL (p.get_float(), 0.5f);
        // make sure we can retrieve rational as nicely formatted string
        OIIO_CHECK_EQUAL (p.get_string(), "1/2");
    }

    // Double check that short data are "local", long data are allocated
    ParamValue pvint ("", TypeInt, 1, nullptr);
    OIIO_CHECK_ASSERT (pvint.datasize() == 4);
    OIIO_CHECK_ASSERT (! pvint.is_nonlocal());
    ParamValue pvcolor ("", TypeColor, 1, nullptr);
    OIIO_CHECK_ASSERT (pvcolor.datasize() == 12);
    OIIO_CHECK_ASSERT (! pvcolor.is_nonlocal());
    ParamValue pvmatrix ("", TypeMatrix, 1, nullptr);
    OIIO_CHECK_ASSERT (pvmatrix.datasize() == 64);
    OIIO_CHECK_ASSERT (pvmatrix.is_nonlocal());
}



static std::string
list_test (const std::string &data, TypeDesc type)
{
    ParamValue p ("name", type, data);
    return p.get_string();
}



void test_from_string ()
{
    std::cout << "test_from_string\n";
    TypeDesc type = TypeInt;
    std::string ret, data, invalid_data;

    data = "142";
    OIIO_CHECK_EQUAL (data, list_test(data,type));

    type = TypeFloat;
    data = "1.23";
    OIIO_CHECK_EQUAL (data, list_test(data,type));

    type = TypeDesc(TypeDesc::FLOAT, 5);
    data = "1.23, 34.23, 35.11, 99.99, 1999.99";
    OIIO_CHECK_EQUAL (data, list_test(data,type));

    type = TypeDesc::UINT64;
    data = "18446744073709551615";
    OIIO_CHECK_EQUAL (data, list_test(data,type));

    type = TypeMatrix;
    data = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16";
    OIIO_CHECK_EQUAL (data, list_test(data,type));

    type = TypeString;
    data = "foo";
    OIIO_CHECK_EQUAL (data, list_test(data,type));
}



static void
test_paramlist ()
{
    std::cout << "test_paramlist\n";
    ParamValueList pl;
    pl.emplace_back ("foo", int(42));
    pl.emplace_back ("pi", float(M_PI));
    pl.emplace_back ("bar", "barbarbar?");

    OIIO_CHECK_EQUAL (pl.get_int("foo"), 42);
    OIIO_CHECK_EQUAL (pl.get_int("pi",4), 4);  // should fail int
    OIIO_CHECK_EQUAL (pl.get_float("pi"), float(M_PI));
    OIIO_CHECK_EQUAL (pl.get_int("bar"), 0);
    OIIO_CHECK_EQUAL (pl.get_int("bar"), 0);
    OIIO_CHECK_EQUAL (pl.get_string("bar"), "barbarbar?");
    OIIO_CHECK_ASSERT(pl.find("foo") != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo") == pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo", TypeDesc::UNKNOWN, false) != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo", TypeDesc::UNKNOWN, true) == pl.cend());
    OIIO_CHECK_ASSERT(pl.find("foo", TypeDesc::INT) != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("foo", TypeDesc::FLOAT) == pl.cend());

    OIIO_CHECK_ASSERT (pl.contains("foo"));
    OIIO_CHECK_ASSERT (! pl.contains("nonono"));
    pl.remove ("foo");
    OIIO_CHECK_ASSERT (! pl.contains("foo"));
    OIIO_CHECK_ASSERT (pl.contains("bar"));
}



int main (int argc, char *argv[])
{
    std::cout << "ParamValue size = " << sizeof(ParamValue) << "\n";
    test_value_types ();
    test_from_string ();
    test_paramlist ();

    return unit_test_failures;
}
