// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_tiling.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    size_t align_up_size(size_t value, size_t alignment)
    {
        if (alignment <= 1)
            return value;
        const size_t remainder = value % alignment;
        if (remainder == 0)
            return value;
        return value + (alignment - remainder);
    }

}  // namespace

bool
build_row_stripe_upload_plan(size_t source_row_pitch_bytes,
                             size_t pixel_stride_bytes, int image_height,
                             size_t max_descriptor_range_bytes,
                             size_t min_offset_alignment_bytes,
                             RowStripeUploadPlan& plan,
                             std::string& error_message)
{
    error_message.clear();
    plan = RowStripeUploadPlan();

    if (image_height <= 0) {
        error_message = "invalid image height for stripe upload plan";
        return false;
    }
    if (source_row_pitch_bytes == 0 || pixel_stride_bytes == 0) {
        error_message = "invalid source pitch for stripe upload plan";
        return false;
    }
    if (max_descriptor_range_bytes == 0) {
        error_message = "invalid descriptor range limit for stripe upload plan";
        return false;
    }

    size_t row_alignment = std::max<size_t>(4, min_offset_alignment_bytes);
    if (row_alignment == 0)
        row_alignment = 4;

    const size_t aligned_row_pitch = align_up_size(source_row_pitch_bytes,
                                                   row_alignment);
    if (aligned_row_pitch == 0) {
        error_message = "aligned row pitch overflow in stripe upload plan";
        return false;
    }
    if (aligned_row_pitch > max_descriptor_range_bytes) {
        error_message = Strutil::fmt::format(
            "aligned row pitch {} exceeds max storage buffer range {}",
            aligned_row_pitch, max_descriptor_range_bytes);
        return false;
    }

    const size_t max_rows_per_stripe = max_descriptor_range_bytes
                                       / aligned_row_pitch;
    if (max_rows_per_stripe == 0) {
        error_message
            = "no rows fit into the Vulkan storage-buffer descriptor range";
        return false;
    }

    const uint32_t stripe_rows = static_cast<uint32_t>(
        std::min<size_t>(static_cast<size_t>(image_height),
                         max_rows_per_stripe));
    const size_t descriptor_range = aligned_row_pitch
                                    * static_cast<size_t>(stripe_rows);
    const uint32_t stripe_count = static_cast<uint32_t>(
        (static_cast<size_t>(image_height) + stripe_rows - 1) / stripe_rows);
    const size_t padded_upload_bytes = descriptor_range
                                       * static_cast<size_t>(stripe_count);

    if (descriptor_range > std::numeric_limits<uint32_t>::max()) {
        error_message = Strutil::fmt::format(
            "descriptor range {} exceeds Vulkan dynamic-offset address space",
            descriptor_range);
        return false;
    }
    if (padded_upload_bytes > std::numeric_limits<uint32_t>::max()) {
        error_message = Strutil::fmt::format(
            "padded upload size {} exceeds Vulkan dynamic-offset address space",
            padded_upload_bytes);
        return false;
    }

    plan.aligned_row_pitch_bytes = aligned_row_pitch;
    plan.descriptor_range_bytes  = descriptor_range;
    plan.padded_upload_bytes     = padded_upload_bytes;
    plan.stripe_rows             = stripe_rows;
    plan.stripe_count            = stripe_count;
    plan.uses_multiple_stripes   = stripe_count > 1;
    return true;
}

bool
copy_rows_to_padded_buffer(const unsigned char* source_pixels,
                           size_t source_pixels_size,
                           size_t source_row_pitch_bytes, int image_height,
                           size_t padded_row_pitch_bytes,
                           unsigned char* destination_pixels,
                           size_t destination_pixels_size,
                           std::string& error_message)
{
    error_message.clear();

    if (source_pixels == nullptr || destination_pixels == nullptr) {
        error_message = "null image buffer in padded row copy";
        return false;
    }
    if (image_height <= 0 || source_row_pitch_bytes == 0
        || padded_row_pitch_bytes < source_row_pitch_bytes) {
        error_message = "invalid row geometry in padded row copy";
        return false;
    }

    const size_t required_source_bytes = source_row_pitch_bytes
                                         * static_cast<size_t>(image_height);
    const size_t required_destination_bytes
        = padded_row_pitch_bytes * static_cast<size_t>(image_height);
    if (source_pixels_size < required_source_bytes) {
        error_message = "source image buffer is smaller than declared row span";
        return false;
    }
    if (destination_pixels_size < required_destination_bytes) {
        error_message
            = "destination image buffer is smaller than padded row span";
        return false;
    }

    std::memset(destination_pixels, 0, destination_pixels_size);
    for (int y = 0; y < image_height; ++y) {
        const size_t source_offset = static_cast<size_t>(y)
                                     * source_row_pitch_bytes;
        const size_t destination_offset = static_cast<size_t>(y)
                                          * padded_row_pitch_bytes;
        std::memcpy(destination_pixels + destination_offset,
                    source_pixels + source_offset, source_row_pitch_bytes);
    }
    return true;
}

}  // namespace Imiv
