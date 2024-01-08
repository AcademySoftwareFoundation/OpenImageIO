// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once
#define OPENIMAGEIO_USTRING_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::string empty_std_string static member variable of ustring below.
// Also remove a warning about the strncpy function not being safe and deprecated in MSVC.
// There is no equivalent safe and portable function and trying to fix this is more trouble than
// its worth. (see http://stackoverflow.com/questions/858252/alternatives-to-ms-strncpy-s)
#    pragma warning(disable : 4251 4996)
#endif

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/strutil.h>
#include <cstring>
#include <iostream>
#include <string>


OIIO_NAMESPACE_BEGIN

// Feature tests
#define OIIO_USTRING_HAS_USTRINGHASH 1
#define OIIO_USTRING_HAS_CTR_FROM_USTRINGHASH 1
#define OIIO_USTRING_HAS_STDHASH 1
#define OIIO_HAS_USTRINGHASH_FORMATTER 1


class ustringhash;  // forward declaration



/// A ustring is an alternative to char* or std::string for storing
/// strings, in which the character sequence is unique (allowing many
/// speed advantages for assignment, equality testing, and inequality
/// testing).
///
/// The implementation is that behind the scenes there is a hash set of
/// allocated strings, so the characters of each string are unique.  A
/// ustring itself is a pointer to the characters of one of these canonical
/// strings.  Therefore, assignment and equality testing is just a single
/// 32- or 64-bit int operation, the only mutex is when a ustring is
/// created from raw characters, and the only malloc is the first time
/// each canonical ustring is created.
///
/// The internal table also contains a std::string version and the length
/// of the string, so converting a ustring to a std::string (via
/// ustring::string()) or querying the number of characters (via
/// ustring::size() or ustring::length()) is extremely inexpensive, and does
/// not involve creation/allocation of a new std::string or a call to
/// strlen.
///
/// We try very hard to completely mimic the API of std::string,
/// including all the constructors, comparisons, iterations, etc.  Of
/// course, the characters of a ustring are non-modifiable, so we do not
/// replicate any of the non-const methods of std::string.  But in most
/// other ways it looks and acts like a std::string and so most templated
/// algorithms that would work on a "const std::string &" will also work
/// on a ustring.
///
/// Note that like a `char*`, but unlike a `std::string`, a ustring is not
/// allowed to contain any embedded NUL ('\0') characters. When constructing
/// ustrings from a std::string or a string_view, the contents will be
/// truncated at the point of any NUL character. This is done to ensure that
/// ustring::c_str() refers to the same C-style character sequence as the
/// ustring itself or ustring::string().
///
/// Usage guidelines:
///
/// Compared to standard strings, ustrings have several advantages:
///
///   - Each individual ustring is very small -- in fact, we guarantee that
///     a ustring is the same size and memory layout as an ordinary char*.
///   - Storage is frugal, since there is only one allocated copy of each
///     unique character sequence, throughout the lifetime of the program.
///   - Assignment from one ustring to another is just copy of the pointer;
///     no allocation, no character copying, no reference counting.
///   - Equality testing (do the strings contain the same characters) is
///     a single operation, the comparison of the pointer.
///   - Memory allocation only occurs when a new ustring is constructed from
///     raw characters the FIRST time -- subsequent constructions of the
///     same string just finds it in the canonical string set, but doesn't
///     need to allocate new storage.  Destruction of a ustring is trivial,
///     there is no de-allocation because the canonical version stays in
///     the set.  Also, therefore, no user code mistake can lead to
///     memory leaks.
///
/// But there are some problems, too.  Canonical strings are never freed
/// from the table.  So in some sense all the strings "leak", but they
/// only leak one copy for each unique string that the program ever comes
/// across.  Also, creation of unique strings from raw characters is more
/// expensive than for standard strings, due to hashing, table queries,
/// and other overhead.
///
/// On the whole, ustrings are a really great string representation
///   - if you tend to have (relatively) few unique strings, but many
///     copies of those strings;
///   - if the creation of strings from raw characters is relatively
///     rare compared to copying or comparing to existing strings;
///   - if you tend to make the same strings over and over again, and
///     if it's relatively rare that a single unique character sequence
///     is used only once in the entire lifetime of the program;
///   - if your most common string operations are assignment and equality
///     testing and you want them to be as fast as possible;
///   - if you are doing relatively little character-by-character assembly
///     of strings, string concatenation, or other "string manipulation"
///     (other than equality testing).
///
/// ustrings are not so hot
///   - if your program tends to have very few copies of each character
///     sequence over the entire lifetime of the program;
///   - if your program tends to generate a huge variety of unique
///     strings over its lifetime, each of which is used only a short
///     time and then discarded, never to be needed again;
///   - if you don't need to do a lot of string assignment or equality
///     testing, but lots of more complex string manipulation.
///
class OIIO_UTIL_API ustring {
public:
    using rep_t      = const char*;  ///< The underlying representation type
    using value_type = char;
    using hash_t     = uint64_t;  ///< The hash type
    using pointer    = value_type*;
    using reference  = value_type&;
    using const_reference        = const value_type&;
    using size_type              = size_t;
    static const size_type npos  = static_cast<size_type>(-1);
    using const_iterator         = std::string::const_iterator;
    using const_reverse_iterator = std::string::const_reverse_iterator;

    /// Default ctr for ustring -- make an empty string.
    constexpr ustring() noexcept
        : m_chars(nullptr)
    {
    }

    /// Construct a ustring from a null-terminated C string (char *).
    explicit ustring(const char* str)
    {
        m_chars = str ? make_unique(str) : nullptr;
    }

    /// Construct a ustring from a string_view, which can be auto-converted
    /// from either a null-terminated C string (char *) or a C++
    /// std::string.
    explicit ustring(string_view str)
    {
        m_chars = str.data() ? make_unique(str) : nullptr;
    }

    /// Construct a ustring from at most n characters of str, starting at
    /// position pos.
    ustring(const char* str, size_type pos, size_type n)
        : m_chars(make_unique(std::string(str, pos, n).c_str()))
    {
    }

    /// Construct a ustring from the first n characters of str.
    ustring(const char* str, size_type n)
        : m_chars(make_unique(string_view(str, n)))
    {
    }

    /// Construct a ustring from n copies of character c.
    ustring(size_type n, char c)
        : m_chars(make_unique(std::string(n, c).c_str()))
    {
    }

    /// Construct a ustring from an indexed substring of a std::string.
    ustring(const std::string& str, size_type pos, size_type n = npos)
    {
        string_view sref(str);
        sref    = sref.substr(pos, n);
        m_chars = make_unique(sref);
    }

    /// Copy construct a ustring from another ustring.
    ustring(const ustring& str) noexcept
        : m_chars(str.m_chars)
    {
    }

    /// Construct a ustring from an indexed substring of a ustring.
    ustring(const ustring& str, size_type pos, size_type n = npos)
    {
        string_view sref(str);
        sref    = sref.substr(pos, n);
        m_chars = make_unique(sref);
    }

#ifndef __CUDA_ARCH__
    /// Construct from a known ustringhash
    inline explicit ustring(ustringhash hash);
#endif

    /// ustring destructor.
    ~ustring() noexcept {}

    /// Conversion to an OIIO::string_view.
    operator string_view() const noexcept { return { c_str(), length() }; }

    /// Conversion to std::string (explicit only!).
    explicit operator std::string() const noexcept { return string(); }

    /// Assign a ustring to *this.
    const ustring& assign(const ustring& str)
    {
        m_chars = str.m_chars;
        return *this;
    }

    /// Assign a substring of a ustring to *this.
    const ustring& assign(const ustring& str, size_type pos, size_type n = npos)
    {
        *this = ustring(str, pos, n);
        return *this;
    }

    /// Assign a std::string to *this.
    const ustring& assign(const std::string& str)
    {
        assign(str.c_str());
        return *this;
    }

    /// Assign a substring of a std::string to *this.
    const ustring& assign(const std::string& str, size_type pos,
                          size_type n = npos)
    {
        *this = ustring(str, pos, n);
        return *this;
    }

    /// Assign a null-terminated C string (char*) to *this.
    const ustring& assign(const char* str)
    {
        m_chars = str ? make_unique(str) : nullptr;
        return *this;
    }

    /// Assign the first n characters of str to *this.
    const ustring& assign(const char* str, size_type n)
    {
        *this = ustring(str, n);
        return *this;
    }

    /// Assign n copies of c to *this.
    const ustring& assign(size_type n, char c)
    {
        *this = ustring(n, c);
        return *this;
    }

    /// Assign a string_view to *this.
    const ustring& assign(string_view str)
    {
        m_chars = str.length() ? make_unique(str) : nullptr;
        return *this;
    }

    /// Assign a ustring to another ustring.
    const ustring& operator=(const ustring& str) { return assign(str); }

    /// Assign a null-terminated C string (char *) to a ustring.
    const ustring& operator=(const char* str) { return assign(str); }

    /// Assign a C++ std::string to a ustring.
    const ustring& operator=(const std::string& str) { return assign(str); }

    /// Assign a string_view to a ustring.
    const ustring& operator=(string_view str) { return assign(str); }

    /// Assign a single char to a ustring.
    const ustring& operator=(char c)
    {
        return *this = ustring(string_view(&c, 1));
    }

    /// Return a C string representation of a ustring.
    const char* c_str() const noexcept { return m_chars; }

    /// Return a C string representation of a ustring.
    const char* data() const noexcept { return c_str(); }

    /// Return a C++ std::string representation of a ustring.
    const std::string& string() const noexcept
    {
        if (m_chars) {
            const TableRep* rep = (const TableRep*)m_chars - 1;
            return rep->str;
        } else
            return empty_std_string;
    }

    /// Reset to an empty string.
    void clear() noexcept { m_chars = nullptr; }

    /// Return the number of characters in the string.
    size_t length() const noexcept
    {
        if (!m_chars)
            return 0;
        const TableRep* rep = ((const TableRep*)m_chars) - 1;
        return rep->length;
    }

    /// Return a hashed version of the string
    hash_t hash() const noexcept
    {
        if (!m_chars)
            return 0;
        const TableRep* rep = ((const TableRep*)m_chars) - 1;
        return rep->hashed;
    }

    /// Return a hashed version of the string
    ustringhash uhash() const noexcept;

    /// Return the number of characters in the string.
    size_t size() const noexcept { return length(); }

    /// Is the string empty -- i.e., is it nullptr or does it point to an
    /// empty string?
    bool empty() const noexcept { return (size() == 0); }

    /// Return a const_iterator that references the first character of
    /// the string.
    const_iterator begin() const noexcept { return string().begin(); }

    /// Return a const_iterator that references the end of a traversal
    /// of the characters of the string.
    const_iterator end() const noexcept { return string().end(); }

    /// Return a const_reverse_iterator that references the last
    /// character of the string.
    const_reverse_iterator rbegin() const noexcept { return string().rbegin(); }

    /// Return a const_reverse_iterator that references the end of
    /// a reverse traversal of the characters of the string.
    const_reverse_iterator rend() const noexcept { return string().rend(); }

    /// Return a reference to the character at the given position.
    /// Note that it's up to the caller to be sure pos is within the
    /// size of the string.
    const_reference operator[](size_type pos) const noexcept
    {
        return c_str()[pos];
    }

    /// Dump into character array s the characters of this ustring,
    /// beginning with position pos and copying at most n characters.
    size_type copy(char* s, size_type n, size_type pos = 0) const
    {
        if (m_chars == nullptr) {
            s[0] = 0;
            return 0;
        }
        char* c = strncpy(s, c_str() + pos, n);  // NOSONAR
        return (size_type)(c - s);
    }

    /// Returns a substring of the ustring object consisting of n
    /// characters starting at position pos.
    ustring substr(size_type pos = 0, size_type n = npos) const
    {
        return ustring(*this, pos, n);
    }

    size_type find(const ustring& str, size_type pos = 0) const noexcept
    {
        return string().find(str.string(), pos);
    }

    size_type find(const std::string& str, size_type pos = 0) const noexcept
    {
        return string().find(str, pos);
    }

    size_type find(const char* s, size_type pos, size_type n) const
    {
        return string().find(s, pos, n);
    }

    size_type find(const char* s, size_type pos = 0) const
    {
        return string().find(s, pos);
    }

    size_type find(char c, size_type pos = 0) const noexcept
    {
        return string().find(c, pos);
    }

    size_type rfind(const ustring& str, size_type pos = npos) const noexcept
    {
        return string().rfind(str.string(), pos);
    }

    size_type rfind(const std::string& str, size_type pos = npos) const noexcept
    {
        return string().rfind(str, pos);
    }

    size_type rfind(const char* s, size_type pos, size_type n) const
    {
        return string().rfind(s, pos, n);
    }

    size_type rfind(const char* s, size_type pos = npos) const
    {
        return string().rfind(s, pos);
    }

    size_type rfind(char c, size_type pos = npos) const noexcept
    {
        return string().rfind(c, pos);
    }

    size_type find_first_of(const ustring& str,
                            size_type pos = 0) const noexcept
    {
        return string().find_first_of(str.string(), pos);
    }

    size_type find_first_of(const std::string& str,
                            size_type pos = 0) const noexcept
    {
        return string().find_first_of(str, pos);
    }

    size_type find_first_of(const char* s, size_type pos, size_type n) const
    {
        return string().find_first_of(s, pos, n);
    }

    size_type find_first_of(const char* s, size_type pos = 0) const
    {
        return string().find_first_of(s, pos);
    }

    size_type find_first_of(char c, size_type pos = 0) const noexcept
    {
        return string().find_first_of(c, pos);
    }

    size_type find_last_of(const ustring& str,
                           size_type pos = npos) const noexcept
    {
        return string().find_last_of(str.string(), pos);
    }

    size_type find_last_of(const std::string& str,
                           size_type pos = npos) const noexcept
    {
        return string().find_last_of(str, pos);
    }

    size_type find_last_of(const char* s, size_type pos, size_type n) const
    {
        return string().find_last_of(s, pos, n);
    }

    size_type find_last_of(const char* s, size_type pos = npos) const
    {
        return string().find_last_of(s, pos);
    }

    size_type find_last_of(char c, size_type pos = npos) const noexcept
    {
        return string().find_last_of(c, pos);
    }

    size_type find_first_not_of(const ustring& str,
                                size_type pos = 0) const noexcept
    {
        return string().find_first_not_of(str.string(), pos);
    }

    size_type find_first_not_of(const std::string& str,
                                size_type pos = 0) const noexcept
    {
        return string().find_first_not_of(str, pos);
    }

    size_type find_first_not_of(const char* s, size_type pos, size_type n) const
    {
        return string().find_first_not_of(s, pos, n);
    }

    size_type find_first_not_of(const char* s, size_type pos = 0) const
    {
        return string().find_first_not_of(s, pos);
    }

    size_type find_first_not_of(char c, size_type pos = 0) const noexcept
    {
        return string().find_first_not_of(c, pos);
    }

    size_type find_last_not_of(const ustring& str,
                               size_type pos = npos) const noexcept
    {
        return string().find_last_not_of(str.string(), pos);
    }

    size_type find_last_not_of(const std::string& str,
                               size_type pos = npos) const noexcept
    {
        return string().find_last_not_of(str, pos);
    }

    size_type find_last_not_of(const char* s, size_type pos, size_type n) const
    {
        return string().find_last_not_of(s, pos, n);
    }

    size_type find_last_not_of(const char* s, size_type pos = npos) const
    {
        return string().find_last_not_of(s, pos);
    }

    size_type find_last_not_of(char c, size_type pos = npos) const noexcept
    {
        return string().find_last_not_of(c, pos);
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.
    int compare(string_view str) const noexcept
    {
        return string_view(*this).compare(str);
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.
    int compare(const char* str) const noexcept
    {
        return strcmp(c_str() ? c_str() : "", str ? str : "");
    }

    /// Return 0 if a is lexicographically equal to b, -1 if a is
    /// lexicographically earlier than b, 1 if a is lexicographically
    /// after b.
    friend int compare(const std::string& a, const ustring& b) noexcept
    {
        return string_view(a).compare(b);
    }

    /// Test two ustrings for equality -- are they comprised of the same
    /// sequence of characters.  Note that because ustrings are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator==(const ustring& str) const noexcept
    {
        return c_str() == str.c_str();
    }

    /// Test two ustrings for inequality -- are they comprised of different
    /// sequences of characters.  Note that because ustrings are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator!=(const ustring& str) const noexcept
    {
        return c_str() != str.c_str();
    }

    /// Test a ustring (*this) for lexicographic equality with std::string
    /// x.
    bool operator==(const std::string& x) const noexcept
    {
        return compare(x) == 0;
    }

    /// Test a ustring (*this) for lexicographic equality with string_view
    /// x.
    bool operator==(string_view x) const noexcept { return compare(x) == 0; }

    /// Test a ustring (*this) for lexicographic equality with char* x.
    bool operator==(const char* x) const noexcept { return compare(x) == 0; }

    /// Test for lexicographic equality between std::string a and ustring
    /// b.
    friend bool operator==(const std::string& a, const ustring& b) noexcept
    {
        return b.compare(a) == 0;
    }

    /// Test for lexicographic equality between string_view a and ustring
    /// b.
    friend bool operator==(string_view a, const ustring& b) noexcept
    {
        return b.compare(a) == 0;
    }

    /// Test for lexicographic equality between char* a and ustring
    /// b.
    friend bool operator==(const char* a, const ustring& b) noexcept
    {
        return b.compare(a) == 0;
    }

    /// Test a ustring (*this) for lexicographic inequality with
    /// std::string x.
    bool operator!=(const std::string& x) const noexcept
    {
        return compare(x) != 0;
    }

    /// Test a ustring (*this) for lexicographic inequality with
    /// string_view x.
    bool operator!=(string_view x) const noexcept { return compare(x) != 0; }

    /// Test a ustring (*this) for lexicographic inequality with
    /// char* x.
    bool operator!=(const char* x) const noexcept { return compare(x) != 0; }

    /// Test for lexicographic inequality between std::string a and
    /// ustring b.
    friend bool operator!=(const std::string& a, const ustring& b) noexcept
    {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic inequality between string_view a and
    /// ustring b.
    friend bool operator!=(string_view a, const ustring& b) noexcept
    {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic inequality between char* a and
    /// ustring b.
    friend bool operator!=(const char* a, const ustring& b) noexcept
    {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic 'less', comes in handy for lots of STL
    /// containers and algorithms.
    bool operator<(const ustring& x) const noexcept { return compare(x) < 0; }

    /// Construct a ustring in a printf-like fashion.  In other words,
    /// something like:
    ///    ustring s = ustring::sprintf("blah %d %g", (int)foo, (float)bar);
    /// The argument list is fully typesafe.
    /// The formatting of the string will always use the classic "C" locale
    /// conventions (in particular, '.' as decimal separator for float values).
    template<typename... Args>
    OIIO_NODISCARD static ustring sprintf(const char* fmt, const Args&... args)
    {
        return ustring(Strutil::sprintf(fmt, args...));
    }

    /// Construct a ustring in a fmt::format-like fashion.  In other words,
    /// something like:
    ///    ustring s = ustring::fmtformat("blah {} {}", (int)foo, (float)bar);
    /// The argument list is fully typesafe.
    /// The formatting of the string will always use the classic "C" locale
    /// conventions (in particular, '.' as decimal separator for float values).
    template<typename... Args>
    OIIO_NODISCARD static ustring fmtformat(const char* fmt,
                                            const Args&... args)
    {
        return ustring(Strutil::fmt::format(fmt, args...));
    }

    /// NOTE: Semi-DEPRECATED! This will someday switch to behave like
    /// fmt::format (or future std::format) but for now, it is back
    /// compatible and equivalent to sprintf.
    template<typename... Args>
    OIIO_FORMAT_DEPRECATED static ustring format(const char* fmt,
                                                 const Args&... args)
    {
        return ustring(Strutil::format(fmt, args...));
    }

    /// Concatenate two strings, returning a ustring, implemented carefully
    /// to not perform any redundant copies or allocations.
    static ustring concat(string_view s, string_view t);

    /// Generic stream output of a ustring.
    friend std::ostream& operator<<(std::ostream& out, const ustring& str)
    {
        if (str.c_str() && out.good())
            out.write(str.c_str(), str.size());
        return out;
    }

    /// Return the statistics output as a string.
    static std::string getstats(bool verbose = true);

    /// Return the amount of memory consumed by the ustring table.
    static size_t memory();

    /// Return the total number of ustrings in the internal table.
    static size_t total_ustrings();

    /// Return the total number ustrings that have the exact hash as another
    /// ustring. If `collisions` is passed, store all the colliding ustrings
    /// in the vector.
    static size_t hash_collisions(std::vector<ustring>* collisions = nullptr);

    /// Given a string_view, return a pointer to the unique
    /// version kept in the internal table (creating a new table entry
    /// if we haven't seen this sequence of characters before).
    /// N.B.: this is equivalent to ustring(str).c_str().  It's also the
    /// routine that is used directly by ustring's internals to generate
    /// the canonical unique copy of the characters.
    static const char* make_unique(string_view str);

    /// Is this character pointer a unique ustring representation of
    /// those characters?  Useful for diagnostics and debugging.
    static bool is_unique(const char* str)
    {
        return str == nullptr || make_unique(str) == str;
    }

    /// Create a ustring from characters guaranteed to already be
    /// ustring-clean, without having to run through the hash yet
    /// again. Use with extreme caution!!!
    static ustring from_unique(const char* unique)
    {
        OIIO_DASSERT(is_unique(unique));  // DEBUG builds -- check it!
        ustring u;
        u.m_chars = unique;
        return u;
    }

    /// Return the ustring corresponding to the given hash, or the empty
    /// ustring() if there is no registered ustring with that hash. Note that
    /// if there are multiple ustrings with the same hash, this will return
    /// the first one it finds in the table.
    OIIO_NODISCARD static ustring from_hash(hash_t hash);

private:
    // Individual ustring internal representation -- the unique characters.
    //
    rep_t m_chars;

public:
    // Representation within the hidden string table -- DON'T EVER CREATE
    // ONE OF THESE YOURSELF!
    // The characters are found directly after the rep.  So that means that
    // if you know the rep, the chars are at (char *)(rep+1), and if you
    // know the chars, the rep is at ((TableRep *)chars - 1).
    struct TableRep {
        hash_t hashed;    // precomputed Hash value
        std::string str;  // String representation
        size_t length;    // Length of the string; must be right before cap
        size_t dummy_capacity;  // Dummy field! must be right before refcount
        int dummy_refcount;     // Dummy field! must be right before chars
        TableRep(string_view strref, hash_t hash);
        ~TableRep();
        const char* c_str() const noexcept { return (const char*)(this + 1); }
    };

private:
    static std::string empty_std_string;
};



/// A ustringhash holds the hash of a ustring in a type-safe way.
///
/// It has a nearly identical interface to a ustring, and still refers to a
/// string in the internal ustring table. But whereas the representation of a
/// ustring is the pointer to the characters, the representation of a
/// ustringhash is the hash of the string.
///
/// For some uses where you don't need access to the characters in any
/// performance-critical paths, this may be a more convenient representation.
/// In particular, it's well suited to a GPU that doesn't have access to the
/// character memory. Another interesting difference is that from run to run,
/// a ustring may have a different literal value, since there's no reason to
/// expect that the pointer to a string like "foo" will refer to the same
/// memory location every time the program executes, but in contrast, the hash
/// is guaranteed to be identical from run to run.
///
class OIIO_UTIL_API ustringhash {
public:
    using rep_t  = ustring::hash_t;  ///< The underlying representation type
    using hash_t = ustring::hash_t;  ///< The hash type

    // Default constructor
    OIIO_HOSTDEVICE constexpr ustringhash() noexcept
        : m_hash(0)
    {
    }

    /// ustringhash destructor.
    ~ustringhash() noexcept = default;

    /// Copy construct a ustringhash from another ustringhash.
    OIIO_HOSTDEVICE constexpr ustringhash(const ustringhash& str) noexcept
        : m_hash(str.m_hash)
    {
    }

    /// Construct from a ustring
    ustringhash(const ustring& str) noexcept
        : m_hash(str.hash())
    {
    }

    /// Construct a ustringhash from a null-terminated C string (char *).
    OIIO_DEVICE_CONSTEXPR explicit ustringhash(const char* str)
#ifdef __CUDA_ARCH__
        // GPU: just compute the hash. This can be constexpr!
        : m_hash(Strutil::strhash(str))
#else
        // CPU: make ustring, get its hash. Note that ustring ctr can't be
        // constexpr because it has to modify the internal ustring table.
        : m_hash(ustring(str).hash())
#endif
    {
    }

    OIIO_DEVICE_CONSTEXPR explicit ustringhash(const char* str, size_t len)
#ifdef __CUDA_ARCH__
        // GPU: just compute the hash. This can be constexpr!
        : m_hash(Strutil::strhash(len, str))
#else
        // CPU: make ustring, get its hash. Note that ustring ctr can't be
        // constexpr because it has to modify the internal ustring table.
        : m_hash(ustring(str, len).hash())
#endif
    {
    }

    /// Construct a ustringhash from a string_view, which can be
    /// auto-converted from either a std::string.
    OIIO_DEVICE_CONSTEXPR explicit ustringhash(string_view str)
#ifdef __CUDA_ARCH__
        // GPU: just compute the hash. This can be constexpr!
        : m_hash(Strutil::strhash(str))
#else
        // CPU: make ustring, get its hash. Note that ustring ctr can't be
        // constexpr because it has to modify the internal ustring table.
        : m_hash(ustring(str).hash())
#endif
    {
    }

    /// Construct from a raw hash value. Beware: results are undefined if it's
    /// not the valid hash of a ustring.
    OIIO_HOSTDEVICE explicit constexpr ustringhash(hash_t hash) noexcept
        : m_hash(hash)
    {
    }

    /// Conversion to an OIIO::string_view.
    operator string_view() const noexcept { return ustring::from_hash(m_hash); }

    /// Conversion to std::string (explicit only!).
    explicit operator std::string() const noexcept
    {
        return ustring::from_hash(m_hash).string();
    }

    /// Assign from a ustringhash
    OIIO_HOSTDEVICE constexpr const ustringhash&
    operator=(const ustringhash& str)
    {
        m_hash = str.m_hash;
        return *this;
    }

    /// Assign from a ustring
    const ustringhash& operator=(const ustring& str)
    {
        m_hash = str.hash();
        return *this;
    }

    /// Reset to an empty string.
    OIIO_HOSTDEVICE void clear() noexcept { m_hash = 0; }

#ifndef __CUDA_ARCH__
    /// Return a pointer to the characters.
    const char* c_str() const noexcept
    {
        return ustring::from_hash(m_hash).c_str();
    }

    /// Return a C string representation of a ustring.
    const char* data() const noexcept { return c_str(); }

    /// Return a C++ std::string representation of a ustring.
    const std::string& string() const noexcept
    {
        return ustring::from_hash(m_hash).string();
    }

    /// Return the number of characters in the string.
    size_t length() const noexcept
    {
        return ustring::from_hash(m_hash).length();
    }
#endif

    /// Return a hashed version of the string
    OIIO_HOSTDEVICE constexpr hash_t hash() const noexcept { return m_hash; }

#ifndef __CUDA_ARCH__
    /// Return the number of characters in the string.
    size_t size() const noexcept { return length(); }
#endif

    /// Is the string empty -- i.e., is it nullptr or does it point to an
    /// empty string? (Empty strings always have a hash of 0.)
    OIIO_HOSTDEVICE constexpr bool empty() const noexcept
    {
        return m_hash == 0;
    }

    /// Test for equality with another ustringhash.
    OIIO_HOSTDEVICE constexpr bool
    operator==(const ustringhash& str) const noexcept
    {
        return m_hash == str.m_hash;
    }

    /// Test for inequality with another ustringhash.
    OIIO_HOSTDEVICE constexpr bool
    operator!=(const ustringhash& str) const noexcept
    {
        return m_hash != str.m_hash;
    }

    /// Test for equality with a char*.
    OIIO_CONSTEXPR17 bool operator==(const char* str) const noexcept
    {
        return m_hash == Strutil::strhash(str);
    }

    /// Test for inequality with a char*.
    OIIO_CONSTEXPR17 bool operator!=(const char* str) const noexcept
    {
        return m_hash != Strutil::strhash(str);
    }

#ifndef __CUDA_ARCH__
    /// Test for equality with a ustring.
    bool operator==(const ustring& str) const noexcept
    {
        return m_hash == str.hash();
    }

    friend bool operator==(const ustring& a, const ustringhash& b) noexcept
    {
        return b == a;
    }

    /// Test for inequality with a ustring.
    bool operator!=(const ustring& str) const noexcept
    {
        return m_hash != str.hash();
    }

    friend bool operator!=(const ustring& a, const ustringhash& b) noexcept
    {
        return b != a;
    }

    OIIO_HOSTDEVICE constexpr bool operator<(const ustringhash& x) const noexcept
    {
        return hash() < x.hash();
    }

    /// Generic stream output of a ustringhash outputs the string it refers
    /// to.
    friend std::ostream& operator<<(std::ostream& out, const ustringhash& str)
    {
        return (out << ustring(str));
    }
#endif

    /// Return the ustringhash corresponding to the given hash. Caveat emptor:
    /// results are undefined if it's not the valid hash of a ustring.
    OIIO_NODISCARD static constexpr ustringhash from_hash(hash_t hash)
    {
        ustringhash u;
        u.m_hash = hash;
        return u;
    }

private:
    // Individual ustringhash internal representation -- the hash value.
    rep_t m_hash;

    friend class ustring;
};



static_assert(sizeof(ustringhash) == sizeof(uint64_t),
              "ustringhash should be the same size as a uint64_t");
static_assert(sizeof(ustring) == sizeof(const char*),
              "ustring should be the same size as a const char*");



inline ustringhash
ustring::uhash() const noexcept
{
    return ustringhash(hash());
}



#ifndef __CUDA_ARCH__
inline ustring::ustring(ustringhash hash)
{
    // The ustring constructor from a ustringhash is just a pretty
    // wrapper around an awkward construct.
    m_chars = ustring::from_hash(hash.hash()).c_str();
}
#endif



/// ustring string literal operator
inline ustring
operator""_us(const char* str, std::size_t len)
{
    return ustring(str, len);
}


/// ustringhash string literal operator
OIIO_DEVICE_CONSTEXPR ustringhash
operator""_ush(const char* str, std::size_t len)
{
    return ustringhash(str, len);
}



#if OIIO_VERSION_LESS(3, 0, 0)
/// Deprecated -- This is too easy to confuse with the ustringhash class. And
/// also it is unnecessary if you use std::hash<ustring>. This will be removed
/// in OIIO 3.0.
using ustringHash = std::hash<ustring>;
#endif



/// Functor class to use for comparisons when sorting ustrings, if you
/// want the strings sorted lexicographically.
class ustringLess {
public:
    size_t operator()(ustring a, ustring b) const noexcept { return a < b; }
};


/// Functor class to use for comparisons when sorting ustrings, if you
/// don't care if the sort order is lexicographic. This sorts based on
/// the pointers themselves, which is safe because once allocated, a
/// ustring's characters will never be moved. But beware, the resulting
/// sorting order may vary from run to run!
class ustringPtrIsLess {
public:
    size_t operator()(ustring a, ustring b) const noexcept
    {
        return size_t(a.data()) < size_t(b.data());
    }
};



/// Case-insensitive comparison of ustrings.  For speed, this always
/// uses a static locale that doesn't require a mutex lock.
inline bool
iequals(ustring a, ustring b)
{
    return a == b || Strutil::iequals(a.string(), b.string());
}

inline bool
iequals(ustring a, const std::string& b)
{
    return Strutil::iequals(a.string(), b);
}

inline bool
iequals(const std::string& a, ustring b)
{
    return Strutil::iequals(a, b.string());
}



// ustring variant stof from OpenImageIO/strutil.h
namespace Strutil {

#ifndef __CUDA_ARCH__

inline float
stof(ustring s)
{
    return Strutil::stof(s.string());
}

template<>
inline std::string
to_string(const ustring& value)
{
    return value.string();
}

template<>
inline std::string
to_string(const ustringhash& value)
{
    return ustring(value).string();
}

#endif

}  // end namespace Strutil

OIIO_NAMESPACE_END


namespace std {  // not necessary in C++17, then we can just say std::hash
// std::hash specialization for ustring
template<> struct hash<OIIO::ustring> {
    std::size_t operator()(OIIO::ustring u) const noexcept
    {
        return static_cast<std::size_t>(u.hash());
    }
};


// std::hash specialization for ustringhash
template<> struct hash<OIIO::ustringhash> {
    OIIO_HOSTDEVICE constexpr std::size_t
    operator()(OIIO::ustringhash u) const noexcept
    {
        return static_cast<std::size_t>(u.hash());
    }
};
}  // namespace std



// Supply a fmtlib compatible custom formatter for ustring and ustringhash.
FMT_BEGIN_NAMESPACE

template<> struct formatter<OIIO::ustring> : formatter<fmt::string_view, char> {
    template<typename FormatContext>
    auto format(const OIIO::ustring& u,
                FormatContext& ctx) OIIO_FMT_CUSTOM_FORMATTER_CONST
    {
        return formatter<fmt::string_view, char>::format({ u.data(), u.size() },
                                                         ctx);
    }
};

template<>
struct formatter<OIIO::ustringhash> : formatter<fmt::string_view, char> {
    template<typename FormatContext>
    auto format(const OIIO::ustringhash& h,
                FormatContext& ctx) OIIO_FMT_CUSTOM_FORMATTER_CONST
    {
        OIIO::ustring u(h);
        return formatter<fmt::string_view, char>::format({ u.data(), u.size() },
                                                         ctx);
    }
};

FMT_END_NAMESPACE
