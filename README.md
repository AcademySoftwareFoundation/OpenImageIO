README for OpenImageIO
======================

[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg?style=flat-square)](https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md)
[![Build Status](https://travis-ci.org/OpenImageIO/oiio.svg?branch=master)](https://travis-ci.org/OpenImageIO/oiio)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/a0l32ti7gcoergtf/branch/master?svg=true)](https://ci.appveyor.com/api/projects/status/a0l32ti7gcoergtf/branch/master)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/2694/badge)](https://bestpractices.coreinfrastructure.org/projects/2694)


Introduction
------------

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
  including TIFF, JPEG/JFIF, OpenEXR, PNG, HDR/RGBE, ICO, BMP, Targa,
  JPEG-2000, RMan Zfile, FITS, DDS, Softimage PIC, PNM, DPX, Cineon,
  IFF, Field3D, OpenVDB, Ptex, Photoshop PSD, Wavefront RLA, SGI, WebP,
  GIF, DICOM, HEIC/HEIF, many "RAW" digital camera formats, and a variety of
  movie formats (readable as individual frames).  More are being developed
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



Licensing
---------

OpenImageIO is (c) Copyright Contributors to the OpenImageIO project.

OpenImageIO is distributed using the [modified BSD license](LICENSE.md)
(also known as the "new BSD" or "3-clause BSD" license).  The documentation
is licensed under the [Creative Commons Attribution 3.0 Unported
License](http://creativecommons.org/licenses/by/3.0/).

The plain English bottom line is that OpenImageIO is free, as well as
freely modifiable and redistributable (in both source and binary form).
You may use part or all of it in your own applications, whether
proprietary or open, free or commercial.  Using it in a commercial or
proprietary application DOES NOT obligate you to pay us, or to use any
particular licensing terms in your own application.

Some code and resources are distributed along with OIIO that have highly
compatible, though slightly different, licenses (generally MIT or Apache).
See the PDF documentation Acknowledgements section, and the
[THIRD-PARTY](THIRD-PARTY.md) file for details.


Building and Installation
-------------------------

Please read the [INSTALL.md](INSTALL.md) file for detailed instructions on
how to build and install OpenImageIO.

If you build with `EMBEDPLUGINS=0`, remember that you need to set the
environment variable `OIIO_LIBRARY_PATH` to point to the 'lib' directory
where OpenImageIO is installed, or else it will not be able to find the
plugins.


Documentation
-------------

The primary user and programmer documentation can be found
on [openimageio.readthedocs.io](https://openimageio.readthedocs.io/en/latest/)
as html or as [PDF](https://readthedocs.org/projects/openimageio/downloads/pdf/latest/).


Contact & reporting problems
----------------------------

Simple "how do I...", "I'm having trouble", or "is this a bug" questions are
best asked on the [oiio-dev developer mail
list](http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org).
That's where the most people will see it and potentially be able to answer
your question quickly (more so than a GH "issue").

Bugs, build problems, and discovered vulnerabilities that you are relatively
certain is a legit problem in the code, and for which you can give clear
instructions for how to reproduce, should be [reported as
issues](https://github.com/OpenImageIO/oiio/issues).

If confidentiality precludes a public question or issue, you may contact us
privately at [info@openimageio.org](info@openimageio.org), or for
security-related issues [security@openimageio.org](security@openimageio.org).


Contributing
------------

OpenImageIO welcomes code contributions, and [nearly 150 people](CREDITS.md)
have done so over the years. We take code contributions via the usual GitHub
pull request (PR) mechanism. Please see [CONTRIBUTING](CONTRIBUTING.md) for
detailed instructions.


Web Resources
-------------

Main web page:      http://www.openimageio.org

GitHub page:        http://github.com/OpenImageIO/oiio

Mail list subscriptions and archives:

* Developer mail list: http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org

* Just release announcements: http://lists.openimageio.org/listinfo.cgi/oiio-announce-openimageio.org

