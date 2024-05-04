<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Release Procedures for OpenImageIO

## Version nomenclature and release cadence

### The meaning of the version parts

Our release numbers are of the form `MAJOR.MINOR.PATCH.TWEAK`, and we follow
an almost-semantic-versioning scheme.

MAJOR changes are those that break backward-compatibility of the public APIs,
requiring existing application code to change their source code where it calls
OIIO API functions in order to compile and work correctly.

MINOR changes guarantee API backward-compatibility with prior releases that
have the same major number, but can introduce ABI or link incompatibilities.
Applications using OIIO need to be recompiled to move to a new minor release,
but should not need to change their source code. Minor releases are allowed
to drop support for sufficiently old compilers and dependencies.

PATCH changes guarantee both API and ABI/link backward-compatibility with
prior releases that have the same minor number. Features and new public API
calls may be added (thus deviating from strict semantic versioning), but only
if they preserve ABI/link compatibility. This means that standalone functions
or static methods may be added, but existing functions will not change their
parameters, and no new virtual methods or struct data layouts change for patch
releases (for classes in public header files). Application code can be
re-linked to a new patch release without needing to be recompiled. We also
guarantee that any compilers or dependencies supported for the first release
of a minor series will continue to be supported for all patches to that minor
series.

TWEAK changes are restricted to critical bug fixes or build breaks that do
not alter public APIs in any way.

### Cadence of releases

Major (arbitrarily incompatible) releases happen rarely, usually with multiple
years between them.

Minor (API backward-compatible) releases are scheduled annually, targeted at
late summer (SIGGRAPH time) to be in beta, and early autumn for a full
release.

Patch (API and ABI backward-compatible) releases are scheduled monthly,
usually aiming for the first day of each month.

Tweak (fully forward and backward compatible) releases are not regularly
scheduled, and only happen sporadically, when a critical fix is needed in
between scheduled patch releases.

### Things are different in "master"

The "master" branch is where ongoing development occurs without any
compatibility guarantees. We don't always know if the next year's release from
master will turn out to be major (API breaking) or minor (only ABI breaking).

Some studios or products need the most up-to-date enhancements, and thus tend
to build OIIO from the "master" branch rather than using tagged releases. For
these users, we try to set version numbers and occasionally tag in master, to
send a signal about which points in master seem safe to rely on various
compatibility levels. However, the rules are slightly different than we use
for releases.

In the "master" branch:

- TWEAK changes within "master" only guarantee ABI back-compatibility, but may
  have additions or non-bug-fix behavior changes (akin to patch releases
  within a release branch).
- PATCH changes within "master" are allowed to break ABI or API (changes that
  would require minor or major releases, if they were not in "master").
- Beware of unmarked breaks in compatibility of commits in master that are
  in between developer preview tags.


---

## Branch and tag names

**Branches** are places where commits containing new development may be added
(and thus the branch markers will move over time). Branch names obey the
following conventions:

- `master` is the area for arbitrary changes intended for the next year's
  major or minor release.
- `dev-1.2` are the areas for staging additions to the next month's patch
  release for the designated minor version series.
- `release` is a special branch marker that is always set to the latest tag of
  the currently supported, stable release family. People can count on
  "release" always being the latest stable release, whatever that may be at
  the time.

**Tags** indicate releases or developer previews, and once a commit is
tagged, it is permanent and will never be moved. Tag names obey the following
conventions:

- `v1.2.3.4` is a specific public stable release, corresponding to official
  major, minor, public, or tweak releases.
- `v1.2.3.4-alpha`, `v1.2.3.4-beta`, or `v1.2.3.4-RC1` are pre-releases for an
  upcoming public stable release, at the alpha, beta, or release candidate
  stages, respectively.
- `v1.2.3.4-dev` is a "developer preview" within "master." It is not an official
  supported release, and while the numbering may indicate compatibility versus
  other recent developer previews, they should not be assumed to be compatible
  with any prior official stable releases.

*Very* occasionally, other branch or tag names (with custom suffixes or
different naming schemes) may be used for special purposes by particular
developers or users, but should not be considered reliable or permanent, and
in general should be ignored by anyone who didn't make the tag or branch.

---

## Release Procedures

### Soft freeze for monthly tweak releases:

We freely backport *safe*, non-ABI-breaking changes by cherry-picking them
from master to the supported branch continually throughout most of the month.

In the seven days before the new month, we do a "soft freeze" where only
important bug fixes are backported. Hold on to any other patches you wish to
backport until after the monthly patch release (i.e., save them for the next
month's release so you never have a release that contains changes that are
only a few days old, and therefore not as well tested). You may, of course,
continue to backport fixes to documentation or other non-code parts of the
project during this soft freeze.

### Branching for annual major/minor releases

By mid-June (about 1-2 months before the beta of a new annual big release),
you should branch and mark it as an alpha.

At the tip of "master", create a new branch named `dev-MAJOR.MINOR` that will
be the staging area for the next release.

Customarily, at that point we add a commit to master (but not to the new
dev-MAJ.MIN branch) that bumps master's version to the next minor level, to
avoid any confusion between builds from master versus what will be the next
release branch. This starts the divergence between master and the release
branch, and henceforth, any big or compatibility-breaking changes will only be
committed to master and not backported to the release branch. (Though we are
loose about this rule during the alpha period and allow continued breaking
changes in the alpha, but by the time we call it beta, we allow no more
breaking changes.) Also at this stage, the master branch's CHANGES.md should
have a heading added at the top for the *next* version.


### Prior to a release

1. Update CI dependencies: Ensure that the "latest" CI test specifies the
   current releases of the dependencies and compilers (check their home
   pages), and that the [INSTALL.md](../INSTALL.md) file correctly documents
   the version ranges that we expect to work and that we actively test.

2. Release notes: [CHANGES.md](../CHANGES.md) should be updated to reflect
   all the new features and fixes. Looking at the notes from older releases
   should provide a good example of what we're aiming for.

   - Patch and tweak releases can be brief one-line descriptions (often the
     first line of the commit messages involved), presented in a single
     section in either chronological or any other order that seems right.
   - Minor or major releases have a much more extensive, prose-based
     description of all changes since the last minor release, broken into
     sections (dependency changes, major new features, enhancements and fixes,
     internals and developer goodies, testing/CI/ports, etc.) and ordered for
     readability and relevance.
   
3. Ensure docs are up to date:

   - [README.md](README.md): Actually read it! Make sure it all seems
     accurate and up to date.
   - [INSTALL.md](INSTALL.md): Be sure that it accurately reflects the
     current minimum and latest-tested versions of each dependency.
   - [CREDITS.md](CREDITS.md): Check against the recent repo history to be
     sure new contributors are listed.
   - [SECURITY.md](SECURITY.md): Make sure it accurately reflects the
     status of which branches get updates.
   - Skim the [user documentation](https://openimageio.readthedocs.io/) to
     ensure it's building correctly and doesn't have any obvious errors,
     especially the parts that describe new features.

4. Make sure the the top-level CMakeLists.txt file is updated:

   - The `OpenImageIO_VERSION` should be correct.
   - The `PROJECT_VERSION_RELEASE_TYPE` variable should be set to "alpha" or
     "beta" if appropriate, "RC" for release candidate, or empty string `""`
     for most supported releases, and "dev" only for the master branch or
     developer previews.
   - The `${PROJECT_NAME}_SUPPORTED_RELEASE` variable should be `ON` for any
     release branch, `OFF` for master.

5. In the https://github.com/AcademySoftwareFoundation/OpenImageIO-images project, create a branch
   `dev-x.y` for the major/minor branch, and in the main oiio repo, update
   src/build-scripts/install_test_images.bash to specify `-b dev-x.y` in the
   checkout of oiio-images to ensure CI tests are against the set of test
   images corresponding to that major/minor release (just in the branch, not
   in master!).

6. Make sure everything passes the usual CI workflow. Also check the daily or
   weekly "analysis" workflows to make sure there aren't any important
   warnings that should be fixed.


### Staging alpha / beta / release candidate (annual minor/major releases only)

For monthly patch releases, we don't have any formal alpha/beta/RC stages.

For the annual minor or major releases we have a staged release:

- `alpha` is everything after we have branched, in which we think all the big
  new features have been added, and while we will make continued changes and
  fixes, we are aiming for increased stability and to minimize additional
  breaking changes prior to the release.
- `beta` should be about one month prior to the final release date. We create
  a tag `v1.2.3.4-beta` and announce the beta (see below "Making the
  release"). Once we tag a beta, we try very hard not to allow any further
  changes that change the API or are not ABI-compatible (i.e., safe fixes
  only).
- `RC` or release candidates around one week prior to the final release date.
  This is a trial run, hopefully identical to the final release (except for
  removing the RC label and making a new tag). The only changes allowed after
  RC are fixes to the most critical bugs or build breaks that would be bad for
  users to encounter. If critical problems are found with RC1, they should be
  fixed and immediately re-tagged as RC2, etc. All other fixes should be
  postponed until the next month's patch release.

### Making the release

A final release is made on the scheduled date, provided the latest RC has
survived for at least a few days of testing without finding any critical
problems. Don't make the release final until you are sure there are no
truly critical bugs or build breaks that users will encounter.

The following are the steps for making the release:

1. Edit the top-level CMakeLists.txt to remove any RC designation
   (i.e., `PROJECT_VERSION_RELEASE_TYPE` should be set to `""`).

2. Edit CHANGES.md to reflect the correct date of the release and ensure it
   includes any last-minute changes that were made during beta or release
   candidate stages.

3. Push it to **your** GitHub, make sure it passes CI.
   
4. Tag the release: `git tag v1.2.3.4` (no more beta, RC, or dev suffix).
   
5. If this will now be the recommended stable release, move the `release`
   branch marker to the same position.

6. Push it to GitHub: `git push OpenImageIO release --tags`

   (This example assumes "OpenImageIO" is the name of the remote for the
   GitHub `AcademySoftwareFoundation/OpenImageIO` repo.)

7. Draft a release on GitHub: On https://github.com/AcademySoftwareFoundation/OpenImageIO/releases
   select "Draft a new release." Choose the new tag you just pushed. Make the
   release title "OpenImageIO v1.2.3.4" (and beta, etc., designation
   if applicable). In the description, paste the release notes for this
   release (from CHANGES.md). If this is a beta or release candidate, check
   "this is a pre-release" box at the bottom.

8. Announce the release on the [oiio-dev mail list](https://lists.aswf.io/g/oiio-dev)
   with the subject "Release: OpenImageIO v1.2.3.4" and using one of these
   templates for the body of the email.

   For a monthly patch release:

    > We have tagged v1.2.3.4 as the latest production release and moved the
    > "release" branch marker to that point.  This is guaranteed to be API,
    > ABI, and link back-compatible with prior 1.2 releases. Release notes
    > are below.
    >
    > (Paste the full set of 1.2 changes here, just copy the appropriate
    > part of CHANGES.md)

   For an annual major/minor release:

    > OpenImageIO version 1.2 has been released! Officially tagged as
    > "v1.2.3.4", we have also moved the "release" branch tag to this
    > position. Henceforth, 1.2 is the supported production release family.
    > The API is now frozen -- we promise that subsequent 1.2.x releases
    > (which should happen monthly) will not break back-compatibility of API,
    > ABI, or linkage, compared to this release. Please note that this release
    > is *not* ABI or link compatible with 1.1 or older releases.
    > 
    > Release notes for 1.2 outlining all the changes since last year's
    > release are below.
    > 
    > Please note that a few of the build and runtime dependencies have
    > changed their minimum supported versions. (List here any important
    > changes to dependencies, compilers, or C++ standard that users should be
    > aware of.)
    > 
    > (List here anything else that people should know about this release
    > family that may be surprising if they haven't followed the last year of
    > development closely, or that they must know even if they are too lazy to
    > read the release notes. If this is a major release that is not
    > backward-compatible with prior versions, warn about that here.)
    > 
    > Enjoy, and please report any problems. We will continue to make patch
    > releases to the 1.2 family roughly monthly, which will contain bug fixes
    > and non-breaking enhancements.
    > 
    > The older 1.1 series of releases is now considered obsolete. We will
    > continue for now to make 1.1 patch releases, but over time, these will
    > become less frequent and be reserved for only the most critical bug
    > fixes.
    > 
    > The "master" branch is now progressing toward an eventual 1.3 release next
    > summer. As usual, you are welcome to use master for real work, but we do
    > not make any compatibility guarantees and don't guarantee continuing API
    > compatibility in master.
    >
    > (Paste the full set of 1.2 changes here, just copy the appropriate
    > part of CHANGES.md)

   For a beta leading up to the annual major/minor release:

    > OpenImageIO version 1.2 is now in beta, tagged as "v1.2.3.4-beta". We
    > will try very hard not to make any further API or ABI changes between
    > now and the final release (unless it is absolutely necessary to fix
    > an important problem identified during beta testing). The final 1.2
    > release is scheduled for [DATE GOES HERE], so please try building and
    > testing the beta so we are sure to find any problems.
    > 
    > Release notes for 1.2 outlining all the changes since last year's
    > release are below.
    > 
    > (Paste the full set of 1.2 changes here, just copy the appropriate
    > part of CHANGES.md)


### After the release

Odds and ends to do after the tag is pushed and the announcements are sent:

- Re-read RELEASING.md and ensure that the instructions match what you
  have done. Update as necessary.

- Go to [readthedocs.org](https://openimageio.readthedocs.io/), and ensure
  that the new release is built, visible, and is the default release shown
  (specified in the Admin section). I tend to keep the latest patch of each
  minor release available for reference indefinitely, but hide the docs for
  earlier patch releases within that minor release series.

- Edit the top-level CMakeList.txt to update the version to the *next*
  anticipated release on the branch, in order to ensure that anybody building
  from subsequent patches won't get a release number that advertises itself
  incorrectly as a prior tagged release. Also edit CHANGES.md to add a new
  (blank) heading for the next patch or release.


