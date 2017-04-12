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


#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

#include <OpenImageIO/platform.h>
#include <OpenImageIO/optparser.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;


class MySystem {
public:
    MySystem() : i(0), f(0) { }

    bool attribute (const std::string &name, int value) {
        std::cout << "iattribute '" << name << "' = " << value << "\n";
        if (name == "i") {
            i = value;
            return true;
        }
        return false;
    }
    bool attribute (const std::string &name, float value) {
        std::cout << "fattribute '" << name << "' = " << value << "\n";
        if (name == "f") {
            f = value;
            return true;
        }
        return false;
    }
    bool attribute (const std::string &name, const std::string &value) {
        std::cout << "sattribute '" << name << "' = '" << value << "'\n";
        if (name == "s") {
            s = value;  return true;
        }
        return false;
    }

    int i;
    float f;
    std::string s;
};



void test_optparser ()
{
    MySystem sys;
    optparser (sys, "i=14");
    OIIO_CHECK_EQUAL (sys.i, 14);
    optparser (sys, "i=-28");
    OIIO_CHECK_EQUAL (sys.i, -28);

    optparser (sys, "f=6.28");
    OIIO_CHECK_EQUAL (sys.f, 6.28f);
    optparser (sys, "f=-56.0");
    OIIO_CHECK_EQUAL (sys.f, -56.0f);
    optparser (sys, "f=-1.");
    OIIO_CHECK_EQUAL (sys.f, -1.0f);

    optparser (sys, "s=foo");
    OIIO_CHECK_EQUAL (sys.s, "foo");
    optparser (sys, "s=\"foo, bar\"");
    OIIO_CHECK_EQUAL (sys.s, "foo, bar");

    optparser (sys, "f=256.29,s=\"phone call\",i=100");
    OIIO_CHECK_EQUAL (sys.i, 100);
    OIIO_CHECK_EQUAL (sys.f, 256.29f);
    OIIO_CHECK_EQUAL (sys.s, "phone call");
}



int main (int argc, char *argv[])
{
    test_optparser ();

    return unit_test_failures;
}
