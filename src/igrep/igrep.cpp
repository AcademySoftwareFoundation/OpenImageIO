// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

static bool help          = false;
static bool invert_match  = false;
static bool list_files    = false;
static bool recursive     = false;
static bool file_match    = false;
static bool print_dirs    = false;
static bool all_subimages = false;
static std::string pattern;
static std::vector<std::string> filenames;



static bool
grep_file(const std::string& filename, std::regex& re,
          bool ignore_nonimage_files = false)
{
    if (!Filesystem::exists(filename)) {
        std::cerr << "igrep: " << filename << ": No such file or directory\n";
        return false;
    }

    if (Filesystem::is_directory(filename)) {
        if (!recursive)
            return false;
        if (print_dirs) {
            std::cout << "(" << filename << "/)\n";
            std::cout.flush();
        }
        bool r = false;
        std::vector<std::string> directory_entries;
        Filesystem::get_directory_entries(filename, directory_entries);
        for (const auto& d : directory_entries)
            r |= grep_file(d, re, true);
        return r;
    }

    auto in = ImageInput::open(filename);
    if (!in.get()) {
        if (!ignore_nonimage_files)
            std::cerr << geterror() << "\n";
        return false;
    }

    if (file_match) {
        bool match = false;
        try {
            match = std::regex_search(filename, re);
        } catch (const std::regex_error& e) {
            std::cerr << "igrep: " << e.what() << "\n";
            return false;
        }
        if (match && !invert_match) {
            std::cout << filename << "\n";
            return true;
        }
    }

    bool found   = false;
    int subimage = 0;
    do {
        if (!all_subimages && subimage > 0)
            break;
        ImageSpec spec = in->spec(subimage);
        for (auto&& p : spec.extra_attribs) {
            TypeDesc t = p.type();
            if (t.elementtype() == TypeDesc::STRING) {
                int n = t.numelements();
                for (int i = 0; i < n; ++i) {
                    bool match = false;
                    try {
                        match = std::regex_search(((const char**)p.data())[i],
                                                  re);
                    } catch (const std::regex_error& e) {
                        std::cerr << "igrep: " << e.what() << "\n";
                        return false;
                    }
                    found |= match;
                    if (match && !invert_match) {
                        if (list_files) {
                            std::cout << filename << "\n";
                            return found;
                        }
                        std::cout << filename << ": " << p.name() << " = "
                                  << ((const char**)p.data())[i] << "\n";
                    }
                }
            }
        }
    } while (in->seek_subimage(++subimage, 0));

    if (invert_match) {
        found = !found;
        if (found)
            std::cout << filename << "\n";
    }
    return found;
}



static int
parse_files(int argc, const char* argv[])
{
    for (int i = 0; i < argc; i++) {
        if (pattern.empty())
            pattern = argv[i];
        else
            filenames.emplace_back(argv[i]);
    }
    return 0;
}



int
main(int argc, const char* argv[])
{
    // Helpful for debugging to make sure that any crashes dump a stack
    // trace.
    Sysutil::setup_crash_stacktrace("stdout");

    Filesystem::convert_native_arguments(argc, argv);
    // clang-format off
    ArgParse ap;
    ap.intro("igrep -- search images for matching metadata\n"
             OIIO_INTRO_STRING)
      .usage("igrep [options] pattern filename...")
      .add_version(OIIO_VERSION_STRING);
    ap.arg("filename")
      .hidden()
      .action(parse_files);
    ap.arg("-i")
      .help("Ignore upper/lower case distinctions");
    ap.arg("-v", &invert_match)
      .help("Invert match (select non-matching files)");
    ap.arg("-E")
      .help( "Pattern is an extended regular expression");
    ap.arg("-f", &file_match)
      .help("Match against file name as well as metadata");
    ap.arg("-l", &list_files)
      .help("List the matching files (no detail)");
    ap.arg("-r", &recursive)
      .help("Recurse into directories");
    ap.arg("-d", &print_dirs)
      .help("Print directories (when recursive)");
    ap.arg("-a", &all_subimages)
      .help("Search all subimages of each file");

    // clang-format on
    ap.parse(argc, argv);
    if (pattern.empty() || filenames.empty()) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        return help ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    auto flag = std::regex_constants::grep;
    if (ap["E"].get<int>())
        flag = std::regex_constants::extended;
    if (ap["i"].get<int>())
        flag |= std::regex_constants::icase;

    bool ok = true;

    try {
        std::regex re(pattern, flag);
        for (auto&& s : filenames)
            grep_file(s, re);
    } catch (const std::regex_error& e) {
        std::cerr << "igrep: " << e.what() << "\n";
        ok = false;
    }
    shutdown();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
