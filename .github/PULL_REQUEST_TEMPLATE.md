


------ :scissors: -------------------------------------------------------------------

YOU MAY DELETE ALL OF THIS IF YOU ALREADY HAVE A DESCRIPTIVE COMMIT MESSAGE!

This is just a template and set of reminders about what constitutes a good PR.
But please look over the checklist at the bottom.

If THIS TEXT is still in your PR description, we'll know you didn't read the
instructions!




### Description

<!-- Please provide a description of what this PR is meant to fix, and  -->
<!-- how it works (if it's not going to be very clear from the code).   -->

### Tests

<!-- Did you / should you add a testsuite case (new test, or add to an  -->
<!-- existing test) to verify that this works?                          -->


### Checklist:

<!-- Put an 'x' in the boxes as you complete the checklist items -->

- [ ] **I have read the guidelines** on [contributions](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/CONTRIBUTING.md) and [code review procedures](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/dev/CodeReview.md).
- [ ] **I have updated the documentation** if my PR adds features or changes
  behavior.
- [ ] **I am sure that this PR's changes are tested somewhere in the
  testsuite**.
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
