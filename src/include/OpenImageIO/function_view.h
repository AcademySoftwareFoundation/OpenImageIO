/*
  Copyright 2017 Larry Gritz and the other authors and contributors.
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

// Portions of the code in this file related to is_invocable and
// function_view are licensed under the AFL 3.0:
// Copyright (c) 2013-2016 Vittorio Romeo
// AFL License page: http://opensource.org/licenses/AFL-3.0
// https://github.com/SuperV1234/vittorioromeo.info


#pragma once

#include <type_traits>
#include <functional>

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/export.h>
#include <OpenImageIO/platform.h>


OIIO_NAMESPACE_BEGIN


#if OIIO_CPLUSPLUS_VERSION >= 14

using std::add_pointer_t;
using std::decay_t;
using std::result_of_t;
using std::enable_if_t;

#else

template<class T> using add_pointer_t = typename std::add_pointer<T>::type;
template<class T> using decay_t = typename std::decay<T>::type;
template<class T> using result_of_t = typename std::result_of<T>::type;
template<bool B, class T=void>
   using enable_if_t = typename std::enable_if<B,T>::type;

#endif



#if OIIO_CPLUSPLUS_VERSION >= 17

using std::is_invocable;

#else

template <typename...>
using void_t = void;

template <class T, class R = void, class = void>
struct is_invocable : std::false_type
{
};

template <class T>
struct is_invocable<T, void, void_t<result_of_t<T>>> : std::true_type
{
};

template <class T, class R>
struct is_invocable<T, R, void_t<result_of_t<T>>>
    : std::is_convertible<result_of_t<T>, R>
{
};

#endif



/// function_view<R(T...)> is a lightweight non-owning generic callable
/// object view, similar to a std::function<R(T...)>, but with much less
/// overhead.
///
/// A function_view invocation should have the same cost as a function
/// pointer (which it basically is underneath). Similar in spirit to a
/// string_view or array_view, the function-like object that the
/// function_view refers to MUST have a lifetime that outlasts any use of
/// the function_view.
///
/// In contrast, a full std::function<> is an owning container for a
/// callable object. It's more robust, especially with restpect to object
/// lifetimes, but the call overhead is quite high. So use a function_view
/// when you can.
///
/// This implementation comes from the following blog article:
/// https://vittorioromeo.info/index/blog/passing_functions_to_functions.html
/// https://github.com/SuperV1234/vittorioromeo.info/blob/master/extra/passing_functions_to_functions/function_view.hpp
/// The code is licensed under the AFL 3.0:
/// Copyright (c) 2013-2016 Vittorio Romeo
/// AFL License page: http://opensource.org/licenses/AFL-3.0
///
/// There are other implementations floating around, for example this one
/// https://chromium.googlesource.com/external/webrtc/+/master/webrtc/base/function_view.h
/// There are also proposals to add it to C++ std at some point.
/// But I think this will serve our purposes for now.

template <typename TSignature>
class function_view;

template <typename TReturn, typename... TArgs>
class function_view<TReturn(TArgs...)> final
{
private:
    using signature_type = TReturn(void*, TArgs...);

    void* _ptr;
    TReturn (*_erased_fn)(void*, TArgs...);

public:
    template <typename T, typename = enable_if_t<
                              is_invocable<T&(TArgs...)>{} &&
                              !std::is_same<decay_t<T>, function_view>{}>>
    function_view(T&& x) noexcept : _ptr{(void*)std::addressof(x)}
    {
        _erased_fn = [](void* ptr, TArgs... xs) -> TReturn {
            return (*reinterpret_cast<add_pointer_t<T>>(ptr))(
                std::forward<TArgs>(xs)...);
        };
    }

#if OIIO_CPLUSPLUS_VERSION >= 14
    decltype(auto)
#else
    TReturn
#endif
    operator()(TArgs... xs) const
        noexcept(noexcept(_erased_fn(_ptr, std::forward<TArgs>(xs)...)))
    {
        return _erased_fn(_ptr, std::forward<TArgs>(xs)...);
    }
};


OIIO_NAMESPACE_END
