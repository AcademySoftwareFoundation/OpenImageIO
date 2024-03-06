---
name: Bug report
about: I think I have identified a legit bug and can describe it.
title: "[BUG]"
labels: ''
assignees: ''

---

**Describe the bug**

A clear and concise description of what the bug is. What happened, and
what did you expect to happen instead.


**OpenImageIO version and dependencies**

Please run `oiiotool --buildinfo` and paste the output here.

Also please tell us if there was anything unusual about your environment or
nonstandard build options you used.


**To Reproduce**

Steps to reproduce the behavior:
1. Do this...
2. Then this...
3. Then THIS happens (reproduce the exact error message if you can)
4. Whereas I expected this other specific thing to happen instead.

If the problem occurs in your C++ or Python code that uses the OIIO APIs, can
you also reproduce the problem using oiiotool? If so, please describe the
exact command line that reproduces the problem. (Being able to reproduce the
problem using only OIIO components makes it a lot easier for the developers
investigate and makes it clear it's not your application's fault.)


**Evidence**

- Error messages (paste them here exactly)
- Screenshots (if helpful)
- Example input: If the problem only happens with certain image files, please
  attach the smallest image you can make that reproduces the problem.


**IF YOU ALREADY HAVE A CODE FIX:** There is no need to file a separate issue,
please just go straight to making a [pull request](https://github.com/AcademySoftwareFoundation/OpenImageIO/pulls).
