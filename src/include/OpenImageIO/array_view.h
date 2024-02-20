// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <OpenImageIO/span.h>

OIIO_NAMESPACE_BEGIN

// Backwards-compatibility: define array_view<> (our old name) as a
// synonym for span<> (which is the nomenclature favored by C++20).

template<typename T> using array_view = span<T>;

template<typename T> using array_view_strided = span_strided<T>;


OIIO_NAMESPACE_END
