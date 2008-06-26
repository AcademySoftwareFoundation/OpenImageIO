/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
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
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


#include <string>

#include "export.h"
#include "thread.h"
#include "strutil.h"
#include "hash.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing Token */
#include "token.h"
#undef DLL_EXPORT_PUBLIC


typedef hash_map <const char *, Token::TableRep *, Strutil::StringHash> TokenTable;

static TokenTable token_table;
static mutex token_mutex;



const Token::TableRep *
Token::_make_unique (const char *str)
{
    // Eliminate NULLs
    if (! str)
        str = "";

    // Check the token table to see if this string already exists.  If so,
    // construct from its canonical representation.
    lock_guard guard(token_mutex);
    TokenTable::const_iterator found = token_table.find (str);
    if (found != token_table.end())
        return found->second;

    // This string is not yet in the token table.  Create a new entry.
    size_t size = sizeof(Token::TableRep) + strlen(str) + 1;
    Token::TableRep *rep = (Token::TableRep *) malloc (size);
    new (rep) Token::TableRep (str);
    token_table[rep->c_str()] = rep;
    return rep;
}



Token
Token::format (const char *fmt, ...)
{
    Token tok;
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
