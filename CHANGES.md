Release 3.2 (target: Sept 2026?) -- compared to 3.1
---------------------------------------------------

### New minimum dependencies and compatibility changes:
### ⛰️  New features and public API changes:
* *New image file format support:*
* *oiiotool new features and major improvements*:
* *Command line utilities*:
* *ImageBuf/ImageBufAlgo*:
* *ImageCache/TextureSystem*:
* New global attribute queries via OIIO::getattribute():
* Miscellaneous API changes:
  - *api*: Versioned namespace to preserve ABI compatibility between minor releases [#4869](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4869) (3.2.0.0)
* Color management improvements:
  - Fix some legacy 'Linear' color references [#4959](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4959) (3.2.0.0)
  - Auto convert between oiio:ColorSpace and CICP attributes in I/O [#4964](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4964) (by Brecht Van Lommel) (3.0.14.0, 3.2.0.0)
  - *openexr*: Write OpenEXR colorInteropID metadata based on oiio:ColorSpace [#4967](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4967) (by Brecht Van Lommel) (3.0.14.0, 3.2.0.0)
  - *jpeg-xl*: CICP read and write support for JPEG-XL [#4968](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4968) (by Brecht Van Lommel) (3.2.0.0, 3.1.9.0)
  - *jpeg-xl*: ICC read and write for JPEG-XL files (issue 4649) [#4905](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4905) (by shanesmith-dwa) (3.0.14.0, 3.2.0.0)
### 🚀  Performance improvements
### 🐛  Fixes and feature enhancements
  - *IBA*: IBA::compare_Yee() accessed the wrong channel [#4976](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4976) (by Pavan Madduri) (3.2.0.0)
  - *exif*: Support EXIF 3.0 tags [#4961](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4961) (3.2.0.0)
  - *imagebuf*: Fix set_pixels bug, didn't consider roi = All [#4949](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4949) (3.2.0.0)
  - *ffmpeg*: 10 bit video had wrong green channel [#4935](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4935) (by Brecht Van Lommel) (3.2.0.0, 3.1.7.0)
  - *iff*: Handle non-zero origin, protect against buffer overflows [#4925](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4925) (3.2.0.0, 3.1.7.0)
  - *jpeg*: Fix wrong pointers/crashing when decoding CMYK jpeg files [#4963](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4963) (3.2.0.0)
  - *jpeg-2000*: Type warning in assertion in jpeg2000output.cpp [#4952](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4952) (3.2.0.0)
  - *jpeg-xl*: ICC read and write for JPEG-XL files (issue 4649) [#4905](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4905) (by shanesmith-dwa) (3.2.0.0)
  - *jpeg-xl*: Correctly set Quality for JPEG XL [#4933](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4933) (3.2.0.0, 3.1.7.0)
  - *jpeg-xl*: CICP read and write support for JPEG XL [#4968](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4968) (by Brecht Van Lommel) (3.2.0.0, 3.1.9.0)
  - *openexr*: Support for idManifest and deepImageState (experimental) [#4877](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4877) (3.2.0.0, 3.1.7.0)
  - *openexr*: ACES Container hint for exr outputs [#4907](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4907) (by Oktay Comu) (3.2.0.0, 3.1.7.0)
  - *openexr*: Write OpenEXR colorInteropID metadata based on oiio:ColorSpace [#4967](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4967) (by Brecht Van Lommel) (3.0.14.0, 3.2.0.0)
  - *openexr*: Improve attribute translation rules [#4946](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4946) (3.2.0.0)
  - *openexr*: ACES container writes colorInteropId instead of colorInteropID [#4966](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4966) (by Brecht Van Lommel) (3.2.0.0)
  - *png*: We were not correctly suppressing hint metadata [#4983](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4983) (3.2.0.0)
  - *sgi*: Implement RLE encoding support for output [#4990](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4990) (by Jesse Yurkovich) (3.2.0.0)
  - *webp*: Allow out-of-order scanlines when writing webp [#4973](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4973) (by Pavan Madduri) (3.2.0.0)
### 🔧  Internals and developer goodies
  - *filesystem.h*: Speedup to detect the existence of files on Windows [#4977](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4977) (by JacksonSun-adsk) (3.2.0.0)
### 🏗  Build/test/CI and platform ports
* OIIO's CMake build system and scripts:
  - *build*: Allow auto-build of just required packages by setting `OpenImageIO_BUILD_MISSING_DEPS` to `required`. [#4927](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4927) (3.2.0.0, 3.1.7.0)
  - *build*: Make dependency report more clear about what was required [#4929](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4929) (3.2.0.0, 3.1.7.0)
* Dependency and platform support:
  - *deps*: Additional auto-build capabilities for dependencies that are not found: GIF library [#4921](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4921) (by Valery Angelique), OpenJPEG [#4911](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4911) (by Danny Greenstein) (3.2.0.0, 3.1.7.0)
  - *deps*: Disable LERC in libTIFF local build script [#4957](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4957) (by LI JI) (3.2.0.0, 3.1.8.0)
  - *deps*: Test against libraw 0.21.5 [#4988](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4988) (3.2.0.0, 3.1.9.0)
* Testing and Continuous integration (CI) systems:
  - *tests*: Image_span_test reduce benchmark load for debug and CI renders [#4951](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4951) (3.2.0.0, 3.1.8.0)
  - *ci*: Python wheel building improvements: use ccache [#4924](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4924) (by Larry Gritz), unbreak wheel release + other enhancements pt 1 [#4937](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4937) (by Zach Lewis) (3.2.0.0, 3.1.7.0)
  - *ci*: Simplify ci workflow by using build-steps for old aswf containers, too [#4932](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4932) (3.2.0.0, 3.1.7.0)
  - *ci*: We were not correctly setting fmt version from job options [#4939](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4939) (3.2.0.0, 3.1.7.0)
  - *ci*: Emergency fix change deprecated sonarqube action [#4969](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4969) (3.2.0.0)
  - *ci*: Try python 3.13 to fix Mac breakage on CI [#4970](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4970) (3.2.0.0)
### 📚  Notable documentation changes
  - *docs*: Update/correct explanation of "openexr:core" attribute, and typo fixes [#4943](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4943) (3.2.0.0, 3.1.7.0)
### 🏢  Project Administration
  - *admin*: Minor rewording in the issue and PR templates [#4982](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4982) (3.2.0.0)
### 🤝  Contributors

---
---



Release 3.1.9.0 (Jan 1, 2026) -- compared to 3.1.8.0
----------------------------------------------------
  - Color management improvements:
      - Auto convert between oiio:ColorSpace and CICP attributes in I/O [#4964](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4964) (by Brecht Van Lommel)
      - *exr*: Write OpenEXR colorInteropID metadata based on oiio:ColorSpace [#4967](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4967) (by Brecht Van Lommel)
      - *jpeg-xl*: CICP read and write support for JPEG-XL [#4968](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4968) (by Brecht Van Lommel)
      - *jpeg-xl*: ICC read and write for JPEG-XL files (issue 4649) [#4905](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4905) (by shanesmith-dwa)
  - *png*: We were not correctly suppressing hint metadata [#4983](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4983)
  - *sgi*: Implement RLE encoding support for output [#4990](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4990) (by Jesse Yurkovich)
  - *webp*: Allow out-of-order scanlines when writing webp [#4973](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4973) (by Pavan Madduri)
  - *fix/IBA*: IBA::compare_Yee() accessed the wrong channel [#4976](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4976) (by Pavan Madduri)
  - *perf/filesystem.h*: Speedup to detect the existence of files on Windows [#4977](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4977) (by JacksonSun-adsk)
  - *ci*: Address tight disk space on GHA runners [#4974](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4974)
  - *ci*: Optimize install_homebrew_deps by coalescing installs [#4975](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4975)
  - *ci*: Build_Ptex.bash should build Ptex using C++17 [#4978](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4978)
  - *ci*: Unbreak CI by adjusting Ubuntu installs [#4981](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4981)
  - *ci*: Test against libraw 0.21.5 [#4988](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4988)
  - *docs*: Fix missing docs for `OIIO:attribute()` and `OIIO::getattribute()` [#4987](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4987)


Release 3.1.8.0 (Dec 1, 2025) -- compared to 3.1.7.0
----------------------------------------------------
  - *exif*: Support EXIF 3.0 tags [#4961](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4961)
  - *jpeg*: Fix wrong pointers/crashing when decoding CMYK jpeg files [#4963](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4963)
  - *openexr*: Improve attribute translation rules [#4946](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4946)
  - *openexr*: ACES container writes colorInteropId instead of colorInteropID [#4966](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4966) (by Brecht Van Lommel)
  - *color mgmt*: Fix some legacy 'Linear' color references [#4959](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4959)
  - *imagebuf*: Fix `ImageBuf::set_pixels()` bug, didn't consider roi = All [#4949](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4949)
  - *tests*: Image_span_test reduce benchmark load for debug and CI renders [#4951](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4951)
  - *build*: Type warning in assertion in jpeg2000output.cpp [#4952](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4952)
  - *build*: Disable LERC in libTIFF local build script [#4957](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4957) (by LI JI)
  - *ci*: Fix broken ci, debug and static cases, bump some latest [#4954](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4954)
  - *ci*: Unbreak icc/icx CI [#4958](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4958)
  - *admin*: Update some license notices [#4955](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4955)


Release 3.1.7.0 (Nov 1, 2025) -- compared to 3.1.6.1
----------------------------------------------------
  - *openexr*: Support for idManifest and deepImageState (experimental) [#4877](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4877) (3.1.7.0)
  - *openexr*: ACES Container hint for exr outputs [#4907](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4907) (by Oktay Comu) (3.1.7.0)
  - *ffmpeg*: 10 bit video had wrong green channel [#4935](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4935) (by Brecht Van Lommel) (3.1.7.0)
  - *iff*: Handle non-zero origin, protect against buffer overflows [#4925](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4925) (3.1.7.0)
  - *jpeg-xl*: Correctly set Quality for JPEG XL [#4933](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4933) (3.1.7.0)
  - *api/docs*: Fix IBA::set_pixels declaration and docs [#4926](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4926) (3.1.7.0)
  - *win*: Address Windows crashes from issue 4641 [#4914](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4914) (3.1.7.0)
  - *fix*: Uninitialized value revealed by clang-21 warning [#4940](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4940) (3.1.7.0)
  - *build/deps*: Additional auto-build capabilities for dependencies that are not found: GIF library [#4921](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4921) (by Valery Angelique), OpenJPEG [#4911](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4911) (by Danny Greenstein) (3.1.7.0)
  - *build*: Allow auto-build of just required packages [#4927](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4927) (3.1.7.0)
  - *build*: Make dependency report more clear about what was required [#4929](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4929) (3.1.7.0)
  - *ci*: Python wheel building improvements: use ccache [#4924](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4924) (by Larry Gritz), unbreak wheel release + other enhancements pt 1 [#4937](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4937) (by Zach Lewis) (3.1.7.0)
  - *ci*: Drop deprecated macos-13 (intel) platform, add macos-15-intel [#4930](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4930) (3.1.7.0)
  - *ci*: Try to avoid ffmpeg install failures [#4936](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4936) (3.1.7.0)
  - *ci*: Simplify ci workflow by using build-steps for old aswf containers, too [#4932](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4932) (3.1.7.0)
  - *ci*: We were not correctly setting fmt version from job options [#4939](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4939) (3.1.7.0)
  - *tests*: Update ref images for heif [#4941](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4941) (3.1.7.0)
  - *docs*: Update/correct explanation of "openexr:core" attribute, and typo fixes [#4943](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4943) (3.1.7.0)


Release 3.1.6.2 (Oct 3, 2025) -- compared to 3.1.6.1
----------------------------------------------------
  - *oiioversion.h*: Restore definition of `OIIO_NAMESPACE_USING` macro [#4920](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4920)


Release 3.1 (Oct 2, 2025) -- compared to 3.0.x
-----------------------------------------------------
- Beta 1: Aug 22, 2025
- Beta 2: Sep 19, 2025
- Release candidate 1: Sep 27, 2025
- Full release, v3.1.6.1: Oct 2, 2025

**Executive Summary / Highlights:**
  - New image file support: Ultra HDR (HDR images in JPEG containers).
  - oiiotool new commands: `--layersplit`, `--pastemeta`, `--demosaic`,
    `--create-dir` and new expression expansion tokens: `IS_CONSTANT`,
    `IS_BLACK`, `SUBIMAGES`.
  - New IBA image processing functions: `scale()`, `demosaic()`.
  - New 2-level namespace scheme that we hope will make it possible in the
    future for our annual releases to NOT need to break backward ABI
    compatibility.
  - Support in Python for `ImageBuf._repr_png_` method allows use of OIIO
    inside [Jupyter Notebooks](https://jupyter.org/) to display computed
    images.
  - Color management improvements: Conform to Color Interchange Forum and
    OpenEXR new conventions for naming and specifying color spaces. PNG,
    HEIC, and ffmpeg/video files now support reading CICP metadata.

### New minimum dependencies and compatibility changes:
* *Python*: 3.9 minimum (from 3.7) [#4830](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4830) (3.1.4.0)
* *OpenColorIO*: 2.3 minimum (from 2.2) [#4865](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4865) (3.1.4.0)

### ⛰️  New features and public API changes:
* *New image file format support:*
    - *jpeg*: Support reading Ultra HDR images [#4484](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4484) (by Loïc Vital) (3.1.0.0/3.0.1.0)
* *oiiotool new features and major improvements*:
    - New expression eval tokens: `IS_CONSTANT`, `IS_BLACK` [#4610](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4610) (by Lydia Zheng) (3.1.1.0); `SUBIMAGES` [#4804](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4804) (3.1.3.0)
    - `--layersplit`, new command to split layers [#4591](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4591) (by Loïc Vital) (3.1.1.0)
    - `--pastemeta` now takes additional modifiers that allows options for merging rather than rewriting, and is able to copy only a subset of the metadata specified by a regex. [#4672](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4672) [#4674](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4674) [#4676](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4676) (3.1.1.0)
    - `--demosaic` add X-Trans demosaicing [#4579](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4579) (by Anton Dukhovnikov) (3.1.1.0)
    - `--render_text` new modifiers `measure=` and `render=` can
      be used to measure the size of rendered text without drawing it.` [#4681](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4681) (3.1.3.0)
    - `--create-dir` flag creates directories needed by `-o`
      if they doesn't already exist [#4762](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4762) (by Dharshan Vishwanatha) (3.1.3.0)
    - `--eraseattrib` new modifier `:fromfile=1` reads from a file
      to get a list of patterns to specify the attributes to erase. [#4763](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4763) (by Lydia Zheng) (3.1.3.0)
    - *oiiotool*: Allow easy splitting of subimages by name [#4874](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4874) (3.1.5.0)
* *Command line utilities*:
    - *iv*: Implement files drag and drop into an iv window [#4774](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4774) (by Aleksandr Motsjonov) (3.1.3.0)
    - *iv*: Area probe [#4767](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4767) (by Danielle Imogu) (3.1.3.0)
    - *iv*: Add min/max/avg and sizing to pixel closeup view [#4807](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4807) (by Aleksandr Motsjonov) (3.1.4.0)
* *ImageBuf/ImageBufAlgo*:
    - New `ImageBuf::merge_metadata()`: merges one IB's metadata into another's without deleting the metadata already present. It can also filter which metadata are copied using a regex. [#4672](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4672) (3.1.1.0)
    - New `ImageBufAlgo::scale()`: scales all channels of a multi-channel image by the single channel of a second image. [#4541](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4541) (by Anton Dukhovnikov) (3.1.0.0/3.0.1.0)
    - `ImageBufAlgo::demosaic()` added support for X-Trans demosaicing [#4579](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4579) (by Anton Dukhovnikov) (3.1.1.0)
* *APIs moving to `span` and `image_span`*:
  Add API calls using `span` and `image_span` as the preferred alternative for
  the historical calls that took raw pointers and spans to designate buffers
  to read from or write into.
    - *image_span.h*: Add `image_span` template, deprecate image_view [#4703](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4703) [#4784](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4784) (3.1.3.0)
    - *ImageInput / ImageOutput*: [#4669](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4669) (3.1.1.0) [#4727](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4727) [#4748](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4748) (3.1.3.0)
    - *ImageCache/TextureSystem*: [#4783](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4783) (3.1.3.0) [#4838](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4838) (3.1.4.0)
    - *ImageBuf*: [#4814](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4814) (3.1.3.0) [#4827](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4827) (3.1.4.0)
* *ImageCache/TextureSystem*:
    - *maketx*: Add maketx flags to increase feature parity with txmake [#4841](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4841) (by Scott Milner) (3.1.4.0)
* New global attribute queries via OIIO::getattribute():
    - Added queries for available font families and styles [#4523](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4523) (by peterhorvath111) (3.1.0.0/3.0.1.0)
    - *api*: Add global attribute `imageinput:strict` [#4560](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4560) (3.1.0.0)
* Miscellaneous API changes:
    - *api*: Make a 2-level namespace scheme [#4567](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4567) (by Larry Gritz) [#4603](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4603) (by Brecht Van Lommel) (3.1.1.0) [#4869](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4869) (3.1.5.0)
    - *api*: ImageSpec::scanline_bytes, tile_bytes, image_bytes [#4631](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4631) (3.1.1.0)
    - *python*: ImageBuf `_repr_png_` method added, which allows use of
      ImageBuf in [Jupyter Notebooks](https://jupyter.org/) as a displayable object. [#4753](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4753) (by Oktay Comu) (3.1.3.0)
    - *python*: Add python stub files [#4692](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4692) (by Chad Dombrova) (3.1.3.0)
    - *api*: Add new ImageInput::supports() query: "mipmap" [#4800](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4800) (3.1.3.0)
* Color management changes
    - *color mgmt*: Don't assume unlabeled OpenEXR files are lin_rec709 [#4840](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4840) (3.1.4.0)
    - *color mgmt*: Color space renaming to adhere to CIF conventions [#4860](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4860) (3.1.4.0)
    - Ability to read CICP metadata from PNG [#4746](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4746) (by Zach Lewis) (3.1.5.0), HEIC [#4880](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4880) (by Brecht Van Lommel) (3.1.5.0), and video files/ffmpeg [#4882](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4882) (by Brecht Van Lommel) (3.1.5.0).


### 🚀  Performance improvements:
  - *perf*: IBA::unsharp_mask() speed and memory optimization [#4513](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4513) (by Vlad (Kuzmin) Erium) (3.1.0.0/3.0.1.0)
  - *perf*: Oiiotool --line --text --point --box speedups [#4518](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4518) (3.1.0.0/3.0.1.0)
  - *perf*: Jpeg2000 valid_file implementation, much faster than trying to open [#4548](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4548) (by Aras Pranckevičius) (3.1.0.0/3.0.1.0)
  - *perf*: Faster utf8<->utf16 conversion on Windows [#4549](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4549) (by Aras Pranckevičius) (3.1.0.0/3.0.1.0)
  - *perf*: Speed up `maketx --envlatl` when multithreaded by over 10x. [#4825](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4825)
  - *perf*: Speed up OpenEXR non-core header read time [#4832](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4832)

### 🐛  Fixes and feature enhancements:
  - *oiiotool*: Better handling of wildcards that match no files [#4627](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4627) (3.1.1.0)
  - *oiiotool*: Invalid loop bound when appending mipmap textures using oiiotool [#4671](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4671) (by Basile Fraboni) (3.1.1.0)
  - *oiiotool*: -i:native=1, fix --native behavior, fix convert datatype [#4708](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4708) (3.1.3.0)
  - *oiiotool*: Fixes to --missingfile behavior [#4803](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4803) (3.1.3.0)
  - *oiiotool*: Allow thread control for --parallel-frames [#4818](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4818) (3.1.3.0)
  - *oiiotool*: Use normalized path when creating wildcard path pattern [#4904](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4904) (by Jesse Yurkovich) (3.1.6.0)
  - *oiiotool*: Ignore empty subimage(s) when calculating non-zero region [#4909](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4909) (by Carine Touraille) (3.1.6.0)
  - *color mgmt*: Support OCIO Viewing Rules, other OCIO-related improvements [#4780](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4780) (by zachlewis) (3.1.3.0)
  - *iv*: Fix crash on .DS_Store; fix uppercase extensions [#4764](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4764) (by Anton Dukhovnikov) (3.1.3.0)
  - *iv*: Do not resize on open and other zoom fixes [#4766](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4766) (by Aleksandr Motsjonov) (3.1.3.0)
  - *iv*: Bug fix for iv window losing focus on mac on startup [#4773](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4773) (by Aleksandr Motsjonov) (3.1.3.0)
  - *iv*: Use screen pixel ratio to render sharp text in pixel view tool [#4768](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4768) (by Aleksandr Motsjonov) (3.1.3.0)
  - *IBA*: IBA:demosaic add white balancing [#4499](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4499) (by Anton Dukhovnikov) (3.1.0.0/3.0.1.0)
  - *IBA*: IBA::demosaic - fix roi channels [#4602](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4602) (by Anton Dukhovnikov) (3.1.1.0)
  - *IBA*: Add 'auto' value for all options of `IBA::demosaic()` [#4786](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4786) (by Anton Dukhovnikov) (3.1.3.0)
  - *ImageBuf*: IB::pixeltype() did not always return the right value [#4614](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4614) (3.1.1.0)
  - *ImageBuf*: Fix bug in ImageBuf construction from ptr + neg strides [#4630](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4630) (3.1.1.0)
  - *ImageBuf*: Better errors for nonexistent subimages/mips [#4801](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4801) (3.1.3.0)
  - *ImageInput*: Incorrect IOProxy logic related to valid_file [#4839](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4839) (3.1.4.0)
  - *python*: Disable loading Python DLLs from PATH by default on Windows [#4590](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4590) (by zachlewis) (3.1.1.0)
  - *python*: Fix handle leak [#4685](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4685) (3.1.3.0)
  - *python*: IBA.demosaic had GIL release in wrong spot [#4777](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4777) (3.1.3.0)
  - *python*: Python ImageBuf.init_spec did not return correct value [#4805](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4805) (3.1.3.0)
  - *python*: Got strides wrong passing 2D numpy pixel array [#4843](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4843) (3.1.4.0)
  - *exr*: Allow an empty "name" metadata to be read [#4528](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4528) [#4536](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4536) (3.1.0.0/3.0.1.0)
  - *exr*: Avoid integer overflow for large deep exr slice strides [#4542](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4542) (3.1.0.0/3.0.1.0)
  - *exr*: Fill in OpenEXR lineOrder attribute when reading [#4628](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4628) (by vernalchen) (3.1.1.0)
  - *exr*: Did not properly allocate 'missingcolor' vector [#4751](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4751) (3.1.3.0)
  - *exr*: Not honoring 'missingcolor' for scanline files [#4757](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4757) (3.1.3.0)
  - *ffmpeg*: FFmpeg incorrectly set zero oiio:BitsPerSample [#4885](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4885) (by Brecht Van Lommel) (3.1.5.0)
  - *ffmpeg*: Add ability to read CICP metadata [#4882](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4882) (by Brecht Van Lommel) (3.1.5.0)
  - *gif*: Gif output didn't handle FramesPerSecond attribute correctly [#4890](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4890) (3.1.5.0)
  - *heic*: Read and write of CICP and support for bit depth 10 and 12 [#4880](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4880) (by Brecht Van Lommel) (3.1.5.0)
  - *ico*: More robust to corrupted ICO files [#4625](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4625) (3.1.1.0)
  - *iff*: Improved IFF support reading and writing z buffers [#4673](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4673) (by Mikael Sundell) (3.1.3.0)
  - *jpeg*: Support encoding/decoding arbitrary metadata as comments [#4430](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4430) (by Lukas Stockner) (3.1.0.0/3.0.1.0)
  - *jpeg-2000*: Write .j2c by adding HTJ2K Encoding using the OpenJPH library. [#4699](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4699) (by Sam Richards) (3.1.3.0)
  - *png*: Alpha premultiplication adjustment and attribute [#4585](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4585) (3.1.1.0)
  - *png*: Increase allowed width/height limit [#4655](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4655) (by Jesse Yurkovich) (3.1.1.0)
  - *png*: CICP metadata support for PNG [#4746](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4746) (by Zach Lewis) (3.1.5.0)
  - *pnm*: Broken pgm having memory access error [#4559](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4559) (3.1.0.0)
  - *psd*: Perform endian byteswap on correct buffer area for PSD RLE [#4600](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4600) (by Jesse Yurkovich) (3.1.1.0)
  - *psd*: ICC profile reading improvements, especially for PSD [#4644](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4644) (3.1.1.0)
  - *psd*: Updated tag recognition for psb [#4663](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4663) (by Lydia Zheng) (3.1.1.0)
  - *raw*: Fix channel layout [#4516](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4516) (by Anton Dukhovnikov) (3.1.0.0/3.0.1.0)
  - *raw*: Add black level and BPS metadata [#4601](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4601) (by Anton Dukhovnikov) (3.1.1.0)
  - *raw*: Add `raw:ForceLoad` ImageInput configuration hint [#4704](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4704) (by Anton Dukhovnikov) (3.1.3.0)
  - *raw*: Add thumbnail support to the raw input plugin [#4887](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4887) (by Anton Dukhovnikov) (3.1.5.0)
  - *rla*: More robust to corrupted RLA files that could overrun buffers [#4624](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4624) (3.1.1.0)
  - *sgi*: Fix valid_file to properly swap bytes on little-endian platforms [#4697](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4697) (by Jesse Yurkovich) (3.1.3.0)
  - *tiff*: The default value for bitspersample should be 1 [#4670](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4670) (by vernalchen) (3.1.1.0)
  - *webp*: Respect the `oiio:UnassociatedAlpha` attribute [#4770](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4770) (by Jesse Yurkovich) (3.1.3.0)
  - *webp*: Allow finer grained control over WEBP compression settings [#4772](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4772) (by Jesse Yurkovich) (3.1.3.0)
  - *webp*: Support reading/writing the ICCProfile attribute [#4878](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4878) (by Jesse Yurkovich) (3.1.5.0)
  - *various formats*: Detect invalid ICC profile tags [#4557](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4557) [#4561](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4561) (3.1.0.0)
  - *various formats*: IPTC fields have length limits [#4568](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4568) (3.1.0.0)

### 🔧  Internals and developer goodies
  - *int*: Some LoggedTimer instances lacked a variable name [#4571](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4571) (3.1.0.0)
  - *int*: Various internal fixes to address Sonar and other warnings [#4577](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4577) (3.1.0.0)
  - *int*: No longer need OIIO_INLINE_CONSTEXPR macro [#4607](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4607) (3.1.1.0)
  - *int*: Get rid of some compiler symbols no longer needed [#4606](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4606) (3.1.1.0)
  - *int*: Switch to spans for some exif manipulation, fixing warnings [#4689](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4689) (3.1.1.0)
  - *int*: Rearrange initialize_cuda() for better err check, no warn [#4726](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4726) (3.1.3.0)
  - *int*: Experimental default_init_allocator and default_init_vector [#4677](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4677) (3.1.3.0)
  - *int*: Address some nitpick sonar warnings about TileID initialization [#4722](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4722) (3.1.3.0)
  - *int*: Address safety warnings in pvt::append_tiff_dir_entry [#4737](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4737) (3.1.3.0)
  - *int*: ImageInput/ImageOutput did not set per-file threads correctly [#4750](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4750) (3.1.3.0)
  - *int/iv*: Add raw string syntax modifier for VSCode and Cursor to understand its glsl [#4796](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4796) (by Aleksandr Motsjonov) (3.1.3.0)
  - *int/iv*: Use R"()" syntax for glsl shader strings for better readability [#4795](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4795) (by Aleksandr Motsjonov) (3.1.3.0)
  - *int*: ImageOutput::check_open logic was flawed [#4779](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4779) (3.1.3.0)
  - *int*: Switch to posix_spawnp for macOS background launch to enable PATH lookup [#4834](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4834) (by Mikael Sundell) (3.1.4.0)
  - *filesystem.h*: Filesystem::getline() [#4569](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4569) (3.1.1.0)
  - *fmath.h*: Add span-based bit_unpack() utility [#4723](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4723) (3.1.3.0)
  - *fmath.h*: Renaming and fixing of shuffle template [#4739](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4739) (3.1.3.0)
  - *paramlist.h*: ParamValue as_span, as_cspan (3.1.1.0)
  - *span.h*: span_memcpy is a safer memcpy when you know the span boundaries [#4597](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4597) (3.1.1.0)
  - *span.h*: Only allow span-from-array if array for compatible size [#4656](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4656) (3.1.1.0)
  - *span.h*: OIIO::span improvements [#4667](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4667) (3.1.1.0)
  - *span.h*: Eliminate needless definitions of std::size [#4652](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4652) (3.1.1.0)
  - *sysutil.h*: Improve the implementation of Sysutil::put_in_background on macOS [#4640](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4640) (by Mikael Sundell) (3.1.1.0)
  - *typedesc.h*: Tidying of type trait templates [#4705](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4705) (3.1.3.0)

### 🏗  Build/test/CI and platform ports:
* CMake build system and scripts:
    - *build*: Python wheel building workflow [#4428](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4428) (by zachlewis) (3.1.1.0), [#4633](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4633) (3.1.1.0), [#4820](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4820) (3.1.3.0), [#4617](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4617) (3.1.1.0), [#4668](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4668) (3.1.1.0), [#4675](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4675) (3.1.1.0), [#4855](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4855) (by Zach Lewis) (3.1.5.0), [#4867](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4867) (3.1.5.0)
    - *build*: C++23 support [#4844](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4844) (3.1.4.0)
    - *build*: Add hardening options [#4538](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4538) (3.1.0.0/3.0.1.0)
    - *build*: Use target_compile_options (fixes a LibRaw build issue) [#4556](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4556) (by Don Olmstead) (3.1.0.0)
    - *build*: Recent change broke when using non-Apple clang on Apple [#4596](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4596) (3.1.1.0)
    - *build*: Fix recently broken rpath setting [#4618](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4618) (3.1.1.0)
    - *build*: Improve OpenJpeg version detection. [#4665](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4665) (by jreichel-nvidia) (3.1.1.0)
    - *build*: Ensure python-based builds use maj.min.patch SO versioning [#4634](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4634) (by zachlewis) (3.1.1.0)
    - *build*: Better disabling of work when USE_PYTHON=0 [#4657](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4657) (3.1.1.0)
    - *build*: Bump auto-build libdeflate to 1.23 to avoid AVX512 not available errors [#4679](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4679) (by LI JI) (3.1.1.0)
    - *build*: Cmake 4.0 compatibility [#4686](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4686) [#4688](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4688) (3.1.1.0)
    - *build*: Make sure the CHANGES-2.x.md makes it into the installation [#4611](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4611) (3.1.1.0)
    - *build*: Address Robin-map vs CMake 4.0 compatibility [#4701](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4701) (3.1.3.0)
    - *build*: Fix broken OIIO_SITE customization [#4709](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4709) (3.1.3.0)
    - *build*: Restore OIIO_AVX512ER_ENABLED preprocessor symbol [#4735](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4735) (3.1.3.0)
    - *build*: Address compiler warnings [#4724](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4724) (3.1.3.0)
    - *build*: Fix furo requirement after recently breaking docs [#4743](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4743) (3.1.3.0)
    - *build*: Clean up Windows compilation warnings [#4706](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4706) (3.1.3.0)
    - *build*: Fix typo related to finding ccache [#4833](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4833) (3.1.4.0)
    - *build*: Clean up obsolete logic for old compilers [#4849](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4849) (3.1.5.0)
    - *build*: Update autobuild defaults for some dependencies [#4910](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4910) (3.1.6.1)
* Dependency and platform support:
    - *build(OCIO)*: Support static OCIO self-builds [#4517](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4517) (by zachlewis) (3.1.0.0/3.0.1.0)
    - *build(PNG)*: Add build recipe for PNG [#4423](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4423) (by zachlewis) (3.1.0.0/3.0.1.0); PNG auto-build improvements [#4835](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4835) (3.1.4.0)
    - *deps(cmake)*: Fix build_cmake.bash script for aarch64, bump its default version [#4581](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4581) (3.1.1.0)
    - *deps(dcmtk)*: Fix new dcmtk 3.6.9 vs C++ warning [#4698](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4698) (3.1.3.0)
    - *deps(ffmpeg)*: Ffmpeg 8 support [#4870](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4870) (3.1.5.0)
    - *deps(ffmpeg)*: Replace deprecated and soon removed avcodec_close with avcodec_free_context [#4837](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4837) (by Vlad Erium) (3.1.4.0)
    - *deps(fmt)*: Fix failed test with old fmt [#4758](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4758) (3.1.3.0)
    - *deps(fmt)*: Fix fmt throwing behavior warnings [#4730](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4730) (3.1.3.0)
    - *deps(freetype)*: Test freetype 2.14 and document that it works [#4876](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4876) (3.1.5.0)
    - *deps(freetype)*: Update ref image for slightly changed freetype accents [#4765](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4765) (3.1.3.0)
    - *deps(jpeg2000)*: Update jpeg2000input.cpp to include cstdarg [#4836](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4836) (by Peter Kovář) (3.1.4.0)
    - *deps(libheif)*: Add new ref output for libheif updates [#4525](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4525) (3.1.0.0/3.0.1.0)
    - *deps(libheif)*: Use get_plane2 introduced by libheif 1.20.2 [#4851](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4851) (by toge) (3.1.4.0)
    - *deps(libraw)*: Fix libraw definitions (again) [#4588](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4588) (3.1.1.0)
    - *deps(libultrahdr)*: Detect libultrahdr version and enforce minimum of 1.3 [#4729](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4729) (3.1.3.0)
    - *deps(OCIO)*: Raise OpenColorIO minimum to 2.3 (from 2.2) [#4865](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4865) (3.1.4.0)
    - *deps(openexr)*: OpenEXR 3.4 supports two compression types for HTJ2K [#4871](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4871) (by Todica Ionut) (3.1.5.0)
    - *deps(openexr)*: Several OpenEXR and OpenJPH build related fixes [#4875](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4875) (3.1.5.0)
    - *deps(openjph)*: Fix openjph target use [#4894](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4894) (3.1.5.0)
    - *deps(openvdb)*: Look for boost headers for OpenVDBs older than 12 [#4873](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4873) (by Alex Fuller) (3.1.5.0)
    - *deps(python)*: Raise minimum supported Python from 3.7 to 3.9 [#4830](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4830) (3.1.4.0)
    - *deps(opencolorio)*: Support for OpenColorIO 2.5 [#4916](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4916) (3.1.6.1)
    - *windows*: Include Windows version information on produced binaries [#4696](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4696) (by Jesse Yurkovich) (3.1.3.0)
    - *windows*: Propagate CMAKE_MSVC_RUNTIME_LIBRARY [#4842](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4842) (3.1.4.0)
    - *windows + ARM64*: Add arm_neon.h include on Windows ARM64 with clang-cl [#4691](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4691) (by Anthony Roberts)
    - *NetBSD*: Fix build on NetBSD [#4857](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4857) (by Thomas Klausner) (3.1.4.0)
    - *build*: Fix some build issues encountered on a musl libc system [#4903](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4903) (by omcaif) (3.1.6.0)
* Testing and Continuous integration (CI) systems:
    - *tests*: Improve Ptex testing [#4573](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4573) (3.1.1.0)
    - *tests*: Better testing coverage of null image reader/writer [#4578](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4578) (3.1.1.0)
    - *tests*: At long last, set up a softimage reading test (3.1.1.0)
    - *tests*: Additional ref output for jpeg-corrupt test [#4595](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4595) (3.1.1.0)
    - *ci*: Increased the macos timeout slightly to fix spurious failures [#4526](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4526) (3.1.0.0/3.0.1.0)
    - *ci*: Don't rebuild docs in CI when only CMakeLists.txt changes [#4539](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4539) (3.1.0.0/3.0.1.0)
    - *ci*: Fix broken CI for ASWF 2021 and 2022 containers [#4543](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4543) (3.1.0.0/3.0.1.0)
    - *ci*: Refactor using a single steps workflow [#4545](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4545) (3.1.0.0)
    - *ci*: Fixups of analysis workflow [#4572](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4572) (3.1.0.0)
    - *ci*: Upgrade to newer actions [#4570](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4570) (3.1.1.0)
    - *ci*: Test and document support for WebP 1.5 and fmt 11.1 [#4574](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4574) (3.1.1.0)
    - *ci*: Only pass build-steps the secrets it needs [#4576](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4576) (3.1.1.0)
    - *ci*: Fix Windows 2019 CI -- make python version match the runner [#4592](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4592) (3.1.1.0)
    - *ci*: CI testing of new dependency releases: fmt 11.1.2 [#4593](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4593), various [#4683](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4683) (3.1.1.0), pugixml versions [#4594](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4594) (3.1.1.0), pybind11 3.0.0 [#4828](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4828) (3.1.4.0), webp and openexr [#4861](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4861) (3.1.4.0)
    - *ci*: Allow special branch names to prune CI jobs [#4604](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4604) (3.1.1.0)
    - *ci*: Unbreak broken CI (mostly broken by random GH runner changes): scorecard workflow [#4605](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4605) (3.1.1.0)
    - *ci*: Switch to using ARM-native linux runners [#4616](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4616) (by zachlewis) (3.1.1.0)
    - *ci*: Add `numpy` as a runtime requirement [#4638](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4638) (by zachlewis) (3.1.1.0)
    - *ci*: Move away from soon-to-be-deprecated ubuntu-20.04 GHA runner [#4636](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4636) (3.1.1.0)
    - *ci*: For docs workflow, lock down versions and speed up [#4646](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4646) (3.1.1.0)
    - *ci*: Improved clang-format CI task [#4647](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4647) (3.1.1.0)
    - *ci*: Add numpy as a runtime requirement [#4654](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4654) (by zachlewis) (3.1.1.0)
    - *ci*: Update libPNG address and version for ci & autobuild [#4659](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4659) (3.1.1.0)
    - *ci*: Step naming adjustments [#4658](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4658) (3.1.1.0)
    - *ci*: Save time by not checking out entire project history [#4731](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4731) (3.1.3.0)
    - *ci*: New variants for VFXP 2025, Windows 2025 [#4744](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4744) (3.1.3.0)
    - *ci*: Bump abi_check standard after ImageOutput changes [#4747](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4747) (3.1.3.0)
    - *ci*: Upload fixed python stubs as an artifact in CI  [#4754](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4754) (by Chad Dombrova) (3.1.3.0)
    - *ci*: Update ref output to compensate for GitHub windows drive changes [#4761](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4761) (3.1.3.0)
    - *ci*: Pkg config libdir fix [#4775](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4775) (by Scott Wilson) (3.1.3.0)
    - *ci*: Add facility for benchmarking as part of CI [#4745](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4745) (3.1.3.0)
    - *ci*: Add Linux ARM test [#4749](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4749) (3.1.3.0)
    - *ci*: Update linux arm clang reference output [#4782](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4782) (3.1.3.0)
    - *ci*: For python stub generation, lock pybind11 to pre-3.0 [#4831](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4831) (3.1.4.0)
    - *ci*: Add a VFX Platform 2026 CI job [#4856](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4856) (3.1.4.0)
    - *ci*: Lock down to ci-oiio container with correct llvm components [#4859](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4859) (3.1.4.0)
    - *ci*: Try to fix Sonar workflow by switching to compile-commands for sonar [#4879](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4879) (by vvalderrv) (3.1.5.0)
    - *ci*: Fix analysis workflow configuration [#4881](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4881) (3.1.5.0)
    - *ci*: Better spread of libpng versions we test against [#4883](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4883) (3.1.5.0)
    - *ci*: More Sonar scan workflow fixes [#4902](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4902) (by vvalderrv) (3.1.6.0)
    - *ci*: Add more exceptions to when we test docs building [#4899](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4899) (3.1.6.0)
    - *ci*: Require all dependencies, with explicit exceptions [#4898](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4898) (3.1.6.0)

### 📚  Notable documentation changes:
  - *docs*: Clarify 'copy_image' example [#4522](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4522) (3.1.0.0/3.0.1.0)
  - *docs*: Update some old links to our new vanity URLs [#4533](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4533) (3.1.0.0/3.0.1.0)
  - *docs*: Quickstart guide [#4531](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4531) (3.1.0.0/3.0.1.0)
  - *docs*: First stab at an archiecture overview [#4530](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4530) (3.1.0.0/3.0.1.0)
  - *docs*: Fix typo in oiiotool's gradient fill argument [#4589](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4589) (by Loïc Vital) (3.1.1.0)
  - *docs*: Argparse documentation/comments typos [#4612](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4612) (3.1.1.0)
  - *docs*: Correct the type for BMP x/y density [#4695](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4695) (by Campbell Barton) (3.1.3.0)
  - *docs*: Specify the units for DPX scanned size [#4694](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4694) (by Campbell Barton) (3.1.3.0)
  - *docs*: INSTALL.md reset updated minimum dependencies from 3.0 [#4700](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4700) (3.1.3.0)
  - *docs*: Online docs improvements, mostly formatting [#4736](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4736) (3.1.3.0)
  - *docs*: Update Windows build instructions to rely on deps auto-build [#4769](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4769) (3.1.3.0)
  - *docs*: Correct docs and type of "resident_memory_used_MB" attribute [#4824](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4824) (3.1.4.0)
  - *docs/python*: Add type hints to Python docs and tests [#4908](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4908) (by Connie Chang) (3.1.6.0)

### 🏢  Project Administration
  - *admin*: Code review guidelines and tips [#4532](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4532) (3.1.0.0/3.0.1.0)
  - *admin*: Document how to make signed release tags [#4529](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4529) (3.1.0.0/3.0.1.0)
  - *admin*: Sign release artifacts [#4580](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4580) (3.1.0.0)
  - *admin*: Document Python Wheel completed in roadmap [#4620](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4620) (by Todica Ionut) (3.1.1.0)
  - *admin*: Add ".vs" to .gitignore [#4645](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4645) (3.1.1.0)
  - *admin*: Set up .gitattributes file [#4648](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4648) (3.1.1.0)
  - *admin*: Update SECURITY to reflect that 2.5 only gets critical fixes now [#4829](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4829)
  - *admin*: Adjust license notices of A2-only source [#4884](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4884) (3.1.5.0)

### :handshake: Contributors

During the course of development of 3.1 (since splitting from the 3.0 branch),
OpenImageIO has had 40 unique contributors, of which 15 (indicated by an
asterisk) had not previously contributed to the project.

|                         |                     |                      |
| ----------------------- | ------------------- | -------------------- |
| Aleksandr Motsjonov (*) | Alex Fuller (*)     | Anthony Roberts (*)  |
| Anton Dukhovnikov       | Aras Pranckevičius  | Basile Fraboni       |
| Brecht Van Lommel       | Campbell Barton (*) | Carine Touraille (*) |
| Chad Dombrova           | Connie Chang (*)    | Danielle Imogu (*)   |
| Dharshan Vishwanatha    | Don Olmstead (*)    | Jesse Yurkovich      |
| Joachim Reichel         | Jonathan Brown      | kaarrot              |
| Larry Gritz             | LI JI (*)           | Loïc Vital           |
| Lukas Stockner          | Lydia Zheng         | Mikael Sundell       |
| Oktay Comu (*)          | omcaif (*)          | Peter Kovář          |
| Peter Horvath           | pfranz              | Rui Chen (*)         |
| Sam Richards            | Scott Milner (*)    | Scott Wilson         |
| Thomas Klausner (*)     | Todica Ionut        | toge                 |
| Vanessa Valderrama (*)  | Vernal Chen         | Vlad (Kuzmin) Erium  |
| Zach Lewis              |                     |                      |


---
---


Release 3.0.14.0 (Jan 1, 2026) -- compared to 3.0.13.0
-------------------------------------------------------
  - *fix(IBA)*: IBA::compare_Yee() accessed the wrong channel [#4976](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4976) (by Pavan Madduri)
  - *ci*: Test against libraw 0.21.5 [#4988](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4988)
  - *ci*: Address tight disk space on GHA runners [#4974](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4974)


Release 3.0.13.0 (Dec 1, 2025) -- compared to 3.0.12.0
-------------------------------------------------------
  - *exif*: Support EXIF 3.0 tags [#4961](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4961)
  - *build*: Disable LERC in libTIFF local build script [#4957](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4957) (by LI JI)
  - *ci*: Fix broken ci, debug and static cases, bump some latest [#4954](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4954)
  - *ci*: Unbreak icc/icx CI [#4958](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4958)


Release 3.0.12.0 (Nov 1, 2025) -- compared to 3.0.11.0
-------------------------------------------------------
  - *iff*: Handle non-zero origin, protect against buffer overflows [#4925](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4925)
  - *jpeg-xl*: Correctly set Quality for JPEG XL [#4933](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4933)
  - *win*: Address Windows crashes from issue 4641 [#4914](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4914)
  - *fix*: Uninitialized value revealed by clang-21 warning [#4940](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4940)
  - *ci*: For python wheel generation, use ccache [#4924](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4924)
  - *ci*: Drop deprecated macos-13 (intel) platform, add macos-15-intel [#4930](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4930)
  - *ci*: We were not correctly setting fmt version from job options [#4939](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4939)


Release 3.0.11.0 (Oct 1, 2025) -- compared to 3.0.10.1
-------------------------------------------------------
  - *oiiotool*: Allow easy splitting output of subimages by name [#4874](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4874)
  - *webp*: Support reading/writing the ICCProfile attribute for WepP files[#4878](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4878) (by Jesse Yurkovich)
  - *gif*: GIF output didn't handle FramesPerSecond attribute correctly [#4890](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4890)
  - *deps*: Test freetype 2.14 and document that it works [#4876](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4876)
  - *deps*: Look for boost headers for OpenVDBs older than 12 [#4873](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4873) (by Alex Fuller)
  - *deps*: Support for OpenColorIO 2.5 [#4916](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4916)


Release 3.0.10.1 (Sep 16, 2025) -- compared to 3.0.10.0
-------------------------------------------------------
  - *ci*: Fix broken python wheel building [#4886](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4886) [#4855](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4855) (by Zach Lewis)
  - *deps*: Several fixes to build against OpenEXR 3.4 and OpenJPH build related fixes [#4875](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4875)


Release 3.0.10.0 (Sep 1, 2025) -- compared to 3.0.9.0
-------------------------------------------------------
  - *exr*: Support for OpenEXR 3.4's new compression types for HTJ2K [#4871](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4871) (by Todica Ionut)
  - *deps*: Ffmpeg 8 support [#4870](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4870)
  - *ci*: Add a VFX Platform 2026 CI job [#4856](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4856)
  - *ci*: Bump webp and openexr for "latest versions" test [#4861](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4861)


Release 3.0.9.1 (Aug 7, 2025) -- compared to 3.0.9.0
-----------------------------------------------------
  - *deps*: C++23 support [#4844](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4844)
  - *deps*: Adapt to libheif 1.20.2 [#4851](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4851) (by toge)


Release 3.0.9.0 (Aug 1, 2025) -- compared to 3.0.8.1
-----------------------------------------------------
  - *maketx*: Add flags to increase feature parity with txmake [#4841](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4841) (by Scott Milner)
  - *perf*: Speed up `maketx --envlatl` when multithreaded by over 10x. [#4825](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4825)
  - *perf*: Speed up OpenEXR non-core header read time [#4832](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4832)
  - *oiiotool*: Allow thread control for --parallel-frames [#4818](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4818)
  - *ImageInput*: Incorrect IOProxy logic related to valid_file [#4839](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4839)
  - *python*: Got strides wrong passing 2D numpy pixel array [#4843](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4843)
  - *ffmpeg*: Replace deprecated and soon removed avcodec_close with avcodec_free_context [#4837](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4837) (by Vlad Erium)
  - *build/python*: For python stub generation, lock pybind11 to pre-3.0 [#4831](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4831)
  - *build*: Fix typo related to finding ccache [#4833](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4833)
  - *build*: PNG auto-build improvements [#4835](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4835)
  - *build*: Propagate CMAKE_MSVC_RUNTIME_LIBRARY [#4842](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4842)
  - *build*: Update jpeg2000input.cpp to include cstdarg [#4836](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4836) (by Peter Kovář)
  - *ci*: Bump 'latest releases' tests to use pybind11 3.0.0 [#4828](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4828)
  - *docs*: Correct docs and type of "resident_memory_used_MB" attribute [#4824](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4824)
  - *admin*: Update SECURITY to reflect that 2.5 only gets critical fixes now [#4829](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4829)


Release 3.0.8.1 (Jul 5, 2025) -- compared to 3.0.8.0
-----------------------------------------------------
  - *build(heif)*: Fixes to build against libheif 1.20 [#4822](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4822) (by Rui Chen)
  - *build*: Wheel upload_pypi step should only run from main repo [#4820](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4820)
  - *ci*: Bump 'latest' test versions [#4819](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4819)


Release 3.0.8.0 (Jul 1, 2025) -- compared to 3.0.7.0
-----------------------------------------------------
  - *oiiotool*: New expression pseudo-metadata term: SUBIMAGES [#4804](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4804)
  - *oiiotool*: Fixes to --missingfile behavior [#4803](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4803)
  - *iv*: Area probe [#4767](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4767) (by Danielle Imogu)
  - *python*: Python ImageBuf.init_spec did not return correct value [#4805](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4805)
  - *fix*: ImageOutput::check_open logic was flawed [#4779](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4779)
  - *int(iv)*: Add raw string syntax modifier for VSCode and Cursor to understand its glsl [#4796](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4796) (by Aleksandr Motsjonov)
  - *int(iv)*: Use R"()" syntax for glsl shader strings for better readability [#4795](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4795) (by Aleksandr Motsjonov)
  - *exr*: Not honoring 'missingcolor' for scanline files [#4757](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4757)
  - *build*: Add arm_neon.h include on Windows ARM64 with clang-cl [#4691](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4691) (by Anthony Roberts)
  - *build*: Adjust pystring finding [#4816](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4816)
  - *build(jxl)*: Use correct cmake variables for the include directories [#4810](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4810) [#4813](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4813)  (by Jesse Yurkovich)
  - *tests*: Remove old test reference output we no longer need [#4817](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4817)
  - *ci*: Remove tests on Windows-2019 GitHub runner [#4793](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4793)
  - *ci*: Various ccache save/restore improvements for CI runs [#4797](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4797)
  - *ci*: Simplify gh-win-installdeps, no more vcpkg [#4809](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4809)
  - *admin*: Remove stale intake documents [#4815](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4815)


Release 3.0.7.0 (Jun 1, 2025) -- compared to 3.0.6.1
-----------------------------------------------------
  - *oiiotool*: `--eraseattrib` new modifier `:fromfile=1` reads from a file
    to get a list of patterns to specify the attributes to erase. [#4763](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4763) (by Lydia Zheng)
  - *oiiotool*: Added `--create-dir` flag to create directories needed by `-o`
    if they doesn't already exist [#4762](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4762) (by Dharshan Vishwanatha)
  - *oiiotool*: -i:native=1, fix --native behavior, fix convert datatype [#4708](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4708)
  - *iv*: Fix crash on .DS_Store; fix uppercase extensions [#4764](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4764) (by Anton Dukhovnikov)
  - *iv*: Do not resize on open and other zoom fixes [#4766](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4766) (by Aleksandr Motsjonov)
  - *iv*: Bug fix for iv window losing focus on mac on startup [#4773](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4773) (by Aleksandr Motsjonov)
  - *iv*: Implement files drag and drop into an iv window [#4774](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4774) (by Aleksandr Motsjonov)
  - *iv*: Use screen pixel ratio to render sharp text in pixel view tool [#4768](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4768) (by Aleksandr Motsjonov)
  - *python*: Add python stub files [#4692](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4692) [#4754](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4754) (by Chad Dombrova)
  - *python*: ImageBuf `_repr_png_` method added, which allows use of
    ImageBuf in [Jupyter Notebooks](https://jupyter.org/) as a displayable object. [#4753](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4753) (by Oktay Comu)
  - *exr*: Did not properly allocate 'missingcolor' vector [#4751](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4751)
  - *exr*: Add `htj2k` as a compression option for OpenEXR. Only works with  OpenEXR 3.4 or higher (or in-progress OpenEXR main). [#4785](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4785) (by Li Ji)
  - *iff*: Improved IFF support reading and writing z buffers [#4673](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4673) (by Mikael Sundell)
  - *webp*: Respect the `oiio:UnassociatedAlpha` attribute [#4770](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4770) (by Jesse Yurkovich)
  - *webp*: Allow finer grained control over WEBP compression settings [#4772](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4772) (by Jesse Yurkovich)
  - *flx/python*: IBA.demosaic had GIL release in wrong spot [#4777](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4777)
  - *fix*: ImageInput/ImageOutput did not set per-file threads correctly [#4750](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4750)
  - *fix*: Address safety warnings in pvt::append_tiff_dir_entry [#4737](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4737)
  - *build*: Fix fmt throwing behavior warnings [#4730](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4730)
  - *build*: Detect libultrahdr version and enforce minimum of 1.3 [#4729](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4729)
  - *build*: Fix failed test with old fmt [#4758](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4758)
  - *ci*: Save time by not checking out entire project history [#4731](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4731)
  - *ci*: New testing variants for VFX Platform 2025, Windows 2025 [#4744](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4744), Linux ARM [#4749](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4749)
  - *ci*: Update ref output to compensate for GitHub windows drive changes [#4761](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4761)
  - *ci*: Pkg config libdir fix [#4775](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4775) (by Scott Wilson)
  - *ci*: For docs workflow, lock down versions and speed up [#4646](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4646)
  - *ci*: Improved clang-format CI task [#4647](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4647)
  - *ci*: Add facility for benchmarking as part of CI [#4745](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4745)
  - *ci*: Update ref image for slightly changed freetype accents [#4765](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4765)
  - *docs*: Online docs improvements, mostly formatting (#4736, #4743)
  - *docs*: Update Windows build instructions to rely on deps auto-build [#4769](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4769)


Release 3.0.6.1 (May 2, 2025) -- compared to 3.0.6.0
-----------------------------------------------------
  - *fix*: Restore OIIO_AVX512ER_ENABLED preprocessor symbol. Its absence could break backwards source compatibility if anyone was using it, even though it was useless and broken. Where compatibility goes, better safe than sorry. [#4735](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4735)


Release 3.0.6.0 (May 1, 2025) -- compared to 3.0.5.0
-----------------------------------------------------
  - *oiiotool*: Add `--render_text` modifiers `measure=` and `render=` [#4681](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4681)
  - *python*: Fix handle leak [#4685](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4685)
  - *bmp*: Correct the type for BMP x/y density [#4695](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4695) (by Campbell Barton)
  - *dpx*: Specify the units for DPX scanned size [#4694](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4694) (by Campbell Barton)
  - *sgi*: Fix valid_file to properly swap bytes on little-endian platforms [#4697](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4697) (by Jesse Yurkovich)
  - *build*: Fix new dcmtk 3.6.9 vs C++ warning [#4698](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4698)
  - *build*: Address Robin-map vs CMake 4.0 compatibility [#4701](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4701)
  - *build*: Fix broken OIIO_SITE customization [#4709](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4709)
  - *build*: Address compiler warnings in simd.h [#4724](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4724)
  - *build/windows*: Clean up Windows compilation warnings [#4706](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4706)
  - *build/windows*: Include Windows version information on produced binaries [#4696](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4696) (by Jesse Yurkovich)
  - *ci*: Move away from soon-to-be-deprecated ubuntu-20.04 GHA runner [#4636](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4636)


Release 3.0.5.0 (Apr 2, 2025) -- compared to 3.0.4.0
-----------------------------------------------------
  - *ImageBuf*: `ImageBuf::merge_metadata()` merges one IB's metadata into another's without deleting the metadata already present. It can also filter which metadata are copied using a regex. [#4672](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4672)
  - *oiiotool*: `--pastemeta` now takes additional modifiers that allows options for merging rather than rewriting, and is able to copy only a subset of the metadata specified by a regex. [#4672](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4672) [#4674](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4674) [#4676](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4676)
  - *oiiotool*: Fix invalid loop bound when appending mipmap textures using oiiotool [#4671](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4671) (by Basile Fraboni)
  - *png*: Increase allowed width/height limit [#4655](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4655) (by Jesse Yurkovich)
  - *psd*: Improved tag recognition in psd files [#4663](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4663) (by Lydia Zheng)
  - *tiff*: The default value for bitspersample should be 1 [#4670](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4670) (by vernalchen)
  - *int*: Switch to spans for some exif manipulation, fixing warnings [#4689](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4689)
  - *span.h*: OIIO::span improvements [#4667](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4667)
  - *build*: Better disabling of work when USE_PYTHON=0 [#4657](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4657)
  - *build*: Improve OpenJpeg version detection. [#4665](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4665) (by jreichel-nvidia)
  - *build*: Bump auto-build libdeflate to 1.23 to avoid AVX512 not available errors [#4679](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4679) (by LI JI)
  - *build*: Cmake 4.0 compatibility [#4686](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4686) [#4688](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4688)
  - *ci*: Fix wheel building on Mac [#4668](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4668) [#4675](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4675)
  - *ci*: Update libPNG address and version for ci & autobuild [#4659](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4659)


Release 3.0.3.1 (Feb 1, 2025) -- compared to 3.0.3.1
-----------------------------------------------------
The code is identical to v3.0.3.0, but some build issues were fixed to allow
proper build and upload of the Python wheels to PyPI for the Linux ARM
variants.

Release 3.0.3.0 (Feb 1, 2025) -- compared to 3.0.2.0
-----------------------------------------------------
  - 🐍🎉🍾 **Python wheels workflow and build backend** -- beginning with
    OpenImageIO 3.0.3.0, releases should automatically trigger building of
    Python wheels and upload to PyPI so that you can do a single-command `pip3
    install openimageio` on any of Linux, Mac, or Windows, and get the Python
    bindings, the OpenImageIO libraries, and a working oiiotool, without
    having to build from source yourself.
    [#4428](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4428) (by zachlewis)
  - *oiiotool*: `oiiotool --layersplit`, new command to split multi-layer
    OpenEXR files (using the usual channel naming convention to delineate
    "layers") into separate images [#4591](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4591) (by Loïc Vital)
  - *IBA*: IBA:demosaic() adds the ability for X-Trans demosaicing [#4579](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4579) (by Anton Dukhovnikov)
  - *IBA*: fix demosaic handling of roi channels [#4602](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4602) (by Anton Dukhovnikov)
  - *png*: Alpha premultiplication adjustment and attribute [#4585](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4585)
  - *psd*: Perform endian byteswap on correct buffer area for PSD RLE [#4600](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4600) (by Jesse Yurkovich)
  - *raw*: Add black level and BPS metadata [#4601](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4601) (by Anton Dukhovnikov)
  - *python*: Disable loading Python DLLs from PATH by default on Windows [#4590](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4590) (by zachlewis)
  - *dev (span.h)*: Span_memcpy is a safer memcpy when you know the span boundaries [#4597](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4597)
  - *dev (filesystem.h)*: Filesystem::getline() [#4569](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4569)
  - *dev (paramlist.h)*: ParamValue as_span, as_cspan [#4582](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4582)
  - *build*: Recent change broke when using non-Apple clang on Apple [#4596](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4596)
  - *build*: Fix build_cmake.bash script for aarch64, bump its default version [#4581](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4581)
  - *build*: Fix libraw definitions (again) [#4588](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4588)
  - *ci*: Upgrade to newer actions [#4570](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4570)
  - *ci*: Test and document support for WebP 1.5 and fmt 11.1 [#4574](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4574)
  - *ci*: Only pass build-steps the secrets it needs [#4576](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4576)
  - *ci*: Fix Windows 2019 CI -- make python version match the runner [#4592](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4592)
  - *ci*: Raise 'latest' tests to use new fmt 11.1.2 [#4593](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4593)
  - *ci*: Adjust some pugixml versions [#4594](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4594)
  - *ci*: Allow special branch names to prune CI jobs [#4604](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4604)
  - *tests*: Improve Ptex testing [#4573](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4573)
  - *tests*: Better testing coverage of null image reader/writer [#4578](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4578)
  - *tests*: At long last, set up a softimage reading test. [#4583](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4583)
  - *tests*: Additional ref output for jpeg-corrupt test [#4595](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4595)
  - *docs*: Fix typo in oiiotool's gradient fill example [#4589](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4589) (by Loïc Vital)


Release 3.0.4.0 (Mar 2, 2025) -- compared to 3.0.3.0
-----------------------------------------------------
  - *oiiotool*: Oiiotool new expression eval tokens IS_CONSTANT, IS_BLACK [#4610](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4610) (by Lydia Zheng)
  - *oiiotool*: Better handling of wildcards that match no files [#4627](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4627)
  - *ImageBuf*: IB::pixeltype() did not always return the right value [#4614](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4614)
  - *ImageBuf*: Fix bug in ImageBuf construction from ptr + neg strides [#4630](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4630)
  - *ICC*: ICC profile recognition and robustness improvements, especially for PSD [#4644](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4644)
  - *exr*: Fill in OpenEXR lineOrder attribute when reading [#4628](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4628) (by vernalchen)
  - *ico*: More robust to corrupted ICO files [#4625](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4625)
  - *rla*: More robust to corrupted RLA files that could overrun buffers [#4624](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4624)
  - *span.h*: Eliminate needless definitions of `std::size(span)` that were triggering strange behavior on recent MSVS compiler versions. [#4652](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4652)
  - *build*: Fix recently broken rpath setting [#4618](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4618)
  - *build/python wheels*: Ensure python-based builds use maj.min.patch SO versioning [#4634](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4634) (by zachlewis)
  - *build/python wheels*: Fix recently broken rpath to restore python wheel building [#4633](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4633)
  - *ci*: Run wheel workflow on certain pushes [#4617](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4617)
  - *docs*: Argparse documentation/comments typos [#4612](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4612)
  - *admin*: Document Python Wheel completed in roadmap [#4620](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4620) (by Todica Ionut)
  - *admin*: Add ".vs" to .gitignore [#4645](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4645)
  - *admin*: Set up .gitattributes file and ensure it properly categorizes certain files for GitHub's language analysis statistics. [#4648](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4648)



Release 3.0.2.0 (Jan 1, 2025) -- compared to 3.0.1.0
-----------------------------------------------------
- *api*: Add global attribute `imageinput:strict` [#4560](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4560)
- *various formats*: Detect invalid ICC profile tags [#4557](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4557) [#4565](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4565)
- *various formats*: IPTC fields have length limits, protect against attributes passed that are too long to fit in them. [#4568](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4568)
- *pnm*: Handle broken pnm files with invalid resolution [#4561](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4561)
- *pnm*: Handle broken pgm having memory access error [#4559](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4559)
- *int*: Some LoggedTimer instances lacked a variable name [#4571](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4571)
- *build*: Use target_compile_options (fixes a LibRaw build issue) [#4556](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4556) (by Don Olmstead)
- *ci*: Refactor using a single steps workflow [#4545](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4545)
- *ci*: Fixups of analysis workflow [#4572](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4572)
- *docs*: Minor fixes and typos [#4564](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4564)
- *admin*: Sign release artifacts [#4580](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4580)


Release 3.0.1.0 (Dec 1, 2024) -- compared to 3.0.0.3
-----------------------------------------------------
- *IBA*: New IBA::scale() [#4541](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4541) (by Anton Dukhovnikov) (3.0.1.0)
- *IBA*: `IBA:demosaic()` add white balancing [#4499](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4499) (by Anton Dukhovnikov) (3.0.1.0)
- *jpeg*: Support reading Ultra HDR images [#4484](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4484) (by Loïc Vital) (3.0.1.0)
- *jpeg*: Support encoding/decoding arbitrary metadata as comments [#4430](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4430) (by Lukas Stockner) (3.0.1.0)
- *api*: `OIIO::getattribute()` queries for available font families and styles [#4523](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4523) (by peterhorvath111) (3.0.1.0)
- *perf*: `IBA::unsharp_mask()` speed and memory optimization [#4513](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4513) (by Vlad (Kuzmin) Erium) (3.0.1.0)
- *perf*: oiiotool `--line`, `--text`, `--point`, and `--box` speedups [#4518](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4518) (3.0.1.0)
- *perf*: Jpeg2000 valid_file implementation, much faster than trying to open [#4548](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4548) (by Aras Pranckevičius) (3.0.1.0)
- *perf*: Faster utf8<->utf16 conversion on Windows [#4549](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4549) (by Aras Pranckevičius) (3.0.1.0)
- *fix(exr)*: Allow an empty "name" metadata to be read [#4528](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4528) [#4536](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4536) (3.0.1.0)
- *fix(exr)*: Avoid integer overflow for large deep exr slice strides [#4542](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4542) (3.0.1.0)
- *fix(raw)*: Fix channel layout [#4516](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4516) (by Anton Dukhovnikov) (3.0.1.0)
- *build*: Support static OCIO self-builds [#4517](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4517) (by zachlewis) (3.0.1.0)
- *build*: Add build recipe for PNG [#4423](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4423) (by zachlewis) (3.0.1.0)
- *build*: Add hardening options [#4538](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4538) (3.0.1.0)
- *ci*: Increased the macos timeout slightly to fix spurious failures [#4526](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4526) (3.0.1.0)
- *ci*: Don't rebuild docs in CI when only CMakeLists.txt changes [#4539](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4539) (3.0.1.0)
- *ci*: Fix broken CI for ASWF 2021 and 2022 containers [#4543](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4543) (3.0.1.0)
- *docs*: Update some old links to our new vanity URLs [#4533](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4533) (3.0.1.0)
- *docs*: Quickstart guide [#4531](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4531) (3.0.1.0)
- *docs*: First stab at an architecture overview [#4530](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4530) (3.0.1.0)
- *docs/admin*: Code review guidelines and tips [#4532](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4532) (3.0.1.0)
- *docs/admin*: Document how to make signed release tags [#4529](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4529) (3.0.1.0)



Release 3.0 (v3.0.0.3 - Nov 8, 2024) -- compared to 2.5.16.0
-------------------------------------------------------------
- v3.0.0.0-beta1 - Oct 15, 2024
- v3.0.0.1-beta2 - Oct 29, 2024
- v3.0.0.2-RC1  - Nov 4, 2024 (no code changes vs beta2)
- v3.0.0.3 / official release - Nov 8, 2024 (no code changes vs RC1)

**Executive Summary / Highlights:**

- Updated minimum toolchain: C++17/gcc9.3, Python 3.7, CMake 3.18.2, and
  raised min versions of most library dependencies.
- New image format support: JPEG XL, R3D.
- oiiotool new commands: `--cryptomatte-colors`, `--demosaic`, `--buildinfo`,
  `--ocionamedtransform`, `--popbottom`, `--stackreverse`, `--stackclear`,
  `--stackextract`; improved `--for` behavior for reverse direction.
- Lots of long-deprecated API calls have been removed entirely.
  Please see [the detailed deprecation list](docs/Deprecations-3.0.md).
- New ImageBufAlgo: `perpixel_op()`, `demosaic()`, `ocionamedtransform()`.
- ImageBuf now by default does not use ImageCache to mediate file images,
  unless you explicitly ask for it.
- ImageCache & TextureSystem now use shared_ptr for creation, not raw
  pointers. And they have been de-virtualized, for easier future expansion
  flexibility without breaking ABI for any small change.
- Improved and more consistent color space name nomenclature.
- Build system now is capable of auto-downloading and building several
  of the most important dependencies if they are missing at build time.
- Please note that the development branch in the GitHub repo is now named
  `main` instead of `master`.

Full details of all changes follow.

### New minimum dependencies and compatibility changes:
* *C++*: Move to C++17 standard minimum (from 14), which also implies a
  minimum gcc 9.3 (raised from 6.3), clang 5 (though we don't test or support
  older than clang10), Intel icc 19+, Intel OneAPI C++ compiler 2022+. [#4199](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4199) (2.6.2.0)
* *Python*: 3.7 minimum (from 2.7). [#4200](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4200) (2.6.2.0)
* *CMake*: 3.18.2 minimum (from 3.15) [#4472](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4472) (3.0.0)
* *Boost*: Is no longer a dependency! [#4191](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4191) (by Christopher Kulla) [#4221](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4221) (by Christopher Kulla) [#4222](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4222) [#4233](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4233) (2.6.2.0)
* *ffmpeg*: 4.0 minimum (from 3.0) [#4352](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4352) (2.6.3.0)
* *Freetype*: 2.10 minimum (from no previously stated minimum, but we had been testing as far back as 2.8) [#4283](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4283) (2.6.2.0)
* *GIF*: 5.0 minimum for giflib (from 4.0) [#4349](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4349) (2.6.3.0)
* *libheif*: 1.11 minimum (from 1.3) [#4380](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4380) (2.6.3.0)
* *LibRaw*: Raise minimum LibRaw to 0.20 (from 0.18) [#4217](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4217) (2.6.2.0)
* *libtiff*: 4.0 minimum (from 3.9) [#4296](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4296) (2.6.2.0)
* *OpenColorIO*: Make OpenColorIO a required dependency and raise the minimum to 2.2 (from 1.1). [#4367](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4367) (2.6.3.0)
* *OpenEXR/Imath*: minimum raised to 3.1 (from 2.4) [#4223](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4223) (2.6.2.0)
* *OpenCV*: 4.0 minimum (from 3.x) [#4353](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4353) (2.6.3.0)
* *OpenVDB*: Raise OpenVDB minimum to 9.0 [#4218](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4218) (2.6.2.0)
* *PNG*: 1.6.0 minimum for libPNG (from 1.5.13) [#4355](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4355) (2.6.3.0)
* *Pybind11*: 2.7 minimum [#4297](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4297) (2.6.2.0)
* *Robin-map*: 1.2.0 minimum [#4287](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4287) (2.6.2.0)
* *WebP*: 1.1 minimum (from 0.6.1) [#4354](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4354) (2.6.3.0)

### ⛰️  New features and public API changes:

* *New image file format support:*
    - *JPEG XL*: Initial JPEG XL support for image input/output [#4055](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4055) (by Peter Kovář) [#4252](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4252) (by Vlad (Kuzmin) Erium) (2.6.2.0) [#4310](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4310) (by Vlad (Kuzmin) Erium) (2.6.3.0) 
    - *R3D*: Add initial support to read R3D files. Note that this capability will only be enabled if OIIO is built with the R3D SDK installed and available to be found by the build system. [#4216](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4216) (by Peter Kovář) (2.6.2.0)
* *oiiotool new features and major improvements*:
    - `--cryptomatte-colors` takes the name of a cryptomatte set of channels, and produces a color-coded matte in which each ID gets a distinct color in the image. This can be useful for visualizing the matte, among other things. [#4093](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4093) (2.6.0.2)
    - `--demosaic` takes 1-channel Bayer patterns and turn them into
      demosaiced 3-channel images [#4366](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4366) (by Anton Dukhovnikov) (2.6.3.0) [#4419](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4419) (by Anton Dukhovnikov) (2.6.6.0)
    - `--buildinfo` command prints build information, including
      version, compiler, and all library dependencies. [#4124](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4124) (2.6.0.3) [#4150](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4150) (2.6.0.3)
    - `--ocionamedtransform`: Implement support for OCIO NamedTransforms [#4393](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4393) (by zachlewis) (2.6.3.0)
    - Several new stack manipulation commands: `--popbottom` discards the bottom
      element of the stack, `--stackreverse` reverses the order of the whole stack,
     `--stackclear` fully empties the stack, `--stackextract <index>` moves the
     indexed item from the stack (index 0 means the top) to the top. [#4348](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4348) (2.6.3.0)
    - `--for` improvements: correct reverse iteration behavior if the step value
      is negative, or if there is no step value but the start value is greater than
      the end value. (https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4348) (2.6.3.0)
    - Expression evaluation improvements: `BOTTOM` refers to the image on the bottom of the stack, `IMG[expression]` is now supported (previously only numeric literals were accepted as the index), check that label/variable names [#4334](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4334) (2.6.3.0)
    - oiiotool now by default does immediate reads without relying on an
      ImageCache, unless the `--cache` option is used, which now both enables
      the use of an underlying IC as well as setting its size. This tends to
      improve performance.
      [#3986](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3986) (2.6.0.1, 2.5.3.1)
    - Change command line embedding for oiiotool & maketx output, by default hiding the command line for security reasons. It can be re-enabled with `--history`. [#4237](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4237) (2.6.2.0)
* *Command line utilities*:
    - *idiff*: Allow users to specify a directory as the 2nd argument [#4015](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4015) (by David Aguilar) (2.6.0.1)
    - *iv*: Implement Directory Argument Loading for iv [#4010](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4010) (by Chaitanya Sharma) (2.6.0.1)
    - *iv*: Split off the current image in iv into a separate window [#4017](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4017) (by Anton Dukhovnikov) (2.6.0.1)
    - *iv*: OCIO color managed display [#4031](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4031) (by Anton Dukhovnikov) (2.6.0.2)
    - *iv*: Iv shows constant brown and GL error messages on start-up. [#4451](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4451) (by David Adler) (2.6.6.0)
    - *iv*: Initialize variables before we use them. [#4457](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4457) (by Bram Stolk) (2.6.6.0)
    - *iv*: Add iv data and display windows overlay feature [#4443](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4443) (by Andy Chan) (2.6.6.0)
* New global attribute queries via OIIO::getattribute():
    - "build:platform", "build:compiler", "build:dependencies" [#4124](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4124) (2.6.0.3)
    - "build:simd" is the new preferred synonym for the old name "oiio:simd" [#4124](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4124) (2.6.0.3)
* *ImageBuf/ImageBufAlgo*:
    - ImageBuf now has span-based constructors for the variety where it
      "wraps" a user buffer. This is preferred over the constructor that
      takes a raw pointer (which is considered deprecated). [#4401](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4401) (2.6.6.0)
    - New span-based versions of get_pixels, set_pixels, setpixel, getpixel,
      interppixel, interppixel_NDC, interppixel_bicubic,
      interppixel_bicubic_NDC. These are preferred over the old versions that
      took raw pointers. [#4426](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4426) (2.6.6.0)
    - Start using optional keyword/value params for some ImageBufAlgo functions. [#4149](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4149)
    - Only back ImageBuf with ImageCache when passed an IC
      [#3986](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3986) (2.6.0.1, 2.5.3.1)
    - Make ImageBuf::Iterator lazy in its making the image writable [#4033](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4033) (2.6.0.2)
    - `IBA::perpixel_op()` is a new way to write IBA-like functions very
      simply, only supplying the very inner part of the loop that operates on
      one pixel. [#4299](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4299) (2.6.3.0) [#4409](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4409) (2.6.6.0)
    - `IBA::demosaic()` takes 1-channel Bayer patterns and turn them into
       demosaiced 3-channel images [#4366](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4366) (by Anton Dukhovnikov) (2.6.3.0)
    - `IBA::ocionamedtransform()`: Implement support for OCIO NamedTransforms [#4393](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4393) (by zachlewis) (2.6.3.0)
* *ImageInput / ImageOutput*:
    - Add virtual `heapsize()` and `footprint()` to ImageInput and ImageOutput [#4323](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4323) (by Basile Fraboni) (2.6.3.0)
* *ImageCache/TextureSystem*:
    - Use `shared_ptr` for ImageCache and TextureSystem creation [#4377](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4377) (2.6.3.0)
    - Overload decode_wrapmode to support ustringhash [#4207](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4207) (by Chris Hellmuth) (2.6.1.0)
    - Add pvt::heapsize() and pvt::footprint() methods and image cache memory tracking [#4322](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4322) (by Basile Fraboni) (2.6.3.0)
    - De-virtualize ImageCache and TextureSystem [#4384](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4384) (2.6.3.0)
    - IC/TS have new `get_imagespec()`, `imagespec()`, and `get_cache_dimensions()`
      methods. [#4442](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4442) (by Basile Fraboni) (2.6.6.0)
    - *python*: Implement ImageCache.get_imagespec() [#3982](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3982) (2.6.0.0, 2.5.3.1-beta2)
    - `TextureOpt` has been refactored a bit: some fields have been reordered;
      it's actually called TextureOpt_v2 (TextureOpt is an alias) to allow
      better compatibility-preserving improvements in the future, and
      similarly, TextureOptBatched is an alias for TextureOptBatch_v1. The
      type names of some enums have been changed, but aliases should preserve
      compatibility in the vast majority of cases. [#4485](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4485) [#4490](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4490)
      (3.0.0.0)
* *API Deprecations*: (please see [the detailed deprecation list](docs/Deprecations-3.0.md))
    - Various other minor deprecations of things that had been marked as
      deprecated for a while in fmath.h [#4309](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4309) (2.6.2.0), typedesc.h [#4311](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4311) (2.6.2.0), simd.h [#4308](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4308) (2.6.2.0), assorted [#4234](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4234) (2.6.2.0), texture.h [#4339](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4339) (2.6.3.0), imageio.h [#4312](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4312) (2.6.3.0), benchmark.h, bit.h, color.h, errorhandler.h [#4335](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4335), parmalist.h, parallel.h, strutil.h, sysutil.h, thread.h, tiffutils.h, ustring.h, type_traits.h [#4338](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4338) (2.6.3.0), imagebuf.h [#4341](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4341) (2.6.3.0), imagebufalgo.h [#4344](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4344) (2.6.3.0),
      dassert.h imagebufalgo.h imagecache.h imageio.h simd.h strutil.h ustring.h [#4480](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4480) [#4488](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4488) (3.0.0.0)
    - The deprecated headers array_view.h and missing_math.h have been removed. [#4335](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4335) [#4338](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4338) (2.6.3.0)
    - Make span::size() return size_t, not a signed type [#4332](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4332) (2.6.3.0)
* *Build system dependency self-builders*: <br>
  The cmake-based build system has
  been enhanced to give a report of what dependencies it found, what was
  missing, what was found but was a version too old for our requirement.
  If the `OpenImageIO_BUILD_MISSING_DEPS` cmake variable is set to "all"
  (or a list of specific packages), the build system will attempt to
  build certain missing dependencies locally. Currently, this works for
  fmt, freetype, Imath, jpeg-turbo, libtiff, OpenColorIO, OpenEXR, pybind11, Robinmap, WebP, Zlib.
  Additional dependencies will learn to self-build over time.
  [#4242](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4242)
  [#4294](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4294) by Larry Gritz,
  [#4392](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4392) by zachlewis (2.6.3.0)
  [#4420](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4420) (by zachlewis) (2.6.6.0)
  [#4422](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4422) (by zachlewis) (3.0.0.1)
  [#4493](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4493) (by kaarrot) (3.0.0.1)
* *Environment variables*
    - The environment variable `OIIO_LIBRARY_PATH` that contains the search
      paths for finding image file format plugins has been changed to be
      called `OPENIMAGEIO_PLUGIN_PATH`. This is more consistent: all the
      "public API" documented environment variables that are meant for
      users/sites to adjust are named starting with `OPENIMAGEIO_`, whereas
      the prefix `OIIO_` is only used for environment variables that are
      "unofficial" (undocumented, temporary, or meant only for developers to
      use for debugging). [#4330](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4330) (2.6.3.0)
    - Rename env variable `OIIOTOOL_METADATA_HISTORY` to
      `OPENIMAGEIO_METADATA_HISTORY` [#4368](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4368) (2.6.3.0)

### 🚀  Performance improvements:
  - *oiiotool*: `--mosaic` improvements to type conversion avoid unnecessary
  copies and format conversions. [#3979](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3979) (2.6.0.0, 2.5.3.1-beta2)
  - *oiiotool*: Use pointer, not static, for internal color config, slightly reducing oiiotool startup overhead when color configs are not needed. [#4433](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4433) (2.6.6.0)
  - *simd*: Faster vint4 load/store with unsigned char conversion [#4071](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4071) (by Aras Pranckevičius) (2.6.0.2)
  - *perf/IBA*: Improve perf of IBA::channels in-place operation [#4088](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4088) (2.6.0.2)
  - *perf*: Overhaul of ColorConfig internals to solve perf issues [#3995](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3995) (2.6.0.1)
  - *perf/TS*: Reduce TextureSystem memory by slimming down internal LevelInfo size [#4337](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4337) (by Curtis Black) (2.6.3.0)
  - *TS*: Have maketx/IBA::make_texture only write full metadata to the first mip level. We presume that other than resolution and encoding-related information, other metadata should not be expected to differ between MIP levels of the same image. This saves file size and memory in the IC/TS. [#4320](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4320) (2.6.3.0)
  - *IC/TS*: Store full metadata only at subimage 0, miplevel 0 for ptex files. [#4376](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4376) (2.6.3.0)
  - *perf*: Additional timing logging for performance investigations [#4506](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4506) (3.0.0.1)
  - *ImageBuf*: ImageBuf file read performance -- double reads, extra copies [#4507](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4507) (3.0.0.1)
  - *perf*: Remove redundant ImageSpec from ImageCache internals [#4664](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4664) (by Basile Fraboni) (3.1.3.0)

### 🐛  Fixes and feature enhancements:
  - *errors*: Print unretrieved global error messages upon application exit.
    This should help beginning developers see important error messages they
    have failed to retrieve. [#4005](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4005) (2.6.0.1)
  - *font rendering*: Improvements to text rendering by
    `ImageBufAlgo::render_text()` and `oiiotool --text`:
      - Look up font in text render based on family and style name, in
        addition to font filename. [#4509](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4509) (by peterhorvath111) (3.0.0.1)
      - Fix incorrect vertical alignment in render_text [#4500](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4500) (by peterhorvath111) (3.0.0.1)
      - Windows newline shows invalid character in text render [#4501](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4501) (by peterhorvath111) (3.0.0.1)
      - Improve internals of font search enumeration [#4508](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4508) (by peterhorvath111) (3.0.0.1)
  - *oiiotool*: Overhaul and fix bugs in mixed-channel propagation [#4127](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4127)
  - *oiiotool*: Expression substitution now understands pseudo-metadata `NONFINITE_COUNT` that returns the number of nonfinite values in the image, thus allowing decision making about fixnan [#4171](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4171)
  - *oiiotool*: --autocc bugfix and color config inventory cleanup [#4060](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4060) (2.6.0.1)
  - *oiiotool*: Improve over-blurring of certain oiiotool --fit situations [#4108](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4108) (2.6.0.3)
  - *oiiotool*: `-i:ch=...` didn't fix up alpha and z channels [#4373](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4373) (2.6.3.0)
  - *iinfo*: iinfo was not reading MIP levels correctly [#4498](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4498) (3.0.0.1)
  - *iv*: Assume iv display gamma 2.2 [#4118](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4118) (2.6.0.3)
  - *dds*: Always seek to the beginning of the ioproxy during open for DDS and PSD files [#4048](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4048) (by Jesse Yurkovich) (2.6.0.1)
  - *dds*: DDS support more DXGI formats [#4220](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4220) (by alexguirre) (2.6.2.0)
  - *heic*: Don't auto-transform camera-rotated images [#4142](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4142) (2.6.0.3) [#4184](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4184) (2.6.1.0)
  - *heic*: Correctly set imagespec size for heif images (by Gerrard Tai) (2.6.3.0)
  - *iff*: Refactor iffoutput.cpp for memory safety [#4144](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4144) (2.6.0.3)
  - *jpeg*: New output hint "jpeg:iptc" can be used to instruct JPEG output to not output the IPTC data to the file's header. [#4346](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4346) (2.6.3.0)
  - *jpeg2000*: Include the headers we need to discern version [#4073](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4073) (2.6.0.2)
  - *jxl*: JPEG-XL improvements [#4252](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4252) (by Vlad (Kuzmin) Erium) (2.6.2.0)
  - *jpegxl*: Handle various bits per sample values in JPEG XL files [#4738](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4738) (by Peter Kovář) (3.1.3.0)
  - *jxl/docs*: Clarify description of JPEG XL faster decoding modes [#4812](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4812) (by Jonathan Brown) (3.1.3.0)
  - *openexr*: Handle edge case of exr attribute that interferes with our hints [#4008](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4008) (2.6.0.1)
  - *openexr*: Add support for luminance-chroma OpenEXR images. [#4070](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4070) (by jreichel-nvidia) (2.6.0.3)
  - *openexr*: Implement copy_image for OpenEXR [#4004](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4004) (by Andy Chan) (2.6.1.0)
  - *openexr*: Fix out-of-bounds reads when using OpenEXR decreasingY lineOrder. [#4215](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4215) (by Aaron Colwell) (2.6.2.0)
  - *openexr*: Add proxy support for EXR multipart output [#4263](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4263) [#4264](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4264) (by jreichel-nvidia) (2.6.2.0)
  - *openexr*: Modernize dwa compression level setting [#4434](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4434) (3.0.0)
  - *openexr*: Add `htj2k` as a compression option for OpenEXR. Note that this only is supported by OpenEXR 3.4 (or OpenEXR main, after the feature was added). [#4785](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4785) (by LI JI) (3.1.3.0)
  - *ffmpeg*: Add proper detection of new FFmpeg versions [#4394](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4394) (by Darby Johnston) (2.6.3.0)
  - *ffmpeg*: FFmpeg additional metadata [#4396](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4396) (by Darby Johnston) (2.6.3.0)
  - *png*: New output compression mode names recognized: "none", "pngfast".
    Also some minor speedups to PNG writes.
    [#3980](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3980) (2.6.0.0)
  - *png*: Write out proper tiff header version in png EXIF blobs [#3984](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3984) (by Jesse Yurkovich) (2.6.0.0, 2.5.3.1)
  - *png*: A variety of minor optimizations to the PNG writer [#3980](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3980)
  - *png*: Improve png write with alpha is low [#3985](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3985) (2.6.0.1)
  - *png*: Fix crash for writing large PNGs with alpha [#4074](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4074) (2.6.0.2)
  - *png*: Correctly read PNGs with partial alpha [#4315](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4315) (2.6.2.0)
  - *png*: Round dpi resolution to nearest 0.1 [#4347](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4347) (2.6.3.0)
  - *png*: Bug in associateAlpha botched alpha=0 pixels [#4386](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4386) (2.6.3.0)
  - *pnm*: Improvements to pnm plugin [#4253](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4253) (by Vlad (Kuzmin) Erium) (2.6.2.0)
  - *pnm*: Initialize m_pfm_flip before use to avoid UB. [#4446](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4446) (by Bram Stolk) (2.6.6.0)
  - *psd*: Always seek to the beginning of the ioproxy during open for DDS and PSD files [#4048](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4048) (by Jesse Yurkovich) (2.6.0.1)
  - *psd*: Add support for 16- and 32-bit Photoshop file reads [#4208](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4208) (by EmilDohne) (2.6.2.0)
  - *psd*: Various PSD files fail to load correctly [#4302](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4302) (by Jesse Yurkovich) (2.6.2.0)
  - *raw*: LibRaw wavelet denoise options [#4028](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4028) (by Vlad (Kuzmin) Erium) (2.6.0.1)
  - *raw*: Avoid buffer overrun for flip direction cases [#4100](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4100) (2.6.0.3)
  - *raw*: Expose additional white balancing hints: "raw:user_black", "raw:use_auto_wb", "raw:grey_box", "dng:version", "dng:baseline_exposure", "dng:calibration_illuminant#", "dng:color_matrix#", "dng:camera_calibrationX". [#4360](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4360) (by Anton Dukhovnikov) (2.6.3.0)
  - *raw*: Make the crop match in-camera JPEG [#4397](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4397) (by Anton Dukhovnikov) (2.6.3.0)
  - *raw*: Check for nullptr in raw input plugin [#4448](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4448) (by Anton Dukhovnikov) (2.6.6.0)
  - *raw*: Raw reader - exposing max_raw_memory_mb [#4454](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4454) (by Ankit Sinha) (2.6.6.0)
  - *tiff*: Fix TIFF export with EXIF data and I/O proxy [#4300](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4300) (by jreichel-nvidia) (2.6.3.0)
  - *ImageBuf*: Fix crash when mutable Iterator used with read-IB [#3997](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3997) (2.6.0.1)
  - *ImageBuf*: Improve IB::nsubimages and other related fixes [#4228](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4228) (2.6.2.0)
  - *ImageBuf*: Copy/paste error in the ImageBuf iterator copy constructor [#4365](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4365) (by Anton Dukhovnikov) (2.6.3.0)
  - *ImageBufAlgo*: IBA::to_OpenCV fails for ImageCache-backed images [#4013](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4013) (2.6.0.1)
  - *ImageBufAlgo*: Add missing version of warp [#4390](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4390) (2.6.3.0)
  - *ImageBufAlgo*: IBA::transpose() didn't set output image's format to input [#4391](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4391) (2.6.3.0)
  - *ImageBufAlgo*: Fix issue when computing perceptual diff [#4061](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4061) (by Aura Munoz) (2.6.0.1)
  - *ImageInput*: Only check REST arguments if the file does not exist, avoiding problems for filenames that legitimately contain a `?` character. [#4085](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4085) (by AdamMainsTL) (2.6.0.2)
  - *fix*: Certain int->float type conversions in TypeDesc/ParamValueList [#4132](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4132) (2.6.0.3)
  - *color management*: Automatically recognize some additional color space name synonyms: "srgb_texture", "lin_rec709" and "lin_ap1". Also add common permutation "srgb_tx" and "srgb texture" as additional aliases for "srgb". [#4166](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4166)
  - *color management*: Color management nomenclature improvements: "linear"
    is now just a legacy synonym for the preferred "lin_rec709", which is
    used widely where applicable. [#4479](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4479) (3.0.0.0)
  - *security*: Don't use (DY)LD_LIBRARY_PATH as plugin search paths [#4245](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4245) (by Brecht Van Lommel) (2.6.2.0)
  - *fix*: Fix crash when no default fonts are found [#4249](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4249) (2.6.2.0)
  - *TextureSystem*: Fix missing initialization in TextureOptBatch [#4226](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4226) (2.6.2.0)
  - *iv*: Avoid crash with OpenGL + multi-channel images [#4087](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4087) (2.6.0.2)
  - *iv*: If OCIO env is not set or doesn't exist, have iv use built-in config [#4285](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4285) (2.6.2.0)
  - *iv*: Iv should enable the ImageCache [#4326](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4326) (by Jesse Yurkovich) (2.6.3.0)
  - *ImageCache*: Simplify tile cache clearing. [#4292](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4292) (by Curtis Black) (2.6.2.0)

### 🔧  Internals and developer goodies
  - *int*: Prevent infinite loop in bit_range_convert [#3996](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3996) (by Jesse Yurkovich) (2.6.0.1)
  - *int*: More switching fprintf/etc to new style print [#4056](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4056) (2.6.0.1)
  - *int*: Various fixes for memory safety and reduce static analysis complaints [#4128](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4128) (2.6.0.3)
  - *int*: Use OIIO functions for byte swapping to make Sonar happy [#4174](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4174) (2.6.1.0)
  - *int*: More conversion to new string formatting [#4189](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4189) (2.6.1.0) [#4231](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4231) (2.6.2.0) [#4247](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4247) (2.6.2.0) [#4258](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4258) (2.6.2.0)
  - *int*: Added validity checks to PNG, JPEG, and EXR readers to try to catch implausible resolutions or channels that are likely to be corrupted or malicious images. [#4452](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4452) (by Dharshan Vishwanatha) (2.6.6.0)
  - *int*: ImageInput: Initialize pixels of partial tile conversion buffer,
    avoiding possible floating point errors. [#4462](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4462) (by Bram Stolk) (2.6.6.0)
  - *bit.h*: Move bitcast, byteswap, and rotl/rotr to new bit.h [#4106](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4106) (2.6.0.3)
  - *bit.h*: OIIO::bitcast adjustments [#4101](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4101) (2.6.0.3)
  - *filesystem.h*: Filesystem::unique_path wasn't using the unicode rectified string [#4203](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4203) (2.6.1.0)
  - *filesystem.h*: IOProxy const method adjustments [#4415](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4415) (2.6.6.0)
  - *fmath.h*: One more fast_exp fix [#4275](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4275) (2.6.2.0)
  - *fmt.h*: Fix build break from recent fmt change [#4227](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4227) (2.6.2.0)
  - *hash.h*: Mismatched pragma push/pop in hash.h [#4182](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4182) (2.6.1.0)
  - *imagebuf.h*: Add `ImageBuf::wrapmode_name()`, inverse of wrapmode_from_string [#4340](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4340) (2.6.3.0)
  - *oiioversion.h*: Coalesce redundant STRINGIZE macros -> OIIO_STRINGIZE [#4121](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4121) (2.6.0.3)
  - *platform.h*: Belatedly change OIIO_CONSTEXPR14 to constexpr [#4153](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4153) (2.6.0.3)
  - *paramlist.h*: Add ParamValueSpan::get_bool() [#4303](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4303) (2.6.2.0)
  - *platform.h*: In platform.h, define OIIO_DEVICE macro [#4290](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4290) (2.6.2.0)
  - *simd.h*: Fix leaking of Imath.h into public headers [#4062](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4062) (2.6.0.2)
  - *simd.h*: Make all-architecture matrix44::inverse() [#4076](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4076) (2.6.0.2)
  - *simd.h*: AVX-512 round function [#4119](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4119) (by AngryLoki) (2.6.0.3)
  - *simd.h*: Simplify vbool16 casting [#4105](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4105) (2.6.0.3)
  - *simd.h*: Address NEON issues [#4143](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4143) (2.6.0.3)
  - *simd.h*: Gather_mask was wrong for no-simd fallback [#4183](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4183) (2.6.1.0)
  - *simd.h*: For simd types, use default for ctrs and assignment where applicable [#4187](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4187) (2.6.1.0)
  - *simd.h*: Fix longstanding probem with 16-wide bitcast for 8-wide HW [#4268](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4268) (2.6.2.0)
  - *span.h*: Span and range checking enhancements [#4125](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4125) (2.6.0.3)
  - *span.h*: Make span default ctr and assignment be `= default` [#4198](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4198) (2.6.1.0)
  - *span.h*: Span utility improvements [#4398](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4398) (2.6.3.0)
  - *span.h*: Fold span_util.h contents into span.h [#4402](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4402) (2.6.6.0)
  - *span.h*: New utility functions `span_within()`, `check_span()`, and
    macro `OIIO_ALLOCA_SPAN`. [#4426](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4426) (2.6.6.0)
  - *string_view.h*: Deprecate OIIO::string_view::c_str() [#4511](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4511) (3.0.0.1)
  - *strutil.h*: Add `Strutil::eval_as_bool()` [#4250](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4250) (2.6.2.0)
  - *strutil.h*: Add `Strutil::string_is_identifier()` [#4333](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4333) (2.6.3.0)
  - *strutil.h*: Change Strutil::format to default to std::format conventions [#4480](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4480) (3.0.0.0)
  - *sysutil.h*: Deprecate Sysutil::physical_concurrency() [#4034](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4034) (2.6.0.1)
  - *texture.h*: Overload decode_wrapmode to support ustringhash [#4207](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4207) (by Chris Hellmuth) (2.6.1.0)
  - *typedesc.h*: Allow TypeDesc to have all the right POD attributes [#4162](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4162) (by Scott Wilson) (2.6.0.3)
  - *typedesc.h*: Add TypeDesc::Vector3i [#4316](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4316) (2.6.2.0)
  - *ustring.h*: Make sure C++ knows ustring & ustringhash are trivially copyable [#4110](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4110) (2.6.0.3)
  - *ustring.h*: Address ignored annotation nvcc warnings on explicitly-defaulted functions [#4291](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4291) (by Chris Hellmuth) (2.6.2.0)
  - *style*: Update our formatting standard to clang-format 17.0 and C++17 [#4096](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4096) (2.6.0.3)
  - *int*: Use spans to solve a number of memory safety issues [#4148](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4148) (2.6.1.0)
  - *cleanup*: Convert more old errorf() to errorfmt() [#4231](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4231) (2.6.2.0)
  - *fix*: Error retrieval safeguards for recycled objects [#4239](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4239) (2.6.2.0)
  - *fix*: Improve error messages when a font is not found [#4284](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4284) (2.6.2.0)
  - *refactor*: Oiiotool break out expression eval methods into separate file [#4256](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4256) (2.6.2.0)
  - *refactor*: Move most of imageio_pvt.h.in to just a regular .h [#4277](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4277) (2.6.2.0)
  - *refactor*: Simplify openexr includes [#4304](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4304) (2.6.3.0)
  - *fix*: Catch potential OCIO exception that we were missing [#4379](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4379) (2.6.3.0)
  - *fix*: Don't let fmtlib exceptions crash the app [#4400](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4400) (2.6.3.0)
  - *fix*: Beef up some error messages [#4369](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4369) (2.6.3.0)
  - *cleanup*: Remove code disabled as of 3.0 [#4487](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4487) (3.0.0.0)
  - *fix*: Address fmt exceptions for left justification [#4510](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4510) (3.0.0.1)

### 🏗  Build/test/CI and platform ports:
* CMake build system and scripts:
  - Fix Cuda ustring.h warnings [#3978](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3978) (2.6.0.0, 2.5.3.1)
  - Remove unnecessary headers from strutil.cpp causing build trouble [#3976](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3976) (by Jesse Yurkovich) (2.6.0.0, 2.5.3.1)
  - Print build-time warnings for LGPL gotchas [#3958](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3958) (by Danny Greenstein) (2.6.0.0, 2.5.3.1-beta2)
  - *build*: Make C++17 be the default C++ standard for building (C++14 is
    still the minimum for now and can be selected via CMAKE_CXX_STANDARD)
    [#4022](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4022) (2.6.0.1)
  - *build*: Provide compile_commands.json for use by tools [#4014](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4014) (by David Aguilar) (2.6.0.1)
  - *build*: Don't fail for 32 bit builds because of static_assert check [#4006](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4006) (2.6.0.1)
  - *build*: Provide compile_commands.json for use by tools [#4014](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4014) (by David Aguilar) (2.6.0.1)
  - *build*: Don't fail for 32 bit builds because of static_assert check [#4006](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4006) (2.6.0.1)
  - *build*: Better cmake verbose behavior [#4037](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4037) (2.6.0.1)
  - *build*: Fix include guard [#4066](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4066) (2.6.0.2)
  - *build*: Add a way to cram in a custom extra library for iv [#4086](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4086) (2.6.0.2)
  - *build*: Don't fail pybind11 search if python is disabled [#4136](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4136) (2.6.0.3)
  - *build*: Cleanup - get rid of "site" files [#4176](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4176) (2.6.1.0)
  - *build*: Fix buld_ninja.bash to make directories and download correctly [#4192](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4192) (by Sergio Rojas) (2.6.1.0)
  - *build*: Need additional include [#4194](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4194) (2.6.1.0)
  - *build*: Make an OpenImageIO_Util_static library and target [#4190](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4190) (2.6.1.0)
  - *build*: Switch to target-based definitions [#4193](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4193) (2.6.1.0) then mostly revert it [#4273](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4273) (2.6.2.0).
  - *build*: iv build issues with glTexImage3D [#4202](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4202) (by Vlad (Kuzmin) Erium) (2.6.1.0)
  - *build*: Restore internals of strhash to compile correctly on 32 bit architectures [#4213](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4213) (2.6.1.0)
  - *build*: LibOpenImageIO_Util does need DL libs, we removed it incorrectly [#4230](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4230) (2.6.2.0)
  - *build*: Fix missing target_link_options for libraries (by kaarrot) (2.6.2.0)
  - *build*: Disable clang18 warnings about deprecated unicode conversion [#4246](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4246) (2.6.2.0)
  - *build*: More warning elimination for clang18 [#4257](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4257) (2.6.2.0)
  - *build*: Add CMath target for the sake of static libtiff [#4261](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4261) (2.6.2.0)
  - *build*: Add appropriate compiler defines and flags for SIMD with MSVC [#4266](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4266) (by Jesse Yurkovich) (2.6.2.0)
  - *build/windows*: Fix warning on windows [#4272](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4272) (2.6.2.0)
  - *build/windows*: Fix for setenv() on Windows [#4381](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4381) (by Vlad (Kuzmin) Erium) (2.6.3.0)
  - *build*: Gcc-14 support, testing, CI [#4270](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4270) (2.6.2.0)
  - *build*: New set_utils.cmake for various handy "set()" wrappers [#4274](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4274) (2.6.2.0) [#4281](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4281) (2.6.2.0)
  - *build*: Upgrade to more modern python3 finding [#4288](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4288) (2.6.2.0)
  - *build*: Add missing includes to libutil CMake target. [#4306](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4306) (by kaarrot) (2.6.2.0)
  - *build*: Avoid rebuilds due to processing of fmt headers [#4313](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4313) (by Jesse Yurkovich) (2.6.2.0)
  - *build*: Rudimentary CUDA support infrastructure (experimental) [#4293](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4293) (2.6.2.0)
  - *build*: A few cmake cleanups and minor code rearrangements [#4359](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4359) (2.6.3.0)
  - *build*: Don't link libOpenImageIO against OpenCV [#4363](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4363) (2.6.3.0)
  - *build*: Fixed the sign compare causing build failure [#4240](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4240) (by Peter Kovář) (2.6.2.0)
  - *build*: Add a build option for profiling [#4432](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4432) (2.6.6.0)
  - *build*: Don't change CMAKE_XXX_OUTPUT_DIRECTORY when built as subdir [#4417](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4417) (by Luc Touraille) (3.0.0)
  - *build*: Add option for build profiling with clang -ftime-trace [#4475](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4475) (3.0.0)
  - *build*: Reduce compile time by trimming template expansion in IBA. [#4476](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4476) (3.0.0.0)
* Dependency support:
  - *deps/OpenVDB*: Protect against mismatch of OpenVDB vs C++ [#4023](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4023) (2.6.0.1)
  - *deps/OpenVDB*: Adjust OpenVDB version requirements vs C++17 [#4030](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4030) (2.6.0.1)
  - *deps*: Ptex support for static library [#4072](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4072) (by Dominik Wójt) (2.6.0.2)
  - *deps*: Account for header changes in fmt project trunk [#4109](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4109) (2.6.0.3)
  - *deps*: Deal with changes in fmt's trunk [#4114](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4114) (2.6.0.3)
  - *deps*: Remove Findfmt.cmake [#4069](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4069) [#4103](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4103) (by Dominik Wójt) (2.6.0.3)
  - *deps*: Correctly disable OpenVDB when it's incompatible [#4120](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4120) (2.6.0.3)
  - *deps*: Fixes for DCMTK [#4147](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4147) (2.6.0.3)
  - *deps*: Fix warning when Freetype is disabled [#4177](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4177) (2.6.1.0)
  - *deps*: Remove boost from strutil.cpp [#4181](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4181) (by Jesse Yurkovich) (2.6.1.0)
  - *deps*: FindOpenColorIO failed to properly set OpenColorIO_VERSION [#4196](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4196) (2.6.1.0)
  - Use exported targets for libjpeg-turbo and bump min to 2.1
    [#3987](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3987) (2.6.0.1, 2.5.3.1-beta2)
  - *deps*: Support fmt 11.0 [#4441](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4441) (2.6.6.0)
  - *deps*: Support and test against OCIO 2.4 [#4459](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4459) [#4467](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4467)  (2.6.6.0)
  - *deps*: No need for OCIO search to use PREFER_CONFIG [#4425](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4425) (2.6.6.0)
  - *deps*: Raise CMake minimum to 3.18.2 [#4472](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4472) (3.0.0)
  - *deps*: Remove the enforced upper version limit for fmt [#4497](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4497) (3.0.0.1)
  - *deps*: Search for libbz2 only if FFmpeg or FreeType is enabled. [#4505](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4505) (by jreichel-nvidia) (3.0.0.1)
  - *deps/jxl*: Use correct cmake variables for the include directories [#4810](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4810) [#4813](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4813) (by Jesse Yurkovich) (3.1.3.0)
  - *deps*: Adjust pystring finding [#4816](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4816) (3.1.3.0)
  - *deps*: Fixes to build against libheif 1.20[#4822](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4822) (by Rui Chen) (3.1.3.0)
* Testing and Continuous integration (CI) systems:
  - Tests for ABI compliance [#3983](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3983), [#3988](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3988) (2.6.0.0, 2.5.3.1)
  - *tests*: Imagebuf_test add benchmarks for iterator traversal [#4007](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4007) (2.6.0.1)
  - *tests*: Add opencv regression test [#4024](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4024) (2.6.0.1)
  - *tests*: Improve color management test in imagebufalgo_test [#4063](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4063) (2.6.0.2)
  - *tests*: Add one more ref output for python-colorconfig test [#4065](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4065) (2.6.0.2)
  - *tests*: Shuffle some tests between directories [#4091](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4091) (2.6.0.2)
  - *tests*: Fix docs test, used wrong namespace [#4090](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4090) (2.6.0.2)
  - *tests/fixes*: Fixes to reduce problems identified by static analysis [#4113](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4113) (2.6.0.3)
  - *tests*: Add test for filter values and 'filter_list' query [#4140](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4140) (2.6.0.3)
  - *tests*: Add new heif test output [#4262](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4262) (2.6.2.0)
  - *tests*: Fix windows quoting for test [#4271](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4271) (2.6.2.0)
  - *tests*: Remove unused test output ref from old dependency versions [#4370](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4370) (2.6.3.0)
  - *tests*: Add switch to imageinout_test for enabling floating point exceptions. [#4463](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4463) (by Bram Stolk) (3.0.0)
  - *tests*: Fixup after directory refactor of OpenImageIO-images [#4473](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4473) (3.0.0)
  - *tests*: Remove old test reference output we no longer need [#4817](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4817) (3.1.3.0)
  - *ci*: Some straggler repo renames in the workflows [#4025](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4025) (2.6.0.1)
  - *ci*: CI tests on MacOS ARM, and fixes found consequently [#4026](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4026) (2.6.0.1)
  - *ci*: Nomenclature change 'os' to 'runner' for clarity [#4036](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4036) (2.6.0.1)
  - *ci*: Add tiff-misc reference for slightly changed error messages [#4052](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4052) (2.6.0.1)
  - *ci*: Remove MacOS-11 test [#4053](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4053) (2.6.0.1)
  - *ci*: Test against gcc-13 [#4059](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4059) (2.6.0.1)
  - *ci*: Restrict Mac ARM running [#4077](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4077) (2.6.0.2)
  - *ci*: Rename macro to avoid conflict during CI unity builds [#4092](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4092) (2.6.0.2)
  - *ci*: Repair Sonar scanner analysis [#4097](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4097) [#4099](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4099) (2.6.0.2)
  - *ci*: Improve parallel builds by basing on number of cores [#4115](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4115) (2.6.0.3)
  - *ci*: Update all github actions to their latest versions that's compatible [#4129](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4129) (2.6.0.3)
  - *ci*: Bump 'latest' test to newer dep versions, document [#4130](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4130) (2.6.0.3)
  - *ci*: Revert to fix scorecard analysis, try version 2.0.6 (2.6.0.3)
  - *ci*: Start using macos-14 ARM runners, bump latest OCIO [#4134](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4134) (2.6.0.3)
  - *ci*: Switch away from deprecated GHA idiom set-output [#4141](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4141) (2.6.0.3)
  - *ci*: Add vfx platform 2024 [#4163](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4163) (2.6.0.3)
  - *ci*: Fix Windows CI, need to build newer openexr and adjust boost search [#4167](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4167) (2.6.0.3)
  - *ci*: Adjust GHA upload-artifact action version [#4179](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4179) (2.6.1.0)
  - *ci*: Allow triggering CI workflow from web [#4178](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4178) (2.6.1.0)
  - *ci*: Make one of the Mac tests build for avx2 [#4188](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4188) (2.6.1.0)
  - *ci*: Enable Windows 2022 CI tests [#4195](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4195) (2.6.1.0)
  - *ci*: Update scrorecard workflow to fix breakage [#4201](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4201) (2.6.1.0)
  - *ci*: Fix broken Windows CI by building our own libtiff [#4214](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4214) (2.6.2.0)
  - *ci*: Typo in build_libtiff.bash [#4280](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4280) (2.6.2.0)
  - *ci*: For Windows CI, build only release of vcpkg packages [#4282](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4282) (2.6.2.0)
  - *ci*: New tets: oldest, hobbled, localbuilds [#4295](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4295) (2.6.2.0)
  - *ci*: Fix GHA CI after they upgraded nodejs [#4324](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4324) (2.6.3.0)
  - *ci*: Sanitizer new warnings about signed/unsigned offsets in openexr [#4351](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4351) (2.6.3.0)
  - *ci*: Deal with CentOS 7 EOL and disappearance of yum mirrors [#4325](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4325) (2.6.3.0)
  - *ci*: CI sanitizer test improvements [#4374](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4374) (2.6.3.0)
  - *ci*: Add a workflow that builds docs [#4413](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4413) (2.6.6.0)
  - *ci*: Streamline the old MacOS-12 CI test [#4465](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4465) (2.6.6.0)
  - *ci*: Test against OpenEXR 3.3 and deal with its 4.0 bump [#4466](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4466) (2.6.6.0)
  - *ci*: Make scrorecards workflow not fail constantly [#4471](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4471)
  - *ci*: Limit when automatic docs building ci happens [#4496](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4496) (3.0.0.1)
  - *ci*: Retire deprecated macos12 runner, try beta macos15 [#4514](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4514) (3.0.0.1)
  - *ci*: Remove tests on Windows-2019 GitHub runner [#4793](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4793) (3.1.3.0)
  - *ci*: Fix broken bleeding edge CI [#4798](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4798) (3.1.3.0)
  - *ci*: Various ccache save/restore improvements for CI runs [#4797](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4797) (3.1.3.0)
  - *ci*: Simplify gh-win-installdeps, no more vcpkg [#4809](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4809) (3.1.3.0)
  - *ci*: Bump 'latest' test versions [#4819](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4819) (3.1.3.0)
* Platform support:
  - *win*: Fix building failed from source on Windows [#4235](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4235) (by Vic P) (2.6.2.0)
  - *win + arm64*: Add arm_neon.h include on Windows ARM64 with clang-cl [#4691](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4691) (by Anthony Roberts) (3.1.3.0)

### 📚  Notable documentation changes:
  - *docs*: Convert code examples within the docs to tests that are built
    executed as part of the testsuite. [#3977](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3977) [#3994](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3994) (2.6.0.0, 2.5.3.1)
    [#4039](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4039) (by Jeremy Retailleau) [#4444](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4444) (by Ziad Khouri) [#4456](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4456) (by pfranz) [#4455](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4455) (by Ziad Khouri)  [#4460](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4460) (by Lydia Zheng) [#4458](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4458) (by Danny Greenstein) (2.6.6.0) (3.0.0.0) [#4468](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4468) (by pfranz) (3.0.0.1)
  - Spruce up the main README and add "Building_the_docs"
    [#3991](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3991) (2.6.0.1, 2.5.3.1)
  - *docs*: Make an example of doc-to-test in the imagebufalgo chapter [#4012](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4012) (2.6.0.1)
  - *docs*: Convert examples within the imagebufalgo chapter. [#4016](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4016) (by Jeremy Retailleau) (2.6.0.1)
  - *docs*: Added tests for Simple Image input and updated rst [#4019](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4019) (by Calvin) (2.6.0.1)
  - *docs*: Convert make_texture doc examples to tests [#4027](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4027) (by Danny Greenstein) (2.6.0.1)
  - *docs*: Fix RTD configuration for v2 [#4032](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4032) (2.6.0.1)
  - *docs*: Update INSTALL.md to reflect the latest versions we've tested against [#4058](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4058) (2.6.0.1)
  - *docs*: Fix typo [#4089](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4089) (2.6.0.1)
  - *docs*: Minor change to formatting and naming [#4098](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4098) (2.6.0.2)
  - *docs*: Fix link to openexr test images [#4080](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4080) (by Jesse Yurkovich) (2.6.0.2)
  - *security*: Document CVE-2023-42295 (2.6.0.1)
  - *docs*: Fix broken IBA color management documentation [#4104](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4104) (2.6.0.3)
  - *docs*: Update SECURITY and RELEASING documentation [#4138](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4138) (2.6.0.3)
  - *docs*: Fix tab that was missing from the rendering on rtd [#4137](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4137) (2.6.0.3)
  - *docs*: Fix python example [#4139](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4139) (2.6.0.3)
  - *docs*: Fix some typos and add missing oiiotool expression explanations [#4169](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4169) (2.6.1.0)
  - *docs*: Update INSTALL.md for windows [#4279](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4279) (by Mel Massadian) (2.6.2.0)
  - *doc*: Add missing documentation of ImageBuf locking methods [#4267](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4267) (2.6.2.0)
  - *doc*: Fixes to formatting and sphinx warnings [#4301](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4301) (2.6.2.0)
  - *docs*: Clarify that IBA::rotate params are pixel coordinates [#4358](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4358) (2.6.3.0)
  - *docs*: Clarify TextureSystem::create use of imagecache when shared=true [#4399](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4399) (2.6.3.0)
  - *docs*: Fix typo where apostrophe was used for possessive of 'it' [#4383](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4383) (by Joseph Goldstone) (2.6.3.0)
  - *docs/security*: Document CVE-2024-40630 resolution (2.6.3.0)
  - *docs*: IBA::st_warp was missing from the documentation [#4431](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4431) (2.6.6.0)
   - *docs*: Move some docs files around [#4470](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4470) (2.6.6.0)
  - *docs*: Various minor fixes [#4477](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4477) (3.0.0)
  - *docs*: Add documenting comments where missing in string_view and span [#4478](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4478) (3.0.0)
  - *docs*: Fix typo in description of Strutil::parse_values [#4512](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4512) (3.0.0.1)

### 🏢  Project Administration
  - *admin*: Repo rename -- fix all URL references [#3998](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3998) [#3999](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3999)
  - *admin*: Alert slack "release-announcements" channel upon OIIO release [#4002](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4002) [#4046](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4046) [#4047](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4047) [#4079](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4079) (2.6.0.3)
  - *admin*: Relicense more code under Apache 2.0 [#4038](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4038) [#3905](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3905)
  - *admin*: Account for duplicate emails in the .mailmap [#4075](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4075) (2.6.0.2)
  - *admin*: Add a ROADMAP document [#4161](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4161) (2.6.1.0)
  - *docs*: Better documentation of past CVE fixes in SECURITY.md [#4238](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4238) (2.6.2.0)
  - *admin*: More CLA explanation and how-to links [#4318](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4318) (2.6.2.0)
  - *admin*: Add deprecation updates to the RELEASING checklist [#4345](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4345) (2.6.3.0)
  - *admin*: Document my git-cliff workflow for release notes [#4319](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4319) (2.6.3.0)
  - *admin*: Change docs and comments references master -> main [#4435](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4435) (2.6.6.0)
  - *admin*: Update OpenImageIO Roadmap [#4469](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4469) (by Todica Ionut) (2.6.6.0)
  - *admin*: Update SECURITY.md for 3.0 beta [#4486](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4486) (3.0.0.0)
  - *admin*: Remove stale intake documents [#4815](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4815) (3.1.3.0)



--------------

For older release notes, see:
* [CHANGES-2.x](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/CHANGES-2.x.md).
* [CHANGES-1.x](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/CHANGES-1.x.md).
* [CHANGES-0.x](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/docs/CHANGES-0.x.md).


<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->
