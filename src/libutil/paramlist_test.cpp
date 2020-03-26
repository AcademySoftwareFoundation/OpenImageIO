// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <limits>

#include <OpenEXR/ImathColor.h>
#include <OpenEXR/ImathMatrix.h>
#include <OpenEXR/ImathVec.h>

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;


// Helper: create a single ParamValue, store data in it, make sure we can
// extract each element again, make sure we can convert to the appropriate
// int or float, and also return a string representation.
template<typename T>
static std::string
test_numeric(T* data, int num_elements, TypeDesc type)
{
    ParamValue p("name", type, num_elements, data);
    int n = type.numelements() * num_elements;
    for (int i = 0; i < n; ++i)
        OIIO_CHECK_EQUAL(p.get<T>(i), ((const T*)data)[i]);
    if (std::numeric_limits<T>::is_integer) {
        OIIO_CHECK_EQUAL(p.get_int(), int(data[0]));
        for (int i = 0; i < n; ++i)
            OIIO_CHECK_EQUAL(p.get_int_indexed(i), int(data[i]));
    } else {
        OIIO_CHECK_EQUAL(p.get_float(), float(data[0]));
        for (int i = 0; i < n; ++i)
            OIIO_CHECK_EQUAL(p.get_float_indexed(i), float(data[i]));
    }
    return p.get_string();
}



// Create ParamValue of various types and make sure it copies the data in
// and out.
void
test_value_types()
{
    std::cout << "test_value_types\n";
    std::string ret;

    {
        int val = 42;
        ret     = test_numeric(&val, 1, TypeDesc::INT);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        unsigned int val = 42;
        ret              = test_numeric(&val, 1, TypeDesc::UINT);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        short val = 42;
        ret       = test_numeric(&val, 1, TypeDesc::INT16);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        unsigned short val = 42;
        ret                = test_numeric(&val, 1, TypeDesc::UINT16);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        char val = 42;
        ret      = test_numeric(&val, 1, TypeDesc::INT8);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        unsigned char val = 42;
        ret               = test_numeric(&val, 1, TypeDesc::UINT8);
        OIIO_CHECK_EQUAL(ret, "42");
    }

    {
        float val = 2.25;
        ret       = test_numeric(&val, 1, TypeDesc::FLOAT);
        OIIO_CHECK_EQUAL(ret, "2.25");
    }

    {
        double val = 2.25;
        ret        = test_numeric(&val, 1, TypeDesc::DOUBLE);
        OIIO_CHECK_EQUAL(ret, "2.25");
    }

    {
        half val = 2.25;
        ret      = test_numeric(&val, 1, TypeDesc::HALF);
        OIIO_CHECK_EQUAL(ret, "2.25");
    }

    {
        const char* val = "hello";
        ParamValue p("name", val);
        OIIO_CHECK_EQUAL(p.get<ustring>(), "hello");
        OIIO_CHECK_EQUAL(p.get_string(), "hello");
    }

    {
        const int* ptr = (const int*)0xdeadbeef;
        ParamValue p("name", TypeDesc::PTR, 1, &ptr);
        OIIO_CHECK_EQUAL(p.get<void*>(), ptr);
        OIIO_CHECK_EQUAL(p.get_string(), "0xdeadbeef");
    }

    {
        int imatrix[] = { 100, 200, 300, 400 };
        ret           = test_numeric(&imatrix[0], 1, TypeInt);
        OIIO_CHECK_EQUAL(ret, "100");
        ret = test_numeric(imatrix, sizeof(imatrix) / sizeof(int), TypeInt);
        OIIO_CHECK_EQUAL(ret, "100, 200, 300, 400");
        OIIO_CHECK_NE(ret, "100, 200, 300, 400,");
        // Test it as an array as well
        ret = test_numeric(&imatrix[0], 1, TypeDesc(TypeDesc::INT, 4));
        OIIO_CHECK_EQUAL(ret, "100, 200, 300, 400");
    }

    {
        float fmatrix[] = { 10.12f, 200.34f, 300.11f, 400.9f };
        ret             = test_numeric(&fmatrix[0], 1, TypeFloat);
        OIIO_CHECK_EQUAL(ret, "10.12");
        ret = test_numeric(fmatrix, sizeof(fmatrix) / sizeof(float), TypeFloat);
        OIIO_CHECK_EQUAL(ret, "10.12, 200.34, 300.11, 400.9");
        OIIO_CHECK_NE(ret, "10, 200, 300, 400");
        OIIO_CHECK_NE(ret, "10.12, 200.34, 300.11, 400.9,");
        ret = test_numeric(&fmatrix[0], 1, TypeDesc(TypeDesc::FLOAT, 4));
        OIIO_CHECK_EQUAL(ret, "10.12, 200.34, 300.11, 400.9");
    }

    {
        unsigned long long ullmatrix[] = { 0xffffffffffffffffULL,
                                           0xffffffffffffffffULL };
        ret = test_numeric(&ullmatrix[0], 1, TypeDesc::UINT64);
        OIIO_CHECK_EQUAL(ret, "18446744073709551615");
        ret = test_numeric(ullmatrix,
                           sizeof(ullmatrix) / sizeof(unsigned long long),
                           TypeDesc::UINT64);
        OIIO_CHECK_EQUAL(ret, "18446744073709551615, 18446744073709551615");
        OIIO_CHECK_NE(ret, "-1, -1");
        OIIO_CHECK_NE(ret, "18446744073709551615, 18446744073709551615,");
    }

    {
        const char* smatrix[] = { "this is \"a test\"",
                                  "this is another test" };

        ParamValue p("name", smatrix[0]);
        OIIO_CHECK_EQUAL(p.get<ustring>(), smatrix[0]);
        OIIO_CHECK_EQUAL(p.get_string(), smatrix[0]);

        ParamValue q("name", TypeString, sizeof(smatrix) / sizeof(char*),
                     &smatrix);
        OIIO_CHECK_EQUAL(q.get_string(),
                         "\"this is \\\"a test\\\"\", \"this is another test\"");
    }

    {
        float matrix16[2][16] = {
            { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
            { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 }
        };
        ParamValue p("name", TypeMatrix, 1, matrix16);
        std::string s = p.get_string();
        OIIO_CHECK_EQUAL(
            s, "1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16");
        ParamValue q("name", TypeMatrix,
                     sizeof(matrix16) / (16 * sizeof(float)), matrix16);
        OIIO_CHECK_EQUAL(
            q.get_string(),
            "1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25");
    }

    // Test rational
    {
        int rat[2] = { 1, 2 };
        ParamValue p("name", TypeRational, 1, rat);
        // make sure we can retrieve it as int[2] (numerator, denominator)
        OIIO_CHECK_EQUAL(p.get<int>(0), rat[0]);
        OIIO_CHECK_EQUAL(p.get<int>(1), rat[1]);
        // make sure we can retrieve rational as float, with conversion
        OIIO_CHECK_EQUAL(p.get_float(), 0.5f);
        // make sure we can retrieve rational as nicely formatted string
        OIIO_CHECK_EQUAL(p.get_string(), "1/2");
    }

    // Double check that short data are "local", long data are allocated
    ParamValue pvint("", TypeInt, 1, nullptr);
    OIIO_CHECK_ASSERT(pvint.datasize() == 4);
    OIIO_CHECK_ASSERT(!pvint.is_nonlocal());
    ParamValue pvcolor("", TypeColor, 1, nullptr);
    OIIO_CHECK_ASSERT(pvcolor.datasize() == 12);
    OIIO_CHECK_ASSERT(!pvcolor.is_nonlocal());
    ParamValue pvmatrix("", TypeMatrix, 1, nullptr);
    OIIO_CHECK_ASSERT(pvmatrix.datasize() == 64);
    OIIO_CHECK_ASSERT(pvmatrix.is_nonlocal());
}



static std::string
list_test(const std::string& data, TypeDesc type)
{
    ParamValue p("name", type, data);
    return p.get_string();
}



void
test_from_string()
{
    std::cout << "test_from_string\n";
    TypeDesc type = TypeInt;
    std::string ret, data, invalid_data;

    data = "142";
    OIIO_CHECK_EQUAL(data, list_test(data, type));

    type = TypeFloat;
    data = "1.23";
    OIIO_CHECK_EQUAL(data, list_test(data, type));

    type = TypeDesc(TypeDesc::FLOAT, 5);
    data = "1.23, 34.23, 35.11, 99.99, 1999.99";
    OIIO_CHECK_EQUAL(data, list_test(data, type));

    type = TypeDesc::UINT64;
    data = "18446744073709551615";
    OIIO_CHECK_EQUAL(data, list_test(data, type));

    type = TypeMatrix;
    data = "1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16";
    OIIO_CHECK_EQUAL(data, list_test(data, type));

    type = TypeString;
    data = "foo";
    OIIO_CHECK_EQUAL(data, list_test(data, type));
}



static void
test_paramlist()
{
    std::cout << "test_paramlist\n";
    ParamValueList pl;
    pl.emplace_back("foo", int(42));
    pl.emplace_back("pi", float(M_PI));
    pl.emplace_back("bar", "barbarbar?");

    OIIO_CHECK_EQUAL(pl.get_int("foo"), 42);
    OIIO_CHECK_EQUAL(pl.get_int("pi", 4), 4);  // should fail int
    OIIO_CHECK_EQUAL(pl.get_float("pi"), float(M_PI));
    OIIO_CHECK_EQUAL(pl.get_int("bar"), 0);
    OIIO_CHECK_EQUAL(pl.get_int("bar"), 0);
    OIIO_CHECK_EQUAL(pl.get_string("bar"), "barbarbar?");
    OIIO_CHECK_ASSERT(pl.find("foo") != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo") == pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo", TypeDesc::UNKNOWN, false) != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("Foo", TypeDesc::UNKNOWN, true) == pl.cend());
    OIIO_CHECK_ASSERT(pl.find("foo", TypeDesc::INT) != pl.cend());
    OIIO_CHECK_ASSERT(pl.find("foo", TypeDesc::FLOAT) == pl.cend());

    OIIO_CHECK_ASSERT(pl.contains("foo"));
    OIIO_CHECK_ASSERT(!pl.contains("nonono"));
    pl.remove("foo");
    OIIO_CHECK_ASSERT(!pl.contains("foo"));
    OIIO_CHECK_ASSERT(pl.contains("bar"));

    {
        // Check merge
        ParamValueList list1, list2;
        list1.emplace_back("b", 2);
        list1.emplace_back("c", 3);
        list1.emplace_back("a", 1);
        list2.emplace_back("d", 11);
        list2.emplace_back("c", 10);
        list1.merge(list2, /*override=*/false);
        OIIO_CHECK_EQUAL(list1.size(), 4);
        OIIO_CHECK_EQUAL(list1.get_int("a"), 1);
        OIIO_CHECK_EQUAL(list1.get_int("b"), 2);
        OIIO_CHECK_EQUAL(list1.get_int("c"), 3);
        OIIO_CHECK_EQUAL(list1.get_int("d"), 11);
        list1.merge(list2, /*override=*/true);
        OIIO_CHECK_EQUAL(list1.size(), 4);
        OIIO_CHECK_EQUAL(list1.get_int("a"), 1);
        OIIO_CHECK_EQUAL(list1.get_int("b"), 2);
        OIIO_CHECK_EQUAL(list1.get_int("c"), 10);
        OIIO_CHECK_EQUAL(list1.get_int("d"), 11);

        // Check sort
        OIIO_CHECK_EQUAL(list1[0].name(), "b");
        OIIO_CHECK_EQUAL(list1[1].name(), "c");
        OIIO_CHECK_EQUAL(list1[2].name(), "a");
        OIIO_CHECK_EQUAL(list1[3].name(), "d");
        list1.sort();
        OIIO_CHECK_EQUAL(list1[0].name(), "a");
        OIIO_CHECK_EQUAL(list1[1].name(), "b");
        OIIO_CHECK_EQUAL(list1[2].name(), "c");
        OIIO_CHECK_EQUAL(list1[3].name(), "d");
    }
}



static void
test_delegates()
{
    std::cout << "test_delegates\n";
    ParamValueList pl;
    pl["foo"]  = 42;
    pl["pi"]   = float(M_PI);
    pl["bar"]  = "barbarbar?";
    pl["bar2"] = std::string("barbarbar?");
    pl["bar3"] = ustring("barbarbar?");
    pl["bar4"] = string_view("barbarbar?");
    pl["red"]  = Imath::Color3f(1.0f, 0.0f, 0.0f);
    pl["xy"]   = Imath::V3f(0.5f, 0.5f, 0.0f);
    pl["Tx"]   = Imath::M44f(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 42, 0, 0, 1);

    OIIO_CHECK_EQUAL(pl["absent"].get<int>(), 0);
    OIIO_CHECK_EQUAL(pl["absent"].type(), TypeUnknown);
    OIIO_CHECK_EQUAL(pl["foo"].get<int>(), 42);
    OIIO_CHECK_EQUAL(pl["foo"].type(), TypeInt);
    OIIO_CHECK_EQUAL(pl["foo"].as_string(), "42");
    OIIO_CHECK_EQUAL(pl["pi"].get<float>(), float(M_PI));
    OIIO_CHECK_EQUAL(pl["bar"].get<std::string>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar"].get<string_view>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar"].get<ustring>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar"].as_string(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar2"].get<std::string>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar3"].get<std::string>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["bar4"].get<std::string>(), "barbarbar?");
    OIIO_CHECK_EQUAL(pl["red"].get<Imath::Color3f>(),
                     Imath::Color3f(1.0f, 0.0f, 0.0f));
    std::vector<float> redvec { 1.0f, 0.0f, 0.0f };
    OIIO_CHECK_EQUAL(pl["red"].as_vec<float>(), redvec);
    OIIO_CHECK_EQUAL(pl["red"].get_indexed<float>(0), 1.0f);
    OIIO_CHECK_EQUAL(pl["red"].get_indexed<float>(1), 0.0f);
    OIIO_CHECK_EQUAL(pl["red"].get_indexed<float>(2), 0.0f);
    OIIO_CHECK_EQUAL(pl["xy"].get<Imath::V3f>(), Imath::V3f(0.5f, 0.5f, 0.0f));
    OIIO_CHECK_EQUAL(pl["Tx"].get<Imath::M44f>(),
                     Imath::M44f(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 42, 0, 0,
                                 1));
    std::string s = pl["foo"];
    OIIO_CHECK_EQUAL(s, "42");

    string_view sv = pl["foo"].get();
    OIIO_CHECK_EQUAL(sv, "42");

    Strutil::printf("Delegate-loaded array is\n");
    for (auto&& p : pl)
        Strutil::printf(" %16s : %s\n", p.name(), p.get_string());
    Strutil::printf("\n");
}



int
main(int /*argc*/, char* /*argv*/[])
{
    std::cout << "ParamValue size = " << sizeof(ParamValue) << "\n";
    test_value_types();
    test_from_string();
    test_paramlist();
    test_delegates();

    return unit_test_failures;
}
