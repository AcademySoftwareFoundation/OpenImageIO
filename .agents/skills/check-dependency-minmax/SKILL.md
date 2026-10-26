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
update this skill as part of that work (see "Project-specific files").

Arguments: `$ARGUMENTS`
- First argument (optional): a GitHub Actions run URL or numeric run id for
  the `ci.yml` workflow. If omitted, use the latest SUCCESSFUL `ci.yml` run
  on the current git branch (see step 1).

## Project-specific files

This skill is shared by OpenImageIO (OIIO) and Open Shading Language (OSL),
whose build, test, and CI scripts are intentionally very similar.
`SKILL.md` and `scripts/` must stay IDENTICAL in both projects. Everything
that differs between them lives in `assets/`, whose contents are expected
to differ:

- `assets/project.md`: facts about this project. Read it before step 1.
  The buildinfo tool, the CI job naming conventions, dependency-specific
  quirks of the buildinfo data, the key dependencies to check in job
  descriptions, and where to find each dependency's latest upstream
  release (step 4).
- `assets/known-issues.md`: problems found by past audits of this project,
  their status, and the right fixes once known. Read it before step 5 and
  update it at the end (step 8).
- `assets/lessons-learned.md`: project-specific tips for running the audit.
  Read it before step 1, and add to it when something is learned.

When updating the skill, decide where the change belongs. A better method
or a new check that would apply to both projects goes in `SKILL.md` or
`scripts/`, and should be copied to the other project too (tell the user,
who will propagate it). A fact about a particular dependency, job, or past
audit of this project goes in `assets/`. Never put project names, tool
names, or dependency-specific details in `SKILL.md` or `scripts/` except as
clearly marked examples.

## Ground truth

- Each `ci.yml` job runs `src/build-scripts/ci-test.bash`, which runs the
  project's buildinfo tool (`<tool> --buildinfo`; the tool is named in
  `assets/project.md`) and appends its output to the job's step summary
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
- The sandbox blocks Bash writes (chmod, heredocs) under `.agents/skills`;
  edit skill files with the Edit/Write tools and run scripts via
  `bash`/`python3` (no exec bit needed).
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

`gh run view <run> --log` fails ("too many API requests"); fetch per job
(`--job <id>`), which the script below does.

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
  parser first. The parser finds the buildinfo block by its first line,
  `<PROJ> <version> | <platform>`, so it needs no project-specific setting.
- Jobs with no buildinfo (ABI checks, clang-format, anything with
  `skip_tests`) are excluded; say so in the report.
- Duplicate entries in `Dependencies:` are normal; the first wins.
- `NONE` means looked for and not found; a dependency that is absent from
  the list was not requested or disabled. A dependency with no version
  (`present` in the parsed data) was found but its version was not
  detected.
- buildinfo does not report CMake; the parser takes it from the
  `-- CMake X.Y.Z` (or `-- Using CMake version X`) status line early in the
  build step of the log and adds it to deps as `CMake`.
- Further quirks of particular dependencies (versions that are API levels
  rather than releases, dependencies buildinfo does not report, and so on)
  are listed in `assets/project.md`.
- A version reported for a main/master build is that tree's own version
  string (e.g., a dependency's master may still say the last release), so
  "master" cannot be confirmed from the summary alone. When it matters,
  grep the job log (strip the log prefix first) with
  `grep -o -E "git clone [^ ]+ [^ ]+( -b;[a-z]+)?|git checkout [a-zA-Z0-9._-]+ --force"`
  and compare against the upstream master's version header.

## Step 2b: the local build

Also gather what the current local tree builds with, since the developer's
machine may have compilers or dependencies newer than CI can get.

- Find the built buildinfo tool (named in `assets/project.md`):
  `find dist build -maxdepth 4 -name <tool> -type f 2>/dev/null`. If
  several, or none, ask which to use (or whether to build first).
- Run `<tool> --buildinfo` (and `--version` if the tool has it); parse the
  same way as CI (compiler line and `Dependencies:`). Record the binary's
  path and modification time, and compare its version to HEAD; warn if it
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
  version-pinning env vars in `setenvs` (e.g. `<DEP>_VERSION`,
  `*_GIT_TAG`, `*_BUILD_VERSION`, `*_ver:` inputs, `container:`,
  `runner:`).
- Defaults for locally built deps live in `src/cmake/build_*.cmake`
  (`*_BUILD_VERSION`) and `src/build-scripts/build_*.bash`. Check that each
  pin in `ci.yml` uses the variable name the script actually reads.

## Step 4: find latest upstream releases

Query real upstream state; do not trust memory. Use tags (many projects
have no GitHub "releases"), filter out rc/beta/dev, `sort -V | tail`.
Also confirm with `gh api repos/<r>/releases --jq` where releases exist.
Check the tag lists by eye: many projects have tags that are not releases
or that do not sort well, so a naive `sort -V | tail` can be wrong.

Where to look for each dependency, with any tag filter it needs, is the
table in `assets/project.md`. If a dependency is missing from it, find its
upstream and add it there.

## Step 5: analyses to perform

Use `assets/known-issues.md` as context: say whether each known issue is
still present, fixed, or changed.

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

B. "oldest" jobs (see `assets/project.md` for which jobs claim to be
   oldest/minimum): is every dependency at the INSTALL minimum? List each
   that is not, and why (system package, pin ignored, pin commented out,
   dependency not installed, different pins between the gcc and clang
   variants).

C. "latest releases" jobs: is every dependency at the newest tagged
   release from step 4? List each that is not. Mark ones that are apt or
   Homebrew system packages versus pinned by us.

D. "bleeding edge" job: which dependencies are on main/master, which fall
   back to the latest tag, which are neither (system packages), and which
   are disabled.

E. Description check: for every matrix entry, compare the `desc` and the
   comments against the summary (compiler and version, C++ std, Python,
   the key dependencies listed in `assets/project.md`, simd, and any
   "main"/"oldest"/"latest" claim). Runner-image compilers drift (Apple
   clang, gcc from apt), and so do default dependency versions, so the
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
SHA; local build info (step 2b); table A; sections B-I; status of each
known issue; a "Not verified" list (things buildinfo cannot show). Show a
concise version to the user. Be concrete: name job, variable, version. Do
not publish externally unless asked. Do not commit the report.

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
     newer than any tested build) unless the user asks; list them in the
     summary instead.

## Step 8: fixes that change code or workflows (phase 3)

These change behavior of CI or the build. Never batch them and never apply
them without a decision.

1. Build a numbered list of candidate fixes from analyses B, C, D, G, H (for
   example: pin variable with the wrong name, missing or commented-out
   pin, job not at its stated minimum or latest, system package instead of
   a pinned version, disable flag that has no effect). For each: what is
   wrong, evidence (job, variable, log line), proposed change, risk.
   Check `assets/known-issues.md` first: a fix tried before and reverted
   must not be proposed again unless something has changed.
2. Present the list and let the user pick items ONE AT A TIME (use
   AskUserQuestion, or ask in prose). Apply only the chosen item, then
   show the diff before moving to the next.
3. Ask before committing anything. Follow the repo and user commit rules
   (only an `Assisted-by:` trailer, terse messages). One commit per fix,
   separate from the phase 2 doc edits unless the user says otherwise.
4. Update `assets/known-issues.md` with this audit's date: add new issues,
   mark resolved ones, and record any newly understood "right way to fix
   it". Add anything learned about running the audit itself to
   `assets/lessons-learned.md`, or to this file if it applies to both
   projects (see "Project-specific files").
