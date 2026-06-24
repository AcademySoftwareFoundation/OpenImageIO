# Tasks: Image Format Fuzzing Infrastructure

**Input**: Design documents from `specs/001-image-fuzzing/`

**Prerequisites**: spec.md ✓, plan.md ✓, research.md ✓, data-model.md ✓, contracts/harness-contract.md ✓

**Tests**: Not included (no TDD requested in spec).

**Organization**: Grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on each other)
- **[Story]**: Which user story this task belongs to (US1–US5)

---

## Phase 1: Setup (Directory Structure)

**Purpose**: Create the `src/fuzz/` subtree skeleton so all subsequent phases have a home.

- [x] T001 Create `src/fuzz/` directory and empty placeholder `CMakeLists.txt`
- [x] T002 [P] Create all 29 corpus directories `src/fuzz/corpora/<format>/` (one per format: bmp, cineon, dds, dicom, dpx, exr, ffmpeg, fits, gif, hdr, heif, ico, iff, jpeg, jpeg2000, jpegxl, openvdb, png, pnm, psd, ptex, raw, rla, sgi, softimage, targa, tiff, webp, zfile) with a `.gitkeep` in each so git tracks them
- [x] T003 [P] Add `src/fuzz` subdirectory to the root `CMakeLists.txt` inside a `if(OIIO_BUILD_FUZZ_TARGETS)` guard so the option gates compilation

---

## Phase 2: Foundational (Shared Fuzz Infrastructure)

**Purpose**: `fuzz_utils.h` and `src/fuzz/CMakeLists.txt` — everything Phase 3 depends on.

**⚠️ CRITICAL**: Phase 3 blocks on this phase.

- [x] T004 Implement `src/fuzz/fuzz_utils.h` with:
  - `OIIO_FUZZ_INIT` macro — calls `OIIO::attribute("imageinput:print_errors", 0)` and
    `OIIO::attribute("exr:threads", 1)` once via static flag
  - `oiio_fuzz_read(data, size, fake_filename)` — `IOMemReader` wrap → `ImageInput::open()`
    → read if `image_pixels() < 64 * 1024 * 1024` → close
  - `oiio_fuzz_read_multi(data, size, fake_filename)` — same but loops subimages via
    `seek_subimage()`
  - `oiio_fuzz_read_dispatch(data, size, plugin_name)` — `ImageInput::create(plugin_name)`
    → open via `IOMemReader` → read → close; returns immediately if `create()` returns null
  - `oiio_format_primary_ext(string_view format)` → `std::string` — queries
    `OIIO::get_string_attribute("extension_list")`, finds the format entry, returns the
    first listed extension
  - `oiio_format_is_multi(string_view format)` → `bool` — true for exr, tiff (checks
    `inp->supports("multiimage")` after a dummy open, or hardcoded set)
  - `oiio_format_is_dispatch(string_view format)` → `bool` — true for raw, ffmpeg
  - Standard copyright/SPDX header and `#pragma once`
- [x] T005 Implement `src/fuzz/CMakeLists.txt` (replaces placeholder from T001):
  - Early-exit guard: `if(NOT OIIO_BUILD_FUZZ_TARGETS) return() endif()` (option declared
    in root CMakeLists)
  - Clang detection: `if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang") message(FATAL_ERROR
    "OIIO_BUILD_FUZZ_TARGETS requires clang") endif()`
  - Resolve fuzzing engine: `if(DEFINED ENV{LIB_FUZZING_ENGINE}) set(OIIO_FUZZING_ENGINE
    "$ENV{LIB_FUZZING_ENGINE}") else() set(OIIO_FUZZING_ENGINE "-fsanitize=fuzzer") endif()`
  - Single target: `add_executable(fuzz_image fuzz_image.cpp)`; `target_link_libraries(fuzz_image
    PRIVATE OpenImageIO)`; `target_compile_options(-fsanitize=address,undefined
    -fno-omit-frame-pointer)`; `target_link_options(-fsanitize=address,undefined
    ${OIIO_FUZZING_ENGINE})`
  - Alias `fuzz_image` as the `all_fuzz_targets` custom target (for OSS-Fuzz `build.sh`)

**Checkpoint**: `cmake -B build -DOIIO_BUILD_FUZZ_TARGETS=ON -DSANITIZE=address,undefined`
configures cleanly; `fuzz_image` target defined but not yet buildable (source not yet written).

---

## Phase 3: User Story 2 — Dynamic Dispatch Fuzz Harness (Priority: P1)

**Goal**: One `fuzz_image` binary covering all formats dynamically, per the contract in
`contracts/harness-contract.md`. New formats are covered automatically with no harness
changes; the CI lint step (`./fuzz_image --list-formats`) enforces corpus coverage.

**Independent Test**: Build `fuzz_image`. Run `OIIO_FUZZ_FORMAT=jpeg ./fuzz_image
src/fuzz/corpora/jpeg/ -max_total_time=30` and `OIIO_FUZZ_FORMAT=png ./fuzz_image
src/fuzz/corpora/png/ -max_total_time=30`. Both exit 0. Run `./fuzz_image --list-formats`
and confirm at least 20 format names are printed.

- [x] T006 [US2] Implement `src/fuzz/fuzz_image.cpp`:
  - `LLVMFuzzerInitialize(int* argc, char*** argv)`:
    - Call `OIIO_FUZZ_INIT`
    - Scan `(*argv)[0]` (binary name) for `fuzz_<format>` pattern; if matched, use `<format>`
    - Check `OIIO_FUZZ_FORMAT` env var (overrides argv[0])
    - Scan remaining args for `--format=<name>` (strip it from argv so libFuzzer doesn't
      see it)
    - If `--list-formats` found: print all formats from `extension_list` (excluding `null`,
      `term`) one per line, then `exit(0)`
    - If no format resolved: print error with list of available formats, then `exit(1)`
    - Validate resolved format appears in `extension_list`; `exit(1)` if not found
    - Store resolved format name in a `static std::string g_format`
    - Store resolved primary extension in `static std::string g_ext` via
      `oiio_format_primary_ext(g_format)`
    - Return 0
  - `LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)`:
    - If `oiio_format_is_dispatch(g_format)`: call `oiio_fuzz_read_dispatch(data, size,
      g_format.c_str())`
    - Else if `oiio_format_is_multi(g_format)`: call `oiio_fuzz_read_multi(data, size,
      fake_filename)` where `fake_filename = "input." + g_ext`
    - Else: call `oiio_fuzz_read(data, size, fake_filename)`
    - Return 0

- [x] T007 [US2] Add CI lint step to `.github/workflows/ci.yml` (the existing non-fuzz CI,
  not the fuzz workflow): a step that builds `fuzz_image` in the sanitizer job (or a
  dedicated short job), runs `./fuzz_image --list-formats`, and diffs the output against
  the set of `src/fuzz/corpora/` directory names; fails with a clear message if any format
  is missing a corpus directory

**Checkpoint**: `cmake --build build --target fuzz_image` succeeds. `OIIO_FUZZ_FORMAT=jpeg
./build/src/fuzz/fuzz_image src/fuzz/corpora/jpeg/ -max_total_time=30` runs for 30 seconds
and exits 0.

---

## Phase 4: User Story 1 — Automated Nightly Fuzz CI Run (Priority: P1)

**Goal**: `.github/workflows/fuzz.yml` runs nightly, all format jobs in parallel via matrix,
each targeting one format via `OIIO_FUZZ_FORMAT`, uploading crash artifacts, persisting
evolved corpus in GHA cache.

**Independent Test**: Trigger `workflow_dispatch` on the branch. The workflow builds
`fuzz_image` once, then matrix jobs each run `OIIO_FUZZ_FORMAT=<format> ./fuzz_image ...`
without crashing. Introducing a known-bad input causes the matching job to upload a
crash artifact.

- [x] T035 [US1] Create `.github/workflows/fuzz.yml` with top-level structure:
  - `name: Fuzz`
  - `on:` block with `schedule: [{cron: "0 2 * * *"}]` and `workflow_dispatch: {inputs:
    {format: {description: "Format name to fuzz (leave empty for all)", required: false,
    default: ""}}}`
  - `jobs: fuzz:` using container `aswf/ci-oiio:2026.3`
- [x] T036 [US1] Add matrix strategy to `fuzz` job in `.github/workflows/fuzz.yml`:
  - `strategy: {fail-fast: false, matrix: {include: [...]}}` with all format entries per
    `data-model.md` and `research.md §4` (12 Tier 1 at `max_total_time: 19800`, 17 Tier 2
    at `max_total_time: 3600`)
  - Each matrix entry has `{format: exr, tier: 1, max_total_time: 19800}` etc.
  - Job-level condition: `if: github.event.inputs.format == '' || github.event.inputs.format
    == matrix.format`
- [x] T037 [US1] Add build step to `fuzz` job in `.github/workflows/fuzz.yml`:
  - Checkout step
  - CMake configure: `cmake -B build -DOIIO_BUILD_FUZZ_TARGETS=ON -DSANITIZE=address,undefined
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
  - CMake build: `cmake --build build --target fuzz_image -j$(nproc)` (one binary serves all
    formats; each matrix job just sets the env var)
- [x] T038 [US1] Add corpus cache restore step to `.github/workflows/fuzz.yml` before the fuzz
  run step:
  - `actions/cache@v4` with `key: fuzz-corpus-${{ matrix.format }}-${{ github.ref_name }}`,
    `restore-keys: ["fuzz-corpus-${{ matrix.format }}-"]`, `path: corpus/${{ matrix.format }}`
  - After restore: `mkdir -p corpus/${{ matrix.format }}` then `cp -rn
    src/fuzz/corpora/${{ matrix.format }}/* corpus/${{ matrix.format }}/ 2>/dev/null || true`
- [x] T039 [US1] Add fuzz run step to `.github/workflows/fuzz.yml`:
  - `env: {OIIO_FUZZ_FORMAT: "${{ matrix.format }}"}`
  - `./build/src/fuzz/fuzz_image corpus/${{ matrix.format }} -max_total_time=${{ matrix.max_total_time }}
    -timeout=60 -artifact_prefix=crash_${{ matrix.format }}_ -jobs=$(nproc)`
  - `continue-on-error: false` (non-zero exit from sanitizer finding fails the step)
- [x] T040 [US1] Add corpus save and crash artifact upload steps (`if: always()` on both):
  - Corpus save: `actions/cache@v4` save with same key as restore
  - Crash upload: `actions/upload-artifact@v4` with `name: fuzz-crashes-${{ matrix.format }}-${{
    github.run_id }}`, `path: crash_${{ matrix.format }}_*`, `if-no-files-found: ignore`,
    `retention-days: 30`
  - Job summary: `echo "Format: ${{ matrix.format }} — $(ls crash_${{ matrix.format }}_* 2>/dev/null
    | wc -l) crash(es)" >> $GITHUB_STEP_SUMMARY`

**Checkpoint**: Trigger `workflow_dispatch` manually. All matrix jobs start, build `fuzz_image`,
and run with `OIIO_FUZZ_FORMAT` set. Tier-2 short-duration jobs complete first.

---

## Phase 5: User Story 3 — Seed Corpus Per Format (Priority: P2)

**Goal**: Every format's corpus directory has 1–5 valid seed files sourced from existing test repositories; formats without existing files get synthetic seeds.

**Independent Test**: For TIFF, EXR, and PNG, verify `src/fuzz/corpora/<format>/` contains at least one valid image (parseable by `oiiotool --info`). Verify `OIIO_FUZZ_FORMAT=tiff ./fuzz_image src/fuzz/corpora/tiff/ -runs=0` etc. exit 0 for all three.

- [x] T041 [US3] Create `src/fuzz/populate_corpora.py` — idempotent Python script that:
  - Defines a source map: each format → list of glob patterns relative to known repo roots (`testsuite/`, `../oiio-images/`, `../j2kp4files_v1_5/`, `../fits-images/`, `../dicom-images-pvt/`, `testsuite/ffmpeg/ref/`) per the table in `research.md §6`
  - Copies up to 5 files per format (preferring smaller files, max 100 KB each) into `src/fuzz/corpora/<format>/`
  - Includes existing `crash-*` files from testsuite dirs as seeds (for bmp, dds, ico, psd, rla, tga, tiff) per `research.md §6` note
  - Prints a report of what was copied and what was missing
  - Accepts `--format <name>` arg to update only one format
- [x] T042 [US3] Run `python src/fuzz/populate_corpora.py` against a local checkout that has the companion repos (`../oiio-images`, etc.) present; commit the resulting seed files to `src/fuzz/corpora/`
- [x] T043 [P] [US3] Generate synthetic seeds for the ~4 formats with no source files (hdr, iff, jpegxl, zfile) using `oiiotool --create 64x64 3 --ch R,G,B -o <format>` or format-specific tools; add to `src/fuzz/corpora/<format>/`
- [ ] T044 [US3] Verify all seed corpora: for each format, run `OIIO_FUZZ_FORMAT=<format>
  ./build/src/fuzz/fuzz_image src/fuzz/corpora/<format>/ -runs=0` (process seeds, no
  mutation); all must exit 0

**Checkpoint**: 29 corpus directories all non-empty. `OIIO_FUZZ_FORMAT=jpeg
./build/src/fuzz/fuzz_image src/fuzz/corpora/jpeg/ -runs=0` exits 0 in under 5 seconds.

---

## Phase 6: User Story 4 — Local Developer Fuzzing Workflow (Priority: P2)

**Goal**: `docs/dev/fuzzing.md` lets a developer with clang build, run, and reproduce a crash within 15 minutes of reading it.

**Independent Test**: A developer with no prior context follows `docs/dev/fuzzing.md` step by step on macOS or Linux with clang ≥ 14; they build `fuzz_image`, run it for 60 seconds targeting JPEG, then reproduce a known crash input.

- [x] T045 [US4] Create `docs/dev/fuzzing.md` with:
  - Prerequisites: clang ≥ 14, CMake ≥ 3.15; note gcc not supported
  - Build: cmake configure with `OIIO_BUILD_FUZZ_TARGETS=ON`, `SANITIZE=address,undefined`,
    `CMAKE_C_COMPILER=clang`, `CMAKE_CXX_COMPILER=clang++`; `cmake --build build --target
    fuzz_image`; note `LIB_FUZZING_ENGINE=-fsanitize=fuzzer` default for local use
  - Listing formats: `./build/src/fuzz/fuzz_image --list-formats`
  - Running a format: `OIIO_FUZZ_FORMAT=jpeg ./build/src/fuzz/fuzz_image
    src/fuzz/corpora/jpeg/ -max_total_time=60`; explain `-timeout`, `-jobs`, `-runs=0`
  - Reproducing a CI crash: download artifact, run `OIIO_FUZZ_FORMAT=<format>
    ./build/src/fuzz/fuzz_image <crash_file>`; explain ASan output
  - Adding corpus seeds for a new format: create `src/fuzz/corpora/<format>/`, add seed
    files (≤100 KB each); the lint check enforces this automatically
  - What happens when a new format is added to OIIO: it appears in `--list-formats`
    automatically; the lint step fails until a corpus dir is created (this is intentional)
  - Minimizing a crash: `OIIO_FUZZ_FORMAT=<format> ./fuzz_image -minimize_crash=1
    -exact_artifact_path=min_crash crash_file`
  - Link to `specs/001-image-fuzzing/quickstart.md` for more detail

**Checkpoint**: The document exists, all commands are copy-pasteable, and a cold reader can follow it to a running fuzz target.

---

## Phase 7: User Story 5 — OSS-Fuzz Onboarding Readiness (Priority: P3)

**Goal**: Draft OSS-Fuzz project files that will allow `infra/helper.py build_fuzzers openimageio` to succeed with no harness source changes.

**Independent Test**: In a local Docker environment with OSS-Fuzz cloned, run `python infra/helper.py build_fuzzers openimageio` using the draft files. It should link all fuzz targets.

- [ ] T046 [P] [US5] Create `ossfuzz/project.yaml`:
  - `homepage`, `language: c++`, `primary_contact`, `auto_ccs`
  - `fuzzing_engines: [libfuzzer, afl, honggfuzz, centipede]`
  - `sanitizers: [address, undefined, memory]`
- [ ] T047 [P] [US5] Create `ossfuzz/Dockerfile`:
  - `FROM gcr.io/oss-fuzz-base/base-builder`
  - `apt-get` installs for all format libraries (libopenexr-dev, libtiff-dev, libjpeg-turbo8-dev, libpng-dev, etc.)
  - `COPY . $SRC/openimageio`
  - `WORKDIR $SRC/openimageio`
- [ ] T048 [US5] Create `ossfuzz/build.sh` per `research.md §8` and `contracts/harness-contract.md §OSS-Fuzz`:
  - cmake configure using `$CC`, `$CXX`, `$SRC`, `$WORK`; `-DOIIO_BUILD_FUZZ_TARGETS=ON`,
    `-DSANITIZE=address,undefined`
  - `cmake --build $WORK/build --target fuzz_image -j$(nproc)`
  - `cp $WORK/build/src/fuzz/fuzz_image $OUT/`
  - Symlink loop and corpus zip using `--list-formats` output:
    ```bash
    for fmt in $($OUT/fuzz_image --list-formats); do
        ln -sf fuzz_image $OUT/fuzz_${fmt}
        zip -j $OUT/fuzz_${fmt}_seed_corpus.zip $SRC/openimageio/src/fuzz/corpora/${fmt}/* 2>/dev/null || true
    done
    ```
  - This loop is self-updating: adding a new format to OIIO automatically creates its
    OSS-Fuzz target and corpus zip with no `build.sh` changes required
- [ ] T049 [US5] Verify draft OSS-Fuzz files pass offline by running `python infra/helper.py
  build_image openimageio && python infra/helper.py build_fuzzers openimageio` in a local
  OSS-Fuzz clone; fix any build script issues

**Checkpoint**: `infra/helper.py build_fuzzers openimageio` exits 0 and `$OUT/` contains
`fuzz_image` plus per-format symlinks `fuzz_jpeg`, `fuzz_exr`, etc.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T050 [P] Run `make clang-format` and verify `src/fuzz/fuzz_image.cpp` and
  `src/fuzz/fuzz_utils.h` pass the `.clang-format` rules enforced by CI
- [ ] T051 [P] Confirm copyright + SPDX header present in both `src/fuzz/*.cpp` and
  `src/fuzz/*.h` files
- [ ] T052 Add `src/fuzz/` mention to `CLAUDE.md` repo map and to `docs/dev/Architecture.md`
- [ ] T053 End-to-end smoke test: build `fuzz_image`, then for each Tier 2 format run
  `OIIO_FUZZ_FORMAT=<fmt> ./build/src/fuzz/fuzz_image src/fuzz/corpora/<fmt>/ -runs=0`;
  all must exit 0

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Requires Phase 1 complete — blocks Phases 3 and 4
- **US2 Harness (Phase 3)**: Requires Phase 2 (`fuzz_utils.h` and `CMakeLists.txt` done)
- **US1 GHA Workflow (Phase 4)**: Requires Phase 3 (`fuzz_image` must exist and build)
- **US3 Corpus (Phase 5)**: Requires Phase 3 (needs `fuzz_image` to verify seeds with `-runs=0`)
- **US4 Docs (Phase 6)**: Requires Phase 3 and 4 to be functionally complete
- **US5 OSS-Fuzz (Phase 7)**: Requires Phase 3 (`fuzz_image` and `--list-formats` must work)
- **Polish (Phase 8)**: Requires all implementation phases

### User Story Dependencies

- **US2 (P1)**: Unblocked after Phase 2 — no dependency on other stories
- **US1 (P1)**: Depends on US2 (`fuzz_image` binary must exist)
- **US3 (P2)**: Depends on US2 (needs compiled `fuzz_image` for `-runs=0` verification)
- **US4 (P2)**: Depends on US1 and US2 (documents the working system)
- **US5 (P3)**: Depends on US2 (`--list-formats` drives the symlink loop in `build.sh`)

### Within Each Phase

- T004 and T005 in Phase 2 can be written in parallel (different files), but T005 build
  only succeeds once T006 (fuzz_image.cpp) exists
- T006 (Phase 3) and T007 (lint CI step) can be written in parallel; T007 only runs
  usefully once T006 compiles

### Parallel Opportunities

```
Phase 1: T001 → T002, T003 in parallel (already done)
Phase 2: T004, T005 in parallel (different files)
Phase 3: T006 → T007 (T007 can be drafted while T006 compiles)
Phase 4: T035→T036→T037→T038→T039→T040 (sequential — one workflow file)
Phase 5: T041→T042; T043 in parallel with T042 → T044
Phase 7: T046, T047 in parallel → T048 → T049
```

---

## Implementation Strategy

### MVP (User Stories 2 + 1 — delivers nightly CI value)

1. Phase 1: Setup (done ✓)
2. Phase 2: Foundational (`fuzz_utils.h` + `CMakeLists.txt`)
3. Phase 3: US2 — single `fuzz_image` binary with dynamic dispatch
4. Phase 4: US1 — GHA workflow with `OIIO_FUZZ_FORMAT` per matrix job
5. **STOP and VALIDATE**: Trigger `workflow_dispatch`; all matrix jobs run; no infrastructure failures
6. Merge — nightly fuzzing is live

### Incremental Delivery

1. Phase 1 + 2 → Build system works; `fuzz_image` target defined
2. Phase 3 (US2) → `fuzz_image` compiles, any format fuzzable locally → Core value
3. Phase 4 (US1) → Nightly CI live → **MVP delivered**
4. Phase 5 (US3) → Seeds populated → Fuzzer efficiency dramatically improved
5. Phase 6 (US4) → Docs → Developers can self-serve locally
6. Phase 7 (US5) → OSS-Fuzz ready → Scalable cloud fuzzing, symlinks auto-generated

---

## Notes

- [P] tasks operate on different files with no shared state — safe to run concurrently
- The harness is ~80 lines total (`fuzz_utils.h` + `fuzz_image.cpp`); most complexity is in dispatch logic, not format-specific code
- New formats added to OIIO automatically appear in `--list-formats`; lint step (T007) fails until `src/fuzz/corpora/<format>/` is created — this is the enforcement mechanism
- After finding and fixing a fuzz crash, add the reproducer to `testsuite/fuzz-<format>/` before merging the fix
- Corpus `.gitkeep` files may be removed once real seeds are committed in Phase 5
- The `all_fuzz_targets` CMake alias (T005) + `--list-formats` loop in `build.sh` (T048) together mean OSS-Fuzz also auto-covers new formats
