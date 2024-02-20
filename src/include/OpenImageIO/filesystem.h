// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

/// @file  filesystem.h
///
/// @brief Utilities for dealing with file names and files portably.
///
/// Some helpful nomenclature:
///  -  "filename" - a file or directory name, relative or absolute
///  -  "searchpath" - a list of directories separated by ':' or ';'.
///


#pragma once

#define OIIO_FILESYSTEM_H

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/string_view.h>

#if defined(_WIN32) && defined(__GLIBCXX__)
#    define OIIO_FILESYSTEM_USE_STDIO_FILEBUF 1
#    include <OpenImageIO/fstream_mingw.h>
#endif


// Define symbols that let client applications determine if newly added
// features are supported.
#define OIIO_FILESYSTEM_SUPPORTS_IOPROXY 1



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
/// file extension, if any) of a UTF-8 encoded filepath.
OIIO_UTIL_API std::string filename (string_view filepath) noexcept;

/// Return the file extension (including the last '.' if
/// include_dot=true) of a UTF-8 encoded filename or filepath.
OIIO_UTIL_API std::string extension (string_view filepath,
                                bool include_dot=true) noexcept;

/// Return all but the last part of the UTF-8 encoded path, for example,
/// parent_path("foo/bar") returns "foo", and parent_path("foo")
/// returns "".
OIIO_UTIL_API std::string parent_path (string_view filepath) noexcept;

/// Replace the file extension of a UTF-8 encoded filename or filepath. Does
/// not alter filepath, just returns a new string.  Note that the
/// new_extension should contain a leading '.' dot.
OIIO_UTIL_API std::string replace_extension (const std::string &filepath, 
                                        const std::string &new_extension) noexcept;

/// Return the filepath in generic format, not any OS-specific conventions.
/// Input and output are both UTF-8 encoded.
OIIO_UTIL_API std::string generic_filepath (string_view filepath) noexcept;

/// Turn a searchpath (multiple UTF-8 encoded directory paths separated by ':'
/// or ';') into a vector<string> containing the name of each individual
/// directory.  If validonly is true, only existing and readable directories
/// will end up in the list.  N.B., the directory names will not have trailing
/// slashes.
OIIO_UTIL_API std::vector<std::string>
searchpath_split(string_view searchpath, bool validonly = false);

inline void searchpath_split (string_view searchpath,
                              std::vector<std::string> &dirs,
                              bool validonly = false)
{
    dirs = searchpath_split(searchpath, validonly);
}

/// Find the first instance of a filename existing in a vector of
/// directories, returning the full path as a string.  If the file is
/// not found in any of the listed directories, return an empty string.
/// If the filename is absolute, the directory list will not be used.
/// If testcwd is true, "." will be tested before the searchpath;
/// otherwise, "." will only be tested if it's explicitly in dirs.  If
/// recursive is true, the directories will be searched recursively,
/// finding a matching file in any subdirectory of the directories
/// listed in dirs; otherwise. All file and directory names are presumed
/// to be UTF-8 encoded.
OIIO_UTIL_API std::string searchpath_find (const std::string &filename,
                                      const std::vector<std::string> &dirs,
                                      bool testcwd = true,
                                      bool recursive = false);

/// Find the given program in the `$PATH` searchpath and return its full path.
/// If the program is not found, return an empty string.
OIIO_UTIL_API std::string find_program(string_view program);

/// Fill a vector-of-strings with the names of all files contained by
/// directory dirname.  If recursive is true, it will return all files below
/// the directory (even in subdirectories). If filter_regex is supplied and
/// non-empty, only filenames matching the regular expression will be
/// returned.  Return true if ok, false if there was an error (such as
/// dirname not being found or not actually being a directory). All file
/// and directory names are presumed to be UTF-8 encoded.
OIIO_UTIL_API bool get_directory_entries (const std::string &dirname,
                               std::vector<std::string> &filenames,
                               bool recursive = false,
                               const std::string &filter_regex=std::string());

/// Return true if the UTF-8 encoded path is an "absolute" (not relative)
/// path. If 'dot_is_absolute' is true, consider "./foo" absolute.
OIIO_UTIL_API bool path_is_absolute (string_view path,
                                     bool dot_is_absolute=false);

/// Return true if the UTF-8 encoded path exists.
///
OIIO_UTIL_API bool exists (string_view path) noexcept;


/// Return true if the UTF-8 encoded path exists and is a directory.
///
OIIO_UTIL_API bool is_directory (string_view path) noexcept;

/// Return true if the UTF-8 encoded path exists and is a regular file.
///
OIIO_UTIL_API bool is_regular (string_view path) noexcept;

/// Return true if the UTF-8 encoded path is an executable file (by any of
/// user, group, or owner).
OIIO_UTIL_API bool is_executable(string_view path) noexcept;

/// Create the directory, whose name is UTF-8 encoded. Return true for
/// success, false for failure and place an error message in err.
OIIO_UTIL_API bool create_directory (string_view path, std::string &err);
inline bool create_directory (string_view path) {
    std::string err;
    return create_directory (path, err);
}

/// Copy a file, directory, or link. It is an error if 'to' already exists.
/// The file names are all UTF-8 encoded. Return true upon success, false upon
/// failure and place an error message in err.
OIIO_UTIL_API bool copy (string_view from, string_view to, std::string &err);
inline bool copy (string_view from, string_view to) {
    std::string err;
    return copy (from, to, err);
}

/// Rename (or move) a file, directory, or link. The file names are all UTF-8
/// encoded. Return true upon success, false upon failure and place an error
/// message in err.
OIIO_UTIL_API bool rename (string_view from, string_view to, std::string &err);
inline bool rename (string_view from, string_view to) {
    std::string err;
    return rename (from, to, err);
}

/// Remove the file or directory. The file names are all UTF-8 encoded. Return
/// true for success, false for failure and place an error message in err.
OIIO_UTIL_API bool remove (string_view path, std::string &err);
inline bool remove (string_view path) {
    std::string err;
    return remove (path, err);
}

/// Remove the file or directory, including any children (recursively). The
/// file names are all UTF-8 encoded. Return the number of files removed.
/// Place an error message (if applicable in err.
OIIO_UTIL_API unsigned long long remove_all (string_view path, std::string &err);
inline unsigned long long remove_all (string_view path) {
    std::string err;
    return remove_all (path, err);
}

/// Return a directory path (UTF-8 encoded) where temporary files can be made.
///
OIIO_UTIL_API std::string temp_directory_path ();

/// Return a unique filename suitable for making a temporary file or
/// directory.  The file names are all UTF-8 encoded.
/// NOTE: this function is not recommended, because it's a known security
/// and stability issue, since another process *could* create a file of the
/// same name after the path is retrieved but before it is created. So in
/// the long run, we want to wean ourselves off this. But in practice, it's
/// not an emergency. We'll replace this with something else eventually.
OIIO_UTIL_API std::string unique_path (string_view model="%%%%-%%%%-%%%%-%%%%");

/// Version of fopen that can handle UTF-8 paths even on Windows.
OIIO_UTIL_API FILE *fopen (string_view path, string_view mode);

/// Version of fseek that works with 64 bit offsets on all systems.
/// Like std::fseek, returns zero on success, nonzero on failure.
OIIO_UTIL_API int fseek (FILE *file, int64_t offset, int whence);

/// Version of ftell that works with 64 bit offsets on all systems.
OIIO_UTIL_API int64_t ftell (FILE *file);

/// Return the current (".") directory path.
///
OIIO_UTIL_API std::string current_path ();

/// Version of std::ifstream.open that can handle UTF-8 paths
///
OIIO_UTIL_API void open (OIIO::ifstream &stream, string_view path,
                    std::ios_base::openmode mode = std::ios_base::in);

/// Version of std::ofstream.open that can handle UTF-8 paths
///
OIIO_UTIL_API void open (OIIO::ofstream &stream, string_view path,
                    std::ios_base::openmode mode = std::ios_base::out);

/// Version of C open() that can handle UTF-8 paths, returning an integer
/// file descriptor. Note that the flags are passed to underlying calls to
/// open()/_open() and therefore may be OS specific -- use with caution! If
/// you want more OS-agnostic file opening, prefer the FILE or stream
/// methods of IO. (N.B.: use of this function requires the caller to
/// `#include <fcntl.h>` in order to get the definitions of the flags.)
OIIO_UTIL_API int open (string_view path, int flags);

/// Read the entire contents of the named text file (as a UTF-8 encoded
/// filename) and place it in str, returning true on success, false on
/// failure.  The optional size parameter gives the maximum amount to read
/// (for memory safety) and defaults to 16MB. Set size to 0 for no limit
/// (use at your own risk).
OIIO_UTIL_API bool read_text_file(string_view filename, std::string &str,
                                  size_t size = (1UL << 24));

/// Run a command line process and capture its console output in `str`,
/// returning true on success, false on failure.  The optional size parameter
/// gives the maximum amount to read (for memory safety) and defaults to 16MB.
/// Set size to 0 for no limit (use at your own risk).
OIIO_UTIL_API bool read_text_from_command(string_view command,
                                          std::string &str,
                                          size_t size = (1UL << 24));

/// Write the entire contents of the string `str` to the named file (UTF-8
/// encoded), overwriting any prior contents of the file (if it existed),
/// returning true on success, false on failure.
OIIO_UTIL_API bool write_text_file (string_view filename, string_view str);

/// Write the entire contents of the span `data` to the file (UTF-8 encoded)
/// as a binary blob, overwriting any prior contents of the file (if it
/// existed), returning true on success, false on failure.
template<typename T>
bool write_binary_file (string_view filename, cspan<T> data)
{
    OIIO::ofstream out;
    Filesystem::open(out, filename, std::ios::out | std::ios::binary);
    out.write((const char*)data.data(), data.size() * sizeof(T));
    return out.good();
}

template<typename T>
bool write_binary_file (string_view filename, const std::vector<T>& data)
{
    return write_binary_file(filename, cspan<T>(data));
}

/// Read a maximum of n bytes from the named file, starting at position pos
/// (which defaults to the start of the file), storing results in
/// buffer[0..n-1]. Return the number of bytes read, which will be n for
/// full success, less than n if the file was fewer than n+pos bytes long,
/// or 0 if the file did not exist or could not be read.
OIIO_UTIL_API size_t read_bytes (string_view path, void *buffer, size_t n,
                                 size_t pos=0);

/// Get last modified time of the file named by `path` (UTF-8 encoded).
///
OIIO_UTIL_API std::time_t last_write_time (string_view path) noexcept;

/// Set last modified time on the file named by `path` (UTF-8 encoded).
///
OIIO_UTIL_API void last_write_time (string_view path, std::time_t time) noexcept;

/// Return the size of the file (in bytes), or uint64_t(-1) if there is any
/// error. The file name is UTF-8 encoded.
OIIO_UTIL_API uint64_t file_size (string_view path) noexcept;

/// Ensure command line arguments are UTF-8 everywhere.
OIIO_UTIL_API void convert_native_arguments (int argc, const char *argv[]);

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
/// Return true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_UTIL_API bool enumerate_sequence (string_view desc,
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
OIIO_UTIL_API bool parse_pattern (const char *pattern,
                                  int framepadding_override,
                                  std::string &normalized_pattern,
                                  std::string &framespec);


/// Given a normalized pattern (such as "foo.%04d.tif") and a list of frame
/// numbers, generate a list of filenames. All the filename strings will be
/// presumed to be UTF-8 encoded.
///
/// Return true upon success, false if the description was too malformed
/// to generate a sequence.
OIIO_UTIL_API bool enumerate_file_sequence (const std::string &pattern,
                                       const std::vector<int> &numbers,
                                       std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "foo_%V.%04d.tif") and a list of frame
/// numbers, generate a list of filenames. "views" is list of per-frame views,
/// or empty. In each frame filename, "%V" is replaced with the view, and "%v"
/// is replaced with the first character of the view. All the filename strings
/// will be presumed to be UTF-8 encoded.
///
/// Return true upon success, false if the description was too malformed to
/// generate a sequence.
OIIO_UTIL_API bool enumerate_file_sequence (const std::string &pattern,
                                       const std::vector<int> &numbers,
                                       const std::vector<string_view> &views,
                                       std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "/path/to/foo.%04d.tif") scan the
/// containing directory (/path/to) for matching frame numbers, views and
/// files. "%V" in the pattern matches views, while "%v" matches the first
/// character of each entry in views. All the filename strings will be
/// presumed to be UTF-8 encoded.
///
/// Return true upon success, false if the directory doesn't exist or the
/// pattern can't be parsed.
OIIO_UTIL_API bool scan_for_matching_filenames (const std::string &pattern,
                                           const std::vector<string_view> &views,
                                           std::vector<int> &frame_numbers,
                                           std::vector<string_view> &frame_views,
                                           std::vector<std::string> &filenames);

/// Given a normalized pattern (such as "/path/to/foo.%04d.tif") scan the
/// containing directory (/path/to) for matching frame numbers and files. All
/// the filename strings will be presumed to be UTF-8 encoded.
///
/// Return true upon success, false if the directory doesn't exist or the
/// pattern can't be parsed.
OIIO_UTIL_API bool scan_for_matching_filenames (const std::string &pattern,
                                           std::vector<int> &numbers,
                                           std::vector<std::string> &filenames);

/// Convert a UTF-8 encoded filename into a regex-safe pattern -- any special
/// regex characters `.`, `(`, `)`, `[`, `]`, `{`, `}` are backslashed. If
/// `simple_glob` is also true, then replace `?` with `.?` and `*` with `.*`.
/// This doesn't support full Unix command line glob syntax (no char sets
/// `[abc]` or string sets `{ab,cd,ef}`), but it does handle simple globbing
/// of `?` to mean any single character and `*` to mean any sequence of 0 or
/// more characters.
OIIO_UTIL_API std::string filename_to_regex(string_view pattern,
                                            bool simple_glob = true);



/// Proxy class for I/O. This provides a simplified interface for file I/O
/// that can have custom overrides. All char-based filenames are assumed to be
/// UTF-8 encoded.
class OIIO_UTIL_API IOProxy {
public:
    enum Mode { Closed = 0, Read = 'r', Write = 'w' };
    IOProxy () {}
    IOProxy (string_view filename, Mode mode)
        : m_filename(filename), m_mode(mode) {}
    IOProxy(const std::wstring& filename, Mode mode)
        : IOProxy(Strutil::utf16_to_utf8(filename), mode) {}
    virtual ~IOProxy () { }
    virtual const char* proxytype () const = 0;
    virtual void close () { }
    virtual bool opened () const { return mode() != Closed; }
    virtual int64_t tell () { return m_pos; }
    // Seek to the position, returning true on success, false on failure.
    // Note the difference between this and std::fseek() which returns 0 on
    // success, and -1 on failure.
    virtual bool seek (int64_t offset) { m_pos = offset; return true; }
    // Read `size` bytes at the current position into `buf[]`, returning the
    // number of bytes successfully read.
    virtual size_t read (void *buf, size_t size);
    // Write `size` bytes from `buf[]` at the current position, returning the
    // number of bytes successfully written.
    virtual size_t write (const void *buf, size_t size);

    /// Read `size` bytes starting at the `offset` position into `buf[]`,
    /// returning the number of bytes successfully read. This function does
    /// not alter the current file position. This function is thread-safe against
    /// all other concurrent calls to pread() and pwrite(), but not against any
    /// other function of IOProxy.
    virtual size_t pread (void *buf, size_t size, int64_t offset);

    /// Write `size` bytes from `buf[]` to file starting at the `offset` position,
    /// returning the number of bytes successfully written. This function does
    /// not alter the current file position. This function is thread-safe against
    /// all other concurrent calls to pread() and pwrite(), but not against any
    /// other function of IOProxy.
    virtual size_t pwrite (const void *buf, size_t size, int64_t offset);

    // Return the total size of the proxy data, in bytes.
    virtual size_t size () const { return 0; }
    virtual void flush () const { }

    Mode mode () const { return m_mode; }
    const std::string& filename () const { return m_filename; }
    template<class T> size_t read (span<T> buf) {
        return read (buf.data(), buf.size()*sizeof(T));
    }
    template<class T> size_t write (span<T> buf) {
        return write (buf.data(), buf.size()*sizeof(T));
    }
    size_t write (string_view buf) {
        return write (buf.data(), buf.size());
    }
    bool seek (int64_t offset, int origin) {
        return seek ((origin == SEEK_SET ? offset : 0) +
                     (origin == SEEK_CUR ? offset+tell() : 0) +
                     (origin == SEEK_END ? offset+int64_t(size()) : 0));
    }

    #define OIIO_IOPROXY_HAS_ERROR 1
    std::string error() const;
    void error(string_view e);

protected:
    std::string m_filename;
    int64_t m_pos = 0;
    Mode m_mode   = Closed;
    std::string m_error;
};


/// IOProxy subclass for reading or writing (but not both) that wraps C
/// stdio 'FILE'.
class OIIO_UTIL_API IOFile : public IOProxy {
public:
    // Construct from a filename, open, own the FILE*.
    IOFile(string_view filename, Mode mode);
    IOFile(const std::wstring& filename, Mode mode)
        : IOFile(Strutil::utf16_to_utf8(filename), mode) {}
    // Construct from an already-open FILE* that is owned by the caller.
    // Caller is responsible for closing the FILE* after the proxy is gone.
    IOFile(FILE* file, Mode mode);
    ~IOFile() override;
    const char* proxytype() const override { return "file"; }
    void close() override;
    bool seek(int64_t offset) override;
    size_t read(void* buf, size_t size) override;
    size_t write(const void* buf, size_t size) override;
    size_t pread(void* buf, size_t size, int64_t offset) override;
    size_t pwrite(const void* buf, size_t size, int64_t offset) override;
    size_t size() const override;
    void flush() const override;

    // Access the FILE*
    FILE* handle() const { return m_file; }

protected:
    FILE* m_file      = nullptr;
    size_t m_size     = 0;
    bool m_auto_close = false;
    std::mutex m_mutex;
};


/// IOProxy subclass for writing that wraps a std::vector<char> that will
/// grow as we write.
class OIIO_UTIL_API IOVecOutput : public IOProxy {
public:
    // Construct, IOVecOutput owns its own vector.
    IOVecOutput()
        : IOProxy("", IOProxy::Write)
        , m_buf(m_local_buf)
    {
    }
    // Construct to wrap an existing vector.
    IOVecOutput(std::vector<unsigned char>& buf)
        : IOProxy("", Write)
        , m_buf(buf)
    {
    }
    const char* proxytype() const override { return "vecoutput"; }
    size_t write(const void* buf, size_t size) override;
    size_t pwrite(const void* buf, size_t size, int64_t offset) override;
    size_t size() const override { return m_buf.size(); }

    // Access the buffer
    std::vector<unsigned char>& buffer() const { return m_buf; }

protected:
    std::vector<unsigned char>& m_buf;       // reference to buffer
    std::vector<unsigned char> m_local_buf;  // our own buffer
    std::mutex m_mutex;                      // protect the buffer
};



/// IOProxy subclass for reading that wraps an cspan<char>.
class OIIO_UTIL_API IOMemReader : public IOProxy {
public:
    IOMemReader(const void* buf, size_t size)
        : IOProxy("", Read)
        , m_buf((const unsigned char*)buf, size)
    {
    }
    IOMemReader(cspan<unsigned char> buf)
        : IOProxy("", Read)
        , m_buf(buf.data(), buf.size())
    {
    }
    const char* proxytype() const override { return "memreader"; }
    bool seek(int64_t offset) override
    {
        m_pos = offset;
        return true;
    }
    size_t read(void* buf, size_t size) override;
    size_t pread(void* buf, size_t size, int64_t offset) override;
    size_t size() const override { return m_buf.size(); }

    // Access the buffer (caveat emptor)
    cspan<unsigned char> buffer() const noexcept { return m_buf; }

protected:
    cspan<unsigned char> m_buf;
};

};  // namespace Filesystem

OIIO_NAMESPACE_END
