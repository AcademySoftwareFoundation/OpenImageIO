// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <OpenImageIO/Imath.h>

// Force the full implementation of all fmath functions, which will compile
// some callable versions here.
#define OIIO_FMATH_HEADER_ONLY 1

#include <OpenImageIO/fmath.h>
