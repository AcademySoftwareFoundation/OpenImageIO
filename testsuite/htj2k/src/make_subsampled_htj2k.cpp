// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Generator for the valid subsampled HTJ2K fixtures in this directory. The
// files it writes are committed, so this only needs to be run if they must
// be regenerated. It needs the OpenJPH headers and library, so it is built
// by hand rather than by CMake:
//
//   c++ -std=c++17 make_subsampled_htj2k.cpp -o make_subsampled_htj2k \
//       -I$OPENJPH_ROOT/include -L$OPENJPH_ROOT/lib -lopenjph
//   ./make_subsampled_htj2k .
//
// ojph_compress cannot write these: its raw (yuv) input path refuses a color
// transform and more than three components, and its ppm path cannot
// subsample. Every fixture is coded reversibly (5/3 wavelet, RCT), so the
// decoded samples equal pattern() exactly and the expected pixels can be
// checked without trusting the reader: replicate each sample over the
// full-resolution pixels it covers, with component column
// floor((XOsiz + x) / XRsiz) - ceil(XOsiz / XRsiz) for image column x, and
// likewise for rows.

#include <openjph/ojph_codestream.h>
#include <openjph/ojph_file.h>
#include <openjph/ojph_mem.h>
#include <openjph/ojph_params.h>

#include <cstdio>
#include <string>
#include <vector>


struct Fixture {
    const char* name;
    ojph::ui32 width, height;  // image size (extent minus offset)
    ojph::ui32 xoff, yoff;     // image offset (XOsiz, YOsiz)
    ojph::ui32 tilew, tileh;   // tile size; 0 means one tile
    ojph::ui32 txoff, tyoff;   // tile offset
    int depth;
    bool mct;
    std::vector<ojph::point> downsampling;  // one per component
};


static ojph::ui32
ceil_div(ojph::ui32 a, ojph::ui32 b)
{
    return (a + b - 1) / b;
}



static int
pattern(int xc, int yc, int c, int depth)
{
    return (xc * 7 + yc * 13 + c * 50 + (xc * yc) % 11) % (1 << depth);
}



static void
write_fixture(const std::string& outdir, const Fixture& fx)
{
    const int nc = int(fx.downsampling.size());
    ojph::codestream cs;
    ojph::param_siz siz = cs.access_siz();
    siz.set_image_extent(ojph::point(fx.xoff + fx.width, fx.yoff + fx.height));
    siz.set_num_components(nc);
    for (int c = 0; c < nc; ++c)
        siz.set_component(c, fx.downsampling[c], fx.depth, false);
    siz.set_image_offset(ojph::point(fx.xoff, fx.yoff));
    siz.set_tile_size(
        fx.tilew ? ojph::size(fx.tilew, fx.tileh)
                 : ojph::size(fx.xoff + fx.width, fx.yoff + fx.height));
    siz.set_tile_offset(ojph::point(fx.txoff, fx.tyoff));
    ojph::param_cod cod = cs.access_cod();
    cod.set_num_decomposition(2);
    cod.set_block_dims(16, 16);
    cod.set_color_transform(fx.mct);
    cod.set_reversible(true);
    // RPCL, the default, requires power-of-2 subsampling factors.
    cod.set_progression_order("LRCP");
    // OpenJPH requires interleaved input when a color transform is used.
    cs.set_planar(!fx.mct);

    // Component c holds columns [ceil(XOsiz/XRsiz), ceil(Xsiz/XRsiz)) and
    // likewise rows; pattern() is evaluated at those absolute coordinates.
    std::vector<ojph::ui32> cw(nc), ch(nc), cx0(nc), cy0(nc);
    for (int c = 0; c < nc; ++c) {
        const ojph::point ds = fx.downsampling[c];
        cx0[c]               = ceil_div(fx.xoff, ds.x);
        cy0[c]               = ceil_div(fx.yoff, ds.y);
        cw[c]                = ceil_div(fx.xoff + fx.width, ds.x) - cx0[c];
        ch[c]                = ceil_div(fx.yoff + fx.height, ds.y) - cy0[c];
    }

    const std::string path = outdir + "/" + fx.name;
    ojph::j2c_outfile file;
    file.open(path.c_str());
    cs.write_headers(&file);
    ojph::ui32 next      = 0;
    ojph::line_buf* line = cs.exchange(nullptr, next);
    auto send            = [&](int c, ojph::ui32 y) {
        for (ojph::ui32 x = 0; x < cw[c]; ++x)
            line->i32[x] = pattern(int(cx0[c] + x), int(cy0[c] + y), c,
                                   fx.depth);
        line = cs.exchange(line, next);
    };
    if (fx.mct) {
        for (ojph::ui32 y = 0; y < ch[0]; ++y)
            for (int c = 0; c < nc; ++c)
                send(c, y);
    } else {
        for (int c = 0; c < nc; ++c)
            for (ojph::ui32 y = 0; y < ch[c]; ++y)
                send(c, y);
    }
    cs.flush();
    cs.close();
    printf("wrote %s\n", path.c_str());
}



int
main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s outdir\n", argv[0]);
        return 1;
    }
    const std::string outdir = argv[1];
    const ojph::point full(1, 1), half(2, 2), quarter(4, 4);
    // clang-format off
    const std::vector<Fixture> fixtures = {
        // name, width, height, xoff, yoff, tilew, tileh, txoff, tyoff,
        //   depth, mct, downsampling
        // 4:2:0 at even and odd sizes. Odd sizes round the chroma planes up.
        { "valid-420.j2c", 16, 16, 0, 0, 0, 0, 0, 0, 8, false,
          { full, half, half } },
        { "valid-420-odd.j2c", 15, 13, 0, 0, 0, 0, 0, 0, 8, false,
          { full, half, half } },
        // Color transform (decoded in interleaved order), all components
        // subsampled alike; one tile, and then tiles plus both offsets.
        { "valid-420-mct.j2c", 11, 7, 0, 0, 0, 0, 0, 0, 8, true,
          { half, half, half } },
        { "valid-420-mct-tiled.j2c", 21, 15, 5, 3, 8, 6, 2, 1, 8, true,
          { half, half, half } },
        // Subsampled first component with an image offset that is not a
        // multiple of its factor, so the image size cannot come from it.
        { "valid-subsampled-offset.j2c", 11, 7, 3, 5, 0, 0, 0, 0, 8, false,
          { half, full, full } },
        // 10-bit, tiles, both offsets, non-power-of-2 factors, and a
        // subsampled first component, all at once.
        { "valid-tiled-offset-10bit.j2c", 21, 15, 5, 3, 8, 6, 2, 1, 10, false,
          { ojph::point(2, 3), full, ojph::point(3, 2) } },
        // Four components, the alpha subsampled differently again.
        { "valid-rgba-subsampled.j2c", 17, 9, 0, 0, 0, 0, 0, 0, 8, false,
          { full, half, half, ojph::point(4, 1) } },
        // Five components with a color transform: the reader keeps only
        // four, but interleaved order still delivers a row of every one.
        { "valid-5comp-mct.j2c", 11, 7, 0, 0, 0, 0, 0, 0, 8, true,
          { half, half, half, half, half } },
        // 3x3 tiles under 4x4 subsampling, each tile holding one chroma
        // sample.
        { "valid-tiles-under-subsampling.j2c", 9, 9, 0, 0, 3, 3, 0, 0, 8,
          false, { full, quarter, quarter } },
        // The first tile row holds no chroma rows (rows 1..3 of a component
        // sampled at multiples of 4), which every OpenJPH skips cleanly.
        { "valid-empty-tile-rows.j2c", 4, 10, 0, 1, 4, 4, 0, 0, 8, false,
          { full, ojph::point(1, 4), ojph::point(1, 4) } },
        // The same with columns, which OpenJPH before 0.27 cannot decode.
        { "valid-empty-tile-cols.j2c", 10, 4, 1, 0, 4, 4, 0, 0, 8, false,
          { full, ojph::point(4, 1), ojph::point(4, 1) } },
    };
    // clang-format on
    for (const Fixture& fx : fixtures)
        write_fixture(outdir, fx);
    return 0;
}
