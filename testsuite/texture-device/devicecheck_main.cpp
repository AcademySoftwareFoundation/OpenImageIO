// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "blend.h"

int
main()
{
    // Keep a direct kernel call in this binary so link-time symbol resolution
    // covers the full devicecheck path.
    texture_device::BlendOp op;
    op.width  = 1;
    op.height = 1;
    texture_device::blend_kernel(0, 0,
                                 texture_device::tagged_ptr<void>(&op, "Host"));

    // Force an observable read of kernel-mutated state to avoid DCE.
    volatile bool kernel_recorded_failure = op.texture_system.failures();
    (void)kernel_recorded_failure;

    return texture_device::run_device_unit_tests() ? 0 : 1;
}
