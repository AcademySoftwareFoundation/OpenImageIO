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


#include <string>
#include <cstdarg>
#include <vector>
#include <iostream>
#include <cmath>
#include <sstream>
#include <limits>
#include <mutex>

#include <boost/algorithm/string.hpp>

#include <OpenImageIO/platform.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/string_view.h>

#ifdef _WIN32
# include <shellapi.h>
#endif



OIIO_NAMESPACE_BEGIN


namespace {
static std::mutex output_mutex;
};



OIIO_NO_SANITIZE_ADDRESS
const char *
string_view::c_str() const
{
    // Usual case: either empty, or null-terminated
    if (m_len == 0)   // empty string
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
Strutil::sync_output (FILE *file, string_view str)
{
    if (str.size() && file) {
        std::unique_lock<std::mutex> lock (output_mutex);
        fwrite (str.data(), 1, str.size(), file);
        fflush (file);
    }
}



void
Strutil::sync_output (std::ostream& file, string_view str)
{
    if (str.size()) {
        std::unique_lock<std::mutex> lock (output_mutex);
        file << str;
    }
}



std::string
Strutil::format_raw (const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    std::string buf = vformat (fmt, ap);
    va_end (ap);
    return buf;
}



std::string
Strutil::vformat (const char *fmt, va_list ap)
{
    // Allocate a buffer on the stack that's big enough for us almost
    // all the time.  Be prepared to allocate dynamically if it doesn't fit.
    size_t size = 1024;
    char stackbuf[1024];
    std::vector<char> dynamicbuf;
    char *buf = &stackbuf[0];
    
    while (1) {
        // Try to vsnprintf into our buffer.
        va_list apsave;
#ifdef va_copy
        va_copy (apsave, ap);
#else
        apsave = ap;
#endif
        int needed = vsnprintf (buf, size, fmt, ap);
        va_end (ap);

        // NB. C99 (which modern Linux and OS X follow) says vsnprintf
        // failure returns the length it would have needed.  But older
        // glibc and current Windows return -1 for failure, i.e., not
        // telling us how much was needed.

        if (needed < (int)size && needed >= 0) {
            // It fit fine so we're done.
            return std::string (buf, (size_t) needed);
        }

        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So try again using a dynamic buffer.  This
        // doesn't happen very often if we chose our initial size well.
        size = (needed > 0) ? (needed+1) : (size*2);
        dynamicbuf.resize (size);
        buf = &dynamicbuf[0];
#ifdef va_copy
        va_copy (ap, apsave);
#else
        ap = apsave;
#endif
    }
}



std::string
Strutil::memformat (long long bytes, int digits)
{
    const long long KB = (1<<10);
    const long long MB = (1<<20);
    const long long GB = (1<<30);
    const char *units = "B";
    double d = (double)bytes;
    if (bytes >= GB) {
        units = "GB";
        d = (double)bytes / GB;
    } else if (bytes >= MB) {
        units = "MB";
        d = (double)bytes / MB;
    } else if (bytes >= KB) {
        // Just KB, don't bother with decimalization
        return format ("%lld KB", (long long)bytes/KB);
    } else {
        // Just bytes, don't bother with decimalization
        return format ("%lld B", (long long)bytes);
    }
    return format ("%1.*f %s", digits, d, units);
}



std::string
Strutil::timeintervalformat (double secs, int digits)
{
    const double mins = 60;
    const double hours = mins * 60;
    const double days = hours * 24;

    std::string out;
    int d = (int) floor (secs / days);
    secs = fmod (secs, days);
    int h = (int) floor (secs / hours);
    secs = fmod (secs, hours);
    int m = (int) floor (secs / mins);
    secs = fmod (secs, mins);
    if (d)
        out += format ("%dd %dh ", d, h);
    else if (h)
        out += format ("%dh ", h);
    if (m || h || d)
        out += format ("%dm %1.*fs", m, digits, secs);
    else
        out += format ("%1.*fs", digits, secs);
    return out;
}



bool
Strutil::get_rest_arguments (const std::string &str, std::string &base,
                             std::map<std::string, std::string> &result)
{
    std::string::size_type mark_pos = str.find_first_of ("?");
    if (mark_pos == std::string::npos) {
        base = str;
        return true;
    }

    base = str.substr (0, mark_pos);
    std::string rest = str.substr (mark_pos + 1);
    std::vector<std::string> rest_tokens;
    Strutil::split (rest, rest_tokens, "&");
    for (const std::string &keyval : rest_tokens) {
        mark_pos = keyval.find_first_of ("=");
        if (mark_pos == std::string::npos)
            return false;
        result[keyval.substr (0, mark_pos)] = keyval.substr (mark_pos + 1);
    }

    return true;
}



std::string
Strutil::escape_chars (string_view unescaped)
{
    std::string s = unescaped;
    for (size_t i = 0;  i < s.length();  ++i) {
        char c = s[i];
        if (c == '\n' || c == '\t' || c == '\v' || c == '\b' || 
            c == '\r' || c == '\f' || c == '\a' || c == '\\' || c == '\"') {
            s[i] = '\\';
            ++i;
            switch (c) {
            case '\n' : c = 'n'; break;
            case '\t' : c = 't'; break;
            case '\v' : c = 'v'; break;
            case '\b' : c = 'b'; break;
            case '\r' : c = 'r'; break;
            case '\f' : c = 'f'; break;
            case '\a' : c = 'a'; break;
            }
            s.insert (i, &c, 1);
        }
    }
    return s;
}



std::string
Strutil::unescape_chars (string_view escaped)
{
    std::string s = escaped;
    for (size_t i = 0, len = s.length();  i < len;  ++i) {
        if (s[i] == '\\') {
            char c = s[i+1];
            if (c == 'n' || c == 't' || c == 'v' || c == 'b' || 
                c == 'r' || c == 'f' || c == 'a' || c == '\\' || c == '\"') {
                s.erase (i, 1);
                --len;
                switch (c) {
                case 'n' : s[i] = '\n'; break;
                case 't' : s[i] = '\t'; break;
                case 'v' : s[i] = '\v'; break;
                case 'b' : s[i] = '\b'; break;
                case 'r' : s[i] = '\r'; break;
                case 'f' : s[i] = '\f'; break;
                case 'a' : s[i] = '\a'; break;
                // default case: the deletion is enough (backslash and quote)
                }
            } else if (c >= '0' && c < '8') {
                // up to 3 octal digits
                int octalChar = 0;
                for (int j = 0;  j < 3 && c >= '0' && c <= '7';  ++j) {
                    octalChar = 8*octalChar + (c - '0');
                    s.erase (i, 1);
                    --len;
                    c = i+1 < len ? s[i+1] : '\0';
                }
                s[i] = (char) octalChar;
            }

        }
    }
    return s;
}



std::string
Strutil::wordwrap (string_view src, int columns, int prefix)
{
    if (columns < prefix+20)
        return src;   // give up, no way to make it wrap
    std::ostringstream out;
    columns -= prefix;  // now columns is the real width we have to work with
    while ((int)src.length() > columns) {
        // break the string in two
        size_t breakpoint = src.find_last_of (' ', columns);
        if (breakpoint == string_view::npos)
            breakpoint = columns;
        out << src.substr(0, breakpoint) << "\n" << std::string (prefix, ' ');
        src = src.substr (breakpoint);
        while (src[0] == ' ')
            src.remove_prefix (1);
    }
    out << src;
    return out.str();
}




static std::locale& loc ()
{
    // Rather than have a file-level static locale which could suffer from
    // the static initialization order fiasco, make it a static pointer
    // inside the function that is allocated the first time it's needed. It
    // will "leak" in the sense that this one locale is never deleted, but
    // so what?
#if OIIO_MSVS_BEFORE_2015
    // MSVS prior to 2015 could not be trusted to make in-function static
    // initialization thread-safe, as required by C++11.
    static spin_mutex mutex;
    spin_lock lock (mutex);
#endif
    static std::locale *loc = new std::locale (std::locale::classic());
    return *loc;
}



bool
Strutil::iequals (string_view a, string_view b)
{
    return boost::algorithm::iequals (a, b, loc());
}


bool
Strutil::starts_with (string_view a, string_view b)
{
    return boost::algorithm::starts_with (a, b);
}


bool
Strutil::istarts_with (string_view a, string_view b)
{
    return boost::algorithm::istarts_with (a, b, loc());
}


bool
Strutil::ends_with (string_view a, string_view b)
{
    return boost::algorithm::ends_with (a, b);
}


bool
Strutil::iends_with (string_view a, string_view b)
{
    return boost::algorithm::iends_with (a, b, loc());
}


bool
Strutil::contains (string_view a, string_view b)
{
    return boost::algorithm::contains (a, b);
}


bool
Strutil::icontains (string_view a, string_view b)
{
    return boost::algorithm::icontains (a, b, loc());
}


void
Strutil::to_lower (std::string &a)
{
    boost::algorithm::to_lower (a, loc());
}


void
Strutil::to_upper (std::string &a)
{
    boost::algorithm::to_upper (a, loc());
}



string_view
Strutil::strip (string_view str, string_view chars)
{
    if (chars.empty())
        chars = string_view(" \t\n\r\f\v", 6);
    size_t b = str.find_first_not_of (chars);
    if (b == std::string::npos)
        return string_view ();
    size_t e = str.find_last_not_of (chars);
    DASSERT (e != std::string::npos);
    return str.substr (b, e-b+1);
}



static void
split_whitespace (string_view str, std::vector<string_view> &result,
                  int maxsplit)
{
    // Implementation inspired by Pystring
    string_view::size_type i, j, len = str.size();
    for (i = j = 0; i < len; ) {
        while (i < len && ::isspace(str[i]))
            i++;
        j = i;
        while (i < len && ! ::isspace(str[i]))
            i++;
        if (j < i) {
            if (--maxsplit <= 0)
                break;
            result.push_back (str.substr(j, i - j));
            while (i < len && ::isspace(str[i]))
                i++;
            j = i;
        }
    }
    if (j < len)
        result.push_back (str.substr(j, len - j));
}



void
Strutil::split (string_view str, std::vector<std::string> &result,
                string_view sep, int maxsplit)
{
    std::vector<string_view> sr_result;
    split (str, sr_result, sep, maxsplit);
    result.clear ();
    result.reserve (sr_result.size());
    for (size_t i = 0, e = sr_result.size(); i != e; ++i)
        result.push_back (sr_result[i]);
}



void
Strutil::split (string_view str, std::vector<string_view> &result,
                string_view sep, int maxsplit)
{
    // Implementation inspired by Pystring
    result.clear();
    if (maxsplit < 0)
        maxsplit = std::numeric_limits<int>::max();
    if (sep.size() == 0) {
        split_whitespace (str, result, maxsplit);
        return;
    }
    size_t i = 0, j = 0, len = str.size(), n = sep.size();
    while (i+n <= len) {
        if (str[i] == sep[0] && str.substr(i, n) == sep) {
            if (--maxsplit <= 0)
                break;
            result.push_back (str.substr(j, i - j));
            i = j = i + n;
        } else {
            i++;
        }
    }
    result.push_back (str.substr(j, len-j));
}



std::string
Strutil::join (const std::vector<string_view> &seq, string_view str)
{
    // Implementation inspired by Pystring
    size_t seqlen = seq.size();
    if (seqlen == 0)
        return std::string();
    std::string result (seq[0]);
    for (size_t i = 1; i < seqlen; ++i) {
        result += str;
        result += seq[i];
    }
    return result;
}



std::string
Strutil::join (const std::vector<std::string> &seq, string_view str)
{
    std::vector<string_view> seq_sr (seq.size());
    for (size_t i = 0, e = seq.size(); i != e; ++i)
        seq_sr[i] = seq[i];
    return join (seq_sr, str);
}



std::string
Strutil::repeat (string_view str, int n)
{
    std::ostringstream out;
    while (n-- > 0)
        out << str;
    return out.str();
}



std::string
Strutil::replace (string_view str, string_view pattern,
                  string_view replacement, bool global)
{
    std::string r;
    while (1) {
        size_t f = str.find (pattern);
        if (f != str.npos) {
            // Pattern found -- copy the part of str prior to the pattern,
            // then copy the replacement, and skip str up to the part after
            // the pattern and continue for another go at it.
            r.append (str.data(), f);
            r.append (replacement.data(), replacement.size());
            str.remove_prefix (f + pattern.size());
            if (global)
                continue;   // Try for another match
        }
        // Pattern not found -- copy remaining string and we're done
        r.append (str.data(), str.size());
        break;
    }
    return r;
}



#ifdef _WIN32
std::wstring
Strutil::utf8_to_utf16 (string_view str)
{
    std::wstring native;
    
    native.resize(MultiByteToWideChar (CP_UTF8, 0, str.data(), str.length(), NULL, 0));
    MultiByteToWideChar (CP_UTF8, 0, str.data(), str.length(), &native[0], (int)native.size());

    return native;
}



std::string
Strutil::utf16_to_utf8 (const std::wstring& str)
{
    std::string utf8;

    utf8.resize(WideCharToMultiByte (CP_UTF8, 0, str.data(), str.length(), NULL, 0, NULL, NULL));
    WideCharToMultiByte (CP_UTF8, 0, str.data(), str.length(), &utf8[0], (int)utf8.size(), NULL, NULL);

    return utf8;
}
#endif



char *
Strutil::safe_strcpy (char *dst, string_view src, size_t size)
{
    if (src.size()) {
        size_t end = std::min (size-1, src.size());
        for (size_t i = 0;  i < end;  ++i)
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
Strutil::skip_whitespace (string_view &str)
{
    while (str.size() && isspace(str[0]))
        str.remove_prefix (1);
}



bool
Strutil::parse_char (string_view &str, char c,
                     bool skip_whitespace, bool eat)
{
    string_view p = str;
    if (skip_whitespace)
        Strutil::skip_whitespace (p);
    if (p.size() && p[0] == c) {
        if (eat) {
            p.remove_prefix (1);
            str = p;
        }
        return true;
    }
    return false;
}



bool
Strutil::parse_until_char (string_view &str, char c, bool eat)
{
    string_view p = str;
    while (p.size() && p[0] != c)
        p.remove_prefix (1);
    if (eat)
        str = p;
    return p.size() && p[0] == c;
}



bool
Strutil::parse_prefix (string_view &str, string_view prefix, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    if (Strutil::starts_with (p, prefix)) {
        p.remove_prefix (prefix.size());
        if (eat)
            str = p;
        return true;
    }
    return false;
}



bool
Strutil::parse_int (string_view &str, int &val, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    if (! p.size())
        return false;
    const char *end = p.begin();
    int v = strtol (p.begin(), (char**)&end, 10);
    if (end == p.begin())
        return false;  // no integer found
    if (eat) {
        p.remove_prefix (end-p.begin());
        str = p;
    }
    val = v;
    return true;
}



bool
Strutil::parse_float (string_view &str, float &val, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    if (! p.size())
        return false;
    const char *end = p.begin();
    float v = (float) strtod (p.begin(), (char**)&end);
    if (end == p.begin())
        return false;  // no integer found
    if (eat) {
        p.remove_prefix (end-p.begin());
        str = p;
    }
    val = v;
    return true;
}



bool
Strutil::parse_string (string_view &str, string_view &val,
                       bool eat, QuoteBehavior keep_quotes)
{
    string_view p = str;
    skip_whitespace (p);
    bool quoted = parse_char (p, '\"');
    const char *begin = p.begin(), *end = p.begin();
    bool escaped = false;
    while (end != p.end()) {
        if (isspace(*end) && !quoted)
            break;   // not quoted and we hit whitespace: we're done
        if (quoted && *end == '\"' && ! escaped)
            break;   // closing quite -- we're done (beware embedded quote)
        escaped = (p[0] == '\\');
        ++end;
    }
    if (quoted && keep_quotes == KeepQuotes) {
        if (*end == '\"')
            val = string_view (begin-1, size_t(end-begin)+2);
        else
            val = string_view (begin-1, size_t(end-begin)+1);
    } else {
        val = string_view (begin, size_t(end-begin));
    }
    p.remove_prefix (size_t(end-begin));
    if (quoted && p.size() && p[0] == '\"')
        p.remove_prefix (1);  // eat closing quote
    if (eat)
        str = p;
    return quoted || val.size();
}



string_view
Strutil::parse_word (string_view &str, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    const char *begin = p.begin(), *end = p.begin();
    while (end != p.end() && isalpha(*end))
        ++end;
    size_t wordlen = end - begin;
    if (eat && wordlen) {
        p.remove_prefix (wordlen);
        str = p;
    }
    return string_view (begin, wordlen);
}



string_view
Strutil::parse_identifier (string_view &str, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    const char *begin = p.begin(), *end = p.begin();
    if (end != p.end() && (isalpha(*end) || *end == '_'))
        ++end;
    else
       return string_view();  // not even the start of an identifier
    while (end != p.end() && (isalpha(*end) || isdigit(*end) || *end == '_'))
        ++end;
    if (eat) {
        p.remove_prefix (size_t(end-begin));
        str = p;
    }
    return string_view (begin, size_t(end-begin));
}



string_view
Strutil::parse_identifier (string_view &str, string_view allowed, bool eat)
{
    string_view p = str;
    skip_whitespace (p);
    const char *begin = p.begin(), *end = p.begin();
    if (end != p.end() && (isalpha(*end) || *end == '_' ||
                           allowed.find(*end) != string_view::npos))
        ++end;
    else
       return string_view();  // not even the start of an identifier
    while (end != p.end() && (isalpha(*end) || isdigit(*end) || *end == '_' ||
                              allowed.find(*end) != string_view::npos))
        ++end;
    if (eat) {
        p.remove_prefix (size_t(end-begin));
        str = p;
    }
    return string_view (begin, size_t(end-begin));
}



bool
Strutil::parse_identifier_if (string_view &str, string_view id, bool eat)
{
    string_view head = parse_identifier (str, false /* don't eat */);
    if (head == id) {
        if (eat)
            parse_identifier (str);
        return true;
    }
    return false;
}



string_view
Strutil::parse_until (string_view &str, string_view sep, bool eat)
{
    string_view p = str;
    const char *begin = p.begin(), *end = p.begin();
    while (end != p.end() && sep.find(*end) == string_view::npos)
        ++end;
    size_t wordlen = end - begin;
    if (eat && wordlen) {
        p.remove_prefix (wordlen);
        str = p;
    }
    return string_view (begin, wordlen);
}


string_view
Strutil::parse_nested (string_view &str, bool eat)
{
    // Make sure we have a valid string and ascertain the characters that
    // nest and unnest.
    string_view p = str;
    if (! p.size())
        return string_view();    // No proper opening
    char opening = p[0];
    char closing = 0;
    if      (opening == '(') closing = ')';
    else if (opening == '[') closing = ']';
    else if (opening == '{') closing = '}';
    else    return string_view();

    // Walk forward in the string until we exactly unnest compared to the
    // start.
    size_t len = 1;
    int nesting = 1;
    for ( ; nesting && len < p.size(); ++len) {
        if (p[len] == opening)
            ++nesting;
        else if (p[len] == closing)
            --nesting;
    }

    if (nesting)
        return string_view();    // No proper closing

    ASSERT (p[len-1] == closing);

    // The result is the first len characters
    string_view result = str.substr (0, len);
    if (eat)
        str.remove_prefix (len);
    return result;
}




/*
Copyright for decode function:
See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

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
};

inline uint32_t
decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
  uint32_t type = utf8d[byte];
  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);
  *state = utf8d[256 + *state + type];
  return *state;
}

void
Strutil::utf8_to_unicode (string_view str, std::vector<uint32_t> &uvec)
{
    const char* begin = str.begin();
    const char* end = str.end();
    uint32_t state = 0;
    for (; begin != end; ++begin) {
        uint32_t codepoint = 0;
        if (!decode(&state, &codepoint, (unsigned char) *begin))
            uvec.push_back(codepoint);
    }
}



/* base64 code is based upon: http://www.adp-gmbh.ch/cpp/common/base64.html
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
Strutil::base64_encode (string_view str)
{
    static const char *base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string ret;
    ret.reserve ((str.size() * 4 + 2)/ 3);
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    while (str.size()) {
        char_array_3[i++] = str.front();
        str.remove_prefix (1);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
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
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        while (i++ < 3)
            ret += '=';
    }
    return ret;
}


OIIO_NAMESPACE_END
