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

- [ ] I have read the [contribution guidelines](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/CONTRIBUTING.md).
- [ ] I have updated the documentation, if applicable. (Check if there is no
  need to update the documentation, for example if this is a bug fix that
  doesn't change the API.)
- [ ] I have ensured that the change is tested somewhere in the testsuite
  (adding new test cases if necessary).
- [ ] If I added or modified a C++ API call, I have also amended the
  corresponding Python bindings (and if altering ImageBufAlgo functions, also
  exposed the new functionality as oiiotool options).
- [ ] My code follows the prevailing code style of this project. If I haven't
  already run clang-format before submitting, I definitely will look at the CI
  test that runs clang-format and fix anything that it highlights as being
  nonconforming.
