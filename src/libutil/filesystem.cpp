// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <regex>
#include <string>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/ustring.h>

#ifdef _WIN32
#    include <windows.h>

#    include <direct.h>
#    include <io.h>
#    include <shellapi.h>
#    include <sys/types.h>
#    include <sys/utime.h>
#else
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
#    include <utime.h>
#endif

namespace filesystem = std::filesystem;
using std::error_code;



OIIO_NAMESPACE_BEGIN


inline filesystem::path
u8path(string_view name)
{
#ifdef _WIN32
    return filesystem::path(Strutil::utf8_to_utf16wstring(name));
#else
    return filesystem::path(name.begin(), name.end());
#endif
}

inline filesystem::path
u8path(const std::string& name)
{
#ifdef _WIN32
    return filesystem::path(Strutil::utf8_to_utf16wstring(name));
#else
    return filesystem::path(name);
#endif
}

inline std::string
pathstr(const filesystem::path& p)
{
#ifdef _WIN32
    return Strutil::utf16_to_utf8(p.native());
#else
    return p.string();
#endif
}



std::string
Filesystem::filename(string_view filepath) noexcept
{
    try {
        return pathstr(u8path(filepath).filename());
    } catch (...) {
        return filepath;
    }
}



std::string
Filesystem::extension(string_view filepath, bool include_dot) noexcept
{
    std::string s;
    try {
        s = pathstr(u8path(filepath).extension());
    } catch (...) {
    }
    if (!include_dot && !s.empty() && s[0] == '.')
        s.erase(0, 1);  // erase the first character
    return s;
}



std::string
Filesystem::parent_path(string_view filepath) noexcept
{
    try {
        return pathstr(u8path(filepath).parent_path());
    } catch (...) {
        return filepath;
    }
}



std::string
Filesystem::replace_extension(const std::string& filepath,
                              const std::string& new_extension) noexcept
{
    try {
        return pathstr(u8path(filepath).replace_extension(new_extension));
    } catch (...) {
        return filepath;
    }
}



std::string
Filesystem::generic_filepath(string_view filepath) noexcept
{
    try {
        return pathstr(u8path(filepath).generic_string());
    } catch (...) {
        return filepath;
    }
}



std::vector<std::string>
Filesystem::searchpath_split(string_view searchpath, bool validonly)
{
    std::vector<std::string> dirs;

    while (searchpath.size()) {
        // Pluck the next path from the searchpath list
        std::string path = Strutil::parse_until(searchpath, ":;");

#ifdef _WIN32
        // On Windows, we might see something like "a:foo" and any human
        // would know that it means drive/directory 'a:foo', NOT
        // separate directories 'a' and 'foo'.  Implement the obvious
        // heuristic here.  Note that this means that we simply don't
        // correctly support searching in *relative* directories that
        // consist of a single letter.
        if (path.size() == 1 && searchpath.size()
            && searchpath.front() == ':') {
            std::string drive = path;
            searchpath.remove_prefix(1);  // eat the separator
            path = drive + ":"
                   + std::string(Strutil::parse_until(searchpath, ":;"));
        }
#endif
        if (searchpath.size())
            searchpath.remove_prefix(1);  // eat the separator

        // Kill trailing slashes (but not simple "/")
        size_t len = path.size();
        while (len > 1 && (path.back() == '/' || path.back() == '\\'))
            path.erase(--len);
        if (path.empty())
            continue;
        // If it's a valid directory, or if validonly is false, add it
        // to the list
        if (!validonly || Filesystem::is_directory(path))
            dirs.push_back(path);
    }
    return dirs;
}



std::string
Filesystem::searchpath_find(const std::string& filename_utf8,
                            const std::vector<std::string>& dirs, bool testcwd,
                            bool recursive)
{
    const filesystem::path filename(u8path(filename_utf8));
    bool abs = filename.is_absolute();

    // If it's an absolute filename, or if we want to check "." first,
    // then start by checking filename outright.
    if (testcwd || abs) {
        if (is_regular(filename_utf8))
            return filename_utf8;
    }

    // Relative filename, not yet found -- try each directory in turn
    for (auto&& d_utf8 : dirs) {
        // std::cerr << "\tPath = '" << d << "'\n";
        const filesystem::path d(u8path(d_utf8));
        filesystem::path f = d / filename;
        error_code ec;
        if (filesystem::is_regular_file(f, ec)) {
            return pathstr(f);
        }

        if (recursive && filesystem::is_directory(d, ec)) {
            std::vector<std::string> subdirs;
            try {
                for (filesystem::directory_iterator s(d, ec), end_iter;
                     !ec && s != end_iter; s.increment(ec)) {
                    if (filesystem::is_directory(s->path(), ec)) {
                        subdirs.push_back(pathstr(s->path()));
                    }
                }
            } catch (...) {
            }
            std::string found = searchpath_find(filename_utf8, subdirs, false,
                                                true);
            if (found.size())
                return found;
        }
    }
    return std::string();
}



std::string
Filesystem::find_program(string_view program)
{
    const filesystem::path filename(u8path(program));
    bool abs = filename.is_absolute();

    // If it's an absolute path, we are only checking if it's executable
    if (abs)
        return Filesystem::is_executable(program) ? std::string(program)
                                                  : std::string();

    // If it's found without searching, and it's executable, return its
    // absolute path.
    if (Filesystem::is_executable(program))
        return pathstr(filesystem::absolute(filename));

    // Relative filename, not yet found -- try each $PATH directory in turn
    for (auto&& d_utf8 : searchpath_split(OIIO::Sysutil::getenv("PATH"))) {
        const filesystem::path d(u8path(d_utf8));
        filesystem::path f = d / filename;
        // Strutil::print("\tPath = {}\n", f);
        auto p = pathstr(filesystem::absolute(f));
        if (is_executable(p))
            return p;
#ifdef _WIN32
        if (!Strutil::iends_with(p, ".exe")
            && is_executable(Strutil::concat(p, ".exe")))
            return Strutil::concat(p, ".exe");
#endif
    }
    return std::string();
}



bool
Filesystem::get_directory_entries(const std::string& dirname,
                                  std::vector<std::string>& filenames,
                                  bool recursive,
                                  const std::string& filter_regex)
{
    filenames.clear();
    if (dirname.size() && !is_directory(dirname))
        return false;
    filesystem::path dirpath(dirname.size() ? u8path(dirname)
                                            : filesystem::path("."));
    std::regex re;
    try {
        re = std::regex(filter_regex);
        if (recursive) {
            error_code ec;
            for (filesystem::recursive_directory_iterator s(dirpath, ec), end;
                 !ec && s != end; s.increment(ec)) {
                std::string file = pathstr(s->path());
                if (!filter_regex.size() || std::regex_search(file, re))
                    filenames.push_back(file);
            }
        } else {
            error_code ec;
            for (filesystem::directory_iterator s(dirpath, ec), end;
                 !ec && s != end; s.increment(ec)) {
                std::string file = pathstr(s->path());
                if (!filter_regex.size() || std::regex_search(file, re))
                    filenames.push_back(file);
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}



bool
Filesystem::path_is_absolute(string_view path, bool dot_is_absolute)
{
    // "./foo" is considered absolute if dot_is_absolute is true.
    // Don't get confused by ".foo", which is not absolute!
    size_t len = path.length();
    if (!len)
        return false;
    return (path[0] == '/')
           || (dot_is_absolute && path[0] == '.' && path[1] == '/')
           || (dot_is_absolute && path[0] == '.' && path[1] == '.'
               && path[2] == '/')
#ifdef _WIN32
           || path[0] == '\\'
           || (dot_is_absolute && path[0] == '.' && path[1] == '\\')
           || (dot_is_absolute && path[0] == '.' && path[1] == '.'
               && path[2] == '\\')
           || (isalpha(path[0]) && path[1] == ':')
#endif
        ;
}



bool
Filesystem::exists(string_view path) noexcept
{
    error_code ec;
    return filesystem::exists(u8path(path), ec);
}



bool
Filesystem::is_directory(string_view path) noexcept
{
    error_code ec;
    return filesystem::is_directory(u8path(path), ec);
}



bool
Filesystem::is_regular(string_view path) noexcept
{
    error_code ec;
    return filesystem::is_regular_file(u8path(path), ec);
}



bool
Filesystem::is_executable(string_view path) noexcept
{
    if (!is_regular(path))
        return false;
    error_code ec;
    auto stat = filesystem::status(u8path(path), ec);
    auto perm = stat.permissions();
    return (perm & filesystem::perms::owner_exec) != filesystem::perms::none
           || (perm & filesystem::perms::group_exec) != filesystem::perms::none
           || (perm & filesystem::perms::others_exec)
                  != filesystem::perms::none;
}



bool
Filesystem::create_directory(string_view path, std::string& err)
{
    error_code ec;
    bool ok = filesystem::create_directory(u8path(path), ec);
    if (ok)
        err.clear();
    else
        err = ec.message();
    return ok;
}


bool
Filesystem::copy(string_view from, string_view to, std::string& err)
{
    error_code ec;
    filesystem::copy(u8path(from), u8path(to), ec);
    if (!ec) {
        err.clear();
        return true;
    } else {
        err = ec.message();
        return false;
    }
}



bool
Filesystem::rename(string_view from, string_view to, std::string& err)
{
    error_code ec;
    filesystem::rename(u8path(from), u8path(to), ec);
    if (!ec) {
        err.clear();
        return true;
    } else {
        err = ec.message();
        return false;
    }
}



bool
Filesystem::remove(string_view path, std::string& err)
{
    error_code ec;
    bool ok = filesystem::remove(u8path(path), ec);
    if (ok)
        err.clear();
    else
        err = ec.message();
    return ok;
}



unsigned long long
Filesystem::remove_all(string_view path, std::string& err)
{
    error_code ec;
    unsigned long long n = filesystem::remove_all(u8path(path), ec);
    if (!ec)
        err.clear();
    else
        err = ec.message();
    return n;
}



std::string
Filesystem::temp_directory_path()
{
    error_code ec;
    filesystem::path p = filesystem::temp_directory_path(ec);
    return ec ? std::string() : pathstr(p);
}



std::string
Filesystem::unique_path(string_view model)
{
    // std::filesystem does not have unique_path(). Punt!
#if defined(_WIN32)
    std::wstring modelStr = Strutil::utf8_to_utf16wstring(model);
    std::wstring name;
#else
    std::string modelStr = model;
    std::string name;
#endif
    static const char chrs[] = "0123456789abcdef";
    static std::mt19937 rg { std::random_device {}() };
    static std::uniform_int_distribution<size_t> pick(0, 15);
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    while (true) {
        name = modelStr;
        // Replace the '%' characters in the name with random hex digits
        for (size_t i = 0, e = modelStr.size(); i < e; ++i)
            if (name[i] == '%')
                name[i] = chrs[pick(rg)];
#if defined(_WIN32)
        if (!exists(Strutil::utf16_to_utf8(name)))
#else
        if (!exists(name))
#endif
            break;
    }
#if defined(_WIN32)
    return Strutil::utf16_to_utf8(name);
#else
    return name;
#endif
}



std::string
Filesystem::current_path()
{
    error_code ec;
    filesystem::path p = filesystem::current_path(ec);
    return ec ? std::string() : pathstr(p);
}



FILE*
Filesystem::fopen(string_view path, string_view mode)
{
#ifdef _WIN32
    // on Windows fopen does not accept UTF-8 paths, so we convert to wide char
    std::wstring wpath = Strutil::utf8_to_utf16wstring(path);
    std::wstring wmode = Strutil::utf8_to_utf16wstring(mode);
    return ::_wfopen(wpath.c_str(), wmode.c_str());
#else
    // on Unix platforms passing in UTF-8 works
    return ::fopen(std::string(path).c_str(), std::string(mode).c_str());
#endif
}



int
Filesystem::fseek(FILE* file, int64_t offset, int whence)
{
#ifdef _MSC_VER
    return _fseeki64(file, __int64(offset), whence);
#else
    return fseeko(file, offset, whence);
#endif
}



int64_t
Filesystem::ftell(FILE* file)
{
#ifdef _MSC_VER
    return _ftelli64(file);
#else
    return ftello(file);
#endif
}



std::string
Filesystem::getline(FILE* file, size_t maxlen)
{
    std::string result;
    char* buf;
    OIIO_ALLOCATE_STACK_OR_HEAP(buf, char, maxlen + 1);
    if (fgets(buf, int(maxlen + 1), file)) {
        buf[maxlen] = 0;  // be sure it is terminated
        if (!feof(file))
            result.assign(buf);
    } else {
        result.assign("");
    }
    return result;
}



void
Filesystem::open(OIIO::ifstream& stream, string_view path,
                 std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ifstream accepts non-standard wchar_t*
    // On MingW, we use our own OIIO::ifstream
    std::wstring wpath = Strutil::utf8_to_utf16wstring(path);
    stream.open(wpath.c_str(), mode);
    stream.seekg(0, std::ios_base::beg);  // force seek, otherwise broken
#else
    stream.open(path, mode);
#endif
}



void
Filesystem::open(OIIO::ofstream& stream, string_view path,
                 std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ofstream accepts non-standard wchar_t*
    // On MingW, we use our own OIIO::ofstream
    std::wstring wpath = Strutil::utf8_to_utf16wstring(path);
    stream.open(wpath.c_str(), mode);
#else
    stream.open(path, mode);
#endif
}



int
Filesystem::open(string_view path, int flags)
{
#ifdef _WIN32
    // on Windows _open does not accept UTF-8 paths, so we convert to wide
    // char and use _wopen.
    std::wstring wpath = Strutil::utf8_to_utf16wstring(path);
    return ::_wopen(wpath.c_str(), flags);
#else
    // on Unix platforms passing in UTF-8 works
    return ::open(std::string(path).c_str(), flags);
#endif
}



/// Read the entire contents of the named file and place it in str,
/// returning true on success, false on failure.
bool
Filesystem::read_text_file(string_view filename, std::string& str, size_t size)
{
    if (size == 0)  // 0 means "no limit"
        size = size_t(-1);
    size_t filesize = Filesystem::file_size(filename);
    OIIO::ifstream in;
    Filesystem::open(in, filename);
    if (!in)
        return false;
    std::ostringstream contents;
    if (filesize <= size) {
        // Simple case, read it as efficiently as possible in one gulp.
        // For info on why this is the fastest method:
        // http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
        // N.B. for binary read: open(in, filename, std::ios::in|std::ios::binary);
        contents << in.rdbuf();
    } else {
        // Caller has asked to limit the size of the resulting string to
        // something smaller than the size of the file. Read the file in
        // 1MB chunks.
        size_t bufsize = std::min(size_t(1UL << 20), filesize);
        std::unique_ptr<char[]> buf(new char[bufsize]);
        while (size > 0) {
            size_t chunksize = std::min(bufsize, size);
            in.read(buf.get(), chunksize);
            contents.write(buf.get(), chunksize);
            size -= chunksize;
        }
    }
    str = contents.str();
    return true;
}



/// Read the entire contents of the named file and place it in str,
/// returning true on success, false on failure.
bool
Filesystem::read_text_from_command(string_view command, std::string& str,
                                   size_t size)
{
    if (size == 0)  // 0 means "no limit"
        size = size_t(-1);

#ifdef _WIN32
    FILE* in = _wpopen(Strutil::utf8_to_utf16wstring(command).c_str(),
                       Strutil::utf8_to_utf16wstring("r").c_str());
#else
    FILE* in = popen(std::string(command).c_str(), "r");
#endif
    if (!in)
        return false;
    std::ostringstream contents;
    size_t bufsize = std::min(size_t(1UL << 20), size);
    std::unique_ptr<char[]> buf(new char[bufsize]);
    while (!feof(in) && size > 0) {
        size_t chunksize = fread(buf.get(), 1, bufsize, in);
        if (chunksize)
            contents.write(buf.get(), chunksize);
        else
            break;
        size -= chunksize;
    }
#ifdef _WIN32
    _pclose(in);
#else
    pclose(in);
#endif
    str = contents.str();
    return true;
}



bool
Filesystem::write_text_file(string_view filename, string_view str)
{
    OIIO::ofstream out;
    Filesystem::open(out, filename);
    // N.B. for binary write: open(out, filename, std::ios::out|std::ios::binary);
    if (out)
        out << str;
    return out.good();
}



/// Read the entire contents of the named file and place it in str,
/// returning true on success, false on failure.
size_t
Filesystem::read_bytes(string_view path, void* buffer, size_t n, size_t pos)
{
    size_t ret = 0;
    if (FILE* file = Filesystem::fopen(path, "rb")) {
        Filesystem::fseek(file, pos, SEEK_SET);
        ret = fread(buffer, 1, n, file);
        fclose(file);
    }
    return ret;
}



std::time_t
Filesystem::last_write_time(string_view path) noexcept
{
#ifdef _WIN32
    struct __stat64 st;
    auto r = _wstat64(u8path(path).c_str(), &st);
#else
    struct stat st;
    auto r = stat(u8path(path).c_str(), &st);
#endif
    if (r == 0) {
        // success
        return st.st_mtime;
    } else {
        // failure
        return 0;
    }
}



void
Filesystem::last_write_time(string_view path, std::time_t time) noexcept
{
#ifdef _WIN32
    struct _utimbuf times;
    times.actime  = time;
    times.modtime = time;
    _wutime(u8path(path).c_str(), &times);
#else
    struct utimbuf times;
    times.actime  = time;
    times.modtime = time;
    utime(u8path(path).c_str(), &times);
#endif
}



uint64_t
Filesystem::file_size(string_view path) noexcept
{
    error_code ec;
    uint64_t sz = filesystem::file_size(u8path(path), ec);
    return ec ? 0 : sz;
}



void
Filesystem::convert_native_arguments(int argc OIIO_MAYBE_UNUSED,
                                     const char* argv[])
{
#ifdef _WIN32
    // Windows only, standard main() entry point does not accept unicode file
    // paths, here we retrieve wide char arguments and convert them to utf8
    if (argc == 0)
        return;

    int native_argc;
    wchar_t** native_argv = CommandLineToArgvW(GetCommandLineW(), &native_argc);

    if (!native_argv || native_argc != argc)
        return;

    for (int i = 0; i < argc; i++) {
        std::string utf8_arg = Strutil::utf16_to_utf8(native_argv[i]);
        argv[i]              = ustring(utf8_arg).c_str();
    }
#else
    // I hate that we have to do this, but gcc gets confused about the
    //    const char* argv OIIO_MAYBE_UNUSED []
    // This seems to be the way around the problem, make it look like it's
    // used.
    (void)argv;
#endif
}



bool
Filesystem::enumerate_sequence(string_view desc, std::vector<int>& numbers)
{
    numbers.clear();

    // Split the sequence description into comma-separated subranges.
    std::vector<string_view> ranges;
    Strutil::split(desc, ranges, ",");

    bool ok = true;

    // For each subrange...
    for (string_view s : ranges) {
        // It's START, START-FINISH, or START-FINISHxSTEP, or START-FINISHySTEP
        // If START>FINISH or if STEP<0, then count down.
        // If 'y' is used, generate the complement.
        int first = 1;
        ok &= Strutil::parse_int(s, first);
        int last        = first;
        int step        = 1;
        bool complement = false;
        if (Strutil::parse_char(s, '-')) {  // it's a range
            ok &= Strutil::parse_int(s, last);
            if (Strutil::parse_char(s, 'x')) {
                ok &= Strutil::parse_int(s, step);
            } else if (Strutil::parse_char(s, 'y')) {
                ok &= Strutil::parse_int(s, step);
                complement = true;
            }
            if (step == 0)
                step = 1;
            if (step < 0 && first < last)
                std::swap(first, last);
            if (first > last && step > 0)
                step = -step;
        }
        int end    = last + (step > 0 ? 1 : -1);
        int itstep = step > 0 ? 1 : -1;
        for (int i = first; i != end; i += itstep) {
            if ((abs(i - first) % abs(step) == 0) != complement)
                numbers.push_back(i);
        }
    }
    return ok;
}



bool
Filesystem::parse_pattern(const char* pattern_, int framepadding_override,
                          std::string& normalized_pattern,
                          std::string& framespec)
{
    std::string pattern(pattern_);

    // The pattern is either a range (e.g., "1-15#"), a
    // set of hash marks (e.g. "####"), or a printf-style format
    // string (e.g. "%04d").
#define ONERANGE_SPEC "[0-9]+(-[0-9]+((x|y)-?[0-9]+)?)?"
#define MANYRANGE_SPEC ONERANGE_SPEC "(," ONERANGE_SPEC ")*"
#define SEQUENCE_SPEC       \
    "(" MANYRANGE_SPEC ")?" \
    "((#|@)+|(%[0-9]*d))"
    static std::regex sequence_re(SEQUENCE_SPEC);
    // std::cout << "pattern >" << (SEQUENCE_SPEC) << "<\n";
    std::match_results<std::string::const_iterator> range_match;
    if (!std::regex_search(pattern, range_match, sequence_re)) {
        // Not a range
        static std::regex all_views_re("%[Vv]");
        if (std::regex_search(pattern, all_views_re)) {
            normalized_pattern = pattern;
            return true;
        }

        return false;
    }

    // It's a range. Generate the names by iterating through the numbers.
    std::string thematch(range_match[0].first, range_match[0].second);
    std::string thesequence(range_match[1].first, range_match[1].second);
    std::string thehashes(range_match[9].first, range_match[9].second);
    std::string theformat(range_match[11].first, range_match[11].second);
    std::string prefix(range_match.prefix().first, range_match.prefix().second);
    std::string suffix(range_match.suffix().first, range_match.suffix().second);

    // std::cout << "theformat: " << theformat << "\n";

    std::string fmt;
    if (theformat.length() > 0) {
        fmt = theformat;
    } else {
        // Compute the amount of padding desired
        int padding = 0;
        for (int i = (int)thematch.length() - 1; i >= 0; --i) {
            if (thematch[i] == '#')
                padding += 4;
            else if (thematch[i] == '@')
                padding += 1;
        }
        if (framepadding_override > 0)
            padding = framepadding_override;
        fmt = Strutil::fmt::format("%0{}d", padding);
    }

    // std::cout << "Format: '" << fmt << "'\n";

    normalized_pattern = prefix + fmt + suffix;
    framespec          = thesequence;

    return true;
}



bool
Filesystem::enumerate_file_sequence(const std::string& pattern,
                                    const std::vector<int>& numbers,
                                    std::vector<std::string>& filenames)
{
    filenames.clear();
    for (int n : numbers) {
        std::string f = Strutil::sprintf(pattern.c_str(), n);
        filenames.push_back(f);
    }
    return true;
}



bool
Filesystem::enumerate_file_sequence(const std::string& pattern,
                                    const std::vector<int>& numbers,
                                    const std::vector<string_view>& views,
                                    std::vector<std::string>& filenames)
{
    OIIO_ASSERT(views.size() == 0 || views.size() == numbers.size());
    filenames.clear();
    for (size_t i = 0, e = numbers.size(); i < e; ++i) {
        std::string f = pattern;
        if (views.size() > 0 && !views[i].empty()) {
            f = Strutil::replace(f, "%V", views[i], true);
            f = Strutil::replace(f, "%v", views[i].substr(0, 1), true);
        }
        f = Strutil::sprintf(f.c_str(), numbers[i]);
        filenames.push_back(f);
    }

    return true;
}



bool
Filesystem::scan_for_matching_filenames(const std::string& pattern,
                                        const std::vector<string_view>& views,
                                        std::vector<int>& frame_numbers,
                                        std::vector<string_view>& frame_views,
                                        std::vector<std::string>& filenames)
{
    static std::regex format_re("%0([0-9]+)d");
    static std::regex all_views_re("%[Vv]"), view_re("%V"), short_view_re("%v");

    frame_numbers.clear();
    frame_views.clear();
    filenames.clear();
    if (std::regex_search(pattern, all_views_re)) {
        if (std::regex_search(pattern, format_re)) {
            // case 1: pattern has format and view
            std::vector<std::pair<std::pair<int, string_view>, std::string>>
                matches;
            for (const auto& view : views) {
                if (view.empty())
                    continue;

                const string_view short_view = view.substr(0, 1);
                std::vector<int> view_numbers;
                std::vector<std::string> view_filenames;

                std::string view_pattern = pattern;
                view_pattern = Strutil::replace(view_pattern, "%V", view, true);
                view_pattern = Strutil::replace(view_pattern, "%v", short_view,
                                                true);

                if (!scan_for_matching_filenames(view_pattern, view_numbers,
                                                 view_filenames))
                    continue;

                for (int j = 0, f = view_numbers.size(); j < f; ++j) {
                    matches.push_back(
                        std::make_pair(std::make_pair(view_numbers[j], view),
                                       view_filenames[j]));
                }
            }

            std::sort(matches.begin(), matches.end());

            for (auto& m : matches) {
                frame_numbers.push_back(m.first.first);
                frame_views.push_back(m.first.second);
                filenames.push_back(m.second);
            }

        } else {
            // case 2: pattern has view, but no format
            std::vector<std::pair<string_view, std::string>> matches;
            for (const auto& view : views) {
                const string_view short_view = view.substr(0, 1);
                std::string view_pattern     = pattern;
                view_pattern = Strutil::replace(view_pattern, "%V", view, true);
                view_pattern = Strutil::replace(view_pattern, "%v", short_view,
                                                true);
                if (exists(view_pattern))
                    matches.push_back(std::make_pair(view, view_pattern));
            }

            std::sort(matches.begin(), matches.end());
            for (auto& m : matches) {
                frame_views.push_back(m.first);
                filenames.push_back(m.second);
            }
        }
        return true;

    } else {
        // case 3: pattern has format, but no view
        return scan_for_matching_filenames(pattern, frame_numbers, filenames);
    }

    return true;
}

bool
Filesystem::scan_for_matching_filenames(const std::string& pattern_,
                                        std::vector<int>& numbers,
                                        std::vector<std::string>& filenames)
{
    numbers.clear();
    filenames.clear();
    std::string pattern = pattern_;
    // Isolate the directory name (or '.' if none was specified)
    std::string directory = Filesystem::parent_path(pattern);
    if (directory.size() == 0) {
        directory = ".";
        pattern   = "./" + pattern;
    }

    if (!exists(directory))
        return false;

    // build a regex that matches the pattern
    static std::regex format_re("%0([0-9]+)d");
    std::match_results<std::string::const_iterator> format_match;
    if (!std::regex_search(pattern, format_match, format_re))
        return false;

    std::string thepadding(format_match[1].first, format_match[1].second);
    std::string prefix(format_match.prefix().first,
                       format_match.prefix().second);
    std::string suffix(format_match.suffix().first,
                       format_match.suffix().second);

    // N.B. make sure that the prefix and suffix are regex-safe by
    // backslashing anything that might be in a literal filename that will
    // be problematic in a regex.
    std::string pattern_re_str = Filesystem::filename_to_regex(prefix, false)
                                 + "([0-9]{" + thepadding + ",})"
                                 + Filesystem::filename_to_regex(suffix, false);
    std::vector<std::pair<int, std::string>> matches;

    // There are some corner cases regex that could be constructed here that
    // are badly structured and might throw an exception.
    try {
        std::regex pattern_re(pattern_re_str);
        error_code ec;
        for (filesystem::directory_iterator it(u8path(directory), ec), end_it;
             !ec && it != end_it; ++it) {
            std::string itpath = Filesystem::generic_filepath(
                it->path().string());
            if (filesystem::is_regular_file(itpath, ec)) {
                const std::string f = pathstr(itpath);
                std::match_results<std::string::const_iterator> frame_match;
                if (regex_match(f, frame_match, pattern_re)) {
                    std::string thenumber(frame_match[1].first,
                                          frame_match[1].second);
                    int frame = Strutil::stoi(thenumber);
                    matches.push_back(std::make_pair(frame, f));
                }
            }
        }

    } catch (...) {
        // Botched regex. Just fail.
        return false;
    }

    // filesystem order is undefined, so return sorted sequences
    std::sort(matches.begin(), matches.end());

    for (auto& m : matches) {
        numbers.push_back(m.first);
        filenames.push_back(m.second);
    }

    return true;
}



std::string
Filesystem::filename_to_regex(string_view pattern, bool simple_glob)
{
    // Replace dot unconditionally, since it's so common in filenames.
    std::string p = Strutil::replace(pattern, ".", "\\.", true);
    // Other problematic chars are rare in filenames, do a quick test to
    // prevent needless string manipulation.
    if (Strutil::contains_any_char(p, "()[]{}")) {
        p = Strutil::replace(p, "(", "\\(", true);
        p = Strutil::replace(p, ")", "\\)", true);
        p = Strutil::replace(p, "[", "\\[", true);
        p = Strutil::replace(p, "]", "\\]", true);
        p = Strutil::replace(p, "{", "\\{", true);
        p = Strutil::replace(p, "}", "\\}", true);
    }
    if (simple_glob && Strutil::contains_any_char(p, "?*")) {
        p = Strutil::replace(p, "?", ".?", true);
        p = Strutil::replace(p, "*", ".*", true);
    }
    return p;
}



size_t
Filesystem::IOProxy::read(void* /*buf*/, size_t /*size*/)
{
    return 0;
}


size_t
Filesystem::IOProxy::write(const void* /*buf*/, size_t /*size*/)
{
    return 0;
}


size_t
Filesystem::IOProxy::pread(void* /*buf*/, size_t /*size*/, int64_t /*offset*/)
{
    return 0;
}


size_t
Filesystem::IOProxy::pwrite(const void* /*buf*/, size_t /*size*/,
                            int64_t /*offset*/)
{
    return 0;
}



// Shared mutex to guard IOProxy error get/set. Shared should be ok. If
// enough file I/O errors are happening that multiple threads are
// simultaneously locking on error retrieval, the user has bigger problems
// than worrying about thread performance.
static std::mutex ioproxy_error_mutex;


std::string
Filesystem::IOProxy::error() const
{
    std::lock_guard<std::mutex> lock(ioproxy_error_mutex);
    return m_error;
}


void
Filesystem::IOProxy::error(string_view e)
{
    std::lock_guard<std::mutex> lock(ioproxy_error_mutex);
    m_error = e;
}



Filesystem::IOFile::IOFile(string_view filename, Mode mode)
    : IOProxy(filename, mode)
{
    // Call Filesystem::fopen since it handles UTF-8 file paths on Windows,
    // which std fopen does not.
    m_file = Filesystem::fopen(m_filename, m_mode == Write ? "w+b" : "rb");
    if (!m_file) {
        m_mode          = Closed;
        int e           = errno;
        const char* msg = e ? std::strerror(e) : nullptr;
        error(msg ? msg : "unknown error");
    }
    m_auto_close = true;
    if (m_mode == Read)
        m_size = Filesystem::file_size(filename);
}

Filesystem::IOFile::IOFile(FILE* file, Mode mode)
    : IOProxy("", mode)
    , m_file(file)
{
    if (m_mode == Read) {
        m_pos = Filesystem::ftell(m_file);           // save old position
        Filesystem::fseek(m_file, 0, SEEK_END);      // seek to end
        m_size = size_t(Filesystem::ftell(m_file));  // size is end position
        Filesystem::fseek(m_file, m_pos, SEEK_SET);  // restore old position
    }
}

Filesystem::IOFile::~IOFile()
{
    if (m_auto_close)
        close();
}

void
Filesystem::IOFile::close()
{
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_mode = Closed;
}

bool
Filesystem::IOFile::seek(int64_t offset)
{
    if (!m_file)
        return false;
    m_pos = offset;
    return Filesystem::fseek(m_file, offset, SEEK_SET) == 0;
}

size_t
Filesystem::IOFile::read(void* buf, size_t size)
{
    if (!m_file || !size || m_mode == Closed)
        return 0;
    size_t r = fread(buf, 1, size, m_file);
    m_pos += r;
    if (r < size) {
        if (feof(m_file))
            error("end of file");
        else if (ferror(m_file)) {
            int e           = errno;
            const char* msg = e ? std::strerror(e) : nullptr;
            error(msg ? msg : "unknown error");
        }
    }
    return r;
}

size_t
Filesystem::IOFile::pread(void* buf, size_t size, int64_t offset)
{
    if (!m_file || !size || offset < 0 || m_mode == Closed)
        return 0;
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_mutex);
    auto origpos = tell();
    seek(offset);
    size_t r = read(buf, size);
    seek(origpos);
    return r;
#else /* Non-Windows: assume POSIX pread is available */
    int fd = fileno(m_file);
    auto r = ::pread(fd, buf, size, offset);
    // FIXME: the system pread returns ssize_t and is -1 on error.
    return r < 0 ? size_t(0) : size_t(r);
#endif
}

size_t
Filesystem::IOFile::write(const void* buf, size_t size)
{
    if (!m_file || !size || m_mode != Write)
        return 0;
    size_t r = fwrite(buf, 1, size, m_file);
    m_pos += r;
    if (m_pos > int64_t(m_size))
        m_size = m_pos;
    return r;
}

size_t
Filesystem::IOFile::pwrite(const void* buf, size_t size, int64_t offset)
{
    if (!m_file || !size || offset < 0 || m_mode != Write)
        return 0;
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_mutex);
    auto origpos = tell();
    seek(offset);
    size_t r = write(buf, size);
    seek(origpos);
    return r;
#else /* Non-Windows: assume POSIX pwrite is available */
    int fd = fileno(m_file);
    auto r = ::pwrite(fd, buf, size, offset);
    // FIXME: the system pwrite returns ssize_t and is -1 on error.
    return r < 0 ? size_t(0) : size_t(r);
#endif
    offset += r;
    if (m_pos > int64_t(m_size))
        m_size = offset;
    return r;
}

size_t
Filesystem::IOFile::size() const
{
    return m_size;
}

void
Filesystem::IOFile::flush()
{
    if (m_file)
        fflush(m_file);
}



size_t
Filesystem::IOVecOutput::read(void* buf, size_t size)
{
    size = pread(buf, size, m_pos);
    m_pos += size;
    return size;
}

size_t
Filesystem::IOVecOutput::write(const void* buf, size_t size)
{
    size = pwrite(buf, size, m_pos);
    m_pos += size;
    return size;
}

size_t
Filesystem::IOVecOutput::pread(void* buf, size_t size, int64_t offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size = std::min(size, size_t(m_buf.size() - offset));
    memcpy(buf, &m_buf[offset], size);
    return size;
}

size_t
Filesystem::IOVecOutput::pwrite(const void* buf, size_t size, int64_t offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (size_t(offset) == m_buf.size()) {  // appending
        if (size == 1)
            m_buf.push_back(*(const unsigned char*)buf);
        else
            m_buf.insert(m_buf.end(), (const char*)buf,
                         (const char*)buf + size);
    } else {
        size_t end = offset + size;
        if (end > m_buf.size())
            m_buf.resize(end);
        memcpy(&m_buf[offset], buf, size);
    }
    return size;
}



size_t
Filesystem::IOMemReader::read(void* buf, size_t size)
{
    size = pread(buf, size, m_pos);
    m_pos += size;
    return size;
}


size_t
Filesystem::IOMemReader::pread(void* buf, size_t size, int64_t offset)
{
    // N.B. No lock necessary
    if (!m_buf.size() || !size)
        return 0;
    if (size + size_t(offset) > std::size(m_buf)) {
        if (offset < 0 || size_t(offset) >= std::size(m_buf)) {
            error(Strutil::fmt::format(
                "Invalid pread offset {} for an IOMemReader buffer of size {}",
                offset, m_buf.size()));
            return 0;
        }
        size = std::size(m_buf) - size_t(offset);
    }
    memcpy(buf, m_buf.data() + offset, size);
    return size;
}


OIIO_NAMESPACE_END
