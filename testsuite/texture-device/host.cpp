// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "arena.h"
#include "blend.h"
#include "texture_device_impl.h"  // IWYU pragma: keep
#include "texture_loader.h"

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include <string>
#include <vector>

using OIIO::ArgParse;
using OIIO::ImageOutput;
using OIIO::ImageSpec;
using OIIO::TypeDesc;
using OIIO::ustringhash;
using texture_device::blend_kernel;
using texture_device::BlendOp;
using texture_device::DTextureSystem;
using texture_device::Host;
using texture_device::MockDevice;
using texture_device::NullArena;
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
main(int argc, char* argv[])
{
    NullArena::use_unified_memory = false;
    std::string output_filename("out.exr");

    ArgParse ap;
    // clang-format off
    ap.intro("texture-device -- mock device texture workflow test")
            .usage("texture-device [--unified] [--output filename]");
        ap.arg("--unified", &NullArena::use_unified_memory)
      .help("Run with unified memory enabled")
      .action(ArgParse::store_true());
    ap.arg("--output %s:FILENAME", &output_filename)
      .help("Output EXR filename");
    // clang-format on
    if (ap.parse(argc, (const char**)argv) < 0) {
        OIIO::print("texture-device: {}\n", ap.geterror());
        ap.print_help();
        return 2;
    }

    if (!run_device_unit_tests()) {
        OIIO::print("texture-device: unit-tests-failed\n");
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
    device.copy_to(device_output,
                   tagged_ptr<const void>(output.data(), Host::mem_tag()),
                   output_bytes);

    op.output_buffer = device_output;

    constexpr int max_passes = 8;
    int completed_passes     = 0;
    bool converged           = false;
    for (int pass = 0; pass < max_passes; ++pass) {
        textures.begin_launch();
        device.copy_to(device_op, tagged_ptr<const void>(&op, Host::mem_tag()),
                       sizeof(op));
        device.run(width, height, &blend_kernel, device_op);
        device.copy_from(tagged_ptr<void>(&op, Host::mem_tag()), device_op,
                         sizeof(op));
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
        OIIO::print("texture-device: retry-limit-hit\n");

    device.copy_from(tagged_ptr<void>(output.data(), Host::mem_tag()),
                     device_output, output_bytes);

    (void)write_output(output_filename, width, height, output);

    device.free(device_op);
    device.free(device_output);

    OIIO::print("texture-device: mode={}\n",
                NullArena::use_unified_memory ? "unified" : "non-unified");
    OIIO::print("texture-device: startup-ok\n");
    OIIO::print("texture-device: passes={}\n", completed_passes);
    OIIO::print("texture-device: requests={}\n",
                textures.request_queue().size());
    OIIO::print("texture-device: wrote {}\n", output_filename);
    return 0;
}
