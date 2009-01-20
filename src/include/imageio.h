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


#ifndef IMAGEIO_H
#define IMAGEIO_H

#include <vector>
#include <string>
#include <limits>
#include <cmath>

#include "export.h"
#include "typedesc.h"   /* Needed for TypeDesc definition */
#include "paramlist.h"



namespace OpenImageIO {


/// Each imageio DSO/DLL should include this statement:
///      GELATO_EXPORT int FORMAT_imageio_version = Gelato::IMAGEIO_VERSION;
/// Applications using imageio DSO/DLL's should check this
/// variable, to avoid using DSO/DLL's compiled against
/// incompatible versions of this header file.
///
/// Version 3 added supports_rectangles() and write_rectangle() to
/// ImageOutput, and added stride parameters to the ImageInput read
/// routines.
/// Version 10 represents forking from NVIDIA's open source version,
/// with which we break backwards compatibility.
const int IMAGEIO_VERSION = 10;


/// Type we use for stride lengths.  It should be 64 bit on all 
/// supported systems.
typedef off_t stride_t;



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
    float quant_dither;       ///< dither amplitude for quantization

    /// Construct a QuantizationSpec from the quantization parameters.
    ///
    QuantizationSpec (int _black, int _white, int _min, int _max, float _dither)
        : quant_black(_black), quant_white(_white),
          quant_min(_min), quant_max(_max), quant_dither(_dither)
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
    enum Linearity {
        UnknownLinearity = 0, ///< Unknown which color space we're in
        Linear = 1,           ///< Color values are linear
        GammaCorrected = 2,   ///< Color values are gamma corrected
        sRGB = 3              ///< Color values are in sRGB
    };

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
    TypeDesc format;          ///< format of data in each channel
                              ///<   N.B., current implementation assumes that
                              ///<   all channels are the same data format.
    int nchannels;            ///< number of image channels, e.g., 4 for RGBA
    std::vector<std::string> channelnames;  ///< Names for each channel,
                                            ///< e.g., {"R","G","B","A"}
    int alpha_channel;        ///< Index of alpha channel, or -1 if not known
    int z_channel;            ///< Index of depth channel, or -1 if not known
    Linearity linearity;      ///< Value mapping of color channels
    float gamma;              ///< gamma exponent of the values in the file
    // quantize, dither are only used for ImageOutput
    int quant_black;          ///< quantization of black (0.0) level
    int quant_white;          ///< quantization of white (1.0) level
    int quant_min;            ///< quantization minimum clamp value
    int quant_max;            ///< quantization maximum clamp value
    float quant_dither;       ///< dither amplitude for quantization

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
    /// and dither and set all other channels to something reasonable.
    ImageSpec (TypeDesc format = TypeDesc::UNKNOWN);

    /// Constructor for simple 2D scanline image with nothing special.
    /// If fmt is not supplied, default to unsigned 8-bit data.
    ImageSpec (int xres, int yres, int nchans, TypeDesc fmt = TypeDesc::UINT8);

    /// Set the data format, and as a side effect set quantize & dither
    /// to good defaults for that format
    void set_format (TypeDesc fmt);

    /// Set the channelnames to reasonable defaults ("R", "G", "B", "A"),
    /// and alpha_channel, based on the number of channels.
    void default_channel_names ();

    /// Given quantization parameters, deduce a TypeDesc that can
    /// be used without unacceptable loss of significant bits.
    static TypeDesc format_from_quantize (int quant_black, int quant_white,
                                          int quant_min, int quant_max);

    ///
    /// Return the number of bytes for each channel datum
    size_t channel_bytes() const { return format.size(); }

    ///
    /// Return the number of bytes for each pixel (counting all channels)
    size_t pixel_bytes() const { return (size_t)nchannels * channel_bytes(); }

    ///
    /// Return the number of bytes for each scanline
    size_t scanline_bytes() const { return (size_t)width * pixel_bytes (); }

    ///
    /// Return the number of pixels for a tile
    size_t tile_pixels() const {
        return (size_t)tile_width * (size_t)tile_height * 
               std::max((size_t)tile_depth,(size_t)1);
    }

    ///
    /// Return the number of bytes for each a tile of the image
    size_t tile_bytes() const { return tile_pixels() * pixel_bytes (); }

    ///
    /// Return the number of pixels for an entire image
    size_t image_pixels() const {
        return (size_t)width * (size_t)height * 
               std::max((size_t)depth,(size_t)1);
    }

    ///
    /// Return the number of bytes for an entire image
    size_t image_bytes() const { return image_pixels() * pixel_bytes (); }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride, stride_t &zstride,
                             TypeDesc format,
                             int nchannels, int width, int height) {
        if (xstride == AutoStride)
            xstride = nchannels * format.size();
        if (ystride == AutoStride)
            ystride = xstride * width;
        if (zstride == AutoStride)
            zstride = ystride * height;
    }

    /// Adjust xstride, if set to AutoStride, to be the right size for
    /// contiguous data with the given format and channels.
    static void auto_stride (stride_t &xstride, TypeDesc format, int nchannels) {
        if (xstride == AutoStride)
            xstride = nchannels * format.size();
    }

    /// Add an optional attribute to the extra attribute list
    ///
    void attribute (const std::string &name, TypeDesc type,
                    int nvalues, const void *value);

    /// Add an int attribute
    void attribute (const std::string &name, unsigned int value) {
        attribute (name, TypeDesc::UINT, 1, &value);
    }

    void attribute (const std::string &name, int value) {
        attribute (name, TypeDesc::INT, 1, &value);
    }

    /// Add a float attribute
    void attribute (const std::string &name, float value) {
        attribute (name, TypeDesc::FLOAT, 1, &value);
    }

    /// Add a string attribute
    void attribute (const std::string &name, const char *value) {
        attribute (name, TypeDesc::STRING, 1, &value);
    }

    /// Add a string attribute
    void attribute (const std::string &name, const std::string &value) {
        attribute (name, value.c_str());
    }

    /// Search for a attribute of the given name in the list of extra
    /// attributes.
    ImageIOParameter * find_attribute (const std::string &name,
                                       TypeDesc searchtype=TypeDesc::UNKNOWN,
                                       bool casesensitive=false);
    const ImageIOParameter *find_attribute (const std::string &name,
                                            TypeDesc searchtype=TypeDesc::UNKNOWN,
                                            bool casesensitive=false) const;
};



/// Deprecated, Gelato <= 3 name for what we now call ImageSpec.
///
typedef ImageSpec ImageIOFormatSpec;



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

    /// Given the name of a 'feature', return whether this ImageIO
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
    ///    "volumes"        Does this format support "3D" pixel arrays?
    ///    "rewrite"        May the same scanline or tile be sent more than
    ///                       once?  (Generally, this will be true for
    ///                       plugins that implement interactive display.)
    ///    "empty"          Does this plugin support passing a NULL data
    ///                       pointer to write_scanline or write_tile to
    ///                       indicate that the entire data block is zero?
    ///
    /// Note that the earlier incarnation of ImageIO that shipped with
    /// NVIDIA's Gelato 2.x had individual supports_foo functions.  When
    /// forking OpenImageIO, we replaced this with a single entry point
    /// that accepts tokens.  The main advantage is that this allows
    /// future expansion of the set of possible queries without changing
    /// the API, adding new entry points, or breaking linkage
    /// compatibility.
    virtual bool supports (const std::string &feature) const = 0;

    /// Open file with given name, with resolution and other format data
    /// as given in newspec.  Open returns true for success, false for
    /// failure.  Note that it is legal to call open multiple times on
    /// the same file without a call to close(), if it supports
    /// multiimage and the append flag is true -- this is interpreted as
    /// appending images (such as for MIP-maps).
    virtual bool open (const std::string &name, const ImageSpec &newspec,
                       bool append=false) = 0;

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
    /// output format (as specified to open()) by this method.  Return
    /// true for success, false for failure.  It is a failure to call
    /// write_scanline with an out-of-order scanline if this format
    /// driver does not support random access.
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride=AutoStride)
        { return false; }

    /// Write the tile with (x,y,z) as the upper left corner.  (z is
    /// ignored for 2D non-volume images.)  The three stride values give
    /// the distance (in bytes) between successive pixels, scanlines,
    /// and volumetric slices, respectively.  Strides set to AutoStride
    /// imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  Return
    /// true for success, false for failure.  It is a failure to call
    /// write_tile with an out-of-order tile if this format driver does
    /// not support random access.
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride, stride_t zstride=AutoStride)
        { return false; }

    /// Write pixels whose x coords range over xmin..xmax (inclusive), y
    /// coords over ymin..ymax, and z coords over zmin...zmax.  The
    /// three stride values give the distance (in bytes) between
    /// successive pixels, scanlines, and volumetric slices,
    /// respectively.  Strides set to AutoStride imply 'contiguous'
    /// data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride * (xmax-xmin+1)
    ///     zstride == ystride * (ymax-ymin+1)
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  Return
    /// true for success, false for failure.  It is a failure to call
    /// write_rectangle for a format plugin that does not return true
    /// for supports("rectangles").
    virtual bool write_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax, TypeDesc format,
                                  const void *data, stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride, stride_t zstride=AutoStride)
        { return false; }

    /// Write the entire image of spec.width x spec.height x spec.depth
    /// pixels, with the given strides and in the desired format.
    /// Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// Depending on spec, write either all tiles or all scanlines.
    /// Assume that data points to a layout in row-major order.
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///   progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image 
    virtual bool write_image (TypeDesc format, const void *data,
                              stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                              stride_t zstride=AutoStride,
                              ProgressCallback progress_callback=NULL,
                              void *progress_callback_data=NULL);

    /// General message passing between client and image output server
    ///
    virtual int send_to_output (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// Return the current error string describing what went wrong if
    /// any of the public methods returned 'false' indicating an error.
    /// (Hopefully the implementation plugin called error() with a
    /// helpful error message.)
    std::string error_message () const { return m_errmessage; }

protected:
    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    void error (const char *format, ...);

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
                                stride_t xstride, stride_t ystride, stride_t zstride,
                                std::vector<unsigned char> &scratch);
    const void *to_native_rectangle (int xmin, int xmax, int ymin, int ymax,
                                     int zmin, int zmax, 
                                     TypeDesc format, const void *data,
                                     stride_t xstride, stride_t ystride, stride_t zstride,
                                     std::vector<unsigned char> &scratch);

protected:
    ImageSpec m_spec;           ///< format spec of the currently open image

private:
    std::string m_errmessage;   ///< private storage of error massage
};



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

    /// Return a reference to the image format specification of the
    /// current subimage.  Note that the contents of the spec are
    /// invalid before open() or after close().
    const ImageSpec &spec (void) const { return m_spec; }

    /// Close an image that we are totally done with.
    ///
    virtual bool close () = 0;

    /// Returns the index of the subimage that is currently being read.
    /// The first subimage (or the only subimage, if there is just one)
    /// is number 0.
    virtual int current_subimage (void) const { return 0; }

    /// Seek to the given subimage.  THe first subimage of the file has
    /// index 0.  Return true on success, false on failure (including
    /// that there is not a subimage with that index).  The new
    /// subimage's vital statistics are put in newspec (and also saved
    /// in this->spec).  The reader is expected to give the appearance
    /// of random access to subimages -- in other words, if it can't
    /// randomly seek to the given subimage, it should transparently
    /// close, reopen, and sequentially read through prior subimages.
    virtual bool seek_subimage (int index, ImageSpec &newspec) {
        return false;
    }

    /// Read the scanline that includes pixels (*,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified (z==0 for non-volume images).  The
    /// stride value gives the data spacing of adjacent pixels (in
    /// bytes).  Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
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

    /// Read the tile that includes pixels (*,y,z) into data, converting
    /// if necessary from the native data format of the file into the
    /// 'format' specified.  (z==0 for non-volume images.)  The stride
    /// values give the data spacing of adjacent pixels, scanlines, and
    /// volumetric slices (measured in bytes).  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// The reader is expected to give the appearance of random access
    /// -- in other words, if it can't randomly seek to the given tile,
    /// it should transparently close, reopen, and sequentially read
    /// through prior tiles.  The base ImageInput class has a default
    /// implementation that calls read_native_tile and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_tile (int x, int y, int z, TypeDesc format,
                            void *data, stride_t xstride=AutoStride,
                            stride_t ystride=AutoStride, stride_t zstride=AutoStride);

    ///
    /// Simple read_tile reads to contiguous float pixels.
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, TypeDesc::FLOAT, data, 
                          AutoStride, AutoStride, AutoStride);
    }

    /// Read the entire image of spec.width x spec.height x spec.depth
    /// pixels into data (which must already be sized large enough for
    /// the entire image) with the given strides and in the desired
    /// format.  Read tiles or scanlines automatically.  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///     progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image 
    virtual bool read_image (TypeDesc format, void *data,
                             stride_t xstride=AutoStride, stride_t ystride=AutoStride,
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
    /// read into contiguous memory (no strides).  It's up to the user to
    /// have enough space allocated and know what to do with the data.
    /// IT IS EXPECTED THAT EACH FORMAT PLUGIN WILL OVERRIDE THIS METHOD.
    virtual bool read_native_scanline (int y, int z, void *data) = 0;

    /// read_native_tile is just like read_tile, except that it
    /// keeps the data in the native format of the disk file and always
    /// read into contiguous memory (no strides).  It's up to the user to
    /// have enough space allocated and know what to do with the data.
    /// IT IS EXPECTED THAT EACH FORMAT PLUGIN WILL OVERRIDE THIS METHOD
    /// IF IT SUPPORTS TILED IMAGES.
    virtual bool read_native_tile (int x, int y, int z, void *data) {
        return false;
    }

    /// General message passing between client and image input server
    ///
    virtual int send_to_input (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// Return the current error string describing what went wrong if
    /// any of the public methods returned 'false' indicating an error.
    /// (Hopefully the implementation plugin called error() with a
    /// helpful error message.)
    std::string error_message () const { return m_errmessage; }

protected:
    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    void error (const char *format, ...);
    
protected:
    ImageSpec m_spec;          ///< format spec of the current open subimage

private:
    std::string m_errmessage;  ///< private storage of error massage
};



// Utility functions

/// If create() fails, there's no ImageInput/Output to use to
/// call error_message(), so call ImageIOErrorMessage().
DLLPUBLIC std::string error_message ();

/// Helper routine: quantize a value to an integer given the 
/// quantization parameters.
DLLPUBLIC int quantize (float value, int quant_black, int quant_white,
                        int quant_min, int quant_max, float quant_dither);

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
/// arbitrary types (specified by TypeDesc's), with optional gain
/// and gamma correction.  Return true if ok, false if it didn't know
/// how to do the conversion.
DLLPUBLIC bool convert_types (TypeDesc src_type, const void *src, 
                              TypeDesc dst_type, void *to, int n,
                              float gain=1, float gamma=1);

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
                              float gain=1, float gamma=1);

/// Add metadata to spec based on raw IPTC (International Press
/// Telecommunications Council) metadata in the form of an IIM
/// (Information Interchange Model).  Return true if all is ok, false if
/// the iptc block was somehow malformed.  This is a utility function to
/// make it easy for multiple format plugins to support embedding IPTC
/// metadata without having to duplicate functionality within each
/// plugin.  Note that IIM is actually considered obsolete and is
/// replaced by an XML scheme called XMP.
DLLPUBLIC bool decode_iptc_iim (const void *exif, int length, ImageSpec &spec);

/// Find all the IPTC-amenable metadata in spec and assemble it into an
/// IIM data block in iptc.  This is a utility function to make it easy
/// for multiple format plugins to support embedding IPTC metadata
/// without having to duplicate functionality within each plugin.  Note
/// that IIM is actually considered obsolete and is replaced by an XML
/// scheme called XMP.
DLLPUBLIC void encode_iptc_iim (ImageSpec &spec, std::vector<char> &iptc);

// to force correct linkage on some systems
DLLPUBLIC void _ImageIO_force_link ();

// Use privately only
DLLPUBLIC void error (const char *format, ...);

}; /* end namespace OpenImageIO */


#endif  // IMAGEIO_H
