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


// A variety of string helper routines


#ifndef STRUTIL_H
#define STRUTIL_H

#include <cstdarg>
#include <string>


namespace Strutil {


/// Return a std::string formatted from printf arguments
///
std::string format (const char *fmt, ...);

/// Return a std::string formatted from printf arguments -- passed
/// already as a va_list.
std::string vformat (const char *fmt, va_list ap);


/// Beautiful little string hasher from Aho, Sethi, and Ullman's 1986
/// Dragon compiler book.  This depends on sizeof(unsigned int) == 4.
inline unsigned int
strhash (const char *s)
{
    if (!s) return 0;
    unsigned int h=0, g;
    while (*s) {
        h = (h<<4) + (unsigned char)(*s);
        if ((g = (h & 0xf0000000))) {
            h ^= g>>24;
            h ^= g;
        }
        ++s;
    }
    return h;
}



/// C++ functor wrapper class for using strhash for hash_map or hash_set.
///
class StringHash
#ifdef WINNT
    : public hash_compare<const char*>
#endif
{
public:
    size_t operator() (const char *s) const {
        return (size_t)Strutil::strhash(s);
    }
    bool operator() (const char *a, const char *b) {
        return strcmp (a, b) < 0;
    }
};




};  // namespace Strutil


#endif // STRUTIL_H
