/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <string>
#include <cstdarg>
#include <vector>
#include <string>

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
    // Allocate a buffer that's big enough for us almost all the time
    size_t size = 1024;
    std::vector<char> buf (size);
    ASSERT (buf.size() == size);

    va_list apcopy;
    va_copy (apcopy, ap);
    int needed = vsnprintf (&buf[0], size, fmt, ap);

    if (needed > size) {
        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So re-allocate and try again.  Presumably,
        // this doesn't happen very often if we chose our initial size
        // well.
        buf.resize (size);
        needed = vsnprintf (&buf[0], size, fmt, apcopy);
        DASSERT (needed <= size);
    }

    return &buf[0];
}
