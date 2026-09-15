// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_BEGIN
namespace pvt {

bool openmetadata_enabled() noexcept;
bool openmetadata_decode_exif(cspan<uint8_t> bytes, ImageSpec& spec) noexcept;
bool openmetadata_decode_xmp(string_view bytes, ImageSpec& spec) noexcept;

// Format already decoded values using the same tables as the native readers.
bool openmetadata_exif_attribute(string_view ifd, uint16_t tag, uint16_t type,
                                 uint32_t count, cspan<uint8_t> bytes,
                                 ImageSpec& spec);
bool openmetadata_xmp_attribute(string_view name, string_view value,
                                bool sequence, ImageSpec& spec);

}  // namespace pvt
OIIO_NAMESPACE_END
