# Contract: Reader-Hardening API Usage

The exact `ImageInput` helper contracts each format audit relies on. Signatures are
authoritative as of this branch (`src/include/OpenImageIO/imageio.h`). Using them
consistently is what makes the audit uniform.

## check_open — extent limits

```cpp
bool ImageInput::check_open(const ImageSpec& spec,
                            ROI range = {0,65535, 0,65535, 0,1, 0,4},
                            uint64_t flags = 0);
```

- **Call**: at the top of `open()`, immediately after the ImageSpec's dimensions,
  channels, and depth are populated from the header, before any allocation/decode.
- **`range`**: pass the FORMAT's own spec maxima (not policy). Widen the defaults
  when the format legitimately allows more (e.g., z-depth for volumetric) or narrow
  when the format caps a dimension (e.g., 16-bit dimension fields → 65535).
- **Also enforces** global policy: `limits:channels` (1024), `limits:resolution`
  (1048576/dim), `limits:imagesize_MB` (32768). Do NOT duplicate these per-format.
- **On false**: it has already called `errorfmt()`. Return false from `open()`.

## check_compression_ratio — decompression-bomb guard

```cpp
bool ImageInput::check_compression_ratio(imagesize_t declared_bytes,
                                         imagesize_t filesize,
                                         imagesize_t max_ratio = 10000,
                                         imagesize_t min_declared_bytes = (1ULL<<30));
// convenience:
bool ImageInput::check_compression_ratio(const ImageSpec& spec, imagesize_t filesize,
                                         imagesize_t max_ratio = 10000,
                                         imagesize_t min_declared_bytes = (1ULL<<30));
```

- **Call**: once `spec.image_bytes(true)` (declared uncompressed size) and the file
  size are known, before decode/allocation. Prefer the `(spec, filesize)` overload.
- **`filesize`**: from `Filesystem::file_size()` or the ioproxy size; pass 0 only if
  truly unknown (then the check is a no-op — avoid).
- **`max_ratio`**: smallest value with no false positives on the format's genuine
  corpus (default 10000). Highly-compressible formats may justify higher; document
  the choice.
- **On false**: `errorfmt()` already called → return false.

## valid_raw_span_size — bounds-checked decode target

```cpp
bool ImageInput::valid_raw_span_size(cspan<std::byte> buf, const ImageSpec& spec,
                                     int xbegin, int xend, int ybegin, int yend,
                                     int zbegin = 0, int zend = 1,
                                     int chbegin = 0, int chend = -1);
```

- **Use**: before writing decoded pixels into `buf`, confirm the `[chbegin,chend) x
  [xbegin,xend) x [ybegin,yend) x [zbegin,zend)` slab fits `buf`. Replaces implicit
  "the caller's buffer is big enough" assumptions.
- **On false**: `errorfmt()` already called → return false.

## limits:* attributes (policy layer — read only, do not add without sign-off)

- `int limits:channels` (1024), `int limits:resolution` (1048576),
  `int limits:imagesize_MB` (32768). Set/read via `OIIO::attribute()` /
  `get_int_attribute()`. New `limits:*` require maintainer approval (research.md D2).

## imageinput:strict — failure posture switch

- `int imageinput:strict`. Read via `OIIO::get_int_attribute("imageinput:strict")`.
- **Metadata-only defect** → strict: `errorfmt()`+false; non-strict: drop item, warn,
  continue.
- **Pixel-affecting defect** → ALWAYS `errorfmt()`+false, ignore `strict`.

## Error-reporting convention

- Report via `this->errorfmt("...", ...)` (never printf/streams). Return `bool`
  status; do not throw. Messages that reject on a policy limit SHOULD tell the user
  which `limits:*` attribute to raise if the file is genuinely valid (match existing
  wording in `imageinput.cpp`).

## Span usage

- Represent untyped buffers as `cspan<std::byte>` / `span<std::byte>`; typed pixel
  runs as `span<T>`/`cspan<T>`. Prefer these over `ptr,len` pairs (project guideline).
