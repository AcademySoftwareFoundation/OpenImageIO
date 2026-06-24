# Implementation Readiness Checklist: Image Format Fuzzing Infrastructure

**Purpose**: Validate that requirements are specific enough to implement without ambiguity — one developer reading spec + plan should reach the same design as another
**Created**: 2026-06-24
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md)
**Audience**: Implementation author, PR reviewer
**Depth**: Standard

---

## Harness Interface Contract

- [ ] CHK001 - Is the exact `LLVMFuzzerTestOneInput` signature (parameter types and return type) specified, or does it reference a standard that defines it unambiguously? [Clarity, Spec §FR-009]
- [ ] CHK002 - Is the per-call timeout mechanism specified — which API sets the timeout, what units, and what value? [Clarity, Spec §FR-010, Gap]
- [ ] CHK003 - Is `fuzz_utils.h`'s `IOMemReader` contract defined — specifically, does it specify that it wraps `Filesystem::IOMemReader` and how raw bytes are passed to `ImageInput::open()`? [Clarity, Plan §Project Structure]
- [ ] CHK004 - Are requirements defined for what `LLVMFuzzerTestOneInput` MUST return on graceful failure (non-crash sanitizer finding vs. ordinary open failure)? [Completeness, Spec §FR-003, Gap]
- [ ] CHK005 - Are requirements specified for minimum input size handling — e.g., must a zero-byte input be handled without crashing before any format detection occurs? [Clarity, Spec §Edge Cases]
- [ ] CHK006 - Is the expected behavior when `ImageInput::open()` returns false (legitimate format rejection) vs. when it crashes specified? Both should result in the same harness behavior, but is this stated? [Clarity, Gap]

---

## CMake Build Integration

- [ ] CHK007 - Is the exact CMake option name (`OIIO_BUILD_FUZZ_TARGETS`) and default value (`OFF`) specified, or left to implementation discretion? [Clarity, Spec §FR-007]
- [ ] CHK008 - Are requirements specified for which compiler flags the fuzz build MUST add (e.g., `-fsanitize=address,undefined,fuzzer-no-link`), and whether these conflict with any existing `SANITIZE` mechanism? [Completeness, Plan §Technical Context, Gap]
- [ ] CHK009 - Is the `add_fuzz_target()` CMake macro's interface defined — what arguments it takes, what targets it produces, and how it handles conditional compilation for optional-library formats? [Clarity, Plan §Project Structure, Gap]
- [ ] CHK010 - Are requirements specified for how the fuzz build interacts with existing sanitizer CI jobs — e.g., must it be a separate build type, or can it share the SANITIZE build? [Completeness, Gap]
- [ ] CHK011 - Is the requirement for `$LIB_FUZZING_ENGINE` linking specified in a way that describes how `CMakeLists.txt` consumes the environment variable (e.g., as a `target_link_libraries` argument)? [Clarity, Spec §FR-009, Gap]

---

## GHA Workflow Requirements

- [ ] CHK012 - Are the exact Tier 1 and Tier 2 format lists fully enumerated in the spec, or is Tier 2 defined only by example ("BMP, ICO, HDR, PNM, GIF, SGI, and similar")? [Clarity, Spec §FR-005a, Ambiguity]
- [ ] CHK013 - Are the specific job duration values for Tier 1 and Tier 2 specified (e.g., "Tier 1: 45 minutes, Tier 2: 15 minutes"), or is only the relative distinction stated? [Clarity, Spec §FR-005a, Gap]
- [ ] CHK014 - Is the GHA cache key schema specified precisely enough to implement — format: `fuzz-corpus-<format>-<sha>`, but which sha (branch head, workflow run sha, or something else)? [Clarity, Spec §FR-008a, Ambiguity]
- [ ] CHK015 - Are requirements defined for the teardown margin ("5 minutes") — specifically, how the harness or workflow enforces it (e.g., via libFuzzer's `-max_total_time` flag)? [Clarity, Plan §Performance Goals, Gap]
- [ ] CHK016 - Is the artifact upload trigger condition specified — does upload occur on any non-zero exit, or only when the output directory contains specific crash artifact files? [Clarity, Spec §FR-006, Gap]
- [ ] CHK017 - Are requirements defined for the job summary output — what information MUST appear in the GHA job summary when a format crashes? [Completeness, Spec §FR-006, Gap]
- [ ] CHK018 - Is the `workflow_dispatch` input schema fully specified — what are the valid values for the format input, and what happens when an invalid format name is provided? [Clarity, Spec Scenario 4, Gap]
- [ ] CHK019 - Is the requirement for "automatically including new format targets" (Spec Scenario 3) specified as a failing check or a dynamic matrix generation mechanism? These have very different implementation costs. [Clarity, Spec Scenario 3, Ambiguity]

---

## Corpus Management

- [ ] CHK020 - Is the corpus directory naming convention specified — are directory names the format plugin name, the file extension, or something else (spec shows `corpora/jpeg/` but plugin is `jpeg.imageio`)? [Clarity, Plan §Project Structure]
- [ ] CHK021 - Are requirements specified for how the harness discovers and loads seed files — via libFuzzer's `-corpus` flag, explicit file enumeration, or another mechanism? [Clarity, Spec §FR-008, Gap]
- [ ] CHK022 - Is the requirement for graceful degradation on missing corpus specified clearly enough to implement — must the harness continue running on random bytes, or must it exit cleanly? [Clarity, Spec §SC-001]
- [ ] CHK023 - Are there size constraints specified for individual seed files (e.g., max bytes per seed) to prevent CI cache bloat or slow initialization? [Completeness, Gap]
- [ ] CHK024 - Is the cache restore/save timing specified precisely — must the restore happen before the harness binary runs, and must the save happen even if the harness exits non-zero (crash found)? [Clarity, Spec §FR-008a, Ambiguity]

---

## Conditional Compilation for Optional Formats

- [ ] CHK025 - Are requirements specified for how `fuzz_openvdb.cpp` and `fuzz_ptex.cpp` signal their absence at CMake time — e.g., are they simply excluded from the matrix, or do they produce a disabled/skipped job? [Completeness, Spec §FR-001, Gap]
- [ ] CHK026 - Is the behavior defined for a CI environment where an optional library is present at configure time but absent at link time? [Edge Case, Gap]
- [ ] CHK027 - Are requirements defined for which CI runner image (`aswf/ci-oiio:2026.3`) provides which optional libraries, and whether Tier 2 optional formats are tested in the nightly run or only locally? [Completeness, Plan §Technical Context, Gap]

---

## OSS-Fuzz Compatibility

- [ ] CHK028 - Is "minimal delta work" for OSS-Fuzz onboarding (FR-011) quantified? The spec says "minimal" but does not define what counts as minimal — is it a specific file count, time estimate, or list of required artifacts? [Clarity, Spec §FR-011, Ambiguity]
- [ ] CHK029 - Are the OSS-Fuzz corpus directory conventions documented in the spec, or only inferred from the OSS-Fuzz documentation? If the latter, is there a reference? [Completeness, Spec §SC-005, Gap]
- [ ] CHK030 - Is the requirement for `project.yaml` and `build.sh` artifacts specified as "must be producible without harness changes" clearly enough to distinguish from "must be committed to the repo in v1"? [Clarity, Spec §FR-011, Ambiguity]

---

## Documentation Requirements

- [ ] CHK031 - Is "within 15 minutes of reading the documentation" (SC-007 / Spec §User Story 4) defined as a measurable requirement on the documentation itself — e.g., maximum step count, or list of prerequisites to state? [Measurability, Spec §SC-007]
- [ ] CHK032 - Are requirements specified for what the local fuzzing documentation MUST include — just build commands, or also: how to interpret ASan output, how to minimize a reproducer, how to submit a corpus seed? [Completeness, Spec §FR-013, Gap]
- [ ] CHK033 - Is the target audience for `docs/dev/fuzzing.md` specified — is it OIIO contributors only, or also external security researchers unfamiliar with the codebase? This affects required prerequisite depth. [Clarity, Spec §FR-013, Gap]

---

## Success Criteria Measurability

- [ ] CHK034 - Is "40+ supported image format readers" in SC-001 consistent with "29 formats in scope" stated in FR-001 and the Assumptions section? [Consistency, Spec §SC-001 vs §FR-001, Conflict]
- [ ] CHK035 - Is "≥ 1,000 executions/sec sustained" (Plan §Performance Goals) a hard requirement or an aspirational target? If hard, is there a CI check that enforces it? [Measurability, Plan §Performance Goals, Gap]
- [ ] CHK036 - Is "under 30 minutes" (SC-004) for adding a new harness testable — is there a defined test procedure for measuring this, or is it a developer experience goal without a verification method? [Measurability, Spec §SC-004]
- [ ] CHK037 - Is "within one iteration of feedback" in SC-005 (OSS-Fuzz PR) specific enough to be a testable acceptance criterion, given it depends on OSS-Fuzz reviewer response time? [Measurability, Spec §SC-005, Ambiguity]

---

## Scope Boundaries

- [ ] CHK038 - Is the exclusion of `null`, `term`, and `r3d` from fuzz coverage specified with enough rationale that a reviewer can confirm the exclusion is correct without re-researching? [Clarity, Spec §FR-001, Assumptions]
- [ ] CHK039 - Is the deferral of write fuzzing (`ImageOutput`) specified with a clear boundary — what would trigger adding write fuzzing in v2, and does the v1 structure need to accommodate it in any specific way? [Completeness, Spec §Assumptions]
- [ ] CHK040 - Are requirements defined for what happens to fuzz findings after triage — the spec says "triaged by maintainer" but does not specify whether a fix must include a regression test, or what the commit/merge policy is for such fixes? [Completeness, Spec §Assumptions, Gap]
