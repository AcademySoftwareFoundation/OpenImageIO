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

OIIO_NAMESPACE_3_1_BEGIN

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


/// Allocator adaptor that interposes construct() calls to convert value
/// initialization into default initialization.
///
/// This is a way to achieve a std::vector whose resize does not force a value
/// initialization of every allocated element. Put in more plain terms, the
/// following:
///
///     std::vector<int> v(Nlarge);
///
/// will zero-initialize the Nlarge elements, which may be a cost we do not
/// wish to pay, particularly when allocating POD types that we are going to
/// write over anyway. Sometimes we do the following instead:
///
///     std::unique_ptr<int[]> v (new int[Nlarge]);
///
/// which does not zero-initialize the elements. But it's more awkward, and
/// lacks the methods you get automatically with vectors.
///
/// But you will get a lack of forced value initialization if you use a
/// std::vector with a special allocator that does default initialization.
/// This is such an allocator, so the following:
///
///    std::vector<T, default_init_allocator<T>> v(Nlarge);
///
/// will have the same performance characteristics as the new[] version.
///
/// For details:
/// https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
///
template<typename T, typename A = std::allocator<T>>
class default_init_allocator : public A {
    typedef std::allocator_traits<A> a_t;

public:
    template<typename U> struct rebind {
        using other
            = default_init_allocator<U, typename a_t::template rebind_alloc<U>>;
    };

    using A::A;

    template<typename U>
    void
    construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value)
    {
        ::new (static_cast<void*>(ptr)) U;
    }
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args)
    {
        a_t::construct(static_cast<A&>(*this), ptr,
                       std::forward<Args>(args)...);
    }
};


/// Type alias for a std::vector that uses the default_init_allocator.
///
/// Consider using a `default_init_vector<T>` instead of `std::vector<T>` when
/// all of the following are true:
///
/// * The use is entirely internal to OIIO (since at present, this type is
///   not defined in any public header files).
/// * The type T is POD (plain old data) or trivially constructible.
/// * The vector is likely to be large enough that the cost of default
///   initialization is worth trying to avoid.
/// * After allocation, the vector will be filled with data before any reads
///   are attempted, so the default initialization is not needed.
///
template<typename T>
using default_init_vector = std::vector<T, default_init_allocator<T>>;


OIIO_NAMESPACE_3_1_END


// Compatibility
OIIO_NAMESPACE_BEGIN
#ifndef OIIO_DOXYGEN
namespace pvt {
using v3_1::pvt::footprint;
using v3_1::pvt::heapsize;
}  // namespace pvt
using v3_1::default_init_allocator;
using v3_1::default_init_vector;
#endif
OIIO_NAMESPACE_END
