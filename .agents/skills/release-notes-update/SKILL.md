---
name: release-notes-update
description: Generate or update release notes for a patch, minor, or major release, or just to update main. Run git-cliff, organize and edit the output per project conventions, and insert into CHANGES.md.
argument-hint: [patch|minor|major|main] [prev-tag]
---

Generate release notes for an OpenImageIO release.

Arguments: `$ARGUMENTS`
- First argument (optional): release type — `patch` (default), `minor`, `major`, or `main`.
- Second argument (optional): previous release tag (e.g. `v3.1.2.0`). If omitted, find the most recent tag automatically. If the release type is `main`, instead of a tag, look for the last commit at which the CHANGES.md file was updated.

## Steps

1. **Determine the previous tag** if not provided:
   ```
   git describe --tags --abbrev=0
   ```
   or look at recent tags: `git tag --sort=-version:refname | head -10`.
   However, if the "release type" is `main`, instead of a tag, just find
   the commit at which CHANGES.md was last updated.

2. **Run git-cliff** to get raw commit data:
   ```
   git cliff -c src/doc/cliff.toml <prev-tag>..HEAD > /tmp/cliff-out.md
   ```
   Read `/tmp/cliff-out.md` to see the raw output.

3. Look up the GitHub user ID's for each author, which should be discernable
   from the posted PRs (but not from the local commits), and change the git
   cliff output that lists the author's name as `(by First Last)` to `(by
   githubid / First Last)`.

4. **Read CHANGES.md** to see the current top of the file and understand where to insert.

5. **Format the release notes** according to the release type:

### For patch releases:

Follow the skeleton in `docs/dev/Changes-skeleton-patch.md`:

```
Release X.Y.Z.W (Month DD, YYYY) -- compared to X.Y.Z.W-1
---------------------------------------------------------
- *category*: Description. [#NNNN](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/NNNN) (by @authorid / author-full-name)
```

Rules:
- **Remove** the section headings git-cliff generates; patch notes are a flat
  list.
- **Add conventional commit prefixes** to any "uncategorized" entries (those
  lacking a `feat:`, `fix:`, etc. prefix).
- **Reorder** entries logically: feature enhancements first, then bug fixes,
  then build/CI fixes, then internal changes, then test improvements, then
  docs/admin.
- **Omit** entries that are purely internal and too minor to matter to users.
  Ask for confirmation about entries you propose to omit.
- Prefer to use author's actual name if known. If the name cannot be found,
  the GitHub userid can be used instead.
- Omit the author if it is the project leader, Larry Gritz, unless he is not
  the dominant author (at least 75% of commits) in this release.
- Keep entries to one line each. Be terse but informative.
- Use the format `*subsystem*:` for the category prefix (e.g., image file
  format like `*exr*:`, utility like `*oiiotool*:`, class or API category like
  `*IBA*:`, or topic category such as `*build*:`, `*ci*:`, `*docs*:`).
- We aim to make patch releases on the first day of each month. If we are
  within a few days of a month end, list the date as the beginning of the
  upcoming month. Ask for confirmation that this is the planned release date.


### For minor or major releases:

Follow the skeleton in `docs/dev/Changes-skeleton-major.md`. Sections:

```
Release X.Y.0.0 (Month, YYYY) -- compared to X.Y-1
--------------------------------------------------

### New minimum dependencies and compatibility changes:
### ⛰️  Major new features and public API changes:
  * *New image file format support:*
  * *oiiotool new features and major improvements*:
  * *Command line utilities*:
  * *API changes*
  * Other notable new feature:
### 🚀  Performance improvements:
### 🐛  Fixes and feature enhancements:
### 🔧  Internals and developer goodies
### 🏗  Build/test/CI and platform ports:
  * CMake build system and scripts:
  * Dependency and platform support:
  * Testing and Continuous integration (CI) systems:
### 📚  Notable documentation changes:
### 🏢  Project Administration
### 🤝  Contributors
```

Note that the section outline may already be present, in which case you
only need to fit items into the existing category outline.

Rules:
- Group commits into sections; within each section, cluster related items together.
- When needed, expand terse one-liners into enough prose that users understand what changed and why it matters.
- For `feat:` commits, make sure the feature is explained sufficiently — don't just copy the commit subject.
- For `api:` or `api!:` commits, clearly call out what changed in the public API.
- Include PR links and author attribution for every entry.
- The notes should "tell the story" of the release, not just be a dump of commit subjects.
- We aim to make major/minor releases approximately in October 1 of each year. If the anticipted release date is already in the file, don't change it. If it is not present, ask for confirmation of the planned release date.

### For updating release notes in main:

Rules:
- Generally, follow the rules for "major/minor releases", except as noted in other items below.
- Don't apply a new skeleton of category listings unless they are not present for the upcoming release.

5. **Insert the formatted notes** into `CHANGES.md` in the appropriate place (as detailed below). Leave the existing content intact.
- When updating `main` or preparing a `major` or `minor` release, insert the updates in the top section for the upcoming major/minor release. Insert a new set of category sections only if it's not already present.
- When doing a `patch` release, insert the changes immediately above the last patch release of that branch, so that CHANGES.md lists releases in descending numerical (version) order.
- When porting a set of release notes from a release branch into main, or from an older (obsolete) release branch to the current release branch, insert it into the right place to maintain overall descending order.

6. **Double check that the notes are adequately descriptive.** (See more
   detailed description of this step below.)

7. **Forward-port release notes from release branches if needed**

8. **Show a summary** of what was inserted and ask the user to review before finalizing.

## Forward-porting release notes

Release notes may have been generated independently in main, and release
branches. When updating one branch, we ensure that any changes from older
branches have been incorporated.

- When preparing `patch` release notes, check the CHANGES.md file in the
  "dev-X.(Y-1)" branch for the previous minor release family to identify any
  X.(Y-1).Z patch release notes that are not reflected in the current release
  notes that we are updating.
- When updating `main` or doing a `major` or `minor` release, check the
  CHANGES.md file in both the "dev-X.(Y-1)" and "dev-X.(Y-2)" branches.
  Check whichever is newer of the local and remote copy of that branch, since
  the local one may have had release notes or its version in CMakeLists.txt
  updated, but not yet pushed to GitHub.
- If any patch releases are present in the older dev branches checked, insert
  the release notes for those patch releases into the right positions in the
  current release notes that we are updating.
- If a change is forward-ported in this manner and the same PR is an update in
  the current set of changes we are updating as our main task, document that
  in the current set of notes using the following convention: in the line of
  the notes, the explanation of the version where it appears should reflect
  the first version of all branches where it appeared, for example, `(3.2.0.0,
  3.1.3.0, 3.0.8.0)` to indicate that the patch was added to each of those
  versions. The versions should be listed in descending order.

## Useful abbreviations for category labels

| Abbrev | Meaning |
|--------|---------|
| IB | ImageBuf |
| IBA | ImageBufAlgo |
| IC | ImageCache |
| TS | TextureSystem |
| oiiotool | the oiiotool command |
| build | CMake/build system |
| deps | Changes to accommodate dependency or toolchain changes |
| ci | CI/GitHub Actions |
| docs | documentation |
| int | internal/refactor |
| test | testsuite or unit tests |
| HEADER.h | Developer utilities in a public header file |

## Combining PRs into single entries

To be more concise and easier to read, within a release's notes, related
PRs/commits can be combined into a single bullet-point line, which would look
like
```
- *category*: Combined Description. [#NNNN1](URL) (by author1) [#NNNN2](URL2) (by author2)  [#NNNN2](URL2) (by author2)

```
if fully combined, or if explained one by one,
```
- *category*: Description 1 [#NNNN1](URL) (by author1), amendment 2, [#NNNN2](URL2) (by author2), amendment 3, [#NNNN3](URL3) (by author3)
```
Choose the fully combined or explained one by one based on which is more clear
to the reader.

If the authors are all the same, only have the author designation at the end.

Here are the cases where it's ok to combine commits in this way:
- If it is clear that multiple PRs are part of the same feature or fix,
  consisting of an initial commit, and subsequent smaller changes or
  continuations of the same topic.
- An initial commit, and a subsequent commit that is obviously a fix to a bug
  in the initial commit.
- "CI" changes that all only add new cases to the test matrix.
- "CI" changes that all only fix spontaneous breakages in the GitHub runners.
- "Build" changes that are all just minor updates to versions of dependencies
  that we support or test against.

Some more examples of combined commit messages:

```
  - feat: Add GPS metadata functionality for TIFF [PR1](PR URL 1) [PR2](PR URL 2) [PR3](PR URL 3) (by author).
  - ci: New CI variants for MSVS 2026 [PR1](PR URL 1) (by author1), VFX Platform 2027 [PR2](PR URL 2) (by author2).
  - ci: Various fixes for unexpected changes to GitHub Actions runners [PR1](PR URL 1), [PR2](PR URL 2) (by author)
  - build: Added support for gcc 15 [PR1](PR URL 1) (by author1), OpenEXR 3.5.1 [PR2](PR URL 2) (by author2), libtiff 4.8 [PR3](PR URL 3) (by author3).
```

Always ask for confirmation before combining commits in this manner. Confirm
separately for each proposed combination of a group of multiple original
commits into a single commit. Give the user the opportunity to revise the
combined description or request other changes for the grouping.

## Double check that the notes are adequately descriptive

For newly added items for this release, read the short descriptions provided
by git-cliff, double check them against the full commit messages to be sure
the one-line summary is adequate. If the summary is misleading, too brief and
leaving out the fact that an important thing was changed, or not adequately
capturing the scope of changes, feel free to propose an alternate wording that
will make it more clear to readers what changed as a result of the PR. Ask for
confirmation on these and explain why you felt the one-line description wasn't
enough.

One example of a case where this is needed is if the one line description
merely mentions a new image processing capability by name, but the full commit
message makes it clear that a new ImageBufAlgo API call was added, and also a
new oiiotool command line argument was added, and that couldn't all be
described in the one-line brief message of the commit.

## Reference

Full release procedures: `docs/dev/RELEASING.md`
Patch skeleton: `docs/dev/Changes-skeleton-patch.md`
Major skeleton: `docs/dev/Changes-skeleton-major.md`
Example good patch notes: https://github.com/AcademySoftwareFoundation/OpenImageIO/releases/tag/v3.0.0.0
Example good major/minor notes: https://github.com/AcademySoftwareFoundation/OpenImageIO/releases/tag/v3.1.6.1
