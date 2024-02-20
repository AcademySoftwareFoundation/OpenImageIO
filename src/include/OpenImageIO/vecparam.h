// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once
#define OIIO_VECPARAM_H 1

#include <algorithm>
#include <cstring>

#include <OpenImageIO/platform.h>


OIIO_NAMESPACE_BEGIN

// NOTE: These interoperable type templates were copied from the
// [Imath project](http://github.com/AcademySoftwareFoundation/imath),
// licensed under the same BSD-3-clause license as OpenImageIO.


/// @{
/// @name Detecting interoperable linear algebra types.
///
/// In order to construct or assign from external "compatible" types without
/// prior knowledge of their definitions, we have a few helper type traits.
/// The intent of these is to allow custom linear algebra types in an
/// application that have seamless conversion to and from similar types.
///
/// `has_xy<T,Base>`, `has_xyz<T,Base>`, `has_xyzw<T,Base>` detect if class
/// `T` has the right set of elements `.x`, `.y`, `.z`, `.w`, all of type
/// `Base` and seems to be the right size to hold exactly those members and
/// nothing more.
///
/// `has_subscript_N<T,Base,Nelem>` detects if class `T` can perform `T[int]`
/// to yield a `Base`, and that it seems to be exactly the right size to
/// hold `Nelem` of those elements.
///
/// This is not exact. It's possible that for a particular user-defined
/// type, this may yield a false negative or false positive. For example:
///   * A class for a 3-vector that contains an extra element of padding
///     so that it will have the right size and alignment to use 4-wide
///     SIMD math ops will appear to be the wrong size.
///   * A `std::vector<T>` is subscriptable and might have N elements at
///     runtime, but the size is dynamic and so would fail this test.
///   * A foreign type may have .x, .y, .z that are not matching our base
///     type but we still want it to work (with appropriate conversions).
///
/// In these cases, user code may declare an exception -- for example,
/// stating that `mytype` should be considered a subscriptable 3-vector:
///
///     template<>
///     struct OIIO::has_subscript_N<mytype, float, 3> : public std::true_type { };
///
/// And similarly, user code may correct a potential false positive (that
/// is, a `mytype` looks like it should be a 3-vector, but we don't want any
/// implicit conversions to happen):
///
///     template<typename B, int N>
///     struct OIIO::has_subscript_N<mytype, B, N> : public std::false_type { };
///


/// `has_xy<T,Base>::value` will be true if type `T` has member variables
/// `.x` and `.y`, all of type `Base`, and the size of a `T` is exactly big
/// enough to hold 2 Base values.
template<typename T, typename Base> struct has_xy {
private:
    typedef char Yes[1];
    typedef char No[2];

    // Valid only if .x, .y exist and are the right type: return a Yes.
    template<typename C,
             OIIO_ENABLE_IF(std::is_same<decltype(C().x), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().y), Base>::value)>
    static Yes& test(int);

    // Fallback, default to returning a No.
    template<typename C> static No& test(...);

public:
    enum {
        value = (sizeof(test<T>(0)) == sizeof(Yes)
                 && sizeof(T) == 2 * sizeof(Base))
    };
};


/// `has_xyz<T,Base>::value` will be true if type `T` has member variables
/// `.x`, `.y`, and `.z`, all of type `Base`, and the size of a `T` is
/// exactly big enough to hold 3 Base values.
template<typename T, typename Base> struct has_xyz {
private:
    typedef char Yes[1];
    typedef char No[2];

    // Valid only if .x, .y, .z exist and are the right type: return a Yes.
    template<typename C,
             OIIO_ENABLE_IF(std::is_same<decltype(C().x), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().y), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().z), Base>::value)>
    static Yes& test(int);

    // Fallback, default to returning a No.
    template<typename C> static No& test(...);

public:
    enum {
        value = (sizeof(test<T>(0)) == sizeof(Yes)
                 && sizeof(T) == 3 * sizeof(Base))
    };
};


/// `has_xyzw<T,Base>::value` will be true if type `T` has member variables
/// `.x`, `.y`, `.z`, and `.w`, all of type `Base`, and the size of a `T` is
/// exactly big enough to hold 4 Base values.
template<typename T, typename Base> struct has_xyzw {
private:
    typedef char Yes[1];
    typedef char No[2];

    // Valid only if .x, .y, .z, .w exist and are the right type: return a Yes.
    template<typename C,
             OIIO_ENABLE_IF(std::is_same<decltype(C().x), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().y), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().z), Base>::value),
             OIIO_ENABLE_IF(std::is_same<decltype(C().w), Base>::value)>
    static Yes& test(int);

    // Fallback, default to returning a No.
    template<typename C> static No& test(...);

public:
    enum {
        value = (sizeof(test<T>(0)) == sizeof(Yes)
                 && sizeof(T) == 4 * sizeof(Base))
    };
};



/// `has_subscript_N<T,Base,Nelem>::value` will be true if type `T` has
/// subscripting syntax, a `T[int]` returns a `Base`, and the size of a `T`
/// is exactly big enough to hold `Nelem` `Base` values.
template<typename T, typename Base, int Nelem> struct has_subscript_N {
private:
    typedef char Yes[1];
    typedef char No[2];

    // Valid only if T[] is possible and is the right type: return a Yes.
    template<
        typename C,
        OIIO_ENABLE_IF(std::is_same<typename std::decay<decltype(C()[0])>::type,
                                    Base>::value)>
    static Yes& test(int);

    // Fallback, default to returning a No.
    template<typename C> static No& test(...);

public:
    enum {
        value = (sizeof(test<T>(0)) == sizeof(Yes)
                 && sizeof(T) == Nelem * sizeof(Base))
    };
};



/// C arrays of just the right length also are qualified for has_subscript_N.
template<typename Base, int Nelem>
struct has_subscript_N<Base[Nelem], Base, Nelem> : public std::true_type {};



/// `has_double_subscript_RC<T,Base,Rows,Cols>::value` will be true if type `T`
/// has 2-level subscripting syntax, a `T[int][int]` returns a `Base`, and
/// the size of a `T` is exactly big enough to hold `R*C` `Base` values.
template<typename T, typename Base, int Rows, int Cols>
struct has_double_subscript_RC {
private:
    typedef char Yes[1];
    typedef char No[2];

    // Valid only if T[][] is possible and is the right type: return a Yes.
    template<typename C,
             OIIO_ENABLE_IF(
                 std::is_same<typename std::decay<decltype(C()[0][0])>::type,
                              Base>::value)>
    static Yes& test(int);

    // Fallback, default to returning a No.
    template<typename C> static No& test(...);

public:
    enum {
        value = (sizeof(test<T>(0)) == sizeof(Yes)
                 && sizeof(T) == (Rows * Cols) * sizeof(Base))
    };
};


/// C arrays of just the right length also are qualified for has_double_subscript_RC.
template<typename Base, int Rows, int Cols>
struct has_double_subscript_RC<Base[Rows][Cols], Base, Rows, Cols>
    : public std::true_type {};

/// @}



/// Vec3Param<T> is a helper class that lets us create an interface that takes
/// a proxy for a `T[3]` analogue for use as a public API function parameter
/// type, in order to not expose the underlying vector type.
///
/// For example, suppose we have a public function like this:
///
///     void foo(Vec3Param<float> v);
///
/// Then any of the following calls will work:
///
///     float array[3];
///     foo(array);
///
///     foo(Imath::V3f(1,2,3));
///
template<typename T> class Vec3Param {
public:
    /// Construct directly from 3 floats.
    OIIO_HOSTDEVICE constexpr Vec3Param(T x, T y, T z) noexcept
        : x(x)
        , y(y)
        , z(z)
    {
    }

    /// Construct from anything that looks like a 3-vector, having .x, .y, and
    /// .z members of type T (and has exactly the size of a `T[3]`). This will
    /// implicitly convert from an Imath::Vector3<T>, among other things.
    template<typename V, OIIO_ENABLE_IF(has_xyz<V, T>::value)>
    OIIO_HOSTDEVICE constexpr Vec3Param(const V& v) noexcept
        : x(v.x)
        , y(v.y)
        , z(v.z)
    {
    }

    /// Construct from anything that looks like a 3-vector, having `[]`
    /// component access returning a `T`, and has exactly the size of a
    /// `T[3]`.
    template<typename V, OIIO_ENABLE_IF(has_subscript_N<V, T, 3>::value
                                        && !has_xyz<V, T>::value)>
    OIIO_HOSTDEVICE constexpr Vec3Param(const V& v) noexcept
        : x(v[0])
        , y(v[1])
        , z(v[2])
    {
    }

#ifdef INCLUDED_IMATHVEC_H
    /// Only if ImathVec.h has been included, we can construct a Vec3Param<T>
    /// out of an Imath::Vec3<T>.
    OIIO_HOSTDEVICE constexpr operator const Imath::Vec3<T>&() const noexcept
    {
        return *(const Imath::Vec3<T>*)this;
    }

    /// Only if ImathVec.h has been included, we can implicitly convert a
    /// `Vec3Param<T>` to a `Imath::Vec3<T>`.
    OIIO_HOSTDEVICE constexpr const Imath::Vec3<T>& operator()() const noexcept
    {
        return *(const Imath::Vec3<T>*)this;
    }
#endif

    // Return a reference to the contiguous values comprising the matrix.
    template<typename V, OIIO_ENABLE_IF(sizeof(V) == 3 * sizeof(T))>
    constexpr const V& cast() const noexcept
    {
        const char* p = (const char*)this;
        return *(const V*)p;
    }

    /// Implicitly convert a `Vec3Param<T>` to a `const V&` for a V that looks
    /// like a 3-vector.
    template<typename V, OIIO_ENABLE_IF(has_subscript_N<V, T, 3>::value
                                        || has_xyz<V, T>::value)>
    OIIO_HOSTDEVICE constexpr operator const V&() const noexcept
    {
        return cast<V>();
    }

    // The internal representation is just the 3 values.
    T x, y, z;
};


/// V3fParam is an alias for Vec3Param<float>
using V3fParam = Vec3Param<float>;



/// MatrixParam is a helper template that lets us create an interface that
/// takes a proxy for a `T[S][S]` analogue for use as a public API function
/// parameter type to pass a square matrix, in order to not expose the
/// underlying matrix types. The common cases are given handy aliases:
/// M33fParam and M33fParam for 3x3 and 4x4 float matrices, respectively
/// (`MatrixParam<float,3>` and `MatrixParam<float,4>` are the long names).
///
/// For example, suppose we have a public function like this:
///
///     void foo(M33fParam v);
///
/// Then any of the following calls will work:
///
///     float array[3][3];
///     foo(array);
///
///     foo(Imath::M33f(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1));
///
template<typename T, int S> class MatrixParam {
public:
    static constexpr int Size = S;

    /// We can construct a MatrixParam out of anything that has the size
    /// of a `T[S][S]` and presents a `[][]` subscript operator.
    template<typename M,
             OIIO_ENABLE_IF(has_double_subscript_RC<M, T, Size, Size>::value)>
    OIIO_HOSTDEVICE constexpr MatrixParam(const M& m) noexcept
        : m_ptr((const T*)&m)
    {
    }

#ifdef INCLUDED_IMATHMATRIX_H
    /// Only if ImathMatrix.h has been included, we can construct a
    /// MatrixParam<T,3> out of an Imath::Matrix33<T>.
    template<typename ThisType = MatrixParam,
             typename std::enable_if<ThisType::Size == 3, int>::type = 0>
    OIIO_HOSTDEVICE constexpr
    operator const Imath::Matrix33<T>&() const noexcept
    {
        return *(const Imath::Matrix33<T>*)(m_ptr);
    }

    /// Only if ImathMatrix.h has been included, we can implicitly convert a
    /// MatrixParam<T,3> into an Imath::Matrix33<T>.
    template<typename ThisType = MatrixParam,
             typename std::enable_if<ThisType::Size == 3, int>::type = 0>
    OIIO_HOSTDEVICE constexpr const Imath::Matrix33<T>&
    operator()() const noexcept
    {
        return *(const Imath::Matrix33<T>*)(m_ptr);
    }

    /// Only if ImathMatrix.h has been included, we can construct a
    /// MatrixParam<T,4> out of an Imath::Matrix44<T>.
    template<typename ThisType = MatrixParam,
             typename std::enable_if<ThisType::Size == 4, int>::type = 0>
    OIIO_HOSTDEVICE constexpr
    operator const Imath::Matrix44<T>&() const noexcept
    {
        return *(const Imath::Matrix44<T>*)(m_ptr);
    }

    /// Only if ImathMatrix.h has been included, we can implicitly convert a
    /// MatrixParam<T,4> into an Imath::Matrix44<T>.
    template<typename ThisType = MatrixParam,
             typename std::enable_if<ThisType::Size == 4, int>::type = 0>
    OIIO_HOSTDEVICE constexpr const Imath::Matrix44<T>&
    operator()() const noexcept
    {
        return *(const Imath::Matrix44<T>*)(m_ptr);
    }
#endif

    /// Return a pointer to the contiguous values comprising the matrix.
    const T* data() const noexcept { return m_ptr; }

private:
    /// Underlying representation is just a pointer.
    const T* m_ptr;
};


/// M33fParam is an alias for MatrixParam<float, 3>
using M33fParam = MatrixParam<float, 3>;

/// M44fParam is an alias for MatrixParam<float, 4>
using M44fParam = MatrixParam<float, 4>;


OIIO_NAMESPACE_END
