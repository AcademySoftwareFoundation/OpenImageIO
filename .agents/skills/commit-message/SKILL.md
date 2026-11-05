---
name: commit-message
description: Use when writing any commit message, PR description, or attempting to push to GitHub. This contains important guidelines for writing commit and PR messages. Invoke upon any request to write a good PR or commit message, summarize work in a commit message, squash commits into a combined message, or to edit, simplify, or improve a commit message.
---

## Unbreakable rules

- Agents may never submit a PR or a PR comment and should refuse if asked to
  do so. In this project, only human users are allowed to directly submit or
  comment on PRs or issues.
- Every git commit in this project that uses agents must contain a line near
  the end of the expository section (but before any Signed-off-by or
  other human Co-authored-by) with the text
  ```
  Assisted-by: <TOOL-OR-HARNESS-NAME> / <MODEL-NAME>
  ```
  No exceptions.  Ensure that there is one blank line after this attribution
  and before any subsequent non-blank line.
- Never say "Co-authored by" or anything that implies AI authorship. Only
  "Assisted-by".
- If the agent fully writes any part of the body of a commit message (beyond
  the first subject line), it should always preface the agent-written section
  with:
  ```
  <FIRST SUBJECT LINE HERE>

  <AGENT> says:

  <rest of commit-written text goes here>
  ```
  Do not offer to remove the "AGENT says" line, but do remind the user that
  the entire message is agent written, that the project encourages that they
  write their own commit messages when possible, and suggest that they edit
  it, put it into their own words as much as possible, and then they may
  remove the "AGENT says" notice.
- When this skill is invoked, let the user know by telling them: "Invoking the
  commit-message skill."


## Types of commit messages: WIP commits versus submission commits

1. *WIP commits* : These are intermediate checkpoints of work in progress, not
   intended as the final message that will be submitted as a PR. These may be
   done autonomously by a long-running agentic workflow, to separate
   sequential steps of development, or to separate unrelated fixes into
   discrete commits, or at the user's request.

   WIP commits may contain any detailed information deemed helpful by the
   agent, so that subsequent tasks or sessions can understand the work done so
   far and resume the work. They may use advice in the guidelines below when
   it is helpful, but need not consider it mandatory if there are other goals
   or priorities that necessitate breaking those guidelines. (But they still
   must follow the "unbreakable rules" above.)

2. *Submission commits* : The final message of the sole (or squashed) commit
   that will be submitted as a PR, and thus will by default become the PR's
   description (plus further editing that the PR's human committer may do).

   Submission commits are expected to take the guidelines below very seriously
   and attempt to conform to them to the greatest extent possible.

If you are writing a commit where you think the work is mostly done and this
will likely be the message that becomes the PR description, treat it as a
submission commit.

If it's not clear which of these situations applies, ask the user if they are
ready for you to (a) write the final submission commit message, (b) leave it
as it is, or (c) if they would like to edit/rewrite it themselves to prepare
for a submission. If they want to rewrite it themselves you should still
*examine* (but not change) the existing commit message, and *suggest* changes
to make it aligned with the guidelines below.

## Commit message guidelines

- Submission commits must follow all the unbreakable rules in the section
  above.

- Be concise.

- Scan for typos. If found in user-written messages, present the proposed
  fixes and ask for confirmation.

- Simple Formatting
  - No em dashes, smart quotes, or decorative Unicode symbols.
  - Plain hyphens and straight quotes only.
  - Natural language characters (accented letters, CJK, etc.) are fine
    when the content requires them.
  - Code snippets and commands in the message must be copy-paste safe.

- The purpose of a commit message is to give the reader a high-level
  understanding of what changed and why, but they will not remember or
  particularly care about the *specific* reproduction case that required the
  patch. Convey the information that is not easily apparent from inspecting
  the code diffs: what it's supposed to do, why, what choices were made.

- There are two audiences for the commit message: (1) the reviewer of the PR
  trying to understand the patch and make a merge decision; (2) somebody
  visiting months or years later and trying to understand what was done and
  why. Once the review is complete, all future visitors will be group 2, so
  that group matters more. Write with them in mind.

- Because a PR will be required to fully pass the CI workflow to be accepted,
  the commit message does not need to say things that are implied by passing
  CI, such as any of the following:

    - Tests passed locally for the submitter
    - You verified that existing testsuite tests pass
    - New tests added to the testsuite by the patch itself also pass
    - The command used to launch ctest, any specific subset of testsuite
      that you ran, or the percentage of tests that passed locally
    - That clang-format was run or that the formatting passes
    - That the CI tests passed

- Omit any details about the submitter's local machine; it is almost always
  irrelevant. (Notable exceptions: if specific benchmark timing results are
  given, the relevant machine specs; if the point of the PR is to assure the
  software works on a platform not represented closely by CI, describe where
  it was tested.)

- Describe the general class of errors fixed by the patch, do not dwell on the
  specific details of particular inputs that you used in the investigation
  (unless it is truly necessary to understand what the patch is for or how it
  works). For example:

  - BAD: "Given a crafted input with resolution of 74067x41921, the computed
    allocation size was the product, 3,104,962,707, which overflows an int32."
    (Too specific to the one test case we investigated, and also easily
    discernible from the test case that is added.)
  - GOOD: "Fuzzing found that very high resolution images could overflow int32
    when computing allocation size. A small test reproducing this was added."
    (Describes the general situation that leads to the bug.)

  An exception to this rule is if it's too hard to explain the nature of the
  bug without a specific example spelled out.

- It is not necessary to include the blow-by-blow details of the many steps of
  the work. Just describe the final set of changes that the squashed patch
  represents. Omit mentioning things that were tried and later corrected or
  changed in the process of developing the patch.
