// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/platform.h>

// Special dance to disable warnings in the included files related to
// the deprecation of unicode conversion functions.
OIIO_PRAGMA_WARNING_PUSH
OIIO_CLANG_PRAGMA(clang diagnostic ignored "-Wdeprecated-declarations")
#include <codecvt>
#include <locale>
OIIO_PRAGMA_WARNING_POP

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#if defined(__APPLE__) || defined(__FreeBSD__)
#    include <xlocale.h>
#endif
#ifdef _WIN32
#    include <windows.h>
#endif

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/ustring.h>


// We use the public domain stb implementation of vsnprintf because
// not all platforms support a locale-independent version of vsnprintf.
// See: https://github.com/nothings/stb/blob/master/stb_sprintf.h
#define STB_SPRINTF_DECORATE(name) oiio_stbsp_##name
#define STB_SPRINTF_IMPLEMENTATION 1
#if defined(__GNUG__) && !defined(__clang__)
#    pragma GCC diagnostic ignored "-Wstrict-aliasing"
#    define STBI__ASAN OIIO_NO_SANITIZE_ADDRESS
#endif
#define stbsp__uintptr std::uintptr_t

#ifdef OpenImageIO_SANITIZE
#    define STB_SPRINTF_NOUNALIGNED
#endif

#include "stb_sprintf.h"


OIIO_NAMESPACE_BEGIN


namespace {
static std::mutex output_mutex;

// On systems that support it, get a location independent locale.
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)           \
    || defined(__GLIBC__)
static locale_t c_loc = newlocale(LC_ALL_MASK, "C", nullptr);
#elif defined(_WIN32)
static _locale_t c_loc = _create_locale(LC_ALL, "C");
#endif

};  // namespace



// Locale-independent quickie ASCII digit and alphanum tests, good enough
// for our parsing.
inline int
isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}
inline int
islower(char c)
{
    return c >= 'a' && c <= 'z';
}
inline int
isalpha(char c)
{
    return isupper(c) || islower(c);
}
inline int
isdigit(char c)
{
    return c >= '0' && c <= '9';
}



OIIO_NO_SANITIZE_ADDRESS const char*
c_str(string_view str)
{
    // Usual case: either empty, or null-terminated
    if (str.size() == 0)  // empty string
        return "";

    // This clause attempts to find out if there's a string-terminating
    // '\0' character just beyond the boundary of the string_view, in which
    // case, simply returning m_chars (with no ustring creation) is a valid
    // C string.
    //
    // BUG: if the string_view does not simply wrap a null-terminated string
    // (including a std::string or ustring) or substring thereof, and the
    // the character past the end of the string is beyond an allocation
    // boundary, this will be flagged by address sanitizer. And it
    // misbehaves if the memory just beyond the string_view, which isn't
    // part of the string, gets changed during the lifetime of the
    // string_view, and no longer has that terminating null. I think that in
    // the long run, we can't use this trick. I'm kicking that can down the
    // road just a bit because it's such a performance win. But we
    // eventually want to get rid of this method anyway, since it won't be
    // in C++17 string_view. So maybe we'll find ourselves relying on it a
    // lot less, and therefore the performance hit of doing it foolproof
    // won't be as onerous.
    if (str[str.size()] == 0)  // 0-terminated
        return str.data();

    // Rare case: may not be 0-terminated. Bite the bullet and construct a
    // 0-terminated string.  We use ustring as a way to avoid any issues of
    // who cleans up the allocation, though it means that it will stay in
    // the ustring table forever. Punt on this for now, it's an edge case
    // that we need to handle, but is not likely to ever be an issue.
    return ustring(str).c_str();
}



void
Strutil::sync_output(FILE* file, string_view str, bool flush)
{
    if (str.size() && file) {
        std::unique_lock<std::mutex> lock(output_mutex);
        fwrite(str.data(), 1, str.size(), file);
        if (flush)
            fflush(file);
    }
}



void
Strutil::sync_output(std::ostream& file, string_view str, bool flush)
{
    if (str.size()) {
        std::unique_lock<std::mutex> lock(output_mutex);
        file << str;
        if (flush)
            file.flush();
    }
}



namespace pvt {
static const char* oiio_debug_env = getenv("OPENIMAGEIO_DEBUG");
#ifdef NDEBUG
OIIO_UTIL_API int
    oiio_print_debug(oiio_debug_env ? Strutil::stoi(oiio_debug_env) : 0);
#else
OIIO_UTIL_API int
    oiio_print_debug(oiio_debug_env ? Strutil::stoi(oiio_debug_env) : 1);
#endif
OIIO_UTIL_API int oiio_print_uncaught_errors(1);
}  // namespace pvt



// ErrorHolder houses a string, with the addition that when it is destroyed,
// it will disgorge any un-retrieved error messages, in an effort to help
// beginning users diagnose their problems if they have forgotten to call
// geterror().
struct ErrorHolder {
    std::string error_msg;

    ~ErrorHolder()
    {
        if (!error_msg.empty() && pvt::oiio_print_uncaught_errors) {
            OIIO::print(
                "OpenImageIO exited with a pending error message that was never\n"
                "retrieved via OIIO::geterror(). This was the error message:\n{}\n",
                error_msg);
        }
    }
};


// To avoid thread oddities, we have the storage area buffering error
// messages for append_error()/geterror() be thread-specific.
static thread_local ErrorHolder error_msg_holder;


void
Strutil::pvt::append_error(string_view message)
{
    // Remove a single trailing newline
    if (message.size() && message.back() == '\n')
        message.remove_suffix(1);
    std::string& error_msg(error_msg_holder.error_msg);
    OIIO_ASSERT(
        error_msg.size() < 1024 * 1024 * 16
        && "Accumulated error messages > 16MB. Try checking return codes!");
    // If we are appending to existing error messages, separate them with
    // a single newline.
    if (error_msg.size() && error_msg.back() != '\n')
        error_msg += '\n';
    error_msg += std::string(message);

    // Remove a single trailing newline
    if (message.size() && message.back() == '\n')
        message.remove_suffix(1);
    error_msg = std::string(message);
}


bool
Strutil::pvt::has_error()
{
    std::string& error_msg(error_msg_holder.error_msg);
    return !error_msg.empty();
}


std::string
Strutil::pvt::geterror(bool clear)
{
    std::string& error_msg(error_msg_holder.error_msg);
    std::string e = error_msg;
    if (clear)
        error_msg.clear();
    return e;
}


void
pvt::log_fmt_error(const char* message)
{
    print("fmt exception: {}\n", message);
    Strutil::pvt::append_error(std::string("fmt exception: ") + message);
}



void
Strutil::pvt::debug(string_view message)
{
    if (OIIO::pvt::oiio_print_debug) {
        static mutex debug_mutex;
        lock_guard lock(debug_mutex);
        static FILE* oiio_debug_file = nullptr;
        if (!oiio_debug_file) {
            const char* filename = getenv("OPENIMAGEIO_DEBUG_FILE");
            oiio_debug_file = filename && filename[0] ? fopen(filename, "a")
                                                      : stderr;
            OIIO_ASSERT(oiio_debug_file);
            if (!oiio_debug_file)
                oiio_debug_file = stderr;
        }
        print(oiio_debug_file, "OIIO DEBUG: {}", message);
        fflush(oiio_debug_file);
    }
}



std::string
Strutil::vsprintf(const char* fmt, va_list ap)
{
    // Allocate a buffer on the stack that's big enough for us almost
    // all the time.  Be prepared to allocate dynamically if it doesn't fit.
    size_t size = 1024;
    char stackbuf[1024];
    std::vector<char> dynamicbuf;
    char* buf = &stackbuf[0];

    while (1) {
        // Try to vsnprintf into our buffer.
        va_list apsave;
#ifdef va_copy
        va_copy(apsave, ap);
#else
        apsave = ap;
#endif

        int needed = oiio_stbsp_vsnprintf(buf, size, fmt, ap);
        va_end(ap);

        // NB. C99 says vsnprintf returns -1 on an encoding error. Otherwise
        // it's the number of characters that would have been written if the
        // buffer were large enough.
        if (needed == -1)
            return std::string("ENCODING ERROR");
        if (needed < (int)size) {
            // It fit fine so we're done.
            return std::string(buf, (size_t)needed);
        }

        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So try again using a dynamic buffer.  This
        // doesn't happen very often if we chose our initial size well.
        size = (needed > 0) ? (needed + 1) : (size * 2);
        dynamicbuf.resize(size);
        buf = &dynamicbuf[0];
#ifdef va_copy
        va_copy(ap, apsave);
#else
        ap = apsave;
#endif
    }
}



std::string
Strutil::memformat(long long bytes, int digits)
{
    const long long KB = (1 << 10);
    const long long MB = (1 << 20);
    const long long GB = (1 << 30);
    const char* units  = "B";
    double d           = (double)bytes;
    if (bytes >= GB) {
        units = "GB";
        d     = (double)bytes / GB;
    } else if (bytes >= MB) {
        units = "MB";
        d     = (double)bytes / MB;
    } else if (bytes >= KB) {
        // Just KB, don't bother with decimalization
        return fmt::format("{} KB", bytes / KB);
    } else {
        // Just bytes, don't bother with decimalization
        return fmt::format("{} B", bytes);
    }
    return Strutil::sprintf("%1.*f %s", digits, d, units);
}



std::string
Strutil::timeintervalformat(double secs, int digits)
{
    const double mins  = 60;
    const double hours = mins * 60;
    const double days  = hours * 24;

    std::string out;
    int d = (int)floor(secs / days);
    secs  = fmod(secs, days);
    int h = (int)floor(secs / hours);
    secs  = fmod(secs, hours);
    int m = (int)floor(secs / mins);
    secs  = fmod(secs, mins);
    if (d)
        out += fmt::format("{}d {}h ", d, h);
    else if (h)
        out += fmt::format("{}h ", h);
    if (m || h || d)
        out += Strutil::sprintf("%dm %1.*fs", m, digits, secs);
    else
        out += Strutil::sprintf("%1.*fs", digits, secs);
    return out;
}



bool
Strutil::get_rest_arguments(const std::string& str, std::string& base,
                            std::map<std::string, std::string>& result)
{
    // Look for `?` as the start of REST arguments, but not if it's part
    // of a `\\?\` special windows filename notation.
    // See: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    size_t start  = starts_with(str, "\\\\?\\") ? 4 : 0;
    auto mark_pos = str.find_first_of("?", start);
    if (mark_pos == std::string::npos) {
        base = str;
        return true;
    }

    base             = str.substr(0, mark_pos);
    std::string rest = str.substr(mark_pos + 1);
    auto rest_tokens = Strutil::splitsv(rest, "&");
    for (auto keyval : rest_tokens) {
        mark_pos = keyval.find_first_of("=");
        if (mark_pos == std::string::npos)
            return false;
        result[keyval.substr(0, mark_pos)] = keyval.substr(mark_pos + 1);
    }

    return true;
}



std::string
Strutil::escape_chars(string_view unescaped)
{
    std::string s = unescaped;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (c == '\n' || c == '\t' || c == '\v' || c == '\b' || c == '\r'
            || c == '\f' || c == '\a' || c == '\\' || c == '\"') {
            s[i] = '\\';
            ++i;
            switch (c) {
            case '\n': c = 'n'; break;
            case '\t': c = 't'; break;
            case '\v': c = 'v'; break;
            case '\b': c = 'b'; break;
            case '\r': c = 'r'; break;
            case '\f': c = 'f'; break;
            case '\a': c = 'a'; break;
            default: break;
            }
            s.insert(i, 1, c);
        }
    }
    return s;
}



std::string
Strutil::unescape_chars(string_view escaped)
{
    std::string s = escaped;
    for (size_t i = 0, len = s.length(); i < len; ++i) {
        if (s[i] == '\\') {
            char c = s[i + 1];
            if (c == 'n' || c == 't' || c == 'v' || c == 'b' || c == 'r'
                || c == 'f' || c == 'a' || c == '\\' || c == '\"') {
                s.erase(i, 1);
                --len;
                switch (c) {
                case 'n': s[i] = '\n'; break;
                case 't': s[i] = '\t'; break;
                case 'v': s[i] = '\v'; break;
                case 'b': s[i] = '\b'; break;
                case 'r': s[i] = '\r'; break;
                case 'f': s[i] = '\f'; break;
                case 'a': s[i] = '\a'; break;
                default: break;  // the deletion is enough (backslash and quote)
                }
            } else if (c >= '0' && c < '8') {
                // up to 3 octal digits
                int octalChar = 0;
                for (int j = 0; j < 3 && c >= '0' && c <= '7'; ++j) {
                    octalChar = 8 * octalChar + (c - '0');
                    s.erase(i, 1);
                    --len;
                    c = i + 1 < len ? s[i + 1] : '\0';
                }
                s[i] = (char)octalChar;
            }
        }
    }
    return s;
}



std::string
Strutil::wordwrap(string_view src, int columns, int prefix, string_view sep,
                  string_view presep)
{
    if (columns < prefix + 20)
        return src;  // give up, no way to make it wrap
    std::ostringstream out;
    columns -= prefix;  // now columns is the real width we have to work with
    std::string allsep = Strutil::concat(sep, presep);
    while ((int)src.length() > columns) {
        // Find the last `sep` break before the column limit.
        size_t breakpoint = src.find_last_of(allsep, columns);
        // If presep is not empty, find the last presep break before the
        // column limit and break one character AFTER that, if it's a better
        // breakpost than the sep break.
        if (presep.size()) {
            size_t presep_break = src.find_last_of(presep, columns);
            if (presep_break >= breakpoint && presep_break < src.size())
                breakpoint = presep_break + 1;
        }
        // If no break was found, punt and hard break at the column limit.
        if (breakpoint == string_view::npos)
            breakpoint = columns;
        // Copy up to the breakpoint, then newline and prefex blanks.
        out << src.substr(0, breakpoint) << "\n" << std::string(prefix, ' ');
        // Pick up where we left off
        src = src.substr(breakpoint);
        // Skip any separator characters at the start of the string
        while (sep.find(src[0]) != string_view::npos)
            src.remove_prefix(1);
    }
    out << src;
    return out.str();
}



namespace Strutil {

// Define a locale-independent, case-insensitive comparison for all platforms.

inline int
strcasecmp(const char* a, const char* b)
{
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)           \
    || defined(__GLIBC__)
    return strcasecmp_l(a, b, c_loc);
#elif defined(_WIN32)
    return _stricmp_l(a, b, c_loc);
#else
#    error("need equivalent of strcasecmp_l on this platform");
#endif
}


inline int
strncasecmp(const char* a, const char* b, size_t size)
{
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)           \
    || defined(__GLIBC__)
    return strncasecmp_l(a, b, size, c_loc);
#elif defined(_WIN32)
    return _strnicmp_l(a, b, size, c_loc);
#else
#    error("need equivalent of strncasecmp_l on this platform");
#endif
}

}  // namespace Strutil



bool
Strutil::iequals(string_view a, string_view b)
{
    int asize = a.size();
    int bsize = b.size();
    if (asize != bsize)
        return false;
    return Strutil::strncasecmp(a.data(), b.data(), bsize) == 0;
}


bool
Strutil::iless(string_view a, string_view b)
{
    size_t asize = a.size();
    size_t bsize = b.size();
    size_t size  = std::min(asize, bsize);
    int c        = Strutil::strncasecmp(a.data(), b.data(), size);
    // If c != 0, we could tell if a<b from the initial characters. But if c==0,
    // the strings were equal up to the position of the lesser length, so
    // whichever is smaller is "less" than the other.
    return (c != 0) ? (c < 0) : (asize < bsize);
}


bool
Strutil::starts_with(string_view a, string_view b)
{
    size_t asize = a.size();
    size_t bsize = b.size();
    if (asize < bsize)
        return false;
    return strncmp(a.data(), b.data(), bsize) == 0;
}


bool
Strutil::istarts_with(string_view a, string_view b)
{
    size_t asize = a.size();
    size_t bsize = b.size();
    if (asize < bsize)  // a can't start with b if a is smaller
        return false;
    return Strutil::strncasecmp(a.data(), b.data(), bsize) == 0;
}


bool
Strutil::ends_with(string_view a, string_view b)
{
    size_t asize = a.size();
    size_t bsize = b.size();
    if (asize < bsize)  // a can't start with b if a is smaller
        return false;
    return strncmp(a.data() + asize - bsize, b.data(), bsize) == 0;
}


bool
Strutil::iends_with(string_view a, string_view b)
{
    size_t asize = a.size();
    size_t bsize = b.size();
    if (asize < bsize)  // a can't start with b if a is smaller
        return false;
    return strncasecmp(a.data() + asize - bsize, b.data(), bsize) == 0;
}


bool
Strutil::contains(string_view a, string_view b)
{
    return find(a, b) != string_view::npos;
}


bool
Strutil::icontains(string_view a, string_view b)
{
    return ifind(a, b) != string_view::npos;
}



bool
Strutil::contains_any_char(string_view a, string_view set)
{
    if (a.size() == 0)
        return false;
    // Leverage Strutil::parse_until, which trims a string until it hits
    // any characters in a set.
    string_view r = parse_until(a, set, false /* don't alter a */);
    // If r encompasses less than all of a, it must have found one of the
    // characters in set.
    return (r.size() < a.size());
}



size_t
Strutil::find(string_view a, string_view b)
{
    return a.find(b);
}


size_t
Strutil::rfind(string_view a, string_view b)
{
    return a.rfind(b);
}


size_t
Strutil::ifind(string_view a, string_view b)
{
    if (a.empty())
        return string_view::npos;
    if (b.empty())
        return 0;

    if (b.size() <= a.size()) {
        const char* start = a.data();
        const char* last  = a.data() + a.size() - b.size();
        while (start <= last) {
            if (Strutil::strncasecmp(start, b.data(), b.size()) == 0) {
                return size_t(start - a.data());
            }
            start++;
        }
    }

    return string_view::npos;
}


size_t
Strutil::irfind(string_view a, string_view b)
{
    if (a.empty())
        return string_view::npos;
    if (b.empty())
        return a.size();

    if (b.size() <= a.size()) {
        const char* start = a.data() + (a.size() - b.size());
        while (start >= a.data()) {
            if (Strutil::strncasecmp(start, b.data(), b.size()) == 0) {
                return size_t(start - a.data());
            }
            start--;
        }
    }

    return string_view::npos;
}


void
Strutil::to_lower(std::string& a)
{
    const std::locale& loc = std::locale::classic();
    std::transform(a.cbegin(), a.cend(), a.begin(),
                   [&loc](char c) { return std::tolower(c, loc); });
}


void
Strutil::to_upper(std::string& a)
{
    const std::locale& loc = std::locale::classic();
    std::transform(a.cbegin(), a.cend(), a.begin(),
                   [&loc](char c) { return std::toupper(c, loc); });
}



bool
Strutil::StringIEqual::operator()(const char* a, const char* b) const noexcept
{
    return Strutil::strcasecmp(a, b) == 0;
}


bool
Strutil::StringILess::operator()(const char* a, const char* b) const noexcept
{
    return Strutil::strcasecmp(a, b) < 0;
}



string_view
Strutil::lstrip(string_view str, string_view chars)
{
    if (chars.empty())
        chars = string_view(" \t\n\r\f\v", 6);
    size_t b = str.find_first_not_of(chars);
    return str.substr(b, string_view::npos);
}



string_view
Strutil::rstrip(string_view str, string_view chars)
{
    if (chars.empty())
        chars = string_view(" \t\n\r\f\v", 6);
    size_t e = str.find_last_not_of(chars);
    return e != string_view::npos ? str.substr(0, e + 1) : string_view();
}



string_view
Strutil::strip(string_view str, string_view chars)
{
    if (chars.empty())
        chars = string_view(" \t\n\r\f\v", 6);
    size_t b = str.find_first_not_of(chars);
    if (b == std::string::npos)
        return string_view();
    size_t e = str.find_last_not_of(chars);
    OIIO_DASSERT(e != std::string::npos);
    return str.substr(b, e - b + 1);
}



static void
split_whitespace(string_view str, std::vector<string_view>& result,
                 int maxsplit)
{
    // Implementation inspired by Pystring
    string_view::size_type i, j, len = str.size();
    for (i = j = 0; i < len;) {
        while (i < len && Strutil::isspace(str[i]))
            i++;
        j = i;
        while (i < len && !Strutil::isspace(str[i]))
            i++;
        if (j < i) {
            if (--maxsplit <= 0)
                break;
            result.push_back(str.substr(j, i - j));
            while (i < len && Strutil::isspace(str[i]))
                i++;
            j = i;
        }
    }
    if (j < len)
        result.push_back(str.substr(j, len - j));
}



std::vector<std::string>
Strutil::splits(string_view str, string_view sep, int maxsplit)
{
    auto sr_result = splitsv(str, sep, maxsplit);
    std::vector<std::string> result;
    result.reserve(sr_result.size());
    for (auto& s : sr_result)
        result.push_back(s);
    return result;
}



void
Strutil::split(string_view str, std::vector<std::string>& result,
               string_view sep, int maxsplit)
{
    result = splits(str, sep, maxsplit);
}



std::vector<string_view>
Strutil::splitsv(string_view str, string_view sep, int maxsplit)
{
    std::vector<string_view> result;
    if (str.size() == 0)
        return result;  // No source string --> no pieces

    // Implementation inspired by Pystring
    if (maxsplit < 0)
        maxsplit = std::numeric_limits<int>::max();
    if (sep.size() == 0) {
        split_whitespace(str, result, maxsplit);
        return result;
    }
    size_t i = 0, j = 0, len = str.size(), n = sep.size();
    while (i + n <= len) {
        if (str[i] == sep[0] && str.substr(i, n) == sep) {
            if (--maxsplit <= 0)
                break;
            result.push_back(str.substr(j, i - j));
            i = j = i + n;
        } else {
            i++;
        }
    }
    result.push_back(str.substr(j, len - j));
    return result;
}



void
Strutil::split(string_view str, std::vector<string_view>& result,
               string_view sep, int maxsplit)
{
    result = splitsv(str, sep, maxsplit);
}



std::string
Strutil::concat(string_view s, string_view t)
{
    size_t sl = s.size();
    size_t tl = t.size();
    if (sl == 0)
        return t;
    if (tl == 0)
        return s;
    size_t len = sl + tl;
    char* buf;
    OIIO_ALLOCATE_STACK_OR_HEAP(buf, char, len);
    memcpy(buf, s.data(), sl);
    memcpy(buf + sl, t.data(), tl);
    return std::string(buf, len);
}



std::string
Strutil::repeat(string_view str, int n)
{
    size_t sl  = str.size();
    size_t len = sl * std::max(0, n);
    std::unique_ptr<char[]> heap_buf;
    char local_buf[256] = "";
    char* buf           = local_buf;
    if (len > sizeof(local_buf)) {
        heap_buf.reset(new char[len]);
        buf = heap_buf.get();
    }
    for (int i = 0; i < n; ++i)
        memcpy(buf + i * sl, str.data(), sl);
    return std::string(buf, len);
}



std::string
Strutil::replace(string_view str, string_view pattern, string_view replacement,
                 bool global)
{
    std::string r;
    while (1) {
        size_t f = str.find(pattern);
        if (f != str.npos) {
            // Pattern found -- copy the part of str prior to the pattern,
            // then copy the replacement, and skip str up to the part after
            // the pattern and continue for another go at it.
            r.append(str.data(), f);
            r.append(replacement.data(), replacement.size());
            str.remove_prefix(f + pattern.size());
            if (global)
                continue;  // Try for another match
        }
        // Pattern not found -- copy remaining string and we're done
        r.append(str.data(), str.size());
        break;
    }
    return r;
}



// Conversion functions between UTF-8 and UTF-16 for windows.
//
// For historical reasons, the standard encoding for strings on windows is
// UTF-16, whereas the unix world seems to have settled on UTF-8.  These two
// encodings can be stored in std::string and std::wstring respectively, with
// the caveat that they're both variable-width encodings, so not all the
// standard string methods will make sense (for example std::string::size()
// won't return the number of glyphs in a UTF-8 string, unless it happens to
// be made up of only the 7-bit ASCII subset).
//
// The standard windows API functions usually have two versions, a UTF-16
// version with a 'W' suffix (using wchar_t* strings), and an ANSI version
// with a 'A' suffix (using char* strings) which uses the current windows
// code page to define the encoding.  (To make matters more confusing there is
// also a further "TCHAR" version which is #defined to the UTF-16 or ANSI
// version, depending on whether UNICODE is defined during compilation.
// This is meant to make it possible to support compiling libraries in
// either unicode or ansi mode from the same codebase.)
//
// Using std::string as the string container (as in OIIO) implies that we
// can't use UTF-16.  It also means we need a variable-width encoding to
// represent characters in non-Latin alphabets in an unambiguous way; the
// obvious candidate is UTF-8.  File paths in OIIO are considered to be
// represented in UTF-8, and must be converted to UTF-16 before passing to
// windows API file opening functions.
//
// On the other hand, the encoding used for the ANSI versions of the windows
// API is the current windows code page.  This is more compatible with the
// default setup of the standard windows command prompt, and may be more
// appropriate for error messages.

std::wstring
Strutil::utf8_to_utf16wstring(string_view str) noexcept
{
#ifdef _WIN32
    // UTF8<->UTF16 conversions are primarily needed on Windows, so use the
    // fastest option (C++11 <codecvt> is many times slower due to locale
    // access overhead, and is deprecated starting with C++17).
    std::wstring result;
    result.resize(
        MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), NULL, 0));
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), result.data(),
                        (int)result.size());
    return result;
#else
    try {
        OIIO_PRAGMA_WARNING_PUSH
        OIIO_CLANG_PRAGMA(GCC diagnostic ignored "-Wdeprecated-declarations")
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> conv;
        OIIO_PRAGMA_WARNING_POP
        return conv.from_bytes(str.data(), str.data() + str.size());
    } catch (const std::exception&) {
        return std::wstring();
    }
#endif
}



std::string
Strutil::utf16_to_utf8(const std::wstring& str) noexcept
{
#ifdef _WIN32
    // UTF8<->UTF16 conversions are primarily needed on Windows, so use the
    // fastest option (C++11 <codecvt> is many times slower due to locale
    // access overhead, and is deprecated starting with C++17).
    std::string result;
    result.resize(WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(),
                                      NULL, 0, NULL, NULL));
    WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), &result[0],
                        (int)result.size(), NULL, NULL);
    return result;
#else
    try {
        OIIO_PRAGMA_WARNING_PUSH
        OIIO_CLANG_PRAGMA(GCC diagnostic ignored "-Wdeprecated-declarations")
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> conv;
        OIIO_PRAGMA_WARNING_POP
        return conv.to_bytes(str);
    } catch (const std::exception&) {
        return std::string();
    }
#endif
}



std::string
Strutil::utf16_to_utf8(const std::u16string& str) noexcept
{
#ifdef _WIN32
    std::string result;
    result.resize(WideCharToMultiByte(CP_UTF8, 0, (const WCHAR*)str.data(),
                                      str.length(), NULL, 0, NULL, NULL));
    WideCharToMultiByte(CP_UTF8, 0, (const WCHAR*)str.data(), str.length(),
                        &result[0], (int)result.size(), NULL, NULL);
    return result;
#else
    try {
        OIIO_PRAGMA_WARNING_PUSH
        OIIO_CLANG_PRAGMA(GCC diagnostic ignored "-Wdeprecated-declarations")
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
        return conv.to_bytes(str);
        OIIO_PRAGMA_WARNING_POP
    } catch (const std::exception&) {
        return std::string();
    }
#endif
}



char*
Strutil::safe_strcpy(char* dst, string_view src, size_t size) noexcept
{
    OIIO_DASSERT(dst);
    if (src.size()) {
        size_t end = std::min(size - 1, src.size());
        for (size_t i = 0; i < end; ++i)
            dst[i] = src[i];
        for (size_t i = end; i < size; ++i)
            dst[i] = 0;
    } else {
        for (size_t i = 0; i < size; ++i)
            dst[i] = 0;
    }
    return dst;
}



char*
Strutil::safe_strcat(char* dst, string_view src, size_t size) noexcept
{
    OIIO_DASSERT(dst);
    if (src.size()) {
        size_t dstsize = strnlen(dst, size);
        size_t end     = std::min(size - dstsize - 1, src.size());
        for (size_t i = 0; i < end; ++i)
            dst[dstsize + i] = src[i];
        dst[dstsize + end] = 0;
    }
    return dst;
}



size_t
Strutil::safe_strlen(const char* str, size_t size) noexcept
{
    return str ? strnlen(str, size) : 0;
}



void
Strutil::skip_whitespace(string_view& str) noexcept
{
    while (str.size() && Strutil::isspace(str.front()))
        str.remove_prefix(1);
}



void
Strutil::remove_trailing_whitespace(string_view& str) noexcept
{
    while (str.size() && Strutil::isspace(str.back()))
        str.remove_suffix(1);
}



bool
Strutil::parse_char(string_view& str, char c, bool skip_whitespace,
                    bool eat) noexcept
{
    string_view p = str;
    if (skip_whitespace)
        Strutil::skip_whitespace(p);
    if (p.size() && p[0] == c) {
        if (eat) {
            p.remove_prefix(1);
            str = p;
        }
        return true;
    }
    return false;
}



bool
Strutil::parse_until_char(string_view& str, char c, bool eat) noexcept
{
    string_view p = str;
    while (p.size() && p[0] != c)
        p.remove_prefix(1);
    if (eat)
        str = p;
    return p.size() && p[0] == c;
}



bool
Strutil::parse_prefix(string_view& str, string_view prefix, bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    if (Strutil::starts_with(p, prefix)) {
        p.remove_prefix(prefix.size());
        if (eat)
            str = p;
        return true;
    }
    return false;
}



bool
Strutil::parse_int(string_view& str, int& val, bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    if (!p.size())
        return false;
    size_t endpos = 0;
    int v         = Strutil::stoi(p, &endpos);
    if (endpos == 0)
        return false;  // no integer found
    if (eat)
        str = p.substr(endpos);
    val = v;
    return true;
}



bool
Strutil::parse_float(string_view& str, float& val, bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    if (!p.size())
        return false;
    size_t endpos = 0;
    float v       = Strutil::stof(p, &endpos);
    if (endpos == 0)
        return false;  // no integer found
    if (eat)
        str = p.substr(endpos);
    val = v;
    return true;
}



bool
Strutil::parse_string(string_view& str, string_view& val, bool eat,
                      QuoteBehavior keep_quotes) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    if (str.empty())
        return false;
    char lead_char    = p.front();
    bool quoted       = parse_char(p, '\"') || parse_char(p, '\'');
    const char *begin = p.begin(), *end = p.begin();
    bool escaped = false;  // was the prior character a backslash
    while (end != p.end()) {
        if (Strutil::isspace(*end) && !quoted)
            break;  // not quoted and we hit whitespace: we're done
        if (quoted && *end == lead_char && !escaped)
            break;  // closing quote -- we're done (beware embedded quote)
        escaped = (end[0] == '\\') && (!escaped);
        ++end;
    }
    if (quoted && keep_quotes == KeepQuotes) {
        if (*end == lead_char)
            val = string_view(begin - 1, size_t(end - begin) + 2);
        else
            val = string_view(begin - 1, size_t(end - begin) + 1);
    } else {
        val = string_view(begin, size_t(end - begin));
    }
    p.remove_prefix(size_t(end - begin));
    if (quoted && p.size() && p[0] == lead_char)
        p.remove_prefix(1);  // eat closing quote
    if (eat)
        str = p;
    return quoted || val.size();
}



bool
Strutil::parse_values(string_view& str, string_view prefix, span<int> values,
                      string_view sep, string_view postfix, bool eat) noexcept
{
    string_view p = str;
    bool ok       = true;
    if (prefix.size())
        ok &= Strutil::parse_prefix(p, prefix);
    for (size_t i = 0, sz = values.size(); i < sz && ok; ++i) {
        ok &= Strutil::parse_value(p, values[i]);
        if (ok && sep.size() && i < sz - 1)
            ok &= Strutil::parse_prefix(p, sep);
    }
    if (ok && postfix.size())
        ok &= Strutil::parse_prefix(p, postfix);
    if (ok && eat)
        str = p;
    return ok;
}

bool
Strutil::parse_values(string_view& str, string_view prefix, span<float> values,
                      string_view sep, string_view postfix, bool eat) noexcept
{
    string_view p = str;
    bool ok       = true;
    if (prefix.size())
        ok &= Strutil::parse_prefix(p, prefix);
    for (size_t i = 0, sz = values.size(); i < sz && ok; ++i) {
        ok &= Strutil::parse_value(p, values[i]);
        if (ok && sep.size() && i < sz - 1)
            ok &= Strutil::parse_prefix(p, sep);
    }
    if (ok && postfix.size())
        ok &= Strutil::parse_prefix(p, postfix);
    if (ok && eat)
        str = p;
    return ok;
}



string_view
Strutil::parse_word(string_view& str, bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    const char *begin = p.begin(), *end = p.begin();
    while (end != p.end() && isalpha(*end))
        ++end;
    size_t wordlen = end - begin;
    if (eat && wordlen) {
        p.remove_prefix(wordlen);
        str = p;
    }
    return string_view(begin, wordlen);
}



string_view
Strutil::parse_identifier(string_view& str, bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    const char *begin = p.begin(), *end = p.begin();
    if (end != p.end() && (isalpha(*end) || *end == '_'))
        ++end;
    else
        return string_view();  // not even the start of an identifier
    while (end != p.end() && (isalpha(*end) || isdigit(*end) || *end == '_'))
        ++end;
    if (eat) {
        p.remove_prefix(size_t(end - begin));
        str = p;
    }
    return string_view(begin, size_t(end - begin));
}



string_view
Strutil::parse_identifier(string_view& str, string_view allowed,
                          bool eat) noexcept
{
    string_view p = str;
    skip_whitespace(p);
    const char *begin = p.begin(), *end = p.begin();
    if (end != p.end()
        && (isalpha(*end) || *end == '_'
            || allowed.find(*end) != string_view::npos))
        ++end;
    else
        return string_view();  // not even the start of an identifier
    while (end != p.end()
           && (isalpha(*end) || isdigit(*end) || *end == '_'
               || allowed.find(*end) != string_view::npos))
        ++end;
    if (eat) {
        p.remove_prefix(size_t(end - begin));
        str = p;
    }
    return string_view(begin, size_t(end - begin));
}



bool
Strutil::parse_identifier_if(string_view& str, string_view id,
                             bool eat) noexcept
{
    string_view head = parse_identifier(str, false /* don't eat */);
    if (head == id) {
        if (eat)
            parse_identifier(str);
        return true;
    }
    return false;
}



string_view
Strutil::parse_until(string_view& str, string_view sep, bool eat) noexcept
{
    string_view p     = str;
    const char *begin = p.begin(), *end = p.begin();
    while (end != p.end() && sep.find(*end) == string_view::npos)
        ++end;
    size_t wordlen = end - begin;
    if (eat && wordlen) {
        p.remove_prefix(wordlen);
        str = p;
    }
    return string_view(begin, wordlen);
}



string_view
Strutil::parse_while(string_view& str, string_view set, bool eat) noexcept
{
    string_view p     = str;
    const char *begin = p.begin(), *end = p.begin();
    while (end != p.end() && set.find(*end) != string_view::npos)
        ++end;
    size_t wordlen = end - begin;
    if (eat && wordlen) {
        p.remove_prefix(wordlen);
        str = p;
    }
    return string_view(begin, wordlen);
}



string_view
Strutil::parse_line(string_view& str, bool eat) noexcept
{
    string_view result;
    auto newlinepos = str.find('\n');
    if (newlinepos >= str.size() - 1) {
        // No newline, or newline is the last char -> return the whole thing
        result = str;
        if (eat)
            str = string_view();
    } else {
        result = str.substr(0, newlinepos + 1);
        if (eat)
            str = str.substr(newlinepos + 1);
    }
    return result;
}



string_view
Strutil::parse_nested(string_view& str, bool eat) noexcept
{
    // Make sure we have a valid string and ascertain the characters that
    // nest and unnest.
    string_view p = str;
    if (!p.size())
        return string_view();  // No proper opening
    char opening = p[0];
    char closing = 0;
    if (opening == '(')
        closing = ')';
    else if (opening == '[')
        closing = ']';
    else if (opening == '{')
        closing = '}';
    else
        return string_view();

    // Walk forward in the string until we exactly unnest compared to the
    // start.
    size_t len  = 1;
    int nesting = 1;
    for (; nesting && len < p.size(); ++len) {
        if (p[len] == opening)
            ++nesting;
        else if (p[len] == closing)
            --nesting;
    }

    if (nesting)
        return string_view();  // No proper closing

    OIIO_ASSERT(p[len - 1] == closing);

    // The result is the first len characters
    string_view result = str.substr(0, len);
    if (eat)
        str.remove_prefix(len);
    return result;
}



std::string
Strutil::excise_string_after_head(std::string& str, string_view head)
{
    std::string result;
    string_view s(str);
    size_t pattern_start = s.find(head);
    if (pattern_start != string_view::npos) {
        // Reposition s to be after the head
        s.remove_prefix(pattern_start + head.size());
        string_view m = Strutil::parse_until(s, " \t\r\n");
        Strutil::skip_whitespace(s);
        result = m;
        str    = str.substr(0, pattern_start) + std::string(s);
    }
    return result;
}



/*
Copyright for decode function:
See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

MIT license

Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] = {
    // clang-format off
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12,
    // clang-format on
};

inline uint32_t
decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
    uint32_t type = utf8d[byte];
    *codep        = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6)
                                            : (0xff >> type) & (byte);
    *state        = utf8d[256 + *state + type];
    return *state;
}

void
Strutil::utf8_to_unicode(string_view str, std::vector<uint32_t>& uvec)
{
    const char* begin  = str.begin();
    const char* end    = str.end();
    uint32_t state     = 0;
    uint32_t codepoint = 0;
    for (; begin != end; ++begin) {
        if (!decode(&state, &codepoint, (unsigned char)*begin))
            uvec.push_back(codepoint);
    }
}



/* base64 code is based upon: http://www.adp-gmbh.ch/cpp/common/base64.html
   https://github.com/ReneNyffenegger/cpp-base64

   Zlib license

   Copyright (C) 2004-2008 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.
   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/
std::string
Strutil::base64_encode(string_view str)
{
    static const char* base64_chars
        = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string ret;
    ret.reserve((str.size() * 4 + 2) / 3);
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    while (str.size()) {
        char_array_3[i++] = str.front();
        str.remove_prefix(1);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4)
                              + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2)
                              + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (int j = 0; j < 4; j++)
                ret += base64_chars[char_array_4[j]];
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4)
                          + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2)
                          + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        while (i++ < 3)
            ret += '=';
    }
    return ret;
}



// Helper: Eat the given number of chars from str, then return the next
// char, or 0 if no more chars are in str.
inline unsigned char
cnext(string_view& str, int eat = 1)
{
    str.remove_prefix(eat);
    return OIIO_LIKELY(str.size()) ? str.front() : 0;
}



int
Strutil::stoi(string_view str, size_t* pos, int base)
{
    // We roll our own stoi so that we can directly use it with a
    // string_view and also hardcode it without any locale dependence. The
    // system stoi/atoi/strtol/etc. needs to be null-terminated.
    string_view str_orig = str;
    Strutil::skip_whitespace(str);

    // c is always the next char to parse, or 0 if there's no more
    unsigned char c = cnext(str, 0);  // don't eat the char

    // Handle leading - or +
    bool neg = (c == '-');
    if (c == '-' || c == '+')
        c = cnext(str);

    // Allow "0x" to start hex number if base is 16 or 0 (any)
    if ((base == 0 || base == 16) && c == '0' && str.size() >= 1
        && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
        c    = cnext(str, 2);
    }
    // For "any" base, collapse to base 10 unless leading 0 means it's octal
    if (base == 0)
        base = c == '0' ? 8 : 10;

    // Accumulate into a 64 bit int to make overflow handling simple.
    // If we ever need a str-to-int64 conversion, we'll need to be more
    // careful with figuring out overflow. But the 32 bit case is easy.
    int64_t acc    = 0;
    bool overflow  = false;  // Set if we overflow
    bool anydigits = false;  // Have we parsed any digits at all?
    int64_t maxval = neg ? -int64_t(std::numeric_limits<int>::min())
                         : std::numeric_limits<int>::max();
    for (; OIIO_LIKELY(c); c = cnext(str)) {
        if (OIIO_LIKELY(isdigit(c)))
            c -= '0';
        else if (isalpha(c))
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else {
            break;  // done
        }
        if (c >= base)
            break;
        anydigits = true;
        if (OIIO_LIKELY(!overflow))
            acc = acc * base + c;
        if (OIIO_UNLIKELY(acc > maxval))
            overflow = true;
    }
    if (OIIO_UNLIKELY(!anydigits)) {
        str = str_orig;
    } else if (OIIO_UNLIKELY(overflow)) {
        acc = neg ? std::numeric_limits<int>::min()
                  : std::numeric_limits<int>::max();
    } else {
        if (neg)
            acc = -acc;
    }
    if (pos)
        *pos = size_t(str.data() - str_orig.data());
    return static_cast<int>(acc);
}



float
Strutil::strtof(const char* nptr, char** endptr) noexcept
{
    // Can use strtod_l on platforms that support it
#ifdef __APPLE__
    // On OSX, strtod_l is for some reason drastically faster than strtof_l.
    return static_cast<float>(strtod_l(nptr, endptr, c_loc));
#elif defined(__linux__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__GLIBC__)
    return strtof_l(nptr, endptr, c_loc);
#elif defined(_WIN32)
    // Windows has _strtod_l
    return static_cast<float>(_strtod_l(nptr, endptr, c_loc));
#else
    // On platforms without strtof_l...
    std::locale native;  // default ctr gets current global locale
    char nativepoint
        = std::use_facet<std::numpunct<char>>(native).decimal_point();
    // If the native locale uses decimal, just directly use strtof.
    if (nativepoint == '.')
        return ::strtof(nptr, endptr);
    // Complex case -- CHEAT by making a copy of the string and replacing
    // the decimal, then use system strtof!
    std::string s(nptr);
    const char* pos = strchr(nptr, nativepoint);
    if (pos) {
        s[pos - nptr] = nativepoint;
        auto d        = strtof(s.c_str(), endptr);
        if (endptr)
            *endptr = (char*)nptr + (*endptr - s.c_str());
        return d;
    }
    // No decimal point at all -- use regular strtof
    return ::strtof(s.c_str(), endptr);
#endif
}


double
Strutil::strtod(const char* nptr, char** endptr) noexcept
{
    // Can use strtod_l on platforms that support it
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__GLIBC__)
    return strtod_l(nptr, endptr, c_loc);
#elif defined(_WIN32)
    // Windows has _strtod_l
    return _strtod_l(nptr, endptr, c_loc);
#else
    // On platforms without strtod_l...
    std::locale native;  // default ctr gets current global locale
    char nativepoint
        = std::use_facet<std::numpunct<char>>(native).decimal_point();
    // If the native locale uses decimal, just directly use strtod.
    if (nativepoint == '.')
        return ::strtod(nptr, endptr);
    // Complex case -- CHEAT by making a copy of the string and replacing
    // the decimal, then use system strtod!
    std::string s(nptr);
    const char* pos = strchr(nptr, nativepoint);
    if (pos) {
        s[pos - nptr] = nativepoint;
        auto d        = ::strtod(s.c_str(), endptr);
        if (endptr)
            *endptr = (char*)nptr + (*endptr - s.c_str());
        return d;
    }
    // No decimal point at all -- use regular strtod
    return ::strtod(s.c_str(), endptr);
#endif
}

// Notes:
//
// FreeBSD's implementation of strtod:
//   https://svnweb.freebsd.org/base/stable/10/contrib/gdtoa/strtod.c?view=markup
// Python's implementation  of strtod: (BSD license)
//   https://hg.python.org/cpython/file/default/Python/pystrtod.c
// Julia's implementation (combo of strtod_l and Python's impl):
//   https://github.com/JuliaLang/julia/blob/master/src/support/strtod.c
// MSDN documentation on Windows _strtod_l and friends:
//   https://msdn.microsoft.com/en-us/library/kxsfc1ab.aspx   (_strtod_l)
//   https://msdn.microsoft.com/en-us/library/4zx9aht2.aspx   (_create_locale)
// cppreference on locale:
//   http://en.cppreference.com/w/cpp/locale/locale



float
Strutil::stof(const char* s, size_t* pos)
{
    if (s) {
        char* endptr;
        float r = Strutil::strtof(s, &endptr);
        if (endptr != s) {
            if (pos)
                *pos = size_t(endptr - s);
            return r;
        }
    }
    // invalid
    if (pos)
        *pos = 0;
    return 0;
}


float
Strutil::stof(const std::string& s, size_t* pos)
{
    return Strutil::stof(s.c_str(), pos);
}


float
Strutil::stof(string_view s, size_t* pos)
{
    // string_view can't be counted on to end with a terminating null, so
    // for safety, create a temporary string. This looks wasteful, but it's
    // not as bad as you think -- fully compliant C++ >= 11 implementations
    // will use the "short string optimization", meaning that this string
    // creation will NOT need an allocation/free for most strings we expect
    // to hold a text representation of a float.
    return Strutil::stof(std::string(s), pos);
}



double
Strutil::stod(const char* s, size_t* pos)
{
    if (s) {
        char* endptr;
        double r = Strutil::strtod(s, &endptr);
        if (endptr != s) {
            if (pos)
                *pos = size_t(endptr - s);
            return r;
        }
    }
    // invalid
    if (pos)
        *pos = 0;
    return 0;
}


double
Strutil::stod(const std::string& s, size_t* pos)
{
    return Strutil::stod(s.c_str(), pos);
}


double
Strutil::stod(string_view s, size_t* pos)
{
    // string_view can't be counted on to end with a terminating null, so
    // for safety, create a temporary string. This looks wasteful, but it's
    // not as bad as you think -- fully compliant C++ >= 11 implementations
    // will use the "short string optimization", meaning that this string
    // creation will NOT need an allocation/free for most strings we expect
    // to hold a text representation of a float.
    return Strutil::stod(std::string(s).c_str(), pos);
}



unsigned int
Strutil::stoui(string_view s, size_t* pos, int base)
{
    // For conversion of string_view to unsigned int, fall back on strtoul.
    char* endptr = nullptr;
    std::string ss(s);
    auto r = strtoul(ss.c_str(), &endptr, base);
    if (pos)
        *pos = size_t(endptr - ss.c_str());
    return static_cast<unsigned int>(r);
}



bool
Strutil::string_is_int(string_view s)
{
    size_t pos;
    Strutil::stoi(s, &pos);
    if (pos) {  // skip remaining whitespace
        s.remove_prefix(pos);
        Strutil::skip_whitespace(s);
    }
    return pos && s.empty();  // consumed the whole string
}


bool
Strutil::string_is_float(string_view s)
{
    size_t pos;
    Strutil::stof(s, &pos);
    if (pos) {  // skip remaining whitespace
        s.remove_prefix(pos);
        Strutil::skip_whitespace(s);
    }
    return pos && s.empty();  // consumed the whole string
}



bool
Strutil::scan_datetime(string_view str, int& year, int& month, int& day,
                       int& hour, int& min, int& sec)
{
    bool ok = parse_int(str, year)
              && (parse_char(str, ':', false) || parse_char(str, '-', false)
                  || parse_char(str, '/', false))
              && parse_int(str, month)
              && (parse_char(str, ':', false) || parse_char(str, '-', false)
                  || parse_char(str, '/', false))
              && parse_int(str, day) && parse_int(str, hour)
              && parse_char(str, ':', false) && parse_int(str, min)
              && parse_char(str, ':', false) && parse_int(str, sec);  //NOSONAR
    return ok && month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour >= 0
           && hour <= 23 && min >= 0 && min <= 59 && sec >= 0 && sec <= 59;
}



// https://en.wikipedia.org/wiki/Levenshtein_distance
// With some guidance from https://github.com/wooorm/levenshtein.c (MIT
// license) as well.
static size_t
levenshtein_distance(string_view a, string_view b)
{
    if (a == b)
        return 0;
    if (a.size() == 0)
        return b.size();
    if (b.size() == 0)
        return a.size();

    size_t* cache;
    OIIO_ALLOCATE_STACK_OR_HEAP(cache, size_t, a.size());
    std::iota(cache, cache + a.size(), 1);  // [ 1, 2, 3, ... ]

    size_t result = 0;
    for (size_t bi = 0; bi < b.size(); ++bi) {
        char code   = b[bi];
        size_t dist = bi;
        for (size_t ai = 0; ai < a.size(); ++ai) {
            size_t bdist = code == a[ai] ? dist : dist + 1;
            dist         = cache[ai];
            result       = dist > result ? (bdist > result ? result + 1 : bdist)
                                         : (bdist > dist ? dist + 1 : bdist);
            cache[ai]    = result;
        }
    }
    return result;
}



size_t
Strutil::edit_distance(string_view a, string_view b, EditDistMetric metric)
{
    return levenshtein_distance(a, b);
}



/// Interpret a string as a boolean value using the following heuristic:
///   - If the string is a valid numeric value (represents an integer or
///     floating point value), return true if it's non-zero, false if it's
///     zero.
///   - If the string is one of "false", "no", or "off", or if it contains
///     only whitespace, return false.
///   - All other non-empty strings return true.
/// The comparisons are case-insensitive and ignore leading and trailing
/// whitespace.
bool
Strutil::eval_as_bool(string_view value)
{
    Strutil::trim_whitespace(value);
    if (Strutil::string_is_int(value)) {
        return Strutil::stoi(value) != 0;
    } else if (Strutil::string_is_float(value)) {
        return Strutil::stof(value) != 0.0f;
    } else {
        return !(value.empty() || Strutil::iequals(value, "false")
                 || Strutil::iequals(value, "no")
                 || Strutil::iequals(value, "off"));
    }
}

OIIO_NAMESPACE_END
