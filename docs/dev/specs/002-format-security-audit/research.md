# Phase 0 Research: Image Format Security & Stability Audit

All spec clarifications were resolved in the `/speckit-clarify` session (failure
posture, limits layering, writer scope, format coverage). No open NEEDS
CLARIFICATION remain. This document records the technical decisions the per-format
audit relies on.

## Decision 1: Reuse existing ImageInput hardening helpers (do not build new machinery)

**Decision**: Base every format's hardening on the helpers already present on
`ImageInput` (declared in `src/include/OpenImageIO/imageio.h`, implemented in
`src/libOpenImageIO/imageinput.cpp`):

- `bool check_open(const ImageSpec& spec, ROI range = {0,65535,0,65535,0,1,0,4}, uint64_t flags = 0)`
  — enforces the ROI (format-spec extents) AND the global `limits:channels`
  (default 1024), `limits:resolution` (default 1048576 per single dimension),
  and `limits:imagesize_MB` (default 32768) policy limits. Returns false + errorfmt
  on violation.
- `bool check_compression_ratio(imagesize_t declared_bytes, imagesize_t filesize, imagesize_t max_ratio = 10000, imagesize_t min_declared_bytes = 1<<30)`
  and the `(const ImageSpec&, filesize, ...)` overload — decompression-bomb guard.
- `bool valid_raw_span_size(cspan<std::byte> buf, const ImageSpec& spec, xbegin, xend, ybegin, yend, zbegin, zend, chbegin, chend)`
  — validates a decoded slab fits the destination span (bounds-checked read target).

**Rationale**: These are the intended, documented mechanisms (spec Assumptions).
They already carry the correct 64-bit arithmetic (`imagesize_t`), the correct
error-reporting convention (`errorfmt()` + `false`), and user-tunable policy via
`limits:*`. Reusing them keeps behavior consistent across formats and avoids
ABI churn.

**Alternatives considered**: Per-format ad-hoc bounds checks (rejected: inconsistent,
error-prone, duplicates logic); a new central "sanitize spec" pass (rejected:
formats need distinct spec-derived extents; `check_open`'s ROI already expresses that).

## Decision 2: ROI = format-spec limits; `limits:*` = application policy

**Decision**: The `ROI range` passed to `check_open()` encodes each format's own
specification maxima (e.g., a format that stores dimensions in a uint16 field caps
at 65535; SGI/RLA/PIC-style caps; TIFF/EXR effectively 32-bit). The global
`limits:*` attributes stay as the user-tunable *policy* layer and are NOT
format-specific.

**Rationale**: Clarification session — format limits are intrinsic to the file
format; policy limits reflect the deploying application's risk tolerance and the
legitimate files it expects. Conflating them would either reject valid large
files or weaken protection.

**Action**: New global `limits:*` attributes are added ONLY when a genuinely new
dimension needs policy control, and ONLY after maintainer (Larry) sign-off. Default
to expressing format caps through the ROI argument.

## Decision 3: Failure posture — metadata vs pixel corruption

**Decision**: Two rules, keyed on whether corruption affects pixels:

1. **Metadata-only** defect (malformed/oversized ICC profile, EXIF/IPTC/XMP block,
   or other skippable ancillary chunk): consult the global `imageinput:strict`
   attribute. Strict → hard error. Non-strict → discard the offending item, drop
   it from the resulting ImageSpec, continue the read. Emit a warning where the
   subsystem supports it.
2. **Pixel-affecting** defect (bad RLE/LZW/scanline/tile decode, short pixel read,
   inconsistent strip/tile offset): ALWAYS a hard error regardless of `strict`.
   Never skip or partially emit a corrupt scanline/tile.

**Rationale**: Directly from the clarification. Metadata recovery is where most
memory-safety bugs hide but also where many readers legitimately tolerate junk;
`strict` gives callers the choice. Pixel integrity is non-negotiable — silently
returning wrong pixels is worse than failing.

**Implementation note**: Look up `imageinput:strict` via the global attribute
(`OIIO::get_int_attribute("imageinput:strict")`) consistent with existing usages;
grep shows the attribute is already consulted in several plugins.

## Decision 4: Integer-overflow discipline

**Decision**: Any product of dimensions / channels / bytes-per-channel, and any
byte offset into the file or a buffer, is computed at 64-bit width
(`imagesize_t` / `int64_t` / `size_t`) BEFORE being used for allocation, seek, or
indexing. Where a format header supplies values that feed such products, validate
each factor against its format cap first, then compute the product in 64-bit and
re-check against `limits:imagesize_MB` (via `check_open`) and against the actual
file size (via `check_compression_ratio`).

**Rationale**: 32-bit intermediates are the classic OIIO decode-bomb / OOB vector
(cf. recent fixes in `raw`, `rla`, `jpeg2000`, and the integer-channel-to-EXR fix
on this branch). `imagesize_t` is already the project's 64-bit size type.

**Alternatives considered**: Rely on `check_open` alone (rejected: it validates the
final spec, but overflow can occur in intermediate offset math the spec never sees,
e.g., per-tile/per-strip byte offsets).

## Decision 5: Prior art to pattern-match

**Decision**: Use the already-hardened plugins as the reference implementation
pattern for each audit:

- `raw` (`src/raw.imageio/rawinput.cpp`) — `check_open` + `limits:*` + libraw
  per-dimension cap; decode-bomb/corrupt-header rejection before unpack (commit
  b112832ff).
- `rla` (`src/rla.imageio/`) — corrupted RLE detection (commit 6b09bf28b).
- `jpeg2000` — oversized/decompression-bomb header guard before decode (commit
  327047e28).
- The `src/fuzz/` harness + per-format seed corpora and `docs/dev/fuzzing.md`
  (commit 4fc2cd5a9).

**Rationale**: Consistency; these encode the reviewed-and-merged conventions for
error messages, limit wording ("If you're sure this is a valid file, raise the
OIIO global attribute ..."), and test placement.

## Decision 6: Underlying-library error handling

**Decision**: For each third-party call reachable from untrusted data, check the
documented failure signal (return code, negative/short read, null handle, error
callback, `setjmp`/error-manager for libjpeg/libpng) and convert to `errorfmt()` +
`false`, releasing any acquired resources on the error path (no leaks). Enumerate
these call sites per format during its audit.

**Rationale**: FR-005. Libraries signal errors heterogeneously; a per-format
call-site census is the only reliable way to catch ignored returns.

## Decision 7: pointer+length → span/cspan

**Decision**: Where a plugin passes raw `ptr,len` (or an implied length) for
buffers — scanline/tile decode targets, metadata blob parsing, header field
extraction — migrate to `span`/`cspan` (`cspan<std::byte>` for untyped bytes),
using `valid_raw_span_size()` for decode targets. Do not churn signatures that are
already bounds-safe.

**Rationale**: FR-006; project guidelines mandate span over pointer+length. Makes
lengths explicit and enables bounds assertions at the call site.

## Decision 8: Format audit ordering (risk × prevalence)

**Decision**: Recommended phase ordering (planning guidance, not binding — final
call is the maintainer's):

1. **Tier 1 — high attack surface, widely received**: tiff, jpeg, png, openexr,
   psd, targa, bmp, gif, dds.
2. **Tier 2 — complex/compressed, less ubiquitous**: jpeg2000, webp, heif, jpegxl,
   raw (already largely done — confirm), dpx, cineon, sgi, rla (RLE done — confirm),
   iff, dicom.
3. **Tier 3 — simpler or niche in-tree parsers**: pnm, hdr, fits, ico, softimage,
   zfile, ptex.
4. **Tier 4 — external-heavy or trivial**: ffmpeg, r3d, openvdb (delegate heavily to
   libs — audit the glue), null, term (no untrusted decode — quick confirm).

**Rationale**: Front-load the decoders most likely to receive hostile input in
production pipelines and with the largest hand-rolled parsing surface.

## Decision 9: Writer shared-hazard scope + deferral log

**Decision**: Within a format's phase, fix writer defects triggerable by a valid
input through `oiiotool infile -o outfile` when they cause unsafe behavior beyond a
clean write failure (canonical case: the writer shares the reader's integer-overflow
in pixel-offset math). Record every deferred writer/other finding in a new tracking
doc `docs/dev/format-hardening-deferrals.md`.

**Rationale**: FR-014 / FR-015. Keeps the read→write path safe without expanding
each PR into a full writer audit, while guaranteeing deferred issues aren't lost.

## Open items deferred to planning/per-phase (not blocking)

- Exact numeric ROI caps per format — determined during each format's phase from
  its spec.
- Exact `max_ratio` per format for `check_compression_ratio` — pick the smallest
  value with no false positives on that format's genuine corpus (default 10000).
- Final phase ordering — maintainer's call at execution time.
