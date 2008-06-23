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
#include "strhash.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing Token */
#include "token.h"
#undef DLL_EXPORT_PUBLIC



struct TokenRep {
    std::string str;     // String representation
    char chars[0];       // The characters
};




typedef hash_map <const char *, TokenRep *, Strutil::StringHash> TokenTable;
static TokenTable token_table;
static mutex token_mutex;



Token::Token (const char *s)
{
    // If we're be asked to construct an empty token, we can do that 
    // trivially, without even checking the table.
    if (s == NULL || *s == 0) {
        m_chars = NULL;
        return;
    }

    // Check the token table to see if this string already exists.  If so,
    // construct from its canonical representation.
    lock_guard guard(token_mutex);
    TokenTable::const_iterator found = token_table.find (s);
    if (found != token_table.end()) {
        m_chars = (found->second)->chars;
        return;
    }

    // This string is not yet in the token table.  Create a new entry.
    size_t len = sizeof (TokenRep) + strlen (s) + 1;
    TokenRep *rep = (TokenRep *) malloc (len);
    new (&rep->str) std::string (s);
    strcpy (rep->chars, s);
    token_table[rep->chars] = rep;
    m_chars = rep->chars;
}



Token::Token (const std::string &s)
{
    *this = Token (s.c_str());
}



const std::string &
Token::string () const
{
    const std::string *str = (const std::string *)m_chars;
    return str[-1];
}



Token
Token::format (const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    std::string buf = Strutil::vformat (fmt, ap);
    va_end (ap);
    return Token (buf.c_str());
}
