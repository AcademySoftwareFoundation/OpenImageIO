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

#include "OpenImageIO/imageio.h"
#include "OpenImageIO/filesystem.h"
#include "OpenImageIO/unittest.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

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
    dirs.push_back (".." SEPARATOR ".." SEPARATOR "cpack");
    std::string s;

    // non-recursive search success
    s = Filesystem::searchpath_find ("License.txt", dirs, false, false);
    OIIO_CHECK_EQUAL (s, ".." SEPARATOR ".." SEPARATOR "cpack" SEPARATOR "License.txt");

    // non-recursive search failure (file is in a subdirectory)
    s = Filesystem::searchpath_find ("oiioversion.h", dirs, false, false);
    OIIO_CHECK_EQUAL (s, "");

    // recursive search success (file is in a subdirectory)
    s = Filesystem::searchpath_find ("oiioversion.h", dirs, false, true);
    OIIO_CHECK_EQUAL (s, ".." SEPARATOR ".." SEPARATOR "include" SEPARATOR "OpenImageIO" SEPARATOR "oiioversion.h");
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
test_file_seq_with_view (const char *pattern, const char *override, const char *view,
                         const std::string &expected)
{
    std::vector<int> numbers;
    std::vector<string_view> views;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    if (override && strlen(override) > 0)
        frame_range = override;
    Filesystem::enumerate_sequence(frame_range.c_str(), numbers);

    if (view) {
        for (size_t i = 0, e = numbers.size(); i < e; ++i)
            views.push_back(view);
    }

    Filesystem::enumerate_file_sequence (normalized_pattern, numbers, views, names);
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

    // Check that we don't crash from exceptions generated by strangely
    // formed patterns.
    const char *weird = "{'cpu_model': 'Intel(R) Xeon(R) CPU E5-2630 @ 2.30GHz'}";
    Filesystem::parse_pattern (weird, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames (normalized_pattern, numbers, names);
    OIIO_CHECK_EQUAL (names.size(), 0);
    // If we didn't crash above, we're ok!
}



static void
test_scan_file_seq_with_views (const char *pattern, const char **views_, const std::string &expected)
{
    std::vector<int> frame_numbers;
    std::vector<string_view> frame_views;
    std::vector<std::string> frame_names;
    std::string normalized_pattern;
    std::string frame_range;
    std::vector<string_view> views;

    for (size_t i = 0; views_[i]; ++i)
        views.push_back(views_[i]);

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames (normalized_pattern, views, frame_numbers, frame_views, frame_names);
    std::string joined = Strutil::join(frame_names, " ");
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

    const char *views1[] = { "left", "right", "foo", "", NULL };
    for (size_t i = 0; i < 5; ++i) {
        const char *view = views1[i];
        test_file_seq_with_view ("foo.1-5#.exr", NULL, view, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view ("foo.5-1#.exr", NULL, view, "foo.0005.exr foo.0004.exr foo.0003.exr foo.0002.exr foo.0001.exr");
        test_file_seq_with_view ("foo.1-3,6,10-12#.exr", NULL, view, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0006.exr foo.0010.exr foo.0011.exr foo.0012.exr");
        test_file_seq_with_view ("foo.1-5x2#.exr", NULL, view, "foo.0001.exr foo.0003.exr foo.0005.exr");
        test_file_seq_with_view ("foo.1-5y2#.exr", NULL, view, "foo.0002.exr foo.0004.exr");

        test_file_seq_with_view ("foo.#.exr", "1-5", view, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view ("foo.#.exr", "1-5x2", view, "foo.0001.exr foo.0003.exr foo.0005.exr");

        test_file_seq_with_view ("foo.1-3@@.exr", NULL, view, "foo.01.exr foo.02.exr foo.03.exr");
        test_file_seq_with_view ("foo.1-3@#.exr", NULL, view, "foo.00001.exr foo.00002.exr foo.00003.exr");

        test_file_seq_with_view ("foo.1-5%04d.exr", NULL, view, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view ("foo.%04d.exr", "1-5", view, "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view ("foo.%4d.exr", "1-5", view, "foo.   1.exr foo.   2.exr foo.   3.exr foo.   4.exr foo.   5.exr");
        test_file_seq_with_view ("foo.%d.exr", "1-5", view, "foo.1.exr foo.2.exr foo.3.exr foo.4.exr foo.5.exr");
    }

//    test_file_seq_with_view ("%V.%04d", NULL, NULL, "");
//    test_file_seq_with_view ("%v", NULL, NULL, "");
//    test_file_seq_with_view ("%V", NULL, "", "");
//    test_file_seq_with_view ("%v", NULL, "", "");
//    test_file_seq_with_view ("%V", NULL, "left", "left");
//    test_file_seq_with_view ("%V", NULL, "right", "right");
//    test_file_seq_with_view ("%v", NULL, "left", "l");
//    test_file_seq_with_view ("%v", NULL, "right", "r");
    test_file_seq_with_view ("foo_%V.1-2#.exr", NULL, "left", "foo_left.0001.exr foo_left.0002.exr");
    test_file_seq_with_view ("%V/foo_%V.1-2#.exr", NULL, "left", "left/foo_left.0001.exr left/foo_left.0002.exr");
    test_file_seq_with_view ("%v/foo_%V.1-2#.exr", NULL, "left", "l/foo_left.0001.exr l/foo_left.0002.exr");
    test_file_seq_with_view ("%V/foo_%v.1-2#.exr", NULL, "left", "left/foo_l.0001.exr left/foo_l.0002.exr");
    test_file_seq_with_view ("%v/foo_%v.1-2#.exr", NULL, "left", "l/foo_l.0001.exr l/foo_l.0002.exr");

    std::cout << "\n";
}



void create_test_file(const string_view& fn)
{
    std::ofstream f(fn.c_str());
    f.close();
}



void test_scan_sequences ()
{
    std::cout << "Testing frame sequence scanning:\n";

    std::vector< std::string > filenames;

    for (size_t i = 1; i <= 5; i++) {
        std::string fn = Strutil::format ("foo.%04d.exr", i);
        filenames.push_back (fn);
        create_test_file(fn);
    }

#ifdef _WIN32
    test_scan_file_seq ("foo.#.exr", ".\\foo.0001.exr .\\foo.0002.exr .\\foo.0003.exr .\\foo.0004.exr .\\foo.0005.exr");
#else
    test_scan_file_seq ("foo.#.exr", "./foo.0001.exr ./foo.0002.exr ./foo.0003.exr ./foo.0004.exr ./foo.0005.exr");
#endif

    filenames.clear();

#ifdef _WIN32
    CreateDirectory("left", NULL);
    CreateDirectory("left/l", NULL);
#else
    mkdir("left", 0777);
    mkdir("left/l", 0777);
#endif

    for (size_t i = 1; i <= 5; i++) {
        std::string fn = Strutil::format ("left/l/foo_left_l.%04d.exr", i);
        filenames.push_back (fn);
        create_test_file(fn);
    }

    const char *views[] = { "left", NULL };

#ifdef _WIN32
    test_scan_file_seq_with_views ("%V/%v/foo_%V_%v.#.exr", views, "left\\l\\foo_left_l.0001.exr left\\l\\foo_left_l.0002.exr left\\l\\foo_left_l.0003.exr left\\l\\foo_left_l.0004.exr left\\l\\foo_left_l.0005.exr");
#else
    test_scan_file_seq_with_views ("%V/%v/foo_%V_%v.#.exr", views, "left/l/foo_left_l.0001.exr left/l/foo_left_l.0002.exr left/l/foo_left_l.0003.exr left/l/foo_left_l.0004.exr left/l/foo_left_l.0005.exr");
#endif

    filenames.clear();

#ifdef _WIN32
    CreateDirectory("right", NULL);
    CreateDirectory("right/r", NULL);
#else
    mkdir("right", 0777);
    mkdir("right/r", 0777);
#endif

    std::string fn;

    fn = "left/l/foo_left_l";
    filenames.push_back(fn);
    create_test_file(fn);

    fn = "right/r/foo_right_r";
    filenames.push_back(fn);
    create_test_file(fn);

    const char *views2[] = { "left", "right", NULL };

#ifdef _WIN32
    test_scan_file_seq_with_views ("%V/%v/foo_%V_%v", views2, "left\\l\\foo_left_l right\\r\\foo_right_r");
#else
    test_scan_file_seq_with_views ("%V/%v/foo_%V_%v", views2, "left/l/foo_left_l right/r/foo_right_r");
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
