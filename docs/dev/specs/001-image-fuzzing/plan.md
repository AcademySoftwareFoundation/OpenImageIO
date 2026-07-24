# Implementation Plan: Image Format Fuzzing Infrastructure

**Branch**: `001-image-fuzzing` | **Date**: 2026-06-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-image-fuzzing/spec.md`

## Summary

Add a libFuzzer-based fuzzing infrastructure to OpenImageIO that exercises all supported
image format readers via the `ImageInput` API using in-memory `IOProxy` buffers for
maximum throughput. A new `src/fuzz/` directory holds a single dynamic dispatch harness
(`oiio_fuzz_image`) plus a shared helper and seed corpora. The harness queries
`OIIO::get_extension_map()` at startup to discover every compiled-in
format automatically — new format plugins are covered without any manual harness update.
A new `.github/workflows/fuzz.yml` runs nightly as a parallel matrix (one GHA job per
format), each job setting `OIIO_FUZZ_FORMAT=<format>` to target a single format and its
corpus. For OSS-Fuzz, per-format symlinks (`fuzz_jpeg -> oiio_fuzz_image`) let the binary
read its own name from `argv[0]` to determine the format — the dispatch logic is built
and ready, but the `ossfuzz/` project files themselves are not (deferred, P3). The CMake
build is gated by `OIIO_BUILD_FUZZ_TARGETS=OFF` and layers its own
`-fsanitize=address,undefined` directly on the fuzz binary rather than depending on the
global `SANITIZE` mechanism (so a fuzz build is fully sanitized even if `SANITIZE` isn't
set). Documentation lives in `docs/dev/fuzzing.md`.

**As-built addition beyond "no production source changes"**: while building the
harness's per-input read loop (chunked scanline/tile reads, subimage/MIP iteration,
first-failure bail-out), it became clear the same logic was generally useful outside
fuzzing — as a way to interactively exercise and debug an `ImageInput` the same way
the fuzzer does. It was moved into the core library as `OIIO::pvt::test_read_image()`
/ `test_read_all_images()` (`src/include/imageio_pvt.h`,
`src/libOpenImageIO/imageinput.cpp`) and exposed via a new `oiiotool --testread` flag,
so the harness and developers share one tested implementation instead of two. This is
a one-time, small addition to the library — not a per-format cost.

## Technical Context

**Language/Version**: C++17 (matches project baseline)

**Primary Dependencies**: OpenImageIO (libOpenImageIO), clang/LLVM (for `-fsanitize=fuzzer,address,undefined`), all optional format libraries (OpenEXR, libtiff, libjpeg-turbo, libpng, etc. — already present in the `aswf/ci-oiio` container; pinned to `:2027` as of landing, was `:2026.3` at spec time)

**Storage**: Seed corpora in `src/fuzz/corpora/<format>/` (checked in, binary image files). Evolved corpus in GHA cache keyed `fuzz-corpus-<format>-<sha>`.

**Testing**: CTest not used for fuzz runs; GHA fuzz workflow is self-contained. Fuzz findings produce regression tests committed to `testsuite/`.

**Target Platform**: Linux (GHA `ubuntu-latest` + `aswf/ci-oiio:2027` container, was `:2026.3` at spec time). Local dev: macOS or Linux with clang ≥ 14 (Apple's own clang is explicitly rejected — see Constraints).

**Project Type**: Infrastructure / build + CI tooling added to an existing C++ library.

**Performance Goals**: Tier 1 formats: ≥ 1,000 executions/sec sustained. Tier 2: any positive throughput. *(These are aspirational targets, not CI-enforced gates — no step checks measured throughput against them.)* As implemented, Tier 1 jobs run for 1 hour and Tier 2 for 30 minutes (not the originally-planned 5.5h/1h split — scaled down to fit CI budget after initial runs). *(No separate "teardown margin" is enforced: both budgets sit far under the GHA job's default 6-hour timeout, so the gap between `-max_total_time` and the job timeout is large by construction rather than a value anyone tuned.)*

**Constraints**: Must not affect Release or Debug builds (`OIIO_BUILD_FUZZ_TARGETS` defaults OFF). Must not require changes to production format plugin source. Harness ABI must satisfy OSS-Fuzz `LLVMFuzzerTestOneInput` convention.

**Scale/Scope**: One harness binary covering all 29 in-scope formats (32 plugins minus 3 excluded: `null`, `term`, `r3d`) discovered at runtime. `openvdb` and `ptex` included as Tier 2 when their libraries are present. Seed corpus: 1–5 small valid images per format sourced from `testsuite/`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Format-Agnostic API Integrity | ✅ Pass | Harnesses call `ImageInput::open()` via the public API; no internal helpers bypassed. |
| II. Safety and Input Robustness | ✅ Pass | This feature *is* the safety gate. ASan+UBSan enforced. Any finding is blocking per constitution. |
| III. Fuzz-First Development | ✅ Pass | Harnesses added alongside the infrastructure; pattern established for all future plugin additions. |
| IV. Minimal Footprint | ✅ Pass (scope revised) | `OIIO_BUILD_FUZZ_TARGETS=OFF` default keeps the build gate minimal. A small, shared read-loop helper (`OIIO::pvt::test_read_image`/`test_read_all_images`) was added to `libOpenImageIO` and exposed via `oiiotool --testread` — a deliberate, one-time addition once it was clear the logic was useful beyond fuzzing, not per-format churn. "No production source changes" as originally scoped no longer applies; see Summary. |
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
├── CMakeLists.txt           # OIIO_BUILD_FUZZ_TARGETS gate; builds oiio_fuzz_image target
├── fuzz_image.cpp           # Single dynamic dispatch harness — one binary for all formats;
│                            #   IOMemReader wrapper, OOM guard, OIIO init, format dispatch
│                            #   helpers, and LLVMFuzzerInitialize all live here (as-built:
│                            #   originally split out into fuzz_utils.h, later folded back
│                            #   in since it was fuzz_image.cpp's only include)
├── oiio_fuzz_image.options  # libFuzzer options (max_len, rss_limit_mb), installed
│                            #   alongside the binary; ClusterFuzz/OSS-Fuzz key it by
│                            #   binary basename
└── corpora/
    ├── bmp/         (1–3 seed images per format)
    ├── cineon/
    ├── dds/
    ├── dicom/
    ├── dpx/
    ├── openexr/     (named after the OIIO format-registry key, not "exr")
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

**Structure Decision**: Single `src/fuzz/` subtree with one `oiio_fuzz_image` binary that
discovers formats at runtime via `OIIO::get_string_attribute("extension_list")`. The
active format is selected via the `OIIO_FUZZ_FORMAT` environment variable (used by GHA
jobs) or by reading `basename(argv[0])` (used by OSS-Fuzz per-format symlinks). Seed
corpora are committed alongside the harness source. Evolved corpus lives only in GHA
cache (never committed).

**New format coverage guarantee**: A CI lint step (`./oiio_fuzz_image --list-formats`) diffs
the runtime format list against `src/fuzz/corpora/` directories and fails if any format
is missing a corpus directory. This replaces the manual "remember to add a harness"
burden with an automated check. This lint runs as the `fuzz-corpus-lint` job, via the
shared `build-steps.yml` reusable workflow. *(As-built history: it first landed as a job
in `.github/workflows/ci.yml`, then was relocated to `.github/workflows/fuzz.yml` to keep
all fuzzing-related CI in one workflow file.)*

**Fuzz step logic lives in a script, not inline YAML**: `build-steps.yml`'s fuzz-related
work (corpus lint, corpus seeding, running the fuzzer, writing the job summary) is a
single step, `id: fuzz`, that calls `src/build-scripts/ci-fuzztest.bash` — matching how
`Build`, `Testsuite`, and `Benchmarks` each delegate to a script rather than inlining
bash in the workflow file. Only the sub-steps that need a marketplace action
(`actions/checkout`, `actions/cache`, `actions/upload-artifact`) remain as separate YAML
steps.

**Note**: the `all_fuzz_targets` CMake alias mentioned in earlier drafts of this plan was
never wired up (it's present only as a comment in `src/fuzz/CMakeLists.txt`) — there is
only one target, `oiio_fuzz_image`, and nothing currently depends on the alias name.

## Complexity Tracking

No constitution violations requiring justification.
