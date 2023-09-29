// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

/////////////////////////////////////////////////////////////////////////
// \file
// type_traits.h is where we collect little type traits and related
// templates.
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>

OIIO_NAMESPACE_BEGIN


// An enable_if helper to be used in template parameters which results in
// much shorter symbols: https://godbolt.org/z/sWw4vP
// Borrowed from fmtlib.
#ifndef OIIO_ENABLE_IF
#    define OIIO_ENABLE_IF(...) std::enable_if_t<(__VA_ARGS__), int> = 0
#endif


// We use std::void_t as an aid to SFINAE. It's part of C++17, so prior to
// that we'll have to roll our own.
#if OIIO_CPLUSPLUS_VERSION >= 17 || defined(__cpp_lib_void_t)
using std::void_t;
#else
namespace pvt {
template<class... Ts> struct make_void { typedef void type; };
}
template<class... Ts> using void_t = typename pvt::make_void<Ts...>::type;
#endif


/// has_size_method<T>::value is true if T has a size() method and it returns
/// an integral type.
template<class, class = void> struct has_size_method : std::false_type { };

template<class T>
struct has_size_method<T, void_t<decltype(std::declval<T&>().size())>>
    : std::is_integral<typename std::decay_t<decltype(std::declval<T&>().size())>> { };
// How does this work? This overload is only defined if there is a size()
// method, and it evaluates to true_type or false_type based on whether size()
// returns an integral type.


/// has_subscript<T>::value is true if T has a subscript operator.
template<class, class = void> struct has_subscript : std::false_type { };

template<class T>
struct has_subscript<T, void_t<decltype(std::declval<T&>()[0])>>
    : std::true_type { };



OIIO_NAMESPACE_END
