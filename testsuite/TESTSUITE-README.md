Diagnosing failed tests and updating reference output
-----------------------------------------------------

A few tips that may help in diagnosing tests that fail for a new platform
or when dependent libraries are updated, causing failures:

A few tips that may help in working our way through these tests:

First, when CI fails, our scripts are set up to create an "artifact"
downloadable from the GitHub page that gives the status summary of the CI
run. The artifact is a zip file containing the output of any individual
tests that failed. Downloading this zip file and unpacking it locally is a
good way to inspect exactly what the failed test output looked like
(especially if you are investigating a test that seems to only fail during
CI but you can't reproduce it locally). The actual test output will be in
(say) `build/<platform>/testsuite/<testname>`, and the reference output we
are trying to match will be in the `ref/` subdirectory underneath that.

Second, many of these "failures" (not the timeouts, but perhaps many of the
others) are running correctly, but simply failed to *exactly* match the test
output. This can happen for a number of reasons that are benign (for
example, decoding some lossy file formats may get occasional single bit
differences depending on the platform and the version of the decoding
software). In this case, the "fix" is merely updating the reference output
so that there is a match. But of course the trick is that you don't want to
simply *replace* the existing reference output with the new Windows one,
because then of course it may not match on Linux and those tests will start
failing.

So the solution to the fact that reference output may be different for
different platforms is that the way our test scripts are written, the test
passes if the output matches *any* of the files found in the `ref/`
subdirectory of the test. Thus, for most tests, you will see an `out.txt` or
whatever, but sometimes you will see additional files like `out-osx.txt` or
`out-libfoo3.4.txt` or whatever. So for many of these "failing" tests where
the output seems plausibly correct, but is merely slightly different on
Windows, it's fine to fix by simply adding a
`testsuite/<testname>/ref/out-win.txt`.

When it's images that are the output, we're using OIIO's `idiff` with flags
that allow just a teensy bit of low-bit errors in a few pixels. Sometimes, a
failure is because it's slightly exceeding that. If so, you may be able to
keep the existing reference output but just adjust the test to allow more
slightly differing pixels. You can see how this is done
[here](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/testsuite/raw/run.py#L30),
where we have raised the image matching threshold quite a bit because
libraw's debayering seems to change a lot from version to version (making
different, but usually perceptually indistinguishible outputs). Be gentle
raising these thresholds, you wouldn't want to inadvertently make it
impossible to notice a real problem in the future because the images are
allowed to differ by too much. In the cases where a small amount of diddling
the diff thresholds is not enough to allow the CI and reference outputs to
match, you can just check in a new output test image variant (like with the
text files), giving it a name to distinguish it in a self-documenting way.
For example, you can see in `testsuite/oiiotool-text/ref` that several of
the output images have multiple reference variants, due to the appearance of
the text in the image varying a bit from one version of freetype to another.

