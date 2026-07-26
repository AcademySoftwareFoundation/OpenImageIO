# Phase 1 Data Model: Image Format Security & Stability Audit

This effort has no runtime data model (it changes decode code, not schemas). The
"entities" here are the audit's *tracking artifacts* — the inventory, per-format
checklist, hazard taxonomy, and deferral log that keep a multi-PR effort coherent.

## Entity: Format Plugin (audit unit)

One record per `src/<FORMAT>.imageio/` reader. The unit of a single phase/PR.

| Field | Meaning |
|-------|---------|
| `name` | Format slug (e.g., `tiff`) |
| `dir` | `src/<name>.imageio/` |
| `decode_backend` | External lib(s) and/or in-tree decoder |
| `untrusted_surface` | none / thin-glue / hand-rolled-parser / compressed |
| `tier` | 1–4 risk ordering (see research.md Decision 8) |
| `status` | pending / in-progress / audited |
| `pr` | Link to the format's PR once opened |
| `findings_summary` | Which checks applied / already-satisfied / N-A + issues found |

### Format inventory (SC-007 completion tracker)

> **This table is the authoritative SC-007 tracker.** `status` legend:
> **pending** = not yet started; **in-progress** = audit branch open, not merged;
> **audited** = the format's hardening is merged (in a per-format or a shared
> cross-format commit, per the amended FR-011) with C1–C10 outcomes recorded. The overall
> effort is complete only when no format remains `pending` or `in-progress`.

| Format | Tier | Untrusted surface | Status |
|--------|------|-------------------|--------|
| tiff | 1 | libtiff, hand-rolled metadata | in-progress (fix/tiff-hardening 6b2a10306, 309889435; PR pending) |
| jpeg | 1 | libjpeg-turbo, EXIF/ICC | in-progress (C2 decompression-bomb guard added; PR pending. C1/C3/C5-C8 already-satisfied. Shared EXIF-decoder OOB logged to deferral doc.) |
| png | 1 | libpng, chunk parsing | in-progress (C2 decompression-bomb guard added; PR pending. C1/C3/C4/C5/C6/C7/C8 already-satisfied.) |
| openexr | 1 | OpenEXR/Imath | in-progress (C2 decompression-bomb guard added to BOTH the C++ and C-API readers; PR pending. C1/C3/C4/C5/C7/C8 already-satisfied; C6 library-bounded framebuffer left as-is.) |
| psd | 1 | hand-rolled parser | in-progress (C1 check_open + C2 bomb guard added on composite; fixed uint32 overflow that undersized the ZIP layer-channel buffer -> OOB read. Commit 9306d8572; PR pending.) |
| targa | 1 | hand-rolled + RLE | in-progress (already hardened: C1 check_open + bespoke plausibility bomb guard, bounds-safe RLE/palette. Added H2 bomb seed. Commit 7969782d0; PR pending.) |
| bmp | 1 | hand-rolled + RLE | in-progress (C2 bomb guard added; fixed INT32_MIN height-negation UB and read_native_scanline y==height OOB. Commit fd6180311; PR pending.) |
| gif | 1 | giflib, LZW | in-progress (C2 bomb guard added; fixed short graphics-control extension OOB read. Commit 66463c8ca; PR pending.) |
| dds | 1 | hand-rolled + block compression | in-progress (C2 bomb guard added; propagated failed readimg_scanlines/tiles returns so truncated files error instead of decoding to black. Commit da87ccda1; PR pending.) |
| jpeg2000 | 2 | openjpeg + openjph | in-progress |
| webp | 2 | libwebp | in-progress (moved check_open + added C2 bomb guard BEFORE the decoded-image allocation, which previously ran first. No reachable bomb: libwebp caps the canvas at 16383 (<=~1 GB) and the demux cross-validates canvas vs frame -- change is defensive/consistency. C3/C5/C7/C8 already-satisfied (bounded valid_file, all libwebp returns checked, exif has local tiff-header guard + strict-gating, icc strict-gated). Added oversized-canvas seed + wired testsuite/webp/src into FORMAT_SOURCES. PR pending.) |
| heif | 2 | libheif | in-progress (C1 APPLIED: the reader had NO check_open at all -- OIIO limits:* were never enforced and dims from libheif went unvalidated. Added check_open on the image-handle's declared dims BEFORE heif_decode_image, so OIIO policy + degenerate-dim rejection apply before libheif decodes/allocates. Verified: limits:channels now rejects. C2 N/A (compressed codec; libheif has its own security limits; fixed-ratio guard would false-positive). C3/C5/C7/C8 already-satisfied (bounded valid_file, C++ API in try/catch + every C-API heif_error checked, exif bounds-checked size>=10 + strict-gated). Bomb fixture impractical (libheif validates box structure); verified by hand. PR pending.) |
| jpegxl | 2 | libjxl | in-progress (3 glue fixes: C4 promoted the out-buffer-size sanity check to 64-bit (uint32 product overflowed for >4 GB images, false-rejecting valid ones); C5 now checks the m_io->read() result (was feeding uninitialized tail bytes to libjxl on a short read); C9 rejects a file that yields basic-info but no decoded image (m_buffer stayed null -> read_native_scanline null-deref). C1 check_open already present + well-positioned; C3 valid_file bounded; C5 all JxlDecoder* returns checked. C2 N/A: JXL legitimately reaches extreme ratios, a fixed compression-ratio guard would false-positive; check_open cap + libjxl memory defenses bound allocation. Added H4 truncated seed + wired testsuite/jxl/src into FORMAT_SOURCES. PR pending.) |
| raw | 2 | libraw | in-progress |
| dpx | 2 | hand-rolled (libdpx) | in-progress (C2 bomb guard added; C1/C3/C5/C7/C8 already-satisfied. libdpx RLE decode unimplemented -> ReadBlock returns false (no RLE bomb); uncompressed read checks short-read; QueryRGBBufferSize uses clamped_mult64. Added H2 bomb seed + wired testsuite/dpx/src into FORMAT_SOURCES. PR pending.) |
| cineon | 2 | hand-rolled (libcineon) | in-progress (C2 bomb guard added via Filesystem::file_size; C1 already-satisfied (check_open + nchannels 1-8 + bitdepth whitelist). valid_file delegates to base open() -- cineon has no ioproxy. Added H2 bomb seed + wired testsuite/cineon/src into FORMAT_SOURCES. PR pending.) |
| sgi | 2 | hand-rolled + RLE | in-progress (C2 bomb guard added; C1/C3/C4/C6/C7/C8 already-satisfied via #5279/#5303/#5321. RLE16 short-scanline OOB from external GHSA draft already fixed by #5321. Added H2 bomb seed + wired testsuite/sgi/src into FORMAT_SOURCES. PR pending.) |
| rla | 2 | hand-rolled + RLE | in-progress |
| iff | 2 | hand-rolled + RLE | in-progress (C2 open-time bomb guard added to close the caller-allocation window ahead of the existing bespoke readimg plausibility check; C1/C4/C5/C6/C7/C8 already-satisfied (heavily hardened: span-bounded RLE/ZBUF decode, chunk-size-vs-filesize, tile-coord + channel-config validation). Added H2 bomb seed + wired testsuite/iff/src into FORMAT_SOURCES. PR pending.) |
| dicom | 2 | DCMTK | in-progress (C5 APPLIED: null-checked getInterData() (used via m_dipixel->getData()/getRepresentation()) and getOutputData() (m_internal_data), both previously unchecked -> corrupt DICOM that fails frame decode caused a null-deref memcpy in read_native_scanline (the OIIO_DASSERT guard compiles out in release). C1 check_open already present; C2 N/A (DCMTK owns the compressed decode + limits). No local test possible: OIIO has no DICOM writer and no local .dcm samples (suite uses the private dicom-images-pvt corpus, exercised on CI fuzz). Verified by compile + inspection. PR pending.) |
| pnm | 3 | hand-rolled | in-progress (C4+C2. Fixed two int-overflow hazards reachable when limits:resolution is raised: nsamples = width*nchannels was int (wrap -> huge imagesize_t count -> OOB WRITE in ascii_to_raw/unpack), and per-scanline numbytes was int (truncating scanline_bytes() -> undersized buf -> OOB READ); both now imagesize_t. Added C2 check_compression_ratio so a tiny header declaring a multi-GB image is rejected before the caller allocates. C1 check_open present; C3 valid_file bounded (1KB header pread); PFM substr is noexcept-clamping (truncation caught by premature-EOF check). Added H2 bomb seed (testsuite/pnm/src already wired). PR pending.) |
| hdr | 3 | hand-rolled RGBE | in-progress (C2 bomb guard added; C1 check_open present (ROI 65535, rejects nonpositive), C3 valid_file bounded (2-byte "#?"), C4/C7 already-safe: RGBE RLE decode fully bounds-checked (count vs ptr_end-ptr, scanline_buffer indices in range), sizes bounded by the 0x7fff RLE cap + 65535 ROI, all IO reads checked. RGBE RLE ratio tops out well under 10000x so the bomb guard won't false-positive. Added H2 bomb seed + wired testsuite/hdr/src into FORMAT_SOURCES. PR pending.) |
| fits | 3 | hand-rolled | in-progress (3 fixes. CRASH: NAXIS count was stoi'd then m_naxis.resize(m_naxes) with no validation -> a huge/negative NAXIS wrapped/blew the resize into OOM/crash before the later >4 check; now rejected (0-999) before resize. C1: reader had NO check_open -> added it (+ reject unknown BITPIX) for non-empty images, guarded by width>0 && height>0 so the legal empty 0x0 primary HDU still passes. C2: added check_compression_ratio (uncompressed FITS -> ratio ~1 for legit files). C3 valid_file bounded (6-byte "SIMPLE"). Added H8 bad-naxis + H2 bomb seeds; wired testsuite/fits/src into FORMAT_SOURCES. PR pending.) |
| ico | 3 | hand-rolled + embedded png/bmp | in-progress (C2 bomb guard added to the embedded-PNG path: check_open was present (ROI 1<<30) but no ratio guard, so an ICO embedding a PNG that declares huge dims drove read_into_buffer into a multi-GB alloc; added check_compression_ratio. The DIB path is capped at 256x256 (no bomb possible). C1 check_open present on both paths; C4/C7 already-safe: DIB palette indices bounds-checked on every case, scanline indexing within slb, m_buf writes within image_bytes; PNG delegates to hardened png_pvt. Added H2 ico-wrapped-png bomb seed (testsuite/ico/src already wired). PR pending.) |
| softimage | 3 | hand-rolled | in-progress (C2 bomb guard added (via Filesystem::file_size; softimage uses FILE*, no ioproxy); C1 check_open present (ROI 65535, uint16 dims). Already well-hardened: channels() only yields indices 0-3 (safe m_channel_map/offset indexing), pure/mixed RLE clamp run counts to width and reject zero-length runs, uniform widest-depth storage with bounds-safe store_native writes within scanline_bytes, all fread checked. Added H2 bomb seed (testsuite/softimage/src already wired). PR pending.) |
| zfile | 3 | hand-rolled + zlib | in-progress (3 fixes. C1: the READER had NO check_open -- header.width/height are signed 16-bit and went straight into the ImageSpec, so a negative/zero dim reached read_native_scanline's gzread(data, width*sizeof(float)) where a wrapped-huge length -> OOB write; added check_open (ROI 32767). C5: gzread returns were unchecked on both the header and the pixel scanline, so a truncated/corrupt gzip returned short/-1 and the reader handed back an uninitialized scanline as valid; now both hard-error (pixel-affecting per FR-013). C2: added check_compression_ratio (zlib-compressed, consistent with png) to catch a gzip-bomb header. C3 valid_file bounded (fixed-size header). Added H2 bomb + H4 truncated seeds; wired testsuite/zfile/src into FORMAT_SOURCES. PR pending.) |
| ptex | 3 | Ptex lib | in-progress (C1/C2/C3/C8 already-satisfied by the hand-rolled 64-byte-header validator `ptex_validate_header` from PR #5265 (e3c14a073): magic/version/meshtype/datatype/nchannels/alphachan/nfaces/nlevels checks, all-blocks-fit-in-file check, and zip-ratio bomb guards on faceinfo/constdata/metadata -- run before PtexTexture::open(), which over-allocates on bogus counts. C5 APPLIED: null-checked the `getData(faceid,res)` and `getTile(tile)` returns in seek_subimage + read_native_tile (Ptex returns null once its reader enters an error state during lazy per-face reads -- a corrupt/truncated per-face block the header check can't catch -> previously a null-deref crash). Added 6 malformed-header fixtures (H1/H2/H8) + generator; run.py checks clean rejection; ptex suite green, valid file byte-identical. No shared-hazard writer analog. PR pending.) |
| ffmpeg | 4 | libavcodec (thin glue) | in-progress |
| r3d | 4 | RED SDK (thin glue) | in-progress (C5+C1+C4. DecodeVideoFrame status was ignored -> read_native_scanline copied out an undecoded buffer (uninitialized heap on the first frame) as a successful read; now hard-errors. Clip width/height/frame-count were unchecked and the ImageSpec narrowed them to int while the buffer used the untruncated size_t; now range-checked + check_open + check_compression_ratio, buffer sized from the validated spec. Failed InitializeSdk left open() using a torn-down SDK and double-finalized; now tracked. All open() failure paths were silent (no errorfmt). Scanline offset promoted to imagesize_t. NOT compile-verified locally -- SDK is proprietary, USE_R3DSDK=OFF; syntax-checked against stub headers. PR pending.) |
| openvdb | 4 | OpenVDB (thin glue) | in-progress |
| null | 4 | none | in-progress (C1 APPLIED. Reads no file, but parses the filename query args and validated none of them: CHANNELS=-1 sign-extended into a vector resize that threw an uncaught length_error and aborted the process; a huge CHANNELS hung building one ustring per channel; RES=-4x-4 / RES=0x0 returned a non-positive spec; negative TILE reached the tile fill loop. Channel count now bounded before name generation, negative tiles rejected, and the assembled spec passes check_open before anything is sized from it. C2-C10 N/A: no file bytes are ever read. Regression fixtures added to testsuite/null. PR pending.) |
| term | 4 | none (output-ish) | audited (N/A by construction: src/term.imageio/termoutput.cpp defines only TermOutput/ImageOutput -- no ImageInput, no term_input_imageio_create, no input extensions. There is no untrusted read path, so C1-C10 do not apply. No change needed.) |

(Inventory is authoritative for SC-007; update `status` as phases land.)

### Shared metadata decoders (Phase 7.5)

Not format plugins, but reachable from every container that embeds the blob, so
each is its own audit unit / PR and counts toward SC-007.

| Decoder | Reached from | Status |
|---------|--------------|--------|
| exif (`src/libOpenImageIO/exif.cpp`) | jpeg, png, tiff, heif, psd, raw | in-progress (3 defects fixed beyond the two in the GHSA draft: `tiff_data_size()`'s `size_t(-1)` unknown-type sentinel wrapped the bounds arithmetic -- reachable from the ordinary Exif 3.0 UTF-8 type 129, giving a heap OOB read and an aborting `std::length_error` on Release; unbounded IFD recursion depth, now capped at 32; and `OIIO_ALLOCA` with a file-controlled RATIONAL count, whose `assert` is compiled out under NDEBUG -- SIGSEGV on Release, now `OIIO_ALLOCATE_STACK_OR_HEAP`. 4 fixtures over the jpeg APP1 and png eXIf routes; ASan/UBSan clean. 2 deferrals. PR pending.) |
| xmp (`src/libOpenImageIO/xmp.cpp`) | jpeg, png, tiff, psd, heif | in-progress (3 defects fixed. C9: `decode_xmp_node()` recursed once per XML nesting level with nothing bounding the descent -- a 2.5 KB PNG SIGSEGVs a plain Release oiiotool; capped at 64 levels. C9: the `IsList`/`IsSeq` append path re-split, re-joined and re-interned the whole accumulated list per item, so cost was quadratic and the intermediates are permanent ustrings -- a 51.6 KB PNG cost 11.6 s and 3.2 GB; and `ImageSpec::attribute()`'s linear scan made a large attribute count quadratic too. Both now bounded by a per-call budget (4096 attributes / 1 MB of payload) that the existing 64 KB per-list guard did not cover, because it only applied when `isList` was true. C10 APPLIED: `encode_xmp` interpolated attribute values into XML with no escaping, so a value read from a hostile file could close its own quote and inject markup into every file written from it. C1/C2/C3 N/A (no dimensions, no allocation from a header, no `valid_file` in a metadata decoder); C4/C6/C7 N/A (`string_view` in, no offset arithmetic, no raw `ptr,len`); C5 already-satisfied (pugixml reports parse failure, deliberately tolerated); C8 unchanged and logged as a deferral. 4 fixtures over the png zTXt and jpeg APP1 routes; ASan/UBSan clean over 400k mutations. 3 deferrals. PR pending.) |
| icc (`src/libOpenImageIO/icc.cpp`) | jpeg, png, tiff, webp, heif | in-progress (3 UB defects fixed; the OOB surface was already closed -- header `< 128` rejected, `profile_size` must equal the buffer exactly, every tag/mluc-string range checked before read. C4/C9 APPLIED: (1) the range checks formed `iccdata.data() + tag.offset` (and `+ stroffset`) *before* comparing -- out-of-bounds pointer formation (UB), and on 32-bit the uint32 pointer math can wrap and let an OOB tag pass; replaced with a 64-bit `range_in_bounds()` that never builds the pointer until the range is valid. (2) every scalar (header, tag_count, each ICCTag, and the mluc nrecords/recordsize/language/country/len/stroffset fields) was read via `*(const T*)(ptr+offset)` at a file-controlled offset -- a misaligned load whenever a tag lands on an odd offset (UBSan `alignment` at icc.cpp:210, and a fault on strict-alignment CPUs). (3) the mluc UTF-16 value was copied through `(const char16_t*)start` -- misaligned when stroffset is odd. A single odd-offset mluc trips all three UBSan sites at once (verified by reverting on the address,undefined build). All reads now go through memcpy; byte values identical so valid profiles are unchanged. C1/C2/C3/C5/C10 N/A (metadata decoder: no dims, no header allocation, no valid_file, hand-rolled, writers embed the blob verbatim); C6 input already cspan; C7 already-satisfied; C8 already-satisfied at all callers (jpeg/png/tiff/webp/jpeg2000/psd gate ICC-decode failure behind imageinput:strict). 2 fixtures over the jpeg APP2 route (unaligned-mluc decodes to "Hi" post-fix; oversized-length rejected); ASan/UBSan clean; no deferrals. PR pending.) |

## Entity: Audit Checklist Item

The fixed set evaluated for EVERY format (FR-001..FR-008, FR-013). Each records an
outcome: `applied` / `already-satisfied` / `not-applicable (reason)`.

| ID | Check | Source FR | Helper / mechanism |
|----|-------|-----------|--------------------|
| C1 | Open-time extent limits (res/channels/depth) via `check_open()` with format-spec ROI | FR-001, FR-008 | `check_open(spec, ROI)` |
| C2 | Decompression-bomb guard | FR-002 | `check_compression_ratio(spec, filesize, max_ratio)` |
| C3 | Working, bounds-safe `valid_file` | FR-003 | signature inspection, no OOB |
| C4 | 64-bit overflow-safe size/offset arithmetic | FR-004 | `imagesize_t`/`int64_t`; validate factors first |
| C5 | Every underlying-library call checks return/error | FR-005 | per-call-site census; `errorfmt()`+`false` |
| C6 | pointer+length → span/cspan | FR-006 | `cspan<std::byte>`, `valid_raw_span_size()` |
| C7 | Declared regions validated against available bytes | FR-007 | offset/size ≤ filesize before read |
| C8 | Failure posture: metadata vs pixel | FR-013 | `imageinput:strict` for metadata; hard error for pixels |
| C9 | Opportunistic extras (unbounded loops, alignment, uninit reads, error-path leaks) | FR-012 | per-format judgment |
| C10 | Shared-hazard writer fix on read→write path | FR-014 | mirror reader overflow fix in writer |

## Entity: Hazard Class (test/corpus dimension)

Malformed-input categories each format's corpus must exercise (Edge Cases + SC-005).

| Class | Example malformed input |
|-------|-------------------------|
| H1 oversized-extent | width/height/channels/depth beyond format or policy cap |
| H2 decompression-bomb | tiny file declaring huge uncompressed size |
| H3 integer-overflow | factors in-range but product overflows 32-bit |
| H4 truncated | file ends mid-header / mid-scanline / mid-tile |
| H5 bad-offset | strip/tile/seek offset negative, unaligned, or past EOF |
| H6 corrupt-compressed | invalid RLE/LZW/DEFLATE/codec block for pixel data |
| H7 bad-metadata-blob | ICC/EXIF/palette length beyond its buffer |
| H8 nonpositive-structural | zero/negative width/height/channels/bit-depth |
| H9 recursive/looping | self-referential or looping offset structures |

## Entity: Deferral Log Entry (FR-015)

Persisted in `docs/dev/format-hardening-deferrals.md`.

| Field | Meaning |
|-------|---------|
| `date` | When spotted |
| `format` | Affected format |
| `location` | file:line or function |
| `kind` | writer-shared / writer-unique / reader-nonblocking / other |
| `description` | The hazard |
| `why_deferred` | Rationale for not fixing now |
| `severity_guess` | P0/P1/P2 per constitution triage |

## Entity: Audit Findings Summary (PR artifact)

Carried in each format's PR description (FR-011): the C1–C10 outcome table, the
hazard-class corpus additions, and links to any deferral-log entries created.

## Validation rules (cross-entity)

- A format's `status` becomes `audited` only when C1–C8 each have a recorded
  outcome and its PR includes ≥1 corpus/test addition per exploitable hazard class
  found (SC-002, SC-005).
- No `status: audited` may regress valid-file behavior (SC-003 — byte-identical).
- Effort complete ⇔ zero `pending`/`in-progress` in the inventory (SC-007).
