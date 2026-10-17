# Per-format notes from the 2026 audit

What each reader needed, and which checks are deliberately N/A. Formats are
grouped by CLASS: how the reader is built, which decides how the audit is
done and where bugs tend to be. Class is NOT a statement of how important a
format is to users (targa and bmp are class A; raw, dpx, and ffmpeg are
among the most used). Priority is a separate, maintainer-set choice; see
"Choosing the target(s)" in SKILL.md.

What the class implies for effort:

- A: OIIO owns the parsing, so this is where arithmetic, RLE, and
  offset bugs live. Full census and a careful hunt of the size math. Most
  were already well hardened and needed only the bomb guard, but read the math
  anyway.
- B: the library owns decode. Check geometry from the decoder against the
  spec, every return, OIIO-owned buffers, and the metadata routes into the
  shared decoders. Bomb guard may be N/A (library has own limits).
- C: same as B but deeper in someone else's code. Do not hunt inside the
  dependency; log what is out of reach.
- D: audit the OIIO-side parsing only (ico delegates to png/bmp paths; null
  parses filename query args).

Check IDs (C1-C10) are defined in SKILL.md. "OK" = already satisfied.

## Class A. Hand-rolled parser (OIIO or vendored code does the decoding)

- targa: already hardened (check_open + bespoke plausibility guard); added seed.
- bmp: C2; INT32_MIN height negation; scanline y==height.
- dds: C2; propagate failed `readimg_*` returns (truncated decoded to black).
- psd: check_open + bomb guard; uint32 overflow undersized ZIP layer buffer;
  signed overflow in layer/mask rect; thumbnail/color-data/layer-channel
  length underflows; assert on layer-info length aborted debug builds.
- dpx: C2; 10-bit filled 1-channel swap overran a group of 3; UserData short
  read published uninitialized tail; large origin overflow in generic read
  paths (origin must be set before `check_open`). libdpx RLE unimplemented.
- cineon: C2 via `Filesystem::file_size` (no ioproxy); valid_file delegates to
  base open().
- sgi: C2; RLE16 short scanline (#5321); RLE run value byte past buffer for
  8-bit data (#5513).
- rla: corrupt RLE; `NextOffset` must advance; offset table indexed unchecked
  (field-rendered images halve height but not table).
- iff: C2 open-time guard ahead of bespoke readimg check; heavily hardened.
- pnm: two int-overflow OOB (nsamples write, scanline numbytes read) +
  C2. Bomb fixture must stay <= 65535/dim.
- hdr: C2 only (RGBE RLE decode bounds-checked). `.hdr` extension conflict
  with raw.
- fits: NAXIS count unvalidated -> `resize` OOM; no check_open (empty 0x0 HDU
  is legal); unknown BITPIX.
- softimage: C2 only; uses FILE* (no ioproxy), use `Filesystem::file_size`.
- zfile: reader lacked check_open (signed 16-bit dims -> gzread OOB write);
  unchecked gzread; C2.

## Class B. Glue over a mature codec/container library

OIIO validates geometry, checks returns, bounds its own buffers.

- tiff: raw-strip path ignored `TIFFReadRawStrip` csize<0 (bail like the tiled
  path); scratch sizing in 64-bit; CMYK/odd bit depths in the TILED path
  overflowed (`nchannels` 3 vs `m_inputchannels` 4); bad subimage left reader
  current. Deferred: `iptcsize *= 4` int overflow (>0.5 GB field);
  `compressBound((uLong)...)` on LLP64.
- jpeg: C2 added; checks must precede `jpeg_start_decompress` /
  `jpeg_read_coefficients` (progressive). Exif/ICC/XMP routes go to the shared
  decoders.
- png: C2 added; rest OK. Chunk-borne eXIf/zTXt/iTXt are the routes into
  shared decoders (and unbounded size: PNG chunks have no small cap).
- gif: C2; short graphics-control extension OOB read; subimage stream position
  tracked separately from index.
- openexr: C2 on BOTH the C++ and C-API readers; tile/display-window limits
  were missing from `check_open`; library bounds its own framebuffer (C6 left).
- jpeg2000: two codepaths (OpenJPEG, OpenJPH/HTJ2K) - guard both before
  decode; HTJ2K error handling continued after failure; zero components read
  past an array; OpenJPH global error handler needs static lifetime.
  Open: >4 components out of step; truncated codestream decodes silently
  (no completeness signal from OpenJPH).
- webp: check_open + C2 moved before the decoded-image allocation; no real
  bomb reachable (libwebp caps canvas, demux cross-validates).
- heif: had NO check_open; C2 N/A (libheif has its own limits); `irot` +
  `oiio:reorient=0` transposed dims => OOB read; subimage-to-item-id off by
  one.
- jpegxl: uint32 size check, unchecked `m_io->read()`, null `m_buffer` after
  basic-info-only; C2 N/A (extreme legit ratios). Writer: close-time failures
  must propagate.
- raw: bomb guard before `unpack()`; `raw_bps` 0/format-code collapsed the
  guard; undebayered flip table covered 4 of 8 codes (returned true without
  writing: heap disclosure); 64-bit index bounded by the real allocation
  (`raw_width*raw_height`, not header geometry: Fuji rotated sensors).
  Open: Fuji unrotation, FilterPattern vs flip, LibRaw internals.
- dicom: null `getInterData()`/`getOutputData()` (DASSERT only); C2 N/A; no
  local samples or writer, so verified by inspection.
- ptex: header validator (#5265) good; null `getData`/`getTile` after lazy-read
  errors; fixtures generated by script, run.py expects clean rejection.

## Class C. Heavy or proprietary external framework

Glue-check only; log library internals as deferrals.

- ffmpeg: heap bounds (gray nchannels), mid-stream frame-size change,
  uninitialized AVPacket, unbounded loop on bad stream, check_open +
  C2 (1 GB floor stays: a valid one-frame movie reaches 13000:1). Open:
  extension-only `valid_file`, single-frame delay-capable codecs, all
  libavcodec internals.
- r3d: ignored `DecodeVideoFrame` status; unchecked clip geometry; failed
  `InitializeSdk` double-finalize; silent open failures. Not compile-verified
  (SDK proprietary). Open: ioproxy ignored, dead scaffolding.
- openvdb: check_open from grid bbox; C2 deliberately NOT added (sparse
  format: extent vs file size unrelated); unguarded `gridPtrCast`, tree walk,
  readGrid; exception text clamped; state not reset on re-open. Open:
  OpenVDB reads declared string lengths unbounded (12 GB RSS on a truncated
  file) and `TypedMetadata::readValue` leaves value uninitialized.

## Class D. Container of other readers, or no file input

- ico: C2 on embedded-PNG path; DIB path capped 256x256.
- null: filename-query args unvalidated (see bug-catalog A6).
- term: output-only, no ImageInput: N/A by construction.

## Shared decoders (audit units of their own)

- exif (`src/libOpenImageIO/exif.cpp`): short blob into TIFF header (#5322);
  recursive IFD count overread at size-1; `size_t(-1)` unknown-type sentinel
  wrapping bounds math (also made Exif 3.0 UTF-8 decode); IFD depth cap 32;
  alloca with file count. Open: unbounded RATIONAL arrays, fresh seen-set in
  MakerNote handler.
- xmp (`xmp.cpp`): unbounded decode recursion (2.5 KB PNG segfaulted Release);
  quadratic list append and attribute scan; encoder did not escape values.
  Open: double materialization of packet; `decode_xmp` never reports failure;
  `string_is_rational(xmlname)` likely meant the value.
- icc (`icc.cpp`): OOB pointer formation, misaligned loads, uint32 pointer
  math; all through memcpy and 64-bit `range_in_bounds`.

## Cross-cutting sweeps that paid off

- check_open + check_compression_ratio in every reader lacking them (one
  batched commit, with bomb fixtures).
- Failed-seek state leak across 8 readers after the TIFF report.
- Callers: oiiotool failed-read deref, idiff/--diff comparing garbage.
- ImageSpec/ImageBuf constructors allocating from unvalidated counts.
