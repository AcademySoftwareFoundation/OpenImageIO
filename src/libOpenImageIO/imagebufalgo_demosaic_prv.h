// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <OpenImageIO/imagebufalgo.h>

OIIO_NAMESPACE_BEGIN

namespace ImageBufAlgo {

std::string OIIO_API
mosaic_float(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
             const std::string& pattern, const float (&white_balance)[4],
             int nthreads);

std::string OIIO_API
mosaic_half(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
            const std::string& pattern, const float (&white_balance)[4],
            int nthreads);

std::string OIIO_API
mosaic_uint16(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
              const std::string& pattern, const float (&white_balance)[4],
              int nthreads);

std::string OIIO_API
mosaic_uint8(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
             const std::string& pattern, const float (&white_balance)[4],
             int nthreads);

}  // namespace ImageBufAlgo

OIIO_NAMESPACE_END
