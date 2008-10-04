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

#include "export.h"
#include "thread.h"
#include "strutil.h"
#include "hash.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ustring */
#include "ustring.h"
#undef DLL_EXPORT_PUBLIC



#ifdef WINNT
typedef hash_map <const char *, ustring::TableRep *, Strutil::StringHash> UstringTable;
#else
typedef hash_map <const char *, ustring::TableRep *, Strutil::StringHash, Strutil::StringEqual> UstringTable;
#endif
static UstringTable ustring_table;
static mutex ustring_mutex;

std::string ustring::empty_std_string ("");



const ustring::TableRep *
ustring::_make_unique (const char *str)
{
    // Eliminate NULLs
    if (! str)
        str = "";

    // Check the ustring table to see if this string already exists.  If so,
    // construct from its canonical representation.
    lock_guard guard(ustring_mutex);
    UstringTable::const_iterator found = ustring_table.find (str);
    if (found != ustring_table.end())
        return found->second;

    // This string is not yet in the ustring table.  Create a new entry.
    size_t size = sizeof(ustring::TableRep) + strlen(str) + 1;
    ustring::TableRep *rep = (ustring::TableRep *) malloc (size);
    new (rep) ustring::TableRep (str);
    ustring_table[rep->c_str()] = rep;
    return rep;
}



ustring
ustring::format (const char *fmt, ...)
{
    ustring tok;
    va_list ap;
    va_start (ap, fmt);

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
        tok.assign (&buf[0]);
    } else {
        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So do a malloc of the right size and try again.
        // This doesn't happen very often if we chose our initial size
        // well.
        std::vector <char> buf;
        size = needed;
        buf.resize (size);
        needed = vsnprintf (&buf[0], size, fmt, apcopy);
        tok.assign (&buf[0]);
    }

    va_end (ap);
    return tok;
}
