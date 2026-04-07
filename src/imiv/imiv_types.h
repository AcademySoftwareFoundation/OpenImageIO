// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_build_config.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/typedesc.h>
#include <imgui.h>

namespace Imiv {

enum class UploadDataType : uint32_t {
    UInt8   = 0,
    UInt16  = 1,
    UInt32  = 2,
    Half    = 3,
    Float   = 4,
    Double  = 5,
    Unknown = 255
};

size_t
upload_data_type_size(UploadDataType type);
const char*
upload_data_type_name(UploadDataType type);
OIIO::TypeDesc
upload_data_type_to_typedesc(UploadDataType type);
bool
map_spec_type_to_upload(OIIO::TypeDesc spec_type, UploadDataType& upload_type,
                        OIIO::TypeDesc& read_format);

struct LoadedImage {
    std::string path;
    std::string metadata_color_space;
    std::string data_format_name;
    int width              = 0;
    int height             = 0;
    int orientation        = 1;
    int nchannels          = 0;
    int subimage           = 0;
    int miplevel           = 0;
    int nsubimages         = 1;
    int nmiplevels         = 1;
    UploadDataType type    = UploadDataType::Unknown;
    size_t channel_bytes   = 0;
    size_t row_pitch_bytes = 0;
    std::vector<unsigned char> pixels;
    std::vector<std::string> channel_names;
    std::vector<std::pair<std::string, std::string>> longinfo_rows;
};

struct PreviewControls {
    float exposure           = 0.0f;
    float gamma              = 1.0f;
    float offset             = 0.0f;
    int color_mode           = 0;
    int channel              = 0;
    int use_ocio             = 0;
    int orientation          = 1;
    int linear_interpolation = 0;
};

}  // namespace Imiv
