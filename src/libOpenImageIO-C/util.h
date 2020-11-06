// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once

#include <OpenImageIO/platform.h>

// Macro to define short conversion functions between C and C++ API types to
// save us from eye-gougingly verbose casts everywhere
#define DEFINE_POINTER_CASTS(TYPE)                       \
    OIIO_MAYBE_UNUSED                                    \
    OIIO::TYPE* to_cpp(OIIO_##TYPE* is)                  \
    {                                                    \
        return reinterpret_cast<OIIO::TYPE*>(is);        \
    }                                                    \
                                                         \
    OIIO_MAYBE_UNUSED                                    \
    const OIIO::TYPE* to_cpp(const OIIO_##TYPE* is)      \
    {                                                    \
        return reinterpret_cast<const OIIO::TYPE*>(is);  \
    }                                                    \
                                                         \
    OIIO_MAYBE_UNUSED                                    \
    OIIO_##TYPE* to_c(OIIO::TYPE* is)                    \
    {                                                    \
        return reinterpret_cast<OIIO_##TYPE*>(is);       \
    }                                                    \
                                                         \
    OIIO_MAYBE_UNUSED                                    \
    const OIIO_##TYPE* to_c(const OIIO::TYPE* is)        \
    {                                                    \
        return reinterpret_cast<const OIIO_##TYPE*>(is); \
    }
