// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/dassert.h>

#include <array>
#include <cstddef>
#include <utility>

namespace texture_device {

template<class T, size_t N> struct vector_lite : public std::array<T, N> {
    using Base = std::array<T, N>;

    vector_lite()
        : Base {}
        , m_size(0)
    {
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return N; }
    bool empty() const { return m_size == 0; }

    void clear() { m_size = 0; }

    void push_back(const T& value)
    {
        OIIO_CONTRACT_ASSERT(m_size < N);
        (*this)[m_size++] = value;
    }

    void push_back(T&& value)
    {
        OIIO_CONTRACT_ASSERT(m_size < N);
        (*this)[m_size++] = std::move(value);
    }

    T* begin() { return this->data(); }
    T* end() { return this->data() + m_size; }
    const T* begin() const { return this->data(); }
    const T* end() const { return this->data() + m_size; }
    const T* cbegin() const { return begin(); }
    const T* cend() const { return end(); }

private:
    unsigned m_size;
};

}  // namespace texture_device
