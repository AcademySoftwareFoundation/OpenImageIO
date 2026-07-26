# Implementation Plan: Image Format Security & Stability Audit

**Branch**: `002-format-security-audit` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/002-format-security-audit/spec.md`

## Summary

Harden OIIO's untrusted-input decode path one format at a time. Each of the
~32 reader-bearing format plugins under `src/<FORMAT>.imageio/` gets an
independent audit phase that applies a fixed checklist of six core
checks plus opportunistic extras, backed by fuzz/regression coverage. The

> **Delivery model (amended).** The audit was originally organized as one
> format per PR. To reduce PR count and reviewer burden, the near-identical
> `check_open()` / `check_compression_ratio()` guards and the simple
> integer-overflow fixes are batched into shared cross-format commits, while
> more extensive per-format hardening stays in its own commit (see spec
> FR-011). The per-format audit scope is unchanged.

technical approach reuses the existing `ImageInput` hardening helpers already
proven on the `raw` and `rla` plugins — `check_open()`, `check_compression_ratio()`,
and `valid_raw_span_size()` / span-based bounds — plus the `imageinput:strict`
attribute for the metadata-vs-pixel failure-posture rule. No new subsystem is
built; this is a repeatable audit *procedure* applied per format, with a shared
inventory tracker and a writer-deferral log so the multi-PR effort stays
coherent and nothing is forgotten.

## Technical Context

**Language/Version**: C++17 (project standard; `-std=c++17`)

**Primary Dependencies**: Per-format decode libraries reachable from untrusted
data — libtiff, libjpeg/-turbo, libpng, OpenEXR/Imath, libraw, openjpeg,
libwebp, libheif, giflib, DCMTK, FreeType-adjacent parsers, plus OIIO's own
in-tree decoders (bmp, dds, dpx/cineon, fits, hdr, iff, pnm, psd, rla, sgi,
softimage, targa, zfile, ico, term). Existing OIIO helpers: `ImageInput::check_open()`,
`ImageInput::check_compression_ratio()`, `ImageInput::valid_raw_span_size()`,
global `limits:*` attributes.

**Storage**: N/A (files are the input; no persistent store introduced)

**Testing**: ctest testsuite (`testsuite/<name>/`), libFuzzer harness
`oiio_fuzz_image` under `src/fuzz/` (gated by `OIIO_BUILD_FUZZ_TARGETS`), ASan+UBSan
sanitizer builds.

**Target Platform**: Cross-platform (Linux/macOS/Windows); sanitizer builds on
Linux/macOS clang.

**Project Type**: C++ library + CLI tools (single project, format-plugin architecture)

**Performance Goals**: No regression to decode throughput on valid files. All
added checks are O(1)/O(header) and run outside pixel inner loops; per FR-009 /
SC-003 valid-file results must be byte-identical.

**Constraints**: Preserve API/ABI and behavior for valid in-spec files.
Fail-closed on pixel-affecting corruption; `imageinput:strict`-gated on
metadata-only corruption (FR-013). Allocation bounded by validated file-supported
extents (SC-006). No new global `limits:*` attribute without maintainer sign-off.

**Scale/Scope**: ~32 reader-bearing format plugins, each an independent audit phase (delivered per the amended FR-011 model: shared cross-format commits for the near-identical checks/overflow guards, separate commits for extensive per-format work).
Effort complete only when every reachable decoder is audited (SC-007).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Constitution v1.0.0 (OpenImageIO Fuzzing Constitution). Relevant gates:

| Principle | Gate | Status |
|-----------|------|--------|
| II. Safety & Input Robustness (NON-NEGOTIABLE) | Every parser handles malformed/truncated/adversarial input with no UB/memory-safety violation; ASan+UBSan clean | **Directly served** — this is the feature's purpose |
| V. Reproducibility | Every fixed defect gets a minimal reproducer committed as a regression test | **Enforced** via FR-010 / SC-005 (per-hazard test or fuzz seed) |
| III. Fuzz-First | Modified plugins carry a fuzz harness runnable on seed corpus | **Satisfied** — reuse existing `oiio_fuzz_image` + per-format seed corpora; add seeds per phase |
| I. Format-Agnostic API Integrity | No parser change leaks into public API; API/ABI stable | **Satisfied** — FR-009 preserves API/ABI; helpers are existing `ImageInput` methods |
| IV. Minimal Footprint | No production logic change solely to ease fuzzing; sanitizer variants don't degrade Release/Debug | **Satisfied** — all changes are production robustness improvements |

Security & Triage: findings map to constitution P0 (ASan/UBSan) / P1 (crash)
severities; potential-CVE findings routed through the private security process
(spec already drafts GHSA advisories in-repo).

**Result: PASS. No violations. Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/002-format-security-audit/
├── plan.md              # This file
├── research.md          # Phase 0: helper inventory, hazard taxonomy, prior-art from raw/rla
├── data-model.md        # Phase 1: format inventory, audit-checklist entity, hazard classes, deferral log
├── quickstart.md        # Phase 1: how to execute one format's audit phase end-to-end
├── contracts/
│   ├── per-format-audit.md      # The repeatable audit procedure + PR acceptance contract
│   └── reader-hardening-api.md  # Contract for using check_open/check_compression_ratio/span helpers
├── checklists/
│   └── requirements.md  # Spec quality checklist (from /speckit-specify)
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/
├── include/OpenImageIO/
│   └── imageio.h                 # ImageInput helpers: check_open, check_compression_ratio,
│                                 #   valid_raw_span_size; limits:* attribute docs
├── libOpenImageIO/
│   ├── imageinput.cpp            # Implementations of the shared hardening helpers
│   └── imageio.cpp               # limits:* global attribute plumbing
├── <FORMAT>.imageio/             # Per-format audit target (one phase each):
│   ├── bmp/ cineon/ dds/ dicom/ dpx/ ffmpeg/ fits/ gif/ hdr/ heif/ ico/
│   ├── iff/ jpeg/ jpeg2000/ jpegxl/ openexr/ png/ pnm/ psd/ ptex/ r3d/
│   ├── raw/ rla/ sgi/ softimage/ targa/ tiff/ webp/ zfile/ openvdb/
│   └── null/ term/               # (trivial/no untrusted decode — quick confirm phases)
└── fuzz/                         # oiio_fuzz_image harness + per-format seed corpora

testsuite/
└── <format>/                     # Per-format regression fixtures incl. malformed inputs

docs/dev/
├── fuzzing.md                    # Existing fuzzing dev doc
└── format-hardening-deferrals.md # NEW: tracking log for deferred (esp. writer) findings (FR-015)
```

**Structure Decision**: Single-project C++ format-plugin layout (Option 1). Each
audit phase touches exactly one `src/<FORMAT>.imageio/` plugin plus its testsuite
fixtures and fuzz seeds, keeping PRs narrow (FR-011). Shared helpers in
`imageio.h`/`imageinput.cpp` are extended only when a needed guard doesn't yet
exist, and such changes ship in the earliest phase that needs them.

## Complexity Tracking

> No Constitution Check violations. Section intentionally empty.
