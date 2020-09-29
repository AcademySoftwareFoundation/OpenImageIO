Release 2.2.7 (1 Oct 2020) -- compared to 2.2.6
-------------------------------------------------
* oiiotool new command: `--pastemeta` takes two images as arguments, and
  appends all the metadata (only) from the first image onto the second
  image's pixels and metadata, producing a combined image. #2708
* TIFF: Fix broken reads of multi-subimage non-spectral files (such as
  photometric YCbCr mode). #2692
* Python: When transferring blocks of pixels (e.g., `ImageInput.read_image()`
  or `ImageOutput.write_scanline()`), "half" pixels ended up mangled into
  uint16, but now they use the correct numpy.float16 type. #2694
* Python: The value passed to `attribute(name, typedesc, value)` can now be
  a tuple, list, numpy array, or scalar value. #2695
* `IBA::contrast_remap()` fixes bug that could crash for very large images
  #2704
* Warn about recommended minimum versions of some dependencies.
* Windows fix: correct OIIO_API declaration on aligned_malloc, aligned_free
  of platform.h. #2701
* Fix oiiotool crash when --resize was used with multi-subimage files. #2711
* Bug fix in Strutil::splits and splitsv: when input is the empty string,
  the split should return no pieces. #2712
* Support for libheif 1.9. #2724
* TIFF: Fix spec() and spec_dimensions() for MIPmapped TIFF files, they
  did not recognize being asked to return the specs for invalid subimage
  indices. #2723
* TIFF: add ability to output 1bpp TIFF files. #2722


Release 2.2 (1 Sept 2020) -- compared to 2.1
----------------------------------------------
New minimum dependencies:
* pybind11 >= 2.4.2
* openjpeg >= 2.0 (if JPEG-2000 support is desired) #2555 (2.2.2)

New major features and public API changes:
* Improved IOProxy support:
    - ImageInput and ImageOutput now have direct API level support for IOProxy
      in their `open()` and `create()` calls, as well as a new `set_ioproxy()`
      method in these classes. #2434 (2.2.0)
    - ImageBuf can now specify a proxy upon construction for reading, and for
      writing via a `set_write_ioproxy()` method that applies to subsequent
      `write` call.  #2477 (2.2.1).
    - DPX input now supports IOProxy. #2659 #2665 (2.2.5)
    - ImageCache (and ImageBuf backed by ImageCache) entries that use IOProxy
      are careful not to fully "close" their proxies when trying to reclaim
      space in the file cache (that would be bad, since the proxy can't be
      re-opened). #2666 (2.2.5)
* Improved support for multi-subimage files:
    - oiiotool: Nearly all operations now allow an optional `:subimages=...`
      modifier that restricts the operation to be performed on only a subset
      of named or indexed subimages. See docs for details. #2582
    - Python `ImageBuf.write()` variety added that takes an open
      `ImageOutput`.  This is the key to writing a multi-subimage file (such
      as a multi-part OpenEXR) using the Python ImageBuf interface. #2640
      (2.2.4)
    - Fixes to `--croptofull` and `-o` with multi-subimages. #2684 (2.2.6)
* Python bindings:
    - Python bindings have been added for missing ParamValue constructors.
      We previously exposed the PV constructors from just a plain int, float,
      or string, but there wasn't an easy way to construct from arbitrary
      data like there is in C++. Now there is. #2417 (2.2.0)
    - `ParamValueList.attribute()`, when being passed attributes containing
      multiple values, now can have those values passed as Python lists and
      numpy arrays (previously they had to be tuples). #2437 (2.1.11/2.2.0)
    - `ImageBufAlgo.color_range_check()` is now available to the Python
      bindings (was previously only C++). #2602 (2.2.3)
    - New variety of `ImageBuf.write()` that takes an open `ImageOutput`.
      This is the key to writing a multi-subimage file (such as a multi-part
      OpenEXR) using the Python ImageBuf interface. #2640 (2.2.4)
* ImageBuf:
    - Easier direct use of IOProxy with ImageBuf: constructor and reset()
      for file-reading ImageBuf now take an optional `IProxy*` parameter,
      and a new `set_write_ioproxy()` method can supply an IOProxy for
      subsequent `write()`. #2477 (2.2.1)
    - Add `ImageBuf::setpixel()` methods that use cspan instead of ptr/len.
      #2443 (2.1.10/2.2.0)
    - Add "missing" `reset()` varieties so that every IB constructor has a
      corresponding `reset()` with the same parameters and vice versa. #2460
* ImageBufAlgo:
    - New `repremult()` is like premult, but will not premult when alpha is
      zero. #2447 (2.2.0)
    - New `max()` and `min()` functions take the pixel-by-pixel maximum
      or minimum of two images. #2470 (2.2.1)
* ColorConfig: add OCIO "role" accessors. #2548
* Low-res I/O of images to terminals that support full color and Unicode
  characters. Just output to a file ending in ".term", and it will convert
  (on certain terminals) to an image displayed directly in the terminal.
  #2631 (2.2.4)
  Try:
      `oiiotool myfile.exr -o out.term`

Performance improvements:
* Greatly improved TextureSystem/ImageCache performance in highly threaded
  situations where access to the cache was a main bottlenecks. In renders of
  scenes with lots of texture access, with dozens of threads all contending
  for the cache, we are seeing some cases of 30-40% reduction in total
  render time. In scenes that are less texture-bottlenecked, or that don't
  use huge numbers of threads, the improvement is more modest. #2433 (2.2.0)

Fixes and feature enhancements:
* oiiotool:
    - Intelligible error messages (rather than crashes) if you attempt to
      create an image too big to fit in memory. #2414 (2.2.0)
    - `--create` and `--proxy` take an additional optional modifier:
      `:type=name` that specifies the type of buffer to be created (the
      default, as usual, is to create an internal float-based buffer). #2414
      (2.2.0)
    - `-o` optional argument `:type=name` is a new (and preferred) synonym
      for what used to be `:datatype=`. #2414 (2.2.0)
    - `--autotrim` now correctly trims to the union of the nonzero regions
      of all subimages, instead of incorrectly trimming all subimages to the
      nonzero region of the first subimage. #2497 (2.2.1.2)
    - `--subimage` now has an optional `:delete=1` modifier that causes the
      operation to delete one named or indexed subimage (versus the default
      behavior of extracing one subimage and deleting the others). #2575
      (2.2.3)
    - The list of dependent libraries (part of `oiiotool --help`) now
      correctly reports the OpenEXR version. #2604 (2.2.3)
    - Fix: `--eraseattrib` did not correctly apply to all subimages when
      `-a` or `:allsubimages=1` were used. #2632 (2.2.4)
* ImageBuf / ImageBufAlgo:
    - Huge ImageBuf allocation failures (more than available RAM) now are
      caught and treated as an ImageBuf error, rather than crashing with an
      uncaught exception. #2414 (2.2.0)
    - ImageBuf constructors that are passed an ImageSpec (for creating an
      allocated writable IB or "wrapping" a user buffer) now check that the
      spec passed has enough information to know the size of the buffer
      (i.e., it will be recognized as an error if the width, height, depth,
      channels, or data type have not been set validly). #2460
    - Fix: `ImageBuf::getchannel()` did not honor its `wrap` parameter.
      #2465 (2.2.1/2.1.12)
    - Fix: `IBA::reorient()` and `IBA::computePixelHashSHA1()` did not honor
      their `nthreads` parameter. #2465 (2.2.1/2.1.12)
    - `resample()` has been modified to more closely match `resize` by using
      clamp wrap mode to avoid a black fade at the outer edge of the
      resampled area. #2481
    - Fix: `ImageBuf::get_pixels()` did not honor the stride parameters.
      #2487. (2.1.12/2.2.1)
    - Fix `resize()` to avoid a crash / stack overflow in certain cases of
      very big images and very large filter kernels. #2643 (2.2.4)
    - Minor improvements to ImageBuf error formatting. #2653 (2.2.5)
* ImageCache / TextureSystem / maketx:
    - New IC/TS attribute "trust_file_extensions", if nonzero, is a promise
      that all files can be counted on for their formats to match their
      extensions, which eliminates some redundant opens and format checks
      in the IC/TS and can reduce needless network/filesystem work. Use with
      caution! #2421 (2.2.0)
    - texture3d() fixed some cases where derivative results were not
      correctly copied to the outputs. #2475 (2.2.1)
    - `maketx`/`IBA::make_texture`: better error detection and messages when
      using "overscan" textures with formats that can't support it properly.
      (Punchline: only OpenEXR textures can do it.) #2521 (2.2.0)
    - Fix possible redundant tile reads in multithread situations (harmless,
      but makes for redundant I/O). #2557 (2.2.2)
* Python:
    - Fixed a bug that lost certain string arguments, especially when passing
      a TypeDesc as its string equivalent. #2587 (2.1.16/2.2.3)
    - Fixed broken bindings of ImageSpec.erase_attribute. #2654
      (2.1.19/2.2.6)
    - Fix missing ImageInput.read_image(). #2677 (2.1.19/2.2.6)
* Exif read: guard better against out of range offsets, fixes crashes when
  reading jpeg files with malformed exif blocks. #2429 (2.1.10/2.2.0)
* Fix: `ImageSpec::erase_attribute()` did not honor its `searchtype`
  parameter. #2465 (2.2.1/2.1.12)
* Fix: Some ColorProcessor::apply() methods were not using their `chanstride`
  parameters correctly. #2475 (2.1.12)
* Fix: iinfo return code now properly indicates failures for files that
  can't be opened. #2511 (2.2.2/2.1.13)
* DPX:
    - IOProxy reading is now supported. #2659 (2.2.5)
    - DPX: Add support for reading DPX files from IOProxy (such as from a
      memory buffer). #2659 #2665 (2.1.19/2.2.6)
* HDR files:
    - Improve performance when reading HDR files out of order (when combined
      with ImageCache, observed to speed up out-of-order HDR reading by 18x).
      #2662 (2.2.5)
* JPEG:
    - Fix resolution unit metadata that was not properly set in JPEG output.
      #2516 (2.2.2/2.1.13)
    - Fix loss of 'config' info upon close/reopen. #2549 (2.2.2)
* OpenEXR:
    - Add support for reading and writing float vector metadata. #2459 #2486
    - Fix bug in the channel sorting order when channels are "X" and
      "Y" (was reversing the order by confusing "Y" for "luminance"). #2595
      (2.1.16/2.2.3)
    - We no longer automatically rename the "worldToNDC" attribute to
      "worldtoscreen" and vice versa. #2609 (2.2.4)
* PNG:
    - Fix loss of 'config' info upon close/reopen. #2549 (2.2.2)
    - Add output configuration hint "png:filter" to control PNG filter
      options. #2650 (2.2.5)
    - Improved propagation of PNG write errors. #2655 (2.2.5)
    - Tell libpng to turn off sRGB profile check, which has a known problem of
      false positives. #2655 (2.2.5)
    - New output option "png:filter" allows control of the PNG filter
      options. #2650 (2.1.19/2.2.6)
* Raw images:
    - Support for new Canon .cr3 file, but only if you build against
      libraw >= 0.20.0 developer snapshot. #2484 (2.2.1) #2613 (2.2.4)
    - RAW input: set the "raw:flip" attribute if the underlying libraw did a
      reorientation. #2572 (2.1.15/2.2.3)
    - Avoid errors (in libraw) that resulted from multiple threads opening
      raw files at the same time. #2633 (2.2.4)
* RLA:
    - Additional sanity checks and error checks/messages for detecting files
      that might be first mistaken for RLA files, but actually are not.
      #2600 (2.2.3)
* TIFF:
    - Internal improvements to handling metadata retrieval for certain
      unusual tags. #2504 (2.2.2/2.1.13)
    - Fix subtle bug when reading Exif directory in the header. #2540
      (2.2.2)
* Video files:
    - Fix possible infinite loop in the FFMpeg-based reader. #2576
      (2.1.15/2.2.3)

Developer goodies / internals:
* argparse.h:
    - Complete overhaul of ArgParse to make it more like Python argparse.
      Please read the extensive comments in argparse.h for documentation.
      For now, the old ArgParse interface still works, but is considered
      deprecated. #2531 (2.2.2) #2618 #2622 (2.2.4)
* attrdelegate.h:
    - New `as_vec<>` method returns the whole attribute as a std::vector.
      #2528 (2.2.2)
* filesystem.h:
    - Catch previously uncaught exceptions that could happen in certain
      Filesystem utility calls. #2522 (2.2.2/2.1.13)
    - New `write_text_file()` convenience function for opening, writing, and
      closing a text file all in one step. #2597 (2.2.3)
* fmath.h:
    - clamp() is 2x faster. #2491 (2.1.12/2.2.2)
    - Very minor fix to OIIO::clamp(), shouldn't affect normal use with
      floats at all, but fixed a subtle quasi-bug in OSL. #2594 (2.1.15/2.2.3)
    - madd() is improved especially on platforms without fma hardware
      #2492 (2.1.12/2.2.2)
    - Perf improvements to `fast_sin`, `fast_cos` #2495 (2.1.12/2.2.2)
    - New `safe_fmod()` is faster than std::fmod. #2495 (2.1.12/2.2.2)
    - New `fast_neg` is faster than simple negation in many cases, if you
      don't care that -(0.0) is 0.0 (rather than a true -0.0). #2495
      (2.1.12/2.2.2)
    - Add vint4, vint8, and vint16 versions of `clamp()`. #2617 (2.2.4)
* oiioversion.h:
    - Fix typo that left the OIIO_VERSION_RELEASE_TYPE symbol undefined.
      #2616 (2.2.4/2.1.16)
    - Add new `OIIO_MAKE_VERSION(maj,min,patch)` macro that constructs the
      proper single integer code for a release version. #2641 (2.2.4/2.1.17)
* paramlist.h:
    - New `ParamValueList::find_pv()` method that is similar to `find()` but
      returns a pointer rather than an iterator and nullptr if the attribute
      is not found. #2527 (2.2.2/2.1.13)
    - Add `get_indexed()` method to ParamValueList and AttrDelegate. #2526
     (2.2.2/2.1.13)
* platform.h:
    - `OIIO_PRETTY_FUNCTION` definition is more robust for weird compilers
      (will fall back to `__FUNCTION__` if all else fails). #2413 (2.2.0)
    - `OIIO_ALIGN` definition is more robust, will fall back to C++11
      alignas when not a compiler with special declspecs (instead of being
      a compile time error!). #2412 (2.2.0)
    - A variety of `OIIO_PRAGMA_...` macros have been added to help deal
      with compiler-specific pragmas. #2467 (2.2.1)
* simd.h:
    - vfloat3 has added a `normalize()`, `length()`, and `length2()`
      methods, to more closely match the syntax of Imath::Vec3f. #2437
      (2.1.11/2.2.0)
    - fix errors in vbool == and !=. #2463 (2.1.11/2.2.1)
    - Add float3 versions of abs, sign, ceil, floor, and round (they already
      existed for float4, float8, float16, but not float4). #2612 (2.2.4)
    - Improved support for ARM NEON SIMD (caveat: this is still not well
      tested). #2614 (2.2.4)
    - Improve performance for many float8/int8 functions and operators when
      running on only 4-wide hardware, by using two 4-wide instructions
      instead of reverting to scalar. #2621
* span.h:
    - Allow the constructor from `std::vector` to allow vectors with custom
      allocators. #2533 (2.2.2)
* strutil.h / ustring.h:
    - New `Strutil::concat()` and `ustring::concat()` concatenate two
      strings, more efficiently than `sprintf("%s%s")` by avoiding any
      unnecessary copies or temporary heap allocations. #2478 (2.2.1)
    - Strutil::upper() and lower() return all-upper and all-lowercase
      versions of a string (like `to_lower` and `to_upper`, but not in-place
      modifications of the existing string). #2525 (2.2.2/2.1.13)
    - `Strutil::repeat()` has been internally rewritten to more efficient by
      avoiding any unnecessary copies or temporary heap allocations. #2478
      (2.2.1)
* typedesc.h:
    - TypeDesc has additional helpers of constexpr values TypeFloat2,
      TypeVector2, TypeVector4, TypeVector2i, TypePointer. #2592 (2.1.16/2.2.3)
* unordered_map_concurrent.h:
    - Fix missing decrement of `size()` after `erase()`. #2624 (2.2.4)
* More reshuffling of printf-style vs fmt-style string formatting. #2424
  (2.2.0) #2649 (2.2.4)
* Internals: changed a lot of assertions to only happen in debug build mode,
  and changed a lot that happen in release builds to only print the error
  but not force a termination. #2435 (2.1.11/2.2.0)
* Internals: Replaced most uses of `boost::thread_specific_ptr` with C++11
  `thread_local`. #2431 (2.2.0)
* oiiotool: Big overhaul and simplification of internals. #2586 #2589 (2.2.3)

Build/test system improvements and platform ports:
* CMake build system and scripts:
    - New non-default CMake build flag `EXTRA_WARNINGS`, when turned on, will
      cause gcc and clang to compile with -Wextra. This identified many new
      warnings (mostly about unused parameters) and fixes were applied in
      #2464, #2465, #2471, #2475, #2476. (2.2.1)
    - FindOpenColorIO.cmake now correctly discerns the OCIO version (2.2.1),
      and now sets up a true imported target. #2529 (2.2.2)
    - FindOpenEXR.cmake has better detection of debug openexr libraries.
      #2505 (2.2.2/2.1.13)
    - Additional cmake controls to customize required vs optional
      dependencies: `REQUIRED_DEPS` (list of dependencies normally optional
      that should be treated as required) and `OPTIONAL_DEPS` (list of
      dependencies normally required that should be optional). The main use
      case is to force certain optional deps to be required for your studio,
      to be sure that missing deps are a full build break, and not a
      successful build that silently lacks features you need. #2507
      (2.2.2/2.1.13)
    - Fix exported cmake config file, it was not ensuring that the Imath
      headers properly ended up in the config include path. #2515
      (2.2.2/2.1.13)
    - Change all CMake references to PACKAGE_FOUND to Package_Found (or
      whatever capitalization matches the actual package name). #2569 (2.2.2)
    - The exported CMake config files now set cmake variable
      `OpenImageIO_PLUGIN_SEARCH_PATH` #2584 (2.1.16/2.2.3)
    - Improved hints printed about missing dependencies. #2682 (2.2.6)
* Dependency version support:
    - Pybind11 is no longer auto-downloaded. It is assumed to be
      pre-installed. A script `src/build-scripts/build_pybind11.bash` is
      provided for convenience if you lack a system install. #2503 (2.2.2)
      Bump the minimum pybind11 version that we accept, to 2.4.2 #2453,
      and add fixes to allow support of pybind11 2.5. #2637 (2.2.4)
    - fmt libray: Un-embed fmt headers. If they are not found on the system
      at build time, they will be auto-downloaded. #2439 (2.2.0)
    - Support for building against libraw 0.20. #2484 (2.2.1) #2580 (2.2.3)
    - Build properly against OpenColorIO's current master (which is the
      in-progress work on OCIO v2). #2530 (2.2.2)
    - Fix static boost to not overlink on Windows. #2537 (2.2.2)
    - Fix build breaks against TOT libtiff master, which had `#define`
      clashes with our GPSTag enum values. #2539 (2.2.2)
    - Ensure compatibility and clean builds with clang 10. #2518 (2.2.2/2.1.3)
    - Support verified for gcc 10, added to CI tests. #2590 (2.2.3)
    - Support for Qt 5.15. #2605 (2.2.3)
    - Fixes to support OpenColorIO 2.0. #2636 (2.2.4)
    - Build against more recent versions of fmtlib. #2639 (2.2.4)
    - Included scripts to download and build libtiff #2543 (2.1.13/2.2.2),
      PugiXML #2648 (2.2.4), zlib, libpng, libjpeg-turbo. #2663 (2.2.5)
    - Minor fixes for libheif 1.8. #2685 (2.2.6)
    - Add a build_libtiff.bash script to make it easy to build the libtiff
      dependency. #2543 (2.1.13/2.2.2)
    - "tinyformat" is no longer used, even optionally. We have switched
      entirely to fmtlib, which is more similar to the upcoming C++20
      std::format. #2647 (2.2.4)
* Testing and Continuous integration (CI) systems:
    - Mostly retire TravisCI for ordinary Linux x64 and Mac builds, now we
      rely on GitHub Actions CI. Nightly test added. Use ASWF docker images
      to test exactly against VFX Platform 2019 and 2020 configurations.
      #2563 (2.2.2) #2579 (2.2.3)
    - Add Travis test for arm64 (aka aarch64) builds. This is still a work
      in progress, and not all testsuite tests pass. #2634 (2.2.4)
    - Our CI tests now have a "bleeding edge" matrix entry that tests against
      the current TOT master build of libtiff, openexr (#2549), and pybind11
      (#2556). (2.2.2)
    - GitHub CI tests, when they fail, leave behind an "artifact" tar file
      containing the output of the tests, so that they can be easily
      downloaded and inspected (or used to create new reference output).
      #2606 (2.2.4)
    - CI Mac tests switch to Python 3.8. (2.2.4)
    - Windows CI switched from using Vcpkg to building its own dependencies.
      #2663 (2.2.5)
    - Testing of TGA now assumes the test images are in the oiio-images
      project, not separately downloaded (the download location disappeared
      from the net). #2512 (2.2.2)
    - Beef up OpenEXR compliance tests, many more examples from
      openexr-images, including many corrupted image failure cases. #2607
      (2.2.4)
* Progress on support for using Conan for dependency installation. This is
  experimental, it can't yet build all dependencies. Work in progress.
  #2461 (2.2.1)
* The version of gif.h that we embed for GIF output has been updated.
  #2466 (2.2.1)
* The `farmhash` functions have been cleaned up to be more careful that none
  of their internal symbols are left visible to the linker. #2473 (2.2.1)
* Clarification about .so name versioning: In supported releases, .so
  contains major.minor, but in master (where ABI is not guaranteed stable,
  we name major.minor.patch). #2488 (2.2.1)
* Protect against certain compiler preprocessor errors for user programs
  that include strutil.h but also include `fmt` on its own. #2498.
  (2.1.12/2.2.2)
* Build: All the `build_foo.bash` helper scripts now use `set -ex` to ensure
  that if any individual commands in the script fails, the whole thing will
  exit with a failure. #2520 (2.2.2/2.1.3)
* Fix compiler warning about incorrect extra braces. #2554 (2.2.2)
* All build-scripts bash scripts now use /usr/bin/env to find bash. #2558
  (2.2.2)
* Avoid possible link errors by fully hiding IBA functions taking IplImage
  parameters, when no OpenCV headers are encountered. #2568 (2.2.2)
* In (obsolete) FindOpenImageIO.cmake, avoid CMake warnings by changing
  the name `OPENIMAGEIO_FOUND` -> `OpenImageIO_FOUND`. #2578 (2.2.3)
* Moved headers that are not part of OIIO's supported public API, but that
  still must be installed to be transitively included, do a "detail"
  subdirectory. #2648 (2.2.4)
* Fix many Mingw compiler warnings. #2657 (2.1.19/2.2.5)
* Windows: Improve Strutil::get_rest_arguments() handling of long path
  syntax (`"\\?\"` style). #2661 (2.1.19/2.2.6)
* Fix compilation error with armv7 + x86. #2660 (2.2.6)

Notable documentation changes:
* Many enhancements in the ImageBuf chapter. #2460 (2.1.11/2.2.0)
* The `LICENSE-THIRD-PARTY.md` file has been reorganized to be clearer,
  grouping parts with identical licenses. #2469 (2.2.1) And renamed to
  "THIRD-PARTY.md" to avoid confusing GitHub's reporting of the project's
  license. (2.2.6)
* Many fixes to the new readthedocs documentation, especially fixes to
  section cross-references and links.
* Improved INSTALL instructions. (2.2.2/2.1.13)
* Fix a variety of breaks on ReadTheDocs. #2581
* Improve the way we discuss optional modifiers.
* Document the PNG output controls for compression level. #2642 (2.2.4)
* Lots of spell check / typo fixes in docs and comments. #2678 (2.2.6)
* INSTALL.md: remove misleading old Windows build instructions. #2679 (2.2.6)
* New file .git-blame-ignore-revs lists the hashes of commits that only
  performed bulk reformatting, so that they don't misattribute authorship
  or modification date. Everybody do this in your local repo:
  `git config blame.ignoreRevsFile .git-blame-ignore-revs`
  #2683 (2.2.6)


Release 2.1.20 (1 Oct 2020) -- compared to 2.1.19
-------------------------------------------------
* Windows: make sure aligned_malloc and aligned_free are properly declared
  as OIIO_API. #2701
* Support for libheif 1.8 and 1.9. #2685 #2724
* Fix crash in IBA::contrast_remap for very large images. #2704
* Bug fix in Strutil::splits and splitsv: when input is the empty string,
  the split should return no pieces. #2712

Release 2.1.19 (1 Sep 2020) -- compared to 2.1.18
-------------------------------------------------
* DPX: Add support for reading DPX files from IOProxy (such as from a memory
  buffer). #2659 #2665
* PNG: New output option "png:filter" allows control of the PNG filter
  options. #2650
* Python: Fix binding of ImageSpec.erase_attribute. #2654
* Python: Fix missing ImageInput.read_image(). #2677
* Windows: Improve Strutil::get_rest_arguments() handling of long path
  syntax (`"\\?\"` style). #2661
* MinGW: Fix a variety of compiler warnings on this platform. #2657
* Fix build on Elbrus 2000 architecture. #2671

Release 2.1.18 (1 Aug 2020) -- compared to 2.1.17
-------------------------------------------------
* Python `ImageBuf.write()` added a variety that takes an open ImageOutput.
  This is the key to writing multi-subimage files from Python. #2640
* `oiiotool --eraseattrib` fixed: was not applying to all subimages. #2632
* RAW: Improve thread safety when more than one thread might be opening
  different raw files at the same time. #2633
* unordered_map_concurrent fixed a missing size decrement upon erase(). #2624
* Fixes to support certain recent pybind11 changes. #2637
* Fixes to support OpenColorIO v2. #2636
* Fixes to support more recent fmtlib versions. #2639
* PNG: document the "png:compressionLevel" output hint. #2642
* In oiioversion.h, add a `OIIO_MAKE_VERSION` macro that constructs the
  integer code for a particular major/minor/patch release. #2641
* 2.1.18.1: Fix version number which for 2.1.18.0 unfortunately still
  said it was 2.1.17.

Release 2.1.17 (1 Jul 2020) -- compared to 2.1.16
-------------------------------------------------
* Build: Use the discovered python binary name, to address the Fedora
  restriction that you must use "python2" or "python3" by name. #2598
* Docs: ImageBufAlgo::nonzero_region had been inadvertently left out of the
  Python chapter.
* Improve RLA reader's ability to detect corrupt or non-RLA files, which
  fixes crashes you could get from trying to read non-image files. #2600
* Support for building against Qt 5.15. (Note: Qt support is only needed
  for the "iv" viewer.) #2605
* Fixes to support LibRaw 0.20 (which is currently in beta3). Note that this
  will make it incompatible with 0.20 beta1 and beta2, due to a fixed typo
  of a struct field in one of the LibRaw's headers. #2613
* oiioversion.h: fix typo that left the OIIO_VERSION_RELEASE_TYPE symbol
  undefined. #2616

Release 2.1.16 (1 Jun 2020) -- compared to 2.1.15
-------------------------------------------------
* OpenEXR: Fix bug in the channel sorting order when channels are "X" and
  "Y" (was reversing the order by confusing "Y" for "luminance"). #2595
* Python: Fixed a bug that lost certain string arguments, especially when
  passing a TypeDesc as its string equivalent. #2587
* fmath: Very minor fix to OIIO::clamp(), shouldn't affect normal use with
  floats at all, but fixed a subtle quasi-bug in OSL. #2594
* TypeDesc has additional helpers of constexpr values TypeFloat2,
  TypeVector2, TypeVector4, TypeVector2i, TypePointer. #2592
* Build: The exported CMake config files now set cmake variable
  `OpenImageIO_PLUGIN_SEARCH_PATH` #2584
* Docs: improvements and fixes to broken page rendering.

Release 2.1.15 (11 May 2020) -- compared to 2.1.14
--------------------------------------------------
* RAW input: set the "raw:flip" attribute if the underlying libraw did a
  reorientation. #2572
* Movie files: Fix possible infinite loop in the FFMpeg-based reader. #2576
* Fixes to allow building against the forthcoming LibRaw 0.20 release. #2484
* Documentation fixes. #2581

Release 2.1.14 (1 May 2020) -- compared to 2.1.13
-------------------------------------------------
* JPEG & PNG: Fix loss of 'config' hints upon close and reopen that could
  happen in cases where scanlines were accessed out of order. #2549
* TIFF: Fix subtle bug when reading certain Exif directories in the header.
  #2540
* Added OCIO role accessors to the ColorConfig class. #2548
* Improve error messages when overscan textures are not possible. #2521
* Build: fix problems when compiling against current libtiff master (symbol
  clash on GPSTAG values). #2539
* Build: Fix static boost to not overlink. #2537.
* Fix some problems with the docs. #2541
* `AttrDelegate::as_vec<>` returns the whole attribute as a std::vector.
  #2528
* Add a build_libtiff.bash script to make it easy to build the libtiff
  dependency. #2543

Release 2.1.13 (1 Apr 2020) -- compared to 2.1.12
-------------------------------------------------
* Fix: iinfo return code now properly indicates failures for files that
  can't be opened. #2511
* Fix: Catch previously uncaught exceptions that could happen in certain
  Filesystem utility calls. #2522
* Fi: Some `span<>` methods involving `std::vector` now will work properly
  with vectors that have custom allocators. #2533
* Fix: ParamValueList `add_or_replace()` was failing to "replace" if the new
  attribute had a different type than the existing one. #2527
* Fix: Fix resolution unit metadata that was not properly set in JPEG output.
  #2516
* Build: Additional cmake controls to customize required vs optional
  dependencies -- `REQUIRED_DEPS` (list of dependencies normally optional
  that should be treated as required) and `OPTIONAL_DEPS` (list of
  dependencies normally required that should be optional). The main use case
  is to force certain optional deps to be required for your studio, to be
  sure that missing deps are a full build break, and not a successful build
  that silently lacks features you need. #2507
* Build: Fix exported config file, it was not ensuring that the Imath
  headers properly ended up in the config include path. #2515
* Build: Ensure compatibility and clean builds with clang 10. #2518
* Build: All the `build_foo.bash` helper scripts now use `set -ex` to ensure
  that if any individual commands in the script fails, the whole thing will
  exit with a failure. #2520
* Build correctly against the current master branch of OpenColorIO
  (previously we were only testing and properly building against the 1.1
  release). #2530
* Added Strutil::upper() and lower() functions. #2525
* ParamValueList enhancement: new `find_pv()` method that is similar to
  `find()` but returns a pointer rather than an iterator and nullptr if the
  attribute is not found. #2527
* Add `get_indexed()` method to ParamValueList and AttrDelegate. #2526

Release 2.1.12 (2 Mar 2020) -- compared to 2.1.11
-------------------------------------------------
* Fix: plugin.h getsym() didn't pass along its report_error param. #2465
* Fix: ImageBuf::getchannel() did not honor its wrap parameter. #2465
* Fix: ImageSpec::erase_attribute() did not honor its `searchtype` param. #2465
* Fix: IBA::reorient() and IBA::computePixelsHashSHA1() did not honor their
  `nthreads` parameter. #2465.
* IBA::resample() now uses the clamp wrap mode to avoid black fringing and
  match the behavior of resize(). #2481
* Fix: ImageBuf::get_pixels() did not honor the stride parameters. #2487.
* fmath.h perf improvements: clamp() is 2x faster; madd() is improved
  especially on platforms without fma hardware; perf improvements in
  `fast_sin`, `fast_cos`; new `safe_fmod` is faster than std::fmod, new
  `fast_neg` is faster than simple negation in many cases, if you don't care
  that -(0.0) is 0.0 (rather than a true -0.0). #2491 #2492 #2494
* strutil: New function: concat(). #2478
* Build: un-embed the 'fmt' headers, instead auto-download if not found.
  #2439
* Build: Protect against certain compiler preprocessor errors for user
  programs that include strutil.h but also include `fmt` on its own. #2498.

Release 2.1.11 (1 Feb 2020) -- compared to 2.1.10
-------------------------------------------------
* Python bindings for `ParamValueList.attribute()`, when being passed
  attributes containing multiple values, now can have those values passed
  as Python lists and numpy arrays (previously they had to be tuples).
  #2437
* OpenEXR support is extended to handle float vector metadata. #2459
* Developer goody: simd.h vfloat3 has added a `normalize()`, `length()`,
  and `length2()` methods, to more closely match the syntax of Imath::Vec3f.
  #2437
* Internals: changed a lot of assertions to only happen in debug build mode,
  and changed a lot that happen in release builds to only print the error
  but not force a termination. #2435
* simd.h fix errors in vbool == and !=. #2463
* Make sure the embedded 'farmhash' implementation is completely hidden
  behind proper namespaces. #2473
* Many docs fixes.

Release 2.1.10.1 (10 Jan 2019)
------------------------------
* Automatically detect the need to link against libatomic (fixes build on
  some less common platforms, should not affect Windows, MacOS, or Linux on
  x86/x86_64 users). #2450 #2455
* Fixes to unordered_map_concurrent.h that affect some users who it for
  things other than OIIO per se (recent changes to the internals broke its
  use for the default underlying std::unordered_map). #2454
* Bump the minimum pybind11 version that we auto-download, and also be sure
  to auto-download if pybind11 is found on the system already but is not an
  adequately new version. #2453
* If libsquish is found on the system at build time, use it, rather than
  the "embedded" copy. This can improve build times of OIIO, and also helps
  us comply with Debian packaging rules that forbid using embedded versions
  of other Debian packages that can be used as simple dependencies. #2451
* Fixes to formatting of man page generation (resolves warnings on Debian
  build process).

Release 2.1.10 (1 Jan 2020) -- compared to 2.1.9
--------------------------------------------------
* Suppress warnings with old libraw on earlier gcc versions. #2413
* Exif read: guard better against out of range offests, fixes crashes when
  reading jpeg files with malformed exif blocks. #2429
* Python: add binding for missing ParamValue constructors. #2417
* oiiotool & ImageBuf better error messages (rather than mysterious crash)
  for certain out of memory conditions. #2414
* oiiotool --create and --pattern take a new optional parameter:
  `:type=name` that overrides the default behavior of allocating all
  internal buffers as float. #2414
* Lots of typo fixes in docs, comments, and error messages. #2438
* Fix broken version in the built openimageio.pc PkgConfig file. #2441
* Fix typo in build script that caused it to fail to set the right symbol
  definition when building static libs. #2442.
* More robust OIIO_PRETTY_FUNCTION definition. #2413
* Better fallback for OIIO_ALIGN, rely on C++11. #2412
* Docs: fix some II and IO chapter examples that used old open() API.
* Build: bump default version of pybind11 to 2.4.3. #2436
* Add ImageBuf::setpixel() methods that use cspan instead of ptr/len. #2443
* Fixes to cmake config generation. #2448


Release 2.1 (8 Dec 2019) -- compared to 2.0
----------------------------------------------
New minimum dependencies:
* CMake minimum is now 3.12. #2348 (2.1.5)

Major new features and performance improvements:
* Support for HEIC/HEIF images. HEIC is the still-image sibling of HEVC
  (a.k.a. H.265), and compresses to about half the size of JPEG but with
  higher visual quality. #2160 #2188 (2.1.0)
* oiiotool new commands: `-evaloff` `-evalon` `--metamerge` `--originoffset`
* ImageCache/TextureSystem improved perf of the tile and file caches under
  heavy thread contention. In the context of a renderer, we have seen
  improvements of around 7% in overall render time, averaged across a suite
  of typical production scenes.  #2314, #2316 (2.1.3) #2381 #2407 (2.1.8)
* Fix huge DPX reading performance regression. Technically this is a bug
  fix that restores performance we once had, but it's a huge speedup.
  #2333 (2.1.4)
* Reading individual frames from very-multi-image files (movie files) has
  been greatly sped up (10x or more). #2345 (2.1.4)

Public API changes:
* ImageSpec new methods `getattribute()` and `getattributetype()`. #2204
  (2.1.1)
* ImageSpec and ParamValueList now support operator `["name"]` as a way
  to set and retrieve attributes. For example,

      myimagespec["compression"] = "zip";
      myimagespec["PixelAspectRatio"] = 1.0f;
      int dither = myimagespec["oiio:dither"].get<int>();
      std::string cs = myimagespec["colorspace"];

  See the documentation about "Attribute Delegates" for more information,
  or the new header `attrdelegate.h`. #2204 (2.1.1) #2297 (2.1.3)
* ImageSpec::find_attribute now will retrive "datawindow" and "displaywindow"
  (type int[4] for images int[6] for volumes) giving the OpenEXR-like bounds
  even though there is no such named metadata for OIIO (the results will
  assembled from x, y, width, height, etc.). #2110 (2.1.0/2.0.4)
* "Compression" names (where applicable) can now have the quality appended
  to the name (e.g., `"jpeg:85"`) instead of requiring quality to be passed
  as a separate piece of metadata. #2111 (2.1.0/2.0.5)
* Python: define `__version__` for the module. #2096 (2.1.0/2.0.4)
* Python error reporting for `ImageOutput` and `ImageBuf.set_pixels`
  involving transferring pixel arrays have changed from throwing exceptions
  to reporting errors through the usual OIIO error return codes and queries.
  #2127 (2.1.0/2.0.5)
* New shell environment variable `OPENIMAGEIO_OPTIONS` can now be used to
  set global `OIIO::attribute()` settings upon startup (comma separated
  name=value syntax). #2128 (2.1.0/2.0.5)
* ImageInput open-with-config new attribute `"missingcolor"` can supply
  a value for missing tiles or scanlines in a file in lieu of treating it
  as an error (for example, how OpenEXR allows missing tiles, or when reading
  an incompletely-written image file). A new global `OIIO::attribute()`
  setting (same name) also accomplishes the same thing for all files read.
  Note that this is only advisory, and not all file times are able to do
  this (OpenEXR is the main one of interest, so that works). #2129 (2.1.0/2.0.5)
* `ImageCache::invalidate()` and `TextureSystem::invalidate()` now take an
  optional `force` parameter (default: true) that if false, will only
  invalidate a file if it has been updated on disk since it was first opened.
  #2133, #2166 (2.1.0/2.0.5)
* New filter name `"nuke-lanczos6"` matches the "lanczos6" filter from Nuke.
  In reality, it's identical to our "lanczos3", but the name alias is
  supposed to make it more clear which one to use to match Nuke, which uses
  a different nomenclature (our "3" is radius, their "6" is full width).
  #2136 (2.1.0/2.0.5)
* New helper functions in `typedesc.h`: `tostring()` converts nearly any
  TypeDesc-described data to a readable string, `convert_type()` does data
  type conversions as instructed by TypeDesc's. #2204 (2.1.1)
* ImageBuf:
    - Construction from an ImageSpec now takes an optional `zero` parameter
      that directly controls whether the new ImageBuf should have its buffer
      zeroed out or left uninitialized. #2237 (2.1.2)
    - `set_write_format()` method has a new flavor that takes a
      `cspan<TypeDesc>` that can supply per-channel data types. #2239 (2.1.1)
* ColorConfig:
    - Added `getColorSpaceFamilyByName()`, `getColorSpaceNames()`,
      `getLookNames()`, `getDisplayNames()`, `getDefaultDisplayName()`,
      `getViewNames()`, `getDefaultViewName()`. #2248 (2.1.2)
    - Added Python bindings for ColorConfig. #2248 (2.1.2)
* Formal version numbers are now four parts: MAJOR.MINOR.PATCH.TWEAK.
  #2313,#2319 (2.1.3)
* ImageInput now sets "oiio:subimages" attribute to an int representing the
  number of subimages in a multi-image file -- if known from reading just
  the header. A positive value can be relied upon (including 1), but a
  value of 0 or no such metadata does not necessarily mean there are not
  multiple subimages, it just means it could not be known from inexpensively
  reading only the header. #2344 (2.1.4)
* The `imagesize_t` and `stride_t` values now have revised definitions.
  It should be fully API/ABI compatible (at least for 64 bit systems), but
  is a simpler, more modern, more platform-independent definition.
  #2351 (2.1.5)
* `DeepData` has been altered to make pixel indices and total counts int64_t
  rather than int, in order to be safe for very large images that have > 2
  Gpixels. #2363 (2.1.5)
* On OSX, we now expect non-embedded plugins to follow the convention of
  naming runtime-loaded modules `foo.imageio.so` (just like on Linux),
  whereas we previously used the convention of `foo.imageio.dylib`. Turns
  out that dylib is supposed to be only for shared libraries, not runtime
  loadable modules. #2376 (2.1.6)

Fixes and feature enhancements:
* oiiotool:
    - New `-evaloff` and `-evalon` lets you disable and enable the expression
      substitution for regions of arguments (for example, if you have an
      input image filename that contains `{}` brace characters that you want
      interpreted literally, not evaluated as an expression). #2100 (2.1.0/2.0.4)
    - `--dumpdata` has more intelligible output for uint8 images. #2124
       (2.1.0/2.0.4)
    - Fixed but that could prevent `-iconvert oiio:UnassociatedAlpha 1` from
      correctly propagating to the input reader. #2172 (2.1.0/2.0.6)
    - `-o:all=1` (which outputs all subimages to separate files) fixed a
      crash that would occur if any of the subimages were 0x0 (it could
      happen; now it just skips outputting those subimages). #2171 (2.1.0)
    - Improved support of files with multiple subimages: Several commands
      honored `-a` but did not respect individual `allsubimages=` modifiers
      (--ch, --sattrib, --attrib, --caption, --clear-keywords,
      --iscolorspace, --orientation, --clamp, -fixnan); Several commands
      always worked on all subimages, but now properly respect `-a` and
      `allsubimages=` (--origin, --fullpixels, --croptofull, --trim);
      Several commands were totally unaware of subimages, but now are so and
      respect `-a` and `allsubimages=` (--crop, --fullsize, --zover, --fill,
      --resize, --resample). #2202 #2219, #2242 (2.1.1, 2.1.2)
    - `--ociodisplay`: empty display or view names imply using the default
      display or view. #2273 (2.0.10/2.1.3)
    - `--metamerge` option causes binary image operations to try to "merge"
      the metadata of their inputs, rather than simply copy the metadata
      from the first input and ignore the others. #2311 (2.1.3)
    - `--colormap` now supports a new "turbo" color map option. #2320 (2.1.4)
    - Expression evaluation has been extended to support operators `//` for
      integer division (whereas `/` is floating point division), and `%`
      for integer modulus. #2362 (2.1.5)
    - New `--originoffset` resets the data window origin relative to its
      previous value (versus the existing `--origin` that sets it absolutely).
      #2369 (2.1.5)
    - `--paste` has two new optional modifiers: `:all=1` pastes the entire
      stack of images together (versus the default of just pasting the top
      two images on the stack), and `:mergeroi=1` causes the result to have
      the merged data window of all inputs, instead of the foreground image
      clipping against the boundary of the background image data. #2369 (2.1.5)
    - `--paste` now works with deep images. #2369 (2.1.5)
    - `--paste` semantics have changed: the meaning of pasting FG into BG at
      (x,y) now means that the (0,0) origin of FG ends up at (x,y), whereas
      before it placed the corner of FG's data window at (x,y). This will
      not change behavior for ordinary images where FG's data window is (0,0),
      but it makes behavior more sensible for "cropped" or "shrink-wrapped"
      FG images that have non-zero data window origin. #2369 (2.1.5)
    - `paste()` is now multithreaded and therefore much faster. #2369 (2.1.5)
    - `--ociotransform` no longer issues an error message when no valid OCIO
      configuration is found (because it's not needed for this operation).
      #2371 (2.1.5)
    - `--compare` would fail to notice differences in deep images where the
      corresponding pixels had differing numbers of samples. #2381 (2.1.8)
* ImageBuf/ImageBufAlgo:
    - `IBA::channel_append()` previously always forced its result to be float,
      if it wasn't previously initialized. Now it uses the usual type-merging
      logic, making the result the "widest" type of the inputs. #2095
      (2.1.0/2.0.4)
    - IBA `resize()`, `fit()`, and `resample()` are no longer restricted to
      source and destination images having the same number of channels.
      #2125 (2.1.0/2.0.5)
    - Improve numerical precision of the unpremult/premult part of certain
      color transformations. #2164 (2.1.0)
    - `ImageBuf::read()` now properly forwards the "progress" parameters
      to any underlying call to `read_image`. #2196 (2.1.1)
    - The `OIIO_DISPATCH_COMMON_TYPES2/3` macros used internally by many IBA
      functions have been expanded to handle a few more cases "natively"
      without conversion to/from float. This may make a few cases of odd
      data type combinations have higher precision. #2203 (2.0.8/2.1.1)
    - IBA `resize()` fix precision issues for 'double' images. #2211
      (2.0.8/2.1.1)
    - `IBA::ociodisplay()`: empty display or view names imply using the
      default display or view. #2273 (2.0.10/2.1.3)
    - `IBA::fixNonFinite()`: fixed impicit float/double casts to half. #2301
      (2.0.10/2.1.3)
    - `IBA::color_map()`:  now supports a new "turbo" color map option.
      #2320 (2.1.4)
    - `IBA::paste()` now works with deep images. #2369 (2.1.5)
    - `paste` semantics have changed: the meaning of pasting FG into BG at
      (x,y) now means that the (0,0) origin of FG ends up at (x,y), whereas
      before it placed the corner of FG's data window at (x,y). This will
      not change behavior for ordinary images where FG's data window is (0,0),
      but it makes behavior more sensible for "cropped" or "shrink-wrapped"
      FG images that have non-zero data window origin. #2369 (2.1.5)
    - `paste()` is now multithreaded and therefore much faster. #2369 (2.1.5)
    - `ociotransform()` no longer issues an error message when no valid OCIO
      configuration is found (because it's not needed for this operation).
      #2371 (2.1.5)
    - Python `ociotransform` and `ociolook` mixed up the names and orders of
      the `inverse` and `unpremult` params, making it so that you couldn't
      properly specify the inverse. #2371 (2.1.5)
    - `IBA::compare()` would fail to notice differences in deep images where
      the corresponding pixels had differing numbers of samples. #2381 (2.1.8)
* ImageInput read_image/scanline/tile fixed subtle bugs for certain
  combination of strides and channel subset reads. #2108 (2.1.0/2.0.4)
* ImageCache / TextureSystem / maketx:
    - More specific error message when tile reads appear to be due to the
      file having changed or been overwritten on disk since it was first
      opened. #2115 (2.1.0/2.0.4)
    - maketx: the `-u` (update mode) is slightly less conservative now,
      no longer forcing a rebuild of the texture just because the file uses
      a different relative directory path than last time. #2109 (2.1.0/2.0.4)
    - Protection against certain divide-by-zero errors when using
      very blurry latong environment map lookups. #2121 (2.1.0/2.0.5)
    - `maketx -u` is smarter about which textures to avoid re-making because
      they are repeats of earlier commands. #2140 (2.1.0/2.05)
    - Fix possible maketx crash on Windows due to a stack overflow within
      MSVS's implementation of std::regex_replace! #2173 (2.1.0/2.0.6)
    - TS: New attribute "max_mip_res" limits filtered texture access to MIP
      levels that are no higher than this resolution in any dimension. The
      default is 1<<30, meaning no effective limit. #2174 (2.1.1)
    - Stats now count the number of `TS::get_texture_info/IC::get_image_info`
      calls, like it did before for texture, etc. #2223 (2.1.1)
    - `TS::environment()` can resolve subimage by name, as we do for
      texture() and texture3d(). #2263
    - Improvements to error message propagation. (2.1.3)
    - Avoid creating a new thread info struct while resolving udims. #2318
      (2.1.4)
    - Work around bug in OpenEXR, where dwaa/dwab compression can crash when
      used on 1-channel tiled images with a tile size < 16. This can crop up
      for MIP-maps (high levels where rez < 16), so we detect this case and
      switch automatically to "zip" compression. #2378 (2.1.6)
    - When converting images to texture (via maketx or IBA::make_texture),
      correctly handle color space conversions for greyscale images.
      #2400 (2.1.8)
* iv viewer:
    - Image info window now sorts the metadata, in the same manner as
      `iinfo -v` or `oiiotool -info -v`. #2159 (2.1.0/2.0.5)
* All command line utilities, when run with just `--help`, will exit with
  return code 0. In other words, `utility --help` is not an error.
  #2364 (2.1.5) #2383 (2.1.8)
* Python bindings:
    - Fix inability for Python to set timecode attributes (specifically, it
      was trouble setting ImageSpec attributes that were unsigned int
      arrays). #2279 (2.0.9/2.1.3)
* Improved performance for ustring creation and lookup. #2315 (2.1.3)
* BMP:
    - Fix bugs related to files with very high resolution (mostly 32 bit
      int overflow issues and care to use 64 bit fseeks). Also speed up
      reading and writing very large files. #2404 (2.1.8)
* DPX:
    - Now recognizes the new transfer/colorimetric code for ADX. #2119
      (2.1.0/2.0.4)
    - Fix potential crash when file open fails. #2186 (2.0.7/2.1.1)
    - Support for reading and writing 1-channel (luma, etc.) images. #2294
      (2.0.10/2.1.3)
    - Fix huge DPX reading performance regression. #2333 (2.1.4)
    - Fix bugs related to int32 math that would lead to incorrect
      behavior in very high-resolution files. #2396 (2.1.3)
* ffmpeg/Movie files:
    - Reading individual frames from very-multi-image files (movie files) has
      been greatly sped up (10x or more). #2345 (2.1.4)
    - Support for reading movie files that (a) contain alpha channels, and
      (b) have bit depths > 8 bits per channel. Previously, such files
      would be read, but would be presented to the app as a 3-channel
      8 bit/channel RGB. #2349 (2.1.5)
* FITS:
    - Fix 16 and 32 bit int pixels which FITS spec says are signed, but we
      were treating as unsigned. #2178 (2.1.0)
* HDR/RGBE:
    - Fix bugs related to files with very high resolution (mostly 32 bit
      int overflow issues and care to use 64 bit fseek). Also speed up
      reading and writing very large files. #2406 (2.1.8)
* IFF
    - Detect and error requests to open files for writing with resolutions
      too high to be properly supported by IFF files. #2397 (2.1.8)
    - Improve error messages when a file can't be opened. #2398 (2.1.8)
* JPEG:
    - Read-from-memory is now supported via IOProxy use. #2180. (2.1.1)
* JPEG-2000:
    - Disable JPEG-2000 support for the (rare) combination of an older
      OpenJPEG 1.x and EMBEDPLUGINS=0 mode, which was buggy. The solution if
      you really need EMBEDPLUGINS and JPEG-2000 support is to please use
      OpenJPEG >= 2.0. #2183. (2.0.7/2.1.1)
* OpenEXR:
    - Avoid some OpenEXR/libIlmImf internal errors with DWA compression by
      switching to zip for single channel images with certain small tile
      sizes. #2147 (2.1.0/2.0.5)
    - Suppress empty string subimage name (fixes a problem with certain
      V-Ray written multi-part exr images). #2190 (2.1.1/2.0.7)
    - Fixed bug that broke th ability to specify compression of multipart
      OpenEXR files. #2252 (2.1.2)
* PNG:
    - More careful catching and reporting errors and corrupt PNG files.
      #2167 (2.1.0/2.0.6)
    - IOProxy reading is now supported. #2180. (2.1.1)
* PSD:
    - When reading PSD files with multiple PhotoShop "layers", properly set
      ImageSpec x, y to the image plane offset (upper left corner) of the
      layer, and set and metadata "oiio:subimagename" to the layer name.
      #2170 (2.1.0)
* RAW:
    - Clarification about color spaces: The open-with-config hint
      "raw:ColorSpace" is more careful about color primaries versus transfer
      curve. Asking for "sRGB" (which is the default) gives you true sRGB --
      both color primaries and transfer. Asking for "linear" gives you
      linear transfer with sRGB/Rec709 primaries. The default is true sRGB,
      because it will behave just like JPEG. #2260 (2.1.2)
    - Added "raw:half_size" and "raw:user_mul" configuration attributes.
      #2307 (2.1.3)
* RLA:
    - Improved logic for determining the single best data type to report
      for all channels. #2282 (2.1.3)
* SGI:
    - Fix bugs when writing extremely high resolution images, due to
      internal 32 bit arithmetic on file offsets. #2402 (2.1.8)
    - Speed up reading and writing of SGI files. #2402 (2.1.8)
* Targa:
    - Put in checks to detect and error requests to write Targa with
      resolutions too high to be supported by the format. #2405 (2.1.8)
* TIFF:
    - Fix problems with JPEG compression in some cases. #2117 (2.1.0/2.0.4)
    - Fix error where reading just a subset of channels, if that subset did
      not include the alpha channel but the image was "unassociated alpha",
      the attempt to automatically associate (i.e. "premultiply" the alpha)
      upon read would get bogus values because the alpha channel was not
      actually read. Now in this case it will not do the premultiplication.
      So if you are purposely reading RGB only from an RGBA file that is
      specifically "unassociated alpha", beware that you will not get the
      automatic premultiplication. #2122 (2.1.0/2.0.4)
    - More careful check and error reporting when user tries to request
      writing to a TIFF file mixed channel data types (which is not supported
      by the underlying libtiff). #2112 (2.1.0/2.0.5)
    - Fix crash reading certain old nconvert-written TIFF files.
      #2207 (2.0.8/2.1.1)
    - Fix bugs when reading TIFF "cmyk" files. #2292. (2.0.10/2.1.3)
    - Correctly handle read and write of 6, 14, and 24 bit per sample
      images. #2296 (2.1.3)
    - Fix potential deadlock in TIFF I/O: minor flaw with threadpool method
      #2327 (2.1.4)
* WebP:
    - Fix bug that gave totally incorrect image read for webp images that
      had a smaller width than height. #2120 (2.1.0/2.0.4)
* zfile:
    - Put in checks to detect and error requests to write zfiles with
      resolutions too high to be supported by the format. #2403 (2.1.8)
* Fix potential threadpool deadlock issue that could happen if you were
  (among possibly other things?) simultaneously calling make_texture from
  multiple application threads. #2132 (2.1.0/2.0.4)
* ImageInput/ImageOutput `create()` now properly lets you specify the type
  for reader/writer from the format name itself (versus just the extension,
  for example "openexr" versus "exr"). #2185 (2.1.1)
* Make all the various "could not open" messages across the writers use the
  same phrasing. #2189 (2.1.1)
* Better care in some image readers/writers to avoid errors stemming from
  integer overflow when compting the size of large images. #2232 (2.1.2)

Build/test system improvements and platform ports:
* Major overhaul of the CMake build system now that our CMake minimum is
  3.12. #2348 #2352 #2357 #2360 #2368 #2370 #2372 #2373 (2.1.5) #2392 (2.1.8)
  Highlights:
    - All optional dependencies (e.g. "Pkg") now can be disabled (even if
      found) with cmake -DUSE_PKG=0 or environment variable USE_PKG=0.
      Previously, some packages supported this, others did not.
    - All dependencies can be given find hints via -DPkg_ROOT=path or by
      setting environment variable Pkg_ROOT=path. Previously, some did, some
      didn't, and the ones that did had totally inconsistent names for the
      path hint variable (PKG_HOME, PKG_ROOT_DIR, PKG_PATH, etc).
    - Nice color coded status messages making it much more clear which
      dependencies were found, which were not, which were disabled.
    - Use standard BUILD_SHARED_LIBS to control shared vs static libraries,
      replacing the old nonstandard BUILDSTATIC name.
    - Use correct PUBLIC/PRIVATE marks with target_link_libraries and
      target_include_directories, and rely on cmake properly understanding
      the transitive dependencies.
    - CMAKE_DEBUG_POSTFIX adds an optional suffix to debug libraries.
    - CMAKE_CXX_STANDARD to control C++ standard (instead of our nonstandard
      USE_CPP).
    - CXX_VISIBILITY_PRESET controls symbol visibility defaults now, not
      our nonstandard HIDE_SYMBOLS. And the default is to keep everything
      hidden that is not part of the public API.
    - At config time, `ENABLE_<name>=0` (either as a CMake variable or an
      env variable) can be used to disable any individual file format or
      command line utility. E.g., `cmake -DENABLE_PNG=0 -DENABLE_oiiotool=0`
      This makes it easier to greatly reduce build time if you are 100%
      sure there are formats or components you don't want or need.
    - Config based install and usage.
* Deprecate "missingmath.h". What little of it is still needed (it mostly
  addressed shortcomings of old MSVS releases) is now in fmath.h. #2086
* Remove "osdep.h" header that was no longer needed. #2097
* Appveyor scripts have been overhauled and simplified by relying on
  `vcpkg` to build dependencies. #2113 (2.1.0/2.0.4)
* Detect and error if builder is trying to use a pybind11 that's too old.
  #2144 (2.1.0/2.0.5)
* New CMake build-time option `OIIO_LIBNAME_SUFFIX` (default: empty) lets
  you append an optional name to the libraries produced (to disambiguate
  two builds at the same facility or distro, much like you could do before
  for symbols with custom namespaces). #2148 (2.1.0)
* On MacOS 10.14 Mojave, fix warnings during `iv` compiler about OpenGL
  being deprecated in future releases. #2151 (2.1.0/2.0.5)
* At build time, the Python version used can be controlled by setting the
  environment variable `$OIIO_PYTHON_VERSION`, which if set will initialize
  the default value of the CMake variable `PYTHON_VERSION`. #2161 (2.0.5/2.1.0)
* On non-Windows systems, the build now generates a PkgConfig file, installed
  at `CMAKE_INSTALL_PREFIX/lib/pkgconfig/OpenImageIO.pc`. #2158 (2.0.5/2.1.0)
* A new unit test has been backported from master, which tries to perform a
  series of read/write tests on every file format. In particular, this tests
  certain error conditions, like files not existing, or the directory not
  being writable, etc. #2181, #2189 (2.0.8/2.1.1)
* Support for CI tests on CircleCI. #2194 (2.1.1) Retired in #2389 (2.1.8).
* New build-time flag `USE_WEBP=0` can be used to disable building WebP
  format support even on platforms where webp libraries are found.
  #2200 (2.1.1)
* Fix compiler warnings on Windows. #2209 #2213 #2214 #2392
* Crashes in the command line utilities now attempt to print a stack trace
  to aid in debugging (but only if OIIO is built with Boost >= 1.65, because
  it relies on the Boost stacktrace library). #2229 (2.0.8/2.1.1)
* Add gcc9 to Travis tet matrix and fix gcc9 related warnings. #2235 (2.1.2)
* VDB reader pulled in the TBB libraries using the wrong CMake variable.
  #2274 (2.1.3)
* The embedded `fmt` implementation has been updated to fix windows
  warnings. #2280 (2.1.3)
* Improvements for finding certain new Boost versions. #2293 (2.0.10/2.1.3)
* Build fixes for MinGW. #2304, #2308 (2.0.10/2.1.3)
* libraw: Fixes to make it build properly against some changes in the
  libraw development master. #2306 (2.1.3)
* Use GitHub Actions CI. Eliminate Appveyor and some Travis tests.
  #2334 (2.1.4) #2356 (2.1.5) #2395 (2.1.8)
* Updated and improved finding of OpenEXR and `build_openexr.bash` script
  that we use for CI. #2343 (2.1.4)
* Upgrade the pybind11 version that we auto-install when not found (to 2.4.2),
  and add logic to detect the presence of some pybind11 versions that are
  known to be (buggily) incompatible with C++11. #2347 (2.1.5)
* Fix errors in very new MSVS versions where it identified a suspicious
  practice of ImageBuf's use of a unique_ptr of an undefined type. Jump
  through some hoops to make that legal. #2350 (2.1.5)
* All Python scripts in the tests have been modified as needed to make them
  correct for both Python 2.7 and 3.x. #2355, #2358 (2.1.5)
* Tests are now safe to run in parallel and in unspecified order. Running
  with env variable CTEST_PARALLEL_LEVEL=[something more than 1] greatly
  speeds up the full testsuite on multi-core machines. #2365 (2.1.5)
* Bump robin map version to latest release (v0.6.2) #2401 (2.1.8)
* Fix compiler warnings in ustring.h when `_LIBCPP_VERSION` is not defined.
  #2415 (2.1.8.1)
* Bump fmt library to v6.1.0. #2423 (2.1.8.1)

Developer goodies / internals:
* argparse.h:
    - Add unit tests. #2192 (2.1.1)
    - Add "%1" which is like "%*" but its list receives only arguments that
      come *before* any other dash-led arguments. #2192 (2.1.1)
    - Allow specifiers such as "%d:WIDTH" the part before the colon is the
      type specifier, the part after the colon is the name of the parameter
      for documentation purposes. #2312 (2.1.3)
* attrdelegate.h:
    - New header implements "attribute delegates." (Read header for details)
      #2204 (2.1.1)
* dassert.h:
    - Spruce up assertion macros: more uniform wording, and use pretty
      function printing to show what function the failure was in. #2262
    - The new preferred assertion macros are `OIIO_ASSERT` and `OIIO_DASSERT`.
      The `OIIO_ASSERT` always tests and prints an error message if the test
      fails, but now only aborts when compiled without NDEBUG defined (i.e.
      no abort for release builds), whereas `OIIO_DASSERT` is for debug mode
      only and does nothing at all (not even perform the test) in release
      mode. These names and behaviors are preferred over the old `ASSERT`
      and `DASSERT`, though those deprecated names will continue for at least
      another major release. #2411 (2.1.8.1)
* filesystem.h:
    - Change many filesystem calls to take string_view arguments. #2388 (2.1.8)
    - New `fseek()` and `ftell()` that always use 64 bit offsets to be safe
      for very large files. #2399 (2.1.8)
* fmath.h:
    - `safe_mod()` does integer modulus but protects against mod-by-zero
      exceptions. #2121 (2.1.0/2.0.5)
    - pow2roundup/pow2rounddown have been renamed ceil2/floor2 to reflect
      future C++20 standard. The old names still work, so it's a fully back
      compatible change. #2199 (2.0.8/2.1.1)
    - To match C++20 notation, use `rotl()` template innstead of separate
      rotl32/rotl64 functions. #2299, #2309 (2.1.3)
* platform.h:
    - New `OIIO_RETURNS_NONNULL` macro implements an attribute that marks
      a function that returns a pointer as guaranteeing that it's never
      NULL. #2150 (2.1.0/2.0.5)
* SHA1.h:
    - Upgraded this embedded code from version 1.8 (2008) to the newest
      release, 2.1 (2012). This fixes some Windows warnings. #2342 (2.1.4)
* simd.h:
    - Added vec4 * matrix44 multiplication. #2165 (2.1.0/2.0.6)
    - Guard against shenanigans when Xlib.h having been included and
     `#define`ing True and False. #2272 (2.0.9/2.1.3)
* strutil.h:
    - Added `excise_string_after_head()`. #2173 (2.1.0/2.0.6)
    - Fixed incorrect return type of `stof()`. #2254 (2.1.2)
    - Added `remove_trailing_whitespace()` and `trim_whitespace()`. #2298
      (2.1.3)
    - `Strutil::wordwrap()` now lets you specify the separation characters
      more flexibly (rather than being hard-coded to spaces as separators).
      #2116 (2.1.0/2.0.4)
    - `Strutil::parse_while()`.  #2139 (2.1.0/2.0.5)
    - Added a variety of `join()` that allows you to set the number of items
      joined, truncating or padding with default values as needed. #2408
      (2.1.8)
    - Fix `join` to produce a joined string of float-like values with
      locale-independent formatting. #2408 (2.1.8)
    - Fix `vsnprintf` to be locale independent. #2410 (2.1.8)
    - New `lstrip()` and `rstrip()` are just like the existing `strip()`,
      but operate only on the beginning/left side or ending/right side of
      the string, respectively. #2409 (2.1.8)
* string_view.h:
    - `string_view` now adds an optional `pos` parameter to the `find_first_of`
      / `find_last_of` family of methods. #2114 (2.1.0/2.0.4)
* sysutil.h:
    - Added `stacktrace()` and `setup_crash_stacktrace()`. (Only functional
      if OIIO is built with Boost >= 1.65, because it relies on the Boost
      stacktrace library). #2229 (2.0.8/2.1.1)
* unittest.h:
    - Add `OIIO_CHECK_IMAGEBUF_STATUS()` test macro. #2394 (2.1.8)
* unordered_map_concurrent.h:
    - Performance improvement by avoiding redundant hashing of keys, and
      improving the speed and properties of the hash function. #2313, #2316
      (2.1.3)
* ustring.h:
    - Bug fix in `ustring::compare(string_view)`, in cases where the
      string_view was longer than the ustring, but had the same character
      sequence up to the length of the ustring. #2283 (2.0.10/2.1.3)
* Wide use of declaring methods `noexcept` when we want to promise that
  they won't throw exceptions. #2156, #2243 (2.1.0, 2.1.2)
* Changed all (we think) internal string formatting that expects printf
  formatting notation to use the errorf/sprintf style calls, in anticipation
  of the error/format (no trailing -f) calls to eventually follow the
  std::format/python formatting notation. #2393 (2.1.8)

Notable documentation changes:
* The whole documentation system has been overhauled. The main docs have
  been converted from LaTeX to Sphinx (using Doxygen and Breathe) for
  beautiful HTML as well as PDF docs and automatic hosting on
  https://openimageio.readthedocs.io  #2247,2250,2253,2255,2268,2265,2270
* Copyright notices have been changed for clarity and conformance with
  SPDX conventions. #2264
* New GitHub issue templates, making separate issue types for bug reports,
  feature requests, build problems, and questions. #2271,#2346



Release 2.0.13 (1 Dec 2019) -- compared to 2.0.12
--------------------------------------------------
* Bug fix in deep image compare (`IBA::compare()` or `oiiotool --compare`)
  would fail to notice differences in deep images where the corresponding
  pixels had differing numbers of samples. #2381 (2.1.8/2.0.13)
* DPX: Fix bugs related to int32 math that would lead to incorrect behavior
  in very high-resolution files. #2396 (2.1.3/2.0.13)
* When converting images to texture (via maketx or IBA::make_texture),
  correctly handle color space conversions for greyscale images. #2400
  (2.1.8/2.0.13)
* Build: suppress warnings with libraw for certain gcc versions.
* Build: Fix compiler warnings in ustring.h when `_LIBCPP_VERSION` is not
  defined. #2415 (2.1.8.1/2.0.13)
* filesystem.h: New `fseek()` and `ftell()` that always use 64 bit offsets
  to be safe for very large files. #2399 (2.1.8/2.0.13)
* `Strutil::parse_string()` - fix bugs that would fail for escaped quotes
  within the string. #2386 (2.1.8/2.0.13)
* `Strutil::join()` added a variety that allows you to set the number of
  items joined, truncating or padding with default values as needed. #2408
  (2.1.8/2.0.13)
* New `Strutil::lstrip()` and `rstrip()` are just like the existing `strip()`,
  but operate only on the beginning/left side or ending/right side of
  the string, respectively. #2409 (2.1.8/2.0.13)

Release 2.0.12 (1 Nov, 2019) -- compared to 2.0.11
--------------------------------------------------
* Fix compiler warnings on some platform. #2375
* Work around bug in OpenEXR, where dwaa/dwab compression can crash when
  used on 1-channel tiled images with a tile size < 16. This can crop up for
  MIP-maps (high levels where rez < 16), so we detect this case and switch
  automatically to "zip" compression. #2378

Release 2.0.11 (1 Oct, 2019) -- compared to 2.0.10
-------------------------------------------------
* Fixes to build against LibRaw master. #2306
* Fix DPX reading performance regression. #2333
* Guard against buggy pybind11 versions. #2347
* Fixes for safe Cuda compilation of `invert<>` in fmath.h. #2197

Release 2.0.10 (1 Aug, 2019) -- compared to 2.0.9
-------------------------------------------------
* ColorConfig improvements: (a) new getColorSpaceFamilyByName(); (b) new
  methods to return the list of all color spaces, looks, displays, or views
  for a display; (c) all of ColorConfig now exposed to Python. #2248
* `IBA::ociodisplay()` and `oiiotool --ociodisplay`: empty display or view
  names imply using the default display or view. #2273
* Bug fix in `ustring::compare(string_view)`, in cases where the string_view
  was longer than the ustring, but had the same character sequence up to
  the length of the ustring. #2283
* `oiiotool --stats`: Fixed bug where `-iconfig` hints were not being
  applied to the file as it was opened to compute the stats. #2288
* Bug fix: `IBA::computePixelStats()` was not properly controlling the
  number of threads with the `nthreads` parameter. #2289
* Bug fix when reading TIFF bugs: In cases where the reader needed to close
  and re-open the file silently (it could happen for certain scanline
  traversal patterns), the re-open was not properly honorig any previous
  "rawcolor" hints from the original open. #2285
* Nuke txWriter updates that expose additional make_texture controls. #2290
* Build system: Improvements for finding certain new Boost versions. #2293
* Build system: Improvements finding OpenEXR installation.
* Fix bugs when reading TIFF "cmyk" files. #2292.
* DPX: support for reading and writing 1-channel (luma, etc.) DPX images.
  #2294
* `IBA::fixNonFinite()`: fixed impicit float/double casts to half. #2301
* Build fixes for MinGW. #2304

Release 2.0.9 (4 Jul, 2019) -- compared to 2.0.8
------------------------------------------------
* RAW: Clarification about color spaces: The open-with-config hint
  "raw:ColorSpace" is more careful about color primaries versus transfer
  curve. Asking for "sRGB" (which is the default) gives you true sRGB --
  both color primaries and transfer. Asking for "linear" gives you linear
  transfer with sRGB/Rec709 primaries. The default is true sRGB, because it
  will behave just like JPEG. #2260 (2.1.2)
* Improved oiiotool support of files with multiple subimages: Several
  commands honored `-a` but did not respect individual `allsubimages=`
  modifiers (--ch, --sattrib, --attrib, --caption, --clear-keywords,
  --iscolorspace, --orientation, --clamp, -fixnan); Several commands always
  worked on all subimages, but now properly respect `-a` and `allsubimages=`
  (--origin, --fullpixels, --croptofull, --trim); Several commands were
  totally unaware of subimages, but now are so and respect `-a` and
  `allsubimages=` (--crop, --fullsize, --zover, --fill, --resize,
  --resample). #2202 #2219, #2242
* Fix broken ability to specify compression of multipart exr files. #2252
* Fix Strutil::stof() return type error and other windows warnings. #2254
* IBA::colortmatrixtransform() and `oiiotool --ccmatrix` allow you to
  perform a matrix-based color space transformation. #2168
* Guard simd.h against shenanigans when Xlib.h having been included and
  `#define`ing True and False. #2272
* RAW: Clarification about color spaces: The open-with-config hint
  "raw:ColorSpace" is more careful about color primaries versus transfer
  curve. Asking for "sRGB" (which is the default) gives you true sRGB --
  both color primaries and transfer. Asking for "linear" gives you linear
  transfer with sRGB/Rec709 primaries. The default is true sRGB, because it
  will behave just like JPEG. #2260
* Fix inability for python to set timecode attributes (specifically, it was
  trouble setting ImageSpec attributes that were unnsigned int arrays).
  #2279

Release 2.0.8 (3 May, 2019) -- compared to 2.0.7
------------------------------------------------
* Fix Windows broken read of JPEG & PNG in some circumstances. #2231
* Some minor fixes to JPEG & PNG reading and file error robustness. #2187
* Fix crash reading certain old nconvert-written TIFF files. #2207
* Internals: The `OIIO_DISPATCH_COMMON_TYPES2/3` macros used by many
  ImageBufAlgo functions have been expanded to handle a few more cases
  "natively" without conversion to/from float. This may make a few cases
  of odd data type combinations have higher precision. #2203
* ImageBufAlgo::resize() fixes precision issues for 'double' images. #2211
* Testing: A new unit test has been backported from master, which tries to
  perform a series of read/write tests on every file format. In partcular,
  this tests certain error conditions, like files not existing, or the
  directory not being writable, etc. #2181
* Crashes in the command line utilities now attempt to print a stack trace
  to aid in debugging (but only if OIIO is built with Boost >= 1.65, because
  it relies on the Boost stacktrace library). #2229
* Dev goodies: fmath.h's powwroundup/pow2rounddown have been renamed
  ceil2/floor2 to reflect future C++ standard. The old names still work, so
  it's a fully back compatible change. #2199

Release 2.0.7 (1 Apr, 2019) -- compared to 2.0.6
------------------------------------------------
* DPX: fix potential crash when file open fails. #2186
* EXR: Suppress empty string for subimage name (fixes a problem when reading
  files written by V-Ray). #2190
* Disable JPEG-2000 support for the (rare) combination of an older OpenJPEG
  1.x and EMBEDPLUGINS=0 mode, which was buggy. The solution if you really
  need EMBEDPLUGINS and JPEG-2000 support is to please use OpenJPEG >= 2.0.
  #2183.
* New build flag `USE_WEBP=0` can be set to 0 to force disabled support of
  WebP even when the webp package is found. #2200
* Bug fix: `ImageInput::create(name)` and `ImageOutput::create(name)` worked
  if `name` was a filename (such as `foo.exr`), or the extension (such as
  `exr`), but previously did not work if it was the name of the format
  (such as `openexr`), despite having been documented as working in that
  case. #2185

Release 2.0.6 (1 Mar, 2019) -- compared to 2.0.5
------------------------------------------------
* PNG: more careful catching of errors and corrupt png files. #2167
* PSD: read now properly extracts layer/subimage name and data window offset
  coordinates. #2170
* ImageBuf: Fix bug in propagating unassociated alpha behavior request. #2172
* `oiiotool -o:all=1` fix crash when outputting 0x0 subimages. #2171
* Developer goodies: simd.h ops for vec4 * mat44 multiplication. #2165
* Developer goodies: `Strutil::excise_string_after_head()` #2173
* Fix crashes on Windows from certain regex replacement happening as part
  of MakeTexture (internally avoid MSVS implementation of std::regex). #2173

Release 2.0.5 (1 Feb, 2019) -- compared to 2.0.4
------------------------------------------------
* `resize()`, `fit()`, and `resample()` are no longer restricted to
  source and destination images having the same numer of channels. #2125
* Python error reporting for `ImageOutput` and `ImageBuf.set_pixels` involving
  transferring pixel arrays have changed from throwing exceptions to reporting
  errors through the usual OIIO error return codes and queries. #2127
* Protection against certain divide-by-zero errors when using very blurry
  latlong environment map lookups. #2121
* New shell environment variable `OPENIMAGEIO_OPTIONS` can now be used to
  set global `OIIO::attribute()` settings upon startup (comma separated
  name=value syntax). #2128
* ImageInput open-with-config new attribute `"missingcolor"` can supply
  a value for missing tiles or scanlines in a file in lieu of treating it
  as an error (for example, how OpenEXR allows missing tiles, or when reading
  an incompletely-written image file). A new global `OIIO::attribute()`
  setting (same name) also accomplishes the same thing for all files read.
  Note that this is only advisory, and not all file times are able to do
  this (OpenEXR is the main one of interest, so that works). #2129
* New filter name `"nuke-lanczos6"` matches the "lanczos6" filter from Nuke.
  In reality, it's identical to our "lanczos3", but the name alias is
  supposed to make it more clear which one to use to match Nuke, which uses
  a different nomenclature (our "3" is radius, their "6" is full width).
  #2136
* `maketx -u` is smarter about which textures to avoid re-making because
   they are repeats of earlier commands. #2140
* Detect/error if builder is trying to use a pybind11 that's too old. #2144
* OpenEXR: avoid some OpenEXR/libIlmImf internal errors with DWA compression
  by switching to zip for single channel images with certain small tile
  sizes. #2147
* On MacOS 10.14 Mojave, fix warnings during `iv` compile about OpenGL
  being deprecated in future releases. #2151
* `iv` info window now sorts the metadata. #2159
* At build time, the Python version used can be controlled by setting the
  environment variable `$OIIO_PYTHON_VERSION`, which if set will initialize
  the default value of the CMake variable `PYTHON_VERSION`. #2161 (2.0.5)
* On non-Windows systems, the build now generates a PkgConfig file, installed
  at `CMAKE_INSTALL_PREFIX/lib/pkgconfig/OpenImageIO.pc`. #2158 (2.0.5)

Release 2.0.4 (Jan 5, 2019) -- compared to 2.0.3
------------------------------------------------
* Fix potential threadpool deadlock issue that could happen if you were
  (among possibly other things?) simultaneously calling make_texture from
  multiple application threads. #2132
* ImageInput read_image/scanline/tile fixed subtle bugs for certain
  combination of strides and channel subset reads. #2108
* TIFF: Fix problems with JPEG compression in some cases. #2117
* TIFF: Fixed error where reading just a subset of channels, if that subset
  did not include the alpha channel but the image was "unassociated alpha",
  the attempt to automatically associate (i.e. "premultiply" the alpha) upon
  read would get bogus values because the alpha channel was not actually
  read. Now in this case it will not do the premultiplication. So if you are
  purposely reading RGB only from an RGBA file that is specifically
  "unassociated alpha", beware that you will not get the automatic
  premultiplication. #2122
* Python: define `__version__` for the module. #2096
* IBA::channel_append() previously always forced its result to be float, if
  it wasn't previously initialized. Now it uses the uaual type-merging
  logic, making the result the "widest" type of the inputs. #2095
* ImageSpec::find_attribute now will retrive "datawindow" and "displaywindow"
  (type int[4] for images int[6] for volumes) giving the OpenEXR-like bounds
  even though there is no such named metadata for OIIO (the results will
  assembled from x, y, width, height, etc.). #2110
* ImageCache/TextureSystem: more specific error message when tile reads
  appear to be due to the file having changed or been overwritten on disk
  since it was first opened. #2115
* oiiotool: New `-evaloff` and `-evalon` lets you disable and enable
  the expression substitution for regions of arguments (for example, if
  you have an input image filename that contains `{}` brace characters that
  you want interpreted literally, not evaluated as an expression). #2100
* oiiotool `--dumpdata` has more intelligible output for uint8 images. #2124
* maketx: the `-u` (update mode) is slightly less conservative now,
  no longer forcing a rebuild of the texture just because the file uses
  a different relative directory path than last time. #2109
* WebP: fix bug that gave totally incorrect image read for webp images that
  had a smaller width than height. #2120
* Developer goodies: string_view now adds an optional `pos` parameter to the
  `find_first_of`/`find_last_of` family of methods. #2114
* Dev goodies: Strutil::wordwrap() now lets you specify the separation
  characters more flexibly (rather than being hard-coded to spaces as
  separators). #2116



Release 2.0 (Dec 1, 2018) -- compared to 1.8.x
----------------------------------------------

New minimum dependencies:
* On Windows compiling with MSVS, the new minimum version is MSVS 2015.

Major new features and improvements:
* ImageInput and ImageOutput static create() and open() methods now return
  `unique_ptr` rather than raw pointers. #1934, #1945 (1.9.3).
* ImageInput improvements to thread safety and concurrency, including some
  new API calls (see "Public API changes" section below).
* ImageBufAlgo overhaul (both C++ and Python): Add IBA functions that
  return image results directly rather than passing ImageBuf references
  as parameters for output (the old kind of calls still exist, too, and
  have their uses). Also in C++, change all IBA functions that took raw
  pointers to per-channel colors into `span<>` for safety. #1961 (1.9.4)
* For some readers and writers, an "IOProxy" can be passed that customizes
  the I/O methods. An important use of this is to write an image "file"
  to memory or to read an image "file" from a memory, rather than disk.
  Currently, OpenEXR supports this for both reading and writing, and PNG
  supports it for writing. You specify a pointer to the proxy via the
  configuration option "oiio:ioproxy". #1931 (1.9.3)
* New Image Format support:
    * OpenVDB file read (as volume images or accessing via texture3d()).
      #2010,2018 (1.9.4)
    * "null" images -- null reader just returns black (or constant colored)
      pixels, null writer just returns. This can be used for benchmarking
      (to eliminate all actual file I/O time), "dry run" where you want to
      test without creating output files. #1778 (1.9.0), #2042 (1.9.4)
* TIFF I/O of multiple scanlines or tiles at once (or whole images, as is
  typical use case for oiiotool and maketx) is sped up by a large factor
  on modern multicore systems. We've seen 10x or more faster oiiotool
  performance for uint8 and uint16 TIFF files using "zip" (deflate)
  compression, on modern 12-16 core machines. #1853 (1.9.2)
* Major refactor of Exif metadata handling, including much more complete
  metadata support for RAW formats and support of camera "maker notes"
  for Canon cameras. #1774 (1.9.0)
* New `maketx` option `--bumpslopes` specifically for converting bump maps,
  saves additional channels containing slope distribution moments that can
  be used in shaders for "bump to roughness" calculations. #1810,#1913,2005
  (1.9.2), #2044 (1.9.4)
* An official FindOpenImageIO.cmake that we invite you to use in other
  cmake-based projects that needs to find OIIO. #2027 (1.9.4)

Public API changes:
* **Python binding overhaul**
  The Python bindings have been reimplemented with
  [`pybind11`](https://github.com/pybind/pybind11), no longer with Boost.Python.
  #1801 (1.9.1)
  In the process (partly due to what's easy or hard in pybind11, but partly
  just because it caused us to revisit the python APIs), there are some minor
  API changes, some of which are breaking! To wit:
    * All of the functions that are passed or return blocks of pixels
      (such as `ImageInput.read_image()`) now use Numpy `ndarray` objects
      indexed as `[y][x][channel]` (no longer using old-style Python
      `array.array` and flattened to 1D).
    * Specilized enum type `ImageInput.OpenMode` has been replaced by string
      parameters, so for example, old `ImageInput.open(filename, ImageInput.Create)`
      is now `ImageInput.open (filename, "Create")`
    * Any function that previously took a parameter of type `TypeDesc`
      or `TypeDesc.BASETYPE` now will accept a string that signifies the
      type. For example, `ImageBuf.set_write_format("float")` is now a
      synonym for `ImageBuf.set_write_format(oiio.TypeDesc(oiio.FLOAT))`.
    * For several color conversion functions, parameter names were changed
      from "from" to "fromspace" and "to" to "tospace" to avoid a clash with
      the Python reserved word `from`. #2084
* **ImageInput API changes for thread safety and statelessness** #1927 (1.9.2)
    * `seek_subimage()` no longer takes an `ImageSpec&`, to avoid the obligatory
       copy. (If the copy is desired, just call `spec()` to get it afterwards.)
    * All of the `read_*()` methods now have varieties that take arguments
      specifying the subimage and mip level. The `read_native_*()` methods
      supplied by ImageInput subclass implementations now ONLY come in the
      variety that takes a subimage and miplevel.
    * All of the `read_*()` calls that take subimage/miplevel explicitly are
      guaranteed to be stateless and thread-safe against each other (it's not
      necessary to call `seek_subimage` first, nor to have to lock a mutex to
      ensure that another thread doesn't change the subimage before you get a
      chance to call read). For back-compatibility, there are still versions
      that don't take subimage/miplevel, require a prior call to seek_subimge,
      and are thus not considered thread-safe.
    * New methods `spec(subimage,miplevel)` and `spec_dimensions(s,m)`
      let you retrieve a copy of the ImageSpec for a given subimage and
      MIP level (thread-safe, and without needing a prior `seek_subimage`)
      call. Note that to be stateless and thread-safe, these return a COPY
      of the spec, rather than the reference returned by the stateful
      `spec()` call that has no arguments and requires a prior `seek_subimage`.
      However, `spec_dimensions()` does not copy the channel names or the
      arbitrary metadata, and is thus very inexpensive if the only thing
      you need from the spec copy is the image dimensions and channel
      formats.
* **ImageInput and ImageOutput create/open changes**
    * The static `create()` and `open()` methods have been changed so that
      instead of returning an `ImageInput *` (or `ImageOutput *`) and
      requiring the caller to correctly manage that resource and eventually
      destroy it, now they return a `unique_ptr` that automatically deletes
      when it leaves scope. In the process we also clean up some edge cases
      on Windows where it was possible for ImageInput/ImageOutput to have
      been allocated in one DLL's heap but freed in a different DLL's heap,
      which could cause subtle heap corruption problems. #1934,#1945 (1.9.3).
* **ImageBuf**
    * New method `set_origin()` changes the pixel data window origin.
      #1949 (1.9.4)
    * Assignment (`operator=`) is now enabled for ImageBuf, both the copying
      and moving variety. Also, an explicit copy() method has been added
      that returns a full copy of the ImageBuf. #1952 (1.9.4)
    * write() method has had its arguments changed and now takes an optional
      TypeDesc that lets you specify a requested data type when writing the
      output file, rather than requiring a previous and separate call to
      set_write_format(). The old call signature of write() still exists,
      but it will be considered deprecated in the future. #1953 (1.9.4)
* **ImageBufAlgo**
    * In C++, functions that take raw pointers for per-channel constant
      values or results are deprecated, in favor of new versions that
      heavily rely on `span<>` to safely pass array references and their
      lengths. #1961 (1.9.4)
    * In both C++ and Python, every IBA function that takes a parameter
      giving an ImageBuf destination reference for results have an additional
      variant that directly returns an ImageBuf result. This makes much
      cleaner, more readable code, in cases where it's not necessary to
      write partial results into an existing IB. #1961 (1.9.4)
    * In C++, many IBA functions that came in multiple versions for whether
      certain parameters could be an image, a per-channel constant, or a
      single constant, have been replaced by a single version that takes
      a new parameter-passing helper class, `Image_or_Const` that will match
      against any of those choices. (No changes are necessary for calling
      programs, but it makes the header and documentation a lot simpler.)
      #1961 (1.9.4)
    * IBA compare(), computePixelStats(), and histogram() now directly
      return their result structures, intead of requiring the passing of
      a destination reference. #1961 (1.9.4)
    * New IBA::fit() resizes and image to just fit in the given size, but
      preserve its aspect ratio (padding with black as necessary). It's just
      like what `oiiotool --fit` has always done, but now you can call it
      directly from C++ or Python. #1993 (1.9.4)
    * New `contrast_remap()` allows flexible linear or sigmoidal contrast
      remapping. #2043 (1.9.4)
    * `ImageBufAlgo::colorconvert` and various `ocio` transformations have
      changed the default value of their `unpremult` parameter from `false`
      to `true`, reflecting the fact that we believe this is almost always
      the more correct choice. Also, if their input image is clearly marked
      as having unasociated alpha already, they will not bracket the color
      conversion with the requested unpremult/premult. #1864 (1.9.2)
    * Updated the OpenCV interoperability with new functions to_OpenCV (make
      an ImageBuf out of a cv::Mat) and from_OpenCV (fill in a cv::Mat with
      the contents of an ImageBuf). Deprecated the old from_IplImage and
      to_IplImage, which are very OpenCV-1.x-centric. (2.0.2)
* **ImageCache/TextureSystem:**
    * ImageCache and TextureSystem now have `close(filename)` and
      `close_all()` methods, which for one file or all files will close the
      files and release any open file handles (also unlocking write access
      to those files on Windows), but without invalidating anything it knows
      about the ImageSpec or any pixel tiles already read from the files, as
      would happen with a call to the much more drastic `invalidate()` or
      `invalidate_all()`. #1950 (1.9.4)
    * `TextureSystem::create()` has an additional optional argument that
      allows the caller to pass an existing app-owned custom ImageCache.
      #2019 (1.9.4)
    * New `TextureSystem::imagecache()` method returns a blind, non-owning
      pointer to the underlying ImageCache of that TS. #2019 (1.9.4)
    * ImageCache: extended add_tile() with an optional `copy` parameter
      (which defaults to `true`), which when set to `false` will make a tile
      that references an app buffer without allocating, copying, and owning
      the memory. In short, this makes it possible to reference existing
      memory holding an image array, as if it were a texture. #2012 (1.9.4)
    * `ImageCache::add_file()` extended with an optional `replace` parameter
      (default: false), that if true, will replace the tile and invalidate
      the old one. #2021 (1.9.4)
* **Changes to string formatting**: #2076 (2.0.1)
    * New `Strutil::sprintf()` and `ustring::sprintf()` functions are for
      printf-style formatted errors and warnings. You are encouraged to
      change your existing `format()` calls to `sprintf()`, since the
      original `format` may in a later version (2.1?) switch to Python-style
      formatting commands, but `sprintf` will continue to reliably use
      C printf style notation.
    * In ImageInput, ImageOutput, ImageBuf, and ErrorHandler, new `errorf()`
      and `warningf()` methods similarly provide printf-style formatted
      errors and warnings. The old `error()/warning()` calls will someday
      (maybe 2.1?) switch to Python-style formatting commands, but
      `errorf` will continue to reliably use C printf style notation.
* ColorConfig changes: ColorConfig methods now return shared pointers to
  `ColorProcessor` rather than raw pointers. It is therefore no longer
  required to make an explicit delete call. Created ColorProcessor objects
  are now internally cached, so asking for the same color transformation
  multiple times is no longer expensive. The ColorProcessor interface is
  now in `color.h` and can be directly used to perform transformations on
  individual colors (previously it was just an opaque pointer and could
  only be used to pass into certain IBA functions). The color space names
  "rgb" and "default" are now understood to be synonyms for the default
  "linear" color space. #1788 (1.9.0)
* Remove long-deprecated API calls:
    * ImageBuf::get_pixels/get_pixel_channels varieties deprecated since 1.6.
    * ImageBuf::set_deep_value_uint, deprecated since 1.7.
    * ImageBuf::deep_alloc, deprecated since 1.7.
    * ImageBufAlgo::colorconvert variety deprecated since 1.7.
    * ImageCache::clear, deprecated since 1.7.
    * ImageCache::add_tile variety deprecated since 1.6.
* ROI new methods: contains()  #1874, #1878 (1.9.2)
* `ImageBufAlgo::pixeladdr()` now takes an additional optional parameter,
  the channel number. #1880 (1.9.2)
* Global OIIO attribute "log_times" (which defaults to 0 but can be overridden
  by setting the `OPENIMAGEIO_LOG_TIMES` environment variable), when nonzero,
  instruments ImageBufAlgo functions to record the number of times they are
  called and how much time they take to execute. A report of these times
  can be retrieved as a string as the "timing_report" attribute, or it will
  be printed to stdout automatically if the value of log_times is 2 or more
  at the time that the application exits. #1885 (1.9.2)
* Moved the definition of `ROI` from `imagebuf.h` to `imageio.h` and make
  most of the methods `constexpr`. #1906 (1.9.2)
* Rename/move of `array_view` to `span`. Deprecated `array_view` and moved
  array_view.h contents to span.h. You should change `array_view<T>`
  to `span<T>` and `array_view<const T>` to `cspan<T>`. #1956,2062 (1.9.4)
* ustring: removed `operator int()` that allowed simple int casting such as:
  ```
      ustring u, v;
      if (u || !v) { ... }
  ```
  This was error-prone, neither std::string nor std::string_view had the
  equivalent, so we are removing it. The preferred idiom is:
  ```
      if (!u.empty() || v.empty()) { ... }
  ```

Performance improvements:
* ImageBufAlgo::computePixelStats is now multithreaded and should improve by
  a large factor when running on a machine with many cores. This is
  particularly noticable for maketx. #1852 (1.9.2)
* Color conversions are sped up by 50% for 4 channel float images, about
  30% for other combinations of channels or data formats. #1868 (1.9.2)
* ImageBuf::get_pixels() sped up by around 3x for the common case of the
  image being fully in memory (the slower path is now only used for
  ImageCache-based images). #1872 (1.9.2)
* ImageBufAlgo::copy() and crop() sped up for in-memory buffers, by about
  35-45% when copying between buffers of the same type, 2-4x when copying
  between buffers of different data types. #1877 (1.9.2)
* ImageBufAlgo::over() when both buffers are in-memory, float, 4-channels,
  sped up by about 2x. #1879 (1.9.2).
* ImageBufAlgo::fill() of a constant color sped up by 1.5-2.5x (depending
  on the data type involved). #1886 (1.9.2)

Fixes and feature enhancements:
* oiiotool
    * `--help` prints important usage tips that explain command parsing,
      syntax of optional modifiers, and the path to PDF docs. #1811 (1.9.2)
    * `--colormap` has new  maps "inferno", "magma", "plasma", "viridis",
      which are perceptually uniform, monotonically increasing luminance,
      look good converted to greyscale, and usable by people with color
      blindness. #1820 (1.9.2)
    * oiiotool no longer enables autotile by default. #1856 (1.9.2)
    * `--colorconvert`, `--tocolorspace`, and all of the `--ocio` commands
      now take an optional modifier `:unpremult=1` which causes the color
      conversion to be internally bracketed by unpremult/premult steps (if
      the image has alpha and is not already marked as having unassociated
      alpha). You should therefore prefer `--colorconvert:unpremult=1 from to`
      rather than the more complex `--unpremult --colorconvert from to -premult`.
      #1864 (1.9.2)
    * `--autocc` will also cause unpremult/premult to bracket any color
      transformations it does automatically for read and write (if the image
      has alpha and does not appear to already be unassociated). #1864 (1.9.2)
    * `--help` prints the name of the OCIO color config file. #1869 (1.9.2)
    * Frame sequence wildcard improvements: fix handling of negative frame
      numbers and ranges, also the `--frames` command line option is not
      enough to trigger a loop over those frame numbers, even if no other
      arguments appear to have wildcard structure. #1894 (1.8.10/1.9.2)
    * `--info -v` now prints metadata in sorted order, making it easier to
      spot the existance of particular metadata. #1982 (1.9.4)
    * `--no-autopremult` fixed, it wasn't working properly for cases that
      were read directly rather than backed by ImageCache. #1984 (1.9.4)
    * New `--contrast` allows for contrast remapping (linear or sigmoidal).
      #2043 (1.9.4)
    * Improved logic for propagating the pixel data format through
      multiple operations, especially for files with multiple subimages.
      #1769 (1.9.0/1.8.6)
    * Outputs are now written to temporary files, then atomically moved
      to the specified filename at the end. This makes it safe for oiiotool
      to "overwrite" a file (i.e. `oiiotool in.tif ... -o out.tif`) without
      problematic situations where the file is truncated or overwritten
      before the reading is complete. #1797 (1.8.7/1.9.1)
    * Fixed problem with reading `half` files where very small (denormalized)
      half values could get squashed to float 0.0 instead of having their
      values preserved, if certain old versions of libopenjpeg were being
      used (because they set a CPU flag strangely upon library load and then
      never changed it back, this is a libopenjpeg bug that has since been
      fixed). #2048 (2.0)
    * `-d chan=type` logic fixed for certain cases of specifying the data
      types of individual channels. #2061 (2.0beta2)
    * Expression evaluation: metadata names can now be enclosed in single
      or double quotes if they don't follow "C" identifier naming conventions.
      For example, `{TOP.'foo/bar'}` retrieves metadata called "foo/bar"
      rather than trying to retrieve "foo" and divide by bar. #2068 (2.0beta2)
    * Expression evaluation: When retrieving metadata, timecode data will be
      expressed properly as a string ("00:00:00:00"). #2068 (2.0beta2)
* ImageBufAlgo:
    * `color_map()` supports new maps "inferno", "magma", "plasma",
      "viridis". #1820 (1.9.2)
    * Across many functions, improve channel logic when combining an image
      with alpha with another image without alpha. #1827 (1.9.2)
    * `mad()` now takes an `img*color+img` variety. (Previously it
       supported `img*img+img` and `img*color+color`.) #1866 (1.9.2)
    * New `fit()` is like resize but fits inside a specified window size,
      while preserving the aspect ratio of the image appearance. #1993.
    * New `contrast_remap()` allows flexible linear or sigmoidal contrast
      remapping. #2043 (1.9.4)
    * `channel_append()` is no longer limited to requiring the two input
      images to have the same pixel data type. #2022 (1.9.4)
    * `isConstantColor()`, `isConstantChannel()`, and `isMonochrome()` have
      added an optional `threshold` parameter that allows you to compute
      whether the image is constant or monochrome within a non-zero
      tolerance (the default is still 0.0, meaning checking for an exact
      match). #2049 (2.0.0)
    * `IBA::ociodisplay()` has better behavior when its "fromspace" parameter
      is left blank -- instead of assuming "linear" (as a space name), it
      assumes it's whatever space in your OCIO color config has the "linear"
      role. #2083 (1.8.17/2.0.1)
* ImageBuf:
    * Bug fixed in IB::copy() of rare types. #1829 (1.9.2)
    * write() automatically tells the ImageCache to 'invalidate' the file
      being written, so cached images will not retain the prior version of
      the files. #1916 (1.9.2)
    * Bug fix to ImageBuf::contains_roi() method -- it erroneously always
      returned `true`. #1997 (1.8.14/1.9.4)
* ImageCache/TextureSystem/maketx:
    * Improved stats on how long we wait for ImageInput mutexes.
      #1779 (1.9.0/1.8.6)
    * Improved performance of IC/TS tile and file caches under heavy
      contention from many threads. #1780 (1.9.0)
    * Increased the default `max_tile_channels` limit from 5 to 6.
      #1803 (1.9.1)
    * maketx: improved image hashing to avoid some (extremely rare) possible
      hash collisions. #1819 (1.9.2)
    * IC/TS performance improvements by changing the underlying hash table
      implementation. #1823,1824,1825,1826,1830 (1.9.2)
    * texture()/texture3d(): when requesting a nonexistent "subimage",
      return the fill color, like we do when requesting nonexistent channels
      (rather than nondeterministically simply not filling in the result).
      #1917 (1.9.2)
    * Relying on some changes to the ImageInput API, there is now much less
      thread locking to protect the underlying ImageInputs, and this should
      improve texture and image cache performance when many threads need
      to read tiles from the same file. #1927 (1.9.2)
    * `get_image_info()`/`get_texture_info()` is now more flexible about
      retrieving arrays vs aggregates, in cases where the total number of
      elements is correct. #1968 (1.9.4)
    * Fix uninitialized read within the texture system (only affected
      statistics, never gave wrong texture results). #2000 (1.9.4)
    * texture3d() transforms lookup points from world into local space
      if the file has a "worldtolocal" metadata giving a 4x4 matrix. #2009
      (1.9.4)
    * Fix minor texture filtering bug where widely disparate "sblur" and
      "tblur" values could in some circumstances lead to incorrect texture
      filter estimation. #2052 (2.0.0)
    * `ImageCache::invalidate(filename)` did not properly invalidate the
      "fingerprint" is used to detect duplicate files. #2081 (1.8.17/2.0.1)
* iv:
    * Fix (especially on OSX) for various ways it has been broken since the
      shift to Qt5. #1946 (1.8.12, 1.9.4)
    * New optin `--no-autopremult` works like oiiotool, causes images with
      unassociated alpha to not be automatically premultiplied by alpha
      as they are read in. #1984 (1.9.4)
* All string->numeric parsing and numeric->string formatting is now
  locale-independent and always uses '.' as decimal marker. #1796 (1.9.0)
* Python Imagebuf.get_pixels and set_pixels bugs fixed, in the varieties
  that take an ROI to describe the region. #1802 (1.9.2)
* Python: Implement missing `ImageOutput.open()` call variety for declaring
  multiple subimages. #2074 (2.0.1)
* More robust parsing of XMP metadata for unknown metadata names.
  #1816 (1.9.2/1.8.7)
* Fix ImageSpec constructor from an ROI, display/"full" window did not get
  the right default origin. #1997 (1.8.14/1.9.4)
* ImageSpec::erase_attribute() fix bug where it got case-sensitivity of the
  search backwards when built using std::regex rather than boost::regex.
  #2003 (1.8.14/1.9.4)
* DPX:
    * Better catching of write errors, including filling the disk while in
      the process of writing a DPX file. #2072 (2.0.1)
* Field3d:
    * Prevent crashes when open fails. #1848 (1.9.2/1.8.8)
    * Fix potential mutex deadlock. #1972 (1.9.4)
* GIF:
    * Fix crash when reading GIF with comment extension but no comment data.
      #2001 (1.8.14/1.9.4)
* JPEG:
    * When writing, be robust to accidentally setting the "density" metadata
      to values larger than JPEG's 16 bit integer field will accommodate.
      #2002 (1.8.14/1.9.4)
    * Better detection and reporting of error conditions while reading
      corrupt JPEG files. #2073 (2.0.1)
* OpenEXR:
    * Gracefully detect and reject files with subsampled channels,
      which is a rarely-to-never-used OpenEXR feature that we don't support
      properly. #1849 (1.9.2/1.8.8)
    * Improved handling of UTF-8 filenames on Windows. #1941 (1.9.3, 1.8.12,
      1.7.19)
* PNG:
    * Fix redundant png_write_end call. #1910 (1.9.2)
* PSD:
    * Fix parse issue of layer mask data. #1777 (1.9.2)
* RAW:
    * Add "raw:HighlightMode" configuration hint to control libraw's
      handling of highlight mode processing. #1851
    * Important bug fix when dealing with rotated (and vertical) images,
      which were not being re-oriented properly and could get strangely
      scrambled. #1854 (1.9.2/1.8.9)
    * Major rewrite of the way makernotes and camera-specific metadata are
      handled, resulting in much more (and more accurate) reporting of
      camera metadata. #1985 (1.9.4)
    * The "oiio:ColorSpace" metadata is now set correctly when reading
      raw DSLR images. And we deprecate the old "raw:ColorSpace" metadata,
      which is useless. #2016 (1.9.4)
    * Add "raw:aber" configuration hint to control libraw's adjustments for
      chromatic aberration. This data is of type "float[2]", the first value
      is the scale factor for red, the second for blue, and both should be
      very close to 1.0. #2030 (1.9.4)
* TIFF:
    * Improve performance of TIFF scanline output. #1833 (1.9.2)
    * Bug fix: read_tile() and read_tiles() input of un-premultiplied tiles
      botched the "shape" of the tile data array. #1907 (1.9.2/1.8.10)
    * Improvement in speed of reading headers (by removing redundant call
      to TIFFSetDirectory). #1922 (1.9.2)
    * When config option "oiio:UnassociatedAlpha" is nonzero (or not set
      -- which is the default), therefore enabling automatic premultiplication
      by alpha for any unassociated alpha files, it will set the metadata
      "tiff:UnassociatedAlpha" to indicate that the original file was
      unassociated. #1984 (1.9.4)
    * Bug fixes for TIFF reads of images with unassociated alpha -- there
      were some edge cases where they pixels failed to automatically
      premultiply upon read. #2032 (1.9.4)
* zfile: more careful gzopen on Windows that could crash when given bogus
  filename. #1839,2070 (1.9.2/1.8.8/2.0.1)
* Windows fix: Safer thread pool destruction on. #2038 (1.9.4)

Build/test system improvements and platform ports:
* Fixes for Windows build. #1793, #1794 (1.9.0/1.8.6), #2025 (1.9.4)
* Fix build bug where if the makefile wrapper got `CODECOV=0`, it would
  force a "Debug" build (required for code coverage tests) even though code
  coverage is instructed to be off. (It would be fine if you didn't specify
  `CODECOV` at all.) #1792 (1.9.0/1.8.6)
* Build: Fix broken build when Freetype was not found or disabled. #1800
  (1.8.6/1.9.1)
* Build: Boost.Python is no longer a dependency, but `pybind11` is. If
  not found on the system, it will be automatically downloaded. #1801, #2031
  (1.9.1)
* Time for a multi-core build of OIIO is reduced by 40% by refactoring some
  extra big modules into more bite-sized pieces. #1806 (1.9.2)
* testtex:
    * Make the "thread workout" cases all honor `--handle`. #1778 (1.9.0)
    * Only prints detailed stats if `-v` is used, and new option
      `--invalidate` will invalidate the cache when starting each
      threadtimes trial. #1828 (1.9.2)
    * New `--anisoratio` lets you choose anisotropic shape for thread
      working tests, and make thread_workout samples twice as big to be more
      typical by interpolating mip levels. #1840 (1.9.2)
    * TextureSystem stats are printed as well as ImageCache. #1840 (1.9.2)
* iv no longer requires GLEW, using QOpenGLFunctions instead. #1840 (1.9.2)
* DICOM: Fix dcmtk build errors on some platforms. Also, the minimum dcmtk
  version we suport is 3.6.1. #1843 (1.9.2/1.8.8)
* Build fixes for Hurd OS. #1850 (1.9.2/1.8.8)
* Clean up leak sanitizer errors. #1855 (1.9.2)
* On Unix/Linux, add explicit DL library dependency to libOpenImageIO.so
  itself instead of only to the binaries and test utilities.
  #1860 (1.9.2/1.8.8)
* The build now bundles a sample OCIO config in testsuite/common so that we
  can do OCIO-based unit tests. #1870 (1.9.2)
* Properly find newer openjpeg 2.3. #1871 (1.9.2)
* Fix testsuite to be Python 2/3 agnostic. #1891 (1.9.2)
* Removed `USE_PYTHON3` build flag, which didn't do anything. #1891 (1.9.2)
* The `PYTHON_VERSION` build variable is now better at selecting among
  several installed versions of Python, and all the tests should work fine
  with Python 3.x now. #2015 (1.9.4)
* Remove some lingering support for MSVS < 2013 (which we haven't advertised
  as working anyway). #1887 (1.9.2)
* Windows/MSVC build fix: use the `/bigobj` option on some large modules
  that need it. #1900, #1902 (1.8.10/1.9.2)
* Add up-to-date Nuke versions to FindNuke.cmake. #1920 (1.8.11, 1.9.2)
* Allow building against ffmpeg 4.0. #1926,#1936 (1.8.11, 1.9.2)
* Disable SSE for 32 bit Windows -- problematic build issues.
  #1933 (1.9.3, 1.8.12, 1.7.19)
* Fixes to the `EMBEDPLUGINS=0` build case, which had at some point stopped
  working properly. #1942 (1.9.3)
* Improvements in finding the location of OpenJPEG with Macports.
  #1948 (1.8.12, 1.9.4)
* Improvement finding libraw properly on Windows. #1959 (1.9.4)
* Fix warnings to allow clean gcc8 builds. #1974 (1.9.4)
* Make sure we build properly for C++17. (1.9.4)
* Check properly for minimal FFMpeg version (2.6). #1981 (1.9.4)
* New build option GLIBCXX_USE_CXX11_ABI, when set to 0 will force the old
  gcc string ABI (even gcc 7+ where the new ABI is the default), and if set
  to 1 will force the new gcc string ABI (on gcc 5-6, where old ABI is the
  default). If not set at all, it will respect the default choice for that
  compiler. #1980 (1.9.4)
* TravisCI builds now use an abbreviated test matrix for most ordinary
  pushes of working branches, but the full test matrix for PRs or pushes
  to "master" or "RB" branches. #1983 (1.9.4)
* Support compilation by clang 7.0. #1995 (1.9.4)
* Support for building against OpenEXR 2.3. #2007 (1.9.4)
* Use OpenEXR pkgconfig if available. #2008 (1.9.4)
* Allow builds outside the source tree to pass testsuite. Defaults to
  finding test image directories such as oiio-images, openexr-images, and
  libtiffpic in the usual ".." from the main OIIO source directory, but now
  it can be overridden with the CMake variable `OIIO_TESTSUITE_IMAGEDIR`.
  #2026 (1.9.4)
* Remove stale python examples from `src/python`. They were untested,
  undocumented, and probably no longer worked against the current APIs.
  #2036 (1.9.4)
* Fixes for Windows when making Unicode builds, and fix Plugin::dlopen
  on Windows to properly support UTF-8 filenames. #1454 (2.0.1)
* Support added for OpenCV 4.0. (2.0.1)

Developer goodies / internals:
* **Formatting with clang-format**: All submissions are expected to be
  formatted using our standard clang-format rules. Please run
  `make clang-format` prior to submitting code. The TravisCI tests include
  one entry just to check that the formatting conforms, and will fail if it
  doesn't, printing the diffs that would bring it to proper formatting.
  (Note: for small changes, if you don't have clang-format locally, it's ok
  to submit, then use the diffs from the failures to fix it by hand and
  resubmit and update.) #2059,2064,2065,2067,2069.
* argparse.h:
    * Add pre- and post-option help printing callbacks. #1811 (1.9.2)
    * Changed to PIMPL to hide implementation from the public headers.
      Also modernized internals, no raw new/delete. #1858 (1.9.2)
* array_view.h:
    * Added begin(), end(), cbegin(), cend() methods, and new
      constructors from pointer pairs and from std::array. (1.9.0/1.8.6)
    * Deprecated, moved contents to span.h. You should change `array_view<T>`
      to `span<T>` and `array_view<const T>` to `cspan<T>`. #1956 (1.9.4)
* color.h: add guards to make this header safe for Cuda compilation.
  #1905 (1.9.2/1.8.10)
* filesystem.h:
    * IOProxy classes that can abstract file operations for custom I/O
      substitutions. #1931 (1.9.3)
    * Proper UTF-8 filenames for unique_path() and temp_directory(), and
      general UTF-8 cleanup/simplification. #1940 (1.9.3, 1.8.12, 1.7.19)
    * Remove extraneous calls to exists() that were doubling the number
      of stat syscalls. #2385 (2.1.8)
* fmath.h:
    * Now defines preprocessor symbol `OIIO_FMATH_H` so other files can
      easily detect if it has been included. (1.9.0/1.8.6)
    * Modify to allow Cuda compilation/use of this header. #1888,#1896
      (1.9.2/1.8.10)
    * Improve numeric approximation of fast_atan() and fast_atan2().
      #1943 (1.9.3)
    * fast_cbrt() is a fast approximate cube root (maximum error 8e-14,
      about 3 times faster than pow computes cube roots). #1955 (1.9.4)
* function_view.h: Overhauled fixed with an alternate implementation
  borrowed from LLVM. (1.9.4)
* hash.h: add guards to make this header safe for Cuda compilation.
  #1905 (1.9.2/1.8.10)
* imageio.h: `convert_image()` and `parallel_convert_image` have been
  simplified to remove optional `alpha_channel` and `z_channel` parameters
  that were never actually used. The old versions are still present but
  are deprecated. #2088 (2.0.1)
* parallel.h:
    * `parallel_options` passed to many functions. #1807 (1.9.2)
    * More careful avoidance of threads not recursively using the thread
      pool (which could lead to deadlocks). #1807 (1.9.2)
    * Internals refactor of task_set #1883 (1.9.2).
    * Make the thread pool better behaved in times if pool congestion -- if
      there are already way too many items in the task queue, the caller may
      do the work itself rather than add to the end and have to wait too
      long to get results. #1884 (1.9.2)
* paramlist.h:
    * ParamValue class has added get_int_indexed() and get_float_indexed()
      methods. #1773 (1.9.0/1.8.6)
    * ParamValue restructured to allow additional common data types to store
      internally rather than requre an allocation. #1812 (1.9.2)
    * New ParamList convenience methods: remove(), constains(),
      add_or_replace(). #1813 (1.9.2)
* platform.h:
    * New OIIO_FALLTHROUGH and OIIO_NODISCARD macros, and renamed
      OIIO_UNUSED_OK to OIIO_MAYBE_UNUSED (to match C++17 naming). #2041
* simd.h:
    * Fixed build break when AVX512VL is enabled. #1781 (1.9.0/1.8.6)
    * Minor fixes especially for avx512. #1846 (1.9.2/1.8.8) #1873,#1893
      (1.9.2)
* span.h:
    * Used to be array_view. Now it's `span<>` and `span_strided`. Also,
      `cspan<T>` is a handy alias for `span<const T>`. #1956 (1.9.4)
    * Added begin(), end(), cbegin(), cend() methods, and new
      constructors from pointer pairs and from std::array. (1.9.0/1.8.6)
    * Added `==` and `!=` to span and span_strided. #2037 (1.9.4)
* strutil.h:
    * All string->numeric parsing and numeric->string formatting is now
      locale-independent and always uses '.' as decimal marker. #1796 (1.9.0)
    * New `Strutil::stof()`, `stoi()`, `stoui()`, `stod()` functions for
      easy parsing of strings to numbers. Also tests `Strutil::string_is_int()`
      and `string_is_float()`. #1796 (1.9.0)
    * New `to_string<>` utility template. #1814 (1.9.2)
    * Fix to strtof, strtod for non-C locales. #1918 (1.8.11, 1.9.2)
    * New `iless()` is case-insensitive locale-independent string_view
      ordering comparison. Also added StringIEqual, StringLess, StringILess
      functors. (1.9.4)
    * `join()` is now a template that can act on any iterable container of
      objects that allow stream output. #2033 (1.9.4)
    * New `splits()`/`splitsv()` that direction returns a vector of
      std::string or string_view, respectively. #2033 (1.9.4)
    * A second version of `extract_from_list_string` that directly returns
      a std::vector (instead of being passed as a param). #2033 (1.9.4)
    * `parse_string` now accepts single quotes as well as double quotes
      to enclose a quoted string. #2066 (2.0beta2)
    * Fix Strutil::vsnprintf detection of encoding errors on Windows. #2082
      (1.8.17/2.0.1)
    * `parse_string()` - fix bugs that would fail for escaped quotes within
      the string. #2386 (2.1.8)
* thread.h:
    * Reimplementaiton of `spin_rw_mutex` has much better performance when
      many threads are accessing at once, especially if most of them are
      reader threads. #1787 (1.9.0)
    * task_set: add wait_for_task() method that waits for just one task in
      the set to finish (versus wait() that waits for all). #1847 (1.9.2)
    * Fix rare crash in thread pool when lowering the number of threads.
      #2013 (1.9.4/1.8.15)
* unittest.h:
    * Made references to Strutil fully qualified in OIIO namespace, so that
      `unittest.h` can be more easily used outside of the OIIO codebase.
      #1791 (1.9.0)
    * `OIIO_CHECK_EQUAL_APPROX` - fix namespace ambiguity. #1998 (1.9.4)
    * `OIIO_CHECK_EQUAL` now can compare two `std::vector`s. #2033 (1.9.4)
    * Make unit test errors respect whether stdout is a terminal when
      deciding whether to print in color. #2045
* Extensive use of C++11 `final` and `override` decorators of virtual
  methods in our internals, especially ImageInput and ImageOutput.
  #1904 (1.9.2)

Notable documentation changes:
* A new THIRD-PARTY.md file reproduces the full open source licenses
  of all code that we distribute with OIIO, incorporate, or derive code from.
  They are have very similar license terms to the main OIIO license
  ("New BSD") -- MIT, Mozilla, Apache 2.0, or public domain. OIIO does not
  use any GPL or other "viral" licenesed code that would change license
  terms of any code that didn't come directly from those packages.
* The CHANGES.md file was getting truly enormous, so we have split the
  release notes from the 0.x and 1.x releases into separate files found
  in src/doc. So CHANGES.md only documents 2.0 and beyond.

--------------

For older release notes, see:
* [CHANGES-0.x](https://github.com/OpenImageIO/oiio/blob/master/src/doc/CHANGES-0.x.md).
* [CHANGES-1.x](https://github.com/OpenImageIO/oiio/blob/master/src/doc/CHANGES-1.x.md).
