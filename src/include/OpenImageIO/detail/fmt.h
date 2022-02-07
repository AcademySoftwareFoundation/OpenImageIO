// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once
#define OIIO_FMT_H

#include <OpenImageIO/platform.h>

// We want the header-only implemention of fmt
#ifndef FMT_HEADER_ONLY
#    define FMT_HEADER_ONLY
#endif

// Disable fmt exceptions
#ifndef FMT_EXCEPTIONS
#    define FMT_EXCEPTIONS 0
#endif

// Use the grisu fast floating point formatting for old fmt versions
// (irrelevant for >= 7.1).
#ifndef FMT_USE_GRISU
#    define FMT_USE_GRISU 1
#endif

// fmt 8.1 stopped automatically enabling formatting of anything that supports
// ostream output. This breaks a lot! Re-enable this old behavior.
#ifndef FMT_DEPRECATED_OSTREAM
#    define FMT_DEPRECATED_OSTREAM 1
#endif

#if OIIO_GNUC_VERSION >= 70000
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <OpenImageIO/detail/fmt/format.h>
#include <OpenImageIO/detail/fmt/ostream.h>
#include <OpenImageIO/detail/fmt/printf.h>

#if OIIO_GNUC_VERSION >= 70000
#    pragma GCC diagnostic pop
#endif
