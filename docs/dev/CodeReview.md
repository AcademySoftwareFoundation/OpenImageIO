<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->


Code Review Procedures
======================

### Why do we code review?

Code review is a process where someone other than the author of a patch
examines the proposed changes and approves or critiques them. The main
benefits are to:

- Encourage submitters to ensure that their changes are well thought out,
  tested, and documented so that their intent and wisdom will be clear to
  others.
- Directly find shortcomings in or suggest improvements to the proposed
  changes, and ensure that the changes are consistent with the project's best
  practices.
- Minimize the amount of the code base that has only been seen or is only
  understood by one person.
- Improve security and robustness of the code base by assuring that at least
  two people (submitter and reviewer) agree that every patch is reasonable and
  safe.

### Reviewer guidelines -- what you should be looking for

Code review is not difficult, and it doesn't require you to be a senior
developer or a domain expert. Even if you don't particularly understand that
region of the code and are not a domain expert in the feature being changed,
you can still provide a helpful second pair of eyes to look for obvious red
flags and give an adequate review:

- This may seem too obvious to say explicitly, but a big part of review is
  just looking at the PR and seeing that it doesn't appear to deface,
  purposely/obviously break, or introduce malware into the project.
- Does the explanation of the PR make logical sense and appear to match (as
  near as you can tell) the changes in the code? Does it seem sensible, even
  if you don't understand all the details?
- Does the test pass all of our existing CI tests? (If it does, at least we
  are reasonably sure it doesn't break commonly used features.)
- Is there any evidence that the changes have been tested? Ideally, something
  already in, or added to the testsuite by this PR, exercises the new or
  altered code. (Though sometimes a patch addresses an edge case that's very
  hard to reproduce in the testsuite, and you have to rely on the submitter's
  word and the sensibility of their explanation.)
- Not required, but nice if we can get it: You're an expert in the part of
  the code being changed, and you agree that this is the best way to achieve
  an improvement.
- Any objections should be explained in detail and invite counter-arguments
  from the author or other onlookers. A reviewer should never close a PR
  unless it fails to conform to even the basic requirements of any submitted
  PR, or if enough time has passed that it's clear that the PR has been
  abandoned by its author. It's ok to ultimately say "no" to a PR that can't
  be salvaged, but that should be the result of a dialogue.

Obviously, you also want any review comments you give to be constructive and
helpful. If you don't understand something, ask for clarification. If you
think something is wrong, suggest a fix if you know how to fix it. If you
think something is missing, suggest what you think should be added. Follow the
expected kind and constructive tone of the project's community.

### Submitter guidelines -- how to encourage good reviews and timely merges

A few tips for submitters to help ensure that your PRs get reviewed and merged
in a timely manner and are respectful of the reviewers' time and effort:

- Aim for small PRs that do a specific thing in a clear way. It is better to
  implement a large change with a series of PRs that each take a small,
  obvious step forward, than to have one huge PR that makes changes so
  extensive that nobody quite knows how to evaluate it.
- Make sure your PR commit summary message clearly explains the why and how of
  your changes. You want someone to read that and judge whether it's a
  sensible approach, know what to look for in the code, and have their
  examination of the code make them say "yes, I see, of course, great, just as
  I expected!"
- Make sure your PR includes a test (if not covered by existing tests).
- Make sure your PR fully passes our CI tests.
- Make sure your PR modifies the documentation as necessary if it adds new
  features, changes any public APIs, or alters behavior of existing features.
- If your PR alters the C++ API, make sure that the changes are also reflected
  in the Python bindings and any other language bindings we support.
- If your PR adds to or alters any ImageBufAlgo function, make sure you have
  exposed those changes with appropriate `oiiotool` commands as well.

### Code review procedures -- in the ideal case

In the best of circumstances, the code review process should be as follows for
**EVERY** pull request:

- At least one reviewer critiques and eventually (possibly after changes are
  made) approves the pull request. Reviewers, by definition, are not the same
  person as the author.
- Reviewers should indicate their approval by clicking the "Approve" button in
  GitHub, or by adding a comment that says "LGTM" (looks good to me).
- If possible, reviewers should have some familiarity with the code being
  changed (though for a large code base, this is not always possible).
- At least a few work days should elapse between posting the PR and merging
  it, even if an approving review is received immediately. This is to allow
  additional reviewers or dissenting voices to get a chance to weigh in.
- If the reviewer has suggestions for improvement, it's the original author
  who should make the changes and push them to the PR, or at the very least,
  the author should okay any changes made by the reviewer before a merge
  occurs (if at all practical).

Not all patches need the highest level of scrutiny. Reviews can be cursory and
quick, and merges performed without additional delay, in some common low-risk
circumstances:

- The patch fixes broken CI, taking the project from a verifiably broken state
  to passing all of our CI builds and tests.
- The changes are only to documentation, comments, or other non-code files,
  and so cannot break the build or introduce new bugs.
- The code changes are trivial and are obviously correct. (This is a judgment
  call, but if the author or reviewer or both are among the project's senior
  developers, they don't need to performatively pretend that they aren't sure
  it's a good patch.)
- The changes are to *localized* and *low risk* code, that even if wrong,
  would only have the potential to break individual non-critical features or
  rarely-used code paths.

Conversely, some patches are so important or risky that if possible, they
should be reviewed by multiple people, preferably from different stakeholder
institutions than the author, and ensure that the approval is made with a
detailed understanding of the issues. In these cases, it may also be worth
bringing up the issue up at a TSC meeting to ensure the attention of many
stakeholders. These include:

- *New directions*: Addition of major new features, new strategic initiatives,
  or changes to APIs that introduce new idioms or usage patterns should give
  ample time for all stakeholders to provide feedback. In such cases, it may
  be worth having the PR open for comments for weeks, not just days.
- *Risky changes*: Changes to core classes or code that could subtly break
  many things if not done right, or introduce performance regressions to
  critical cases.
- *Compatibility breaks*: changes that propose to break backward API
  compatibility with existing code or data files, or that change the required
  toolchain or dependencies needed to build the project.
- *Security*: Changes that could introduce security vulnerabilities, or that
  fix tricky security issues where the best fix is not obvious.

### Code review compromises -- because things are rarely ideal

We would love to have many senior reviewers constantly watching for PRs, doing
thorough reviews immediately, and following the above guidelines without
exception. Wouldn't that be great! But in reality, we have to compromise,
hurry, or cut corners, or nothing would ever get done. Some of these
compromises are understandable and acceptable if they aren't happening too
often.

- Lack of reviews: If no reviews are forthcoming after a couple days, a "are
  there any objections?" comment may be added to the PR, and if no objections
  are raised within another few days, the PR may be merged by the author if
  they are a senior developer on the project. This is a fact of life, but
  should be minimized (with a rigor proportional to where the patch falls on
  the "low risk / high risk" scale described above). If this is done, you
  should leave an additional comment explaining why it was merged without
  review and inviting people to submit post-merge comments if they
  subsequently spot something that can be improved.
- Time-critical patches: urgent security fixes, broken CI or builds, critical
  bugs affecting major stakeholders who are waiting for a repaired release,
  fixes that are blocking other work or preventing other PRs from advancing.
  In such cases, it is understandable for senior developers to try to speed up
  the process of getting the changes integrated (though the very nature of
  their criticality also indicates that it's worth getting a review if at all
  possible).
- Failing tests: Sometimes a PR must be merged despite failing CI tests.
  Usually this is for one of two reasons: (1) spurious CI failures are known
  to be occurring at that time and are unrelated to the PR; (2) the PR is part
  of a series of patches that are only expected to fully pass CI when the last
  piece is completed.

These changes are understandable, but we still are striving to reduce their
frequency. It is the responsibility of the senior developers, TSC members, and
major stakeholders to be monitoring the incoming PRs and helping with reviews
as much as possible, to ensure that review exceptions are rare.

### Code review pathologies -- things to avoid

These are anti-patterns that generally are indications of an unhealthy open
source project:

* An author merging their own PR without any review comments that indicate
  meaningful scrutiny or approval from another party, and without giving
  adequate warning that a merge will occur if no reviews are received.
* PRs languishing for weeks or months without any review or merge.
* PRs that don't adequately test their changes (basically crossing your
  fingers and hoping it works).
* PRs that are merged despite failing CI that might be related or, if
  unrelated, that might be masking problems with the PR being evaluated.

Again, sometimes these things happen out of necessity to keep the project
moving forward under constrained development resources. But we strive as much
as possible to ensure that they are rare exceptions.
