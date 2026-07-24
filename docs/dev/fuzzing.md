<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Fuzzing OpenImageIO

OpenImageIO uses [libFuzzer](https://llvm.org/docs/LibFuzzer.html) to
exercise image format readers against malformed input. A single binary,
`oiio_fuzz_image`, covers all compiled-in formats by dispatching at runtime
based on the `OIIO_FUZZ_FORMAT` environment variable. Nightly CI fuzzes every
format in parallel; crash reproducers are uploaded as GitHub Actions
artifacts.


## Prerequisites

- **clang ≥ 14** with libFuzzer support (`-fsanitize=fuzzer`). GCC does not
  support libFuzzer and will be rejected by CMake with a clear error.
  - On **macOS**, Apple's clang (from Xcode / Command Line Tools) does *not*
    ship the libFuzzer runtime, so `-fsanitize=fuzzer` fails to link. Install
    upstream LLVM (`brew install llvm`) and point CMake at it:
    `-DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang
    -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++`. With AppleClang,
    CMake warns and skips the fuzz targets (the rest of OIIO still builds).
- **CMake ≥ 3.18**
- All optional format libraries you want fuzz coverage for (the same ones
  used in a normal OIIO build). The `aswf/ci-oiio:2027` container has all
  of them pre-installed.


## Building the fuzz target

```bash
cmake -B build -S . \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DOIIO_BUILD_FUZZ_TARGETS=ON \
    -DSANITIZE=address,undefined

cmake --build build --target oiio_fuzz_image -j$(nproc)
```

`OIIO_BUILD_FUZZ_TARGETS=ON` is the only new flag. `SANITIZE=address,undefined`
instruments the full OpenImageIO library for maximum bug detection — omitting
it produces a valid binary but with much weaker coverage.

By default the local fuzzing engine is `-fsanitize=fuzzer`. On OSS-Fuzz the
`LIB_FUZZING_ENGINE` environment variable is set to the engine's `.a` path and
is picked up automatically by `src/fuzz/CMakeLists.txt`.


## Listing supported formats

```bash
build/src/fuzz/oiio_fuzz_image --list-formats
```

Prints one format name per line for every format compiled into this build.
This list drives the CI lint check: the build fails if any format returned
here lacks a directory under `src/fuzz/corpora/`.


## Running the fuzzer locally

```bash
# Fuzz JPEG for 60 seconds using the seed corpus as a starting point:
OIIO_FUZZ_FORMAT=jpeg \
    build/src/fuzz/oiio_fuzz_image \
    src/fuzz/corpora/jpeg/ \
    -max_total_time=60

# Useful flags:
#   -timeout=60       kill any single input that takes longer than 60 s
#   -jobs=4           run 4 worker processes in parallel
#   -runs=0           process seeds only, no mutation (useful to verify seeds)
#   -max_len=65536    cap input size to 64 KB (speeds up throughput)
```

The fuzzer writes new interesting inputs into the corpus directory and any
crash reproducers as `crash_<hash>` files in the current directory.


## Reproducing a CI crash

1. Download the crash artifact from the failed GitHub Actions run
   (named `fuzz-crashes-<format>-<run_id>`).
2. Extract the `crash_<format>_<hash>` file.
3. Run:

```bash
OIIO_FUZZ_FORMAT=<format> \
    build/src/fuzz/oiio_fuzz_image \
    crash_<format>_<hash>
```

The AddressSanitizer or UBSan report appears on stderr. The key fields are
the error type (e.g. `heap-buffer-overflow`), the stack trace at the point of
the violation, and the allocation site.


## Minimizing a crash

Reduce a crash reproducer to the smallest input that still triggers it:

```bash
OIIO_FUZZ_FORMAT=<format> \
    build/src/fuzz/oiio_fuzz_image \
    -minimize_crash=1 \
    -exact_artifact_path=min_crash \
    crash_<format>_<hash>
```

Commit `min_crash` to `testsuite/fuzz-<format>/` as a regression test before
merging the fix.


## Adding seeds for a new format

Seeds are **not** hand-curated into `src/fuzz/corpora/` per format. At fuzz
time `ci-fuzztest.bash` gathers every example we have for the format from the
directories listed in `FORMAT_SOURCES` (`src/fuzz/populate_corpora.py`) — the
format's own `testsuite/` fixtures (valid images *and* malformed/regression
files) plus the companion image repos — capped only by a per-file size limit
(`MAX_BYTES`, 5 MB), with **no cap on the number of seeds**. It then runs
`oiio_fuzz_image -merge=1` to distill that pile down to the coverage-increasing
subset before the timed run. Feeding in the full, size-diverse set (not just a
few tiny files) is deliberate: mutating structurally rich real images reaches
the deep decode paths where subtle bugs hide.

When a new format plugin is added to OIIO:

1. The format automatically appears in `--list-formats` (no harness changes
   needed).
2. The CI lint job (`fuzz-corpus-lint` in `.github/workflows/fuzz.yml`) will
   **fail** until `src/fuzz/corpora/<format>/` exists. This is intentional —
   it enforces that every compiled-in format has at least a corpus directory
   (a `.gitkeep` is enough).
3. Wire the format's seed sources into `FORMAT_SOURCES` in
   `src/fuzz/populate_corpora.py`: its `testsuite/<format>/src` (and any
   dedicated malformed-fixture dirs, e.g. `testsuite/<format>-corrupt/src`)
   and the relevant companion-repo path. Regression fixtures added to
   `testsuite/` are then picked up automatically — do **not** also copy them
   into `src/fuzz/corpora/`.
4. Only when a format has **no** testsuite fixture or companion source (e.g.
   `hdr`, `iff`, `sgi`, `jpegxl`), commit a small synthetic seed directly:

```bash
oiiotool --create 64x64 3 --ch R,G,B -o src/fuzz/corpora/<format>/seed.<ext>
```

5. Verify the gathered seeds parse cleanly (`-runs=0` processes seeds without
   mutating):

```bash
python3 src/fuzz/populate_corpora.py --format <format> --dest /tmp/seeds
OIIO_FUZZ_FORMAT=<format> build/src/fuzz/oiio_fuzz_image /tmp/seeds/<format>/ -runs=0
```

`populate_corpora.py` auto-detects companion repos at `../oiio-images` (sibling
of the repo root, as checked out by the fuzz CI job) or at
`build/testsuite/oiio-images` (fetched by `oiio_setup_test_data` during a
regular test run). The committed `src/fuzz/corpora/<format>/` dir holds only
synthetic seeds that have no testsuite fixture; everything else is sourced from
`testsuite/` or companion repos at fuzz time.


## How format selection works

`oiio_fuzz_image` resolves the active format in priority order:

1. `OIIO_FUZZ_FORMAT` environment variable — used by CI matrix jobs.
2. `basename(argv[0])` stripped of `fuzz_` prefix — used by OSS-Fuzz
   per-format symlinks (`fuzz_jpeg → oiio_fuzz_image`).
3. `--format=<name>` command-line argument.
4. None set → the binary prints available formats and exits with an error.


## Memory limits and false-positive OOMs

libFuzzer enforces an `rss_limit_mb` (set in
`src/fuzz/oiio_fuzz_image.options`, currently 4096 MB) and reports a crash if
the process exceeds it. OIIO's own decode-bomb guards default to much larger
values (`limits:imagesize_MB` = 32768, `limits:resolution` = 1048576), so a
corrupt header claiming a multi-GB image would trip libFuzzer's OOM kill before
OIIO's guard rejects it — a false positive.

To keep the two budgets commensurate, `OIIO_FUZZ_INIT` (in
`src/fuzz/fuzz_image.cpp`) lowers OIIO's limits well under the RSS budget:
`limits:imagesize_MB` to 2048 (half the budget, leaving headroom for decode
scratch and process overhead) and `limits:resolution` to 65536. With these in
place OIIO rejects oversized headers through its normal error path instead of
allocating into an OOM kill. If you change `rss_limit_mb`, update these limits
to match.


## CI overview

- **Nightly fuzz** (`.github/workflows/fuzz.yml`): 29-format parallel matrix,
  Tier 1 formats run for 1 hour, Tier 2 for 30 minutes. Evolved corpus is
  cached per format per branch (see `data-model.md` EvolvedCorpus for the
  exact cache-key scheme).
- **Corpus lint** (`fuzz-corpus-lint` job in `.github/workflows/fuzz.yml`):
  builds `oiio_fuzz_image` on every PR, runs `--list-formats`, and fails if
  any compiled-in format lacks a `src/fuzz/corpora/<format>/` directory.


## OSS-Fuzz

Not yet onboarded. No `ossfuzz/` directory exists in the repo yet — this is
deferred future work. The harness already supports what OSS-Fuzz would need
(`$LIB_FUZZING_ENGINE` linkage, `argv[0]`-based per-format dispatch for
`fuzz_<format>` symlinks, `--list-formats`), so onboarding remains a small,
additive step: a `build.sh` would loop over `--list-formats` output to create
per-format symlinks and seed corpus zips automatically, so new formats would
be covered without any `build.sh` changes. See
`specs/001-image-fuzzing/research.md §8` for the intended design.
