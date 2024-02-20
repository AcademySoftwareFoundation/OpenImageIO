// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <limits>
#include <type_traits>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/type_traits.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;
using OIIO::Strutil::print;


struct test {
    std::string size() { return ""; }
};



int
main(int /*argc*/, char* /*argv*/[])
{
    print("type_traits test\n");

    // Test has_size_method
    {
        // std::string has a size() method
        OIIO_CHECK_EQUAL(has_size_method<std::string>::value, true);
        // int does not have a size() method
        OIIO_CHECK_EQUAL(has_size_method<int>::value, false);
        // struct test has a size method, but it returns a non-integral type
        OIIO_CHECK_EQUAL(has_size_method<test>::value, false);
    }

    // Test has_subscript
    {
        // std::string has operator[]
        OIIO_CHECK_EQUAL(has_subscript<std::string>::value, true);
        // int does not have operator[]
        OIIO_CHECK_EQUAL(has_subscript<int>::value, false);
        // struct test does not have operator[]
        OIIO_CHECK_EQUAL(has_subscript<test>::value, false);
    }

    return unit_test_failures;
}
