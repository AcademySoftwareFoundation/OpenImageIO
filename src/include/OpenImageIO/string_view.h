/*
  Copyright 2014 Larry Gritz and the other authors and contributors.
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


#pragma once

#include <cstddef>
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <ostream>

#include "oiioversion.h"
#include "export.h"


OIIO_NAMESPACE_BEGIN


/// string_view : a non-owning, non-copying, non-allocating reference to a
/// sequence of characters.  It encapsulates both a character pointer and a
/// length.
///
/// A function that takes a string input (but does not need to alter the
/// string in place) may use a string_view parameter and accept input that
/// is any of char* (C string), string literal (constant char array), a
/// std::string (C++ string), or OIIO ustring.  For all of these cases, no
/// extra allocations are performed, and no extra copies of the string
/// contents are performed (as they would be, for example, if the function
/// took a const std::string& argument but was passed a char* or string
/// literal).
///
/// Furthermore, a function that returns a copy or a substring of one of its
/// inputs (for example, a substr()-like function) may return a string_view
/// rather than a std::string, and thus generate its return value without
/// any allocation or copying. Upon assignment to a std::string or ustring,
/// it will properly auto-convert.
///
/// There are two important caveats to using this class:
/// 1. The string_view merely refers to characters owned by another string,
///    so the string_view may not be used outside the lifetime of the string
///    it refers to. Thus, string_view is great for parameter passing, but
///    it's not a good idea to use a string_view to store strings in a data
///    structure (unless you are really sure you know what you're doing).
/// 2. Because the run of characters that the string_view refers to may not
///    be 0-terminated, it is important to distinguish between the data()
///    method, which returns the pointer to the characters, and the c_str()
///    method, which is guaranteed to return a valid C string that is
///    0-terminated. Thus, if you want to pass the contents of a string_view
///    to a function that expects a 0-terminated string (say, fopen), you
///    must call fopen(my_string_view.c_str()).  Note that the usual case
///    is that the string_view does refer to a 0-terminated string, and in
///    that case c_str() returns the same thing as data() without any extra
///    expense; but in the rare case that it is not 0-terminated, c_str()
///    will incur extra expense to internally allocate a valid C string.
///


class OIIO_API string_view {
public:
    typedef char charT;
    typedef charT value_type;
    typedef const charT* pointer;
    typedef const charT* const_pointer;
    typedef const charT& reference;
    typedef const charT& const_reference;
    typedef const_pointer const_iterator;
    typedef const_iterator iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef const_reverse_iterator reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef std::char_traits<charT> traits;
    static const size_type npos = ~size_type(0);

    /// Default ctr
    string_view () { init(NULL,0); }
    /// Copy ctr
    string_view (const string_view &copy) {
        init (copy.m_chars, copy.m_len);
    }
    /// Construct from char* and length.
    string_view (const charT *chars, size_t len) { init (chars, len); }
    /// Construct from char*, use strlen to determine length.
    string_view (const charT *chars) {
        init (chars, chars ? strlen(chars) : 0);
    }
    /// Construct from std::string.
    string_view (const std::string &str) { init (str.data(), str.size()); }

    std::string str() const {
        return (m_chars ? std::string(m_chars,m_len) : std::string());
    }

    /// Explicitly request a 0-terminated string. USUALLY, this turns out to
    /// be just data(), with no significant added expense. But in the more
    /// rare case that the string_view represetns a non-0-terminated
    /// substring, it will force an allocation and copy underneath.
    const char * c_str() const;

    // assignments
    string_view& operator= (const string_view &copy) {
        init (copy.data(), copy.length());
        return *this;
    }

    operator std::string() const { return str(); }

    // iterators
    const_iterator begin() const { return m_chars; }
    const_iterator end() const { return m_chars + m_len; }
    const_iterator cbegin() const { return m_chars; }
    const_iterator cend() const { return m_chars + m_len; }
    const_reverse_iterator rbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator (begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator (begin()); }

    // capacity
    size_type size() const { return m_len; }
    size_type length() const { return m_len; }
    size_type max_size() const { return m_len; }
    bool empty() const { return m_len == 0; }

    // element access
    const charT& operator[] (size_type pos) const { return m_chars[pos]; }
    const charT& at (size_t pos) const {
        if (pos >= m_len)
            throw (std::out_of_range ("OpenImageIO::string_view::at"));
        return m_chars[pos];
    }
    const charT& front() const { return m_chars[0]; }
    const charT& back() const { return m_chars[m_len-1]; }
    const charT* data() const { return m_chars; }

    // modifiers
    void clear() { init(NULL,0); }
    void remove_prefix(size_type n) {
        if (n > m_len)
            n = m_len;
        m_chars += n;
        m_len -= n;
    }
    void remove_suffix(size_type n) {
        if (n > m_len)
            n = m_len;
        m_len -= n;
    }

    string_view substr (size_type pos, size_type n=npos) const {
        if (pos > size())
            return string_view();  // start past end -> return empty
        if (n == npos || pos + n > size())
            n = size() - pos;
        return string_view (data() + pos, n);
    }

    int compare (string_view x) const {
        const int cmp = std::char_traits<char>::compare (m_chars, x.m_chars, (std::min)(m_len, x.m_len));
        return cmp != 0 ? cmp : int(m_len - x.m_len);
        // Equivalent to:
        //  cmp != 0 ? cmp : (m_len == x.m_len ? 0 : (m_len < x.m_len ? -1 : 1));
    }

#if 0
    // Do these later if anybody needs them
    bool starts_with(string_view x) const;
    bool ends_with(string_view x) const;
#endif

    /// Find the first occurrence of substring s in *this, starting at
    /// position pos.
    size_type find (string_view s, size_t pos=0) const {
        if (pos > size())
            pos = size();
        const_iterator i = std::search (this->cbegin()+pos, this->cend(),
                                        s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    /// Find the first occurrence of character c in *this, starting at
    /// position pos.
    size_type find (charT c, size_t pos=0) const {
        if (pos > size())
            pos = size();
        const_iterator i = std::find_if (this->cbegin()+pos, this->cend(),
                                         traits_eq(c));
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    /// Find the last occurrence of substring s *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (string_view s, size_t pos=npos) const {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::search (b, e, s.crbegin(), s.crend(), traits::eq);
        return i == e ? npos : (reverse_distance(this->crbegin(),i) - s.size() + 1);
    }

    /// Find the last occurrence of character c in *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (charT c, size_t pos=npos) const {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::find_if (b, e, traits_eq(c));
        return i == e ? npos : reverse_distance (this->crbegin(),i);
    }

    size_type find_first_of (charT c, size_t pos=0) const { return find (c, pos); }

    size_type find_last_of (charT c, size_t pos=npos) const { return rfind (c, pos); }

    size_type find_first_of (string_view s) const {
        const_iterator i = std::find_first_of (this->cbegin(), this->cend(),
                                               s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_last_of (string_view s) const {
        const_reverse_iterator i = std::find_first_of (this->crbegin(), this->crend(),
                                                       s.cbegin(), s.cend(), traits::eq);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_first_not_of (string_view s) const {
        const_iterator i = find_not_of (this->cbegin(), this->cend(), s);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_first_not_of (charT c) const {
        for (const_iterator i = this->cbegin(); i != this->cend(); ++i)
            if (! traits::eq (c, *i))
                return std::distance (this->cbegin(), i);
        return npos;
    }

    size_type find_last_not_of (string_view s) const {
        const_reverse_iterator i = find_not_of (this->crbegin(), this->crend(), s);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_last_not_of (charT c) const {
        for (const_reverse_iterator i = this->crbegin(); i != this->crend(); ++i)
            if (! traits::eq (c, *i))
                return reverse_distance (this->crbegin(), i);
        return npos;
    }

private:
    const charT * m_chars;
    size_t m_len;

    void init (const charT *chars, size_t len) {
        m_chars = chars;
        m_len = len;
    }

    template <typename r_iter>
    size_type reverse_distance (r_iter first, r_iter last) const {
        return m_len - 1 - std::distance (first, last);
    }

    template <typename iter>
    iter find_not_of (iter first, iter last, string_view s) const {
        for ( ; first != last ; ++first)
            if (! traits::find (s.data(), s.length(), *first))
                return first;
        return last;
    }

    class traits_eq {
    public:
        traits_eq (charT ch) : ch(ch) {}
        bool operator () (charT val) const { return traits::eq (ch, val); }
        charT ch;
    };

};



inline bool operator== (string_view x, string_view y) {
    return x.size() == y.size() ? (x.compare (y) == 0) : false;
}

inline bool operator!= (string_view x, string_view y) {
    return x.size() == y.size() ? (x.compare (y) != 0) : true;
}

inline bool operator< (string_view x, string_view y) {
    return x.compare(y) < 0;
}

inline bool operator> (string_view x, string_view y) {
    return x.compare(y) > 0;
}

inline bool operator<= (string_view x, string_view y) {
    return x.compare(y) <= 0;
}

inline bool operator>= (string_view x, string_view y) {
    return x.compare(y) >= 0;
}



// Inserter
inline std::ostream& operator<< (std::ostream& out, const string_view& str) {
    if (out.good())
        out.write (str.data(), str.size());
    return out;
}



// Temporary name equivalence
typedef string_view string_ref;


OIIO_NAMESPACE_END
