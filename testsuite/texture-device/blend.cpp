// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "blend.h"
#include "filtering_impl.h"
#include "texture_device_impl.h"

namespace texture_device {

void
blend_kernel(int x, int y, tagged_ptr<void> data)
{
    tagged_ptr<BlendOp> op(data);
    const float resx = static_cast<float>(op->width);
    const float resy = static_cast<float>(op->height);
    const float invx = 1.0f / resx;
    const float invy = 1.0f / resy;
    const float u    = (static_cast<float>(x) + 0.5f) * invx;
    const float v    = (static_cast<float>(y) + 0.5f) * invy;

    Vec2 duA { invx, 0.0f };
    Vec2 dvA { 0.0f, invy };
    Vec2 duB = 4.0f * duA;
    Vec2 dvB = 4.0f * dvA;

    RGBA A = op->texture_system.lookup(op->name_a, u, v, duA, dvA);
    RGBA B = op->texture_system.lookup(op->name_b, u, v, duB, dvB);

    if (op->texture_system.failures())
        return;

    op->output_buffer[y * op->width + x] = 0.5f * A + 0.5f * B;
}

}  // namespace texture_device
