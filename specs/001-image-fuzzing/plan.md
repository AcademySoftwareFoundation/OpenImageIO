# Implementation Plan: Image Format Fuzzing Infrastructure

**Branch**: `001-image-fuzzing` | **Date**: 2026-06-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-image-fuzzing/spec.md`

## Summary

Add a libFuzzer-based fuzzing infrastructure to OpenImageIO that exercises all supported
image format readers via the `ImageInput` API using in-memory `IOProxy` buffers for
maximum throughput. A new `src/fuzz/` directory holds a single dynamic dispatch harness
(`fuzz_image`) plus a shared helper and seed corpora. The harness queries
`OIIO::get_string_attribute("extension_list")` at startup to discover every compiled-in
format automatically — new format plugins are covered without any manual harness update.
A new `.github/workflows/fuzz.yml` runs nightly as a parallel matrix (one GHA job per
format), each job setting `OIIO_FUZZ_FORMAT=<format>` to target a single format and its
corpus. For OSS-Fuzz, per-format symlinks (`fuzz_jpeg -> fuzz_image`) let the binary
read its own name from `argv[0]` to determine the format. The CMake build is gated by
`OIIO_BUILD_FUZZ_TARGETS=OFF` and reuses the existing `SANITIZE` mechanism.
Documentation lives in `docs/dev/fuzzing.md`.

## Technical Context

**Language/Version**: C++17 (matches project baseline)

**Primary Dependencies**: OpenImageIO (libOpenImageIO), clang/LLVM (for `-fsanitize=fuzzer,address,undefined`), all optional format libraries (OpenEXR, libtiff, libjpeg-turbo, libpng, etc. — already present in `aswf/ci-oiio:2026.3`)

**Storage**: Seed corpora in `src/fuzz/corpora/<format>/` (checked in, binary image files). Evolved corpus in GHA cache keyed `fuzz-corpus-<format>-<sha>`.

**Testing**: CTest not used for fuzz runs; GHA fuzz workflow is self-contained. Fuzz findings produce regression tests committed to `testsuite/`.

**Target Platform**: Linux (GHA `ubuntu-latest` + `aswf/ci-oiio:2026.3` container). Local dev: macOS or Linux with clang ≥ 14.

**Project Type**: Infrastructure / build + CI tooling added to an existing C++ library.

**Performance Goals**: Tier 1 formats: ≥ 1,000 executions/sec sustained. Tier 2: any positive throughput. Each fuzz job runs until GHA per-job limit minus a 5-minute teardown margin.

**Constraints**: Must not affect Release or Debug builds (`OIIO_BUILD_FUZZ_TARGETS` defaults OFF). Must not require changes to production format plugin source. Harness ABI must satisfy OSS-Fuzz `LLVMFuzzerTestOneInput` convention.

**Scale/Scope**: One harness binary covering all 29 in-scope formats (32 plugins minus 3 excluded: `null`, `term`, `r3d`) discovered at runtime. `openvdb` and `ptex` included as Tier 2 when their libraries are present. Seed corpus: 1–5 small valid images per format sourced from `testsuite/`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Format-Agnostic API Integrity | ✅ Pass | Harnesses call `ImageInput::open()` via the public API; no internal helpers bypassed. |
| II. Safety and Input Robustness | ✅ Pass | This feature *is* the safety gate. ASan+UBSan enforced. Any finding is blocking per constitution. |
| III. Fuzz-First Development | ✅ Pass | Harnesses added alongside the infrastructure; pattern established for all future plugin additions. |
| IV. Minimal Footprint | ✅ Pass | `OIIO_BUILD_FUZZ_TARGETS=OFF` default; no production source changes required. |
| V. Reproducibility | ✅ Pass | Crash reproducers uploaded as GHA artifacts; regression test commitment required before fix merges. |

All gates pass. Proceeding.

## Project Structure

### Documentation (this feature)

```text
specs/001-image-fuzzing/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output (= docs/dev/fuzzing.md)
├── contracts/
│   └── harness-contract.md  # Fuzz harness interface contract
└── tasks.md             # Phase 2 output (/speckit-tasks command)
```

### Source Code (repository root)

```text
src/fuzz/
├── CMakeLists.txt           # OIIO_BUILD_FUZZ_TARGETS gate; builds fuzz_image target
├── fuzz_utils.h             # Shared: IOMemReader wrapper, OOM guard, OIIO init,
│                            #   format dispatch helpers, LLVMFuzzerInitialize
├── fuzz_image.cpp           # Single dynamic dispatch harness — one binary for all formats
└── corpora/
    ├── bmp/         (1–3 seed images per format)
    ├── cineon/
    ├── dds/
    ├── dicom/
    ├── dpx/
    ├── exr/
    ├── ffmpeg/
    ├── fits/
    ├── gif/
    ├── hdr/
    ├── heif/
    ├── ico/
    ├── iff/
    ├── jpeg/
    ├── jpeg2000/
    ├── jpegxl/
    ├── png/
    ├── pnm/
    ├── psd/
    ├── raw/
    ├── rla/
    ├── sgi/
    ├── softimage/
    ├── targa/
    ├── tiff/
    ├── webp/
    ├── zfile/
    ├── openvdb/
    └── ptex/

.github/workflows/fuzz.yml   # Nightly parallel matrix fuzz workflow

docs/dev/fuzzing.md          # Local fuzzing quickstart + crash reproduction guide
```

**Structure Decision**: Single `src/fuzz/` subtree with one `fuzz_image` binary that
discovers formats at runtime via `OIIO::get_string_attribute("extension_list")`. The
active format is selected via the `OIIO_FUZZ_FORMAT` environment variable (used by GHA
jobs) or by reading `basename(argv[0])` (used by OSS-Fuzz per-format symlinks). Seed
corpora are committed alongside the harness source. Evolved corpus lives only in GHA
cache (never committed).

**New format coverage guarantee**: A CI lint step (`./fuzz_image --list-formats`) diffs
the runtime format list against `src/fuzz/corpora/` directories and fails if any format
is missing a corpus directory. This replaces the manual "remember to add a harness"
burden with an automated check.

## Complexity Tracking

No constitution violations requiring justification.
