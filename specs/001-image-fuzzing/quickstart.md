# Fuzzing Quickstart (Developer Guide)

**Audience**: OpenImageIO contributors who want to run fuzzing locally, add a new
harness, or reproduce a crash found by CI.

This document will be published at `docs/dev/fuzzing.md` once the feature lands.

---

## Prerequisites

- clang ≥ 14 with libFuzzer support (`clang -fsanitize=fuzzer -x c /dev/null -o /dev/null` should succeed)
- All OIIO format dependencies installed (see `src/build-scripts/gh-installdeps.bash` for a reference list, or use the `aswf/ci-oiio:2026.3` Docker container)
- CMake ≥ 3.15, Ninja (optional but faster)

On macOS, install clang via Homebrew: `brew install llvm` and use
`/opt/homebrew/opt/llvm/bin/clang`.

---

## Build the Fuzz Targets

```bash
cmake -B build-fuzz -S . \
    -DOIIO_BUILD_FUZZ_TARGETS=ON \
    -DSANITIZE=address,undefined \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DUSE_PYTHON=OFF

cmake --build build-fuzz --target all_fuzz_targets -j$(nproc)
```

Binaries land at `build-fuzz/src/fuzz/fuzz_<format>`.

---

## Run a Fuzz Target

```bash
# Fuzz jpeg for 60 seconds using the seed corpus:
./build-fuzz/src/fuzz/fuzz_jpeg src/fuzz/corpora/jpeg \
    -max_total_time=60 \
    -timeout=30 \
    -artifact_prefix=./crashes/

# Fuzz openexr for 10 minutes, using 4 parallel workers:
./build-fuzz/src/fuzz/fuzz_exr src/fuzz/corpora/exr \
    -max_total_time=600 \
    -timeout=60 \
    -jobs=4
```

libFuzzer prints a status line every few seconds showing executions/sec and corpus size.
Press Ctrl+C to stop early — the corpus directory is updated as it runs.

---

## Reproduce a Crash

CI uploads crashing inputs as GHA artifacts named `fuzz-crashes-<format>-<run_id>`.

1. Download and unzip the artifact.
2. Run the fuzz target with the crash file as a positional argument:

```bash
./build-fuzz/src/fuzz/fuzz_tiff crash_tiff_a3f2b1c4
```

libFuzzer replays the single input and exits. ASan prints the stack trace and crash
report to stderr.

To get a cleaner stack trace without fuzzer overhead:
```bash
./build-fuzz/src/fuzz/fuzz_tiff crash_tiff_a3f2b1c4 -runs=1
```

---

## Populate the Seed Corpus

Seeds are committed in `src/fuzz/corpora/<format>/`. If you need to repopulate them
(e.g., after adding a new format or updating the source repos), run:

```bash
python src/fuzz/populate_corpora.py
```

This script pulls from the test image repos that CI already checks out alongside the
source tree (`../oiio-images`, `../fits-images`, `../j2kp4files_v1_5`,
`../dicom-images-pvt`) plus `testsuite/` (including `testsuite/ffmpeg/ref/` for video
seeds). For the handful of formats not covered by those repos (hdr, iff, jpegxl,
zfile), it generates minimal synthetic seeds using `oiiotool`. Commit any changed
corpus files alongside your PR.

## Add a Harness for a New Format

1. Create `src/fuzz/fuzz_<format>.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Copyright Contributors to the OpenImageIO project.
#include "fuzz_utils.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OIIO_FUZZ_INIT;
    oiio_fuzz_read(data, size, "input.<ext>");
    return 0;
}
```

Replace `<ext>` with one of the format's registered extensions (e.g., `tga` for Targa).
For multi-subimage formats (EXR, TIFF), use `oiio_fuzz_read_multi` instead.

2. Register the target in `src/fuzz/CMakeLists.txt`:

```cmake
# For formats with no optional dependency:
add_fuzz_target(myformat)

# For formats that require an optional library:
if (MYLIB_FOUND OR TARGET MyLib::MyLib)
    add_fuzz_target(myformat)
endif()
```

3. Add seeds to `src/fuzz/corpora/<format>/`. At minimum, include one small valid file.

4. Add the format to the GHA matrix in `.github/workflows/fuzz.yml` with its tier and
   `max_total_time`.

5. Run the harness locally for 30 seconds to verify it compiles and runs cleanly.

---

## Commit a Reproducer as a Regression Test

After triaging a crash and writing a fix:

1. Copy the minimal reproducer to `testsuite/fuzz-<format>/src/<sha_prefix>.<ext>`.
2. Add a test entry in `testsuite/fuzz-<format>/run.py` that runs `iinfo` or a small
   C++ test against the file and expects no crash (exit 0).
3. Include the reproducer in the same PR as the fix.

---

## OSS-Fuzz Local Simulation (optional)

To test OSS-Fuzz compatibility without submitting to OSS-Fuzz:

```bash
# Clone OSS-Fuzz
git clone https://github.com/google/oss-fuzz /tmp/oss-fuzz

# Build fuzzers in OSS-Fuzz's Docker environment
python /tmp/oss-fuzz/infra/helper.py build_fuzzers openimageio --source_path $(pwd)

# Run a specific fuzzer
python /tmp/oss-fuzz/infra/helper.py run_fuzzer openimageio fuzz_jpeg
```

This requires Docker. The OSS-Fuzz project files (`projects/openimageio/`) are part of
the P3 user story and will be added in a follow-up.

---

## Troubleshooting

**"clang: error: unsupported option '-fsanitize=fuzzer'"**  
Your clang is too old or is Apple clang (which does not include libFuzzer). Install
LLVM clang ≥ 14: `brew install llvm` on macOS, or `apt install clang` on Ubuntu.

**"ASAN: DEADLYSIGNAL — stack overflow"**  
Increase the stack size limit: `ulimit -s unlimited` before running the fuzz target.

**Fuzz target runs at < 100 exec/sec**  
The target may be doing slow I/O or hitting a complex parser. This is normal for
formats like EXR. Check `-print_final_stats=1` output for clues.

**"No module named openimageio" during cmake**  
Set `-DUSE_PYTHON=OFF` for fuzz builds — Python bindings are not needed.
