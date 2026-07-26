# Feature Specification: Image Format Security & Stability Audit

**Feature Branch**: `002-format-security-audit`

**Created**: 2026-07-23

**Status**: Draft

**Input**: User description: "Comprehensive image file format-by-format audit of problems that could affect stability or security. Proceed one image format at a time, each format a separate phase / separate PR. Per format: ensure a check_open that enforces format-based and common-sense limits (resolution, etc.); use check_compression_ratio() to guard against decompression bombs; ensure a working valid_file method; check for 32-bit integer overflows in dimension/size/offset arithmetic; verify all calls into underlying image libraries check return/error conditions and handle them; replace pointer+length (or implied-length) parameters with span/cspan for explicit bounds. Watch for other reader-hardening opportunities."

## Clarifications

### Session 2026-07-23

- Q: Failure posture on malformed-but-partially-decodable input → A: Case-by-case, keyed on whether the corruption affects pixels. Metadata-only items (e.g., a malformed ICC profile or EXIF block) are governed by the global `imageinput:strict` attribute: hard error when strict, silently discarded (item dropped, read continues) when non-strict. Anything indicating pixel data is wrong (e.g., a bad RLE decode block for a scanline) is ALWAYS a hard error regardless of `strict` — a bad scanline/tile is never skipped the way a bad ICC profile can be.
- Q: How resolution/size limits are configured → A: Two distinct layers. Values passed into `check_open()` (image ROI / extents) reflect **format limits** derived from each format's own specification. The global `limits:*` attributes (e.g., `limits:channels`, `limits:imagesize_MB`) represent **application policy** limits that users tune for their risk tolerance, security posture, and expected legitimate files. Reuse the existing global `limits:*` mechanism; introduce any NEW global limit attribute only when genuinely necessary and only after discussing the specifics with the maintainer first.
- Q: Format coverage scope / completion boundary → A: All reader-bearing formats are in scope, one independently-shippable phase each, ordered by attack surface and prevalence. The overall effort is "done" only when every reachable decoder has been audited (not limited to a fixed high-risk subset or only formats with known fuzzing hits).
- Q: Are writers in scope? → A: Mostly out of scope (programs using OIIO APIs are trusted; we do not defend against maliciously hand-crafted outputs). BUT the `oiiotool infile -o outfile` path matters: a readable, technically-valid input can carry a property that makes a writer misbehave (beyond merely failing to write) — e.g., the same integer-overflow in pixel offsets the reader has. Fix such obvious shared-hazard writer bugs in the same format phase. Defer more unusual/unique writer issues, but record every deferred writer finding in a tracking document so it is not lost.

## User Scenarios & Testing *(mandatory)*

<!--
  The unit of independent delivery here is a single image format. Each format
  is audited and hardened in its own phase and shipped as its own PR. Any one
  format's hardening is a viable, self-contained slice of value.
-->

### User Story 1 - Hostile input cannot crash or corrupt memory in a single hardened format (Priority: P1)

An application (or the OIIO tools such as `oiiotool`, `iinfo`, `iconvert`) opens an untrusted, possibly malicious or corrupt image file of a given format. Instead of crashing, over-allocating, reading/writing out of bounds, or hanging, the reader detects the malformed input, reports a clear error, and returns cleanly.

**Why this priority**: Memory safety and crash resistance against untrusted input is the core security goal. A single fully-hardened format delivers real, shippable protection on its own and establishes the pattern every later format follows.

**Independent Test**: Take one format, feed it a corpus of malformed/adversarial files (oversized dimensions, decompression bombs, truncated data, integer-overflowing headers) via the existing fuzz harness and testsuite, and confirm every case ends in a graceful error rather than a crash, hang, or sanitizer report — with no regression on valid files.

**Acceptance Scenarios**:

1. **Given** a file whose header declares dimensions or channel counts far beyond any plausible image, **When** the format's reader opens it, **Then** the open is rejected with a clear error and no large allocation is attempted.
2. **Given** a small file whose header claims it decompresses to an enormous image (decompression bomb), **When** the reader opens it, **Then** the implied compression ratio is checked and the file is rejected before decoding.
3. **Given** a file whose dimensions multiply to a value exceeding 32-bit range, **When** the reader computes buffer sizes and offsets, **Then** all arithmetic is performed at 64-bit width and either succeeds correctly or is rejected — never silently overflows.
4. **Given** a truncated or structurally invalid file, **When** any underlying library call fails, **Then** the failure is detected, propagated as an error, and does not leave the reader in an inconsistent state.
5. **Given** a byte-identical set of valid, in-spec image files, **When** the hardened reader opens them, **Then** they still open and decode identically to before the change (no regression).

---

### User Story 2 - Format hardening lands as small, reviewable, independent PRs (Priority: P2)

A maintainer reviews the audit work one format at a time. Each format's audit, fixes, tests, and notes arrive as a self-contained change that can be understood, reviewed, and merged (or reverted) without depending on other formats' work.

**Why this priority**: Keeping each format isolated makes the review tractable, limits blast radius, and lets protection ship incrementally rather than in one giant risky change.

**Independent Test**: Confirm each format phase produces a branch/PR touching essentially only that format's plugin plus its tests/corpus, builds and passes the testsuite on its own, and carries a short audit summary of findings and fixes.

**Acceptance Scenarios**:

1. **Given** the audit of one format is complete, **When** its PR is opened, **Then** the diff is scoped to that format's plugin and its tests, and the PR description enumerates which hardening checks were applied, which found issues, and which were already satisfied.
2. **Given** one format's PR, **When** it is merged or reverted, **Then** no other format's behavior is affected.

---

### User Story 3 - Every reader has a reliable valid_file detector (Priority: P3)

A caller relies on OIIO to identify a file's format by content (not just extension). Each audited format provides a working `valid_file` check that cheaply and safely inspects the file's signature/structure without being fooled by, or crashing on, malformed data.

**Why this priority**: Correct, safe format detection prevents mis-dispatch and is itself an early line of defense, but it depends on and complements the deeper open-time guards from Story 1.

**Independent Test**: For each audited format, verify `valid_file` returns true for genuine files, false for non-matching or corrupt files, and never crashes or reads out of bounds on malformed input.

**Acceptance Scenarios**:

1. **Given** a genuine file of the format, **When** `valid_file` is called, **Then** it returns true.
2. **Given** a truncated, empty, or wrong-format file, **When** `valid_file` is called, **Then** it returns false without crashing or reading past the available bytes.

---

### Edge Cases

- Header declares zero or negative width, height, channels, or depth.
- Dimensions individually in-range but their product (or product times bytes-per-pixel/channel) overflows 32-bit and/or 64-bit arithmetic.
- Declared data/scanline/tile/strip sizes exceed the actual file length.
- Compression ratio implied by header is physically impossible for the file's byte size (decompression bomb).
- File is truncated mid-header, mid-scanline, or mid-tile.
- Underlying library call returns an error, null, or short read that current code ignores.
- Metadata/EXIF/ICC/palette blobs claim lengths beyond the buffer they live in.
- Offsets or seek targets inside the file are negative, unaligned, or beyond EOF.
- Recursive or self-referential structures (e.g., nested/looping offsets) that could cause unbounded work.
- A valid but genuinely large production image must NOT be rejected by the new limits.

## Requirements *(mandatory)*

### Functional Requirements

Requirements below are applied per format, once per audited format phase.

- **FR-001**: Each audited format's reader MUST enforce open-time limits (via a `check_open`-style guard) on declared resolution, channel count, depth, and other format-specific extents, rejecting values that exceed limits before any large allocation or decode. Two distinct layers apply: (a) the extents/ROI passed into `check_open()` reflect **format limits** taken from the format's own specification; (b) the global `limits:*` attributes (`limits:channels`, `limits:imagesize_MB`, etc.) express **application policy** the user tunes for their risk tolerance. New global `limits:*` attributes are added only when genuinely necessary and only after maintainer discussion.
- **FR-002**: Each audited format's reader MUST guard against decompression bombs by using `check_compression_ratio()` (or an equivalent check) to reject inputs whose header-implied uncompressed size is impossibly large relative to the actual file size.
- **FR-003**: Each audited format MUST provide a working `valid_file` implementation that safely inspects file content and does not crash or read out of bounds on malformed input.
- **FR-004**: Each audited format's reader MUST perform all dimension, buffer-size, and offset arithmetic at a width sufficient to avoid 32-bit integer overflow (i.e., promote to 64-bit where intermediate or final products can exceed 32-bit range), and MUST reject inputs whose computed sizes/offsets cannot be represented or exceed limits.
- **FR-005**: Each audited format's reader MUST check the return value / error condition of every call into an underlying image or compression library and handle failures by reporting an error and returning cleanly, without proceeding on unchecked results.
- **FR-006**: Wherever a format's reader passes buffers as raw pointer + separate length, or with an implied (unpassed) length, the audit MUST, where practical, convert these to `span`/`cspan` (`span<std::byte>`/`cspan<std::byte>` for untyped data) so lengths are explicit and bounds are checked.
- **FR-007**: Each audited format's reader MUST validate that declared data regions (scanlines, tiles, strips, metadata/palette/EXIF blobs) fall within the actual available bytes before reading them, AND MUST reject any internal offset or seek target that is negative, misaligned for its data type, or beyond EOF before using it.
- **FR-008**: Each audited format MUST reject non-positive or nonsensical structural values (zero/negative width, height, channels, depth, bit depth, and tile/strip/block dimensions) at open time.
- **FR-009**: The hardening for each format MUST preserve API, ABI, and existing behavior for valid, in-spec files — no regression in what successfully reads today (barring genuinely malformed inputs that should now be rejected).
- **FR-010**: Each audited format MUST add or extend regression/fuzz coverage (malformed-input test cases and/or seed corpus entries) demonstrating that the identified hazards are now handled gracefully.
- **FR-011**: The audit work MUST be delivered as narrowly-scoped, independently reviewable changes, each accompanied by a summary of which checks were applied, which surfaced issues, and which were already satisfied. The cross-cutting guards that are nearly identical across formats — the `check_open()` / `check_compression_ratio()` additions (FR-001, FR-002) and the simple integer-overflow fixes (FR-004) — MAY be batched into shared cross-format commits so a reviewer can examine them together; more extensive, format-specific hardening remains its own per-format commit. (An earlier revision of this spec required exactly one format per phase/PR; that is deliberately relaxed here to cut PR count and reviewer burden, without changing the per-format audit *scope* of the work itself.)
- **FR-013**: Each audited format's reader MUST apply failure posture by corruption class: (a) **metadata-only** defects (malformed/oversized ICC profile, EXIF/IPTC/XMP block, or similar skippable ancillary data) MUST be a hard error when the global `imageinput:strict` attribute is set, and MUST otherwise be discarded (the offending item dropped, the read continuing) when non-strict; (b) any defect indicating the **pixel data itself is wrong** (e.g., a bad RLE/scanline/tile decode) MUST ALWAYS be a hard error regardless of `strict` — a corrupt scanline or tile is never skipped or partially emitted. Decision rule distinguishing the two classes: a defect is **pixel-affecting** if ignoring or working around it could change any decoded pixel value or its position (or the count/geometry of pixels produced); otherwise, if the affected data is ancillary and its loss cannot alter decoded pixels, it is **metadata-only**.
- **FR-012**: The audit MUST remain alert to additional reader-hardening opportunities beyond the enumerated checks (e.g., unbounded loops, alignment assumptions, uninitialized reads, error-path resource leaks) and address material findings within the same format phase. A finding is **material** if it can cause a crash, hang, memory-safety violation, or incorrect decode on some input; cosmetic or stylistic issues are not material and may be left alone or deferred.
- **FR-014**: Although writers are primarily out of scope (callers of OIIO are trusted; the audit does not defend against maliciously hand-authored outputs), each format phase MUST fix writer defects that a **readable, technically-valid input** can trigger through the read→write path (e.g., `oiiotool infile -o outfile`) when the defect causes unsafe behavior beyond a clean write failure — most obviously a writer sharing the same hazard as its reader (such as integer overflow in pixel offsets). Such obvious shared-hazard writer bugs are fixed in the same phase as the corresponding reader. A writer defect qualifies as an **obvious shared hazard** when it is the direct analog of a defect found in that format's reader during the same phase — the same unsafe computation or pattern (e.g., the identical offset/size arithmetic) appearing on the write side. Writer defects that are not such direct analogs are treated as unusual/unique and deferred per FR-015.
- **FR-015**: Any writer-side (or other) hardening issue that is spotted but deliberately deferred MUST be recorded in a persistent deferral/tracking document so it is not forgotten and can be scheduled later.
- **FR-016**: When a format audit uncovers a defect that is NOT part of that format's reader-hardening scope — an unrelated bug in shared/helper code, in a different format, or in the writer beyond the shared-hazard rule of FR-014 — the fix MUST NOT be folded into the format's hardening PR. It MUST instead be either (a) split into its own separate commit/PR, or (b) if not fixed now, logged to the deferral document (FR-015). The format's own change stays scoped to that format's reader plus its tests/corpus (per FR-011). (A shared cross-format checks/overflow commit per FR-011 is not such an unrelated defect — it is the deliberate batching of the FR-001/FR-002/FR-004 guards, not folded-in scope creep.)

### Key Entities

- **Image format plugin**: A per-format reader/writer under `src/<FORMAT>.imageio/`; the unit of a single audit phase.
- **Malformed-input corpus**: Adversarial and corrupt sample files (fuzz seeds + testsuite fixtures) exercising each hazard class for a format.
- **Audit findings summary**: The per-format record of which checks were applied, what was found, and what was fixed, carried in the PR.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every audited format, no malformed or adversarial input in the test/fuzz corpus produces a crash, hang, out-of-bounds access, or sanitizer (ASan/UBSan) report — all such inputs terminate in a clean, reported error.
- **SC-002**: 100% of the mandatory checks are explicitly evaluated for each audited format — the six core checks (open-time limits, compression-ratio guard, valid_file, overflow-safe arithmetic, library-error handling, span-based bounds) PLUS region/offset bounds (FR-007), non-positive-value rejection (FR-008), and failure-posture (FR-013) — with the outcome (applied / already-satisfied / not-applicable-with-reason) recorded (see the C1–C10 checklist in data-model.md).
- **SC-003**: Zero regressions on valid, in-spec files: every previously-readable conformant image in the testsuite still reads with identical results after a format's hardening.
- **SC-004**: Each format's hardening ships as its own reviewable PR scoped to that format plus its tests.
- **SC-005**: Each audited format has at least one added malformed-input test or fuzz-seed case per hazard class that was found to be exploitable or unguarded before the change.
- **SC-006**: No declared-oversized or decompression-bomb input causes an allocation disproportionate to the actual file size (allocation is bounded by validated, file-supported extents).
- **SC-007**: Every reader-bearing format plugin has a completed audit phase; a checklist of all in-scope formats tracks each as pending or audited, and the effort is complete only when none remain pending.

## Assumptions

- Scope is primarily the image **readers** (ImageInput side and format detection). Writers are mostly out of scope — OIIO callers are trusted and the audit does not defend against maliciously hand-authored outputs — EXCEPT that obvious shared-hazard writer bugs reachable from a valid input via the read→write path (`oiiotool infile -o outfile`) are fixed in the same phase (FR-014), and any deferred writer issue is logged in a tracking document (FR-015).
- All reader-bearing format plugins are in scope; the effort completes only when every reachable decoder has been audited. Ordering is by risk and prevalence (largest attack surface / most widely-received untrusted formats first); exact ordering is a planning decision, not fixed by this spec.
- "Common-sense" resolution/channel/depth limits use existing OIIO conventions and any format-specification maxima; limits are chosen not to reject legitimate large production imagery, and remain adjustable through existing configuration/attribute mechanisms where such exist.
- The existing helpers `check_open()` and `check_compression_ratio()` (and the fuzz harness under `src/fuzz/`) are the intended mechanisms; formats lacking them get them wired in.
- Existing utility types (`span`/`cspan`/`image_span`, `string_view`) are the preferred vehicles for making buffer lengths explicit, consistent with project guidelines.
- Each format phase includes updating tests/corpus and any needed docs (`CHANGES.md`), per the project change-impact checklist.
- "All underlying library calls" covers third-party decode/compression libraries (e.g., libtiff, libjpeg/-turbo, OpenEXR, libpng, libraw, openjpeg) and internal decode helpers reachable from untrusted data.
