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
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "dassert.h"

#include "strutil.h"


OIIO_NAMESPACE_ENTER
{

std::string
Strutil::format (const char *fmt, ...)
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
Strutil::memformat (off_t bytes, int digits)
{
    const long long KB = (1<<10);
    const long long MB = (1<<20);
    const long long GB = (1<<30);
    const char *units = "B";
    double d = bytes;
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
        out += format ("%dd ", d);
    if (h || d)
        out += format ("%2dh ", h);
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

    boost::char_separator<char> sep ("&");
    std::string rest = str.substr (mark_pos + 1);
    boost::tokenizer<boost::char_separator<char> > rest_tokens (rest, sep);
    BOOST_FOREACH (std::string keyval, rest_tokens) {
        mark_pos = keyval.find_first_of ("=");
        if (mark_pos == std::string::npos)
            return false;
        result[keyval.substr (0, mark_pos)] = keyval.substr (mark_pos + 1);
    }

    return true;
}



std::string
Strutil::escape_chars (const std::string &unescaped)
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
Strutil::unescape_chars (const std::string &escaped)
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
                    c = s[i+1];
                }
                s[i] = (char) octalChar;
            }

        }
    }
    return s;
}



std::string
Strutil::wordwrap (std::string src, int columns, int prefix)
{
    std::ostringstream out;
    if (columns < prefix+20)
        return src;   // give up, no way to make it wrap
    columns -= prefix;  // now columns is the real width we have to work with
    while ((int)src.length() > columns) {
        // break the string in two
        size_t breakpoint = src.find_last_of (' ', columns);
        if (breakpoint == std::string::npos)
            breakpoint = columns;
        out << src.substr(0, breakpoint) << "\n" << std::string (prefix, ' ');
        src = src.substr (breakpoint);
        while (src[0] == ' ')
            src.erase (0, 1);
    }
    out << src;
    return out.str();
}



namespace {
static std::locale loc = std::locale::classic();
}


bool
Strutil::iequals (const std::string &a, const std::string &b)
{
    return boost::algorithm::iequals (a, b, loc);
}


bool
Strutil::iequals (const char *a, const char *b)
{
    return boost::algorithm::iequals (a, b, loc);
}


bool
Strutil::istarts_with (const std::string &a, const std::string &b)
{
    return boost::algorithm::istarts_with (a, b, loc);
}


bool
Strutil::istarts_with (const char *a, const char *b)
{
    return boost::algorithm::istarts_with (a, b, loc);
}


bool
Strutil::iends_with (const std::string &a, const std::string &b)
{
    return boost::algorithm::iends_with (a, b, loc);
}


bool
Strutil::iends_with (const char *a, const char *b)
{
    return boost::algorithm::iends_with (a, b, loc);
}


void
Strutil::to_lower (std::string &a)
{
    boost::algorithm::to_lower (a, loc);
}


void
Strutil::to_upper (std::string &a)
{
    boost::algorithm::to_upper (a, loc);
}


}
OIIO_NAMESPACE_EXIT
