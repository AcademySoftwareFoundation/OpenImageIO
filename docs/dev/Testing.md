<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->


Testing OpenImageIO
===================

How to run the tests, add new ones, update reference output, and diagnose
failures.

## Running tests

Common test commands via the Makefile convenience wrapper:

```bash
make test                             # full testsuite
make test TEST=<pattern>              # subset matching regex
```

Or directly with cmake:

```bash
ctest --test-dir build --output-on-failure  # run without rebuilding
ctest --test-dir build -R <pattern>         # only tests whose names match regex
```


## Test structure

| Category | Location and registration | Description |
|----------|---------------------------|-------------|
| **C++ unit tests** | `src/lib*/*_test.cpp`; registered as `unit_*` in nearby `CMakeLists.txt` files | Standalone binaries that test C++ components directly. |
| **End-to-end tests** | `testsuite/*/run.py`; registered in `testing.cmake` | Run command-line tools or scripts and usually compare generated files against `ref/`. |
| **Python binding tests** | `testsuite/python-*`; registered in the Python sections of `testing.cmake` | End-to-end tests that exercise the Python bindings. |
| **Platform-specific tests** | Registered directly with CTest rather than as testsuite tests | Run only on applicable platforms, and run their own script instead of `runtest.py`. |


### Test registration and availability

End-to-end tests are registered at CMake configuration time, in
`oiio_add_all_tests()` in
[src/cmake/testing.cmake](../../src/cmake/testing.cmake). One is skipped when
its `oiio_add_tests()` entry names:

| Keyword | Skips the test when |
|---------|---------------------|
| `FOUNDVAR` | the variable is undefined or false, meaning a dependency is missing |
| `ENABLEVAR` | the variable exists and is false, either as a CMake variable or in the environment; not existing at all is fine |
| `DISABLEVAR` | the variable exists and is true |
| `IMAGEDIR` | the named external test data directory is absent |

A skipped test is never registered, so it does not appear in `ctest -N`. After
adding a test to `oiio_add_all_tests()`, reconfigure the build so CTest can
see it.

### Test variants

Some tests are registered more than once with a suffix, re-running the same
test under another configuration: `.batch` (`TESTTEX_BATCH=1`), `.core`
(`OPENIMAGEIO_OPTIONS=openexr:core=1`), `.hwy` (`OPENIMAGEIO_ENABLE_HWY=1`),
and `.nanobind` (the nanobind bindings). Each variant gets its own directory
under `build/testsuite/`, so for example:

```
build/testsuite/texture-derivs
build/testsuite/texture-derivs.batch
```

### External test data

Some image collections do not ship with OIIO: `oiio-images`, `openexr-images`,
`fits-images`, and `j2kp4files_v1_5`. CMake stages them into
`build/testsuite/` at configure time. Configure with
`-DOIIO_DOWNLOAD_MISSING_TESTDATA=ON` to clone a missing `oiio-images` or
`openexr-images`. For anything it cannot clone, CMake prints where to find it.

`OIIO_TESTSUITE_IMAGEDIR` is the base path to a test's external images. For
instance, `openexr-suite` is registered with `IMAGEDIR openexr-images` in
[src/cmake/testing.cmake](../../src/cmake/testing.cmake#L454-L457), so its
[run.py](../../testsuite/openexr-suite/run.py#L12) can do:

```python
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/ScanLines"
# -> build/testsuite/openexr-images/ScanLines
```

Tests registered without `IMAGEDIR` get plain `build/testsuite/`.


## Anatomy of an end-to-end test

```
testsuite/mytest/
    ref/       expected output
    src/       optional: input fixtures
    run.py     required: builds the commands, names the outputs
```

When writing `run.py` yourself, there are variables and helpers that you might
want to take advantage of. [runtest.py](../../testsuite/runtest.py) defines
them before it `exec`s your `run.py`, so they are already in scope. The most
useful ones are:

| Variable | Default | Meaning |
|----------|---------|---------|
| `command` | `""` | shell commands to run |
| `outputs` | `["out.txt"]` | files to compare against `ref/` |
| `redirect` | `" >> out.txt "` | where output goes; use `" >> out.txt 2>&1 "` to also capture stderr |
| `failureok` | `0` | if true, no command in the test fails on a nonzero exit (for tests that provoke errors) |
| `anymatch` | `False` | if true, after the exact name, try every `*.*` entry in each reference directory |
| `failthresh` | `0.004` | per-pixel difference tolerated by `idiff` |
| `failpercent` | `0.02` | percentage of pixels allowed to exceed `failthresh` |
| `hardfail` | `0.012` | one pixel this far off fails outright |
| `allowfailures` | `0` | freebie failing pixels |
| `refdirlist` | `["ref/"]` | complete ordered list of reference directories; append extras |
| `wrapper_cmd` | `""` | prefix for commands built by `oiio_app` and helpers that call it |

| Helper | What it builds |
|--------|----------------|
| `oiiotool(args)`, `iconvert(args)` | an invocation of that tool |
| `oiio_app(name)` | the path to any other tool in the build tree |
| `run_app(cmd)` | an arbitrary command, with the usual redirect |
| `info_command(file)` | `oiiotool --info -v -a --hash`; `safematch=True` drops fields that change run to run |
| `diff_command(a, b)` | `idiff` at the current thresholds |
| `maketx_command(in, out)` | `maketx`, optionally followed by an info dump |
| `rw_command(dir, file)` | the read/write round trip: info + hash, convert a copy, `idiff` it against the original |
| `testtex_command(file)` | a `testtex` invocation |
| `pythonbin` | the interpreter to use for Python tests |

Most take `silent=` to skip the redirect. `oiiotool`, `iconvert`, and
`info_command` also take `failureok=`, which appends `|| true` to that one
command.

A test passes when its commands succeed and every entry in `outputs` matches
something in `ref/`; an empty `outputs` checks the commands only. Images are
compared with `idiff` at the thresholds above, and `.txt` files with a unified
diff.


## Adding tests

**To an existing test:**<br>
append to `command`, run it, check the new output, then copy it into `ref/`.

```python
command += oiiotool ("a.tif b.tif --echo \"TOP = {TOP.filename}\"")
```

**A new end-to-end test:**<br>
create `testsuite/mytest/run.py` (copy a license header from a neighbor) plus
`src/` fixtures if needed. Register it in `oiio_add_all_tests()`, using
`FOUNDVAR`/`ENABLEVAR`/`IMAGEDIR` so it skips rather than fails where the
dependency is absent:

```cmake
oiio_add_tests (mytest ENABLEVAR ENABLE_PNG IMAGEDIR oiio-images/png)
```

Re-run cmake configure, run the test, check its output by eye, then copy it
into `ref/`.

**A new C++ unit test:**<br>
use the macros in [unittest.h](../../src/include/OpenImageIO/unittest.h)
(`OIIO_CHECK_EQUAL`, `OIIO_CHECK_ASSERT`, `OIIO_CHECK_EQUAL_THRESH`, ...) and
end `main()` with `return unit_test_failures;`, or the test can never fail.
Put the source next to the code it tests and register it in that directory's
`if (OIIO_BUILD_TESTS AND BUILD_TESTING)` block; see
[src/libOpenImageIO/CMakeLists.txt](../../src/libOpenImageIO/CMakeLists.txt#L245).

Unit tests suit anything checkable without files: math, strings, memory
layout, API edge cases, error paths. Use an end-to-end test when the behavior
only shows up through a real file or a real tool.

**A new Python binding test:**<br>
same structure, with a `run.py` that runs a script; see
[testsuite/python-imagebuf/run.py](../../testsuite/python-imagebuf/run.py) for
an example.

Register a binding test in the Python block of `testing.cmake`, in both the
pybind11 and nanobind lists when it supports both backends. Binding tests are
skipped for sanitizer builds. A Python script that does not exercise the
bindings should be registered as an ordinary end-to-end test rather than
duplicated in the backend-specific lists.

### What to cover

- **Bug fix:** a regression test.
- **New feature:** its error paths too. `failureok = 1` allows an expected
  nonzero exit, and `redirect = " >> out.txt 2>&1 "` captures the error so the
  reference comparison can assert that it was reported gracefully.
- **New `ImageBufAlgo`:** C++ (or the docs examples), `oiiotool` if exposed
  there, and Python.
- **Format plugin:** a round trip (`rw_command`), metadata, and edge cases;
  corrupt-file regressions with a tiny fixture in `src/`.
- **CLI:** exercise the new option in the test for that tool: `oiiotool-*`,
  `iinfo`, `maketx`, or `igrep`.


## Updating reference output

`build/testsuite/<name>/ref` is a symlink into the source tree, so:

```bash
ctest --test-dir build -R '^mytest$' --output-on-failure
cd build/testsuite/mytest
diff ref/out.txt out.txt        # read this
cp out.txt ref/out.txt          # only if every difference is intended
```

(On Windows `ref/` is a copy, so update `testsuite/<name>/ref/` directly.)

A difference you cannot explain is a bug, not a stale reference. Overwriting
one without reading the diff turns a real regression into a permanent expected
result.

If the output is correct but merely differs by platform, compiler, or
dependency version, add a *variant* named for whatever makes it differ,
instead of overwriting. A test passes if an output matches any
`ref/<prefix>-*<ext>*`:

```
testsuite/mytest/ref/out.txt              the common case
testsuite/mytest/ref/out-win.txt          Windows
testsuite/mytest/ref/out-libfoo3.4.txt    another dependency version
```

Images work the same way; `testsuite/oiiotool-text/ref` carries several for
different FreeType versions. Delete variants that nothing builds any more, since
each is another chance for a real regression to match by accident.

When an image differs only because a decoder changed, loosening that test's
`idiff` thresholds is sometimes the least-bad fix:

```python
# Fairly high hard fail, since libraw seems to diddle with its debayering
# from version to version, it's hard to make a single reference image.
hardfail = 0.017
```

Raise the minimum needed and say why: too loose a threshold silently
retires the test, and `CI` or `DEBUG` doubles whatever the test sets. If that
is not enough, check in an image variant instead.

`info_command(file)` writes a pixel hash to `out.txt`, verifying exact decoded
pixels without checking an image into the repo. It also dumps image info and
verbose metadata into that text reference, so prefer a real image comparison
when output may legitimately shift across platforms.


## Diagnosing failures

On a `.txt` mismatch, `runtest.py` prints the output, the reference, and the
diff; for an extension in `image_extensions`, it prints the `idiff` results.
So `--output-on-failure` usually tells you what happened. After that:

1. `build/testsuite/<name>/` holds everything the test produced. The driver
   always creates or truncates `out.txt` and `out.err.txt`; commands may then
   populate either one. A failed `.txt` comparison writes `<name>.diff`.
   A failed command does not stop the run, so later commands keep appending
   to `out.txt` and the mismatch you see may not be the original error.
2. `ctest --test-dir build -R '^name$' -V` shows the exact commands, so you
   can rerun a single failing `oiiotool` invocation and iterate.
3. If the test never ran at all, look at the cmake configure output.
4. If it fails only in CI, download the run's artifact zip from the GitHub
   Actions page: it holds the output of each failing test, so you can compare
   what CI produced against what you get locally.
5. If it fails only on someone else's machine, check the environment before
   the code. End-to-end tests registered by `oiio_add_tests` pin OpenColorIO
   (`OCIO=ocio://default`). `runtest.py` doubles image thresholds when `CI` or
   `DEBUG` is set. CI pins `OPENIMAGEIO_OPTIONS=limits:imagesize_MB=32768`
   so error messages are identical everywhere; locally the limit scales with
   physical memory and is often lower. A per-test value of that variable, such
   as the OpenEXR core setting, replaces the inherited one entirely. These
   differences can change what a test prints.
