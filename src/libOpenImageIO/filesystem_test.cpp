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

#include "imageio.h"
#include "filesystem.h"
#include "unittest.h"

OIIO_NAMESPACE_USING;


void test_filename_decomposition ()
{
    std::cout << "filename(\"/directory/filename.ext\") = "
              << Filesystem::filename("/directory/filename.ext") << "\n";
    OIIO_CHECK_EQUAL (Filesystem::filename("/directory/filename.ext"),
                      "filename.ext");

    std::cout << "extension(\"/directory/filename.ext\") = "
              << Filesystem::extension("/directory/filename.ext") << "\n";
    OIIO_CHECK_EQUAL (Filesystem::extension("/directory/filename.ext"),
                      ".ext");
    OIIO_CHECK_EQUAL (Filesystem::extension("/directory/filename"),
                      "");
    OIIO_CHECK_EQUAL (Filesystem::extension("/directory/filename."),
                      ".");
}



void test_filename_searchpath_find ()
{
    // This will be run via testsuite/unit_filesystem, from the
    // build/ARCH/libOpenImageIO directory.  One level up will be
    // build/ARCH.
    std::vector<std::string> dirs;
    dirs.push_back ("..");
    std::string s;

    // non-recursive search success
    s = Filesystem::searchpath_find ("License.txt", dirs, false, false);
    OIIO_CHECK_EQUAL (s, "../License.txt");

    // non-recursive search failure (file is in a subdirectory)
    s = Filesystem::searchpath_find ("version.h", dirs, false, false);
    OIIO_CHECK_EQUAL (s, "");

    // recursive search success (file is in a subdirectory)
    s = Filesystem::searchpath_find ("version.h", dirs, false, true);
    OIIO_CHECK_EQUAL (s, "../include/version.h");
}



int main (int argc, char *argv[])
{
    test_filename_decomposition ();
    test_filename_searchpath_find ();

    return unit_test_failures;
}
