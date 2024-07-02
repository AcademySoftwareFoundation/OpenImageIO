// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO/


#pragma once

// Define IMATH_HALF_NO_LOOKUP_TABLE to ensure we convert float<->half without
// using the lookup table (and thus eliminating the need to link against
// libImath just for half conversion).
#ifndef IMATH_HALF_NO_LOOKUP_TABLE
#    define IMATH_HALF_NO_LOOKUP_TABLE
#endif

#include <Imath/half.h>


#if defined(FMT_VERSION) && !defined(OIIO_HALF_FORMATTER)
#    if FMT_VERSION >= 100000
#        define OIIO_HALF_FORMATTER
FMT_BEGIN_NAMESPACE
template<> struct formatter<half> : ostream_formatter {};
FMT_END_NAMESPACE
#    endif
#endif
