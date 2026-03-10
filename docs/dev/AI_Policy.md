Proposed OpenImageIO Policy on AI Coding Assistants
===================================================

- first proposal as of 6-Mar-2026
- revised 10-Mar-2026


Use of "AI coding assistants" is permitted on this project, with the following
guidelines and principles.

Summary of our core values:
- Human always in the loop and is the responsible party.
- You're still on the hook for fully understanding and standing behind what
  you submit.
- Interact with the project and community yourself, not by agent.
- Disclose what tools you used and how.
- Don't waste maintainer's time with low quality PRs.

The long version:

### Human authorship and interaction with the project

**The human making a PR is considered the author** and is fully responsible
for the code they submit.

**No PRs should be submitted, reviewed/merged, or deployed without a human
fully in the loop.** You may use agents locally to help you write code (and
docs, tests, etc.), but you may not use agents to autonomously interact with
the project community. Submit your own PRs, write your own discussion
comments, respond to community members in your own words, approve or reject
code in review using your own brain. 

Code autonomously submitted without an identifiable human author and
responsible party will be immediately closed, and the account associated with
the submission may be banned from future project participation.

**DO NOT "vibe code."** This term refers to a pure prompt-to-behavior cycle
where the human accepts un-inspected code based on the program behavior alone.
We don't want this. Code that must be maintained by the project should be
truly understood. Project code is ideally the result of discussion / pair
programming between the author and assistant, but even if the machine is doing
most of the work, all code should be approved and fully understood by the
author prior to PR submission.

### Professionalism and quality

**The usual high quality level of PRs should be maintained regardless of tools
used.** This includes not only code, but also the design, testing, PR
preparation, and the author's ability to explain and defend the code and
respond to questions and requests for changes. We never want to hear "but the
AI..." as an excuse for anything.

**PRs should always be [reviewed](CodeReview.md) and approved by someone other
than their author if at all possible.** This is especially true of code with
machine-generated components, because of the additional risk that the
submitter/operator may not fully understand what they did not write
themselves. This is part of the reason for AI tool disclosure -- to ensure
that another pair of eyes is on it. AI code review may be requested for
additional input, but the AI cannot give "approval", nor merge code itself.

Even when using a coding assistant to write the code, you are strongly
encouraged to write your own PR description, in your own words, if for no
other reason than to force yourself to understand the code well enough to
describe it to others.

### Disclosure

**Disclosure is required.** Use of coding assistants to generate more than de
minimis changes should have a short disclosure in the PR submission comments.
Commits should have a "Assisted-by: " sign-off as a minimum. Ideally, the PR
description will also have a summary of what tool was used, for what purpose,
and paraphrase the gist of the key prompts or direction of the dialog. A full
log is not necessary; a short summary will do.

There are several reasons for this disclosure/documentation, even though it
will sometimes be inconvenient:
- Reproducibility: We want even automated work to be reproducible by others in
  principle, just like a scientific paper.
- Education: We want to actively teach each other what tools, prompts, or
  methods are successful at making high quality results for the project.
- Metrics: We would like the ability to look back and compare assisted vs
  non-assisted code for things like defect rate (how often were new bugs
  introduced or commits needed to be reverted), whether people are more likely
  to write comprehensive tests when using assistants, etc.
- Compliance: Many key users and developers are in companies where they need
  to disclose how AI tools were used in their work, and in other software that
  they use. Let's make it easy for them.
- Insurance: Should future legal decisions radically change the IP status of
  AI-generated code, or if particular models are implicated in copyright
  violations, we want a way to find out which contributions might need to be
  revisited in order to bring the project back into compliance.

### Intellectual Property

Regardless of how the code came to be -- from your head alone, from a friend,
from Stack Overflow, from a blog post, from reading other code bases, or from
a coding assistant -- we expect the terms of the
[DCO](https://developercertificate.org/) and CLA to apply, and for the author
to take reasonable care that code is not copied from a source with an
incompatible license.

You may find that your confidence about complying with the DCO+CLA depends on:

- If you provided a detailed specification and how much you edited or guided
  the results yourself.
- Whether you are modifying or extending existing code versus writing large
  amounts of entirely new code.
- The degree to which the generated code seems to fit into our structure and
  idiomatic style, and therefore seems unlikely to be copied from elsewhere.
- Whether your tool has guardrails to prevent answers that are too similar to
  existing code, for example as claimed by [Claude](https://privacy.claude.com/en/articles/10023638-why-am-i-receiving-an-output-blocked-by-content-filtering-policy-error) and [Copilot](https://docs.github.com/en/copilot/how-tos/manage-your-account/).

### Extractive submissions

**Maintainer time and attention are precious commodities**, and use of coding
assistants is not an excuse to submit poor PRs or to externalize costs onto
maintainers/reviewers (such as responsibility for understanding, testing, or
fixing PRs that the human author does not have a full understanding of). 

**We discourage use of AI tools to fix GitHub issues labeled as "good first
issue" or for "Dev Days" work.** Cultivating and educating new contributors is
important, and as such, we do not want people to swoop in and use automated
tools to trivially solve tasks that were curated specifically for somebody to
actually learn from.

Maintainers are free to take counter-measures against submitters of sub-par
PRs, or other violations of this policy, up to and including banning habitual
abusers from future participation in the project.

### Exceptions

This AI tool use policy is not meant to encompass cases such as:
- "Smart auto-complete", spell-checking, grammar checking, or other uses that
  aren't really contributing substantively to authorship.
- Use of LLMs to explain code or learn about the codebase, answer basic
  programming questions, or help with background research.
- Language translation for non-fluent English speakers or other accessibility
  accommodations.
- Trivial or de-minimis fixes such as fixing a typo, obviously wrong variable
  use, etc.
- Reviewing your own code for mistakes prior to submitting a PR (as long as it
  isn't making the fixes for you).

### References and inspiration

Our policy has been informed and inspired by the following efforts in other communities:
- [LLVM AI Tool Policy](https://llvm.org/docs/AIToolPolicy.html)
- [Fedora Project policy on AI-assisted contributions](https://communityblog.fedoraproject.org/council-policy-proposal-policy-on-ai-assisted-contributions/)
- [Linux Foundation policy on Generative AI](https://www.linuxfoundation.org/legal/generative-ai)
- [Rust policy on rejecting burdensome PRs](https://github.com/rust-lang/compiler-team/issues/893)
- The METR paper [Measuring the Impact of Early-2025 AI on Experienced Open-Source Developer Productivity](https://metr.org/blog/2025-07-10-early-2025-ai-experienced-os-dev-study/)


