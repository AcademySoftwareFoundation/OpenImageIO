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
--------------------------------------------------
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
  was longer than the ustring, but had the same character sequennce up to
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
  directory not being writeable, etc. #2181
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

Release 2.0.4 (5 Jan, 2019) -- compared to 2.0.3
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


Release 2.0 (Dec 7, 2018) -- compared to 1.8.x
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
    * texture()/texture3d(): when requesting a nonexistant "subimage",
      return the fill color, like we do when requesting nonexistant channels
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
* A new LICENSE-THIRD-PARTY.md file reproduces the full open source licenses
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
