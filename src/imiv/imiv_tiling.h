// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Imiv {

struct RowStripeUploadPlan {
    size_t aligned_row_pitch_bytes  = 0;
    size_t descriptor_range_bytes   = 0;
    size_t padded_upload_bytes      = 0;
    uint32_t stripe_rows            = 0;
    uint32_t stripe_count           = 0;
    bool uses_multiple_stripes      = false;
};

bool
build_row_stripe_upload_plan(size_t source_row_pitch_bytes,
                             size_t pixel_stride_bytes, int image_height,
                             size_t max_descriptor_range_bytes,
                             size_t min_offset_alignment_bytes,
                             RowStripeUploadPlan& plan,
                             std::string& error_message);

bool
copy_rows_to_padded_buffer(const unsigned char* source_pixels,
                           size_t source_pixels_size,
                           size_t source_row_pitch_bytes, int image_height,
                           size_t padded_row_pitch_bytes,
                           unsigned char* destination_pixels,
                           size_t destination_pixels_size,
                           std::string& error_message);

}  // namespace Imiv
