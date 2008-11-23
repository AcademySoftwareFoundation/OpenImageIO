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

#include "dassert.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing Strutil */
#include "strutil.h"
#undef DLL_EXPORT_PUBLIC



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
    // all the time.
    size_t size = 1024;
    char buf[size];

    // Try to vsnprintf into our buffer.
    va_list apcopy;
    va_copy (apcopy, ap);
    int needed = vsnprintf (&buf[0], size, fmt, ap);

    if (needed <= size) {
        // It fit fine the first time, we're done.
        return std::string (&buf[0]);
    } else {
        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So do a malloc of the right size and try again.
        // This doesn't happen very often if we chose our initial size
        // well.
        std::vector <char> buf;
        size = needed;
        buf.resize (size);
        needed = vsnprintf (&buf[0], size, fmt, apcopy);
        DASSERT (needed <= size);
        return std::string (&buf[0]);
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
        units = "KB";
        d = (double)bytes / KB;
    }
    return format ("%1.*g %s", digits, d, units);
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
    int s = (int) floor (secs);
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
