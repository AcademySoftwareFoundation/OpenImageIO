---
name: oiio-input-security-audit
description: Audit OpenImageIO file readers (ImageInput plugins and shared metadata decoders like exif/xmp/icc) for crash, memory-safety, hang, and decompression-bomb flaws on malformed input, then fix them in small reviewable commits. Use when asked to harden, audit, or fuzz-fix a format reader, triage a fuzzer crash or security report against a reader, or "do another audit like 002-format-security-audit".
---

# OIIO input security audit

Repeatable procedure for hardening `ImageInput` readers against hostile or
corrupt files. Distilled from the 2026 format-by-format audit (about 30
readers plus the shared exif/xmp/icc decoders) and the fuzz-driven fixes
around it. Keep this skill current: see "Feeding new lessons back" at the end.

Companion files (read on demand):

- `references/bug-catalog.md` - the bug classes to hunt, with grep recipes
  and real examples. Read this before the census.
- `references/format-notes.md` - what each format needed last time, and
  formats where a check is deliberately N/A. Read the row for your target.
- `docs/dev/fuzzing.md` - fuzz harness, corpus seeding, crash repro.
- `docs/dev/format-hardening-deferrals.md` - log of findings spotted but
  deliberately not fixed. Check it first (may already be known); append to it.

## Threat model and scope

- In scope: readers (`ImageInput`, `valid_file`, `open`, `seek_subimage`,
  `read_native_*`), the shared metadata decoders they call, and the glue to
  third-party decoders.
- Writers are mostly out of scope (callers of OIIO are trusted). Exception:
  a valid, readable input that drives a writer into the same unsafe behavior
  via `oiiotool in -o out` (e.g. same offset overflow) is fixed in the same
  pass. Anything else writer-side: log to the deferral doc.
- Glue-check posture for external libraries (libwebp, libheif, libjxl,
  DCMTK, FFmpeg, LibRaw, OpenVDB, R3D SDK): check every return, bound the
  header-derived geometry and the buffers OIIO owns. Do NOT hunt inside the
  dependency; log deep library issues to the deferral doc.
- Material finding = can cause crash, hang, memory-safety violation,
  uninitialized-data disclosure, or incorrect decode on some input. Fix those.
  Cosmetic or style items: leave alone.

## Choosing the target(s)

The skill takes a target spec from the user. Resolve it first:

- **One format**: `audit tiff`. Plugin dir is `src/<fmt>.imageio/`. Also
  includes that format's embedded-format paths (ico -> png/bmp).
- **A list**: `audit png jpeg exif`. Treat as independent units, in the order
  given. If unordered, ask the maintainer for priority.
- **All**: enumerate with `ls -d src/*.imageio` plus the shared decoders
  (`exif.cpp`, `xmp.cpp`, `icc.cpp` in `src/libOpenImageIO/`). Skip `term`
  (output only). Priority is the maintainer's call (user impact, not reader
  class; classes in `references/format-notes.md` describe architecture and
  audit method only): ask, or take an explicit list. For a re-audit, favor
  formats whose reader changed since the last audit:
  `git log --since=<date> -- src/<fmt>.imageio`, and formats added
  after the audit (not in format-notes.md at all: audit those first, full
  checklist).
- **A shared decoder** (exif/xmp/icc): its own unit; see Delivery.
- **A bug class instead of a format** ("sweep for misaligned loads"): run that
  one bug-catalog entry's grep recipe across all readers rather than the
  full per-format procedure.

For multiple units: run the preflight for all, then one unit at a time through
steps 2-8, each ending in its own commit(s). Do not interleave edits across
formats. For "all", farm "already-hardened, just verify/add seed" units to
subagents per "Division of labor", keep the strongest-model hunt for
unfamiliar or complex parsers, and finish with a summary table (unit x
C1-C10 outcome) plus any new deferral rows. Update format-notes.md rows for
every unit touched.

## Design rules (decided, do not relitigate)

1. Two limit layers. The `ROI` passed to `check_open()` is the FORMAT's own
   spec maximum (e.g. 65535 for uint16 fields). The global `limits:*`
   attributes (`limits:channels`, `limits:resolution`, `limits:imagesize_MB`)
   are user policy and are enforced by `check_open()` itself. Never add a new
   `limits:*` attribute without asking the maintainer.
2. Failure posture by corruption class:
   - Metadata-only defect (bad ICC/EXIF/XMP/IPTC/palette, skippable ancillary
     data whose loss cannot change a decoded pixel): `imageinput:strict` set
     = hard error; otherwise drop the item and keep reading.
   - Pixel-affecting defect (bad RLE/LZW/scanline/tile, short pixel read,
     inconsistent offsets, anything that could change a pixel value, position,
     or the pixel count): ALWAYS a hard error. Never emit a partial or
     uninitialized scanline/tile as success.
3. Valid in-spec files must read byte-identically after the change. API and
   ABI unchanged. Be conservative about new rejections of legitimate big files.
4. Error reporting: `errorfmt()` + `return false`. No exceptions, no printf.
   Release resources on every early-return path. When rejecting on a policy
   limit, say which `limits:*` attribute to raise.
5. Code comments: 0-2 lines, no bug-history narrative. Rationale goes in the
   commit message. Do not explain `check_open`/`check_compression_ratio` at
   call sites. Do not edit `CHANGES.md` (filled at release; breaks backports).
6. `Strutil::format`, not `Strutil::fmt::format`, in new code. Follow the
   rest of `AGENTS.md` (span over ptr+len, 80 cols, clang-format).

## The checklist (record an outcome for each: applied / already-satisfied / N-A + reason)

| ID | Check |
|----|-------|
| C1 | `check_open(spec, ROI)` near top of `open()`/`seek_subimage()`, after dims are known and BEFORE any allocation or decode. Reject non-positive width/height/channels/depth/bitdepth/tile sizes. Also must run before expensive library calls (e.g. before `jpeg_start_decompress`, `opj_decode`, `heif_decode_image`). |
| C2 | `check_compression_ratio(spec, filesize)` once declared bytes and file size are known, before decode/allocation. N/A with reason for sparse formats (OpenVDB), formats with legitimately extreme ratios (JXL, one-frame movies - FFmpeg keeps the 1 GB floor), or where the library owns the limits (HEIF, DICOM). Pick the smallest `max_ratio` with no false positive on genuine files. |
| C3 | `valid_file()` exists, reads a bounded prefix, never reads past what it has, and returns false on short/garbage input. |
| C4 | All size/offset/index math in 64-bit (`imagesize_t`, `int64_t`, `size_t`), factors validated BEFORE multiplying. Includes writer-side analogs. |
| C5 | Every call into a library or IO layer has its return/null/short-read checked; failure becomes `errorfmt()`+`false` with cleanup. |
| C6 | ptr+len pairs become `span`/`cspan`; `valid_raw_span_size()` for decode targets. Do not churn already-safe signatures. |
| C7 | Every declared region (blob, scanline, tile, strip, chunk, palette) is checked against bytes actually available; every internal offset/seek target checked for negative, misaligned, beyond-EOF. |
| C8 | Failure posture wired per rule 2 above. |
| C9 | Opportunistic: unbounded loops/recursion, alignment, uninitialized reads, error-path leaks, stale state after failed seek. |
| C10 | Read-to-write hazard (see scope). |

## Procedure

1. **Preflight.** Read the deferral log rows for the target. Read the target
   row in `references/format-notes.md`. Check open PRs/issues for the same
   plugin (some audits were gated on in-review PRs; do not collide).
   Use a branch per concern (see "Delivery").
2. **Census, targeted not wholesale.** Do not read a 40-80 KB reader end to
   end. Use the grep recipes in `references/bug-catalog.md` to list: header
   fields feeding allocation/loops/offsets; every library call; every ptr+len
   hand-off; every metadata blob parse; every `read`/`memcpy`/`resize`/
   `reserve`/`alloca`. Then read only those functions.
3. **Hunt.** Work through the bug-catalog classes against the census. The
   subtle bugs are in the intermediate math and in "already checked" paths:
   do not accept "already hardened" without reading the arithmetic.
4. **Reproduce before fixing** when feasible: craft a minimal malformed file
   (patch a valid file's header, truncate, hand-write an ASCII header) and
   confirm the failure under ASan/UBSan. Skip only when the dependency is
   unavailable (record "verified by inspection" honestly).
5. **Fix.** One atomic, single-concern commit per concern per format.
6. **Cover.** Add a regression fixture per hazard class (H1-H9 below) that was
   unguarded. Put it in `testsuite/<fmt>/src` (or a dedicated dir), add a line
   to the format's `run.py`, update `ref/out.txt`, and make sure the dir is in
   `FORMAT_SOURCES` in `src/fuzz/populate_corpora.py`. Do NOT also copy the
   file into `src/fuzz/corpora/<fmt>/` (that holds only synthetic seeds with no
   testsuite home). A new dedicated test dir needs registering in
   `src/cmake/testing.cmake`.
7. **Verify.** See "Build and verify". Then clang-format.
8. **Record.** Outcome table (C1-C10), fixtures added, deferrals logged.

### Hazard classes for fixtures

H1 oversized extent; H2 decompression bomb; H3 integer-overflow product;
H4 truncated (mid-header, mid-scanline, mid-tile are distinct); H5 bad
offset (negative, misaligned, past EOF); H6 corrupt compressed block;
H7 bad metadata blob length; H8 non-positive structural value;
H9 recursive or looping structure.

Fixture gotchas:

- `check_open`'s default ROI caps dims at 65535, so a bomb fixture must stay
  at or under 65535 per dimension and declare over 1 GB to reach the ratio
  guard (the ratio guard's floor is `min_declared_bytes` = 1 GB). Larger dims
  just hit the ROI check, which is a different test.
- Bomb fixture recipe: patch a valid file's header dims (or write an ASCII
  header), truncate to header size.
- Bombs are mostly uncraftable past a library's own validation (libwebp
  demux, libheif box parse, DCMTK). Verify those guards by hand (e.g. lower
  `limits:channels`) and say so.
- Some `run.py` files do not capture stderr; set `redirect=' >> out.txt 2>&1 '`
  or use `-nostderr`, else the error never reaches `out.txt`.
- `runtest.py` pins `limits:imagesize_MB=32768` so bomb tests exercise the
  format ratio check, not the memory-dependent size limit.

## Build and verify

- Reconfigure from inside `build/` (`cd repo && cmake .` triggers an
  in-source build error). Any time you switch branches, start with
  `cmake --fresh` to clear the cache and build artifacts.
- Sanitizer build: `-DSANITIZE=address,undefined`. Run ctest with
  `ctest -j3 --timeout 900`; `-j6` mass-times-out. Never run ctest while a
  rebuild is in flight. Run ctest outside the Claude sandbox (sandboxed OMP
  /tmp warnings pollute `out.txt` and fail text-diff tests).
- Known unrelated local failures vary by machine (python bindings mismatch,
  ffmpeg version, filter-kernel hashes). Compare against a baseline run
  before blaming your change.
- AppleClang cannot build the libFuzzer target; the fuzzer is run on GitHub
  Actions (`.github/workflows/fuzz.yml`, nightly or on demand). Locally:
  testsuite green plus valid-file byte-identity. Do not stand up a local
  libFuzzer build on macOS unless upstream LLVM is installed.
- Proprietary-SDK readers (R3D) are not compiled in normal builds: syntax-check
  against stub headers and say that it was not compile-verified.

## Delivery

- Atomic commits: one concern per commit per format, so they can be
  cherry-picked by theme later. Near-identical cross-format guards
  (`check_open` + `check_compression_ratio` adds) and simple overflow fixes
  MAY be batched into one shared commit/PR; extensive or format-unique work
  stays its own commit/PR.
- A defect outside the target's reader scope (shared helper, other format,
  writer beyond C10) is NOT folded in: split it into its own commit/PR, or
  log it in the deferral doc.
- Commit subject: `fix(<fmt>): ...` using the repo type prefixes. Terse; a
  subject-only message is fine for simple guards. Body only for non-obvious
  why (state the mechanism, not a specific attack file). Trailer is only
  `Assisted-by: <tool> / <model>`; never Co-Authored-By or session IDs.
- Never push, open PRs, or offer to; the user does that.
- Shared decoders (exif/xmp/icc) are their own audit units: a bug there
  affects every container, so fix in the decoder, with fixtures through each
  container route (jpeg APP1/APP2, png eXIf/zTXt, ...), not in a plugin.
- External security reports: reproduce on current HEAD first; many are already
  fixed upstream. Only the live remainder needs a patch. Keep reporter evidence
  files out of the tree.

## Division of labor (token economy)

- The census and bug hunt (finding subtle intermediate-overflow, sentinel,
  and ignored-return bugs) needs the strongest model; weaker passes tend to
  report "already hardened".
- Mechanical tails (apply a decided fix, make the fixture, wire
  `FORMAT_SOURCES`, build, regenerate refs, commit) and whole
  "already-hardened, just add a bomb seed" formats can go to a cheaper
  subagent. Delegate early with a light spec; the benefit is keeping verbose
  build/ref-diff output out of the parent context, not fewer total tokens.
- Subagents must not switch branches in the shared worktree.

## Feeding new lessons back

When an audit, fuzz crash, or security report reveals a flaw class not in
`references/bug-catalog.md`, or a mistake in this procedure:

1. Add a catalog entry: the pattern, a grep recipe if one exists, the fix
   shape, one real example (commit/PR). Keep entries short.
2. If it implies a new check, add it to the C-list above (renumber nothing;
   append C11, ...) and a hazard class if a new fixture type is needed.
3. Update the format's row in `references/format-notes.md`.
4. Log unfixed findings in `docs/dev/format-hardening-deferrals.md`.
5. After fixing a flaw class in one place, grep the other readers for the same
   pattern before closing the task. Cross-format sweeps found real bugs
   repeatedly (e.g. subimage-rejection state leak found in 8 readers after the
   TIFF fix).
