# Contract: Per-Format Audit Procedure & PR Acceptance

This is the repeatable contract every format phase MUST satisfy. It is the CLI/
process "interface" of this feature (there is no network API). One phase = one
format = one PR.

## Inputs

- Target format `<F>` and its plugin directory `src/<F>.imageio/`.
- The shared helpers in `imageio.h` / `imageinput.cpp`.
- The format's existing testsuite fixtures and `src/fuzz/` seed corpus (if any).

## Procedure (ordered)

1. **Census**: Enumerate, in the reader, (a) every header/dimension field feeding
   an allocation, offset, or loop bound; (b) every call into an underlying library
   or in-tree decoder; (c) every raw `ptr,len` buffer hand-off; (d) every place
   ancillary metadata (ICC/EXIF/palette/etc.) is parsed.
2. **Apply checklist C1–C8** (see data-model.md) using `reader-hardening-api.md`:
   - C1 `check_open()` with a format-spec ROI at the top of `open()`.
   - C2 `check_compression_ratio()` once the header-implied uncompressed size and
     the file size are known, before decode/allocation.
   - C3 audit `valid_file()` for correctness and bounds-safety (add one if missing).
   - C4 promote all size/offset math to 64-bit; validate factors pre-multiply.
   - C5 check every library return/error; convert to `errorfmt()` + `false`, free
     resources on error paths.
   - C6 convert `ptr,len` → `span`/`cspan`; use `valid_raw_span_size()` for decode
     targets.
   - C7 verify declared regions ≤ available bytes before reading.
   - C8 wire failure posture: `imageinput:strict` for metadata-only defects; hard
     error (always) for pixel-affecting defects.
3. **Opportunistic C9**: fix material extras found (unbounded loops, alignment
   assumptions, uninitialized reads, error-path leaks).
4. **C10 read→write hazard**: if a valid input can drive the writer into the same
   unsafe behavior (e.g., shared offset overflow), fix it here; log any deferred
   writer issue to `docs/dev/format-hardening-deferrals.md`.
5. **Coverage**: add a malformed-input regression fixture and/or fuzz seed for each
   hazard class (H1–H9) that was exploitable or unguarded before the change.
6. **Verify**: sanitizer build (ASan+UBSan) clean on the new corpus; full testsuite
   green; valid-file outputs byte-identical to pre-change.
7. **Document**: PR description carries the C1–C10 outcome table + corpus additions
   + deferral links. Update the format's `status` in data-model.md inventory. Do
   NOT edit `CHANGES.md` — it is filled in at release time; editing the
   in-development section makes the patch conflict when backported to release
   branches that lack that section.

## Acceptance criteria (Definition of Done for one format)

- [ ] C1–C8 each have a recorded outcome (applied / already-satisfied / N-A+reason). (SC-002)
- [ ] No malformed corpus input crashes, hangs, reads/writes OOB, or trips ASan/UBSan. (SC-001)
- [ ] Every declared-oversized / bomb input is rejected with a clear `errorfmt()`
      message and bounded allocation. (SC-006)
- [ ] Metadata-only defects honor `imageinput:strict`; pixel defects are always hard
      errors. (FR-013)
- [ ] ≥1 new test/seed per exploitable hazard class. (SC-005)
- [ ] Zero regressions: all previously-readable valid files read byte-identically. (SC-003)
- [ ] API/ABI unchanged. (FR-009)
- [ ] PR scoped to `src/<F>.imageio/` + its tests/seeds (+ shared helper only if a
      needed guard is newly added). (FR-011, SC-004)
- [ ] Deferred writer/other findings logged. (FR-015)

## Outputs

- A merged PR hardening format `<F>`.
- Updated inventory status and deferral log (CHANGES.md is left for release time).
- New regression fixtures + fuzz seeds.
