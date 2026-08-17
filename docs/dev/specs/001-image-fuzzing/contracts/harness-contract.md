# Contract: Fuzz Harness Interface

**Feature**: 001-image-fuzzing | **Date**: 2026-06-24 (revised)

There is one fuzz harness binary (`oiio_fuzz_image`) that handles all supported image formats.
It discovers formats at runtime and dispatches based on the active format selection.
This contract specifies what `fuzz_image.cpp` MUST satisfy. *(As-built: the shared
helpers originally split into a separate `fuzz_utils.h` header were later folded
directly into `fuzz_image.cpp` — it was the header's only include, so the split
bought no reuse, just an extra file to open.)*

---

## Format Selection

The active format for a given run is resolved in priority order:

1. **`OIIO_FUZZ_FORMAT` environment variable** — used by GHA matrix jobs
   (`OIIO_FUZZ_FORMAT=jpeg ./oiio_fuzz_image corpus/jpeg/ ...`)
2. **`basename(argv[0])`** — used by OSS-Fuzz per-format symlinks
   (`fuzz_jpeg -> oiio_fuzz_image`; binary reads its own name, strips `fuzz_` prefix)
3. **`--format=<name>` pseudo-argument** — parsed in `LLVMFuzzerInitialize` before
   libFuzzer strips its own flags; useful for local one-off runs
4. **None set** — harness MUST abort with a clear error message listing available formats
   (running without a format target produces meaningless blended coverage)

Format selection MUST be performed in `LLVMFuzzerInitialize`, stored in a `static`
variable, and accessed read-only in `LLVMFuzzerTestOneInput`.

---

## `LLVMFuzzerInitialize` Signature

```cpp
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
```

**Requirements**:
- MUST call `OIIO_FUZZ_INIT` (one-time OIIO attribute setup macro defined in
  `fuzz_image.cpp`).
- MUST resolve the active format via the priority order above.
- MUST validate the format name against the OIIO format registry (as implemented:
  `OIIO::get_extension_map()`, not the `extension_list` string attribute) and abort
  if the format is not recognized or not compiled in.
- MUST support a `--list-formats` pseudo-argument that prints all available formats
  (from the extension map, excluding `null` and `term`) to stdout and exits 0.
  This is used by the CI lint step to detect missing corpus directories. As
  implemented, `stdout` is explicitly `fflush()`ed before `exit(0)` — ASan's leak
  detector can `_Exit()` from an atexit hook on the way out, which would otherwise
  skip the normal stdio auto-flush and truncate the printed list.
- MUST return 0.

---

## `LLVMFuzzerTestOneInput` Signature

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
```

**Requirements**:
- MUST have C linkage (`extern "C"`).
- MUST accept `const uint8_t*` and `size_t` parameters exactly.
- MUST return `0` (non-zero is undefined behavior in future ABI).
- MUST NOT call `exit()`, `abort()`, or `_exit()` directly.
- MUST NOT retain mutable state between calls beyond the static format selection set
  in `LLVMFuzzerInitialize`.

---

## OIIO API Usage

*(As implemented: the read loop itself lives in `libOpenImageIO`, not in the
harness — see below. Once written for the harness, this logic turned out to be
generally useful for interactively exercising an `ImageInput` outside of fuzzing
too, so it was moved into the library and exposed via `oiiotool --testread` as
well, superseding the original "no production source changes" constraint.)*

**Standard formats** (single subimage — most formats):
- MUST use `OIIO::Filesystem::IOMemReader` to wrap input bytes — no disk I/O.
- MUST call `ImageInput::open(fake_filename, nullptr, &mem_reader)`.
- `fake_filename` MUST end with the primary extension for the target format
  (e.g., `"input.jpg"` for `jpeg`, `"input.tif"` for `tiff`), obtained from
  `oiio_format_primary_ext()`.
- MUST hand the open `ImageInput&` to `OIIO::pvt::test_read_all_images(inp, TypeUInt8)`
  (declared in `imageio_pvt.h`) rather than calling `read_image()` directly. This
  shared helper reads tiled images one full-width row of tiles at a time and
  scanline images in bounded chunks — never allocating a buffer sized to the
  (possibly adversarial) claimed image dimensions — and stops at the first read
  failure instead of continuing through a huge chunk grid.
- MUST call `inp->close()` before returning (or rely on `unique_ptr` RAII).

**OOM defense** is no longer a fixed per-call pixel-count check in the harness.
Instead, `OIIO_FUZZ_INIT` lowers OIIO's own global guards
(`OIIO::attribute("limits:imagesize_MB", 2048)`,
`OIIO::attribute("limits:resolution", 65536)`) to well under libFuzzer's
`rss_limit_mb` (4096, set in `oiio_fuzz_image.options`), so a corrupt header
claiming a multi-GB image is rejected by OIIO's normal error path before it can
trip libFuzzer's out-of-band RSS-limit kill (which would otherwise register as a
false-positive crash). `test_read_all_images()`'s chunked reads provide a second,
independent bound on top of this.

**Multi-subimage / MIP formats** (openexr, tiff, and any other format that
declares support): no longer a harness-level branch. `test_read_all_images()`
checks `inp->supports("multiimage")` / `"mipmap"` itself and iterates
automatically; the harness calls the same function for every format.

**Dispatch plugin formats** (raw, ffmpeg):
- These plugins dispatch to a third-party library that does not support
  `IOProxy`-based in-memory reads. Unlike the original plan (which called for
  `ImageInput::create()` + `IOMemReader`), the implemented dispatch path writes
  the fuzz bytes to a process-unique temp file (reused/overwritten across calls
  for throughput) and calls `ImageInput::open(tmppath)` — the extension on the
  temp path selects the plugin (`.cr2` → raw, `.mkv` → ffmpeg).
- If `ImageInput::create()` (or the temp-file `open()`) returns null (plugin not
  compiled in, or input rejected), return immediately without treating it as a
  failure.

---

## Error Handling

- If `ImageInput::open()` returns null (format rejected the input), return `0` — normal
  for most fuzz inputs.
- MUST NOT treat a null open result as a test failure.
- MUST NOT print to stdout or stderr during normal operation.
- Sanitizer findings are caught automatically by the runtime.

---

## Linking

- MUST link against `OpenImageIO` library.
- MUST link against `$LIB_FUZZING_ENGINE` (resolved from env; default `-fsanitize=fuzzer`).
- MUST be compiled with `-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer`
  (compile-time coverage instrumentation without linking a fuzzer runtime yet; the
  runtime comes from `$LIB_FUZZING_ENGINE` at link time) and linked with
  `-fsanitize=address,undefined $LIB_FUZZING_ENGINE`.
- MUST be compiled with clang or clang++. As implemented, both gcc (no `-fsanitize=fuzzer`
  support) and Apple's Xcode/Command-Line-Tools clang (missing the libFuzzer runtime,
  `libclang_rt.fuzzer_osx.a`) are rejected with a CMake warning and the fuzz target is
  skipped, rather than the whole build failing — see `data-model.md` FuzzBuildConfig.

---

## Shared Helpers (defined in `fuzz_image.cpp`)

| Symbol | Purpose |
|--------|---------|
| `OIIO_FUZZ_INIT` | One-time OIIO setup macro: single-threaded mode (`threads`, `exr_threads`) plus `limits:imagesize_MB` / `limits:resolution` tuned under libFuzzer's RSS budget |
| `oiio_fuzz_read(data, size, fake_filename)` | Open via `IOMemReader`, then delegate to `OIIO::pvt::test_read_all_images()` — covers single- and multi-subimage formats alike |
| `oiio_fuzz_read_dispatch(data, size, fake_filename)` | Dispatch-plugin read (raw, ffmpeg): write to a reused temp file, `ImageInput::open(tmppath)`, delegate to `test_read_all_images()` |
| `oiio_format_primary_ext(format_name)` | Returns primary extension for a format name |
| `oiio_format_is_dispatch(format_name)` | True for raw, ffmpeg |

**Dropped from the original design**: `oiio_fuzz_read_multi()` and
`oiio_format_is_multi()`. Subimage/MIP iteration moved into
`OIIO::pvt::test_read_all_images()` in `libOpenImageIO` itself, so every format
goes through the same call — there is no separate "multi" code path in the
harness to select.

---

## Corpus Convention

Each format has a corpus directory at `src/fuzz/corpora/<format>/`.
- Files MUST be valid images for the format.
- File names MUST be lowercase hexadecimal or descriptive (no spaces).
- The directory MAY be empty (slower but valid — fuzzer starts from random bytes).
- **Presence is mandatory**: the CI lint step (`./oiio_fuzz_image --list-formats`) fails if
  any format returned by `extension_list` (excluding `null`, `term`) lacks a
  `src/fuzz/corpora/<format>/` directory.

---

## OSS-Fuzz Compatibility

**Status: not yet built.** No `ossfuzz/` directory exists in the repo — this section
remains a forward-looking contract for User Story 5 (P3, deferred). The harness-side
support it depends on (`argv[0]` dispatch, `$LIB_FUZZING_ENGINE` linkage,
`--list-formats`) is already in place, so onboarding remains additive whenever it's
picked up.

OSS-Fuzz requires separate named binaries per fuzz target. `build.sh` satisfies this by
creating per-format symlinks in `$OUT/`:

```bash
cp $WORK/build/src/fuzz/oiio_fuzz_image $OUT/
for fmt in $(./oiio_fuzz_image --list-formats); do
    ln -s oiio_fuzz_image $OUT/fuzz_${fmt}
    zip -j $OUT/fuzz_${fmt}_seed_corpus.zip src/fuzz/corpora/${fmt}/* 2>/dev/null || true
done
```

When OSS-Fuzz invokes `fuzz_jpeg`, the binary reads `basename(argv[0])` = `fuzz_jpeg`,
strips the `fuzz_` prefix, and targets the `jpeg` format. No harness source changes needed.
