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


# texture.h

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

# varyinref.h

* This header has been removed completely, since we no longer use the classes
  it defines.


