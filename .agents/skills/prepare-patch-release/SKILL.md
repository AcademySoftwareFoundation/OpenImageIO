---
name: prepare-patch-release
description: Do all the tasks needed for a patch release of the project.
argument-hint: [new-version]
---

Do all the tasks needed for a patch release of the project. These patch
releases generally happen on the first day of every month.

Arguments: `$ARGUMENTS`
- First argument (optional): the version of the upcoming release we are
preparing. If omitted, find the most recent tag on this branch, and the
new version will update the third portion of the version. It might be
either a version number (like `3.1.4.0`) or a tag (like `v3.1.4.0`).

Hint: The numeric "version" is a four-part numeric deignation with the pattern
`MAJOR.MINOR.PATCH.TWEAK`. The "version tag" is usually the numeric version
with a "v" prepended, for example, `v3.1.4.0`.

## Steps and Checklist

- [ ] Determine the version of the new release.
- [ ] Update the main @CMakeLists.txt.
- [ ] Use the "release-notes-update" skill to update @CHANGES.md.
- [ ] Review the release notes to ensure that the changes do not deprecate any
      API calls or break API or ABI backward-compatibility, or remove support
      for any dependency or toolchain versions. If you think these rules are
      being violated, ask for confirmation.
- [ ] Review @README.md for changes.
- [ ] Review @INSTALL.md for changes.
- [ ] Review @CREDITS.md for changes.
- [ ] Review @SECURITY.md for changes.

## Steps to determine the versions of the last and new releases.

1. **Determine the previous tag** if not provided by looking at the 
   most recent git tag in the current branch, and deducing the 4-part
   version number from that.
2. The assumed new version for a patch release has the same major and minor
   numbers, the patch number is incremented by one since the last tag, and the
   tweak number is "0".
3. If no optional version was present in the arguments, use the assumed new
   version.
4. If a version was supplied in the arguments, double check that it differs
   from the last tag as explained above. If it does not, ask for verification
   that the version requested is correct before proceeding.

## Steps to update CMakeLists.txt

1. In the main @CMakeLists.txt, alter the version to that of the new release,
   if it isn't already the same.
2. For a patch release, ensure that `${PROJECT_NAME}_SUPPORTED_RELEASE` should
   be set to ON. In main (not yet a supported release) it shold be OFF. If you
   find these to not be as expected, ask for confirmation of whether to fix.
3. For a patch release, `PROJECT_VERSEION_RELEASE_TYPE` should be set to empty
   (""). In main, it will typically be "dev", "beta", or other designations.
   If you find these to not be as expected, ask for confirmation of whether to
   fix.

## Reviewing README.md

If this release added support for any new image file formats, be sure that
README.md mentions them in its list of image formats supported. 

## Reviewing INSTALL.md

If any of the new commits (as described in the release notes for this version)
appear to add dependencies, or add support for new versions of dependencies,
be sure that INSTALL.md is updated to include that dependency (if not already
listed among the required and optional dependencies), and reflects the latest
version that we claim to support.

Double check @externalpackages.cmake and ensure that any minimum required
versions of dependencies that cmake will enforce match the oldest versions
of those dependencies as documentd in INSTALL.md.

## Reviewing CREDITS.md

The @CREDITS.md file lists all known contributors to the project, sorted alpha
by first name. In cases where an author's actual name is unknown, we use the
GitHub userid.

Ensure that any authors referenced in the new release notes for the version we
are releasing are inserted in the credit list, if they are not already
present. You don't need to check older versions, we presume those have already
been included.

## Reviewing SECURITY.md

The @SECURITY.md file lists which versions are currently supported at what
levels, and lists all previously-fixed SVE's or security advisories. Be sure
to check this file in older branches (the last two releases, say), and if you
are preparing a patch release, also check in main and more recent (higher
numbered) releases for more recently modified SECURITY.md, and be sure that
this branch gets amended with any information that seems to have been updated
more recently in those other branches. Check whichever is newer of the local
and remote copy of that branch, since the local one may have had release notes
or its version in CMakeLists.txt updated, but not yet pushed to GitHub.


## Reference

- Full release procedures: `docs/dev/RELEASING.md`
- Steps for updating release notes: `release-notes-update/SKILL.md`
