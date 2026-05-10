// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "arena.h"
#include "blend.h"
#include "texture_device_impl.h"  // IWYU pragma: keep
#include "texture_loader.h"

#include <OpenImageIO/imageio.h>

#include <iostream>
#include <string>
#include <vector>

using OIIO::ImageOutput;
using OIIO::ImageSpec;
using OIIO::TypeDesc;
using OIIO::ustringhash;
using texture_device::blend_kernel;
using texture_device::BlendOp;
using texture_device::DTextureSystem;
using texture_device::Host;
using texture_device::MockDevice;
using texture_device::Request;
using texture_device::RGBA;
using texture_device::run_device_unit_tests;
using texture_device::tagged_ptr;
using texture_device::TextureLoader;

namespace {

bool
write_output(const std::string& filename, int width, int height,
             const std::vector<RGBA>& pixels)
{
    auto out = ImageOutput::create(filename);
    if (!out)
        return false;

    ImageSpec spec(width, height, 4, TypeDesc::FLOAT);
    if (!out->open(filename, spec))
        return false;

    bool ok = out->write_image(TypeDesc::FLOAT, pixels.data());
    ok      = out->close() && ok;
    return ok;
}

}  // namespace

int
main()
{
    if (!run_device_unit_tests()) {
        std::cout << "texture-device: unit-tests-failed\n";
        return 2;
    }

    const int width  = 256;
    const int height = 256;

    Host host;
    MockDevice device;
    TextureLoader loader;

    BlendOp op;
    DTextureSystem<Host, MockDevice> textures(host, device, op.texture_system);
    static constexpr const char* kTextureSearchPaths[] = {
        "../common/textures",
        "../../../testsuite/common/textures",
        "testsuite/common/textures",
    };
    for (const char* path : kTextureSearchPaths)
        loader.add_texture_path(path);
    textures.request_queue().clear();

    std::vector<RGBA> output(width * height, RGBA(0.0f, 0.0f, 0.0f, 0.0f));

    op.width         = width;
    op.height        = height;
    op.name_a        = ustringhash("grid.tx");
    op.name_b        = ustringhash("checker.tx");
    op.output_buffer = nullptr;
    textures.sync_to_managed();

    const size_t output_bytes      = output.size() * sizeof(RGBA);
    tagged_ptr<void> device_output = device.alloc(output_bytes,
                                                  "host::output_buffer");
    tagged_ptr<void> device_op     = device.alloc(sizeof(BlendOp),
                                                  "host::blend_op");
    device.copy_to(device_output, tagged_ptr<const void>(output.data(), "Host"),
                   output_bytes);

    op.output_buffer = device_output;

    constexpr int max_passes = 8;
    int completed_passes     = 0;
    bool converged           = false;
    for (int pass = 0; pass < max_passes; ++pass) {
        textures.begin_launch();
        device.copy_to(device_op, tagged_ptr<const void>(&op, "Host"),
                       sizeof(op));
        device.run(width, height, &blend_kernel, device_op);
        device.copy_from(tagged_ptr<void>(&op, "Host"), device_op, sizeof(op));
        ++completed_passes;

        textures.sync_from_managed();
        if (textures.needs_retry()) {
            textures.sync_to_managed();
            continue;
        }

        if (!textures.failures()) {
            converged = true;
            break;
        }

        textures.process_requests(
            [&](const Request& req, DTextureSystem<Host, MockDevice>* ts) {
                return loader.process_request(req, ts);
            });

        textures.sync_to_managed();
    }

    if (!converged)
        std::cout << "texture-device: retry-limit-hit\n";

    device.copy_from(tagged_ptr<void>(output.data(), "Host"), device_output,
                     output_bytes);

    (void)write_output("out.exr", width, height, output);

    device.free(device_op);
    device.free(device_output);

    std::cout << "texture-device: startup-ok\n";
    std::cout << "texture-device: passes=" << completed_passes << "\n";
    std::cout << "texture-device: requests=" << textures.request_queue().size()
              << "\n";
    std::cout << "texture-device: wrote out.exr\n";
    return 0;
}
