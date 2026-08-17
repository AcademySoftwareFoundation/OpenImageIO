// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "texture_device_decl.h"

namespace texture_device {

struct BlendOp {
    int width  = 0;
    int height = 0;
    OIIO::ustringhash name_a;
    OIIO::ustringhash name_b;
    DTextureSystem<MockDevice> texture_system;
    tagged_ptr<RGBA> output_buffer = nullptr;
};

void
blend_kernel(int x, int y, tagged_ptr<void> data);

}  // namespace texture_device
