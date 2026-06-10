# OpenImageIO Agent Guide

This file provides guidance to coding assistants when working with code in
this repository.

## Project Overview

OpenImageIO (OIIO) is a C++17 library and toolset for reading, writing, and
manipulating image files across many formats, designed for VFX/animation/film
production. Part of the Academy Software Foundation (ASWF). Canonical repo:
http://github.com/AcademySoftwareFoundation/OpenImageIO

**Core libraries:**

- `libOpenImageIO` — Main library: ImageInput/ImageOutput (file I/O), ImageBuf/ImageBufAlgo (image processing), ImageCache/TextureSystem (texture mapping)
- `libOpenImageIO_Util` — Utility library (string_view, ustring, span, threading, filesystem, etc.)

**Command line tools**

- `oiiotool`: general image processing CLI
- `iinfo`: print info & metadata for an image file
- `iconvert`: simple format conversion
- `maketx`: convert images to tiled and MIP-mapped textures.
- `iv`: image viewer

## Repo map

- `src/include/OpenImageIO/` : Public API headers
- `src/libOpenImageIO/` : Core library (ImageBuf, ImageBufAlgo,
  ImageInput/ImageOutput base classes)
- `src/libtexture/` : ImageCache, TextureSystem
- `src/libutil/` : Utility class implementations
- `src/<FORMAT>.imageio/` : Per-format ImageInput/ImageOutput plugins.
- `src/python/` : Python bindings using pybind11
- `src/<TOOL>` : CLI tools (oiiotool, iinfo, iconvert, maketx, iv)
- `testsuite/` : End-to-end/regression tests + reference outputs
- `src/cmake/`,  `CMakeLists.txt` : Build system
- `.github/workflows/ci.yml` : GitHub Actions CI
- `src/build-scripts`  Helper scripts used for build & CI
- `src/doc/` : User manual source (+ Doxygen comments in the public headers)
- `docs/dev/` : Developer documentation
- `docs/dev/Architecture.md` : Major subsystems overview


## Build and verification

Common build commands via the Makefile convenience wrapper:
```bash
make                        # configure, build, install (Release)
make debug                  # debug build
make clean                  # wipe build dir (needed when switching branches/modes)
make clang-format           # format all source files
make test                   # full testsuite
make test TEST=<pattern>    # subset matching regex
```

Or directly with cmake:
```bash
cmake -B build -S .
cmake --build build --target install
ctest --test-dir build -R <pattern> --output-on-failure
```

By default, builds into `./build` and installs into `./dist`.

## Testsuite notes

- Test output lands in `build/testsuite/<testname>/`; references in
  `testsuite/<testname>/ref/`
- Read `testsuite/TESTSUITE-README.md` before updating references or
  diagnosing failures
- For platform-specific diffs, add a variant ref (e.g. `out-win.txt`) rather
  than overwriting
- Be conservative loosening image diff thresholds — use the minimum needed
- Check uploaded CI artifacts before changing references when local
  reproduction is unclear

## Code formatting and file conventions

- `clang-format` enforced (`.clang-format`); CI rejects non-conforming code —
  run `make clang-format` before committing
- Lines ~80 cols; ASCII only in code and comments; `#pragma once` for headers
- New files: standard copyright + SPDX notice
- `CamelCase` classes, `snake_case` locals, `ALL_CAPS` macros, `m_foo` private
  members
- 3 blank lines between free functions/classes; 1 between class methods; max 1
  blank line inside a function body
- `//` for regular comments; `///` Doxygen for public API
- If a file has a strong local style, imitate that.

## C++ guidelines

- Error handling: prefer `bool` returns, explicit status propagation, and
  `errorfmt()`-style reporting — not exceptions — consistent with the
  surrounding subsystem
- Preserve API, ABI, and behavior compatibility unless the task explicitly
  requires a break
- For hot paths: no hidden allocation in inner loops or parallel regions;
  precompute outside hot paths
- For kernel-style image work, use existing `ImageBufAlgo` and
  `parallel_image` patterns before inventing a new execution model

## OIIO utility types (prefer over raw C++ equivalents)

Prefer C++17 `std` and Imath types except as noted below. Avoid introducing
new third-party dependencies without a strong reason.

- `OIIO::string_view` — non-owning string/`char*` (like `std::string_view`)
- `OIIO::span` / `OIIO::cspan` — non-owning contiguous data (like
  `std::span`); use instead of pointer+length pairs
- `OIIO::image_span` — describes pixel buffer memory layout (pointer, sizes,
  strides)
- `OIIO::Filesystem::*` — file/directory utilities (`filesystem.h`)
- `strutil.h` : string processing utilities
- `Strutil::format()` / `Strutil::print()` (= `OIIO::print()`) — string
  formatting/output; **never** `printf` or `<<` streams
- `fmath.h` — fast/safe math, avoids NaN/Inf
- `simd.h` — SIMD helpers
- `unittest.h` — unit test macros

## Safe programming in C++

- Try to avoid passing raw pointers as function arguments.
- Use `std::unique_ptr` and `std::shared_ptr` rather than raw ownership when
  new ownership must be expressed, but do not churn existing code just to
  "modernize" it.
- Prefer `OIIO::string_view` when passing non-mutable strings or C-style
  `char*` strings.
- Prefer `OIIO::span` rather than passing raw pointers + a separate length, or
  passing a raw pointer with an implied (but not explicitly passed) length.
  `OIIO::cspan` is a synonmym when the underlying data is const/non-mutable.
  `OIIO::span<std::byte>` or `OIIO::cspan<std::byte>` can be used to represent
  contiguous untyped data. These are our equivalent of C++ `std::span`.
- Prefer `OIIO::image_span` to describe the memory layout of multi-dimensional
  pixel buffers (such as are used to describe where to read or write image
  pixels), rather than passing raw pointers, sizes, and strides.

## Performance and determinism

- For hot image-processing code, avoid hidden allocation in inner loops or
  parallel regions.
- Precompute setup work outside hot paths when practical.
- Keep data flow and ownership explicit.
- Preserve deterministic results where practical.
- Avoid unnecessary global state or synchronization.
- For kernel-style work, look for existing `ImageBufAlgo` and
  `parallel_image` patterns before inventing a new execution model.
- Prefer incremental performance fixes backed by measurement or concrete
  reasoning over speculative rewrites.

## Change impact checklist

- Bug fixes and behavior changes → add/update tests
- Public API changes → update Python bindings, docs, `CHANGES.md`
- `ImageBufAlgo` changes → consider oiiotool exposure, docs, Python, tests
- Format plugin changes → regression coverage for metadata, edge cases,
  read/write
- CLI changes → update tests and help text
- Build/dependency changes → check CMake logic, CI, optional-feature coverage

## Commits and PRs

- Keep PRs narrow and easy to review.
- Prefix format: `type(subsystem): message` — subsystem is optional but helpful.
- Valid types: `fix:`, `feat:`, `perf:`, `api:`, `int:`, `build:`, `test:`,
  `ci:`, `docs:`, `refactor:`, `style:`, `admin:`, `revert:`.
- Add a subsystem tag when it helps, e.g. `fix(exr):` or `perf(IBA):`.
- Write commit messages and PR descriptions that explain why the change is
  needed, what behavior changes, and any non-obvious implementation choices.

## AI policy

Refer to `docs/dev/AI_Policy.md`.

See `docs/dev/AI_Policy.md`. Key rule: if AI assistance contributed materially
to a patch, the commit must include `Assisted-by: <TOOL> / <MODEL>`. The human
author is responsible for understanding, testing, and defending all changes.

## References

- `CONTRIBUTING.md` : general contribution guidelines and recap of coding
  conventions.
