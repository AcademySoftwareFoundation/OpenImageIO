/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


// Utilities for dealing with file names and files.  We use
// boost::filesystem anywhere we can, but that doesn't cover everything
// we want to do.
//
// Some helpful nomenclature:
//    "filename" - a file or directory name, relative or absolute
//    "searchpath" - a list of directories separated by ':' or ';'.
//


#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include "export.h"


namespace Filesystem {

/// Return just the leaf file name (excluding directories) of a
/// potentially full file path name.
DLLPUBLIC std::string file_leafname (const std::string &filepath);

/// Return just the leaf file name (excluding directories) of a
/// potentially full file path name.
DLLPUBLIC std::string file_directory (const std::string &filepath);

/// Return the file extension (just the part after the last '.') of a 
/// filename.
DLLPUBLIC std::string file_extension (const std::string &filepath);

/// Turn a searchpath (multiple directory paths separated by ':' or ';')
/// into a vector<string> containing each individual directory.  If
/// validonly is true, only existing and readable directories will end
/// up in the list.  N.B., the directory names will not have trailing
/// slashes.
DLLPUBLIC void searchpath_split (const std::string &searchpath,
                                 std::vector<std::string> &dirs,
                                 bool validonly = false);

/// Find the first instance of a filename existing in a vector of
/// directories, returning the full path as a string.  If the file is
/// not found in any of the listed directories, return an empty string.
/// If the filename is absolute, the directory list will not be used.
/// If testcwd is true, "." will be tested before the searchpath; if
/// testcwd is false, "." will only be tested if it's explicitly in
/// dirs.
DLLPUBLIC std::string searchpath_find (const std::string &filename,
                                       const std::vector<std::string> &dirs,
                                       bool testcwd = true);

/// Return true if the path is an "absolute" (not relative) path.
/// If 'dot_is_absolute' is true, consider "./foo" absolute.
DLLPUBLIC bool path_is_absolute (const std::string &path,
                                 bool dot_is_absolute=false);


};  // namespace Filesystem

#endif // FILESYSTEM_H
