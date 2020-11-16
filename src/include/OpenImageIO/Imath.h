// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/

// clang-format off

#pragma once


// Determine which Imath we're dealing with and include the appropriate
// headers.

#include <OpenEXR/OpenEXRConfig.h>
#define OIIO_OPENEXR_VERSION ((10000*OPENEXR_VERSION_MAJOR) + \
                              (100*OPENEXR_VERSION_MINOR) + \
                              OPENEXR_VERSION_PATCH)

#if OIIO_OPENEXR_VERSION >= 20599 /* 2.5.99: pre-3.0 */
#   define OIIO_USING_IMATH 3
#   include <Imath/ImathColor.h>
#   include <Imath/ImathMatrix.h>
#   include <Imath/ImathVec.h>
#   include <Imath/half.h>
#else
#   define OIIO_USING_IMATH 2
#   include <OpenEXR/ImathColor.h>
#   include <OpenEXR/ImathMatrix.h>
#   include <OpenEXR/ImathVec.h>
#   include <OpenEXR/half.h>
#endif
