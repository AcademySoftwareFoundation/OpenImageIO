// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_loaded_image.h"

#include <cstddef>
#include <string>

using namespace OIIO;

namespace Imiv {

bool
describe_loaded_image_layout(const LoadedImage& image,
                             LoadedImageLayout& layout,
                             std::string& error_message)
{
    error_message.clear();
    layout = LoadedImageLayout();

    if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0
        || image.channel_bytes == 0) {
        error_message = "invalid source image dimensions";
        return false;
    }

    const size_t width         = static_cast<size_t>(image.width);
    const size_t height        = static_cast<size_t>(image.height);
    const size_t channels      = static_cast<size_t>(image.nchannels);
    const size_t pixel_stride  = channels * image.channel_bytes;
    const size_t min_row_pitch = width * pixel_stride;
    if (pixel_stride == 0 || image.row_pitch_bytes < min_row_pitch) {
        error_message = "invalid source row pitch";
        return false;
    }

    const size_t required_bytes = image.row_pitch_bytes * height;
    if (image.pixels.size() < required_bytes) {
        error_message = "source pixel buffer is smaller than declared stride";
        return false;
    }

    layout.pixel_stride_bytes  = pixel_stride;
    layout.min_row_pitch_bytes = min_row_pitch;
    layout.required_bytes      = required_bytes;
    return true;
}

bool
loaded_image_pixel_pointer(const LoadedImage& image, int x, int y,
                           const unsigned char*& out_pixel,
                           LoadedImageLayout* out_layout,
                           std::string* error_message)
{
    out_pixel = nullptr;
    LoadedImageLayout layout;
    std::string local_error;
    if (!describe_loaded_image_layout(image, layout, local_error)) {
        if (error_message != nullptr)
            *error_message = local_error;
        return false;
    }
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        if (error_message != nullptr)
            *error_message = "requested pixel is outside the loaded image";
        return false;
    }

    const size_t row_start   = static_cast<size_t>(y) * image.row_pitch_bytes;
    const size_t pixel_start = static_cast<size_t>(x)
                               * layout.pixel_stride_bytes;
    const size_t offset = row_start + pixel_start;
    if (offset + layout.pixel_stride_bytes > image.pixels.size()) {
        if (error_message != nullptr)
            *error_message = "requested pixel lies outside the loaded buffer";
        return false;
    }

    out_pixel = image.pixels.data() + offset;
    if (out_layout != nullptr)
        *out_layout = layout;
    if (error_message != nullptr)
        error_message->clear();
    return true;
}

bool
imagebuf_from_loaded_image(const LoadedImage& image, ImageBuf& out,
                           std::string& error_message)
{
    error_message.clear();
    const TypeDesc format = upload_data_type_to_typedesc(image.type);
    if (format == TypeUnknown) {
        error_message = "unsupported source pixel type";
        return false;
    }

    LoadedImageLayout layout;
    if (!describe_loaded_image_layout(image, layout, error_message))
        return false;

    ImageSpec spec(image.width, image.height, image.nchannels, format);
    if (image.channel_names.size() == static_cast<size_t>(image.nchannels))
        spec.channelnames = image.channel_names;
    spec.attribute("Orientation", image.orientation);
    if (!image.metadata_color_space.empty())
        spec.attribute("oiio:ColorSpace", image.metadata_color_space);

    out.reset(spec);
    const std::byte* begin = reinterpret_cast<const std::byte*>(
        image.pixels.data());
    const cspan<std::byte> byte_span(begin, layout.required_bytes);
    const stride_t xstride = static_cast<stride_t>(layout.pixel_stride_bytes);
    const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
    if (!out.set_pixels(ROI::All(), format, byte_span, begin, xstride, ystride,
                        AutoStride)) {
        error_message = out.geterror();
        if (error_message.empty())
            error_message = "failed to copy source pixels into ImageBuf";
        return false;
    }
    return true;
}

}  // namespace Imiv
