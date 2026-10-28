# OIIO project facts for check-dependency-minmax

Project-specific information used by `SKILL.md`. Keep this up to date;
the Open Shading Language copy of this file has the same sections with
different contents.

## Buildinfo tool

- Tool: `oiiotool`. `src/build-scripts/ci-test.bash` runs
  `$OpenImageIO_ROOT/bin/oiiotool --version` and then
  `oiiotool --buildinfo`.
- Local build: `find dist build -maxdepth 4 -name oiiotool -type f`.
- `ci-test.bash` may append a `Recovered from clone failures of: ...` line
  to the summary right after the buildinfo; the parser stops there.

## CI job conventions

- `oldest` jobs (analysis B): `desc` starts with `oldest`, plus `hobbled`.
- `latest releases` jobs (analysis C): `desc` contains `latest releases`
  (including the Linux ARM ones).
- `bleeding edge` job (analysis D): `bleeding edge ...`.
- `VFX20xx` jobs use the ASWF VFX Platform containers for that year, so
  most dependencies are container versions.
- Jobs with no buildinfo: `ABI check ...`, `clang-format`, and anything
  with `skip_tests`.

## Buildinfo data quirks

- Duplicate entries in `Dependencies:` (libdeflate, zstd, ZLIB, Ptex) are
  normal; the first wins.
- `Ptex present` means the version was not detected.
- `JPEG 80` is the libjpeg API version, not a release (so it says nothing
  about jpeg 9 or turbo version).
- buildinfo does not report NumPy.
- buildinfo labels `__cplusplus` 202400 as C++23 even for C++26 builds
  (cosmetic).

## Key dependencies for the description check (analysis E)

OpenEXR, OCIO, Qt, and simd (`avx2`, `no-sse`, ...), in addition to the
compiler, C++ std, and Python that every job is checked for.

## Where to find latest upstream releases (step 4)

| Dependency | Where to look |
|---|---|
| fmt | fmtlib/fmt |
| Imath, OpenEXR, OCIO, OpenVDB | AcademySoftwareFoundation/{Imath,openexr,OpenColorIO,openvdb} |
| libtiff | GitLab: `https://gitlab.com/api/v4/projects/libtiff%2Flibtiff/repository/tags` |
| libjpeg-turbo | libjpeg-turbo/libjpeg-turbo (tags also include jpeg-9x/10; ignore) |
| zlib | madler/zlib |
| robin-map | Tessil/robin-map |
| pugixml | zeux/pugixml |
| nanobind | wjakob/nanobind (drop `-dev` tags) |
| pybind11 | pybind/pybind11 |
| libpng | pnggroup/libpng (tags only, `^v1\.6\.[0-9]+$`) |
| LibRaw | LibRaw/LibRaw |
| OpenJPEG | uclouvain/openjpeg (tags `^v[0-9]`) |
| libjxl | libjxl/libjxl |
| libheif | strukturag/libheif |
| DCMTK | DCMTK/dcmtk (`DCMTK-x.y.z`) |
| WebP | webmproject/libwebp |
| Ptex | wdas/ptex |
| Freetype | freetype/freetype (`^VER-2-[0-9]+-[0-9]+$`) |
| libultrahdr | google/libultrahdr |
| OpenJPH | aous72/OpenJPH |
| TBB | uxlfoundation/oneTBB |
| OpenCV | opencv/opencv (numeric tags) |
| ffmpeg | FFmpeg/FFmpeg (`^n[0-9.]+$` tags) |
| giflib | `https://sourceforge.net/projects/giflib/best_release.json` |
| Qt | qt/qtbase (`^v6\.[0-9]+\.[0-9]+$`; Qt5 is EOL, use INSTALL) |
| pystring | imageworks/pystring |
| CMake | Kitware/CMake (`^v[0-9]+\.[0-9]+\.[0-9]+$`) |
| NumPy | numpy/numpy |
| Python | python.org; CI uses runner/container Pythons |
