// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Regression test: the flat read_native_scanline(s)/tile(s) calls must refuse
// a deep OpenEXR file rather than unpack every deep sample into the caller's
// one-value-per-channel buffer (a heap overrun when pixels hold several
// samples). These are the reader's own entry points, below the ImageInput
// read_scanlines/read_tiles wrappers, so they need their own guard.
// Conversely, the deep calls must refuse a flat file.

#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#include <vector>

using namespace OIIO;


// Fill a guard-padded buffer, run the read, and check that it failed with an
// error and left the buffer untouched.
template<typename READ>
static int
check(ImageInput& in, const char* label, const char* what, size_t bytes,
      READ read)
{
    std::vector<unsigned char> buf(bytes + 256, 0xCC);
    bool ok         = read(buf.data());
    std::string err = in.geterror();
    size_t smashed  = 0;
    for (auto c : buf)
        if (c != 0xCC)
            ++smashed;
    bool pass = !ok && !err.empty() && !smashed;
    OIIO::print("{} {}: ok={} smashed={} err=\"{}\" -> {}\n", label, what, ok,
                smashed, err, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}



static int
check_file(const char* filename, int use_core)
{
    OIIO::attribute("openexr:core", use_core);
    const char* label = use_core ? "core" : "classic";
    auto in           = ImageInput::open(filename);
    if (!in) {
        OIIO::print("{}: open failed: {}\n", label, OIIO::geterror());
        return 1;
    }
    const ImageSpec& spec = in->spec();
    int C = spec.nchannels, z = spec.z;
    int x0 = spec.x, x1 = spec.x + spec.width;
    int y0 = spec.y, y1 = spec.y + spec.height;
    if (!spec.deep) {
        // Flat file: the deep calls must refuse it.
        DeepData dd;
        bool ok = spec.tile_width
                      ? in->read_native_deep_tiles(0, 0, x0, x1, y0, y1, z,
                                                   z + 1, 0, C, dd)
                      : in->read_native_deep_scanlines(0, 0, y0, y1, z, 0, C,
                                                       dd);
        std::string err = in->geterror();
        bool pass       = !ok && !err.empty();
        OIIO::print("{} read_native_deep_{}: ok={} err=\"{}\" -> {}\n", label,
                    spec.tile_width ? "tiles" : "scanlines", ok, err,
                    pass ? "PASS" : "FAIL");
        return pass ? 0 : 1;
    }
    size_t pixelbytes = spec.pixel_bytes(true);
    size_t allbytes   = pixelbytes * spec.image_pixels();
    int err           = 0;
    if (spec.tile_width) {
        size_t tilebytes = spec.tile_bytes(true);
        err += check(*in, label, "read_native_tile", tilebytes, [&](void* d) {
            return in->read_native_tile(0, 0, x0, y0, z, d);
        });
        err += check(*in, label, "read_native_tiles", allbytes, [&](void* d) {
            return in->read_native_tiles(0, 0, x0, x1, y0, y1, z, z + 1, 0, C,
                                         d);
        });
    } else {
        size_t linebytes = spec.scanline_bytes(true);
        err += check(*in, label, "read_native_scanline", linebytes,
                     [&](void* d) {
                         return in->read_native_scanline(0, 0, y0, z, d);
                     });
        err += check(*in, label, "read_native_scanlines", allbytes,
                     [&](void* d) {
                         return in->read_native_scanlines(0, 0, y0, y1, z, 0, C,
                                                          d);
                     });
    }
    return err;
}



int
main(int argc, char** argv)
{
    int err = 0;
    for (int i = 1; i < argc; ++i) {
        OIIO::print("{}\n", Filesystem::filename(argv[i]));
        err += check_file(argv[i], 0);  // classic reader
        err += check_file(argv[i], 1);  // OpenEXR-Core reader
    }
    return err ? 1 : 0;
}
