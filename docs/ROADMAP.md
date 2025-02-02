<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# OpenImageIO Roadmap

This describes the major tasks we hope to accomplish on the road to
OpenImageIO 3.0, which should be released circa September 2024. Where there
are links to issue, please read the issue for more extensive description and
discussion about the modifications required.

OpenImageIO is fairly mature and one could argue that its being used
extensively in production for 15 years puts an empirical limit on the
criticality of any "missing" feature. So while we welcome new features, those
that don't break old APIs can be added in any release, so there are no "must
have features" that are required to be completed for this release.

However, 2.x -> 3.0 is one of the rare opportunities for things that might
break backward compatibility, i.e., that might require actual source code
changes to software that calls OIIO. Since this happens only every 5 years or
so, this means that for many such changes (including removing previously
deprecated API calls), it is a matter of doing in now or waiting several
years.



## Dependency modernization ([project](https://github.com/orgs/AcademySoftwareFoundation/projects/28))

We would like to pull forward a whole lot of other dependencies so that their
minimums are somewhere in the "released 3-5 years ago" range. The full list of
our dependencies [can be found
here](https://github.com/AcademySoftwareFoundation/OpenImageIO/discussions/4151).

See the [Dependency proposal wiki page](https://github.com/AcademySoftwareFoundation/OpenImageIO/discussions/4151)

* [x] Big required upgrades with potentially widespread impact on the code base
  - [x] C++17 [#4155](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4155)
  - [x] Python 3.7 [#4157](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4157)
  - [x] OpenEXR/Imath 3.1 [#4156](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4156)

* [x] Miscellaneous optional upgrades whose changes will be very localized

  This isn't a complete list, nor do we need to do all of these, but the
  highest priorities are:

  - [X] CMake 3.18 (from 3.15). [#4472](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4472)
  - [x] OpenColorIO 2.2, and make it required. [#4367](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4367)
  - [ ] fmt 8.0 (from 7.0), which has many improvements. CANCELLED: fmt 8.0+
        does not work with the old icc compiler.
  - [x] GIFlib 5.0 (from 4.0), which adds thread safety. [#4349](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4349)
  - [x] libheif 1.11 (from 1.3), which supports many additional features of that format. [#4380](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4380)
  - [x] WebP 1.2 (from 0.6) which lets us use their exported CMake configs and retire FindWebP.cmake. [#4354](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4354)
  - [x] pybind11 2.6 or 2.7 (from 2.4). [#4297](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4297)


- [x] [#4158](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4158) Eliminate the last few places where we use Boost and eliminate it as a dependency.

<br>


## API Modernization

- [ ] [#4159](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4159)  Using span and string_view instead of raw pointers

  Scrub the APIs to find where we have API calls that take a pointer and size
  (or worse -- just a pointer and an assumption about the size), and instead
  use `span<T>` / `cspan<T>` for things that are like arrays, and
  `string_view` for things that are like strings.

- [ ] [#4160](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4160)
  Using named keyword/value lists more extensively in imagebufalgo.h

- [ ] Deprecate as much as possible of the old printf-style string formatting we use internally, instead using the fmt/std::format style everywhere. This includes changing Strutil::format to alias to Strutil::fmt::format (currently it aliases to Strutil::old::format, which uses printf style).

- [ ] Remove deprecated API elements

  Hunt for things marked DEPRECATED and try to get rid of them where possible.
  If we can't remove them, at least make sure they are marked as
  `OIIO_DEPRECATED` (or `[[deprecated]]` in modern C++).


## The rest

Other initiatives we hope to have completed by the time of this next
major release:

- [ ] [#4164](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4164) Better color management

  Regardless of OCIO availability, version, or contents of any configs, have universal support for the [canonical color spaces](https://github.com/AcademySoftwareFoundation/MaterialX/blob/main/documents/Specification/MaterialX.Specification.md#color-spaces-and-color-management-systems) that seem to be the common consensus of ACES, MaterialX, and USD.
- [ ] Preliminary Rust bindings for most of the OpenImageIO APIs.
- [x] Python wheel construction so `pip install openimageio` will be an easy way
      for users to install the whole banana. [#4428](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/4428)

If there is something you think should be on the roadmap for the next major
release but is not, please open an issue or discussion to propose it, or
bring it up at a TSC meeting, or on the mail list or Slack channel.


## Parking

Here is where we will put things that definitely should be on the roadmap, but
that need not be completed in time for the fall 2024 release of OIIO 3.0.

...
