// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <locale.h>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#if defined(__APPLE__) || defined(__FreeBSD__)
#    include <xlocale.h>
#endif

#include <boost/algorithm/string.hpp>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/ustring.h>

#ifdef _WIN32
#    include <shellapi.h>
#endif


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
#include "stb_sprintf.h"


OIIO_NAMESPACE_BEGIN


namespace {
static std::mutex output_mutex;

// On systems that support it, get a location independent locale.
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__GLIBC__)
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



OIIO_NO_SANITIZE_ADDRESS
const char*
string_view::c_str() const
{
    // Usual case: either empty, or null-terminated
    if (m_len == 0)  // empty string
        return "";

    // This clause attempts to find out if there's a string-teriminating
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
    if (m_chars[m_len] == 0)  // 0-terminated
        return m_chars;

    // Rare case: may not be 0-terminated. Bite the bullet and construct a
    // 0-terminated string.  We use ustring as a way to avoid any issues of
    // who cleans up the allocation, though it means that it will stay in
    // the ustring table forever. Punt on this for now, it's an edge case
    // that we need to handle, but is not likely to ever be an issue.
    return ustring(m_chars, 0, m_len).c_str();
}



void
Strutil::sync_output(FILE* file, string_view str)
{
    if (str.size() && file) {
        std::unique_lock<std::mutex> lock(output_mutex);
        fwrite(str.data(), 1, str.size(), file);
        fflush(file);
    }
}



void
Strutil::sync_output(std::ostream& file, string_view str)
{
    if (str.size()) {
        std::unique_lock<std::mutex> lock(output_mutex);
        file << str;
        file.flush();
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
        ap     = apsave;
#endif
    }
}



std::string
Strutil::vformat(const char* fmt, va_list ap)
{
    // For now, just treat as a synonym for vsprintf
    return vsprintf(fmt, ap);
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
        return format("%lld KB", (long long)bytes / KB);
    } else {
        // Just bytes, don't bother with decimalization
        return format("%lld B", (long long)bytes);
    }
    return format("%1.*f %s", digits, d, units);
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
        out += format("%dd %dh ", d, h);
    else if (h)
        out += format("%dh ", h);
    if (m || h || d)
        out += format("%dm %1.*fs", m, digits, secs);
    else
        out += format("%1.*fs", digits, secs);
    return out;
}



bool
Strutil::get_rest_arguments(const std::string& str, std::string& base,
                            std::map<std::string, std::string>& result)
{
    // Disregard the Windows long path question style prefix "\\?\"
    static const std::string longPathQPrefix("\\\\?\\");
    auto find_start_pos = starts_with(str, longPathQPrefix)
                              ? longPathQPrefix.size()
                              : size_t(0);

    std::string::size_type mark_pos = str.find_first_of("?", find_start_pos);
    if (mark_pos == std::string::npos) {
        base = str;
        return true;
    }

    base             = str.substr(0, mark_pos);
    std::string rest = str.substr(mark_pos + 1);
    std::vector<std::string> rest_tokens;
    Strutil::split(rest, rest_tokens, "&");
    for (const std::string& keyval : rest_tokens) {
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
            }
            s.insert(i, &c, 1);
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
                case 'a':
                    s[i] = '\a';
                    break;
                    // default case: the deletion is enough (backslash and quote)
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
    std::string allsep = Strutil::sprintf("%s%s", sep, presep);
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



// DEPRECATED(2.0) -- for link compatibility
namespace Strutil {
std::string
wordwrap(string_view src, int columns, int prefix)
{
    return wordwrap(src, columns, prefix, " ", "");
}
}  // namespace Strutil



bool
Strutil::iequals(string_view a, string_view b)
{
    return boost::algorithm::iequals(a, b, std::locale::classic());
}


bool
Strutil::iless(string_view a, string_view b)
{
    return boost::algorithm::ilexicographical_compare(a, b,
                                                      std::locale::classic());
}


bool
Strutil::starts_with(string_view a, string_view b)
{
    return boost::algorithm::starts_with(a, b);
}


bool
Strutil::istarts_with(string_view a, string_view b)
{
    return boost::algorithm::istarts_with(a, b, std::locale::classic());
}


bool
Strutil::ends_with(string_view a, string_view b)
{
    return boost::algorithm::ends_with(a, b);
}


bool
Strutil::iends_with(string_view a, string_view b)
{
    return boost::algorithm::iends_with(a, b, std::locale::classic());
}


bool
Strutil::contains(string_view a, string_view b)
{
    return boost::algorithm::contains(a, b);
}


bool
Strutil::icontains(string_view a, string_view b)
{
    return boost::algorithm::icontains(a, b, std::locale::classic());
}


void
Strutil::to_lower(std::string& a)
{
    boost::algorithm::to_lower(a, std::locale::classic());
}


void
Strutil::to_upper(std::string& a)
{
    boost::algorithm::to_upper(a, std::locale::classic());
}



bool
Strutil::StringIEqual::operator()(const char* a, const char* b) const noexcept
{
    return boost::algorithm::iequals(a, b, std::locale::classic());
}


bool
Strutil::StringILess::operator()(const char* a, const char* b) const noexcept
{
    return boost::algorithm::ilexicographical_compare(a, b,
                                                      std::locale::classic());
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
        while (i < len && ::isspace(str[i]))
            i++;
        j = i;
        while (i < len && !::isspace(str[i]))
            i++;
        if (j < i) {
            if (--maxsplit <= 0)
                break;
            result.push_back(str.substr(j, i - j));
            while (i < len && ::isspace(str[i]))
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
    size_t sl  = s.size();
    size_t tl  = t.size();
    size_t len = sl + tl;
    std::unique_ptr<char[]> heap_buf;
    char local_buf[256];
    char* buf = local_buf;
    if (len > sizeof(local_buf)) {
        heap_buf.reset(new char[len]);
        buf = heap_buf.get();
    }
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
    char local_buf[256];
    char* buf = local_buf;
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



#ifdef _WIN32
std::wstring
Strutil::utf8_to_utf16(string_view str) noexcept
{
    std::wstring native;

    native.resize(
        MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), NULL, 0));
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), &native[0],
                        (int)native.size());

    return native;
}



std::string
Strutil::utf16_to_utf8(const std::wstring& str) noexcept
{
    std::string utf8;

    utf8.resize(WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), NULL,
                                    0, NULL, NULL));
    WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), &utf8[0],
                        (int)utf8.size(), NULL, NULL);

    return utf8;
}
#endif



char*
Strutil::safe_strcpy(char* dst, string_view src, size_t size) noexcept
{
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



void
Strutil::skip_whitespace(string_view& str) noexcept
{
    while (str.size() && isspace(str.front()))
        str.remove_prefix(1);
}



void
Strutil::remove_trailing_whitespace(string_view& str) noexcept
{
    while (str.size() && isspace(str.back()))
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
        if (isspace(*end) && !quoted)
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
    *state = utf8d[256 + *state + type];
    return *state;
}

void
Strutil::utf8_to_unicode(string_view str, std::vector<uint32_t>& uvec)
{
    const char* begin = str.begin();
    const char* end   = str.end();
    uint32_t state    = 0;
    for (; begin != end; ++begin) {
        uint32_t codepoint = 0;
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
        acc       = acc * base + c;
        anydigits = true;
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
    // static initialization inside function is thread-safe by C++11 rules!
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
    return Strutil::stof(std::string(s).c_str(), pos);
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


OIIO_NAMESPACE_END
