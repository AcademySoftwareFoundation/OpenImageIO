// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <OpenImageIO/optparser.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;


class MySystem {
public:
    MySystem()
        : i(0)
        , f(0)
    {
    }

    bool attribute(const std::string& name, int value)
    {
        std::cout << "iattribute '" << name << "' = " << value << "\n";
        if (name == "i") {
            i = value;
            return true;
        }
        return false;
    }
    bool attribute(const std::string& name, float value)
    {
        std::cout << "fattribute '" << name << "' = " << value << "\n";
        if (name == "f") {
            f = value;
            return true;
        }
        return false;
    }
    bool attribute(const std::string& name, const std::string& value)
    {
        std::cout << "sattribute '" << name << "' = '" << value << "'\n";
        if (name == "s") {
            s = value;
            return true;
        }
        return false;
    }

    int i;
    float f;
    std::string s;
};



void
test_optparser()
{
    MySystem sys;
    optparser(sys, "i=14");
    OIIO_CHECK_EQUAL(sys.i, 14);
    optparser(sys, "i=-28");
    OIIO_CHECK_EQUAL(sys.i, -28);

    optparser(sys, "f=6.28");
    OIIO_CHECK_EQUAL(sys.f, 6.28f);
    optparser(sys, "f=-56.0");
    OIIO_CHECK_EQUAL(sys.f, -56.0f);
    optparser(sys, "f=-1.");
    OIIO_CHECK_EQUAL(sys.f, -1.0f);

    optparser(sys, "s=foo");
    OIIO_CHECK_EQUAL(sys.s, "foo");
    optparser(sys, "s=\"foo, bar\"");
    OIIO_CHECK_EQUAL(sys.s, "foo, bar");

    optparser(sys, "f=256.29,s=\"phone call\",i=100");
    OIIO_CHECK_EQUAL(sys.i, 100);
    OIIO_CHECK_EQUAL(sys.f, 256.29f);
    OIIO_CHECK_EQUAL(sys.s, "phone call");
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_optparser();

    return unit_test_failures;
}
