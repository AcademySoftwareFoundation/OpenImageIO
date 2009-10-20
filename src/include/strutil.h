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


/// \file
///
/// A variety of string helper routines
///



#ifndef OPENIMAGEIO_STRUTIL_H
#define OPENIMAGEIO_STRUTIL_H

#include <cstdarg>
#include <string>
#include <cstring>

#ifdef _WIN32
#include "hash.h"
#endif

#include "export.h"

#ifndef OPENIMAGEIO_PRINTF_ARGS
#   ifndef __GNUC__
#       define __attribute__(x)
#   endif
    // Enable printf-like warnings with gcc by attaching
    // OPENIMAGEIO_PRINTF_ARGS to printf-like functions.  Eg:
    //
    // void foo (const char* fmt, ...) OPENIMAGEIO_PRINTF_ARGS(1,2);
    //
    // The arguments specify the positions of the format string and the the
    // beginning of the varargs parameter list respectively.
    //
    // For member functions with arguments like the example above, you need
    // OPENIMAGEIO_PRINTF_ARGS(2,3) instead.  (gcc includes the implicit this
    // pointer when it counts member function arguments.)
#   define OPENIMAGEIO_PRINTF_ARGS(fmtarg_pos, vararg_pos) \
        __attribute__ ((format (printf, fmtarg_pos, vararg_pos) ))
#endif

#ifdef OPENIMAGEIO_NAMESPACE
namespace OPENIMAGEIO_NAMESPACE {
#endif

namespace Strutil {


/// Return a std::string formatted from printf-like arguments.
///
std::string DLLPUBLIC format (const char *fmt, ...)
                                         OPENIMAGEIO_PRINTF_ARGS(1,2);

/// Return a std::string formatted from printf-like arguments -- passed
/// already as a va_list.
std::string DLLPUBLIC vformat (const char *fmt, va_list ap)
                                         OPENIMAGEIO_PRINTF_ARGS(1,0);

/// Return a string expressing a number of bytes, in human readable form.
///  - memformat(153)           -> "153 B"
///  - memformat(15300)         -> "14.9 KB"
///  - memformat(15300000)      -> "14.6 MB"
///  - memformat(15300000000LL) -> "14.2 GB"
std::string DLLPUBLIC memformat (off_t bytes, int digits=3);

/// Return a string expressing an elapsed time, in human readable form.
/// e.g. "0:35.2"
std::string DLLPUBLIC timeintervalformat (double secs, int digits=1);



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
/// The way this is used, in conjunction with StringEqual, to build an
/// efficient hash_map for char*'s is as follows:
/// \code
///   #ifdef _WIN32
///    hash_map <const char *, Key, Strutil::StringHash>
///   #else
///    hash_map <const char *, Key, Strutil::StringHash, Strutil::StringEqual>
///   #endif
/// \endcode
class StringHash
#ifdef _WIN32
    : public hash_compare<const char*>
#endif
{
public:
    size_t operator() (const char *s) const {
        return (size_t)Strutil::strhash(s);
    }
#ifdef _WIN32
    bool operator() (const char *a, const char *b) {
        return strcmp (a, b) < 0;
    }
#endif
};



/// C++ functor class for comparing two char*'s for equality of their
/// strings.
class StringEqual {
public:
    bool operator() (const char *a, const char *b) {
        return strcmp (a, b) == 0;
    }
};


};  // namespace Strutil


#ifdef OPENIMAGEIO_NAMESPACE
}; // end namespace OPENIMAGEIO_NAMESPACE
using namespace OPENIMAGEIO_NAMESPACE;
#endif

#endif // OPENIMAGEIO_STRUTIL_H
