# Feature Specification: Image Format Fuzzing Infrastructure

**Feature Branch**: `001-image-fuzzing`

**Created**: 2026-06-23

**Status**: Draft

**Input**: User description: "fuzzing strategy for OpenImageIO image format readers, GHA CI workflow, oss-fuzz compatible"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Automated Nightly Fuzz CI Run (Priority: P1)

A developer or security engineer wants confidence that OpenImageIO image format readers
are robust against malformed, corrupted, and adversarially crafted inputs. They want a
GitHub Actions workflow that runs on a nightly schedule (and on demand), exercises every
supported image format reader with a fuzzing engine for as long as the job allows, and
fails loudly if any crash or memory safety violation is found — capturing a reproducer
artifact for investigation.

**Why this priority**: This is the core ask. Without a running CI workflow, the fuzzing
infrastructure has no operational value. It also delivers immediate security value every
night without any developer action.

**Independent Test**: Trigger the GHA workflow manually on the branch. It should build
fuzz targets, run them against a seed corpus, and complete without crashing. Introduce a
known-bad input (e.g., a file that triggers an out-of-bounds read) and verify the run
fails and uploads a reproducer artifact.

**Acceptance Scenarios**:

1. **Given** the nightly schedule fires, **When** all image format fuzz targets run for
   their allotted time, **Then** the workflow reports success and no crash artifacts are
   uploaded.
2. **Given** a fuzz target finds a crash or sanitizer violation, **When** the run
   completes, **Then** the workflow step fails, the reproducer input is uploaded as a
   GHA artifact, and the failing format is identified in the job summary.
3. **Given** a developer pushes a new format plugin, **When** the nightly workflow runs,
   **Then** that format's fuzz target is included automatically (or a failing check
   reminds the developer to add one).
4. **Given** the workflow is triggered manually via `workflow_dispatch`, **When** a
   specific format name is passed as input, **Then** only that format's target runs
   (useful for re-checking a fix).

---

### User Story 2 - Per-Format Fuzz Harness Library (Priority: P1)

A developer adding or modifying an image format reader wants a clear, low-friction pattern
for writing a fuzz harness. The harness structure MUST be uniform across all formats,
lightweight to add, and OSS-Fuzz-compatible from day one so that future OSS-Fuzz
onboarding requires minimal rework.

**Why this priority**: Without harnesses, there is nothing to fuzz. A consistent pattern
reduces the cost of adding coverage for each of the 40+ existing format plugins.

**Independent Test**: Pick two format plugins (e.g., JPEG, PNG). Write harnesses using
the defined pattern. Compile both with sanitizers enabled. Run each locally against a
minimal seed corpus for 30 seconds. Verify both run without crashes and produce coverage
output.

**Acceptance Scenarios**:

1. **Given** the fuzz harness template/pattern is defined, **When** a developer follows
   it for a new format, **Then** the harness compiles cleanly under the sanitizer build
   configuration without modifying production source files.
2. **Given** a harness is built, **When** it is fed a valid seed image for that format,
   **Then** it processes the image without crashing.
3. **Given** a harness is built, **When** it is fed a zero-byte input or random bytes,
   **Then** it returns gracefully without undefined behavior (crash == fail).
4. **Given** the harness entry point follows the OSS-Fuzz `LLVMFuzzerTestOneInput`
   signature, **When** the OSS-Fuzz build script runs, **Then** it links without
   modification.

---

### User Story 3 - Seed Corpus Per Format (Priority: P2)

A developer wants each format's fuzz target to start from a meaningful seed corpus
(small, valid, representative sample images) rather than random bytes, improving coverage
speed and reducing time-to-first-interesting-mutation.

**Why this priority**: Seeds dramatically improve fuzzer effectiveness. However, fuzz
targets without seeds still run — they just find bugs more slowly. Seeds are high value
but not blocking for initial deployment.

**Independent Test**: For three formats (e.g., TIFF, EXR, PNG), verify that a non-empty
seed corpus exists in the designated corpus directory, that each seed file is a valid
image of the correct format, and that the fuzz target processes every seed without
crashing.

**Acceptance Scenarios**:

1. **Given** seeds exist in the corpus directory, **When** the fuzz target initializes,
   **Then** it loads and processes all seeds before beginning mutation.
2. **Given** the seed corpus for a format is empty or missing, **When** the fuzz target
   runs, **Then** it starts from scratch without crashing (graceful degradation).
3. **Given** OIIO's existing test suite contains valid sample images, **When** seeds are
   collected, **Then** a subset of those images is reused as seeds to avoid duplicating
   storage.

---

### User Story 4 - Local Developer Fuzzing Workflow (Priority: P2)

A developer working on a format plugin wants to run fuzzing locally against their changes
before pushing — to catch crashes before CI does, to reproduce a specific finding
interactively, or to extend the seed corpus. The build system MUST make local fuzz runs
as easy as `cmake --build . --target fuzz-jpeg` (or equivalent), without requiring
special environment setup beyond the fuzz build configuration.

**Why this priority**: Without a local workflow, developers cannot act on CI fuzz
findings efficiently. Reproducing a crash requires re-running CI or guessing at the
reproducer. Local fuzzing also enables pre-push validation of format changes.

**Independent Test**: On a developer workstation with clang and ASan available, configure
the project with the fuzz build option enabled. Build and run the JPEG fuzz target
locally for 60 seconds. Verify it runs, processes seeds, and exits cleanly. Then feed
it a known-crashing input and verify it exits non-zero with an ASan report.

**Acceptance Scenarios**:

1. **Given** a developer has a fuzz-enabled build configured, **When** they run the fuzz
   target binary for a specific format directly, **Then** it starts fuzzing without
   additional setup.
2. **Given** a crash reproducer file, **When** the developer passes it as an argument to
   the fuzz target binary, **Then** the crash is reproduced reliably with a full ASan
   stack trace.
3. **Given** a developer wants to extend the seed corpus, **When** they add a new image
   file to the corpus directory, **Then** the next fuzz run picks it up automatically.
4. **Given** a developer is on macOS or Linux with clang installed, **When** they follow
   the documented local fuzzing steps, **Then** they can build and run at least one fuzz
   target within 15 minutes of reading the documentation.

---

### User Story 5 - OSS-Fuzz Onboarding Readiness (Priority: P3)

A project maintainer wants to submit OpenImageIO to OSS-Fuzz so that Google's
infrastructure continuously fuzzes the project at scale. The local fuzzing infrastructure
(harnesses, build configuration, corpus) MUST be structured so that writing an OSS-Fuzz
`project.yaml` and build script requires minimal delta work.

**Why this priority**: OSS-Fuzz integration is a stated future goal but not required for
the nightly CI value. Getting the structure right from day one means OSS-Fuzz onboarding
is a small incremental step rather than a rework.

**Independent Test**: Write a draft `project.yaml` and `build.sh` for OSS-Fuzz that
references the existing harnesses and build configuration. Verify that the OSS-Fuzz
`infra/helper.py build_fuzzers openimageio` command succeeds in a local Docker
environment.

**Acceptance Scenarios**:

1. **Given** the harness entry points follow the `LLVMFuzzerTestOneInput` ABI, **When**
   OSS-Fuzz's `build.sh` compiles them, **Then** all targets link without changes to
   harness source.
2. **Given** the corpus structure follows OSS-Fuzz conventions, **When** OSS-Fuzz syncs
   the corpus, **Then** seeds are picked up automatically.
3. **Given** a draft OSS-Fuzz PR is submitted, **When** OSS-Fuzz CI validates it,
   **Then** the check passes within one iteration of feedback.

---

### Edge Cases

- What happens when a format reader depends on an optional third-party library that is
  not available in the fuzz build? (Harness must degrade gracefully or be conditionally
  compiled.)
- How does the system handle a fuzz target that times out rather than crashes? (Timeout
  should be treated as a failure and flagged.)
- What happens when the seed corpus grows too large over time and slows down CI?
  (Corpus minimization strategy needed.)
- What if a fuzz finding is a hang (infinite loop) rather than a crash? (Harness MUST
  set a wall-clock timeout on image open/read operations.)

## Clarifications

### Session 2026-06-23

- Q: Should GHA fuzz jobs run sequentially (one job, shared time) or in parallel (matrix, one job per format)? → A: Parallel matrix — one job per format so each gets its own full runtime budget.
- Q: Should all formats get equal job duration, or should time be weighted by format complexity/risk? → A: Two tiers — complex/high-risk formats (EXR, TIFF, JPEG, PNG, DPX, PSD, HEIC, WebP, JXL, JPEG2000) get full-duration jobs; simpler formats (BMP, ICO, HDR, PNM, GIF, SGI, etc.) get a shorter capped duration.
- Q: Should the evolved fuzz corpus be discarded after each run, cached between runs, or committed to the repo? → A: Persist via GHA cache keyed per format; corpus grows nightly and is restored at the start of each run.
- Q: When one format job finds a crash, should remaining format jobs be cancelled or continue to completion? → A: Continue all — all format jobs run to completion regardless of other failures (`fail-fast: false`); overall workflow fails if any job failed.
- Q: Should harnesses link against libFuzzer specifically, or use the `$LIB_FUZZING_ENGINE` abstraction? → A: Use `$LIB_FUZZING_ENGINE` — libFuzzer is the local default; OSS-Fuzz automatically runs all four engines (libFuzzer, AFL++, Honggfuzz, Centipede) with no harness changes required.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide one fuzz harness per supported image format reader.
  Of the 32 format plugins in `src/*.imageio/`, 29 are in scope. Excluded: `null`
  (no real parsing), `term` (output-only), `r3d` (proprietary SDK unavailable in CI).
  `openvdb` and `ptex` are included as Tier 2, conditionally compiled on library
  availability.
- **FR-002**: Each harness MUST accept arbitrary byte sequences as input and attempt to
  open and read image data through the standard `ImageInput` API.
- **FR-003**: Harnesses MUST be compiled with AddressSanitizer and UndefinedBehaviorSan
  enabled; any sanitizer finding MUST cause a non-zero exit.
- **FR-004**: A GitHub Actions workflow MUST run all fuzz targets on a nightly schedule
  and on `workflow_dispatch`.
- **FR-005**: The GHA workflow MUST use a parallel matrix strategy — one job per format
  (or per small batch of formats) — so each format receives its own dedicated runtime
  budget rather than competing for a shared time slice.
- **FR-005a**: Formats MUST be tiered by complexity and risk: Tier 1 (complex/high-risk:
  EXR, TIFF, JPEG, PNG, DPX, PSD, HEIC, WebP, JXL, JPEG2000) runs full-duration jobs;
  Tier 2 (simpler formats: BMP, ICO, HDR, PNM, GIF, SGI, and similar) runs shorter
  capped jobs. Tier membership MUST be explicitly declared in the workflow configuration.
- **FR-005b**: The matrix MUST use `fail-fast: false` so all format jobs run to
  completion regardless of whether other format jobs have failed. The overall workflow
  MUST report failure if any individual format job fails.
- **FR-006**: The GHA workflow MUST upload reproducer artifacts (crashing inputs) when a
  target crashes or a sanitizer violation is detected.
- **FR-007**: The fuzz build MUST be controlled by a CMake option (e.g.,
  `OIIO_BUILD_FUZZ_TARGETS`) that defaults to `OFF` and does not affect normal builds.
- **FR-008**: Each format target MUST have a seed corpus directory; the harness MUST
  process all seeds before mutation begins.
- **FR-008a**: The GHA fuzz workflow MUST restore a per-format evolved corpus from GHA
  cache at the start of each run and save the updated corpus back to cache on completion,
  so fuzzer coverage compounds across nightly runs. Seed corpus serves as the fallback
  when no cache entry exists (first run or after cache eviction).
- **FR-009**: Harness entry points MUST use the `LLVMFuzzerTestOneInput(const uint8_t*,
  size_t)` signature and MUST link against `$LIB_FUZZING_ENGINE` (not a hardcoded
  `-fsanitize=fuzzer` flag) so the same harness binary works with libFuzzer locally and
  with all four OSS-Fuzz engines (libFuzzer, AFL++, Honggfuzz, Centipede) without
  source changes.
- **FR-010**: The harness MUST enforce a per-call timeout on image read operations to
  prevent hangs from appearing as successful runs.
- **FR-011**: The build and harness structure MUST support producing an OSS-Fuzz
  `project.yaml` and `build.sh` with minimal additional work.
- **FR-012**: Fuzz target binaries MUST be runnable directly on a developer workstation
  (macOS or Linux with clang) without additional tooling beyond the fuzz build
  configuration, enabling local reproduce-and-debug workflows.
- **FR-013**: Developer documentation MUST describe how to build and run fuzz targets
  locally, including how to reproduce a crash from a given input file.

### Key Entities

- **Fuzz Target**: A compiled binary for one image format that accepts raw bytes and
  exercises the format's `ImageInput` implementation.
- **Seed Corpus**: A directory of small, valid image files for a specific format used to
  seed the fuzzer's initial mutation queue.
- **Reproducer**: A minimal input file that reliably triggers a specific crash or
  sanitizer finding; committed to the test suite after triage.
- **Fuzz Build**: A CMake build configuration with sanitizers and fuzzer instrumentation
  enabled, separate from Release/Debug builds.
- **GHA Fuzz Workflow**: The GitHub Actions workflow file that orchestrates nightly fuzz
  runs, time budgeting, and artifact upload.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 40+ supported image format readers have a corresponding fuzz target
  that compiles and runs without immediate crashes against their seed corpus.
- **SC-002**: Each format's nightly fuzz job completes within the GitHub Actions per-job
  time limit without manual intervention; all format jobs run in parallel via a matrix
  strategy.
- **SC-003**: Any crash or sanitizer violation found during a fuzz run causes the GHA
  workflow to fail within the same run and upload a reproducer artifact.
- **SC-004**: A developer can add a new format harness by following the established
  pattern in under 30 minutes, with no changes to production source files required.
- **SC-005**: The harness structure passes OSS-Fuzz's build validation check
  (`infra/helper.py build_fuzzers`) without source changes to the harnesses themselves.
- **SC-006**: Seed corpora cover at least the 10 most-used formats (JPEG, PNG, TIFF,
  EXR, DPX, BMP, GIF, WebP, HEIC, PSD) at initial rollout.
- **SC-007**: A developer can reproduce a CI-found crash locally by downloading the
  reproducer artifact and running the fuzz target binary with it in under 5 minutes,
  with no steps beyond the documented local fuzzing setup.

## Assumptions

- The initial rollout covers 29 of 32 format plugins. Excluded: `null` (test stub, no
  parsing), `term` (output-only), `r3d` (proprietary RED SDK). All optional-dependency
  formats (heif, jpegxl, jpeg2000, raw, dicom, ffmpeg, openvdb, ptex) are
  conditionally compiled — present when the library is available, silently absent when
  not.
- Reading is the primary fuzzing target; writing (ImageOutput) is out of scope for v1
  but the harness structure should not preclude adding write fuzzing later.
- Harnesses link against `$LIB_FUZZING_ENGINE` rather than hardcoding libFuzzer. For
  local development, libFuzzer (clang's `-fsanitize=fuzzer`) is the default engine.
  When onboarded to OSS-Fuzz, the same harnesses run under all four supported engines
  (libFuzzer, AFL++, Honggfuzz, Centipede) automatically with no source changes.
- Google's FuzzTest framework was evaluated and explicitly deferred for v1. FuzzTest's
  value is structured domain constraints on typed inputs; for raw-byte file format
  parsing, it provides no advantage over `LLVMFuzzerTestOneInput`. FuzzTest is the
  right tool for future structured API fuzzing (ImageBufAlgo, oiiotool, TextureSystem)
  and is fully compatible with these harnesses — both can coexist in the same repo.
  See `research.md §1a` for full rationale.
- GHA-hosted runners (Linux, `ubuntu-latest`) are sufficient for the nightly fuzz job;
  macOS and Windows fuzz targets are out of scope for v1.
- Fuzz findings discovered by the nightly CI are triaged by the project maintainer;
  no automated bug-filing integration (e.g., to GitHub Issues) is required for v1.
- OIIO's existing regression test image files (under `testsuite/`) are suitable as
  initial seed corpus material without additional licensing concerns.
- The project's existing CMake build system will be extended (not replaced) to support
  the fuzz build configuration.
