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


/////////////////////////////////////////////////////////////////////////
/// @file  strutil.h
///
/// @brief String-related utilities, all in namespace Strutil.
/////////////////////////////////////////////////////////////////////////



#ifndef OPENIMAGEIO_STRUTIL_H
#define OPENIMAGEIO_STRUTIL_H

#include <cstdarg>
#include <string>
#include <cstring>
#include <map>
#include <sys/types.h>   // to safely get off_t

#include "export.h"
#include "version.h"

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


OIIO_NAMESPACE_ENTER
{
/// @namespace Strutil
///
/// @brief     String-related utilities.
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
std::string DLLPUBLIC memformat (off_t bytes, int digits=1);

/// Return a string expressing an elapsed time, in human readable form.
/// e.g. "0:35.2"
std::string DLLPUBLIC timeintervalformat (double secs, int digits=1);


/// Get a map with RESTful arguments extracted from the given string 'str'.
/// Add it into the 'result' argument (Warning: the 'result' argument may
/// be changed even if 'get_rest_arguments ()' return an error!).
/// Return true on success, false on error.
/// Acceptable forms:
///  - text?arg1=val1&arg2=val2...
///  - ?arg1=val1&arg2=val2...
/// Everything before question mark will be saved into the 'base' argument.
bool DLLPUBLIC get_rest_arguments (const std::string &str, std::string &base,
                                   std::map<std::string, std::string> &result);

/// Take a string that may have embedded newlines, tabs, etc., and turn
/// those characters into escape sequences like \n, \t, \v, \b, \r, \f,
/// \a, \\, \".
std::string DLLPUBLIC escape_chars (const std::string &unescaped);

/// Take a string that has embedded escape sequences (\\, \", \n, etc.)
/// and collapse them into the 'real' characters.
std::string DLLPUBLIC unescape_chars (const std::string &escaped);

/// Word-wrap string 'src' to no more than columns width, splitting at
/// space characters.  It assumes that 'prefix' characters are already
/// printed, and furthermore, if it should need to wrap, it prefixes that
/// number of spaces in front of subsequent lines.  By illustration, 
/// wordwrap("0 1 2 3 4 5 6 7 8", 4, 10) should return:
/// "0 1 2\n    3 4 5\n    6 7 8"
std::string DLLPUBLIC wordwrap (std::string src, int columns=80, int prefix=0);

/// Hash a string without pre-known length.  We use the Jenkins
/// one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function),
/// which seems to be a good speed/quality/requirements compromise.
inline size_t
strhash (const char *s)
{
    if (! s) return 0;
    unsigned int h = 0;
    while (*s) {
        h += (unsigned char)(*s);
        h += h << 10;
        h ^= h >> 6;
        ++s;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}



/// Case-insensitive comparison of strings.  For speed, this always uses
/// a static locale that doesn't require a mutex.
bool DLLPUBLIC iequals (const std::string &a, const std::string &b);
bool DLLPUBLIC iequals (const char *a, const char *b);

/// Does 'a' start with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a mutex.
bool DLLPUBLIC istarts_with (const std::string &a, const std::string &b);
bool DLLPUBLIC istarts_with (const char *a, const char *b);

/// Does 'a' end with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a mutex.
bool DLLPUBLIC iends_with (const std::string &a, const std::string &b);
bool DLLPUBLIC iends_with (const char *a, const char *b);

/// Does 'a' end with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a mutex.
bool DLLPUBLIC iends_with (const std::string &a, const std::string &b);
bool DLLPUBLIC iends_with (const char *a, const char *b);

/// Convert to upper case, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void DLLPUBLIC to_lower (std::string &a);

/// Convert to upper case, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void DLLPUBLIC to_upper (std::string &a);



/// C++ functor wrapper class for using strhash for hash_map or hash_set.
/// The way this is used, in conjunction with StringEqual, to build an
/// efficient hash_map for char*'s or std::string's is as follows:
/// \code
///   #ifdef OIIO_HAVE_BOOST_UNORDERED_MAP
///    boost::unordered_map <const char *, Key, Strutil::StringHash, Strutil::StringEqual>
///   #else
///    hash_map <const char *, Key, Strutil::StringHash, Strutil::StringEqual>
///   #endif
/// \endcode
class StringHash {
public:
    size_t operator() (const char *s) const {
        return (size_t)Strutil::strhash(s);
    }
    size_t operator() (const std::string &s) const {
        return (size_t)Strutil::strhash(s.c_str());
    }
};



/// C++ functor class for comparing two char*'s or std::string's for
/// equality of their strings.
class StringEqual {
public:
    bool operator() (const char *a, const char *b) const {
        return strcmp (a, b) == 0;
    }
};


};  // namespace Strutil

}
OIIO_NAMESPACE_EXIT


#endif // OPENIMAGEIO_STRUTIL_H
