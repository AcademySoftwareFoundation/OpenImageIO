// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <fstream>
#include <sstream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/unittest.h>

#ifndef _WIN32
#    include <sys/stat.h>
#endif

using namespace OIIO;


// This will be run via testsuite/unit_filesystem, from the
// build/ARCH/src/libOpenImageIO directory.  Two levels up will be
// build/ARCH.



void
test_filename_decomposition()
{
    std::string test("/directoryA/directory/filename.ext");

    std::cout << "Testing filename, extension, parent_path\n";
    OIIO_CHECK_EQUAL(Filesystem::filename(test), "filename.ext");
    OIIO_CHECK_EQUAL(Filesystem::extension(test), ".ext");
    OIIO_CHECK_EQUAL(Filesystem::extension("./foo.dir/../blah/./bar/file.ext"),
                     ".ext");
    OIIO_CHECK_EQUAL(Filesystem::extension("/directory/filename"), "");
    OIIO_CHECK_EQUAL(Filesystem::extension("/directory/filename."), ".");
    OIIO_CHECK_EQUAL(Filesystem::extension("a.foo"), ".foo");
    OIIO_CHECK_EQUAL(Filesystem::extension("a.foo", false), "foo");
    OIIO_CHECK_EQUAL(Filesystem::extension("foo"), "");
    OIIO_CHECK_EQUAL(Filesystem::extension("foo", false), "");
    OIIO_CHECK_EQUAL(Filesystem::extension(".foo"), "");
    OIIO_CHECK_EQUAL(Filesystem::extension(".foo", false), "");

    OIIO_CHECK_EQUAL(Filesystem::parent_path(test), "/directoryA/directory");

    std::cout << "Testing path_is_absolute\n";
    OIIO_CHECK_EQUAL(Filesystem::path_is_absolute("/foo/bar"), true);
    OIIO_CHECK_EQUAL(Filesystem::path_is_absolute("foo/bar"), false);
    OIIO_CHECK_EQUAL(Filesystem::path_is_absolute("../foo/bar"), false);

    std::cout << "Testing replace_extension\n";
    OIIO_CHECK_EQUAL(Filesystem::replace_extension(test, "foo"),
                     "/directoryA/directory/filename.foo");

    std::cout << "Testing generic_filepath\n";
#if _WIN32
    OIIO_CHECK_EQUAL(Filesystem::generic_filepath("\\x\\y"), "/x/y");
    OIIO_CHECK_EQUAL(Filesystem::generic_filepath("c:\\x\\y"), "c:/x/y");
#endif

    std::cout << "Testing filename_to_regex\n";
    OIIO_CHECK_EQUAL(Filesystem::filename_to_regex("/foo/bar/baz.exr"),
                     "/foo/bar/baz\\.exr");
    OIIO_CHECK_EQUAL(Filesystem::filename_to_regex("/f(o)o/b[a]r/b{a}z.exr"),
                     "/f\\(o\\)o/b\\[a\\]r/b\\{a\\}z\\.exr");
    OIIO_CHECK_EQUAL(Filesystem::filename_to_regex("/foo/bar/baz.*"),
                     "/foo/bar/baz\\..*");
    OIIO_CHECK_EQUAL(Filesystem::filename_to_regex("/fo?/b*r/b?z.*"),
                     "/fo.?/b.*r/b.?z\\..*");
}



void
test_filename_searchpath_find()
{
#if _WIN32
#    define DIRSEP "\\"
#else
#    define DIRSEP "/"
#endif
#define PATHSEP ":"
    std::string pathlist(".." DIRSEP ".." PATHSEP ".." DIRSEP ".." DIRSEP
                         "cpack" PATHSEP "foo/bar/baz");

    std::cout << "Testing searchpath_split\n";
    std::vector<std::string> dirs;

    // Split of empty string should make an empty path vector
    dirs.clear();
    Filesystem::searchpath_split("", dirs, false);
    OIIO_CHECK_EQUAL(dirs.size(), 0);

    // Test that empty paths don't show up in the result vector
    dirs.clear();
    Filesystem::searchpath_split(":", dirs, false);
    OIIO_CHECK_EQUAL(dirs.size(), 0);
    Filesystem::searchpath_split("::", dirs, false);
    OIIO_CHECK_EQUAL(dirs.size(), 0);
    dirs.clear();
    Filesystem::searchpath_split(":abc::def:", dirs, false);
    OIIO_CHECK_EQUAL(dirs.size(), 2);
    OIIO_CHECK_EQUAL(dirs[0], "abc");
    OIIO_CHECK_EQUAL(dirs[1], "def");

    dirs.clear();
    Filesystem::searchpath_split(pathlist, dirs);
    OIIO_CHECK_EQUAL(dirs.size(), 3);
    OIIO_CHECK_EQUAL(dirs[0], ".." DIRSEP "..");
    OIIO_CHECK_EQUAL(dirs[1], ".." DIRSEP ".." DIRSEP "cpack");
    OIIO_CHECK_EQUAL(dirs[2], "foo/bar/baz");

    std::cout << "Testing searchpath_find\n";

    // non-recursive search success
    OIIO_CHECK_EQUAL(Filesystem::searchpath_find("License.txt", dirs, false,
                                                 false),
                     ".." DIRSEP ".." DIRSEP "cpack" DIRSEP "License.txt");

    // non-recursive search failure (file is in a subdirectory)
    OIIO_CHECK_EQUAL(Filesystem::searchpath_find("oiioversion.h", dirs, false,
                                                 false),
                     "");

    // recursive search success (file is in a subdirectory)
    OIIO_CHECK_EQUAL(Filesystem::searchpath_find("oiioversion.h", dirs, false,
                                                 true),
                     ".." DIRSEP ".." DIRSEP "include" DIRSEP
                     "OpenImageIO" DIRSEP "oiioversion.h");

    // Test find_program
    OIIO_CHECK_ASSERT(
        Filesystem::is_executable(Filesystem::find_program("bash")));
}



inline std::string
my_read_text_file(string_view filename)
{
    std::string err;
    std::string contents;
    bool ok = Filesystem::read_text_file(filename, contents);
    OIIO_CHECK_ASSERT(ok);
    return contents;
}


inline std::string
my_read_text_file(string_view filename, size_t size)
{
    std::string err;
    std::string contents;
    bool ok = Filesystem::read_text_file(filename, contents, size);
    OIIO_CHECK_ASSERT(ok);
    return contents;
}


inline std::string
my_read_text_from_command(string_view filename)
{
    std::string err;
    std::string contents;
    bool ok = Filesystem::read_text_from_command(filename, contents);
    OIIO_CHECK_ASSERT(ok);
    return contents;
}


inline std::string
my_read_text_from_command(string_view filename, size_t size)
{
    std::string err;
    std::string contents;
    bool ok = Filesystem::read_text_from_command(filename, contents, size);
    OIIO_CHECK_ASSERT(ok);
    return contents;
}



static void
test_file_status()
{
    // Make test file, test Filesystem::fopen in the process.
    FILE* file = Filesystem::fopen("testfile", "wb");
    OIIO_CHECK_ASSERT(file != NULL);
    const char testtext[] = "test\nfoo\nbar\n";
    fputs(testtext, file);
    fclose(file);

    std::cout << "Testing file_size:\n";
    OIIO_CHECK_EQUAL(Filesystem::file_size("testfile"), 13);

    std::cout << "Testing read_text_file\n";
    OIIO_CHECK_EQUAL(my_read_text_file("testfile"), testtext);
    std::cout << "Testing write_text_file\n";
    Filesystem::write_text_file("testfile4", testtext);
    OIIO_CHECK_EQUAL(my_read_text_file("testfile4"), testtext);
    std::cout << "Testing read_text_file with size limit\n";
    OIIO_CHECK_EQUAL(my_read_text_file("testfile", 10), "test\nfoo\nb");
    std::cout << "Testing read_text_from_command\n";
    OIIO_CHECK_EQUAL(my_read_text_from_command("cat testfile"), testtext);
    std::cout << "Testing read_text_from_command with size limit\n";
    OIIO_CHECK_EQUAL(my_read_text_from_command("cat testfile", 7), "test\nfo");

    std::cout << "Testing read_bytes:\n";
    char buf[3];
    size_t nread = Filesystem::read_bytes("testfile", buf, 3, 5);
    OIIO_CHECK_EQUAL(nread, 3);
    OIIO_CHECK_EQUAL(buf[0], 'f');
    OIIO_CHECK_EQUAL(buf[1], 'o');
    OIIO_CHECK_EQUAL(buf[2], 'o');

    std::cout << "Testing create_directory\n";
    Filesystem::create_directory("testdir");

    std::cout << "Testing exists\n";
    OIIO_CHECK_ASSERT(Filesystem::exists("testfile"));
    OIIO_CHECK_ASSERT(Filesystem::exists("testdir"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("noexist"));
    std::cout << "Testing is_directory, is_regular, is_executable\n";
    OIIO_CHECK_ASSERT(Filesystem::is_regular("testfile"));
    OIIO_CHECK_ASSERT(!Filesystem::is_directory("testfile"));
#ifndef _WIN32
    // Directories look "executable" on Unix, not on Windows
    OIIO_CHECK_ASSERT(!Filesystem::is_executable("testfile"));
#endif
    OIIO_CHECK_ASSERT(!Filesystem::is_regular("testdir"));
    OIIO_CHECK_ASSERT(Filesystem::is_directory("testdir"));
    OIIO_CHECK_ASSERT(!Filesystem::is_executable("testdir"));
    OIIO_CHECK_ASSERT(!Filesystem::is_regular("noexist"));
    OIIO_CHECK_ASSERT(!Filesystem::is_directory("noexist"));
    OIIO_CHECK_ASSERT(!Filesystem::is_executable("noexist"));
    OIIO_CHECK_ASSERT(Filesystem::is_executable(Sysutil::this_program_path()));

    std::cout << "Testing copy, rename, remove\n";
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile2"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile3"));
    Filesystem::copy("testfile", "testfile2");
    OIIO_CHECK_ASSERT(Filesystem::exists("testfile2"));
    OIIO_CHECK_EQUAL(my_read_text_file("testfile2"), testtext);
    Filesystem::rename("testfile2", "testfile3");
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile2"));
    OIIO_CHECK_ASSERT(Filesystem::exists("testfile3"));
    OIIO_CHECK_EQUAL(my_read_text_file("testfile3"), testtext);
    Filesystem::remove("testfile");
    Filesystem::remove("testfile3");
    Filesystem::remove("testfile4");
    Filesystem::remove("testdir");
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile2"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile3"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("testfile4"));
    OIIO_CHECK_ASSERT(!Filesystem::exists("testdir"));
}



static void
test_seq(const char* str, const char* expected)
{
    std::vector<int> sequence;
    Filesystem::enumerate_sequence(str, sequence);
    std::stringstream joined;
    for (size_t i = 0; i < sequence.size(); ++i) {
        if (i)
            joined << " ";
        joined << sequence[i];
    }
    std::cout << "  \"" << str << "\" -> " << joined.str() << "\n";
    OIIO_CHECK_EQUAL(joined.str(), std::string(expected));
}



static void
test_file_seq(const char* pattern, string_view overrideval,
              const std::string& expected)
{
    std::vector<int> numbers;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    if (overrideval.size())
        frame_range = overrideval;
    Filesystem::enumerate_sequence(frame_range, numbers);
    Filesystem::enumerate_file_sequence(normalized_pattern, numbers, names);
    std::string joined = Strutil::join(names, " ");
    Strutil::print(" {}{}{} -> {}\n", pattern, overrideval.size() ? " + " : "",
                   overrideval, joined);
    OIIO_CHECK_EQUAL(joined, expected);
}



static void
test_file_seq_with_view(const char* pattern, string_view overrideval,
                        string_view view, const std::string& expected)
{
    std::vector<int> numbers;
    std::vector<string_view> views;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    if (overrideval.size())
        frame_range = overrideval;
    Filesystem::enumerate_sequence(frame_range, numbers);

    if (view.size()) {
        for (size_t i = 0, e = numbers.size(); i < e; ++i)
            views.emplace_back(view);
    }

    Filesystem::enumerate_file_sequence(normalized_pattern, numbers, views,
                                        names);
    std::string joined = Strutil::join(names, " ");
    Strutil::print(" {}{}{} -> {}\n", pattern, overrideval.size() ? " + " : "",
                   overrideval, joined);
    OIIO_CHECK_EQUAL(joined, expected);
}



static void
test_scan_file_seq(const char* pattern, const std::string& expected)
{
    std::vector<int> numbers;
    std::vector<std::string> names;
    std::string normalized_pattern;
    std::string frame_range;

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames(normalized_pattern, numbers, names);
    std::string joined = Strutil::join(names, " ");
    std::cout << "  " << pattern;
    std::cout << " -> " << joined << "\n";
    OIIO_CHECK_EQUAL(joined, expected);

    // Check that we don't crash from exceptions generated by strangely
    // formed patterns.
    const char* weird
        = "{'cpu_model': 'Intel(R) Xeon(R) CPU E5-2630 @ 2.30GHz'}";
    Filesystem::parse_pattern(weird, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames(normalized_pattern, numbers, names);
    OIIO_CHECK_EQUAL(names.size(), 0);
    // If we didn't crash above, we're ok!
}



static void
test_scan_file_seq_with_views(const char* pattern, const char** views_,
                              const std::string& expected)
{
    std::vector<int> frame_numbers;
    std::vector<string_view> frame_views;
    std::vector<std::string> frame_names;
    std::string normalized_pattern;
    std::string frame_range;
    std::vector<string_view> views;

    for (size_t i = 0; views_[i]; ++i)
        if (views_[i])
            views.emplace_back(views_[i]);

    Filesystem::parse_pattern(pattern, 0, normalized_pattern, frame_range);
    Filesystem::scan_for_matching_filenames(normalized_pattern, views,
                                            frame_numbers, frame_views,
                                            frame_names);
    for (auto& f : frame_names)
        f = Filesystem::generic_filepath(f);
    std::string joined = Strutil::join(frame_names, " ");
    std::cout << "  " << pattern;
    std::cout << " -> " << joined << "\n";
    OIIO_CHECK_EQUAL(joined, expected);
}



void
test_frame_sequences()
{
    std::cout << "Testing frame number sequences:\n";
    test_seq("3", "3");
    test_seq("1-5", "1 2 3 4 5");
    test_seq("5-1", "5 4 3 2 1");
    test_seq("1-3,6,10-12", "1 2 3 6 10 11 12");
    test_seq("1-5x2", "1 3 5");
    test_seq("1-5y2", "2 4");
    std::cout << "\n";

    test_file_seq(
        "foo.1-5#.exr", NULL,
        "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq(
        "foo.5-1#.exr", NULL,
        "foo.0005.exr foo.0004.exr foo.0003.exr foo.0002.exr foo.0001.exr");
    test_file_seq(
        "foo.1-3,6,10-12#.exr", NULL,
        "foo.0001.exr foo.0002.exr foo.0003.exr foo.0006.exr foo.0010.exr foo.0011.exr foo.0012.exr");
    test_file_seq("foo.1-5x2#.exr", NULL,
                  "foo.0001.exr foo.0003.exr foo.0005.exr");
    test_file_seq("foo.1-5y2#.exr", NULL, "foo.0002.exr foo.0004.exr");

    test_file_seq(
        "foo.#.exr", "1-5",
        "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq("foo.#.exr", "1-5x2",
                  "foo.0001.exr foo.0003.exr foo.0005.exr");

    test_file_seq("foo.1-3@@.exr", NULL, "foo.01.exr foo.02.exr foo.03.exr");
    test_file_seq("foo.1-3@#.exr", NULL,
                  "foo.00001.exr foo.00002.exr foo.00003.exr");

    test_file_seq(
        "foo.1-5%04d.exr", NULL,
        "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq(
        "foo.%04d.exr", "1-5",
        "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
    test_file_seq(
        "foo.%4d.exr", "1-5",
        "foo.   1.exr foo.   2.exr foo.   3.exr foo.   4.exr foo.   5.exr");
    test_file_seq("foo.%d.exr", "1-5",
                  "foo.1.exr foo.2.exr foo.3.exr foo.4.exr foo.5.exr");

    const char* views1[] = { "left", "right", "foo", "", NULL };
    for (auto view : views1) {
        test_file_seq_with_view(
            "foo.1-5#.exr", NULL, view,
            "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view(
            "foo.5-1#.exr", NULL, view,
            "foo.0005.exr foo.0004.exr foo.0003.exr foo.0002.exr foo.0001.exr");
        test_file_seq_with_view(
            "foo.1-3,6,10-12#.exr", NULL, view,
            "foo.0001.exr foo.0002.exr foo.0003.exr foo.0006.exr foo.0010.exr foo.0011.exr foo.0012.exr");
        test_file_seq_with_view("foo.1-5x2#.exr", NULL, view,
                                "foo.0001.exr foo.0003.exr foo.0005.exr");
        test_file_seq_with_view("foo.1-5y2#.exr", NULL, view,
                                "foo.0002.exr foo.0004.exr");

        test_file_seq_with_view(
            "foo.#.exr", "1-5", view,
            "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view("foo.#.exr", "1-5x2", view,
                                "foo.0001.exr foo.0003.exr foo.0005.exr");

        test_file_seq_with_view("foo.1-3@@.exr", NULL, view,
                                "foo.01.exr foo.02.exr foo.03.exr");
        test_file_seq_with_view("foo.1-3@#.exr", NULL, view,
                                "foo.00001.exr foo.00002.exr foo.00003.exr");

        test_file_seq_with_view(
            "foo.1-5%04d.exr", NULL, view,
            "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view(
            "foo.%04d.exr", "1-5", view,
            "foo.0001.exr foo.0002.exr foo.0003.exr foo.0004.exr foo.0005.exr");
        test_file_seq_with_view(
            "foo.%4d.exr", "1-5", view,
            "foo.   1.exr foo.   2.exr foo.   3.exr foo.   4.exr foo.   5.exr");
        test_file_seq_with_view(
            "foo.%d.exr", "1-5", view,
            "foo.1.exr foo.2.exr foo.3.exr foo.4.exr foo.5.exr");
    }

    //    test_file_seq_with_view ("%V.%04d", NULL, NULL, "");
    //    test_file_seq_with_view ("%v", NULL, NULL, "");
    //    test_file_seq_with_view ("%V", NULL, "", "");
    //    test_file_seq_with_view ("%v", NULL, "", "");
    //    test_file_seq_with_view ("%V", NULL, "left", "left");
    //    test_file_seq_with_view ("%V", NULL, "right", "right");
    //    test_file_seq_with_view ("%v", NULL, "left", "l");
    //    test_file_seq_with_view ("%v", NULL, "right", "r");
    test_file_seq_with_view("foo_%V.1-2#.exr", NULL, "left",
                            "foo_left.0001.exr foo_left.0002.exr");
    test_file_seq_with_view("%V/foo_%V.1-2#.exr", NULL, "left",
                            "left/foo_left.0001.exr left/foo_left.0002.exr");
    test_file_seq_with_view("%v/foo_%V.1-2#.exr", NULL, "left",
                            "l/foo_left.0001.exr l/foo_left.0002.exr");
    test_file_seq_with_view("%V/foo_%v.1-2#.exr", NULL, "left",
                            "left/foo_l.0001.exr left/foo_l.0002.exr");
    test_file_seq_with_view("%v/foo_%v.1-2#.exr", NULL, "left",
                            "l/foo_l.0001.exr l/foo_l.0002.exr");

    std::cout << "\n";
}



void
create_test_file(string_view fn)
{
    Filesystem::write_text_file(fn, "");
}



void
test_scan_sequences()
{
    std::cout << "Testing frame sequence scanning:\n";

    std::vector<std::string> filenames;

    for (size_t i = 1; i <= 5; i++) {
        std::string fn = Strutil::fmt::format("foo.{:04d}.exr", i);
        filenames.push_back(fn);
        create_test_file(fn);
    }
    // Deliberate file that's not a match! Make sure dots in the filename
    // aren't regex dots that match any character.
    filenames.push_back("fooX0000Xexr");
    create_test_file("fooX0000Xexr");

    test_scan_file_seq(
        "foo.#.exr",
        "./foo.0001.exr ./foo.0002.exr ./foo.0003.exr ./foo.0004.exr ./foo.0005.exr");

    filenames.clear();

    Filesystem::create_directory("left");
    Filesystem::create_directory("left/l");

    for (size_t i = 1; i <= 5; i++) {
        std::string fn = Strutil::fmt::format("left/l/foo_left_l.{:04d}.exr",
                                              i);
        filenames.push_back(fn);
        create_test_file(fn);
    }

    const char* views[] = { "left", NULL };

    test_scan_file_seq_with_views(
        "%V/%v/foo_%V_%v.#.exr", views,
        "left/l/foo_left_l.0001.exr left/l/foo_left_l.0002.exr left/l/foo_left_l.0003.exr left/l/foo_left_l.0004.exr left/l/foo_left_l.0005.exr");

    filenames.clear();

    Filesystem::create_directory("right");
    Filesystem::create_directory("right/r");

    std::string fn;

    fn = "left/l/foo_left_l";
    filenames.push_back(fn);
    create_test_file(fn);

    fn = "right/r/foo_right_r";
    filenames.push_back(fn);
    create_test_file(fn);

    const char* views2[] = { "left", "right", NULL };

    test_scan_file_seq_with_views("%V/%v/foo_%V_%v", views2,
                                  "left/l/foo_left_l right/r/foo_right_r");
}



void
test_mem_proxies()
{
    std::cout << "Testing memory file proxies:\n";
    std::vector<unsigned char> input_buf { 10, 11, 12, 13, 14,
                                           15, 16, 17, 18, 19 };
    std::vector<unsigned char> output_buf;

    Filesystem::IOMemReader in(input_buf);
    Filesystem::IOVecOutput out(output_buf);
    char b[4];
    size_t len = 0;
    while ((len = in.read(b, 4)))  // read up to 4 bytes at a time
        out.write(b, len);
    OIIO_CHECK_ASSERT(input_buf == output_buf);
    // Now test seeking
    in.seek(3);
    out.seek(1);
    in.read(b, 2);
    out.write(b, 2);
    std::vector<unsigned char> ref_buf {
        10, 13, 14, 13, 14, 15, 16, 17, 18, 19
    };
    OIIO_CHECK_ASSERT(output_buf == ref_buf);
}



void
test_last_write_time()
{
    Filesystem::write_text_file("oiio-testtime.txt", "test");
    time_t t = Filesystem::last_write_time("oiio-testtime.txt");
    std::cout << "Last write time of oiio-testtime.txt is " << t << "\n";
    Filesystem::last_write_time("oiio-testtime.txt", t - 42);
    OIIO_CHECK_EQUAL(Filesystem::last_write_time("oiio-testtime.txt"), t - 42);
    Filesystem::remove("oiio-testtime.txt");
}



void
test_getline()
{
    const char* contents    = "Line 1\n"
                              "Line 2\n"
                              "Really really really long long line line 3\n"
                              "Line 4\n"
                              "Line 5 no newline";
    const char* tmpfilename = "getline.txt";
    Filesystem::write_text_file(tmpfilename, contents);

    FILE* in = Filesystem::fopen(tmpfilename, "r");
    OIIO_CHECK_ASSERT(in != nullptr);
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "Line 1\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "Line 2\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30),
                     "Really really really long long");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), " line line 3\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "Line 4\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "");  // EOF before \n
    fclose(in);

    // Again, with a complete line at the end to be sure we read it
    Filesystem::write_text_file(tmpfilename, "Line 1\n"
                                             "Line 2\n");
    in = Filesystem::fopen(tmpfilename, "r");
    OIIO_CHECK_ASSERT(in != nullptr);
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "Line 1\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "Line 2\n");
    OIIO_CHECK_EQUAL(Filesystem::getline(in, 30), "");
    OIIO_CHECK_ASSERT(feof(in));
    fclose(in);

    Filesystem::remove(tmpfilename);
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_filename_decomposition();
    test_filename_searchpath_find();
    test_file_status();
    test_frame_sequences();
    test_scan_sequences();
    test_mem_proxies();
    test_last_write_time();
    test_getline();

    return unit_test_failures;
}
