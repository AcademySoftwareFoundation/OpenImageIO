// Copyright 2021-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once


#include <OpenImageIO/Imath.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/typedesc.h>

#include <OpenEXR/ImfChannelList.h>

#ifdef OPENEXR_VERSION_MAJOR
#    define OPENEXR_CODED_VERSION                                    \
        (OPENEXR_VERSION_MAJOR * 10000 + OPENEXR_VERSION_MINOR * 100 \
         + OPENEXR_VERSION_PATCH)
#else
#    define OPENEXR_CODED_VERSION 20000
#endif

#if OPENEXR_CODED_VERSION >= 20400 \
    || __has_include(<OpenEXR/ImfFloatVectorAttribute.h>)
#    define OPENEXR_HAS_FLOATVECTOR 1
#else
#    define OPENEXR_HAS_FLOATVECTOR 0
#endif

#define ENABLE_READ_DEBUG_PRINTS 0


OIIO_PLUGIN_NAMESPACE_BEGIN

#if OIIO_CPLUSPLUS_VERSION >= 17 || defined(__cpp_lib_gcd_lcm)
using std::gcd;
#else
template<class M, class N, class T = std::common_type_t<M, N>>
inline T
gcd(M a, N b)
{
    while (b) {
        T t = b;
        b   = a % b;
        a   = t;
    }
    return a;
}
#endif


// Split a full channel name into layer and suffix.
inline void
split_name(string_view fullname, string_view& layer, string_view& suffix)
{
    size_t dot = fullname.find_last_of('.');
    if (dot == string_view::npos) {
        suffix = fullname;
        layer  = string_view();
    } else {
        layer  = string_view(fullname.data(), dot + 1);
        suffix = string_view(fullname.data() + dot + 1,
                             fullname.size() - dot - 1);
    }
}


OIIO_PLUGIN_NAMESPACE_END
