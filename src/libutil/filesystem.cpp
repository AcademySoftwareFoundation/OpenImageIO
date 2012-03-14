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

#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "dassert.h"

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
Filesystem::extension (const std::string &filepath)
{
#if BOOST_FILESYSTEM_VERSION == 3
    return boost::filesystem::path(filepath).extension().string();
#else
    return boost::filesystem::path(filepath).extension();
#endif
}



std::string
Filesystem::file_extension (const std::string &filepath)
{
    // Search for the LAST dot in the filepath
    const char *ext = strrchr (filepath.c_str(), '.');

    // If there was no dot at all, or if it was the last character, there's
    // no file extension, so just return "".
    if (! ext || !ext[1])
        return "";

    ++ext;  // Advance to the char AFTER the dot

    // But if there's a slash after the last dot, then the dot was part
    // of the directory, not the leaf name.
    if (strchr (ext, '/'))
        return "";

    // The extension starts AFTER the period!
    return ext;
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
                             bool testcwd)
{
    bool abs = Filesystem::path_is_absolute (filename);

    // If it's an absolute filename, or if we want to check "." first,
    // then start by checking filename outright.
    if (testcwd || abs) {
        if (Filesystem::is_regular (filename))
            return filename;
    }

#if 0 // NO, don't do this any more.
    // If an absolute filename was not found, we're done.
    if (abs)
        return std::string();
#endif

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
    }
    return std::string();
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
    } catch (const std::exception &e) {
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
    } catch (const std::exception &e) {
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
    } catch (const std::exception &e) {
        r = false;
    }
    return r;
}


}
OIIO_NAMESPACE_EXIT
