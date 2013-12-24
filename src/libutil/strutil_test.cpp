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

#include "strutil.h"
#include "unittest.h"

OIIO_NAMESPACE_USING;



void test_format ()
{
    // Test formatting
    OIIO_CHECK_EQUAL (Strutil::format ("%d %f %g", int(3), 3.14f, 3.14f),
                      "3 3.140000 3.14");
    OIIO_CHECK_EQUAL (Strutil::format ("'%s' '%s'", "foo", std::string("foo")),
                      "'foo' 'foo'");
    OIIO_CHECK_EQUAL (Strutil::format ("'%3d' '%03d' '%-3d'", 3, 3, 3),
                      "'  3' '003' '3  '");

    // Test '+' format modifier
    OIIO_CHECK_EQUAL (Strutil::format ("%+d%+d%+d", 3, -3, 0), "+3-3+0");

    // FIXME -- we should make comprehensive tests here
}



void test_memformat ()
{
    OIIO_CHECK_EQUAL (Strutil::memformat (15), "15 B");
    OIIO_CHECK_EQUAL (Strutil::memformat (15LL*1024), "15 KB");
    OIIO_CHECK_EQUAL (Strutil::memformat (15LL*1024*1024), "15.0 MB");
    OIIO_CHECK_EQUAL (Strutil::memformat (15LL*1024*1024*1024), "15.0 GB");
    OIIO_CHECK_EQUAL (Strutil::memformat (15LL*1024*1024+200000), "15.2 MB");
    OIIO_CHECK_EQUAL (Strutil::memformat (15LL*1024*1024+200000, 3), "15.191 MB");
}



void test_timeintervalformat ()
{
    OIIO_CHECK_EQUAL (Strutil::timeintervalformat (15.321), "15.3s");
    OIIO_CHECK_EQUAL (Strutil::timeintervalformat (150.321), "2m 30.3s");
    OIIO_CHECK_EQUAL (Strutil::timeintervalformat (15000.321), "4h 10m 0.3s");
    OIIO_CHECK_EQUAL (Strutil::timeintervalformat (150000.321), "1d 17h 40m 0.3s");
    OIIO_CHECK_EQUAL (Strutil::timeintervalformat (150.321, 2), "2m 30.32s");
}



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



void test_escape_sequences ()
{
    OIIO_CHECK_EQUAL (Strutil::unescape_chars("\\\\ \\n \\r \\017"),
                      "\\ \n \r \017");
    OIIO_CHECK_EQUAL (Strutil::escape_chars("\\ \n \r"),
                      "\\\\ \\n \\r");
}



void test_strip ()
{
    OIIO_CHECK_EQUAL (Strutil::strip ("abcdefbac", "abc"), "def");
    OIIO_CHECK_EQUAL (Strutil::strip ("defghi", "abc"), "defghi");
    OIIO_CHECK_EQUAL (Strutil::strip ("  \tHello, world\n"), "Hello, world");
    OIIO_CHECK_EQUAL (Strutil::strip (" \t"), "");
    OIIO_CHECK_EQUAL (Strutil::strip (""), "");
}



void test_safe_strcpy ()
{
    { // test in-bounds copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy (result, "A", 3);
        OIIO_CHECK_EQUAL (result[0], 'A');
        OIIO_CHECK_EQUAL (result[1],  0);
        OIIO_CHECK_EQUAL (result[2], '2'); // should not overwrite
        OIIO_CHECK_EQUAL (result[3], '3'); // should not overwrite
    }
    { // test over-bounds copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy (result, "ABC", 3);
        OIIO_CHECK_EQUAL (result[0], 'A');
        OIIO_CHECK_EQUAL (result[1], 'B');
        OIIO_CHECK_EQUAL (result[2],  0);
        OIIO_CHECK_EQUAL (result[3], '3'); // should not overwrite
    }
    { // test empty string copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy (result, "", 3);
        OIIO_CHECK_EQUAL (result[0], 0);
        OIIO_CHECK_EQUAL (result[1], '1'); // should not overwrite
        OIIO_CHECK_EQUAL (result[2], '2'); // should not overwrite
        OIIO_CHECK_EQUAL (result[3], '3'); // should not overwrite
    }
    { // test NULL case
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy (result, NULL, 3);
        OIIO_CHECK_EQUAL (result[0], 0);
        OIIO_CHECK_EQUAL (result[1], '1'); // should not overwrite
        OIIO_CHECK_EQUAL (result[2], '2'); // should not overwrite
        OIIO_CHECK_EQUAL (result[3], '3'); // should not overwrite
    }
}



int main (int argc, char *argv[])
{
    test_format ();
    test_memformat ();
    test_timeintervalformat ();
    test_get_rest_arguments ();
    test_escape_sequences ();
    test_strip ();
    test_safe_strcpy ();

    return unit_test_failures;
}
