/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include "imageio.h"
#include "unittest.h"

OIIO_NAMESPACE_USING;

void test_get_rest_arguments ()
{
    int ret;
    std::map <std::string, std::string> result;
    std::string base;
    std::string url = "someplace?arg1=value1&arg2=value2";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, true);
    OIIO_CHECK_EQUAL (base, "someplace");
    OIIO_CHECK_EQUAL (result["arg1"], "value1");
    OIIO_CHECK_EQUAL (result["arg2"], "value2");
    OIIO_CHECK_EQUAL (result["arg3"], "");

    result.clear();
    url = "?arg1=value1&arg2=value2";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, true);
    OIIO_CHECK_EQUAL (base, "");
    OIIO_CHECK_EQUAL (result["arg1"], "value1");
    OIIO_CHECK_EQUAL (result["arg2"], "value2");

    result.clear();
    url = "arg1=value1&arg2=value2";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, true);
    OIIO_CHECK_EQUAL (base, "arg1=value1&arg2=value2");
    OIIO_CHECK_EQUAL (result["arg1"], "");
    OIIO_CHECK_EQUAL (result["arg2"], "");

    result.clear();
    url = "";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, true);
    OIIO_CHECK_EQUAL (base, "");
    OIIO_CHECK_EQUAL (result["arg1"], "");
    OIIO_CHECK_EQUAL (result["arg2"], "");

    result.clear();
    url = "sometextwithoutasense????&&&&&arg4=val1";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, false);
    OIIO_CHECK_EQUAL (base, "sometextwithoutasense");
    OIIO_CHECK_EQUAL (result["arg1"], "");
    OIIO_CHECK_EQUAL (result["arg2"], "");
    OIIO_CHECK_EQUAL (result["arg4"], "");

    result.clear();
    url = "atext?arg1value1&arg2value2";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, false);
    OIIO_CHECK_EQUAL (base, "atext");
    OIIO_CHECK_EQUAL (result["arg1"], "");
    OIIO_CHECK_EQUAL (result["arg2"], "");

    result.clear();
    url = "atext?arg1=value1&arg2value2";
    result["arg2"] = "somevalue";
    ret = Strutil::get_rest_arguments (url, base, result);
    OIIO_CHECK_EQUAL (ret, false);
    OIIO_CHECK_EQUAL (base, "atext");
    OIIO_CHECK_EQUAL (result["arg1"], "value1");
    OIIO_CHECK_EQUAL (result["arg2"], "somevalue");
}



int main (int argc, char *argv[])
{
    test_get_rest_arguments ();

    return unit_test_failures;
}
