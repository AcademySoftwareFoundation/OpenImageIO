<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->
<p align="center">
  <img src="ASWF/logos/openimageio-horizontal-gradient.png">
</p>

[![License](https://img.shields.io/badge/license-Apache2.0-blue.svg?style=flat-square)](https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/LICENSE.md)
[![CI](https://github.com/AcademySoftwareFoundation/OpenImageIO/actions/workflows/ci.yml/badge.svg)](https://github.com/AcademySoftwareFoundation/OpenImageIO/actions/workflows/ci.yml)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/2694/badge)](https://bestpractices.coreinfrastructure.org/projects/2694)
[![latest packaged version(s)](https://repology.org/badge/latest-versions/openimageio.svg)](https://repology.org/project/openimageio/versions)


Introduction
------------

**Mission statement**: OpenImageIO is a toolset for reading, writing, and
manipulating image files of any image file format relevant to VFX / animation
via a format-agnostic API with a feature set, scalability, and robustness
needed for feature film production.

The primary target audience for OIIO is VFX studios and developers of
tools such as renderers, compositors, viewers, and other image-related
software you'd find in a production pipeline.

OpenImageIO consists of:

* Simple but powerful ImageInput and ImageOutput APIs that provide
  an abstraction for reading and writing image files of nearly any
  format, without the calling application needing to know any of the
  details of these file formats, and indeed without the calling
  application needing to be aware of which formats are available.

* A library that manages subclasses of ImageInput and ImageOutput that
  implement I/O from specific file formats, with each file format's
  implementation stored as a plug-in.  Therefore, an application using
  OpenImageIO's APIs can read and write any image file for which a
  plugin can be found at runtime.

* Plugins implementing I/O for several popular image file formats,
  including TIFF, JPEG/JFIF, JPEG XL, OpenEXR, PNG, HDR/RGBE, ICO, BMP, Targa,
  JPEG-2000, RMan Zfile, FITS, DDS, Softimage PIC, PNM, DPX, Cineon,
  IFF, OpenVDB, Ptex, Photoshop PSD, Wavefront RLA, SGI, WebP,
  GIF, DICOM, HEIF/HEIC/AVIF, many "RAW" digital camera formats, and a variety
  of movie formats (readable as individual frames).  More are being developed
  all the time.

* Several command line image tools based on these classes, including
  oiiotool (command-line format conversion and image processing), iinfo
  (print detailed info about images), iconvert (convert among formats,
  data types, or modify metadata), idiff (compare images), igrep (search
  images for matching metadata), and iv (an image viewer). Because these
  tools are based on ImageInput/ImageOutput, they work with any image
  formats for which ImageIO plugins are available.

* An ImageCache class that transparently manages a cache so that it
  can access truly vast amounts of image data (tens of thousands of
  image files totaling multiple TB) very efficiently using only a tiny
  amount (tens of megabytes at most) of runtime memory.

* A TextureSystem class that provides filtered MIP-map texture
  lookups, atop the nice caching behavior of ImageCache.  This is used
  in commercial renderers and has been used on many large VFX and
  animated films.

* ImageBuf and ImageBufAlgo functions -- a simple class for storing
  and manipulating whole images in memory, and a collection of the
  most useful computations you might want to do involving those images,
  including many image processing operations.

* Python bindings for all of the major APIs.



🏢 Project administration and Licensing
---------------------------------------

OpenImageIO is (c) Copyright Contributors to the OpenImageIO project.

For original code, we use the [Apache-2.0 license](LICENSE.md), and for
documentation, the [Creative Commons Attribution 4.0 Unported
License](http://creativecommons.org/licenses/by/4.0/). In 2023 we asked
historical users to [relicense](RELICENSING.md) from the original BSD-3-clause
license to Apache-2.0, and over 99.86% of lines of code have been relicensed
to Apache-2.0. A small amount of code incorporated into this repository from
other projects are covered by compatible [third-party open source
licenses](THIRD-PARTY.md).

The OpenImageIO project is part of the [Academy Software
Foundation](https://www.aswf.io/), a part of the Linux Foundation formed in
collaboration with the Academy of Motion Picture Arts and Sciences. The
[Technical Charter](aswf/Technical-Charter.md) and [Project
Governance](GOVERNANCE.md) explain how the project is run, who makes
decisions, etc. Please be aware of our [Code of Conduct](CODE_OF_CONDUCT.md).


💁 User Documentation
---------------------

[OpenImageIO Documentation](https://docs.openimageio.org)
is the best place to start if you are interested in how to use OpenImageIO,
its APIs, its component programs (once they are built). There is also a [PDF
version](https://readthedocs.org/projects/openimageio/downloads/pdf/latest/).

Additional resources:

- [User quick start](docs/QuickStart.md) is a quick example of using
  OpenImageIO in Python, C++, and the command line.


👷 Building and installing OpenImageIO
--------------------------------------
- [Build and installation instructions](INSTALL.md) for OpenImageIO. Such
  as it is. This could use some work, particularly for Windows.


🚑 Contact & reporting problems
-------------------------------

Simple "how do I...", "I'm having trouble", or "is this a bug" questions are
best asked on the [oiio-dev developer mail
list](https://lists.aswf.io/g/oiio-dev). That's where the most people will see
it and potentially be able to answer your question quickly (more so than a GH
"issue"). For quick questions, you could also try the [ASWF
Slack](https://slack.aswf.io) `#openimageio` channel.

Bugs, build problems, and discovered vulnerabilities that you are relatively
certain is a legit problem in the code, and **for which you can give clear
instructions for how to reproduce**, should be [reported as
issues](https://github.com/AcademySoftwareFoundation/OpenImageIO/issues).

If confidentiality precludes a public question or issue, you may contact us
privately at [info@openimageio.org](info@openimageio.org), or for
security-related issues [security@openimageio.org](security@openimageio.org).


🔧 Contributing and developer documentation
-------------------------------------------

OpenImageIO welcomes code contributions, and [nearly 200 people](CREDITS.md)
have done so over the years. We take code contributions via the usual GitHub
pull request (PR) mechanism.

* [Architecture overview](docs/dev/Architecture.md) is a high-level
  description of the major classes and their relationships.
* [CONTRIBUTING](CONTRIBUTING.md) has detailed instructions about the
  development process.
* [ROADMAP](docs/ROADMAP.md) is a high-level overview of the current
  development priorities for the next annual release, expected in September,
  2024.
* [RELEASING](docs/dev/RELEASING.md) explains our policies and procedures for
  making releases. We have a major, possibly-compatibility-breaking, release
  annually in September/October, and minor bug fix and safe feature addition
  release at the beginning of every month.
* [Building the docs](src/doc/Building_the_docs.md) has instructions for
  building the documentation locally, which may be helpful if you are editing
  the documentation in nontrivial ways and want to preview the appearance.
* Other developer documentation is in the [docs/dev](docs/dev) directory.


☎️ Communications channels and additional resources
--------------------------------------------------

* [Main web page](http://www.openimageio.org)
* [GitHub project page](http://github.com/AcademySoftwareFoundation/OpenImageIO)
* [Developer mail list](https://lists.aswf.io/g/oiio-dev)
* [ASWF Slack](https://slack.aswf.io) (look for the `#openimageio` channel)
* Biweekly Technical Steering Committee (TSC) Zoom meetings are on the [ASWF
  Calendar](https://calendar.openimageio.org) (click on the OpenImageIO
  meeting entries, every second Monday, to get the Zoom link, anyone may join)
