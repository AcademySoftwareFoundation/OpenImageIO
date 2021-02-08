// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

// clang-format off

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>


OIIO_NAMESPACE_BEGIN


/// A `string_view` is a non-owning, non-copying, non-allocating reference
/// to a sequence of characters.  It encapsulates both a character pointer
/// and a length.
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
    using charT = char;
    using Traits = std::char_traits<charT>;  // standard name
    using traits_type = Traits;
    using value_type = charT;
    using pointer = const charT*;
    using const_pointer = const charT*;
    using reference = const charT&;
    using const_reference = const charT&;
    using const_iterator = const_pointer;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using traits = std::char_traits<charT>;  // obsolete custom name
    static const size_type npos = ~size_type(0);

    /// Default ctr
    constexpr string_view() noexcept : m_chars(nullptr), m_len(0) { }

    /// Copy ctr
    constexpr string_view(const string_view& copy)
        : m_chars(copy.data()), m_len(copy.size()) { }

    /// Construct from char* and length.
    constexpr string_view(const charT* chars, size_t len)
        : m_chars(chars), m_len(len) { }

    /// Construct from char*, use strlen to determine length.
    OIIO_CONSTEXPR17 string_view(const charT* chars)
        : m_chars(chars), m_len(chars ? Traits::length(chars) : 0) { }
    // N.B. char_traits::length() is constexpr starting with C++17.

    /// Construct from std::string. Remember that a string_view doesn't have
    /// its own copy of the characters, so don't use the `string_view` after
    /// the original string has been destroyed or altered.
    OIIO_CONSTEXPR20 string_view(const std::string& str) noexcept
        : m_chars(str.data()), m_len(str.size()) { }
    // N.B. std::string::size() is constexpr starting with C++20.

    /// Convert a string_view to a `std::string`.
    OIIO_CONSTEXPR20 std::string str() const
    {
        return (m_chars ? std::string(m_chars, m_len) : std::string());
        // N.B. std::string ctr from chars+len is constexpr in C++20.
    }

    /// Explicitly request a 0-terminated string. USUALLY, this turns out to
    /// be just data(), with no significant added expense (because most uses
    /// of string_view are simple wrappers of C strings, C++ std::string, or
    /// ustring -- all of which are 0-terminated). But in the more rare case
    /// that the string_view represents a non-0-terminated substring, it
    /// will force an allocation and copy underneath.
    ///
    /// Caveats:
    /// 1. This is NOT going to be part of the C++17 std::string_view, so
    ///    it's probably best to avoid this method if you want to have 100%
    ///    drop-in compatibility with std::string_view.
    /// 2. It is NOT SAFE to use c_str() on a string_view whose last char
    ///    is the end of an allocation -- because that next char may only
    ///    *coincidentally* be a '\0', which will cause c_str() to return
    ///    the string start (thinking it's a valid C string, so why not just
    ///    return its address?), if there's any chance that the subsequent
    ///    char could change from 0 to non-zero during the use of the result
    ///    of c_str(), and thus break the assumption that it's a valid C str.
    const char* c_str() const;

    // Assignment
    OIIO_CONSTEXPR14 string_view& operator=(const string_view& copy) noexcept = default;

    /// Convert a string_view to a `std::string`.
    OIIO_CONSTEXPR20 operator std::string() const { return str(); }

    // iterators
    constexpr iterator begin() const noexcept { return m_chars; }
    constexpr iterator end() const noexcept { return m_chars + m_len; }
    constexpr const_iterator cbegin() const noexcept { return m_chars; }
    constexpr const_iterator cend() const noexcept { return m_chars + m_len; }
    OIIO_CONSTEXPR17 reverse_iterator rbegin() const noexcept { return reverse_iterator (end()); }
    OIIO_CONSTEXPR17 reverse_iterator rend() const noexcept { return reverse_iterator (begin()); }
    OIIO_CONSTEXPR17 const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator (cend()); }
    OIIO_CONSTEXPR17 const_reverse_iterator crend() const noexcept { return const_reverse_iterator (cbegin()); }

    // capacity
    constexpr size_type size() const noexcept { return m_len; }
    constexpr size_type length() const noexcept { return m_len; }
    constexpr size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max();
    }
    /// Is the string_view empty, containing no characters?
    constexpr bool empty() const noexcept { return m_len == 0; }

    /// Element access of an individual character (beware: no bounds
    /// checking!).
    constexpr const_reference operator[](size_type pos) const { return m_chars[pos]; }
    /// Element access with bounds checking and exception if out of bounds.
    OIIO_CONSTEXPR17 const_reference at(size_t pos) const
    {
        if (pos >= m_len)
            throw(std::out_of_range("OpenImageIO::string_view::at"));
        return m_chars[pos];
    }
    constexpr const_reference front() const { return m_chars[0]; }
    constexpr const_reference back() const { return m_chars[m_len - 1]; }
    constexpr const_pointer data() const noexcept { return m_chars; }

    // modifiers
    OIIO_CONSTEXPR14 void clear() noexcept { init(nullptr, 0); }
    OIIO_CONSTEXPR14 void remove_prefix(size_type n) noexcept
    {
        if (n > m_len)
            n = m_len;
        m_chars += n;
        m_len -= n;
    }
    OIIO_CONSTEXPR14 void remove_suffix(size_type n) noexcept
    {
        if (n > m_len)
            n = m_len;
        m_len -= n;
    }

    OIIO_CONSTEXPR14 string_view substr(size_type pos, size_type n = npos) const noexcept
    {
        if (pos >= size())
            return string_view();  // start past end -> return empty
        if (n == npos || pos + n > size())
            n = size() - pos;
        return string_view(data() + pos, n);
    }

    OIIO_CONSTEXPR17 int compare (string_view x) const noexcept {
        // N.B. char_traits<char>::compare is constexpr for C++17
        const int cmp = traits_type::compare (m_chars, x.m_chars, (std::min)(m_len, x.m_len));
        return cmp != 0 ? cmp : int(m_len - x.m_len);
        // Equivalent to:
        //  cmp != 0 ? cmp : (m_len == x.m_len ? 0 : (m_len < x.m_len ? -1 : 1));
    }

#if 0
    // Do these later if anybody needs them
    bool starts_with(string_view x) const noexcept;
    bool ends_with(string_view x) const noexcept;
    size_type copy(CharT* dest, size_type count, size_type pos = 0) const;
#endif

    /// Find the first occurrence of substring s in *this, starting at
    /// position pos.
    size_type find(string_view s, size_t pos = 0) const noexcept
    {
        if (pos > size())
            pos = size();
        const_iterator i = std::search(this->cbegin() + pos, this->cend(),
                                       s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance(this->cbegin(), i);
    }

    /// Find the first occurrence of character c in *this, starting at
    /// position pos.
    size_type find (charT c, size_t pos=0) const noexcept {
        if (pos > size())
            pos = size();
        const_iterator i = std::find_if (this->cbegin()+pos, this->cend(),
                                         traits_eq(c));
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    /// Find the last occurrence of substring s *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::search (b, e, s.crbegin(), s.crend(), traits::eq);
        return i == e ? npos : (reverse_distance(this->crbegin(),i) - s.size() + 1);
    }

    /// Find the last occurrence of character c in *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (charT c, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::find_if (b, e, traits_eq(c));
        return i == e ? npos : reverse_distance (this->crbegin(),i);
    }

    size_type find_first_of (charT c, size_t pos=0) const noexcept { return find (c, pos); }

    size_type find_last_of (charT c, size_t pos=npos) const noexcept { return rfind (c, pos); }

    size_type find_first_of (string_view s, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        const_iterator i = std::find_first_of (this->cbegin()+pos, this->cend(),
                                               s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_last_of (string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        const_reverse_iterator i = std::find_first_of (this->crbegin()+off, this->crend(),
                                                       s.cbegin(), s.cend(), traits::eq);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_first_not_of (string_view s, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        const_iterator i = find_not_of (this->cbegin()+pos, this->cend(), s);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_first_not_of (charT c, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        for (const_iterator i = this->cbegin()+pos; i != this->cend(); ++i)
            if (! traits::eq (c, *i))
                return std::distance (this->cbegin(), i);
        return npos;
    }

    size_type find_last_not_of (string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        const_reverse_iterator i = find_not_of (this->crbegin()+off, this->crend(), s);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_last_not_of (charT c, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        for (const_reverse_iterator i = this->crbegin()+off; i != this->crend(); ++i)
            if (! traits::eq (c, *i))
                return reverse_distance (this->crbegin(), i);
        return npos;
    }

private:
    const charT* m_chars = nullptr;
    size_t m_len = 0;

    OIIO_CONSTEXPR14 void init(const charT* chars, size_t len) noexcept
    {
        m_chars = chars;
        m_len   = len;
    }

    template<typename r_iter>
    size_type reverse_distance(r_iter first, r_iter last) const noexcept
    {
        return m_len - 1 - std::distance(first, last);
    }

    template<typename iter>
    iter find_not_of(iter first, iter last, string_view s) const noexcept
    {
        for (; first != last; ++first)
            if (!traits::find(s.data(), s.length(), *first))
                return first;
        return last;
    }

    class traits_eq {
    public:
        constexpr traits_eq (charT ch) noexcept : ch(ch) {}
        constexpr bool operator () (charT val) const noexcept { return traits::eq (ch, val); }
        charT ch;
    };
};



inline OIIO_CONSTEXPR17 bool
operator==(string_view x, string_view y) noexcept
{
    return x.size() == y.size() ? (x.compare(y) == 0) : false;
}

inline OIIO_CONSTEXPR17 bool
operator!=(string_view x, string_view y) noexcept
{
    return x.size() == y.size() ? (x.compare(y) != 0) : true;
}

inline OIIO_CONSTEXPR17 bool
operator<(string_view x, string_view y) noexcept
{
    return x.compare(y) < 0;
}

inline OIIO_CONSTEXPR17 bool
operator>(string_view x, string_view y) noexcept
{
    return x.compare(y) > 0;
}

inline OIIO_CONSTEXPR17 bool
operator<=(string_view x, string_view y) noexcept
{
    return x.compare(y) <= 0;
}

inline OIIO_CONSTEXPR17 bool
operator>=(string_view x, string_view y) noexcept
{
    return x.compare(y) >= 0;
}



// Inserter
inline std::ostream&
operator<<(std::ostream& out, const string_view& str)
{
    if (out.good())
        out.write(str.data(), str.size());
    return out;
}



// Temporary name equivalence
typedef string_view string_ref;


OIIO_NAMESPACE_END



// Declare std::size and std::ssize for our string_view.
namespace std {

#if OIIO_CPLUSPLUS_VERSION < 17
constexpr size_t size(const OIIO::string_view& c) { return c.size(); }
#endif

#if OIIO_CPLUSPLUS_VERSION < 20
constexpr ptrdiff_t ssize(const OIIO::string_view& c) {
    return static_cast<ptrdiff_t>(c.size());
}
#endif

// Allow client software to easily know if the std::size/ssize was added for
// our string_view.
#define OIIO_STRING_VIEW_HAS_STD_SIZE 1


} // namespace std
