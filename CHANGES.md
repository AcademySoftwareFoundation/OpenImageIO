Release 1.9 (in progress) -- compared to 1.8.x
----------------------------------------------
New minimum dependencies:

Major new features and improvements:
* Major refactor of Exif metadata handling, including much more complete
  metadata support for RAW formats and support of camera "maker notes"
  for Canon cameras. #1774 (1.9.0)
* New "null" I/O plugin -- null reader just returns black (or constant
  colored) pixels, null writer just returns. This can be used for
  benchmarking (to eliminate all actual file I/O time), "dry run" where you
  want to test without creating output files. #1778 (1.9.0)
* New `maketx` option `--bumpslopes` specifically for converting bump maps,
  saves additional channels containing slope distribution moments that can
  be used in shaders for "bump to roughness" calculations. #1810 (1.9.2)

Public API changes:
* **Python binding overhaul**
  The Python bindings have been reimplemented with `pybind11`
  (https://github.com/pybind/pybind11), no longer with Boost.Python.
  #1801 (1.9.1)
  In the process (partly due to what's easy or hard in pybind11, but partly
  just because it caused us to revisit the python APIs), there are some minor
  API changes, some of which are breaking! To wit:
    * All of the functions that are passed or return blocks of pixels
      (such as `ImageInput.read_image()`) now use Numpy `ndarray` objects
      indexed as `[y][x][channel]` (no longer using old-style Python
      `array.array` and flattened to 1D).
    * Specilized enum type `ImageInput.OpenMode` has been replaced by string
      parameters, so for example, old `ImageInput.open (filename, ImageInput.Create)`
      is now `ImageInput.open (filename, "Create")`
    * Any function that previously took a parameter of type `TypeDesc`
      or `TypeDesc.BASETYPE` now will accept a string that signifies the
      type. For example, `ImageBuf.set_write_format("float")` is now a
      synonym for `ImageBuf.set_write_format(oiio.TypeDesc(oiio.FLOAT))`.
* ColorConfig changes: ColorConfig methods now return shared pointers to
  `ColorProcessor`s rather than raw pointers. It is therefore no longer
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

Fixes, minor enhancements, and performance improvements:
* oiiotool
    * Improved logic for propagating the pixel data format through
      multiple operations, especially for files with multiple subimages.
      #1769 (1.9.0/1.8.6)
    * Outputs are now written to temporary files, then atomically moved
      to the specified filename at the end. This makes it safe for oiiotool
      to "overwrite" a file (i.e. `oiiotool in.tif ... -o out.tif`) without
      problematic situations where the file is truncated or overwritten
      before the reading is complete. #1797 (1.8.7/1.9.1)
    * `--help` prints important usage tips that explain command parsing,
      syntax of optional modifiers, and the path to PDF docs. #1811 (1.9.2)
    * `--colormap` has new  maps "inferno", "magma", "plasma", "viridis",
       which are perceptually uniform, monotonically increasing luminance,
       look good converted to greyscale, and usable by people with color
       blindness. #1820 (1.9.2)
    * oiiotool no longer enables autotile by default. #1856 (1.9.2)
* ImageBufAlgo:
    * `color_map()` new  maps "inferno", "magma", "plasma", "viridis".
       #1820 (1.9.2)
    * Across many functions, improve channel logic when combining an image
      with alpha with another image without alpha. #1827 (1.9.2)
* ImageBuf:
    * Bug fixed in IB::copy() of rare types. #1829 (1.9.2)
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
* All string->numeric parsing and numeric->string formatting is now
  locale-independent and always uses '.' as decimal marker. #1796 (1.9.0)
* Python Imagebuf.get_pixels and set_pixels bugs fixed, in the varieties
  that take an ROI to describe the region. #1802 (1.9.2)
* More robust parsing of XMP metadata for unknown metadata names.
  #1816 (1.9.2/1.8.7)
* All string->numeric parsing and numeric->string formatting is now
  locale-independent and always uses '.' as decimal marker. #1796 (1.9.0)
* Field3d: Prevent crashes when open fails. #1848 (1.9.2/1.8.8)
* OpenEXR: gracefully detect and reject files with subsampled channels,
  which is a rarely-to-never-used OpenEXR feature that we don't support
  properly. #1849 (1.9.2/1.8.8)
* PSD:
    * Fix parse issue of layer mask data. #1777 (1.9.2)
* RAW: Add "raw:HighlightMode" configuration hint to control libraw's
  handling of highlight mode processing. #1851
* TIFF:
    * Improve performance of TIFF scanline output. #1833 (1.9.2)
* zfile: more careful gzopen on Windows that could crash when given bogus
  filename. #1839 (1.9.2/1.8.8)

Build/test system improvements:
* Fixes for Windows build. #1793, #1794 (1.9.0/1.8.6)
* Fix build bug where if the makefile wrapper got `CODECOV=0`, it would
  force a "Debug" build (required for code coverage tests) even though code
  coverage is instructed to be off. (It would be fine if you didn't specify
  `CODECOV` at all.) #1792 (1.9.0/1.8.6)
* Build: Fix broken build when Freetype was not found or disabled. #1800
  (1.8.6/1.9.1)
* Build: Boost.Python is no longer a dependency, but `pybind11` is. If
  not found on the system, it will be automatically downloaded. #1801 (1.9.1)
* Time for a multi-core build of OIIO is reduced by ~40% by refactoring some
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

Developer goodies / internals:
* argparse.h: Add pre- and post-option help printing callbacks. #1811 (1.9.2)
* array_view.h: added begin(), end(), cbegin(), cend() methods, and new
  constructors from pointer pairs and from std::array. (1.9.0/1.8.6)
* fmath.h: Now defines preprocessor symbol `OIIO_FMATH_H` so other files can
  easily detect if it has been included. (1.9.0/1.8.6)
* parallel.h:
    * `parallel_options` passed to many functions. #1807 (1.9.2)
    * More careful avoidance of threads not recursively using the thread
      pool (which could lead to deadlocks). #1807 (1.9.2)
* paramlist.h:
    * ParamValue class has added get_int_indexed() and get_float_indexed()
      methods. #1773 (1.9.0/1.8.6)
    * ParamValue restructured to allow additional common data types to store
      internally rather than requre an allocation. #1812 (1.9.2)
    * New ParamList convenience methods: remove(), constains(),
      add_or_replace(). #1813 (1.9.2)
* simd.h:
    * Fixed build break when AVX512VL is enabled. #1781 (1.9.0/1.8.6)
    * Minor fixes especially for avx512. #1846 (1.9.2/1.8.8)
* string.h:
    * All string->numeric parsing and numeric->string formatting is now
      locale-independent and always uses '.' as decimal marker. #1796 (1.9.0)
    * New `Strutil::stof()`, `stoi()`, `stoui()`, `stod()` functions for
      easy parsing of strings to numbers. Also tests `Strutil::string_is_int()`
      and `string_is_float()`. #1796 (1.9.0)
    * New `to_string<>` utility template. #1814 (1.9.2)
* thread.h:
    * Reimplementaiton of `spin_rw_mutex` has much better performance when
      many threads are accessing at once, especially if most of them are
      reader threads. #1787 (1.9.0)
    * task_set: add wait_for_task() method that waits for just one task in
      the set to finish (versus wait() that waits for all). #1847 (1.9.2)
* unittest.h: Made references to Strutil fully qualified in OIIO namespace,
  so that `unittest.h` can be more easily used outside of the OIIO codebase.
  #1791 (1.9.0)



Release 1.8.8 (1 Feb 2018) -- compared to 1.8.7
-------------------------------------------------
* OpenEXR: gracefully detect and reject files with subsampled channels,
  which is a rarely-to-never-used OpenEXR feature that we don't support
  properly. #1849
* Field3d: Prevent crashes when open fails. #1848
* RAW: Add "raw:HighlightMode" configuration hint to control libraw's
  handling of highlight mode processing. #1851
* zfile: more careful gzopen on Windows that could crash when given bogus
  filename. #1839
* DICOM: Fix dcmtk build errors on some platforms. Also, the minimum dcmtk
  version we suport is 3.6.1. #1843
* simd.h: Minor fixes especially for avx512. #1846
* iv: Drop GLEW and obsolete GL stuff from iv in favor of QOpenGLFunctions,
  and fix broken pixelview text rendering. #1834
* On Unix/Linux, add explicit DL library dependency to libOpenImageIO.so
  itself instead of only to the binaries and test utilities. #1860
* Build fixes for Hurd OS. #1850

Release 1.8.7 (1 Jan 2018) -- compared to 1.8.6
-------------------------------------------------
* All string->numeric parsing and numeric->string formatting is now
  locale-independent and always uses '.' as decimal marker. #1796
* oiiotool outputs are now written to temporary files, then atomically moved
  to the specified filename at the end. This makes it safe for oiiotool
  to "overwrite" a file (i.e. `oiiotool in.tif ... -o out.tif`) without
  problematic situations where the file is truncated or overwritten
  before the reading is complete. #1797
* Python bindings for ImageBuf.get_pixels and set_pixels fixed some bugs
  when passed an ROI without a channel range specified. #1802
* More robust parsing of XMP metadata for unknown metadata names. #1816
* strutil.h now includes a to_string<> utility template. #1814

Release 1.8.6 (1 Nov 2017) -- compared to 1.8.5
-------------------------------------------------
* oiiotool: Improved logic for propagating the pixel data format through
  multiple operations, especially for files with multiple subimages.
  #1769
* ImageCache/TextureSystem: improved stats on how long we wait for
  ImageInput mutexes. #1779
* Build: Fix build bug where if the makefile wrapper got CODECOV=0, it would
  force a "Debug" build (required for code coverage tests) even though code
  coverage is instructed to be off. (It would be fine if you didn't specify
  CODECOV at all.) #1792
* Build: minor fixes for Windows build. #1793, #1794
* Developers: The ParamValue class has added get_int_indexed() and
  get_float_indexed() methods. #1773
* Developers: array_view added begin(), end(), cbegin(), cend() methods,
  and new constructors from pointer pairs and from std::array.
* Developers: Fixed build break in simd.h when AVX512VL is enabled. #1781
* Developers: fmath.h now defined OIIO_FMATH_H so other files can easily
  detect if it has been included.
* Build: Fix broken build when Freetype was not found or disabled. #1800
* Python: fixed missing exposure of RATIONAL enum value. #1799


Release 1.8 (1.8.5 - beta) -- compared to 1.7.x
----------------------------------------------
New minimum dependencies:
 * **C++11** (should also build with C++14 and C++17)
 * **Compilers**: gcc 4.8.2 - gcc 7, clang 3.3 - 5.0, or MSVS 2013 - 2017
 * **Boost >= 1.53** (tested up through 1.65)
 * **CMake >= 3.2.2** (tested up through 3.9)
 * **OpenEXR >= 2.0** (recommended: 2.2)
 * (optional) **Qt >= 5.6**
 * (optional) **Python >= 2.7** (3.x is also ok)

**Changes to install layout**: fonts now get installed to
  `prefix/share/fonts/OpenImageIO`, OIIO docs now get installed to
  `prefix/share/doc/OpenImageIO`, and the Python module gets installed to
  `prefix/lib/pythonMAJ.MIN/site-packages`. #1747 #1760 (1.8.5)

Major new features and improvements:
* New oiiotool features:
   * `--info:format=xml` format option requests what format the info
      is printed. Current choices: `"text"`, `"xml"`. #1504 (1.8.0)
   * `--info:verbose=1` verbose option make file info print full metadata,
     but without needing to make other oiiotool operations verbose as would
     happen with `--info -v`. #1504 (1.8.0)
   * `--colormap` applies a color map based on the input values; the
     map can be one of several named ones, or given explicitly with
     numerical values. #1552 (1.8.1)
   * `-i:type=...` lets you override the internal buffer type that will be
     used for an input image. #1541 (1.8.1)
   * `-i:ch=a,...` lets you restrict the input to only the listed channels.
     This is semantically equivalent to following the input with a `--ch`
     command, but by integrating into the input itself, it can sometimes
     avoid using memory and I/O for unneeded channels. #1541 (1.8.1)
   * `--echo STRING` prints the string to the console. This can contain
     expressions! So you can do something like
         oiiotool file.exr -echo "Size is {TOP.width}x{TOP.height}"
     #1633 (1.8.3)
   * `--eraseattrib REGEX` erases all metadata attributes from the top image
     whose names match the regular expression. #1638 (1.8.3)
   * `--text` takes new optional modifiers: `xalign=` (left, right, center)
     and `yalign=` (baseline, top, bottom, center) to control how the text
     is aligned to the (x,y) position specified, `shadow=` (default = 0)
     than when nonzero controls the width of a "drop shadow" that makes the
     text clearer when rendered on a background image of similar color.
     #1646 (1.8.3)
   * `--deepholdout` culls all samples that are farther away than the
     opaque depth of a second holdout image. #1691 (1.8.4)
* New ImageBufAlgo functionality:
   * `color_map()` applies a color map based on the input values; the
     map can be one of several named ones, or given explicitly with
     numerical values. #1552 (1.8.1)
   * Added implementation of ImageBufAlgo::to_IplImage(). #1461 (1.7.9/1.8.1)
   * `render_text()` has added parameters controlling text alignment and
     drop shadows. #1646 (1.8.3)
   * `deep_holdout()` culls all samples that are farther away than the
     opaque depth of a second holdout image. #1691 (1.8.4)
* DICOM file format support (currently read-only). DICOM is the standard for
  medical imaging data. Will only build if dependency "dcmtk" is found at
  build time. #1534 (1.8.1)
* Experimental: The TextureSystem API has been extended to support batches
  of texture lookups in a SIMD fashion. At present, only the new API was
  added, a full implementation is in progress and may not be forthcoming
  or reliable until 1.9. Even the API is experimental, and may change for
  future releases. #1733 (1.8.5)

Public API changes:
* TypeDesc:
   * Rational support: new 'semantic' hint RATIONAL and TypeDesc::Rational.
     A rational is an int of aggregate VEC2 and hint RATIONAL, and should
     be interpreted as val[0]/val[1]. #1698 (1.8.5)
   * Added OIIO-scoped `static constexpr` versions of preconstructed
     TypeDescs (e.g., `TypeFloat`). We are deprecating the ones that were
     static data members of TypeDesc (e.g., TypeDesc::TypeFloat), they will
     be removed in some future release. (1.8.5)
* ImageSpec:
   * New `ImageSpec::serialize()` returns a string with a serialized version
     of the contents of the ImageSpec. It may be text (human readable, like
     is printed by `oiiotool -info -v`) or XML. #1504 (1.8.0)
   * `ImageSpec` has a new constructor that accepts a `ROI` for image
     dimensions and channels. #1646 (1.8.3)
   * New `ImageSpec::channelspec()` retrieves the index of a named channel.
     #1691 (1.8.4)
   * New `ImageSpec::channelname(int)` safely retrieves the index of a named
     channel. #1706 (1.8.5)
* ColorConig::createLookTransform() and createDisplayTransform() have been
  extended to allow multiple key/value context pairs, by making them
  comma-separated lists. The createColorProcessor() method has also been
  extended to take context key/value pairs. #1542 (1.7.8, 1.8.0)
* `ImageBuf::read()` now has a variety that takes a channel range, allowing
  you to populate an ImageBuf with a subset of channels in a file,
  potentially saving memory and I/O for the unneeded channels. #1541 (1.8.1)
* Python: Fix unimplemented ImageBufAlgo.computePixelStats. #1596
  (1.8.2/1.7.11)
* imageio.h: Fix incorrect declaration of declare_imageio_format().
  #1609 (1.8.2/1.7.11)
* `ImageBuf::wrap_mode_from_string()` converts string wrap mode names
  (such as "black") into `ImageBuf::WrapMode` enum values. #1615 (1.8.3)
* New `OIIO::getattribute()` queries:
   * `"input_format_list"` and `"output_format_list"` return comma-separated
     lists of all formats that support input and output, respectively.
     #1577 (1.8.3)
   * `"oiio:simd"` returns a comma-separated list of SIMD capabilities that
      were enabled at build time, and `"hw:simd"` returns the list of
      capabilities available on the currently running hardware. #1719 (1.8.5)
* `ImageBufAlgo::render_text()` API call has been overhauled, in addition
  to new alignment and shadow aprameters, the color has changed from a raw
  pointer to an `array_view<const float>` for better memory safety. Also,
  it is now valid for the destination image to be uninitialized, in which
  case it will be initialized to be just big enough for the text.
  #1646 (1.8.3)
* DeepData:
   * New `DeepData::opaque_z()` returns the depth value at which a pixel
     becomes fully opaque. #1691 (1.8.4)
   * New `DeepData::initialized()` and `allocated()` return whether the
     DD is initialized and allocated, respectively. #1691 (1.8.4)
   * `DeepData::split()` has been changed to return a `bool` indicating
     whether any split occurred. #1691 (1.8.4)
* Remove some long-deprecated varieties of `ImageBufAlgo::colorconvert()`,
  `ociolook()`, and `ociodisplay()`. #1695 (1.8.4)
* TypeDesc now allows specification of "rational" values, using a vec2i
  (aggregate 2-vector of int) with a semantic hint of RATIONAL, i.e.,
  `TypeDesc(INT, VEC2, RATIONAL)`, which is also aliased as
  `TypeDesc::TypeRational`. The value is understood to be val[0]/val[1].
  #1698 (1.8.5)
* ParamValueList::get_float will automatically convert rational values to
  float. #1698 (1.8.5)
* The standard metadata "FramesPerSecond" has had its definition changed
  from `float` to `rational`. Retrieving it as `float` should still work
  as always. But apps and plugins that wish to treat it as a true rational
  with no loss of precision are able to do so. This is only known to
  directly affect the OpenEXR, GIF, and FFMPEG metadata. #1698 (1.8.5)

Fixes, minor enhancements, and performance improvements:
* oiiotool:
   * `--chappend` resolves redundant channel names by using the subimage
     name, if available. #1498 (1.8.0/1.7.8)
   * `--mosaic` now gracefully handles the case of not having enough
     images to completely fill the MxN matrix, with "left over" slots
     black. #1501 (1.8.0/1.7.8)
   * When command line arguments fail to parse properly, `oiiotool` will
     exit with a non-zero shell status. #1540 (1.8.0)
   * `--colorconvert`, `--ociodisplay`, and `--ociolook` can now take
     multiple context key/value pairs (by allowing `key=` and `value=`
     optional paramters be comma-separated lists). #1504 (1.8.0)
   * Handle 'oiiotool --colorconvert X X' (transform from and to spaces that
     are the same) without considering it an error. #1550 (1.8.0/1.7.8)
   * Expression substitution now recognizes the following new metadata
     names: MINCOLOR, MAXCOLOR, AVGCOLOR. An example use is to stretch
     the value range of an image to fill the full [0-1] range:
        oiiotool in.exr -subc {TOP.MINCOLOR} -divc {TOP.MAXCOLOR} -o out.exr
     #1553 (1.8.1)
   * `--fit:exact=1` use of the new `exact=1' option will perform the resize
     to preserve exact aspect ratio and centering of the fit image to true
     sub-pixel precision (at the possible risk of slight blurring,
     especially of the edges), whereas the default (`exact=0`) will keep
     the image sharper but round the size and offset to the nearest whole
     pixel values. (1.8.1).
   * `-i:type=...` optional modifier to `-i` forces a read of the input
     file into a particular data type. #1541 (1.8.1)
   * `-i:ch=...` optional modifier to `-i` specifies that only certain named
     channels should be read from the file. Under extreme circumstances
     when only a small subset of channels is needed from an image file with
     many channels, this may improve speed and I/O. #1541 (1.8.1)
   * Improved logic governing output data formats: non-cached inputs didn't
     set default output data format correctly, and per-channel output
     formats updated defaults when they shouldn't have. #1541 (1.8.1)
   * `--diff` : in addition to the pixel coordinates and differences of the
      biggest differing pixel, it now also prints the full values of all
      channels of that pixel for both images. #1570 (1.8.1)
   * `oiiotool -d` giving per-channel formats is no longer confused by
     channel renaming with `--chnames`. #1563 (1.8.1/1.7.9)
   * `--debug` nor prints the total runtime and peak memory use after each
     individual op. #1583 (1.8.1)
   * `-iconfig` not in all cases correctly propagate the input config
     attribute to the file read. #1605 (1.8.2/1.7.11)
   * Fixed `--crop`: it did not honor the `-a` flag to apply the crop to
     all subimages. #1613 (1.8.2/1.7.11)
   * In the case of runtime errors, `oiiotool` now echoes the entire command
     line. This is helpful for debugging mangled oiiotool command lines
     assembled by scripts. #1614 (1.8.3)
   * Improved error reporting of file open errors when -iconfig is used.
     #1626 (1.8.3/1.7.13)
   * Expression evaluation now substitutes `FRAME_NUMBER` with the numeric
     frame number for frame sequence wildcards, and `FRAME_NUMBER_PAD`
     with the frame number 0-padded to the number of total digits specified
     by the command line `--framepadding` argument. #1648 (1.8.3)
   * `--resize` and `--resample` now have more intuitive behavior for images
     where the display and pixel data windows are not the same, especially if
     the data window had a nonzero origin (such as with crop or overscan).
     #1667 (1.8.4/1.7.14)
   * `--resample` has been extended to work for "deep" images. #1668
     (1.8.4/1.7.14)
   * `--deepmerge` now will give a useful error message when the image do
     not have the same number of channels. #1675 (1.8.4/1.7.14)
   * `--autocc` more gracefully handles unknown color spaces with a warning,
     rather than a full error and termination. #1681 (1.8.4)
   * `--resample` now takes an optional modifier `interp=0` to control
     whether bilinear sample is used (default) or true closest-pixel point
     sampling. #1694 (1.8.4)
   * You can set rational metadata on the command line like this:
      `oiiotool foo.exr --attrib:type=rational onehalf "50/100" -o rat.exr`
      #1698 (1.8.5)
   * `--fillholes` fixed a bug where, if asked to operate on an image with
      nonzero origin (crop, overscan, shrink-wrap), it would incorrectly
      move the pixels to the origin. #1768 (1.8.5)
* ImageBufAlgo:
   * `channel_append()` resolves redundant channel names by using the
     subimage name, if available. #1498 (1.8.0/1.7.8)
   * `colorconvert()`, `ociodisplay()`, and `ociolook()` can now take
     multiple context key/value pairs (by allowing they `context_key` and
     `context_value` paramters be comma-separated lists). #1504 (1.8.0)
   * `draw_rectangle()` (and `oiiotool --box`) wasn't drawing properly for
     the degenerate case of a rectangle that takes up just one
     pixel. #1601 (1.8.2)
   * `resample()` has been extended to work for "deep" images.
      #1668 (1.8.4/1.7.14)
   * `deep_merge()` now will give a useful error message when the images do
     not have the same number of channels. #1675 (1.8.4/1.7.14)
   * `deep_merge()` performance has been greatly improved. #1739 (1.8.5)
   * `resample()` fixed a subtle 1/2 pixel shift, now it more closely
      aligns with `resize()`. #1694 (1.8.4)
   * `fillholes_pushpull()` fixed a bug where, if asked to operate on an
      image with nonzero origin (crop, overscan, shrink-wrap), it would
      incorrectly move the pixels to the origin. #1768 (1.8.5)
* ImageBuf:
   * Fix broken threads(n) method, which didn't correctly pass the right
     number of threads along. #1622. (1.8.3/1.7.12)
   * Copy constructor from another ImageBuf was previously broken for
     IB's that wrap application buffers. #1665 (1.8.4/1.7.13)
* TextureSystem / ImageCache / maketx:
   * `IC::get_image_info` (or `TS::get_texture_info`) queries for "channels"
     on UDIM file patterns now succeed, returning the value for the first
     matching file it finds. (N.B.: Relies on all textures within the same
     UDIM set having the same nchannels.) #1502, #1519, #1530 (1.8.0/1.7.8)
   * maketx: multiple simultaneous maketx process trying to create the same
     texture will no longer clobber each other's output. #1525 (1.8.0/1.7.8)
   * ImageCache: make robust to changes in autotile after opening and reading
     from untiled files. #1566 (1.8.1/1.7.9)
   * ImageCache: fix initialization bug that made the reported stats output
     nonsensical in the numbers it gave for "redundant reads". #1567
     (1.8.1/1.7.9)
   * get_image_info queries of "displaywindow" and "datawindow" did not
     correctly return a 'true' value when the data was found.
     #1574 (1.8.1/1.7.9)
   * maketx fix: two textures that had identical source pixels but differed
     in whether they resized with "highlight compensation" incorrectly
     ended up with identical hashes, and thus could be confused by the
     TextureSystem at runtime into thinking they were duplicates. The hash
     is now fixed. #1599 (1.8.2/1.7.11)
   * Statistics no longer list as "BROKEN" files which were invalidated, or
     files that were initialized with an ImageCacheFile but never opened.
     #1655 (1.8.4)
   * ImageCache::get_image_info() will now return a proper error (and not
     hig an assertion) if asked for information about a subimage or MIP
     level that does not exist in the file. #1672 (1.8.4/1.7.14)
   * TextureSystem::get_texels fixes crashing behavior. #1669 (1.8.4/1.7.14)
   * Big performance improvement on Windows with MSVC by removing certain
     empty destructors in simd.h that was preventing MSVC from fully inlining
     those classes. #1685 (1.8.4/1.7.15)
   * maketx now supports `--colorconfig` option to explicitly point it to
     an OpenColorIO config file, just like `oiiotool `--colorconfig`.
     #1692 (1.8.4)
   * Fix rare edge case crash in ImageCache. #1696 (1.8.4/1.7.15)
   * Improved error messages for broken files. Specifically, it's much more
     clear now when a file is broken because it's being rejected as a
     texture because it's untiled or not MIP-mapped. #1751 (1.8.5)
   * The maketx-generated metadata "oiio:SHA-1", "oiio:ConstantColor" and
     "oiio:AverageColor" are ignored if the file has signs that it was not
     directly generated by `maketx` or `oiiotool -otex` (specifically, if
     it's not tiled, has no "textureformat" tag, or if its "software" tag
     doesn't mention maketx or oiiotool). This helps for the case where a
     maketx-generated file is loaded into PhotoShop (or otherwise altered),
     saved with different pixel values but the old SHA-1, which would no
     longer be valid and therefore cause the new file to be misidentified
     as a duplicate texture even though it's not. #1762 (1.8.5)
* Bug fix to possible crashes when adding dither to tiled file output
  (buffer size miscalculation). #1518 (1.8.0/1.7.8)
* Make sure that sRGB<->linear color transform still work (in the obvious
  way) even when OpenColorIO is present but that its configuration for some
  reason doesn't know about "sRGB" space. #1554 (1.8.1)
* Improved performance of input of many-channel files with differing
  per-channel data formats. #1541 (1.8.1)
* `idiff` : in addition to the pixel coordinates and differences of the
  biggest differing pixel, it now also prints the full values of all
  channels of that pixel for both images. #1570 (1.8.1)
* ImageInput::read_tiles when only a channel subset is read, fixed case
  with certain data sizes where the copy to user buffer got mangled.
  #1595 (1.8.2/1.7.11)
* BMP:
   * Add support for version 5 of the BMP format header. $1616 (1.8.3/1.7.12)
* FFMpeg/movies:
   * "FramesPerSecond" metadata has had its type changed to rational.
     #1709 (1.8.5)
* GIF:
   * "FramesPerSecond" metadata has had its type changed to rational.
     #1709 (1.8.5)
* IFF:
   * Fix IFF output that didn't correctly save the "Author" and "Date"
     metadata. #1549 (1.8.1/1.7.8)
* JPEG:
  * Be more reslient to malformed Exif data blocks with bogus offsets
    #1585 (1.8.1/1.7.10) and #1639 (1.8.3/1.7.13).
  * When you ask the JPEG writer to output files with unsupported channel
    counts (JFIF files only allow 1 or 3 channels), it will just silently
    drop extra channels, rather than having a hard error. #1643 (1.8.3)
* OpenEXR:
  * Fix global attribute "exr_threads" value, -1 should disable IlmImf's
    thread pools as advertised. #1582 (1.8.1)
  * Allow compression "none" for deep exr files. (1.8.2/1.7.11)
  * Fixed input problem with sorting order of spectral alpha channels (RA,
    GA, BA, or AR, AG, AB). #1674 (1.8./1.7.14)
  * Can handle true rational metadata, including FramesPerSecond and
    captureRate. #1698 (1.8.5)
  * Fix problem with 2-channel images putting the channels in the wrong
    order. #1717 (1.8.5/1.7.16)
* PNG: Better extraction of XMP from PNG files. #1689 (1.8.4)
* PSD:
   * Support has been added for "cmyk", "multichannel", and "grayscale"
     color modes. And support was fixed for rgb and grayscale 32 bit per
     sample bit depth. #1641 (1.8.3/1.7.13)
   * Fix issue for layer mask channels. #1714 (1.8.5)
* RAW:
   * Fix possible crash when reading certain raw metadata. #1547 (1.7.8/1.8.0)
   * The default value for missing "raw:use_camera_matrix" has been changed
     to 1 (not 0) to better match default behavior of libraw/dcraw.
     #1629 (1.8.3/1.7.13)
   * Add support for selecting new demosaicing algorithms: "DMT" (mode 11)
     and "AAHD" (mode 12). Renamed the "Modified AHD" to "AHD-Mod" to
     simplify and match libraw terminology. Made matching of demosaicing
     algorithms case-insensitive. #1629 (1.8.3/1.7.13)
   * Support "ACES" color space for direct conversion while reading RAW
     images (supported in libraw 0.18 or newer). #1626 (1.8.3/1.7.13)
   * Add "raw:user_sat" configuration attribute to the reader.
     #1666 (1.7.15/1.8.4)
   * The pixels are now decoded (expensive) only when they are read, not
     when the file is first opened. This makes raw reading much faster for
     apps that are only interested in the metadata and not the pixel data.
     #1741 (1.8.5)
   * Unpack pixels when they are needed, not when the file is opened. This
     makes it much faster to read RAW file only to extract the metadata,
     if you don't need the pixel values. #1741 (1.8.5)
* RLA:
   * Fix RLA reading and writing with certain channel orders and mixded data
     formats. #1499 (1.8.0/1.7.8)
* TIFF:
   * Fix to TIFF handling of certain unusual tags. #1547 (1.7.8/1.8.0)
   * Now has a way to read raw pixel values from CMYK files, without
     the automatic conversion to RGB (pass configuration attribute
     "oiio:RawColor" set to nonzero). #1605 (1.8.2/1.7.11)
   * Improved I/O of color separation images, particularly those with
     custom InkSet attributes. #1658 (1.8.4/1.7.15)
   * Fix typo that prevented correct reading of some Exif fields. #1625
     (1.8.3/1.7.12)
   * TIFF output omitted setting the "Make" and "Model" metadata tags.
     #1642 (1.8.3/1.7.13)
   * Images with fewer than 4 channels, but one of those channels was alpha,
     were not correctly marking spec.alpha_channel. #1718 (1.8.5/1.7.16)
   * The XPOSITION and YPOSITION tags are now interpreted as relative to
     the RESOLUTIONUNIT, whereas before it was assumed to be measured in
     pixels. We are confident that the new way is more in line with the
     intent of the TIFF spec. #1631 (1.8.5)
* webp:
   * Several new sanity checks prevent the webp reader from spending too
     much I/O time and memory reading bogus files (malformed, corrupted,
     or not a webp after all). #1640 (1.8.3/1.7.13)
* Nuke plugin: Fix txReader to properly restore saved MIP level knob value.
  #1531 (1.8.0)
* Fixed several (so far unnoticed) buffer overruns and memory leaks.
  #1591 (1.8.2)
* TIFF, JPEG, others: Improved reading Exif meatdata from XMP blocks, now it
  does a better job of discerning the proper data types. #1627 (1.8.3/1.7.13)
* In several places, the formatting of date metadata has been changed to
  have a leading zero rather than leading space for hours < 10, i.e.,
  "02:00:00" rather than " 2:00:00". #1630 (1.8.3)
* Improve XMP parsing (for any format): for malformed XMP, still honor the
  parts that could be parsed properly; recognize additinoal tags for
  GPano (Google's Photo Sphere metadata schema) and camera raw (crs: prefix)
  metadata; improve speed of XMP parsing.  #1679 (1.8.4)
* Improved handling and color conversion of gamma-corrected images (DPX,
  HDR, PNG, RLA, Targa) by supporting linearization correctly even in the
  presence of OCIO configs that don't know about it. #1684 (1.8.4)
* Fixed static initialization order fiasco error involving interaction
  between ColorConfig and Strutil. #1757 (1.8.5)

Build/test system improvements:
* **Changes to install layout**: fonts now get installed to
  `prefix/share/fonts/OpenImageIO`, OIIO docs now get installed to
  `prefix/share/doc/OpenImageIO`, and the Python module gets installed to
  `prefix/lib/pythonMAJ.MIN/site-packages`. #1747 #1760 (1.8.5)
* Support for building against ffmpeg 3.1 (their API has changed).
  #1515 (1.8.0/1.7.8)
* Build no longer gets confused about include files from older installations
  of OIIO elsewhere on the system. #1524 (1.8.0/1.7.8)
* Improvements in finding OpenJPEG. #1520 (1.8.0/1.7.8)
* Improved finding of OCIO headers. #1528 (1.8.0/1.7.8)
* Fix compile warnings for Clang 3.9. #1529 (1.8.0/1.7.8)
* Minimum C++ standard of C++11 is expected and enforced. #1513 (1.8.0)
* Minimum Boost is now 1.53. #1526 (1.8.0)
* Fix compiler warning on Fedora aarch64. #1592 (1.8.1)
* Tweak OpenJPEG include file search rules to prefer newer versions when
  multiple are installed. #1578 (1.8.1)
* Build option `SANITIZE=...` lets us use the sanitizers. #1591 (1.8.2)
* Big refactoring of the cmake build files. #1604 (1.8.2)
* When using a recent enough C++ compiler and standard library, OIIO will
  now use C++11 std::regex rather than boost regex. #1620,#1644 (1.8.3)
* Support for clang-tidy using build time flags CLANG_TIDY=1,
  CLANG_TIDY_ARGS=..., and optionally CLANG_TIDY_FIX=1. #1649 (1.8.4)
* Fix Windows warnings about SIMD types as function arguments
  and about bool vs int. #1659 (1.8.4)
* Changed the way namespaces work. Instead of a 2-level namespace, make a
  one-level namespace, and automatically embed the major and minor version
  in it. (Can stll override the basename.) #1662 (1.8.4)
* Fixes to OSX rpath behavior of linked binaries. #1671
* Upgraded the local PugiXML (when not using an external system version)
  to release 1.8.1.  #1679 (1.8.4)
* Beef up Strutil unit tests. (1.8.5)
* `iv` has been upgraded to use Qt 5.x. Support for Qt 4.x is hereby
  deprecated. #1711 (1.8.5)
* Make the search for boost_python3 more reliable. #1727 (1.8.5)
* Fix python site-packages path for installation. #1722 (1.8.5)
* Fixes for building with gcc 7. (1.8.5)
* Support and fixes for building with clang 5.0. #1746 (1.8.5)
* Support/fixes for Boost 1.65. #1553 (1.8.5)
* Simplify CMake scripts by using GNUInstallDirs to set standard installation
  paths. #1747 (1.8.5)

Developer goodies / internals:
* Sysutil::Term formatting now works properly in Windows (though is only
  functional for Windows 10 or above). (1.8.0/1.7.8) #1527
* C++11 idioms:
   * We now eschew BOOST_FOREACH, in favor of C++11 "range for". #1535
   * We now use std::unique_ptr, no longer use boost::scoped_ptr or
     boost::scoped_array. #1543 (1.8.0) #1586 (1.8.1)
   * Instead of the various boost components, we now use `std::` versions of
     unordered_map, unordered_set, shared_ptr, hash, bind. #1586 (1.8.1)
   * Change deprecated C headers (such as `<ctype.h>`) to C++ (`<cctype>`).
     #1649 (1.8.4)
   * Use `std::vector<>::emplace_back()` where applicable. #1657 (1.8.4)
   * Mark ImageInput/ImageOutput derived classes as 'final'. (1.8.5)
* array_view.h:
   * Add front() and back() methods. #1724 (1.8.5)
   * Simplified array_view template to be 1D only. #1734 (1.8.5)
* atomic.h:
   * Added atomic_min and atomic_max. #1661 (1.8.4)
   * Added atomic_fetch_add for `std::atomic<float>` and double. #1661 (1.8.4)
   * Assume std::atomic is available, remove all code that is only needed
     for pre-C++11. #1661 (1.8.4)
* benchmark.h:
   * New `Benchmarker` class utility for micro-benchmarking. #1577 (1.8.5)
   * Moved time_trial, timed_thead_wedge, DoNotOptimize, clobber_all_memory
     to benchmark.h. #1577 (1.8.5)
* errorhandler.h: Change all ErrorHandler methods to use variadic templates
  rather than varargs. #1653 (1.8.4)
* filesystem.h:
   * Better exception safety for Filesystem::searchpath_find. #1680 (1.8.4/1.7.15)
* fmath.h:
   * Fixed typo in fmath.h that made bitcast_to_float incorrect. #1543 (1.8.0)
   * Templatize round_to_multiple() so it works with types other than `int`.
     #1548 (1.8.0)
   * `interpolate_linear()` utility linearly interpolates from a list of
     evenly-spaced knots. #1552 (1.8.1)
   * Slight reformulation of clamp() ensures sane results even with NaN
     parameters. #1617 (1.8.3)
   * Bug fixes to `ifloor()` and `floorfrac()`, which turned out to give
     incorrect results for exact negative integer values. Also added
     simd vector-based versions of `floorfrac`. #1766 (1.8.5)
* paramlist.h:
   * ParamValueList has been refactored and now inherets from, rather than
     containts, a `std::vector<ParamValue>`. This removes most of the
     additional code from the class. #1677 (1.8.4)
   * ParamValue new methods: `get<>()`, `get_int()`, `get_float()`,
     `get_string()`, `get_ustring()` retrieve and convert. #1686 (1.8.4)
   * ParamValueList new methods: `get_int()`, `get_float()`, `get_string()`,
     `get_ustring()` search, retrieve, and convert (much like the
     equivalent versions in ImageSpec did). #1686 (1.8.4)
   * Remove the pointless typedefs `ImageIOParameter` and
     `ImageIOParameterList` in favor of `ParamValue` and `ParamValueList`,
     respectively. #1690 (1.8.4)
* platform.h:
   * More `cpu_has_...()` tests for newer CPU capabilties. #1719 (1.8.5)
   * Remove deprecated OIIO_NOTHROW macro, which should now simply be
     C++11 `noexcept`. #1736 (1.8.5)
* strutil.h / Strutil:
   * Add `Strutil::printf()` and `Strutil::fprintf()`, typesafe and
     non-thread-jumbled replacements for C versions. #1579, #1656 (1.8.1, 1.8.4)
   * `from_string<>` has been extended to 'unsigned int'. (1.8.3/1.7.13)
   * `Strutil::parse_identifier_if` #1647 (1.8.3)
   * safe_strcpy now takes a string_view, and more closely conforms to the
     behavior of strncpy by filling in the extra space with 0 padding.
     (1.8.5)
* simd.h:
   * Add a matrix44 constructor from 16 floats. #1552 (1.8.1)
   * Renamed files floatN, intN, boolN to vfloatN, vintN, vboolN, to avoid
     confusion with bit lengths of scalars. #1719 (1.8.5)
   * Overhaul to support AVX-512 via vfloat16, vint16, vbool16 classes.
     #1719 (1.8.5)
   * load_mask, store_mask, and scatter/gather added for all types.
     #1732 (1.8.5)
   * Renamed `floori()` to `ifloor()` to match the analogous scalar function
     in fmath.h. #1766 (1.8.5)
* strided_ptr.h: The `strided_ptr<>` template is now also templated on
     the stride units -- the default is to measure strides in units of
     sizeof(T), but it's possible to override and measure in bytes (the old
     behavior). #1758 (1.8.5)
* thread.h:
   * thread_pool class offers true persistent thread pool.
     #1556, #1581 (1.8.1)
   * Lots of C++ modernization of thread_group and spin_mutex. #1556 (1.8.1)
   * Environment variable `OPENIMAGEIO_THREADS` can artificially raise or
     lower the default number of threads (otherwise, it defaults to the
     number of processor cores available). #1581 (1.8.1)
* timer.h: added timed_thread_widge() that benchmark code and prints
  statistics about thread scaling. #1660 (1.8.4)
* typedesc.h:
   * Modernized TypeDesc with C++11 constexpr where applicable. #1684 (1.8.4)
   * New `TypeDesc::basevalues()` method. #1688 (1.8.4)
* unittest.h:
   * Colored error messages and auto error return on completion. #1731 (1.8.5)
* *NEW* parallel.h:
   * parallel_for, parallel_for_chunked, parallel_for_each offer simple
     thread_pool-based parallel looping in 1 and 2 dimensions.
     #1556, #1581 (1.8.1)
* Internal `OIIO::debugmsg()` call has been renamed to `OIIO::debug()`,
  and setting env variable OPENIMAGEIO_DEBUG_FILE can redirect the debug
  messages to a file. #1580 (1.8.1)
* Upgraded tinyformat to the latest master, also changed all the places
  where we used TINYFORMAT_WRAP_FORMAT to use of C++11 variadic templates.
  #1618 (1.8.3)
* *NEW* function_view.h : function_view<> is a very lightweight, non-owning,
  generic callable object view. Cheaper than std::function, but the view
  is not allowed to outlive the callable object it references. #1660 (1.8.4)
* Deprecate the pre-C++11 macros OIIO_CONSTEXPR, OIIO_CONSTEXPR_OR_CONST,
  and OIIO_NOEXCEPT. #1678 (1.8.4)

Docs:
* Improve docs about deep IBA functions. (1.8.1)
* Fix 'Building OIIO on Windows' link. #1590 (1.8.1)



Release 1.7.17 (1 Sep 2017) -- compared to 1.7.16
-------------------------------------------------
* Repair build breaks against Boost 1.65. #1753
* Fix a subtle static initialization order problem. #1757
* Build: Improved finding LibRaw. #1749

Release 1.7.16 (1 Aug 2017) -- compared to 1.7.15
-------------------------------------------------
* OpenEXR: fix problem with 2-channel images putting the channels in the
  wrong order. #1717
* TIFF: images with fewer than 4 channels, but one of those channels was
  alpha, were not correctly marking their spec.alpha_channel. #1718
* Several minor updates to simd.h backported from mater.

Release 1.7.15 (1 Jun 2017) -- compared to 1.7.14
-------------------------------------------------
* Add "raw:user_sat" configuration attribute to the reader. #1666
* Better exception safety for `Filesystem::searchpath_find()`. #1680
* Improved I/O of color separation images, particularly those with custom
  InkSet attributes. #1658
* Big TextureSystem performance improvement on Windows with MSVC by removing
  certain empty destructors in simd.h that prevented MSVC from fully
  inlining the class. #1685
* Fix rare case TextureSystem crash. #1685

Release 1.7.14 (1 May 2017) -- compared to 1.7.13
-------------------------------------------------
* oiiotool expression substitution now recognizes FRAME_NUMBER and
  FRAME_NUMBER_PAD. #1648
* oiiotool -resize and -resample now have more intuitive behavior for images
  where the display and pixel data windows are not the same, especially if
  the data window had a nonzero origin (such as with crop or overscan).
  #1667
* oiiotool --resample and ImageBufAlgo::resample() have been extended to
  work for "deep" images. #1668
* ImageCache::get_image_info() will now return a proper error (and not hit
  an assertion) if asked for information about a subimage or MIP level that
  does not exist in the file. #1672
* ImageBuf copy constructor from another ImageBuf was previously broken for
  IB's that wrap application buffers. #1665
* TextureSystem::get_texels fixes crashing behavior. #1669
* Fixes to OSX rpath behavior of linked binaries. #1671
* OpenEXR file input: fixed problem with sorting order of spectral alpha
  channels (RA, GA, BA, or AR, AG, AB). #1674
* ImageBufAlgo::deep_merge() (and oiiotool --deepmerge) now will give a
  useful error message if you try to merge two deep images that do not have
  the same number of channels. #1675
* ImageCache/TextureSystem statistics no longer list as "BROKEN" files which
  were invalidated, or files that were initialized with an ImageCacheFile
  but never opened. #1655
* Fix Windows warnings about SIMD types as function args, and about
  int vs bool. #1659

Release 1.7.13 (1 Apr 2017) -- compared to 1.7.12
----------------------------------------------
* TIFF, JPEG, others: Improved reading Exif meatdata from XMP blocks, now it
  does a better job of discerning the proper data types. #1627
* RAW: The default value for missing "raw:use_camera_matrix" has been changed
  to 1 (not 0) to better match default behavior of libraw/dcraw. #1629
* RAW: Add support for selecting new demosaicing algorithms: "DMT" (mode 11)
  and "AAHD" (mode 12). Renamed the "Modified AHD" to "AHD-Mod" to
  simplify and match libraw terminology. Made matching of demosaicing
  algorithms case-insensitive. #1629
* RAW: Support "ACES" color space for direct conversion while reading RAW
  images (supported in libraw 0.18 or newer). #1626
* oiiotool: Improved error reporting of file open errors when -iconfig is
  used. #1626
* oiiotool `--echo STRING` prints the string to the console. This can contain
  expressions! So you can do something like
      oiiotool file.exr -echo "Size is {TOP.width}x{TOP.height}"
  #1633
* JPEG: Be more reslient to malformed Exif data blocks with bogus offsets.
  #1585, #1639
* TIFF output omitted setting the "Make" and "Model" metadata tags. #1642
* webp: Several new sanity checks prevent the webp reader from spending too
  much I/O time and memory reading bogus files (malformed, corrupted,
  or not a webp after all). #1640
* PSD: Support has been added for "cmyk", "multichannel", and "grayscale"
  color modes. And support was fixed for rgb and grayscale 32 bit per
  sample bit depth. #1641

Release 1.7.12 (1 Mar 2017) -- compared to 1.7.11
----------------------------------------------
* BMP: add support for version 5 headers. #1616
* TIFF: Fix typo that prevented correct reading of some Exif fields. #1625
* ImageBuf: Fix broken threads(n) method, which didn't correctly pass the
  right number of threads along. #1622.
* Fix build warnings about undefined OIIO_MSVS_AT_LEAST_2013 symbol.

Release 1.7.11 (1 Feb 2017) -- compared to 1.7.10
----------------------------------------------
* maketx fix: two textures that had identical source pixels but differed
  in whether they resized with "highlight compensation" incorrectly
  ended up with identical hashes, and thus could be confused by the
  TextureSystem at runtime into thinking they were duplicates. The hash
  is now fixed.  (1599)
* OpenEXR: Allow compression "none" for deep exr files.
* Fix unimplemented python ImageBufAlgo.computePixelStats. (1596)
* IBA::draw_rectangle (and oiiotool --box) wasn't drawing properly for
  the degenerate case of a rectangle that takes up just one
  pixel. (1601)
* Fixed several (so far unnoticed) buffer overruns and memory leaks. (1591)
* ImageInput::read_tiles when only a channel subset is read, fixed case
  with certain data sizes where the copy to user buffer got mangled. (1595)
* oiiotool -iconfig fixed, did not in all cases correctly propagate the
  input config attribute to the file read. (1605)
* TIFF: now has a way to read raw pixel values from CMYK files, without
  the automatic conversion to RGB (pass configuration attribute
  "oiio:RawColor" set to nonzero). (1605)
* imageio.h: Fix incorrect declaration of declare_imageio_format(). (1609)
* `oiiotool --crop` did not properly honor the `-a` flag and apply the crop
  to all subimages. (1613)

Release 1.7.10 (1 Jan 2017) -- compared to 1.7.9
----------------------------------------------
* Fix "exr_threads" value, -1 should disable IlmImf's thread pools. #1582
* Be more reslient to malformed Exif data blocks with bogus offsets. #1585
* Build: Fix regarding iv man page.
* Build: Fix compiler warning on Fedora aarch64. #1592
* Docs: improve docs about deep IBA functions.
* Docs: fix 'Building OIIO on Windows' link. #1590

Release 1.7.9 (1 Dec 2016) -- compared to 1.7.8
----------------------------------------------
* Make sure that sRGB<->linear color transform still work (in the obvious
  way) even when OpenColorIO is present but that its configuration for some
  reason doesn't know about "sRGB" space. #1554
* ImageCache: make robust to changes in autotile after opening and reading
  from untiled files. #1566
* ImageCache: fix initialization bug that made the reported stats output
  nonsensical in the numbers it gave for "redundant reads". #1567
* IC/TS get_image_info queries of "displaywindow" and "datawindow" did not
  correctly return a 'true' value when the data was found. #1574
* oiiotool -d giving per-channel formats is no longer confused by channel
  renaming with --chnames. #1563
* Added implementation of ImageBufAlgo::to_IplImage(). #1461

Release 1.7.8 (1 Nov 2016) -- compared to 1.7.7
----------------------------------------------
* Fix gcc warnings when compiling for AVX2. #1511
* Fix a variety of Windows warnings and breaks. #1512, #1523
* Added support for new API introduced with ffmpeg 3.1. #1515
* Improve oiiotool --mosaic, wasn't reliably clearing the blank spaces
  for missing images.
* Smarter channel_append channel renaming: try to resolve redundant
  channel names by using the subimage name, if available. #1498
* Texture: get_image_info queries for "channels" on UDIM file patterns
  now succeeds, returning the value for the first matching file it finds.
  (Relies on all textures within the same UDIM set having the same
  nchannels.) #1502, #1519
* Bug fix to possible crashes when adding dither to tiled file output
  (buffer size miscalculation). #1518
* maketx: multiple simultaneous maketx process trying to create the same
  texture will no longer clobber each other's output. #1525
* Build no longer gets confused about include files from older installations
  of OIIO elsewhere on the system. #1524
* Improvements in finding OpenJPEG. #1520
* Sysutil::Term formatting now works properly in Windows (though is only
  functional for Windows 10 or above). #1527
* Fix RLA reading and writing with certain channel orders and mixded data
  formats. #1499
* Improved finding of OCIO headers. #1528
* Better recognition of C++11 features in MSVS.
* Fix compile warnings with Clang 3.9. #1529
* Texture: Fix UDIM channels query. #1530
* nuke: Fix txReader to properly restore saved mip level knob value (#1531)
* Fix warnings on some 32 bit platforms #1539
* Exit oiiotool with non-zero status when command-line args fail to
  parse properly. #1540
* Fix typo in fmath bitcast_to_float declaration #1543
* Allow multiple key/value pairs in OCIO wrappers. #1542
* colorconvert API extended to take context key/values #1542
* Fix to TIFF handling of certain unusual tags, which also affected raw
  files. #1547
* fmath.h: templatize round_to_multiple so it works with other types
  (like size_t). #1548
* Fix IFF output that didn't correctly save the "Author" and "Date"
  metadata. #1549
* Handle 'oiiotool --colorconvert X X' (transform from and to spaces that
  are the same) without considering it an error. #1550

Release 1.7 (1 Oct 2016) -- compared to 1.6.x
----------------------------------------------
Major new features and improvements:
 * New oiiotool commands:
    * `-otex` and `-oenv` allow oiiotool to directly output proper texture
      maps, and it can now do everything that maketx can do. #1351 (1.7.2)
    * `--line` can draw polylines into an image. #1319 (1.7.1)
    * `--box` can draw a filled or unfilled box into an image. #1319 (1.7.1)
    * `--laplacian` computes the Laplacian. #1332 (1.7.1)
    * `--deep_merge` does a full merge/composite of deep images. #1388 (1.7.2)
    * `-i` inputs a file. Options `autocc=`, `now=`, `info=` control aspects
      of reading that one file. #1389 (1.7.2)
    * `--dilate` and `--erode` perform the basic morphological operations
      of dilation and erosion. #1486 (1.7.5)
 * New ImageBufAlgo functions: render_point(), render_line(), render_box()
   #1319 (1.7.1); laplacian() #1332 (1.7.2); copy() #1388 (1.7.2);
   deep_merge() #1388,1393 (1.7.2); dilate() and erode() (1.7.5).
 * UDIM support for textures: filenames like `"tex_<UDIM>.exr"` will
   automatically be resolved to the correct UTIM tile based on the s,t
   coordinates. #1426 (1.7.3)
 * Behavior change: When reading files without explicit channel names,
   single channel images now name their channel "Y" (no longer "A",
   which was confusing to algorithms that treat alpha in special ways).
   Similarly, 2-channel images name their channels "R" and "G". #1434 (1.7.3)

Public API changes:
 * DeepData internals and API overhaul: struct internals hidden, now you
   must use the API; DeepData declaration is now in deepdata.h, not in
   imageio.h; DD methods now allow insertion and erasure of individual
   samples. #1289 (1.7.0) New DeepData methods: split, sort, merge_overlaps,
   merge_deep_pixels, occlusion_cull. #1388,1393 (1.7.2)
 * imageio.h: Removed items deprecated since 1.4: a version of convert_types()
   that took alpha and z channel indices (but never used them). #1291
 * fmath.h: Removed safe_sqrtf, safe_acosf, fast_expf, which have been
   deprecated since 1.5. (1.7.0) #1291
 * Removed ImageBufAlgo::flipflop(), which was deprecated since 1.5 and
   is now called rotate180. #1291 (1.7.0)
 * Several varieties of ImageCache and TextureSystem getattribute methods
   were noticed to not be properly declared 'const'. This was fixed.
   #1300 (1.7.0/1.6.9)
 * For API calls that are deprecated but not yet removed, we now mark
   them with deprecated attributes for compilers that support it,
   meaning that you will get compile warnings and explanations when you
   use deprecated OIIO API functions. #1313,#1318 (1.7.1)
 * ImageBuf::contains_roi() reveals whether an ROI is completely contained
   in the data region of the IB. #1310 (1.7.1)
 * TypeDesc::is_signed() return true of the TypeDesc returns a type that
   can represent negative values. #1320
 * Python: improve: reading with request for type UNKNOWN returns native
   data in an unsigned char array. Also, requesting HALF returns the half
   bits in an unsigned short array (since there is no 'half' type in Python).
   #1362 (1.7.2/1.6.11)
 * Deprecate ImageCache/TextureSystem clear() methods, which never did
   anything useful. #1347 (1.7.3)
 * ImageSpec::set_format() clears any per-channel format information. #1446
   (1.7.4)
 * OIIO::getattribute("library_list", ...) can retrieve a list of all the
   dependent libraries used when OIIO was built. #1458 (1.7.5)
 * simd::mask4 class has been renamed simd::bool4 (though the old name
   is still typedef'ed to work for now). #1484 (1.7.5)
 * Python bindings for ImageBuf.reset() now properly understands the
   argument names and default values. #1492 (1.7.6)

Fixes, minor enhancements, and performance improvements:
 * oiiotool:
    * oiiotool --subimage now takes as an argument either the subimage
      numeric index, or a subimage name. #1287 (1.7.0)
    * oiiotool's image cache was smaller than intended because of typo.
      (1.7.0/1.6.9)
    * Allow command-line expression metadata names to contain ':'. #1321
      (1.7.1/1.6.10)
    * --info more clearly prints info about subimage formats. #1320 (1.7.1)
    * --ch: when the channels were only renamed, not reordered, the
      renaming didn't happen properly. #1326 (1.7.1/1.6.10)
    * Improved error message propagation from the underlying IBA functions
      and from errors encounered during --stats. #1338 (1.7.2)
    * --dumpdata:empty=0 now does something for non-deep files: skips
      reporting of pixels where all channels are black. Also fixed errors
      for dumpdata of deep, but non-float, files. #1355 (1.7.2/1.6.11)
    * '--attrib:type=t name value' lets you explicitly name the top of
      the attribute you're seting. This helps for ambiguous cases, and also
      lets you create aggregate types (such as 'matrix' or 'int[2]' --
      the value can be a comma-separated list in those cases). #1351 (1.7.2)
    * --fixnan can now take option "error", meaning that upon finding a
      NaN, the program considers it an error (rather than fixing).
      #1351 (1.7.2)
    * -o now takes optional arguments that control the output of just that
      one file, including :datatype=, :bits=, :dither=, :autocc=,
      :autocrop=, :autotrim=, :separate=, :contig=.  #1351 (1.7.2)
    * --resize and --fit sped up by approximately 2x. #1372 (1.7.2)
    * --runstats not tracks and reports max memory usage. #1385 (1.7.2)
    * New --cache and --autotile options let you set the ImageCache size and
      autotile options. #1385 (1.7.2)
    * --native results in slightly different behavior: does not force float,
      but still uses the ImageCache as backing if the native data type
      is one of the types directly supported by IC. #1385 (1.7.2)
    * --fixnan now works with deep images. #1397 (1.7.3)
    * --stats when used with deep images, if it encounters any nonfinite
      values, will print the location of a representative one. #1397 (1.7.3)
    * --add, --sub, and --absdiff, when its two image operands have differing
      number of channels, now try to do the "correct" thing and have the
      result have the larger number of channels, treating any "missing"
      channels in either input as if they were 0-valued. #1402 (1.7.3)
    * --attrib:type-timecode lets you set timecodes in "human-readable" form:
        oiiotool in.exr -attrib:type=timecode TimeCode 11:34:03:00 -o out.exr
      #1415 (1.7.3)
    * -info -v will print timecodes in human-readable form. #1415 (1.7.3)
    * Bug fix: --fullsize didn't work properly when followed by --attrib.
      #1418 (1.7.3/1.6.14)
    * Easily understandable warnings for common mistakes: no output specified,
      an image that's modified but never output, or when -autocc causes
      output to be a data format that overrides a previous -d directive.
      #1419 (1.7.3)
    * --origin, --croptofull, and --trim all do their thing to all subimages,
      if the image at the top of the stack has multiple subimages.
      #1440 (1.7.3)
    * --crop operates on all subimages if either the -a flag was used, or
      if the specific optinoal override `--crop:allsubimages=1` is used.
      #1440 (1.7.3)
    * --trim will trim all subimages to the same region, containing the
      union of all nonzero pixels in all subimages. #1440 (1.7.3)
    * --help now prints all of the dependent libraries for individual
      formats. #1458 (1.7.5)
    * Bug fix: make sure to propagate per-channel formats from input
      to output. #1491 (1.7.6)
    * -o:all=n will output all images currently on the stack, and the
      filename argument will be assumed to be a pattern containing a %d,
      which will be substituted with the index of the image (beginning with
      n). For example, to take a multi-image TIFF and extract all the
      subimages separately,
          oiiotool multi.tif -sisplit -o:all=1 sub%04d.tif
      will output the subimges as sub0001.tif, sub0002.tif, and so on.
      #1494 (1.7.6)
    * --mosaic MxN would fail mysteriously if the number of images on the
      stack was less then M*N. This has been fixed to handle too-few images
      gracefully and just leave blank spaces as needed. #1501 (1.7.7)
 * ImageBuf:
    * ImageBuf::iterator performance is improved -- roughly cutting in half
      the overhead of iterating over pixels. #1308 (1.7.1/1.6.10)
    * ImageBuf reads from disk have been improved substantially (in some
      cases cutting read time in half) in many cases. #1328 (1.7.1)
    * ImageBuf::copy_pixels() has been sped up. #1358 (1.7.2)
 * ImageBufAlgo:
    * The varieties of add(), sub(), mul(), and div() that take an
      image operand and a per-channel constant operand have all been
      modified to work properly for "deep" images. #1297 (1.7.0/1.6.10)
    * mad() is sped up significantly (10x) for the common case of float
      images already stored in memory (not cached). #1310 (1.6.1)
    * render_point(), render_line(), render_box() can be used to render
      points, lines, and boxes into an image. #1319 (1.7.1)
    * channels(): when the channels were only renamed, not reordered,
      the renaming didn't happen properly. #1326 (1.7.1/1.6.10)
    * computePixelStats: Improved numerical accuracy to avoid getting
      NaN values from imprecision. #1333 (1.7.2/1.6.11)
    * laplacian() computes the laplacian of an image. #1332 (1.7.2)
    * fixNonFinite() takes a new option: NONFINITE_ERROR, which will
      return an error if nonfinite values are encountered. #1351 (1.7.2)
    * convolve() and unsharp_mask() have been sped up by about 35% for
      common cases. #1357 (1.7.2)
    * IBA::resize sped up by approximately 2x. #1372 (1.7.2)
    * IBA::fixNonFinite() now works with deep images. #1397 (1.7.3)
    * dilate() and erode() perform basic morphological operations.
      #1486 (1.7.5)
 * ImageCache / TextureSystem:
    * Less unnecessary pausing after read errors when falure_retries == 0.
      #1336 (1.7.2/1.6.11)
    * Texture: slight improvement in texture sharpness. #1369 (1.7.2/1.6.11)
    * New statistics related to redundant tile reads. #1417 (1.7.3)
    * TextureSystem option "flip_t", if nonzero, will flip the vertical
      direction of all texture lookups. Use this for renderers that adhere
      to the convention that the t=0 texture coordinate is the visible
      "bottom" of the texture. #1428 (1.7.3) #1462 (1.7.5)
    * UDIM support for textures: filenames like `"tex_<UDIM>.exr"` will
      automatically be resolved to the correct UTIM tile based on the
      s,t coordinates. #1426 (1.7.3)
    * Avoid repeated broken texture error messages. #1423 (1.7.3)
    * New IC/TS attribute: "max_errors_per_file" limits how many error
      messages are printed for each file. #1423 (1.7.3)
    * Improved statistics: for the "top 3" stats, print the top 3 that aren't
      broken. Also print a count & list of broken/invalid files. #1433
      (1.7.3/1.6.15)
    * Add ability to retrieve various per-file statistics. #1438 (1.7.3/1.6.15)
    * IC will clamp the max_open_files to the maximum allowed by the
      system, so you can no longer crash a program by incorrectly
      setting this limit too high. #1457 (1.7.5)
    * IC/TS statistics now report separately the total size of images
      referenced, in terms of in-cache data size, as well as on-disk
      size (the latter may be compressed). #1481 (1.7.5)
 * maketx:
    * maketx -u now remakes the file if command line arguments or OIIO
      version changes, even if the files' dates appear to match.
      #1281 (1.7.0)
    * Remove long-obsolete and non-functional command line options: --new,
      --old, --hash. #1351 (1.7.2)
 * iinfo:
    * More clearly prints info about subimage formats. #1320 (1.7.1)
    * Print timecodes in human-readable form. #1415 (1.7.3)
 * ImageOutput: fix cases with native data but non-contiguous strides.
   #1416 (1.7.3/1.6.15)
 * Cineon:
    * Improved deduction/setting of color space info. #1466 (1.7.5)
 * GIF:
    * GIF reader failed to set spec full_width, full_height. #1348
      (1.7.2/1.6.11)
 * JPEG:
    * Fix bad memory access crash when reading specific JPEG files that were
      written without their comment field including a null character to
      end the string. #1365 (1.7.2/1.6.11)
    * Change in behavior writing JPEG files when XResolution & YResolution
      are not provided, but a PixelAspectRatio is requested. Previously, we
      obeyed the JPEG/JFIF spec exactly, but it turns out that popular apps
      including PhotoShop and Nuke use the field differently than the spec
      dictates. So now we conform to how these apps work, rather than to
      the letter of the spec. #1412 (1.7.3/1.6.15)
 * OpenEXR:
    * Fix broken multipart output when parts had different pixel data
      types. #1306,#1316 (1.7.1/1.6.10)
    * Improved error reporting for bad tile reads. #1338 (1.7.2/1.6.11)
    * Fix errors reading tiles for mixed-format EXR files. #1352 (1.7.2/1.6.11)
    * The global OIIO::attribute("exr_threads") has been modified so that 0
      means to use full available hardware, -1 disables the OpenEXR
      thread pool and execute in the caller thread. #1381 (1.7.2)
    * When writing EXR, double check that there are no repeated channel
      names, and if so, rename them so the data is not lost (since the
      underlying libIlmImf will silently drop channels with repeated
      names).  #1435 (1.7.3)
    * More robust detected of when OpenEXR is tiled (for weird files).
      #1441 (1.7.3/1.6.15) (and a fix in #1448/1.7.4)
    * Fixed minor bug with OpenEXR output with correctly setting
      PixelAspectRatio based on the "XResolution" and "YResolution"
      attributes. #1453 (Fixes #1214) (1.7.4/1.6.16)
    * Fix setting "chromaticity" metadata in EXR files. #1487 (1.7.5)
    * When writing OpenEXR, accept compression requests with quality numbers
      appended to the compression algorithm name, such as "dwaa:200" to mean
      dwaa compression with a dwaCompressionLevel set to 200. #1493 (1.7.6)
 * PNG:
    * Per the PNG spec, name 2-channel images Y,A. #1435 (1.7.3)
    * Enforce that alpha premultiplication on output MUST consider alpha
      to be the last channel of 2 or 4 channel images, no other cases
      (as dictated by the PNG spec). #1435 (1.7.3)
 * PNM:
    * Fixed byte swapping when reading 16 but PNM files. #1352 (1.7.2/1.6.11)
 * RAW:
    * Changes to how we instruct libraw to process images when reading:
      Now, by default, auto-bright adjustment is off, camera white
      balance is on, and maximum threshoding is set to 0. There are
      "open with config" overrides for all of these, for anybody who
      doesn't like the default. #1490 (1.7.6)
 * RLA:
    * Fixes for both reading and writing of RLA images that are cropped
      (i.e., data window is a subset of display window). #1224 (1.7.0/1.6.10)
 * TIFF:
    * When outputting a TIFF file, a special attribute "tiff:half", if
      set to nonzero, will enable writing of 16-bit float pixel data
      (obviously, only if the spec.format is HALF). #1283 (1.7.0)
    * TIFF input: erase redundant IPTC:Caption and IPTC:OriginatingProgram
      if they are identical to existing ImageDescription and Software
      metadata, respectively. (1.7.0/1.6.9)
    * Output: "tiff:zipquality" attribute controls time-vs-quality for
      ZIP compression (1-9, defualt 6, higher means more compression).
      #1295 (1.7.1)
    * Fix typo that made TIFF files incorrectly name color space metadata
      "oiio::ColorSpace" instead of "oiio:ColorSpace". #1394 (1.7.2)
    * More robust handling of non-zero origin of full/display window.
      #1414 (1.6.14/1.7.3)
 * Video formats:
    * The ffmpeg-based reader had a variety of fixes. #1288 (1.7.0)
    * Support for reading 10-bit and 12-bit movies. #1430 (1.7.5)
 * Improved accuracy of "lanczos3" filter; speed up blackman-harris filter.
   #1379 (1.7.2)
 * Speed up linear<->sRGB color conversions (as used by any of the IBA color
   conversion functions as well as oiiotool --colorconvert and friends),
   approximately doubling the speed when no OpenColorIO config is found.
   #1383 (1.7.2)
 * ImageInput::create() and ImageOutput::create() will now gracefully
   handle unexpected exceptions inside an ImageInput or ImageOutput
   constructor -- return an error rather than crashing.  #1456 (1.7.4/1.6.16)
 * Nuke txWriter adds UI to let you choose which type of texture you are
   building (ordinary 2D texture, latlong env map, etc). #1488 (1.7.6)

Build/test system improvements:
 * Default build is now C++11! #1344 (1.7.2) You can still (for now) build
   for C++03 using 'make USE_CPP11=0' or 'cmake -DOIIO_BUID_CPP11=0', but
   some time soon we will be C++11 minimum.
 * Fix build break against Boost 1.60. #1299,#1300 (1.7.0/1.6.9/1.5.23)
 * filesystem_test now much more comprehensively tests the contents of
   Filesystem. #1302 (1.7.0)
 * fmath_test adds benchmarks for various data conversions. #1305 (1.7.0)
 * Travis: add DEBUG builds to the matrix to fix any warnings or failures
   that only show up for DEBUG builds. #1309 (1.7.1/1.6.10)
 * Fix build issues on some platforms for SHA1.h, by adding proper include
   of `<climits>`. #1298,#1311,#1312 (1.7.1/1.6.10)
 * Cleanup of include logic in simd.h that fixed build problems for gcc < 4.4.
   #1314 (1.7.1/1.6.10)
 * Fix build breaks for certain 32 bit platforms. #1315,#1322 (1.7.1/1.6.10)
 * imagespeed_test can not specify the data conversion type for reads,
   can optionally allow skipping the IB iteration tests, and can set the
   IC tile cache size. #1323 (1.7.1)
 * Fix build breaks for gcc 6. #1339 (1.7.2/1.6.11) #1436 (1.7.3/1.6.15)
 * Fix errors in finding the correct locaiton of pugixml.hpp when using
   USE_EXTERNAL_PUGIXML=1. #1339 (1.7.2/1.6.11)
 * Rewrite of FindOpenEXR.cmake. Solves many problems and is simpler.
   No more FindIlmbase.cmake at all. #1346 (1.7.2/1.6.11)
 * 'make CODECOV=1; make CODECOV=1 test' can build and test in a way that
   provides a code coverage report. #1356 (1.7.2)
 * Fix Filesystem::open() issues with UTF-8 filenames on MinGW.
   #1353,#1357 (1.7.2/1.6.11)
 * Allow build against a wider range of ffmpeg versions. #1359 (1.7.2)
 * Build correctly against FFMPEG 3.0. #1374 (1.7.2)
 * If found, libjpeg-turbo is used rather than libjpeg; this gives about a
   2x speed improvement for reading & writing JPEG files. #1390 (1.7.2/1.6.13)
 * USE_CPP11=... and USE_CPP14=... are the build flags for both the Make
   wrapper and the CMake scripts. (Before, it was confusing to have USE_CPP11
   for make but OIIO_BUILD_CPP11 for CMake. Now they are one.) #1391 (1.7.2)
 * Improved LINKSTATIC=1, now mostly works. #1395 (1.7.3)
 * Big CMake refactor, got rid of redundancies. #1395 (1.7.3)
 * Remove old embedded Ptex, now must find Ptex externally. Also modified
   the build scripts to correctly handle newer versions of Ptex. #1400
   (1.7.3/1.6.13)
 * Got Appveyor building OIIO. This is a continuous integration service
   much like Travis, but it's for Windows. Hopefully this means that it will
   be much harder for us to make changes that inadvertently break the build
   on Windows. #1399 (1.7.3)
 * Make FindOpenEXR.cmake more robust when the version number is embedded
   in the library name. #1401 (1.7.3/1.6.15)
 * Clear up some inconsistencies in the CMake files and the Makefile wrapper:
   the flag to compile with libc++ is now always called USE_LIBCPLUSPLUS,
   not sometimes OIIO_BUILD_LIBCPLUSPLUS. #1404 (1.7.3)
 * Overhaul OpenCV dependency finding and make it work with OpenCV 3.x.
   #1409 (1.7.3/1.6.13)
 * Allow custom JPEG_PATH to hint location of JPEG library. #1411
   (1.7.3/1.6.13)
 * Windows UTF-8 filename safety fixes. #1420 (1.7.3/1.6.14)
 * Various Windows compilation & warning fixes. #1443 (1.7.3/1.6.15)
 * Now builds correctly against OpenJPEG 2.x, it previously only supported
   OpenJPEG 1.x. #1452  (Fixes #957, #1449) (1.7.4/1.6.16)
 * Fix Filesystem::searchpath_find on Windows with UTF-8 paths.
   #1469 (1.7.51.6.17)
 * Improved the way OpenEXR installations are found. #1464 (1.7.5)

Developer goodies / internals:
 * thread.h has had all the atomic operations split into a separate atomic.h.
   #1443 (1.7.3)
 * atomic.h: add atomic and, or, and xor. #1417 (1.7.2/1.6.14);
 * parallel_image has been improved in several ways: can choose split
   direction; raised minimum chunk size to prevent thread fan-out for
   images too small to benefit; uses the calling thread as part of the
   pool. #1303 (1.7.0)
 * timer.h: DoNotOptimize() and clobber_all_memory() help to disable certain
   optimizations that would interfere with micro-benchmarks. #1305 (1.7.0)
 * simd.h improvements: select(); round(); float4::store(half*),
   int4::store(unsigned short*), int4::store(unsigned char*). #1305 (1.7.0)
   Define insert, extract, and ^ (xor), and ~ (bit complement) for mask4,
   and add ~ for int4. #1331 (1.7.2); madd, msub, nmadd, nmsub, rint,
   andnot #1377 (1.7.2); exp, log #1384 (1.7.2); simd::float3 is like float4,
   but only loads and stores 3 components, it's a good Vec3f replacement (but
   padded) #1473 (1.7.5); matrix44 4x4 matrix class #1473 (1.7.5);
   mask4 renamed to bool4, and addition of float8, int8, bool8 classes
   for 8-wide AVX/AVX2 SIMD #1484 (1.7.5).
 * fmath.h: convert_types has new special cases that vastly speed up
   float <-> uint16, uint8, and half buffer conversions #1305 (1.7.0);
   ifloor (1.7.2); SIMD versions of fast_log2, fast_log, fast_exp2,
   fast_exp, fast_pow_pos #1384 (1.7.2); fix sign of expm1 for small
   arguments #1482 (1.7.5); added fast_log1p #1483 (1.75).
 * Fix pesky precision discrepancy in internal convert_type<> that used
   slightly different math when converting one value at a time, versus
   converting whole arrays. #1350 (1.7.2)
 * thread.h: add mutex_pool #1425 (1.7.3/1.6.15)
 * compute_test: new unit test can be used to benchmark computation
   times. #1310 (1.7.1)
 * filesystem.h: Filesystem::file_size() returns file size in bytes;
   Filesystem::read_bytes() reads the first n (or all) bytes from a file
   into a buffer. #1451 (1.7.4/1.6.16)
 * strutil.h: Strutil::extract_from_list_string is more flixible by
   allowing the vals list to start empty, in which case it will add as
   many values as it finds rather than only replacing existing
   values #1319 (1.7.1); Strutil::replace #1422 (1.7.3/1.6.15);
   utf_to_unicode now takes a string_view rather than a std::string&
   #1450 (1.7.4); add Strutil::base64_encode() #1450 (1.7.4).
 * sysutil.h: Sysutil::getenv() safely gets an env variable as a string_view
   #1451 (1.7.4/1.6.16); terminal_columns() now has a correct implementation
   on Windows #1460 (1.7.5); max_open_files() retrieves the maximum number
   of files the process may open simultaneously #1457 (1.7.5).
 * platform.h: better distinguishing beteen Apple and Generic clang,
   separately set OIIO_CLANG_VERSION and OIIO_APPLE_CLANG_VERSION. Also change
   OIIO_GNUC_VERSION to 0 for clang, only nonzero for true gcc. #1380 (1.7.2)
 * ImageCache: remove unused shadow matrix fields, save space. #1424 (1.7.3)
 * Many documentation files (such as README, CHANGES, LICENSE, CREDITS,
   and INSTALL) have been changed from plain text to MarkDown. #1442 (1.7.3)
 * Sysutil::Term class makes it easy to use color output on the terminal.
   #1479 (1.7.5)



Release 1.6.18 (released 1 Nov 2016 -- compared to 1.6.17)
------------------------------------------------
* Fix setting "chromaticity" metadata in EXR files. #1487
* maketx: multiple simultaneous maketx process trying to create the same
  texture will no longer clobber each other's output. #1525
* Fix compile warnings with Clang 3.9. #1529
* Fix IFF output that didn't correctly save the "Author" and "Date"
  metadata. #1549


Release 1.6.17 (released 1 Sep 2016 -- compared to 1.6.16)
------------------------------------------------
* Fix build for newer ffmpeg release that deprecated functions.
* Improved finding of OCIO installations. #1467
* Fixed Sysutil::terminal_columns() for WIndows. #1460
* Fix build break in Windows when roundf function not found. #1468
* Fix Filesystem::searchpath_find on Windows with UTF-8 paths. #1469

Release 1.6.16 (released 1 Aug 2016 -- compared to 1.6.15)
------------------------------------------------
* Fix EXR tile logic for OpenEXR 1.x (fixes a break introduced in 1.6.15,
  is not an issue for exr 2.x). #1448
* Now builds correctly against OpenJPEG 2.x, it previously only supported
  OpenJPEG 1.x. #1452  (Fixes #957, #1449)
* New utility functions: Sysutil::getenv(), Filesystem::file_size(),
  FileSystem::read_bytes(). #1451
* Fixed minor bug with OpenEXR output with correctly setting
  PixelAspectRatio based on the "XResolution" and "YResolution"
  attributes. #1453 (Fixes #1214)
* Gracefully handle unexpected exceptions inside an ImageInput or
  ImageOutput constructor -- return an error rather than crashing.
  #1456

Release 1.6.15 (released 1 Jul 2016 -- compared to 1.6.14)
------------------------------------------------
* Improved statistics: for the "top 3" stats, print the top 3 that aren't
  broken. Also print a count & list of broken/invalid files. #1433
* Change in behavior writing JPEG files when XResolution & YResolution
  are not provided, but a PixelAspectRatio is requested. Previously, we
  obeyed the JPEG/JFIF spec exactly, but it turns out that popular apps
  including PhotoShop and Nuke use the field differently than the spec
  dictates. So now we conform to how these apps work, rather than to
  the letter of the spec. #1412
* IC/TS: add ability to retrieve various per-file statistics. #1438
* Windows UTF-8 filename safety fixes. #1420
* ImageOutput: fix cases with native data but non-contiguous strides.  #1416
* Make FindOpenEXR.cmake more robust when the version number is embedded
  in the library name. #1401
* Fix build breaks for gcc 6. #1339 (1.7.2/1.6.11) #1436
* More robust detected of when OpenEXR is tiled (for weird files).  #1441
* Various Windows compilation and warning fixes.
* strutil.h: added replace(). #1422
* thread.h: added mutex_pool. #1425

Release 1.6.14 (released 1 Jun 2016 -- compared to 1.6.13)
------------------------------------------------
* More robust handling of TIFF I/O when non-zero origin of full/display
  window. (#1414)
* oiiotool --fullsize didn't work properly when followed by --attrib. (#1418)

Release 1.6.13 (released 1 May 2016 -- compared to 1.6.12)
------------------------------------------------
* Use libjpeg-turbo if found. It's a drop-in replacement for libjpeg, but
  is around 2x faster. #1390
* Fix some Windows compiler warnings and errors.
* Remove old embedded Ptex, now must find Ptex externally. Also modified
  the build scripts to correctly handle newer versions of Ptex. #1400
* Overhaul OpenCV dependency finding and make it work with OpenCV 3.x. #1409
* Allow custom JPEG_PATH to hint location of JPEG library. #1411

Release 1.6.12 (released 1 Apr 2016 -- compared to 1.6.11)
------------------------------------------------
* Build correctly against FFMPEG 3.0. #1374
* The global OIIO::attribute("exr_threads") has been modified so that 0
  means to use full available hardware, -1 disables the OpenEXR thread
  pool and execute in the caller thread. #1381
* Thread-pool counts initialized to hardware_concurrency, not
  physical_concurrency (i.e., they will include hyperthread cores by
  default). #1378
* oiiotool --autocc bug fixed.
* Miscellaneous improvements to simd.h ported from master.
* Fix typo that made TIFF files incorrectly name color space metadata
  "oiio::ColorSpace" instead of "oiio:ColorSpace". #1394

Release 1.6.11 (released 1 Mar 2016 -- compared to 1.6.10)
------------------------------------------------
* Fix potential of IBA::computePixelStats (including oiiotool --stats)
  to end up with NaNs due to numerical imprecision. #1333
* Less unnecessary pausing after read errors when falure_retries == 0.
  #1336
* Fix errors in finding the correct locaiton of pugixml.hpp when using
  USE_EXTERNAL_PUGIXML=1. #1339
* Fix build breaks for gcc 6. #1339
* GIF reader failed to set spec full_width, full_height. #1348
* PNM: Fixed byte swapping when reading 16 but PNM files. #1352
* OpenEXR: Improved error reporting for bad tile reads. #1338
* OpenEXR: Fix errors reading tiles for mixed-format EXR files. #1352
* oiiotool --dumpdata:empty=0 now does something for non-deep files: skips
  reporting of pixels where all channels are black. Also fixed errors
  for dumpdata of deep, but non-float, files. #1355
* Fix Filesystem::open() issues with UTF-8 filenames on MinGW. #1353
* Rewrite of FindOpenEXR.cmake. Solves many problems and is simpler.
  No more FindIlmbase.cmake at all. #1346
* Fix build break for older gcc < 4.4 that didn't have immintrin.h.
* Fix bad memory access crash when reading specific JPEG files that were
  written without their comment field including a null character to
  end the string. #1365
* The ffmpeg-based reader had a variety of fixes. #1288
* Python: improve: reading with request for type UNKNOWN returns native
  data in an unsigned char array. Also, requesting HALF returns the half
  bits in an unsigned short array (since there is no 'half' type in Python).
  #1362
* Texture: slight improvement in texture sharpness. #1369
* Update webp testsuite references for new webp version.

Release 1.6.10 (released 1 Feb 2016 -- compared to 1.6.9)
------------------------------------------------
* ImageBufAlgo add, sub, mul, and div, for the varieties that combine
  an image with a (per-channel) constant, now work for "deep" images.
  #1257
* ImageBuf::iterator performance is improved -- roughly cutting in half
  the overhead of iterating over pixels. #1308
* OpenEXR: Fix broken multipart output when parts had different pixel
  data types. #1306,#1316
* Allow oiiotool command-line expression metadata names to contain ':'.
* Fix oiiotool --ch (or IBA::channels) when the channels were only renamed,
  not reordered, the renaming didn't happen properly. #1326
* Fixes for both reading and writing of RLA images that are cropped
  (i.e., data window is a subset of display window). #1224
* Fix build issues on some platforms for SHA1.h, by adding proper include
  of `<climits>`. #1298,#1311,#1312
* Cleanup of include logic in simd.h that fixed build problems for gcc < 4.4.
  #1314
* Fix build breaks for certain 32 bit platforms. #1315,#1322

Release 1.6.9 (released 5 Jan 2016 -- compared to 1.6.8)
------------------------------------------------
* Several varieties of ImageCache and TextureSystem getattribute methods
  were noticed to not be properly declared 'const'. This was fixed.
  #1300 (1.6.9)
* Fix build break against Boost 1.60. #1299,#1300 (1.6.9/1.5.23)
* The Python bindings for ImageCache was overhauled after several
  of the methods were found to be horribly broken. #1300 (1.6.9)
* oiiotool --subimage now allows a subimage name as argument, as well
  as the numeric index. #1271,#1287 (1.6.9)
* TIFF input: erase redundant IPTC:Caption and IPTC:OriginatingProgram
  if they are identical to existing ImageDescription and Software metadata,
  respectively. (1.6.9)
* Fix oiiotool image cache smaller than intended because of typo. (1.6.9)


Release 1.6 (1.6.8 released Dec 21, 2015 -- compared to 1.5.x)
----------------------------------------------
Major new features and improvements:
 * New oiiotool functionality:
    * Expression evaluation/substitution on the oiiotool command line.
      Anything enclosed in braces { } in a command line argument will be
      substituted by the evaluation of the enclosed expression. Expressions
      may be numbers, simple arithmetic (like 'expr+expr'), or retrieving
      image metadata from named images or images on the stack.
      Please see the PDF documentation, Section 12.1 for details and
      examples.
    * --absdiff, --absdiffc compute the absolute difference (abs(A-B)) of
      two images, or between an image and a constant color. #1029 (1.6.0)
    * --abs computes the absolute value of an image.  #1029 (1.6.0)
    * --div, divc divide one image by another (pixel by pixel), or divides
      the pixels of an image by a constant color. #1029 (1.6.0)
    * --addc, --subc, --mulc, --powc are the new names for --cadd, --csub,
      --cmul, and --cpow. The old ones will continue to work but are
      considered depcrected. #1030 (1.6.0)
    * --pattern supports new patterns: "fill" makes a solid, vertical or
       horizontal gradient, or four-corner interpolated image (just like
       the --fill commmand) (1.6.0); "noise" can generate uniform, gaussian,
       or salt & pepper noise (1.6.2).
    * --fill, in addition to taking optional parameter color=... to give a
      solid color for the fill region, now also takes top=...:bottom=... to
      make a vertical gradient, left=...:right=... to make a horizontal
      gradient, and topleft=...:topright=...:bottomleft=...:bottomright=...
      to make a 4-corner gradient. (1.6.0)
    * --noise adds noise to the current image: additive uniform or gaussian
      noise, or making "salt & pepper" noise. (1.6.2)
    * --trim crops the image to the minimal rectangle containing all
      the non-0 pixels. (1.6.3)
    * --autocc : when turned on, automatic color conversion of input files
      into a scene_linear space, and conversion to an appropriate space
      and pixel type upon output. It infers the color spaces based on
      metadata and filenames (looking for OCIO-recognized color space names
      as substrings of the filenames). #1120 (1.6.3)
    * --mad takes three image arguments, multiplies the first two and then
      adds the third to it. #1125 (1.6.3)
    * --invert computes the color inverse (1-value) for color channels.
      #1125 (1.6.3)
    * --colorconfig allows you to specify a custom OCIO configuration file
      (rather than strictly relying on the $OCIO env variable). #1129 (1.6.3)
    * --deepen converts flat images to "deep". #1130 (1.6.3)
    * -n (no saved output) performs all calculations (including timing and
      stats) but does not write any output files to disk. #1134 (1.6.3)
    * --debug prints debugging information, this is now separate from
      -v which just makes more verbose (non-debugging) output. #1134 (1.6.3)
    * --pixelaspect rescales the image to have the given pixel aspect
      ratio. #1146 (1.6.5)
    * --ociofiletransform() implements OpenColorIO "file" transforms.
      #1213 (1.6.5)
 * New ImageBufAlgo functions:
    * absdiff() computes the absolute difference (abs(A-B)) of two images,
      or between an image and a constant color. #1029 (1.6.0)
    * abs() computes the absolute value of an image. #1029 (1.6.0)
    * div() divides one image by another (pixel by pixel), or divides all
      the pixels of an image by a constant color. #1029 (1.6.0)
    * fill() has been extended with new varieties that take 2 colors (making
      a vertical gradient) and 4 colors (one for each ROI corner, for a
      bilinearly interpolated gradient). (1.6.0)
    * noise() injects noise into an image -- uniform, gaussian/normal,
      or salt & pepper noise. (1.6.2)
    * mad() multiplies the first two arguments and then adds the third to
      it. #1125 (1.6.3)
    * invert() computes 1-val. #1125 (1.6.3)
    * deepen() turns a flat RGBA (and optional Z) image into a "deep"
      image. #1130 (1.6.3)
    * ociofiletransform() implements OpenColorIO "file" transforms.
      #1213 (1.6.5)
 * Some open source fonts are now distributed with OIIO (DroidSans,
   DroidSans-Bold, DroidSerif, DroidSerif-Bold, DroidSerif-Italic,
   DroidSerif-BoldItalic, and DroidSansMono), and so those are always
   available to ImageBufAlgo::render_text() and oiiotool --text, on all
   platforms and even if you don't have any other installed fonts on
   your system. DroidSans is now the default font. #1132 (1.6.3)
 * GIF output support (including writing animated GIF images, just write it
   as a multi-subimage file). For example, this works:
      oiiotool foo*.jpg -siappendall -attrib FramesPerSecond 10.0 -o anim.gif
   #1193 (1.6.4)

Public API changes:
 * TypeDesc:
    * New helper methods: is_array(), is_unsized_array(), is_sized_array().
      #1136 (1.6.3)
    * New constructor and fromstring of a string_view, in addition to
      the old versions that took char*. #1159 (1.6.4/1.5.16)
    * New aggregate type: MATRIX33. #1265,#1267 (1.6.6)
 * ImageSpec:
    * ImageSpec::metadata_val() is now static, rather than simply const,
      since it doesn't need access to *this at all. #1063 (1.6.1)
    * Added a new variety of find_attribute that takes a temporary
      ImageIOParameter as scratch space. The advantage of this call is
      that it can retrieve items from the named ImageSpec fields, such
      as "width", "full_x", etc. Also, the get_int_attribute,
      get_float_attribute, and get_string_attribute can now retrieve
      these fixed fields as well.  #1063 (1.6.1)
 * ImageInput & ImageOutput:
    * New ImageOutput::supports() tags: supports("alpha") should be true
      for image formats that support an alpha channel, supports("nchannels")
      should be true for output formats that support an arbitrary number
      of output channels. (1.6.2/1.5.13)
    * ImageInput and ImageOutput supports() method has been changed to accept
      a string_view (rather than a const std::string&), and return an int
      (rather than a bool). (1.6.2)
    * ImageInput and ImageOutput have added destroy() static
      methods. They are just wrappers around 'delete', but can help you
      to ensure that II and IO objects are deleted on the same side of a
      DLL boundary as where they were created. (Helps with using OIIO
      from DLL-based plugins on Windows.)  (1.6.3)
    * New ImageInput query: "procedural" -- returns 1 if the ImageInput may
      not correspond to an actual file. #1154 (1.6.4/1.5.16)
    * ImageInput and ImageOutput's error() method is changed from protected
      to public, making it easier for an app to set an error on a reader
      or writer. (1.6.4)
    * ImageOutput::copy_to_image_buffer is a helper function that
      generalizes the existing copy_tile_to_image_buffer, but for any
      rectangle.  #1193 (1.6.4)
    * ImageInput::read_image() variant that takes a channel range to
      read just a subset of the channels present. #1222 (1.6.5)
    * ImageInput and ImageOutput now have new method threads(n) that sets
      the thread "fan-out" for the ImageInput or ImageOutput individually,
      overriding any global attribute("threads"). #1259 (1.6.6)
 * ImageBuf:
    * Add make_writeable(), which forces ImageCache-backed read-only
      ImageBuf to read into locally allocated pixels so they can be
      subsequently altered. #1087 (1.6.2)
    * ImageBuf::Iterator has added set_deep_samples() and set_deep_value()
      methods. (1.6.3)
    * ImageBuf::set_pixels() now provides a way to set an arbitrary
      rectancle of an ImageBuf from raw values. #1167 (1.6.4)
    * ImageBuf::get_pixels() now has a variety that takes an ROI to
      describe the rectangle of pixels being requested. #1167 (1.6.4)
    * ImageBuf now has new method threads(n) that sets the thread
      "fan-out" for the ImageInput or ImageOutput individually,
      overriding any global attribute("threads"). #1259 (1.6.6)
 * ImageCache/TextureSystem:
    * Clarified in the docs that TextureSystem::get_texture_info and
      ImageCache::get_image_info "exists" queries should return true, and
      place in *data the value 1 or 0 depending on whether the image exists
      and can be read. (1.6.0/1.5.10)
    * Added handle-based versions of TextureSystem get_texture_info(),
      get_imagespec(), imagespec(), and get_texels(), in addition to the
      existing name-based versions of those methods. Note that
      texture(), environment(), and texture3d() already had both
      name-based and handle-based varieties. #1057 (1.6.1) #1083 (1.6.2)
    * Add create_thread_info() and destroy_thread_info() methods that
      allow an app to manage the per-thread records needed by the IC.
      #1080 (1.6.2)
    * Added ImageCache get_perthread_info() and get_image_handle() to
      return opaque perthread and file handle pointers, much like
      TextureSystem already had, and added handle-based versions of
      get_image_info(), get_imagespec(), imagespec(), get_pixels(), and
      get_tile(), in addition to the existing name-based versions of
      those methods. #1057 (1.6.1)
    * ImageCache get_tile and get_pixels have new varieties that let you
      request channel begin/end range. This allows you to control which
      channel ranges are in the cache, and thus be much more efficient
      with cache storage when only a few channels are needed from a file
      with many channels. #1226 (1.6.5)
 * ImageBufAlgo:
    * New ImageBufAlgo functions: abs, absdiff, div, fill, noise, mad,
      invert, deepen, ociofiletransform.
    * nchannels() now takes an 'nthreads' parameters, just like all the
      other ImageBufAlgo functions. #1261 (1.6.6)
 * Python bindings:
    * Added previously-M.I.A. ImageSpec::erase_attribute(). #1063 (1.6.1)
    * ImageSpec.set_channel_formats() now works when the channel
      type lists are either TypeDesc, in addition to the existing support
      for BASETYPE. #1113 (1.6.3/1.5.13)
    * Added Python bindings for DeepData and deep reads (ImageInput) and
      writes (ImageOutput), as well as additional DeepData and ImageBuf
      methods to fully match the C++ API. #1113 #1122 (1.6.3/1.5.13)
    * ImageBuf.set_pixels, and ImageBuf.get_pixels with ROI. #1167,1179 (1.6.4)
    * Change Python ImageOutput bindings to simplify the write_* methods.
      They no longer take both a TypeDesc and an array; it can figure out
      the type from the array itself. Also get rid of the stride parameters,
      which weren't useful in a Python context. #1184 (1.6.4)
    * ImageBufAlgo colorconvert, ociolook, and ociodisplay now take an
      optional string colorconfig argument. #1187 (1.6.4)
    * Fix missing Python bindings for global OIIO::getattribute(). #1290
      (1.6.8)
 * The ColorConfig wrapper for OCIO functionality has been extended to
   parse color names from filename strings, and to report the recommended
   pixel data type for a color space. #1129 (1.6.3)
 * C++11 definitions: oiioversion.h defines OIIO_BUILD_CPP11 as nonzero
   if OIIO itself was built in C++11 (or later) mode, and platform.h
   defines OIIO_USING_CPP11 as nonzero if at this moment C++11 (or
   later) mode is detected. Note that these can differ if one set of
   compiler flags was used to build OIIO, and a different set is used to
   build a project that uses OIIO headers. #1148 (1.6.4)
 * Renamed the "fps" standard metadata to "FramesPerSecond. #1193 (1.6.4)
 * Removed deprecated header "string_ref.h" (use string_view.h). (1.6.1)
 * oiioversion.h: Renamed the namespace macros OIIO_NAMESPACE_ENTER/EXIT to
   OIIO_NAMESPACE_BEGIN/END, and roll the braces into it. #1196 (1.6.4)
 * array_view.h: Refactor array_view to be more in line with what is slated
   for C++17, in particular it is now templated on Rank and so can be a view
   to a multi-dimensional array. Also change array_view_strided to have
   strides measured in units of sizeof(T), not bytes (to keep with C++17).
   This also adds coordinate.h to give definitions for the offset<>,
   bounds<>, and bounds_iterator<> templates used by array_view. #1205
   (1.6.4)
 * Add top-level OIIO::get_int_attribute(), get_float_attribute(), and
   get_string_attribute() helpers, similar to how they work in many
   of the classes. #1283 (1.6.7)

Fixes, minor enhancements, and performance improvements:
 * oiiotool
    * Bug fix for frame sequences -- could crash in Windows. #1060 (1.6.1)
    * Gracefully handle requests to save an image with more channels than
      the output file format can handle. Instead of being a fatal error,
      now it's just a warning, and extra channels are dropped. It tries to
      to find R, G, B, and A channels, saving them. If those names are
      not found, it just saves the first 3 (or 4) channels. #1058 (1.6.1)
    * Improve error messages when files can't be read. It is now easier
      to to distinguish files that don't exist from those that are an
      unknown format from those that are corrupted or have read
      errors. #1065 (1.6.1)
    * Flag errors properly when -d specifies an unknown data format name.
      #1077 (1.6.2/1.5.13)
    * oiiotool numeric wildcard improvement: allow more digits to match.
      #1082 (1.6.2/1.5.13)
    * Bug fix: input file data format didn't always end up in the output.
      (1.6.3)
    * --channels bugs were fixed when dealing with "deep" images. (1.6.3)
    * All the color space conversion operations run much faster now,
      since the underlying IBA::colorconvert() has been parallelized. (1.6.3)
    * --crop logic bug fixed in cases where the crop region was the same
      size as the original pixel data window. #1128 (1.6.3)
    * oiiotool now gives proper error messages when asked to perform
      unsupported operations on deep images. (1.6.3)
    * Bug fix: --frames incorrectly overrode explicit frame sequence
      wildcards on the command line. #1133 (1.6.3)
    * --crop, --trim, and --autotrim have been extended to work on
      "deep" images. #1137 (1.6.3)
    * For "procedural" ImageInputs, don't give "file doesn't exist"
      errors. (1.6.4)
    * Suppress output/copying of "textureformat" metadata inherited from
      input if it's not plausibly still a valid texture (i.e., if it's
      no longer tiled or MIPmapped). #1206 (1.6.4)
    * oiiotool's full help message lists all supported formats. #1210 (1.6.5)
    * oiiotool --help prints a briefer help screen. Use --help -v for
      the full-detail help. #1214 (1.6.5)
    * Bug fix in --fit when the image didn't need to be resized.
      #1227 (1.6.5/1.5.21)
    * Bug fix in --ch for "deep" files when the channel reordering is
      the same as it already was. #1286 (1.6.7)
 * ImageBufAlgo:
    * compare() (and therefore oiiotool -diff and idiff) did not notice
      image differences when the pixels that differed had NaN or NaN or
      Inf values! Now it is right. #1109 (1.6.3/1.5.13)
    * channels() bugs were fixed when dealing with "deep" images. (1.6.3)
    * colorconvert() has been parallelized, and thus on most systems will
      now run much faster. (1.6.3)
    * render_text() handles UTF-8 input. #1121 (1.6.3)
    * colorconvert(), ociodisplay(), and ociolook() have new varities that
      accept an optional ColorConfig, rather than having no choice but to
      construct a new one internally. (1.6.3)
    * nonempty_region() and crop() have been extended to handle "deep"
      images. #1137 (1.6.3)
    * Fix bug in fft() -- was not always zeroing out the imaginary channel.
      #1171 (1.6.4/1.5.17)
    * Fixed uninitialized variable bugs with rangecompress() and
      rangeexpand() when using luma. #1180 (1.6.4)
    * The lanczos3, radial-lanczos, and catrom filters have been change
      from fixed-width to fully scalable. This fixes artifacts that
      occur when using them as upsizing filters. #1228,#1232 (1.6.5/1.5.21)
 * maketx, TextureSystem, and ImageCache:
    * TextureSystem/IC now directly stores uint16 and half pixel data in
      the cache rather than converting internally to float for tile storage,
      thus effectively doubling the cache capacity for files of those
      formats. (1.6.3)
    * Fix broken bicubic texture sampling with non-power-of-two sized
      tiles. #1035 (1.6.0/1.5.10)
    * maketx: when the source image was a crop (data window != display
      window), and the sharpening filters were used, it would
      incorrectly issue an "unknown filter name" error. #1059 (1.6.1/1.5.12)
    * maketx: Flag errors properly when -d specifies an unknown data
      format name. #1077 (1.5.13)
    * maketx now writes to a temporary file, then moving it to the final
      requested output filename only when the write completed without
      error.  This prevents situations where maketx crashes or is killed
      and leaves behind a file that looks correct but is actually
      corrupted or truncated. #1072 (1.6.2/1.5.13)
    * TextureSystem bug fix that occasionally resulted in NaN in the
      alpha channel result when looking up from 3-channel images. #1108
      (1.6.3/1.5.13)
    * maketx --runstats prints runtime staticstics (deprecating --stats).
      #1152 (1.6.4)
    * Fixed trilinear MIPmap texture lookups that gave invalid alpha fill.
      #1163 (1.6.4/1.5.16)
    * The lanczos3, radial-lanczos, and catrom filters have been change
      from fixed-width to fully scalable. This fixes artifacts that
      occur when using them as upsizing filters. #1228,#1232 (1.6.5)
    * Texture cache memory efficiency is much better for the special case
      of accessing just a few channels from a texture file with large
      numbers of channels. #1226 (1.6.5)
    * Eliminate spurious ImageCache invalidation just because the shared
      cache is requested again. #1157 (1.6.4/1.5.16)
    * Statistics output also shows all the option setting values. #1226 (1.6.5)
    * Data copy error in ImageCache::get_pixels for partial-channel-set
      copies. #1246 (1.6.5)
    * maketx -u now remakes the file if command line arguments or OIIO
      version changes, even if the files' dates appear to match.
      #1281 (1.6.8)
 * GIF:
    * Write support! #1193 (1.6.4)
    * On input, renamed "fps" metadata to "FramesPerSecond". #1193 (1.6.4)
 * IFF:
    * Fix botched output of 16 bit uncompressed data. #1234 (1.6.5/1.5.21)
    * Make "rle" compression the default. #1234 (1.6.5/1.5.21)
 * JPEG:
    * Now properly read/write xdensity and ydensity (what OIIO and TIFF
      call "XResolution" and "YResolution" and, therefore,
      "PixelAspectRatio". #1042 #1066 (1.6.0, 1.6.1)
    * Support JPEG files encoded as CMYK (by converting to RGB upon read)
      #1044 (1.6.1)
    * Fix misdeclared supports() which would make the JPEG plugin appear
      to not support exif or iptc. #1192 (1.6.4)
 * JPEG-2000:
    * Fix handling of un-premultiplied alpha (which is dictated by the
      JPEG-2000 spec). (1.6.3)
    * Fix reading of YUV-encoded files. (1.6.3)
    * Read and write the ICC profile, if present. (1.6.3)
    * Handle all bit depth precisions properly (previously only 8, 10,
      12, and 16 were right). (1.6.3)
    * Set the full/display window correctly. (1.6.3)
    * Deal with differing per-channel data windows and sampling rates. (1.6.3)
 * OpenEXR:
    * Improved handling of density and aspect ratio. #1042 (1.6.0)
    * Fix read_deep_tiles() error when not starting at the image origin.
      #1040 (1.6.0/1.5.10)
    * Fix output of multi-part exr file when some parts are tiled and
      others aren't. #1040 (1.6.0/1.5.10)
    * write_tile() with AutoStride calculated the wrong default strides
      for "edge" tiles when the image width or length was not an integer
      multiple of the tile size. Also clarified the PDF and imageio.h
      docs in how they explain strides for this case. #1055 (1.6.1/1.5.12)
    * Fix bugs in reading deep OpenEXR images with mixed channel types.
      #1113 (1.6.3/1.5.13)
    * OpenEXR output supports("deepdata") now correctly returns 'true'.
      #1238 (1.6.5/1.5.21)
    * A separate global OIIO::attribute("exr_threads") sets the thread pool
      size for OpenEXR's libIlmImf, independent of the OIIO thread fan-out
      attribute OIIO::attribute("threads"). #1244 (1.6.5)
    * Correctly read and write Matrix33 and double (scalar, 2d, 3d, m33, m44)
      metadata. #1265,#1267 (1.6.6)
    * Recognize AR/AG/AB channel names in addition to the old RA/RG/RB
      #1277 (1.6.6)
 * PNG:
    * Writing PNG files now honors the PixelAspectRatio metadata.
      #1142 (1.6.3)
 * PFM:
    * PFM (float extension of PNM) was incorrectly flipped top to bottom.
      Now fixed. #1230 (1.6.5)
 * PSD:
    * Better error handling for files lacking "global layer mask info"
      or "additional layer info". #1147 (1.6.4/1.5.18)
    * Additional PSD signatures for global additional layer info.
      #1147 (1.6.4/1.5.18)
    * Better error handling when dealing with an empty layer mask.
      #1147 (1.6.4/1.5.18)
 * TIFF:
    * Improved handling of density and aspect ratio. #1042 (1.6.0)
    * Improved proper handling of the interplay between "XResolution",
      "YResolution", and "PixelAspectRatio". #1042 (1.6.0)
    * TIFF output: recognize special "tiff:write_exif" metadata, which when
      present and set to 0, will skip writing the Exif directory into the
      TIFF file. This can be helpful when you expect the resulting TIFF
      file to be read with very old versions of libtiff. #1185 (1.6.4/1.5.18)
    * Correct read and write of JPEG-compressed TIFF. #1207 (1.6.4)
    * Correct support for reading LAB, LOG, YCbCr, subsampled chroma.
       #1207 (1.6.4)
    * Make robust to strange TIFF files that have unexpected MIP
      level-to-MIP level changes in planarconfig, photometric, palette,
      extrasamples, etc. #1220,1221 (1.6.5/1.5.20)
    * Support output of 2, 4, 10, and 12 bit unsigned ints into TIFF files.
      #1216 (1.6.5)
    * Make TIFF reading more robust to certain subimage-to-subimage
      changes that were thought to be invariant. #1221 (1.6.5)
    * CMYK is properly read and written. Upon read, CMYK is auto-converted
      to RGB (and the "tiff:ColorSpace" metadata is set to "CMYK"). For
      output, if "tiff:ColorSpace" metadata is set and nonzero, the RGB
      passed in will be auto-converted to CMYK upon writing.
      #1233 #1245 (1.6.5)
    * Recognize Exif tags in the main directory, not only the special
      Exif directory. #1250 (1.6.5)
    * Fix bug in read_scanlines when reading TIFF files with UNassociated
      alpha and unusual ystride values. #1278 (1.6.6)
 * ImageBuf iterator constructors with 0-size ranges or ROIs have been
   fixed to look like they are immediately done(). #1141 (1.6.3)
 * Fix bug in internal convert_image() that could corrupt certain image
   copying of non-contiguous data layouts. #1144 (1.6.3)
 * Also search for OIIO plugins in [DY]LD_LIBRARY_PATH. #1153 (1.6.4/1.5.16)
 * Nuke plugin: don't crash with NULL Knob* in TxReaderFormat::setMipLabels.
   #1212 (1.6.5/1.5.20)
 * idiff -q results in quiet mode -- output nothing for success, only
   minimal errors to stderr for failure. #1231 (1.6.5)

Build/test system improvements:
 * Python plugin is now build as a cmake "module" rather than "library",
   which fixes some things on OSX. #1043 (1.6.0/1.5.10)
 * Various build fixes for Windows. #1052 #1054 (1.6.1)
 * New CMake build-time option to specify the default plugin search path.
   #1056 (1.6.1/1.5.12)
 * Fix build breaks for very old versions of Ilmbase (1.6 and earlier)
   that lack a definition of V4f used by our simd.h. #1048 (1.6.1/1.5.11)
 * Fix signed/unsigned warning on 32 bit platforms in jpeginput.cpp.
   #1049 (1.6.1/1.5.11)
 * New CMake build-time option to specify the default plugin search path.
   #1056 (1.6.1/1.5.12)
 * Fix gcc 5.0 compiler warning in PtexHalf.cpp. (1.6.1/1.5.12)
 * Remove dependency of OpenSSL by default. #1086 (1.6.2/1.5.13)
 * Fix warnings when compiling with C++11. (1.6.3/1.5.13)
 * Dont link Python framework on OSX. #1099 (1.6.3/1.5.13)
 * Changed the way testtex warps the image to give faux perspective to
   test texture mapping. (1.6.3)
 * Build-time USE_SIMD=... has been changed from accepting a single tag to
   a comma-separated list of feature options. So you can, for example, do
   make USE_SIMD=avx,f16c ...  (1.6.3)
 * make USE_NINJA=1 causes CMake to build Ninja build files instead of
   Makefiles (they execute much faster, espectially for incremental builds).
   #1158 (1.6.4)
 * PSD & JPEG plugins fixes for Win32 compilation. #1150 (1.6.4/1.5.16)
 * Fix Nuke plugin build files to not do anything if USE_NUKE=0.
   #1156 (1.6.4/1.5.16)
 * Builds now produce much less console output by default (use VERBOSE=1
   to get all the details, most of which is only useful when debugging
   broken builds). #1162 (1.6.4)
 * Fix support for older ffmpeg version on Ubuntu 14.04. #1168 (1.6.4/1.5.17)
 * Build-time fixes for Nocona CPUs that have SSE3 without SSSE3.
   #1175 (1.6.4/1.5.17)
 * ustring internals fixes for gcc 5.x changs to std::string ABI. #1176 (1.6.4)
 * Fixes for clean build with clang 3.6. #1182,1183 (1.6.4)
 * Fix signed/unsigned comparison error. #1186 (1.6.4)
 * Top-level Makefile option USE_OPENCV=0 to turn off even searching for
   OpenCV components. #1194 (1.6.4/1.5.18)
 * If a system-installed (external) PTex implementation is found, use
   it.  Only use the "bundled" version if no other is found. Also add a
   top-level USE_PTEX=0 that will skip PTex support, even if the library
   is found.  #1195,1197 (1.6.4)
 * Fix compiler warnings about int vs size_t mismatches. 1199 (1.6.4)
 * Improve C++11 and C++14 readiness. #1200
 * Fix build break with certain new versions of libraw. #1204 (1.6.4/1.5.19)
 * Fix build warnings for new Apple tools release that upgrades the standard
   clang release. #1218 (1.6.5/1.5.20)
 * When compiling in C++11 mode, std::unordered_map, mutex,
   recursive_mutex, lock_guard, bind, ref, cref, thread, shared_ptr will
   be used rather than boost equivalents, and our own thread_group and
   intrusive_ptr are now used rather than the boost equivalents. We
   believe that this completely removes all Boost headers and types from
   the OIIO public APIs when in C++11 mode. (Though internals still use
   Boost in some cases.) #1262 #1266 (1.6.6)
 * We are now set up to use Travis-CI (https://travis-ci.org) for continuous
   integration / automatic builds of all merges and pull requests.
   #1268, #1269, #1273 (1.6.6)
 * Don't install fonts if USE_FREETYPE is disabled. #1275 (1.6.6)
 * Use ccache for builds when detected and safe (unless USE_CCACHE=0).
   #1274,#1285 (1.6.7)
 * Failed tests now print their non-matching text output to the console
   when doing 'make test'. This makes it much easier to spot most errors.
   #1284 (1.6.7)

Developer goodies / internals:
 * Strutil additions: parse_until, parse_nested (1.6.1), repeat
   (#1272/1.6.6/1.5.21).
 * Give Strutil::parse_string an option to not strip surrounding quotes.
   (1.6.4)
 * Made TypeDesc::equivalent accept comparisons of arrays of unspecified
   length with ones of definite length. #1072  (1.6.2/1.5.13)
 * Add Filesystem::rename() utility. #1070  (1.6.2/1.5.13)
 * New SIMD methods: insert<>, xyz0, vreduce_add, dot, dot3, vdot, vdot3,
   AxBxCxDx, blend0not (1.6.2)
 * array_view enhancements that let you initialize an `array_view<const float>`
   from a const `std::vector<float>&`.  #1084 (1.6.2/1.5.14)
 * hash.h contains several new hashes in namespaces 'OIIO::xxhash' and
   'OIIO::farmhash'. Also, Strutil::strhash now uses farmhash rather than
   the Jenkins one-at-a-time hash, bringing big speed improvements
   (including ustring creation). Beware that the strhash value returned
   will be different than they were before. #1090 (1.6.3)
 * fmath: safe_fast_pow improves the precision of its results for
   special cases of pow(x,1) and pow(x,2). #1094 (1.6.3/1.5.13)
 * Added TypeDesc::TypeHalf(). #1113 (1.6.3/1.5.13)
 * thread.h: our atomic types have had their API adjusted somewhat to
   more closely conform to C++11's std::atomic. (1.6.3)
 * ustring's internals and underlying hash table have been overhauled,
   yielding much higher performance, especially when many threads are
   simultaneously creating ustrings. (1.6.3)
 * ROI improvement: make intersection & union robust to uninitialized ROIs
   as arguments. (1.6.3)
 * osdep.h is deprecated. Use platform.h instead. (1.6.3)
 * The DISPATCH_TYPES utility macros used internally by IBA have been
   improved, and in particular the DISPATCH_COMMON_TYPES now handle ALL
   types ("uncommon" ones are silently converted to float). (1.6.3)
 * platform.h moves the endian functions into the OIIO namespace. (1.6.3)
 * platform.h adds functions for runtime query of CPU capabilities. (1.6.3)
 * simd.h: float4 and int4 can now construct and load from unsigned short*,
   short*, unsigned char*, char*, and 'half'. (1.6.3)
 * Strutil::utf8_to_unicode (1.6.3)
 * Filesystem::current_path(). #1124 (1.6.3/1.5.21)
 * Filesystem enumerate_file_sequence and scan_for_matching_filenames
   have been modified to clear their result vectors rather than simply
   assume they are empty. #1124 (1.6.3)
 * oiiotool internals have been refactored to be class-oriented and move
   a lot of boilerplate repeated in each op to be part of the base
   class. #1127 (1.6.3)
 * timer.h: Timer and ScopedTimer have changed slightly. This isn't used
   in any public OIIO APIs, but may affect 3rd party programs that like
   to use OIIO's timer.h for convenience. #1201 (1.6.4/1.5.19)
 * dassert.h: added OIIO_STATIC_ASSERT macros for static
   assertion. Doesn't affect existing OIIO apps since they are new
   additions, but feel free to use them! #1202 (1.6.4/1.5.19)
 * New unit test for imagecache. #1246 (1.6.5)
 * Sysutil::hardware_concurrency() and physical_concurrency(). #1263
   (1.6.6/1.5.21)



Release 1.5.24 (1 Mar 2016) -- compared to 1.5.23)
---------------------------------------------------
* Fix oiiotool --dumpdata, didn't work properly for non-float files.
* Fix broken OpenEXR multi-part output when parts have different pixel types.
* Update webp testsuite references for new webp version.

Release 1.5.23 (28 Dec 2015) -- compared to 1.5.22)
---------------------------------------------------
* Fix build break against Boost 1.60. #1299,#1300

Release 1.5.22 (16 Dec 2015) -- compared to 1.5.21)
---------------------------------------------------
* Deep OpenEXR: recognize the newer AR/AG/AB channel name convention. #1277
* Fix ffmpeg plugin compilation in some configurations. #1288
* Bug fix: TIFF read_scanlines of files with unassociated alpha didn't
  honor the 'ystride' parameter and could run off the end of the buffer
  for nonstandard stride arranagements. #1278
* Fix missing Python bindings for global OIIO::getattribute(). #1290

Release 1.5.21 (1 Dec 2015) -- compared to 1.5.20)
---------------------------------------------------
* Bug fix in --fit when the image didn't need to be resized. #1227
* IFF: Fix botched output of 16 bit uncompressed data. #1234
* IFF: Make "rle" compression the default for output. #1234
* OpenEXR output supports("deepdata") now correctly returns 'true'. #1238
* The lanczos3, radial-lanczos, and catrom filters have been changed
  from fixed-width to fully scalable. This fixes artifacts that
  occur when using them as upsizing filters. #1228,#1232
* Filesystem::current_path(). #1124
* Sysutil::hardware_concurrency() and physical_concurrency(). #1263
* Strutil::repeat() #1272

Release 1.5.20 (28 Sep 2015) -- compared to 1.5.19)
---------------------------------------------------
* Nuke plugin: don't crash with NULL Knob* in TxReaderFormat::setMipLabels.
  #1212
* Fix build warnings for new Apple tools release that upgrades the standard
  clang release. #1218
* Make TIFF reader robust to strange TIFF files that have unexpected MIP
  level-to-MIP level changes in planarconfig, photometric, palette,
  extrasamples, etc. We previously assumed these things would never vary
  between MIP levels of the same file, and Murphy called our bluff. #1220,1221

Release 1.5.19 (8 Sep 2015) -- compared to 1.5.18)
--------------------------------------------------
* Fix compile warnings on some platforms/compilers.
* Fix build break with certain new versions of libraw. #1204
* Internals: Timer and ScopedTimer have changed slightly. This isn't used
  in any public OIIO APIs, but may affect 3rd party programs that like
  to use OIIO's timer.h for convenience. #1201
* Internals: dassert.h has added OIIO_STATIC_ASSERT macros for static
  assertion. Doesn't affect existing OIIO apps since they are new
  additions, but feel free to use them! #1202

Release 1.5.18 (4 Aug 2015) -- compared to 1.5.17)
---------------------------------------------------
* PSD input improvements: better error handling for files lacking "global
  layer mask info" or "additional layer info"; additional PSD signatures
  for global additional layer info; better error handling when dealing
  with an empty layer mask. #1147
* TIFF output: recognize special "tiff:write_exif" metadata, which when
  present and set to 0, will skip writing the Exif directory into the TIFF
  file. This can be helpful when you expect the resulting TIFF file to be
  read with very old versions of libtiff. #1185
* Top-level Makefile option USE_OPENCV=0 to turn off even searching for
  OpenCV components. #1194

Release 1.5.17 (13 Jul 2015) -- compared to 1.5.16)
---------------------------------------------------
* Fix support for older ffmpeg version on Ubuntu 14.04. #1168
* Fix bug in fft -- was not always zeroing out the imaginary channel. #1171
* Build-time fixes for Nocona CPUs that have SSE3 without SSSE3. #1175
* ustring fixes for new gcc (5.1+) and new std::string ABI. #1176
* Fixes for unit test timer_test for new OSX versions with timer
  coalescing. #1181
* Fix bugs with rangecompress and rangeexpand when using luma. #1180
* Fixes for clean build when using clang 3.6. #1182

Release 1.5.16 (11 Jun 2015) -- compared to 1.5.15)
---------------------------------------------------
* PNG writes now honor PixelAspectRatio attribute. #1142
* Build fixes for Visual Studio 2010 #1140
* PSD & JPEG plugins fixes for Win32 compilation.
* Also search for OIIO plugins in [DY]LD_LIBRARY_PATH. #1153
* Give Strutil::parse_string an option to not strip surrounding quotes.
* Fix Nuke plugin build files to not do anything if USE_NUKE=0  #1156
* New ImageInput query: "procedural" -- returns 1 if the ImageInput may
  not correspond to an actual file. #1154
* TypeDesc has a new constructor and fromstring of a string_view, in
  addition to the old versions that took char*. #1159
* Eliminate spurious ImageCache invalidation just because the shared
  cache is requested again. #1157
* Fixed trilinear MIPmap texture lookups that gave invalid alpha fill. #1163
* Filesystem: sequence matching should clear results arrays upon start.

Release 1.5.15 (11 May 2015) -- compared to 1.5.14)
---------------------------------------------------
* Bug fix with IBA::channels() with deep data with UINT channels.
* Fix TypeDesc compatibility with OSL.
* Misc WIN32 / VS2010 fixes.
* Fix incorrect logic in convert_image with certain channel types and
  strides. #1144

Release 1.5.14 (10 April 2015) -- compared to 1.5.13)
----------------------------------------------
* fmath: save_fast_pow improves the precision of its results for
  special cases of pow(x,1) and pow(x,2). #1094 (1.5.13)
* Fix warnings when compiling with C++11. (1.5.13)
* Dont link Python framework on OSX. #1099 (1.5.13)
* Improve IBA::compare() (and therefore oiiotool -diff and idiff) when
  the images being compared have NaN or Inf values. #1109 (1.5.13)
* TextureSystem bug fix that occasionally resulted in NaN in the alpha
  channel result when looking up from 3-channel images. #1108 (1.5.13)
* Added TypeDesc::TypeHalf(). #1113 (1.5.13)
* Fix IBA::channels() bugs when dealing with "deep" images. #1113 (1.5.13)
* Python ImageSpec.set_channel_formats() now works when the channel
  type lists are either TypeDesc, in addition to the existing support
  for BASETYPE. #1113 (1.5.13)
* Added Python bindings for DeepData and deep reads (ImageInput) and
  writes (ImageOutput). #1113 (1.5.13)
* Fix bugs in reading deep OpenEXR images with mixed channel types.
  #1113 (1.5.13)
* Fix bug in IBA::convolve() for the case when the kernel image passed
  is not a float image. #1116 (1.5.13)

Release 1.5.13 (10 Mar 2015) -- compared to 1.5.12)
----------------------------------------------
 * oiiotool: Bug fix for frame sequences -- could crash in Windows. #1060
 * New ImageOutput::supports() tags: supports("alpha") should be true for
   image formats that support an alpha channel, supports("nchannels") should
   be true for output formats that support an arbitrary number of output
   channels. #1058
 * oiiotool: Gracefully handle requests to save an image with more channels
   than the output file format can handle. Instead of being a fatal error,
   now it's just a warning, and extra channels are dropped. It tries to
   to find R, G, B, and A channels, saving them. If those names are
   not found, it just saves the first 3 (or 4) channels. #1058
 * Improved handling of "PixelAspectRatio" for JPEG, TIFF, and OpenEXR.
   #1042 #1066
 * oiiotool: Improve error messages when files can't be read. It is now
   easier to to distinguish files that don't exist from those that
   are an unknown format from those that are corrupted or have read
   errors. #1065
 * maketx now writes to a temporary file, then moving it to the final
   requested output filename only when the write completed without error.
   This prevents situations where maketx crashes or is killed and leaves
   behind a file that looks correct but is actually corrupted or
   truncated. #1072
 * Python: added previously-M.I.A. ImageSpec.erase_attribute(). #1063
 * Add Filesystem::rename() utility. #1070
 * Made TypeDesc::equivalent accept comparisons of arrays of unspecified
   length with ones of definite length. #1072
 * oiiotool & maketx have improved error message when unknown data format
   names are requested with "-d". #1077
 * oiiotool numeric wildcard improvement: allow more digits to match. #1082
 * Remove dependency of OpenSSL by default. #1086

Release 1.5.12 (11 Feb 2015) -- compared to 1.5.11)
----------------------------------------------
* Various build fixes for Windows. #1052 #1054
* New CMake build-time option to specify the default plugin search path.
  #1056 (1.5.12)
* OpenEXR: fixed write_tile() with AutoStride calculated the wrong
  default strides for "edge" tiles when the image width or length was
  not an integer multiple of the tile size. Also clarified the PDF and
  imageio.h docs in how they explain strides for this case. #1055 (1.5.12)
* maketx: when the source image was a crop (data window != display window),
  and the sharpening filters were used, it would incorrectly issue an
  "unknown filter name" error. #1059 (1.5.12)
* Fix gcc 5.0 compiler warning in PtexHalf.cpp. (1.5.12)

Release 1.5.11 (28 Jan 2015) -- compared to 1.5.10)
----------------------------------------------
* Fix build breaks for very old versions of Ilmbase (1.6 and earlier)
  that lack a definition of V4f used by our simd.h. #1048
* Fix signed/unsigned warning on 32 bit platforms in jpeginput.cpp. #1049


Release 1.5 (26 Jan 2015) -- compared to 1.4.x
----------------------------------------------
Major new features and improvements:
* New oiiotool functionality/commands:
   * --rotate90, --rotate180, --rotate270 rotate the image in 90 degree
     axially-aligned increments with no filtering. (1.5.2)
   * --reorient will perform whatever series of rotations or flips are
     necessary to move the pixels to match the "Orientation" metadata that
     describes the desired display orientation. (1.5.2)
   * --autoorient will automatically do the equivalent of --reorient on
     every image as it is read in, if it has a nonstandard orientation.
     (This is generally a good idea to use if you are using oiiotool to
     combine images that may have different orientations.) (1.5.2)
   * --rotate rotates an image by arbitrary angle and center point,
     with high-quality filtering. #932 (1.5.3)
   * --warp transforms an image using a 3x3 matrix, with high-quality
     filtering. #932 (1.5.3)
   * --median performs a median filter. (1.5.4)
* New ImageBufAlgo functions:
   * rotate90(), rotate180(), rotate270() rotate the image in 90 degree
     axially-aligned increments with no filtering. (1.5.2)
   * reorient() will perform whatever series of rotations or flips are
     necessary to move the pixels to match the "Orientation" metadata that
     describes the desired display orientation. (1.5.2)
   * rotate() performs rotation with arbitrary angle and center point,
     with high-quality filtering. #932 (1.5.3)
   * warp() transforms an image by a 3x3 matrix, with high-quality
     filtering. #932 (1.5.3)
   * median_filter performs a median filter. (1.5.4)
* Significant internal speedups by utilizing SIMD instructions (SSE) in
  the TextureSystem (1.5.5 / #948, 1.5.6 / #990). To use this to its
  fullest extent, build OIIO with the make/cmake option USE_SIMD=arch,
  where arch is sse2, ssse3, sse4.1, sse4.2, depending on what machines
  you'll be deploying to. (Note that x86_64 automatically implies at
  least sse2.) We're finding that this has approximately doubled the
  speed of the math part of texture mapping (it doesn't speed up the disk
  I/O, of course).  (1.5.5)
* Basic support for many movie files via a plugin using 'ffmpeg'. Works
  with avi, mov, qt, mp4, m4a, 3gp, 3g2, mj2, m4v, mpg, and more.  Movie
  files simply look like multi-image files to OIIO. There isn't really
  support for audio yet, and although this lets you retrieve and process
  individual frames of a movie file, OIIO is still not meant to be a
  video-processing library. Currently, these formats can be read, but
  there is no write support (maybe coming soon). #928 #1010 (1.5.5)
* Nuke plugins -- a txReader plugins that will read OIIO texture files,
  and a txWriter that will output proper (tiled & mip-mapped) texture files
  from Nuke. Contributed by Nathan Rusch / Luma Pictures. #973 (1.5.6)

Public API changes:
* TextureSystem API calls have been modified: the nchannels, dresultds,
  dresultdt fields have been removed from TextureOpt and are now passed
  explicitly as arguments to texture(), environment(), and texture3d().
  Some long-deprecated methods of TextureSystem have been removed, and
  also some new API calls have been added that provide multi-point
  texturing given a TextureHandle/PerThread rather than a ustring
  filename.  (1.5.5) (#948)
* New filters available to direct users of filter.{h,cpp} and for
  ImageBufAlgo, oiiotoo, and maketx: "cubic", "keys", "simon", "rifman".
  (1.5.0/1.4.9) (#874)
* Global attribute "read_chunk" can set the default number of scanlines
  read at a time internally by read_image() calls. #925
* The Python ImageBuf class did not expose an equivalent to the C++
  ImageBuf::get_pixels. This is exposed now. #931 (1.5.3)
* The Filter API now uses string_view. (1.5.3)
* ImageBuf and ImageBufAlgo now use string_view extensively. (1.5.4)
* ImageInput::supports() and ImageOutput::supports() now accept new tags:
  "arbitrary_metadata" reveals if the format allows arbitrarily-named
  metadata; "exif" if the format can store camera Exif data; "iptc" if
  the format can store IPTC metadata. #1001 (1.5.7)
* Removed ImageBuf and ImageBufAlgo functions that have been deprecated
  since OIIO 1.3. #1016 (1.5.8)
* ImageCache::add_file has been extended to allow an ImageSpec pointer
  as "configuration", which will be passed along to the underlying
  ImageInput::open() when the file is first opened. This allows you to
  use the same kind of configuration options/hints that you would with a
  raw ImageInput. (1.5.8)
* ImageBuf constructor and reset methods have been extended to allow an
  ImageSpec pointer as "configuration", which will be passed along to
  the underlying ImageCache and/or ImageInput::open() when the file is
  first opened. This allows you to use the same kind of configuration
  options/hints that you would with a raw ImageInput. (1.5.8)
* The DeepData helper structure now has methods to set (already allocated)
  deep values, as well as to retrieve uint values. (1.5.8)
* ImageBuf methods to get and set deep values and to allocate the DeepData
  structure. (1.5.8)
* ImageBuf::interppixel_NDC has been changed to consider the coordinates
  relate to full (aka display) window, rather than data window. (1.5.8)
* ImageBuf::interppixel_bicubic and interppixel_bicubic_NDC added to
  sample an image with B-spline bicubic interpolation. (1.5.8)
* Clarified in the docs that TextureSystem::get_texture_info and
  ImageCache::get_image_info "exists" queries should return true, and
  place in *data the value 1 or 0 depending on whether the image exists
  and can be read. (1.5.10)
* Clarified in the docs that TextureSystem::get_texture_info and
  ImageCache::get_image_info "exists" queries should return true, and
  place in *data the value 1 or 0 depending on whether the image exists
  and can be read. (1.5.10)

Fixes, minor enhancements, and performance improvements:
* ImageBufAlgo:
   * flip(), flop(), flipflop() have been rewritten to work more
     sensibly for cropped images. In particular, the transformation now
     happens with respect to the display (full) window, rather than
     simply flipping or flopping within the data window. (1.5.2)
   * channels() now works properly with "deep" images. (1.5.8)
* oiiotool:
   * oiiotool --nosoftwareattrib suppresses the altering of Software and
     ImageHistory fields upon output. (1.5.1)
   * oiiotool --ch now will search for channel names case-insensitively,
     if the channel names are not first found with case-sensitive
     compares. #897 (1.5.1/1.4.11)
   * oiiotool --wildcardoff/--wildcardon can selectively disable and
     re-enable the frame sequence numeric wildcards (useful if you have
     a filename or other argument that must actually contain '#' or '@'
     characters). #892 (1.5.1/1.4.11)
   * oiiotool --no-autopremult/--autopremult can disable and re-enable
     the automatic premultiplication of color by opacity, in files where
     they are stored not-premultiplied and you wish to preserve the
     original un-premultiplied values. #906 (1.5.1/1.4.11)
   * oiiotool --sansattrib omits "-attrib" and "-sattrib" and their
     arguments from the command line that is put in the Software and
     ImageHistory metadata (makes it less pointlessly verbose). (1.5.1)
   * Now get helpful error messages if input images are corrupt or
     incomplete. (1.5.1)
   * oiiotool --flip, --flop, --flipflop, and --transpose now have more
     sensible behavior for cropped images. (1.5.2)
   * oiiotool --orientcw, --orientccw, and --orient180 are the new names
     for --rotcw, --rotccw, --rot180. The old names were confusing, and now
     it is more clear that these merely adust the Orientation metadata that
     suggests viewing orientation, rather than actually rotating the image
     data. (1.5.2)
   * oiiotool color conversion ops crashed when inputs were files with
     MIPmap levels (such as textures). #930 (1.5.3/1.4.13)
   * More graceful handling when outputting an image with negative pixel
     window origin to a file format that doesn't support this feature.
     (1.5.3)
   * oiiotool --unsharp_mask:kernel=median will perform a median filter
     for the blurring step (rather than a gaussian or other convolution
     filter), which tends to sharpen small details but not over-sharpen
     long edges as the traditional unsharp mask often does. (1.5.4)
   * oiiotool's help message now says explicitly if it was compiled without
     any support for OpenColorIO. (1.5.5/1.4.15)
   * oiiotool -stats, on deep files, now prints a histogram of the number
     of samples per pixel. #992 (1.5.6)
   * oiiotool --dump of "deep" files improves output with int32 channels
     (1.5.6).
   * oiiotool --stats of "deep" files prints a histogram of samples/pixel
     (1.5.6).
   * oiiotool -subimage has better error detection and reporting for
     requests for nonexistant subimages. #1005 (1.5.7)
   * oiiotool --ch is a bit more flexible in its channel-shuffling syntax:
     you are now able to say newchannel=oldchannel, both shuffling and
     renaming channels simultaneously, thus removing a frequent necessity
     of a --chnames action following a --ch. (1.5.8)
   * oiiotool -d now supports uint32, sint32. #1013 (1.5.8)
   * oiiotool -native forces native data format read in cases where going
     through the ImageCache would have sacrificed precision or range.
     #1014 (1.5.8)
   * oiiotool --resize (and similar, such as --fix, --resample, etc.) size
     specifiers now allow percentage values that are not the same in each
     direction, such as "200%x50%", which would double horizontal resolution
     while cutting vertical resolution in half. #1017 (1.5.8)
   * oiiotool --ch now works properly with "deep" images. (1.5.8)
* maketx & TextureSystem:
   * texture lookups have roughly doubled in performance because of SSE
     optimization. (1.5.5 / #948, 1.5.6 / #990).
   * maketx: Fix case typo for LatLong env map creation when in 'prman'
     mode. #877.  (1.5.1/1.4.10)
   * maketx --attrib, --sattrib, and --sansattrib now work like
     oiiotool; in other words, you can use command line arguments to add
     or alter metadata in the process of creating a texture. #901 (1.5.1)
   * `maketx --sharpen <AMT>` adds slight sharpening and emphasis of
     higher frequencies when creating MIP-maps. #958 (1.5.5)
   * Fix crash when using maketx --checknan (1.5.5)
   * maketx now embeds metadata hints ("oiio:ConstantColor") for
     constant textures (all pixels are identical). The texture system
     notices this hint and will automatically replace texture lookups
     (as long as wrap mode is not black) with this constant value, about
     10x faster than a real texture lookup would have been. This sounds
     silly, but in production sometimes you end up with uniform textures
     and this helps. Also, you can retrieve the constant color via
     get_texture_info of "constantcolor" and "constantalpha" (though the
     retrieval will only succeed if the metadata is present, even if
     the texture is actually constant). #1006 (1.5.7)
   * maketx now embeds metadata hints ("oiio:AverageColor") containing
     the average value of the texture. This can be retrieved with
     get_texture_info of "averagecolor" and "averagealpha". #1006 (1.5.7)
* Python:
    * Bug fix: the Python binding of ImageInput.read_scanlines did not
      properly honor any channel subset specified. (1.5.5/1.4.15)
    * All of the expensive functions in the Python bindings now release the
      Python GIL (global interpreter lock) as much as possible. This allows
      other Python threads to make progress even when OIIO is doing a
      lengthy operation such as file I/O or an expensive image processing
      operation. (1.5.5)
    * Bug fix in ImageInput: could crash if calling ImageInput.read_*
      functions without specifying a data format. #998 (1.5.6)
    * Bug fix in ImageOutput: could crash if calling ImageInput.write_*
      functions if the buffer format did not match the file data format.
      #999 (1.5.6)
* OpenEXR:
   * Improve the quality of lossy b44 compression by more correctly
     using the pLinear value of channels (we were incorrectly using the
     flag to indicate linear channels, but it's really for channels
     that are perceptually linear). (#867) (1.5.0/1.4.9)
   * Fix potential build breaks due to incorrect use of
     Imf::isDeepData() which apparently was not intended to be an
     externally-visible function. (1.5.0/1.4.9) (#875)
   * Fix crashes/exceptions when passing string array attributes
     for OpenEXR output. #907 (1.5.1/1.4.11)
   * Improved ordering of channel names for OpenEXR files with many
     "layers".  #904 (1.5.1/1.4.11)
   * Fixed issue where a stringvec of 16 items was mishandled as a
     matrix. #929 (1.5.3/1.4.13)
   * OpenEXR 2.2 support, including the new DWAA/DWAB compression modes.
     (1.5.4)
   * Fix bug that prevented proper saving/reporting of subimage name
     for multi-subimage OpenEXR files. (1.5.5).
   * Fix read_deep_tiles() error when not starting at the image origin.
     #1040 (1.6.0/1.5.10)
   * Fix output of multi-part exr file when some parts are tiled and
     others aren't. #1040 (1.6.0/1.5.10)
* JPEG:
   * Fix broken recognition of .jfi extension. (#876)
   * Read and write ICC profiles as attribute "ICCProfile". (#911)
   * Fix seek_subimage for JPEG input -- did not properly return the
     spec. (1.5.2/1.4.12)
   * Support for reporting and controlling chroma-subsampling value,
     via the "jpeg:subsampling" attribute. #978 (1.5.5)
* PNG:
   * Read and write ICC profiles as attribute "ICCProfile". (#911)
* TIFF:
   * Read and write ICC profiles as attribute "ICCProfile". (#911)
   * Graceful handling of output of images with negative data window
     origin. (1.5.3)
   * Improved precision when auto-premultiplying 8 bit unassociated alpha
     images (if the user is reading them into a higher-precision buffer).
     #960, #962 (1.5.2)
   * Change default compression (if otherwise unspecified) to "zip". (1.5.6)
   * Robust to Exif blocks with corrupted GPS data. #1008 (1.5.8/1.4.16)
   * Fixes to allow proper output of TIFF files with uint32 or int32 pixels
     (1.5.8)
* RAW:
   * Fix for portrait-orientation image reads. (#878) (1.5.1/1.4.10)
   * Fix bug with "open with config" options. #996 (1.5.6)
* RLA: Fix bug that caused RLA output to not respect requests for 10 bit
  output (would force it to 16 in that case). #899 (1.5.1)
* ImageSpec::get_float_attribute() is now better about retrieving values
  that were stored as integers, even though you are querying them as
  floats. (#862) (1.5.0/1.4.9)
* ImageCache fix for when files are not found, then subsequently the
  searchpath is changed. #913 (1.4.12/1.5.2)
* Fixed some alloca calls that did not get the right amount of memory.
  (#866) (1.5.0/1.4.9)
* Fix an off-by-one loop in IBA::resize that would not get the wrong
  image result, but might trigger debuggers to flag it as touching the
  wrong memory. (#868) (1.5.0/1.4.9)
* Better exception safety in Filesystem::scan_for_matching_filenames().
  #902 (1.5.1/1.4.11)
* Python: ImageBuf.get_pixels() is now implemented, was previously omitted.
  (1.5.2)
* TextureSystem: anisotropic texture lookups are slightly sped up (~7%)
  by using some approximate math (but not visibly different) #953. (1.5.5)
* Better exception safety in Filesystem::exists() and similar functions.
  #1026 (1.5.8/1.4.16)

Build/test system improvements:
* Fix several compiler warnings and build breakages for a variety of
  platforms and compiler versions. (#858, #861) (1.5.0/1.4.8)
* Move around some cpack-related components. (#863) (1.5.0/1.4.9)
* Allow in-source build (not recommended, but necessary for Macports).
  (#863) (1.5.0/1.4.9)
* Fixes to docs-building makefiles. (#873) (1.5.0/1.4.9)
* More robust build when OpenEXR and IlmBase have installed their
  respective header files in different directories rather than the
  expected behavior of being installed together. (#861) (1.5.0/1.4.9)
* Fix build break in DEBUG compiles for ustring internals. (#869) (1.5.0/1.4.9)
* Fix warnings about potentially uninitialized variables. (#871) (1.5.0/1.4.9)
* Make thread.h use more modern gcc intrinsics when gcc >= 4.8, this
  allows correct thread.h operations for PPC and MIPS platforms that
  were't working before. (#865) (1.5.0/1.4.9)
* Fix Windows build when OIIO_STATIC_BUILD is used. (#872) (1.5.0/1.4.9)
* Fixes to get a clean compile on Windows + MSVC 9. (#859) (1.5.0/1.4.8)
* Fixes to get a clean compile on Windows + MSVC 9. (#872) (1.5.0/1.4.9)
* Make 3.0 compatibility fixes on OSX. (1.5.1/1.4.10)
* Fix segfaults on 32 bit systems with gcc 4.2. #889 (1.5.1/1.4.11)
* Fixes to Filesystem internals to work with older Boost releases older
  than 1.45. #891 (1.5.1/1.4.11)
* Fixes to find libraw properly with Visual Studio 2010. #895 (1.5.1/1.4.11)
* Fix bad casts in thread.h that broke some platforms. #896 (1.5.1/1.4.11)
* Several fixes for Windows build, including properly finding OpenEXR
  libraries when OpenEXR 2.1 is used. (1.5.1/1.4.11)
* Fixes to fmath.h to work with MSVC 2013 (some long-omitted math ops that
  we'd always needed to define for Windows only are finally supported
  in MSVC 2013). #912 (1.4.12/1.5.2) #927 (1.4.13/1.5.3)
* Fix for Linux + Boost >= 1.55 combination: need to link with -lrt.
  #914 (1.4.12/1.5.2)
* Fix Ptex + static linkage. (1.4.12/1.5.2)
* Rudimentary support for C++11: building with USE_CPP11=1 tries to set
  compiler flags (for gcc and clang, anyway) to use C++11 mode, and also
  building with USE_LIBCPLUSPLUS=1 tries to link with libc++ if you are
  using clang. (1.5.2)
* Fixes for Boost Filesystem 1.46-1.49. (1.5.2/1.4.12)
* testtex new options: --nchannels (forces num channels to look up, rather
  than always doing what's in the file), --derivs (force the kind of texture
  calls that return derivs of the texture lookups. (1.5.4)
* Make it able to compile against giflib >= 5.1. #975 (1.4.14/1.5.5)
* Add an option to link against static Ilmbase and OpenEXR libraries (1.5.5).
* Add testtex options -interpmode and -mipmode. #988 (1.5.6)
* Build against Python 3 if the make/cmake build flag USE_PYTHON3=1 is used.
  (1.5.6)
* grid.tx and checker.tx have been moved from ../oiio/images to
  testsuite/common/texture (allow it to be versioned along with any changes
  to maketx. (1.5.7)
* Support for freetype 2.5.4. #1012 (1.5.8/1.4.16)
* Python plugin is now build as a cmake "module" rather than "library",
  which fixes some things on OSX. #1043 (1.6.0/1.5.10)

Developer goodies / internals:
* New Strutil string comparison functions: starts_with, ends_with.
  (1.5.1/1.4.10)
* New Strutil simple parsing functions: skip_whitespace, parse_char,
  parse_until_char, parse_prefix, parse_int, parse_float, parse_string,
  parse_word, parse_identifier, parse_until. (1.5.1/1.4.10, 1.5.2/1.4.11)
* New Filesystem functions: create_directory, copy, remove, remove_all,
  temp_directory_path, unique_path. (1.5.1/1.4.10)
* thread.h: add atomic_exchange() function. #898 (1.5.1/1.4.11)
* Improved error propagation through ImageCache, ImageBuf, and oiiotool.
  (1.5.1)
* Moved certain platform-dependent macros from sysutil.h to platform.h
  (1.5.4/1.4.16)
* ustring: add comparisons between ustrings versus char* and string_view.
  (1.5.4)
* New simd.h exposes float4, int4, and mask4 classes that utilize SSE
  instructions for big speedups. (1.5.5 / #948, #954, #955)
* Big reorganization of fmath.h, moved stuff around, organized, added lots
  of safe_blah() functions that clamp to valid ranges and fast_blah()
  functions that are approximations that may differ slightly several
  decimal places in but are much faster than the full precision libm/IEEE
  versions. #953, #956 (1.5.5)
* New utility function: premult(), which can premultiply non-alpha, non-z
  channels by alpha, for a whole buffer (with strides, any data type).
  #962 (1.5.5)
* Timer API now has queries for ticks as well as seconds. (1.5.5)
* platform.h now has macros to mark functions as pure, const, nothrow,
  or unused. (1.5.5)
* Filesystem::read_text_file reads a whole text file into a string
  efficiently. (1.5.8)



Release 1.4.16 (19 Jan 2015 -- compared to 1.4.15)
--------------------------------------------------
* Fix gcc 4.9 warnings.
* Improved error propagation through ImageCache, ImageBuf, and oiiotool.
* TIFF more robust to Exif blocks with corrupted GPS data. #1008
* Support for freetype 2.5.4. #1012
* Better exception safety in Filesystem::exists() and similar functions. #1026

Release 1.4.15 (24 Nov 2014 -- compared to 1.4.14)
--------------------------------------------------
* OpenEXR: fix botched writing of subimage name.
* 'oiiotool --help' now says explicitly if it was built without OpenColorIO
  enabled.
* Python read_scanlines() did not honor the channel subset.
* RAW file reading: the open-with-config variety had the wrong function
  signature, and therefore did not properly override the base class, causing
  configuration hints to be ignored.
* Bug fix in Python ImageInput: could crash if calling ImageInput.read_*
  functions without specifying a data format. #998
* Bug fix in Python ImageOutput: could crash if calling ImageInput.write_*
  functions if the buffer format did not match the file data format. #999
* RAW: Fix bug with "open with config" options. #996

Release 1.4.14 (20 Oct 2014 -- compared to 1.4.13)
--------------------------------------------------
* GIF: Fix to make it able to compile against giflib >= 5.1. #975
* JPEG support for reporting and controlling chroma-subsampling value,
  via the "jpeg:subsampling" attribute. #978

Release 1.4.13 (12 Sep 2014 -- compared to 1.4.12)
--------------------------------------------------
* Now builds against OpenEXR 2.2, including support for the new
  DWAA and DWAB compression modes.
* OpenEXR: fixed issue where a stringvec of 16 items was mishandled as
  a matrix.
* Fix fmath.h to move the 'using' statements around properly.
* Fix oiiotool color conversion bug that crashed when the input image was
  a file with MIP levels.
* TIFF output now gracefully handles negative origins without hitting an
  assertion.
* Developer details: platform macros moved from sysutil.h to platform.h.
  (But sysutil.h now includes platform.h, so you shouldn't notice.)
* Developer details: ustring now has comparisons between ustrings versus
  char* and string_view.

Release 1.4.12 (31 Jul 2014 -- compared to 1.4.11)
--------------------------------------------------
* ImageCache fix for when files are not found, then subsequently the
  searchpath is changed. #913 (1.4.12/1.5.2)
* Fixes to fmath.h to work with MSVC 2013 (some long-omitted math ops that
  we'd always needed to define for Windows only are finally supported
  in MSVC 2013). #912 (1.4.12/1.5.2)
* Fix for Linux + Boost >= 1.55 combination: need to link with -lrt.
  #914 (1.4.12/1.5.2)
* Fix Ptex + static linkage. (1.4.12/1.5.2)

Release 1.4.11 (9 Jul 2014 -- compared to 1.4.10)
-------------------------------------------------
* OpenEXR output fix for crashes/exceptions when passing string array
  attributes. #907
* Improved ordering of channel names for OpenEXR files with many "layers".
  #904
* oiiotool --ch now will search for channel names case-insensitively,
  if the channel names are not first found with case-sensitive compares. #897
* oiiotool --wildcardoff/--wildcardon can selectively disable and re-enable
  the frame sequence numeric wildcards (useful if you have a filename or
  other argument that must actually contain '#' or '@' characters). #892
* oiiotool --no-autopremult/--autopremult can disable and re-enable
  the automatic premultiplication of color by opacity, in files where
  they are stored not-premultiplied and you wish to preserve the original
  un-premultiplied values. #906
* Fix segfaults on 32 bit systems with gcc 4.2. #889
* Fixes to Filesystem internals to work with older Boost releases < 1.45. #891
* Fixes to find libraw properly with Visual Studio 2010. #895
* Fix bad casts in thread.h that broke some platforms. #896
* Strutil: add another variety of parse_identifier with an expanded
  character set.
* thread.h: add atomic_exchange() function. #898
* Several fixes for Windows build, including properly finding OpenEXR
  libraries when OpenEXR 2.1 is used.
* Better exception safety in Filesystem::scan_for_matching_filenames(). #902

Release 1.4.10 (20 Jun 2014 -- compared to 1.4.9)
-------------------------------------------------
* Fix for portrait-orientation RAW image reads. (#878)
* maketx: Fix case typo for LatLong env map creation when in 'prman'
  mode (#877).
* New Strutil string comparison functions: starts_with, ends_with.
* Make 3.0 compatibility fixes on OSX.
* New Strutil simple parsing functions: skip_whitespace, parse_char,
  parse_until_char, parse_prefix, parse_int, parse_float, parse_string,
  parse_word, parse_identifier, parse_until.
* New Filesystem functions: create_directory, copy, remove, remove_all,
  temp_directory_path, unique_path.

Release 1.4.9 (6 Jun 2014 -- compared to 1.4.8)
-----------------------------------------------
* Allow in-source build (not recommended, but necessary for MacPorts). (#863)
* CPack improvements. (#863)
* Fixes to docs-building makefiles. (#873)
* Make ImageSpec::get_float_attribute correctly convert many integer types.
  (#862)
* Fixed some alloca calls that did not get the right amount of memory. (#866)
* OpenEXR: Improve the quality of lossy b44 compression by more correctly
  using the pLinear value of channels (we were incorrectly using the flag
  to indicate linear channels, but it's really for channels that are
  perceptually linear). (#867)
* More robust build when OpenEXR and IlmBase have installed their
  respective header files in different directories rather than the
  expected behavior of being installed together. (#861)
* Fix an off-by-one loop in IBA::resize that would not get the wrong
  image result, but might trigger debuggers to flag it as touching the
  wrong memory. (#868)
* Fix build break in DEBUG compiles for ustring internals. (#869)
* Fix warnings about potentially uninitialized variables. (#871)
* Make thread.h use more modern gcc intrinsics when gcc >= 4.8, this
  allows correct thread.h operations for PPC and MIPS platforms that
  were't working before. (#865)
* Fix Windows build when OIIO_STATIC_BUILD is used. (#872)
* Fixes to get a clean compile on Windows + MSVC 9. (#872)
* New filters available to direct users of filter.{h,cpp} and for
  ImageBufAlgo, oiiotoo, and maketx: "cubic", "keys", "simon", "rifman". (#874)
* OpenEXR: Fix potential build breaks due to incorrect use of
  Imf::isDeepData() which apparently was not intended to be an
  externally-visible function. (#875)
* JPEG: Fix broken recognition of .jfi extension. (#876)

Release 1.4.8 (23 May 2014 -- compared to 1.4.7)
------------------------------------------------
* Fix several compiler warnings and build breakages for a variety of
  platforms and compiler versions. No new feature or true bug fixes.
  #857, #858, #859


Release 1.4 (19 May 2014) -- compared to 1.3.x
----------------------------------------------
Major new features and improvements:
* The PNM reader now supports "PFM" files, which are the floating point
  extension to PNM. (1.4.1)
* Preliminary support for reading a wide variety of digital camera "RAW"
  files.  (1.4.1)
* New oiiotool commands:
    * `--cpow` : raise pixel values to a power (1.4.1)
    * `--label` : give a name to the top-of-stack image, can be referred
       to later in the command line (1.4.1)
    * `--cut` : combine --crop, --origin +0+0, and --fullpixels. (1.4.3)
    * `--pdiff` : perceptual diff (#815) (1.4.4)
    * `--polar`, `--unpolar` : complex <-> polar conversion. (#831) (1.4.5)
* oiiotool --resize and --fit, and also maketx when using "good" filters
  for downsizing, have been significantly sped up. When downsizing
  with large filters, we have seen up to 3x improvement. (#808) (1.4.3)

Public API changes:
* New ImageBufAlgo functions:
  - pow() raises pixel values to a power. (1.4.1)
  - cut() cuts a region of pixels and moves it to the origin (combines
    crop, reset origin, and set full res = data resolution). (1.4.3)
  - complex_to_polar() and polar_to_complex() convert from (real,imag)
    to (amplitude,phase) and back. (#831) (1.4.5)
* New string_view class (in string_view.h) describes a non-owning
  reference to a string.  The string_view is now used in many places
  throughout OIIO's APIs that used to pass parameters or return values
  as char* or std::string&.  Read string_view.h for an explanation of why
  this is good. (1.4.2, 1.4.3) (N.B. this was called string_ref until 1.4.6,
  when it was renamed string_view to conform to C++17 draft nomenclature.)
* New array_view<>, array_view_strided<>, strided_ptr<>, and image_view<>
  templates are great utility for passing bounded and strided arrays. (1.4.3)
* Removed deprecated PT_* definitions from typedesc.h.
* Removed the quantization-related fields from ImageSpec. (1.4.3)
* Dither: If ImageOutput::open() is passed an ImageSpec containing the
  attribute "oiio:dither" and it is nonzero, then any write_*() calls
  that convert a floating point buffer to UINT8 output in the file will
  have a hashed dither added to the pixel values prior to quantization
  in order to prevent the appearance of visible banding. The specific
  nonzero value passed for the attribute will serve as a hash seed so
  that the pattern is repeatable (or not). (1.4.3)

Fixes, minor enhancements, and performance improvements:
* Improved oiiotool features:
  * --stats on deep files now prints additional info, such as the minimum
    and maximum depth and on which pixels they were encountered, as well
    as which pixel had the maximum number of depth samples. (1.4.1)
  * --resize and --resample allow WIDTHx0 or 0xHEIGHT, where the '0'
    value will be interpreted as automatically computing the missing 
    dimension to preserve the aspect ratio of the source image.
    (#797, #807) (1.4.3)
  * Fixed possible crash when using --origin with tiled, cached
    images. (1.3.12/1.4.2)
  * --pdiff does a perceptual diff (like 'idiff -p'). (#815) (1.4.4)
  * --dumpdata takes a noptional modifier empty=0 that will cause empty
    deep pixels to not produce any output. (#821) (1.4.5)
  * --dumpdata correctly prints values of uint32 channels. #989 (1.5.6)
    deep pixels to not produce any output. (#821) (1.4.5)
  * --polar, --unpolar convert from complex (real,imag) to polar
    (amplitude, phase) and vice versa. (#831) (1.4.5)
  * View wildcards: similar to frame range wildcards, "%V" is replaced by
    new names, "%v" by the first letter of each view. The view list is
    {"left","right"} by default, but may be set with the --views argument.
    (1.4.5)
  * --over and --zover set the resulting display/full window to the union
    of those of the inputs; previously it set the display window to that
    of the foreground image, which is often a poor default. (1.4.7)
* ImageCache/TextureSystem:
  - The multi-point version of environment() was broken. (1.3.9/1.4.1)
  - Don't honor the SHA-1 fingerprint found in a file if the "Software"
    metadata doesn't indicate that the file was written by maketx or
    oiiotool. (1.4.3)
* OpenEXR:
  - Multi-part EXR (2.0) didn't write the required "name" attribute for
    each part. (1.3.10/1.4.1)
  - Fix crashing bug when reading stringvector attributes in the
    file. (1.3.11/1.4.2)
  - Add .sxr and .mxr as possible filename extensions (1.3.12/1.4.2)
  - Smarter channel ordering of input of files with ZBack, RA, GA, or BA
    channels (#822) (1.4.5).
  - Adhere to the misunderstood limitation that OpenEXR library doesn't
    allow random writes to scanline files. (1.4.6)
  - More robust with certain malformed metadata. (#841) (1.4.6)
* TIFF: Give a more explicit error message for unsupported tile sizes (1.4.4)
* GIF: Fixes to subimage generation; GIF frames are treated as sequential
  windows to be drawn on canvas rather than as independent images; respect
  "disposal" method; initial canvas is now transparent and all GIFs are
  presented as 4-channel images. (#828) (1.4.5)
* iconvert: properly handle multi-image files for formats that can't
  append subimages. (1.3.10/1.4.1)
* iv info window should print native file info, not translated
  ImageBuf/ImageCache info. (1.3.10/1.4.1)
* Fix ImageCache::get_pixels() for the chbegin != 0 case, when cache
  and output buffer types were not identical. (1.3.10/1.4.1)
* DPX:
  - Fixed several places in the where it could have had buffer
    overruns when processing certain malformed string fields. (1.4.1)
  - Fixed inappropriate use of "dpx_ImageDescriptor" could make invalid
    DPX files (especially when reading metadata from one DPX file,
    changing the number of channels, then writing out again as a DPX
    file). (1.3.10/1.4.1)
  - For output, honor the "Software" metadata attribute passed in.
    (1.3.11/1.4.2)
  - Ignore negative image origin values, which are not allowed by the
    DPX spec which states they are unsigned. (#813) (1.4.4)
  - Fix improper handling of unsupported pixel data types. (#818) (1.4.5)
  - Accept pixel ratio (x/0) to mean 1.0, not NaN. (#834) (1.4.5/1.3.13)
  - Pad subimages to 8k boundaries, as suggested by the DPX spec (1.4.7)
  - Properly write "userdata" field to DPX files if set. (1.4.7)
* PNG:
  - add "png:compressionlevel" and "compression" strategy attributes.
    (1.3.12/1.4.2)
  - output properly responds to "oiio:UnassociatedAlpha"=1 to indicate
    that the buffer is unassociated (not premultiplied) and therefore it
    should not automatically unpremultiply it. (1.4.5)
* Make ImageBuf iterators return valid black pixel data for missing
  tiles. (1.3.12/1.4.2)
* Make the ImageOutput implementations for all non-tiled file formats
  emulate tiles by accepting write_tile() calls and buffering the image
  until the close() call, at which point the scanlines will be output.
  (1.4.3)
* All ImageBufAlgo functions, and oiiotool, strip any "oiio:SHA-1" hash
  values in the metadata in order not to confuse the TextureSystem. (1.4.3)
* IFF: accept write_scanline, even though IFF is tile only. (1.4.3)
* The implementation of the Lanczos filter (and any operations using it)
  have been sped up by using an approximate fast_sinpi instead of the
  more expensive sin() (1.4.3).
* Speed up iinfo --hash / oiiotool --hash by about 20%. (#809) (1.4.4)
* All format writer plugins: ensure that calling close() twice is safe.
  (#810) (1.4.4)
* oiiotool --info and iinfo output have been altered slightly to make them
  match and be consistent. Also, oiiotool didn't say that deep files were
  deep (1.4.4).
* Fixed bad bugs in IBA::flatten() and oiiotool --flatten. (#819) (1.4.5)
* Fix Parameter neglect of properly copying the m_interp field for assignment
  and copy construction. (#829) (1.4.5/1.3.13)
* Fix ImageBufAlgo::circular_shift (and oiiotool --cshift) that did not
  wrap correctly for negative shifts. (#832) (1.4.5/1.3.13)
* The "gaussian" flter incorrectly had default width 2 (correct = 3),
  and the "mitchell" filter incorrect had default width 3 (correct = 4).
  These were bugs/typos, the new way is correct. If you were using those
  filters in ways that used the default width value, appearance may change
  slightly. (1.4.6)

Build/test system improvements:
* libOpenImageIO_Util is now built that only has the utility functions
  in libutil (in addition to the libOpenImageIO, which contains everything).
  This is handy for apps that want to use OIIO's utility functions (such
  as ustring or Filesystem) but doesn't really need any of the image
  stuff.  A build flag BUILD_OIIOUTIL_ONLY=1 will cause only the util
  library to be built.  (1.4.1)
* New build option OIIO_THREAD_ALLOW_DCLP=0 will turn off an
  optimization in thread.h, resulting in possibly worse spin lock
  performance during heavy thread contention, but will no longer get
  false positive errors from Thread Sanitizer.  The default is the old
  way, with full optimization! (1.4.1)
* More robust detection of OpenEXR library filenames. (1.4.1)
* Always reference OpenEXR and Imath headers as `<OpenEXR/foo.h>` rather
  than `<foo.h>`. (1.4.1)
* Unit test strutil_test now comprehensively tests Strutil. (1.4.1)
* Fix broken build when EMBEDPLUGINS=0. (1.4.3/1.3.13)
* Fix broken build against OpenEXR 1.x. (1.4.3/1.3.13)
* version.h has been renamed oiioversion.h.  For back compatibility, there
  is still a version.h, but it merely includes oiioversion.h. (#811) (1.4.4)
* Moved all the public header files from src/include to
  src/include/OpenImageIO, so that the src/include area more closely
  matches the layout of an OIIO install area. (#817) (1.4.4)
* Fix compilation problems for PowerPC (#825). (1.4.5/1.3.13)
* Fixes for OpenBSD compilation. (#826/#830) (1.4.5/1.3.13)
* Fixes for Linux compilation when building with BUILDSTATIC=1. (1.4.6)
* Fixes for compilation against IlmBase/OpenEXR 2.1. (1.4.6)
* Improve finding of Freetype on some systems (1.4.6).
* Add to top level Makefile the option STOP_ON_WARNING=0 to let it cleanly
  compile code that generates compiler warnings, without stopping the build.
  (1.4.7)

Developer goodies / internals:
* TBB has been removed completely. (1.4.2)
* Slightly faster timer queries in timer.h for OSX and Windows. (1.4.1)
* Strutil :
  - safe_strcpy() -- like strncpy, but copies the terminating 0 char. (1.4.1)
  - split() fixes bug when maxsplit is not the default value. (1.3.10/1.4.1)
* ParamValue/ParamValueList :
  - ParamValue now allows get/set of the internal 'interp' field. (1.3.9/1.4.1)
  - ParamValueList::push_back is not properly const-ified. (1.4.1)
  - New PVL::find() lets you search on the PVL. (1.4.6)
* fmath.h :
  - New fast_sin, fast_cos, fast_sinpi, fast_cospi are much faster
    polynomial approximations (with max absolute error of ~0.001). (1.4.3)
  - round_to_multiple_of_pow2 - a faster version of the existing
    round_to_multiple(), but only works when the multiple is known to be
    a power of 2. (1.4.6)
* TypeDesc now has operator<, which makes it easier to use STL data structures
  and algorithms that expect an ordering, using TypeDesc as a key. (1.4.6)
* thread.h
  - Slight thread.h portability tweaks. (1.4.1)
  - spin_rw_lock now has more standard lock()/unlock() as synonym for
    for exclusive/write lock, and lock_shared()/unlock_shared() as
    synonym for "read" locks. (1.4.6)
* ustring :
  - new ustringLess and ustringHashIsLess functors make it easier to use
    ustring as keys in STL data structures and algorithms that require
    an ordering function. (1.4.6)
  - improve thread performance significantly by using an
    unordered_map_concurrent internally for the ustring table. (1.4.6)
* unordered_map_concurrent.h :
  - Allow umc template to specify a different underlying map for the
    bins. (1.4.6)
  - Add retrieve() method that's slightly faster than find() when you just
    need a value, not an iterator. (1.4.6)
  - Align bins to cache lines for improved thread performance. (1.4.6)
* ImageBuf iterators have a new rerange() method that resets the iteration
  range, without changing images or constructing a new iterator. (1.4.6)



Release 1.3.14 (19 May 2014 -- compared to 1.3.13)
--------------------------------------------------
* OpenEXR output: More robust with certain malformed metadata. (#841) (1.4.6)
* Rename the string_ref class to string_view. (This is unused in OIIO, it
  is for compatibility with OSL.)
* Build fixes on Linux when using BUILDSTATIC=1.
* Add round_to_multiple_of_pow2 to fmath.h
* Add STOP_ON_WARNING option to the top level Makeile wrapper.
* Add documentation on the Python binding for IBA::cut.
* oiiotool --over and --zover now set the output image's display window to
  the union of the inputs' display window, rather than to the foreground.

Release 1.3.13 (2 Apr 2014 -- compared to 1.3.12)
-------------------------------------------------
* Bug fix to string_ref::c_str().
* Make ImageBuf iterators return valid black pixel data for missing tiles.
* Fix broken build when EMBEDPLUGINS=0.
* Fix broken build when building against OpenEXR 1.x.
* Fix bad bugs in IBA::flatten() and oiiotool --flatten. (#819)
* Fix DPX handling of unsupported pixel types. (#818)
* Fix compilation problems for PowerPC.
* Fix Parameter neglect of proerly copying the m_interp field for assignment
  and copy construction. (#829)
* Fixes for OpenBSD compilation. (#826/#830)
* DPX: accept pixel ratio (x/0) to mean 1.0, not NaN. (#834)
* Fix ImageBufAlgo::circular_shift (and oiiotool --cshift) that did not
  wrap correctly for negative shifts. (#832)

Release 1.3.12 (25 Jan 2014 -- compared to 1.3.11)
--------------------------------------------------
* Add .sxr and .mxr as possible filename extensions for OpenEXR.
* PNG: add "png:compressionlevel" and "compression" strategy attributes.
* Fix recent build break where OIIO would no longer compile properly
  against OpenEXR <= 1.6.
* oiiotool --origin could crash with certain large ImageCache-backed
  images.

Release 1.3.11 (8 Jan 2014 -- compared to 1.3.10)
-------------------------------------------------
* DPX output: honor the "Software" metadata attribute passed in.
* OpenEXR: fix crashing bug when reading stringvector attributes in the
  file.
* Fix build breaks when building against OpenEXR 1.x.
* Fix warnings with Boost Python + gcc 4.8.

Release 1.3.10 (2 Jan 2014 -- compared to 1.3.9)
------------------------------------------------
* OpenEXR fix: multi-part EXR (2.0) didn't write the required "name"
  attribute for each part.
* iconvert: properly handle multi-image files for formats that can't
  append subimages.
* iv info window should print native file info, not translated
  ImageBuf/ImageCache info.
* Improved strutil_test now much more comprehensively unit tests
  Strutil.
* Strutil::split() fixes bug when maxsplit is not the default value.
* Fix ImageCache::get_pixels() for the chbegin != 0 case, when cache
  and output buffer types were not identical.
* DPX bug fix -- inappropriate use of "dpx_ImageDescriptor" could make
  invalid DPX files (especially when reading metadata from one DPX
  file, changing the number of channels, then writing out again as a
  DPX file).



Release 1.3 (2 Dec 2013 -- compared to 1.2.x)
----------------------------------------------
Major new features and improvements:
* Huge overhaul of the Python bindings: TypeDesc, ImageSpec (1.3.2),
  ImageInput, ImageOutput (1.3.3), ROI, ImageBuf (1.3.4), ImageBufAlgo
  (1.3.6).  The Python bindings were pretty rusty, badly tested,
  undocumented, and had not kept up with recent changes in the C++ APIs.
  That's all fixed now, the Python APIs are finally first-class citizens
  (including full functionality, unit tests, and docs), and we intend to
  keep it that way.
* The ability for an application to supply custom ImageInput and associate
  them with a file extension. Those II's can do anything, including 
  generate image data procedurally.
* GIF reader

Public API changes:
* Large overhaul of the Python bindings. See the (finally existing!) docs.
* ImageBufAlgo:
  * New functions: nonzero_region(); ociodisplay(), resize() variety
    that lets you specify the filter by name; 2-argument (non-in-place)
    versions of add, sub, mul, rangecompress, rangeexpand, unpremult,
    premult, clamp fixNonFinite; sub() varieties that take float or
    float* operands.
  * Removed several IBA functions that have been deprecated since 1.2.
  * Deprecated the single-image in-place versions of add, sub, mul,
    rangecompress, rangeexpand, unpremult, premult, clamp fixNonFinite.
* ImageBuf:
  * read() and init_spec() are no longer required, somewhat simplifying
    application code that uses ImageBuf.  All ImageBuf API calls
    automatically read the spec and/or pixels from their named file if
    they are needed, if it has not already been done.  (1.3.4)
  * save() is deprecated, and new ImageBuf::write() is now preferred
    (naming symmetry). (1.3.4)
  * New set_write_format() and IB::set_write_tiles() allow override of
    default choices for data format and tile size for subsequent calls
    to ImageBuf::write(). (1.3.4)
* ImageCache / TextureSystem:
  * ImageCache::add_file() lets you seed the ImageCache with a "virtual file"
    that will read from a custom ImageInput.  This lets you add "procedural
    images" to the IC.
  * ImageCache::add_tile() lets you add tiles to the ImageCache. The caller
    can initialize those tiles with any pixel values it chooses.
  * A new variety of IC/TS::destroy() takes a 'bool teardown' parameter
    that, when true, does a complete teardown of the underlying ImageCache,
    even if it's the "shared" one. (1.3.7)
* OIIO::declare_imageio_format() exposes a way to give OIIO a custom
  ImageInput and/or ImageOutput (via factory functions) and associate them
  with particular file extensions. This makes it especially easy for an
  app to make a procedural image generator that looks to the entire rest
  of OIIO like a regular image file. (1.3.2)
* TypeDesc::VECSEMANTICS now have additional enum tags for TIMECODE and
  KEYCODE to indicate that the data represents an SMPTE timecode or
  SMPTE keycode, respectively. (1.3.7)

Fixes, minor enhancements, and performance improvements:
* oiiotool improvements:
  * --autotrim   Shrinks pixel data window upon output to trim black 
    edges. (1.3.2)
  * --siappend   Appends subimages of top two images on the stack. (1.2.2)
  * --siappendall Appends ALL images on the stack into a single image with
    multiple subimages. #1178 (1.6.4)
  * --sisplit   Splits the top multi-image into separate images on the
    stack for each subimage. #1178 (1.6.4)
  * --datadump will print all pixel values of an image (debugging tool) (1.3.6)
  * --flatten turns a "deep" image into a flat one by depth-compositing within
    each pixel (1.3.6).
  * --ociodisplay  applies an OpenColorIO display transformation. (1.3.7)
  * Fix memory leak when processing frame range. (1.2.1/1.3.2)
  * --help now returns a success error code, not a failure. (1.2.1/1.3.2)
  * Fix incorrect help message about --ociolook. (1.2.1/1.3.2)
  * Fix typo in "oiio:Colorspace" attribute name that interfered
    with correct color space conversion in --colorconvert. (1.2.1)
  * Many fixes and improvements to XMP & IPTC metadata handling. (1.2.2)
  * Multithread speed improvement when opening files by reducing how
    much time ImageInput::create and/or ImageOutput::create hold a
    global mutex.
  * oiiotool --origin and --fullpixels, when operating on cropped or
    overscanned image, could sometimes do the wrong thing. (1.2.2/1.3.3)
  * oiiotool --colorconvert did not work properly when the color
    transformation was detected to be a no-op. (1.2.2/1.3.3)
  * oiiotool --fit did not handle padding or offsets properly. (1.2.2/1.3.3)
  * Changed/improved the behavior of --rangecompress/--rangeexpand. (1.3.3)
  * 'oiiotool --pattern checker' was incorrect when nonzero offsets were
    used. (1.2.3/1.3.4)
  * oiiotool --runstats prints the total time/memory on every iteration
    when doing file sequence wildcard iteration. (1.3.4)
  * Eliminated a particular situation that might hit an ASSERT. Instead,
    bubble up a real error message. (1.3.4)
  * oiiotool --resize and --resample fixed for overscan images (1.3.5)  
  * --ociolook applies OCIO looks. (1.3.6)
  * Supports printf-style frame range wildcards ('%04d') in addition to the
    '#' style, and scan for matching frames if no explicit framespec is
    provided. (1.3.6)
* ImageBufAlgo improvements:
  * colorconvert() did not work properly when the color transformation was
    detected to be a no-op.
  * colorconvert(): added a variety that specifies color spaces by name.
  * New ociolook() function applies OCIO "looks." (1.3.6)
  * checker() was incorrect when nonzero offsets were used.
  * checker() now has default values of 0 for the 'offset' parameters
    (and so may be omitted if you want 0 offsets). (1.3.4)
  * unsharp_mask() bug when src and dst were different data formats.
    (1.2.3/1.3.4)
  * Better dealing with cases of IBA functions detecting and issuing
    errors when inputs that must be initialized are not. (1.3.4)
  * We changed the behavior of rangecompress/rangeexpand.  We swear
    the new way is better. (1.3.3)
  * New nonzero_region() returns the shrink-wrapped nonzero pixel data window.
    (1.3.2)
  * resize() has a new variety that lets you specify the filter by name
    (rather than allocating ans passing a Filter2D*).
  * resize() and resample() fixed to more robustly handle overscan
    images. (1.3.5)
  * over()/zover() are no longer restricted to float images. (1.3.7)
* ImageBuf:
  * ImageBuf::write() writes untiled images by default, fixing some
    tricky issues when IB's start thinking they're tiled because of
    interaction with the ImageCache (which makes everything look tiled).
  * ImageBuf::file_format_name() never worked properly, now is fixed (1.3.4)
  * Fixed bug that caused incorrect ImageBuf::copy_pixels() when the two
    IB's had different data types.  (1.3.4/1.2.3)
  * Improved iterator's handling of how overscanned pixels interact
    with wrap modes. (1.3.6)
  * Fixed a bug with black wrap mode not working correctly. (1.3.7/1.2.4)
* ImageCache/TextureSystem:
  * More careful with texture de-duplication -- texture value lookups
    use de-duplication, but metadata lookups (e.g., get_texture_info)
    uses the metadata from the original file.
  * get_image_info/get_texture_info queries for "datawindow" and
    "displaywindow". (1.3.6)
  * The multi-point version of environment() was broken. (1.3.9/1.4.1)
* maketx: --hicomp uses the new range compression/expansion formula.  (1.3.3)
* DPX:
  * support multi-image (often used for stereo frames).
  * Fixed DPX input that didn't recognized offset/cropped images.
    (1.2.2/1.3.3, another fix in 1.3.4)
  * Fixed DPX output crash with cropped images. (1.2.2/1.3.3)
  * Now correctly get and set "smpte:TimeCode" and "smpte:KeyCode"
    metadata.  (1.3.7).
* OpenEXR:
  * Fixed write_scanlines handling of per-channel data types (1.3.6)
  * Several OpenEXR 2.0 deep file fixes: only some compression types
    supported, write_tiles passed wrong parameters, must suppress some
    attribute names. (1.2.3/1.3.6)
  * Now correctly get and set "smpte:TimeCode" and "smpte:KeyCode"
    metadata.  (1.3.7).
* JPEG: fixed that some JPEG files were not being recognized because of
  magic number issues.
* TGA: Correctly unassociate alpha if it's from an unasociated file;
  also, always write unassociated data because so few Targa readers in
  the wild seem to properly handle associated alpha.
* PNG: More correct handling of unassociated alpha.
* TIFF: More correct handling of unassociated alpha.
* PSD: fix handling of associated vs unassociated alpha. (1.2.3)
* maketx fixed to handle inputs that are a mixture of cropped and 
  overscanned. (1.3.5)
* Fix segfault if OCIO is set to a non-existant file. (1.3.6)
* Slight performance increase when writing images to disk (1.3.6)
* Many fixes to make OIIO compile with libc++ (clang's new C++ library,
  and the default on OSX Mavericks). (1.2.3/1.3.6, 1.3.7)
* Fixed several potential buffer overflow errors from unsafe strcpy. (1.3.8)

Build/test system improvements:
* Fix broken tests under Windows. (1.3.2)
* Many fixes for compiler warnings on various platforms: fmath_test.cpp,
  field3dinput.cpp, sysutil.cpp, argparse.cpp, oiiotool.cpp. (1.2.1/1.3.2)
* Fixes problems on little-endian architecture with texture3d.cpp.
  (1.2.1/1.3.2)
* Fix compilation problems on architectures with gcc, but no 'pause' 
  instruction. (1.2.1/1.3.2)
* Fix build search path for correctly finding libopenjpeg 1.5. (1.2.1)
* Work around bug in older MSVC versions wherein Filesystem::open needed
  to explicitly seek to the beginning of a file. (1.2.1/1.3.2)
* Build fixes for FreeBSD. (1.2.1/1.3.2, 1.2.4/1.3.6)
* Fix testsuite/oiiotool on Windows -- windows shell doesn't expand
  wildcards. (1.2.1/1.3.2)
* Fix warnings for new GCC 4.8 compiler.
* Always search for and use the release HDF5 libraries, not the debugging
  ones, even when building debug OIIO (this fixes errors when a system
  does not have the debugging HDF5 libraries installed). (1.2.2/1.3.3)
* Extensive unit tests in the testsuite for the Python bindings.
* Fix compiler error on MIPS platform. (1.2.2/1.3.3)
* Add FIELD3D_HOME description to 'make help' (1.2.2/1.3.3)
* Add cmake variables ILMBASE_CUSTOM_INCLUDE_DIR, ILMBASE_CUSTOM_LIB_DIR,
  OPENEXR_CUSTOM_INCLUDE_DIR, and OPENEXR_CUSTOM_LIB_DIR to make it
  easier to have site-specific hints for these packages' locations. (1.3.4)
* Add BOOST_HOME and OCIO_HOME controls from the top-level Makefile wrapper.
  (1.3.4/1.2.3)
* Accommodate new cmake release that slightly changes the HDF5 library
  naming. (1.3.6)
* Various fixes to make the code compile properly with libc++ (clang's
  rewrite of the C++ standard library). (1.3.6)
* Updated PugiXML (partly to help compilation with libc++) (1.3.6)
* Better support for NOTHREADS (for some legacy systems) (1.3.6)
* Fix to __attribute__(visibility) for gcc < 4.1.2 (1.3.6)
* Improve the CMake build files to fully quote path constructions to make
  it more robust for builds with paths containing spaces. (1.3.7)
* Moved the main CMakeLists.txt file to the top level directory, per usual
  CMake conventions. (1.3.7)
* Fixed timer_test to allow for timing slop of newer OSX versions that have
  "timer coalescing." (1.6.4)

Developer goodies:
* Docs improvement: full documentation of ImageBufAlgo. (1.2.1/1.3.2)
* Merge improved "Tinyformat" that fixes a bug in some old glibc versions
  (1.3.2).
* Now each command line tools explicitly converts to UTF native arguments,
  rather than relying on it happening in ArgParse (which no longer does
  so). (1.3.2)
* Strutil::contains() and icontains(). (1.2.2/1.3.3)
* Updatd "Tinyformat" to the latest release (1.3.6)
* Sysutil::physical_memory() tries to figure out the total physical memory
  on the machine. (1.3.6)
* Strutil::safe_strcpy (1.3.8)
* ParamValue now allows get/set of the hidden 'interp' field. (1.3.9/1.4.1)



Release 1.2.3 (1 Nov 2013)
--------------------------
* 'oiiotool --pattern checker' (and ImageBufAlgo::checker) was
  incorrect when nonzero offsets were used.
* ImageBufAlgo::unsharp_mask() bug when src and dst were different
  data formats.
* PSD: fix handling of associated vs unassociated alpha.
* Fixed bug that caused incorrect ImageBuf::copy_pixels() when the two
  IB's had different data types.
* Add BOOST_HOME and OCIO_HOME controls from the top-level Makefile wrapper.
* Several OpenEXR 2.0 deep file fixes: only some compression types
  supported, write_tiles passed wrong parameters, must suppress some
  attribute names.
* DPX - several fixes to properly handle images with nonzero origins.
* Fixes for recent cmake not finding HDF5 properly.
* Many fixes to make OIIO compile with libc++ (clang's new C++ library,
  and the default on OSX Mavericks).
* Fix OpenEXR write_scanlines handling of per-channel data types.
* Upgraded PugiXML to a more modern version (necessary for clean compile
  with libc++).


Release 1.2.2 (1 Oct 2013)
--------------------------
* New features:
  * New oiiotool --siappend : append subimages of top two images on stack.
  * Utilities: added Strutil::contains() and icontains().
* Fixes:
  * Fixes in handling XMP & IPTC metadata.
  * oiiotool --origin and --fullpixels did not correctly propagate their
    changes to the output images.
  * oiiotool --colorconvert (and the underlying ImageBufAlgo::colorconvert)
    could crash if given a color conversion recognized as a no-op.
  * DPX output could crash when writing crop images.
  * DPX input was not recognizing the proper image offset or originalsize.
  * oiiotool --fit wasn't padding correctly or modifying offsets properly.
* Build fixes:
  * Fix compiler error on MIPS platform.
  * Add FIELD3D_HOME description to 'make help'
  * Always use the HDF5 release libraries (for Field3D), not the debug ones.


Release 1.2.1 (5 Aug 2013)
---------------------------
* oiiotool: Fix memory leak when processing frame range.
* Docs improvement: full documentation of ImageBufAlgo.
* oiiotool --help now returns a success error code, not a failure.
* oiiotool: fix incorrect help message about --ociolook.
* oiiotool: Fix typo in "oiio:Colorspace" attribute name that interfered
  with correct color space conversion in --colorconvert.
* Many fixes for compiler warnings on various platforms: fmath_test.cpp,
  field3dinput.cpp, sysutil.cpp, argparse.cpp, oiiotool.cpp.
* Fixes problems on little-endian architecture with texture3d.cpp.
* Fix compilation problems on architectures with gcc, but no 'pause' 
  instruction.
* Fix build search path for correctly finding libopenjpeg 1.5.
* Work around bug in older MSVC versions wherein Filesystem::open needed
  to explicitly seek to the beginning of a file.
* Build fixes for FreeBSD.
* Fix testsuite/oiiotool on Windows -- windows shell doesn't expand wildcards.


Release 1.2 (8 July 2013)
-------------------------
Major new features and improvements:
* New oiiotool commands:
    * `--swap`        Exchanges the top two items on the image stack.
    * `--fit`         Resize image to fit into a given resolution (keeping aspect).
    * `--ch`          Select/cull/reorder/add channels within an image.
    * `--chappend`    Merge two images by appending their color channels.
    * `--chnames`     Rename some or all of the color channels in an image.
    * `--zover`       Depth compositing
    * `--cadd`        Add constant per-channel values to all pixels
    * `--cmul`        Multiply an image by a scalar or per-channel constant.
    * `--fillholes`   Smoothly interpolate for hole filling.
    * `--resample`    Similar to `--resize`, but just uses closest pixel lookup.
    * `--clamp`       Clamp pixel values
    * `--rangeexpand` Expand range for certain HDR processing
    * `--rangecompress`  Compress range for certain HDR processing
    * `--unpremult`   Divide colors by alpha (un-premultiply).
    * `--premult`     Multiply colors by alpha.
    * `--kernel`      Make a convolution kernel using a filter name.
    * `--convolve`    Convolve two images.
    * `--blur`        Blur an image.
    * `--unsharp`     Sharpen an image using an unsharp mask.
    * `--paste`       Paste one image on another.
    * `--mosaic`      Create a rectilinear image mosaic.
    * `--transpose`   Transpose an image (flip along the diagonal axis)
    * `--chsum`       Sum all channels in each pixel
    * `--cshift`      Circular shift an image pixels
    * `--fft` `--ifft`  Forward and inverse Fourier transform
    * `--colorcount`  Counts how many pixels are one of a list of colors.
    * `--rangecheck`  Counts how many pixels fall outside the given range.
    * `--ociolook`    Apply OpenColorIO "looks"
* oiiotool can loop over entire numeric frame ranges by specifying
  wildcard filenames such as "foo.#.tif" or "bar.1-10#.exr".
* oiiotool --frames and --framepadding give more explicit control over
  frame range wildcards.
* Significant performance improvements when reading and writing images
  using the ImageBuf::read and ImageCache::get_pixels interfaces, and in
  some cases also when using regular ImageInput.  This also translates
  to improved performance and memory use for oiiotool and maketx.
* At least doubled the performance of maketx for large images when run
  on multi-core machines.
* Significant performance improvements when using ImageBuf::Iterator
  or ConstIterator to traverse the pixels in an ImageBuf, and the iterators
  now support "wrap" modes (black, clamp, periodic, mirror).
* maketx --hicomp does "highlight compensation" by compressing the
  HDR value range prior to inter-MIP resizes, then re-expanding the range.
* Field3D writer (it could read f3d files before, but not write them).
* idiff can now compare that are not the same size (treating pixels
  beyond the pixel data window is being 0 valued).
* maketx --lightprobe turns a "lightprobe" iamge into a latlong environment
  map.
* Significant improvements and fixes to EXIF, IPTC, and XMP metadata
  reading and writing.
* Significant thread scalability improvements to TextureSystem and
  ImageCache.
* Huge overhaul of functionality, style, and performance of the
  entire ImageBufAlgo set of functions (see the "Public API changes"
  section below, and the imagebufalgo.h file for details).

Public API changes:
* ImageOutput semantics change: If the spec passed to open() has
  spec.format set fo UNKNOWN, then select a default data format for the
  output file that is "most likely to be able to be read" and/or "most
  typical for files of that format in the wild."  Also,
  ImageOutput::open() will never fail because a requested data format is
  unavailable; if the requested format is not supported, a reasonable
  alternate will always be chosen.
* ImageBuf has been changed to a "PIMPL" idiom, wherein all the
  internals are no longer exposed in the public API.  This allows us to
  change ImageBuf internals in the future without breaking API or link
  compatibility (and thus giving us more freedom to backport important
  improvements to prior releases).
* Overhaul of ImageBufAlgo functions: they all take an ROI parameter;
  use the DISPATCH macros to make them work with all pixel data types
  where practical (previously, many supported float only); use Iterator
  rather than getpixel/setpixel, leading to huge speed improvements;
  multithread when operating on enough pixels, leading to huge speed
  improvements; work on 3D (volume) images where applicable; always
  gracefully handle uninitialized dest image or undefined ROI.
* New ImageBufAlgo functions: channels(), channel_append(), mul(),
  paste(), zover(), add() and mul() varieties that that add/multiply a
  constant amount to all pixels, fillholes_pp(), resample(), clamp(),
  rangecompress(), rangeexpand(), make_kernel(), unsharp_mask(),
  transpose(), channel_sum(), circular_shift(), fft(), ifft(),
  color_count(), color_range_check().
  [look in imagebufalgo.h for documentation.]
* ImageBufAlgo::make_texture() allows you to do the same thing that
  maketx does, but from inside an application and without launching a
  shell invocation of maketx.  Two varieties exist: one that takes a
  filename and reads from disk, another that takes an ImageBuf already
  in memory.
* ImageBuf Iterator/ConstIterator now take "wrap" mode parameters that
  allow out-of-range iterators to be able to retrieve valid data. Supported
  wrap modes include black, clamp, periodic, and mirror.  This simplifies
  a lot of algorithms using IB::Iterator, they can now be written to 
  rely on wrap behavior rather than being cluttered with checks for
  "if (it.exits())" clauses.
* ImageBufAlgo::computePixelHashSHA1 has been refactored to take ROI,
  a block size, and thread count, and thus can be parallelized with threads.
  The block size means that rather than computing a single SHA-1 for all
  the pixels, it computes separate (parallel) SHA-1 for each group of
  blocksize scanlines, then returns the SHA-1 of all the individual SHA-1
  hashed blocks. This is just as strong a hash as before, thought the value
  is different than doing the whole thing at once, but by breaking it into
  blocks the computation can be multithreaded.
* ImageBuf::swap() makes it easy to swap two ImageBuf's.
* ImageSpec::get_channelformats is now const (and always should have been).

Fixes, minor enhancements, and performance improvements:
* TextureSystem improvements:
  * Make sure "black" wrap wins out over "fill" value when they conflict
    (looking up an out-of-range channel beyond the pixel data window).
  * "mirror" wrap mode was slightly incorrect and has been fixed.
* oiiotool improvements:
  * oiiotool -v breaks down timing by individual function.
  * oiiotool has been sped up by forcing read of the whole image up front
      for reasonably-sized files (instead of relying on ImageCache).
  * oiiotool does not write output images if fatal errors have occurred.
  * oiiotool --diff: Better error handling, better error printing, and
    now it can compare images with differing data windows or channel
    numbers ("missing" channels or pixels are presumed to be 0 for the
    purposes of comparing).
  * oiiotool --resize (and --fit): properly handle the case of resizing
    to the same size as the original image.
  * oiiotool -d CHAN=TYPE can set the output for just one channel.
* ImageBufAlgo improvements:
  * Internal overhaul of IBA::resize to greatly speed it up.
  * Improve IBA::resize to handle the image edge better -- instead of 
    clamping, just don't consider nonexistant pixels.
  * More careful selection of filter when resizing (IBA::resize, oiiotool
    --resize and --fit, and maketx).
  * Fix IBA::paste() error when the foreground image runs off the end of
    the background image.
* Bug fix when computing SHA-1 hash of 0-sized images.
* Image format support improvements:
  * Bug fix where some format readers (PNM, SGI, and IFF) would leave the
    file handle opened if the ImageInput was destroyed without calling 
    close() first.  Now we guarantee that destroying the II always causes
    the file to close().
  * DPX: output allocation bug fix; properly set pixel aspect ratio for
    DPX write.
  * IFF: bug fix for endian swap for IFF file input.
  * JPEG2000: fix warnings, make sure event manager transfer object
    remains valid.
  * OpenEXR: when reading, was botching the ordering of per-channel data
    formats.
  * SGI write: bug fix for the case of 15 bpp RLE encoding, was
    double-swapping bytes.
  * Targa: more robust check for presence of alpha channels; bug fix where
    R and B channels were reversed for certain kinds of palette images.
  * TIFF: Store the orientation flag properly when outputting a TIFF file.
* maketx improvements:
  * maketx --chnames allows you to rename the channels when you create a
    texture.
  * maketx bug fixes: incorrect weighting when resizing MIP levels for
    latlong environment map images that could make visible artifacts
    on some intermediate MIP levels.
* encode_exif() didn't copy the right number of bytes.
* Python bindings: ImageSpec extra_attribs now properly responds to
  iterator calls.
* Fix bug in sRGB -> linear conversion.
* iv: make pixelview display coordinates & color even when outside the
  data window.

Build/test system improvements:
* Many fixes to improve builds and eliminate warnings on Windows and MinGW.
* Fix missing InterlockedExchangeAdd64 for Windows XP.
* New make/cmake boags: OIIO_BUILD_TOOLS=0 will exclude building of the
  command line tools (just build libraries), OIIO_BUILD_TESTS=0 will
  exclude building of unit test binaries.
* Improved matching of testsuite reference images on different platforms.
* Lots of fixes to compiler warnings on newer gcc and clang releases.
* Unit tests for Timer class.
* libOpenImageio/imagespeed_test benchmarks various methods of reading
  and writing files and iterating image pixels (to help us know what to
  optimize).
* If OpenSSL is available at build time, OIIO will use its SHA-1
  implementation instead of our own (theirs is faster). We still fall
  back on ours if OpenSSL is not available or when OIIO is built with
  USE_OPENSSL=0.
* Allow default the shared library suffix to be overridden with the
  CMake variable OVERRIDE_SHARED_LIBRARY_SUFFIX.
* Eliminated all uses of the custom DEBUG symbol, and instead use the
  more standard idiom "#ifndef NDEBUG".
* Compatibility fixes for Python3.
* MSVC 2008: Prevent a redefinition error when using boost::shared_ptr.
* Fixes for compatibility with libtiff 4.0.
* Fixes for MSVC debug mode having out-of-bound exceptions.
* Fixes for libjpeg 9.x.
* Compile to treat warnings as errors (you can disable this with 
  STOP_ON_WARNING=0).
* New filter: "sharp-gaussian".
* Fix various Windows build errors.
* Improvements to the build when finding IlmBase/OpenEXR.
* Various fixes to compile on ARM architecture.
* Fixes to compile on ESA/390 mainframe (!).
* testtex --threadtimes, --trials, --iters, --nodup, --wedge.  These
  are helpful in using testtext to benchmark the texture system.
* Improvements to make more tests work properly on Windows.

Developer goodies:
* Improved ASSERT and DASSERT macros to not generate warning for certain
  debug compiles; key their debug behavior by the absence of the standard
  NDEBUG idiom rather than presence of a custom DEBUG symbol; rename the
  message variants ASSERT_MSG and DASSERT_MSG.
* Change the default for Sysutil::memory_used to report resident memory
  rather than virtual process size.
* Multithread/parallel version of utility function convert_image().
* imagebufalgo.h improvements and expansion of the various DISPATCH_*
  macros.
* New Filesystem utilities: parent_path(), get_directory_entries().
* New Strutil utilities: extract_from_list_string
* spinlock tweaks make it faster than TBB's spin locks!
* By default, we no longer build or use TBB (it's considered deprecated,
  but in 1.2 can still be turned on with USE_TBB=1).
* In fmath.h, added definitions for safe_inversesqrt, safelog, safe_log2,
  safe_log10, safe_logb.
* In typedesc.h, added TypeDesc::tostring() function.
* unordered_map_concurrent.h contains a template for a thread-safe
  unordered_map that is very efficient even for large number of threads
  simultaneously accessing it.
* Documentation: Finally, a chapter in the PDF docs that fully describes
  the ImageBuf class.


Release 1.1.13 (24 Jun 2013)
----------------------------
* Texture: make sure wrap mode "black" wins over "fill" value when they
  conflict.

Release 1.1.12 (20 Jun 2013)
----------------------------
* Fix oiiotool '#' wildcard, was broken on Windows.
* Fix an overflow problem that plagued 'maketx' when running on input
  larger than 32k x 32k (among other possible failures).

Release 1.1.11 (29 May 2013)
----------------------------
* IFF input: bug in endian swap of 16 bit IFF files.
* oiiotool: fix a minor bug where tiled files were output inappropriately.
  (Had been patched in master some time ago.)
* fmath.h additions: safe_inversesqrt, safe_log, safe_log2, safe_log10,
  safe_logb.  These are versions that clamp their inputs so that they
  can't throw exceptions or return Inf or NaN.
* Fix to not incorrectly print ImageCache stats for certain broken files.

Release 1.1.10 (13 Apr 2013)
----------------------------
* IBA::fillholes() and oiiotool --fillholes can smoothly fill in alpha
  holes with nearby colors. Great for extrapolating the empty areas of
  texture atlas images so that filtered texture lookups pull in a plausible
  color at part edges.
* IBA::clamp and oiiotool --clamp clamp pixel values to a scalar or
  per-channel min and/or max, or clamp alpha to [0,1].
* IBA::rangecompress()/rangeexpand(), and oiiotool --rangecompress /
  --rangeexpand compress the excess >1 values of HDR images to a log
  scale (leaving the <= 1 part linear), and re-expand to the usual
  linear scale.  This is very helpful to reduce ringing artifacts that
  can happen when an HDR image is resized with a good filter with negative
  lobes (such as lanczos3), by doing a range compression, then the resize,
  then range expansion. It's not mathematically correct and loses energy,
  but it often makes a much more pleasing result.
* maketx --hicomp does highlight compression -- automatically doing a
  range compress before each high-quality resize step, and then a
  range expansion and clamp-to-zero (squash negative pixels) after 
  each resize.
* DPX - when writing DPX files, properly set the pixel aspect ratio.

Release 1.1.9 (2 Apr 2013)
--------------------------
* IBA::resize and oiiotool --resize/--fit: Bug fixes to resize filter
  size selection fix artifacts wherein extreme zooms could end up with
  black stripes in places where the filters fell entirely between samples.
* oiiotool --fit: fix subtle bugs with aspect ratio preservation for
  images with differing data and display windows; and allow "filter=..."
  to override the default filter used for fit.
* Resize improvement: fix potential artifacts at the image edges resulting
  from odd clamping behavior.
* Even more frame range wildcard flexibility with oiiotool --frames and
  --framepadding options.
* oiiotool --resize and --fit (and the underlying IBA::resize()) have been
  sped up significantly and are now also multithreaded.

Release 1.1.8 (15 Mar 2013)
---------------------------
* oiiotool --chappend (and ImageBufAlgo::channel_append() underneath) allow
  you to take two files and concatenate their color channels.
* oiiotool --chnames allows you to rename some or all of a file's color
  channels.
* oiiotool can loop over entire frame ranges by specifying wildcard
  filenames such as "foo.#.tif" or "bar.1-10#.exr".
* Cmake: OVERRIDE_SHARED_LIBRARY_SUFFIX allows the shared library suffix
  to be overridden (e.g., if you need to force .so names on OSX rather 
  than the usual default of .dylib).

Release 1.1.7 (21 Feb 2013)
---------------------------
* Back out dangerous change to thread.h that was in 1.1.6, which could
  cause performance problems.
* Compile fix for WIN32 in strutil.cpp
* Compile fix for Windows XP - add implementation of InterlockedExchangeAdd64

Release 1.1.6 (11 Feb 2013)
---------------------------
* Fix bug that could generate NaNs or other bad values near the poles of
  very blurry environment lookups specifically from OpenEXR latlong env maps.
* Fix bug in oiiotool --crop where it could mis-parse the geometric parameter.
* Fix bug in ImageCache::invalidate() where it did not properly delete the
  fingerprint of an invalidated file.
* Cleanup and fixes in the oiiotool command line help messages.
* New function ImageBufAlgo::paste() copies a region from one IB to another.
* oiiotool --fit resizes an image to fit into a given resolution (keeping the
  original aspect ratio and padding with black if needed).
* ImageBufAlgo::channels() and "oiiotool --ch" have been extended to allow
  new channels (specified to be filled with numeric constants) to also be
  named.
* New function ImageBufAlgo::mul() and "oiiotool --cmul" let you multiply
  an image by a scalar constant (or per-channel constant).
* Important maketx bug fix: when creating latlong environment maps as 
  OpenEXR files, it was adding energy near the poles, making low-res
  MIP levels too bright near the poles.
* Fix to "oiiotool --text" and "oiiotool --fill" -- both were
  incorrectly making the base image black rather than drawing overtop of
  the previous image.
* Fix FreeBSD compile when not using TBB.
* New oiiotool --swap exchanges the top two items on the image stack.

Release 1.1.5 (29 Jan 2013)
---------------------------
* Bug fix in ImageBufAlgo::parallel_image utility template -- care when
  not enough work chunks to dole out to all the threads (was previously
  sending work to threads with nonsensical ROI's, now we just stop when
  all the regions have been doled out).
* Additional optional argument to IBA::zover that, when nonzero, will
  treat z=0 pixels as infinitely far away, not super close.  You can turn
  this on from oiiotool with:  oiiotool --zover:zeroisinf=1 ...

Release 1.1.4 (27 Jan 2013)
---------------------------
* ImageBufAlgo::make_texture() allows you to do the same thing that
  maketx does, but from inside an application and without launching a
  shell invocation of maketx.
* oiiotool now recognizes --metamatch and --nometamatch arguments which
  cause metadata names matching (or only info NOT matching) the given
  regular expression to be printed with --info.
* oiiotool --zover does z (depth) composites (it's like a regular "over",
  but uses the z depth at each pixel to determine which of the two images
  is the foreground and which is the background).
* ImageBufAlgo::zover() performs z compositing (same as oiiotool --zover).
* ImageBufAlgo::fixNonFinite didn't work properly with 'half' image buffers.
* Performance improvements when reading and writing images.
* Fix error when writing tiled 'float' TIFF images, corrupted output.
  (Could easily happen when using 'maketx' to convert float images into
  TIFF textures.)
* Eliminate warnings when compiling with Clang 3.2.
* New CMake variable "USE_EXTERNAL_TBB" can optionally be set to force use
  of an external TBB library rather than the embedded one.
* Additional testsuite tests (doesn't affect users, but makes bugs easier
  to catch).
* Fix build problem with SHA1.cpp on some platforms.

Release 1.1.3 (9 Jan 2013)
---------------------------
* Build fix: incorrectly named OpenEXR 2.x files.
* Bug fix in oiiotool --croptofull on OSX
* Build fixes for MinGW on Windows.
* maketx --fullpixels option ignores any origin or display window in the
  source image, pretending the pixel data is the entire 0-1 image range
  starting at the origin (useful when the source image is created by an
  application that incorrectly writes it out as if it were a crop window).
* maketx no longer will clobber existing ImageDescription metadata
  when it adds SHA-1 hash or other info as it creates the texture.
* Many additional Exif and IPTC tags are correctly recognized.
* maketx and oiiotool recognize and take advantage of IPTC:ImageHistory
  metadata.

Release 1.1.2 (5 Dec 2012)
--------------------------
* maketx fixes -- was botching creation of textures from source images that
  were crop windows (pixel window smaller than display window).
* Minor bug fix to Timer when repeatedly starting and restopping (Apple only).
* Bug fix in ustring:find_last_not_of.

Release 1.1.1 (16 Nov 2012)
---------------------------
* Altered the ImageInput::read_scanlines, read_tiles, read_native_scanlines,
  read_native_tiles, read_native_deep_scanlines, read_native_deep_tiles,
  and the channel-subset version of ImageSpec::pixel_bytes, so that
  instead of specifying channel subsets as (firstchan, nchans), they are
  specified as [chbegin, chend), to match how spatial extents are done,
  as well as how channel ranges already were specified in ROI and
  ImageBuf.  We hate changing API meanings, but we really think this is
  better and more consistent.  Note that the two common uses of channel
  subsets were firstchan=0,nchans=nchannels (select all channels) and
  firstchan=foo,nchans=1, and we have rigged it so that [chbegin,chend)
  returns the same channels in both of these cases (in the latter case,
  because we retrieve a minimum of 1 channel), so we believe this is
  unlikely to break working code in the vast majority of cases.
* OpenEXR: support reading and writing V2f attributes.
* OIIO::getattribute("extension_list") returns a list of all formats
  supported, and all extensions for each format, in the form:
  "formatA:ext1,ext2,ext3;formatB:ext4,ext5;..."
* The new ImageCache per-file stats that list numbers of tiles read per
  MIPmap level have been reformatted slightly, and now print only for
  files that are actually MIP-mapped.
* New ImageCache::get_pixels() variety that can retrieve a subset of
  channels.
* Substantial speedup of ImageCache::get_pixels, used to be about 50%
  more expensive to call IC::get_pixels compared to a direct call to
  ImageInput::read_image; now is only about 15% more expensive to use
  the cache.



Release 1.1 (9 Nov 2012)
------------------------
Major new features and improvements:
* Support for reading and writing "deep" images (including OpenEXR 2.0).
* Big ImageCache/TextureSystem improvements:
  - Improved accuracy of anisotropic texture filtering, especially when
    combined with "blur."
  - Improve performance in cases with high numbers of threads using the
    TS simultaneously (mostly due to use of reader-writer locks on the
    tile cache rather than unique locks).
* New ImageBufAlgo functions:
    * `fromIplImage()` : converts/copies an OpenCV image to an ImageBuf.
    * `capture_image()` : captures from a camera device (only if OpenCV is found)
    * `over()` : Porter/Duff "over" compositing operation
    * `render_text()` : render text into an image
    * `histogram()` : compute value histogram information for an image
    * `histogram_draw()` : compute an image containing a graph of the histogram
       of another image
    * `channels()` : select, shuffle, truncate, or extend channels of an image.
* New oiiotool commands:
    * `--capture` : captures from a camera device (only if OpenCV is found)
    * `--pattern` constant : creates a constant-color image
    * `--over` : Porter/Duff "over" compositing operation
    * `--text` : render text into an image.
    * `--histogram` : computes an image containing a graph of the histogram of
       the input image.
    * `--fill` : fills a region with a solid color
    * `--ch` : select, shuffle, truncate, or extend channels

API changes:
* A new static ImageInput::open(filename [,config]) combines the old
  create-and-open idiom into a single call, which is also much more
  efficient because it won't needlessly open and close the file multiple
  times.  This is now the preferred method for reading a file, though
  the old-style create() and open() still work as always.
* Deep image support: ImageInput adds read_native_deep_scanlines,
  read_native_deep_tiles, read_native_deep_image, and ImageOutput adds
  write_deep_scanlines, write_deep_tiles, write_deep_image, as well as a
  supports("deepdata") query.  Also, a 'deep' field has been added to
  ImageSpec, and some deep data access functions have been added to
  ImageBuf.
* Altered the ImageInput::read_scanlines, read_tiles, read_native_scanlines,
  read_native_tiles, read_native_deep_scanlines, read_native_deep_tiles
  so that instead of specifying channel subsets as (firstchan, nchans),
  they are specified as [chbegin, chend), to match how spatial extents
  are done, as well as how channel ranges already were specified in ROI
  and ImageBuf.  We hate changing API meanings, but we really think this
  is better and more consistent.  Note that the two common uses of channel
  subsets were firstchan=0,nchans=nchannels (select all channels) and
  firstchan=foo,nchans=1, and we have rigged it so that [chbegin,chend)
  returns the same channels in both of these cases (in the latter case,
  because we retrieve a minimum of 1 channel), so we believe this is
  unlikely to break working code in the vast majority of cases.
* ImageInput plugins now may supply a valid_file(filename) method which 
  detects whether a given file is in the right format, less expensively
  than doing a full open() and checking for errors.  (It's probably the same
  cost as before when the file is not the right time, but when it is, it's
  less expensive because it can stop as soon as it knows it's the right
  type, without needing to do a full header read and ImageSpec setup.)
* New ImageCache::get_pixels() method that can retrieve a subset of
  channels.
* Removed various error_message() functions that had been deprecated for
  a long time (in favor of newer getmessage() functions).
* Define a namespace alias 'OIIO' that gets you past all the custom
  namespacesin a convenient way.
* TextureOpt now contains a 'subimagename' field that allows subimages
  to be addressed by name as well as by index (only for multi-image textures,
  of course).
* ImageBuf improvements:
  - A new constructor allows an ImageBuf to "wrap" an existing buffer
    memory owned by the calling application without allocating/copying.
  - Renamed the old ImageBuf::copy_pixels -> get_pixels, and it now
    works for 3D (volumetric) buffers.
  - New ImageBuf::copy(), and eliminated operator= which was confusing.
  - New ImageBuf methods: reres(), copy_metadata(), copy_pixels(),
    get_pixel_channels().
  - ImageBuf::specmod() allows writable access to the ImageSpec (caution!).
  - Better error reporting mechanism.
  - get_pixels and get_pixel_channels take optional strides.
* ImageBufAlgo changes:
  - Many ImageBufAlgo functions now take a 'ROI' that restricts the
    operation to a particular range of pixels within the image (usually
    defaulting to the whole image), and for some operations a range of
    channels.
  - zero() and fill() take ROI arguments.
  - ImageBufAlgo::CompareResults struct changed the failure and warning
    counts to imagesize_t so they can't overflow int for large images.
* OIIO::getattribute("format_list") now can retrieve the comma-separated
  list of all known image file formats.
* OIIO::getattribute("extension_list") returns a list of all formats
  supported, and all extensions for each format, in the form:
  "formatA:ext1,ext2,ext3;formatB:ext4,ext5;..."

Fixes, minor enhancements, and performance improvements:
* ImageCache/TextureSystem:
  - Anisotropic texture lookups are more robust when the derivatives are tiny.
  - Attribute "deduplicate" controls whether the identical-image
    deduplication is enabled (on by default).
  - Attribute "substitute_image" lets you force all texture references to a
    single image (helpful for debugging).
  - Texture files are no longer limited to having tile sizes that are
    powers of 2.
  - Much faster TIFF texture access (by speeding up switching of MIPmap levels).
  - More graceful handling of the inability to free handles or tiles
    under extreme conditions. Rather than assert when we can't free
    enough to stay within limits, just issue an error and allow the
    limits to be exceeded (hopefully only by a little, and temporarily).
  - Detailed per-file stats now track the number of tile reads per
    MIPmap level.
  - Attribute "unassociatedalpha" (when nonzero) requests that
    IC images not convert unassociated alpha image to associated alpha.
  - Substantial speedup of ImageCache::get_pixels, used to be about 50%
    more expensive to call IC::get_pixels compared to a direct call to
    ImageInput::read_image; now is only about 15% more expensive to use
    the cache.
* iconvert handles the int32 and uint32 cases.
* Bug fix in to_native_rectangle, which could lead to errors in certain
  data format conversions.
* iv improvements:
  - better behavior after closing the last image of the sequence.
  - file load/save dialogs can filter to show just certain image file types.
  - remember last open dialog directory
  - "About" dialog has a link to the OIIO home page
* Improve ::create to more robustly handle files whose extensions don't
  match their actual formats.
* OpenImageIO::geterror() is now thread-specific, so separate threads will
  no longer clobber each others' error messages.
* OpenEXR: support for building with OpenEXR 2.x, including use of
  multi-part EXR and "deep" data.
* Fix reading bugs in DPX and Cineon.
* DPX: fix endianness problem for 15 bit DPX output.
* PNG: fix handling of gamma for sRGB images.
* oiiotool fixes: print MIP messages correctly (it was only printing for
  the first MIP level); make sure stray "oiio:BitsPerSample" in an input
  file doesn't mess up the -d flags.
* Field3D fixes: properly catch exceptions thrown by the Field3D open();
  conform metadata to Field3D conventions; multi-layer f3d files will
  present as a multi-image file with the "oiio:subimagename" giving a
  unique name for each layer subimage; 
* OpenEXR: suppress format-specific metadata from other formats.
* OpenEXR: support reading and writing V2f attributes.
* Targa: fix several bugs that were preventing certain metadata from being
  written properly.
* TIFF: recognize the SAMPLEFORMAT_IEEEFP/bitspersample=16 as an image
  composed of "half" pixels; enable PREDICTOR_FLOATINGPOINT to give slightly
  better compression of float images.
* Handle UTF-8 paths properly on Windows.
* Removed the obsolete "iprocess" utility.
* Fix allocation and stride bugs when dealing with images having different data
  formats per channel, and tiled images with partially filled border tiles.
* Field3D: Bug fix when reading vector f3d files.
* Significant performance improvements of our atomics and spin locks when
  compiling with USE_TBB=0.
* Fix quantize() to properly round rather than truncate.
* ImageBufAlgo functions now by convention will save error messages into
  the error state of their output ImageBuf parameter.
* Improve I/O error checking -- many file reads/writes did not previously
  have their result status checked.
* Fixed missing OpenEXR open() error message.
* Clean up error reporting in iconvert.
* Fixes to handle Windows utf8 filenames properly.
* ImageBufAlgo::compare() gives a sensible error (rather than an assertion)
  if the images being compared are not float.
* maketx:
  - Better error messages for a variety of things that could go wrong when
    reading or writing image files.
  - Fixes for bug preventing certain ImageCache efficiencies.
  - new option --ignore-unassoc leaves unassociated alpha data as it is
    (no auto-conversion to associated alpha) and/or ignores the tags for
    an input file that is associated but incorrectly tagged as
    unassociated alpha.
  - Option --monochrome-detect was buggy for images with alpha.
  - Option --constant-color-detect didn't do anything; now it works.
  - New option: --compression allows you to override the default compresion.
* oiiotool & info: the --hash command had a bug wherein when applied to
  images there were MIP-mapped, would hash the lowest-res MIP level rather
  than the highest-res.  This could result in two different images, if
  they happened to have the same average color, to incorrectly report
  the same SHA-1 hash.  Note that this did NOT affect non-MIPmapped images,
  nor did it affect the SHA-1 hashing that occurred in maketx to allow
  the TextureSystem to detect duplicate textures.

Build/test system improvements:
* Various Windows build fixes, including fixes for Windows 7, and 
  improvements to running the testsuite on Windows.
* Testsuite additions and improvements: png fmath_test
* Compilation fixes on FreeBSD.
* Compilation fixes on GNU Hurd platform.
* Compilation and warning fixes for Clang 3.1.
* Add FIELD3D_HOME build variable to allow explicit path to Field3D
  implementation.
* Remove support for Boost < 1.40.
* Improved unit tests for atomics, spin locks, and rw locks.
* Avoid generating iv man pages when USE_QT=0
* New testtex options: --aniso, --stblur
* CMake option 'EXTRA_CPP_DEFINITIONS' lets custom builds inject
  site-specific compiler flags.
* Make/cmake option: HIDE_SYMBOLS=1 will try to restrict symbol visibility
  so that only symbols intended to be part of the public APIs will be
  visible in the library when linked.
* The old DLLPUBLIC and LLEXPORT macros, which could clash with other
  packages, have been renamed to OIIO_API and OIIO_EXPORT.
* Greatly reduced output when building with cmake; by default, most
  non-error status messages only are printed when VERBOSE=1 compilation
  is requested.

Developer goodies:
* Strutil new utilities: iequals, istarts_with, iends_with, to_lower,
  to_upper, strip, join.
* Use Chris Foster's 'tinyformat' for type-safe printf-like formatting,
  and this now forms the basis of Strutil::format, ustring::format, and
  many of the classes' error() methods.
* TypeDesc::equivalent() tests for type equality but allows triples with
  different' vector semantics to match.
* In timer.h, a new time_trial() template that makes multiple trial
  benchmarks easy.
* Macros for memory and cache alignment (in sysutil.h).
* Extend Filesystem::searchpath_find() to be able to search recursively.
* Strutil::strip() strips whitespace (or other specified character sets) from
  the beginning or ending of strings.
* Change threads.h to set USE_TBB=0 if undefined as a compiler flag; this
  makes it easier to use threads.h in other applications without worrying
  about TBB at all.
* Windows utf8 filename utilities path_to_windows_native and
  path_from_windows_native.



Release 1.0.10 (5 Nov 2012)
---------------------------
* ImageCache: more graceful handling of the inability to free handles or
  tiles under extreme conditions. Rather than assert when we can't free
  enough to stay within limits, just issue an error and allow the limits
  to be exceeded (hopefully only by a little, and temporarily).
* ImageCache: Detailed per-file stats now track the number of tile reads 
  per MIPmap level.
* ImageCache attribute "unassociatedalpha" (when nonzero) requests that
  IC images not convert unassociated alpha image to associated alpha.
* maketx option --ignore-unassoc leaves unassociated alpha data as it is
  (no auto-conversion to associated alpha) and/or ignores the tags for
  an input file that is associated but incorrectly tagged as unassociated
  alpha.
* oiiotool & info: the --hash command had a bug wherein when applied to
  images there were MIP-mapped, would hash the lowest-res MIP level rather
  than the highest-res.  This could result in two different images, if
  they happened to have the same average color, to incorrectly report
  the same SHA-1 hash.  Note that this did NOT affect non-MIPmapped images,
  nor did it affect the SHA-1 hashing that occurred in maketx to allow
  the TextureSystem to detect duplicate textures.

Release 1.0.9 (4 Sep 2012)
----------------------------
* Improve error messages when trying to open an OpenEXR image that doesn't
  exist or is not a valid OpenEXR file.
* Make the TextureSystem work properly with MIPmapped images whose tile
  size is not a power of 2 (mostly back-ported from master, but with
  additional fixes).

Release 1.0.8 (17 July 2012)
----------------------------
* Fix quantization/truncation bug that sometimes left tiny alpha holes in
  8 bit images (making some alpha value that should be 255, instead 254).
* TextureSystem: fix fill_channels for monochrome+alpha images to properly
  expand to "RRRA."

Release 1.0.7 (8 July 2012)
---------------------------
* Bug fix when reading vector Field3D files.
* Fix input of tiled images with per-channel formats.
* Add testsuite/nonwhole-tiles and testsuite/perchannel.
* Bug fix when reading binary PNM files.

Release 1.0.6 (12 Jun 2012)
---------------------------
* Fix allocation and stride bugs in that could overrun a buffer when
  reading tiled images whose resolution was not a whole number of tiles.
* Fix stride bugs when reading scanline images with differing data types
  per channel.
* Fixes for FreeBSD compilation.

Release 1.0.5 (3 Jun 2012)
--------------------------
* Various fixes for FreeBSD/kFreeBSD systems.
* Various fixes to compile with Clang 3.1 without warnings.
* Fixed some DPX and Cineon bugs related to channel names.
* Fixed some mangled text in the PDF documentation.
* Developer goodie: TypeDesc::equivalent() tests two TypeDesc's for
  equality, but allows 'triples' with differing vector semantics to match.

Release 1.0.4 (2 May 2012)
--------------------------
* DPX fixes for 12 bit DPX and packing methods.
* Cineon fixes: remove buggy 32 and 64 bit output, which wasn't needed;
  fix for 10 bit -> 16 bit promotion.
* bmp fix: wasn't setting oiio:BitsPerSample correctly.
* oiiotool fixes: improved argument help and add man page generation;
  print data format info correctly for non-byte bit depths; better
  inference of output tile size and data format from the inputs (when
  not explicitly requested); --resize n% was broken; print data format
  info correctly for non-byte bit depths.
* iinfo fixes: make --stats print correctly; print data format info 
  correctly for non-byte bit depths.
* Fix roundoff error when converting from float buffers to int image files.
* More precise filter normalization in ImageBufAlgo::resize (and therefore
  oiiotool --resize).

Release 1.0.3 (16 Apr 2012)
---------------------------
* Fix reading bugs in DPX and Cineon.
* iconvert handles the int32 and uint32 cases.
* Bug fix in to_native_rectangle, which could lead to errors in certain
  data format conversions.
* Various Windows build fixes, including fixes for Windows 7.
* Compilation fixes on FreeBSD.

Release 1.0.2 (19 Mar 2012)
----------------------------
* Fixed TARGA reader bug where for 16-bpp, 4-channel images, we weren't
  reading the alpha properly.
* Fix ill-formed default output names for maketx (and in the process,
  add Filesystem::replace_extension utility).
* Threading performance improvement in the texture system as a result of
  wrapping various internal "iequals" calls to pass a static locale
  rather than relying on their default behavior that would use a mutex
  underneath to access a global locale.

Release 1.0.1 (13 Mar 2012, compared to 1.0.0)
----------------------------------------------
Fixes, minor enhancements, and performance improvements:
 * Improvements in anisotropic texture filtering quality.
 * oiiotool --hash prints the SHA-1 hash of each input image.
 * oiiotool: properly print error message and exit when an input file
   cannot be opened.
 * Changed the default behavior of idiff and "oiiotool --diff" to print
   the pixel difference report only for failures (not for successful
   matches), unless in verbose (-v) mode.
Developer goodies:
 * dassert.h: New ASSERTMSG and DASSERTMSG allow even more flexible
   assertion messages with full printf argument generality.
 * Windows compilation fixes.
 * Major testsuite overhaul: All tests are copied and run in the
   build/ARCH/testsuite directory, no longer leaving any clutter in the
   "source" testsuite area.  The testing scripts have been cleaned up
   and greatly simplified.  An individual test can be run using "make
   test TEST=name" (also works with regular expressions).  The usual
   "make test" will exclude tests that are expected to be broken (such
   as tests for portions of the system that were not built because their
   required libraries were not found), but "make testall" will run all
   tests including nominally "broken" ones.


Release 1.0 (25 Feb 2012, compared to 0.10.5)
---------------------------------------------

Major new features and improvements:
 * New ImageInput & ImageOutput methods that can efficiently read/write
   multiple scanlines or tiles at a time.
 * New ImageInput methods that can read a subset of channels from an image.
 * WebP format reader/writer.
 * PSD (Adobe Photoshop) format reader.
 * RLA (Wavefront) format reader/writer.
 * Cineon support is re-enabled after various bug fixes.
 * New utility: oiiotool.  This is still a work in progress, but largely
   subsumes the functionality of iprocess, iinfo, iconvert, idiff.
 * Use OpenColorIO (www.opencolorio.org) for color space conversion, if
   detected at build time and a valid OCIO configuration is found at runtime.
   Color conversion commands have been added to oiiotool and maketx.

API changes:
 * New ImageInput & ImageOutput methods that can efficiently read/write
   multiple scanlines or tiles at a time: read_scanlines, read_tiles,
   write_scanlines, write_tiles.
 * New ImageInput methods that can read a subset of channels from an image.
 * Change the last couple functions that took min/max pixel range
   specifications to conform to our usual [begin,end) convention --
   write_rectangle and to_native_rectangle.
 * exif_encode, exif_decode now available as general utilities (previously
   were private to the JPEG plugin).
 * New ImageOutput::supports() queries: "displaywindow" queries whether the
   file format is able to handle differing display ("full") and pixel data
   windows, "negativeorigin" queries whether data origin or full/display
   origin may be negative.
 * TextureSystem and ImageCache now accept attribute "options", that is a
   comma-separated list of name=value setings (e.g.
   "max_memory_MB=256,max_files=1000").  Also, upon startup, the environment
   variables OPENIMAGEIO_TEXTURE_OPTIONS and OPENIMAGEIO_IMAGECACHE_OPTIONS
   are parsed for these startup values.
 * TextureSystem/ImageCache: add a separate "plugin_searchpath" attribute
   separate from the "searchpath" for images.

Fixes, minor enhancements, and performance improvements:
 * ImageBufAlgo new algorithms: compare, compare_Yee, isConstantChannel,
   fixNonFinite.
 * TextureOpt: add ustring-aware versions of the decode_wrapmode utility.
 * TypeDesc: allow stream << output.
 * iv: raised maximum ImageCache size from 2 GB to 8 GB.
 * PNM: fix bug where file exceptions could go uncaught.
 * Properly create coefficients for Kodak color transform.
 * iprocess: Fix bug calling read.
 * maketx new options: --opaque-detect omits alpha from texture whose input
   images had alpha=1 everywhere; --mipimage option allows custom MIP
   levels to be assembled; --fixnan repairs NaN & Inf values in the inputs.
 * Fixed bugs in sinc and Blackman-Harris filters.
 * ImageCache/TextureSystem -- new reset_stats() method resets all the
   statistics back to zero.
 * TIFF: better handling of unexpected bitsperpixel combinations; support
   the nonstandard use of IEEEFP/16bit as "half"; fix many small bugs 
   related to unusual depth depths and contig/separate conversions.
 * JPEG-2000 plugin rewritten to use OpenJpeg library instead of Jasper.
 * DPX: various bug fixes.
 * RLA plugin overhauled and now has good support for non-8-bit depths.
 * oiiotool improvements: --pop, --dup, --selectmip, --origin,
   --incolorspace, --tocolorspace, --colorconvert.
 * TextureSystem supports textures with "overscan" (including proper
   maketx support for input images with overscan).
 * TS/IC invalidate_all previously cleared all fingerprint info, but now
   only clears fingerprints for individual files that are invalidated
   (this makes for better duplicate detection).

Build system improvements:
 * Support compilation on FreeBSD.
 * Improved custom detection of boost-python on Windows.
 * Easier to compile OIIO without using TBB.

Developer goodies:
 * ArgParse enhancements: make %! indicate a bool that's set to false if
   the option is found, %@ indicates an immediate callback, allow
   callbacks for bool options, option matching ignores characters after
   ':' in the option, wrap lines at word breaks when printing usage help.
 * Generate man pages for the command-line tools.
 * Strutil additions: escape_chars, unescape_chars, word_wrap.
 * Filesystem additions: filename(), extension().
 * Sysutil additions: terminal_columns()
 * Use github.com/OpenImageIO/oiio-images project for test images that are
   too big to fit in testsuite.
 * Fixed bugs in Timer::lap().
 * Aded 'invert' algorithm to fmath.h.
 * Clarify Timer docs and fix Apple-specific bug.
 * testtex improvements: --wrap



Release 0.10.5 (20 Feb 2012)
----------------------------
* Improvements to anisotropic texture filtering: (1) fix for degenerate
  derivatives that could corrupt the filter footpring calculations,
  resulting in an infinitely long major axis; (2) more efficient subpixel
  filtering for very narrow anisotropic footprints when on the highest-res
  MIP level.


Release 0.10.4 (November 20, 2011)
----------------------------------
* Important texture bug fix: Improve robustness of texture lookups with
  very small derivatives.  The previous bug/misunderstanding had the
  result of some filter footprints with very small (but valid)
  derivatives inappropriately using the highest-resolution MIPmap level
  and maximum anisotropy, resulting in terrible performance, alising,
  and in some cases visible seams on the boundary between where this
  happened and where it didn't.  Be aware that the fixed code will make
  some areas of texture look less sharp, but that's only because it was
  aliasing before and using a totally incorrect MIPmap level.


Release 0.10.3 (November 5, 2011)
---------------------------------
* New ImageCache/TextureSystem option: "autoscanline", which, when 
  autotile is turned on, causes the virtual tiles to be the full width
  of the image scanlines, rather than square.  This improves performance
  for some apps.
* Bug fix: PNG files with both associated alpha and gamma correction lost
  precision when converting.
* Bug fix: ICO and Targa did not properly force requested (but
  unsupported) UINT16 output to be UINT8.
* maketx (and Filter classes): fixes to sinc, blackman-harris filters.
* Minor Python binding bug fixes.
* Allow stream << of TypeDesc.
* Fix minor Timer::lap() bug.


Release 0.10.2 (August 6, 2011)
-------------------------------
* Improve the performance of ustring constructor when highly multithread.
* Remove old out-of-date Doxygen html pages.


Release 0.10.1 (August 2, 2011)
-------------------------------
* Fix TextureSystem::get_texture_info(file,"exists") (and the equivalent for
  ImageCache), it was previously incorrectly giving an error if the file 
  didn't exist.
* Fixed an error where we were losing the error message if ImageInput::create
  failed.
* maketx: --hash is deprecated, the SHA-1 hash is always computed; the
  hash takes into account upstream image changes, such as resizing; the
  --filter command line argument only takes the filter name, the width
  is now automatically computed.
* Add static methods to Filter classes allowing queries about the names
  and vital info about all available filters.
* New Filesystem::is_regular() wraps the boost is_regular and catches
  exceptions.
* iv: raise the maximum ImageCache settable in the UI from 2GB to 8GB.
* Bug fixes with per-channel data formats.
* Add Strutil::escape_chars() and unescape_chars() utility functions.
* TextureOpt: add ustring-aware versions of the decode_wrapmode() utility.



Release 0.10 (June 9 2010)
--------------------------

Major new features and improvements:

* TextureSystem: fix longstanding texture quality issues: underestimation
  of anisotropic filter footprint, improved metric for determining when to
  switch to bicubic filtering, better MIP level selection for non-square
  images.
* maketx --filter allows you to specify the filter for resizing and 
  downsizing to generate MIPmap levels (this lets you choose a filter
  that is better than the default "box").
* TextureSystem option "gray_to_rgb", when set to nonzero, promotes 
  grayscale (single channel) texture lookups to RGB (rather than using
  the fill color for missing channel.
* IFF (Maya) support from Mikael Sundell.

API changes:

* TextureSystem has additional API entry points for apps that want to
  retrieve an opaque texture handle and per-thread info and then pass it
  back to texture lookups, saving some name resolution and per-thread
  retrieval time.  (The old routines that do this all automatically still
  work just fine.)
* New ImageBufAlgo utilities: setNumChannels, isConstantColor, isMonochrome,
  computePixelHashSHA1, transform.

Fixes, minor enhancements, and performance improvements:

* ImageCache/TextuerSystem:
  - option "accept_untiled" wasn't properly recognized (0.9.1); new
    attribute "accept_unmipped" (default to 1), when set to zero, will
    treat any un-MIPmapped file as an error (analogous to the existing
    "accept_untiled") (0.9.2);
  - fix deadlock when file handles are exhausted (0.9.3);
    invalidate_all() no longer closes all files unconditionally, only
    the ones actually invalidated;
  - fix longstanding problem where multiple threads could redundantly
    open and read the same file if they need it simultaneously and it
    isn't in cache already;
  - get_pixels issues a single error from a corrupt file, rather than
    reporting error after error on the same file.
* Texture: Fixes to make latlong environment maps more correct for OpenEXR
  files, which have some particular unique conventions. (0.9.1); 
  bug fix to TextureOpt default initializer that could screw up texture
  lookups. (0.9.1)
* maketx fixes: the -oiio command line option also enables hash generation;
  resize properly if any of the dimensions change (previously only did
  if ALL dimensions changed) (0.9.3); --nchannels lets you set the number 
  of output channels.
* Added ImageBufAlgo::transform to allow for 'flip' & 'flop' in iprocess.
  (0.9.1)
* DPX: fix file reading when number of channels not equal to 3 (0.9.3);
  support for endianness specification, fix lots of problems writing metadata.
* BMP: RGB-to-BGR conversion fixed, force UINT8 output; scanline size was
  incorrect when copying to temporary buffers.
* JPEG: reader is more robust to corrupted files and other problems.
* JPEG-2000: support files with more than 8 bits per channel.
* Targa: properly expand 5 bit per channel to full bytes.
* Fixed incorrectly set "ResolutionUnit" and "BitsPerSample" usage in several
  format plugins.
* Improved handling of file formats that allow unassociated alpha.
* iv: display non-Latin filenames properly.
* iconvert --noclobber option ensures that existing files aren't overwritten.
* iinfo: fixes to properly print subimage and mipmap statistics.

For developers / build issues:

* Fix USE_TBB macro on Windows build. (0.9.1)
* Fixes required for Windows compile in conjunction with OSL. (0.9.1)
* Removed some pointless debugging output to the console. (0.9.1)
* Fix subtle bug in convert_type utility function that was causing a slight
  incorrect rounding when converting float to a signed integer type. (0.9.3)
* Fix to compile properly against Boost 1.46. (0.9.3)
* Update pugixml from 0.5 to 1.0.
* Remove boost::test and gtest as dependencies, use our own macros.
* Fixes to allow use of libtiff 4.0.
* make USE_JASPER=0 USE_FIELD3D=0 make it easy to disable Jasper and
  Field3D as dependencies.
* Various fixes to make cleaner compiles with clang.
* ustring: Added find* methods that match those of std::string, expose
  make_unique, is_unique, and from_unique helper functions.
* Add Filesystem::exists and Filesystem::is_directory.



Release 0.9 (Dec 9 2010, updated Feb 23, 2011)
----------------------------------------------
Major new features:

* New format plugin: DPX
* New format plugin: Cineon (currently read only) (r1599,1600,1601,1617)
* New format plugin: Ptex (currently read only) (r1655,1664).
* New format plugin: Field3D (currently read only) (r1659,1666,1669)
* Support for files that are simultaneously multi-image and where each
  subimage may also be mipmapped (these concepts were previously
  comingled).  This mainly effects ImageInput::seek_subimage and
  ImageOutput::open, as well as some minor methods of ImageCache and
  ImageBuf.  (r1655,1656,1664,1671)
* Support for per-channel data formats via the new ImageSpec::channelformats
  vector and interpreting read_foo/write_foo format parameter of UKNOWN
  as a request for the true native format.  (r1674)
* Full support of TextureSystem environment() for lat-long maps.

API changes:

* Single-point texture lookup struct (TextureOpt) and additional
  single-point texture lookup entry points.  (r1679)
* Filter{1D,2D} class now has a destroy() method to match its create(),
  and create() accepts "bspline" and "catrom" as synonyms for the
  existing "b-spline" and "catmull-rom" fileters. (r1542)
* Add methods to ImageSpec to read/write an XML representation of the
  ImageSpec (r1574).
* Finally put all the helper classes (ustring, TypeDesc, etc.) that were
  in the main OpenImageIO namespace, as well as centralized version numbering
  and custom namespace control.
* ParamList now has a method to remove attributes.
* Color handling change: color space now is a metadata string,
  "oiio:ColorSpace", not 'linearity' data member of ImageSpec; remnants of 
  bad 'dither' ideas have been removed; "BitsPerSample" metadata has been
  renamed "oiio:BitsPerSample" and several bugs have been fixed related to
  it in some of the image plugins.
* Moved some ImageBuf methods into functions in imagebufalgo.h.

Fixes, minor enhancements, and performance improvements:

* OpenEXR: Allow read/write with different data formats per channel (r1674).
* SGI: add support for files with any number of channels (r1630).
* PNG: improve PNG write speed by 4x by adjusting compression tradeoffs
  (r1677)
* JPEG: assume sRGB unless EXIF says otherwise (r1693); fix broken JPEG
  4-channel to 3-channel conversion (r1696).
* PNM: monochrome data was output incorrectly in both binary & ascii forms;
  adopt the Netbpm convention for endianness in the 16 bit case; open binary
  image files in binary mode to avoid newline mangling (r1709).
* TIFF: more sensible checkpointing logic greatly reduces header rewriting.
* iinfo: add --stats option (r1618)
* iv: Now can sort the image list by file date, metadata date, name, or
  file path (r1514).
* ImageCache: fixed bug that allowed the max_open_files limit to be
  exceeded (r1657); raise the default IC cache size to 256 MB (r1663);
  automip unmipped files even if they are tiled (r1670); fix bug wherein
  an invalidated and modified file would continue to flush in subsequent
  invalidations, even if the file was not modified again (r1712/0.8.8).
* New ImageBuf algorithm: computePixelStats (r1618)
* Fixes in ImageCache and ImageBuf to allow correct handling of
  3D (volumetric) files. (r1659,1660)
* ImageCache fixed to ensure that multiple threads don't try to concurrently
  open the same file.
* Properly append error messages; ASSERT if the error message buffer
  exceeds 16 MB (which means somebody is failing to call geterror) (1672)
* Fix subtle Strutil::format and ustring::format crasher bugs with long
  strings (r1654 - 0.8.8).
* Print the OIIO version in the ImageCache stats so we don't guess
  when somebody sends us a log file with complaints.
* ImageCache::getattribute can retrieve its interesting internal
  statistics individually by name. (r1721)
* idiff and iv increased their IC cache size. (r1722)
* idiff bug fixes: (1) files with different number of MIPmap levels
  immediately failed, whereas they should have compared their top
  levels, and only fail if the "-a" flag was used; (2) some failure
  modes incorrectly printed a "PASS" message despite actually failing. (r1722)
* Changed the environment variable that contains the plugin search path
  from IMAGEIO_LIBRARY_PATH to OPENIMAGEIO_LIBRARY_PATH. (r1723)
* Bug fix to ImageInput::read_image -- could crash due to an internal
  buffer allocated as the wrong size. (r1724)
* Bug fixes to write_image(), related to subtle stride errors.
* Improved strhash, fewer ustring hash collisions.
* New maketx functionality: --constant-color-detect, --monochrome-detect,
  --prman, --oiio (look in docs for explanation).

For developers / build issues:

* testtex: print memory use (r1522)
* Embedded plugins are now built within the OIIO namespace, if defined (r1559).
* Fixed implementation of TypeDesc construction from a string. (r1562)
* Incorporate PugiXML for XML parsing/writing needs (r1569).
* In-progress socket I/O plugin is in the code base, but not yet fully
  supported.
* Disable python support if boost_python is not found. (r1701)


Release 0.8 (May 26 2010)
-------------------------
Major new features:

* Python bindings for the ImageInput, ImageOutput, ImageSpec, ImageBuf, 
  and ImageCache classes.
* New format plugin: SGI image file
* New format plugin: PNM/PPM/PGM/PBM
* New format plugin: DDS (currently reading only)
* New format plugin: Softimage PIC (currently reading only)

API changes:

* New "linearity" tags include AdobeRGB, Rec709, and KodakLog.
* ColorTransfer helper class can convert among the linearity types, and
  may be optionally passed to convert_image and convert_types.
* Added to fmath.h: sincos, exp2f, log2f
* Renamed ErrHandler::ErrCode enums with EH_ prefix (to fix conflicts
  with some Windows headers).
* ustring now has getstats() and memory() methods.

Fixes, minor enhancements, and performance improvements:

* ImageInput::create() error messages are more helpful.
* Fixed some error messages in FITS output, iconvert.
* maketx: Console flushes in status messages to that a calling process
  will get status messages right away.
* Fix subtle ImageCache bug with invalidate().
* ImageCache/TextureSystem have improved multithreading performance
  when large untiled files are needed simultaneously by many threads.
* TextureSystem: new 'missingcolor' texture option that, when provided,
  can specify a color that will be used for missing textures rather than
  generating errors.  (If not supplied, missing tex is still an error.)
* BMP plugin enhancements.
* TIFF: support 64-bit float pixels, proper random scanline access emulation
  for all appropriate compression types, handle incorrectly set-to-zero
  image_full_width and image_full_height. (r1515 - 0.8.1)
* PNG: properly handle palette images, unassociated alpha, gamma
  correction, endianness for 16-bit files, and has vastly better memory
  consumption due to reading scanlines individually rather than
  buffering the whole image (r1523 - 0.8.1); fix clamping/wrapping
  problem for certain values when alpha > color. (r1605 - 0.8.3)
* iv fixes: fix improper recentering after image reload; fix crash when
  image info window opened without any image files loaded; better status
  window message when image reads fail; iv goes into background properly
  in Windows; "slide show" mode; pixel view display moves if you need to
  look at pixels underneath it; 
* ImageCache bug: previously couldn't designate a cache > 2GB (because of
  integer overflow issues).
* ImageCache::get_image_info and TextureSystem::get_texture_info now respond
  to a new "exists" query that merely tests for existance of the file. (0.8.1)
* ImageCache/TextureSystem fix for a threading logic bug that could potentially
  lead to a deadlock (and definitely led to hitting a DASSERT when compiled
  for DEBUG mode). (0.8.1)
* maketx performance improvements: --noresize is now the default (use
  --resize if you really want power-of-two resizing); much better
  performance because it doesn't use ImageCache unless the image being
  converted is very large; takes advantage of multiple cores by
  automatically multithreading (the number of threads can be controlled
  by the "-t" option, with the default to use the same number of threads
  as hardware cores). (r1546 - 0.8.2)
* Fix potential crash in read_tile for files with tiles so big that they
  would not fit on the stack (heap allocation used instead). (0.8.2)
* OpenEXR: add support for vector metadata attributes. (r1554 - 0.8.2)
* Improve TIFF open error messages. (r1570 - 0.8.3)
* Make ImageCache::get_pixels() and TextureSystem::get_texels() safe for
  crop windows -- fill with zero outside the valid pixel data area. (r1579 - 0.8.3)
* In ImageCache::attribute (and by extension, TS::attribute), only
  invalidate the cache if the attributes actually CHANGED. (r1582 - 0.8.3)
* maketx: --checknan option double checks that no source image pixels
  are NaN or Inf (r1584 - 0.8.3).
* Fixed crash that could result from certain XML strings embedded in TIFF
  headers (uncaught exception). (0.8.5)
* Fixed ImageCache deadlock when using autotile. (r1631 - 0.8.6)
* Fixed a longstanding performance issue with ImageCache automip, wherein
  an unmipped file that is larger than the cache size leads to pathological
  thrashing.  The solution is to automatically raise the cache size to be
  large enough to automip the file without danger of thrashing. (r1657 - 0.8.7)

For developers / build issues:

* EMBEDPLUGINS=1 is now the default.  This means that all the format
  plugins that come with OIIO are compiled into the main library, so
  there's no reason for users to set $IMAGEIO_LIBRARY_PATH unless they
  need custom format plugins not supplied with the main distribution.
* Fix compiler warnings (mostly under Windows): TBB stuff, ustring, windows.h.
* Option to build static libraries (with 'make LINKSTATIC=1').
* Fixes to make clean compilation with gcc-4.4.2.
* Allow custom 'platform' designation in the build.
* Allow custom installation destination ('make INSTALLDIR=...').
* ustring now takes half the memory (by no longer redundantly storing the
  characters on Linux and OS X).  
* Always use TBB (better performance on Windows for atomics). [0.8.2]



Release 0.7 (Nov 26 2009)
--------------------------

Major new features:

* New format plugin: JPEG-2000 (r1050)
* New format plugin: FITS (r1287 et al)
* TextureSystem: two new entries to TextureOptions which allow the texture
  system to return the derivatives in s and t of the texture. (r1308)

API changes:

* Added imagespec() method to ImageCache and TextureSystem that returns a
  reference to the internal ImageSpec of the image.  This is much more
  efficient than get_imagespec, but beware, the pointer is only valid 
  until somebody calls invalidate() on the file.  (r1266)
* TextureOptions: eliminated the 'alpha' field.  Added the dresultds and
  dresultdt fields.
* Extend TypeDesc to include INT64 and UINT64. (r1145)

Fixes, minor enhancements, and performance improvements:

* Make EMBEDPLUGINS=1 the default. (0.7.1)
* Improvements to the Targa plugin, bringing it into compliance with
  TGA 2.0 (r1163, r1297)
* Fixed PNG-related crashes on 64 bit machines. (r1336)
* iv improvements: support for multichannel images and different color
  modes (r1129), support auto use mipmap level based on zooming (r1093),
  correct pixelview for rotated images (r1092), fix off-by-one error
  with some zoom levels (r1089).
* maketx: fixed problem where it was sometimes not setting the output
  data format to match the input data format correctly (r1290), fixed
  problems with writing EXR files with --nomipmap (r1286), fixed cases
  where data window was not the same as display window (i.e. crop or
  overscan).
* ImageCache/TextureSystem: various threading and performance improvements.
  (r1188, r1211, r1288, r1299)
* TS: fixed incorrect "texturetype" results of get_texture_info. (r1314)
* IC/TS: fixed crasher bugs when doing get_pixels of images that had
  non-zero data window origin. (r1313)
* IC/TS: better error messages and recovery from spooky open and read_tile
  failures. (r1321)
* When IC/TS reads and entire (untiled) image, the file is closed afterwards.
  This is especially helpful on Windows where open files are locked to
  writing by other processes. (r1298)
* HUGE speedup of ImageCache::get_image_info (and TS::get_texture_info)
  b replacing strcmp's with ustring == (r1281).
* IC: fixed various subtle logic errors with broken files and
  invalidate/invalidate_all. (r1252, r1279)
* IC/TS: fixed memory leak of per-thread imagecache data and subtle race 
  conditions. (r1057, r1216, r1222, r1238)
* TS: fixed problem where missing or broken textures weren't using the 
  right fill color. (r1268)
* IC: Clamp cache size to a reasonable lower limit (r1256)
* TS: improvements to filter estimation (1134) and bicubic interpolation 
  numerical stability and speed (r1166, r1179, r1333).
* IC: when autotile=0 but automip=1, fixed bug that was wasting HUGE
  amounts of memory by using the wrong resolution for mip levels! (r1147)
* IC: fix an edge case where tiles could leak. (r1044)
* Fixed some hairy static initialization problems with ustring (r1280)
* Use a spin lock rather than block in ustring constructor gives HUGE 
  speedup especially on Windows. (r1167)
* TS: Make everything work for textures whose image origin is not (0,0)
  or whose pixel data window doesn't match the image window (i.e. crop
  windows or overscan).  (r1332)
* IC/TS: Correctly invalidate files afected by recently changed "automip"
  setting. (r1337)
* IC/TS: fix crash that could occur with non-existant textures in combination
  with invalidate_all(). (r1338)
* Make create() error messages more helpful. (0.7.1)

For developers:

* Build more easily when older OpenEXR versions are found. (r1082)
* HTML Doxygen documentation on the public APIs. (r1311, r1312, et al)
* Sysutil::this_program_path finds the full path of the running program.
  (r1304)
* Better compiler-side error checking of printf-like functions (r1302)
* A new site/... area where important users with local build customization 
  needs can check in (reasonably sized) custom makefiles or other helpful
  things. (r1284)
* New ErrorHandler class, currently unused by OIIO itself, but very handy.
  (r1265)
* Fixed lots of compiler warnings.
* Upgraded to a more recent TBB, which fixed some atomic problems. (r1211)
* ustring: make string comparison safe for empty strings. (r1330)
* Include file fixes for gcc 4.4. (r1331)
* Regularize all #include references to Imath and Openexr to 
  `<OpenEXR/blah>`. (r1335)



Release 0.6 (Jul 20, 2009)
--------------------------

Major new features:

* Everything has been ported to Windows.
* iv: handle older cards or versions of OpenGL, including lack of GLSL,
  non-pow2 textures, etc.  Generally should now be usable (if slightly
  degraded functionality) for most OpenGL 1.x implementations. (r764)
* ImageBuf that only reads images is now automatically backed by
  ImageCache.  In the process, add Iterator and ConstIterator as "safe"
  and efficient ways to visit all the pixels within a region of the
  image, and eliminate the unsafe pixeladdr() method.  Also added
  ImageCache::attribute("forcefloat") to conveniently force all
  ImageCache internal storage to be float.  (r770,771,772,775,803,805)
* iv can now support "big" images, in particular larger than the OpenGL
  texture limit (4k), and also very big images via the use of ImageCache 
  (r912).
* Truevision Targa (TGA) support. (r776,792)

API changes:
* In a variety of places that specified pixel rectangles (including
  ImageCache::get_pixels and TextureSystem::get_texels), specify regions
  as (xbegin,xend,ybegin,yend) rather than (xmin, ymin, xmax, ymax).
  Note that 'end' is, like STL, one past the last pixel. (r771)
* All classes now query error messages using geterror().  Previously some
  used geterror() and others used error_message(). The old error_message
  is deprecated and will be removed in a future release (r957).

Fixes and minor enhancements:

* OpenEXR plugin improvements: don't set "textureformat" attribute
  unless it really is a mip-mapped texture.  Preserve the mipmap
  rounding mode when copying OpenEXR files, by using the
  "openexr:roundingmode" metadata (r801). Properly mark the alpha
  and z channels in the ImageSpec (r885).
* TIFF plugin improvements: handle 2 bpp images, properly name channels
  when reading palette images (r802), no longer uses the
  PREDICTOR_FLOATINGPOINT, since older versions of libtiff can't read
  those files (r752). Properly set the Exif sRGB marker (r888).
* BMP plugin improvements: allows top-down scanlines as well as bottom-up,
  correctly reads 4-, 8- and 24-bit images that have scanlines padded to 
  4-byte boundaries.
* ImageBuf algorithms: crop, add (r892).
* EXPERIMENTAL: 'iprocess' utility that lets you do some simple image
  processing operations (r892).
* ImageCache additional statistics: file open time (r743), alert if
  mip-mapped images are accessed at only their highest-res level (r743).
  Properly emulates random access reads of LZW-compressed files (r920).
* iv: fix problems displaying images whose width was not a multiple of 4
  bytes (r767), when loading small images, the window starts out a
  usable minimum size, iv always raises the window upon first opening,
  fix pixelview of alpha in RGB images (r939).
* iv: Fix off-by-one error that drew the last scanline incorrectly
  sometimes (r1089).  Give feedback when doing a slow repaint (r1089).
* iv improvements: fix skew-like problems with Intel cards, fix non-GLSL
  texture mapping, limit texture size to 4096^2 to keep GL memory use
  reasonable make "Reload" work again after breaking a few patches ago
  (r1090).
* maketx: in the case where the input texture was already float and needed
  no pow2 rounding, we didn't get the tiling or other metadata right (r824)
* ImageCache and TextureSystem do a better job of reporting low-level
  ImageInput errors up the chain (r945).
* ImageCache: new option "accept_untiled", when set to zero, will reject
  untiled images (r979).
* 'maketx --hash' embeds a SHA-1 fingerprint of the source image's
  pixels in the texture file header's "ImageDescription" field.
  ImageCache (and TextureSystem) will recognize these and eliminate
  redundant I/O when it finds multiple files with identical pixels.
  (r741,742)
* iinfo: eliminate --md5 in favor of --hash (computing SHA-1). (r749)
* Fix ImageCache and TextureSystem to have thread-specific error 
  reporting. (r1045)
* TextureSystem: fixed subtle bug in destruction order that could
  double-free per-thread data. (r1057)
* ImageCache: now get_image_info("format") returns teh native data format
  of the file. (r1058)
* maketx: properly handle input files where the data window is not the
  same as the display window or if the image offset was nonzero.  The
  correct semantics are that the DISPLAY window is what maps to [0,1] in
  texture space. (r1059)

For developers:

* Lots of fixes for Windows compilation (r754, r860)
* A build option for whether or not to use TBB for atomics.  (r780)
* New test suite entries: tiff-suite, tiff-depths (r787,788), openexr-suite,
  openexr-multires, openexr-chroma (r789,790,791).
* New unit tests for ImageBuf::zero, ImageBuf::fill, ImageBufAlgo::crop (r891).
* Reorganization of unit tests.
* Improvements to ArgParse internals and interface.
* All the macros that prevent double-inclusion of header files have had
  their names changed from FILENAME_H to OPENIMAGEIO_FILENAME_H so that
  they don't conflict with other package (r967).
* Reorganized test suite hierarchy.
* Optionally allow the entire library to be enclosed in a versioned
  namespace (via 'make NAMESPACE=foo') .
* Upgraded to a more recent version of TBB that seems to have fixed some
  bugs with atomic counters. (r1027)



Release 0.5 (31 May 2009)
-------------------------

Features:

* New image format plugins: zfile (r529), ICO (r579,585,588,619,637),
  BMP (reads only) (r580,584,614,625)
* Support for multiple subimages in iinfo (r607), iconvert, idiff (r631),
* ImageCache and TextureSystem: better stats (r528, r717), bug fixes for
  large untiled images (r558,561), anisotropic improvements, stats
  improvements, thread safety improvements (r566),
  invalidate/invalidate_all (r591), better error reporting (r606),
  thread safety fixes (r650), fix problem when filter size was precisely
  at a mipmap level it blurred to higher level (r687), avoid problems
  when blur > 1 and there is no 1x1 mip level (r687).
* maketx: --shadow (r530), --nomipmap (r531), big speedups (r699).
* idiff: add RMS error and PSNR (r622).
* OpenEXR plugin: support "openexr:lineOrder" attribute so
  random-tile-order files may be written (r569).
* API: better handling of huge images that could have sizes > 32 bits (r575)

Fixes and minor enhancements:

* iinfo: fix - lack of help message when no files specified (r513).
* maketx: make -u work properly (r517), wasn't honoring --separate (r532).
* iconvert: add --separate and --contig (r535).
* TIFF plugin: work around error in old versions of libtiff for IPTC
  packets (r674).
* JPEG plugin: if linearity is sRGB, set Exif:ColorSpace correctly (r536)
* iv: more robust to certain OpenGL versions (r550), support for OpenGL
  versions that don't support non-pow2 textures (r563), correct texture
  mapping when GL_NV_texture_rectangle is the best texture mapping
  extension we can find (r572).
* idiff: refactored to use ImageBuf internally (r541)

For developers:

* Switch to CMake for builds.
* Build enhancements: 'make USE_OPENGL=0' (r512), better handling of
  certain system OpenGL headers (r512), more robust with Qt location
  (r542), handle older Boost 1.35 (r574).
* Tests: test_libOpenImageIO (r581), ico (r621), 
* More work towards clean windows compilation (r659,672).



Release 0.4 (15 Mar 2009 - not formally released)
-------------------------------------------------
(compared to the 'initial' developer release of 1 Sep 2008)

Features:

* Lots of work on docs.
* API changes: 
    - Replaced ParamBaseType/ParamType with TypeDesc.  
    - ImageSpec: add full_{x,y,z} fields.
    - Changed ImageInput/ImageOutput create(), open(), and suports() to
      take std::string instead of char* (r297)
    - Added ImageOutput::copy_image (r428)
    - TypeDesc - distinguish COLOR from NOXFORM. (r466)
    - ImageInput:open(name,newspec,config). (r482)
* igrep utility searches metadata of images (r447,455,499)
* iconvert: add --caption, --keyword, --clear-keywords, --adjust-time
  --attrib, --inplace (r484,488,491), --compression (r354), --quality
  (r362).
* iv: put into background after launch unless -F arg (r240),
  alt-leftmouse zooms, handle sRGB correctly, GAMMA env variable, full
  HDR for half and float (r243), honor full/display window versus data
  window (r300), better view re-centering behavior (r355), fix
  orientation bugs (r363,380,381).
* TextureSystem: single point texture lookups (r247), have all routines
  return a bool giving error status, rename gettextureinfo ->
  get_texture_info, add get_imagespec, get_texels, geterror (r252,265),
  replace hard-coded get/set routines with generic
  attribute/getattribute (r321), accept non-tiled and non-mipped
  textures (r317,319,388,389,390), separate the image cache into a
  separate ImageCache class that may be used independently of
  TextureSystem (r326,327,393), better statistics including per-file
  stats (r333,360,375,429), invalidate method (r460).
* TIFF plugin: read/write EXIF, IPTC IIM, and IPTC XPM/XML data
  (r406,407,456,458)
* JPEG plugin: read/write IPTC IIM, XMP, and GPS metadata
  (r408,411,458,461), implement ImageOutput::copy_data() can copy images
  without altering pixel values (r483).

Fixes and minor enhancements:

* ImageBuf: add option to read() that allows data format conversion (r244),
  add oriented{x,y} and oriented_full_{width,height} methods (r296).
* TextureSystem: fix bicubic filetering (r309), big memory savings by
  not having libtiff use memory mapping (r332), lots of performance
  tuning (r351), anisotropic texture improvements (r364), bug fixes for
  search paths (r459).
* iinfo: print color space and file format info (r241), better printing
  of matrix metadata (r365), new options -f, -m (r501).
* idiff: bug fix - not producing difference image (r402)
* maketx: deduce default file format from extension (r275).
* All format plugins: better error detection in open() for senseless
  resolutions (r294,295)
* OpenEXR plugin: handle float as well as half data, fixes when image
  origin is not (0,0) (r291), fix leak of exr writer (r292), conform to
  common conventions for camera matrix naming (r367), regularize
  capitalization of metadata (r412)
* TIFF plugin: bug fix for combination of tile + separate (r304), fixes
  to retrieval of matrix tags (r366)
* HDR plugin: emulate random access of scanlines (r387), better error
  reporting (r451).
* JPEG plugin: respect "CompressionQuality" (r361), emulate random
  access of scanlines (r387), properly read & write JPEG_COM marker for
  comments (r403), assume sRGB unless the metadata say otherwise (r414).

For developers:

* Preliminary work on Windows port (r398,399)
* Include all the needed .h files in the dist area (r251)
* Handle older gcc (r273), older boost (r301,431), older OpenEXR
  (r301), older libtiff (r432).
* 'make EMBEDPLUGINS=1' compiles the bundled plugins right into main
  library (r302).
* Put header files in dist/ARCH/include/OpenImageIO (r303), rename
  src/libimageio -> src/libOpenImageIO (r382).



Initial developer release 0.1 (1 Sep 2008):
---------------------------------------

* ImageInput, ImageOutput, TextureSystem APIs pretty mature
* Plugins: TIFF, JPEG/JFIF, OpenEXR, PNG, HDR/rgbe
* iv - basic display, multiple images, menus, status bar, info window,
  basic prefs window, pixel view tool, zoom, pan, open, open recent,
  close, save as, view subimages, view channels, gamma, exposure,
  fit window to image, fit image to window, full screen.
* iconvert
* idiff
* maketx
* API docs for ImageInput, ImageOutput, writing plugins
* Linux and OSX
