Contributing to OpenImageIO
===========================

> NOTE: This is the proposed post-ASWF-move version of CONTRIBUTING. After the
> project is legally transferred and moved to the new repo, this file will
> replace the one at the project root.
>
> TO DO:
>
> - [X] Update the mail list sign-up page after the mail list moves.
> - [ ] Update the repo URL
> - [ ] Double check the security and info email addresses.
>

Code contributions to OpenImageIO are always welcome, and [nearly 200
people](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/CREDITS.md) have done
so over the years.  Please review this document to get a briefing on our
process.


Mail List and Slack
-------------------

Contributors should be reading the oiio-dev mail list:

* [oiio-dev](https://lists.aswf.io/g/oiio-dev)
For developers of the OpenImageIO code itself, or users who are really
interested in the OIIO internals. This is where we mostly discuss the code
(including bug reports), but are also happy to answer user questions about
use or working of OIIO.

You can sign up for the mail list on your own using the link above.

The [ASWF Slack](https://slack.aswf.io/) has an `openimageio` channel. Sign up
for the Slack on your own, then under "channels", select "browse channels" and
you should see the openimageio channel (among those of the other projects and
working groups).


How to Ask for Help
-------------------

If you have trouble installing, building, or using OpenImageIO, but there's
not yet a solid reason to suspect you've encountered a genuine bug, start by
posting a question to the [oiio-dev mailing
list](https://lists.aswf.io/g/oiio-dev).
This is the place for questions such has "How do I...".


Bug Reports and Issue Tracking
------------------------------

We use GitHub's issue tracking system for reporting bugs and requesting
enhancements: https://github.com/AcademySoftwareFoundation/OpenImageIO/issues

**If you are merely asking a question ("how do I...")**, please do not file an
issue, but instead ask the question on the [oiio-dev mailing
list](https://lists.aswf.io/g/oiio-dev).

If you are submitting a bug report, please be sure to note which version of
OIIO you are using, on what platform (OS/version, which compiler you used,
and any special build flags or other unusual environmental issues). Please
give a specific, as-detailed-as-possible account of

* what you tried (command line, source code example)
* what you expected to happen
* what actually happened (crash? error message? ran but didn't give the
  correct result?)

with enough detail that others can easily reproduce the problem just by
following your instructions. Please quote the exact error message you
received. If you are having trouble building, please post the full cmake
output of a fresh VERBOSE=1 build.

Suspected security vulnerabilities should be reported by the same process.

If confidentiality precludes a public question or issue for any reason, you
may contact us privately at [security@openimageio.org](security@openimageio.org).


Contributor License Agreement (CLA) and Intellectual Property
-------------------------------------------------------------

To protect the project -- and the contributors! -- we do require a Contributor
License Agreement (CLA) for anybody submitting changes. This is for your own
safety, as it prevents any possible future disputes between code authors and
their employers or anyone else who might think they might own the IP output of
the author.

* [Corporate CLA](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/ASWF/CLA-corporate.md) :
  If you are writing the code as part of your job, or if there is any
  possibility that your employers might think they own any intellectual
  property you create. This needs to be executed by someone who has
  signatory power for the company.

* [Individual CLA](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/ASWF/CLA-individual.md) :
  If you are an individual writing the code on your own time, using your own
  equipment, and you're SURE you are the sole owner of any intellectual
  property you contribute.

Please note that it is extremely common (nearly ubiquitous in the US and
Canada, and maybe other places) for full-time employees of technology and
entertainment companies to have IP clauses in their employment agreement (in
that pile of paperwork you sign on your first day of work and then promptly
forget about) that give the company rights to any code you write, even on your
own time. The OpenImageIO project expects you to know and follow the rules of
your employer and to have them sign a corporate CLA if necessary.

The easiest way to sign CLAs is digitally [using
EasyCLA](https://corporate.v1.easycla.lfx.linuxfoundation.org). There are
detailed step-by-step instructions about using the EasyCLA system for
[corporate CLAs](https://docs.linuxfoundation.org/lfx/easycla/v2-current/contributors/corporate-contributor)
and [individual CLAs](https://docs.linuxfoundation.org/lfx/easycla/v2-current/contributors/individual-contributor#github).

Companies who prefer not to use the online tools may sign, scan, and email
the executed copy to manager@lfprojects.org.

The corporate CLA allows a company to name a "CLA Manager" (who does not need
signatory power) who has the ability to use the online system to add or delete
individual employees of the company who are authorized to submit pull
requests, without needing to get an executive to amend and sign the agreement
each time.

Please note that these CLAs are based on the Apache 2.0 CLAs, and differ
minimally, only as much as was required to correctly describe the EasyCLA
process and our use of a CLA manager.

**Contribution sign off**

This project requires the use of the [Developer’s Certificate of Origin 1.1
(DCO)](https://developercertificate.org/), which is the same mechanism that
the [Linux®
Kernel](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/process/submitting-patches.rst#n416)
and many other communities use to manage code contributions. The DCO is
considered one of the simplest tools for sign offs from contributors as the
representations are meant to be easy to read and indicating signoff is done
as a part of the commit message.

Here is an example Signed-off-by line, which indicates that the submitter
accepts the DCO:

    Signed-off-by: John Doe <john.doe@example.com>

You can include this automatically when you commit a change to your local
git repository using `git commit -s`. You might also want to
leverage this [command line tool](https://github.com/coderanger/dco) for
automatically adding the signoff message on commits.


Commit messages
---------------

### Summary heuristic

Look at the commit history of the project to get a sense of the style and
level of detail that is expected in commit messages.

### General guidelines and expectations

The first line of the commit message should be a short (less than 80
characters) summary of the change, prefixed with the general nature of the
change (see below).

The rest of the commit message should be a more detailed explanation of the
changes. Some commits are self-explanatory and don't need more than the
summary line. Others may need a more detailed explanation. Hallmarks of
a good commit message include:

* An explanation of why the change is necessary and what you hope to
  accomplish with it.
* A description of any major user- or developer-facing changes that people
  should be aware of: changes in APIs or behaviors, new features, etc.
* An explanation of any tricky implementation details or design decisions that
  might not be immediately obvious to someone reading the code.
* Guideposts for somebody reviewing the code to understand the rationale and
  have any supporting background information to fully understanding the code
  changes.

Remember that at some point in the future, a developer unfamiliar with your
change (maybe you, long after you've forgotten the details) might need to
understand or fix your patch. Keep that person in mind as your audience and
strive to write a commit message that explains the context in a way that saves
them time and effort. Be the hero in the story of their future debugging
effort!

### Using "conventional commits" prefixes

We strive to follow the recommendations known as [conventional
commits](https://www.conventionalcommits.org/), which means that we would like
merged commit messages to have their first line start with one of the
following prefixes:

- `feat:`  new features
- `fix:`  bug fixes
- `perf:`  performance improvements
- `api:`  changes to the public APIs
- `int:`  changes to code internals that don't affect the public API
- `build:`  changes to the build system
- `test:`  changes to the test suite or unit tests
- `ci:`  changes to the CI system
- `docs:`  changes to the documentation
- `refactor:`  code refactoring
- `style:`  formatting or other stylistic non-functional changes to the code
- `admin:`  project administration or policy changes
- `revert:`  reverting a previous commit

Obviously, some (if not most) PRs may qualify for more than one of these
categories (for example, a new feature may also introduce a new API call, add
tests, and include new documentation). If that is the case, use your best
judgment to pick the category that best captures the essence or most important
aspect of the change. When ambiguous, consider the list above to be a priority
ranking (e.g., a change that fixes a bug and adds a new test should be listed
under `fix:`, because that appears first in the list).

It is also encouraged, when it makes sense to do so, to put a subcategory in
parenthesis after the prefix, like `fix(exr):` or `feat(IBA):`. It's ok to use
obvious abbreviations for major classes or subsections: IB=ImageBuf,
IBA=ImageBufAlgo, IC=ImageCace, TS=TextureSystem, etc. If there is no clear
single format or class that is the man focus of the patch, then you can omit
the subcategory.

API or ABI-breaking changes should additionally be marked with an exclamation
point at the end of the prefix, like `feat!:` or `api!:` to make it easily
identifiable as a breaking change from the first line of the commit message.



Pull Requests and Code Review
-----------------------------

The best way to submit changes is via GitHub Pull Request. GitHub has a
[Pull Request Howto](https://help.github.com/articles/using-pull-requests/).

All code must be formally reviewed before being merged into the official
repository. The protocol is like this:

1. Get a GitHub account, fork AcademySoftwareFoundation/OpenImageIO to create
your own repository on GitHub, and then clone it to get a repository on your
local machine.

1. Edit, compile, and test your changes. Run clang-format (see the
instructions on coding style below).

1. Push your changes to your fork (each unrelated pull request to a separate
"topic branch", please).

1. Make a "pull request" on GitHub for your patch.

2. If your patch will induce a major compatibility break, or has a design
component that deserves extended discussion or debate among the wider OIIO
community, then it may be prudent to email oiio-dev pointing everybody to
the pull request URL and discussing any issues you think are important.

1. All pull requests automatically launch CI jobs on GitHub Actions to
ensure that the build completes and that the tests suite runs correctly, for
a variety of platform, compiler, library, and flag combinations. The status
of the CI tests for your PR will be displayed on the GitHub PR page. We will
not accept PRs that don't build cleanly or pass the existing testsuite.

1. The reviewer will look over the code and critique on the "comments" area.
Reviewers may ask for changes, explain problems they found, congratulate the
author on a clever solution, etc. But until somebody says "LGTM" (looks good
to me), the code should not be committed. Sometimes this takes a few rounds
of give and take. Please don't take it hard if your first try is not
accepted. It happens to all of us.

1. After approval, one of the senior developers (with commit approval to the
official main repository) will merge your fixes into the main branch.

Please see the [Code Review](docs/dev/CodeReview.md) document for more
explanaton of the code review process.

Coding Style
------------

### File conventions

C++ implementation should be named `*.cpp`. Headers should be named `.h`.
All headers should contain

    #pragma once

All source files should begin with these three lines:

    // Copyright Contributors to the OpenImageIO project.
    // SPDX-License-Identifier: Apache-2.0
    // https://github.com/AcademySoftwareFoundation/OpenImageIO

as a comment in the syntax of whatever source code is used in that file.

Occasionally a file may contain substantial code from another project and will
also list its original copyright and license information. Do NOT alter that
notice or copy it to any new files, it really only applies to the particular
file in which it appears.


### Formatting

We use [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
to uniformly format our source code prior to PR submission. Make sure that
clang-format is installed on your local machine, and just run

    make clang-format

and it will automatically reformat your code according to the configuration
file found in the `.clang-format` file at the root directory of the OIIO
source code checkout.

One of the CI test matrix entries runs clang-format and fails if any
diffs were generated (that is, if any of your code did not 100% conform to
the `.clang-format` formatting configuration). If it fails, clicking on that
test log will show you the diffs generated, so that you can easily correct
it on your end and update the PR with the formatting fixes.

If you don't have clang-format set up on your machine, and your patch is not
very long, you may find that it's more convenient to just submit it and hope
for the best, and if it doesn't pass the CI test, look at the diffs in the log
for the "clang-format" CI run and make the corrections by hand and then submit
an update to the patch (i.e. relying on CI to run clang-format for you).

Because the basic formatting is automated by clang-format, we won't
enumerate the rules here.

For the occasional non-clang-format regions of code, NEVER alter somebody
else's code to reformat just because you found something that violates the
rules. Let the group/author/leader know, and resist the temptation to change
it yourself.

Each line of text in your code, including comments, should be at most 80
characters long. Exceptions are allowed for those rare cases where letting a
line be longer (and wrapping on an 80-character window) is actually a better
and clearer alternative than trying to split it into two lines. Sometimes this
happens, but it's extremely rare.

We prefer three (3) consecutive empty lines between freestanding functions or
class definitions, one blank line between method declarations within a class
definition. Put a single blank line within a function if it helps to visually
separate different sequential tasks, but don't put multiple blank lines in a
row within a function, or blank lines right after an opening brace or right
before a closing brace. The goal is to use just enough white space to help
developers visually parse the code (for example, spotting at a glance where
new functions begin), but not so much as to spread it out or be confusing.


### Identifiers

In general, classes and templates should start with upper case and
capitalize new words: `class CustomerList;` In general, local variables
should start with lower case. Macros should be `ALL_CAPS`, if used at all.

If your class is extremely similar to, or modeled after, something in the
standard library, or something else we interoperate with, it's ok to
use their naming conventions. For example, very general utility classes and
templates (the kind of thing you would normally find in std) should
be lower case with underscores separating words, as they would be if they
were standards.

    template <class T> shared_ptr;
    class scoped_mutex;

Names of data should generally be nouns. Functions/methods are trickier: a
the name of a function that returns a value but has no side effects should
be a noun, but a procedure that performs an action should be a verb.

### Class structure

Try to avoid public data members, although there are some classes that serve
a role similar to a simple C struct -- a very straightforward collection of
data members. In these, it's fine to make the data members public and have
clients set and get them directly.

Private member data should be named m_foo (alternately, it's ok to use the
common practice of member data foo_, but don't mix the conventions within a
class).

Private member data that needs public accessors should use the convention:

    void foo (const T& newfoo) { m_foo = newfoo; }
    const T& foo () const { return m_foo; }

Avoid multiple inheritance.

Namespaces: yes, use them!

### Third-party libraries

Prefer C++17 `std` rather than other libraries, where both can do the same
task.

Please do use Imath vector, matrix, and utility classes where applicable.
Don't write your own vector or matrix code!

Use these libraries for OIIO internals, but please DO NOT require them in
any of our main external APIs unless it's been thoroughly discussed and
approved by the group.

### Comments and Doxygen

Comment philosophy: try to be clear, try to help teach the reader what's
going on in your code.

Prefer C++ comments (starting line with `//`) rather than C comments (`/* ... */`).

For public APIs we tend to use Doxygen-style comments (start with `///`).
They looks like this:

    /// Explanation of a class.  Note THREE slashes!
    class myclass {
        ....
        float foo;  ///< Doxygen comments on same line look like this
    }

### Miscellaneous

Macros should be used only rarely -- prefer inline functions, templates,
enums, or "const" values wherever possible.

Prefer `std::unique_ptr` over raw new/delete.

#### Bottom Line

When in doubt, look elsewhere in the code base for examples of similar
structures and try to format your code in the same manner, or ask on the
oiio-dev mail list.

