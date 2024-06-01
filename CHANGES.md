Release 2.5.12.0 (June 1, 2024) -- compared to 2.5.11.0
-------------------------------------------------------
- *exr*: Add IOProxy support for EXR multipart output [#4263](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4263) [#4264](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4264) (by jreichel-nvidia)
- *pnm*: Improvements to pnm plugin: support for uint16 and 32-float, "pnm:bigendian" and "pnm:pfmflip" controls for output. [#4253](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4253) (by Vlad (Kuzmin) Erium)
- *ImageBuf*: Improve behavior of IB::nsubimages and other related fixes [#4228](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4228)
- *simd.h*: Fix longstanding problem with 16-wide bitcast for 8-wide HW [#4268](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4268)
- *strutil.h*: Add Strutil::eval_as_bool [#4250](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4250)
- *tests*: Add new heif test output [#4262](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4262)
- *tests*: Fix windows quoting for test [#4271](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4271)
- *build*: More warning elimination for clang18 [#4257](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4257)
- *build*: Add appropriate compiler defines and flags for SIMD with MSVC [#4266](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4266) (by Jesse Yurkovich)
- *build*: Gcc-14 support, testing, CI [#4270](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4270)
- *docs*: Fix stray references to the old repo home [#4255](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4255)


Release 2.5.11.0 (May 1, 2024) -- compared to 2.5.10.0
-------------------------------------------------------
- *dds*: DDS support more DXGI formats [#4220](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4220) (by alexguirre)
- *psd*: Add support for 16- and 32-bit Photoshop file reads [#4208](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4208) (by EmilDohne)
- *fix(fmt.h)*: Fix build break from recent fmt change [#4227](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4227)
- *fix(openexr)*: Fix out-of-bounds reads when using OpenEXR decreasingY lineOrder. [#4215](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4215) (by Aaron Colwell)
- *fix*: Don't use (DY)LD_LIBRARY_PATH as plugin search paths [#4245](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4245) (by Brecht Van Lommel)
- *fix*: Fix crash when no default fonts are found [#4249](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4249)
- *build*: Disable clang18 warnings about deprecated unicode conversion [#4246](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4246)
- *security*: Better documentation of past CVE fixes in SECURITY.md [#4238](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4238)


Release 2.5.10.1 (Apr 1, 2024) -- compared to 2.5.9.0
------------------------------------------------------
- *oiiotool*: Expression substitution now understands pseudo-metadata `NONFINITE_COUNT` that returns the number of nonfinite values in the image, thus allowing decision making about fixnan [#4171](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4171)
- *color managements*: Automatically recognize some additional color space name synonyms: "srgb_texture", "lin_rec709" and "lin_ap1". Also add common permutation "srgb_tx" and "srgb texture" as additional aliases for "srgb". [#4166](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4166)
- *openexr*: Implement copy_image for OpenEXR [#4004](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4004) (by Andy Chan)
- *heic*: Don't auto-transform camera-rotated images [#4142](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4142) [#4184](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4184)
- *hash.h*: Mismatched pragma push/pop in hash.h [#4182](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4182)
- *simd.h*: gather_mask() was wrong for no-simd fallback [#4183](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4183)
- *texture.h*: Overload decode_wrapmode to support ustringhash [#4207](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4207) (by Chris Hellmuth)
- *build*: Fix warning when Freetype is disabled [#4177](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4177)
- *build*: iv build issues with glTexImage3D [#4202](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4202) (by Vlad (Kuzmin) Erium)
- *build*: Fix buld_ninja.bash to make directories and download correctly [#4192](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4192) (by Sergio Rojas)
- *build*: Need additional include [#4194](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4194)
- *build*: FindOpenColorIO failed to properly set OpenColorIO_VERSION [#4196](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4196)
- *build*: Restore internals of strhash to compile correctly on 32 bit architectures [#4123](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4123)
- *ci*: Allow triggering CI workflow from web [#4178](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4178)
- *ci*: Make one of the Mac tests build for avx2 [#4188](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4188)
- *ci*: Enable Windows 2022 CI tests [#4195](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4195)
- *docs*: Fix some typos and add missing oiiotool expression explanations [#4169](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4169)
- *admin*: Add a ROADMAP document [#4161](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4161)


Release 2.5.9.0 (Mar 1, 2024) -- compared to 2.5.8.0
-----------------------------------------------------
- *oiiotool*: Overhaul and fix bugs in mixed-channel propogation [#4127](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4127)
- *oiiotool*: Fixes to buildinfo queries [#4150](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4150)
- *dcmtk*: Fixes for DCMTK [#4147](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4147)
- *build*: Address NEON issues in simd.h. [#4143](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4143)
- *typedesc.h*: Allow TypeDesc to have all the right POD attributes [#4162](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4162) (by Scott Wilson)
- *internals*: Various fixes for memory safety and reduce static analysis complaints [#4128](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4128)
- *internals*: Coalesce redundant STRINGIZE macros -> OIIO_STRINGIZE [#4121](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4121)
- *ci*: Start using macos-14 ARM runners, bump latest OCIO [#4134](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4134)
- *ci*: Switch away from deprecated GHA idiom set-output [#4141](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4141)
- *ci*: Add vfx platform 2024 [#4163](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4163)
- *ci*: Fix Windows CI, need to build newer openexr and adjust boost search [#4167](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4167)
- *tests*: Add test for filter values and 'filter_list' query [#4140](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4140)
- *docs*: Update SECURITY and RELEASING documentation [#4138](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4138)
- *docs*: Fix tab that was missing from the rendering on rtd [#4137](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4137)
- *docs*: Fix python example [#4139](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4139)
- *admin*: More updated relicensing code under Apache 2.0


Release 2.5.8.0 (Feb 1, 2024) -- compared to 2.5.7.0
-----------------------------------------------------
- *feat(oiiotool)*: New `--buildinfo` command prints build information,
  including version, compiler, and all library dependencies. [#4124](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4124)
- *feat(api)*: New global getattribute queries: "build:platform",
  "build:compiler", "build:dependencies" [#4124](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4124)
- *feat(openexr)*: Add support for luminance-chroma OpenEXR images. [#4070](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4070) (by jreichel-nvidia)
- *fix(raw)*: Avoid buffer overrun for flip direction cases [#4100](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4100)
- *fix(iv)*: Assume iv display gamma 2.2 if `GAMMA` env variable is not set [#4118](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4118)
- *fix*: Fixes to reduce problems identified by static analysis [#4113](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4113)
- *dev(simd.h)*: Make all-architecture matrix44::inverse() [#4076](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4076)
- *dev(simd.h)*: Fix AVX-512 round function [#4119](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4119) (by AngryLoki)
- *build(deps)*: Remove Findfmt.cmake, rely on that package's exported config. [#4069](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4069) [#4103](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4103) (by Dominik Wójt)
- *build(deps)*: Account for header changes in fmt project trunk [#4109](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4109) [#4114](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4114)
- *tests*: Shuffle some tests between directories [#4091](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4091)
- *tests*: Fix docs test, used wrong namespace [#4090](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4090)
- *docs*: Fix broken IBA color management documentation [#4104](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4104)
- *style*: Update our formatting standard to clang-format 17.0 and C++17 [#4096](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4096)


Release 2.5.7.0 (Jan 1, 2024) -- compared to 2.5.6.0
-----------------------------------------------------
- *fix(iv)*: Avoid crash with OpenGL + multi-channel images [#4087](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4087)
- *fix(png)*: Fix crash for writing large PNGs with alpha [#4074](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4074)
- *fix(ImageInput)*: Only check REST arguments if the file does not exist, avoiding problems for filenames that legitimately contain a `?` character. [#4085](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4085) (by AdamMainsTL)
- *perf(IBA)*: Improve perf of ImageBufAlgo::channels in-place operation [#4088](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4088)
- *build*: Ptex support for static library [#4072](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4072) (by Dominik Wójt)
- *build*: Add a way to cram in a custom extra library for iv [#4086](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4086)
- *build*: JPEG2000: Include the headers we need to discern version [#4073](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4073)
- *tests*: Improve color management test in imagebufalgo_test [#4063](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4063)
- *tests*: Add one more ref output for python-colorconfig test [#4065](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4065)
- *ci*: Restrict Mac ARM running [#4077](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4077)
- *ci*: Rename macro to avoid conflict during CI unity builds [#4092](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4092)
- *docs*: Fix typo [#4089](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4089)
- *docs*: Fix link to openexr test images [#4080](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4080) (by Jesse Yurkovich)
- *admin*: Account for duplicate emails in the .mailmap [#4075](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4075)
- *dev*: Faster vint4 load/store with unsigned char conversion [#4071](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4071) (by Aras Pranckevičius)

Release 2.5.6.0 (Dec 1, 2023) -- compared to 2.5.5.0
-----------------------------------------------------
- *oiiotool*: --autocc bugfix and color config inventory cleanup [#4060](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4060)
- *idiff*: Fix issue when computing perceptual diff [#4061](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4061) (by Aura Munoz)
- *ci*: Add tiff-misc reference for slightly changed error messages [#4052](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4052)
- *ci*: Remove MacOS-11 test [#4053](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4053)
- *ci*: Test against gcc-13 [#4059](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4059)
- *build*: Better cmake verbose behavior [#4037](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4037)
- *docs*: Update INSTALL.md to reflect the latest versions we've tested against [#4058](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4058)
- *admin*: Fix typo in slack-release-notifier version [#4046](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4046) [#4047](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4047)
- *admin*: More relicensing code under Apache 2.0 [#4038](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4038)

Release 2.5.5.0 (Nov 1, 2023) -- compared to 2.5.4.0
-----------------------------------------------------
- *build*: Provide compile_commands.json for use by tools [#4014](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4014) (by David Aguilar)
- *build*: Don't fail for 32 bit builds because of static_assert check [#4006](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4006)
- *build*: Protect against mismatch of OpenVDB vs C++ [#4023](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4023)
- *build*: Adjust OpenVDB version requirements vs C++17 [#4030](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4030)
- *ci*: CI tests on MacOS ARM, and fixes found consequently [#4026](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4026)
- *idiff*: Allow users to specify a directory as the 2nd argument [#4015](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4015) (by David Aguilar)
- *iv*: Implement Directory Argument Loading for iv [#4010](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4010) (by Chaitanya Sharma)
- *iv*: Split off the current image in iv into a separate window [#4017](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4017) (by Anton Dukhovnikov)
- Print unretrieved global error messages [#4005](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4005)
- *ImageBuf*: Fix crash when mutable Iterator used with read-IB [#3997](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3997)
- *exr*: Handle edge case of exr attribute that interferes with our hints [#4008](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4008)
- *raw*: LibRaw wavelet denoise options [#4028](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4028) (by Vlad (Kuzmin) Erium)
- *OpenCV*: IBA::to_OpenCV fails for ImageCache-backed images [#4013](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4013)
- *tests*: Add opencv regression test [#4024](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4024)


Release 2.5 (2.5.4.0, Oct 1, 2023) -- compared to 2.4
-----------------------------------------------------

### New minimum dependencies and compatibility changes:
* CMake: minimum needed to build OpenImageIO has been raised from 3.12 to
  3.15.  [#3924](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3924) (2.5.2.1)
* LibRaw: minimum has ben raised from 0.15 to 0.18.
  [#3921](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3921) (2.5.2.1)
* The new OpenEXR minimum is 2.4 (raised from 2.3).
  [#3928](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3928) (2.5.2.1)
* The new `fmt` library minimum is 7.0 (raised from 6.1)
  [#3973](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3973) (2.5.3.0)
* The new libjpeg-turbo (if used; it is optional) has been raised to
  2.1. [#3987](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3987) (2.5.3.1-beta2)

### ⛰️  New features and public API changes:

* TextureSystem color management: [#3761](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3761) (2.5.1.0)
    - TextureOpt and TextureOptBatch have a new field, `colortransformid`,
      which supplies an integer ID for a requested color space transformation
      to be applied as texture tiles are read. The default value 0 means no
      transformation because the texture is presumed to be in the working
      color space (this is the old behavior, and most performant). Tiles from
      the same texture file but using different color transformations are
      allowed and will not interfere with each other in the cache.
    - New `TextureSystem::get_colortransform_id(from, to)` maps from/to named
      color spaces to a color transform ID that can be passed to texture
      lookup calls.
    - `ImageCache::get_image_handle` and `TextureSystem::get_texture_handle`
      now take an optional `TextureOpt*` parameter that can supply additional
      constraints (such as color transformation) that TS/IC implementations
      may wish to split into separate handles. This is currently not used, but
      is reserved so that the API doesn't need to be changed if we use it in
      the future.
    - texture.h defines symbol `OIIO_TEXTURESYSTEM_SUPPORTS_COLORSPACE` that
      can be tested for existence to know if the new fields are in the
      TextureOpt structure.
* Extensive support for OpenColorIO 2.2 functionality and improved color
  management:
    - When building against OCIO 2.2, you can specify/use the new "built-in"
      configs, including using `ocio://default`, which will automatically be
      usd if no config is specified and the `$OCIO` variable is not set #3662
      #3707 (2.5.0.0)
    - OIIO tries to find and honor the common color space aliases
      "scene_linear", "srgb", "lin_srgb", and "ACEScg". When building against
      OCIO 2.2+, it will know which of any config's color spaces are
      equivalent to these, even if they are named something totally different,
      thanks to the magic of OCIO 2.2 built-in configs. For older OCIO (2.1 or
      older), it is less robust and may have to make best guesses based on the
      name of the color spaces it finds. [#3707](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3707) (2.5.0.0)
      [#3995](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3995) (2.5.3.2-rc1)
    - New ColorConfig methods: `getAliases()` #3662; `isColorSpaceLinear()`
      #3662; `resolve(name)` turns any color space name, alias, role, or OIIO
      name (like "sRGB") into a canonical color space name;
      `equivalent(n1,n2)` returns true if it can tell that the two names
      refer, ultimately, to equivalent color spaces #3707 (2.5.0.0)
    - New `ImageSpec::set_colorspace()` method is a more thorough way to set
      the color space data than simply setting the "oiio:ColorSpace" metadata
      (though it also does that). #3734 (2.5.0.1)
    - Improve OIIO's ability to guess which of the config's color space names
      are aliases for common spaces such as srgb, lin_srgb, and acescg (quite
      robustly if using OCIO >= 2.2, even for totally nonstandard names in the
      config). #3755 (2.5.0.1)
    - New `ColorConfig::getColorSpaceIndex()` looks up a color space index by
      its name, alias, or role. #3758 (2.5.0.1)
* oiiotool new commands and features:
    - New `--iccread` and `--iccwrite` add an ICC profile from an external
      file to the metadata of an image, or extract the ICC profile metadata
      and save it as a separate file. #3550 (2.5.0.0)
    - New `--parallel-frames` parallelizes execution over a frame range rather
      than over regions within each image operation. This should be used with
      caution, as it will give incorrect results for an oiiotool command line
      involving a frame range where the iterations have a data dependency on
      each other and must be executed in order. But in cases where the order
      of frame processing doesn't matter and there are many more frames in the
      sequence than cores, you can get a substantial performance improvement
      using this flag. [#3849](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3849) (2.5.2.0)
    - New `--no-error-exit` causes an error to not exit immediately, but
      rather to try to execute the remaining command line operations. This is
      intended primarily for unit tests and debugging. #3643 (2.5.0.0)
    - New `--colorconfiginfo` prints the full inventory of color management
      information, including color space aliases, roles, and with improved
      readability of output for larger configs. #3707 (2.5.0.0)
    - The `--resize` command takes new optional `:from=`, `:to=`, and
      `:edgeclamp=` modifiers to give more general and fine control over the
      specific correspondence between display windows in the input image and
      resized destination image (including allowing partial pixel offsets).
      #3751 #3752 (2.5.0.1)
    - New expression substitution additions:
        - New syntax for retrieving metadata `{TOP[foo]}` is similar to the
          existing `{TOP.foo}`, if there is no `foo` metadata found, the
          former evaluates to an empty string, whereas the latter is an error.
          #3619 (2.4.5/2.5.0.0)
        - `{NIMAGES}` gives current stack depth
          [#3822](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3822)  (2.5.2.0)
        - `{nativeformat}` is the pixel data format of the file, whereas the
          existing `format` has always returned the data type used in memory.
          Also, `METANATIVE` and `METANATIVEBRIEF` are the full metadata
          keywords for native file metadata, comapred to the previously
          existing `META` and `METABRIEF` which we now clarify reflect the
          in-memory representation. #3639 (2.5.0.0)
    - `--ociodisplay` now takes an optional `:inverse=1` modifier. #3650
      (2.5.0.0)
    - New `-otex` optional modifier `forcefloat=0` can improve memory use for
      enormous texture conversion.
      [#3829](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3829) (2.5.2.0)
    - `--printinfo` now takes new optional modifiers: `:native=1` ensures
      that the metadata printed is of the file, not changed by the way the
      image is stored in memory (for example, it may have been converted to
      a more convenient in-memory data type); `:verbose=1` prints verbose
      stats even if `-v` is not used; `:stats=1` prints full pixel stats,
      even if `--stats` is not used. #3639 (2.5.0.0)
    - New `--normalize` mormalizes image that represent 3D vectors (i.e.,
      divide by their length) textures. This is helpful for normal maps.
      [#3945](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3945) (by Vlad (Kuzmin) Erium) (2.5.3.0)
* ImageBufAlgo additions:
    - A new flavor of `ociodisplay()` now contains an inverse parameter.
      #3650 (2.5.0.0)
    - New `normalize()` normalizes image that represent 3D vectors (i.e.,
      divide by their length) textures. This is helpful for normal maps.
      [#3945](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3945) (by Vlad (Kuzmin)
      Erium) [#3963](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3963) (2.5.3.0)
* ImageBuf changes:
    - *ImageBuf*: Only back IB with IC when passed an IC
      [#3986](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3986) (2.5.3.1-beta2)
* ImageInput / ImageOutput:
    - New `ImageOutput::check_open()` method can be used by format writers
      authors to centralize certain validity tests so they don't need to be
      implemented separately for each file type. This is not meant to be
      called by client application code, only by format writer authors. #3686
      (2.5.0.0)
    - Add an `ImageInput::valid_file(IOProxy)` overload
      [#3826](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3826) (by Jesse Y)
      (2.5.2.0) and implement its overloads for DDS, PSD, and WEBP
      [#3831](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3831)  (by Jesse Y)
      (2.5.2.0)
    - New `ImageOutput::check_open()` and `ImageInput::open_check()` can be
      used by format reader/writer authors to centralize certain validity
      tests so they don't need to be implemented separately for each file
      type. This is not meant to be called by client application code, only by
      format reader/writer authors.
      [#3686](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3686) (2.5.0.0)
      [#3967](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3967) (2.5.3.0)
    - *ImageInput*: Add an ImageInput::valid_file(IOProxy) overload
      [#3826](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3826)  (by Jesse Y) (2.5.2.0)
    - *ImageInput*: Implement valid_file(IOProxy) overloads for DDS, PSD, and
      WEBP [#3831](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3831)  (by Jesse
      Y)  (2.5.2.0)
* New top-level `namespace OIIO` functions:
    - `OIIO::shutdown()` method [#3882](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3882) (by Ray Molenkamp)
    - `OIIO::default_thread_pool_shutdown()` [#2382](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/2382)
    - `OIIO::print()` exposes `Strutil::print()` in the main OIIO namespace.
  #3667 (2.4.6/2.5.0.0)
* OIIO::getattribute() new queries:
    - `font_list`, `font_file_list`, `font_dir_list` return, respectively, the
      list of fonts that OIIO found, the list of full paths to font files, and
      the list of directories searched for fonts. All return a single string
      that is a semicolon-separated list of the items. #3633 (2.5.0.0)
    - `opencolorio_version` returns the human-readable (e.g. "2.2.0") version
      of OpenColorIO that is being used. #3662 (2.5.0.0)
* Python bindings:
    - Implement `ImageCache.get_imagespec()` [#3982](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3982) (2.5.3.1-beta2)
* IOProxy support for additional file formats: SGI #3641, RLA #3642, IFF #3647
  (2.5.0.0), ICO input [#3919](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3919)
  (by jasonbaumeister) (2.5.2.0)
* Remove long deprecated/nonfunctional C API headers [#3567](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3567)

### 🚀  Performance improvements:

* Fixed some ImageBuf and IBA internals to avoid unnecessary/redundant zeroing
  out of newly allocated buffer memory. #3754 (2.5.0.1)
* *oiiotool*: `--parallel-frames` parallelizes execution over a frame
  range rather than over regions within each image operation.
  [#3849](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3849)  (2.5.2.0)
* *psd*: Improve memory efficiency of PSD read
  [#3807](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3807)  (2.5.2.0)
* Improvements to performance and memory when making very large textures
  [#3829](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3829) (2.5.2.0)
* OpenEXR: Change to using exr-core for reading by default [#3788](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3788)
* TextureSystem: Improve texture lookup performance by remove redundant
  instructions from tile hash [#3898](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3898) (by Curtis Black) (2.5.2.0)
* *oiiotool*: `--mosaic` improvements to type conversion avoid unnecessary
  copies and format conversions. [#3979](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3979) (2.5.3.1-beta2)

### 🐛  Fixes and feature enhancements:

* ImageInput: fix typo in debug output [#3956](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3956) (by Jesse Yurkovich)
* Python bindings:
    - Add ability to `getattribute()` of int64 and uint64 data [#3555](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3555) (2.5.0.0)
    - Fixed ability to add and retrieve `uint8[]` metadata, which on the
      python side will be numpy arrays of type uint8 (dtype='B'). This is
      important for being able to set and retrieve "ICCProfile" metadata.
      #3556 (2.5.0.0)
    - Eliminate redundant code in Python IBA bindings [#3615](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3615)
    - Improve error messages for when passing incorrect python array sizes.
      #3801 (2.5.1.0)
    - Fix arithmetic overflow in oiio_bufinfo (Python interop) [#3931](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3931) (by Jesse Yurkovich) (2.5.2.0)
* ImageBuf improvements:
    - Fixes to subtle bugs when ImageBuf is used with IOProxy. #3666
      (2.4.6/2.5.0.0)
    - Auto print uncaught ImageBuf errors. When an IB is destroyed, any errors
      that were never retrieved via `geterror()` will be printed, to aid users
      who would not notice the errors otherwise.
      [#3949](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3949) (2.5.3.0)
    - oiiotool now does immediate reads without relying on an ImageCache,
      unless the `--cache` option is used, which now both enables the use of
      an underlying IC as well as setting its size.
      [#3986](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3986) (2.5.3.1-beta2)
* ImageBufAlgo improvements:
    - IBAPrep should not zero out deep images when creating a new destination
      image. #3724 (2.5.0.0/2.4.8.0)
    - Improve error message for IBA::ocio functions [#3887](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3887)
    - UTF-8 text rendering fixes [#3935](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3935)
      (by Nicolas) (2.5.3.0)
* make_texture() / maketx / TextureSystem / ImageCache:
    - Ensure proper setting of certain metadata when using a texture as a
      source to build another texture. #3634 (2.4.5/2.5.0.0)
    - Minor improvements in statistics printing for IC. #3654 (2.5.0.0)
    - When creating a texture, don't overwrite an existing DateTime metadata
      in the source file. #3692 (2.5.0.0)
    - Fix environment mapping in batch mode when >4 channels are requested in
      a lookup. #3694 (2.5.0.0)
    - Fixed `maketx --lightprobe`, which never worked properly for images
      that weren't `float` pixel data type. #3732 (2.5.0.0/2.4.7.0)
    - Fix bad handling of `maketx --cdf`, which was trying to take an extra
      command line argument that it didn't need. #3748 (2.5.0.1)
    - Improve IC statistics appearance by omitting certain meaningless stats
      when no files were read by the IC/TS. #3765 (2.4.9.0/2.5.0.1)
    - Fixes that avoid deadlock situations on the file handle cache in certain
      scenarios with very high thread contention. #3784 (2.4.10.0/2.5.0.3)
    - maketx and oiiotool --otex: Add support for CDFs of bumpslopes channels.
      Previously, if you used both --bumpslopes and --cdf at the same time,
      the CDFs were not produced for all channels. #3793 (by Tom Knowles) (2.4.10.0/2.5.1.0)
* oiiotool improvements:
    - When `-q` (quiet mode) is used, and when an error occurs, only print the
      error message and not the full help message. #3649 (2.5.0.0)
    - Fix problems with `--point` when there is no alpha channel. #3684
      (2.4.6/2.5.0.0)
    - `--dumpdata` fix channel name output. #3687 (2.4.6/2.5.0.0)
    - `--help` now prints a much abbreviated color management section (just the
      OCIO version and config file). Use the new `--colorconfiginfo` argument
      to print the full color management information, which is both more
      readable and more detailed than what `--help` used to print. #3707
      (2.5.0.0)
    - Don't propagate unsupported channels
      [#3838](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3838)  (2.5.2.0)
    - *oiiotool*: Work around static destruction order issue #3295 [#3591](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3591) (by Aras Pranckevičius)
* ICC Profiles found in JPEG, JPEG-2000, PSN, PSD, and TIFF files are now
  examined and several key fields are extracted as separate metadata. #3554
  (2.5.0.0)
* Various protections against corrupted files [#3954](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3954) (2.5.3.0)
* BMP:
    - Fix reading 16bpp images. [#3592](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3592) (by Aras Pranckevičius) (2.4.5/2.5.0.0)
    - Protect against corrupt pixel coordinates. (TALOS-2022-1630,
      CVE-2022-38143) [#3620](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3620) (2.4.5/2.5.0.0)
    - Fix possible write errors, fixes TALOS-2022-1653 / CVE-2022-43594,
      CVE-2022-43595. #3673 (2.4.6/2.5.0.0)
    - Mark color space as sRGB, which seems likely to be true of any BMP
      files anybody encounters. #3701 (2.5.0.0)
    - Fix signed integer overflow when computing total number of pixels.
      Fixes CVE-2023-42295. [#3948](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3948) (by xiaoxiaoafeifei) (2.5.3.0)
* DDS:
    - Fix heap overflow in DDS input. #3542 (2.5.0.0)
    - Improved support for DTX5, ATI2/BC5 normal maps, R10G10B10A2
      format, RXGB, BC4U, BC5U, A8, improved low bit expansion to 8 bits.
      [#3573](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3573)
      (by Aras Pranckevičius)(2.4.4.2/2.5.0.0)
    - Fix alpha/luminance files, better testing. [#3581](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3581) (by Aras Pranckevičius) (2.4.5/2.5.0.0)
    - Optimize loading of compressed images, improves 3-5x. [#3583](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3583) (by Aras Pranckevičius) #3584
      (2.4.5/2.5.0.0)
    - Honor ImageInput thread policy [#3584](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3584)
    - Fix crashes for cubemap files when a cube face was not present, and
      check for invalid bits per pixel. (TALOS-2022-1634, CVE-2022-41838)
      (TALOS-2022-1635, CVE-2022-41999) [#3625](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3625) (2.4.5/2.5.0.0)
      (TALOS-2022-1635, CVE-2022-41999) #3625 (2.4.5/2.5.0.0)
    - Fix divide-by-0 during DXT4 DDS load [#3959](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3959) (by Jesse Yurkovich) (2.5.3.0)
* DPX:
    - Fix possible write errors, fixes TALOS-2022-1651 / CVE-2022-43592 and
      TALOS-2022-1652 / CVE-2022-43593. #3672 (2.4.6/2.5.0.0)
* FITS:
    - Ensure that the file is closed if open fails to find the right magic
      number. #3771 (2.5.1.0)
* GIF:
    - Fix potential array overrun when writing GIF files. #3789
      (2.4.10.0/2.5.1.0)
    - Prevent heap-buffer-overflow
      [#3841](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3841) (2.5.2.0) (by
      xiaoxiaoafeifei)
* HEIC:
    - Support the ".hif" extension, which seems to be used by some Canon
      cameras instead of .heif.
      [#3813](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3813)(by AdamMainsTL)
* HDR:
    - Fix a 8x (!) read performance regression for HDR files that was
      introduced in OIIO in 2.4. On top of that, speed up by another 4x beyond
      what we ever did before by speeding up the RGBE->float conversion.
      [#3588](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3588) [#3590](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3590)
      (by Aras Pranckevičius) (2.4.5/2.5.0.0)
* ICO:
    - Heap-buffer-overflow [#3872](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3872)  (by xiaoxiaoafeifei)  Fixes CVE-2023-36183.
* IFF:
    - Protect against 0-sized allocations. #3603 (2.5.0.0)
    - IOProxy support. #3647 (2.4.6/2.5.0.0)
      FIXME Temporarily disable IOProxy support to avoid bugs in #3760
      (2.5.0.1)
    - Fix possible write errors, fixes TALOS-2022-1654 / CVE-2022-43596,
      TALOS-2022-1655 / CVE-2022-43597 CVE-2022-43598, TALOS-2022-1656 /
      CVE-2022-43599 CVE-2022-43600 CVE-2022-43601 CVE-2022-43602  #3676
      (2.4.6/2.5.0.0)
    - Catch possible exception, identified by static analysis [#3681](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3681)
* JPEG
    - *jpeg*: Fix density calculation  for jpeg output
      [#3861](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3861)  (2.5.2.0) (by
      Loïc Vital) 
* JPEG2000:
    - Better pixel type promotion logic [#3878](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3878) (2.5.2.0)
* OpenEXR:
    - Fix potential use of uninitialized value when closing. #3764 (2.5.0.1)
    - Try to improve exr thread pool weirdness
      [#3864](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3864)  (2.5.2.0)
    - Controlled shutdown of IlmThread pool in all apps in which we use it.
      [#3805](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3805)  (2.5.2.0)
    - Correction to dwa vs zip logic when outputting OpenEXR [#3884](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3884) (2.5.2.0)
    - Enable openexr core library by default when recent enough
      [#3942](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3942) (2.5.3.0)
* PBM:
    - Fix accidental inversion for 1-bit bitmap pbm files. [#3731](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3731)
      (2.5.0.0/2.4.8.0)
* PNG:
    - Fix memory leaks for error conditions. #3543 #3544 (2.5.0.0)
    - Add EXIF write support to PNG output. [#3736](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3736) (by Joris Nijs) (2.5.0.1)
    - Write out proper tiff header version in png EXIF blobs
      [#3984](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3984) (by Jesse
      Yurkovich) (2.5.3.1-beta2)
    - A variety of minor optimizations to the PNG writer
      [#3980](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3980) (2.5.3.2-rc1)
    - Improve PNG write data quality when alpha is low
      [#3985](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3985) (2.5.3.2-rc1)
* PSD:
    - Fix a PSD read error on ARM architecture. #3589 (2.4.5/2.5.0.0)
    - Protect against corrupted embedded thumbnails. (TALOS-2022-1626,
      CVE-2022-41794) #3629 (2.4.5/2.5.0.0)
    - Fix thumbnail extraction. #3668 (2.4.6/2.5.0.0)
    - When reading, don't reject padded thumbnails. #3677 (2.4.6/2.5.0.0)
    - Fix wrong "oiio:UnassociatedAlpha" metadata. #3750 (2.5.0.1)
    - Handle very wide images with more than 64k resolution in either
      direction. #3806 (2.5.1.0/2.4.11)
    - Improve memory efficiency of PSD read [#3807](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3807)  (2.5.2.0)
    - Prevent simultaneous psd thumbnail reads from clashing [#3877](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3877)
    - CMYK PSD files now copy alpha correctly [#3918](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3918) (by jasonbaumeister) (2.5.2.0)
* RAW:
    - Add color metadata: pre_mul, cam_mul, cam_xyz, rgb_cam. #3561 #3569
      #3572 (2.5.0.0)
    - Update Exif orientation if user flip is set. #3669 (2.4.6/2.5.0.0)
    - Correctly handle 1-channel raw images. #3798 (2.5.1.0/2.4.11.0)
    - Fix LibRaw flip to Exif orientation conversion
      [#3847](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3847)
      [#3858](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3858)  (2.5.2.0) (by
      Loïc Vital) 
* RLA:
    - Fix potential buffer overrun. (TALOS-2022-1629, CVE-2022-36354) #3624
      (2.4.5/2.5.0.0)
    - IOProxy support. #3642 (2.5.0.0)
    - Fix possible invalid read from an empty vector during RLA load
      [#3960](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3960) (by Jesse Yurkovich) (2.5.3.0)
* SGI:
    - IOProxy support. #3641 (2.5.0.0)
* Targa:
    - Fix incorrect unique_ptr allocation. #3541 (2.5.0.0)
    - Fix string overflow safety. (TALOS-2022-1628, CVE-2022-41981)
      [#3622](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3622) (2.4.5/2.5.0.0)
    - Guard against corrupted tga files Fixes TALOS-2023-1707 /
      CVE-2023-24473, TALOS-2023-1708 / CVE-2023-22845. #3768 (2.5.1.0/2.4.8.1)
* TIFF:
    - Guard against corrupt files with buffer overflows. (TALOS-2022-1627,
      CVE-2022-41977) [#3628](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3628) (2.4.5/2.5.0.0)
    - Guard against buffer overflow for certain CMYK files. (TALOS-2022-1633,
      CVE-2022-41639) (TALOS-2022-1643, CVE-2022-41988)
      [#3632](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3632) (2.4.5/2.5.0.0)
    - While building against the new libtiff 4.5, use its new per-tiff error
      handlers to ensure better thread safety. #3719 (2.5.0.0/2.4.8.0)
    - Better logic for making TIFF PhotometricInterpretation tag and
      oiio:ColorSpace metadata correspond to each other correctly for TIFF
      files. [#3746](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3746) (2.5.0.1)
    - Fix: race condition in TIFF reader, fixes TALOS-2023-1709 /
      CVE-2023-24472. #3772 (2.5.1.0/2.4.8.1)
    - Disable writing TIFF files with JPEG compression -- it never worked
      properly and we can't seem to fix it. The fact that nobody noticed that
      it never worked is taken as evidence that nobody needs it. If asked for,
      it just uses the default ZIP compression instead. The TIFF reader can
      still read JPEG-compressed TIFF files just fine, it's only writing that
      appears problematic. #3791 (2.5.0.4)
* Zfile:
    - Zfile write safety, fixes TALOS-2022-1657 / CVE-2022-43603. #3670
      (2.4.6/2.5.0.0)
* Exif (all formats that support it, TIFF/JPEG/PSD):
    - Fix EXIF bugs where corrupted exif blocks could overrun memory.
      (TALOS-2022-1626, CVE-2022-41794) (TALOS-2022-1632, CVE-2022-41684)
      (TALOS-2022-1636 CVE-2022-41837) #3627 (2.4.5/2.5.0.0)
    - Fix typo that prevented us from correctly naming Exif
      "CameraElevationAngle" metadata.
      [#3783](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3783) (by Fabien
      Castan) (2.4.10.0/2.5.1.0)
    - Convert paramvalue string to integer when needed [#3886](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3886) (by Fabien Servant @ TCS) (2.5.2.0)
    - Squash some alignment problems caught by ubsan [#3646](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3646)
* Fix missing OIIO::getattribute support for `limits:channels` and
  `limits:imagesize_MB`. #3617 (2.4.5/2.5.0.0)
* IBA::render_text and `oiiotool --text` now can find ".ttc" font files. #3633
  (2.5.0.0)
* Fix ImageOutput::check_open error conditions. #3769 (2.5.1.0)
* Fix thread safety issue when reading ICC profiles from multiple files
  simultaneously. This could affect any files with ICC profiles. #3767
  (2.5.1.0)
* Improve searching for fonts for the text rendering functionality. #3802
  #3803 (2.5.1.0)
* Improve OpenCV support -- errors, version, half
  [#3853](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3853)  (2.5.2.0)
* Prevent possible deadlock when reading files with wrong extensions
  [#3845](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3845) 
* Wait for terminated threads to join in thread_pool::Impl::resize [#3879](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3879) (by Ray Molenkamp)

### 🔧  Internals and developer goodies

* filesystem.h:
    - Add an optional size parameter to `read_text_file()` to limit the
      maximum size that will be allocated and read (default to a limit of
      16MB). New `read_text_from_command()` is similar to `read_text_file`,
      but reads from the console output of a shell command. #3635 (2.5.0.0)
    - New `Filesystem::is_executable()` and `find_program()`. #3638
      (2.4.6/2.5.0.0)
    - Change IOMemReader constructor to take a const buffer pointer. #3665
      (2.4.6/2.5.0.0)
    - IOMemReader::pread now detects and correctly handles out-of-range
      read positions. #3712 (2.4.7/2.5.0.0)
* fmath.h:
    - Fix a wrong result with `fast_exp2()` with MSVS and sse4.2.
      [#3804](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3804) (by Eric Mehl)
      (2.5.1.0/2.4.11)
    - Prevent infinite loop in bit_range_convert [#3996](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3996) (by Jesse Yurkovich) (2.5.3.2-rc1)
* platform.h:
    - New macros for detecting MSVS 2019 and 2022. #3727 (2.5.0.0/2.4.8.0)
* simd.h:
    - Fixes to ensure safe compilation for Cuda. #3810 (2.5.1.0/2.4.11)
    - Fix sense of hiding deprecated type names [#3870](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3870) (2.5.2.0)
    - Fix broken OIIO_NO_NEON definition [#3911](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3911)
* span.h:
    - `cspan<>` template now allows for Extent template argument (as `span<>`
      already did). #3685 (2.5.0.0)
    - New custom fmt formatter can print spans. #3685 (2.5.0.0)
* string_view.h:
    - Avoid sanitizer warning unsigned integer overflow [#3678](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3678)(by wayne-arnold-adsk)
* strutil.h:
    - Add a new flavor of `utf16_to_utf8()` to convert between std::u16string
      and std::string (UTF-8 encoded). Note the contrast between this and the
      existing flavor that takes a `std::wstring` with UTF-16 encoding
      (`std::wchar/wstring` is not guaranteed to 16 bits on all platforms, but
      `u16char/u16string` is). #3553 (2.5.0.0)
    - New `trimmed_whitspace()`. [#3636](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3636) (2.4.5/2.5.0.0)
    - Use std::forward properly for sync::print().
      [#3825](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3825)  (2.5.2.0)
    - Ensure proper constexpr of string hashing [#3901](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3901)
* timer.h:
    - Minor improvements to Timer and LoggedTimer classes. #3753 (2.5.0.1)
* tiffutils.h:
    - `decode_icc_profile` extracts several fields from an ICC profile binary
      blob and adds them as metadata to an ImageSpec. #3554 (2.5.0.0)
* typedesc.h:
    - Extend TypeDescFromC template to the full set of pixel types. #3726
      (2.5.0.0/2.4.8.0)
* ustring.h:
    - Make `std::hash` work for ustring, add `operator<` for ustringhash, add
      `from_hash()` to ustringhash, make ustringhash `==` and `!=` be
      constexpr for C++17 and beyond. #3577 (2.4.5/2.5.0.0)
    - Custom fmt formatter for ustringhash that prints the string rather than
      the hash. #3614 (2.4.5/2.5.0.0)
    - Ensure that ustring hashes are always 64 bits, even on 32-bit
      architectures. #3606 (2.5.0.0)
    - Ensure safe use of this header from Cuda. #3718 (2.4.7/2.5.0.0)
    - ustringhash: Make an explicit constructor from a hash value.
      [#3778](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3778) (2.4.9.0/2.5.1.0)
    - String literal operator for ustring and ustringhash
      [#3939](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3939) (2.5.3.0)
    - Fix Cuda warnings [#3978](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3978) (2.5.3.1-beta2)
* Safety: excise the last instances of unsafe sprintf. #3705 (2.5.0.0)
* Root out stray uses of deprecated simd type names; `OIIO_DISABLE_DEPRECATED`
  [#3830](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3830)  (2.5.2.0)
* Convert iconvert.cpp stream io and sprintf to modern [#3925](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3925)
* *oiiotool*: Refactor to get rid of the global Oiiotool singleton.
  [#3848](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3848) (2.5.2.0)

### 🏗  Build/test/CI and platform ports:

* CMake build system and scripts:
    - It is now possible to `-DOpenImageIO_VERSION` to override the version
      number being built (use with extreme caution). #3549 #3653 (2.5.0.0)
    - Perform parallel builds with MSVS. #3571 (2.5.0.0)
    - New CMake cache variable `FORTIFY_SOURCE`, if enabled, builds with the
      specified gcc `_FORTIFY_SOURCE` option defined. This  may be desirable
      for people deploying OIIO in security-sensitive environments. #3575
      (2.4.5/2.5.0.0)
    - CMake config should not include a find of fmt if it's internalized.
      #3739 (2.4.7.1/2.5.0.0)
    - New CMake cache variable `OIIO_DISABLE_BOOST_STACKTRACE` to disable the
      stacktrace functionality for users who want to avoid the Boost
      stacktrace library. #3777 (by jreichel-nvidia) (2.4.9.0/2.5.1.0)
    - Check need for libatomic with check_cxx_source_compiles instead of the
      more expensive check_cxx_source_runs. #3774 (2.4.9.0/2.5.1.0)
    - Fix incorrect CMake variable name to control symbol visibility
      [#3834](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3834)  (2.5.2.0)
    -  Make sure use of `${PROJECT_NAME}` doesn't occur before the call to
       `project()`. [#3651](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3651)
    - Fix use of OIIO_LOCAL_DEPS_PATH [#3865](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3865)
    - Added check for Boost_NO_BOOST_CMAKE, ignore if already set
      [#3961](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3961) (by Mikael Sundell) (2.5.3.0)
    - Remove unnecessary headers from strutil.cpp causing build trouble
      [#3976](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3976) (by Jesse Yurkovich) (2.5.3.1-beta2)
    - Print build-time warnings for LGPL gotchas
      [#3958](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3958) (by Danny Greenstein) (2.5.3.1-beta2)

* Dependency support:
    - Support for OpenColorIO 2.2. #3644 (2.5.0.0)
    - New CMake option `INTERNALIZE_FMT` (default ON), if set to OFF, will
      force OIIO clients to use the system fmt library and will not copy the
      necessary fmt headers into the OIIO include area. #3598 (2.4.7/2.5.0.0)
    - build_libtiff.bash changed to build shared library by default. #3586
      (2.5.0.0)
    - build_openexr.bash changed to build v3.1.5 by default. #3703 (by Michael Oliver) (2.5.0.0)
    - Qt6 support for iv. #3779 (2.4.9.0/2.5.1.0)
    - Fmt 10.0 support [#3836](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3836) (2.5.2.0)
    - FFmpeg 6.0 support [#3812](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3812)  (2.5.2.0)
    - Disable new warning for fmt headers in gcc13
      [#3827](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3827)  (2.5.2.0)
    - Raise minimum CMake dependency from 3.12 to 3.15 [#3924](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3924) (2.5.2.0)
    - Raise minimum libraw to 0.18 [#3921](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3921) (2.5.2.0)
    - Raise OpenEXR minimum from 2.3 to 2.4 [#3928](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3928) (2.5.2.0)
    - Fix WebP linking if CMAKE_FIND_PACKAGE_PREFER_CONFIG is ON [#3863](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3863) (by Benjamin Buch)
    - Find OpenEXR equally well with our FindOpenEXR and exr's exported config
      file [#3862](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3862) (by Benjamin
      Buch)
    - Fix fmt vs gcc warning that had version typo [#3874](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3874)
    - Fix broken libheif < 1.13 [#3970](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3970) (2.5.3.0)
    - Use exported targets for libjpeg-turbo and bump min to 2.1
      [#3987](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3987) (2.5.3.1-beta2)
* Testing and Continuous integration (CI) systems:
    - Restored sanitizer tests which had been inadvertently disabled. #3545
      (2.5.0.0)
    - Added tests for undefined behavior sanitizer. #3565 (2.5.0.0)
    - Added tests or improved test coverage for Cineon files #3607, iinfo
      #3605 #3613 #3688 #3706, texture statistics #3612, oiiotool unit tests
      #3616, oiiotool expression substitution #3636, various oiiotool #3626
      #3637 #3643 #3649, oiiotool control flow #3643, oiiotool sequence errors
      and selecting out of range subimages or mip levels #3649, Strutil
      functionality #3655, ImageCache #3654, environment mapping #3694,
      texture3d #3699, term output #3714, igrep #3715, oiiotool --pdiff #3723,
      zover, fixnan for deep images, 2D filters #3730, pbm files #3731, maketx
      --lightprobe #3732 (2.5.0.0), TypeDesc::tostring, python ImageCache and
      ImageBuf #3745, maketx #3748 (2.5.0.1), ImageBufAlgo python functions
      #3766 (2.5.1.0), etc. #3745
    - Make testsuite/oiiotool-control run much faster by combining commands
      into fewer oiiotool invocations (speeds up testsuite) #3618 (2.5.0.0)
    - CI color related tests use the OCIO buit-in configs, when OCIO 2.2+ is
      available. #3662 (2.5.0.0)
    - Fix compiler warnings [#3833](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3833)  (2.5.2.0)
    - Fix package name for icc [#3860](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3860)  (2.5.2.0)
    - Change the few symbolic links to copies to help Windows
      [#3818](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3818)  (2.5.2.0)
    - Fix incorrect branch name when cloning openexr-images for the tests
      [#3814](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3814)  (2.5.2.0) (by
      Jesse Y) 
    - Sonar analysis should exclude stb_sprintf.h [#3609](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3609)
    - Eliminate xxhash and farmhash code from coverage analysis [#3621](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3621)
    - No longer add the ppa:ubuntu-toolchain-r/test repository [#3671](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3671)
    - Test against latest webp, and deal with its master->main repo change [#3695](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3695)
    - Fix broken Mac CI with proper numpy install [#3702](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3702)
    - Lock down to older icc so it's not broken [#3744](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3744)
    - Updates to runners, dependencies [#3786](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3786)
    - Bump 'latest versions' test to the newest openexr release [#3796](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3796)
    - Fix broken heif dependency and test [#3894](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3894)
    - Add test with new aswf containers for VFX Platform 2023 [#3875](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3875)
    - Simplify build_llvm.bash script [#3892](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3892)
    - Get rid of long-unused install_test_images.bash [#3895](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3895)
    - Test against pybind11 v2.11 [#3912](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3912)
    - Lock down icx version [#3929](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3929)
    - Fix missing simd test due to copy paste typo [#3896](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3896)
    - Bump build_openexr and build_opencolorio defaults to latest versions [#3920](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3920)
    - Test both openexr old and core versions [#3604](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3604)
    - Add benchmarking of strutil.h ways to concatenate strings. [#3787](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3787)
    - Make timer_test more robust [#3953](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3953) (2.5.3.0)
    - Tests for ABI compliance [#3983](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3983), [#3988](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3988) (2.5.3.1-beta2)
* Platform support:
    - Windows: protect against OpenEXR thread deadlock on shutdown. #3582
      (2.4.5/2.5.0.0)
    - Windows: Work around a static destruction order issue. #3591
      (2.4.5/2.5.0.0)
    - Windows: define `NOGDI` to keep the inclusion of windows.h from adding
      as many unneeded symbols. [#3596](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3596) (by Aras Pranckevičius) (2.4.5/2.5.0.0)
    - Windows: Stop including Windows.h from public OIIO headers. [#3597](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3597) (by Aras Pranckevičius) (2.5.0.0)
    - Windows: Fix windows.h pre-definitions
      [#3965](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3965) (2.5.3.0)
    - Windows on ARM64 build fixes. #3690 (2.4.6/2.5.0.0)
    - Windows: Fix unresolved external symbol for MSVS 2017.
      [#3763](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3763) (by Latios96) (2.5.0.1)
    - Windows: Fix build error with MSVC [#3832](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3832)  (by Ray Molenkamp)  (2.5.2.0)
    - MinGW: fix incorrect symbol visibility issue for ImageBuf iterators. #3578
    - Mac: Suppress warnings about deprecated std::codecvt on newest Apple
      clang. #3709 #3710 (2.4.7/2.5.0.0)
    - Mac: Fixes to make a clean build using Apple Clang 11.0.
      [#3795](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3795) (by johnfea) (2.4.10.0/2.5.1.0)
    - Mac: Fixes for latest xcode on MacOS 13.3 Ventura [#3854](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3854)  (2.5.2.0)
    - Mac: Suppress Xcode warnings [#3940](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3940) (by Anton Dukhovnikov) (2.5.3.0)
      #3722 (2.4.7/2.5.0.0)
    - Mac: Fixes for latest xcode on MacOS 13.3 Ventura [#3854](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3854)  (2.5.2.0)
    - Fixes to make a clean build on Mac using Apple Clang 11.0.
    - ARM: Fix signed/unsigned mismatch compiler errors in vbool4 methods.
    - ARM: improve SIMD operations for ARM NEON. #3599 (2.4.5/2.5.0.0)
    - ARM: Fix signed/unsigned mismatch compiler errors in vbool4 methods.
    - ARM Mac: Fix build break. #3735 (2.5.0.0/2.4.7.1)
    - Fixes to build properly on OpenBSD. [#3808](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3808) (by Brad Smith) (2.5.1.0/2.4.11)
    - Squash warning in gcc 12.1 + C++20 [#3679](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3679)
    - Work around problems with fmt library + NVPTX relating to unknown
      float128 type. [#3823](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3823)
      (by Edoardo Dominici)
    - Silence gcc new/delete warnings for texturesys [#3944](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3944) (by Shootfast) (2.5.3.0)

### 📚  Notable documentation changes:

- Docs: Better Windows build instructions in INSTALL.md. [#3602](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3602) (by Aras Pranckevičius) (2.4.5/2.5.0.0)
- ImageInput and ImageOutput docs updated to Python 3. [#3866](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3866) (by Ziggy Cross)
- Fix explanation of raw:Exposure config hint [#3889](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3889)
- Many fixes to python code examples [#3869](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3869) (by Jesse Y)
- Update ImageInput docs to not use deprecated APIs [#3907](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3907) (by Jesse Y)
- New initiative where we are (bit by bit) ensuring that all code examples in
  the documentation are tested in the testsuite and can therefore never be
  incorrect nor out of date with the evolution of the APIs.
  [#3977](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3977)
  [#3994](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3994) (2.5.3.1-beta2)
- Spruce up the main README and add "Building_the_docs"
  [#3991](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3991) (2.5.3.1-beta2)

### 🏢  Project Administration

- **Moved repo to https://github.com/AcademySoftwareFundation/OpenImageIO**.
  Also moved the "oiio-images" repo to AcademySoftareFoundation/OpenImageIO
- Added RELEASING.md documenting our versioning and release procedures #3564
#3580
- Update CONTRIBUTING and SECURITY [#3852](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3852)  (2.5.2.0)
- Update mail list to https://lists.aswf.io/g/oiio-dev [#3880](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3880)
- Add charter and other ASWF documents to the repo [#3850](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3850)
- Document use of the DCO which is now required for all PRs [#3897](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3897)
- Put logo on the main readme [#3927](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3927)
- Remove old CLAs that are no longer in effect
- Change open source license to Apache 2.0. #3899, #3903, #3904, #3906, #3914,
  #3922, #3926, #3938, #3966, #3989.
- Make sure README has links for slack and meetings [#3936](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3936)
- Update pull request and issue templates [#3946](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3946)
- Add Jesse Yurkovich to the TSC [#3937](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3937)



Release 2.4.16.0 (1 Oct 2023) -- compared to 2.4.15.0
-------------------------------------------------------
- *png*: Write out proper tiff header version in png EXIF blobs [#3984](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3984) (by Jesse Yurkovich)
- *ustring*: Fix Cuda warnings [#3978](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3978)
- *fmath*: Prevent infinite loop in bit_range_convert [#3996](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3996) (by Jesse Yurkovich)
- *build*: Fixes to work properly with fmt 10.1 (partial port of #3973)
- *admin*: Relicense code under Apache 2.0 [#3989](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3989)


Release 2.4.15.0 (1 Sep 2023) -- compared to 2.4.14.0
-------------------------------------------------------
- *bmp*: Fix signed integer overflow when computing total number of pixels. Fixes CVE-2023-42295. [#3948](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3948)  (by xiaoxiaoafeifei)
- *dds*: Fix div by 0 during DXT4 DDS load [#3959](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3959)  (by Jesse Yurkovich)
- *rla*: Invalid read from an empty vector during RLA load [#3960](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3960)  (by Jesse Yurkovich)
- *fix*:  Various protections against corrupted files [#3954](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3954)
- *fix*: Improve Utf-8 text rendering [#3935](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3935)  (by Nicolas)
- *fix*: Fix typo in debug output [#3956](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3956)  (by Jesse Yurkovich)
- *ustring.h*: String literal operator for ustring and ustringhash [#3939](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3939)
- *build* Suppress Xcode warnings [#3940](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3940)  (by Anton Dukhovnikov)
- *build*: Silence gcc new/delete warnings for texturesys [#3944](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3944)  (by Shootfast)
- *build* Added check for Boost_NO_BOOST_CMAKE, ignore if already set [#3961](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3961)  (by Mikael Sundell)
- *build* Fix broken libheif < 1.13 [#3970](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3970)
- *ci*: Make more robust timer_test [#3953](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3953)
- *admin* Relicense code under Apache 2.0 [#3938](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3938) [#3966](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3966)

Release 2.4.14.0 (1 Aug 2023) -- compared to 2.4.13.0
-------------------------------------------------------
- *ico*: IOProxy support for ICO input [#3919](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3919) (by jasonbaumeister)
- *fix(psd)*: CMYK PSD files now copy alpha correctly [#3918](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3918) (by jasonbaumeister)
- *fix(python)*: Fix arithmetic overflow in oiio_bufinfo (Python interop) [#3931](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3931) (by Jesse Yurkovich)
- *build*: Fix WebP linking if CMAKE_FIND_PACKAGE_PREFER_CONFIG is ON
  [#3863](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3863) (by Benjamin Buch)
- *build*: Find OpenEXR equally well with our FindOpenEXR and exr's exported
  config file [#3862](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3862) (by Benjamin Buch)
- *build*: Fix broken OIIO_NO_NEON definition [#3911](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3911)
- *ci*: Lock down icx version [#3929](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3929)
- *ci*: Bump build_openexr and build_opencolorio defaults to latest versions [#3920](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3920)
- *admin*: Update mail list to https://lists.aswf.io/g/oiio-dev
  [#3880](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3880)
- *admin*: Add charter and other ASWF documents to the repo
  [#3850](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3850)
- *admin*: Document use of the DCO which is now required for all PRs
  [#3897](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3897)
- *admin*: Put logo on the main readme
  [#3927](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3927)
- *admin*: Remove old CLAs that are no longer in effect
- *admin*: Change open source license to Apache 2.0. #3899, #3903, #3904,
  #3906, #3914, #3922, #3926.

Release 2.4.13.0 (1 July 2023) -- compared to 2.4.12.0
-------------------------------------------------------
- *OpenCV*: Improve OpenCV support -- errors, version, half [#3853](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3853)
- *IBA*: Improve error message for IBA::ocio functions [#3887](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3887)
- *exif*: Convert paramvalue string to integer when needed [#3886](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3886) (by Fabien Servant @ TCS)
- *exr*: Correction to dwa vs zip logic when outputting OpenEXR [#3884](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3884)
- *ico*: Heap-buffer-overflow [#3872](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3872) (by xiaoxiaoafeifei) Fixes CVE-2023-36183.
- *jpeg*: Fix density calculation  for jpeg output [#3861](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3861) (by Loïc Vital)
- *jpeg2000*: Better pixel type promotion logic [#3878](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3878)
- *psd*: Prevent simultaneous psd thumbnail reads from clashing [#3877](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3877)
- *strutil.h*: Ensure proper constexpr of string hashing [#3901](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3901)
- *build* Fix use of OIIO_LOCAL_DEPS_PATH [#3865](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3865)
- *build* Fix fmt vs gcc warning that had version typo [#3874](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3874)
- *ci*: Add test with new aswf containers for VFX Platform 2023 [#3875](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3875)
- *ci*: Fix broken heif dependency and test [#3894](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3894)
- *ci*: Simplify build_llvm.bash script [#3892](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3892)
- *tests*: Fix missing simd test due to copy paste typo [#3896](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3896)
- *docs*: Update CONTRIBUTING and SECURITY [#3852](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3852)
- *docs*: ImageInput and ImageOutput docs updated to Python 3. [#3866](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3866) (by Ziggy Cross)
- *docs*: Many fixes to python code examples [#3869](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3869) (by Jesse Y)
- *docs*: Update mail list URL [#3880](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3880)
- *docs*: Fix explanation of raw:Exposure config hint [#3889](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3889)
- *docs*: Document use of the DCO which is now required for all PRs [#3897](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3897)

Release 2.4.12.0 (1 June 2023) -- compared to 2.4.11.1
------------------------------------------------------
- *oiiotool*: Don't propagate unsupported channels [#3838](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3838)
- *oiiotool*: Improvements to performance and memory when making very large textures [#3829](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3829)
- *fix*: Prevent possible deadlock when reading files with wrong extensions [#3845](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3845)
- *gif*: Prevent possible heap buffer overflow [#3841](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3841)  (by xiaoxiaoafeifei)
- *psd*: Improve memory efficiency of PSD read [#3807](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3807)
- *raw*: Fix LibRaw flip to Exif orientation conversion [#3847](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3847)  (by Loïc Vital)
- *raw*: Raw input fix user_flip usage [#3858](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3858)  (by Loïc Vital)
- *strutil*: Use forward properly for sync::print(). [#3825](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3825)
- *build*: Fixes for latest xcode on MacOS 13.3 Ventura [#3854](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3854)
- *build*: Fix build error with MSVC [#3832](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3832)  (by Ray Molenkamp)
- *ci*: Fix warnings [#3833](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3833)
- *ci*: Fix package name for icc [#3860](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3860)

Release 2.4.11.1 (14 May 2023) -- compared to 2.4.11.0
------------------------------------------------------
- *build*: Fmt 10.0 support [#3836](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3836)

Release 2.4.11.0 (1 May 2023) -- compared to 2.4.10.0
------------------------------------------------------
* oiiotool: For expression evaluation, `NIMAGES` now evaluates to the current
  image stack depth. #3822
* Python: Improve error messages when passing wrong python array sizes. #3801
* Raw: handle 1-channel raw images. #3798
* HEIC: Support the ".hif" extension, which seems to be used by some Canon
  cameras instead of .heif. [#3813](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3813)(by AdamMainsTL)
* PSD: Fix problems reading images with width > 64k pixels. #3806
* Windows/fmath: Work around MSVS bug(?) that generated wrong code for
  fast_exp2. #3804
* Build: Fix building on OpenBSD. #3808
* Build: Refactor simd.h to disable Intel intrinsics when not on Intel
  (including Cuda compiles). #3814
* Build: Fix building against new ffmpeg 6.0. #3812
* Build: Work around problems with fmt library + NVPTX relating to unknown
  float128 type. [#3823](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3823) (by Edoardo Dominici)
* CI/test: Fix incorrect branch name when cloning openexr-images for the
  testsuite. #3814
* Test: Use copies instead of symlinks in a couple spots to help on Windows.
  #3818

Release 2.4.10.0 (1 Apr 2023) -- compared to 2.4.9.0
-----------------------------------------------------
* Exif: Fix typo that prevented us from correctly naming Exif
  "CameraElevationAngle" metadata. #3783
* IC/TS: Fixes that avoid deadlock situations on the file handle cache
  in certain scenarios with very high thread contention. #3784
* Docs: Some retroactive edits to INSTALL.md to correctly document changed
  dependencies of the 2.4 series.
* GIF: Fix potential array overrun when writing GIF files. #3789
* Build: Fixes to make a clean build on Mac using Apple Clang 11.0. #3795
* FYI: This version of OIIO should build against Clang 16.
* maketx: Fix a broken --cdf flag, which was set up to take an argument, but
  should always simply have acted as a simple boolean flag on its own. The
  incorrect way it was set up not only was useless, but also could lead to
  occasional crashes. #3748
* maketx and oiiotool --otex: Add support for CDFs of bumpslopes channels.
  Previously, if you used both --bumpslopes and --cdf at the same time, the
  CDFs were not produced for all channels. #3793

Release 2.4.9.0 (1 Mar 2023) -- compared to 2.4.8.1
-----------------------------------------------------
* Build: check need for libatomic with check_cxx_source_compiles instead of
  the more expensive check_cxx_source_runs. #3774
* Fix(IC): Avoid bad IC stats when no files were read. #3765
* Build: Add a cmake option OIIO_DISABLE_BOOST_STACKTRACE to disable use and
  dependency of boost stacktrace. #3777
* ustringhash: Make an explicit constructor from a hash value. #3778
* Build: Add ability to build against Qt6. #3779

Release 2.4.8.1 (13 Feb 2023) -- compared to 2.4.8.0
-----------------------------------------------------
* Fix(targa): guard against corrupted tga files Fixes TALOS-2023-1707 /
  CVE-2023-24473, TALOS-2023-1708 / CVE-2023-22845. #3768
* Fix: race condition in TIFF reader, fixes TALOS-2023-1709 / CVE-2023-24472.
  #3772
* Windows: Fix unresolved external symbol for MSVS 2017 #3763
* Fix: Initialize OpenEXROutput::m_levelmode() in init(). #3764
* Fix: improve thread safety for concurrent tiff loads. #3767
* Fix(fits): Make sure to close if open fails to find right magic number.
  #3771

Release 2.4.8.0 (1 Feb 2023) -- compared to 2.4.7.1
----------------------------------------------------
* oiiotool --pdiff: test, be sure to count it as making output. #3723
* IBAprep should not zero out deep images when creating dst #3724
* PBM: Fix for incorrect inverting of 1-bit pbm images. #3731
* New `ImageSpec:set_colorspace()` sets color space metadata in a consistent
  way. #3734
* BMP: set colorspace to sRGB #3701
* PNG: Add EXIF support when writing PNG files. #3735
* PSD: Fix wrong oiio:UnassociatedAlpha metadata for PSD files. #3750
* platform.h: set up macros for detecting MSVS 2019 and 2022 #3727
* typedesc.h: Extend TypeDescFromC template to the full set of pixel types
  #3726
* Testing: many improvements for testing and code coverage. #3730 #3654 #3694
  #3699 #3732 #3741 #3745 #3747
* Testing: Fix long-broken ref images for texture-icwrite test #3733
* Docs: Updated RTD docmentation style, looks much nicer. #3737
* Docs: improve description of ociodisplay and others.
* Docs: Fix old release notes to document all CVEs addressed in certain
  prior releases.

Release 2.4.7.1 (3 Jan 2023) -- compared to 2.4.7.0
----------------------------------------------------
* Fix build break for Mac ARM. #3735
* CMake config should not include a find of fmt if it's internalized. #3739

Release 2.4.7.0 (1 Jan 2023) -- compared to 2.4.6.0
----------------------------------------------------
* IOMemReader detects and errors for out-of-range read positions. #3712
* Build/Mac: Suppress some deprecation warnings when building wth the newest
  Apple clang. #3709 #3710
* ARM: Fix signed/unsigned SIMD mismatch in vbool4::load. #3722
* Build: New CMake variable `INTERNALIZE_FMT`, when set to OFF will ensure
  that the fmt headers are not internalized (copied to the installed part
  of OIIO). The default is ON, matching old behavior. #3598
* Testing: Improved testing of iinfo #3688 #3706, 'term' output #3714, igrep
  #3715.
* build_openexr.bash: bump default version of OpenEXR/Imath retrieved to be
  3.1.5. #3703
* span.h: Make sure the cspan alias also allows the Extent template
  argument; add a custom formatter to print spans. #3685
* ustring.h: `#if` guards to let the header be Cuda-safe. #3718
* Internals: refactoring to remove duplicated code for iinfo and
  `oiiotool --info`. #3688
* Internals: remove the last instances of unsafe std::sprintf. #3705

Release 2.4.6 (1 Dec 2022) -- compared to 2.4.5.0
---------------------------------------------------
* make_texture / maketx : ensure proper setting of certain metadata when
  using a texture as a source to build another texture. #3634
* Build: Make sure use of `${PROJECT_NAME}` doesn't occur before the call to
  `project()`. #3651
* Fix null pointer dereference when OCIO no configuration is found. #3652
* Support for building against OpenColorIO 2.2. #3662
* Fixes to subtle bugs when ImageBuf is used with IOProxy. #3666
* oiiotool: Fix problems with `--point` when there is no alpha channel. #3684
* oiiotool: `--dumpdata` fix channel name output. #3687
* BMP: Fix possible write errors, fixes TALOS-2022-1653 / CVE-2022-43594,
  CVE-2022-43595. #3673
* DPX: Fix possible write errors, fixes TALOS-2022-1651 / CVE-2022-43592 and
  TALOS-2022-1652 / CVE-2022-43593. #3672
* IFF files: Add IOProxy support. #3647
* IFF: Fix possible write errors, fixes TALOS-2022-1654 / CVE-2022-43596,
  TALOS-2022-1655 / CVE-2022-43597 CVE-2022-43598, TALOS-2022-1656 /
  CVE-2022-43599 CVE-2022-43600 CVE-2022-43601 CVE-2022-43602
  [#3676](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3676)
* PSD: Fix thumbnail extraction. #3668
* PSD: when reading, don't reject padded thumbnails. [#3677](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3677)
* Raw: Update Exif orientation if user flip is set. #3669
* Zfile write safety, fixes TALOS-2022-1657 / CVE-2022-43603. #3670
* filesystem.h: new `Filesystem::is_executable()` and `find_program()`. #3638
* filesystem.h: Change IOMemReader constructor to take a const buffer
  pointer. #3665
* strutil.h: new `trimmed_whitspace()`. #3636
* New `OIIO::print()` exposes `Strutil::print()` in the main OIIO namespace.
  #3667
* Testing: improved testing of oiiotool #3637 #3643 #3649, Strutil #3655

Release 2.4.5 (1 Nov 2022) -- compared to 2.4.4.2
---------------------------------------------------
* oiiotool: new commands `--iccread`  reads a named file and adds its contents
  as the ICCProfile metadata of the top image, `--iccwrite` saves the
  ICCProfile metadata of the top file to a named file. #3550
* TIFF, JPEG, JPEG-2000, PNG, and PSD files containing ICC profiles now
  extract and report extra metadata related to aspects of those profiles.
  #3554
* Python: support `int8[]` metadata and retrieving the `ICCPorofile` metadata.
  #3556
* oiiotool: New expression syntax for retrieving metadata `{TOP[foo]}` is
  similar to the existing `{TOP.foo}`, if there is no `foo` metadata found,
  the former evaluates to an empty string, whereas the latter is an error.
  #3619
* Strutil: new `utf16_to_utf8(const std::u16string&)` and
  `Strutil::utf8_to_utf16wstring()`. #3553
* ustring: make `std::hash` work for ustring, add `operator<` for ustringhash,
  add `from_hash()` to ustringhash, make ustringhash `==` and `!=` be
  constexpr for C++17 and beyond. #3577  Custom fmt formatter for ustringhash
  that prints the string rather than the hash. #3614
* Build: the version number is now a CMake cache variable that can be
  overridden (caveat emptor). #3549
* Build/security: New CMake cache variable `FORTIFY_SOURCE`, if enabled,
  builds with the specified gcc `_FORTIFY_SOURCE` option defined. This may be
  desirable for people deploying OIIO in security-sensitive environments.
  #3575
* CI: testing now includes using undefined behavior sanitizer. #3565
* Windows: protect against OpenEXR thread deadlock on shutdown. #3582
* Windows: Work around a static destruction order issue. #3591
* Windows: define `NOGDI` to keep the inclusion of windows.h from adding as
  many unneeded symbols. #3596
* MinGW: fix incorrect symbol visibility issue for ImageBuf iterators. #3578
* ARM: improve SIMD operations for ARM NEON. #3599
* Docs: New RELEASING.md documents our releasing procedures. #3564 #3580
* Docs: Better Windows build instructions in INSTALL.md. #3602
* Fix missing OIIO::getattribute support for `limits:channels` and
  `limits:imagesize_MB`. #3617
* BMP: fix reading 16bpp images. #3592
* BMP: protect against corrupt pixel coordinates. (TALOS-2022-1630,
  CVE-2022-38143) #3620
* DDS: fix alpha/luminance files, better testing. #3581
* DDS: optimize loading of compressed images, improves 3-5x. #3583 #3584
* DDS: Fix crashes for cubemap files when a cube face was not present, and
  check for invalid bits per pixel. (TALOS-2022-1634, CVE-2022-41838)
  (TALOS-2022-1635, CVE-2022-41999) #3625
* HDR: fix a 8x (!) read performance regression for HDR files that was
  introduced in OIIO in 2.4. #3588  On top of that, speed up by another 4x
  beyond what we ever did before by speeding up the RGBE->float conversion.
  #3590
* PNG: fix memory leaks when errors take an early exit. #3543 #3544
* PSD: fix a PSD read error on ARM architecture. #3589
* PSD: protect against corrupted embedded thumbnails. (TALOS-2022-1626,
  CVE-2022-41794) #3629
* RAW: additional color metadata is now recognized: `pre_mul`, `cam_mul`,
  `cam_xyz`, `rgb_cam`. #3561 #3569 #3572
* RLA: fix potential buffer overrun. (TALOS-2022-1629, CVE-2022-36354) #3624
* Targa: string overflow safety. (TALOS-2022-1628, CVE-2022-41981) #3622
* TIFF/JPEG/PSD: Fix EXIF bugs where corrupted exif blocks could overrun
  memory. (TALOS-2022-1626, CVE-2022-41794) (TALOS-2022-1631, CVE-2022-41649)
  (TALOS-2022-1632, CVE-2022-41684) (TALOS-2022-1636 CVE-2022-41837) #3627
* TIFF: guard against corrupt files with buffer overflows. (TALOS-2022-1627,
  CVE-2022-41977) #3628
* TIFF: guard against buffer overflow for certain CMYK files.
  (TALOS-2022-1633, CVE-2022-41639) (TALOS-2022-1643, CVE-2022-41988) #3632

Release 2.4.4.2 (3 Oct 2022) -- compared to 2.4.4.1
---------------------------------------------------
* DDS: Improved support for DTX5, ATI2/BC5 normal maps, R10G10B10A2
  format, RXGB, BC4U, BC5U, A8, improved low bit expansion to 8 bits.
  #3573 (2.4.4.2)
* DDS: Fix possible heap overflow on input. #3542 (2.4.4.2)

Release 2.4 (1 Oct 2022) -- compared to 2.3
----------------------------------------------
New minimum dependencies and compatibility changes:
* OpenEXR minimum is now 2.3 (raised from 2.0). #3109 (2.4.0)
* Field3D support has been removed entirely. The Field3D library appears to be
  no longer maintained, and is incompatible with modern versions of
  OpenEXR/Imath. We believe that all prior uses of Field3D use via OIIO have
  been migrated to OpenVDB. #3151 (2.4.0)

New major features and public API changes:
* Imath header and class hiding:
    - Header includes have been shuffled around so that Imath headers are not
      included from OIIO headers where they are not needed, and some OIIO
      headers that need Imath types only for few function parameters now guard
      those functions with `#if` so that Imath-taking functions are not
      visible unless the calling app has previously had an `#include` of
      Imath. If your app uses Imath types but did not include the right Imath
      headers (relying on the accidental fact of other OIIO headers
      transitively including them), you may need to adjust your includes.
      #3301 #3332 (2.4.0.2) #3406 #3474 (2.4.2)
    - New `V3fParam`, `M33fParam`, and `M44fParam` (defined in vecparam.h) are
      used just for parameter passing in public APIs, instead of Imath::V3f,
      M33f, or M44f, in order to more fully hide Imath types from our public
      interfaces. These are only parameter-passing classes, and are not useful
      as vector or matrix classes in their own right. But they seamlessly cast
      to and from other vector- and matrix-like classes. #3330 (2.4.1.0)
    - `OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY` is a new CMake cache variable
      at OIIO build time that controls whether the Imath library dependencies
      will be declared as PUBLIC (default) or PRIVATE target dependencies of
      libOpenImageIO and libOpenImageIO_Util. #3322 (4.2.0.2) #3339 (4.2.0.3)
    - For *downstream projects* that consume OIIO's exported cmake config
      files, setting CMake variable `OPENIMAGEIO_CONFIG_DO_NOT_FIND_IMATH` to
      ON will skip the find_depencency() calls for Imath and OpenEXR. To
      clarify, this is not a variable that has any effect when building OIIO,
      it's something set in the downstream project itself.  #3322 (4.2.0.2)
* The dithering that happens when saving high bit depth image data to low bit
  depth formats has been improved in several ways. It now applies when writing
  >8 bit data to <= 8 bit files, not just when the source is float or half.
  The dither pattern is now based on blue noise, and this dramatically
  improves the visual appearance. #3141 (2.4.0/2.3.10)
* TextureSystem now supports stochastic sampling. If the new TextureOpt field
  `rnd` (which now defaults to -1.0) is set to a value >= 0, the filtered
  texture lookup can use stochastic sampling to save work. The shading system
  attribute "stochastic" is set to the stochastic strategy: 0 = no stochastic
  sampling; 1 = for trilinear or anisotropic MIP modes, choose one MIP level
  stochastically instead of blending between two levels; 2 = for anisotropic
  mode, use just one anisotropic sample, chosen across the filter axis. (This
  is a bit field, so 3 combines both strategies.) We measure this speeding up
  texture lookups by 25-40%, though with more visual noise (which should be
  resolved cleanly by a renderer that uses many samples per pixel). This
  should only used for texture lookups where many samples per pixel will be
  combined, such as for surface or light shading. It should not be used for
  texture lookups that must return a single correct value (such as for
  displacement, when each grid position is sampled only once). Even when the
  "stochastic" attribute is nonzero, any individual texture call may be made
  non-stochastic by setting TextureOpt.rnd to a negative value. #3127
  (2.4.0/2.3.10) #3457 (2.4.2)
* maketx/make_texture() now supports options to store Gaussian forward and
  inverse transform lookup tables in image metadata (must be OpenEXR textures
  for this to work) to aid writing shaders that use histogram-preserving
  blending of texture tiling. This is controlled by new maketx arguments
  `--cdf`, `--cdfsigma`, `--sdfbits`, or for `IBA::make_texture()` by using
  hints `maketx:cdf`, `maketx:cdfsigma`, and `maketx:cdfbits`. #3159 #3206
  (2.4.0/2.3.10)
* oiiotool new commands and features:
    - Control flow via `--if`, `--else`, `--endif`, `--while`, `--endwhile`,
      `--for`, `--endfor` let you prototypes conditional execution and loops
      in the command sequence. #3242 (2.4.0)
    - `--set` can set variables and their values can be retrieved in
      expressions. #3242 (2.4.0)
    - Expressions now support: numerical comparisons via `<`, `>`, `<=`, `>=`,
       `==`, `!=`, `<=>`; logical operators `&&`, `||`, `!`, `not()`; string
       comparison functions `eq(a,b)` and `neq()`. #3242 #3243 (2.4.0)
    - `--oiioattrib` can set "global" OIIO control attributes for an oiiotool
      run (equivalent of calling `OIIO::attribute()`). #3171 (2.4.0/2.3.10)
    - `--repremult` exposes the previously existing `IBA::repremult()`. The
      guidance here is that `--premult` should be used for one-time conversion
      of "unassociated alpha/unpremultiplied color" to
      associated/premultiplied, but when you are starting with a premultiplied
      image and have a sequence of unpremultiply, doing some adjustment in
      unpremultiplied space, then re-premultiplying, it's `--repremult` you
      want as the last step, because it preserves alpha = 0, color > 0 data
      without crushing it to black. #3192 (2.4.0/2.3.10)
    - `--saturate` can adjust saturation level of a color image. #3190
      (2.4.0/2.3.10)
    - `--maxchan` and `--minchan` turn an N-channel image into a 1-channel
      images that for each pixel, contains the maximum value in any channel of
      the original for that pixel. #3198 (2.4.0/2.3.10)
    - `--point` lets you color one or more pixels in an image (analogous to
      IBA::render_point). #3256 (2.4.0)
    - `--warp` now takes an optional modifier `:wrap=...` that lets you set
      which wrap mode to use when sampling past the edge of the source image.
      #3341 (2.4.0.3)
    - New `--st_warp` performs warping of an image where a second image gives
      the (s,t) coordinates to look up from at every pixel. #3379
      (2.4.2/2.3.14)
    - Many attribute actions now take optional modifier `:subimages=` that
      instructs oiiotool to apply to a particular subset of subimges in
      multi-subimage files (such as multi-part exr). The commands so enabled
      include `--attrib`, `--sattrib`, `--eraseattrib`, `--caption`,
      `--orientation`, `--clear-keywords`, `--iscolorspace`. The default, if
      subimages are not specified, is to only change the attributes of the
      first subimage, unless `-a` is used, in which case the default is to
      change the attributes of all subimages. #3384 (2.4.2)
* ImageSpec :
    - New constructors to accept a string for the data type. #3245
      (2.4.0/2.3.12)
* ImageBuf/ImageBufAlgo :
    - `IBA::saturate()` can adjust saturation level of a color image. #3190
      (2.4.0/2.3.10)
    - `IBA::maxchan()` and `minchan()` turn an N-channel image into a
      1-channel images that for each pixel, contains the maximum value in any
      channel of the original for that pixel. #3198 (2.4.0/2.3.10)
    - New `IBA::st_warp()` performs warping of an image where a second image
      gives the (s,t) coordinates to look up from at every pixel. #3379
      (2.4.2/2.3.14)
    - `IBA::bluenoise_image()` returns a reference to a periodic blue noise
      image. #3141 #3254 (2.4.0/2.3.10)
* ImageCache / TextureSystem :
    - IC/TS both have added a `getattributetype()` method, which retrieves
      just the type of a named attribute. #3559 (2.4.4.0)
* Python bindings:
    - New ImageBuf constructor and reset() from a NumPy array only -- it
      deduces the resolution, channels, and data type from the array
      dimensions and type. #3246 (2.4.0/2.3.12)
    - ROI now has a working `copy()` method. #3253 (2.4.0/2.3.12)
    - ImageSpec and ParamValueList now support `'key' in spec`, `del
      spec['key']`, and `spec.get('key', defaultval)` to more fully emulate
      Python `dict` syntax for manipulating metadata. #3252 (2.3.12/2.4.0)
    - Support uint8 array attributes in and out. This enables the proper
      Python access to "ICCProfile" metadata. #3378 (2.4.1.0/2.3.14)
    - New `ImageSpec.get_bytes_attribute()` method is for string attributes,
      but in Python3, skips decoding the underlying C string as UTF-8 and
      returns a `bytes` object containing the raw byte string. #3396 (2.4.2)
    - Fixes for Python 3.8+ to ensure that it can find the OpenImageIO module
      as long as it's somewhere in the PATH. This behavior can be disabled by
      setting environment variable `OIIO_LOAD_DLLS_FROM_PATH=0`. #3470
      (2.4.0/2.3.18)
* New global OIIO attributes:
    - `"try_all_readers"` can be set to 0 if you want to override the default
      behavior and specifically NOT try any format readers that don't match
      the file extension of an input image (usually, it will try that one
      first, but it if fails to open the file, all known file readers will be
      tried in case the file is just misnamed, but sometimes you don't want it
      to do that). #3172  (2.4.0/2.3.10)
    - `"use_tbb"` if nonzero, and if OIIO was built with TBB enabled and
      found, then will use the TBB thread pool instead of the OIIO internal
      thread pool. #3473 (2.4.2.2)
    - `"version"` (read only) now retrieves the version string. #3534
      (2.3.19.0/2.4.2.2)
* Full IOProxy support has been added to TIFF #3075 (2.4.0/2.3.8), JPEG, GIF
  #3181 #3182 (2.4.0/2.3.10), DDS #3217, PNM #3219, PSD #3220, Targa #3221,
  WebP #3224, BMP #3223, JPEG-2000 #3226 (2.4.0).
* Convention change: Image readers who wish to convey that the color space of
  an input image is a simple gamma-corrected space will now call the color
  space "GammaX.Y" (previously we sometimes used this, but sometimes called it
  "GammaCorrectedX.Y"). #3202 (2.4.0)
* `oiioversion.h` now defines symbols `OIIO_USING_IMATH_VERSION_MAJOR` and
  `OIIO_USING_IMATH_VERSION_MINOR` that reveal which Imath version was used
  internally to OIIO when it was built (which may be different than the
  version found when the downstream app is being compiled). #3305 (2.4.0.1)
* Most of the major APIs (including ImageInput, ImageOutput, and ImageBuf)
  that took a std::string or string_view to indicate a filename (assumed to
  support UTF-8 encoding of Unicode filenames) now have additional versions
  that directly take a `std::wstring`, thus supporting UTF-16 Unicode
  filenames as "wide strings". #3312 #3318 (2.4.0.1)
* The ColorConfig API adds new calls `getDisplayViewColorSpaceName()` and
  `getDisplayViewLooks()` that expose the underlying OpenColorIO
  functionality. #3319 (2.4.0.2)
* Many long-deprecated functions in imageio.h and imagbufalgo.h are now
  marked as OIIO_DEPRECATED, and therefore may start to generate warnings
  if used by downstream software. #3328 (2.4.1.0)

Performance improvements:
* Raise the default ImageCache default tile cache from 256MB to 1GB. This
  should improve performance for some operations involving large images or
  images with many channels. #3180 (2.4.0/2.3.10)
* Performance of JPEG-2000 I/O is improved by 2-3x due to multithreading,
  but only if you are using a sufficiently new version of OpenJPEG (2.2
  for encoding, 2.4 for decoding). #2225 (2.3.11/2.4.0)
* Dramatically speed up (50-100x) the implementation of Strutil iequals,
  iless, starts_with, istarts_with, ends_with, iends_with. This in turn speeds
  up ParamValueList find/get related methods, ImageSpec find/get methods, and
  TS::get_texture_info. #3388 (2.4.1.1)
* Renderer users of the TextureSystem might see up to a ~40% speedup if
  using the new stochastic sampling features. #3127 #3457
* Speed up reading of uncompressed DDS images by about 3x. #3463 (2.4.2.0)

Fixes and feature enhancements:
* ImageSpec:
    - Implemented deserialization of extra_attribs from XML. #3066 (2.4.0/2.3.8)
    - Allow `getattribute("format")` to retrieve the name of the pixel data
      type. #3247 (2.4.0)
* ImageInput / ImageOutput:
    - Protected methods that make it easier to implement support for IOProxy
      in image readers and writers. #3231 (2.4.0)
    - Fix crash when ioproxy is passed to a plugin that doesn't support it.
      #3453 (2.4.2)
* ImageBuf / ImageBufAlgo:
    - Fix ImageBuf::read bug for images of mixed per-channel data types. #3088
      (2.4.0/2.3.8)
    - `IBA::noise()` now takes "blue" as a noise name. Also, "white" is now
      the preferred name for what used to be "uniform" (which still works as a
      synonym). #3141 (2.4.0/2.3.10)
    - Refactor ImageBuf::Iterator, ConstIterator templates, reduces compile
      time substantially. #3195 (2.4.0)
    - IBA functions taking a `cspan<>` now more flexibly can be passed
      an init list like `{ 0.2f, 0.4f, 0.5f }` instead of needing to wrap it
      in a `cspan<float>()` constructor. #3257 (2.3.12/2.4.0)
    - `make_texture()`: ensure that "maketx:ignore_unassoc" is honored.
      #3269 (2.4.0.1/2.3.12)
    - `IBA::computePixelStats()` improved precision. #3353 (2.4.1.0/2.3.14)
    - `IBA::isConstantColor()` is faster -- now if one thread finds its
      portion not constant, it can signal the other threads to stop
      immediately instead of completing their regions. #3383 (2.4.1.1)
    - A new flavor of `IBA::compare()` allows relative as well as absolute
      error thresholds. #3508 (2.3.19.0/2.4.2.2)
* ImageCache / TextureSystem / maketx:
    - When textures are created with the "monochrome_detect" feature enabled,
      which turns RGB textures where all channels are always equal into true
      single channel greyscale, the single channel that results is now
      correctly named "Y" instead of leaving it as "R" (it's not red, it's
      luminance). #3205 (2.4.0/2.3.10)
    - Enhance safety/correctness for untiled images that exceed 2GB size
      (there was an integer overflow problem in computing offsets within
      tiles). #3232 (2.3.11/2.4.0)
    - Improve error propagation from ImageCache to higher levels, especially
      for tile-reading errors encountered during ImageBuf iterator use, and
      ImageCache errors encountered when using the TextureSystem. #3233
      (2.4.0)
    - Support an additional UDIM pattern `<UVTILE>`, which is specified by
      MaterialX. #3280 #3285 (2.3.12/2.4.0.1)
    - Add support for UDIM pattern `<uvtile>` (used by Clarisse & V-Ray). #3358
      (2.4.1.0/2.3.14)
    - The `maketx --handed` option, or `oiiotool -attrib -otex:handed=...`, or
      adding "handed" metadata to the configuration ImageSpec being passed to
      `IBA::make_texture()` is now supported for communicating the handedness
      of a vector displacement or normal map. #3331 (2.4.0.2)
    - Speed up UDIM lookups by eliminating internal mutex. #3417 (2.4.0)
    - TextureSystem: Fix typo that prevented "max_tile_channels" attribute from
      being set or retrieved. (2.4.2/2.3.17)
* oiiotool:
    - `--ch` now has virtually no expense if the arguments require no change
      to the channel order or naming (previously, it would always incur an
      image allocation and copy even if no real work needed to be done). #3068
      (2.4.0/2.3.8)
    - `--ch` now warns if you specify a channel name that was not present
      in the input image. #3290 (2.4.0.1)
    - `--runstats` timing report has been improved and now more accurately
      attributes time to each operation. In particular, input I/O is now
      credited to "-i" rather than being incorrectly attributed to the other
      ops that happen to trigger I/O of previously mentioned files. #3073
      (2.4.0/2.3.8)
    - Allow quotes in command modifiers. #3112 (2.4.0/2.3.9)
    - Fix `--dumpdata` getting the formatting of floating point values wrong.
      #3131 (2.4.0/2.3.9)
    - `--dumpdata:C=name` causes the dumped image data to be formatted with
      the syntax of a C array. #3136 (2.4.0/2.3.9)
    - `--noise` now takes "blue" as an additional noise type. #3141
      (2.4.0/2.3.10)
    - `-d` now accepts "uint1", "uint2", "uint4", and "uint6" for formats that
      support such low bit depths (TIFF). #3141 (2.4.0/2.3.10)
    - `--invert` fixed to avoid losing the alpha channel values. #3191
      (2.4.0/2.3.10)
    - Fix bug when autocropping output images when the entire pixel data
      window is in the negative coordinate region. #3164 (2.4.0/2.3.10)
    - Improved detection of file reading failures. #3165 (2.4.0/2.3.10)
    - All commands that do filtering (--rotate, --warp, --reize, --fit, and
      --pixelaspect) now accept optional modifier `:highlightcomp=1` to enable
      "highlight compensation" to avoid ringing artifacts in HDR images with
      very high-contrast regions. #3239 (2.4.0)
    - `--pattern checker` behavior has changed slightly: if the optional
      modifier `:width=` is specified but `:height=` is not, the height will
      be equal to the width. #3255 (2.4.0)
    - `--pixelaspect` fixes setting of the "PixelAspectRatio", "XResolution",
      and "YResolution" metadata, they were not set properly before. #3340
      (2.4.0.3)
    - Fix bug that prevented metadata from being able to print as XML. #3499
      (2.4.2.2)
    - `i:ch=...` fixes crashes, and also improves the warning message in cases
      where the requested channels don't exist in the source image. #3513
      (2.4.2.2)
* Python bindings:
    - Subtle/asymptomatic bugs fixed in `ImageBufAlgo.color_range_check()` and
      `histogram()` due to incorrect release of the GIL. #3074 (2.4.0)
    - Bug fix for `ImageBufAlgo.clamp()`: when just a float was passed for the
      min or max, it only clamped the first channel instead of all channels.
      #3265 (2.3.12/2.4.0)
    - Fix the ability to `getattribute()` of int64 and uint64 metadata or
      attributes. #3555 (2.4.4.0)
* idiff:
    - `--allowfailures` allows that number of failed pixels of any magnitude.
      #3455 (2.4.2)
    - `--failrelative` and `--warnrelative` allows the failure and warning
      threshold to use a symmetric mean relative error (rather than the
      absolute error implied by the existing `--fail` and `--warn` arguments).
      #3508 (2.3.19.0, 2.4.2.2)
* BMP:
    - IOProxy support. #3223 (2.4.0)
    - Support for additional (not exactly fully documented) varieties used by
      some Adobe apps. #3375 (2.4.1.0/2.3.14)
    - Better detection of corrupted files with nonsensical image dimensions or
      total size. #3434 (2.4.2/2.3.17/2.2.21)
    - Protect against corrupted files that have palette indices out of bound.
      #3435 (2.4.2/2.3.17/2.2.21)
* DDS:
    - Don't set "texturetype" metadata, it should always have been only
      "textureformat". Also, add unit testing of DDS to the testsuite. #3200
      (2.4.0/2.3.10)
    - IOProxy support. #3217 (2.4.0)
    - Add support for BC4-BC7 compression methods. #3459 (2.4.2)
    - Speed up reading of uncompressed DDS images (by about 3x). #3463
    - Better handling of cube maps with MIPmap levels. #3467 (2.4.0)
    - For 2-channel DDS files, label them as Y,A if the flags indicate
      luminance and/or alpha, otherwise label them as R,G. #3530 (2.4.2.2)
    - Do not set "oiio:BitsPerSample" for cases where the dds.fmt.bpp field is
      not assumed to be valid. MS docs say it's valid only if the flags field
      indicates RGB, LUMINANCE, or YUV types. #3530 (2.4.2.2)
* FFMpeg
    - Now uses case-insensitive tests on file extensions, so does not get
      confused on Windows with all-caps filenames. #3364 (2.4.1.0/2.3.14)
    - Take care against possible double-free of allocated memory crash upon
      destruction. #3376 (2.4.1.0/2.3.14)
* GIF
    - IOProxy support. #3181 (2.4.0/2.3.10)
* HDR:
    - IOProxy support. #3218 (2.4.0)
* HEIF:
    - Handle images with unassociated alpha. #3146 (2.4.0/2.3.9)
* JPEG:
    - IOProxy support. #3182 (2.4.0/2.3.10)
    - Better handling of PixelAspectRatio. #3366 (2.4.1.0)
    - Fix multithreaded race condition in read_native_scanline. #3495
      (2.4.2.2)
    - Fix bug in XRes,YRes aspect ratio logic. #3500 (2.4.2.2)
    - When asked to output 2-channel images (which JPEG doesn't support), use
      the channel names to decide whether to drop the second channel (if it
      seems to be a luminance/alpha image) or add a third black channel (other
      cases). #3531 (2.4.2.2)
* JPEG2000:
    - Enable multithreading for decoding (if using OpenJPEG >= 2.2) and
      encoding (if using OpenJPEG >= 2.4). This speeds up JPEG-2000 I/O by
      3-4x. #2225 (2.3.11/2.4.0)
    - IOProxy support. #3226 (2.4.0)
    - Better detection and error reporting of failure to open the file.
      #3440 (2.4.2)
* OpenEXR:
    - When building against OpenEXR 3.1+ and when the global OIIO attribute
      "openexr:core" is set to nonzero, do more efficient multithreaded
      reading of OpenEXR files. #3107 (2.4.0/2.3.9.1)
    - Fix excessive memory usage when saving EXR files with many channels.
      #3176 (2.4.0/2.3.10)
    - When building against OpenEXR >= 3.1.3, our OpenEXR output now supports
      specifying the zip compression level (for example, by passing the
      "compression" metadata as "zip:4"). Also note than when using OpenEXR >=
      3.1.3, the default zip compression has been changed from 6 to 4, which
      writes compressed files significantly (tens of percent) faster, but only
      increases compressed file size by 1-2%. #3157 (2.4.0/2.3.10) Fixes in
      #3387 (2.4.1.1)
    - Fix writing deep exrs when buffer datatype doesn't match the file. #3369
      (2.4.1.0/2.3.14)
* PNG:
    - Assume sRGB color space as default when no color space attribute is
      in the file. #3321 (2.4.0.2/2.3.13)
    - Improve error detection and propagation for corrupt/broken files. #3442
      (2.4.2)
    - Improve error detection and messages when writing PNG files, for various
      kinds of errors involving metadata. #3535 (2.4.2.2)
* PPM:
    - Mark all PPM files as Rec709 color space, which they are by
      specification. #3321 (2.4.0.2/2.3.13)
* PSD:
    - IOProxy support. #3220 (2.4.0)
    - Better error messages for corrupted files and other failures. #3469
      (2.4.0)
* RAW:
    - When using libraw 0.21+, now support new color space names "DCE-P3",
      "Rec2020", and "sRGB-linear", and "ProPhoto-linear". Fix incorrect gamma
      values for "ProPhoto". #3123 #3153 (2.4.0/2.3.9.1)
* RLA:
    -  Better guards against malformed input. #3163 (2.4.0/2.3.10)
* Targa:
    - Improved error detection for read errors and corrupted files. #3120
      (2.4.0/2.3.9.1) #3162 (2.4.0/2.3.10)
    - Fixed bug when reading x-flipped images. #3162 (2.4.0/2.3.10)
    - IOProxy support. #3221 (2.4.0)
    - Better interpretation of TGA 1.0 files with alpha that is zero
      everywhere. Be more consistent with Targa attributes all being called
      "targa:foo". Add "targa:version" to reveal whether the file was TGA 1.0
      or 2.0 version of the format. #3279 (2.4.0.1/2.3.12)
    - Fix parsing of TGA 2.0 extension area when the software name was
      missing form the header. #3323 (2.4.0.2/2.3.13)
    - Fix reading of tiny 1x1 2-bpp Targa 1.0 images. #3433 (2.3.17/2.2.21)
* Socket imageio plugin has been removed entirely, it never was completed or
  did anything useful. #3527 (2.3.2.2)
* TIFF:
    - IOProxy is now supported for TIFF output. #3075 (2.4.0/2.3.8)
    - Honor zip compression quality request when writing TIFF. #3110
      (2.4.0/2.3.11)
    - Automatically switch to "bigtiff" format for really big (> 4GB) images.
      #3158 (2.4.0)
    - Support for palette images with 16 bit palette indices. #3262
      (2.4.0/2.3.12)
    - Gracefully handle missing ExtraSamples tag. #3287 (2.4.0.1/2.3.12)
    - New output configuration hint: "tiff:write_extrasamples" (default: 1),
      if set to 0, will cause the TIFF output to NOT include the required
      EXTRASAMPLES tag in the header. This is not recommended, but fixes
      a specific problem in some circumstances where PhotoShop misbehaves
      when the extrasamples tag is present. #3289 (2.4.0.1)
    - No longer write IPTC blocks to the TIFF header by default, it caused
      trouble and was sometimes corrupted. You can force it to write an IPTC
      block by using the output open configuration hint "tiff:write_iptc" set
      to nonzero. #3302 (2.4.0.1)
    - Fix read problems with TIFF files with non-zero y offset. #3419
      (2.3.17/2.4.2)
    - Fixed some longstanding issues with IPTC data in the headers. #3465
      (2.4.0)
    - Protect against crashes with certain empty string attributes. #3491
      (2.4.2.1)
* WebP:
    - Fix previous failure to properly set the "oiio:LoopCount" metadata
      when reading animated webp images. #3183 (2.4.0/2.3.10)
    - IOProxy support. #3224 (2.4.0)
* Better catching of exceptions thrown by OCIO 1.x if it encounters 2.0 config
  files. #3089 (2.4.0/2.3.9)
* Improved internal logic and error reporting of missing OCIO configs. #3092
  #3095
* Improved finding of fonts (by IBA::render_text and oiiotool --text). It now
  honors environment variable `$OPENIMAGEIO_FONTS` and global OIIO attribute
  "font_searchpath" to list directories to be searched when fonts are needed.
  #3096 (2.4.0/2.3.8)
* Fix crash that could happen with invalidly numbered UDIM files. #3116
  (2.4.0/2.3.9)
* Fix possible bad data alignment and SIMD assumptions inside TextureSystems
  internals. #3145 (2.4.0/2.3.9)
* Update internal stb_printf implementation (avoids some sanitizer alerts).
  #3160 (2.4.0/2.3.10)
* Replace the few remaining instances of `sscanf` in the codebase with Strutil
  utilities that are guaranteed to be locale-independent. #3178 (2.4.0)
* Security: New global OIIO attributes "limits:channels" (default: 1024) and
  "limits:imagesize_MB" (default: 32768, or 32 GB) are intended to reject
  input files that exceed these limits, on the assumption that they are either
  corrupt or maliciously constructed, and would, if read, lead to absurd
  allocations, crashes, or other mayhem. Apps may lower or raise these limits
  if they know that a legitimate input image exceeds these limits. Currently,
  only the TIFF reader checks these limits, but others will be modified to
  honor the limits over time. #3230 (2.3.11/2.4.0)
* Fix integer overflow warnings. #3329 (2.4.1.0)
* Improved behavior when opening a file whose format doesn't correctly match
  its extension: try common formats first, rather than alphabetically; and
  improve error messages. #3400 (2.4.2)
* The maximum number of threads you can set with option "oiio:threads"
  has been increased from 256 to 512. #3484 (2.4.2.1)
* Make access to the internal imageio_mutex not be recursive. #3489 (2.4.2.2)
* Various protections against string metadata found in file that has zero
  length. #3493 (2.4.2.2)
* Fix possible null pointer dereference in inventory_udim. #3498 (2.4.2.2)
* oiiotool, maketx, iinfo, igrep, and iv now all take a `--version` command
  line argument, which just prints the OIIO version and exits. #3534

Developer goodies / internals:
* benchmark.h:
    - Alter the declaration of DoNotOptimize() so that it doesn't have
      compilation problems on some platforms. #3444 (2.4.2/2.3.17)
* filesystem.h:
    - A new version of `searchpath_split` returns the vector of strings rather
      than needing to be passed a reference. #3154 (2.4.0/2.3.10)
    - New `write_binary_file()` utility function. #3199 (2.4.0/2.3.10)
    - `searchpath_split()` fixes to better handle empty paths. #3306 (2.4.0.1)
* fmath.h:
    - Added `round_down_to_multiple()`. Also, more correctly handle
      `round_to_multiple()` results when the value is < 0. #3104
    - Add `round_down_to_multiple()` and improve `round_to_multiple()` to
      correctly handle cases where the value is less than 0. #3104
      (2.4.0/2.3.8)
    - Make bit_cast specialization take refs, like the template. This fixes
      warnings for some compilers. #3213 (2.4.0/2.3.10.1)
* imageio.h:
    - ImageInput and ImageOutput have many new helper methods (protected,
      meant for II and IO subclass implementations to use, not users of these
      classes) for helping to implement IOProxy support in format readers and
      writers. #3203 #3222 (2.4.0)
* Imath.h:
    - In addition to including the right Imath headers for the version that
      OIIO is built with, this defines custom formatters for the Imath types
      for use with fmt::format/print or Strutil::format/print. #3367 (2.4.1.0)
* oiioversion.h
    - `OIIO_MAKE_VERSION_STRING` and `OIIO_VERSION_STRING` now print all 4
      version parts. #3368 (2.4.1.0)
* parallel.h
    - Refactoring of the entry points (back compatible for API), and add
      support for using TBB for the thread pool (which seems slightly faster
      than our internal thread pool). By default it still uses the internal
      pool, but if OIIO::attribute("use_tbb") is set to nonzero, it will use
      the TBB thread pool if built against TBB. #3473 (2.4.2.2) #3566
      (2.4.4.0)
* paramlist.h
    - Various internal fixes that reduce the amount of ustring construction
      that happens when constructing ParamValue and ParamList, and making
      certain ImageSpec::attribute() calls. #3342 (2.4.1.0)
* simd.h:
    - Better guards to make it safe to include from Cuda. #3291 #3292
      (2.4.0.1/2.3.12)
    - Fix compiler warnings related to discrepancies between template
      declaration and redeclaration in simd.h. #3350 (2.4.1.0/2.3.14)
    - The vector types all now have a `size()` method giving its length.
      #3367 (2.4.1.0)
    - Defines custom formatters for the vector and matrix types, for use
      with fmt::format/print or Strutil::format/print. #3367 (2.4.1.0)
* string_view.h
    - Auto-conversion between our string_view, std::string_view (when
      available), and fmt::string_view. #3337 (2.4.1.0)
    - OIIO::string_view is now fully templated, to match std::string_view.
      #3344 (2.4.1.0)
* strutil.h:
    - New utility functions: parse_values(), scan_values(), scan_datetime()
      #3173 #3177 (2.4.0/2.3.10), edit_distance() #3229 (2.4.0/2.3.11)
    - The `utf8_to_utf16()` and `ut16_to_utf8()` utilities are now exposed on
      all platforms, not just windows (and their internals have been
      modernized). #3307 (2.4.0.1)
    - `Strutil::isspace()` is a safe alternative to C isspace(), that works
      even for signed characters. #3310 (2.4.1.0)
    - `Strutil::print()` now is buffered (and much more efficient, and
      directly wraps fmt::print). A new `Strutil::sync::print()` is a version
      that does a flush after every call. #3348 (2.4.1.0)
    - `get_rest_arguments()` fixes conflict between RESTful and Windows long
      path notations. #3372 (2.4.1.0/2.3.14)
    - Dramatically speed up (50-100x) Strutil iequals, iless, starts_with,
      istarts_with, ends_with, iends_with. #3388 (2.4.1.1)
    - New `safe_strcat` is a buffer-length-aware safe replcement for strcat.
      #3471 (2.4.0/2.3.18)
    - `Strutil::debug()` is the new OIIO::debug(), moving it from
      libOpenImageIO to libOpenImageIO_Util. #3486 (2.4.2.1)
    - New `Strutil::safe_strlen()` is a portable safe strlen replacement.
      #3501 (2.4.2.2)
* sysutil.h:
    - The `Term` class now recognizes a wider set of terminal emulators as
      supporting color output. #3185 (2.4.0)
* timer.h:
    - `Timer::add_seconds()` and `Timer::add_ticks()` allows add/subtract
      directly to a timer's elapsed value. #3070 (2.4.0/2.3.8)
    - For Linux, switch from using gettimeofday to clock_gettime, for
      potentially higher resolution. #3443 (2.4.2)
* typedesc.h:
    - Add Cuda host/device decorations to TypeDesc methods to make them GPU
      friendly. #3188 (2.4.0/2.3.10)
    - TypeDesc constructor from a string now accepts "box2f" and "box3f"
      as synonyms for "box2" and "box3", respectively. #3183 (2.4.0/2.3.10)
* type_traits.h:
    - This new header contains a variety of type traits used by other OIIO
      headers. They aren't really part of the public API, but they are sometimes
      used by public headers. #3367 (2.4.1.0)
* unittest.h:
    - Changes `OIIO_CHECK_SIMD_EQUAL_THRESH` macro to compare `<= eps`
      instead of `<`. #3333 (2.4.0.3)
* unordered_map_concurrent.h: Fix bug in `erase()` method. #3485 (2.4.2.2)
* ustring.h:
    - New static method `from_hash()` creates a ustring from the known hash
      value. #3397 (2.4.2)
    - New `ustringhash` class is just like a ustring, except that the "local"
      representation is the hash, rather than the unique string pointer. #3436
      (2.4.2)
* vecparam.h:
    - New `V3fParam`, `M33fParam`, and `M44fParam` (defined in vecparam.h) are
      used just for parameter passing in public APIs, instead of Imath::V3f,
      M33f, or M44f, in order to more fully hide Imath types from our public
      interfaces. These are only parameter-passing classes, and are not useful
      as vector or matrix classes in their own right. But they seamlessly cast
      to and from other vector- and matrix-like classes. #3330 (2.4.1.0)
* More internals conversion to use the new fmt style for string formatting,
  console output, error messages, and warnings: oiiotool internals #3240
  (2.4.0); TS/IC stats output #3374 (2.4.1.0); misc #3777 (2.4.1.0); testshade
  #3415 (2.4.2)
* Internals are working toward removing all uses of string_view::c_str(),
  since that isn't part of C++17 std::string_view. #3315 (2.4.0.1)
* New testtex options: `--minthreads` sets the minimum numer of threads that
  will be used for thread wedges, `--lowtrials` is an optional maximum number
  of trials just for the 1 or 2 thread cse. #3418 (2.4.2)
* Internals: internal classes with vertual methods now mark all their
  overridden destructors correctly as `override`. #3481 (2.4.2.1) #3488 #3511
  (2.4.2.2)

Build/test system improvements and platform ports:
* CMake build system and scripts:
    - Remove the old FindOpenImageIO.cmake module; downstream clients should
      instead use our exported configs. #3098 (2.4.0/2.3.8)
    - Fix over-use of targets when we should have been using variables. #3108
      (2.4.0/2.3.9)
    - CMake variable `-DENABLE_INSTALL_testtex=1` causes `testtex` to be
      installed as an application. #3111 (2.4.0)
    - Make `OpenImageIO_SUPPORTED_RELEASE` into a CMake cache variable so it
      can be overridden at build time. #3142 (2.4.0)
    - New build option `-DTIME_COMMANDS=ON` will print time to compile each
      module (for investigating build performance; only useful when building
      with `CMAKE_BUILD_PARALLEL_LEVEL=1`). #3194 (2.4.0/2.3.10)
    - `PROJECT_VERSION_RELEASE_TYPE` is now a cache variable that can be
      overridden at build time. #3197 (2.4.0/2.3.10)
    - Set and use the variable `PROJECT_IS_TOP_LEVEL` to know if OIIO is a
      top level project or a subproject. #3197 (2.4.0/2.3.10)
    - Restore code that finds Jasper when using statically-linked libraw.
      #3210 (2.4.0/2.3.10.1)
    - Gracefully handle failing to find git for test data download. #3212
      (2.4.0/2.3.10.1)
    - Make sure to properly use the tbb target if it exists. #3214
      (2.4.0/2.3.10.1)
    - Use a project-specific "OpenImageIO_CI" for whether we're running in CI,
      rather than the confusingly generic "CI" #3211 (2.4.0/2.3.11)
    - If CMake variable `BUILD_TESTING` is OFF, don't do any automatic
      downloading of missing test data. #3227 (2.3.11/2.4.0)
    - Fixes to FindOpenColorIO.cmake module, now it prefers an OCIO exported
      cmake config (for OCIO 2.1+) unless OPENCOLORIO_NO_CONFIG=ON is set.
      #3278 (2.4.0.1/2.3.12)
    - Fix problems with FindOpenEXR build script for Windows. #3281
      (2.4.0.1/2.3.12)
    - New CMake cache variable `DOWNSTREAM_CXX_STANDARD` specifies which C++
      standard is the minimum for downstream projects (currently 14), which
      may be less than the `CMAKE_CXX_STANDARD` that specifies which C++
      standard we are using to build OIIO itself. #3288 (2.4.0.1)
    - The exported cmake configs now use relative paths so they are
      relocatable. #3302 (2.4.0.1)
    - CMake variable `OPENIMAGEIO_CONFIG_DO_NOT_FIND_IMATH`, if set to ON,
      will make our generated config file not do a find_dependency(Imath).
      (Use with caution.) #3335 (2.4.0.3)
    - Can now do unity/jumbo builds. This isn't helpful when building with
      many cores/threads, but in thread-limited situtations (such as CI), it
      can speed up builds a lot to use `-DCMAKE_UNITY_BUILD=ON`. #3381 #3389
      #3392 #3393 #3398 #3402 (2.4.2.0)
    - Do not auto-download test images by default. To auto download test
      images, build with `-DOIIO_DOWNLOAD_MISSING_TESTDATA=ON`. #3409 (2.4.0)
    - Allow using the Makefile wrapper on arm64 systems. #3456 (2.4.2)
    - The export OpenImageIOConfig.cmake fixes `OpenImageIO_INCLUDE_DIR` to
      work correctly on Debian systems where there are multiple filesystem
      components to the path. #3487 (2.4.2.1)
    - On MacOS, do not force MACOS_RPATH on. #3523 (2.4.2.2)
    - Improvements to the generated cmake config files when building static
      libraries. #3552 #3557 (by Povilas Kanapickas) (2.4.4.0)
* Dependency version support:
    - When using C++17, use std::gcd instead of boost. #3076 (2.4.0)
    - When using C++17, use `inline constexpr` instead of certain statics.
      #3119 (2.4.0)
    - Fixes to work with the libraw 202110 snapshot. #3143 (2.4.0/2.3.9.1)
    - Fix occasional build breaks related to OpenCV headers. #3135
      (2.4.0/2.3.9)
    - The internals of `Filesystem::searchpath_split` have been reimplemented
      in such a way as to no longer need boost.tokenzer. #3154 (2.4.0/2.3.10)
    - Deal with the fact that OpenColorIO has changed its default branch
      name to "main". #3169 (2.4.0/2.3.10/2.2.20)
    - pybind11 v2.9.0 incorporated into our testing and CI. #3248
    - Fix clang10 compile warnings. #3272 (2.4.0.1/2.3.12)
    - Support for ffmpeg 5.0. #3282 (2.4.0.2/2.3.13)
    - Support for fmtlib 9.0.0. #3327 (2.4.0.2/2.3.13) #3466 (2.4.2/2.3.18)
    - `build_opencolorio.bash` helper script bumped its default build of
      OpenColorIO to 2.1.2. #3475 (2.4.2.1)
    - When building with C++17 or higher, Boost.filesystem is no longer
      needed or used. #3472 #3477 (2.4.2.1)
    - Upgrade the internal fallback implemention of PugiXML to the latest
      version. #3494 (2.4.2.2)
    - Fixes for ffmpeg 5.1 detection. #3516 (2.3.19.0/2.4.2.2)
    - Support for gcc 12.1. #3480 (2.4.2.1) #3551 (2.4.4.0)
    - Support building with clang 15.0. #3563 (2.4.4.0)
    - Try FORTIFY_SOURCE [#3575](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3575)
* Testing and Continuous integration (CI) systems:
    - Properly test against all the versions of VFX Platform 2022. #3074
      (2.4.0)
    - The helper script `build_libtiff.bash` now allows you to override the
      build type (by setting `LIBTIFF_BUILD_TYPE`) and also bumps the default
      libtiff that it downloads and builds from 4.1.0 to 4.3.0. #3161 (2.4.0)
    - New tests on MacOS 11 #3193 (2.4.0/2.3.10) and MacOS 12, remove test on
      MacOS 10.15 (GitHub Actions is imminently removing MacOS 10.15). #3528
      (2.3.19.0/2.4.2.2)
    - Add a DDS test (we never had one before). #3200 (2.4.0/2.3.10)
    - `imageinout_test` now has options to make it easy to unit test just one
      named format, as well as to preserve the temp files for inspection.
      #3201 (2.4.0/2.3.10)
    - Add an HDR test (we never had one before). #3218 (2.4.0)
    - Fix bugs in the build_opencolorio.bash script, did not correctly handle
      installation into custom directories. #3278 (2.4.0.1/2.3.12)
    - Failed build artifact storage is revised to save more cmake-related
      artifacts to help debugging. #3311 (2.4.0.1)
    - Now doing CI builds for Intel icc and icx compilers. #3355 #3363
      (2.4.1.0/2.3.13) #3407 (2.4.0)
    - Overhaul of ci.yml file to be more clear and compact by using GHA
      "strategy" feature. #3356 #3365 (2.4.1.0/2.3.13)
    - Removed CI for windows-2016 GHA instance which will soon be removed.
      #3370 (2.4.1.0)
    - Test against clang 14. #3404
    - Various guards against supply chain attacks durig CI. #3454 (2.4.2)
    - Test against pybind11 v2.10. #3478 (2.4.2.1)
    - Run SonarCloud nightly for static analysis and coverage analysis. #3505
      (2.4.2.2)
* Platform support:
    - Fix when building with Clang on big-endian architectures. #3133
      (2.4.0/2.3.9)
    - Improvements to NetBSD and OpenBSD support. #3137. (2.4.0/2.3.9)
    - Fixes for MSVS compile. #3168 (2.4.0/2.3.10)
    - Fix problems on Windows with MSVC for Debug builds, where crashes were
      occurring inside `isspace()` calls. #3310
    - Improved simd.h support for armv7 and aarch32. #3361 (2.4.1.0/2.3.14)
    - Suppress MacOS wasnings about OpenGL deprecation. #3380 (2.4.1.0/2.3.14)
    - Fix MSVS/Windows errors. #3382 (2.4.1.1)
    - Fix cross-compiling on Android failing due to `-latomic` check.
      [#3560](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3560) (by Povilas Kanapickas) (2.4.4.0)
    - Fix building on iOS. #3562 (2.4.4.0)

Notable documentation changes:
* Add an oiiotool example of putting a border around an image. #3138
  (2.4.0/2.3.9)
* Fix explanation of ImageCache "failure_retries" attribute. #3147
  (2.4.0/2.3.9)
* Improved maketx argument explanations.
* Clean up intra-document section references. #3238 (2.3.11/2.4.0)
* New explanations about input and output configuration hints. #3238
  (2.3.11/2.4.0)
* More code examples in both C++ and Python (especially for the ImageInput,
  ImageOutput, and ImageBufAlgo chapters). #3238 #3244 #3250 (2.3.11/2.4.0)
  #3263 (2.3.12/2.4.0)
* Pretty much anyplace where a parameter that represents a filename, and it is
  supporting UTF-8 encoding of Unicode filenames, the docs for that function
  explicitly say that the string is assumed to be UTF-8. #3312 (2.4.0.1)
* Fix many typos in docs. #3492 (2.4.2.2)



Release 2.3.20 (1 Oct 2022) -- compared to 2.3.19
-------------------------------------------------
* Fixes to compile with gcc 12. #3551
* Fixes to compile with clang 15. #3563
* PNG: better error handling when errors are encountered while writing. #3535

Release 2.3.19 (1 Sep 2022) -- compared to 2.3.18
---------------------------------------------------
* idiff: `--allowfailures` allows the specified number of pixels to differ by
  any amount, and still consider the images to compare successfully. #3455
* idiff: `--failrelative` and `--warnrelative` allows the failure and warning
  threshold to use a symmetric mean relative error (rather than the absolute
  error implied by the existing `--fail` and `--warn` arguments). #3508
* Build: Fixes for ffmpeg 5.1 detection. #3516
* Build: suppress incorrect warnings for gcc 12. #3524
* CI: New test on MacOS 12, remove test on MacOS 10.15 (GitHub Actions is
  imminently removing MacOS 10.15). #3528
* oiiotool, maketx, iinfo, igrep, and iv now all take a `--version` command
  line argument, which just prints the OIIO version and exits. #3534
* `OIIO::getattribute("version")` now retrieves the version string. #3534
* Developer goodies: `ArgParse::add_version(str)` tells ArgParse the version
  string, which will automatically add an option `--version`. #3534

Release 2.3.18 (1 Aug 2022) -- compared to 2.3.17
---------------------------------------------------
* Windows: Allow loading of dlls from PATH on Python 3.8+. #3470
* JPEG: Fix a race condition in read_native_scanline. #3495
* JPEG: Fix aspect ratio logic. #3500
* Bug fix: incorrect assignment of oiio_missingcolor attribute. #3497
* Bug fix: possible null pointer dereference in inventory_udim(). #3498
* Bug fix: print_info_subimage botched condition. #3499
* CI: Test against fmt 9.0.0. #3466
* CI: Test against pybind11 v2.10. #3478
* Strutil: safe_strcat() #3471 and safe_strlen() #3501
* Change build_opencolorio.bash to default to OCIO 2.1.2. #3475

Release 2.3.17 (1 Jul 2022) -- compared to 2.3.16
--------------------------------------------------
* TIFF: fix read problems with TIFF files with non-zero y offset. #3419
* Targa: Fix reading of tiny 1x1 2-bpp Targa 1.0 images. #3433 (2.3.17/2.2.21)
* BMP: better detection of corrupted files with nonsensical image dimensions
  or total size. #3434 (2.3.17/2.2.21)
* BMP: protect against corrupted files that have palette indices out of bound.
  #3435 (2.3.17/2.2.21)
* TextureSystem: Fix typo that prevented "max_tile_channels" attribute from
  being set or retrieved. (2.3.17)
* ustring.h: ustring has added a from_hash() static method #3397, and a
  ustringhash helper class #3436. (2.3.17/2.2.21)
* benchmark.h: Alter the declaration of DoNotOptimize() so that it doesn't
  have compilation problems on some platforms. #3444 (2.3.17)
* Fix crash when ioproxy is passed to an image writer that doesn't support it.
  #3453 (2.3.17)
* Fix the "Makefile" wrapper to correctly recognize arm64 ("Apple silicon").
  #3456 (2.3.17)

Release 2.3.16 (1 Jun 2022) -- compared to 2.3.15
--------------------------------------------------
* Support for Intel llvm-based compiler 2022.1.0. #3407
* Internals: custom fmt formatters for vector types. #3367
* Fix compiler breaks when using some changes in fmtlib master (not yet
  released). #3416
* UDIM textures have been sped up by 5-8%. #3417

Release 2.3.15 (1 May 2022) -- compared to 2.3.14
--------------------------------------------------
* JPEG: Better handling of PixelAspectRatio. #3366
* OpenEXR: Fix DWAA compression default level. #3387
* Perf: Huge speed-up of case-insensitive string comparisons (Strutil iequals,
  iless, starts_with, istarts_with, ends_with, iends_with), which also speeds
  up searches for attributes by name in ImageSpec and ParamValueList. #3388
* New `ImageBufAlgo::st_warp()` (and `oiiotool --st_warp`) perform warping of
  an image where a second image gives the (s,t) coordinates to look up from at
  every pixel. #3379
* Python: Add ImageSpec and ParamValueList method `get_bytes_attribute()`,
  which is like `get_string_attribute()`, but returns the string as a Python
  bytes object. In Python 3, strings are UTF-8, so this can be useful if you
  know that a string attribute might contain non-UTF8 data. #3396

Release 2.3.14 (1 Apr 2022) -- compared to 2.3.13
--------------------------------------------------
* Add support for UDIM pattern `<uvtile>` (used by Clarisse & V-Ray). #3358
* BMP: Support for additional (not exactly fully documented) varieties used by
  some Adobe apps. #3375
* Python: support uint8 array attributes in and out. This enables the proper
  Python access to "ICCProfile" metadata. #3378
* Improved precision in IBA::computePixelStats(). #3353
* ffmpeg reader not uses case-insensitive tests on file extensions. #3364
* Fix writing deep exrs when buffer datatype doesn't match the file. #3369
* Fix conflict between RESTful and Windows long path notations. #3372
* ffmpeg reader: take care against possible double-free of allocated memory
  crash upon destruction. #3376
* simd.h fixes for armv7 and aarch32. #3361
* Fix compiler warnings related to discrepancies between template declaration
  and redeclaration in simd.h and benchmark.h. #3350
* Suppress MacOS warnings about OpenGL depreation. #3380
* Now doing CI builds for Intel icc and icx compilers. #3355 #3363
* CI: Overhaul of yml file to be more clear and compact by using GHA
  "strategy" feature. #3356 #3365

Release 2.3.13 (1 Mar 2022) -- compared to 2.3.12
--------------------------------------------------
* Filesystm::searchpath_split better handling of empty paths. #3306
* New Strutil::isspace() is an isspace replacement that is safe for char
  values that are < 0. #3310
* Expose the Strutil::utf8_to_utf16() and utf16_to_utf8() utilities on
  non-Windows platforms (and also modernize their internals). #3307
* For the most important ImageInput, ImageOutput, and ImageBuf methods that
  take a filename, add new flavors that can accept a `wstring` as the
  filename. #3312 #3318
* PPM: properly report color space as Rec709 (as dictated by PPM spec). #3321
* PNG: more robust reporting of color space as sRGB in the absence of header
  fields contradicting this. #3321
* strutil.h: Split the including of fmt.h and some related logic into a
  separate detail/fmt.h. This is still included by strutil.h, so users
  should not notice any change. #3327
* Targa: Fix parsing of TGA 2.0 extension area. #3323
* Support building against FFmpeg 5.0. #3282
* oiiotool --pixelaspect : fix setting of "PixelAspectRatio", "XResolution",
  and "YResolution" attributes in the output file (were not set properly
  before). #3340

Release 2.3.12 (1 Feb 2022) -- compared to 2.3.11
--------------------------------------------------
* oiiotool: Don't give spurious warnings about no output when the --colorcount
  or --rangecheck commands are used. #3262
* `oiiotool --pattern checker` fixed behavior so that if only the checker
  width was specified but not the height, height will be equal to width. #3255
* `oiiotool --point` lets you set individual pixels. #3256
* Python: A new ImageBuf constructor was added that takes only a NumPy
  ndarray, and deduces the resolution, channels, and format (pixel data type)
  from the shape of the array. #3246
* Python: Implement `ROI.copy()`. #3253
* Python: ImageSpec and ParamValueList now support `'key' in spec`,
  `del spec['key']`, and `spec.get('key', defaultval)` to more fully emulate
  Python `dict` syntax for manipulating metadata. #3252 (2.3.12/2.4.0)
* Python bug fix: fix `clamp()` when the min or max are just a float. Now
  it uses that one value for all channels, instead of using it only for
  the first channel. #3265
* ImageSpec gained an additional constructor that takes a string
  representation of the pixel data type (where it used to insist on a
  TypeDesc), such as `ImageSpec(640, 480, 3, "uint8")`. This is especially
  helpful for new/casual users, or when you want code to be maximally
  readable. #3245
* `Imagepec::getattribute()` new query token `"format"` can retrieve the pixel
  data type. #3247
* `IBA::make_texture()`: ensure that "maketx:ignore_unassoc" is honored. #3269
* Support an additional UDIM pattern `<UVTILE>`, which is specified by
  MaterialX. #3280
* TIFF: support 16-bit palette images. #3260
* TIFF: Gracefully handle missing ExtraSamples tag. #3287
* Targa: Better interpretation of TGA 1.0 files with alpha that is zero
  everywhere. Be more consistent with Targa attributes all being called
  "targa:foo". Add "targa:version" to reveal whether the file was TGA 1.0
  or 2.0 version of the format. #3279
* simd.h: Better guards to make it safe to include from Cuda. #3291 #3292
* Fix bugs in the build_opencolorio.bash script, did not correctly handle
  installation into custom directories. #3278
* Fixes to FindOpenColorIO.cmake module, now it prefers an OCIO exported cmake
  config (for OCIO 2.1+) unless OPENCOLORIO_NO_CONFIG=ON is set. #3278
* Docs: The ImageBufAlgo chapter now has examples for C++, Python, and
  oiiotool for almost every operation. #3263

Release 2.3.11 (1 Jan 2022) -- compared to 2.3.10
--------------------------------------------------
* JPEG2000: enable multithreading for decoding (if using OpenJPEG >= 2.2)
  and encoding (if using OpenJPEG >= 2.4). This speeds up JPEG-2000 I/O
  by 3-4x. #2225
* TIFF: automatically switch to "bigtiff" format for >4GB images. #3158
* Security: New global OIIO attributes "limits:channels" (default: 1024) and
  "limits:imagesize_MB" (default: 32768, or 32 GB) are intended to reject
  input files that exceed these limits, on the assumption that they are either
  corrupt or maliciously constructed, and would, if read, lead to absurd
  allocations, crashes, or other mayhem. Apps may lower or raise these limits
  if they know that a legitimate input image exceeds these limits. Currently,
  only the TIFF reader checks these limits, but others will be modified to
  honor the limits over time. #3230
* TextureSystem: enhance safety/correctness for untiled images that exceed
  2GB size (there was an integer overflow problem in computing offsets within
  tiles). #3232
* Cleanup: Get rid of an obsolete header c-imageio.h that was experimental
  and the functions declared therein were not implemented in this release.
  #3237
* Build: rely on env variable "OpenImageIO_CI" to tell us if we are running in
  a CI environment, not the more generic "CI" which could be set for other
  reasons in some environments. #3211
* Build: Improvements in how we find OpenVDB. #3216
* Build: If CMake variable `BUILD_TESTING` is OFF, don't do any automatic
  downloading of missing test data. #3227
* Dev: Add Strutil::edit_distance(). #3229
* Docs: Clean up intra-document section references, new explanations about
  input and output configuration hints, more code examples in both C++ and
  Python (especially for the ImageInput and ImageOutput chapters). #3238 #3244

Release 2.3.10.1 (7 Dec 2021) -- compared to 2.3.10.0
-----------------------------------------------------
* Build: restore code that finds Jasper when using statically-linked libraw.
  #3210
* Build/test: Gracefully handle failing to find git for test data download.
  #3212
* fmath.h: bit_cast specialization should take refs, like the template.
  This made warnings for some compilers. #3213
* Build: make sure to properly use the tbb target if it exists. #3214

Release 2.3.10 (1 Dec 2021) -- compared to 2.3.9
--------------------------------------------------
New (non-compatibility-breaking) features:
* TextureSystem: add feature for stochastic mipmap interpolation. This adds
  new interpolation modes "StochasticTrilinear" and "StochasticAniso", which
  in conjunction with the "rnd" field in TextureOpt, will stochastically
  choose between bracketing MIPmap levels rather than interpolating them. This
  reduces texture lookup cost by up to 40%, but it's only useful in the
  context of a renderer that uses many samples per pixel. #3127
* maketx/make_texture() now supports options to store Gaussian forward and
  inverse transform lookup tables in image metadata (must be OpenEXR textures
  for this to work) to aid writing shaders that use histogram-preserving
  blending of texture tiling. This is controlled by new maketx arguments
  `--cdf`, `--cdfsigma`, `--sdfbits`, or for `IBA::make_texture()` by using
  hints `maketx:cdf`, `maketx:cdfsigma`, and `maketx:cdfbits`. #3159 #3206
* `oiitool --oiioattrib` can set "global" OIIO control attributes for
  an oiiotool run (equivalent of calling `OIIO::attribute()`). #3171
* `oiiotool --repremult` exposes the previously existing `IBA::repremult()`.
  The guidance here is that `--premult` should be used for one-time conversion
  of "unassociated alpha/unpremultiplied color" to associated/premultiplied,
  but when you are starting with a premultiplied image and have a sequence of
  unpremultiply, doing some adjustment in unpremultiplied space, then
  re-premultiplying, it's `--repremult` you want as the last step, because it
  preserves alpha = 0, color > 0 data without crushing it to black. #3192
* `oiiotool --saturate` and `IBA::saturate()` can adjust saturation level of a
  color image. #3190
* `oiiotool --maxchan` and `--minchan`, and `IBA::maxchan()` and `minchan()`
  turn an N-channel image into a 1-channel images that for each pixel,
  contains the maximum value in any channel of the original for that pixel.
  #3198
* When building against OpenEXR >= 3.1.3, our OpenEXR output now supports
  specifying the zip compression level (for example, by passing the
  "compression" metadata as "zip:4"). Also note than when using OpenEXR >=
  3.1.3, the default zip compression has been changed from 6 to 4, which
  writes compressed files significantly (tens of percent) faster, but only
  increases compressed file size by 1-2%. #3157
* Improved image dithering facilities: When dithering is chosen, it now
  happens any time you reduce >8 bits to <= 8 bits (not just when converting
  from float or half); change the dither pattern from hashed to blue noise,
  which looks MUCH better (beware slightly changed appearance); `IBA::noise()`
  and `oiiotool --noise` now take "blue" as a noise name, giving a blue noise
  pattern; `IBA::bluenoise_image()` returns a reference to a stored periodic
  blue noise image; `oiiotool -d` now lets you ask for "uint6", "uint4",
  "uint2", and "uint1" bit depths, for formats that support them. #3141
* New global OIIO attribute `"try_all_readers"` can be set to 0 if you want to
  override the default behavior and specifically NOT try any format readers
  that don't match the file extension of an input image (usually, it will try
  that one first, but it if fails to open the file, all known file readers
  will be tried in case the file is just misnamed, but sometimes you don't
  want it to do that). #3172
* Raise the default ImageCache default tile cache from 256MB to 1GB. This
  should improve performance for some operations involving large images or
  images with many channels. #3180
* IOProxy support has been added to JPEG output (already was supported for
  JPEG input) and for GIF input and output. #3181 #3182

Bug fixes:
* Fix `oiiotool --invert` to avoid losing the alpha channel values. #3191
* Fix excessive memory usage when saving EXR files with many channels. #3176
* WebP: Fix previous failure to properly set the "oiio:LoopCount" metadata
  for animated webp images. #3183
* Targa: Better detection/safety when reading corrupted files, and fixed
  bug when reading x-flipped images. #3162
* RLA: better guards against malformed input. #3163
* Fix oiiotool bug when autocropping output images when the entire pixel data
  window is in the negative coordinate region. #3164
* oiiotool improved detection of file reading failures. #3165
* DDS: Don't set "texturetype" metadata, it should always have been only
  "textureformat". Also, add unit testing of DDS to the testsuite. #3200
* When textures are created with the "monochrome-detect" feature enabled,
  which turns RGB textures where all channels are always equal into true
  single channel greyscale, the single channel that results is now correctly
  named "Y" instead of leaving it as "R" (it's not red, it's luminance).
  #3205

Build fixes and developer goodies:
* Update internal stb_printf implementation (avoids some sanitizer alerts).
  #3160
* Fixes for MSVS compile. #3168
* Add Cuda host/device decorations to TypeDesc methods to make them GPU
  friendly. #3188
* TypeDesc: The constructor from a string now accepts "box2f" and "box3f"
  as synonyms for "box2" and "box3", respectively. #3183
* New Strutil::parse_values, scan_values, scan_datetime, parse_line. #3173
  #3177
* New `Filesystem::write_binary_file()` utility function. #3199
* New build option `-DTIME_COMMANDS=ON` will print time to compile each module
  (for investigating build performance; only useful when building with
  `CMAKE_BUILD_PARALLEL_LEVEL=1`). #3194

Release 2.3.9.1 (1 Nov 2021) -- compared to 2.3.8
--------------------------------------------------
* OpenEXR: When building against OpenEXR 3.1+ and when the global OIIO
  attribute "openexr:core" is set to nonzero, do more efficient multithreaded
  reading of OpenEXR files. #3107
* `oiiotool --dumpdata:C=name` causes the dumped image data to be formatted
  with the syntax of a C array. #3136
* oiiotool: Allow quotes in command modifiers. #3112
* jpeg input: remove stray debugging output to console. #3134
* HEIF: Handle images with unassociated alpha. #3146
* Targa: improved error detection for read errors and corrupted files. #3120
* raw: When using libraw 0.21+, now support new color space names "DCE-P3",
  "Rec2020", and "sRGB-linear", and "ProPhoto-linear". Fix incorrect gamma
  values for "ProPhoto". #3123 #3153 Fixes to work with the libraw 202110
  snapshot. #3143
* TIFF: honor zip compression quality request when writing TIFF. #3110
* Fix broken oiiotool --dumpdata output. #3131
* Fix possible bad data alignment and SIMD assumptions inside TextureSystems
  internals. #3145
* Field3D, which is no longer actively supported, now has support disabled in
  the default build. To enable Field3D support, you must build with
  `-DENABLE_FIELD3D=1`. Note that we expect it to be no longer supported at
  all, beginning with OIIO 2.4. #3140
* Build: Address new warnings revealed by clang 13. #3122
* Build: Fix when building with Clang on big-endian architectures. #3133
* Build: Fix occasional build breaks related to OpenCV headers. #3135
* Build: Improvements to NetBSD and OpenBSD support. #3137.
* docs: Add an oiiotool example of putting a border around an image. #3138
* docs: Fix explanation of ImageCache "falure_retries" attribute. #3147

Release 2.3.8 (1 Oct 2021) -- compared to 2.3.7
--------------------------------------------------
* Fix ImageBuf::read() bug for images of mixed per-channel data types. #3088
* Fix crash that could happen with invalidly numbered UDIM files. #3116
* Better catching of exceptions and other error handling when OpenColorIO 1.x
  is used but encounters an OpenColorIO 2.x config file. #3089 #3092 #3095
* Ensure that OpenColorIO doesn't send info messages to the console if no
  config is found. #3113
* Fix: make sure ImageSpec deserialization works for arbitrary attribs. #3066
* `oiiotool -ch` now has greatly reduced cost (no useless allocations or
  copies) when the channel order and names don't change. #3068
* `oiiotool --runstats` is now much better about correctly attributing I/O
  time to `-i` instead of to the subsequent operations that triggers the
  read. #3073
* TIFF output now supports IO proxies. #3075 #3077
* Improved finding of fonts (by IBA::render_text and oiiotool --text). It now
  honors environment variable `$OPENIMAGEIO_FONTS` and global OIIO attribute
  "font_searchpath" to list directories to be searched when fonts are needed.
  #3096
* We no longer install a FindOpenImageIO.cmake module. It was incomplete, out
  of date, and wholly unnecessary now that we correctly export a config file
  OpenImageIOConfig.cmake and friends. #3098
* When building against OpenEXR 3.1+, use of the OpenEXRCore library no longer
  requires a build-time option, but instead is always available (though off by
  default) and can be enabled by an application setting the OIIO global
  attribute "openexr:core" to 1. #3100
* dev: Timer::add_seconds and add_ticks methods. #3070
* dev: Add `round_down_to_multiple()` and improve `round_to_multiple()` to
  correctly handle cases where the value is less than 0. #3104

Release 2.3 (1 Sept 2021) -- compared to 2.2
----------------------------------------------
New minimum dependencies and compatibility changes:
* C++ standard: **C++14 is now the minimum (gcc 6.1 - 11.2, clang 3.4 - 12,
  MSVS 2017 - 2019, icc 17+).** The default C++ standard mode, if none is
  explicitly specified, is now C++14. #2918 (2.3.4) #2955 #2977 (2.3.5)
* FFMmpeg (optional) dependency minimum is now 3.0 (raised from 2.6). #2994
* OpenCV (optional) dependency minimum is now 3.0 (raised from 2.0). #3015

New major features and public API changes:
* Changes and clarifications to `geterror()` in all classes that have one:
    - `geterror()` now takes an optional `clear` parameter controls if the
      pending error is cleared as well as retrieved (default `true`).
    - All classes with `geterror()` are ensured to have `has_error()`.
    - `geterror()` appends to the pending error message, if one is already
      present but not retrieved. Appending errors will be automatically
      separated by newlines, also any newline at the end of a geterror
      call will be stripped. #2740 (2.3.0.1)
* ImageInput error messages are now thread-specific. This prevents multiple
  threads simultaneously reading from the same ImageInput from getting the
  other thread's error messages. But it means you must always retrieve an
  error with `getmessage()` from the same thread that made the read call
  that generated the error. #2752 (2.3.0.1)
* ImageInput and ImageOutput have removed several fields (like the m_mutex)
  and instead now use a PIMPL idiom that hides internals better from the
  ABI. If you had a custom ImageInput/Output class that directly accessed
  the m_mutex, replace it instead with calls to ImageInput::lock() and
  unlock() (and ImageOutput). #2752 (2.3.1.0)
* Clarify that ImageBuf methods `subimage()`, `nsubimages()`, `miplevel()`,
  `nmipevels()`, and `file_format_name()` refer to the file that an ImageBuf
  was read from, and are thus only meaningful for ImageBuf's that directly
  read from files. "Computed" or copied ImageBuf's are solo images in memory
  that no longer correspond to a file even if the input to the computation
  was from a file. #2759 (2.3.0.1/2.2.9)
* New `get_extension_map()` returns a map that list all image file formats
  and their presumed file extensions. #2904 (2.3.4)
* Overhaul of how we handle "thumbnails" (small preview images embedded in
  the headers of some image file formats) #3021 (2.3.7):
    - Thumbnails no longer embedded in the ImageSpec as "thumbnail_image"
      metadata of a big character array blob.
    - ImageInput, ImageBuf, and ImageCache have varieties of `get_thumbnail()`
      method that returns the thumbnail as an ImageBuf. If this is not
      called, ideally the reader will not undergo the expense of reading and
      storing thumbnail data.
    - ImageOutput and ImageBuf have `set_thumbnail()` to communicate a
      thumbnail image as an ImageBuf.
    - The net effect is that reading files and retrieving ImageSpec's should
      be more efficient for apps that don't have a use for thumbnails.
* Significant improvements to UDIM handling in ImageCache/TextureSystem:
    - New UDIM texture name patterns recognized: `%(UDIM)d` is the Houdini
      convention, and `_u##v##` is for Animal Logic's internal renderer.
      #3006 (2.2.16/2.3.6)
    - It is now permissible for `get_texture_info()/get_image_info()` to
      retrieve metadata for UDIM patterns -- previously it failed, but now
      it will succeed in retrieving any metadata as long as it is present
      and has the same value in all the matching "UDIM tile" panels. #3049
      (2.3.7)
    - TextureSystem now exposes methods `is_udim()`, `resolve_udim()`,
      `inventory_udim()` that should be helpful for apps dealing with UDIM
      textures. See the documentation for details. #3054 (2.3.7)
    - Performance improvements when using UDIM textures. #3049 (2.3.7)
* ImageBuf, when "wrapping" an app-owned buffer, now allows explicit
  specification of stride lengths. This allows you to wrap a buffer that
  has internal padding or other non-contiguous spacing of pixels, scanlines,
  or image planes. #3022 (2.3.6)
* oiiotool new commands and options:
    - `--pastemeta` takes two images as arguments, and appends all the
      metadata (only) from the first image onto the second image's pixels
      and metadata, producing a combined image. #2708 (2.3.0.0)
    - `--chappend` and `--siappend` both allow an optional modifier `:n=`
      to specify the number of images from the stack to be combined
      (default n=2). #2709 (2.3.0.0)
    - `--fit` now takes an additional optional modifier: `fillmode=` with
      choices of `letterbox` (default), `width`, and `height`. #2784 (2.3.2.0)
    - `--autocc` now has an optional modifier `:unpremult=1` that causes
      any color transformations related to autocc to unpremultiply by alpha
      before the transformation and then re-premultiply afterwards, exactly
      the same control that exists for individual `--colorconvert` but never
      existed for autocc. #2890 (2.3.3)
    - `--list-formats` lists all the image file formats that OIIO knows
      about, and for each, the associated file extensions. #2904 (2.3.4)
    - `--skip-bad-frames` causes an error (such as an input file not being
      found) to simply skip to the next frame in the frame range loop, rather
      that immediately exiting. #2905 (2.3.4)
    - When doing expression substitution, `{getattribute(name)}` will be
      replaced by the value that `OIIO::getattribute(name, ...)` would
      retrieve.  #2932 (2.3.4)
    - `--missingfile` (which takes a subsequent argument of `error`, `black`,
      or `checker`) determines the behavior when an input image file is
      missing (the file does not exist at all, does not include cases where
      the file exists but there are read errors). The default value of
      `error` matches the old behavior: report an error and terminate all
      command processing. Other choices of `black` or `checker` will continue
      processing but substitute either a black or checkerboard image for the
      missing file. #2949 (2.3.4)
    - `--printinfo` prints verbose metadata info about the current top
      image on the stack. #3056 (2.3.7)
    - `--printstats` prints statistics about the current top image on the
      stack, and optionally can limit the statistics to a rectangular
      subregion of the image. #3056 (2.3.7)
    - Expanded expression now support the following new metadata notations:
      `META`, `METABRIEF`, and `STATS`. #3025 (2.3.6)
    - `--mosaic` now takes optional `fit=WxH` modifier that lets you set the
      "cell" size, with all constituent images resized as needed (similarly
      to if you used `--fit` on each individually). This is helpful for
      creating contact sheets without having to resize separately or to know
      the resolution of the original images. #3026 (2.3.6)
    - REMOVED `--histogram` which had been deprecated and undocumented since
      before OIIO 2.0. #3029 (2.3.6)
* Python bindings:
    - When transferring blocks of pixels (e.g., `ImageInput.read_image()`
      or `ImageOutput.write_scanline()`), "half" pixels ended up mangled
      into uint16, but now they use the correct `numpy.float16` type. #2694
      (2.3.0.0)
    - The value passed to `attribute(name, typedesc, value)` can now be a
      tuple, list, numpy array, or scalar value. #2695 (2.3.0.0)
    - Added Python bindings for the TextureSystem. #2842
    - Add the previously (inadvertently) omitted enum value for
      `ImageBufAlgo.MakeTxBumpWithSlopes`. #2951 (2.3.4)
* Environment variables:
    - `CUE_THREADS` env variable, if set, is now honored to set the default
      size of the OIIO thread pool. This helps to make OIIO-based apps be
      good citizens when run as OpenCue jobs. #3038 (2.3.7)
* Library organization:
    - All the utility classes are now in libOpenImageIO_Util *only* and
      libOpenImageIO depends on and links to libOpenImageIO_Util, rather
      than the utility classes being defined separately in both libraries.
      #2906 (2.3.4)
    - Include file change: `imagebuf.h` no longer includes `imagecache.h`.
      It should never have needed to, since it didn't need any of its
      declarations. Other code that should always have included both headers,
      but inadvertently only included `imagebuf.h`, may now need to add an
      explicit `#include <OpenImageIO/imagecache.h>`. #3036 (2.3.7)
* Important change to `Makefile` wrapper: We have a 'Makefile' that just wraps
  cmake for ease of use. This has changed its practice of putting builds in
  `build/ARCH` to just `build` (and local installs from `dist/ARCH` to `dist`)
  to better match common cmake practice. So where your local builds end up may
  shift a bit. #3057 (2.3.7)
* Deprecations: A number of long-deprecated functions and methods have been
  given deprecation attributes that will cause warnings if you use them. They
  will eventually be removed entirely at the next "breaking" release (3.0,
  whenever that is). #3032 (2.3.7)

Performance improvements:
* Speed up BMP reading by eliminating wasteful allocation and copying
  done on each scanline read. #2934 (2.3.4)
* For `IBA::to_OpenCV()`, improve efficiency for certain image copies. #2944
  (2.3.4) 
* Fix runaway parsing time for pathological XMP metadata. #2968 (2.3.5/2.2.15)
* TextureSystem reduction in overhead when using UDIM textures. #3049 (2.3.7)

Fixes and feature enhancements:
* Fix a situation where if an ImageBuf backed by an ImageCache reads an
  image, then the image changes on disk, then another ImageBuf or ImageCache
  tries to read it, it could end up with the old version. This involved some
  strategic cache invalidation when ImageBuf's write images to disk. #2696
  (2.3.0.0)
* Improve parsing of XMP records in metadata: more correct handling of
  lists/sequences, better inference of types that look like int or float
  (rather than forcing unknown fields into strings), fixed bugs in parsing
  rational values. #2865 (2.2.12/2.3.3)
* ImageBuf/ImageBufAlgo:
    - `IBA::contrast_remap()` fixes bug that could crash for very large
      images #2704 (2.3.0.0)
    - Fix stack overflow crash in IBA::colorconvert of unusually wide images.
      #2716 (2.3.0.1/2.2.8)
    - Fix `IBA::make_texture()` incorrectly setting tile sizes. #2737
      (2.3.0.1/2.2.8)
    - `IBA::make_texture()` now correctly propagates error messages, which
      can be retrieved via the global OIIO::geterror() call. #2747 (2.3.0.1)
    - `IBA::fit()` now takes a `fillmode` parameter that controls exactly
      how the resize will occur in cases where the aspect ratio of the new
      frame doesn't exactly match that of the source image. The old way (and
      the default value now) is "letterbox", but now also "width" and
      "height" modes are allowed. See `fit()` documentation for details.
      #2784 (2.3.2.0)
    - `IBA::fit()` fixes some slight ringing at image edges when using a
      combination of black wrap mode and a filter with negative lobes.
      #2787 (2.3.2.0)
    - Fix: `ociolook()` and `ociofiletransform()` internally reversed the
      order of their `inverse` and `unpremult` arguments, making it hard to
      select the inverse transformation. #2844 (2.3.3/2.2.11)
    - Fix crash for certain calls to `ImageBuf::set_write_format()` when
      writing files that support per-channel data types. #2885 (2.3.3)
    - Fix: `IBA::fillholes_pushpull` did not correctly understand which
      channel was alpha when generating subimages. #2939 (2.3.4)
    - Fix: `IBA::render_text` did not properly account for non-1 alpha of
      the text color. #2981 (2.3.5/2.2.15)
    - `IBA::colorconvert`, `colormatrixtransform`, `ociolook`, `ociodisplay`,
      and `ociofiletransform` have been fixed so that if input is more than
      4 channels, the additional channels will be copied unchanged, rather
      than inadvertently set to 0. #2987 (2.3.5/2.2.16)
    - Thread safety has been improved for `ImageBuf::init_spec()` and `read()`
      (though no problem was ever reported in the wild). #3018 (2.3.6)
    - `render_text()` now accepts text strings with embedded linefeeds and
      will turn them into multiple lines of rendered text. #3024 (2.3.6)
* ImageCache/TextureSystem/maketx:
    - Fix ImageCache bug: add_tile/get_tile not properly honoring when
     `chend < chbegin` it should get all channels. #2742 (2.3.0.1/2.2.8)
    - `ImageBufAlgo::make_texture()` (as well as `maketx` and `oiiotool -otex`)
      clamp `half` values to their maximum finite range to prevent very large
      float inputs turning into `Inf` when saved as half values. #2891 (2.3.3)
    - Fix crash when sampling non-zero-base channels. #2962 (2.3.5/2.2.15)
    - `IBA::make_texture()` rejects "deep" input images, since they cannot
      be made into textures. Doing this could previously crash. #2991
      (2.3.5/2.2.16)
    - maketx and `oiiotool -otex` have fixed double printing of error
      messages #2992 (2.3.5).
    - Making bumpslopes textures now allows scaled slopes UV normalization.
      This is exposed as new maketx argument `--uvsopes_scale`, or passing
      the attribute `uvslopes_scale` (int) to make_texture(). #3012 (2.3.6)
    - There is a new variety of ImageCache::invalidate() that takes an
      `ImageHandle*` argument instead of a filename. #3035 (2.3.7)
* oiiotool:
    - `--resize` of images with multi-subimages could crash. #2711 (2.3.0.0)
    - Improve oiiotool's guessing about the desired output format based on
      inputs (in the absence of `-d` to specify the format). #2717
      (2.3.0.1/2.2.8)
    - `--text` now accepts text strings with embedded linefeeds and
      will turn them into multiple lines of rendered text. #3024 (2.3.6)
* BMP
    - Fix reading BMP images with bottom-to-top row order. #2776
      (2.3.1.1/2.2.9)
    - Correctly read BMP images of the older V1 variety without crashing.
      #2899 (2.3.3/2.2.13)
    - Fix error in the rightmost pixel column when reading 4 bpp images with
      odd horizontal resolution. #2899 (2.3.3/2.2.13)
    - BMP reads now return metadata "bmp:bitsperpixel" (only when not 24
      or 32) to indicate non-whole-byte channels sizes in the file, and
      "bmp:version" to indicate the version of the BMP format. #2899
      (2.3.3/2.2.13)
    - Speed up BMP reading by eliminating wasteful allocation and copying
      done on each scanline read. #2934 (2.3.4)
    - For BMP files that are 8 bits per pixel and all palette entries have
      R == G == B values, read the file as an 8-bit grayscale image instead
      of needlessly promoting to a full RGB 3-channel image. #2943 (2.3.4)
    - Full support for reading RLE-compressed BMP images. #2976 (2.3.5/2.2.15)
    - Write single channel BMP as 8 bit palette images. #2976 (2.3.5/2.2.15)
* DPX
    - Output to DPX files now supports IOProxy. (Input already did.) #3013
      (2.2.17/2.3.6)
* FFMpeg/movies:
    - Avoid potential crash when a frame can't be read. #2693 (2.3.0.0)
    - Several varieties of encoded videos with 12 bits per channel and
      having alpha were incorrectly reported as 8 bits and without alpha.
      #2989 (2.3.5/2.2.16)
* GIF
    - Support UTF-8 filenames on Windows for GIF files. #2777 (2.3.2.0)
    - Fix error checking for non-existent GIF files. #2886 (2.3.3)
* HEIC
    - Fix error decoding Apple HEIF files. #2794/#2809 (2.3.2.0)
    - Better valid_file() check for HEIF. #2810 (2.3.2.0)
    - Enabled AVIF decoding of heic files (requires libheif >= 1.7 and for
      it to have been built with an AV1 encoder/decoder). #2811 #2812 #2814
      #2818 (2.3.3.0)
* IFF
    - Fix broken reads of 16 bit iff files. #2736 (2.3.0.1/2.2.8)
* JPEG
    - Allow reading of JPEG files with mildly corrupted headers. #2927 (2.3.4)
* OpenEXR:
    - Fix rare crash that was possible when multithreaded writing openexr
      files. #2781 (2.3.2.0)
    - Improved error reporting when errors are encountered writing OpenEXR
      files. #2783 (2.3.2.0)
    - Fix potential crash parsing OpenEXR header that contains Rational
      attributes with certain values. #2791 (2.2.10/2.3.2)
    - EXPERIMENTAL: When building against OpenEXR 3.1, the OIIO CMake option
      `-DOIIO_USE_EXR_C_API=ON` will use a new OpenEXR API that we think
      will allow higher performance texture reads in a multithreaded app.
      This has not yet been benchmarked or tested thoroughly. #3009 #3027
      (2.3.6)
    - The reader no longer renames file metadata "version" into the special
      "openexr:version" that would indicate the OpenEXR file format version
      of the file. That's never what it meant! It really was just arbitrary
      metadata. #3044 (2.3.7)
* PNG
    - Read Exif data from PNG files. #2767 (2.3.1.1/2.2.9)
* PSD
    - Add support for reading ISD profiles for PSD input. #2788 (2.3.2.0)
    - Fix loading PSB files with cinf tags. #2877 (2.2.12/2.3.2)
* RAW:
    - Additional input configuration hints to control options in the
      underlying LibRaw: `"raw:user_flip"` #2769 (2.3.1.0)
      `"raw:balance_clamped"`, `"raw:apply_scene_linear_scale"`,
      `"raw:camera_to_scene_linear_scale"` #3045 (2.3.7)
    - Correctly handle RAW files with Unicode filenames on Windows. #2888
      (2.3.3)
    - Fix garbled output when `raw:Demosaic` hint is `"none"`. #3045 (2.3.7)
* Targa
    - Fix potential crash when reading files with no thumbnail. #2903
      (2.3.3/2.2.13)
    - Fix alpha handling for some files. #3019 (2.3.6)
* TIFF:
    - Fix broken reads of multi-subimage non-spectral files (such as
      photometric YCbCr mode). #2692 (2.3.0.0)
    - Fix spec() and spec_dimensions() for MIPmapped TIFF files, they did
      not recognize being asked to return the specs for invalid subimage
      indices. #2723 (2.3.0.1/2.2.7)
    - Add ability to output 1bpp TIFF files. #2722 (2.3.0.1/2.2.7)
    - Fix reading TIFF files with "separate" planarconfig and rowsperstrip
      more than 1. #2757 (2.3.0.1/2.2.9)
    - Fix incorrect reading of tiled TIFF files using certain rare TIFF
      features that also have a vertical resolution that is not a whole
      multiple of the tile size. #2895 (2.3.3/2.2.13)
    - Support IOProxy for reading TIFF files. #2921 (2.3.4)
    - TIFF plugin now properly honors caller request for single-threaded
      operation. #3016 (2.3.6)
* WebP:
    - Add support for requesting compression "lossless". #2726 (2.3.0.1/2.2.8)
    - Input improvements including: RGB images are kept as RGB instead of
      always adding alpha; more efficient by skipping alpha premultiplication
      when it's unnecessary; now can read animated WebP images as
      multi-subimage files. #2730 (2.3.0.1/2.2.8)
* Fix memory leak during decoding of some invalid Exif blocks. #2824
* Fix possible divide-by-zero error in read_image/read_scanlines for invalid
  image sizes (usually from corrupt files). #2983 (2.3.5/2.2.16)

Developer goodies / internals:
* Internals now use C++11 `final` keywords wherever applicable. #2734
  (2.3.0.1)
* More internals conversion of old Strutil::sprintf to Strutil::fmt::format
  and related changes. #2889 (2.3.3)
* Redundant format conversion code removed from imageio.cpp #2907 (2.3.4)
* Eliminate direct output to std::cerr and std::cout by library calls.
  #2923 (2.3.4)
* argparse.h:
    - ArgParse::abort() lets the response to a command line argument signal
      that no further arguments should be parsed. #2820 (2.3.3/2.2.11)
* color.h:
    - New `ColorConfig::OpenColorIO_version_hex()` returns the hex code for
      the version of OCIO we are using (0 for no OCIO support). #2849 (2.3.3)
* farmhash.h:
    - Clean up all non-namespaced preprocessor symbols that are set
      by this header and may pollute the caller's symbols. #3002 (2.2.16/2.3.6)
* filesystem.h:
    - New Filesystem::generic_filepath() returnss a filepath in generic
      format (not OS specific). #2819 (2.3.3/2.2.11)
    - Improve exception safety in Filesystem directory iteration. #2998
      (2.2.16/2.3.6)
    - New `filename_to_regex()` makes a filename "safe" to use as a
      regex pattern (for example, properly backslashing any `.` characters).
      #3046 (2.3.7)
    - Some methods that used to take a `const std::string&` now take a
      `string_view`. #3047 (2.3.7)
* fmath.h:
    - Use CPU intrinsics to speed up swap_ending (by 8-15x when swapping
      bytes of large arrays). #2763 (2.3.1.0)
* hash.h:
    - `farmhash::inlined::Hash` now is constexpr and works for Cuda.
      #2843 (2.3.3) #2914 #2930 (2.3.4)
* oiioversion.h:
    - New macros `OIIO_VERSION_GREATER_EQUAL` and `OIIO_VERSION_LESS`.
      #2831 (2.3.3/2.2.11)
* platform.h:
    - New macro OIIO_INLINE_CONSTEXPR, equivalent to `inline constexpr` for
      C++17, but just constexpr for C++ <= 14. #2832 (2.3.3/2.2.11)
* simd.h:
    - Fix incorrect ARM NEON code in simd.h. #2739 (2.3.0.1/2.2.8)
* span.h:
    - `std::size()` and `std::ssize()` should work with OIIO::span now.
      #2827 (2.3.3/2.2.11)
* string_view.h:
    - `std::size()` and `std::ssize()` should work with OIIO::string_view
      now. #2827 (2.3.3/2.2.11)
    - More thorough constexr of string_view methods. #2841 (2.3.3)
* strongparam.h:
    - New StrongParam helper for disambiguating parameters. #2735 (2.3.2)
* strutil.h:
    - Strutil `splits()` and `splitsv()` should return no pieces when passed
      an empty string. (It previously erroneously returned one piece
      consisting of an empty string.) #2712 (2.3.0.0)
    - Fix build break when strutil.h was included in Cuda 10.1 code. #2743
      (2.3.0.1/2.2.8)
    - `strhash()` is now constexpr for C++14 and higher. #2843 (2.3.3)
    - New Strutil functions: find, rfind, ifind, irfind #2960 (2.3.4/2.2.14)
      contains_any_char #3034 (2.3.7) rcontains, ircontains #3053 (2.3.7)
* sysutil.h:
    - Extend `getenv()` to take a default if the environment variable is not
      found. #3037 #3040 (2.3.7)
* typedesc.h:
    - `TypeDesc::basetype_merge(a,b)` returns a BASETYPE having the
      precision and range to hold the basetypes of either `a` or `b`.
      #2715 (2.3.0.0)
    - TypeDesc can now describe 2D and 3D bounding boxes, as arrays of 2
      VEC2 aggregates (for 2D) or VEC3 aggregates (for 3D) with "BOX"
      semantic. The shorthand for these are `TypeBox2`, `TypeBox3` (for
      float), and `TypeBox2i` and `TypeBox3i` for integer or pixel coordinate
      boxes. #3008 (2.2.17/2.3.6)
* unordered_map_concurrent.h:
    - New methods find_or_insert, nobin_mask(). #2867 (2.2.12/2.3.3)
* ustring.h:
    - ustring internals now have a way to ask for the list of ustrings whose
      hashses collided.  #2786 (2.2.11/2.3.2.0)
    - ustring now guarantees that no two ustrings will return the exact same
      value for `hash()`. #2870 (2.3.3)
    - ustring now explicitly disallows embedded NUL (0) characters in the
      middle of a string. A ustring constructed from such a thing will
      truncate the string after the first NUL. #3326 (2.4.0.2)

Build/test system improvements and platform ports:
* CMake build system and scripts:
    - Instead of defaulting to looking for Python 2.7, the OIIO build now
      defaults to whatever Python is found (though a specific one can still
      be requested via the PYTHON_VERSION variable). #2705 (2.3.0.0/2.2.8)
      #2764 (2.3.0.1/2.2.8)
    - Make the OIIO CMake files work properly if OIIO is a subproject. Also
      various other CMake script refactoring. #2770 (2.3.1.1/2.2.9)
    - Extend checked_find_package with VERSION_MIN and VERSION_MAX #2773
      (2.3.1.1/2.2.9), DEFINITIONS and SETVARIABLES #3061 (2.3.7)
    - No longer directly link against python libraries when unnecessary.
      #2807 (2.2.11/2.3.3)
    - On Windows, fix some linkage problems by changing the pybind11
      bindings to make a CMake "shared" library rather than "module". Sounds
      wrong, but seems to work. We will reverse if this causes problems.
      #2830 (2.3.3/2.2.11)
    - Improvements to building or linking static libraries. #2854 (2.2.12/2.3.3)
    - Change default STOP_ON_WARNING to OFF for release branches (including
      this one) so that small change in compiler warnings after our release
      don't break anybody's builds. (Though we still stop on warnings for CI
      builds). #2861 (2.2.12/2.3.3)
    - The pkgconfig OpenImageIO.pc was specifying the include path
      incorrectly. #2869 (2.2.12/2.3.3)
    - Support for CMake 3.20. #2913 #2931 (2.3.4)
    - Be sure to have the namespace include the patch number for pre-release
      builds from the master branch. #2948 (2.3.4)
    - Use modern style cmake targets for PNG and ZLIB dependencies. #2957
      (2.3.4)
    - Propagate C++14 minimum requirement to downstream projects via our
      cmake config exports. #2965 (2.3.5)
    - Fix exported cmake config files, which could fail if Imath and OpenEXR
      weren't the at the same version number. #2975 (2.3.5/2.2.15)
    - Change default postfix for debug libraries to `_d`. (2.3.5)
    - Our CMake `checked_find_package` is now able to be requested to favor
      an exported config file, if present, on a package-by-package basis.
      #2984 (2.3.5/2.2.15)
    - If a package is requested to be disabled, skip its related tests rather
      than reporting them as broken. #2988 (2.3.5/2.2.16)
    - Better support for running testsuite when the build dir in odd places.
      #3065 #3067 (2.3.7.1)
    - To prevent accidental overwrite of sensitive areas (such as
      /usr/local), you now need to explicitly set CMAKE_INSTALL_PREFIX if you
      want the "install" to not be local to the build area. #3069 (2.3.7.1)
* Dependency version support:
    - C++20 is now supported. #2891 (2.3.3)
    - Fix deprecation warnings when building with very new PugiXML versions.
      #2733 (2.3.0.1/2.2.8)
    - Fixes to build against OpenColorIO 2.0. #2765 (2.3.0.1/2.2.8) #2817
      #2849 (2.3.3/2.2.11) #2911 (2.3.4)
    - Fix to accommodate upcoming OpenColorIO 2.1 deprecation of
      parseColorSpaceFromString. #2961 (2.3.5/2.2.15)
    - Work to ensure that OIIO will build correctly against Imath 3.0 and
      OpenEXR 3.0, as well as Imath 3.1 / OpenEXR 3.1. #2771 (2.3.1.1/2.2.9)
      #2876 #2678 #2883 #2894 (2.3.3/2.2.12) #2935 #2941 #2942 #2947 (2.3.4)
    - Better finding of OpenJpeg 2.4. #2829 (2.3.3/2.2.11)
    - On Mac, libheif 1.10 is very broken. Don't use that version. #2847
      (2.3.3/2.2.11)
    - Fix build break against changes coming in future libtiff, where it
      is changing from some libtiff-defined integer types to the equivalent
      stdint.h types. #2848 (2.3.3/2.2.11)
    - Fixes to support the libraw 202101 snapshot (their in-progress 0.21.0).
      #2850 (2.3.3/2.2.11)
    - More clear warnings about using OpenVDB 8+ when building for C++11,
      because OpenVDB 8 requires C++14 or higher. #2860  (2.2.12/2.3.3)
    - More gracefully handle building against a custom Imath/OpenEXR even
      when another exists in the system area. #2876 (2.2.12/2.3.3)
    - Minor fixes to build cleanly against the upcoming Imath 3.0. #2878
      (2.2.12/2.3.3)
    - Remove obsolete dependency on Boost random (now use std::random).
      #2896 (2.3.3)
    - libtiff 4.3 is supported. #2953 (2.3.4)
    - We now include a `build_OpenJPEG.bash` script that can conveniently
      build a missing OpenJPEG dependency. (2.3.5/2.2.16)
    - Changes to make it build against TBB 2021. #2985 (2.3.5/2.2.15)
    - Support for building OIIO with gcc 11. #2995 (2.2.16/2.3.6)
    - Fixes to accommodate Imath 3.1 upcoming changes. #2996 (2.2.16/2.3.6)
    - Finding FFMpeg now correctly detects the version. #2994 (2.2.16/2.3.6)
    - FFMpeg minimum version is now >= 3.0. #2999 (2.3.6)
    - Fixes for detecting and using Ptex, among other things got the version
      wrong. #3001 (2.2.16/2.3.6)
    - Fixes for building against fmt 8.0. #3007 (2.3.6)
    - The OpenCV minimum version is now >= 3.0. #3015 (2.3.6)
    - Fixes to build properly against the upcoming OpenColorIO 2.1. #3050
      (2.3.7)
    - Finding boost is more flexible when desiring static libraries. #3031
      (2.3.7/2.2.17)
    - FindTBB.cmake module updated to create proper targets. #3060 (2.3.7)
    - All the `src/build_scripts/build_*.bash` scripts now honor an env
      variable called `DEP_DOWNLOAD_ONLY`, which if set will only do the
      downloads but not the builds. #3058 #3072 (2.3.7)
    - Better finding of OpenCV on Windows. #3062 (2.3.7.1)
* Testing and Continuous integration (CI) systems:
    - Completely get rid of the old appveyor CI. #2782 (2.3.2.0)
    - Test against libtiff 4.2 in the "latest releases" test. #2792 (2.3.2.0)
    - Got Windows CI fully working, bit by bit. #2796 #2798 #2805 #2821 #2826
      #2834 #2835 #2836 #2838 #2839 #2840 (2.3.3)
    - Reduce redundant verbosty in CI output logs during dependency building.
    - Modify hash_test to verify correctness and stability of the hashes.
      #2853 (2.3.3)
    - When building custom OpenEXR with build_openexr.bash, don't have it
      build the examples. #2857 (2.2.12/2.3.3)
    - Speed up CI by using GitHub 'cache' actions + ccache. #2859 (2.2.12/2.3.3)
    - Separate stages (setup, deps, build, test) into separate GHA "steps"
      for better logging and understanding of the timing and performance.
      #2862 (2.2.12/2.3.3)
    - Now actively testing libheif in Linux CI. #2866 (2.2.12/2.3.3)
    - Remove the last vestiges of Travis-CI, which we no longer use. #2871
      (2.2.12/2.3.3)
    - For failed tests, add CMake cache and log part of the saved artifacts.
      (2.2.12/2.3.3)
    - CI now tests build in C++20 mode. #2891 (2.3.3)
    - Our CI clang-format test now uses LLVM/clang-format 11. #2966
    - Test the clang11 + C++17 combo. #3004 (2.3.6)
* Platform support:
    - Fixes for mingw. #2698 (2.3.0.0)
    - Windows fix: correct OIIO_API declaration on aligned_malloc,
      aligned_free of platform.h. #2701 (2.3.0.0)
    - Fix boost linkage problem on Windows. #2727 (2.3.0.1/2.2.8)
    - Fix warnings when compiling webpinput.cpp on 32 bit systems. #2783
      (2.3.2.0)
    - Fix problems with `copysign` sometimes defined as a preprocessor symbol
      on Windows. #2800 (2.3.2)
    - Fixes related to Mac M1-based systems: Fix crash in ustring internals
      #2990 (2.3.5/2.2.15.1)

Notable documentation changes:
* Make Readthedocs generate downloadable HTML as well as PDF. #2746
* Explain how to read image data into separate per-channel buffers. #2756
  (2.3.0.1/2.2.8)
* Improve use of breathe and formatting. #2762 (2.3.1.0)
* Remove documentation of deprecated ImageBufAlgo functions. #2762 (2.3.1.0)
* Document missing texture option fields. #2762 (2.3.1.0)
* Starting to use "sphinx_tabs" as a clear way to present how things should
  be done for different language bindings. #2768 (2.3.1.0)



Release 2.2.21 (1 Jul 2022) -- compared to 2.2.20
--------------------------------------------------
* BMP: gain the ability to read some very old varieties of BMP files. #3375
* BMP: better detection of corrupted files with nonsensical image dimensions
  or total size. #3434
* BMP: protect against corrupted files that have palette indices out of bound.
  #3435
* ffmpeg: Support for ffmpeg 5.0. #3282
* ffmpeg: protect against possible double-free. #3376
* ffmpeg: make the supported file extension check be case-insensitive. This
  prevents movie files from being incorrectly unable to recognize their format
  if they have the wrong capitalization of the file extension. #3364
* hdr/rgbe files: Avoid possible Windows crash when dealing with characters
  with the high bit set. #3310
* TIFF: fix read problems with TIFF files with non-zero y offset. #3419
* Dev goodies: ustring has added a from_hash() static method #3397, and a
  ustringhash helper class #3436.
* simd.h fixes for armv7 and aarch32. #3361

Release 2.2.20 (1 Feb 2022) -- compared to 2.2.19
--------------------------------------------------
* Fix some address sanitizer failures. #3160
* Build/CI: Deal with OpenColor renaming its master branch to main. #3169
* Windows: Fix error when compiling with MSVC. #3168
* Fix excessive memory usage when saving EXR with many channels. #3176
* TIFF: now works for 16-bit palette images. #3260
* Fix ImageBuf::read bug for images of mixed per-channel data types. #3088

Release 2.2.19 (1 Nov 2021) -- compared to 2.2.18
--------------------------------------------------
* Better catching of exceptions thrown by OCIO 1.x if it encounters 2.0 config
  files. #3089
* Address new warnings revealed by clang 13. #3122
* Fixed some minor python binding bugs. #3074 #3094
* Fix when building with Clang on big-endian architectures. #3133
* Fix occasional build breaks related to OpenCV headers. #3135
* Improvements to NetBSD and OpenBSD support. #3137.
* Fixes to work with the libraw 202110 snapshot. #3143

Release 2.2.18 (1 Sep 2021) -- compared to 2.2.17
--------------------------------------------------
* Honor env variable `CUE_THREADS` (used by OpenCue) to set the default size
  of OIIO's thread pool. #3038
* Compatibility with OpenColorIO 2.1. #3050
* Dev: Extend Sysutil::getenv() to take a default if the environment variable
  is not found. #3047 #3048

Release 2.2.17 (1 Aug 2021) -- compared to 2.2.16
--------------------------------------------------
* Output to DPX files now supports IOProxy. (Input already did.) #3013
* typedesc.h: TypeDesc can now describe 2D and 3D bounding boxes, as arrays
  of 2 VEC2 aggregates (for 2D) or VEC3 aggregates (for 3D) with "BOX"
  semantic. The shorthand for these are `TypeBox2`, `TypeBox3` (for float),
  and `TypeBox2i` and `TypeBox3i` for integer or pixel coordinate
  boxes. #3008
* Build: Fixes for building against fmt 8.0.0. #3007
* Build: Finding boost is more flexible when desiring static libraries. #3031

Release 2.2.16 (1 Jul 2021) -- compared to 2.2.15
--------------------------------------------------
* New UDIM texture name patterns recognized: `%(UDIM)d` is the Houdini
  convention, and `_u##v##` is for Animal Logic's internal renderer. #3006
  (2.2.16)
* When doing color space transforms on images with > 4 channels -- the
  additional channels are now copied unaltered, rather than leaving them
  black. #2987 (2.2.16)
* FFMpeg: fix some encodings that didn't correctly recognize that they were
  more than 8 bits, or had alpha. #2989 (2.2.16)
* farmhash.h: Clean up all non-namespaced preprocessor symbols that are set
  by this header and may pollute the caller's symbols. #3002 (2.2.16)
* Fix crashes on M1 (ARM) based Mac. #2990 (2.2.16)
* Bug fix: avid divide-by-0 error computing chunk size for invalid image
  sizes. #2983 (2.2.16)
* `make_texture` (and `maketx` and `oiiotool -otex`) no longer crash if you
  try to make a texture out of a "deep" image; instead it will return an
  error message. #2991 (2.2.16)
* filesystem.h: Improve exception safety in Filesystem directory iteration.
  #2998 (2.2.16)
* Build: Improve finding of OpenJPEG. #2979 (2.2.16)
* Build: Support for building OIIO with gcc 11. #2995 (2.2.16)
* Build: Fixes to accommodate Imath 3.1 upcoming changes. #2996 (2.2.16)
* Build: Finding FFMpeg now correctly detects the version. #2994 (2.2.16)
* Build: clang + C++17 + LibRaw < 0.20 are mutually incompatible. Detect
  this combination and warn / disable libraw under those conditions. #3003
  (2.2.16)
* Build: Fix CMake behavior for `REQUIRED_DEPS` due to a typo. #3011 (2.2.16)
* Build: Fixes for detecting and using Ptex, among other things got the
  version wrong. #3001 (2.2.16)
* Testing: If a feature is disabled, skip its tests rather than reporting
  them as broken. #2988 (2.2.16)
* CI: Test the combination of clang and C++17. #3003 (2.2.16)

Release 2.2.15.1 (3 Jun 2021) -- compared to 2.2.15.0
-----------------------------------------------------
* Fix crash / misbehavior in ustring internals on Apple M1 ARM. #2990

Release 2.2.15 (1 Jun 2021) -- compared to 2.2.14
--------------------------------------------------
* BMP improvements: now support reading rle-compressed BMP files; writing
  single channel grayscale images now save as 8bpp palette images intead of
  24bpp; and reading 8bpp where all palette entries have R==G==B looks like
  a 1-channel grayscale instead of 3-channel RGB. #2976
* Bug: IBA::render_text did not properly account for alpha of the draw
  color. #2981
* Bug: Fix runaway parsing time for pathological XMP metadata. #2968
* Bug: Fixed a crash is ImageCacheFile::read_unmipped when sampling
* Fix exported cmake config files, which could fail if Imath and OpenEXR
  weren't the at the same version number. #2975
* Build: Modernize cmake to use targets for PNG and ZLIB. #2957
* Build: Fix to accommodate upcoming OpenColorIO 2.1 deprecation of
  parseColorSpaceFromString. #2961
* Build: Changes to make it build against TBB 2021. #2985
* Dev: Add Strutil functions: find, rfind, ifind, irfind. #2960

Release 2.2.14 (1 May 2021) -- compared to 2.2.13
--------------------------------------------------
* JPEG: Improve readin of files with mildly corrupted headers. #2927
* TIFF: Support IOProxy for input. #2921
* BMP: Improve performance by eliminating wasteful per-scanline allocation
  and needless data copying. #2934
* Build/CI: Fix all the build_*.bash scripts to not use cmake --config flag,
  which was harmlessly ignored but is flagged as an error for CMake 3.20.
  #2931
* Build: More fixes related to supporting a wide range of OpenEXR versions,
  and making our exported cmake configs correctly transmit dependencies on
  OpenEXR include paths. #2935 #2941 #2942 #2947
* ImageBufAlgo::fillholes_pushpull: added logic to correctly set the spec's
  alpha_channel field when generating sub-images. #2939
* Python: MakeTxBumpWithSlopes enum value had been inadvertently omitted
  from the Python bindings. #2951

Release 2.2.13 (1 Apr 2021) -- compared to 2.2.12
--------------------------------------------------
* Get ready for upcoming Imath/OpenEXR 3.0 release. #2883 #2894 #2897
* GIF: Fix error checking for non-existent GIF files. #2886
* Fix crash related to ImageBuf:set_write_format() when used in conjunction
  with a file format that doesn't support per-channel data types. #2885
* Make RAW files handle Unicode filenames on Windows. #2888
* Make sure OIIO builds cleanly with C++20. #2891
* BMP: Several bug fixes when reading an older (V1) variety of BMP files. 
  #2899
* Targa: fix reading tga files with 0-sized thumbnails. #2903
* TIFF: fix reading of certain tiled TIFF files with the vertical image size
  is not a whole multiple of the tile size. #2895
* Avoid OpenColorIO v2 exception when color-conversion of single points (we
  didn't pass a correct stride, thinking that stride isn't used for a single
  point). #2911
* make_texture: When outputting to a 'half' data format file, clamp filtered
  values to the half range so large values don't inadvertently get converted
  to half 'infinite' values. #2892
* Improvements to ustring internals to ensure that no two ustrings ever end
  up with the same hash() value. #2870
* Fixes for compiler errors when compiling farmhash.h using C++11 mode with
  newer gcc (6+). #2914

Release 2.2.12 (1 Mar 2021) -- compared to 2.2.11
--------------------------------------------------
* Bug fix: Improve parsing of XMP records in metadata: more correct handling
  of lists/sequences, better inference of types that look like int or float
  (rather than forcing unknown fields into strings), fixed bugs in parsing
  rational values. #2865
* Bug fix: Fix loading PSB files with cinf tags. #2877
* Build: Improvements to building or linking static libraries. #2854
* Build: Change default STOP_ON_WARNING to OFF for release branches
  (including this one) so that small change in compiler warnings after our
  release don't break anybody's builds. (Though we still stop on warnings
  for CI builds). #2861
* Build: More clear warnings about using OpenVDB 8+ when building for C++11,
  because OpenVDB 8 requires C++14 or higher. #2860
* Build: The pkgconfig OpenImageIO.pc was specifying the include path
  incorrectly. #2869
* Build: More gracefully handle building against a custom Imath/OpenEXR even
  when another exists in the system area. #2876
* Build: Minor fixes to build cleanly against the upcoming Imath 3.0. #2878
* Dev: hash.h: Make many of the hash functions constexpr. #2843
* Dev: Better unit tests to verify correctness and stability over time of
  the hash functions. #2853
* Dev: unordered_map_concurrent.h: New methods find_or_insert, nobin_mask().
  #2867
* CI: Speed up CI builds by not building OpenEXR example programes. #2857
* CI: Speed up CI by using GitHub 'cache' actions + ccache. #2859
* CI: Separate stages (setup, deps, build, test) into separate GHA "steps"
  for better logging and understanding of the timing and performance. #2862
* CI: Now actively testing libheif in Linux CI. #2866
* CI: Remove the last vestiges of Travis-CI, which we no longer use. #2871
* CI: For failed tests, add CMake cache and log part of the saved artifacts.
* PSA: Avoid libheif 1.10 on Mac, it is broken. Libheif 1.11 is fine.

Release 2.2.11.1 (1 Feb 2021) -- compared to 2.2.11.0
-----------------------------------------------------
* Fix build break against Qt 5.15.2 (deprecated enum). #2852

Release 2.2.11 (1 Feb 2021) -- compared to 2.2.10
--------------------------------------------------
* Enabled AVIF decoding of heic files (requires libheif >= 1.7 and for it
  to have been built with an AV1 encoder/decoder). #2811 #2812 #2814 #2818
* `oiiotool --help` now prints the OCIO version (where it prints the config
  file and known color space). #2849
* Bug fix: ImageBufAlgo::ociolook() and ociofiletransform() internally
  reversed the order of their `inverse` and `unpremult` arguments, making it
  hard to select the inverse transformation. #2844
* Fix memory leak during decoding of some invalid Exif blocks. #2824
* Build: Fixed warnings when building against python 2.x. #2815
* Build: No longer directly link against python libraries when unnecessary.
  #2807
* Build: Better finding of OpenJpeg 2.4. #2829
* Build: On Windows, fix some linkage problems by changing the pybind11
  bindings to make a CMake "shared" library rather than "module". Sounds
  wrong, but seems to work. We will reverse if this causes problems. #2830
* Build: On Mac, libheif 1.10 is very broken. Don't use that version. #2847
* Build: Fix build break against changes coming in future libtiff, where it
  is changing from some libtiff-defined integer types to the equivalent
  stdint.h types. #2848
* Build: Some final touches to prepare for release of OpenColor 2.0. #2849
* Build: Fixes to support the libraw 202101 snapshot (their in-progress
  0.21.0). #2850
* CI: Got Windows CI fully working, bit by bit. #2796 #2805 #2821 #2826
  #2834 #2835 #2836 #2840
* Dev: Some internal rearrangement of span.h and string_view.h (that should
  not break source or ABI compatibility). `std::size()` and `std::ssize()`
  should work with OIIO::span and OIIO::string_view now. #2827
* Dev: ustring internals now have a way to ask for the list of ustrings
  whose hashses collided.  #2786
* Dev: New Filesystem::generic_filepath() returnss a filepath in generic
  format (not OS specific). #2819
* Dev: ArgParse::abort() lets the response to a command line argument signal
  that no further arguments should be parsed. #2820
* Dev: In oiioversion.h, added macros `OIIO_VERSION_GREATER_EQUAL` and
  `OIIO_VERSION_LESS`. #2831
* Dev: In platform.h, added macro OIIO_INLINE_CONSTEXPR, which is equivalent
  to `inline constexpr` for C++17, but just constexpr for C++ <= 14. #2832

Release 2.2.10.1 (7 Jan 2021) -- compared to 2.2.10.0
-----------------------------------------------------
* Fix build break against OpenColorIO v2.0 RC1. #2817

Release 2.2.10 (1 Jan 2021) -- compared to 2.2.9
-------------------------------------------------
* GIF: support for UTF-8 filenames on Windows. #2777
* OpenEXR: Fix rare crash during multithreaded output. #2781
* OpenEXR: Fix potential crash parsing OpenEXR header that contains Rational
  attributes with certain values. #2791
* Improved error reporting for IOFile failures to open the file. #2780
* Build: Fix webp compile break on 32 bit systems. #2783
* Build/Windows: Fix symbol definition conflict with pyconfig.h. #2800
* CI: Test the latest fmt, PugiXML, and pybind11 releases. #2778
* Docs: Add explanation of oiiotool -otex modifiers that were missing from
  the docs. #2790  Fix some duplicated text. #2785

Release 2.2.9 (1 Dec 2020) -- compared to 2.2.8
-------------------------------------------------
* TIFF: Fix reading files with "separate" planarconfig and rowsperstrip more
  than 1. #2757 (2.3.0.1/2.2.9)
* RAW: add "raw:user_flip" input configuration hint to control this option
  in the underlying libraw. #2769 (2.3.1.0)
* PNG: Read Exif data from PNG files. #2767
* BMP: Fix reading BMP images with bottom-to-top row order. #2776
* Work to ensure that OIIO will build correctly against the upcoming
  Imath 3.0 and OpenEXR 3.0.
* Make the OIIO CMake files work properly if OIIO is a subproject. Also
  various other CMake script refactoring. #2770

Release 2.2.9 (1 Dec 2020) -- compared to 2.2.8
-------------------------------------------------
* TIFF: Fix reading files with "separate" planarconfig and rowsperstrip more
  than 1. #2757 (2.3.0.1/2.2.9)
* RAW: add "raw:user_flip" input configuration hint to control this option
  in the underlying libraw. #2769 (2.3.1.0)
* PNG: Read Exif data from PNG files. #2767
* BMP: Fix reading BMP images with bottom-to-top row order. #2776
* Work to ensure that OIIO will build correctly against the upcoming
  Imath 3.0 and OpenEXR 3.0. #2771
* Make the OIIO CMake files work properly if OIIO is a subproject. Also
  various other CMake script refactoring. #2770

Release 2.2.8 (1 Nov 2020) -- compared to 2.2.7
-------------------------------------------------
* Fix that ImageBuf images backed by ImageCache, could hold an outdated copy
  of the image if it was in the imagecache once, then changed on disk. #2696
* Fix stack overflow crash in IBA::colorconvert of unusually wide images.
  #2716
* Fix boost linkage problem on Windows. #2727
* Fix broken reads of 16 bit iff files. #2736
* Fix make_texture incorrectly setting tile sizes. #2737
* Fix incorrect ARM NEON code in simd.h. #2739
* Improve oiiotool --chappend and --siappend, allowing an optional modifier
  ":n=" to specify the number of images from the stack to be combined by
  having their channels or subimages appended. #2709
* WebP: add support for requesting compression "lossless". #2726
* Improve build system for finding Python, now if a specific version is not
  requested, default to whichever is found rather than always defaulting to
  Python 2.7. #2705 #2764
* Fix deprecation warnings when building with very new PugiXML versions.
  #2733
* Fix ImageCache bug: add_tile/get_tile not properly honoring when
  `chend < chbegin` it should get all channels. #2742
* Fix build break when strutil.h was included in Cuda 10.1 code. #2743
* Docs: Make readthedocs generate downloadable htm as well as pdf. #2746
* Improve oiiotool's guessing about the desired output format based on
  inputs (in the absence of `-d` to specify the format). #2717
* JPEG output: add support for writing progressive JPEGS (set the
  attribute "jpeg:progressive" to 1). #2749
* WebP input improvements including: RGB images are kept as RGB instead of
  always adding alpha; more efficient by skipping alpha premultiplication
  when it's unnecessary; now can read animated WebP images as multi-subimage
  files. #2730
* Docs: ImageInput chapter now has an example of reading image data into
  separate per-channel buffers. #2756
* Fixes to build against recent changes in OpenColorIO v2 master. #2765

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

Release 2.1.10.1 (10 Jan 2020)
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
* ImageSpec::find_attribute now will retrieve "datawindow" and "displaywindow"
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
* ImageSpec::find_attribute now will retrieve "datawindow" and "displaywindow"
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
      spot the existence of particular metadata. #1982 (1.9.4)
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
  version we support is 3.6.1. #1843 (1.9.2/1.8.8)
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
    * Reimplementation of `spin_rw_mutex` has much better performance when
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
* [CHANGES-0.x](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/master/src/doc/CHANGES-0.x.md).
* [CHANGES-1.x](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/master/src/doc/CHANGES-1.x.md).
