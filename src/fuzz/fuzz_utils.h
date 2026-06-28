// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>


// One-time OIIO setup: single-threaded mode plus memory limits sized to the
// fuzzer's own RSS budget.
//
// libFuzzer's rss_limit_mb (see oiio_fuzz_image.options, currently 4096 MB)
// kills the process if it exceeds that resident size. OIIO's default
// decode-bomb guards (limits:imagesize_MB = 32768, limits:resolution =
// 1048576) are far larger than that budget, so a corrupt header claiming a
// multi-GB image would trip the fuzzer's OOM kill *before* OIIO's own guard
// rejects it -- a false positive crash. Lowering OIIO's limits well under the
// RSS budget lets OIIO reject such headers cleanly via the normal error path.
// imagesize_MB is set to half the RSS budget to leave headroom for decode
// scratch, the input buffer, and process overhead; resolution caps any single
// dimension to a value no real image reaches but that bounds per-scanline
// allocations.
//
// Safe to call from LLVMFuzzerInitialize; idempotent via static flag.
#define OIIO_FUZZ_INIT                                    \
    do {                                                  \
        static bool _inited = false;                      \
        if (!_inited) {                                   \
            _inited = true;                               \
            OIIO::attribute("threads", 1);                \
            OIIO::attribute("exr_threads", 1);            \
            OIIO::attribute("limits:imagesize_MB", 2048); \
            OIIO::attribute("limits:resolution", 65536);  \
        }                                                 \
    } while (0)



// Read all pixels of one subimage in small chunks, exercising the decode path
// without ever allocating a buffer proportional to the whole image. Tiled
// images are read one row of tiles at a time; scanline images are read 16 rows
// at a time. This keeps the resident buffer tiny so a corrupt-but-large image
// does not trip the fuzzer's RSS limit with a false positive. Read errors are
// intentionally ignored.
inline void
oiio_fuzz_read_subimage(OIIO::ImageInput* inp, int subimage)
{
    const OIIO::ImageSpec& spec = inp->spec();
    if (spec.image_pixels() <= 0 || spec.nchannels <= 0)
        return;
    const int nch = spec.nchannels;

    if (spec.tile_width > 0) {
        // Tiled: read a full-width row of tiles (one tile high, one tile deep)
        // per iteration. Tile extents are clamped to the image so a corrupt
        // header claiming a giant tile cannot force a giant buffer; read_tiles
        // accepts the image edge in place of a tile boundary.
        const int th = std::min(spec.tile_height > 0 ? spec.tile_height : 1,
                                spec.height);
        const int td = std::min(spec.tile_depth > 0 ? spec.tile_depth : 1,
                                spec.depth);
        const int xbegin = spec.x;
        const int xend   = spec.x + spec.width;
        std::vector<uint8_t> buf(size_t(spec.width) * th * td * nch);
        for (int z = spec.z; z < spec.z + spec.depth; z += td) {
            const int zend = std::min(z + td, spec.z + spec.depth);
            for (int y = spec.y; y < spec.y + spec.height; y += th) {
                const int yend = std::min(y + th, spec.y + spec.height);
                OIIO::image_span<uint8_t> ispan(buf.data(), nch, xend - xbegin,
                                                yend - y, zend - z);
                (void)inp->read_tiles(subimage, 0, xbegin, xend, y, yend, z,
                                      zend, 0, nch, ispan);
            }
        }
    } else {
        // Scanline: read 16 rows at a time. read_scanlines is 2D-oriented, so
        // depth is treated as 1 (volumetric data is tiled).
        const int chunk = 16;
        std::vector<uint8_t> buf(size_t(spec.width) * chunk * nch);
        for (int y = spec.y; y < spec.y + spec.height; y += chunk) {
            const int yend = std::min(y + chunk, spec.y + spec.height);
            OIIO::image_span<uint8_t> ispan(buf.data(), nch, spec.width,
                                            yend - y, 1);
            (void)inp->read_scanlines(subimage, 0, y, yend, 0, nch, ispan);
        }
    }
}



// Standard single-subimage read. Wraps raw bytes in IOMemReader, opens via
// the public ImageInput API, reads pixels in small chunks, then closes.
inline void
oiio_fuzz_read(const uint8_t* data, size_t size, const char* fake_filename)
{
    OIIO::Filesystem::IOMemReader mem(data, size);
    auto inp = OIIO::ImageInput::open(fake_filename, nullptr, &mem);
    if (!inp) {
        (void)OIIO::geterror();
        return;
    }
    oiio_fuzz_read_subimage(inp.get(), 0);
    inp->close();
}



// Multi-subimage read for formats that support multiple subimages (e.g.,
// EXR, TIFF). Iterates all subimages, reading each in small chunks.
inline void
oiio_fuzz_read_multi(const uint8_t* data, size_t size,
                     const char* fake_filename)
{
    OIIO::Filesystem::IOMemReader mem(data, size);
    auto inp = OIIO::ImageInput::open(fake_filename, nullptr, &mem);
    if (!inp) {
        (void)OIIO::geterror();  // discard any errors
        return;
    }
    do {
        oiio_fuzz_read_subimage(inp.get(), inp->current_subimage());
    } while (inp->seek_subimage(inp->current_subimage() + 1, 0));
    inp->close();
}



// Dispatch-plugin read for formats (raw, ffmpeg) whose underlying libraries
// do not support in-memory IOProxy reads. Writes fuzz data to a
// process-unique temp file, opens it, reads, and closes. The temp file is
// reused across calls (overwritten each time) for throughput.
//
// The fake_filename extension selects the right plugin: ".cr2" → raw.imageio
// (LibRaw handles sub-format dispatch internally), ".mkv" → ffmpeg.imageio.
inline void
oiio_fuzz_read_dispatch(const uint8_t* data, size_t size,
                        const char* fake_filename)
{
    // Compute a process-unique temp path once; reuse across calls.
    static std::string tmppath;
    if (tmppath.empty()) {
        // unique_path() substitutes '%%%%' with random hex digits.
        std::string ext  = OIIO::Filesystem::extension(fake_filename);
        std::string base = OIIO::Filesystem::unique_path(
            OIIO::Filesystem::temp_directory_path() + "/oiio_fuzz_%%%%");
        tmppath = base + ext;
    }

    // Write fuzz data.
    FILE* f = fopen(tmppath.c_str(), "wb");
    if (!f)
        return;
    fwrite(data, 1, size, f);
    fclose(f);

    // Open and read via the normal public API.
    auto inp = OIIO::ImageInput::open(tmppath);
    if (!inp) {
        (void)OIIO::geterror();  // discard any errors
        return;
    }
    oiio_fuzz_read_subimage(inp.get(), 0);
    inp->close();
}



// Return the primary file extension for a named format (e.g., "jpeg" →
// "jpg", "tiff" → "tif"). Uses the live extension_list so newly registered
// formats are covered automatically. Returns "" if the format is unknown.
inline std::string
oiio_format_primary_ext(OIIO::string_view format)
{
    auto extmap = OIIO::get_extension_map();
    auto it     = extmap.find(std::string(format));
    if (it == extmap.end() || it->second.empty())
        return {};
    return it->second[0];
}



// True for formats whose readers support multiple subimages (EXR, TIFF, …).
// Uses ImageInput::supports("multiimage") on a created reader so the check
// stays correct as new formats are added.
inline bool
oiio_format_is_multi(OIIO::string_view format)
{
    auto inp = OIIO::ImageInput::create(format);
    return inp && inp->supports("multiimage");
}



// True for dispatch plugins (raw, ffmpeg) whose underlying libraries do not
// support IOProxy and require a temp-file read path instead.
inline bool
oiio_format_is_dispatch(OIIO::string_view format)
{
    return format == "raw" || format == "ffmpeg";
}
