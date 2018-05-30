// OpenImageIO Copyright 2018 Larry Gritz, et al.  All Rights Reserved.
//  https://github.com/OpenImageIO/oiio
// BSD 3-clause license:
//  https://github.com/OpenImageIO/oiio/blob/master/LICENSE


#pragma once

#include <OpenImageIO/array_view.h>

OIIO_NAMESPACE_BEGIN

// C++20 will have std::span<> which is basically the same as our
// array_view<>. We want to slowly transition to their nomenclature, and
// then eventually (in a far future when we can count on C++20 everywhere)
// switch to std::span.
//
// First step: just make OIIO::span<> is a synonym for OIIO::array_view<>.

template<typename T>
using span = array_view<T>;


OIIO_NAMESPACE_END
