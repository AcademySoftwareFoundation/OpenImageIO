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
/// Define the ustring class, unique strings with efficient storage and
/// very fast copy and comparison.


/////////////////////////////////////////////////////////////////////////////
/// \class ustring
///
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
/// course, the charaters of a ustring are non-modifiable, so we do not
/// replicate any of the non-const methods of std::string.  But in most
/// other ways it looks and acts like a std::string and so most templated
/// algorthms that would work on a "const std::string &" will also work
/// on a ustring.
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
///   - Memory allocation only occurs when a new ustring is construted from
///     raw characters the FIRST time -- subsequent constructions of the
///     same string just finds it in the canonial string set, but doesn't
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
/////////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_USTRING_H
#define OPENIMAGEIO_USTRING_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::string empty_std_string static member variable of ustring below.
// Also remove a warning about the strncpy function not being safe and deprecated in MSVC.
// There is no equivalent safe and portable function and trying to fix this is more trouble than
// its worth. (see http://stackoverflow.com/questions/858252/alternatives-to-ms-strncpy-s)
#  pragma warning (disable : 4251 4996)
#endif

#include <string>
#include <iostream>
#include <cstring>
#include <OpenImageIO/export.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/oiioversion.h>

#ifndef NULL
#define NULL 0
#endif

OIIO_NAMESPACE_BEGIN

class OIIO_API ustring {
public:
    typedef char value_type;
    typedef value_type * pointer;
    typedef value_type & reference;
    typedef const value_type & const_reference;
    typedef size_t size_type;
    static const size_type npos = static_cast<size_type>(-1);
    typedef std::string::const_iterator const_iterator;
    typedef std::string::const_reverse_iterator const_reverse_iterator;

    /// Default ctr for ustring -- make an empty string.
    ///
    ustring (void) : m_chars(NULL) { }

    /// Construct a ustring from a null-terminated C string (char *).
    ///
    explicit ustring (const char *str) {
        m_chars = str ? make_unique(str) : NULL;
    }

    /// Construct a ustring from a string_view, which can be auto-converted
    /// from either a null-terminated C string (char *) or a C++
    /// std::string.
    explicit ustring (string_view str) {
        m_chars = str.data() ? make_unique(str) : NULL;
    }

    /// Construct a ustring from at most n characters of str, starting at
    /// position pos.
    ustring (const char *str, size_type pos, size_type n)
        : m_chars (make_unique(std::string(str,pos,n).c_str())) { }

    /// Construct a ustring from the first n characters of str.
    ///
    ustring (const char *str, size_type n)
        : m_chars (make_unique(string_view(str,n))) { }

    /// Construct a ustring from n copies of character c.
    ///
    ustring (size_type n, char c)
        : m_chars (make_unique(std::string(n,c).c_str())) { }

    /// Construct a ustring from an indexed substring of a std::string.
    ///
    ustring (const std::string &str, size_type pos, size_type n=npos) {
        string_view sref(str);
        sref = sref.substr (pos, n);
        m_chars = make_unique(sref);
    }

    /// Copy construct a ustring from another ustring.
    ///
    ustring (const ustring &str) : m_chars(str.m_chars) { }

    /// Construct a ustring from an indexed substring of a ustring.
    ///
    ustring (const ustring &str, size_type pos, size_type n=npos) {
        string_view sref(str);
        sref = sref.substr (pos, n);
        m_chars = make_unique(sref);
    }

    /// ustring destructor.
    ///
    ~ustring () { }

    /// Conversion to string_view
    operator string_view() const { return string_view(c_str(), length()); }

    /// Assign a ustring to *this.
    ///
    const ustring & assign (const ustring &str) {
        m_chars = str.m_chars;
        return *this;
    }

    /// Assign a substring of a ustring to *this.
    ///
    const ustring & assign (const ustring &str, size_type pos, size_type n=npos)
        { *this = ustring(str,pos,n); return *this; }

    /// Assign a std::string to *this.
    ///
    const ustring & assign (const std::string &str) {
        assign (str.c_str());
        return *this;
    }

    /// Assign a substring of a std::string to *this.
    ///
    const ustring & assign (const std::string &str, size_type pos,
                            size_type n=npos)
        { *this = ustring(str,pos,n); return *this; }

    /// Assign a null-terminated C string (char*) to *this.
    ///
    const ustring & assign (const char *str) {
        m_chars = str ? make_unique(str) : NULL;
        return *this;
    }

    /// Assign the first n characters of str to *this.
    ///
    const ustring & assign (const char *str, size_type n)
        { *this = ustring(str,n); return *this; }

    /// Assign n copies of c to *this.
    ///
    const ustring & assign (size_type n, char c)
        { *this = ustring(n,c); return *this; }

    /// Assign a string_view to *this.
    const ustring & assign (string_view str) {
        m_chars = str.length() ? make_unique(str) : NULL;
        return *this;
    }

    /// Assign a ustring to another ustring.
    ///
    const ustring & operator= (const ustring &str) { return assign(str); }

    /// Assign a null-terminated C string (char *) to a ustring.
    ///
    const ustring & operator= (const char *str) { return assign(str); }

    /// Assign a C++ std::string to a ustring.
    ///
    const ustring & operator= (const std::string &str) { return assign(str); }

    /// Assign a string_view to a ustring.
    ///
    const ustring & operator= (string_view str) { return assign(str); }

    /// Assign a single char to a ustring.
    ///
    const ustring & operator= (char c) {
        char s[2];
        s[0] = c; s[1] = 0;
        *this = ustring (s);
        return *this;
    }

    /// Return a C string representation of a ustring.
    ///
    const char *c_str () const {
        return m_chars;
    }

    /// Return a C string representation of a ustring.
    ///
    const char *data () const { return c_str(); }

    /// Return a C++ std::string representation of a ustring.
    ///
    const std::string & string () const {
        if (m_chars) {
            const TableRep *rep = (const TableRep *)m_chars - 1;
            return rep->str;
        }
        else return empty_std_string;
    }

    /// Reset to an empty string.
    ///
    void clear (void) {
        m_chars = NULL;
    }

    /// Return the number of characters in the string.
    ///
    size_t length (void) const {
        if (! m_chars)
            return 0;
        const TableRep *rep = ((const TableRep *)m_chars) - 1;
        return rep->length;
    }

    /// Return a hashed version of the string
    ///
    size_t hash (void) const {
         if (! m_chars)
             return 0;
         const TableRep *rep = ((const TableRep *)m_chars) - 1;
         return rep->hashed;
     }

    /// Return the number of characters in the string.
    ///
    size_t size (void) const { return length(); }

    /// Is the string empty -- i.e., is it the NULL pointer or does it
    /// point to an empty string?
    bool empty (void) const { return (size() == 0); }

    /// Cast to int, which is interpreted as testing whether it's not an
    /// empty string.  This allows you to write "if (t)" with the same
    /// semantics as if it were a char*.
    operator int (void) const { return !empty(); }

    /// Return a const_iterator that references the first character of
    /// the string.
    const_iterator begin () const { return string().begin(); }

    /// Return a const_iterator that references the end of a traversal
    /// of the characters of the string.
    const_iterator end () const { return string().end(); }

    /// Return a const_reverse_iterator that references the last
    /// character of the string.
    const_reverse_iterator rbegin () const { return string().rbegin(); }

    /// Return a const_reverse_iterator that references the end of
    /// a reverse traversal of the characters of the string.
    const_reverse_iterator rend () const { return string().rend(); }

    /// Return a reference to the character at the given position.
    /// Note that it's up to the caller to be sure pos is within the
    /// size of the string.
    const_reference operator[] (size_type pos) const { return c_str()[pos]; }

    /// Dump into character array s the characters of this ustring,
    /// beginning with position pos and copying at most n characters.
    size_type copy (char* s, size_type n, size_type pos = 0) const {
        if (m_chars == NULL)  {
            s[0] = 0;
            return 0;
        }
        char *c = strncpy (s, c_str()+pos, n);
        return (size_type)(c-s);
    }

    /// Returns a substring of the ustring object consisting of n
    /// characters starting at position pos.
    ustring substr (size_type pos = 0, size_type n = npos) const {
        return ustring (*this, pos, n);
    }

    // FIXME: implement compare.

    size_type find(const ustring &str, size_type pos = 0) const {
        return string().find(str.string(), pos);
    }

    size_type find(const std::string &str, size_type pos = 0) const {
        return string().find(str, pos);
    }

    size_type find(const char *s, size_type pos, size_type n) const {
        return string().find(s, pos, n);
    }

    size_type find(const char *s, size_type pos = 0) const {
        return string().find(s, pos);
    }

    size_type find(char c, size_type pos = 0) const {
        return string().find(c, pos);
    }

    size_type rfind(const ustring &str, size_type pos = npos) const {
        return string().rfind(str.string(), pos);
    }

    size_type rfind(const std::string &str, size_type pos = npos) const {
        return string().rfind(str, pos);
    }

    size_type rfind(const char *s, size_type pos, size_type n) const {
        return string().rfind(s, pos, n);
    }

    size_type rfind(const char *s, size_type pos = npos) const {
        return string().rfind(s, pos);
    }

    size_type rfind(char c, size_type pos = npos) const {
        return string().rfind(c, pos);
    }

    size_type find_first_of(const ustring &str, size_type pos = 0) const {
        return string().find_first_of(str.string(), pos);
    }

    size_type find_first_of(const std::string &str, size_type pos = 0) const {
        return string().find_first_of(str, pos);
    }

    size_type find_first_of(const char *s, size_type pos, size_type n) const {
        return string().find_first_of(s, pos, n);
    }

    size_type find_first_of(const char *s, size_type pos = 0) const {
        return string().find_first_of(s, pos);
    }

    size_type find_first_of(char c, size_type pos = 0) const {
        return string().find_first_of(c, pos);
    }

    size_type find_last_of(const ustring &str, size_type pos = npos) const {
        return string().find_last_of(str.string(), pos);
    }

    size_type find_last_of(const std::string &str, size_type pos = npos) const {
        return string().find_last_of(str, pos);
    }

    size_type find_last_of(const char *s, size_type pos, size_type n) const {
        return string().find_last_of(s, pos, n);
    }

    size_type find_last_of(const char *s, size_type pos = npos) const {
        return string().find_last_of(s, pos);
    }

    size_type find_last_of(char c, size_type pos = npos) const {
        return string().find_last_of(c, pos);
    }

    size_type find_first_not_of(const ustring &str, size_type pos = 0) const {
        return string().find_first_not_of(str.string(), pos);
    }

    size_type find_first_not_of(const std::string &str, size_type pos = 0) const {
        return string().find_first_not_of(str, pos);
    }

    size_type find_first_not_of(const char *s, size_type pos, size_type n) const {
        return string().find_first_not_of(s, pos, n);
    }

    size_type find_first_not_of(const char *s, size_type pos = 0) const {
        return string().find_first_not_of(s, pos);
    }

    size_type find_first_not_of(char c, size_type pos = 0) const {
        return string().find_first_not_of(c, pos);
    }

    size_type find_last_not_of(const ustring &str, size_type pos = npos) const {
        return string().find_last_not_of(str.string(), pos);
    }

    size_type find_last_not_of(const std::string &str, size_type pos = npos) const {
        return string().find_last_not_of(str, pos);
    }

    size_type find_last_not_of(const char *s, size_type pos, size_type n) const {
        return string().find_last_not_of(s, pos, n);
    }

    size_type find_last_not_of(const char *s, size_type pos = npos) const {
        return string().find_last_not_of(s, pos);
    }

    size_type find_last_not_of(char c, size_type pos = npos) const {
        return string().find_last_not_of(c, pos);
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.

    int compare (const ustring& str) const {
        return (c_str() == str.c_str()) ? 0 
            : strcmp (c_str() ? c_str() : "", str.c_str() ? str.c_str() : "");
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.
    int compare (const std::string& str) const {
        return strcmp (c_str() ? c_str() : "", str.c_str());
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.
    int compare (string_view str) const {
        return strncmp (c_str() ? c_str() : "",
                        str.data() ? str.data() : "", str.length());
    }

    /// Return 0 if *this is lexicographically equal to str, -1 if
    /// *this is lexicographically earlier than str, 1 if *this is
    /// lexicographically after str.
    int compare (const char *str) const {
        return strcmp (c_str() ? c_str() : "", str ? str : "");
    }

    /// Return 0 if a is lexicographically equal to b, -1 if a is
    /// lexicographically earlier than b, 1 if a is lexicographically
    /// after b.
    friend int compare (const std::string& a, const ustring &b) {
        return strcmp (a.c_str(), b.c_str() ? b.c_str() : "");
    }

    /// Test two ustrings for equality -- are they comprised of the same
    /// sequence of characters.  Note that because ustrings are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator== (const ustring &str) const {
        return c_str() == str.c_str();
    }

    /// Test two ustrings for inequality -- are they comprised of different
    /// sequences of characters.  Note that because ustrings are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator!= (const ustring &str) const {
        return c_str() != str.c_str();
    }

    /// Test a ustring (*this) for lexicographic equality with std::string
    /// x.
    bool operator== (const std::string &x) const { return compare(x) == 0; }

    /// Test a ustring (*this) for lexicographic equality with string_view
    /// x.
    bool operator== (string_view x) const { return compare(x) == 0; }

    /// Test a ustring (*this) for lexicographic equality with char* x.
    bool operator== (const char *x) const { return compare(x) == 0; }

    /// Test for lexicographic equality between std::string a and ustring
    /// b.
    friend bool operator== (const std::string &a, const ustring &b) {
        return b.compare(a) == 0;
    }

    /// Test for lexicographic equality between string_view a and ustring
    /// b.
    friend bool operator== (string_view a, const ustring &b) {
        return b.compare(a) == 0;
    }

    /// Test for lexicographic equality between char* a and ustring
    /// b.
    friend bool operator== (const char *a, const ustring &b) {
        return b.compare(a) == 0;
    }

    /// Test a ustring (*this) for lexicographic inequality with
    /// std::string x.
    bool operator!= (const std::string &x) const { return compare(x) != 0; }

    /// Test a ustring (*this) for lexicographic inequality with
    /// string_view x.
    bool operator!= (string_view x) const { return compare(x) != 0; }

    /// Test a ustring (*this) for lexicographic inequality with
    /// char* x.
    bool operator!= (const char *x) const { return compare(x) != 0; }

    /// Test for lexicographic inequality between std::string a and
    /// ustring b.
    friend bool operator!= (const std::string &a, const ustring &b) {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic inequality between string_view a and
    /// ustring b.
    friend bool operator!= (string_view a, const ustring &b) {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic inequality between char* a and
    /// ustring b.
    friend bool operator!= (const char *a, const ustring &b) {
        return b.compare(a) != 0;
    }

    /// Test for lexicographic 'less', comes in handy for lots of STL
    /// containers and algorithms.
    bool operator< (const ustring &x) const { return compare(x) < 0; }

    /// Construct a ustring in a printf-like fashion.  In other words,
    /// something like:
    ///    ustring s = ustring::format ("blah %d %g", (int)foo, (float)bar);
    /// The argument list is fully typesafe.
    /// The formatting of the string will always use the classic "C" locale
    /// conventions (in particular, '.' as decimal separator for float values).
    template<typename... Args>
    static ustring format (string_view fmt, const Args&... args)
    {
        return ustring (Strutil::format (fmt, args...));
    }

    /// Generic stream output of a ustring.
    ///
    friend std::ostream & operator<< (std::ostream &out, const ustring &str) {
        if (str.c_str() && out.good())
            out.write (str.c_str(), str.size());
        return out;
    }

    /// Return the statistics output as a string.
    ///
    static std::string getstats (bool verbose = true);

    /// Return the amount of memory consumed by the ustring table.
    ///
    static size_t memory ();

    /// Given a string_view, return a pointer to the unique
    /// version kept in the internal table (creating a new table entry
    /// if we haven't seen this sequence of characters before).  
    /// N.B.: this is equivalent to ustring(str).c_str().  It's also the
    /// routine that is used directly by ustring's internals to generate
    /// the canonical unique copy of the characters.
    static const char * make_unique (string_view str);

    /// Is this character pointer a unique ustring representation of
    /// those characters?  Useful for diagnostics and debugging.
    static bool is_unique (const char *str) {
        return str == NULL || make_unique(str) == str;
    }

    /// Create a ustring from characters guaranteed to already be
    /// ustring-clean, without having to run through the hash yet
    /// again. Use with extreme caution!!!
    static ustring from_unique (const char *unique) {
        DASSERT (is_unique(unique));  // DEBUG builds -- check it!
        ustring u;
        u.m_chars = unique;
        return u;
    }

private:

    // Individual ustring internal representation -- the unique characters.
    //
    const char *m_chars;

public:
    // Representation within the hidden string table -- DON'T EVER CREATE
    // ONE OF THESE YOURSELF!
    // The characters are found directly after the rep.  So that means that
    // if you know the rep, the chars are at (char *)(rep+1), and if you
    // know the chars, the rep is at ((TableRep *)chars - 1).
    struct TableRep {
        size_t hashed;       // precomputed Hash value
        std::string str;     // String representation
        size_t length;       // Length of the string; must be right before cap
        size_t dummy_capacity;  // Dummy field! must be right before refcount
        int    dummy_refcount;  // Dummy field! must be right before chars
        TableRep (string_view strref, size_t hash);
        ~TableRep ();
        const char *c_str () const { return (const char *)(this + 1); }
    };

private:
    static std::string empty_std_string;
};



/// Functor class to use as a hasher when you want to make a hash_map or
/// hash_set using ustring as a key.
class ustringHash
{
public:
    size_t operator() (const ustring &s) const { return s.hash(); }
};



/// Functor class to use for comparisons when sorting ustrings, if you
/// want the strings sorted lexicographically.
class ustringLess
{
public:
    size_t operator() (ustring a, ustring b) const {return a<b; }
};


/// Functor class to use for comparisons when sorting ustrings, if you
/// don't care if the sort order is lexicographic. This sorts based on
/// the pointers themselves, which is safe because once allocated, a
/// ustring's characters will never be moved. But beware, the resulting
/// sorting order may vary from run to run!
class ustringPtrIsLess
{
public:
    size_t operator() (ustring a, ustring b) const {
        return size_t(a.data()) < size_t(b.data());
    }
};



/// Case-insensitive comparison of ustrings.  For speed, this always
/// uses a static locale that doesn't require a mutex lock.
inline bool iequals (ustring a, ustring b) {
    return a==b || Strutil::iequals(a.string(), b.string());
}
inline bool iequals (ustring a, const std::string &b) {
    return Strutil::iequals(a.string(), b);
}
inline bool iequals (const std::string &a, ustring b) {
    return Strutil::iequals(a, b.string());
}



// ustring variant stof from OpenImageIO/strutil.h
namespace Strutil {
inline int stof (ustring s) { return Strutil::stof (s.string()); }
template<> inline std::string to_string (const ustring& value) { return value.string(); }
} // end namespace Strutil

OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_USTRING_H
