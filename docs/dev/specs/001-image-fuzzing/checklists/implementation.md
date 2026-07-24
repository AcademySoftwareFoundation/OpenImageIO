# Implementation Readiness Checklist: Image Format Fuzzing Infrastructure

**Purpose**: Validate that requirements are specific enough to implement without ambiguity — one developer reading spec + plan should reach the same design as another
**Created**: 2026-06-24
**Reconciled against as-built**: 2026-07-12
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md)
**Audience**: Implementation author, PR reviewer
**Depth**: Standard

**Note**: This checklist was written before implementation, when the design was
still "one harness binary per format." The feature shipped with a different
design (single dynamic-dispatch binary), so several items below are answered
by as-built annotations in `spec.md`/`plan.md`/`data-model.md`/`research.md`/
`contracts/harness-contract.md` rather than by the original pre-implementation
draft text the item cites. Checked items are resolved as of the reconciliation
date above; unchecked items are genuine open gaps (some newly discovered during
this pass — see notes).

---

## Harness Interface Contract

- [x] CHK001 - Is the exact `LLVMFuzzerTestOneInput` signature (parameter types and return type) specified, or does it reference a standard that defines it unambiguously? [Clarity, Spec §FR-009] — Resolved: `contracts/harness-contract.md` §`LLVMFuzzerTestOneInput Signature` gives the exact `extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)` signature and return-value rules.
- [x] CHK002 - Is the per-call timeout mechanism specified — which API sets the timeout, what units, and what value? [Clarity, Spec §FR-010, Gap] — Resolved: FR-010's as-built note + `data-model.md` `GHAFuzzWorkflow.per_input_timeout` specify libFuzzer's `-timeout=60` (seconds), enforced externally rather than in harness source.
- [x] CHK003 - Is the `IOMemReader` contract defined — specifically, does it specify that it wraps `Filesystem::IOMemReader` and how raw bytes are passed to `ImageInput::open()`? [Clarity, Plan §Project Structure] — Resolved: `contracts/harness-contract.md` §OIIO API Usage, "Standard formats" bullet. *(Originally asked about `fuzz_utils.h`; that header was folded into `fuzz_image.cpp` — see T063 in `tasks.md` — so the contract now lives there instead.)*
- [x] CHK004 - Are requirements defined for what `LLVMFuzzerTestOneInput` MUST return on graceful failure (non-crash sanitizer finding vs. ordinary open failure)? [Completeness, Spec §FR-003, Gap] — Resolved: `contracts/harness-contract.md` §Error Handling: ordinary failure (`open()` returns null) → return 0, not a test failure; sanitizer findings are caught by the runtime, not via a return value at all.
- [x] CHK005 - Are requirements specified for minimum input size handling — e.g., must a zero-byte input be handled without crashing before any format detection occurs? [Clarity, Spec §Edge Cases] — Resolved: `spec.md` User Story 2, Acceptance Scenario 3 ("fed a zero-byte input or random bytes... returns gracefully").
- [x] CHK006 - Is the expected behavior when `ImageInput::open()` returns false (legitimate format rejection) vs. when it crashes specified? Both should result in the same harness behavior, but is this stated? [Clarity, Gap] — Resolved: `contracts/harness-contract.md` §Error Handling states the null-open case explicitly; FR-003 states any sanitizer finding MUST cause non-zero exit. The two paths are distinct and both specified.

---

## CMake Build Integration

- [x] CHK007 - Is the exact CMake option name (`OIIO_BUILD_FUZZ_TARGETS`) and default value (`OFF`) specified, or left to implementation discretion? [Clarity, Spec §FR-007] — Resolved: FR-007 names the option and default explicitly; matches `src/fuzz/CMakeLists.txt` gate in the root `CMakeLists.txt`.
- [x] CHK008 - Are requirements specified for which compiler flags the fuzz build MUST add (e.g., `-fsanitize=address,undefined,fuzzer-no-link`), and whether these conflict with any existing `SANITIZE` mechanism? [Completeness, Plan §Technical Context, Gap] — Resolved: `contracts/harness-contract.md` §Linking gives exact compile/link flags; `plan.md` Summary states the fuzz binary layers its own `-fsanitize=address,undefined` independent of the global `SANITIZE` variable.
- [x] CHK009 - Is the `add_fuzz_target()` CMake macro's interface defined — what arguments it takes, what targets it produces, and how it handles conditional compilation for optional-library formats? [Clarity, Plan §Project Structure, Gap] — Resolved by supersession: `research.md` §5 and `plan.md`'s `all_fuzz_targets` note record that this macro was never built — the single dynamic-dispatch design replaced per-format targets entirely, so no macro interface is needed.
- [x] CHK010 - Are requirements specified for how the fuzz build interacts with existing sanitizer CI jobs — e.g., must it be a separate build type, or can it share the SANITIZE build? [Completeness, Gap] — Resolved: `fuzz.yml` is a fully separate GHA workflow/job (not sharing the main CI sanitizer job); `plan.md` Summary + `data-model.md` `FuzzBuildConfig` document the independent `SANITIZE`/fuzz-flag relationship.
- [x] CHK011 - Is the requirement for `$LIB_FUZZING_ENGINE` linking specified in a way that describes how `CMakeLists.txt` consumes the environment variable (e.g., as a `target_link_libraries` argument)? [Clarity, Spec §FR-009, Gap] — Resolved: `src/fuzz/CMakeLists.txt` reads `ENV{LIB_FUZZING_ENGINE}` into `OIIO_FUZZING_ENGINE`, defaulting to `-fsanitize=fuzzer`, consumed via `target_link_options`; documented in `contracts/harness-contract.md` §Linking.

---

## GHA Workflow Requirements

- [x] CHK012 - Are the exact Tier 1 and Tier 2 format lists fully enumerated in the spec, or is Tier 2 defined only by example ("BMP, ICO, HDR, PNM, GIF, SGI, and similar")? [Clarity, Spec §FR-005a, Ambiguity] — Resolved: FR-005a enumerates all 10 Tier 1 and 19 Tier 2 formats by name, matching `.github/workflows/fuzz.yml`'s matrix exactly.
- [x] CHK013 - Are the specific job duration values for Tier 1 and Tier 2 specified (e.g., "Tier 1: 45 minutes, Tier 2: 15 minutes"), or is only the relative distinction stated? [Clarity, Spec §FR-005a, Gap] — Resolved: FR-005a states 1 hour/job (Tier 1) and 30 minutes/job (Tier 2), matching the matrix's `max_total_time` values.
- [x] CHK014 - Is the GHA cache key schema specified precisely enough to implement — format: `fuzz-corpus-<format>-<sha>`, but which sha (branch head, workflow run sha, or something else)? [Clarity, Spec §FR-008a, Ambiguity] — Resolved: `data-model.md` `EvolvedCorpus.cache_key` gives the exact as-built key (`fuzz-corpus-<format>-<ref_name>-<run_id>`) and restore-key prefix fallback chain.
- [ ] CHK015 - Are requirements defined for the teardown margin ("5 minutes") — specifically, how the harness or workflow enforces it (e.g., via libFuzzer's `-max_total_time` flag)? [Clarity, Plan §Performance Goals, Gap] — Partially resolved: `plan.md` Performance Goals now notes no separate margin is enforced (both tiers sit far under GHA's default 6h job timeout by construction). Left unchecked because this reframes the requirement rather than answering the original question as posed — there is no "5 minute" value anywhere in the as-built system to point to.
- [x] CHK016 - Is the artifact upload trigger condition specified — does upload occur on any non-zero exit, or only when the output directory contains specific crash artifact files? [Clarity, Spec §FR-006, Gap] — Resolved: upload is gated on `steps.fuzz.outcome == 'failure'` (any non-zero exit of the fuzz step), documented in `tasks.md` T040 and `build-steps.yml`.
- [x] CHK017 - Are requirements defined for the job summary output — what information MUST appear in the GHA job summary when a format crashes? [Completeness, Spec §FR-006, Gap] — Resolved: `src/build-scripts/ci-fuzztest.bash` writes Format/Status/Detail/crash-count fields to `$GITHUB_STEP_SUMMARY`; documented in `tasks.md` T040.
- [ ] CHK018 - Is the `workflow_dispatch` input schema fully specified — what are the valid values for the format input, and what happens when an invalid format name is provided? [Clarity, Spec Scenario 4, Gap] — **Newly discovered gap (2026-07-12)**: `fuzz.yml`'s `workflow_dispatch.inputs.format` is declared but never read anywhere in the workflow (no `github.event.inputs.format` reference) — the matrix always runs every format regardless of what's passed. Spec §User Story 1 Acceptance Scenario 4 ("a specific format name is passed as input, Then only that format's target runs") is currently unimplemented. Left unchecked; this is a real implementation gap, not just a documentation gap.
- [ ] CHK019 - Is the requirement for "automatically including new format targets" (Spec Scenario 3) specified as a failing check or a dynamic matrix generation mechanism? These have very different implementation costs. [Clarity, Spec Scenario 3, Ambiguity] — **Gap clarified, not resolved (2026-07-12)**: harness *coverage* is automatic (runtime format discovery), and the corpus-lint failing check catches a missing `src/fuzz/corpora/<format>/` directory — but `fuzz.yml`'s matrix is a static hand-maintained list with no check that a new/corpus-complete format is actually added to it. A new format could pass corpus-lint yet never be nightly-fuzzed. Left unchecked.

---

## Corpus Management

- [x] CHK020 - Is the corpus directory naming convention specified — are directory names the format plugin name, the file extension, or something else (spec shows `corpora/jpeg/` but plugin is `jpeg.imageio`)? [Clarity, Plan §Project Structure] — Resolved: `data-model.md` `FuzzTarget.format` specifies the OIIO format-registry key (e.g. `openexr`, not `exr`), with the exr→openexr rename (T058) as a concrete worked example.
- [x] CHK021 - Are requirements specified for how the harness discovers and loads seed files — via libFuzzer's `-corpus` flag, explicit file enumeration, or another mechanism? [Clarity, Spec §FR-008, Gap] — Resolved by consistent example: every invocation shown (`contracts/harness-contract.md`, `docs/dev/fuzzing.md`, `research.md`) passes the corpus directory as a positional libFuzzer argument (`oiio_fuzz_image corpus/jpeg/ ...`) — the harness itself does no seed enumeration; libFuzzer's standard corpus-directory loading applies.
- [x] CHK022 - Is the requirement for graceful degradation on missing corpus specified clearly enough to implement — must the harness continue running on random bytes, or must it exit cleanly? [Clarity, Spec §SC-001] — Resolved: `spec.md` US3 Acceptance Scenario 2 and `contracts/harness-contract.md` §Corpus Convention both state an empty/missing corpus dir is valid — the fuzzer starts from random bytes.
- [x] CHK023 - Are there size constraints specified for individual seed files (e.g., max bytes per seed) to prevent CI cache bloat or slow initialization? [Completeness, Gap] — Resolved: `data-model.md` `SeedCorpus` constraints (≤100 KB/file per `tasks.md` T041/T043, ≤5 MB total per format).
- [x] CHK024 - Is the cache restore/save timing specified precisely — must the restore happen before the harness binary runs, and must the save happen even if the harness exits non-zero (crash found)? [Clarity, Spec §FR-008a, Ambiguity] — Resolved: `data-model.md` `EvolvedCorpus` state transitions plus the (now-clarified) `GHAFuzzWorkflow` Invariants state the save step runs unconditionally (`if: always()`), regardless of crash.

---

## Conditional Compilation for Optional Formats

- [x] CHK025 - Are requirements specified for how optional-library formats signal their absence — e.g., are they simply excluded from the matrix, or do they produce a disabled/skipped job? [Completeness, Spec §FR-001, Gap] — Resolved by supersession: there are no more per-format source files (`fuzz_openvdb.cpp` etc. were never built — see `research.md` §5). `data-model.md` `FuzzTarget` constraints state optional-library formats simply don't appear in `get_extension_map()` when their library isn't compiled in; no CMake guards needed in the single harness source. openvdb/ptex remain in the nightly matrix regardless (marked `†` conditionally-compiled in `data-model.md`'s tier table) and are silently skipped at runtime by `ci-fuzztest.bash`'s `--list-formats` check if genuinely absent.
- [ ] CHK026 - Is the behavior defined for a CI environment where an optional library is present at configure time but absent at link time? [Edge Case, Gap] — Not addressed anywhere in spec/plan/data-model; this is a general OIIO plugin-registration edge case that the fuzzing feature docs don't specifically cover. Left unchecked.
- [x] CHK027 - Are requirements defined for which CI runner image (`aswf/ci-oiio:2026.3`) provides which optional libraries, and whether Tier 2 optional formats are tested in the nightly run or only locally? [Completeness, Plan §Technical Context, Gap] — Resolved: `docs/dev/fuzzing.md` Prerequisites and `research.md` §4 state the `aswf/ci-oiio` container (now `:2027`) has all optional format libraries pre-installed; openvdb/ptex run in the nightly Tier 2 matrix, not just locally.

---

## OSS-Fuzz Compatibility

- [ ] CHK028 - Is "minimal delta work" for OSS-Fuzz onboarding (FR-011) quantified? The spec says "minimal" but does not define what counts as minimal — is it a specific file count, time estimate, or list of required artifacts? [Clarity, Spec §FR-011, Ambiguity] — Still unquantified; `contracts/harness-contract.md` §OSS-Fuzz Compatibility describes what's needed qualitatively (draft `project.yaml`/`build.sh`) but gives no measurable bound. Left unchecked (User Story 5 also remains deferred/not started).
- [x] CHK029 - Are the OSS-Fuzz corpus directory conventions documented in the spec, or only inferred from the OSS-Fuzz documentation? If the latter, is there a reference? [Completeness, Spec §SC-005, Gap] — Resolved: `contracts/harness-contract.md` §OSS-Fuzz Compatibility gives the exact `build.sh` snippet showing the `fuzz_<fmt>_seed_corpus.zip` naming convention.
- [x] CHK030 - Is the requirement for `project.yaml` and `build.sh` artifacts specified as "must be producible without harness changes" clearly enough to distinguish from "must be committed to the repo in v1"? [Clarity, Spec §FR-011, Ambiguity] — Resolved: `spec.md` Status line + Assumptions explicitly state User Story 5 is deferred and no `ossfuzz/` files exist yet, while the harness-side prerequisites (dispatch, `$LIB_FUZZING_ENGINE`, `--list-formats`) are already built — the "producible without changes" vs. "committed now" distinction is explicit.

---

## Documentation Requirements

- [ ] CHK031 - Is "within 15 minutes of reading the documentation" (SC-007 / Spec §User Story 4) defined as a measurable requirement on the documentation itself — e.g., maximum step count, or list of prerequisites to state? [Measurability, Spec §SC-007] — Still an unfalsifiable time claim; no step-count or prerequisite-count bound defined anywhere. Left unchecked.
- [x] CHK032 - Are requirements specified for what the local fuzzing documentation MUST include — just build commands, or also: how to interpret ASan output, how to minimize a reproducer, how to submit a corpus seed? [Completeness, Spec §FR-013, Gap] — Resolved: `docs/dev/fuzzing.md` has all of Prerequisites, Building, Listing formats, Running, Reproducing a CI crash (ASan), Minimizing a crash, and Adding seeds sections, matching `tasks.md` T045.
- [ ] CHK033 - Is the target audience for `docs/dev/fuzzing.md` specified — is it OIIO contributors only, or also external security researchers unfamiliar with the codebase? This affects required prerequisite depth. [Clarity, Spec §FR-013, Gap] — `docs/dev/fuzzing.md` has no explicit audience statement (unlike the superseded `quickstart.md` draft, which named "OpenImageIO contributors"). Left unchecked.

---

## Success Criteria Measurability

- [x] CHK034 - Is "40+ supported image format readers" in SC-001 consistent with "29 formats in scope" stated in FR-001 and the Assumptions section? [Consistency, Spec §SC-001 vs §FR-001, Conflict] — Resolved: SC-001 rewritten to say "all in-scope image format readers" with an as-built note pointing at FR-001's 29-of-32 count, removing the "40+" conflict.
- [x] CHK035 - Is "≥ 1,000 executions/sec sustained" (Plan §Performance Goals) a hard requirement or an aspirational target? If hard, is there a CI check that enforces it? [Measurability, Plan §Performance Goals, Gap] — Resolved: `plan.md` Performance Goals now states explicitly these are aspirational, not CI-enforced (no step measures throughput).
- [x] CHK036 - Is "under 30 minutes" (SC-004) for adding a new harness testable — is there a defined test procedure for measuring this, or is it a developer experience goal without a verification method? [Measurability, Spec §SC-004] — Resolved: SC-004's as-built note explains the claim is now near-trivially true by construction (adding a format is just creating a corpus directory), not something requiring a timed test procedure.
- [ ] CHK037 - Is "within one iteration of feedback" in SC-005 (OSS-Fuzz PR) specific enough to be a testable acceptance criterion, given it depends on OSS-Fuzz reviewer response time? [Measurability, Spec §SC-005, Ambiguity] — Still depends on an external party's response time and User Story 5 remains deferred/not started, so this can't be tested yet. Left unchecked.

---

## Scope Boundaries

- [x] CHK038 - Is the exclusion of `null`, `term`, and `r3d` from fuzz coverage specified with enough rationale that a reviewer can confirm the exclusion is correct without re-researching? [Clarity, Spec §FR-001, Assumptions] — Resolved: FR-001 gives a one-line reason for each exclusion (no real parsing / output-only / proprietary SDK unavailable in CI).
- [ ] CHK039 - Is the deferral of write fuzzing (`ImageOutput`) specified with a clear boundary — what would trigger adding write fuzzing in v2, and does the v1 structure need to accommodate it in any specific way? [Completeness, Spec §Assumptions] — Partially resolved: Assumptions state write fuzzing is out of scope for v1 and the harness structure "should not preclude" adding it later, but no explicit trigger condition for when v2 work would start is given. Left unchecked.
- [x] CHK040 - Are requirements defined for what happens to fuzz findings after triage — the spec says "triaged by maintainer" but does not specify whether a fix must include a regression test, or what the commit/merge policy is for such fixes? [Completeness, Spec §Assumptions, Gap] — Resolved: `data-model.md` `Reproducer` lifecycle states a regression test MUST be committed to `testsuite/fuzz-<format>/` before the fix merges.
