<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

OpenImageIO 3.0 Deprecations
============================

For minor or patch releases, we try very hard to never fully remove
functionality that will force downstream applications using OpenImageIO to
change their source code. However, for major releases (e.g., 2.x -> 3.0),
which only occur once every several years, we allow removal of functionality.

OpenImageIO v3.0 is a major release that includes removal of many
long-deprecated API facets. This document lists the deprecations and removals.

NOTE: This is an in-progress document. Some things currently only warning
about being deprecated will be removed in the final 3.0 release.

### Glossary

- "Marked as deprecated" means that we consider an API facet to be obsolete,
  and document it as such in the comments or documentation (or remove it from
  the documentation).

- "Deprecation warning" means that a deprecated function is tagged with
  attributes that should cause a compiler warning if the function is used. The
  warning can be disabled by the downstream project, but it is recommended
  that you fix the code to use the new API before it is permanently removed.

- "Removed" means that the deprecated API facet has been removed from the
  library completely.


---

---

## argparse.h

* Several long-deprecated old method names now will give deprecation warnings
  if used. Most notable are `parse()` (you should use `parse_args()` instead)
  and `usage()` (use `print_help()` instead).

## array_view.h

* This header has been eliminated. It originally had the template `array_view`,
  which many years ago was renamed `span<>` and lives in span.h, and since then
  the array_view.h header has merely made an alias.

## bit.h

* The `bit_cast` template, which was deprecated and warned since 2.5, has been
  removed. It was confusing because C++23 has a `std::bit_cast` that used the
  reverse order of arguments. Users should instead use `std::bit_cast` (if
  C++23) or the equivalent `OIIO::bitcast`.
* The `rotl32` and `rotl64` functions which have been marked as deprecated
  since 2.1 now have deprecation warnings. Use `rotl` instead.

## benchmark.h

* The full `time_trial()` function now has deprecation warnings. Users should
  generally use the `Benchmrker` class instead.
* The abbreviated `time_trial()` function (that lacks a `repeats` parameter
  has been removed.

## color.h

* `ColorConfig::error()` now has a deprecation warning. Use
  `ColorConfig::has_error()` instead.
* The versions of `createDisplayTransform()` that lack an `inverse` parameter
  now have deprecation warnings. Use the version that takes an `inverse` bool.

## dassert.h

* Poorly named `ASSERT`, `DASSERT`, and `ASSERTMSG` macros have been removed.
  They were deprecated since 2.1, since they could easily clash with macros
  from other projects. Please instead use the `OIIO_ASSERT`, `OIIO_DASSERT`,
  and `OIIO_ASSERT_MSG` macros.

## errorhandler.h

* All of the old methods that did printf-style formatting have been deprecated
  (info/infof, warning/warningf, error/errorf, severe/severef,
  message/messagef, debug/debugf). Instead, use infofmt, warningfmt, errorfmt,
  severefmt, messagefmt, debugfmt, respectively, which all use the std::format
  notation.

## imagebuf.h

* Add deprecation warnings to the varieties of ImageBuf constructor and
  `reset()` that don't take subimage and miplevel parameters, which have been
  marked as deprecated since OIIO 2.2. The equivalent is to just pass `0` for
  both of those parameters.
* The misspelled `ImageBuf::make_writeable()` has been given deprecation
  warnings. Since OIIO 2.2, we have used the correct spelling,
  `make_writable`.
* The `ImageBuf::error()` method that uses printf-style formatting conventions
  now has deprecation warnings. Use `ImageBuf::errorfmt()` instead.
* The `ImageBuf::interppixel_NDC_full()` method, which has been marked as
  deprecated since OIIO 1.5, now has deprecation warnings. Use
  `interppixel_NDC()` instead.

## imageio.h

* The global OIIO::attribute query "opencv_version" has been removed. The
  libOpenImageIO library itself no longer has OpenCV as a dependency or links
  against it. (However, the IBA functions involving OpenCV still exist and are
  defined in `imagebufalgo_opencv.h` as inline functions, so it is up to the
  application calling these API functions to find and link against OpenCV.)
* The old varieties of ImageInput::read_scanlines, read_tiles, and read_image
  that did not take `subimage` and `miplevel` parameters, and were not
  thread-safe, have been removed. These have been marked as deprecated since
  OIIO 2.0.
* The type aliases ImageIOParameter and ImageIOParameterList, which have been
  marked as deprecated since OIIO 2.0, have been removed. Use ParamValue and
  ParamValueList instead.
* The utility functions convert_image and parallel_convert_image (the variety
  that took alpha_channel and z_channel arguments) that have been deprecated
  since OIIO 2.0 have been removed.

## imagebuf.h

* The style of ImageBuf constructor that "wraps" a caller-owned memory buffer
  now has a new, preferred, version that takes a `span<>` or `cspan<>` instead
  of a raw pointer. The old versions is considered deprecated.
* New span-based versions of get_pixels, set_pixels, setpixel, getpixel,
  interppixel, interppixel_NDC, interppixel_bicubic, interppixel_bicubic_NDC.
  These are preferred over the old versions that took raw pointers.

## imagebufalgo.h

* The old versions (deprecated since 2.0) of IBA::compare() and
  computePixelStats() that took a reference to the CompareResults or
  PixelStats structure now have deprecation warnings. Use the versions that
  return the structure instead.
* The deprecated (often since as far back as 2.0) versions of functions that
  took raw pointers to color values now have deprecations warnings. Use the
  versions that take `span<>` or `cspan<>` instead. These include versions of:
  isConstantColor, isConstantChannel, isMonochrome, isConstantChannel,
  colorconvert, fill, checker, add, sub, absdiff, mul, div, mad, pow,
  channel_sum, channels, clamp, color_count, color_range_check, render_text.
* The `histogram()` function that takes a reference to a result vector
  (deprecated since 2.0 and previously warned) has been removed. Use the
  version that returns the vector instead. The `histogram_draw()` (deprecated
  since 2.0 and previously warned) has been removed and has no replacement
  since it was always silly.
* The versions of ociodisplay that lacked an `inverse` parameter, which were
  marked as deprecated since 2.5, now have deprecation warnings. Use the
  version that takes an `inverse` bool.
* The OpenCV-related functions that take old-style IplImage pointers
  (deprecated since 2.0) have been removed. Use the modern ones that use
  cv::Mat.
* The pre-KWArgs versions of resize, warp, and fit now have deprecation
  warnings. Use the versions that take KWArgs instead.
* The OpenCV-related functions `to_OpenCV()`, `from_OpenCV()`, and
  `capture_image()` have moved to the `imagebufalgo_opencv.h` header.

## imagebufalgo_util.h

* IBA::type_merge, deprecated since 2.3, now has a deprecation warning.
  Instead, use TypeDesc::basetype_merge().

## imagecache.h

* `ImageCache::create()` now returns a `std::shared_ptr<ImageCache>` instead
  of a raw pointer.

## missingmath.h

* This header has been removed entirely. It has was originally needed for
  pre-C++11 MSVS, but has been unused since OIIO 2.0 (other than transitively
  including fmath.h).

## paramlist.h

* Removed some ParamValue constructor variants that have been deprecated since
  OIIO 2.4. The removed variants took an optional `bool` parameter indicating
  whether the value was copied. If you need to override the usual copy
  behavior, please use the newer variety of constructors that instead use a
  "strong" type where you pass `ParamValue:Copy(bool)`.

## parallel.h

* Removed several varieties of `parallel_for` functions where the task
  functions took a thread ID argument in addition to the range, which have
  been considered deprecated since OIIO 2.3. Please use task functions that do
  not take a thread ID parameter.

## platform.h

* Removed macros `OIIO_CONSTEXPR`, `OIIO_CONSTEXPR14`, and
  `OIIO_CONSTEXPR_OR_CONST` and deprecated `OIIO_CONSTEXPR17` (use regular C++
  `constexpr` in place of all of these). Removed macro `OIIO_NOEXCEPT` (use
  C++ `noexcept`).
* Removed macro `OIIO_UNUSED_OK`, which had been deprecated since 2.0. Marked
  `OIIO_MAYBE_UNUSED` as deprecated as well, now that C++17 is the minimum,
  there's no reason not to directly use the C++ attribute `[[maybe_unused]]`.

## simd.h

* The old (OIIO 1.x) type names float3, float4, float8, int4, int8, mask4,
  bool4, bool8 have been removed. Use the new vbool4, vint4, vfloat4, etc.
* The old rotl32 functions have been removed. They had been deprecated since
  OIIO 2.1. Please use `rotl()` intead.
* The old floori functions have been removed. They had been deprecated since
  OIIO 1.8. Please use ifloor() instead.
* The old OIIO_SIMD_HAS_FLOAT8 macro has been removed. It was deprecated since
  OIIO 1.8.

## string_view.h

* The string_view::c_str() method has been marked as deprecated, since
  it is not present in C++17 std::string_view. If you must use this
  functionality (with caution about when it is safe), then use the
  freestanding OIIO::c_str(string_view) function instead.

## strutil.h

* The default behavior of `Strutil::format()` has been changed to use the
  `std::format` conventions. If you want the old behavior, use
  `Strutil::old::format()` instead.
* Added deprecation warnings to all the old (printf-convention) string
  `format()` function.

## texture.h

* `TextureSystem::create()` now returns a `std::shared_ptr<TextureSystem>`
  instead of a raw pointer.
* Removed stochastic-related tokens from the MipMode and InterpMode enums.
  These were originally experimental but never removed.
* Removed the `bias` field from the TextureOpt structure. This was originally
  there for the sake of shadow maps, but was never used because we never
  implemented shadow maps in OIIO's TextureSystem.
* Removed the TextureOptions, which hasn't been used since OIIO 1.x. We
  switched to the alternate TextureOpt structure in OIIO 2.0.
* Fully removed the long-deprecated methods of TextureSystem that operated
  on batches using the VaryingRef class. These were replaced by alternatives
  a long time ago.

## thread.h

* `OIIO::yield()` now has a deprecation warning, having been marked as
  deprecatd since OIIO 2.4. Use `std::this_thread::yield()`.

## tiffutils.h

* Removed the version of decode_exif that takes a pointer and length
  (deprecated since 1.8). Use the version that takes a `string_view`, or the
  one that takes a `cspan<uint8_t>`.

## type_traits.h

* Removed the `OIIO::void_t` template, which now should be replaced with
  `std::void_t`.

## ustring.h

* Removed old `ustringHash` (which was just an alias for `std::hash<ustring>`,
  which should be used instead).

## varyingref.h

* This header has been removed completely, since we no longer use the classes
  it defines.


## python bindings

* The `ImageBufAlgo.capture_image()` function has been removed from the
  Python bindings. Python scripts that wish to capture images from live
  cameras should use OpenCV or other capture APIs of choice and then
  pass the results to OIIO to construct an ImageBuf.
* Static type names within the `TypeDesc` class (such as `TypeDesc.TypeFloat`)
  have been removed after being considered deprecated since OIIO 1.8. Use the
  names in the overall OpenImageIO namespace instead.
* Older versions of `ImageBufAlgo.fit()` have been removed. Use the newer
  function signatures.
* Older versions of `ImageInput::read_native_deep_scanlines()` and
  `read_native_deep_tiles()` have been removed. They had been deprecated
  since OIIO 2.0.

## maketx

* The `--noresize` option has been removed. It was deprecated since OIIO 2.0,
  when it became the default behavior.
* The `--stats` option has been removed. It was deprecated since OIIO 1.6,
  when it was renamed `--runstats`.
