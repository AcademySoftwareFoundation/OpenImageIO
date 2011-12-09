/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/////////////////////////////////////////////////////////////////////////////
/// \file
///
/// Provides a simple API that abstracts the reading and writing of
/// images.  Subclasses, which may be found in DSO/DLL's, implement
/// particular formats.
///
/////////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_IMAGEIO_H
#define OPENIMAGEIO_IMAGEIO_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::vector<T> and std::string members of the classes below.
#  pragma warning (disable : 4251)
#endif

#include <vector>
#include <string>
#include <limits>
#include <cmath>

#include "export.h"
#include "typedesc.h"   /* Needed for TypeDesc definition */
#include "paramlist.h"
#include "colortransfer.h"
#include "version.h"

OIIO_NAMESPACE_ENTER
{

/// Type we use for stride lengths.  This is only used to designate
/// pixel, scanline, tile, or image plane sizes in user-allocated memory,
/// so it doesn't need to represent sizes larger than can be malloced,
/// therefore ptrdiff_t seemed right.
typedef ptrdiff_t stride_t;


/// Type we use to express how many pixels (or bytes) constitute an image,
/// tile, or scanline.  Needs to be large enough to handle very big images
/// (which we presume could be > 4GB).
#if defined(LINUX64) || defined(_WIN64) /* add others if we know for sure size_t is ok */
typedef size_t imagesize_t;
#else
typedef unsigned long long imagesize_t;
#endif



/// Special value to indicate a stride length that should be
/// auto-computed.
const stride_t AutoStride = std::numeric_limits<stride_t>::min();



/// Pointer to a function called periodically by read_image and
/// write_image.  This can be used to implement progress feedback, etc.
/// It takes an opaque data pointer (passed to read_image/write_image)
/// and a float giving the portion of work done so far.  It returns a
/// bool, which if 'true' will STOP the read or write.
typedef bool (*ProgressCallback)(void *opaque_data, float portion_done);



typedef ParamValue ImageIOParameter;
typedef ParamValueList ImageIOParameterList;



class QuantizationSpec {
public:
    int quant_black;          ///< quantization of black (0.0) level
    int quant_white;          ///< quantization of white (1.0) level
    int quant_min;            ///< quantization minimum clamp value
    int quant_max;            ///< quantization maximum clamp value

    /// Construct a QuantizationSpec from the quantization parameters.
    ///
    QuantizationSpec (int _black, int _white, int _min, int _max)
        : quant_black(_black), quant_white(_white),
          quant_min(_min), quant_max(_max)
    { }

    /// Construct the "obvious" QuantizationSpec appropriate for the
    /// given data type.
    QuantizationSpec (TypeDesc _type);

    /// Return a special QuantizationSpec that is a marker that the
    /// recipient should use the default quantization for whatever data
    /// type it is dealing with.
    static QuantizationSpec quantize_default;
};



/// ImageSpec describes the data format of an image --
/// dimensions, layout, number and meanings of image channels.
class DLLPUBLIC ImageSpec {
public:
    int x, y, z;              ///< origin (upper left corner) of pixel data
    int width;                ///< width of the pixel data window
    int height;               ///< height of the pixel data window
    int depth;                ///< depth of pixel data, >1 indicates a "volume"
    int full_x;               ///< origin of the full (display) window
    int full_y;               ///< origin of the full (display) window
    int full_z;               ///< origin of the full (display) window
    int full_width;           ///< width of the full (display) window
    int full_height;          ///< height of the full (display) window
    int full_depth;           ///< depth of the full (display) window
    int tile_width;           ///< tile width (0 for a non-tiled image)
    int tile_height;          ///< tile height (0 for a non-tiled image)
    int tile_depth;           ///< tile depth (0 for a non-tiled image,
                              ///<             1 for a non-volume image)
    int nchannels;            ///< number of image channels, e.g., 4 for RGBA
    TypeDesc format;          ///< data format of the channels
    std::vector<TypeDesc> channelformats;   ///< Optional per-channel formats
    std::vector<std::string> channelnames;  ///< Names for each channel,
                                            ///< e.g., {"R","G","B","A"}
    int alpha_channel;        ///< Index of alpha channel, or -1 if not known
    int z_channel;            ///< Index of depth channel, or -1 if not known
    
    // quantize is used for ImageOutput
    int quant_black;          ///< quantization of black (0.0) level
    int quant_white;          ///< quantization of white (1.0) level
    int quant_min;            ///< quantization minimum clamp value
    int quant_max;            ///< quantization maximum clamp value

    /// The above contains all the information that is likely needed for
    /// every image file, and common to all formats.  Rather than bloat
    /// this structure, customize it for new formats, or break back
    /// compatibility as we think of new things, we provide extra_attribs
    /// as a holder for any other properties of the image.  The public
    /// functions attribute and find_attribute may be used to access
    /// these data.  Note, however, that the names and semantics of such
    /// extra attributes are plugin-dependent and are not enforced by
    /// the imageio library itself.
    ImageIOParameterList extra_attribs;  ///< Additional attributes

    /// Constructor: given just the data format, set the default quantize
    /// and set all other channels to something reasonable.
    ImageSpec (TypeDesc format = TypeDesc::UNKNOWN);

    /// Constructor for simple 2D scanline image with nothing special.
    /// If fmt is not supplied, default to unsigned 8-bit data.
    ImageSpec (int xres, int yres, int nchans, TypeDesc fmt = TypeDesc::UINT8);

    /// Set the data format, and as a side effect set quantize
    /// to good defaults for that format
    void set_format (TypeDesc fmt);

    /// Set the channelnames to reasonable defaults ("R", "G", "B", "A"),
    /// and alpha_channel, based on the number of channels.
    void default_channel_names ();

    /// Given quantization parameters, deduce a TypeDesc that can
    /// be used without unacceptable loss of significant bits.
    static TypeDesc format_from_quantize (int quant_black, int quant_white,
                                          int quant_min, int quant_max);

    /// Return the number of bytes for each channel datum, assuming they
    /// are all stored using the data format given by this->format.
    size_t channel_bytes() const { return format.size(); }

    /// Return the number of bytes needed for the single specified
    /// channel.  If native is false (default), compute the size of one
    /// channel of this->format, but if native is true, compute the size
    /// of the channel in terms of the "native" data format of that
    /// channel as stored in the file.
    size_t channel_bytes (int chan, bool native=false) const;

    /// Return the number of bytes for each pixel (counting all channels).
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    /// This will return std::numeric_limits<size_t>::max() in the
    /// event of an overflow where it's not representable in a size_t.
    size_t pixel_bytes (bool native=false) const;

    /// Return the number of bytes for just the subset of channels in
    /// each pixel described by [firstchan,firstchan+nchans).
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    /// This will return std::numeric_limits<size_t>::max() in the
    /// event of an overflow where it's not representable in a size_t.
    size_t pixel_bytes (int firstchan, int nchans, bool native=false) const;

    /// Return the number of bytes for each scanline.  This will return
    /// std::numeric_limits<imagesize_t>::max() in the event of an
    /// overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t scanline_bytes (bool native=false) const;

    /// Return the number of pixels for a tile.  This will return
    /// std::numeric_limits<imagesize_t>::max() in the event of an
    /// overflow where it's not representable in an imagesize_t.
    imagesize_t tile_pixels () const;

    /// Return the number of bytes for each a tile of the image.  This
    /// will return std::numeric_limits<imagesize_t>::max() in the event
    /// of an overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t tile_bytes (bool native=false) const;

    /// Return the number of pixels for an entire image.  This will
    /// return std::numeric_limits<imagesize_t>::max() in the event of
    /// an overflow where it's not representable in an imagesize_t.
    imagesize_t image_pixels () const;

    /// Return the number of bytes for an entire image.  This will
    /// return std::numeric_limits<image size_t>::max() in the event of
    /// an overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t image_bytes (bool native=false) const;

    /// Verify that on this platform, a size_t is big enough to hold the
    /// number of bytes (and pixels) in a scanline, a tile, and the
    /// whole image.  If this returns false, the image is much too big
    /// to allocate and read all at once, so client apps beware and check
    /// these routines for overflows!
    bool size_t_safe() const {
        const imagesize_t big = std::numeric_limits<size_t>::max();
        return image_bytes() < big && scanline_bytes() < big &&
            tile_bytes() < big;
    }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride,
                             stride_t &zstride, stride_t channelsize,
                             int nchannels, int width, int height) {
        if (xstride == AutoStride)
            xstride = nchannels * channelsize;
        if (ystride == AutoStride)
            ystride = xstride * width;
        if (zstride == AutoStride)
            zstride = ystride * height;
    }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride,
                             stride_t &zstride, TypeDesc format,
                             int nchannels, int width, int height) {
        auto_stride (xstride, ystride, zstride, format.size(),
                     nchannels, width, height);
    }

    /// Adjust xstride, if set to AutoStride, to be the right size for
    /// contiguous data with the given format and channels.
    static void auto_stride (stride_t &xstride, TypeDesc format, int nchannels) {
        if (xstride == AutoStride)
            xstride = nchannels * format.size();
    }

    /// Add an optional attribute to the extra attribute list
    ///
    void attribute (const std::string &name, TypeDesc type, const void *value);

    /// Add an optional attribute to the extra attribute list.
    ///
    void attribute (const std::string &name, TypeDesc type, const std::string &value);

    /// Add an unsigned int attribute
    ///
    void attribute (const std::string &name, unsigned int value) {
        attribute (name, TypeDesc::UINT, &value);
    }

    /// Add an int attribute
    ///
    void attribute (const std::string &name, int value) {
        attribute (name, TypeDesc::INT, &value);
    }

    /// Add a float attribute
    ///
    void attribute (const std::string &name, float value) {
        attribute (name, TypeDesc::FLOAT, &value);
    }

    /// Add a string attribute
    ///
    void attribute (const std::string &name, const char *value) {
        attribute (name, TypeDesc::STRING, &value);
    }

    /// Add a string attribute
    ///
    void attribute (const std::string &name, const std::string &value) {
        attribute (name, value.c_str());
    }

    /// Remove the specified attribute from the list of extra
    /// attributes. If not found, do nothing.
    void erase_attribute (const std::string &name,
                          TypeDesc searchtype=TypeDesc::UNKNOWN,
                          bool casesensitive=false);

    /// Search for a attribute of the given name in the list of extra
    /// attributes.
    ImageIOParameter * find_attribute (const std::string &name,
                                       TypeDesc searchtype=TypeDesc::UNKNOWN,
                                       bool casesensitive=false);
    const ImageIOParameter *find_attribute (const std::string &name,
                                            TypeDesc searchtype=TypeDesc::UNKNOWN,
                                            bool casesensitive=false) const;

    /// Simple way to get an integer attribute, with default provided.
    /// Automatically will return an int even if the data is really
    /// unsigned, short, or byte.
    int get_int_attribute (const std::string &name, int defaultval=0) const;

    /// Simple way to get a float attribute, with default provided.
    /// Automatically will return a float even if the data is really
    /// double or half.
    float get_float_attribute (const std::string &name,
                               float defaultval=0) const;

    /// Simple way to get a string attribute, with default provided.
    ///
    std::string get_string_attribute (const std::string &name,
                          const std::string &defaultval = std::string()) const;

    /// For a given parameter (in this ImageSpec's extra_attribs),
    /// format the value nicely as a string.  If 'human' is true, use
    /// especially human-readable explanations (units, or decoding of
    /// values) for certain known metadata.
    std::string metadata_val (const ImageIOParameter &p,
                              bool human=false) const;

    /// Convert ImageSpec class into XML string.
    ///
    std::string to_xml () const;

    /// Get an ImageSpec class from XML string.
    ///
    void from_xml (const char *xml);

    /// Helper function to verify that the given pixel range exactly covers
    /// a set of tiles.  Also returns false if the spec indicates that the
    /// image isn't tiled at all.
    bool valid_tile_range (int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend) {
        return (tile_width &&
                ((xbegin-x) % tile_width)  == 0 &&
                ((ybegin-y) % tile_height) == 0 &&
                ((zbegin-z) % tile_depth)  == 0 &&
                (((xend-x) % tile_width)  == 0 || (xend-x) == width) &&
                (((yend-y) % tile_height) == 0 || (yend-y) == height) &&
                (((zend-z) % tile_depth)  == 0 || (zend-z) == depth));
    }
};




/// ImageInput abstracts the reading of an image file in a file
/// format-agnostic manner.
class DLLPUBLIC ImageInput {
public:
    /// Create and return an ImageInput implementation that is willing
    /// to read the given file.  The plugin_searchpath parameter is a
    /// colon-separated list of directories to search for ImageIO plugin
    /// DSO/DLL's (not a searchpath for the image itself!).  This will
    /// actually just try every imageio plugin it can locate, until it
    /// finds one that's able to open the file without error.  This just
    /// creates the ImageInput, it does not open the file.
    static ImageInput *create (const std::string &filename,
                               const std::string &plugin_searchpath="");

    ImageInput () { }
    virtual ~ImageInput () { }

    /// Return the name of the format implemented by this class.
    ///
    virtual const char *format_name (void) const = 0;

    /// Open file with given name.  Various file attributes are put in
    /// newspec and a copy is also saved in this->spec.  From these
    /// attributes, you can discern the resolution, if it's tiled,
    /// number of channels, and native data format.  Return true if the
    /// file was found and opened okay.
    virtual bool open (const std::string &name, ImageSpec &newspec) = 0;

    /// Open file with given name, similar to open(name,newspec). The
    /// 'config' is an ImageSpec giving requests or special
    /// instructions.  ImageInput implementations are free to not
    /// respond to any such requests, so the default implementation is
    /// just to ignore config and call regular open(name,newspec).
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec & /*config*/) { return open(name,newspec); }

    /// Return a reference to the image format specification of the
    /// current subimage/MIPlevel.  Note that the contents of the spec
    /// are invalid before open() or after close().
    const ImageSpec &spec (void) const { return m_spec; }

    /// Given the name of a 'feature', return whether this ImageInput
    /// supports input of images with the given properties.
    /// Feature names that ImageIO plugins are expected to recognize
    /// include:
    ///    none currently supported, this is for later expansion
    ///
    /// Note that main advantage of this approach, versus having
    /// separate individual supports_foo() methods, is that this allows
    /// future expansion of the set of possible queries without changing
    /// the API, adding new entry points, or breaking linkage
    /// compatibility.
    virtual bool supports (const std::string & /*feature*/) const { return false; }

    /// Close an image that we are totally done with.
    ///
    virtual bool close () = 0;

    /// Returns the index of the subimage that is currently being read.
    /// The first subimage (or the only subimage, if there is just one)
    /// is number 0.
    virtual int current_subimage (void) const { return 0; }

    /// Returns the index of the MIPmap image that is currently being read.
    /// The highest-res MIP level (or the only level, if there is just
    /// one) is number 0.
    virtual int current_miplevel (void) const { return 0; }

    /// Seek to the given subimage and MIP-map level within the open
    /// image file.  The first subimage of the file has index 0, the
    /// highest-resolution MIP level has index 0.  Return true on
    /// success, false on failure (including that there is not a
    /// subimage or MIP level with the specified index).  The new
    /// subimage's vital statistics are put in newspec (and also saved
    /// in this->spec).  The reader is expected to give the appearance
    /// of random access to subimages and MIP levels -- in other words,
    /// if it can't randomly seek to the given subimage/level, it should
    /// transparently close, reopen, and sequentially read through prior
    /// subimages and levels.
    virtual bool seek_subimage (int subimage, int miplevel,
                                ImageSpec &newspec) {
        if (subimage == current_subimage() && miplevel == current_miplevel()) {
            newspec = spec();
            return true;
        }
        return false;
    }

    /// Seek to the given subimage -- backwards-compatible call that
    /// doesn't worry about MIP-map levels at all.
    bool seek_subimage (int subimage, ImageSpec &newspec) {
        return seek_subimage (subimage, 0 /* miplevel */, newspec);
    }

    /// Read the scanline that includes pixels (*,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified (z==0 for non-volume images).  The
    /// stride value gives the data spacing of adjacent pixels (in
    /// bytes).  Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// The reader is expected to give the appearance of random access --
    /// in other words, if it can't randomly seek to the given scanline,
    /// it should transparently close, reopen, and sequentially read
    /// through prior scanlines.  The base ImageInput class has a
    /// default implementation that calls read_native_scanline and then
    /// does appropriate format conversion, so there's no reason for
    /// each format plugin to override this method.
    virtual bool read_scanline (int y, int z, TypeDesc format, void *data,
                                stride_t xstride=AutoStride);

    ///
    /// Simple read_scanline reads to contiguous float pixels.
    bool read_scanline (int y, int z, float *data) {
        return read_scanline (y, z, TypeDesc::FLOAT, data);
    }

    /// Read multiple scanlines that include pixels (*,y,z) for all
    /// ybegin <= y < yend, into data, using the strides given and
    /// converting to the requested data format (unless format is
    /// TypeDesc::UNKNOWN, in which case pixels will be copied in the
    /// native data layout, including per-channel data formats).  This
    /// is analogous to read_scanline except that it may be used to read
    /// more than one scanline at a time (which, for some formats, may
    /// be able to be done much more efficiently or in parallel).
    virtual bool read_scanlines (int ybegin, int yend, int z,
                                 TypeDesc format, void *data,
                                 stride_t xstride=AutoStride,
                                 stride_t ystride=AutoStride);

    /// Read multiple scanlines that include pixels (*,y,z) for all
    /// ybegin <= y < yend, into data, using the strides given and
    /// converting to the requested data format (unless format is
    /// TypeDesc::UNKNOWN, in which case pixels will be copied in the
    /// native data layout, including per-channel data formats).  Only
    /// channels [firstchan,firstchan+nchans) will be read/copied
    /// (firstchan=0, nchans=spec.nchannels reads all scanlines,
    /// yielding equivalent behavior to the simpler variant of
    /// read_scanlines).
    virtual bool read_scanlines (int ybegin, int yend, int z,
                                 int firstchan, int nchans,
                                 TypeDesc format, void *data,
                                 stride_t xstride=AutoStride,
                                 stride_t ystride=AutoStride);

    /// Read the tile whose upper-left origin is (x,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified.  (z==0 for non-volume images.)  The
    /// stride values give the data spacing of adjacent pixels,
    /// scanlines, and volumetric slices (measured in bytes).  Strides
    /// set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// The reader is expected to give the appearance of random access
    /// -- in other words, if it can't randomly seek to the given tile,
    /// it should transparently close, reopen, and sequentially read
    /// through prior tiles.  The base ImageInput class has a default
    /// implementation that calls read_native_tile and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_tile (int x, int y, int z, TypeDesc format,
                            void *data, stride_t xstride=AutoStride,
                            stride_t ystride=AutoStride,
                            stride_t zstride=AutoStride);

    ///
    /// Simple read_tile reads to contiguous float pixels.
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, TypeDesc::FLOAT, data,
                          AutoStride, AutoStride, AutoStride);
    }

    /// Read the block of multiple tiles that include all pixels in
    /// [xbegin,xend) X [ybegin,yend) X [zbegin,zend), into data, using
    /// the strides given and converting to the requested data format
    /// (unless format is TypeDesc::UNKNOWN, in which case pixels will
    /// be copied in the native data layout, including per-channel data
    /// formats).  This is analogous to read_tile except that it may be
    /// used to read more than one tile at a time (which, for some
    /// formats, may be able to be done much more efficiently or in
    /// parallel).  The begin/end pairs must correctly delineate tile
    /// boundaries, with the exception that it may also be the end of
    /// the image data if the image resolution is not a whole multiple
    /// of the tile size.
    virtual bool read_tiles (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, TypeDesc format,
                             void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride);

    /// Read the block of multiple tiles that include all pixels in
    /// [xbegin,xend) X [ybegin,yend) X [zbegin,zend), into data, using
    /// the strides given and converting to the requested data format
    /// (unless format is TypeDesc::UNKNOWN, in which case pixels will
    /// be copied in the native data layout, including per-channel data
    /// formats).  Only channels [firstchan,firstchan+nchans) will be
    /// read/copied (firstchan=0, nchans=spec.nchannels reads all
    /// scanlines, yielding equivalent behavior to the simpler variant
    /// of read_tiles).
    virtual bool read_tiles (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, 
                             int firstchan, int nchans, TypeDesc format,
                             void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride);

    /// Read the entire image of spec.width x spec.height x spec.depth
    /// pixels into data (which must already be sized large enough for
    /// the entire image) with the given strides and in the desired
    /// format.  Read tiles or scanlines automatically.  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///     progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image
    virtual bool read_image (TypeDesc format, void *data,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride,
                             ProgressCallback progress_callback=NULL,
                             void *progress_callback_data=NULL);

    ///
    /// Simple read_image reads to contiguous float pixels.
    bool read_image (float *data) {
        return read_image (TypeDesc::FLOAT, data);
    }

    /// read_native_scanline is just like read_scanline, except that it
    /// keeps the data in the native format of the disk file and always
    /// reads into contiguous memory (no strides).  It's up to the user to
    /// have enough space allocated and know what to do with the data.
    /// IT IS EXPECTED THAT EACH FORMAT PLUGIN WILL OVERRIDE THIS METHOD.
    virtual bool read_native_scanline (int y, int z, void *data) = 0;

    /// read_native_scanlines is just like read_scanlines, except that
    /// it keeps the data in the native format of the disk file and
    /// always reads into contiguous memory (no strides).  It's up to
    /// the user to have enough space allocated and know what to do with
    /// the data.  If a format reader subclass does not override this
    /// method, the default implementation it will simply be a loop
    /// calling read_native_scanline for each scanline.
    virtual bool read_native_scanlines (int ybegin, int yend, int z,
                                        void *data);

    /// A variant of read_native_scanlines that reads only channels
    /// [firstchan,firstchan+nchans).  If a format reader subclass does
    /// not override this method, the default implementation will simply
    /// call the all-channel version of read_native_scanlines into a
    /// temporary buffer and copy the subset of channels.
    virtual bool read_native_scanlines (int ybegin, int yend, int z,
                                        int firstchan, int nchans,
                                        void *data);

    /// read_native_tile is just like read_tile, except that it
    /// keeps the data in the native format of the disk file and always
    /// read into contiguous memory (no strides).  It's up to the user to
    /// have enough space allocated and know what to do with the data.
    /// IT IS EXPECTED THAT EACH FORMAT PLUGIN WILL OVERRIDE THIS METHOD
    /// IF IT SUPPORTS TILED IMAGES.
    virtual bool read_native_tile (int x, int y, int z, void *data);

    /// read_native_tiles is just like read_tiles, except that it keeps
    /// the data in the native format of the disk file and always reads
    /// into contiguous memory (no strides).  It's up to the caller to
    /// have enough space allocated and know what to do with the data.
    /// If a format reader does not override this method, the default
    /// implementation it will simply be a loop calling read_native_tile
    /// for each tile in the block.
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend, void *data);

    /// A variant of read_native_tiles that reads only channels
    /// [firstchan,firstchan+nchans).  If a format reader subclass does
    /// not override this method, the default implementation will simply
    /// call the all-channel version of read_native_tiles into a
    /// temporary buffer and copy the subset of channels.
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend,
                                    int firstchan, int nchans, void *data);

    /// General message passing between client and image input server
    ///
    virtual int send_to_input (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror () const {
        std::string e = m_errmessage;
        m_errmessage.clear ();
        return e;
    }
    /// Deprecated
    ///
    std::string error_message () const { return geterror (); }

protected:
    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    void error (const char *format, ...) OPENIMAGEIO_PRINTF_ARGS(2,3);

protected:
    ImageSpec m_spec;  ///< format spec of the current open subimage/MIPlevel

private:
    mutable std::string m_errmessage;  ///< private storage of error massage
};




/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
class DLLPUBLIC ImageOutput {
public:
    /// Create an ImageOutput that will write to a file, with the format
    /// inferred from the extension of the name.  The plugin_searchpath
    /// parameter is a colon-separated list of directories to search for
    /// ImageIO plugin DSO/DLL's.  This just creates the ImageOutput, it
    /// does not open the file.
    static ImageOutput *create (const std::string &filename,
                                const std::string &plugin_searchpath="");

    ImageOutput () { }
    virtual ~ImageOutput () { }

    /// Return the name of the format implemented by this class.
    ///
    virtual const char *format_name (void) const = 0;

    // Overrride these functions in your derived output class
    // to inform the client which formats are supported

    /// Given the name of a 'feature', return whether this ImageOutput
    /// supports output of images with the given properties.
    /// Feature names that ImageIO plugins are expected to recognize
    /// include:
    ///    "tiles"          Is this format able to write tiled images?
    ///    "rectangles"     Does this plugin accept arbitrary rectangular
    ///                       pixel regions, not necessarily aligned to
    ///                       scanlines or tiles?
    ///    "random_access"  May tiles or scanlines be written in
    ///                       any order (false indicates that they MUST
    ///                       be in successive order).
    ///    "multiimage"     Does this format support multiple images
    ///                       within a file?
    ///    "mipmap"         Does this format support multiple resolutions
    ///                       for an image/subimage?
    ///    "volumes"        Does this format support "3D" pixel arrays?
    ///    "rewrite"        May the same scanline or tile be sent more than
    ///                       once?  (Generally, this will be true for
    ///                       plugins that implement interactive display.)
    ///    "empty"          Does this plugin support passing a NULL data
    ///                       pointer to write_scanline or write_tile to
    ///                       indicate that the entire data block is zero?
    ///    "channelformats" Does the plugin/format support per-channel
    ///                       data formats?
    ///    "displaywindow"  Does the format support display ("full") windows
    ///                        distinct from the pixel data window?
    ///
    /// Note that main advantage of this approach, versus having
    /// separate individual supports_foo() methods, is that this allows
    /// future expansion of the set of possible queries without changing
    /// the API, adding new entry points, or breaking linkage
    /// compatibility.
    virtual bool supports (const std::string & /*feature*/) const { return false; }

    enum OpenMode { Create, AppendSubimage, AppendMIPLevel };

    /// Open the file with given name, with resolution and other format
    /// data as given in newspec.  Open returns true for success, false
    /// for failure.  Note that it is legal to call open multiple times
    /// on the same file without a call to close(), if it supports
    /// multiimage and mode is AppendSubimage, or if it supports
    /// MIP-maps and mode is AppendMIPlevel -- this is interpreted as
    /// appending a subimage, or a MIP level to the current subimage,
    /// respectively.
    virtual bool open (const std::string &name, const ImageSpec &newspec,
                       OpenMode mode=Create) = 0;

    /// Return a reference to the image format specification of the
    /// current subimage.  Note that the contents of the spec are
    /// invalid before open() or after close().
    const ImageSpec &spec (void) const { return m_spec; }

    /// Close an image that we are totally done with.  This should leave
    /// the plugin in a state where it could open a new file safely,
    /// without having to destroy the writer.
    virtual bool close () = 0;

    /// Write a full scanline that includes pixels (*,y,z).  (z is
    /// ignored for 2D non-volume images.)  The stride value gives the
    /// distance between successive pixels (in bytes).  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Return true for success, false for failure.  It is a failure to
    /// call write_scanline with an out-of-order scanline if this format
    /// driver does not support random access.
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride=AutoStride);

    /// Write multiple scanlines that include pixels (*,y,z) for all
    /// ybegin <= y < yend, from data.  This is analogous to
    /// write_scanline except that it may be used to write more than one
    /// scanline at a time (which, for some formats, may be able to be
    /// done much more efficiently or in parallel).
    virtual bool write_scanlines (int ybegin, int yend, int z,
                                  TypeDesc format, const void *data,
                                  stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride);

    /// Write the tile with (x,y,z) as the upper left corner.  (z is
    /// ignored for 2D non-volume images.)  The three stride values give
    /// the distance (in bytes) between successive pixels, scanlines,
    /// and volumetric slices, respectively.  Strides set to AutoStride
    /// imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Return true for success, false for failure.  It is a failure to
    /// call write_tile with an out-of-order tile if this format driver
    /// does not support random access.
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride);

    /// Write the block of multiple tiles that include all pixels in
    /// [xbegin,xend) X [ybegin,yend) X [zbegin,zend).  This is
    /// analogous to write_tile except that it may be used to write more
    /// than one tile at a time (which, for some formats, may be able to
    /// be done much more efficiently or in parallel).
    /// The begin/end pairs must correctly delineate tile boundaries,
    /// with the exception that it may also be the end of the image data
    /// if the image resolution is not a whole multiple of the tile size.
    virtual bool write_tiles (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              const void *data, stride_t xstride=AutoStride,
                              stride_t ystride=AutoStride,
                              stride_t zstride=AutoStride);

    /// Write a rectangle of pixels given by the range
    ///   [xbegin,xend) X [ybegin,yend) X [zbegin,zend)
    /// The three stride values give the distance (in bytes) between
    /// successive pixels, scanlines, and volumetric slices,
    /// respectively.  Strides set to AutoStride imply 'contiguous'
    /// data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride * (xmax-xmin+1)
    ///     zstride == ystride * (ymax-ymin+1)
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  If
    /// format is TypeDesc::UNKNOWN, it will just copy pixels assuming
    /// they are already in the file's native data layout (including,
    /// possibly, per-channel data formats).
    ///
    /// Return true for success, false for failure.  It is a failure to
    /// call write_rectangle for a format plugin that does not return
    /// true for supports("rectangles").
    virtual bool write_rectangle (int xbegin, int xend, int ybegin, int yend,
                                  int zbegin, int zend, TypeDesc format,
                                  const void *data, stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride,
                                  stride_t zstride=AutoStride);

    /// Write the entire image of spec.width x spec.height x spec.depth
    /// pixels, with the given strides and in the desired format.
    /// Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// Depending on spec, write either all tiles or all scanlines.
    /// Assume that data points to a layout in row-major order.
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///   progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image
    virtual bool write_image (TypeDesc format, const void *data,
                              stride_t xstride=AutoStride,
                              stride_t ystride=AutoStride,
                              stride_t zstride=AutoStride,
                              ProgressCallback progress_callback=NULL,
                              void *progress_callback_data=NULL);

    /// Read the current subimage of 'in', and write it as the next
    /// subimage of *this, in a way that is efficient and does not alter
    /// pixel values, if at all possible.  Both in and this must be a
    /// properly-opened ImageInput and ImageOutput, respectively, and
    /// their current images must match in size and number of channels.
    /// Return true if it works ok, false if for some reason the
    /// operation wasn't possible.
    ///
    /// If a particular ImageOutput implementation does not supply a
    /// copy_image method, it will inherit the default implementation,
    /// which is to simply read scanlines or tiles from 'in' and write
    /// them to *this.  However, some ImageIO implementations may have a
    /// special technique for directly copying raw pixel data from the
    /// input to the output, when both input and output are the SAME
    /// file type and the same data format.  This can be more efficient
    /// than in->read_image followed by out->write_image, and avoids any
    /// unintended pixel alterations, especially for formats that use
    /// lossy compression.
    virtual bool copy_image (ImageInput *in);

    /// General message passing between client and image output server
    ///
    virtual int send_to_output (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror () const {
        std::string e = m_errmessage;
        m_errmessage.clear ();
        return e;
    }
    /// Deprecated
    ///
    std::string error_message () const { return geterror (); }

protected:
    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    void error (const char *format, ...) OPENIMAGEIO_PRINTF_ARGS(2,3);

    /// Helper routines used by write_* implementations: convert data (in
    /// the given format and stride) to the "native" format of the file
    /// (described by the 'spec' member variable), in contiguous order.
    /// This requires a scratch space to be passed in so that there are
    /// no memory leaks.  Returns a pointer to the native data, which may
    /// be the original data if it was already in native format and
    /// contiguous, or it may point to the scratch space if it needed to
    /// make a copy or do conversions.
    const void *to_native_scanline (TypeDesc format,
                                    const void *data, stride_t xstride,
                                    std::vector<unsigned char> &scratch);
    const void *to_native_tile (TypeDesc format, const void *data,
                                stride_t xstride, stride_t ystride,
                                stride_t zstride,
                                std::vector<unsigned char> &scratch);
    const void *to_native_rectangle (int xbegin, int xend, int ybegin, int yend,
                                     int zbegin, int zend,
                                     TypeDesc format, const void *data,
                                     stride_t xstride, stride_t ystride,
                                     stride_t zstride,
                                     std::vector<unsigned char> &scratch);

protected:
    ImageSpec m_spec;           ///< format spec of the currently open image

private:
    mutable std::string m_errmessage;   ///< private storage of error massage
};



// Utility functions

/// Retrieve the version of OpenImageIO for the library.  This is so
/// plugins can query to be sure they are linked against an adequate
/// version of the library.
DLLPUBLIC int openimageio_version ();

/// Special geterror() called after ImageInput::create or
/// ImageOutput::create, since if create fails, there's no object on
/// which call obj->geterror().
DLLPUBLIC std::string geterror ();

/// Deprecated
///
inline std::string error_message () { return geterror (); }

/// Helper routine: quantize a value to an integer given the
/// quantization parameters.
DLLPUBLIC int quantize (float value, int quant_black, int quant_white,
                        int quant_min, int quant_max);

/// Helper routine: compute (gain*value)^invgamma
///
inline float exposure (float value, float gain, float invgamma)
{
    if (invgamma != 1 && value >= 0)
        return powf (gain * value, invgamma);
    // Simple case - skip the expensive pow; also fall back to this
    // case for negative values, for which gamma makes no sense.
    return gain * value;
}

/// Helper function: convert contiguous arbitrary data between two
/// arbitrary types (specified by TypeDesc's).  Return true if ok, false
/// if it didn't know how to do the conversion.  If dst_type is UNKNWON,
/// it will be assumed to be the same as src_type.
DLLPUBLIC bool convert_types (TypeDesc src_type, const void *src,
                              TypeDesc dst_type, void *to, int n);

/// Helper function: convert contiguous arbitrary data between two
/// arbitrary types (specified by TypeDesc's), with optional transfer
/// function. Return true if ok, false if it didn't know how to do the
/// conversion.  If dst_type is UNKNWON, it will be assumed to be the
/// same as src_type.
DLLPUBLIC bool convert_types (TypeDesc src_type, const void *src,
                              TypeDesc dst_type, void *to, int n,
                              ColorTransfer *tfunc,
                              int alpha_channel = -1, int z_channel = -1);

/// Helper routine for data conversion: Convert an image of nchannels x
/// width x height x depth from src to dst.  The src and dst may have
/// different data formats and layouts.  Clever use of this function can
/// not only exchange data among different formats (e.g., half to 8-bit
/// unsigned), but also can copy selective channels, copy subimages,
/// etc.  If you're lazy, it's ok to pass AutoStride for any of the
/// stride values, and they will be auto-computed assuming contiguous
/// data.  Return true if ok, false if it didn't know how to do the
/// conversion.
DLLPUBLIC bool convert_image (int nchannels, int width, int height, int depth,
                              const void *src, TypeDesc src_type,
                              stride_t src_xstride, stride_t src_ystride,
                              stride_t src_zstride,
                              void *dst, TypeDesc dst_type,
                              stride_t dst_xstride, stride_t dst_ystride,
                              stride_t dst_zstride,
                              ColorTransfer *tfunc = NULL,
                              int alpha_channel = -1, int z_channel = -1);

/// Helper routine for data conversion: Copy an image of nchannels x
/// width x height x depth from src to dst.  The src and dst may have
/// different data layouts, but must have the same data type.  Clever
/// use of this function can change layouts or strides, copy selective
/// channels, copy subimages, etc.  If you're lazy, it's ok to pass
/// AutoStride for any of the stride values, and they will be
/// auto-computed assuming contiguous data.  Return true if ok, false if
/// it didn't know how to do the conversion.
DLLPUBLIC bool copy_image (int nchannels, int width, int height, int depth,
                           const void *src, stride_t pixelsize,
                           stride_t src_xstride, stride_t src_ystride,
                           stride_t src_zstride,
                           void *dst, stride_t dst_xstride,
                           stride_t dst_ystride, stride_t dst_zstride);

/// Decode a raw Exif data block and save all the metadata in an
/// ImageSpec.  Return true if all is ok, false if the exif block was
/// somehow malformed.  The binary data pointed to by 'exif' should
/// start with a TIFF directory header.
bool decode_exif (const void *exif, int length, ImageSpec &spec);

/// Construct an Exif data block from the ImageSpec, appending the Exif 
/// data as a big blob to the char vector.
void encode_exif (const ImageSpec &spec, std::vector<char> &blob);

/// Add metadata to spec based on raw IPTC (International Press
/// Telecommunications Council) metadata in the form of an IIM
/// (Information Interchange Model).  Return true if all is ok, false if
/// the iptc block was somehow malformed.  This is a utility function to
/// make it easy for multiple format plugins to support embedding IPTC
/// metadata without having to duplicate functionality within each
/// plugin.  Note that IIM is actually considered obsolete and is
/// replaced by an XML scheme called XMP.
DLLPUBLIC bool decode_iptc_iim (const void *iptc, int length, ImageSpec &spec);

/// Find all the IPTC-amenable metadata in spec and assemble it into an
/// IIM data block in iptc.  This is a utility function to make it easy
/// for multiple format plugins to support embedding IPTC metadata
/// without having to duplicate functionality within each plugin.  Note
/// that IIM is actually considered obsolete and is replaced by an XML
/// scheme called XMP.
DLLPUBLIC void encode_iptc_iim (const ImageSpec &spec, std::vector<char> &iptc);

/// Add metadata to spec based on XMP data in an XML block.  Return true
/// if all is ok, false if the xml was somehow malformed.  This is a
/// utility function to make it easy for multiple format plugins to
/// support embedding XMP metadata without having to duplicate
/// functionality within each plugin.
DLLPUBLIC bool decode_xmp (const std::string &xml, ImageSpec &spec);

/// Find all the relavant metadata (IPTC, Exif, etc.) in spec and
/// assemble it into an XMP XML string.  This is a utility function to
/// make it easy for multiple format plugins to support embedding XMP
/// metadata without having to duplicate functionality within each
/// plugin.  If 'minimal' is true, then don't encode things that would
/// be part of ordinary TIFF or exif tags.
DLLPUBLIC std::string encode_xmp (const ImageSpec &spec, bool minimal=false);

// to force correct linkage on some systems
DLLPUBLIC void _ImageIO_force_link ();

}
OIIO_NAMESPACE_EXIT

#endif  // OPENIMAGEIO_IMAGEIO_H
