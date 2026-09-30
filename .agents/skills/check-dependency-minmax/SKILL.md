---
name: check-dependency-minmax
description: Audit which dependency versions CI really tests (from a ci.yml run's job summaries) against INSTALL.md claims and ci.yml job descriptions. Explicit slash-command use only.
argument-hint: [ci-run-url-or-id]
disable-model-invocation: true
---

Audit the dependency versions tested in practice by our CI against what
`INSTALL.md` documents and what the `ci.yml` job descriptions and comments
claim, and against the local build. The skill has three phases:

1. Gather data, run all checks, save the dated report (steps 1-6).
2. Make the simple, behavior-neutral doc fixes without asking (step 7):
   `ci.yml` descriptions/comments and `INSTALL.md` tested maxima.
3. Offer fixes that change code or workflows, applied only one at a time
   as the user selects them, with no commit without asking (step 8).

This is a LIVING DOCUMENT. When we discover a new thing worth checking, a
better way to do a step, or the right fix for a problem the audit finds,
update this file (and the scripts next to it) as part of that work.

Arguments: `$ARGUMENTS`
- First argument (optional): a GitHub Actions run URL or numeric run id for
  the `ci.yml` workflow. If omitted, use the latest SUCCESSFUL `ci.yml` run
  on the current git branch (see step 1).

## Ground truth

- Each `ci.yml` job runs `src/build-scripts/ci-test.bash`, which runs
  `oiiotool --buildinfo` and appends it to the job's step summary
  (`$GITHUB_STEP_SUMMARY`). That output (compiler, C++ standard, and a
  `Dependencies:` list) is the ground truth for what a job used. The same
  text is in the job log. The step summary itself is not available through
  the API, so use the job logs.
- `INSTALL.md` documents, per dependency, the minimum supported version and
  the maximum version tested (sometimes "and main/master").
- `ci.yml` matrix entries have a `desc:` (which becomes the job name) and
  `#` comments that state intent, such as "oldest" or "bleeding edge".

## Step 0: environment

- `gh` needs the sandbox disabled here (TLS error otherwise); run every `gh`
  and `curl` call with `dangerouslyDisableSandbox: true`.
- Work in the session scratchpad dir, not the repo. Put the report in the
  repo root as `./dependency-report-YYYY-MM-DD.md` (untracked; do not
  commit it).
- Never hard-code a run URL in this skill or in files you edit.

## Step 1: choose the run

- If an argument names a run, use it (`gh run view <id-or-url> --json ...`).
- Otherwise:
  `gh run list --workflow ci.yml --branch "$(git branch --show-current)" --status success --limit 1 --json databaseId,url,headSha,createdAt`
  If nothing, say so and ask; do not silently fall back to another branch.
  Runs on forks live in the fork's repo; if the branch has no runs in the
  default repo, check `git remote -v` and pass `--repo`.
- Report the run URL, head SHA, and whether that SHA matches local HEAD
  (if not, the report may not match the local ci.yml; say so, or read
  ci.yml at that SHA with `git show <sha>:.github/workflows/ci.yml`).

## Step 2: fetch job logs and parse buildinfo

`gh run view <run> --log` fails ("too many API requests"); fetch per job.

1. `bash scripts/fetch_logs.sh <run-id-or-url> <outdir> [--repo owner/name]`
   downloads every job's log in parallel to `<outdir>/logs/<jobid>.log`
   (and a job list to `<outdir>/jobs.tsv`). Paths are relative to this
   skill's directory.
2. `python3 scripts/parse_buildinfo.py <outdir> [dep ...]` writes
   `<outdir>/parsed.json` (per job: name, compiler, C++ std, deps),
   `<outdir>/versions.txt` (all distinct versions per dependency, including
   `CMake`; the direct input for table A), and prints a per-dependency pivot
   (version -> jobs).

Notes on the data:
- Log lines are interleaved with shell trace (`+ echo ...`), and long
  `Dependencies:` lines can wrap onto indented continuation lines. The
  parser handles this; if the format of `--buildinfo` changes, fix the
  parser first.
- Jobs with no buildinfo (ABI checks, clang-format, anything with
  `skip_tests`) are excluded; say so in the report.
- Duplicate entries in `Dependencies:` (libdeflate, zstd, ZLIB, Ptex) are
  normal; the first wins.
- `NONE` means looked for and not found; a dependency that is absent from
  the list was not requested or disabled. `Ptex present` means the version
  was not detected. `JPEG 80` is the libjpeg API version, not a release
  (so it says nothing about jpeg 9 or turbo version).
- buildinfo does not report CMake; the parser takes it from the
  `-- CMake X.Y.Z` (or `-- Using CMake version X`) status line early in the
  build step of the log and
  adds it to deps as `CMake`.
- buildinfo does not report NumPy. It labels `__cplusplus` 202400 as C++23
  even for C++26 builds (cosmetic).
- A version reported for a main/master build is that tree's own version
  string (e.g., libtiff master may still say the last release), so
  "master" cannot be confirmed from the summary alone. Check the job log
  for `git checkout master`/`main` lines when it matters.

## Step 2b: the local build

Also gather what the current local tree builds with, since the developer's
machine may have compilers or dependencies newer than CI can get.

- Find built tools:
  `find dist build -maxdepth 4 -name oiiotool -type f 2>/dev/null`. If
  several, or none, ask which to use (or whether to build first).
- Run `<oiiotool> --version` and `<oiiotool> --buildinfo`; parse the same
  way as CI (compiler line and `Dependencies:`). Record the binary's path
  and modification time, and compare its version to HEAD; warn if it
  looks stale.
- `<build>/CMakeCache.txt` gives compiler paths, `CMAKE_CXX_STANDARD`, and
  `*_ROOT`/`*_DIR` hints if a version is ambiguous; `cmake --version` gives
  the local CMake.
- Local Python and NumPy versions (`python3 -c 'import sys,numpy'`) if the
  bindings were built.
- Treat the local build as unverified evidence: it may raise a documented
  maximum only after user confirmation (step 7).

## Step 3: read the claims

- `INSTALL.md` "Dependencies" section: min, and "tested through" (and
  main/master) per dependency and per compiler/CMake.
- `.github/workflows/ci.yml`: every matrix entry's `desc`, comments, and the
  version-pinning env vars in `setenvs` (e.g. `LIBPNG_VERSION`,
  `LIBTIFF_VERSION`, `PTEX_VERSION`, `*_GIT_TAG`, `*_BUILD_VERSION`,
  `*_ver:` inputs, `container:`, `runner:`).
- Defaults for locally built deps live in `src/cmake/build_*.cmake`
  (`*_BUILD_VERSION`) and `src/build-scripts/build_*.bash`. Check that each
  pin in `ci.yml` uses the variable name the script actually reads.

## Step 4: find latest upstream releases

Query real upstream state; do not trust memory. Use tags (many projects
have no GitHub "releases"), filter out rc/beta/dev, `sort -V | tail`.
Also confirm with `gh api repos/<r>/releases --jq` where releases exist.

| Dependency | Where to look |
|---|---|
| fmt | fmtlib/fmt |
| Imath, OpenEXR, OCIO, OpenVDB | AcademySoftwareFoundation/{Imath,openexr,OpenColorIO,openvdb} |
| libtiff | GitLab: `https://gitlab.com/api/v4/projects/libtiff%2Flibtiff/repository/tags` |
| libjpeg-turbo | libjpeg-turbo/libjpeg-turbo (tags also include jpeg-9x/10; ignore) |
| zlib | madler/zlib |
| robin-map | Tessil/robin-map |
| pugixml | zeux/pugixml |
| nanobind | wjakob/nanobind (drop `-dev` tags) |
| pybind11 | pybind/pybind11 |
| libpng | pnggroup/libpng (tags only, `^v1\.6\.[0-9]+$`) |
| LibRaw | LibRaw/LibRaw |
| OpenJPEG | uclouvain/openjpeg (tags `^v[0-9]`) |
| libjxl | libjxl/libjxl |
| libheif | strukturag/libheif |
| DCMTK | DCMTK/dcmtk (`DCMTK-x.y.z`) |
| WebP | webmproject/libwebp |
| Ptex | wdas/ptex |
| Freetype | freetype/freetype (`^VER-2-[0-9]+-[0-9]+$`) |
| libultrahdr | google/libultrahdr |
| OpenJPH | aous72/OpenJPH |
| TBB | uxlfoundation/oneTBB |
| OpenCV | opencv/opencv (numeric tags) |
| ffmpeg | FFmpeg/FFmpeg (`^n[0-9.]+$` tags) |
| giflib | `https://sourceforge.net/projects/giflib/best_release.json` |
| Qt | qt/qtbase (`^v6\.[0-9]+\.[0-9]+$`; Qt5 is EOL, use INSTALL) |
| pystring | imageworks/pystring |
| NumPy | numpy/numpy |
| Python | python.org; CI uses runner/container Pythons |

## Step 5: analyses to perform

A. Per-dependency table, with exactly these columns:

   ```
   | Dependency | INSTALL min/max | CI versions | Latest |
   |---|---|---|---|
   | libFoo | 1.5 - 3.2 | 1.5.2, 1.8.4, 2.0.5, 3.0.1, 3.2.1, main | 3.2.1 |
   | libBar | *3.0* - *4.0* | 3.1, 3.2, 3.8 | *4.2* |
   ```

   - "INSTALL min/max": the documented minimum and maximum tested.
     Bold the min or max (`*x*` in the example above; use `**x**` in
     the real report) if it does NOT reflect what CI tests as its min or
     max, respectively.
   - If INSTALL gives only major.minor, any tested patch release of that
     major.minor satisfies it (INSTALL min 3.1 is met by CI 3.1.x). If
     INSTALL gives a full major.minor.patch, the table must flag it
     (bold) when CI does not test that exact release.
   - "CI versions": every distinct version tested by any CI job (from the
     buildinfo pivot), ascending, with `main`/`master` last if a job
     really builds from it (see step 2 notes). Include compilers (gcc,
     clang, Apple clang, MSVC, icx), CMake, Python. CMake versions come
     from `parsed.json`/`versions.txt` (key `CMake`, taken from the build
     log; see step 2 notes), so list them like any other dependency and
     never report CMake as "not verified". Ignore jobs that do not build
     (clang-format).
   - "Latest": the highest tagged release upstream (step 4), if it can be
     detected. Bold it if it is newer than anything CI tests.

B. "oldest" jobs (`desc` starts with `oldest`, and `hobbled`): is every
   dependency at the INSTALL minimum? List each that is not, and why
   (system package, pin ignored, pin commented out, dependency not
   installed, different pins between the gcc and clang variants).

C. "latest releases" jobs: is every dependency at the newest tagged
   release from step 4? List each that is not. Mark ones that are apt or
   Homebrew system packages versus pinned by us.

D. "bleeding edge" job: which dependencies are on main/master, which fall
   back to the latest tag, which are neither (system packages), and which
   are disabled.

E. Description check: for every matrix entry, compare the `desc` and the
   comments against the summary (compiler and version, C++ std, Python,
   OpenEXR, OCIO, Qt, simd, and any "main"/"oldest"/"latest" claim).
   Runner-image compilers drift (Apple clang, gcc from apt), so the
   summary, not the name, is right. Also check `nametag`s for stale
   version strings.

F. INSTALL.md claims of "main"/"master": each must correspond to a job that
   really builds that dependency from main/master.

G. INSTALL.md minimum claims that no job exercises, and jobs that pass
   below the documented minimum (e.g., ASWF containers shipping an older
   system library).

H. Pin sanity: every version-pinning variable in `ci.yml` must be one the
   build scripts read (see step 3), and must show up in the summary.

I. Local build (from step 2b): for each compiler, CMake, or dependency
   where the local tree's build is NEWER than the newest CI job achieves,
   list it as a candidate documented maximum (needs user confirmation before
   INSTALL.md is changed, see step 7). Also note anything local that is
   older than INSTALL's minimum.

Add to this list when we think of new checks.

## Step 6: report (phase 1 ends here)

Write `./dependency-report-YYYY-MM-DD.md` (today's date) with: run URL and
SHA; local build info (step 2b); table A; sections B-I; a "Not verified"
list (things buildinfo cannot show). Show a concise version to the user.
Be concrete: name job, variable, version. Do not publish externally unless
asked. Do not commit the report.

## Step 7: simple fixes, done unconditionally (phase 2)

These do not change what CI does, and are trivially revertible by the user
before committing, so make them without asking. Do not commit them.

1. `ci.yml` `desc:` strings, `nametag:` version strings, and `#` comments
   must reflect what each job actually tests per its summary (E). Say in
   your final message that `desc` is the job name, so renaming can break
   required-status-check rules.
2. `INSTALL.md` must reflect the actual maximum tested version:
   - Where CI tests something newer than INSTALL's "tested through",
     update it.
   - Where INSTALL says "and main/master", keep it only if a job really
     builds that dependency from main/master; otherwise remove it. Add
     "and main/master" where a job really does.
   - Local-build maxima (step 2b, analysis I): FIRST list every case where
     the local tree's build uses a compiler or dependency newer than the
     newest CI achieves, and ask the user to confirm that these should be
     recorded as the documented maximum on the assumption that they work.
     Edit INSTALL.md only for the confirmed ones. (Do the CI-based edits
     without waiting for this answer.)
   - Do NOT lower claims that CI merely fails to support (a documented max
     newer than any tested build, e.g. ffmpeg) unless the user asks; list
     them in the summary instead.

## Step 8: fixes that change code or workflows (phase 3)

These change behavior of CI or the build. Never batch them and never apply
them without a decision.

1. Build a numbered list of candidate fixes from analyses B, C, D, G, H (for
   example: pin variable with the wrong name, missing or commented-out
   pin, job not at its stated minimum or latest, system package instead of
   a pinned version, disable flag that has no effect). For each: what is
   wrong, evidence (job, variable, log line), proposed change, risk.
2. Present the list and let the user pick items ONE AT A TIME (use
   AskUserQuestion, or ask in prose). Apply only the chosen item, then
   show the diff before moving to the next.
3. Ask before committing anything. Follow the repo and user commit rules
   (only an `Assisted-by:` trailer, terse messages). One commit per fix,
   separate from the phase 2 doc edits unless the user says otherwise.
4. Record any newly understood "right way to fix it" in the "Known issues"
   section of this file.

## Known issues from past audits (update this list)

From the 2026-09-29 audit (all still to be resolved unless noted):
- `oldest` jobs used to set `BUILD_PNG_VERSION=1.6.0`, which nothing reads,
  so system libpng is used. Renaming it to `LIBPNG_VERSION=v1.6.0` made
  CI fail (building libpng 1.6.0 broke other dependencies on those
  machines), so the useless line was just deleted (2026-10). libpng in
  oldest/hobbled stays the system version (1.6.37); do not try to pin it.
- `hobbled` sets `USE_JPEGTURBO=0` yet libjpeg-turbo still appears in the
  summary.
- ASWF container jobs use libtiff 4.0.9, below INSTALL's minimum of 4.1.
- Latest-release jobs use Ubuntu apt Imath 3.1.9 with OpenEXR 3.5.x, and
  apt ffmpeg/giflib/libheif/TBB/OpenCV/zlib/Python that are far older
  than the newest releases.
- `bleeding edge` briefly pinned OpenEXR to a tag (lj2k bug); restored to
  `main` on 2026-09-30. Verify on the next audit that the summary shows a
  main-built OpenEXR, and whether Imath really follows main (the 09-29 run
  found system Imath 3.1.9 even with OpenEXR built from a tag, since
  `build_openexr.bash` may use a system Imath if found).
- Imath: `build_openexr.bash` (used when a job sets `openexr_ver`) now
  builds the Imath that OpenEXR prefers (`OPENEXR_FORCE_INTERNAL_IMATH=ON`,
  installed next to OpenEXR) and exports `Imath_ROOT` so OIIO uses it.
  What "prefers" means varies: v3.1.0 -> Imath v3.1.1, v3.4.x -> v3.2.2,
  v3.5.x and main -> Imath `main` (override with
  `-DOPENEXR_IMATH_TAG=...` in `OPENEXR_CMAKE_FLAGS`). Imath main reports
  its version as 3.2.0. Jobs with no `openexr_ver` (ASWF containers) still
  use the container's Imath. Added 2026-09-30; confirm on the next audit
  that summaries show the expected Imath. A separate `IMATH_VERSION`
  selector was tried and dropped as unnecessary for now.
- OIIO's own OpenEXR auto-build (`build_OpenEXR.cmake`) passes
  `-D Imath_DIR=...` so OpenEXR uses the same Imath as OIIO.
- OpenJPH is always the `build_openjph.cmake` default; no job tests an
  older OpenJPH.
- Minimums not tested by any job: libjpeg 9, libjpeg-turbo 2.1, zlib
  1.2.7 to 1.2.10, OpenJPEG below 2.4, OpenVDB 9, TBB 2018 to 2019,
  giflib 5.0, libheif below 1.19, libjxl below 0.11.1, DCMTK below 3.7,
  Qt5 below 5.15, ffmpeg below 4.4, clang 10, MSVS 2017/2019.

## Lessons learned (update this list)

- The sandbox blocks Bash writes (chmod, heredocs) under `.agents/skills`;
  edit skill files with the Edit/Write tools and run scripts via
  `bash`/`python3` (no exec bit needed).

- To confirm "bleeding edge" really builds main/master: grep the job log
  with `grep -o -E "git clone [^ ]+ [^ ]+( -b;[a-z]+)?|git checkout [a-zA-Z0-9._-]+ --force"`
  (strip the log prefix first). A reported version equal to the last
  release is normal (the 2026-10-03 check: LibRaw master still says
  0.22.0 because 0.22.x patch releases come from a branch; pugixml,
  libtiff, Freetype, OpenJPEG, pybind11 master headers also match their
  latest tag). Compare against the upstream master's version header.
  Never pinned to main there: libjxl, robin-map, OpenJPH (tags/defaults).
- `gh run view --log` for a whole run fails; use `--job <id>` per job.
- Grep the tag lists carefully: naive `sort -V | tail` on libjpeg-turbo,
  freetype, openjpeg, ffmpeg, nanobind returns non-release tags.
- Version strings in job names (`desc`) and the `nametag` drift whenever a
  runner image or default dependency version changes.
