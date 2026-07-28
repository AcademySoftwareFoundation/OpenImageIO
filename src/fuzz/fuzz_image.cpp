// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include "imageio_pvt.h"

using OIIO::Strutil::print;
using OIIO::Strutil::starts_with;


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



inline void
oiio_fuzz_read(const uint8_t* data, size_t size, const char* fake_filename)
{
    OIIO::Filesystem::IOMemReader mem(data, size);
    auto inp = OIIO::ImageInput::open(fake_filename, nullptr, &mem);
    if (!inp) {
        (void)OIIO::geterror();  // discard any errors
        return;
    }
    OIIO::pvt::test_read_all_images(*inp, OIIO::TypeUInt8);
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
    OIIO::Filesystem::write_binary_file(tmppath, OIIO::make_cspan(data, size));

    // Open and read via the normal public API.
    auto inp = OIIO::ImageInput::open(tmppath);
    if (!inp) {
        (void)OIIO::geterror();  // discard any errors
        return;
    }
    OIIO::pvt::test_read_all_images(*inp, OIIO::TypeUInt8);
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



// True for dispatch plugins (raw, ffmpeg) whose underlying libraries do not
// support IOProxy and require a temp-file read path instead.
inline bool
oiio_format_is_dispatch(OIIO::string_view format)
{
    return format == "raw" || format == "ffmpeg";
}



// Active format for this process, set once in LLVMFuzzerInitialize.
static std::string format_name;  // e.g., "jpeg"
static std::string fake_name;    // fake filename for open, e.g., "input.jpg"
static bool is_dispatch;         // true for raw, ffmpeg (no IOProxy support)


// Return all known format names sorted, excluding internal pseudo-formats.
static std::vector<std::string>
all_formats()
{
    std::vector<std::string> result;
    for (auto& [fmt, exts] : OIIO::get_extension_map()) {
        if (fmt != "null" && fmt != "term")
            result.push_back(fmt);
    }
    std::sort(result.begin(), result.end());
    return result;
}



extern "C" int
LLVMFuzzerInitialize(int* argc, char*** argv)
{
    OIIO_FUZZ_INIT;

    std::string format;

    // Priority 1: OIIO_FUZZ_FORMAT env var (used by GHA matrix jobs).
    if (const char* env = getenv("OIIO_FUZZ_FORMAT"))
        format = env;

    // Priority 2: argv[0] basename stripped of "fuzz_" prefix.
    // OSS-Fuzz invokes per-format symlinks: fuzz_jpeg -> fuzz_image.
    if (format.empty()) {
        std::string base = OIIO::Filesystem::filename((*argv)[0]);
        if (starts_with(base, "fuzz_"))
            format = base.substr(5);
        if (format == "image")  // canonical binary name, not a format
            format.clear();
    }

    // Priority 3: --format=<name> pseudo-arg. Also handle --list-formats.
    // Strip our pseudo-args before returning so libFuzzer doesn't reject them.
    for (int i = 1; i < *argc;) {
        OIIO::string_view arg((*argv)[i]);
        if (arg == "--list-formats") {
            for (auto& fmt : all_formats())
                print("{}\n", fmt);
            // Flush before exit(): ASan's LSAN atexit hook calls _Exit() when
            // it finds leaks, skipping stdio auto-flush.  Flushing first
            // ensures the list is on the pipe regardless of the atexit order.
            fflush(stdout);
            exit(0);
        }
        if (starts_with(arg, "--format=")) {
            if (format.empty())
                format = std::string(arg.substr(9));
            for (int j = i; j < *argc - 1; ++j)
                (*argv)[j] = (*argv)[j + 1];
            --*argc;
            continue;
        }
        ++i;
    }

    if (format.empty()) {
        print(stderr, "fuzz_image: no format specified.\n"
                      "  Set OIIO_FUZZ_FORMAT=<format>, use --format=<name>, "
                      "or invoke as fuzz_<format>.\n"
                      "  Available formats:\n");
        for (auto& fmt : all_formats())
            print(stderr, "    {}\n", fmt);
        exit(1);
    }

    // Validate: format must be compiled into this build.
    auto extmap = OIIO::get_extension_map();
    if (extmap.find(format) == extmap.end()) {
        print(stderr,
              "fuzz_image: unknown or unsupported format '{}'\n"
              "  Available formats:\n",
              format);
        for (auto& fmt : all_formats())
            print(stderr, "    {}\n", fmt);
        exit(1);
    }

    format_name = format;
    is_dispatch = oiio_format_is_dispatch(format_name);
    fake_name   = "input." + oiio_format_primary_ext(format_name);

    return 0;
}



extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (is_dispatch)
        oiio_fuzz_read_dispatch(data, size, fake_name.c_str());
    else
        oiio_fuzz_read(data, size, fake_name.c_str());
    return 0;
}
