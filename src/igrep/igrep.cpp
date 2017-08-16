/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>
#include <memory>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#ifdef USE_BOOST_REGEX
# include <boost/regex.hpp>
  using boost::regex;
  using boost::regex_search;
  using namespace boost::regex_constants;
#else
# include <regex>
  using std::regex;
  using std::regex_search;
  using namespace std::regex_constants;
#endif

using namespace OIIO;

static bool help = false;
static bool invert_match = false;
static bool ignore_case = false;
static bool list_files = false;
static bool recursive = false;
static bool file_match = false;
static bool print_dirs = false;
static bool all_subimages = false;
static bool extended_regex = false;
static std::string pattern;
static std::vector<std::string> filenames;



static bool
grep_file (const std::string &filename, regex &re,
           bool ignore_nonimage_files=false)
{
    if (! Filesystem::exists (filename)) {
        std::cerr << "igrep: " << filename << ": No such file or directory\n";
        return false;
    }

    if (Filesystem::is_directory (filename)) {
        if (! recursive)
            return false;
        if (print_dirs) {
            std::cout << "(" << filename << "/)\n";
            std::cout.flush();
        }
        bool r = false;
        std::vector<std::string> directory_entries;
        Filesystem::get_directory_entries (filename, directory_entries);
        for (const auto& d : directory_entries)
            r |= grep_file (d, re, true);
        return r;
    }

    std::unique_ptr<ImageInput> in (ImageInput::open (filename.c_str()));
    if (! in.get()) {
        if (! ignore_nonimage_files)
            std::cerr << geterror() << "\n";
        return false;
    }
    ImageSpec spec = in->spec();

    if (file_match) {
        bool match = regex_search (filename, re);
        if (match && ! invert_match) {
            std::cout << filename << "\n";
            return true;
        }
    }

    bool found = false;
    int subimage = 0;
    do {
        if (!all_subimages && subimage > 0)
            break;
        for (auto&& p : spec.extra_attribs) {
            TypeDesc t = p.type();
            if (t.elementtype() == TypeDesc::STRING) {
                int n = t.numelements();
                for (int i = 0;  i < n;  ++i) {
                    bool match = regex_search (((const char **)p.data())[i], re);
                    found |= match;
                    if (match && ! invert_match) {
                        if (list_files) {
                            std::cout << filename << "\n";
                            return found;
                        }
                        std::cout << filename << ": " << p.name() << " = " 
                                  << ((const char **)p.data())[i] << "\n";
                    }
                }
            }
        }
    } while (in->seek_subimage (++subimage, 0, spec));

    if (invert_match) {
        found = !found;
        if (found)
            std::cout << filename << "\n";
    }
    return found;
}



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++) {
        if (pattern.empty())
            pattern = argv[i];
        else
            filenames.emplace_back(argv[i]);
    }
    return 0;
}



int
main (int argc, const char *argv[])
{
    Filesystem::convert_native_arguments (argc, argv);
    ArgParse ap;
    ap.options ("igrep -- search images for matching metadata\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  igrep [options] pattern filename...",
                "%*", parse_files, "",
                "-i", &ignore_case, "Ignore upper/lower case distinctions",
                "-v", &invert_match, "Invert match (select non-matching files)",
                "-E", &extended_regex, "Pattern is an extended regular expression",
                "-f", &file_match, "Match against file name as well as metadata",
                "-l", &list_files, "List the matching files (no detail)",
                "-r", &recursive, "Recurse into directories",
                "-d", &print_dirs, "Print directories (when recursive)",
                "-a", &all_subimages, "Search all subimages of each file",
                "--help", &help, "Print help message",
                NULL);
    if (ap.parse(argc, argv) < 0 || pattern.empty() || filenames.empty()) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

#if USE_BOOST_REGEX
    boost::regex_constants::syntax_option_type flag = boost::regex_constants::grep;
    if (extended_regex)
        flag = boost::regex::extended;
    if (ignore_case)
        flag |= boost::regex_constants::icase;
#else
    std::regex_constants::syntax_option_type flag = std::regex_constants::grep;
    if (extended_regex)
        flag = std::regex_constants::extended;
    if (ignore_case)
        flag |= std::regex_constants::icase;
#endif
    regex re (pattern, flag);
    for (auto&& s : filenames) {
        grep_file (s, re);
    }

    return 0;
}
