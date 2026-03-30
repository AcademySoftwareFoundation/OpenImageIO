// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_types.h"

#include <cstddef>
#include <string>

#include <OpenImageIO/imagebuf.h>

namespace Imiv {

struct LoadedImageLayout {
    size_t pixel_stride_bytes = 0;
    size_t min_row_pitch_bytes = 0;
    size_t required_bytes = 0;
};

bool
describe_loaded_image_layout(const LoadedImage& image,
                             LoadedImageLayout& layout,
                             std::string& error_message);

bool
loaded_image_pixel_pointer(const LoadedImage& image, int x, int y,
                           const unsigned char*& out_pixel,
                           LoadedImageLayout* out_layout = nullptr,
                           std::string* error_message = nullptr);

bool
imagebuf_from_loaded_image(const LoadedImage& image, OIIO::ImageBuf& out,
                           std::string& error_message);

}  // namespace Imiv
