// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include "fuzz_utils.h"

using OIIO::Strutil::print;
using OIIO::Strutil::starts_with;


// Active format for this process, set once in LLVMFuzzerInitialize.
static std::string format_name;  // e.g., "jpeg"
static std::string fake_name;    // fake filename for open, e.g., "input.jpg"
static bool is_dispatch;         // true for raw, ffmpeg (no IOProxy support)
static bool is_multi;            // true for formats with multiple subimages


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
    is_multi    = oiio_format_is_multi(format_name);
    fake_name   = "input." + oiio_format_primary_ext(format_name);

    return 0;
}



extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (is_dispatch)
        oiio_fuzz_read_dispatch(data, size, fake_name.c_str());
    else if (is_multi)
        oiio_fuzz_read_multi(data, size, fake_name.c_str());
    else
        oiio_fuzz_read(data, size, fake_name.c_str());
    return 0;
}
