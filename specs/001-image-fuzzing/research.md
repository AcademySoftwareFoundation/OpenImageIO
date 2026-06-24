# Research: Image Format Fuzzing Infrastructure

**Feature**: 001-image-fuzzing | **Date**: 2026-06-23

## 1. Fuzzing Engine Selection

**Decision**: Use `$LIB_FUZZING_ENGINE` abstraction (defaults to libFuzzer locally).

**Rationale**: The harness entry point (`LLVMFuzzerTestOneInput`) is the common ABI shared
by libFuzzer, AFL++, Honggfuzz, and Centipede. Linking against `$LIB_FUZZING_ENGINE`
instead of hardcoding `-fsanitize=fuzzer` means OSS-Fuzz runs all four engines on the
same harness binaries with zero changes. Locally, developers set
`LIB_FUZZING_ENGINE=-fsanitize=fuzzer` (libFuzzer) since it requires only clang.

Centipede was evaluated and rejected for local use: its README self-describes as
"work-in-progress, tested within a small team on a couple of targets, no stable
interface." AFL++ was evaluated but its fork-server model is slower for the in-process
library fuzzing pattern OIIO uses; it provides value at OSS-Fuzz scale where it runs
automatically.

**Alternatives considered**:
- libFuzzer hardcoded: functionally identical for v1, but blocks future OSS-Fuzz
  engine diversity without a build script change.
- AFL++ only: slower in-process throughput; requires file-based I/O unless using
  persistent mode (more complex harness).
- Centipede: experimental, unstable interface, overkill for the scale we target.

## 1a. FuzzTest vs libFuzzer: Why Not FuzzTest for File Format Harnesses?

**Decision**: Use raw `LLVMFuzzerTestOneInput` harnesses (not FuzzTest) for file format
fuzzing. FuzzTest is explicitly reserved for future structured API fuzzing.

**What FuzzTest is**: A C++ testing framework from Google that sits *on top of* a fuzzing
engine (libFuzzer or Centipede). It provides a property-based testing API using a
`FUZZ_TEST` macro that integrates with GoogleTest:

```cpp
void ReadingImageNeverCrashes(int width, int height, std::vector<uint8_t> pixels) {
    MyApi(width, height, pixels);
}
FUZZ_TEST(Suite, ReadingImageNeverCrashes)
    .WithDomains(InRange(1,16384), InRange(1,16384), Arbitrary<std::vector<uint8_t>>());
```

The same test runs as a normal unit test in regular CI (fast, deterministic seed
replay) and as a full fuzzing campaign when built with fuzzer instrumentation.

**Why it does not help for file format fuzzing**: FuzzTest's core value is
*structured domain constraints* — generating type-safe, range-bounded inputs rather
than raw bytes. For image format reader fuzzing, the input IS raw bytes (`Arbitrary<
std::vector<uint8_t>>()`), which is exactly equivalent to `LLVMFuzzerTestOneInput`.
FuzzTest adds a GoogleTest dependency and build complexity without improving coverage
or throughput for this use case.

**Why it does NOT conflict with future FuzzTest use**: The two approaches are
completely independent — separate source files, separate CMake targets, separate GHA
jobs, separate OSS-Fuzz target names. FuzzTest harnesses can be added to `src/fuzz/`
(or a new `src/fuzztest/` directory) at any time without modifying any existing
`LLVMFuzzerTestOneInput` harness. OSS-Fuzz supports both in the same project.

**Where FuzzTest would add clear value** (future work, not v1):
- `ImageBufAlgo` operations with typed parameters (width, height, channel count,
  filter type, pixel format) — structured inputs where domain constraints dramatically
  reduce wasted iterations.
- `oiiotool` command-line argument fuzzing — structured sequences of valid operations
  with random but type-correct arguments.
- `ImageCache`/`TextureSystem` parameter fuzzing — tile sizes, cache sizes, filter
  modes, thread counts.

These are all cases where the input has typed structure and domain invariants that
FuzzTest can exploit. File bytes have none of that structure.

**Alternatives considered**:
- Use FuzzTest for everything: Adds GoogleTest build dependency. For byte-stream
  parsing, provides no coverage improvement. Rejected.
- Use FuzzTest for file formats via `Arbitrary<vector<uint8_t>>`: Functionally
  identical to `LLVMFuzzerTestOneInput` but adds dependency overhead. Rejected.

## 2. In-Memory I/O via IOProxy

**Decision**: Use `OIIO::Filesystem::IOMemReader` to pass raw fuzz bytes directly to
`ImageInput::open()`, avoiding disk I/O entirely.

**Rationale**: `ImageInput::open(filename, config, ioproxy)` accepts an `IOProxy*`
argument. `IOMemReader` wraps a `const void*, size_t` buffer as a seekable in-memory
stream. This means:
- No temp file creation per fuzz iteration (critical for throughput — disk I/O would
  reduce executions/sec by 10–100×).
- No cleanup needed per iteration.
- The format is indicated by passing a fake filename with the correct extension
  (e.g., `"input.exr"`) so the format dispatcher routes to the correct plugin.

The `IOMemReader` approach is already used in OIIO's test infrastructure and is the
canonical way to fuzz in-process readers without disk I/O.

**Alternatives considered**:
- Write bytes to a temp file per iteration: correct but 10–100× slower; temp file
  cleanup is error-prone under sanitizer exits.
- Use `ImageInput::create("formatname")` + `open_memory()`: no such API exists;
  `IOProxy` is the correct abstraction.

## 3. Build System Integration

**Decision**: New `OIIO_BUILD_FUZZ_TARGETS` CMake option (default OFF) in a new
`src/fuzz/CMakeLists.txt`. An `add_fuzz_target(format)` macro handles the repetitive
per-format target definition. The existing `SANITIZE` CMake variable handles
`-fsanitize=address,undefined`; fuzzer instrumentation (`-fsanitize=fuzzer` or
`$LIB_FUZZING_ENGINE`) is handled separately as a linker flag on fuzz targets only.

**Rationale**: OIIO already has `SANITIZE` support in `src/cmake/compiler.cmake` (adds
`-fsanitize=<list>` compile and link options). The fuzz targets need
`-fsanitize=address,undefined` for sanitizer coverage AND `-fsanitize=fuzzer` (or
`$LIB_FUZZING_ENGINE`) for the fuzzer runtime. These must be applied to fuzz target
executables only — not to libOpenImageIO itself — so a dedicated section in
`src/fuzz/CMakeLists.txt` is the right place.

Key CMake pattern:
```cmake
# Resolve fuzzing engine: OSS-Fuzz sets LIB_FUZZING_ENGINE; local default is -fsanitize=fuzzer
if(DEFINED ENV{LIB_FUZZING_ENGINE})
    set(OIIO_FUZZING_ENGINE "$ENV{LIB_FUZZING_ENGINE}")
else()
    set(OIIO_FUZZING_ENGINE "-fsanitize=fuzzer")
endif()

macro(add_fuzz_target name)
    add_executable(fuzz_${name} fuzz_${name}.cpp)
    target_link_libraries(fuzz_${name} PRIVATE OpenImageIO)
    target_compile_options(fuzz_${name} PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_${name} PRIVATE
        -fsanitize=address,undefined ${OIIO_FUZZING_ENGINE})
endmacro()
```

Note: `libOpenImageIO` itself is built with ASan+UBSan compile flags (via the `SANITIZE`
cmake variable) but NOT with `-fsanitize=fuzzer` — the fuzzer instrumentation applies
only to fuzz executables.

**Alternatives considered**:
- Separate fuzz CMake preset: more isolated but duplicates all dependency resolution.
- Makefile targets wrapping cmake: not worth the indirection.

## 4. GHA Workflow Architecture

**Decision**: `.github/workflows/fuzz.yml` with a parallel matrix (`fail-fast: false`),
two-tier time budgets, GHA cache for evolved corpus, and artifact upload on crash.

**Key workflow design decisions**:

### Container
Reuse `aswf/ci-oiio:2026.3` (same as the existing sanitizer CI job). This container
already has clang, all format libraries (OpenEXR, libtiff, libpng, libjpeg-turbo, etc.),
and is the known-good environment for ASan builds. No new container needed.

### Matrix definition
```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      # Tier 1: complex/high-risk — 5h30m (19800s)
      - { format: exr,       tier: 1, max_total_time: 19800 }
      - { format: tiff,      tier: 1, max_total_time: 19800 }
      - { format: jpeg,      tier: 1, max_total_time: 19800 }
      - { format: png,       tier: 1, max_total_time: 19800 }
      - { format: dpx,       tier: 1, max_total_time: 19800 }
      - { format: psd,       tier: 1, max_total_time: 19800 }
      - { format: heif,      tier: 1, max_total_time: 19800 }
      - { format: webp,      tier: 1, max_total_time: 19800 }
      - { format: jpegxl,    tier: 1, max_total_time: 19800 }
      - { format: jpeg2000,  tier: 1, max_total_time: 19800 }
      - { format: raw,       tier: 1, max_total_time: 19800 }
      - { format: dicom,     tier: 1, max_total_time: 19800 }
      # Tier 2: simpler — 1h (3600s)
      - { format: bmp,       tier: 2, max_total_time: 3600 }
      - { format: cineon,    tier: 2, max_total_time: 3600 }
      - { format: dds,       tier: 2, max_total_time: 3600 }
      - { format: fits,      tier: 2, max_total_time: 3600 }
      - { format: gif,       tier: 2, max_total_time: 3600 }
      - { format: hdr,       tier: 2, max_total_time: 3600 }
      - { format: ico,       tier: 2, max_total_time: 3600 }
      - { format: iff,       tier: 2, max_total_time: 3600 }
      - { format: pnm,       tier: 2, max_total_time: 3600 }
      - { format: rla,       tier: 2, max_total_time: 3600 }
      - { format: sgi,       tier: 2, max_total_time: 3600 }
      - { format: softimage, tier: 2, max_total_time: 3600 }
      - { format: targa,     tier: 2, max_total_time: 3600 }
      - { format: ffmpeg,    tier: 2, max_total_time: 3600 }
      - { format: zfile,     tier: 2, max_total_time: 3600 }
      - { format: openvdb,   tier: 2, max_total_time: 3600 }  # conditional on OPENVDB_FOUND
      - { format: ptex,      tier: 2, max_total_time: 3600 }  # conditional on PTEX_FOUND
```

### Corpus cache key
`fuzz-corpus-${{ matrix.format }}-${{ github.ref_name }}`

Restoring from a broader fallback key `fuzz-corpus-${{ matrix.format }}-` ensures the
cache is seeded even when switching branches. This is standard GHA cache pattern.

### Per-format fuzz invocation
```bash
./fuzz_${FORMAT} \
    corpus/${FORMAT} \
    -max_total_time=${{ matrix.max_total_time }} \
    -timeout=60 \
    -artifact_prefix=crash_${FORMAT}_ \
    -jobs=$(nproc)
```

`-timeout=60` kills any single input that takes more than 60 seconds (prevents hangs).
`-jobs=$(nproc)` uses all available cores within the job for parallel fuzzing workers.

### Crash detection and artifact upload
libFuzzer writes crash files as `crash_<format>_<hash>` in the working directory when
it finds a bug. A post-run step checks for `crash_*` files and uploads them; the step
exit code propagates failure.

**Alternatives considered**:
- Custom time-budgeting script: unnecessary with parallel matrix (each job owns its time).
- ubuntu-latest without container: would need manual installation of all format libs,
  fragile and maintenance-heavy.

## 5. Harness Pattern

**Decision**: Each harness is ~25 lines. Shared logic in `fuzz_utils.h`.

**Standard harness template**:
```cpp
// fuzz_jpeg.cpp
// SPDX-License-Identifier: Apache-2.0
#include "fuzz_utils.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    oiio_fuzz_read(data, size, "input.jpg");
    return 0;
}
```

**`fuzz_utils.h` core function**:
```cpp
inline void oiio_fuzz_read(const uint8_t* data, size_t size,
                            const char* fake_filename)
{
    OIIO::Filesystem::IOMemReader mem(
        const_cast<void*>(static_cast<const void*>(data)), size);
    OIIO::ImageSpec config;
    auto inp = OIIO::ImageInput::open(fake_filename, &config, &mem);
    if (!inp)
        return;
    OIIO::ImageSpec spec = inp->spec();
    // Cap pixel read to avoid OOM on adversarially large dimensions
    if (spec.image_pixels() > 0 && spec.image_pixels() < 64 * 1024 * 1024) {
        std::vector<uint8_t> buf(spec.image_pixels() * spec.nchannels);
        inp->read_image(0, 0, 0, spec.nchannels, OIIO::TypeUInt8, buf.data());
    }
    inp->close();
}
```

**OOM guard**: The pixel dimension check caps memory allocation. A fuzzer-generated
header claiming a 1M×1M image would cause a multi-GB allocation without this guard.
64 MP (≈256MB at 4 bytes/pixel) is a reasonable cap; format readers that crash before
`read_image` are still detected.

**Multi-subimage formats** (EXR, TIFF): The EXR/TIFF harness iterates subimages:
```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OIIO::Filesystem::IOMemReader mem(...);
    auto inp = OIIO::ImageInput::open("input.exr", nullptr, &mem);
    if (!inp) return 0;
    do {
        auto& spec = inp->spec();
        if (spec.image_pixels() < 64 * 1024 * 1024) {
            std::vector<uint8_t> buf(spec.image_pixels() * spec.nchannels);
            inp->read_image(0, 0, 0, spec.nchannels, OIIO::TypeUInt8, buf.data());
        }
    } while (inp->seek_subimage(inp->current_subimage() + 1, 0));
    inp->close();
    return 0;
}
```

**Alternatives considered**:
- Calling internal plugin methods directly: violates Principle I, no benefit.
- Writing to disk per iteration: 10–100× slower, rejected.
- Using `ImageBuf` instead of `ImageInput`: adds ImageCache overhead, less direct.

## 6. Seed Corpus Strategy

**Decision**: Source seeds from existing test image repositories already checked out for
CI. Copy 1–3 small (< 100KB) files per format into `src/fuzz/corpora/<format>/` via a
seed population script. Only ~5 formats require generating synthetic seeds from scratch.

### Corpus sources (all already present in CI checkout)

The testsuite and several companion repos live alongside the OIIO source in CI and in
local development. `testsuite/runtest.py` already detects `../oiio-images` at that
relative path. The fuzz seed population script mirrors this convention.

| Format(s) | Source repo | Path |
|-----------|-------------|------|
| exr, tiff, jpeg, png, bmp, dds, ico, pnm, psd, rla, sgi, tga, openvdb, ptex | **testsuite** (in-repo) | `testsuite/<format>/src/` and `testsuite/common/` |
| gif, webp, heif, dpx, cineon, raw†, softimage | **oiio-images** | `../oiio-images/<format>/` |
| jpeg2000 | **j2kp4files_v1_5** | `../j2kp4files_v1_5/` |
| fits | **fits-images** | `../fits-images/ftt4b/` |
| dicom | **dicom-images-pvt** | `../dicom-images-pvt/` |
| ffmpeg†† | **testsuite** | `testsuite/ffmpeg/ref/*.mkv` |
| hdr, iff, jpegxl, zfile | **generate** | `oiiotool --create` or format-specific tool |

† `raw.imageio` is a dispatch plugin over LibRaw that reads all camera RAW formats (CR2,
NEF, RAF, ORF, ARW, PEF, RW2, …). It is read-only in OIIO. Seeds in `../oiio-images/raw/`
cover several camera brands.

†† `ffmpeg.imageio` is a dispatch plugin over the FFmpeg library that reads all video
container formats (MKV, MP4, MOV, AVI, …). It is read-only in OIIO. Seeds in
`testsuite/ffmpeg/ref/` cover two MKV variants with different color spaces.

Only the last row (~4 formats) requires generating synthetic seeds. All other formats
have real sample files available in repos already used by CI.

**Note on crash-* files in testsuite**: Several formats (bmp, dds, ico, psd, rla, tga,
tiff) have existing `crash-*` files in their testsuite directories — past bug reproducers
that were fixed. These are excellent corpus seeds: they test known edge cases and the
fuzzer will mutate around them to find nearby bugs. Include them alongside valid files.

**Seed population script**: `src/fuzz/populate_corpora.py` — run once to copy seeds from
the source repos into `src/fuzz/corpora/`. The copied files are committed to the repo so
the corpus is self-contained (required for OSS-Fuzz corpus zip packaging). The script is
idempotent and documents exactly where each seed came from.

### Special harness design for multi-format dispatch plugins (raw, ffmpeg)

Both `raw.imageio` (LibRaw) and `ffmpeg.imageio` (FFmpeg) are read-only dispatch plugins
that hand off parsing to a well-maintained third-party library. Both libraries are
heavily fuzzed by their own projects (LibRaw and FFmpeg each have OSS-Fuzz coverage).

**Fuzzing goal for these two**: verify that the OIIO plugin wrapper correctly handles
errors and unexpected return values from the underlying library — not to find bugs in
LibRaw or FFmpeg themselves. A small corpus of 2–3 representative files is sufficient.

**Harness strategy**: use `ImageInput::create("raw")` / `ImageInput::create("ffmpeg")`
to force the specific plugin regardless of file extension, then open via `IOMemReader`.
This avoids the need to fake a specific extension for sub-format routing; LibRaw and
FFmpeg both detect the actual format internally from magic bytes.

```cpp
// fuzz_raw.cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OIIO_FUZZ_INIT;
    OIIO::Filesystem::IOMemReader mem(const_cast<void*>((const void*)data), size);
    auto inp = OIIO::ImageInput::create("raw");
    if (!inp) return 0;  // plugin not compiled in
    OIIO::ImageSpec config;
    if (!inp->open("input", config, &mem)) return 0;
    // read + close as usual
    inp->close();
    return 0;
}
```

**Corpus minimization**: libFuzzer's `-merge=1` flag can post-process the evolved corpus
after an extended run to keep only files that add coverage. This is a manual step,
not automated in v1.

## 7. Excluded Formats

| Format | Reason |
|--------|--------|
| `null` | Test stub with no real parsing; fuzzing adds no value. |
| `term` | Output-only (terminal display); no `ImageInput` implementation. |
| `r3d` | Requires proprietary RED Digital Cinema SDK; not available in open CI. |

**openvdb and ptex are included** (Tier 2, conditionally compiled). Initial plan excluded
them with weak reasoning. Both have full `ImageInput` implementations, both accept
binary file data through the standard API, and both are complex enough to harbor parser
bugs. They are wrapped in `if(OPENVDB_FOUND)` / `if(PTEX_FOUND)` CMake guards exactly
like `heif`, `jpegxl`, and `raw`. They are conditionally included in the GHA matrix
only when their libraries are present in the build container.

## 8. OSS-Fuzz Onboarding Path

OSS-Fuzz requires three files in `projects/openimageio/`:
1. `project.yaml` — project metadata, language: c++, primary_contact, fuzzing_engines.
2. `Dockerfile` — `FROM gcr.io/oss-fuzz-base/base-builder` + format library installs.
3. `build.sh` — cmake configure + build, then `cp $WORK/build/src/fuzz/fuzz_* $OUT/`.

The `build.sh` pattern:
```bash
cmake -B $WORK/build $SRC/openimageio \
    -DOIIO_BUILD_FUZZ_TARGETS=ON \
    -DSANITIZE=address,undefined \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX
cmake --build $WORK/build --target all_fuzz_targets -j$(nproc)
cp $WORK/build/src/fuzz/fuzz_* $OUT/
for fmt in exr tiff jpeg png ...; do
    zip -j $OUT/fuzz_${fmt}_seed_corpus.zip src/fuzz/corpora/${fmt}/*
done
```

OSS-Fuzz sets `LIB_FUZZING_ENGINE`, `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `OUT`, `WORK`,
`SRC` as environment variables. Our CMakeLists respects these automatically.

These three files are **not** part of v1 deliverables (US-5 is P3) but the structure
documented above is what makes them a small incremental step.
