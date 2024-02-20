// Comments that don't go anywhere in the source code, but help to
// generate the "main page" or other docs for doxygen.

// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


namespace OpenImageIO {

/**

@mainpage OpenImageIO Programmer Documentation

@section imageio_sec Basic Image Input/Output and Helper Classes

The core of OpenImageIO consists of simple classes for format-agnostic 
reading and writing of image files:

\li ImageInput is the public API that allows you to read images.
\li ImageOutput is the public API that allows you to write images.
\li ImageSpec is a helper class that describes an image (resolution, data
    format, metadata, etc.).

@section helper_sec Helper Classes used by the main APIs

\li TypeDesc describes data types and is used to describe channel
   formats and many other things throughout %OpenImageIO. (\ref typedesc.h)
\li ParamValueList and ParamValue are used for storing arbitrary name/data
    pairs and are used to store metadata.  (\ref paramlist.h)
\li \ref ustring is string class that's especially suitable for super fast
    string copying and comparison (== and !=) and that stores the character
    strings uniquely.  (\ref ustring.h)

@section ic_and_ts_sec Cached Images and Filtered Texture

\li ImageCache provides a way to access an unlimited number of images
    and amount of pixel data (thousands of files, hundreds of GB of
    pixels) using a read-on-demand cache system that uses as little as
    several tens of MB of RAM.
\li ImageBuf is a handy method for manipulating image pixels in memory,
    completely hiding even the details of reading, writing, and memory
    management (by being based internally upon ImageInput, ImageOutput, and
    ImageCache).
\li TextureSystem is an API for performing filtered anisotropic texture
    lookups (backed by ImageCache, so it can easily scale to essentially 
    unlimited number of texture files and/or pixel data).

@section gifts_sec Gifts for developers

These classes and utilities are not exposed through any of the public
OIIO APIs and are not necessary to understand even for people who are
integrating OIIO into their applications.  But we like them, feel that
they are pretty useful, and so distribute them so that OIIO users may
rely on them in their own apps.

\li \ref argparse.h : The ArgParse class that provides a really
    simple way to parse command-line arguments.
\li \ref dassert.h : Handy assertion macros.
\li errorhandler.h : An ErrorHandler class.
\li \ref filesystem.h : Platform-independent utilities for handling file
    names, etc.
\li \ref fmath.h : Lots of numeric utilities.
\li \ref hash.h : Definitions helpful for using hash maps and hash functions.
\li \ref paramlist.h : The ParamValue and ParamValueList classes.
\li \ref refcnt.h : A "mix-in" class for intrusive reference counting.
\li \ref strutil.h : String utilities.
\li \ref sysutil.h : Platform-independent OS, hardware, and system utilities.
\li \ref thread.h : Threading, mutexes, atomics, etc.
\li \ref timer.h : A simple \ref Timer class.
\li \ref typedesc.h : The TypeDesc class.
\li \ref ustring.h : The ustring class.
\li \ref varyingref.h : The VaryingRef template.

 */



};
