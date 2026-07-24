# Fuzzing Quickstart (Developer Guide)

**Status**: Superseded by [`docs/dev/fuzzing.md`](../../docs/dev/fuzzing.md), the
published, maintained version of this document. Read that file for current
build/run/reproduce instructions — this file is kept only as a historical record
of the Phase 1 design draft and a note on how it diverged from what shipped.

**Audience**: OpenImageIO contributors who want to run fuzzing locally, add a new
harness, or reproduce a crash found by CI.

---

## Why this file is no longer authoritative

This draft was written against the *original* plan of one harness binary per
format (`fuzz_jpeg`, `fuzz_exr`, ..., built via a per-target `add_fuzz_target()`
CMake macro and an `all_fuzz_targets` aggregate target). During implementation the
design pivoted to a single dynamic-dispatch binary, `oiio_fuzz_image`, that
selects its target format at runtime via `OIIO_FUZZ_FORMAT` (or `argv[0]`, or
`--format=`). See `plan.md` and `contracts/harness-contract.md` for the as-built
contract.

Concretely, everywhere this draft said:

| This draft said | The shipped system does |
|---|---|
| `build-fuzz/src/fuzz/fuzz_jpeg`, `fuzz_exr`, `fuzz_tiff`, ... (one binary per format) | `build/src/fuzz/oiio_fuzz_image` (one binary; `OIIO_FUZZ_FORMAT=jpeg ./oiio_fuzz_image ...` selects the format) |
| `cmake --build build-fuzz --target all_fuzz_targets` | `cmake --build build --target oiio_fuzz_image` (`all_fuzz_targets` was never wired up — it exists only as a comment in `src/fuzz/CMakeLists.txt`) |
| Register a new format in `src/fuzz/CMakeLists.txt` via `add_fuzz_target(myformat)` | Nothing to register — any format OIIO compiles in is fuzzable immediately; only a `src/fuzz/corpora/<format>/` directory is needed (enforced by the `fuzz-corpus-lint` CI job) |
| Per-format `fuzz_<format>.cpp` harness with its own `LLVMFuzzerTestOneInput` | One `src/fuzz/fuzz_image.cpp`; the read loop was recognized as generally useful and moved into `OIIO::pvt::test_read_image()` / `test_read_all_images()` in `libOpenImageIO` itself, also exposed via `oiiotool --testread` — a deliberate addition, not just an exception made for the harness's sake |
| Seeds committed wholesale for every format after running `populate_corpora.py` | Only formats with no other source commit a synthetic seed (dpx, fits, hdr, iff, jpeg2000, jpegxl, openexr, sgi); all other seeds are pulled fresh at CI time by a `populate_corpora.py` workflow step, not committed |
| OSS-Fuzz local simulation section, implying `projects/openimageio/` groundwork was imminent | No `ossfuzz/` files exist yet — deferred (User Story 5, P3) |

The harness-selection priority order, `--list-formats`, crash reproduction flow,
and corpus-lint enforcement mechanism described in the rest of this draft are
conceptually correct and now documented accurately (with real command lines,
tier timings, and container versions) in `docs/dev/fuzzing.md`.
