// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#pragma once

#ifdef _WIN32
//#undef FAR
#    define XMD_H
#endif

extern "C" {
#include "jpeglib.h"
}

OIIO_PLUGIN_NAMESPACE_BEGIN
namespace psd_pvt {

// This function allows you to read a JPEG from memory using libjpeg.
// Newer versions of libjpeg have jpeg_mem_src which has the same functionality.
// inbuffer is the buffer that holds the JPEG data
// insize is the size of the buffer
void
jpeg_memory_src(j_decompress_ptr cinfo, unsigned char* inbuffer,
                unsigned long insize);

}  // end namespace psd_pvt

OIIO_PLUGIN_NAMESPACE_END
