<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Release Procedures for OpenImageIO

## Version nomenclature and release cadence

### The meaning of the version parts

Our release numbers are of the form `MAJOR.MINOR.PATCH.TWEAK`, and we follow
an almost-semantic-versioning scheme.

MAJOR releases are those that break backward-compatibility of the public APIs,
requiring existing application code to change their source code where it calls
OIIO API functions in order to compile and work correctly.

MINOR releases guarantee API backward-compatibility with prior releases that
have the same major number, but can introduce ABI or link incompatibilities.
Applications using OIIO need to be recompiled to move to a new minor release,
but should not need to change their source code (except, of course, to take
advantage of new features). Minor releases are allowed to drop support for
sufficiently old compilers and dependencies.

PATCH releases guarantee both API and ABI/link backward-compatibility with
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

TWEAK releases are restricted to critical bug fixes or build breaks that do
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

### Things are different in "main"

The "main" branch is where ongoing development occurs without any
compatibility guarantees. We don't always know if the next year's release from
main will turn out to be major (API breaking) or minor (only ABI breaking).

Some studios or products need the most up-to-date enhancements, and thus tend
to build OIIO from the "main" branch rather than using tagged releases. For
these users, we try to set version numbers and occasionally tag in main, to
send a signal about which points in main seem safe to rely on various
compatibility levels. However, the rules are slightly different than we use
for releases.

In the "main" branch:

- TWEAK tags within "main" only guarantee ABI back-compatibility, but may
  have additions or non-bug-fix behavior changes (akin to patch releases
  within a release branch).
- PATCH tags within "main" are allowed to break ABI or API (changes that
  would require minor or major releases, if they were not in "main").
- Beware of unmarked breaks in compatibility of commits in main that are
  in between developer preview tags.


---

## Branch and tag names

**Branches** are places where commits containing new development may be added
(and thus the branch markers will move over time). Branch names obey the
following conventions:

- `main` is the area for arbitrary changes intended for the next year's
  major or minor release.
- `dev-1.2` are the areas for staging additions that will become the next
  tweak or patch release for the minor version series.
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
- `v1.2.3.4-dev` is a "developer preview" within "main." It is not an official
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
from main to the supported branch continually throughout most of the month.

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

At the tip of "main", create a new branch named `dev-MAJOR.MINOR` that will
be the staging area for the next release.

Customarily, at that point we add a commit to main (but not to the new
dev-MAJ.MIN branch) that bumps main's version to the next minor level, to
avoid any confusion between builds from main versus what will be the next
release branch. This starts the divergence between main and the release
branch, and henceforth, any big or compatibility-breaking changes will only be
committed to main and not backported to the release branch. (Though we are
loose about this rule during the alpha period and allow continued breaking
changes in the alpha, but by the time we call it beta, we allow no more
breaking changes.) Also at this stage, the main branch's CHANGES.md should
have a heading added at the top for the *next* version.


### Prior to a release

1. Update CI dependencies: Ensure that the "latest" CI test specifies the
   current releases of the dependencies and compilers (check their home
   pages), and that the [INSTALL.md](../INSTALL.md) file correctly documents
   the version ranges that we expect to work and that we actively test.

2. Update deprecations: Deprecations tend to happen in stages, spread over a
   few releases, taking steps forward depending on what kind of release is
   occurring:

   - Tweak releases (a.b.c.d -> a.b.c.D+1): should not introduce new
     deprecations.
   - Monthly patch releases (a.b.c.d -> a.b.C+1.0): advertise that a function
     is deprecated in its comments, documentation, and release notes. Remember
     that these releases are not allowed to break ABI/link compatibility.
   - Annual minor releases (a.b.c.d -> a.B+1.0.0): functions previously
     marked/documented as deprecated should receive deprecation warnings by
     adding `OIIO_DEPRECATED` macros to function or class declarations where
     possible, if that has not already been done. Use good judgment about
     whether to add these warnings, or postpone to a later release, based on
     the amount of disruption that they will likely cause, and how important
     it is to rapidly discourage people from using the deprecated function.
     For a minor release, we don't allow backwards-incompatible API changes,
     so only *remove* a function if there is another function that will accept
     the same call signature in the source code. It's allowed to break ABI for
     minor releases, so it's ok/encouraged to make a deprecated function from
     linked to inline, for example (which might allow it to be removed later
     without an ABI change).
   - Major releases (a.b.c.d -> A+1.b.c.d): API compatibility breaks are
     allowed here, so this is the time to completely remove any functions that
     have already had a full minor release where they had been given
     deprecation warning macros.
   
3. Release notes: [CHANGES.md](../CHANGES.md) should be updated to reflect
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
   
4. Ensure docs are up to date:

   - [README.md](README.md): Actually read it! Make sure it all seems
     accurate and up to date.
   - [INSTALL.md](INSTALL.md): Be sure that it accurately reflects the
     current minimum and latest-tested versions of each dependency.
   - [CREDITS.md](CREDITS.md): Check against the recent repo history to be
     sure new contributors are listed.
   - [SECURITY.md](SECURITY.md): Make sure it accurately reflects the
     status of which branches get updates.
   - Skim the [user documentation](https://docs.openimageio.org) to
     ensure it's building correctly and doesn't have any obvious errors,
     especially the parts that describe new features.

5. Make sure the the top-level CMakeLists.txt file is updated:

   - The `OpenImageIO_VERSION` should be correct.
   - The `PROJECT_VERSION_RELEASE_TYPE` variable should be set to "alpha" or
     "beta" if appropriate, "RC" for release candidate, or empty string `""`
     for most supported releases, and "dev" only for the main branch or
     developer previews.
   - The `${PROJECT_NAME}_SUPPORTED_RELEASE` variable should be `ON` for any
     release branch, `OFF` for main.

6. In the https://github.com/AcademySoftwareFoundation/OpenImageIO-images project, create a branch
   `dev-x.y` for the major/minor branch, and in the main oiio repo, update
   src/build-scripts/install_test_images.bash to specify `-b dev-x.y` in the
   checkout of oiio-images to ensure CI tests are against the set of test
   images corresponding to that major/minor release (just in the branch, not
   in main!).

7. Make sure everything passes the usual CI workflow. Also check the daily or
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

1. Double check the top-level CMakeLists.txt to ensure the right version
   number and type of version designation (i.e.,
   `PROJECT_VERSION_RELEASE_TYPE` should be set appropriately).

2. Edit CHANGES.md to reflect the correct date of the release and ensure it
   includes any last-minute changes that were made during beta or release
   candidate stages. Please see the section "Generating release notes" below.

3. Push it to **your** GitHub fork, make sure it passes CI.
   
4. Tag the release with a signed tag:

       $ git tag -s v1.2.3.4      # add a signed tag
       $ git tag -v v1.2.3.4      # verify the tag
   
   Be sure that the tag also reflects if it's an alpha (v1.2.3.4-alpha),
   beta (v1.2.3.4-beta), release candidate (v1.2.3.4-RC1), or developer
   preview (v1.2.3.4-dev). If it's a final release, just use the plain
   version number (v1.2.3.4).

   Note that the tag signing requires you to have a GPG key set up and
   associated with your GitHub account. Please see [GitHub's signed tagging    instructions](https://docs.github.com/en/authentication/managing-commit-signature-verification/signing-tags) for more information.

   If you get errors messages "gpg: signing failed: Inappropriate ioctl for device", then the solution is to first:

       $ export GPG_TTY=$(tty)

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

    > We have tagged v3.0.1.2 as the latest production release and moved the
    > "release" branch marker to that point.  This is guaranteed to be API,
    > ABI, and link back-compatible with prior 3.0 releases. Release notes
    > can be found at *LINK TO THE GITHUB RELEASE PAGE.*

   For an annual major/minor release:

    > OpenImageIO version 3.0 has been released! Officially tagged as
    > "v3.0.0.0", we have also moved the "release" branch tag to this
    > position. Henceforth, 3.0 is the supported production release family.
    > The API is now frozen -- we promise that subsequent 3.0.x releases
    > (which should happen monthly) will not break back-compatibility of API,
    > ABI, or linkage, compared to this release. Please note that this release
    > is *not* ABI or link compatible with 2.5 or older releases.
    > 
    > Release notes for 3.0 outlining all the changes since last year's
    > release can be found at *LINK TO THE GITHUB RELEASE PAGE.*
    > (Optionally, you can include a brief summary of the most important
    > changes in this email.)
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
    > releases to the 3.0 family roughly monthly, which will contain bug fixes
    > and non-breaking enhancements.
    > 
    > The older 2.5 series of releases is now considered obsolete. We will
    > continue for now to make 2.4 patch releases, but over time, these will
    > become less frequent and be reserved for only the most critical bug
    > fixes.
    > 
    > The "main" branch is now progressing toward an eventual 3.1 release next
    > fall. As usual, you are welcome to use main for real work, but we do
    > not make any compatibility guarantees and don't guarantee continuing API
    > compatibility in main.
    >
    > (Paste the full set of 3.0 changes here, just copy the appropriate
    > part of CHANGES.md)

   For a beta leading up to the annual major/minor release:

    > OpenImageIO version 3.0 is now in beta, tagged as "v3.0.0.0-beta". We
    > will try very hard not to make any further API or ABI changes between
    > now and the final release (unless it is absolutely necessary to fix
    > an important problem identified during beta testing). The final 3.0
    > release is scheduled for [DATE GOES HERE], so please try building and
    > testing the beta so we are sure to find any problems.
    > 
    > Release notes for 3.0 outlining all the changes since last year's
    > release are below.
    > 
    > (Paste the full set of 3.0 changes here, just copy the appropriate
    > part of CHANGES.md)


### After the release

Odds and ends to do after the tag is pushed and the announcements are sent:

- Re-read RELEASING.md and ensure that the instructions match what you
  have done. Update as necessary.

- Go to [OIIO's docs hosted on readthedocs.org](https://docs.openimageio.org),
  and ensure that the new release is built, visible, and is the default
  release shown (specified in the Admin section). I tend to keep the latest
  patch of each minor release available for reference indefinitely, but hide
  the docs for earlier patch releases within that minor release series.

- Edit the top-level CMakeList.txt to update the version to the *next*
  anticipated release on the branch, in order to ensure that anybody building
  from subsequent patches won't get a release number that advertises itself
  incorrectly as a prior tagged release. Also edit CHANGES.md to add a new
  (blank) heading for the next patch or release.


## Generating release notes

We strongly encourage the use of "conventional commit" prefixes in commit
messages. See [CONTRIBUTING.md](CONTRIBUTING.md#commit-messages) for details.

Many PRs are submitted (even by the author of this section you are now
reading!) without the conventional commit prefix. The person who approves and
merges the commit should take responsibility during the merge process to edit
the commit message to add the appropriate prefix (and for that matter, to edit
any part of the commit message for clarity). But at the end of the day, if we
end up with some commits lacking a conventional commit prefix, it's no big
deal -- we can fix it all up by hand when we make the release notes. But
having CC prefixes in as many commit messages possible helps make the release
notes process be simpler and more automated.

We have been using the [git-cliff](https://github.com/orhun/git-cliff) tool
as the starting point for release notes. The command we use is:

    git cliff -c src/doc/cliff.toml -d v1.2.3.4..HEAD > cliff.out.md

where v1.2.3.4 in this example is the tag of the last release. You could also
use commit hashes to denote the range of changes you want to document.

**For monthly patch releases**

We have found that the git-cliff output is most of what we need for the patch
releases, and can be copied into the CHANGES.md file with only some minor
editing needed. The template for the patch release notes can be found in
[Changes-skeleton-patch.md](docs/dev/Changes-skeleton-patch.md).

* Get rid of the headings that git-cliff generates. We don't use the headings
  for the patch releases.
* Add prefixes to any commits that don't have them (they will be marked as
  "uncategorized" by git-cliff), and feel free to change any of the existing
  prefixes that you think are wrong or could be improved for clarity.
* Rearrange the order of the entries to be logical and readable. I prefer the
  order: feature enhancements, bug fixes, build system fixes that might impact
  users, internal changes, test improvements, documentation and administrative
  changes.
* For patch releases, feel free to omit any entries that you think are not
  user-facing and are too minor to be worth mentioning in the release notes.

Strive to keep the release notes just long enough for users to know if the
patch contains any fixes relevant to them, but short enough to be read at a
glance and without extraneous detail.

Here is an example of well-constructed monthly patch release notes:
https://github.com/AcademySoftwareFoundation/OpenImageIO/releases/tag/v2.5.12.0

**For annual major/minor releases**

For major releases, the git-cliff output is just a starting point and need
significant editing to get the detail and quality level we expect for our
major releases. A simple bullet list of commits is not sufficient -- we aim
for a prose-based description of important changes that "tell the story" of
the year's work and will be thoroughly understood by our stakeholders who need
to understand what has changed.

* Copy all the headings from [Changes-skeleton-major.md](docs/dev/Changes-skeleton-major.md)
  or the previous year's release notes to get the skeleton of the major and
  minor headers that you fit everything into. Note that it mostly corresponds
  to sections of the git-cliff output, but with a more carefully constructed
  hierarchy of categories.
* Add prefixes to any commits that don't have them (they will be marked as
  "uncategorized" by git-cliff), and feel free to change any of the existing
  prefixes that you think are wrong or could be improved for clarity.
* Rearrange the git-cliff output into the hierarchy of our preferred major
  release notes organization. Within each section and subsection, group
  similar changes together. The chronological order of the commits isn't as
  important as clearly explaining what changed over the course of the year.
* The git-cliff output is a good starting point for populating the notes with
  every PR, plus automatically adding a reference and link to the PR and the
  name of the author of the patch.
* Look with a skeptical eye at the one-line bullet points from git-cliff (i.e,
  from the first line of each PR's commit message). Often, they are too terse
  or tend to [bury the lede](https://www.dictionary.com/e/slang/bury-the-lede/).
  Expand into as much explanation is necessary for users who need to know
  about the change or feature to know whether it is relevant and have some
  idea what to do or that they should look at the relevant PRs for more
  detail.

Here is an example of well-constructed annual major release notes:
https://github.com/AcademySoftwareFoundation/OpenImageIO/releases/tag/v2.5.4.0
