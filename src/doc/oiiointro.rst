Introduction
############

Welcome to OpenImageIO!

|

    |    I kinda like "Oy-e-oh" with a bit of a groaning Yiddish accent, as in
    |    "OIIO, did you really write yet another file I/O library?"
    |
    |    Dan Wexler

|
|


Overview
========

OpenImageIO provides simple but powerful ImageInput and ImageOutput APIs
that abstract the reading and writing of 2D image file formats.  They
don't support every possible way of encoding images in memory, but for a
reasonable and common set of desired functionality, they provide an
exceptionally easy way for an application using the APIs support a wide
--- and extensible --- selection of image formats without knowing the
details of any of these formats.

Concrete instances of these APIs, each of which implements the ability
to read and/or write a different image file format, are stored as
plugins (i.e., dynamic libraries, DLL's, or DSO's) that are loaded at
runtime.  The OpenImageIO distribution contains such plugins for several
popular formats.  Any user may create conforming plugins that implement
reading and writing capabilities for other image formats, and any
application that uses OpenImageIO would be able to use those plugins.

The library also implements the helper class ImageBuf, which is a handy way
to store and manipulate images in memory.  ImageBuf itself uses ImageInput
and ImageOutput for its file I/O, and therefore is also agnostic as to
image file formats. A variety of functions in the ImageBufAlgo namespace
are available to perform common image processing operations on ImageBuf's.

The ImageCache class transparently manages a cache so that it can
access truly vast amounts of image data (thousands of image files totaling
hundreds of GB to several TBs) very efficiently using only a tiny amount
(tens of megabytes to a few GB at most) of runtime memory.  Additionally, a
TextureSystem class provides filtered MIP-map texture lookups, atop
the nice caching behavior of ImageCache.

Finally, the OpenImageIO distribution contains several utility programs that
operate on images, each of which is built atop this functionality, and
therefore may read or write any image file type for which an appropriate
plugin is found at runtime.  Paramount among these utilities `oiiotool`,
a command-line image processing engine, and `iv`, an image viewing
application.  Additionally, there are programs for converting images among
different formats, comparing image data between two images, and examining
image metadata.

All of this is released as "open source" software using the very permissive
"BSD 3-clause" license.  So you should feel free to use any or all of
OpenImageIO in your own software, whether it is private or public, open
source or proprietary, free or commercial.  You may also modify it on your
own.  You are encouraged to contribute to the continued development of
OpenImageIO and to share any improvements that you make on your own, though
you are by no means required to do so.


Simplifying Assumptions
=======================

OpenImageIO is not the only image library in the world.  Certainly there
are many fine libraries that implement a single image format (including
the excellent ``libtiff``, ``libjpeg``, and ``OpenEXR`` that
OpenImageIO itself relies on).  Many libraries attempt to present a uniform
API for reading and writing multiple image file formats.  Most of these
support a fixed set of image formats, though a few of these
also attempt to provide an extensible set by using the plugin approach.

But in our experience, these libraries are all flawed in one or more
ways: (1) They either support only a few formats, or many formats but
with the majority of them somehow incomplete or incorrect.  (2) Their
APIs are not sufficiently expressive as to handle all the image features
we need (such as tiled images, which is critical for our texture
library).  (3) Their APIs are *too complete*, trying to handle
every possible permutation of image format features, and as a result
are horribly complicated.

The third sin is the most severe, and is almost always the main problem
at the end of the day.  Even among the many open source image libraries
that rely on extensible plugins, we have not found one that is both
sufficiently flexible and has APIs anywhere near as simple to understand
and use as those of OpenImageIO.

Good design is usually a matter of deciding what *not* to do, and
OpenImageIO is no exception.  We achieve power and elegance only by
making simplifying assumptions.  Among them:

* OpenImageIO only deals with ordinary 2D images, and to a limited
  extent 3D volumes, and image files that contain multiple (but
  finite) independent images within them.  OpenImageIO's support of "movie"
  files is limited to viewing them as a sequence of separate frames within
  the file, but not as movies per se (for example, no support for dealing
  with audio or synchronization).

* Pixel data are presented as 8- 16- or 32-bit int (signed or unsigned), 16-
  32- or 64-bit float.  NOTHING ELSE.  No :math:`<8` bit images, or pixel value
  boundaries that aren't byte boundaries.  Files with :math:`<8` will
  appear to the client application as 8-bit unsigned grayscale images.

* Only fully elaborated, non-compressed data are accepted
  and returned by the API.  Compression or special encodings are
  handled entirely within an OpenImageIO plugin.

* Color space is by default converted to grayscale or RGB. Non-spectral
  color models, such as
  XYZ, CMYK, or YUV, are converted to RGB upon reading. (There is a
  way to override this and ask for raw pixel values.)

* All color channels can be treated (by apps or readers/writers)
  as having the same data format (though there is a way to deal with
  per-channel formats for apps and readers/writers that truly need
  it).

* All image channels in a subimage are sampled at the same
  resolution.  For file formats that allow some channels to be
  subsampled, they will be automatically up-sampled to the highest
  resolution channel in the subimage.

* Color information is always in the order R, G, B, and the alpha
  channel, if any, always follows RGB, and Z channel (if any) always
  follows alpha.  So if a file actually stores ABGR, the plugin is
  expected to rearrange it as RGBA.


It's important to remember that these restrictions apply to data passed
through the APIs, not to the files themselves.  It's perfectly fine to
have an OpenImageIO plugin that supports YUV data, or 4 bits per channel, or
any other exotic feature.  You could even write a movie-reading
ImageInput (despite OpenImageIO's claims of not supporting movies) and
make it look to the client like it's just a series of images within the
file.  It's just that all the nonconforming details are handled entirely
within the OpenImageIO plugin and are not exposed through the main OpenImageIO
APIs.


Historical Origins
==================

OpenImageIO is the evolution of concepts and tools I've been working on
for two decades.

In the 1980's, every program I wrote that output images would have a
simple, custom format and viewer.  I soon graduated to using a standard
image file format (TIFF) with my own library implementation.  Then I
switched to Sam Leffler's stable and complete ``libtiff``.

In the mid-to-late-1990's, I worked at Pixar as one of the main
implementors of PhotoRealistic RenderMan, which had *display drivers*
that consisted of an API for opening files and outputting
pixels, and a set of DSO/DLL plugins that each implement image output
for each of a dozen or so different file format.  The plugins all
responded to the same API, so the renderer itself did not need to know
how to the details of the image file formats, and users could (in
theory, but rarely in practice) extend the set of output image formats
the renderer could use by writing their own plugins.

This was the seed of a good idea, but PRMan's display driver plugin API
was abstruse and hard to use.  So when I started Exluna in 2000, Matt
Pharr, Craig Kolb, and I designed a new API for image output for our own
renderer, Entropy.  This API, called "ExDisplay," was C++, and much
simpler, clearer, and easier to use than PRMan's display drivers.

NVIDIA's Gelato (circa 2002), whose early work was done by myself, Dan
Wexler, Jonathan Rice, and Eric Enderton, had an API
called "ImageIO."  ImageIO was *much*
more powerful and descriptive than ExDisplay, and had an
API for *reading* as well as writing images.  Gelato was not only
"format agnostic" for its image output, but also for its
image input (textures, image viewer, and other image utilities).
We released the API specification and headers (though not the
library implementation) using the BSD open source license, firmly
repudiating any notion that the API should be specific to NVIDIA or
Gelato.

For Gelato 3.0 (circa 2007), we refined ImageIO again (by this time,
Philip Nemec was also a major influence, in addition to Dan, Eric, and
myself [#]_).  This revision was not a major
overhaul but more of a fine tuning.  Our ideas were clearly approaching
stability.  But, alas, the Gelato project was canceled before Gelato 3.0
was released, and despite our prodding, NVIDIA executives would not open
source the full ImageIO code and related tools.

.. [#] Gelato as a whole had many other contributors; those I've named here
   are the ones I recall contributing to the design or implementation of the
   ImageIO APIs.

After I left NVIDIA, I was determined to recreate this work once
again -- and ONLY once more -- and release it as open source from the
start.  Thus, OpenImageIO was born.  I started with the existing Gelato
ImageIO specification and headers (which were BSD licensed all along),
and made further refinements since I had to rewrite the entire
implementation from scratch anyway.  I think the additional changes are
all improvements.

Over the years and with the help of dozens of open source contributors,
OpenImageIO has expanded beyond the original simple image format input/output
to encompass a wide range of image-related functionality. It has grown into
a foundational technology in many products and tools, particularly for the
production of animation and visual effects for motion pictures (but also
many other uses and fields). This is the software you have in your hands
today.


Acknowledgments
===============

OpenImageIO incorporates, depends upon, or dynamically links against several
other open source packages, detailed below. These other packages are all
distributed under licenses that allow them to be used by OpenImageIO. Where not
specifically noted, they are all using the same BSD license that OpenImageIO
uses. Any omissions or inaccuracies in this list are inadvertent and will be
fixed if pointed out. The full original licenses can be found in the
relevant parts of the source code.

OpenImageIO incorporates, distributes, or contains derived works of:

* The SHA-1 implemenation we use is public domain by Dominik Reichl  http://www.dominik-reichl.de/
* Squish © 2006 Simon Brown, MIT license. http://sjbrown.co.uk/?code=squish
* PugiXML © 2006-2009 by Arseny Kapoulkine (based on work © 2003 Kristen Wegner), MIT license. http://pugixml.org/
* DPX reader/writer © 2009 Patrick A. Palmer, BSD 3-clause license. https://github.com/patrickpalmer/dpx}
* lookup3 code by Bob Jenkins, Public Domain. http://burtleburtle.net/bob/c/lookup3.c
* xxhash © 2014 Yann Collet, BSD 2-clause license. https://github.com/Cyan4973/xxHash
* farmhash © 2014 Google, Inc., MIT license. https://github.com/google/farmhash
* gif.h by Charlie Tangora, public domain. https://github.com/ginsweater/gif-h
* KissFFT © 2003--2010 Mark Borgerding, 3-clause BSD license. https://github.com/mborgerding/kissfft
* CTPL thread pool © 2014 Vitaliy Vitsentiy, Apache License. https://github.com/vit-vit/CTPL
* Droid fonts from the Android SDK are distributed under the Apache license.  http://www.droidfonts.com
* function_view.h contains code derived from LLVM, © 2003--2018 University of Illinois at Urbana-Champaign. UIUC license (compatible with BSD)  http://llvm.org
* FindOpenVDB.cmake © 2015 Blender Foundation, BSD license.
* FindTBB.cmake © 2015 Justus Calvin, MIT license.
* fmt library © Victor Zverovich. MIT license. https://github.com/fmtlib/fmt
* UTF-8 decoder © 2008-2009 Bjoern Hoehrmann, MIT license. http://bjoern.hoehrmann.de/utf-8/decoder/dfa
* Base-64 encoder © René Nyffenegger, Zlib license. http://www.adp-gmbh.ch/cpp/common/base64.html
* stb_sprintf © 2017 Sean Barrett, public domain (or MIT license where that   may not apply). https://github.com/nothings/stb

OpenImageIO Has the following build-time dependencies (using
system installs, referencing as git submodules, or downloading as part of
the build), including link-time dependencies
against dynamic libraries:

* libtiff © 1988-1997 Sam Leffler and 1991-1997 Silicon Graphics, Inc.  http://www.remotesensing.org/libtiff
* IJG libjpeg © 1991-1998, Thomas G. Lane.  http://www.ijg.org
* OpenEXR, Ilmbase, and Half © 2006, Industrial Light & Magic. http://www.openexr.com
* zlib © 1995-2005 Jean-loup Gailly and Mark Adler. http://www.zlib.net
* libpng © 1998-2008 Glenn Randers-Pehrson, et al. http://www.libpng.org
* Boost © various authors. http://www.boost.org
* GLEW © 2002-2007 Milan Ikits, et al. http://glew.sourceforge.net
* Jasper © 2001-2006 Michael David Adams, et al. http://www.ece.uvic.ca/~mdadams/jasper/
* Ptex © 2009 Disney Enterprises, Inc. http://ptex.us
* Field3D © 2009 Sony Pictures Imageworks. http://sites.google.com/site/field3d/
* GIFLIB © 1997 Eric S. Raymond (MIT Licensed). http://giflib.sourceforge.net/
* LibRaw © 2008-2013 LibRaw LLC (LGPL, CDDL, and LibRaw licenses).  http://www.libraw.org/
* FFmpeg © various authors and distributed under LGPL. https://www.ffmpeg.org
* FreeType © 1996-2002, 2006 by David Turner, Robert Wilhelm, and Werner Lemberg. Distributed under the FreeType license (BSD compatible).
* JPEG-Turbo © 2009--2015 D. R. Commander. Distributed under the BSD license.
* pybind11 © 2016 Wenzel Jakob. Distributed under the BSD license. https://github.com/pybind/pybind11
* OpenVDB © 2012-2018 DreamWorks Animation LLC, Mozilla Public License 2.0. https://www.openvdb.org/
* Thread Building Blocks © Intel. Apache 2.0 license. https://www.threadingbuildingblocks.org/
* libheif \copyright 2017-2018 Struktur AG (LGPL). https://github.com/strukturag/libheif

