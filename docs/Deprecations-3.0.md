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

## strutil.h

* Added deprecation warnings to all the old (printf-convention) string
  `format()` function.

## texture.h

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
  which should be used instead.

## varyingref.h

* This header has been removed completely, since we no longer use the classes
  it defines.


