/*
  Copyright 2015 Larry Gritz and the other authors and contributors.
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

#include <vector>
#include <stdexcept>
#include <iostream>

#include "oiioversion.h"
#include "platform.h"
#include "dassert.h"

#if OIIO_CPLUSPLUS_VERSION >= 11
# include <initializer_list>
#endif

OIIO_NAMESPACE_BEGIN

template <size_t Rank> class offset;
template <size_t Rank> class bounds;
template <size_t Rank> class bounds_iterator;



/// An offset<Rank> represents a Rank-dimensional offset. Think of it as
/// a generalization of an array index. Underneath, it's a bit like a
/// int[Rank].

template <size_t Rank>
class offset {
    OIIO_STATIC_ASSERT (Rank >= 1);
public:
    // constants and types
#if OIIO_CPLUSPLUS_VERSION >= 11
    static OIIO_CONSTEXPR_OR_CONST size_t rank = Rank;
    using reference           = ptrdiff_t&;
    using const_reference     = const ptrdiff_t&;
    using size_type           = size_t;
    using value_type          = ptrdiff_t;
#else
    static const size_t rank = Rank;
    typedef ptrdiff_t&  reference;
    typedef const ptrdiff_t& const_reference;
    typedef size_t  size_type;
    typedef ptrdiff_t value_type;
#endif

    /// Default constructor
    OIIO_CONSTEXPR14 offset() OIIO_NOEXCEPT {
        std::fill (m_ind+0, m_ind+Rank, 0);
    }
    /// Constructor for 1D case
    OIIO_CONSTEXPR14 offset (value_type v) OIIO_NOEXCEPT {
        DASSERT (Rank == 1);
        m_ind[0] = v;
        std::fill (m_ind+1, m_ind+Rank, 1);
    }
    /// Constructor for 2D case
    OIIO_CONSTEXPR14 offset (value_type v0, value_type v1) OIIO_NOEXCEPT {
        DASSERT (Rank == 2);
        m_ind[0] = v0;
        m_ind[1] = v1;
        std::fill (m_ind+2, m_ind+Rank, 1);
    }
#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Constructor from initializer_list. Only for C++11.
    OIIO_CONSTEXPR14 offset (std::initializer_list<value_type> il) {
        DASSERT (il.size() == Rank);
        std::copy (il.begin(), il.end(), m_ind+0);
    }
#endif

    /// Equality test.
    OIIO_CONSTEXPR bool operator== (const offset& rhs) const OIIO_NOEXCEPT {
        return std::equal (m_ind+0, m_ind+Rank, rhs.m_ind+0);
    }
    /// Inequality test.
    OIIO_CONSTEXPR bool operator!= (const offset& rhs) const OIIO_NOEXCEPT {
        return ! (*this == rhs);
    }

    /// Component access
    reference operator[](size_type n) {
        DASSERT (n < Rank);
        return m_ind[n];
    }
    OIIO_CONSTEXPR14 const_reference operator[] (size_type n) const {
        DASSERT (n < Rank);
        return m_ind[n];
    }

    // Arithmetic
    OIIO_CONSTEXPR14 offset operator+ (const offset& rhs) const {
        offset result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = m_ind[i] + rhs[i];
        return result;
    }
    OIIO_CONSTEXPR14 offset operator- (const offset& rhs) const {
        offset result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = m_ind[i] - rhs[i];
        return result;
    };
    OIIO_CONSTEXPR14 offset& operator+= (const offset& rhs) {
        for (size_t i = 0; i < Rank; ++i)
            m_ind[i] += rhs[i];
        return *this;
    }
    OIIO_CONSTEXPR14 offset& operator-= (const offset& rhs) {
        for (size_t i = 0; i < Rank; ++i)
            m_ind[i] -= rhs[i];
        return *this;
    }

    OIIO_CONSTEXPR14 offset& operator++ () {  // prefix increment
        DASSERT (Rank == 1);
        ++m_ind[0];
        return *this;
    }
    OIIO_CONSTEXPR14 offset  operator++ (int) {  // postfix increment
        DASSERT (Rank == 1);
        offset ret;
        ++(*this);
        return ret;
    }
    OIIO_CONSTEXPR14 offset& operator-- () { // prefix increment
        DASSERT (Rank == 1);
        --m_ind[0];
        return *this;
    }
    OIIO_CONSTEXPR14 offset  operator-- (int) { // postfix increment
        DASSERT (Rank == 1);
        offset ret;
        --(*this);
        return ret;
    }

    OIIO_CONSTEXPR offset operator+ () const OIIO_NOEXCEPT { return *this; }
    OIIO_CONSTEXPR14 offset operator- () const {
        offset result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = -m_ind[i];
        return result;
    };

    OIIO_CONSTEXPR14 offset operator* (value_type v) const {
        offset result = *this;
        result *= v;
        return result;
    }
    friend OIIO_CONSTEXPR14 offset operator* (value_type v, const offset &off) {
        offset result = off;
        result *= v;
        return result;
    }
    OIIO_CONSTEXPR14 offset operator/ (value_type v) const {
        offset result = *this;
        result /= v;
        return result;
    }
    OIIO_CONSTEXPR14 offset& operator*= (value_type v) {
        for (size_t i = 0; i < Rank; ++i)
            m_ind[i] *= v;
        return *this;
    }
    OIIO_CONSTEXPR14 offset& operator/= (value_type v) {
        for (size_t i = 0; i < Rank; ++i)
            m_ind[i] /= v;
        return *this;
    }
    friend std::ostream& operator<< (std::ostream& out, const offset& off) {
        out << off[0];
        for (size_t i = 1; i < Rank; ++i)
            out << ',' << off[i];
        return out;
    }
private:
    value_type m_ind[Rank];
};



/// A bounds<Rank> represents the size of a Rank-dimensional array.
template <size_t Rank>
class bounds {
    OIIO_STATIC_ASSERT (Rank >= 1);
public:
    // constants and types
#if OIIO_CPLUSPLUS_VERSION >= 11
    static OIIO_CONSTEXPR_OR_CONST size_t rank = Rank;
    using reference           = ptrdiff_t&;
    using const_reference     = const ptrdiff_t&;
    using size_type           = size_t;
    using value_type          = ptrdiff_t;
    using iterator            = bounds_iterator<Rank>;
    using const_iterator      = bounds_iterator<Rank>;
#else
    static const size_t rank = Rank;
    typedef ptrdiff_t&  reference;
    typedef const ptrdiff_t& const_reference;
    typedef size_t  size_type;
    typedef ptrdiff_t value_type;
    typedef bounds_iterator<Rank> iterator;
    typedef bounds_iterator<Rank> const_iterator;
#endif

    /// Default constructor
    OIIO_CONSTEXPR14 bounds() OIIO_NOEXCEPT {
        std::fill (m_bnd+0, m_bnd+Rank, 0);
    }
    /// Constructor for 1D case
    OIIO_CONSTEXPR14 bounds (value_type v) {
        DASSERT (Rank == 1);   // only if Rank == 1
        m_bnd[0] = v;
        std::fill (m_bnd+1, m_bnd+Rank, 1);
    }
    /// Constructor for 2D case
    OIIO_CONSTEXPR14 bounds (value_type v0, value_type v1) {
        DASSERT (Rank == 2);
        m_bnd[0] = v0;
        m_bnd[1] = v1;
        std::fill (m_bnd+2, m_bnd+Rank, 1);
    }
#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Constructor from initializer_list. Only for C++11.
    OIIO_CONSTEXPR14 bounds (std::initializer_list<value_type> il) {
        DASSERT (il.size() == Rank);
        std::copy (il.begin(), il.end(), m_bnd+0);
    }
#endif
  
    OIIO_CONSTEXPR14 size_type size() const OIIO_NOEXCEPT {
        size_type r = m_bnd[0];
        for (size_t i = 1; i < Rank; ++i)
            r *= m_bnd[i];
        return r;
    }
    OIIO_CONSTEXPR14 bool contains(const offset<Rank>& idx) const OIIO_NOEXCEPT {
        for (size_t i = 0; i < Rank; ++i)
            if (idx[i] < 0 || idx[i] > m_bnd[i])
                return false;
        return true;
    }

    /// Equality test.
    OIIO_CONSTEXPR bool operator== (const bounds& rhs) const OIIO_NOEXCEPT {
        return std::equal (m_bnd+0, m_bnd+Rank, rhs.m_bnd+0);
    }
    /// Inequality test.
    OIIO_CONSTEXPR bool operator!= (const bounds& rhs) const OIIO_NOEXCEPT {
        return ! (*this == rhs);
    }

    // bounds iterators
    const_iterator begin() const OIIO_NOEXCEPT {
        return const_iterator(*this, offset<Rank>());
    }
    // Return a bounds_iterator that is one-past-the-end of *this.
    const_iterator end() const OIIO_NOEXCEPT {
        offset<Rank> off;
        off[0] = m_bnd[0];
        return const_iterator (*this, off);
    }

    /// Component access
    OIIO_CONSTEXPR14 reference operator[] (size_type n) {
        DASSERT (n < Rank);
        return m_bnd[n];
    }
    OIIO_CONSTEXPR14 const_reference operator[] (size_type n) const {
        DASSERT (n < Rank);
        return m_bnd[n];
    }

    // bounds arithmetic
    friend OIIO_CONSTEXPR14 bounds operator+ (const bounds& bnd, const offset<Rank>& off) {
        bounds result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = bnd[i] + off[i];
        return result;
    }
    friend OIIO_CONSTEXPR14 bounds operator+ (const offset<Rank>& off, const bounds& bnd) {
        return bnd + off;
    }
    friend OIIO_CONSTEXPR14 bounds operator- (const bounds& bnd, const offset<Rank>& off) {
        bounds result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = bnd[i] - off[i];
        return result;
    }
    friend OIIO_CONSTEXPR14 bounds operator- (const offset<Rank>& off, const bounds& bnd) {
        return bnd - off;
    }
    friend OIIO_CONSTEXPR14 bounds operator* (const bounds& bnd, const value_type v) {
        bounds result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = bnd[i] * v;
        return result;
    }
    friend OIIO_CONSTEXPR14 bounds operator* (const value_type v, const bounds& bnd) {
        return bnd * v;
    }
    friend OIIO_CONSTEXPR14 bounds operator/ (const bounds& bnd, const value_type v) {
        bounds result;
        for (size_t i = 0; i < Rank; ++i)
            result[i] = bnd[i] / v;
        return result;
    }
    OIIO_CONSTEXPR14 bounds& operator+= (const offset<Rank>& rhs) {
        for (size_t i = 0; i < Rank; ++i)
            m_bnd[i] += rhs[i];
        return *this;
    }
    OIIO_CONSTEXPR14 bounds& operator-= (const offset<Rank>& rhs) {
        for (size_t i = 0; i < Rank; ++i)
            m_bnd[i] -= rhs[i];
        return *this;
    }
    OIIO_CONSTEXPR14 bounds& operator*= (value_type v) {
        for (size_t i = 0; i < Rank; ++i)
            m_bnd[i] *= v;
        return *this;
    }
    OIIO_CONSTEXPR14 bounds& operator/= (value_type v) {
        for (size_t i = 0; i < Rank; ++i)
            m_bnd[i] /= v;
        return *this;
    }

    friend std::ostream& operator<< (std::ostream& out, const bounds& bnd) {
        out << bnd[0];
        for (size_t i = 1; i < Rank; ++i)
            out << ',' << bnd[i];
        return out;
    }

private:
    value_type m_bnd[Rank];
};



namespace detail {
template <int Rank>
class bounds_iterator_pointer
{
public:
    explicit bounds_iterator_pointer (offset<Rank> const& off) : m_off(off) { }
    offset<Rank> const& operator*() const { return m_off; }
    offset<Rank> const* operator->() const { return &m_off; }
private:
    offset<Rank> m_off;
};
} // end namespace detail



template <size_t Rank>
class bounds_iterator
{
    OIIO_STATIC_ASSERT (Rank >= 1);
public:
#if OIIO_CPLUSPLUS_VERSION >= 11
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = offset<Rank>;
    using difference_type   = ptrdiff_t;
    using pointer           = detail::bounds_iterator_pointer<Rank>;
    using reference         = const offset<Rank>;
#else
    typedef offset<Rank> value_type;
    typedef ptrdiff_t difference_type;
    typedef detail::bounds_iterator_pointer<Rank> pointer;
    typedef const offset<Rank> reference;
#endif

    explicit bounds_iterator (const bounds<Rank> &bnd, const offset<Rank> &off)
        : bnd_(bnd), off_(off)
    {}

    bool operator== (const bounds_iterator &rhs) const {
        return off_ == rhs.off_;
    }
    bool operator!= (const bounds_iterator &rhs) const {
        return ! (*this == rhs);
    }
    bool operator< (bounds_iterator const& rhs) const {
        for (std::size_t i = 0; i < Rank; ++i) {
            if (off_[i] < rhs.off_[i])
                return true;
            else if (off_[i] > rhs.off_[i])
                return false;
        }
        return false;
    }
    bool operator<= (bounds_iterator const& rhs) const {
        return !(rhs < *this);
    }
    bool operator> (bounds_iterator const& rhs) const {
        return rhs < *this;
    }
    bool operator>= (bounds_iterator const& rhs) const {
        return !(*this < rhs);
    }

    bounds_iterator& operator++()  {
        for (int i = Rank - 1; i >= 0; --i) {
            if (++off_[i] < bnd_[i])
                return *this;
            off_[i] = 0;
        }
        off_[0] = bnd_[0];
        return *this;
    }

    bounds_iterator operator++ (int) {
        bounds_iterator r(*this);
        ++(*this);
        return r;
    }

    bounds_iterator& operator--()  {
        for (int i = int(Rank) - 1; i >= 0; --i) {
            if (--off_[i] >= 0)
                return *this;
            off_[i] = bnd_[i] - 1;
        }
        // off_[Rank - 1] == -1;
        return *this;
    }

    bounds_iterator operator-- (int) {
        bounds_iterator r(*this);
        --(*this);
        return r;
    }

    bounds_iterator operator+ (difference_type n) const {
        return bounds_iterator(*this) += n;
    }

    bounds_iterator& operator+= (difference_type n) {
        for (int i = Rank - 1; i >= 0 && n != 0; --i) {
            std::ptrdiff_t nx = off_[i] + n;
            if (nx >= bnd_[i]) {
                n = nx / bnd_[i];
                off_[i] = nx % bnd_[i];
            } else {
                off_[i] = nx;
                return *this;
            }
        }
        off_[0] = bnd_[0];
        return *this;
    }

    bounds_iterator operator- (difference_type n) const {
        return bounds_iterator(*this) -= n;
    }

    bounds_iterator& operator-= (difference_type n) {
        return (*this += (-n));
    }

    difference_type operator- (bounds_iterator const& rhs) const {
        difference_type r = 0;
        difference_type flat_bounds = 1;
        for (int i = Rank - 1; i >= 0; --i) {
            r += (off_[i] - rhs.off_[i]) * flat_bounds;
            flat_bounds *= bnd_[i];
        }
        return r;
    }

    reference operator*() const { return off_; }
    pointer   operator->() const { return pointer(off_); }
    reference operator[] (difference_type n) const { return *(*this+n); }

    friend std::ostream& operator<< (std::ostream& out, const bounds_iterator& i) {
        return out << i.off_;
    }

private:
    bounds<Rank> bnd_;  // exposition only
    offset<Rank> off_;  // exposition only
};



OIIO_NAMESPACE_END
