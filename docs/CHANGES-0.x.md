These are all the release notes for versions 0.1 (Sept 2008) - 0.10 (Feb 2012).
For the full release notes of all versions, see:
[CHANGES](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/CHANGES.md).


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
  and maximum anisotropy, resulting in terrible performance, aliasing,
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
  vector and interpreting read_foo/write_foo format parameter of UNKNOWN
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
  to a new "exists" query that merely tests for existence of the file. (0.8.1)
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
* IC/TS: fix crash that could occur with non-existent textures in combination
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
    - Changed ImageInput/ImageOutput create(), open(), and supports() to
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
