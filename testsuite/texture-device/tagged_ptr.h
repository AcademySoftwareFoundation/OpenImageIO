// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include <OpenImageIO/strutil.h>

namespace texture_device {

#define OPT_FUNCT(condition, return_type) \
    template<typename __Q = return_type> std::enable_if_t<condition, __Q>

uint64_t
arena_context();

template<size_t N>
inline constexpr uint64_t
maketag(const char (&s)[N])
{
    static_assert(N > 1, "tag literal must be non-empty");
    return OIIO::Strutil::strhash64(N - 1, s);
}

inline constexpr uint64_t
unified_ptr_tag()
{
    return maketag("unified");
}

template<class T> class tagged_ptr {
public:
    using element_type            = T;
    static constexpr bool IsVoid  = std::is_void<T>::value;
    static constexpr bool IsConst = std::is_const<T>::value;
    using ElementRef              = std::add_lvalue_reference_t<T>;
    using ElementPtr              = std::add_pointer_t<T>;

    tagged_ptr() = default;
    tagged_ptr(std::nullptr_t)
        : m_ptr(nullptr)
        , m_tag(0)
    {
    }

    template<class U,
             class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    tagged_ptr(U* p) = delete;

    template<class U,
             class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    tagged_ptr(U* p, uint64_t tag)
        : m_ptr(p)
        , m_tag(tag)
    {
    }

    template<class U,
             class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    tagged_ptr(const tagged_ptr<U>& other)
        : m_ptr(other.get())
        , m_tag(other.tag())
    {
    }

    template<class U,
             class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    tagged_ptr& operator=(const tagged_ptr<U>& other)
    {
        m_ptr = other.get();
        m_tag = other.tag();
        return *this;
    }

    tagged_ptr(const tagged_ptr<void>& other)
        : m_ptr(static_cast<T*>(other.get()))
        , m_tag(other.tag())
    {
    }

    OPT_FUNCT(!IsVoid, tagged_ptr&)
    operator=(const tagged_ptr<void>& other)
    {
        m_ptr = static_cast<T*>(other.get());
        m_tag = other.tag();
        return *this;
    }

    tagged_ptr(const tagged_ptr<const void>& other)
        : m_ptr(static_cast<T*>(other.get()))
        , m_tag(other.tag())
    {
    }

    OPT_FUNCT(!IsVoid && IsConst, tagged_ptr&)
    operator=(const tagged_ptr<const void>& other)
    {
        m_ptr = static_cast<T*>(other.get());
        m_tag = other.tag();
        return *this;
    }

    T* get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr != nullptr; }

    bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(std::nullptr_t) const { return m_ptr != nullptr; }

    bool is(uint64_t tag) const { return m_tag == tag; }
    uint64_t tag() const { return m_tag; }

    OPT_FUNCT(!IsVoid, ElementRef)
    operator*() const
    {
        check_deref_allowed();
        return *m_ptr;
    }

    OPT_FUNCT(!IsVoid, ElementPtr)
    operator->() const
    {
        check_deref_allowed();
        return m_ptr;
    }

    OPT_FUNCT(!IsVoid, ElementRef)
    operator[](size_t i) const
    {
        check_deref_allowed();
        return m_ptr[i];
    }

private:
    template<class U> friend class tagged_ptr;

    void check_deref_allowed() const
    {
        // Enforce explicit host/device context boundaries at dereference time.
        if (m_tag != arena_context() && m_tag != unified_ptr_tag())
            std::abort();
    }

    T* m_ptr       = nullptr;
    uint64_t m_tag = 0;
};

template<class T, class U>
inline bool
operator==(const tagged_ptr<T>& a, const tagged_ptr<U>& b)
{
    return a.get() == b.get() && a.tag() == b.tag();
}

template<class T, class U>
inline bool
operator!=(const tagged_ptr<T>& a, const tagged_ptr<U>& b)
{
    return !(a == b);
}

}  // namespace texture_device

#undef OPT_FUNCT
