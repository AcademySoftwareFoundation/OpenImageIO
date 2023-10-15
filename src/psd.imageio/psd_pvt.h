// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace psd_pvt {

struct FileHeader {
    char signature[4];
    uint16_t version;
    uint16_t channel_count;
    uint32_t height;
    uint32_t width;
    uint16_t depth;
    uint16_t color_mode;
};



struct ColorModeData {
    uint32_t length;
    std::unique_ptr<uint8_t[]> data;
};



struct ImageResourceBlock {
    char signature[4];
    uint16_t id;
    std::string name;
    uint32_t length;
    int64_t pos;
};

}  // namespace psd_pvt

OIIO_PLUGIN_NAMESPACE_END
