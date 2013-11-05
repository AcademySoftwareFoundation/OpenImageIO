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
#include <iostream>
#include <string>
#include <algorithm>

#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "dassert.h"
#include "ustring.h"

#include "filesystem.h"


OIIO_NAMESPACE_ENTER
{


std::string
Filesystem::filename (const std::string &filepath)
{
    // To simplify dealing with platform-specific separators and whatnot,
    // just use the Boost routines:
#if BOOST_FILESYSTEM_VERSION == 3
    return boost::filesystem::path(filepath).filename().string();
#else
    return boost::filesystem::path(filepath).filename();
#endif
}



std::string
Filesystem::extension (const std::string &filepath, bool include_dot)
{
    std::string s;
#if BOOST_FILESYSTEM_VERSION == 3
    s = boost::filesystem::path(filepath).extension().string();
#else
    s = boost::filesystem::path(filepath).extension();
#endif
    if (! include_dot && !s.empty() && s[0] == '.')
        s.erase (0, 1);  // erase the first character
    return s;
}



std::string
Filesystem::parent_path (const std::string &filepath)
{
    return boost::filesystem::path(filepath).parent_path().string();
}



std::string
Filesystem::replace_extension (const std::string &filepath,
                               const std::string &new_extension)
{
    return boost::filesystem::path(filepath).replace_extension(new_extension).string();
}



void
Filesystem::searchpath_split (const std::string &searchpath,
                              std::vector<std::string> &dirs,
                              bool validonly)
{
    dirs.clear();

    std::string path_copy = searchpath;
    std::string last_token;
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(":;");
    tokenizer tokens (searchpath, sep);
    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end(); last_token = *tok_iter, ++tok_iter) {
        std::string path = *tok_iter;
#ifdef _WIN32
        // On Windows, we might see something like "a:foo" and any human
        // would know that it means drive/directory 'a:foo', NOT
        // separate directories 'a' and 'foo'.  Implement the obvious
        // heuristic here.  Note that this means that we simply don't
        // correctly support searching in *relative* directories that
        // consist of a single letter.
        if (last_token.length() == 1 && last_token[0] != '.') {
            // If the last token was a single letter, try prepending it
            path = last_token + ":" + (*tok_iter);
        } else
#endif
            path = *tok_iter;
        // Kill trailing slashes (but not simple "/")
        size_t len = path.length();
        while (len > 1 && (path[len-1] == '/' || path[len-1] == '\\'))
            path.erase (--len);
        // If it's a valid directory, or if validonly is false, add it
        // to the list
        if (!validonly || Filesystem::is_directory (path))
            dirs.push_back (path);
    }
#if 0
    std::cerr << "Searchpath = '" << searchpath << "'\n";
    BOOST_FOREACH (std::string &d, dirs)
        std::cerr << "\tPath = '" << d << "'\n";
    std::cerr << "\n";
#endif
}



std::string
Filesystem::searchpath_find (const std::string &filename,
                             const std::vector<std::string> &dirs,
                             bool testcwd, bool recursive)
{
    bool abs = Filesystem::path_is_absolute (filename);

    // If it's an absolute filename, or if we want to check "." first,
    // then start by checking filename outright.
    if (testcwd || abs) {
        if (Filesystem::is_regular (filename))
            return filename;
    }

    // Relative filename, not yet found -- try each directory in turn
    BOOST_FOREACH (const std::string &d, dirs) {
        // std::cerr << "\tPath = '" << d << "'\n";
        boost::filesystem::path f = d;
        f /= filename;
        // std::cerr << "\tTesting '" << f << "'\n";
        if (Filesystem::is_regular (f.string())) {
            // std::cerr << "Found '" << f << "'\n";
            return f.string();
        }

        if (recursive && Filesystem::is_directory (d)) {
            std::vector<std::string> subdirs;
            for (boost::filesystem::directory_iterator s(d); 
                 s != boost::filesystem::directory_iterator();  ++s)
                if (Filesystem::is_directory(s->path().string()))
                    subdirs.push_back (s->path().string());
            std::string found = searchpath_find (filename, subdirs, false, true);
            if (found.size())
                return found;
        }
    }
    return std::string();
}



bool
Filesystem::get_directory_entries (const std::string &dirname,
                                   std::vector<std::string> &filenames,
                                   bool recursive,
                                   const std::string &filter_regex)
{
    filenames.clear ();
    if (dirname.size() && ! is_directory(dirname))
        return false;
    boost::filesystem::path dirpath (dirname.size() ? dirname : std::string("."));
    boost::regex re (filter_regex);

    if (recursive) {
        for (boost::filesystem::recursive_directory_iterator s (dirpath);
             s != boost::filesystem::recursive_directory_iterator();  ++s) {
            std::string file = s->path().string();
            if (!filter_regex.size() || boost::regex_search (file, re))
                filenames.push_back (file);
        }
    } else {
        for (boost::filesystem::directory_iterator s (dirpath);
             s != boost::filesystem::directory_iterator();  ++s) {
            std::string file = s->path().string();
            if (!filter_regex.size() || boost::regex_search (file, re))
                filenames.push_back (file);
        }
    }
    return true;
}



bool
Filesystem::path_is_absolute (const std::string &path, bool dot_is_absolute)
{
    // "./foo" is considered absolute if dot_is_absolute is true.
    // Don't get confused by ".foo", which is not absolute!
    size_t len = path.length();
    if (!len)
        return false;
    return (path[0] == '/')
        || (dot_is_absolute && path[0] == '.' && path[1] == '/')
        || (dot_is_absolute && path[0] == '.' && path[1] == '.' && path[2] == '/')
#ifdef _WIN32
        || path[0] == '\\'
        || (dot_is_absolute && path[0] == '.' && path[1] == '\\')
        || (dot_is_absolute && path[0] == '.' && path[1] == '.' && path[2] == '\\')
        || (isalpha(path[0]) && path[1] == ':')
#endif
        ;
}



bool
Filesystem::exists (const std::string &path)
{
    bool r = false;
    try {
        r = boost::filesystem::exists (path);
    } catch (const std::exception &) {
        r = false;
    }
    return r;
}



bool
Filesystem::is_directory (const std::string &path)
{
    bool r = false;
    try {
        r = boost::filesystem::is_directory (path);
    } catch (const std::exception &) {
        r = false;
    }
    return r;
}



bool
Filesystem::is_regular (const std::string &path)
{
    bool r = false;
    try {
        r = boost::filesystem::is_regular_file (path);
    } catch (const std::exception &) {
        r = false;
    }
    return r;
}



FILE*
Filesystem::fopen (const std::string &path, const std::string &mode)
{
#ifdef _WIN32
    // on Windows fopen does not accept UTF-8 paths, so we convert to wide char
    std::wstring wpath = Strutil::utf8_to_utf16 (path);
    std::wstring wmode = Strutil::utf8_to_utf16 (mode);

    return ::_wfopen (wpath.c_str(), wmode.c_str());
#else
    // on Unix platforms passing in UTF-8 works
    return ::fopen (path.c_str(), mode.c_str());
#endif
}



void
Filesystem::open (std::ifstream &stream,
                  const std::string &path,
                  std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ifstream accepts non-standard wchar_t* 
    std::wstring wpath = Strutil::utf8_to_utf16(path);
    stream.open (wpath.c_str(), mode);
    stream.seekg (0, std::ios_base::beg); // force seek, otherwise broken
#else
    stream.open (path.c_str(), mode);
#endif
}



void
Filesystem::open (std::ofstream &stream,
                  const std::string &path,
                  std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ofstream accepts non-standard wchar_t*
    std::wstring wpath = Strutil::utf8_to_utf16 (path);
    stream.open (wpath.c_str(), mode);
#else
    stream.open (path.c_str(), mode);
#endif
}

std::time_t
Filesystem::last_write_time (const std::string& path)
{
    try {
#ifdef _WIN32
        std::wstring wpath = Strutil::utf8_to_utf16 (path);
        return boost::filesystem::last_write_time (wpath);
#else
        return boost::filesystem::last_write_time (path);
#endif
    } catch (const std::exception &) {
        // File doesn't exist
        return 0;
    }
}



void
Filesystem::last_write_time (const std::string& path, std::time_t time)
{
    try {
#ifdef _WIN32
        std::wstring wpath = Strutil::utf8_to_utf16 (path);
        boost::filesystem::last_write_time (wpath, time);
#else
        boost::filesystem::last_write_time (path, time);
#endif
    } catch (const std::exception &) {
        // File doesn't exist
    }
}



void
Filesystem::convert_native_arguments (int argc, const char *argv[])
{
#ifdef _WIN32
    // Windows only, standard main() entry point does not accept unicode file
    // paths, here we retrieve wide char arguments and convert them to utf8
    if (argc == 0)
        return;

    int native_argc;
    wchar_t **native_argv = CommandLineToArgvW (GetCommandLineW (), &native_argc);

    if (!native_argv || native_argc != argc)
        return;

    for (int i = 0; i < argc; i++) {
        std::string utf8_arg = Strutil::utf16_to_utf8 (native_argv[i]);
        argv[i] = ustring (utf8_arg).c_str();
    }
#endif
}



bool
Filesystem::enumerate_sequence (const char *desc, std::vector<int> &numbers)
{
    numbers.clear ();

    // Split the sequence description into comma-separated subranges.
    std::vector<std::string> ranges;
    Strutil::split (desc, ranges, ",");

    // For each subrange...
    BOOST_FOREACH (const std::string &s, ranges) {
        // It's START, START-FINISH, or START-FINISHxSTEP, or START-FINISHySTEP
        // If START>FINISH or if STEP<0, then count down.
        // If 'y' is used, generate the complement.
        std::vector<std::string> range;
        Strutil::split (s, range, "-", 1);
        int first = strtol (range[0].c_str(), NULL, 10);
        int last = first;
        int step = 1;
        bool complement = false;
        if (range.size() > 1) {
            last = strtol (range[1].c_str(), NULL, 10);
            if (const char *x = strchr (range[1].c_str(), 'x'))
                step = (int) strtol (x+1, NULL, 10);
            else if (const char *x = strchr (range[1].c_str(), 'y')) {
                step = (int) strtol (x+1, NULL, 10);
                complement = true;
            }
            if (step == 0)
                step = 1;
            if (step < 0 && first < last)
                std::swap (first, last);
            if (first > last && step > 0)
                step = -step;
        }
        int end = last + (step > 0 ? 1 : -1);
        int itstep = step > 0 ? 1 : -1;
        for (int i = first; i != end; i += itstep) {
            if ((abs(i-first) % abs(step) == 0) != complement)
                numbers.push_back (i);
        }
    }
    return true;
}

bool
Filesystem::parse_pattern (const char *pattern_,
                           int framepadding_override,
                           std::string &normalized_pattern,
                           std::string &framespec)
{
    std::string pattern (pattern_);

    // The pattern is either a range (e.g., "1-15#"), a
    // set of hash marks (e.g. "####"), or a printf-style format
    // string (e.g. "%04d").
#define ONERANGE_SPEC "[0-9]+(-[0-9]+((x|y)-?[0-9]+)?)?"
#define MANYRANGE_SPEC ONERANGE_SPEC "(," ONERANGE_SPEC ")*"
#define SEQUENCE_SPEC "(" MANYRANGE_SPEC ")?" "((#|@)+|(%[0-9]*d))"
    static boost::regex sequence_re (SEQUENCE_SPEC);
    // std::cout << "pattern >" << (SEQUENCE_SPEC) << "<\n";
    boost::match_results<std::string::const_iterator> range_match;
    if (! boost::regex_search (pattern, range_match, sequence_re)) {
        // Not a range
        return false;
    }

    // It's a range. Generate the names by iterating through the numbers.
    std::string thematch (range_match[0].first, range_match[0].second);
    std::string thesequence (range_match[1].first, range_match[1].second);
    std::string thehashes (range_match[9].first, range_match[9].second);
    std::string theformat (range_match[11].first, range_match[11].second);
    std::string prefix (range_match.prefix().first, range_match.prefix().second);
    std::string suffix (range_match.suffix().first, range_match.suffix().second);

    // std::cout << "theformat: " << theformat << "\n";

    std::string fmt;
    if (theformat.length() > 0) {
        fmt = theformat;
    }
    else {
        // Compute the amount of padding desired
        int padding = 0;
        for (int i = (int)thematch.length()-1;  i >= 0;  --i) {
            if (thematch[i] == '#')
                padding += 4;
            else if (thematch[i] == '@')
                padding += 1;
        }
        if (framepadding_override > 0)
            padding = framepadding_override;
        fmt = Strutil::format ("%%0%dd", padding);
    }

    // std::cout << "Format: '" << fmt << "'\n";

    normalized_pattern = prefix + fmt + suffix;
    framespec = thesequence;

    return true;
}



bool
Filesystem::enumerate_file_sequence (const std::string &pattern,
                                     const std::vector<int> &numbers,
                                     std::vector<std::string> &filenames)
{
    for (size_t i = 0, e = numbers.size(); i < e; ++i) {
        std::string f = Strutil::format (pattern.c_str(), numbers[i]);
        filenames.push_back (f);
    }
    return true;
}



bool
Filesystem::scan_for_matching_filenames(const std::string &pattern_,
                                        std::vector<int> &numbers,
                                        std::vector<std::string> &filenames)
{
    std::string pattern = pattern_;

    // Isolate the directory name (or '.' if none was specified)
    std::string directory = Filesystem::parent_path (pattern);
    if (directory.size() == 0) {
        directory = ".";
#ifdef _WIN32
        pattern = ".\\\\" + pattern;
#else
        pattern = "./" + pattern;
#endif
    }

    if (! boost::filesystem::exists(directory))
        return false;

    // build a regex that matches the pattern
    boost::regex format_re ("%0([0-9]+)d");
    boost::match_results<std::string::const_iterator> format_match;
    if (! boost::regex_search (pattern, format_match, format_re))
        return false;

    std::string thepadding (format_match[1].first, format_match[1].second);
    std::string prefix (format_match.prefix().first, format_match.prefix().second);
    std::string suffix (format_match.suffix().first, format_match.suffix().second);

    std::string pattern_re_str = prefix + "([0-9]{" + thepadding + "})" + suffix;
    boost::regex pattern_re (pattern_re_str);

    std::vector< std::pair< int, std::string > > matches;

    boost::filesystem::directory_iterator end_it;
#ifdef _WIN32
    std::wstring wdirectory = Strutil::utf8_to_utf16 (directory);
    for (boost::filesystem::directory_iterator it(wdirectory); it != end_it; ++it) {
#else
    for (boost::filesystem::directory_iterator it(directory); it != end_it; ++it) {
#endif
        if (boost::filesystem::is_regular_file(it->status())) {
            const std::string f = it->path().string();
            boost::match_results<std::string::const_iterator> frame_match;
            if (boost::regex_match (f, frame_match, pattern_re)) {
                std::string thenumber (frame_match[1].first, frame_match[1].second);

                int frame = (int)strtol (thenumber.c_str(), NULL, 10);

                matches.push_back (std::make_pair (frame, f));
            }
        }
    }

    // filesystem order is undefined, so return sorted sequences
    std::sort (matches.begin(), matches.end());

    for (size_t i = 0; i < matches.size(); ++i) {
        numbers.push_back (matches[i].first);
        filenames.push_back (matches[i].second);
    }

    return true;
}

}
OIIO_NAMESPACE_EXIT
