Contributing to OpenImageIO
===========================

Code contributions to OpenImageIO are always welcome, and [nearly 150
people](CREDITS.md) have done so over the years.  Please review this
document to get a briefing on our process.


Mail Lists
----------

There are two mail lists associated with OpenImageIO development:

* [oiio-dev](http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org)
For developers of the OpenImageIO code itself, or users who are really
interested in the OIIO internals. This is where we mostly discuss the code
(including bug reports), but are also happy to answer user questions about
use or working of OIIO.

* [oiio-announce](http://lists.openimageio.org/listinfo.cgi/oiio-announce-openimageio.org)
For announcements about major OIIO releases or other important news.

You can sign up for these mail lists on your own using the links above.


How to Ask for Help
-------------------

If you have trouble installing, building, or using OpenImageIO, but there's
not yet reason to suspect you've encountered a genuine bug, start by posting
a question to the
[oiio-dev](http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org)
mailing list. This is the place for question such has "How do I...".


Bug Reports and Issue Tracking
------------------------------

We use GitHub's issue tracking system for bugs and enhancements:
https://github.com/OpenImageIO/oiio/issues

If you are submitting a bug report, please be sure to note which version of
OIIO you are using, on what platform (OS/version, which compiler you used,
and any special build flags or other unusual environmental issues). Please
give a specific an account of

* what you tried
* what happened
* what you expected to happen instead

with enough detail that others can reproduce the problem. Please quote the
exact error message you received. If you are having trouble building, please
post the full cmake output of a fresh VERBOSE=1 build.

Suspected security vulnerabilities should be reported by the same process.

If confidentiality precludes a public question or issue for any reason, you
may contact us privately at [security@openimageio.org](security@openimageio.org).


Contributor License Agreement (CLA) and Intellectual Property
-------------------------------------------------------------

To protect the project -- and the contributors! -- we do require a
Contributor License Agreement (CLA) for anybody submitting substantial
changes. Trivial changes (such as an alteration to the Makefiles, a one-line
bug fix, etc.) may be accepted without a CLA, at the sole discretion of the
project leader, but anything complex needs a CLA. This is for your own
safety.

* If you are an individual writing the code on your own time and you're SURE
you are the sole owner of any intellectual property you contribute, use the
[Individual CLA](https://github.com/OpenImageIO/oiio/blob/master/src/doc/CLA-INDIVIDUAL).

* If you are writing the code as part of your job, or if there is any
possibility that your employers might think they own any intellectual
property you create, then you should use the [Corporate
CLA](https://github.com/OpenImageIO/oiio/blob/master/src/doc/CLA-CORPORATE).

Download the appropriate CLA from the links above (or find them in the
src/doc directory of the software distribution), print, sign, and rescan it
(or just add a digital signature directly), and email it back to us
(info@openimageio.org).

Our CLA's are identical to those used by Apache and many other open source
projects.


Pull Requests and Code Review
-----------------------------

The best way to submit changes is via GitHub Pull Request. GitHub has a
[Pull Request Howto](https://help.github.com/articles/using-pull-requests/).

All code must be formally reviewed before being merged into the official
repository. The protocol is like this:

1. Get a GitHub account, fork OpenImageIO/oiio to create your own repository
on GitHub, and then clone it to get a repository on your local machine.

2. Edit, compile, and test your changes. Run clang-format (see the
instructions on coding style below).

3. Push your changes to your fork (each unrelated pull request to a separate
"topic branch", please).

4. Make a "pull request" on GitHub for your patch.

5. If your patch will induce a major compatibility break, or has a design
component that deserves extended discussion or debate among the wider OIIO
community, then it may be prudent to email oiio-dev pointing everybody to
the pull request URL and discussing any issues you think are important.

6. All pull requests automatically launch jobs on [TravisCI](https://travis-ci.org)
for Linux and MacOS and on [Appveyor](http://appveyor.com) for Windows.
This ensures that the build completes and that the tests suite runs
correctly, for a varity of platform, compiler, library, and flag combinations.
The status of the CI tests for your PR will be displayed on the GitHub PR
page. We will not accept PRs that don't build cleanly or pass the existing
testsuite.

7. The reviewer will look over the code and critique on the "comments" area.
Reviewers may ask for changes, explain problems they found, congratulate the
author on a clever solution, etc. But until somebody says "LGTM" (looks good
to me), the code should not be committed. Sometimes this takes a few rounds
of give and take. Please don't take it hard if your first try is not
accepted. It happens to all of us.

8. After approval, one of the senior developers (with commit approval to the
official main repository) will merge your fixes into the master branch.


Coding Style
------------

#### Formatting

We use [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
to uniformly format our source code prior to PR submission. Make sure that
clang-format is installed on your local machine, and just run

    make clang-format

and it will automatically reformat your code according to the configuration
file found in the `.clang-format` file at the root directory of the OIIO
source code checkout.

One of the TravisCI test matrix entries runs clang-format and fails if any
diffs were generated (that is, if any of your code did not 100% conform to
the `.clang-format` formatting configuration). If it fails, clicking on that
test log will show you the diffs generated, so that you can easily correct
it on your end and update the PR with the formatting fixes.

If you don't have clang-format set up on your machine, and your patch is not
very long, you may find that it's more convenient to just submit it and hope
for the best, and if it doesn't pass the Travis test, look at the diffs in
the log and make the corrections by hand and then submit an update to the
patch (i.e. relying on Travis to run clang-format for you).

Because the basic formatting is automated by clang-format, we won't
enumerate the rules here.


#### File conventions

C++ implementation should be named `*.cpp`. Headers should be named `.h`.
All headers should contain

    #pragma once

All source files should begin with the copyright and license, which can be
found in the LICENSE.md file (or cut and pasted from any other other source
file).

For NEW source files, please change the copyright year to the present. DO
NOT edit existing files only to update the copyright year, it just creates
pointless deltas and offers no increased protection.


#### Identifiers

In general, classes and templates should start with upper case and
capitalize new words: `class CustomerList;` In general, local variables
should start with lower case. Macros should be `ALL_CAPS`, if used at all.

If your class is extremely similar to, or modeled after, something in the
standard library, Boost, or something else we interoperate with, it's ok to
use their naming conventions. For example, very general utility classes and
templates (the kind of thing you would normally find in std or boost) should
be lower case with underscores separating words, as they would be if they
were standards.

    template <class T> shared_ptr;
    class scoped_mutex;

Names of data should generally be nouns. Functions/methods are trickier: a
the name of a function that returns a value but has no side effects should
be a noun, but a procedure that performs an action should be a verb.

#### Class structure

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

#### Third-party libraries

Prefer C++11 `std` rather than Boost, where both can do the same task.
Feel free to use Boost classes you already see in the code base, but don't
use any Boost you don't see us already using, without first checking with
the project leader.

Please do use IlmBase vector, matrix, and utility classes where applicable.
Don't write your own vector or matrix code!

Use these libraries for OIIO internals, but please DO NOT require them in
any of our main external APIs unless it's been thoroughly discussed and
approved by the group.

#### Comments and Doxygen

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

#### Miscellaneous

Macros should be used only rarely -- prefer inline functions, templates,
enums, or "const" values wherever possible.

Prefer `std::unique_ptr` over raw new/delete.

#### Bottom Line

When in doubt, look elsewhere in the code base for examples of similar
structures and try to format your code in the same manner, or ask on the
oiio-dev mail list.

