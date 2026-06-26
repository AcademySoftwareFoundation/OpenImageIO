// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>


// One-time OIIO setup: single-threaded mode.
// Safe to call from LLVMFuzzerInitialize; idempotent via static flag.
#define OIIO_FUZZ_INIT                         \
    do {                                       \
        static bool _inited = false;           \
        if (!_inited) {                        \
            _inited = true;                    \
            OIIO::attribute("threads", 1);     \
            OIIO::attribute("exr_threads", 1); \
        }                                      \
    } while (0)



// Standard single-subimage read. Wraps raw bytes in IOMemReader, opens via
// the public ImageInput API, reads pixels with an OOM guard, then closes.
inline void
oiio_fuzz_read(const uint8_t* data, size_t size, const char* fake_filename)
{
    OIIO::Filesystem::IOMemReader mem(data, size);
    auto inp = OIIO::ImageInput::open(fake_filename, nullptr, &mem);
    if (!inp) {
        (void)OIIO::geterror();
        return;
    }
    const OIIO::ImageSpec& spec = inp->spec();
    if (spec.image_pixels() > 0 && spec.image_pixels() < 256 * 1024 * 1024) {
        std::vector<uint8_t> buf(spec.image_pixels() * spec.nchannels);
        (void)inp->read_image(0, 0, 0, spec.nchannels, OIIO::TypeUInt8,
                              buf.data());
    }
    inp->close();
}



// Multi-subimage read for formats that support multiple subimages (e.g.,
// EXR, TIFF). Iterates all subimages with the same OOM guard per subimage.
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
        const OIIO::ImageSpec& spec = inp->spec();
        if (spec.image_pixels() > 0
            && spec.image_pixels() < 256 * 1024 * 1024) {
            std::vector<uint8_t> buf(spec.image_pixels() * spec.nchannels);
            (void)inp->read_image(inp->current_subimage(), 0, 0, spec.nchannels,
                                  OIIO::TypeUInt8, buf.data());
        }
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
        OIIO::string_view ext = OIIO::Filesystem::extension(fake_filename);
        std::string base      = OIIO::Filesystem::unique_path(
            OIIO::Filesystem::temp_directory_path() + "/oiio_fuzz_%%%%");
        tmppath = base + std::string(ext);
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
    const OIIO::ImageSpec& spec = inp->spec();
    if (spec.image_pixels() > 0 && spec.image_pixels() < 256 * 1024 * 1024) {
        std::vector<uint8_t> buf(spec.image_pixels() * spec.nchannels);
        (void)inp->read_image(0, 0, 0, spec.nchannels, OIIO::TypeUInt8,
                              buf.data());
    }
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
