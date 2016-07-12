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
/// @brief Utilities for dealing with file names and files portably.
///
/// Some helpful nomenclature:
///  -  "filename" - a file or directory name, relative or absolute
///  -  "searchpath" - a list of directories separated by ':' or ';'.
///


#ifndef OPENIMAGEIO_FILESYSTEM_H
#define OPENIMAGEIO_FILESYSTEM_H

#include <stdint.h>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include "export.h"
#include "oiioversion.h"
#include "string_view.h"

#if defined(_WIN32) && defined(__GLIBCXX__)
#define OIIO_FILESYSTEM_USE_STDIO_FILEBUF 1
#include "fstream_mingw.h"
#endif

OIIO_NAMESPACE_BEGIN

#if OIIO_FILESYSTEM_USE_STDIO_FILEBUF
// MingW uses GCC to build, but does not support having a wchar_t* passed as argument
// of ifstream::open or ofstream::open. To properly support UTF-8 encoding on MingW we must
// use the __gnu_cxx::stdio_filebuf GNU extension that can be used with _wfsopen and returned
// into a istream which share the same API as ifsteam. The same reasoning holds for ofstream.
typedef basic_ifstream<char> ifstream;
typedef basic_ofstream<char> ofstream;
#else
typedef std::ifstream ifstream;
typedef std::ofstream ofstream;
#endif

/// @namespace Filesystem
///
/// @brief Platform-independent utilities for manipulating file names,
/// files, directories, and other file system miscellany.

namespace Filesystem {

/// Return the filename (excluding any directories, but including the
/// file extension, if any) of a filepath.
OIIO_API std::string filename (const std::string &filepath);

/// Return the file extension (including the last '.' if
/// include_dot=true) of a filename or filepath.
OIIO_API std::string extension (const std::string &filepath,
                                bool include_dot=true);

/// Return all but the last part of the path, for example,
/// parent_path("foo/bar") returns "foo", and parent_path("foo")
/// returns "".
OIIO_API std::string parent_path (const std::string &filepath);

/// Replace the file extension of a filename or filepath. Does not alter
/// filepath, just returns a new string.  Note that the new_extension
/// should contain a leading '.' dot.
OIIO_API std::string replace_extension (const std::string &filepath, 
                                        const std::string &new_extension);

/// Turn a searchpath (multiple directory paths separated by ':' or ';')
/// into a vector<string> containing each individual directory.  If
/// validonly is true, only existing and readable directories will end
/// up in the list.  N.B., the directory names will not have trailing
/// slashes.
OIIO_API void searchpath_split (const std::string &searchpath,
                                std::vector<std::string> &dirs,
                                bool validonly = false);

/// Find the first instance of a filename existing in a vector of
/// directories, returning the full path as a string.  If the file is
/// not found in any of the listed directories, return an empty string.
/// If the filename is absolute, the directory list will not be used.
/// If testcwd is true, "." will be tested before the searchpath;
/// otherwise, "." will only be tested if it's explicitly in dirs.  If
/// recursive is true, the directories will be searched recursively,
/// finding a matching file in any subdirectory of the directories
/// listed in dirs; otherwise.
OIIO_API std::string searchpath_find (const std::string &filename,
                                      const std::vector<std::string> &dirs,
                                      bool testcwd = true,
                                      bool recursive = false);

/// Fill a vector-of-strings with the names of all files contained by
/// directory dirname.  If recursive is true, it will return all files
/// below the directory (even in subdirectories), but if recursive is
/// false (the default)If filter_regex is supplied and non-empty, only
/// filenames matching the regular expression will be returned.  Return
/// true if ok, false if there was an error (such as dirname not being
/// found or not actually being a directory).
OIIO_API bool get_directory_entries (const std::string &dirname,
                               std::vector<std::string> &filenames,
                               bool recursive = false,
                               const std::string &filter_regex=std::string());

/// Return true if the path is an "absolute" (not relative) path.
/// If 'dot_is_absolute' is true, consider "./foo" absolute.
OIIO_API bool path_is_absolute (const std::string &path,
                                bool dot_is_absolute=false);

/// Return true if the file exists.
///
OIIO_API bool exists (const std::string &path);


/// Return true if the file exists and is a directory.
///
OIIO_API bool is_directory (const std::string &path);

/// Return true if the file exists and is a regular file.
///
OIIO_API bool is_regular (const std::string &path);

/// Create the directory. Return true for success, false for failure and
/// place an error message in err.
OIIO_API bool create_directory (string_view path, std::string &err);
inline bool create_directory (string_view path) {
    std::string err;
    return create_directory (path, err);
}

/// Copy a file, directory, or link. It is an error if 'to' already exists.
/// Return true upon success, false upon failure and place an error message
/// in err.
OIIO_API bool copy (string_view from, string_view to, std::string &err);
inline bool copy (string_view from, string_view to) {
    std::string err;
    return copy (from, to, err);
}

/// Rename (or move) a file, directory, or link.  Return true upon success,
/// false upon failure and place an error message in err.
OIIO_API bool rename (string_view from, string_view to, std::string &err);
inline bool rename (string_view from, string_view to) {
    std::string err;
    return rename (from, to, err);
}

/// Remove the file or directory. Return true for success, false for
/// failure and place an error message in err.
OIIO_API bool remove (string_view path, std::string &err);
inline bool remove (string_view path) {
    std::string err;
    return remove (path, err);
}

/// Remove the file or directory, including any children (recursively).
/// Return the number of files removed.  Place an error message (if
/// applicable in err.
OIIO_API unsigned long long remove_all (string_view path, std::string &err);
inline unsigned long long remove_all (string_view path) {
    std::string err;
    return remove_all (path, err);
}

/// Return a directory path where temporary files can be made.
///
OIIO_API std::string temp_directory_path ();

/// Return a unique filename suitable for making a temporary file or
/// directory.
OIIO_API std::string unique_path (string_view model="%%%%-%%%%-%%%%-%%%%");

/// Version of fopen that can handle UTF-8 paths even on Windows
///
OIIO_API FILE *fopen (string_view path, string_view mode);

/// Return the current (".") directory path.
///
OIIO_API std::string current_path ();

/// Version of std::ifstream.open that can handle UTF-8 paths
///
OIIO_API void open (OIIO::ifstream &stream, string_view path,
                    std::ios_base::openmode mode = std::ios_base::in);

/// Version of std::ofstream.open that can handle UTF-8 paths
///
OIIO_API void open (OIIO::ofstream &stream, string_view path,
                    std::ios_base::openmode mode = std::ios_base::out);


/// Read the entire contents of the named text file and place it in str,
/// returning true on success, false on failure.
OIIO_API bool read_text_file (string_view filename, std::string &str);

/// Read a maximum of n bytes from the named file, starting at position pos
/// (which defaults to the start of the file), storing results in
/// buffer[0..n-1]. Return the number of bytes read, which will be n for
/// full success, less than n if the file was fewer than n+pos bytes long,
/// or 0 if the file did not exist or could not be read.
OIIO_API size_t read_bytes (string_view path, void *buffer, size_t n,
                            size_t pos=0);

/// Get last modified time of file
///
OIIO_API std::time_t last_write_time (const std::string& path);

/// Set last modified time on file
///
OIIO_API void last_write_time (const std::string& path, std::time_t time);

/// Return the size of the file (in bytes), or uint64_t(-1) if there is any
/// error.
OIIO_API uint64_t file_size (string_view path);

/// Ensure command line arguments are UTF-8 everywhere
///
OIIO_API void convert_native_arguments (int argc, const char *argv[]);

/// Turn a sequence description string into a vector of integers.
/// The sequence description can be any of the following
///  * A value (e.g., "3")
///  * A value range ("1-10", "10-1", "1-10x3", "1-10y3"):
///     START-FINISH        A range, inclusive of start & finish
///     START-FINISHxSTEP   A range with step size
///     START-FINISHySTEP   The complement of a stepped range, that is,
///                           all numbers within the range that would
///                           NOT have been selected by 'x'.
///     Note that START may be > FINISH, or STEP may be negative.
///  * Multiple values or ranges, separated by a comma (e.g., "3,4,10-20x2")
/// Retrn true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_API bool enumerate_sequence (string_view desc,
                                  std::vector<int> &numbers);

/// Given a pattern (such as "foo.#.tif" or "bar.1-10#.exr"), return a
/// normalized pattern in printf format (such as "foo.%04d.tif") and a
/// framespec (such as "1-10").
///
/// If framepadding_override is > 0, it overrides any specific padding amount
/// in the original pattern.
///
/// Return true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_API bool parse_pattern (const char *pattern,
                             int framepadding_override,
                             std::string &normalized_pattern,
                             std::string &framespec);


/// Given a normalized pattern (such as "foo.%04d.tif") and a list of frame
/// numbers, generate a list of filenames.
///
/// Return true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_API bool enumerate_file_sequence (const std::string &pattern,
                                       const std::vector<int> &numbers,
                                       std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "foo_%V.%04d.tif") and a list of frame
/// numbers, generate a list of filenames. "views" is list of per-frame
/// views, or empty. In each frame filename, "%V" is replaced with the view,
/// and "%v" is replaced with the first character of the view.
///
/// Return true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_API bool enumerate_file_sequence (const std::string &pattern,
                                       const std::vector<int> &numbers,
                                       const std::vector<string_view> &views,
                                       std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "/path/to/foo.%04d.tif") scan the
/// containing directory (/path/to) for matching frame numbers, views and files.
/// "%V" in the pattern matches views, while "%v" matches the first character
/// of each entry in views.
///
/// Return true upon success, false if the directory doesn't exist or the
/// pattern can't be parsed.
OIIO_API bool scan_for_matching_filenames (const std::string &pattern,
                                           const std::vector<string_view> &views,
                                           std::vector<int> &frame_numbers,
                                           std::vector<string_view> &frame_views,
                                           std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "/path/to/foo.%04d.tif") scan the
/// containing directory (/path/to) for matching frame numbers and files.
///
/// Return true upon success, false if the directory doesn't exist or the
/// pattern can't be parsed.
OIIO_API bool scan_for_matching_filenames (const std::string &pattern,
                                           std::vector<int> &numbers,
                                           std::vector<std::string> &filenames);

};  // namespace Filesystem

OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_FILESYSTEM_H
