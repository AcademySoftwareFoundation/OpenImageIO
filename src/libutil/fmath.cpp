// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/half.h>

// Force the full implementation of all fmath functions, which will compile
// some callable versions here.
#define OIIO_FMATH_HEADER_ONLY 1

#include <OpenImageIO/fmath.h>
