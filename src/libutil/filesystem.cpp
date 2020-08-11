// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/tokenizer.hpp>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/refcnt.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/ustring.h>

#ifdef _WIN32
// # include <windows.h>   // Already done by platform.h
#    include <direct.h>
#    include <io.h>
#    include <shellapi.h>
#else
#    include <unistd.h>
#endif

#ifdef USE_BOOST_REGEX
#    include <boost/regex.hpp>
using boost::match_results;
using boost::regex;
using boost::regex_search;
#else
#    include <regex>
using std::match_results;
using std::regex;
using std::regex_search;
#endif

#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
using error_code     = boost::system::error_code;
// FIXME: use std::filesystem when available



OIIO_NAMESPACE_BEGIN


// boost internally doesn't use MultiByteToWideChar (CP_UTF8,...
// to convert char* to wchar_t* because they do not know the encoding
// See boost/filesystem/path.hpp
// The only correct way to do this is to do the conversion ourselves.

inline filesystem::path
u8path(string_view name)
{
#ifdef _WIN32
    return filesystem::path(Strutil::utf8_to_utf16(name));
#else
    return filesystem::path(name.begin(), name.end());
#endif
}

inline filesystem::path
u8path(const std::string& name)
{
#ifdef _WIN32
    return filesystem::path(Strutil::utf8_to_utf16(name));
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



#ifdef _MSC_VER
// fix for https://svn.boost.org/trac/boost/ticket/6320
const std::string dummy_path = "../dummy_path.txt";
const std::string dummy_extension
    = filesystem::path(dummy_path).extension().string();
#endif

std::string
Filesystem::filename(const std::string& filepath) noexcept
{
    // To simplify dealing with platform-specific separators and whatnot,
    // just use the Boost routines:
    return pathstr(u8path(filepath).filename());
}



std::string
Filesystem::extension(const std::string& filepath, bool include_dot) noexcept
{
    std::string s = pathstr(u8path(filepath).extension());
    if (!include_dot && !s.empty() && s[0] == '.')
        s.erase(0, 1);  // erase the first character
    return s;
}



std::string
Filesystem::parent_path(const std::string& filepath) noexcept
{
    return pathstr(u8path(filepath).parent_path());
}



std::string
Filesystem::replace_extension(const std::string& filepath,
                              const std::string& new_extension) noexcept
{
    return pathstr(u8path(filepath).replace_extension(new_extension));
}



void
Filesystem::searchpath_split(const std::string& searchpath,
                             std::vector<std::string>& dirs, bool validonly)
{
    dirs.clear();

    std::string path_copy = searchpath;
    std::string last_token;
    typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
    boost::char_separator<char> sep(":;");
    tokenizer tokens(searchpath, sep);
    for (tokenizer::iterator tok_iter         = tokens.begin();
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
        while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\'))
            path.erase(--len);
        // If it's a valid directory, or if validonly is false, add it
        // to the list
        if (!validonly || Filesystem::is_directory(path))
            dirs.push_back(path);
    }
#if 0
    std::cerr << "Searchpath = '" << searchpath << "'\n";
    for (auto& d : dirs)
        std::cerr << "\tPath = '" << d << "'\n";
    std::cerr << "\n";
#endif
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
        if (filesystem::is_regular(f, ec)) {
            return pathstr(f);
        }

        if (recursive && filesystem::is_directory(d, ec)) {
            std::vector<std::string> subdirs;
            for (filesystem::directory_iterator s(d, ec), end_iter;
                 !ec && s != end_iter; ++s) {
                if (filesystem::is_directory(s->path(), ec)) {
                    subdirs.push_back(pathstr(s->path()));
                }
            }
            std::string found = searchpath_find(filename_utf8, subdirs, false,
                                                true);
            if (found.size())
                return found;
        }
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
    regex re;
    try {
        re = regex(filter_regex);
    } catch (...) {
        return false;
    }

    if (recursive) {
        error_code ec;
        for (filesystem::recursive_directory_iterator s(dirpath, ec);
             !ec && s != filesystem::recursive_directory_iterator(); ++s) {
            std::string file = pathstr(s->path());
            if (!filter_regex.size() || regex_search(file, re))
                filenames.push_back(file);
        }
    } else {
        error_code ec;
        for (filesystem::directory_iterator s(dirpath, ec);
             !ec && s != filesystem::directory_iterator(); ++s) {
            std::string file = pathstr(s->path());
            if (!filter_regex.size() || regex_search(file, re))
                filenames.push_back(file);
        }
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
    error_code ec;
    filesystem::path p = filesystem::unique_path(u8path(model), ec);
    return ec ? std::string() : pathstr(p);
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
    std::wstring wpath = Strutil::utf8_to_utf16(path);
    std::wstring wmode = Strutil::utf8_to_utf16(mode);

    return ::_wfopen(wpath.c_str(), wmode.c_str());
#else
    // on Unix platforms passing in UTF-8 works
    return ::fopen(path.c_str(), mode.c_str());
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



void
Filesystem::open(OIIO::ifstream& stream, string_view path,
                 std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ifstream accepts non-standard wchar_t*
    // On MingW, we use our own OIIO::ifstream
    std::wstring wpath = Strutil::utf8_to_utf16(path);
    stream.open(wpath.c_str(), mode);
    stream.seekg(0, std::ios_base::beg);  // force seek, otherwise broken
#else
    stream.open(path.c_str(), mode);
#endif
}



void
Filesystem::open(OIIO::ofstream& stream, string_view path,
                 std::ios_base::openmode mode)
{
#ifdef _WIN32
    // Windows std::ofstream accepts non-standard wchar_t*
    // On MingW, we use our own OIIO::ofstream
    std::wstring wpath = Strutil::utf8_to_utf16(path);
    stream.open(wpath.c_str(), mode);
#else
    stream.open(path.c_str(), mode);
#endif
}


/// Read the entire contents of the named file and place it in str,
/// returning true on success, false on failure.
bool
Filesystem::read_text_file(string_view filename, std::string& str)
{
    // For info on why this is the fastest method:
    // http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
    OIIO::ifstream in;
    Filesystem::open(in, filename);

    // N.B. for binary read: open(in, filename, std::ios::in|std::ios::binary);
    if (in) {
        std::ostringstream contents;
        contents << in.rdbuf();
        str = contents.str();
        return true;
    }
    return false;
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
    error_code ec;
    std::time_t t = filesystem::last_write_time(u8path(path), ec);
    return ec ? 0 : t;
}



void
Filesystem::last_write_time(string_view path, std::time_t time) noexcept
{
    error_code ec;
    filesystem::last_write_time(u8path(path), time, ec);
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
    static regex sequence_re(SEQUENCE_SPEC);
    // std::cout << "pattern >" << (SEQUENCE_SPEC) << "<\n";
    match_results<std::string::const_iterator> range_match;
    if (!regex_search(pattern, range_match, sequence_re)) {
        // Not a range
        static regex all_views_re("%[Vv]");
        if (regex_search(pattern, all_views_re)) {
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
        fmt = Strutil::sprintf("%%0%dd", padding);
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
    static regex format_re("%0([0-9]+)d");
    static regex all_views_re("%[Vv]"), view_re("%V"), short_view_re("%v");

    frame_numbers.clear();
    frame_views.clear();
    filenames.clear();
    if (regex_search(pattern, all_views_re)) {
        if (regex_search(pattern, format_re)) {
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
#ifdef _WIN32
        pattern = ".\\\\" + pattern;
#else
        pattern = "./" + pattern;
#endif
    }

    if (!exists(directory))
        return false;

    // build a regex that matches the pattern
    static regex format_re("%0([0-9]+)d");
    match_results<std::string::const_iterator> format_match;
    if (!regex_search(pattern, format_match, format_re))
        return false;

    std::string thepadding(format_match[1].first, format_match[1].second);
    std::string prefix(format_match.prefix().first,
                       format_match.prefix().second);
    std::string suffix(format_match.suffix().first,
                       format_match.suffix().second);

    std::string pattern_re_str = prefix + "([0-9]{" + thepadding + ",})"
                                 + suffix;
    std::vector<std::pair<int, std::string>> matches;

    // There are some corner cases regex that could be constructed here that
    // are badly structured and might throw an exception.
    try {
        regex pattern_re(pattern_re_str);

        error_code ec;
        for (filesystem::directory_iterator it(u8path(directory), ec), end_it;
             !ec && it != end_it; ++it) {
            if (filesystem::is_regular(it->path(), ec)) {
                const std::string f = pathstr(it->path());
                match_results<std::string::const_iterator> frame_match;
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



Filesystem::IOFile::IOFile(string_view filename, Mode mode)
    : IOProxy(filename, mode)
{
    // Call Filesystem::fopen since it handles UTF-8 file paths on Windows,
    // which std fopen does not.
    m_file = Filesystem::fopen(m_filename.c_str(),
                               m_mode == Write ? "wb" : "rb");
    if (!m_file)
        m_mode = Closed;
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
    if (!m_file || !size || m_mode != Read)
        return 0;
    size_t r = fread(buf, 1, size, m_file);
    m_pos += r;
    return r;
}

size_t
Filesystem::IOFile::pread(void* buf, size_t size, int64_t offset)
{
    if (!m_file || !size || offset < 0 || m_mode != Read)
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
    return ::pread(fd, buf, size, offset);
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
    int fd   = fileno(m_file);
    size_t r = ::pwrite(fd, buf, size, offset);
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
Filesystem::IOFile::flush() const
{
    if (m_file)
        fflush(m_file);
}



size_t
Filesystem::IOVecOutput::write(const void* buf, size_t size)
{
    size = pwrite(buf, size, m_pos);
    m_pos += size;
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
    if (size + size_t(offset) > size_t(m_buf.size()))
        size = m_buf.size() - size_t(offset);
    memcpy(buf, m_buf.data() + offset, size);
    return size;
}


OIIO_NAMESPACE_END
