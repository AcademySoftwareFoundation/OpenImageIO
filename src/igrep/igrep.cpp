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

#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
using namespace boost::filesystem;

#include "argparse.h"
#include "strutil.h"
#include "imageio.h"
using namespace OpenImageIO;


static bool help = false;
static bool invert_match = false;
static bool ignore_case = false;
static bool list_files = false;
static bool recursive = false;
static std::string pattern;
static std::vector<std::string> filenames;



static bool
grep_file (const std::string &filename, boost::regex &re,
           bool ignore_nonimage_files=false)
{
    boost::filesystem::path path (filename);
    if (! boost::filesystem::exists (path)) {
        std::cerr << "igrep: " << filename << ": No such file or directory\n";
        return false;
    }

    if (boost::filesystem::is_directory (path)) {
        if (! recursive)
            return false;
        // std::cerr << filename << " is directory\n";
        bool r = false;
        boost::filesystem::directory_iterator end_itr;  // default is past-end
        for (boost::filesystem::directory_iterator itr(path);  itr != end_itr;  ++itr) {
            // std::cout << "  rec " << itr->path() << "\n";
            r |= grep_file (itr->path().string(), re, true);
        }
        return r;
    }

    boost::scoped_ptr<ImageInput> in (ImageInput::create (filename.c_str()));
    if (! in.get()) {
        if (! ignore_nonimage_files)
            std::cerr << OpenImageIO::error_message() << "\n";
        return false;
    }
    ImageSpec spec;
    if (! in->open (filename.c_str(), spec)) {
        std::cerr << "igrep: Could not open \"" << filename << "\" : "
                  << in->error_message() << "\n";
        return false;
    }

    bool found = false;
    BOOST_FOREACH (const ImageIOParameter &p, spec.extra_attribs) {
        TypeDesc t = p.type();
        if (t.elementtype() == TypeDesc::STRING) {
            int n = t.numelements();
            for (int i = 0;  i < n;  ++i) {
                bool match = boost::regex_search (((const char **)p.data())[i], re);
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
            filenames.push_back (argv[i]);
    }
    return 0;
}



int
main (int argc, const char *argv[])
{
    ArgParse ap;
    ap.options ("Usage:  igrep [options] pattern filename...",
                "%*", parse_files, "",
                "-i", &ignore_case, "Ignore upper/lower case distinctions",
                "-l", &list_files, "List the matching files (no detail)",
                "-r", &recursive, "Recurse into directories",
                "-v", &invert_match, "Invert match (select non-matching files)",
                "--help", &help, "Print help message",
                NULL);
    if (ap.parse(argc, argv) < 0 || pattern.empty() || filenames.empty()) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    boost::regex_constants::syntax_option_type flag = boost::regex_constants::grep;
    if (ignore_case)
        flag |= boost::regex_constants::icase;
    boost::regex re (pattern, flag);
    BOOST_FOREACH (const std::string &s, filenames) {
        grep_file (s, re);
    }

    return 0;
}
