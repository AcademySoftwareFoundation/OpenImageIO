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



/////////////////////////////////////////////////////////////////////////////
// Token class
//
// A Token is an alternative to char* or std::string for storing strings
// (hereafter jointly referred to as "standard strings").  Tokens are
// designed to make certain string operations extremely inexpensive.
// They are NOT designed to be a general replacement for standard
// strings, because certain other operations are a little clunky.  Read
// below and choose wisely.
//
// The implementation is that behind the scenes there is a hash set of
// allocated strings, so the characters of each string are unique.  A
// Token itself is a pointer to the characters of one of these canonical
// strings.  Therefore, assignment and equality testing is just a single
// 32- or 64-bit int operation, the only mutex is when a Token is
// created from raw characters, and the only malloc is the first time
// each canonical Token is created.
//
// Compared to standard strings, Tokens have several advantages:
// 
//   * Each individual Token is very small -- just a single pointer.
//     In fact, we guarantee that a Token is the same size and memory
//     layout as an ordinary char*.
//   * Overall storage is frugal since there is only one allocated copy
//     of each unique character sequence, throughout the entire lifetime
//     of the program.
//   * Assignment from one token to another is just copy of the pointer,
//     a single integer operation.  No allocation, no character copying,
//     no reference counting.
//   * Equality testing (do the strings contain the same characters) is
//     a single operation, the comparison of the pointer.
//   * Extraction of the pointer to the characters is trivial, since the
//     Token itself is just the pointer.
//   * It's very thread-safe and thread-friendly -- mutexes only occur
//     when a new Token is constructed from raw characters.
//   * Memory allocation only occurs when a new Token is construted from
//     raw characters the FIRST time -- subsequent constructions of the
//     same string just finds it in the canonial string set, but doesn't
//     need to allocate new storage.  Destruction of a Token is trivial,
//     there is no de-allocation because the canonical version stays in
//     the set.  Also, therefore, no user code mistake can lead to
//     memory leaks.
//
// But there are some problems, too.  Canonical strings are never freed
// from the table.  So in some sense all the strings "leak", but they
// only leak one copy for each unique string that the program ever comes
// across.  Also, creation of unique strings from raw characters is more
// expensive than for standard strings.
//
// On the whole, Tokens are a really great string representation
//   * if you tend to have (relatively) few unique strings, but many
//     copies of those strings;
//   * if the creation of strings from raw characters is relatively
//     rare compared to copying existing strings;
//   * if you tend to make the same strings over and over again, and
//     if it's relatively rare that a single unique character sequence
//     is used only once in the entire lifetime of the program;
//   * if your most common string operations are assignment and equality
//     testing and you want them to be as fast as possible;
//   * if you are doing relatively little character-by-character assembly
//     of strings, string concatenation, or other "string manipulation"
//     (other than equality testing).
//
// Tokens are not so hot
//   * if your program tends to have very few copies of each character
//     sequence over the entire lifetime of the program;
//   * if your program tends to generate a huge variety of unique
//     strings over its lifetime, each of which is used only a short
//     time and then discarded, never to be needed again;
//   * if you don't need to do a lot of string assignment or equality
//     testing, but lots of more complex string manipulation.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include "export.h"


// FIXME: want a namespace 
// namespace blah {


class DLLPUBLIC Token {
public:
    /// Default ctr for Token -- make it like a NULL string
    ///
    Token (void) : m_chars(NULL) { }

    /// Construct a Token from a C string (char *).
    ///
    explicit Token (const char *s = NULL);

    /// Construct a Token from a C++ std::string.
    ///
    explicit Token (const std::string &s);

    /// Copy construct a Token from another Token.
    ///
    Token (const Token &x) : m_chars(x.m_chars) { }

    /// Token destructor.
    ///
    ~Token () { }

    /// Assign a Token to another Token.
    ///
    const Token & operator= (const Token &x) {
        m_chars = x.m_chars;
        return *this;
    }

    /// Assign a C string (char *) to a Token.
    ///
    const Token & operator= (const char *s) {
        *this = Token(s);
        return *this;
    }

    /// Assign a C++ std::string to a Token.
    ///
    const Token & operator= (const std::string &s) {
        *this = Token(s);
        return *this;
    }

    /// Return a C string representation of a Token.
    ///
    const char *c_str () const { return m_chars; }

    /// Return a C++ std::string representation of a Token.
    ///
    const std::string &string () const;

    /// Reset to the NULL token.
    ///
    void clear (void) { m_chars = NULL; }

    /// Is the string empty -- i.e., is it the NULL pointer or does it
    /// point to an empty string?
    bool empty (void) const { return (m_chars == NULL) || (*m_chars == 0); }

    /// Test two Tokens for equality -- are they comprised of the same
    /// sequence of characters.  Note that because Tokens are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator== (const Token &x) { return (m_chars == x.m_chars); }

    /// Test two Tokens for inequality -- are they comprised of different
    /// sequences of characters.  Note that because Tokens are unique,
    /// this is a trivial pointer comparison, not a char-by-char loop as
    /// would be the case with a char* or a std::string.
    bool operator!= (const Token &x) { return (m_chars != x.m_chars); }

    /// Cast to int, which is interpreted as testing whether it's not an
    /// empty string.  This allows you to write "if (t)" with the same
    /// semantics as if it were a char*.
    operator int (void) { return !empty(); }

    /// Construct a token in a printf-like fashion.
    ///
    static Token format (const char *fmt, ...);

    /// Generic stream output of a Token.
    ///
    friend std::ostream & operator<< (std::ostream &out, const Token &tok) {
        if (tok.c_str())
            out << tok.c_str();
        return out;
    }

private:
    const char *m_chars;    ///< Pointer to the unique characters
};



class TokenHash
#ifdef WINNT
    : public hash_compare<Token>
#endif
{
public:
    size_t operator() (Token s) const { return (size_t)s.c_str(); }
    bool operator() (Token a, Token b) {
        return strcmp (a.c_str(), b.c_str()) < 0;
    }
};




// };  // end namespace blah

#endif // TOKEN_H
