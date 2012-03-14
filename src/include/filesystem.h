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


/// @file  filesystem.h
///
/// @brief Utilities for dealing with file names and files.  We use
/// boost::filesystem anywhere we can, but that doesn't cover everything
/// we want to do.
///
/// Some helpful nomenclature:
///  -  "filename" - a file or directory name, relative or absolute
///  -  "searchpath" - a list of directories separated by ':' or ';'.
///


#ifndef OPENIMAGEIO_FILESYSTEM_H
#define OPENIMAGEIO_FILESYSTEM_H

#include <string>

#include "export.h"
#include "version.h"


OIIO_NAMESPACE_ENTER
{

/// @namespace Filesystem
///
/// @brief Platform-independent utilities for manipulating file names,
/// files, directories, and other file system miscellany.

namespace Filesystem {

/// Return the filename (excluding any directories, but including the
/// file extension, if any) of a filepath.
DLLPUBLIC std::string filename (const std::string &filepath);

/// Return the file extension (including the last '.') of a filename or
/// filepath.
DLLPUBLIC std::string extension (const std::string &filepath);

/// Return the file extension (just the part after the last '.') of a
/// filename or filepath.  DEPRECATED.
DLLPUBLIC std::string file_extension (const std::string &filepath);

/// Replace the file extension of a filename or filepath. Does not
/// alter filepath, just returns a new string
DLLPUBLIC std::string replace_extension (const std::string &filepath, 
                                         const std::string &new_extension);

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


/// Return true if the file exists.
///
DLLPUBLIC bool exists (const std::string &path);


/// Return true if the file exists and is a directory.
///
DLLPUBLIC bool is_directory (const std::string &path);

/// Return true if the file exists and is a regular file.
///
DLLPUBLIC bool is_regular (const std::string &path);

};  // namespace Filesystem

}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_FILESYSTEM_H
