# Known issues from past audits of OIIO (update this list)

Problems found by `check-dependency-minmax` audits, with the date found,
their status, and the right fix once known. Mark resolved items instead of
deleting them while the lesson is still useful (for example, a fix that was
tried and had to be reverted).

From the 2026-09-29 audit (all still to be resolved unless noted):
- `oldest` jobs used to set `BUILD_PNG_VERSION=1.6.0`, which nothing reads,
  so system libpng is used. Renaming it to `LIBPNG_VERSION=v1.6.0` made
  CI fail (building libpng 1.6.0 broke other dependencies on those
  machines), so the useless line was just deleted (2026-10). libpng in
  oldest/hobbled stays the system version (1.6.37); do not try to pin it.
- `hobbled` sets `USE_JPEGTURBO=0` yet libjpeg-turbo still appears in the
  summary.
- ASWF container jobs use libtiff 4.0.9, below INSTALL's minimum of 4.1.
- Latest-release jobs use Ubuntu apt Imath 3.1.9 with OpenEXR 3.5.x, and
  apt ffmpeg/giflib/libheif/TBB/OpenCV/zlib/Python that are far older
  than the newest releases.
- `bleeding edge` briefly pinned OpenEXR to a tag (lj2k bug); restored to
  `main` on 2026-09-30. Verify on the next audit that the summary shows a
  main-built OpenEXR, and whether Imath really follows main (the 09-29 run
  found system Imath 3.1.9 even with OpenEXR built from a tag, since
  `build_openexr.bash` may use a system Imath if found).
- Imath: `build_openexr.bash` (used when a job sets `openexr_ver`) now
  builds the Imath that OpenEXR prefers (`OPENEXR_FORCE_INTERNAL_IMATH=ON`,
  installed next to OpenEXR) and exports `Imath_ROOT` so OIIO uses it.
  What "prefers" means varies: v3.1.0 -> Imath v3.1.1, v3.4.x -> v3.2.2,
  v3.5.x and main -> Imath `main` (override with
  `-DOPENEXR_IMATH_TAG=...` in `OPENEXR_CMAKE_FLAGS`). Imath main reports
  its version as 3.2.0. Jobs with no `openexr_ver` (ASWF containers) still
  use the container's Imath. Added 2026-09-30; confirm on the next audit
  that summaries show the expected Imath. A separate `IMATH_VERSION`
  selector was tried and dropped as unnecessary for now.
- OIIO's own OpenEXR auto-build (`build_OpenEXR.cmake`) passes
  `-D Imath_DIR=...` so OpenEXR uses the same Imath as OIIO.
- OpenJPH is always the `build_openjph.cmake` default; no job tests an
  older OpenJPH.
- Minimums not tested by any job: libjpeg 9, libjpeg-turbo 2.1, zlib
  1.2.7 to 1.2.10, OpenJPEG below 2.4, OpenVDB 9, TBB 2018 to 2019,
  giflib 5.0, libheif below 1.19, libjxl below 0.11.1, DCMTK below 3.7,
  Qt5 below 5.15, ffmpeg below 4.4, clang 10, MSVS 2017/2019.
- ffmpeg: INSTALL.md documents a maximum newer than any CI job tests.
  Left as is unless the user asks to lower it (SKILL.md step 7).
