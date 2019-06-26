---
name: Build problems
about: I'm having trouble building OIIO. Help!
title: "[BUILD]"
labels: ''
assignees: ''

---

**PLEASE DO NOT REPORT BUILD TROUBLES AS ISSUES**

The best way to get help with your build problems is to ask a question on the
[oiio-dev developer mail list](http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org).

When you email about this, please attach one or both of the following:
1. The full verbose build log, which you can create like this:
   ```make clean ; make VERBOSE=1 > build.log```
2. The `CMakeCache.txt` file from your build directory.

If you are pretty sure that you have identified a **BUG** in the OpenImageIO
build scripts, please file a [bug report issue](https://github.com/OpenImageIO/oiio/issues/new?template=bug_report.md).
