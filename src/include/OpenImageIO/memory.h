// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/////////////////////////////////////////////////////////////////////////
/// @file   memory.h
///
/// @brief  Utilities for memory tracking.
/////////////////////////////////////////////////////////////////////////


#pragma once

#define OPENIMAGEIO_MEMORY_H

#include <cstring>
#include <memory>
#include <vector>

OIIO_NAMESPACE_BEGIN

namespace pvt {

/// Return the total heap memory allocated by `object`.
/// The template specialization can be used to give improved results for non trivial types
/// that perform heap allocation, and to include members allocations recursively.
template<typename T>
inline size_t
heapsize(const T& t)
{
    return 0;
}

/// Return the total memory footprint of `object`. If possible, including any heap
/// allocations done by any constituent parts. The default implementation just reduces
/// to sizeof(object), given that heapsize(object) would return 0.
/// The template specialization can be used to give improved results for non trivial types
/// that perform heap allocation.
template<typename T>
inline size_t
footprint(const T& t)
{
    return sizeof(T) + heapsize(t);
}

template<typename T>
inline size_t
footprint(const T* t)
{
    return sizeof(T*) + (t ? footprint(*t) : 0);
}

/// Specializations for common STL types


// heapsize specialization for std::string
template<>
inline size_t
heapsize<std::string>(const std::string& s)
{
    // accounts for small string optimization that does not
    // use any heap allocations
    const char* const sbegin = (const char*)&s;
    const char* const send   = sbegin + sizeof(std::string);
    const char* const sdata  = s.data();
    const bool is_small      = sdata >= sbegin && sdata < send;
    return is_small ? 0 : s.capacity();
}


// heapsize specialization for std::shared_ptr
template<typename T>
inline size_t
heapsize(const std::shared_ptr<T>& ref)
{
    return ref ? footprint(*ref.get()) : 0;
}

// footprint specialization for std::shared_ptr
template<typename T>
inline size_t
footprint(const std::shared_ptr<T>& ref)
{
    return sizeof(std::shared_ptr<T>) + heapsize(ref);
}

// heapsize specialization for std::unique_ptr
template<typename T>
inline size_t
heapsize(const std::unique_ptr<T>& ref)
{
    return ref ? footprint(*ref.get()) : 0;
}

// footprint specialization for std::unique_ptr
template<typename T>
inline size_t
footprint(const std::unique_ptr<T>& ref)
{
    return sizeof(std::unique_ptr<T>) + heapsize(ref);
}

// heapsize specialization for std::vector
template<typename T>
inline size_t
heapsize(const std::vector<T>& vec)
{
    size_t size = 0;
    // account for used allocated memory
    for (const T& elem : vec)
        size += footprint(elem);
    // account for unused allocated memory
    size += (vec.capacity() - vec.size()) * sizeof(T);
    return size;
}

// footprint specialization for std::vector
template<typename T>
inline size_t
footprint(const std::vector<T>& vec)
{
    return sizeof(std::vector<T>) + heapsize<T>(vec);
}


}  // namespace pvt


OIIO_NAMESPACE_END