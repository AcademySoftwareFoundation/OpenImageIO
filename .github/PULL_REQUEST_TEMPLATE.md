<!-- This is just a guideline and set of reminders about what constitutes -->
<!-- a good PR. Feel free to delete all this matter and replace it with   -->
<!-- your own detailed message about the PR, assuming you hit all the     -->
<!-- important points made below.                                         -->


## Description

<!-- Please provide a description of what this PR is meant to fix, and  -->
<!-- how it works (if it's not going to be very clear from the code).   -->

## Tests

<!-- Did you / should you add a testsuite case (new test, or add to an  -->
<!-- existing test) to verify that this works?                          -->


## Checklist:

<!-- Put an 'x' in the boxes as you complete the checklist items -->

- [ ] **I have read the guidelines** on [contributions](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/CONTRIBUTING.md) and [code review procedures](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/dev/CodeReview.md).
- [ ] **I have read the [Policy on AI Coding Assistants](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/dev/AI_Policy.md)**
  and if I used AI coding assistants, I have an `Assisted-by: TOOL / MODEL`
  line in the pull request description above.
- [ ] **I have updated the documentation** if my PR adds features or changes
  behavior.
- [ ] **I am sure that this PR's changes are tested in the testsuite**.
- [ ] **I have run and passed the testsuite in CI** *before* submitting the
  PR, by pushing the changes to my fork and seeing that the automated CI
  passed there. (Exceptions: If most tests pass and you can't figure out why
  the remaining ones fail, it's ok to submit the PR and ask for help. Or if
  any failures seem entirely unrelated to your change; sometimes things break
  on the GitHub runners.)
- [ ] **My code follows the prevailing code style of this project** and I
  fixed any problems reported by the clang-format CI test.
- [ ] If I added or modified a public C++ API call, I have also amended the
  corresponding Python bindings. If altering ImageBufAlgo functions, I also
  exposed the new functionality as oiiotool options.
