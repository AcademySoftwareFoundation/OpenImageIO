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

#include <sstream>
#include <fstream>

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
#if _WIN32
# define SEPARATOR "\\"
#else
# define SEPARATOR "/"
#endif

    // This will be run via testsuite/unit_filesystem, from the
    // build/ARCH/src/libOpenImageIO directory.  Two levels up will be
    // build/ARCH.
    std::vector<std::string> dirs;
    dirs.push_back (".." SEPARATOR "..");
    std::string s;

    // non-recursive search success
    s = Filesystem::searchpath_find ("License.txt", dirs, false, false);
    OIIO_CHECK_EQUAL (s, ".." SEPARATOR ".." SEPARATOR "License.txt");

    // non-recursive search failure (file is in a subdirectory)
    s = Filesystem::searchpath_find ("version.h", dirs, false, false);
    OIIO_CHECK_EQUAL (s, "");

    // recursive search success (file is in a subdirectory)
    s = Filesystem::searchpath_find ("version.h", dirs, false, true);
    OIIO_CHECK_EQUAL (s, ".." SEPARATOR ".." SEPARATOR "include" SEPARATOR "version.h");
}



static void
test_seq (const char *str, const char *expected)
{
    std::vector<int> sequence;
    Filesystem::enumerate_sequence (str, sequence);
    std::stringstream joined;
    for (size_t i = 0;  i < sequence.size();  ++i) {
        if (i)
            joined << " ";
        joined << sequence[i];
    }
    std::cout << "  \"" << str << "\" -> " << joined.str() << "\n";
    OIIO_CHECK_EQUAL (joined.str(), std::string(expected));
}



static void
test_file_seq (const char *pattern, const char *override,
               const std::string &expected)
{
    std::vector<int> numbers;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    if (override && strlen(override) > 0)
        frame_range = override;
    Filesystem::enumerate_sequence(frame_range.c_str(), numbers);
    Filesystem::enumerate_file_sequence (normalized_pattern, numbers, names);
    std::string joined = Strutil::join(names, " ");
    std::cout << "  " << pattern;
    if (override)
        std::cout << " + " << override;
    std::cout << " -> " << joined << "\n";
    OIIO_CHECK_EQUAL (joined, expected);
}



static void
test_scan_file_seq (const char *pattern, const std::string &expected)
{
    std::vector<int> numbers;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames (normalized_pattern, numbers, names);
    std::string joined = Strutil::join(names, " ");
    std::cout << "  " << pattern;
    std::cout << " -> " << joined << "\n";
    OIIO_CHECK_EQUAL (joined, expected);
}



void test_frame_sequences ()
{
    std::cout << "Testing frame number sequences:\n";
    test_seq ("3", "3");
    test_seq ("1-5", "1 2 3 4 5");
    test_seq ("5-1", "5 4 3 2 1");
    test_seq ("1-3,6,10-12", "1 2 3 6 10 11 12");
    test_seq ("1-5x2", "1 3 5");
    test_seq ("1-5y2", "2 4");
    std::cout << "\n";

    test_file_seq ("foo.1-5#.exr", NULL, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq ("foo.5-1#.exr", NULL, "foo.0005.exr foo.0004.exr foo.0003.exr foo.0002.exr foo.0001.exr");
    test_file_seq ("foo.1-3,6,10-12#.exr", NULL, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0006.exr foo.0010.exr foo.0011.exr foo.0012.exr");
    test_file_seq ("foo.1-5x2#.exr", NULL, "foo.0001.exr foo.0003.exr foo.0005.exr");
    test_file_seq ("foo.1-5y2#.exr", NULL, "foo.0002.exr foo.0004.exr");

    test_file_seq ("foo.#.exr", "1-5", "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq ("foo.#.exr", "1-5x2", "foo.0001.exr foo.0003.exr foo.0005.exr");

    test_file_seq ("foo.1-3@@.exr", NULL, "foo.01.exr foo.02.exr foo.03.exr");
    test_file_seq ("foo.1-3@#.exr", NULL, "foo.00001.exr foo.00002.exr foo.00003.exr");

    test_file_seq ("foo.1-5%04d.exr", NULL, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq ("foo.%04d.exr", "1-5", "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq ("foo.%4d.exr", "1-5", "foo.   1.exr foo.   2.exr foo.   3.exr foo.   4.exr foo.   5.exr");
    test_file_seq ("foo.%d.exr", "1-5", "foo.1.exr foo.2.exr foo.3.exr foo.4.exr foo.5.exr");
    std::cout << "\n";
}



void test_scan_sequences ()
{
    std::cout << "Testing frame sequence scanning:\n";

    std::vector< std::string > filenames;

    for (size_t i = 1; i <= 5; i++) {
        std::string fn = Strutil::format ("foo.%04d.exr", i);
        filenames.push_back (fn);
        std::ofstream f(fn.c_str());
        f.close();
    }

#ifdef _WIN32
    test_scan_file_seq ("foo.#.exr", ".\\foo.0001.exr .\\foo.0002.exr .\\foo.0003.exr .\\foo.0004.exr .\\foo.0005.exr");
#else
    test_scan_file_seq ("foo.#.exr", "./foo.0001.exr ./foo.0002.exr ./foo.0003.exr ./foo.0004.exr ./foo.0005.exr");
#endif
}



int main (int argc, char *argv[])
{
    test_filename_decomposition ();
    test_filename_searchpath_find ();
    test_frame_sequences ();
    test_scan_sequences ();

    return unit_test_failures;
}
