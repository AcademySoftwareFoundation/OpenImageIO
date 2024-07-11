// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

/////////////////////////////////////////////////////////////////////////
/// @file  strutil.h
///
/// @brief String-related utilities, all in namespace Strutil.
/////////////////////////////////////////////////////////////////////////



#pragma once

#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/export.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/string_view.h>

#include <OpenImageIO/detail/fmt.h>
#include <OpenImageIO/detail/farmhash.h>

// Allow client software to know if this version of OIIO has Strutil::sprintf
#define OIIO_HAS_SPRINTF 1

// Allow client software to know (and to determine, by setting this value
// before including this header) if this version of OIIO has Strutil::format
// behave like sprintf (OIIO_FORMAT_IS_FMT==0) or like python / {fmt} /
// C++20ish std::format (OIIO_FORMAT_IS_FMT==1).
#ifndef OIIO_FORMAT_IS_FMT
#    define OIIO_FORMAT_IS_FMT 0
#endif

// If OIIO_HIDE_FORMAT is defined, mark the old-style format functions as
// deprecated. (This is a debugging aid for downstream projects who want to
// root out any places where they might be using the old one).
#ifdef OIIO_HIDE_FORMAT
#    define OIIO_FORMAT_DEPRECATED OIIO_DEPRECATED("old style (printf-like) formatting version of this function is deprecated")
#else
#    define OIIO_FORMAT_DEPRECATED
#endif

// If OIIO_PRINT_IS_SYNCHRONIZED is not defined, assume unsynchronized.
#ifndef OIIO_PRINT_IS_SYNCHRONIZED
#    define OIIO_PRINT_IS_SYNCHRONIZED 0
#endif

// Allow client software to know that at this moment, the fmt-based string
// formatting is locale-independent. This was 0 in older versions when fmt
// was locale dependent.
#define OIIO_FMT_LOCALE_INDEPENDENT 1



OIIO_NAMESPACE_BEGIN
/// @namespace Strutil
///
/// @brief     String-related utilities.
namespace Strutil {

/// Output the string to the file/stream in a synchronized fashion, with an
/// internal mutex used to prevent threads from clobbering each other --
/// output strings coming from concurrent threads may be interleaved, but each
/// string is "atomic" and will never splice each other
/// character-by-character. If `flush` is true, the underlying stream will be
/// flushed after the string is output.
void OIIO_UTIL_API sync_output (FILE* file, string_view str,
                                bool flush = true);
void OIIO_UTIL_API sync_output (std::ostream& file, string_view str,
                                bool flush = true);


/// Construct a std::string in a printf-like fashion.  For example:
///
///    std::string s = Strutil::sprintf ("blah %d %g", (int)foo, (float)bar);
///
/// Uses the fmt library underneath, so it's fully type-safe, and
/// works with any types that understand stream output via '<<'.
/// The formatting of the string will always use the classic "C" locale
/// conventions (in particular, '.' as decimal separator for float values).
#ifdef OIIO_DOXYGEN
template<typename Str, typename... Args>
std::string sprintf(const Str& fmt, Args&&... args);
#else
using ::fmt::sprintf;
#endif



/// format() constructs formatted strings. Note that this is in transition!
///
/// Strutil::old::format() uses printf conventions and matches format() used
/// in OIIO 1.x. It is equivalent to Strutil::sprintf().
///
///    std::string s = Strutil::old::sprintf ("blah %d %g", (int)foo, (float)bar);
///
/// Strutil::fmt::format() uses "Python" conventions, in the style of string
/// formatting used by C++20 std::format and implemented today in the {fmt}
/// package (https://github.com/fmtlib/fmt). For example:
///
///    std::string s = Strutil::format ("blah {}  {}", (int)foo, (float)bar);
///
/// Straight-up Strutil::format is today aliased to old::format for the sake
/// of back-compatibility, but will someday be switched to fmt::format.
///
/// Recommended strategy for users:
/// * If you want printf conventions, switch to Strutil::sprintf().
/// * If you want to use the python conventions prior to the big switch,
///   use Strutil::fmt::format() explicitly (but see the caveat below).
/// * Use of unspecified Strutil::format() is, for back compatibility,
///   currently equivalent to sprintf, but beware that some point it will
///   switch to the future-standard formatting rules.
///

namespace fmt {
template<typename Str, typename... Args>
OIIO_NODISCARD
inline std::string format(const Str& fmt, Args&&... args)
{
    return ::fmt::vformat(fmt, ::fmt::make_format_args(args...));
}
} // namespace fmt

namespace old {
template<typename... Args>
OIIO_FORMAT_DEPRECATED
inline std::string format (const char* fmt, const Args&... args)
{
    return Strutil::sprintf (fmt, args...);
}

// DEPRECATED(2.0) string_view version. Phasing this out because
// std::string_view won't have a c_str() method.
template<typename... Args>
OIIO_FORMAT_DEPRECATED
inline std::string format (string_view fmt, const Args&... args)
{
    return format ({ fmt.data(), fmt.size() }, args...);
}
} // namespace old


// Choose whether Strutil::format is the old or new kind based on
// OIIO_FORMAT_IS_FMT.
#if OIIO_FORMAT_IS_FMT
using fmt::format;
#else
using old::format;
#endif



/// Strutil::printf (fmt, ...)
/// Strutil::fprintf (FILE*, fmt, ...)
/// Strutil::fprintf (ostream& fmt, ...)
///
/// Output formatted strings to stdout, a FILE*, or a stream, respectively.
/// All use printf-like formatting rules, are type-safe, are thread-safe
/// (the outputs are "atomic", at least versus other calls to
/// Strutil::*printf), and automatically flush their outputs. They are all
/// locale-independent (forcing classic "C" locale).

template<typename... Args>
inline void printf (const char* fmt, const Args&... args)
{
    sync_output (stdout, Strutil::sprintf(fmt, args...));
}

template<typename... Args>
inline void fprintf (FILE *file, const char* fmt, const Args&... args)
{
    sync_output (file, Strutil::sprintf(fmt, args...));
}

template<typename... Args>
inline void fprintf (std::ostream &file, const char* fmt, const Args&... args)
{
    sync_output (file, Strutil::sprintf(fmt, args...));
}



namespace sync {

/// Strutil::sync::print (fmt, ...)
/// Strutil::sync::print (FILE*, fmt, ...)
/// Strutil::sync::print (ostream& fmt, ...)
///
/// Output formatted strings to stdout, a FILE*, or a stream, using a
/// "Python-like/std::format" type-safe formatting description. Results are
/// locale-independent (use `{:n}` locale-aware formatting).
///
/// Output is fully thread-safe (the outputs are "atomic" to the file or
/// stream), and if the stream is buffered, it is flushed after the output).

#if FMT_VERSION >= 70000
template<typename Str, typename... Args>
inline void print (FILE *file, const Str& fmt, Args&&... args)
{
    sync_output (file, ::fmt::vformat(fmt, ::fmt::make_format_args(args...)));
}

template<typename Str, typename... Args>
inline void print (const Str& fmt, Args&&... args)
{
    sync_output(stdout, ::fmt::vformat(fmt, ::fmt::make_format_args(args...)));
}

template<typename Str, typename... Args>
inline void print (std::ostream &file, const Str& fmt, Args&&... args)
{
    sync_output (file, ::fmt::vformat(fmt, ::fmt::make_format_args(args...)));
}

#else

template<typename... Args>
inline void print (FILE *file, const char* fmt, Args&&... args)
{
    sync_output (file, ::fmt::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void print (const char* fmt, Args&&... args)
{
    print(stdout, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void print (std::ostream &file, const char* fmt, Args&&... args)
{
    sync_output (file, ::fmt::format(fmt, std::forward<Args>(args)...));
}
#endif
} // namespace sync



/// Strutil::print (fmt, ...)
/// Strutil::print (FILE*, fmt, ...)
/// Strutil::print (ostream& fmt, ...)
///
/// Output formatted strings to stdout, a FILE*, or a stream, using a
/// "Python-like/std::format" type-safe formatting description. Results are
/// locale-independent (use `{:n}` locale-aware formatting).
///
/// As wrappers around fmt::print (https://fmt.dev), these appear currently to
/// be thread-safe and "atomic" (multiple concurrent calls using the same
/// stream should not interleave their characters within the output strings).
/// But we can't 100% guarantee that on all platforms and all versions of fmt.
/// See the `Strutil::sync::print()` for similar functionality that is
/// guaranteed to be thread-safe and atomic, as well as flush their outputs
/// fully after each call (but are, as expected, slower). If
/// `OIIO_PRINT_IS_SYNCHRONIZED` is defined to be 1 prior to including
/// `<OpenImageIO/strutil.h>`, then print will be also be thread-safe and
/// atomic, like `sync::print`.
#ifdef OIIO_DOXYGEN
template<typename... Args>
void print (const char* fmt, const Args&... args);

template<typename... Args>
void print (FILE *file, const char* fmt, const Args&... args);

template<typename... Args>
void print (std::ostream &file, const char* fmt, const Args&... args);

#elif FMT_VERSION >= 70000 && !OIIO_PRINT_IS_SYNCHRONIZED
using ::fmt::print;
#else
using sync::print;
#endif


namespace pvt {
OIIO_UTIL_API void debug(string_view str);
}

/// `debug(format, ...)` prints debugging message when attribute "debug" is
/// nonzero, which it is by default for DEBUG compiles or when the environment
/// variable OPENIMAGEIO_DEBUG is set. This is preferred to raw output to
/// stderr for debugging statements.
template<typename... Args>
void debug(const char* fmt, Args&&... args)
{
    pvt::debug(fmt::format(fmt, std::forward<Args>(args)...));
}



/// Return a std::string formatted from printf-like arguments -- passed
/// already as a va_list.  This is not guaranteed type-safe and is not
/// extensible like format(). Use with caution!
std::string OIIO_UTIL_API vsprintf (const char *fmt, va_list ap)
#if defined(__GNUC__) && !defined(__CUDACC__)
    __attribute__ ((format (printf, 1, 0) ))
#endif
    ;

/// Return a std::string formatted like Strutil::format, but passed
/// already as a va_list.  This is not guaranteed type-safe and is not
/// extensible like format(). Use with caution!
OIIO_DEPRECATED("use `vsprintf` instead")
std::string OIIO_UTIL_API vformat (const char *fmt, va_list ap)
#if defined(__GNUC__) && !defined(__CUDACC__)
    __attribute__ ((format (printf, 1, 0) ))
#endif
    ;

/// Return a string expressing a number of bytes, in human readable form.
///  - memformat(153)           -> "153 B"
///  - memformat(15300)         -> "14.9 KB"
///  - memformat(15300000)      -> "14.6 MB"
///  - memformat(15300000000LL) -> "14.2 GB"
std::string OIIO_UTIL_API memformat (long long bytes, int digits=1);

/// Return a string expressing an elapsed time, in human readable form.
/// e.g. "0:35.2"
std::string OIIO_UTIL_API timeintervalformat (double secs, int digits=1);


/// Get a map with RESTful arguments extracted from the given string 'str'.
/// Add it into the 'result' argument (Warning: the 'result' argument may
/// be changed even if 'get_rest_arguments ()' return an error!).
/// Return true on success, false on error.
/// Acceptable forms:
///  - text?arg1=val1&arg2=val2...
///  - ?arg1=val1&arg2=val2...
/// Everything before question mark will be saved into the 'base' argument.
bool OIIO_UTIL_API get_rest_arguments (const std::string &str, std::string &base,
                                   std::map<std::string, std::string> &result);

/// Take a string that may have embedded newlines, tabs, etc., and turn
/// those characters into escape sequences like `\n`, `\t`, `\v`, `\b`,
/// `\r`, `\f`, `\a`, `\\`, `\"`.
std::string OIIO_UTIL_API escape_chars (string_view unescaped);

/// Take a string that has embedded escape sequences (`\\`, `\"`, `\n`,
/// etc.) and collapse them into the 'real' characters.
std::string OIIO_UTIL_API unescape_chars (string_view escaped);

/// Word-wrap string `src` to no more than `columns` width, starting with an
/// assumed position of `prefix` on the first line and intending by `prefix`
/// blanks before all lines other than the first.
///
/// Words may be split AT any characters in `sep` or immediately AFTER any
/// characters in `presep`. After the break, any extra `sep` characters will
/// be deleted.
///
/// By illustration,
///     wordwrap("0 1 2 3 4 5 6 7 8", 10, 4)
/// should return:
///     "0 1 2\n    3 4 5\n    6 7 8"
std::string OIIO_UTIL_API wordwrap (string_view src, int columns = 80,
                               int prefix = 0, string_view sep = " ",
                               string_view presep = "");


/// Our favorite "string" hash of a length of bytes. Currently, it is just
/// a wrapper for an inlined, constexpr, Cuda-safe farmhash.
/// It returns a size_t, so will be a 64 bit hash on 64-bit platforms, but
/// a 32 bit hash on 32-bit platforms.
inline constexpr size_t
strhash(size_t len, const char *s)
{
    return size_t(OIIO::farmhash::inlined::Hash64(s, len));
}


/// A guaranteed 64-bit string hash on all platforms.
inline constexpr uint64_t
strhash64(size_t len, const char *s)
{
    return OIIO::farmhash::inlined::Hash64(s, len);
}


/// Hash a string_view. This is OIIO's default favorite string hasher.
/// Currently, it uses farmhash, is constexpr (for C++14), and works in Cuda.
/// This is rigged, though, so that empty strings hash always hash to 0 (that
/// isn't what a raw farmhash would give you, but it's a useful property,
/// especially for trivial initialization). It returns a size_t, so will be a
/// 64 bit hash on 64-bit platforms, but a 32 bit hash on 32-bit platforms.
inline constexpr size_t
strhash(string_view s)
{
    return s.length() ? strhash(s.length(), s.data()) : 0;
}



/// Hash a string_view, guaranteed 64 bits (even on 32 bit platforms).
inline constexpr uint64_t
strhash64(string_view s)
{
    return s.length() ? strhash64(s.length(), s.data()) : 0;
}



/// Case-insensitive comparison of strings.  For speed, this always uses a
/// static locale that doesn't require a mutex. Caveat: the case-sensivive
/// `==` of string_view's is about 20x faster than this case-insensitive
/// function.
bool OIIO_UTIL_API iequals (string_view a, string_view b);

/// Case-insensitive ordered comparison of strings.  For speed, this always
/// uses a static locale that doesn't require a mutex.
bool OIIO_UTIL_API iless (string_view a, string_view b);

/// Does 'a' start with the string 'b', with a case-sensitive comparison?
bool OIIO_UTIL_API starts_with (string_view a, string_view b);

/// Does 'a' start with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a
/// mutex. Caveat: the case-sensivive starts_with() is about 20x faster than
/// this case-insensitive function.
bool OIIO_UTIL_API istarts_with (string_view a, string_view b);

/// Does 'a' end with the string 'b', with a case-sensitive comparison?
bool OIIO_UTIL_API ends_with (string_view a, string_view b);

/// Does 'a' end with the string 'b', with a case-insensitive comparison?
/// For speed, this always uses a static locale that doesn't require a
/// mutex. Caveat: the case-sensivive ends_with() is about 20x faster than
/// this case-insensitive function.
bool OIIO_UTIL_API iends_with (string_view a, string_view b);

/// Return the position of the first occurrence of `b` within `a`, or
/// std::npos if not found.
size_t OIIO_UTIL_API find(string_view a, string_view b);

/// Return the position of the first occurrence of `b` within `a`, with a
/// case-insensitive comparison, or std::npos if not found. Caveat: the
/// case-sensivive find() is about 20x faster than this case-insensitive
/// function.
size_t OIIO_UTIL_API ifind(string_view a, string_view b);

/// Return the position of the last occurrence of `b` within `a`, or npos if
/// not found.
size_t OIIO_UTIL_API rfind(string_view a, string_view b);

/// Return the position of the last occurrence of `b` within `a`, with a
/// case-insensitive comparison, or npos if not found. Caveat: the
/// case-sensivive rfind() is about 20x faster than this case-insensitive
/// function.
size_t OIIO_UTIL_API irfind(string_view a, string_view b);

/// Does 'a' contain the string 'b' within it?
bool OIIO_UTIL_API contains (string_view a, string_view b);

/// Does 'a' contain the string 'b' within it, using a case-insensitive
/// comparison? Caveat: the case-sensivive contains() is about 20x faster
/// than this case-insensitive function.
bool OIIO_UTIL_API icontains (string_view a, string_view b);

/// Does 'a' contain the string 'b' within it? But start looking at the end!
/// This can be a bit faster than contains() if you think that the substring
/// `b` will tend to be close to the end of `a`.
inline bool rcontains (string_view a, string_view b) {
    return rfind(a, b) != string_view::npos;
}

/// Does 'a' contain the string 'b' within it? But start looking at the end!
/// This can be a bit faster than contains() if you think that the substring
/// `b` will tend to be close to the end of `a`. Caveat: the case-sensivive
/// rcontains() is about 20x faster than this case-insensitive function.
inline bool ircontains (string_view a, string_view b) {
    return irfind(a, b) != string_view::npos;
}

/// Does 'a' contain any of the characters within `set`?
bool OIIO_UTIL_API contains_any_char (string_view a, string_view set);

/// Convert to upper case in place, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void OIIO_UTIL_API to_lower (std::string &a);

/// Convert to upper case in place, faster than std::toupper because we use
/// a static locale that doesn't require a mutex lock.
void OIIO_UTIL_API to_upper (std::string &a);

/// Return an all-upper case version of `a` (locale-independent).
inline std::string lower (string_view a) {
    std::string result(a);
    to_lower(result);
    return result;
}

/// Return an all-upper case version of `a` (locale-independent).
inline std::string upper (string_view a) {
    std::string result(a);
    to_upper(result);
    return result;
}



/// Return a reference to the section of str that has all consecutive
/// characters in chars removed from the beginning and ending.  If chars is
/// empty, it will be interpreted as " \t\n\r\f\v" (whitespace).
string_view OIIO_UTIL_API strip (string_view str, string_view chars=string_view());

/// Return a reference to the section of str that has all consecutive
/// characters in chars removed from the beginning (left side).  If chars is
/// empty, it will be interpreted as " \t\n\r\f\v" (whitespace).
string_view OIIO_UTIL_API lstrip (string_view str, string_view chars=string_view());

/// Return a reference to the section of str that has all consecutive
/// characters in chars removed from the ending (right side).  If chars is
/// empty, it will be interpreted as " \t\n\r\f\v" (whitespace).
string_view OIIO_UTIL_API rstrip (string_view str, string_view chars=string_view());


/// Fills the "result" list with the words in the string, using sep as
/// the delimiter string.  If `maxsplit` is > -1, the string will be split
/// into at most `maxsplit` pieces (a negative value will impose no
/// maximum). If sep is "", any whitespace string is a separator.  If the
/// source `str` is empty, there will be zero pieces.
void OIIO_UTIL_API split (string_view str, std::vector<string_view> &result,
                     string_view sep = string_view(), int maxsplit = -1);
void OIIO_UTIL_API split (string_view str, std::vector<std::string> &result,
                     string_view sep = string_view(), int maxsplit = -1);

/// Split the contents of `str` using `sep` as the delimiter string. If
/// `sep` is "", any whitespace string is a separator. If `maxsplit > -1`,
/// at most `maxsplit` split fragments will be produced (for example,
/// maxsplit=2 will split at only the first separator, yielding at most two
/// fragments). The result is returned as a vector of std::string (for
/// `splits()`) or a vector of string_view (for `splitsv()`). If the source
/// `str` is empty, there will be zero pieces.
OIIO_UTIL_API std::vector<std::string>
splits (string_view str, string_view sep = "", int maxsplit = -1);
OIIO_UTIL_API std::vector<string_view>
splitsv (string_view str, string_view sep = "", int maxsplit = -1);

/// Join all the strings in 'seq' into one big string, separated by the
/// 'sep' string. The Sequence can be any iterable collection of items that
/// are able to convert to string via stream output. Examples include:
/// std::vector<string_view>, std::vector<std::string>, std::set<ustring>,
/// std::vector<int>, etc.
template<class Sequence>
std::string join (const Sequence& seq, string_view sep="")
{
    std::ostringstream out;
    out.imbue(std::locale::classic());  // Force "C" locale
    bool first = true;
    for (auto&& s : seq) {
        if (! first && sep.size())
            out << sep;
        out << s;
        first = false;
    }
    return out.str();
}

/// Join all the strings in 'seq' into one big string, separated by the
/// 'sep' string. The Sequence can be any iterable collection of items that
/// are able to convert to string via stream output. Examples include:
/// std::vector<string_view>, std::vector<std::string>, std::set<ustring>,
/// std::vector<int>, etc. Values will be rendered into the string in a
/// locale-independent manner (i.e., '.' for decimal in floats). If the
/// optional `len` is nonzero, exactly that number of elements will be
/// output (truncating or default-value-padding the sequence).
template<class Sequence>
std::string join (const Sequence& seq, string_view sep /*= ""*/, size_t len)
{
    using E = typename std::remove_reference<decltype(*std::begin(seq))>::type;
    std::ostringstream out;
    out.imbue(std::locale::classic());  // Force "C" locale
    bool first = true;
    for (auto&& s : seq) {
        if (! first)
            out << sep;
        out << s;
        first = false;
        if (len && (--len == 0))
            break;
    }
    while (len--) {
        if (! first)
            out << sep;
        out << E();
        first = false;
    }
    return out.str();
}

/// Concatenate two strings, returning a std::string, implemented carefully
/// to not perform any redundant copies or allocations.
std::string OIIO_UTIL_API concat(string_view s, string_view t);

/// Repeat a string formed by concatenating str n times.
std::string OIIO_UTIL_API repeat (string_view str, int n);

/// Replace a pattern inside a string and return the result. If global is
/// true, replace all instances of the pattern, otherwise just the first.
std::string OIIO_UTIL_API replace (string_view str, string_view pattern,
                              string_view replacement, bool global=false);


/// strtod/strtof equivalents that are "locale-independent", always using
/// '.' as the decimal separator. This should be preferred for I/O and other
/// situations where you want the same standard formatting regardless of
/// locale.
float OIIO_UTIL_API strtof (const char *nptr, char **endptr = nullptr) noexcept;
double OIIO_UTIL_API strtod (const char *nptr, char **endptr = nullptr) noexcept;


// stoi() returns the int conversion of text from a string.
// No exceptions or errors -- parsing errors just return 0, over/underflow
// gets clamped to int range. No locale consideration.
OIIO_UTIL_API int stoi (string_view s, size_t* pos=0, int base=10);

// stoui() returns the unsigned int conversion of text from a string.
// No exceptions or errors -- parsing errors just return 0. Negative
// values are cast, overflow is clamped. No locale considerations.
OIIO_UTIL_API unsigned int stoui (string_view s, size_t* pos=0, int base=10);

/// stof() returns the float conversion of text from several string types.
/// No exceptions or errors -- parsing errors just return 0.0. These always
/// use '.' for the decimal mark (versus atof and std::strtof, which are
/// locale-dependent).
OIIO_UTIL_API float stof (string_view s, size_t* pos=0);
#define OIIO_STRUTIL_HAS_STOF 1  /* be able to test this */

// Temporary fix: allow separate std::string and char* versions, to avoid
// string_view allocation on some platforms. This will be deprecated once
// we can count on all supported compilers using short string optimization.
OIIO_UTIL_API float stof (const std::string& s, size_t* pos=0);
OIIO_UTIL_API float stof (const char* s, size_t* pos=0);
// N.B. For users of ustring, there's a stof(ustring) defined in ustring.h.

OIIO_UTIL_API double stod (string_view s, size_t* pos=0);
OIIO_UTIL_API double stod (const std::string& s, size_t* pos=0);
OIIO_UTIL_API double stod (const char* s, size_t* pos=0);



/// Return true if the string is exactly (other than leading and trailing
/// whitespace) a valid int.
OIIO_UTIL_API bool string_is_int (string_view s);

/// Return true if the string is exactly (other than leading or trailing
/// whitespace) a valid float. This operations in a locale-independent
/// manner, i.e., it assumes '.' as the decimal mark.
OIIO_UTIL_API bool string_is_float (string_view s);



// Helper template to convert from generic type to string. Used when you
// want stoX but you're in a template. Rigged to use "C" locale.
template<typename T>
inline T from_string (string_view s) {
    return T(s); // Generic: assume there is an explicit converter
}

#ifndef OIIO_DOXYGEN
// Special case for int
template<> inline int from_string<int> (string_view s) {
    return Strutil::stoi(s);
}
// Special case for uint
template<> inline unsigned int from_string<unsigned int> (string_view s) {
    return Strutil::stoui(s);
}
// Special case for float -- note that by using Strutil::strtof, this
// always treats '.' as the decimal mark.
template<> inline float from_string<float> (string_view s) {
    return Strutil::stof(s);
}
// Special case for double -- note that by using Strutil::strtof, this
// always treats '.' as the decimal mark.
template<> inline double from_string<double> (string_view s) {
    return Strutil::stod(s);
}

template<> inline int64_t from_string<int64_t>(string_view s) {
    // For conversion of string_view to unsigned int, fall back on strtoll.
    auto r = strtoll(std::string(s).c_str(), nullptr, 10);
    return static_cast<int64_t>(r);
}

template<> inline uint64_t from_string<uint64_t>(string_view s) {
    // For conversion of string_view to unsigned int, fall back on strtoull.
    auto r = strtoull(std::string(s).c_str(), nullptr, 10);
    return static_cast<uint64_t>(r);
}
#endif



/// Template function to convert any type to a string. The default
/// implementation is just to use sprintf or fmt::to_string. The template
/// can be overloaded if there is a better method for particular types.
template<typename T>
inline std::string to_string (const T& value) {
    return ::fmt::to_string(value);
}

// Some special pass-through cases
inline std::string to_string (const std::string& value) { return value; }
inline std::string to_string (string_view value) { return value; }
inline std::string to_string (const char* value) { return value; }



// Helper template to test if a string is a generic type. Used instead of
// string_is_X, but when you're inside templated code.
template<typename T>
inline bool string_is (string_view /*s*/) {
    return false; // Generic: assume there is an explicit specialization
}
// Special case for int
template <> inline bool string_is<int> (string_view s) {
    return string_is_int (s);
}
// Special case for float. Note that by using Strutil::stof, this always
// treats '.' as the decimal character.
template <> inline bool string_is<float> (string_view s) {
    return string_is_float (s);
}




/// Given a string containing values separated by a comma (or optionally
/// another separator), extract the individual values, placing them into
/// vals[] which is presumed to already contain defaults.  If only a single
/// value was in the list, replace all elements of vals[] with the value.
/// Otherwise, replace them in the same order.  A missing value will simply
/// not be replaced. Return the number of values found in the list
/// (including blank or malformed ones). If the vals vector was empty
/// initially, grow it as necessary.
///
/// For example, if T=float, suppose initially, vals[] = {0, 1, 2}, then
///   "3.14"       results in vals[] = {3.14, 3.14, 3.14}
///   "3.14,,-2.0" results in vals[] = {3.14, 1, -2.0}
///
/// This can work for type T = int, float, or any type for that has
/// an explicit constructor from a std::string.
template<class T, class Allocator>
int extract_from_list_string (std::vector<T, Allocator> &vals,
                              string_view list,
                              string_view sep = ",")
{
    size_t nvals = vals.size();
    std::vector<string_view> valuestrings;
    Strutil::split (list, valuestrings, sep);
    for (size_t i = 0, e = valuestrings.size(); i < e; ++i) {
        T v = from_string<T> (valuestrings[i]);
        if (nvals == 0)
            vals.push_back (v);
        else if (valuestrings[i].size()) {
            if (vals.size() > i)  // don't replace non-existent entries
                vals[i] = from_string<T> (valuestrings[i]);
        }
        /* Otherwise, empty space between commas, so leave default alone */
    }
    if (valuestrings.size() == 1 && nvals > 0) {
        vals.resize (1);
        vals.resize (nvals, vals[0]);
    }
    return list.size() ? (int) valuestrings.size() : 0;
}


/// Given a string containing values separated by a comma (or optionally
/// another separator), extract the individual values, returning them as a
/// std::vector<T>. The vector will be initialized with `nvals` elements
/// with default value `val`. If only a single value was in the list,
/// replace all elements of vals[] with the value. Otherwise, replace them
/// in the same order.  A missing value will simply not be replaced and
/// will retain the initialized default value. If the string contains more
/// then `nvals` values, they will append to grow the vector.
///
/// For example, if T=float,
///   extract_from_list_string ("", 3, 42.0f)
///       --> {42.0, 42.0, 42.0}
///   extract_from_list_string ("3.14", 3, 42.0f)
///       --> {3.14, 3.14, 3.14}
///   extract_from_list_string ("3.14,,-2.0", 3, 42.0f)
///       --> {3.14, 42.0, -2.0}
///   extract_from_list_string ("1,2,3,4", 3, 42.0f)
///       --> {1.0, 2.0, 3.0, 4.0}
///
/// This can work for type T = int, float, or any type for that has
/// an explicit constructor from a std::string.
template<class T>
std::vector<T>
extract_from_list_string (string_view list, size_t nvals=0, T val=T(),
                          string_view sep = ",")
{
    std::vector<T> vals (nvals, val);
    extract_from_list_string (vals, list, sep);
    return vals;
}



/// Scan a string for date and time information. Return true upon success,
/// false if the string did not appear to contain a valid date/time. If, after
/// parsing a valid date/time (including out of range values), `str` contains
/// more characters after that, it is not considered a failure.
///
/// Valid date/time formats include:
///   * YYYY-MM-DD HH:MM:SS
///   * YYYY:MM:DD HH:MM:SS
///   * YYYY/MM/DD HH:MM:SS
OIIO_UTIL_API bool
scan_datetime(string_view str, int& year, int& month, int& day,
              int& hour, int& min, int& sec);



/// C++ functor wrapper class for using strhash for unordered_map or
/// unordered_set.  The way this is used, in conjunction with
/// StringEqual, to build an efficient hash map for char*'s or
/// std::string's is as follows:
/// \code
///    unordered_map <const char *, Key, Strutil::StringHash, Strutil::StringEqual>
/// \endcode
class StringHash {
public:
    size_t operator() (string_view s) const {
        return (size_t)Strutil::strhash(s);
    }
};



/// C++ functor for comparing two strings for equality of their characters.
struct OIIO_UTIL_API StringEqual {
    bool operator() (const char *a, const char *b) const noexcept { return strcmp (a, b) == 0; }
    bool operator() (string_view a, string_view b) const noexcept { return a == b; }
};


/// C++ functor for comparing two strings for equality of their characters
/// in a case-insensitive and locale-insensitive way.
struct OIIO_UTIL_API StringIEqual {
    bool operator() (const char *a, const char *b) const noexcept;
    bool operator() (string_view a, string_view b) const noexcept { return iequals (a, b); }
};


/// C++ functor for comparing the ordering of two strings.
struct OIIO_UTIL_API StringLess {
    bool operator() (const char *a, const char *b) const noexcept { return strcmp (a, b) < 0; }
    bool operator() (string_view a, string_view b) const noexcept { return a < b; }
};


/// C++ functor for comparing the ordering of two strings in a
/// case-insensitive and locale-insensitive way.
struct OIIO_UTIL_API StringILess {
    bool operator() (const char *a, const char *b) const noexcept;
    bool operator() (string_view a, string_view b) const noexcept { return a < b; }
};



/// Conversion of normal char-based strings (presumed to be UTF-8 encoding)
/// to a UTF-16 encoded wide char string, wstring.
std::wstring OIIO_UTIL_API utf8_to_utf16wstring (string_view utf8str) noexcept;

#if OPENIMAGEIO_VERSION < 30000
/// Old name for utf8_to_utf16wstring. Will be deprecated for OIIO 2.5+ and
/// removed for OIIO 3.0. Use utf8_to_utf16wstring which is more clear that
/// this particular conversion from utf8 to utf16 returns a std::wstring and
/// not a std::u16string.
#if OPENIMAGEIO_VERSION >= 20500
OIIO_DEPRECATED("Use utf8_to_utf16wstring instead")
#endif
std::wstring OIIO_UTIL_API utf8_to_utf16 (string_view utf8str) noexcept;
#endif

/// Conversion from wstring UTF-16 to a UTF-8 std::string.  This is the
/// standard way to convert from Windows wide character strings used for
/// filenames into the UTF-8 strings OIIO expects for filenames when passed to
/// functions like ImageInput::open().
std::string OIIO_UTIL_API utf16_to_utf8(const std::wstring& utf16str) noexcept;

/// Conversion from UTF-16 std::u16string to a UTF-8 std::string.
std::string OIIO_UTIL_API utf16_to_utf8(const std::u16string& utf16str) noexcept;



/// Copy at most size characters (including terminating 0 character) from
/// src into dst[], filling any remaining characters with 0 values. Returns
/// dst. Note that this behavior is identical to strncpy, except that it
/// guarantees that there will be a terminating 0 character.
OIIO_UTIL_API char * safe_strcpy (char *dst, string_view src, size_t size) noexcept;


/// Append `src` to the end of the C-string buffer dst, plus a terminating
/// null, assuming that dst can hold at most `size` characters (including
/// terminating 0 character). Returns dst. Note that this behavior is similar
/// to strcat, but guarantees that the resulting string fits into `size` bytes
/// and is null-terminated.
OIIO_UTIL_API char* safe_strcat(char *dst, string_view src, size_t size) noexcept;


/// Return the length of null-terminated string `str`, up to maximum `size`.
/// If str is nullptr, return 0. This is equivalent to C11 strnlen_s.
OIIO_UTIL_API size_t safe_strlen(const char* str, size_t size) noexcept;


/// Return a string_view that is a substring of the given C string, ending at
/// the first null character or after size characters, whichever comes first.
inline string_view safe_string_view(const char* str, size_t size) noexcept
{
    return string_view(str, safe_strlen(str, size));
}

/// Return a std::string that is a substring of the given C string, ending at
/// the first null character or after size characters, whichever comes first.
inline std::string safe_string(const char* str, size_t size)
{
    return safe_string_view(str, size);
}



/// Is the character a whitespace character (space, linefeed, tab, carrage
/// return)? Note: this is safer than C isspace(), which has undefined
/// behavior for negative char values. Also note that it differs from C
/// isspace by not detecting form feed or vertical tab, because who cares.
inline bool isspace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

/// Modify str to trim any leading whitespace (space, tab, linefeed, cr)
/// from the front.
void OIIO_UTIL_API skip_whitespace (string_view &str) noexcept;

/// Modify str to trim any trailing whitespace (space, tab, linefeed, cr)
/// from the back.
void OIIO_UTIL_API remove_trailing_whitespace (string_view &str) noexcept;

/// Modify str to trim any whitespace (space, tab, linefeed, cr) from both
/// the front and back.
inline void trim_whitespace (string_view &str) noexcept {
    skip_whitespace(str);
    remove_trailing_whitespace(str);
}

/// Return the portion of str that is trimmed of any whitespace (space, tab,
/// linefeed, cr) from both the front and back.
inline string_view trimmed_whitespace (string_view str) noexcept {
    skip_whitespace(str);
    remove_trailing_whitespace(str);
    return str;
}

/// If str's first character is c (or first non-whitespace char is c, if
/// skip_whitespace is true), return true and additionally modify str to
/// skip over that first character if eat is also true. Otherwise, if str
/// does not begin with character c, return false and don't modify str.
bool OIIO_UTIL_API parse_char (string_view &str, char c,
                          bool skip_whitespace = true, bool eat=true) noexcept;

/// Modify str to trim all characters up to (but not including) the first
/// occurrence of c, and return true if c was found or false if the whole
/// string was trimmed without ever finding c. But if eat is false, then
/// don't modify str, just return true if any c is found, false if no c
/// is found.
bool OIIO_UTIL_API parse_until_char (string_view &str, char c, bool eat=true) noexcept;

/// If str's first non-whitespace characters are the prefix, return true and
/// additionally modify str to skip over that prefix if eat is also true.
/// Otherwise, if str doesn't start with optional whitespace and the prefix,
/// return false and don't modify str.
bool OIIO_UTIL_API parse_prefix (string_view &str, string_view prefix, bool eat=true) noexcept;

/// If str's first non-whitespace characters form a valid integer, return
/// true, place the integer's value in val, and additionally modify str to
/// skip over the parsed integer if eat is also true. Otherwise, if no
/// integer is found at the beginning of str, return false and don't modify
/// val or str.
bool OIIO_UTIL_API parse_int (string_view &str, int &val, bool eat=true) noexcept;

/// If str's first non-whitespace characters form a valid float, return
/// true, place the float's value in val, and additionally modify str to
/// skip over the parsed float if eat is also true. Otherwise, if no float
/// is found at the beginning of str, return false and don't modify val or
/// str.
bool OIIO_UTIL_API parse_float (string_view &str, float &val, bool eat=true) noexcept;

/// Synonym for parse_int
inline bool parse_value(string_view &str, float &val, bool eat=true) noexcept
{
    return parse_float(str, val, eat);
}

/// Synonym for parse_float
inline bool parse_value(string_view &str, int &val, bool eat=true) noexcept
{
    return parse_int(str, val, eat);
}

/// Parse from `str`: a `prefix`, a series of int values separated by the
/// `sep` string, and a `postfix`, placing the values in the elements of
/// mutable span `values`, where the span length indicates the number of
/// values to read. Any of the prefix, separator, or postfix may be empty
/// strings. If `eat` is true and the parse was successful, `str` will be
/// updated in place to trim everything that was parsed, but if any part of
/// the parse failed, `str` will not be altered from its original state.
bool OIIO_UTIL_API
parse_values(string_view& str, string_view prefix, span<int> values,
             string_view sep = "", string_view postfix = "",
             bool eat = true) noexcept;
/// parse_values for int.
bool OIIO_UTIL_API
parse_values(string_view& str, string_view prefix, span<float> values,
             string_view sep = "", string_view postfix = "",
             bool eat = true) noexcept;

/// Similar to parse_values, but with no option to "eat" from
/// or modify the source string.
inline bool
scan_values(string_view str, string_view prefix, span<int> values,
            string_view sep = "", string_view postfix = "") noexcept
{
    string_view sv(str);
    return parse_values(sv, prefix, values, sep, postfix);
}

inline bool
scan_values(string_view str, string_view prefix, span<float> values,
            string_view sep = "", string_view postfix = "") noexcept
{
    string_view sv(str);
    return parse_values(sv, prefix, values, sep, postfix);
}

enum QuoteBehavior { DeleteQuotes, KeepQuotes };
/// If str's first non-whitespace characters form a valid string (either a
/// single word separated by whitespace or anything inside a double-quoted
/// ("") or single-quoted ('') string, return true, place the string's value
/// (not including surrounding double quotes) in val, and additionally
/// modify str to skip over the parsed string if eat is also true.
/// Otherwise, if no string is found at the beginning of str, return false
/// and don't modify val or str. If keep_quotes is true, the surrounding
/// double quotes (if present) will be kept in val.
bool OIIO_UTIL_API parse_string (string_view &str, string_view &val, bool eat=true,
                            QuoteBehavior keep_quotes=DeleteQuotes) noexcept;

/// Return the first "word" (set of contiguous alphabetical characters) in
/// str, and additionally modify str to skip over the parsed word if eat is
/// also true. Otherwise, if no word is found at the beginning of str,
/// return an empty string_view and don't modify str.
string_view OIIO_UTIL_API parse_word (string_view &str, bool eat=true) noexcept;

/// If str's first non-whitespace characters form a valid C-like identifier,
/// return the identifier, and additionally modify str to skip over the
/// parsed identifier if eat is also true. Otherwise, if no identifier is
/// found at the beginning of str, return an empty string_view and don't
/// modify str.
string_view OIIO_UTIL_API parse_identifier (string_view &str, bool eat=true) noexcept;

/// If str's first non-whitespace characters form a valid C-like identifier,
/// return the identifier, and additionally modify str to skip over the
/// parsed identifier if eat is also true. Otherwise, if no identifier is
/// found at the beginning of str, return an empty string_view and don't
/// modify str. The 'allowed' parameter may specify a additional characters
/// accepted that would not ordinarily be allowed in C identifiers, for
/// example, parse_identifier (blah, "$:") would allow "identifiers"
/// containing dollar signs and colons as well as the usual alphanumeric and
/// underscore characters.
string_view OIIO_UTIL_API parse_identifier (string_view &str,
                                       string_view allowed, bool eat = true) noexcept;

/// If the C-like identifier at the head of str exactly matches id,
/// return true, and also advance str if eat is true. If it is not a match
/// for id, return false and do not alter str.
bool OIIO_UTIL_API parse_identifier_if (string_view &str, string_view id,
                                   bool eat=true) noexcept;

/// Return the longest prefix of `str` that does not contain any characters
/// found in `set` (which defaults to the set of common whitespace
/// characters). If `eat` is true, then `str` will be modified to trim off
/// this returned prefix (but not the separator character).
string_view OIIO_UTIL_API parse_until (string_view &str,
                                  string_view set=" \t\r\n", bool eat=true) noexcept;

/// Return the longest prefix of `str` that contain only characters found in
/// `set`. If `eat` is true, then `str` will be modified to trim off this
/// returned prefix.
string_view OIIO_UTIL_API parse_while (string_view &str,
                                  string_view set, bool eat=true) noexcept;

/// Return the prefix of str up to and including the first newline ('\n')
/// character, or all of str if no newline is found within it. If `eat` is
/// true, then `str` will be modified to trim off this returned prefix
/// (including the newline character).
string_view OIIO_UTIL_API parse_line(string_view& str, bool eat = true) noexcept;


/// Assuming the string str starts with either '(', '[', or '{', return the
/// head, up to and including the corresponding closing character (')', ']',
/// or '}', respectively), recognizing nesting structures. For example,
/// parse_nested("(a(b)c)d") should return "(a(b)c)", NOT "(a(b)". Return an
/// empty string if str doesn't start with one of those characters, or
/// doesn't contain a correctly matching nested pair. If eat==true, str will
/// be modified to trim off the part of the string that is returned as the
/// match.
string_view OIIO_UTIL_API parse_nested (string_view &str, bool eat=true) noexcept;

/// Does the string follow the lexical rule of a C identifier?
inline bool
string_is_identifier(string_view str)
{
    // If a leading identifier is the entirety of str, it's an ident.
    string_view ident = parse_identifier(str);
    return (!ident.empty() && str.empty());
}

/// Look within `str` for the pattern:
///     head nonwhitespace_chars whitespace
/// Remove that full pattern from `str` and return the nonwhitespace
/// part that followed the head (or return the empty string and leave `str`
/// unmodified, if the head was never found).
OIIO_UTIL_API std::string
excise_string_after_head (std::string& str, string_view head);


/// Converts utf-8 string to vector of unicode codepoints. This function
/// will not stop on invalid sequences. It will let through some invalid
/// utf-8 sequences like: 0xfdd0-0xfdef, 0x??fffe/0x??ffff. It does not
/// support 5-6 bytes long utf-8 sequences. Will skip trailing character if
/// there are not enough bytes for decoding a codepoint.
///
/// N.B. Following should probably return u32string instead of taking
/// vector, but C++11 support is not yet stabilized across compilers.
/// We will eventually add that and deprecate this one, after everybody
/// is caught up to C++11.
void OIIO_UTIL_API utf8_to_unicode (string_view str, std::vector<uint32_t> &uvec);

/// Encode the string in base64.
/// https://en.wikipedia.org/wiki/Base64
std::string OIIO_UTIL_API base64_encode (string_view str);


enum class EditDistMetric { Levenshtein };

/// Compute an edit distance metric between strings `a` and `b`, roughly
/// speaking, the number of changes that would be made to transform one string
/// into the other. Identical strings have a distance of 0. The `method`
/// selects among possible algorithms, which may have different distance
/// metrics or allow different types of edits. (Currently, the only method
/// supported is Levenshtein; this parameter is for future expansion.)
OIIO_UTIL_API size_t
edit_distance(string_view a, string_view b,
              EditDistMetric metric = EditDistMetric::Levenshtein);


/// Evaluate a string as a boolean value using the following heuristic:
///   - If the string is a valid numeric value (represents an integer or
///     floating point value), return true if it's non-zero, false if it's
///     zero.
///   - If the string is one of "false", "no", or "off", or if it contains
///     only whitespace, return false.
///   - All other non-empty strings return true.
/// The comparisons are case-insensitive and ignore leading and trailing
/// whitespace.
OIIO_UTIL_API bool
eval_as_bool(string_view value);

}  // namespace Strutil


// Bring the ever-useful Strutil::print into the OIIO namespace.
using Strutil::print;

OIIO_NAMESPACE_END
