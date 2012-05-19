README for OpenImageIO
======================


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
  IFF, Field3D, Ptex, Photoshop PSD, Wavefront RLA, SGI, and WebP.
  More are being developed all the time.

* An image viewer, iv, that is based on ImageIO plugins and therefore
  can read images of any format for which an appropriate plugin may be
  found.

* Several image tools based on these classes, including oiiotool
  (command-line format conversion and basic image processing), iinfo
  (print detailed info about images), iconvert (convert among formats,
  data types, or modify metadata), idiff (compare images), igrep
  (search images for matching metadata). Because these tools are based
  on ImageInput/ImageOutput, they work with any image formats for
  which ImageIO plugins are available.

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
  most useful computations you might want to do involving those images.



Licensing
---------

OpenImageIO is (c) Copyright 2008 by Larry Gritz and the other
contributors.  All Rights Reserved.

OpenImageIO is distributed using the modified BSD license.  Please read
the "LICENSE" file for the legal wording.  The plain English bottom line
is that OpenImageIO is free, as well as freely modifiable and
redistributable (in both source and binary form).  You may use part or
all of it in your own applications, whether proprietary or open, free or
commercial or not.  Using it in a commercial or proprietary application
DOES NOT obligate you to pay us, or to use any particular licensing
terms in your own application.


Web Resources
-------------

Main web page:      http://www.openimageio.org

GitHub page:        http://github.com/OpenImageIO/oiio

Mail lists:         http://lists.openimageio.org


Contact
-------

info@openimageio.org



Building and Installation
-------------------------

Please read the "INSTALL" file for detailed instructions on how to
build and install OpenImageIO.

Remember that you need to set the environment variable
IMAGEIO_LIBRARY_PATH to point to the 'lib' directory where OpenImageIO
is installed, or else it will not be able to find the plugins.


Documentation
-------------

The primary user and programmer documentation can be found in
src/doc/openimageio.pdf (in a source distribution) or in the
doc/openimageio.pdf file of an installed binary distribution.
