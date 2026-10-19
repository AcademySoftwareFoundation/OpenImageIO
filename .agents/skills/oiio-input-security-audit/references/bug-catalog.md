# Bug catalog for OIIO input readers

Each entry: pattern, how to find it, fix shape, a real example. Grep from the
repo root; narrow to the plugin dir under audit.

Append new classes at the end of their section (see SKILL.md "Feeding new
lessons back"). Keep entries short.


## A. Size, extent, and allocation

### A1. Allocation before validation
Header dims drive an allocation or an expensive library call before
`check_open()`/`check_compression_ratio()` runs.
- Find: `grep -n "check_open\|check_compression_ratio" <plugin>`; compare line
  numbers with the first `resize|reserve|new |malloc|make_unique|vector<` and
  the first decode call (`jpeg_start_decompress`, `jpeg_read_coefficients`,
  `opj_decode`, `heif_decode_image`, `WebPDecode`, `JxlDecoder`).
- Fix: validate right after the header is parsed, before the call.
- Seen: jpeg (progressive: checks ran after `jpeg_read_coefficients`), webp
  (check after the decoded-image alloc), jpeg2000 (check after `opj_decode`),
  HTJ2K path (no check at all; plugin had TWO codepaths, guard both).
- Also: plugins with two decode paths (C++ and C API in openexr, OpenJPEG and
  OpenJPH in jpeg2000, DIB and embedded PNG in ico) need the guard on each.

### A2. Reader with no check_open at all
- Find: plugins where `check_open` is absent (heif, fits, zfile were).
- Gotcha: formats that allow an empty image (FITS 0x0 HDU) must guard
  `width>0 && height>0` before calling check_open.

### A3. Limits applied to only part of the spec
`check_open` once applied `limits:resolution` only to the data window; a tiny
EXR declaring a 2^28-wide tile or 838M display window got through and later
`read_tiles`/maketx tried to allocate terabytes. Check tile dims, display
window, z/depth, subimage count, mip levels, frame counts, and the
"ImageBuf allocation failed" path (ImageBuf ctor zeroed missing pixels).
- Also order checks so deterministic ones (resolution) run before
  memory-dependent ones (`limits:imagesize_MB`): a corrupt file should fail the
  same way on every machine.

### A4. Declared length used to size a buffer without a file-size bound
- Find: `resize(` / `reserve(` / `assign(` / `vector<..>(n)` where n comes
  from the file (ICC length, thumbnail length, UserSize, layer channel length,
  string length prefix).
- Fix: compare against bytes remaining (not whole file size: the block starts
  after earlier headers), and use unsigned comparison for 64-bit fields (PSB
  data_length cast to int64 wraps negative and slips past a `<` guard).
- Seen: psd thumbnail (`length - 28` underflow), psd color data, psd layer
  channel data, dpx UserSize (compared to total file size, not remaining).

### A5. Default-initialized buffer published after short read
A failed or short read leaves the tail of a buffer uninitialized, then the
buffer is published as metadata or pixels (disclosure of stale heap).
- Find: every `read(`/`gzread`/`ioread`/`ReadUserData`/`fread` whose return
  is ignored or only partially checked; `default_init_vector`, `resize` then
  read.
- Fix: treat short read as error (pixel => always hard error; metadata =>
  strict rule), never publish the unread tail.
- Seen: dpx UserData, zfile gzread (header and scanline), jpegxl input read,
  raw undebayered flips that returned true without writing the buffer.

### A6. Pathological counts in names/vectors
- Find: loops generating one string/ustring per channel, `default_channel_names`
  style `reserve(nchannels)`, procedural readers parsing query args.
- Fix: bound the count BEFORE generating names. Even ImageSpec construction
  allocates, so `check_open` is too late for it; validate in the constructor
  path too.
- Seen: null reader (CHANNELS=-1 sign-extended into a vector resize and
  aborted with length_error; huge CHANNELS hung), ImageSpec::default_channel_names.

### A7. Unbounded resource use from metadata
Linear-in-input work or memory that is not a safety defect but amplifies:
XMP packet materialized twice (8.7x RSS), huge RATIONAL arrays, quadratic list
re-join. Fix with per-call budgets (xmp: 4096 attributes / 1 MB payload, depth
64). If a cap risks dropping legitimate metadata, it is a policy call: log as
P2 deferral and ask the maintainer.


## B. Integer arithmetic

### B1. 32-bit product feeding a 64-bit use
`width*height*bytes` or `width*nchannels` computed in `int`/`uint32_t`, then
used for allocation, indexing, or loop count.
- Find: `grep -nE "width *\* *|height *\* *|nchannels *\* *|\* *(depth|bps|bits)"`
  and check each operand's type; look for `int`/`uint32_t` locals and struct
  fields; `(int)` casts; `size_t(a*b)` where the multiply happens before the
  cast; LLP64 `uLong` (Windows) in zlib calls (`compressBound`).
- Fix: promote one operand first (`imagesize_t(w) * h * ...`), validate factors
  against format caps pre-multiply, make allocation and indexing use the same
  width.
- Seen: pnm (nsamples int wrap => OOB WRITE; per-scanline numbytes int trunc
  => OOB READ), psd ZIP layer buffer (uint32 undersized, 64-bit row offset
  => OOB read), jpegxl out-buffer sanity check (also false-rejected valid >4 GB
  images), raw (offsets int), openvdb dense-window math, tiff scratch,
  r3d scanline offset, scanline chunk size > 2 GB truncating to 0 then
  dividing by zero.

### B2. Signed overflow / negation UB
- Find: `abs(`, unary `-x` on file-controlled signed fields, `right - left`
  with `(int)` casts, struct fields typed unsigned that really hold signed
  values.
- Fix: retype fields to what they hold, widen to int64 before subtracting,
  negate in 64-bit; then let `check_open` reject the range.
- Seen: bmp INT32_MIN height negation, psd layer rect `abs((int)r-(int)l)`.

### B3. Origin/offset overflow in generic read paths
A file-supplied data-window origin makes `y + spec.y + chunk` overflow `int`
in the generic `read_image`/`read_scanlines`/`read_tiles`; the wrapped range
writes past the caller's buffer. `check_open` now rejects origin+size overflow
and zero tile height/depth (which looped forever). When touching generic read
paths, step by 64-bit offsets and clamp partial edge tiles.
- Seen: DPX (origin must be set BEFORE `check_open` so it can see it); PSD
  applies the check per layer.

### B4. Off-by-one in range tests
- Find: `y > height`, `x > width`, `<= size`, `offset >= size` followed by a
  multi-byte read at that offset.
- Seen: bmp `read_native_scanline` allowed y==height; exif recursive IFD
  checked `offset < size` but then read a 2-byte count at `size-1`; sgi RLE
  loop read the run value byte one past the buffer when a scanline ended in a
  run count (the literal-copy branch had the check, the run branch did not).
  Sibling branches should have symmetric checks: compare them.

### B5. Sentinel values entering arithmetic
`size_t(-1)` "unknown type" from a size table multiplied by a count wraps the
bounds check (exif `tiff_data_size`). Find tables with -1/0 "unknown" returns
and every caller that multiplies the result.


## C. Library and IO return handling

### C-1. Ignored or unchecked returns
- Find: calls as bare statements; `bool ok = ...` never read; `(void)` casts;
  `OIIO_DASSERT`/`assert` standing in for a runtime check (compiled out in
  Release).
- Seen: dds `readimg_*` (truncated file decoded to black), r3d
  `DecodeVideoFrame` (undecoded buffer returned as success), raw
  `do_unpack`, jpeg2000 HTJ2K decode loops (logged error but continued), dicom
  `getInterData()` null (only DASSERT-guarded), ptex `getData`/`getTile` null
  once the library enters an error state, openvdb `readGrid` null,
  `gridPtrCast` null, jpegxl output close-time failures.

### C-2. Success returned after error logged
A function logs `errorfmt` but does not `return false`, or a later call
overwrites the failure. Callers then copy from an empty buffer. Check each
`errorfmt` is followed by an exit.

### C-3. Library state that is not exception/longjmp safe
- libjpeg/libpng use setjmp error managers; a `longjmp` over C++ objects with
  destructors leaks or is UB. Keep C++ objects out of the setjmp scope.
- OpenVDB/OpenJPH use exceptions; wrap in try/catch with catch-all; clamp
  exception text (OpenVDB embedded a multi-GB file-supplied string in the
  message; clamp to 1 KB).
- Process-global handlers: install once with static lifetime from a
  thread-safe static initializer (OpenJPH stored a pointer to a freed stack
  handler; later error => stack-use-after-return). Do not reinstall per
  `open()`: the globals are plain non-atomic and read from decode threads.

### C-4. Decoder-provided geometry disagreeing with the spec
Streams and rotated images can change geometry after the header.
- Seen: ffmpeg frame size/pix_fmt changes mid-stream (smaller frame read past
  its end); ffmpeg gray movies set one plane but nchannels=3 (3x over-read);
  heif `irot` with `oiio:reorient=0` (spec swapped w/h, libheif returned the
  stored plane); raw returned channel count vs spec; tiff CMYK->RGB
  (`m_spec.nchannels`=3 while `m_inputchannels`=4: tile path unpacked 4
  values into 3-channel buffer). Fix: compare decoder-reported dims, channel
  count, and stride against the spec before every copy-out; size scratch
  buffers from the decoder-reported values, not the header.
- Uninitialized packet structs: `av_packet_alloc()`, not stack AVPacket,
  because `av_read_frame` does not init on failure.

### C-5. Null model state after "success"
open() returns true with a member still null (jpegxl basic-info without an
image-out buffer => null deref in read_native_scanline). After the decode
loop, assert every member later dereferenced.


## D. Structure walking: loops, recursion, state

### D1. Self-referential or non-advancing chains
- Find: loops that follow a next-offset/next-IFD/next-subimage/next-chunk.
- Fix: require strict forward progress (rla `NextOffset` must point past the
  current header) and/or a seen-set; plus a depth cap for recursion (exif IFD
  depth 32, xmp nesting 64).
- Watch: a fresh seen-set per nested handler re-walks IFDs (exif MakerNote).

### D2. Stack exhaustion
- Find: recursion over file nesting; `alloca`/`OIIO_ALLOCA` with a
  file-controlled count (assert compiled out under NDEBUG). Use
  `OIIO_ALLOCATE_STACK_OR_HEAP`.

### D3. Failed seek leaves a half-switched reader
Readers that parse the target subimage header into `m_spec` before deciding to
accept it, then return false without undoing it, leave `m_subimage`/state
mismatching the decoder. Then `seek_subimage()`'s "already there" early-out lies.
- Fix: on rejection, reset to "no current subimage" (index -1), and put the
  early-out AFTER the range check so `seek_subimage(-1, ...)` cannot match the
  sentinel. Also check subimage-to-item-id style lookups for off-by-one.
- Seen: tiff, then dds, dpx, fits, gif, heif (id lookup off by one), ico,
  openexr, ptex (adopted face res before checking mip level), rla.
- Note: reopening an ImageInput after a SUCCESSFUL close() is not a supported
  contract; don't report that as a defect. Only argue per-file state reset on
  its own merits.

### D4. Wrong chain: caller of the reader
The reader may be safe but the tool using it is not: oiiotool dereferenced
specs of a failed read (no subimages); idiff/`--diff` compared uninitialized
lazy ImageCache tiles after a failed pixel read and could report a false PASS.
Check callers that go through ImageCache (lazy reads: header opens, pixels
fail later) and procedural readers that claim `supports("procedural")`.


## E. Alignment and pointer formation

### E1. Misaligned typed loads from a byte blob
`*(const T*)(ptr + off)` at a file-controlled offset: UB and a fault on
strict-alignment CPUs. Use `memcpy` into a local. Same for `char16_t*`
reinterpretation of UTF-16 strings at odd offsets.
- Find: `grep -nE "\*\(const [a-z_0-9:]+ ?\*\)|reinterpret_cast<const"`.
- Seen: icc (header, tag table, mluc fields), exif interop IFD.

### E2. Out-of-bounds pointer formation
`base + file_offset` formed before the range check is UB even if never
dereferenced, and on 32-bit the uint32 sum can wrap past the check. Check
`range_in_bounds(offset, len, size)` in 64-bit FIRST, then form the pointer.
- Seen: icc tag/mluc ranges.

### E3. SIMD partial loads
Public-header helpers that read the full vector then mask (NEON
`vfloat4::load(values, n)` read 12 bytes past the buffer). Check `n<width`
branches in `simd.h`.


## F. Format-specific patterns worth re-checking everywhere

- RLE decoders: run count vs remaining output AND vs remaining input; zero
  length runs; run byte read at end of buffer; literal vs run branch symmetry;
  16-bit variants with half-width lengths.
- Packed-sample unpackers (DPX 10/12-bit methods A/B, 1-channel swap
  workaround): the group-of-N loop must confirm a full group remains.
- Palette/color-map indexing: bounds-check every index case, including
  bits-per-pixel variants.
- Container sub-block / extension lengths (GIF graphics control extension):
  confirm length before reading its fields.
- Metadata thumbnails, userdata, makernotes: length fields are attacker input.
- Headers whose count fields feed `resize` (FITS NAXIS: valid range 0-999).
- Zero-valued denominators from headers: bit depth 0 (raw_bps) collapsed
  declared_bytes to 0 and disabled the bomb guard; floor it. Check every
  divisor and every "declared size" that can legitimately evaluate to 0.
- Format identification: `valid_file` that only matches the extension, or that
  conflicts with another plugin (`.hdr` claimed by both hdr and raw).
- Writers that echo read metadata into XML/headers (xmp `encode_xmp` did not
  escape values from a hostile file: markup injection into every output file).
  Thumbnail metadata leaking to formats that can't hold a thumbnail.
