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
#include <cstdlib>
#include <vector>
#include <map>

#include "export.h"
#include "oiioversion.h"
#include "tinyformat.h"
#include "string_view.h"

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

/// Construct a std::string in a printf-like fashion.  In other words,
/// something like:
///    std::string s = Strutil::format ("blah %d %g", (int)foo, (float)bar);
///
/// The printf argument list is fully typesafe via tinyformat; format
/// conceptually has the signature
///
/// std::string Strutil::format (const char *fmt, ...);
TINYFORMAT_WRAP_FORMAT (std::string, format, /**/,
    std::ostringstream msg;, msg, return msg.str();)

/// Return a std::string formatted from printf-like arguments.  Like the
/// real sprintf, this is not guaranteed type-safe and is not extensible
/// like format().  You would only want to use this instead of the safe
/// format() in rare situations where you really need to use obscure
/// printf features that aren't supported by tinyformat.
std::string OIIO_API format_raw (const char *fmt, ...)
                                         OPENIMAGEIO_PRINTF_ARGS(1,2);

/// Return a std::string formatted from printf-like arguments -- passed
/// already as a va_list.  Like vsprintf, this is not guaranteed
/// type-safe and is not extensible like format().
std::string OIIO_API vformat (const char *fmt, va_list ap)
                                         OPENIMAGEIO_PRINTF_ARGS(1,0);

/// Return a string expressing a number of bytes, in human readable form.
///  - memformat(153)           -> "153 B"
///  - memformat(15300)         -> "14.9 KB"
///  - memformat(15300000)      -> "14.6 MB"
///  - memformat(15300000000LL) -> "14.2 GB"
std::string OIIO_API memformat (long long bytes, int digits=1);

/// Return a string expressing an elapsed time, in human readable form.
/// e.g. "0:35.2"
std::string OIIO_API timeintervalformat (double secs, int digits=1);


/// Get a map with RESTful arguments extracted from the given string 'str'.
/// Add it into the 'result' argument (Warning: the 'result' argument may
/// be changed even if 'get_rest_arguments ()' return an error!).
/// Return true on success, false on error.
/// Acceptable forms:
///  - text?arg1=val1&arg2=val2...
///  - ?arg1=val1&arg2=val2...
/// Everything before question mark will be saved into the 'base' argument.
bool OIIO_API get_rest_arguments (const std::string &str, std::string &base,
                                   std::map<std::string, std::string> &result);

/// Take a string that may have embedded newlines, tabs, etc., and turn
/// those characters into escape sequences like \n, \t, \v, \b, \r, \f,
/// \a, \\, \".
std::string OIIO_API escape_chars (string_view unescaped);

/// Take a string that has embedded escape sequences (\\, \", \n, etc.)
/// and collapse them into the 'real' characters.
std::string OIIO_API unescape_chars (string_view escaped);

/// Word-wrap string 'src' to no more than columns width, splitting at
/// space characters.  It assumes that 'prefix' characters are already
/// printed, and furthermore, if it should need to wrap, it prefixes that
/// number of spaces in front of subsequent lines.  By illustration, 
/// wordwrap("0 1 2 3 4 5 6 7 8", 4, 10) should return:
/// "0 1 2\n    3 4 5\n    6 7 8"
std::string OIIO_API wordwrap (string_view src, int columns=80, int prefix=0);

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


/// Hash a string_view.  We use the Jenkins
/// one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function),
/// which seems to be a good speed/quality/requirements compromise.
inline size_t
strhash (string_view s)
{
    if (! s.length()) return 0;
    unsigned int h = 0;
    for (size_t i = 0;  i < s.length();  ++i) {
        h += (unsigned char)(s[i]);
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}



/// Case-insensitive comparison of strings.  For speed, this always uses
/// a static locale that doesn't require a mutex.
bool OIIO_API iequals (string_view a, string_view b);

/// Does 'a' start with the string 'b', with a case-sensitive comparison?
bool OIIO_API starts_with (string_view a, string_view b);

/// Does 'a' start with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a mutex.
bool OIIO_API istarts_with (string_view a, string_view b);

/// Does 'a' end with the string 'b', with a case-sensitive comparison?
bool OIIO_API ends_with (string_view a, string_view b);

/// Does 'a' end with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a mutex.
bool OIIO_API iends_with (string_view a, string_view b);

/// Does 'a' contain the string 'b' within it?
bool OIIO_API contains (string_view a, string_view b);

/// Does 'a' contain the string 'b' within it, using a case-insensitive
/// comparison?
bool OIIO_API icontains (string_view a, string_view b);

/// Convert to upper case, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void OIIO_API to_lower (std::string &a);

/// Convert to upper case, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void OIIO_API to_upper (std::string &a);

/// Return a reference to the section of str that has all consecutive
/// characters in chars removed from the beginning and ending.  If chars is
/// empty, it will be interpreted as " \t\n\r\f\v" (whitespace).
string_view OIIO_API strip (string_view str, string_view chars=string_view());

/// Fills the "result" list with the words in the string, using sep as
/// the delimiter string.  If maxsplit is > -1, at most maxsplit splits
/// are done. If sep is "", any whitespace string is a separator.
void OIIO_API split (string_view str, std::vector<string_view> &result,
                     string_view sep = string_view(), int maxsplit = -1);
void OIIO_API split (string_view str, std::vector<std::string> &result,
                     string_view sep = string_view(), int maxsplit = -1);

/// Join all the strings in 'seq' into one big string, separated by the
/// 'sep' string.
std::string OIIO_API join (const std::vector<string_view> &seq,
                           string_view sep = string_view());
std::string OIIO_API join (const std::vector<std::string> &seq,
                           string_view sep = string_view());



// Helper template to convert from generic type to string
template<typename T>
inline T from_string (string_view s) {
    return T(s); // Generic: assume there is an explicit converter
}
// Special case for int
template<> inline int from_string<int> (string_view s) {
    return strtol (s.c_str(), NULL, 10);
}
// Special case for float
template<> inline float from_string<float> (string_view s) {
    return (float)strtod (s.c_str(), NULL);
}



/// Given a string containing float values separated by a comma (or
/// optionally another separator), extract the individual values,
/// placing them into vals[] which is presumed to already contain
/// defaults.  If only a single value was in the list, replace all
/// elements of vals[] with the value. Otherwise, replace them in the
/// same order.  A missing value will simply not be replaced.
///
/// For example, if T=float, suppose initially, vals[] = {0, 1, 2}, then
///   "3.14"       results in vals[] = {3.14, 3.14, 3.14}
///   "3.14,,-2.0" results in vals[] = {3.14, 1, -2.0}
///
/// This can work for type T = int, float, or any type for that has
/// an explicit constructor from a std::string.
template<class T>
void extract_from_list_string (std::vector<T> &vals,
                               string_view list,
                               string_view sep = string_view(",",1))
{
    size_t nvals = vals.size();
    std::vector<string_view> valuestrings;
    Strutil::split (list, valuestrings, sep);
    for (size_t i = 0, e = valuestrings.size(); i < e; ++i) {
        if (valuestrings[i].size())
            vals[i] = from_string<T> (valuestrings[i]);
    }
    if (valuestrings.size() == 1) {
        vals.resize (1);
        vals.resize (nvals, vals[0]);
    }
}




/// C++ functor wrapper class for using strhash for unordered_map or
/// unordered_set.  The way this is used, in conjunction with
/// StringEqual, to build an efficient hash map for char*'s or
/// std::string's is as follows:
/// \code
///    boost::unordered_map <const char *, Key, Strutil::StringHash, Strutil::StringEqual>
/// \endcode
class StringHash {
public:
    size_t operator() (const char *s) const {
        return (size_t)Strutil::strhash(s);
    }
    size_t operator() (const std::string &s) const {
        return (size_t)Strutil::strhash(s.c_str());
    }
    size_t operator() (string_view s) const {
        return (size_t)Strutil::strhash(s);
    }
};



/// C++ functor class for comparing two char*'s or std::string's for
/// equality of their strings.
class StringEqual {
public:
    bool operator() (const char *a, const char *b) const {
        return strcmp (a, b) == 0;
    }
    bool operator() (string_view a, string_view b) const {
        return a == b;
    }
};


#ifdef _WIN32
/// Conversion functions between UTF-8 and UTF-16 for windows.
///
/// For historical reasons, the standard encoding for strings on windows is
/// UTF-16, whereas the unix world seems to have settled on UTF-8.  These two
/// encodings can be stored in std::string and std::wstring respectively, with
/// the caveat that they're both variable-width encodings, so not all the
/// standard string methods will make sense (for example std::string::size()
/// won't return the number of glyphs in a UTF-8 string, unless it happens to
/// be made up of only the 7-bit ASCII subset).
///
/// The standard windows API functions usually have two versions, a UTF-16
/// version with a 'W' suffix (using wchar_t* strings), and an ANSI version
/// with a 'A' suffix (using char* strings) which uses the current windows
/// code page to define the encoding.  (To make matters more confusing there is
/// also a further "TCHAR" version which is #defined to the UTF-16 or ANSI
/// version, depending on whether UNICODE is defined during compilation.
/// This is meant to make it possible to support compiling libraries in
/// either unicode or ansi mode from the same codebase.)
///
/// Using std::string as the string container (as in OIIO) implies that we
/// can't use UTF-16.  It also means we need a variable-width encoding to
/// represent characters in non-Latin alphabets in an unambiguous way; the
/// obvious candidate is UTF-8.  File paths in OIIO are considered to be
/// represented in UTF-8, and must be converted to UTF-16 before passing to
/// windows API file opening functions.

/// On the other hand, the encoding used for the ANSI versions of the windows
/// API is the current windows code page.  This is more compatible with the
/// default setup of the standard windows command prompt, and may be more
/// appropriate for error messages.

// Conversion to wide char
//
std::wstring OIIO_API utf8_to_utf16 (string_view utf8str);

// Conversion from wide char
//
std::string OIIO_API utf16_to_utf8(const std::wstring& utf16str);
#endif


/// Safe C string copy.  Basically strncpy but ensuring that there's a
/// terminating 0 character at the end of the resulting string.
OIIO_API char * safe_strcpy (char *dst, const char *src, size_t size);

inline char * safe_strcpy (char *dst, const std::string &src, size_t size) {
    return safe_strcpy (dst, src.length() ? src.c_str() : NULL, size);
}



/// Modify str to trim any whitespace (space, tab, linefeed, cr) from the
/// front.
void OIIO_API skip_whitespace (string_view &str);

/// If str's first character is c (or first non-whitespace char is c, if
/// skip_whitespace is true), return true and additionally modify str to
/// skip over that first character if eat is also true. Otherwise, if str
/// does not begin with character c, return false and don't modify str.
bool OIIO_API parse_char (string_view &str, char c,
                          bool skip_whitespace = true, bool eat=true);

/// Modify str to trim all characters up to (but not including) the first
/// occurrence of c, and return true if c was found or false if the whole
/// string was trimmed without ever finding c. But if eat is false, then
/// don't modify str, just return true if any c is found, false if no c
/// is found.
bool OIIO_API parse_until_char (string_view &str, char c, bool eat=true);

/// If str's first non-whitespace characters are the prefix, return true and
/// additionally modify str to skip over that prefix if eat is also true.
/// Otherwise, if str doesn't start with optional whitespace and the prefix,
/// return false and don't modify str.
bool OIIO_API parse_prefix (string_view &str, string_view prefix, bool eat=true);

/// If str's first non-whitespace characters form a valid integer, return
/// true, place the integer's value in val, and additionally modify str to
/// skip over the parsed integer if eat is also true. Otherwise, if no
/// integer is found at the beginning of str, return false and don't modify
/// val or str.
bool OIIO_API parse_int (string_view &str, int &val, bool eat=true);

/// If str's first non-whitespace characters form a valid float, return
/// true, place the float's value in val, and additionally modify str to
/// skip over the parsed float if eat is also true. Otherwise, if no float
/// is found at the beginning of str, return false and don't modify val or
/// str.
bool OIIO_API parse_float (string_view &str, float &val, bool eat=true);

/// If str's first non-whitespace characters form a valid string (either a
/// single word weparated by whitespace or anything inside a double-quoted
/// string (""), return true, place the string's value (not including
/// surrounding double quotes) in val, and additionally modify str to skip
/// over the parsed string if eat is also true. Otherwise, if no string is
/// found at the beginning of str, return false and don't modify val or str.
bool OIIO_API parse_string (string_view &str, string_view &val, bool eat=true);

/// Return the first "word" (set of contiguous alphabetical characters) in
/// str, and additionally modify str to skip over the parsed word if eat is
/// also true. Otherwise, if no word is found at the beginning of str,
/// return an empty string_view and don't modify str.
string_view OIIO_API parse_word (string_view &str, bool eat=true);

/// If str's first non-whitespace characters form a valid C-like identifier,
/// return the identifier, and additionally modify str to skip over the
/// parsed identifier if eat is also true. Otherwise, if no identifier is
/// found at the beginning of str, return an empty string_view and don't
/// modify str.
string_view OIIO_API parse_identifier (string_view &str, bool eat=true);

/// If str's first non-whitespace characters form a valid C-like identifier,
/// return the identifier, and additionally modify str to skip over the
/// parsed identifier if eat is also true. Otherwise, if no identifier is
/// found at the beginning of str, return an empty string_view and don't
/// modify str. The 'allowed' parameter may specify a additional characters
/// accepted that would not ordinarily be allowed in C identifiers, for
/// example, parse_identifier (blah, "$:") would allow "identifiers"
/// containing dollar signs and colons as well as the usual alphanumeric and
/// underscore characters.
string_view OIIO_API parse_identifier (string_view &str,
                                       string_view allowed, bool eat);

/// Return the characters until any character in sep is found, storing it in
/// str, and additionally modify str to skip over the parsed section if eat
/// is also true. Otherwise, if no word is found at the beginning of str,
/// return an empty string_view and don't modify str.
string_view OIIO_API parse_until (string_view &str,
                                  string_view sep=" \t\r\n", bool eat=true);


}  // namespace Strutil

}
OIIO_NAMESPACE_EXIT


#endif // OPENIMAGEIO_STRUTIL_H
