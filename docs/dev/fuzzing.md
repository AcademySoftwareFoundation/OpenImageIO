<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Fuzzing OpenImageIO

OpenImageIO uses [libFuzzer](https://llvm.org/docs/LibFuzzer.html) to
exercise image format readers against malformed input. A single binary,
`fuzz_image`, covers all compiled-in formats by dispatching at runtime based
on the `OIIO_FUZZ_FORMAT` environment variable. Nightly CI fuzzes every
format in parallel; crash reproducers are uploaded as GitHub Actions
artifacts.


## Prerequisites

- **clang ≥ 14** with libFuzzer support (`-fsanitize=fuzzer`). GCC does not
  support libFuzzer and will be rejected by CMake with a clear error.
- **CMake ≥ 3.15**
- All optional format libraries you want fuzz coverage for (the same ones
  used in a normal OIIO build). The `aswf/ci-oiio:2026.3` container has all
  of them pre-installed.


## Building the fuzz target

```bash
cmake -B build -S . \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DOIIO_BUILD_FUZZ_TARGETS=ON \
    -DSANITIZE=address,undefined

cmake --build build --target fuzz_image -j$(nproc)
```

`OIIO_BUILD_FUZZ_TARGETS=ON` is the only new flag. `SANITIZE=address,undefined`
instruments the full OpenImageIO library for maximum bug detection — omitting
it produces a valid binary but with much weaker coverage.

By default the local fuzzing engine is `-fsanitize=fuzzer`. On OSS-Fuzz the
`LIB_FUZZING_ENGINE` environment variable is set to the engine's `.a` path and
is picked up automatically by `src/fuzz/CMakeLists.txt`.


## Listing supported formats

```bash
build/src/fuzz/fuzz_image --list-formats
```

Prints one format name per line for every format compiled into this build.
This list drives the CI lint check: the build fails if any format returned
here lacks a directory under `src/fuzz/corpora/`.


## Running the fuzzer locally

```bash
# Fuzz JPEG for 60 seconds using the seed corpus as a starting point:
OIIO_FUZZ_FORMAT=jpeg \
    build/src/fuzz/fuzz_image \
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
    build/src/fuzz/fuzz_image \
    crash_<format>_<hash>
```

The AddressSanitizer or UBSan report appears on stderr. The key fields are
the error type (e.g. `heap-buffer-overflow`), the stack trace at the point of
the violation, and the allocation site.


## Minimizing a crash

Reduce a crash reproducer to the smallest input that still triggers it:

```bash
OIIO_FUZZ_FORMAT=<format> \
    build/src/fuzz/fuzz_image \
    -minimize_crash=1 \
    -exact_artifact_path=min_crash \
    crash_<format>_<hash>
```

Commit `min_crash` to `testsuite/fuzz-<format>/` as a regression test before
merging the fix.


## Adding seeds for a new format

When a new format plugin is added to OIIO:

1. The format automatically appears in `--list-formats` (no harness changes
   needed).
2. The CI lint job (`fuzz-corpus-lint` in `.github/workflows/ci.yml`) will
   **fail** until `src/fuzz/corpora/<format>/` exists. This is intentional —
   it enforces that every compiled-in format has at least a corpus directory.
3. Create the directory and add 1–5 representative seed files (each ≤ 100 KB):

```bash
mkdir src/fuzz/corpora/<format>/
# copy or generate seed files
cp path/to/sample.<ext>  src/fuzz/corpora/<format>/
# Or generate a small synthetic seed:
oiiotool --create 64x64 3 --ch R,G,B -o src/fuzz/corpora/<format>/seed.<ext>
```

4. Verify the seeds parse cleanly:

```bash
OIIO_FUZZ_FORMAT=<format> \
    build/src/fuzz/fuzz_image \
    src/fuzz/corpora/<format>/ \
    -runs=0
```

5. Commit both the corpus directory and any seed files.

To populate corpus seeds from testsuite and companion image repos run:

```bash
# Populate src/fuzz/corpora/<format>/ (local dev)
python3 src/fuzz/populate_corpora.py [--format <name>]

# Or write directly into a working corpus dir (as CI does):
python3 src/fuzz/populate_corpora.py --format <name> --dest corpus/
```

The script auto-detects companion repos at `../oiio-images` (sibling of the
repo root, as checked out by the fuzz CI job) or at `build/testsuite/oiio-images`
(fetched by `oiio_setup_test_data` during a regular test run). Only synthetic
seeds (formats with no existing source files, such as `seed.hdr`) are committed
to `src/fuzz/corpora/`; all other seeds are sourced from `testsuite/` or
companion repos at fuzz time.


## How format selection works

`fuzz_image` resolves the active format in priority order:

1. `OIIO_FUZZ_FORMAT` environment variable — used by CI matrix jobs.
2. `basename(argv[0])` stripped of `fuzz_` prefix — used by OSS-Fuzz
   per-format symlinks (`fuzz_jpeg → fuzz_image`).
3. `--format=<name>` command-line argument.
4. None set → the binary prints available formats and exits with an error.


## CI overview

- **Nightly fuzz** (`.github/workflows/fuzz.yml`): 29-format parallel matrix,
  Tier 1 formats run for 5.5 hours, Tier 2 for 1 hour. Evolved corpus is
  cached per format per branch.
- **Corpus lint** (`fuzz-corpus-lint` job in `.github/workflows/ci.yml`):
  builds `fuzz_image` on every PR, runs `--list-formats`, and fails if any
  compiled-in format lacks a `src/fuzz/corpora/<format>/` directory.


## OSS-Fuzz

The `ossfuzz/` directory (Phase 7) contains `project.yaml`, `Dockerfile`, and
`build.sh`. The `build.sh` loops over `--list-formats` output to create
per-format symlinks and seed corpus zips automatically, so new formats are
covered on OSS-Fuzz without any `build.sh` changes.
