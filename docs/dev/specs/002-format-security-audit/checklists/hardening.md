# Requirements Quality Checklist: Format Security & Stability Audit

**Purpose**: Unit-test the *requirements* in `spec.md` for completeness, clarity,
consistency, and measurability — for BOTH the hardening requirements and the
repeatable per-format audit process. Not a verification of code behavior.
**Created**: 2026-07-24
**Feature**: [spec.md](../spec.md)
**Focus**: Security/robustness requirement quality + per-format audit-process definition
**Depth**: Release-gate/standard blend
**Audience**: Spec author (tighten now) + PR reviewer (gate each format)

## Requirement Completeness

- [x] CHK001 - Are per-format extent limits required for EVERY structural dimension (width, height, depth, channels, tiles/strips, bit-depth), or only the subset named? [Completeness, Spec §FR-001/§FR-008]
- [x] CHK002 - Does the spec require an authoritative *source* for each format's ROI caps (the format spec's field widths/maxima), rather than leaving "format-defined maxima" unsourced? [Completeness, Spec §FR-001]
- [x] CHK003 - Are requirements present for validating file-internal offsets/seeks (negative, unaligned, past-EOF), beyond the general "declared data regions" clause? [Completeness, Spec §FR-007, §Edge Cases]
- [x] CHK004 - Is unbounded/looping work (recursive or self-referential offsets, runaway decode loops) tied to an actual requirement, or only mentioned in Edge Cases? [Gap, Spec §Edge Cases]
- [x] CHK005 - Is resource cleanup on error paths (no leaks) required for the early-returns that hardening introduces? [Completeness, Spec §FR-012]
- [x] CHK006 - Does the spec define which underlying-library error signals count (return code, null handle, short read, longjmp/error-manager), or leave "error condition" per-library undefined? [Completeness, Spec §FR-005]

## Requirement Clarity

- [x] CHK007 - Is "common-sense sanity bounds" quantified or bound to an objective source, or does it remain a vague adjective? [Ambiguity, Spec §FR-001]
- [x] CHK008 - Is the metadata-only vs pixel-affecting corruption boundary defined by an enumerable rule, or by examples alone? [Clarity, Spec §FR-013]
- [x] CHK009 - Is "impossibly large relative to actual file size" pinned to a ratio/threshold source (max_ratio), or left qualitative? [Clarity, Spec §FR-002]
- [x] CHK010 - Is "obvious shared-hazard writer bugs" given objective criteria that distinguish it from the "unusual/unique" writer issues that get deferred? [Ambiguity, Spec §FR-014/§FR-015]
- [x] CHK011 - Is "material findings" (FR-012) defined precisely enough for a reviewer to decide fix-now vs defer? [Clarity, Spec §FR-012]
- [x] CHK012 - Is "sufficient width to avoid 32-bit overflow" tied to a concrete type contract in the requirement itself, not only the plan? [Clarity, Spec §FR-004]

## Requirement Consistency

- [x] CHK013 - Do FR-009 (no behavior change for valid files) and FR-013 (newly rejecting some inputs) consistently delineate which now-rejected inputs are exempt from "no regression"? [Consistency, Spec §FR-009/§FR-013]
- [x] CHK014 - Are the failure-posture rules in Clarifications, FR-013, and US1 acceptance scenarios mutually consistent (no case where the metadata rule contradicts the pixel rule)? [Consistency, Spec §Clarifications/§FR-013]
- [x] CHK015 - Is the "readers only" scope (Assumptions) consistent with FR-014's writer-fix mandate, and is the reader/writer seam unambiguous? [Consistency, Spec §Assumptions/§FR-014]
- [x] CHK016 - Are the six enumerated checks referred to identically (same names/count) across the FR list, SC-002, and the audit contract? [Consistency, Spec §Requirements/§SC-002]

## Acceptance Criteria Quality (Measurability)

- [x] CHK017 - Is SC-001 ("no crash/hang/OOB/sanitizer report") tied to a defined corpus and sanitizer configuration so it is objectively verifiable per format? [Measurability, Spec §SC-001]
- [x] CHK018 - Is "allocation disproportionate to file size" (SC-006) measurable via a stated bound, rather than subjective? [Measurability, Spec §SC-006]
- [x] CHK019 - Is "byte-identical" (SC-003) defined against a specified reference set of valid files? [Measurability, Spec §SC-003]
- [x] CHK020 - Does SC-002 require recording each check's outcome in a defined artifact/location, making per-format completion auditable? [Measurability, Spec §SC-002/§FR-011]
- [x] CHK021 - Is SC-007 completion ("every reader-bearing format") backed by an authoritative enumerated format list, so "done" is objective? [Measurability, Spec §SC-007]
- [x] CHK022 - Is "at least one test/seed per hazard class that was exploitable" (SC-005) backed by an enumerable hazard-class list? [Measurability, Spec §SC-005]

## Scenario & Edge-Case Coverage

- [x] CHK023 - Are the truncation scenarios (mid-header / mid-scanline / mid-tile) covered as distinct requirements, or lumped together? [Coverage, Spec §Edge Cases]
- [x] CHK024 - Does the non-positive-structural-value requirement (FR-008) span every field, including bit-depth and tile/strip sizes? [Coverage, Spec §FR-008]
- [x] CHK025 - Is the read->write hazard scenario (FR-014) given an acceptance criterion, not just narrative? [Coverage, Spec §FR-014]
- [x] CHK026 - Are metadata-blob overrun scenarios (ICC/EXIF/palette length beyond its buffer) covered with a requirement AND an explicit strict/non-strict expectation? [Coverage, Spec §Edge Cases/§FR-013]
- [x] CHK027 - Is the "valid but genuinely large" false-positive scenario covered by a requirement that guards against over-aggressive limits? [Coverage, Spec §Edge Cases/§Assumptions]

## Non-Functional Requirements

- [x] CHK028 - Is the no-performance-regression expectation for added checks stated as a requirement (checks outside hot/inner loops), not only in the plan? [Gap, Spec §FR-009]
- [x] CHK029 - Are the constitution's security-triage severities/timelines (P0/P1) reflected as requirements in this spec, or left disconnected from it? [Consistency, Spec §Requirements]

## Dependencies & Assumptions

- [x] CHK030 - Is the assumption that `check_open` / `check_compression_ratio` / `imageinput:strict` exist and behave as needed *validated* for each target format, not merely assumed? [Assumption, Spec §Assumptions]
- [x] CHK031 - Is the list of "underlying libraries" complete and mapped to formats, or illustrative ("e.g., ...")? [Completeness, Spec §Assumptions]
- [x] CHK032 - Is the deferral tracking document (FR-015) specified with a location and owner so it cannot be silently dropped? [Dependency, Spec §FR-015]

## Audit Process & Delivery Definition

- [x] CHK033 - Is the per-format Definition of Done precise enough that two reviewers would agree a format is "audited"? [Clarity, Spec §FR-011/§SC-002]
- [x] CHK034 - Is the rule to split unanticipated/unrelated findings into separate commits/PRs stated as an explicit requirement, or only implied by "narrowly-scoped"? [Gap, Spec §FR-011]
- [x] CHK035 - Does the spec define what belongs in the same PR vs a separate PR when an audit uncovers an unrelated bug in shared/helper code? [Ambiguity, Spec §FR-011]
- [x] CHK036 - Is the required content of the per-format "audit findings summary" enumerated (checks applied / already-satisfied / N-A + issues found)? [Completeness, Spec §FR-011]
- [x] CHK037 - Are the defer-vs-fix decision criteria AND the record-it requirement stated together, so deferred items are consistently logged? [Consistency, Spec §FR-012/§FR-014/§FR-015]

## Ambiguities & Traceability

- [x] CHK038 - Is the requirement / acceptance-criteria ID scheme (FR-### / SC-###) established and used consistently to support per-item traceability? [Traceability]

## Notes

- Items are questions about the *requirements text*, not the decode code. A "no"/
  "unclear" answer means tighten `spec.md` before `/speckit-tasks`, not that code is wrong.
- CHK034/CHK035 flag the one gap surfaced during generation: the "split unrelated
  findings into separate commits/PRs" intent is implied (FR-011) but not written as
  its own requirement — decide whether to promote it to an explicit FR.
