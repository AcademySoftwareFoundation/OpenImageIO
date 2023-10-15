// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
#include <OpenImageIO/detail/fmt.h>

// Some compilers already have a string_view pre-C++17
// N.B. This logic is taken from fmtlib.
#if (__has_include(<string_view>) &&                                    \
     (__cplusplus > 201402L || defined(_LIBCPP_VERSION))) ||            \
    (defined(_MSVC_LANG) && _MSVC_LANG > 201402L && _MSC_VER >= 1910)
#    include <string_view>
#    define OIIO_STD_STRING_VIEW_AVAILABLE
#elif __has_include("experimental/string_view")
#    include <experimental/string_view>
#    define OIIO_EXPERIMENTAL_STRING_VIEW_AVAILABLE
#endif


OIIO_NAMESPACE_BEGIN


/// A `string_view` is a non-owning, non-copying, non-allocating reference
/// to a sequence of characters.  It encapsulates both a character pointer
/// and a length. This is analogous to C++17 std::string_view, but supports
/// C++14.
///
/// Note: `string_view` is an alias for `basic_string_view<char>`.
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
///    method, which returns the pointer to the characters, and a proper
///    c_str() method (which is NOT provided by std::string_view), which would
///    be guaranteed to return a valid C string that is 0-terminated. Thus, if
///    you want to pass the contents of a string_view to a function that
///    expects a 0-terminated string (say, fopen), the usual practice is to
///    call `fopen(std::string(my_string_view).c_str())`.
///


template<class CharT, class Traits = std::char_traits<CharT>>
class basic_string_view {
public:
    using charT = CharT;  // DEPRECATED(2.4)
    using traits_type = Traits;
    using value_type = CharT;
    using pointer = const CharT*;
    using const_pointer = const CharT*;
    using reference = const CharT&;
    using const_reference = const CharT&;
    using const_iterator = const_pointer;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using traits = std::char_traits<CharT>;  // obsolete custom name
    using string = std::basic_string<CharT, Traits>;
    static const size_type npos = ~size_type(0);

    /// Default ctr
    constexpr basic_string_view() noexcept : m_chars(nullptr), m_len(0) { }

    /// Copy ctr
    constexpr basic_string_view(const basic_string_view& copy)
        : m_chars(copy.data()), m_len(copy.size()) { }

    /// Construct from char* and length.
    constexpr basic_string_view(const CharT* chars, size_t len) noexcept
        : m_chars(chars), m_len(len) { }

    /// Construct from char*, use strlen to determine length.
    constexpr basic_string_view(const CharT* chars) noexcept
        : m_chars(chars), m_len(chars ? cestrlen(chars) : 0) { }

    /// Construct from std::string. Remember that a string_view doesn't have
    /// its own copy of the characters, so don't use the `string_view` after
    /// the original string has been destroyed or altered.
    basic_string_view(const string& str) noexcept
        : m_chars(str.data()), m_len(str.size()) { }
    // N.B. std::string::size() is constexpr starting with C++20.

#if defined(OIIO_STD_STRING_VIEW_AVAILABLE) || defined(OIIO_DOXYGEN)
    // Construct from a std::string_view.
    constexpr basic_string_view(const std::basic_string_view<CharT, Traits>& sv) noexcept
        : m_chars(sv.data()), m_len(sv.size()) { }
#endif

#ifdef OIIO_EXPERIMENTAL_STRING_VIEW_AVAILABLE
    // Construct from a std::experimental::string_view.
    constexpr basic_string_view(const std::experimental::basic_string_view<CharT, Traits>& sv) noexcept
        : m_chars(sv.data()), m_len(sv.size()) { }
#endif

    /// Convert a string_view to a `std::string`. NOTE: the `str()` method is
    /// not part of the C++17 std::string_view. If strict interchangeability
    /// with std::string_view is desired, you might prefer the equivalent
    /// idiom `std::string(sv)`.
    OIIO_CONSTEXPR20 string str() const
    {
        return *this;
        // return (m_chars ? string(m_chars, m_len) : string());
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
    const CharT* c_str() const;

    // Assignment
    OIIO_CONSTEXPR14 basic_string_view& operator=(const basic_string_view& copy) noexcept = default;

    /// Convert a string_view to a `std::string`.
    operator std::basic_string<CharT, Traits>() const {
        return (m_chars ? string(m_chars, m_len) : std::basic_string<CharT, Traits>());
    }

#if defined(OIIO_STD_STRING_VIEW_AVAILABLE) || defined(OIIO_DOXYGEN)
    // Convert an OIIO::string_view to a std::string_view.
    constexpr operator std::basic_string_view<CharT, Traits>() const
    {
        return { data(), size() };
    }
#endif

#ifdef OIIO_EXPERIMENTAL_STRING_VIEW_AVAILABLE
    // Convert an OIIO::string_view to a std::experimental::string_view.
    constexpr operator std::experimental::basic_string_view<CharT, Traits>() const noexcept
    {
        return { data(), size() };
    }
#endif

#ifdef FMT_VERSION
    // Convert an OIIO::string_view to a fmt::string_view. This enables
    // fmt::format() and friends to accept an OIIO::string_view.
    constexpr operator fmt::string_view() const noexcept { return { data(), size() }; }
#endif

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
    /// Is the basic_string_view empty, containing no characters?
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

    OIIO_CONSTEXPR14 basic_string_view substr(size_type pos, size_type n = npos) const noexcept
    {
        if (pos >= size())
            return basic_string_view();  // start past end -> return empty
        if (n == npos || pos + n > size())
            n = size() - pos;
        return basic_string_view(data() + pos, n);
    }

    OIIO_CONSTEXPR17 int compare (basic_string_view x) const noexcept {
        // N.B. char_traits<char>::compare is constexpr for C++17
        const int cmp = traits_type::compare (m_chars, x.m_chars, (std::min)(m_len, x.m_len));
        return cmp != 0 ? cmp : int(m_len) - int(x.m_len);
        // Equivalent to:
        //  cmp != 0 ? cmp : (m_len == x.m_len ? 0 : (m_len < x.m_len ? -1 : 1));
    }

#if 0
    // Do these later if anybody needs them
    bool starts_with(basic_string_view x) const noexcept;
    bool ends_with(basic_string_view x) const noexcept;
    size_type copy(CharT* dest, size_type count, size_type pos = 0) const;
#endif

    /// Find the first occurrence of substring s in *this, starting at
    /// position pos.
    size_type find(basic_string_view s, size_t pos = 0) const noexcept
    {
        if (pos > size())
            pos = size();
        const_iterator i = std::search(this->cbegin() + pos, this->cend(),
                                       s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance(this->cbegin(), i);
    }

    /// Find the first occurrence of character c in *this, starting at
    /// position pos.
    size_type find (CharT c, size_t pos=0) const noexcept {
        if (pos > size())
            pos = size();
        const_iterator i = std::find_if (this->cbegin()+pos, this->cend(),
                                         traits_eq(c));
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    /// Find the last occurrence of substring s *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (basic_string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::search (b, e, s.crbegin(), s.crend(), traits::eq);
        return i == e ? npos : (reverse_distance(this->crbegin(),i) - s.size() + 1);
    }

    /// Find the last occurrence of character c in *this, but only those
    /// occurrences earlier than position pos.
    size_type rfind (CharT c, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        const_reverse_iterator b = this->crbegin()+(size()-pos);
        const_reverse_iterator e = this->crend();
        const_reverse_iterator i = std::find_if (b, e, traits_eq(c));
        return i == e ? npos : reverse_distance (this->crbegin(),i);
    }

    size_type find_first_of (CharT c, size_t pos=0) const noexcept { return find (c, pos); }

    size_type find_last_of (CharT c, size_t pos=npos) const noexcept { return rfind (c, pos); }

    size_type find_first_of (basic_string_view s, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        const_iterator i = std::find_first_of (this->cbegin()+pos, this->cend(),
                                               s.cbegin(), s.cend(), traits::eq);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_last_of (basic_string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        const_reverse_iterator i = std::find_first_of (this->crbegin()+off, this->crend(),
                                                       s.cbegin(), s.cend(), traits::eq);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_first_not_of (basic_string_view s, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        const_iterator i = find_not_of (this->cbegin()+pos, this->cend(), s);
        return i == this->cend() ? npos : std::distance (this->cbegin(), i);
    }

    size_type find_first_not_of (CharT c, size_t pos=0) const noexcept {
        if (pos >= size())
            return npos;
        for (const_iterator i = this->cbegin()+pos; i != this->cend(); ++i)
            if (! traits::eq (c, *i))
                return std::distance (this->cbegin(), i);
        return npos;
    }

    size_type find_last_not_of (basic_string_view s, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        const_reverse_iterator i = find_not_of (this->crbegin()+off, this->crend(), s);
        return i == this->crend() ? npos : reverse_distance (this->crbegin(), i);
    }

    size_type find_last_not_of (CharT c, size_t pos=npos) const noexcept {
        if (pos > size())
            pos = size();
        size_t off = size()-pos;
        for (const_reverse_iterator i = this->crbegin()+off; i != this->crend(); ++i)
            if (! traits::eq (c, *i))
                return reverse_distance (this->crbegin(), i);
        return npos;
    }

    friend OIIO_CONSTEXPR17 bool
    operator==(basic_string_view x, basic_string_view y) noexcept
    {
        return x.size() == y.size() ? (x.compare(y) == 0) : false;
    }

    friend OIIO_CONSTEXPR17 bool
    operator!=(basic_string_view x, basic_string_view y) noexcept
    {
        return x.size() == y.size() ? (x.compare(y) != 0) : true;
    }

    friend OIIO_CONSTEXPR17 bool
    operator<(basic_string_view x, basic_string_view y) noexcept
    {
        return x.compare(y) < 0;
    }

    friend OIIO_CONSTEXPR17 bool
    operator>(basic_string_view x, basic_string_view y) noexcept
    {
        return x.compare(y) > 0;
    }

    friend OIIO_CONSTEXPR17 bool
    operator<=(basic_string_view x, basic_string_view y) noexcept
    {
        return x.compare(y) <= 0;
    }

    friend OIIO_CONSTEXPR17 bool
    operator>=(basic_string_view x, basic_string_view y) noexcept
    {
        return x.compare(y) >= 0;
    }

    friend std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& out,
               const basic_string_view& str)
    {
        if (out.good())
            out.write(str.data(), str.size());
        return out;
    }

private:
    const CharT* m_chars = nullptr;
    size_t m_len = 0;

    OIIO_CONSTEXPR14 void init(const CharT* chars, size_t len) noexcept
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
    iter find_not_of(iter first, iter last, basic_string_view s) const noexcept
    {
        for (; first != last; ++first)
            if (!traits::find(s.data(), s.length(), *first))
                return first;
        return last;
    }

    // Guaranteed constexpr length of a C string
    static constexpr size_t cestrlen(const charT* chars) {
#if OIIO_CPLUSPLUS_VERSION >= 17
        return Traits::length(chars);
#else
        if (chars == nullptr)
            return 0;
        size_t len = 0;
        while (chars[len] != 0)
            len++;
        return len;
#endif
    }

    class traits_eq {
    public:
        constexpr traits_eq (CharT ch) noexcept : ch(ch) {}
        constexpr bool operator () (CharT val) const noexcept { return traits::eq (ch, val); }
        CharT ch;
    };
};



/// string_view is an alias for `basic_string_view<char>`. This is the
/// common use case.
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;



// DEPRECATED name equivalence
OIIO_DEPRECATED("Use string_view (2.3)")
typedef string_view string_ref;


/// Return a safe pointer to a null-terminated C string with the contents of
/// the string_view.
///
/// ENORMOUS CAVEAT: This nonstandard functionality is only safe if the
/// string_view is a true "subset of a C/C++ string". It will fail in the
/// unfortunate case where the last character of `str` is the last byte of a
/// readable page of memory and the next character is on a page that's not
/// readable. This can never happen to a string_view that was constructed from
/// a C string, a C++ std::string, an OIIO ustring, or any subset of
/// characters from any of those. But still, watch out if you're constructing
/// a string_view a pointer to some other memory region that's not at all part
/// of a string-like object.
///
/// How do we get a safe c_str from a string_view? It's a neat trick! First,
/// we check if `str[str.size()]` is a null character. If it is -- and this is
/// the very common case of the string_view being a way to pass a reference to
/// an entire std::string, a C string (char*), or a ustring -- then it just
/// returns `str.data()`, since that is a totally valid null-terminated C
/// string! On the other hand, if `str[str.size()] != 0`, then we construct a
/// ustring and return its `ustring::c_str()`, which is safe because ustring
/// memory is never freed.
OIIO_UTIL_API const char* c_str(string_view str);


template<> inline const char*
basic_string_view<char>::c_str() const {
    return OIIO::c_str(*this);
}


OIIO_NAMESPACE_END


#if FMT_VERSION >= 100000
FMT_BEGIN_NAMESPACE
template <> struct formatter<OIIO::string_view> : formatter<string_view>
{
    auto format(OIIO::string_view c, format_context& ctx) const {
        return formatter<string_view>::format(string_view(c), ctx);
    }
};
FMT_END_NAMESPACE
#endif



// Declare std::size and std::ssize for our string_view.
namespace std {

#if OIIO_CPLUSPLUS_VERSION < 17
template<class CharT, class Traits = std::char_traits<CharT>>
constexpr size_t size(const OIIO::basic_string_view<CharT, Traits>& c) {
    return c.size();
}
#endif

#if OIIO_CPLUSPLUS_VERSION < 20
template<class CharT, class Traits = std::char_traits<CharT>>
constexpr ptrdiff_t ssize(const OIIO::basic_string_view<CharT, Traits>& c) {
    return static_cast<ptrdiff_t>(c.size());
}
#endif

// Allow client software to easily know if the std::size/ssize was added for
// our string_view.
#define OIIO_STRING_VIEW_HAS_STD_SIZE 1


} // namespace std
