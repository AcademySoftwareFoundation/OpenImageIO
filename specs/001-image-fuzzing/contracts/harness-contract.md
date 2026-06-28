# Contract: Fuzz Harness Interface

**Feature**: 001-image-fuzzing | **Date**: 2026-06-24 (revised)

There is one fuzz harness binary (`oiio_fuzz_image`) that handles all supported image formats.
It discovers formats at runtime and dispatches based on the active format selection.
This contract specifies what `fuzz_image.cpp` and `fuzz_utils.h` MUST satisfy.

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
- MUST call `OIIO_FUZZ_INIT` (one-time OIIO attribute setup from `fuzz_utils.h`).
- MUST resolve the active format via the priority order above.
- MUST validate the format name against `OIIO::get_string_attribute("extension_list")`
  and abort if the format is not recognized or not compiled in.
- MUST support a `--list-formats` pseudo-argument that prints all available formats
  (from `extension_list`, excluding `null` and `term`) to stdout and exits 0.
  This is used by the CI lint step to detect missing corpus directories.
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

**Standard formats** (single subimage — most formats):
- MUST use `OIIO::Filesystem::IOMemReader` to wrap input bytes — no disk I/O.
- MUST call `ImageInput::open(fake_filename, config_or_nullptr, &mem_reader)`.
- `fake_filename` MUST end with the primary extension for the target format
  (e.g., `"input.jpeg"` for `jpeg`, `"input.tiff"` for `tiff`).
- MUST guard pixel reads with an OOM check: skip `read_image` if
  `spec.image_pixels() >= 64 * 1024 * 1024` (64 MP limit).
- MUST call `inp->close()` before returning (or rely on `unique_ptr` RAII).

**Multi-subimage formats** (exr, tiff):
- MUST iterate subimages via `seek_subimage()` with the same OOM guard per subimage.
- Detected by checking `inp->supports("multiimage")` after open.

**Dispatch plugin formats** (raw, ffmpeg):
- These plugins dispatch to a third-party library that performs format detection
  internally. MUST use `ImageInput::create("raw")` / `ImageInput::create("ffmpeg")`
  rather than relying on file extension routing.
- If `create()` returns null (plugin not compiled in), return 0 immediately.

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
- MUST be compiled with `-fsanitize=address,undefined -fno-omit-frame-pointer`.
- MUST be compiled with clang or clang++ (gcc does not support `-fsanitize=fuzzer`).

---

## `fuzz_utils.h` Shared Helpers

| Symbol | Purpose |
|--------|---------|
| `OIIO_FUZZ_INIT` | One-time OIIO setup macro (suppress errors, set thread count=1) |
| `oiio_fuzz_read(data, size, fake_filename)` | Standard single-subimage read with OOM guard |
| `oiio_fuzz_read_multi(data, size, fake_filename)` | Multi-subimage read (EXR, TIFF) |
| `oiio_fuzz_read_dispatch(data, size, plugin_name)` | Dispatch-plugin read using `create()` |
| `oiio_format_primary_ext(format_name)` | Returns primary extension for a format name |
| `oiio_format_is_multi(format_name)` | True for formats that support multiple subimages |
| `oiio_format_is_dispatch(format_name)` | True for raw, ffmpeg |

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
