/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation and Copyright 2008 Larry Gritz.
// All Rights Reserved.
//
// Extensions by Larry Gritz based on open-source code by NVIDIA.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of NVIDIA nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// (This is the Modified BSD License)
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// ImageIO
// -------
//
// Provides a simple API that abstracts the reading and writing of
// images.  Subclasses, which may be found in DSO/DLL's, implement
// particular formats.
// 
/////////////////////////////////////////////////////////////////////////////


#ifndef IMAGEIO_H
#define IMAGEIO_H

#include <vector>
#include <string>
#include <limits>
#include <cmath>

#include "export.h"
#include "paramtype.h"   /* Needed for ParamBaseType definition */


namespace OpenImageIO {


/// Each imageio DSO/DLL should include this statement:
///      GELATO_EXPORT int imageio_version = Gelato::IMAGEIO_VERSION;
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



/// ImageIOParameter holds a parameter and a pointer to its value(s)
///
class DLLPUBLIC ImageIOParameter {
public:
    std::string name;           ///< data name
    ParamBaseType type;         ///< data type
    int nvalues;                ///< number of elements

    ImageIOParameter () : type(PT_UNKNOWN), nvalues(0), m_nonlocal(false) { }
    ImageIOParameter (const std::string &_name, ParamBaseType _type,
                      int _nvalues, const void *_value, bool _copy=true) {
        init (_name, _type, _nvalues, _value, _copy);
    }
    ImageIOParameter (const ImageIOParameter &p) {
        init (p.name, p.type, p.nvalues, p.data(), m_copy);
    }
    ~ImageIOParameter () { clear_value(); }
    const ImageIOParameter& operator= (const ImageIOParameter &p) {
        clear_value();
        init (p.name, p.type, p.nvalues, p.data(), m_copy);
        return *this;
    }
    const void *data () const {
        return m_nonlocal ? m_value.ptr : &m_value;
    }

private: 
    union {
        ptrdiff_t localval;
        const void *ptr;
    } m_value;

    bool m_copy, m_nonlocal;
    void init (const std::string &_name, ParamBaseType _type,
               int _nvalues, const void *_value, bool _copy=true);
    void clear_value();
    friend class ImageIOFormatSpec;
};



/// ImageIOFormatSpec describes the data format of an image --
/// dimensions, layout, number and meanings of image channels.
class DLLPUBLIC ImageIOFormatSpec {
public:
    int x, y, z;              ///< image origin (0,0,0)
    int width;                ///< width of the crop window containing data
    int height;               ///< height of the crop window containing data
    int depth;                ///< depth, >1 indicates a "volume"
    int full_width;           ///< width of entire image (not just cropwindow)
    int full_height;          ///< height of entire image (not just cropwindow)
    int full_depth;           ///< depth of entire image (not just cropwindow)
    int tile_width;           ///< tile width (0 for a non-tiled image)
    int tile_height;          ///< tile height (0 for a non-tiled image)
    int tile_depth;           ///< tile depth (0 for a non-tiled image,
                              ///<             1 for a non-volume image)
    ParamBaseType format;     ///< format of data in each channel
                              ///<   N.B., current implementation assumes that
                              ///<   all channels are the same data format.
    int nchannels;            ///< number of image channels, e.g., 4 for RGBA
    std::vector<std::string> channelnames;  ///< Names for each channel,
                                            ///< e.g., {"R","G","B","A"}
    int alpha_channel;        ///< Index of alpha channel, or -1 if not known
    int z_channel;            ///< Index of depth channel, or -1 if not known
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
    std::vector<ImageIOParameter> extra_attribs;  //< Additional attributes

    /// Constructor: given just the data format, set the default quantize
    /// and dither and set all other channels to something reasonable.
    ImageIOFormatSpec (ParamBaseType format = PT_UNKNOWN);

    /// Constructor for simple 2D scanline image with nothing special.
    /// If fmt is not supplied, default to unsigned 8-bit data.
    ImageIOFormatSpec (int xres, int yres, int nchans, 
                       ParamBaseType fmt = PT_UINT8);

    /// Set the data format, and as a side effect set quantize & dither
    /// to good defaults for that format
    void set_format (ParamBaseType fmt);

    /// Given quantization parameters, deduce a ParamBaseType that can
    /// be used without unacceptable loss of significant bits.
    static ParamBaseType format_from_quantize (int quant_black, int quant_white,
                                               int quant_min, int quant_max);

    ///
    /// Return the number of bytes for each channel datum
    size_t channel_bytes() const { return (size_t)ParamBaseTypeSize(format); }

    ///
    /// Return the number of bytes for each pixel (counting all channels)
    size_t pixel_bytes() const { return (size_t)nchannels * channel_bytes(); }

    ///
    /// Return the number of bytes for each scanline
    size_t scanline_bytes() const { return (size_t)width * pixel_bytes (); }

    ///
    /// Return the number of bytes for each scanline
    size_t tile_bytes() const {
        return (size_t)tile_width * (size_t)tile_height *
               (size_t)std::max(1,tile_depth) * pixel_bytes ();
    }

    ///
    /// Return the number of bytes for an entire image
    size_t image_bytes() const {
        return (size_t)width * (size_t)height *
               (size_t)std::max(depth,1) * pixel_bytes ();
    }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride, stride_t &zstride,
                             ParamBaseType format,
                             int nchannels, int width, int height) {
        if (xstride == AutoStride)
            xstride = nchannels * ParamBaseTypeSize(format);
        if (ystride == AutoStride)
            ystride = xstride * width;
        if (zstride == AutoStride)
            zstride = ystride * height;
    }

    /// Adjust xstride, if set to AutoStride, to be the right size for
    /// contiguous data with the given format and channels.
    static void auto_stride (stride_t &xstride, ParamBaseType format, int nchannels) {
        if (xstride == AutoStride)
            xstride = nchannels * ParamBaseTypeSize(format);
    }

    /// Add an optional attribute to the extra attribute list
    ///
    void attribute (const std::string &name, ParamBaseType type,
                    int nvalues, const void *value);

    /// Add an int attribute
    void attribute (const std::string &name, unsigned int value) {
        attribute (name, PT_UINT, 1, &value);
    }

    void attribute (const std::string &name, int value) {
        attribute (name, PT_INT, 1, &value);
    }

    /// Add a float attribute
    void attribute (const std::string &name, float value) {
        attribute (name, PT_FLOAT, 1, &value);
    }

    /// Add a string attribute
    void attribute (const std::string &name, const char *value) {
        attribute (name, PT_STRING, 1, &value);
    }

    /// Add a string attribute
    void attribute (const std::string &name, const std::string &value) {
        attribute (name, value.c_str());
    }

    /// Search for a attribute of the given name in the list of extra
    /// attributes.
    ImageIOParameter * find_attribute (const std::string &name,
                                       bool casesensitive=false);

private:
    // Special storage space for strings that go into extra_parameters
    std::vector< std::string > m_strings;
};



/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
class DLLPUBLIC ImageOutput {
public:
    /// Create an ImageOutput that will write to a file, with the format
    /// inferred from the extension of the file.  The plugin_searchpath
    /// parameter is a colon-separated list of directories to search for
    /// ImageIO plugin DSO/DLL's.  This just creates the ImageOutput, it
    /// does not open the file.
    static ImageOutput *create (const char *filename, 
                                const char *plugin_searchpath=NULL);

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
    ///                       within a tile?
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
    virtual bool supports (const char *feature) const = 0;

    /// Open file with given name, with resolution and other format data
    /// as given in newspec.  Open returns true for success, false for
    /// failure.  Note that it is legal to call open multiple times on
    /// the same file without a call to close(), if it supports
    /// multiimage and the append flag is true -- this is interpreted as
    /// appending images (such as for MIP-maps).
    virtual bool open (const char *name, const ImageIOFormatSpec &newspec,
                       bool append=false) = 0;

    /// Return a reference to the image format specification of the
    /// current subimage.  Note that the contents of the spec are
    /// invalid before open() or after close().
    const ImageIOFormatSpec &spec (void) const { return m_spec; }

    /// Close an image that we are totally done with.  This should leave
    /// the plugin in a state where it could open a new file safely,
    /// without having to destroy the writer.
    virtual bool close () = 0;

    /// Write a full scanline that includes pixels (*,y,z).  (z is
    /// ignored for 2D non-volume images.)  The stride value gives the
    /// distance between successive pixels (in bytes).  Strides set to
    /// AutoStride imply 'contiguous' data (i.e. xstride ==
    /// spec.nchannels*ParamBaseTypeSize(format)).  The data are
    /// automatically converted from 'format' to the actual output
    /// format (as specified to open()) by this method.  Return true for
    /// success, false for failure.  It is a failure to call
    /// write_scanline with an out-of-order scanline if this format
    /// driver does not support random access.
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, stride_t xstride=AutoStride)
        { return false; }

    /// Write the tile with (x,y,z) as the upper left corner.  (z is
    /// ignored for 2D non-volume images.)  The three stride values give
    /// the distance (in bytes) between successive pixels, scanlines,
    /// and volumetric slices, respectively.  Strides set to AutoStride
    /// imply 'contiguous' data (i.e.,
    /// xstride == spec.nchannels*ParamBaseTypeSize(format),
    /// ystride==xstride*spec.tile_width, zstride=ystride*spec.tile_height.
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  Return
    /// true for success, false for failure.  It is a failure to call
    /// write_tile with an out-of-order tile if this format driver does
    /// not support random access.
    virtual bool write_tile (int x, int y, int z, ParamBaseType format,
                             const void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride, stride_t zstride=AutoStride)
        { return false; }

    /// Write pixels whose x coords range over xmin..xmax (inclusive), y
    /// coords over ymin..ymax, and z coords over zmin...zmax.  The
    /// three stride values give the distance (in bytes between
    /// successive pixels, scanlines, and volumetric slices,
    /// respectively.  Strides set to AutoStride imply 'contiguous' data
    /// (i.e. xstride == spec.nchannels*ParamBaseTypeSize(format)),
    /// ystride==xstride*(xmax-xmin+1), zstride=ystride*(ymax-ymin+1).  The
    /// data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  Return
    /// true for success, false for failure.  It is a failure to call
    /// write_rectangle for a format plugin that does not return true
    /// for supports_rectangles().
    virtual bool write_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax, ParamBaseType format,
                                  const void *data, stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride, stride_t zstride=AutoStride)
        { return false; }

    /// Write the entire image of spec.width x spec.height x spec.depth
    /// pixels, with the given strides and in the desired format.
    /// Strides set to AutoStride imply 'contiguous' data (i.e.
    /// xstride==spec.nchannels*ParamBaseTypeSize(format),
    /// ystride==xstride*spec.width, zstride=ystride*spec.height).
    /// Depending on spec, write either all tiles or all scanlines.
    /// Assume that data points to a layout in row-major order.
    virtual bool write_image (ParamBaseType format, const void *data,
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
    const void *to_native_scanline (ParamBaseType format,
                                    const void *data, stride_t xstride,
                                    std::vector<unsigned char> &scratch);
    const void *to_native_tile (ParamBaseType format, const void *data,
                                stride_t xstride, stride_t ystride, stride_t zstride,
                                std::vector<unsigned char> &scratch);
    const void *to_native_rectangle (int xmin, int xmax, int ymin, int ymax,
                                     int zmin, int zmax, 
                                     ParamBaseType format, const void *data,
                                     stride_t xstride, stride_t ystride, stride_t zstride,
                                     std::vector<unsigned char> &scratch);

protected:
    ImageIOFormatSpec m_spec;   ///< format spec of the currently open image

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
    static ImageInput *create (const char *filename, 
                               const char *plugin_searchpath);

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
    virtual bool open (const char *name, ImageIOFormatSpec &newspec) = 0;

    /// Return a reference to the image format specification of the
    /// current subimage.  Note that the contents of the spec are
    /// invalid before open() or after close().
    const ImageIOFormatSpec &spec (void) const { return m_spec; }

    /// Close an image that we are totally done with.
    ///
    virtual bool close () = 0;

    /// Return the subimage number of the subimage we're currently
    /// reading.  Obviously, this is always 0 if there is only one
    /// subimage in the file.
    virtual int current_subimage (void) const { return 0; }

    /// Seek to the given subimage.  Return true on success, false on
    /// failure (including that there is not a subimage with that
    /// index).  The new subimage's vital statistics are put in newspec
    /// (and also saved in this->spec).  The reader is expected to give
    /// the appearance of random access to subimages -- in other words,
    /// if it can't randomly seek to the given subimage, it should
    /// transparently close, reopen, and sequentially read through prior
    /// subimages.
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec) {
        return false;
    }

    /// Read the scanline that includes pixels (*,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified (z==0 for non-volume images).  The
    /// stride value gives the data spacing of adjacent pixels (in
    /// bytes).  Strides set to AutoStride imply 'contiguous' data (i.e.
    /// xstride==spec.nchannels*ParamBaseTypeSize(format)).  The reader
    /// is expected to give the appearance of random access -- in other
    /// words, if it can't randomly seek to the given scanline, it
    /// should transparently close, reopen, and sequentially read
    /// through prior scanlines.  The base ImageInput class has a
    /// default implementation that calls read_native_scanline and then
    /// does appropriate format conversion, so there's no reason for
    /// each format plugin to override this method.
    virtual bool read_scanline (int y, int z, ParamBaseType format, void *data,
                                stride_t xstride=AutoStride);

    ///
    /// Simple read_scanline reads to contiguous float pixels.
    bool read_scanline (int y, int z, float *data) {
        return read_scanline (y, z, PT_FLOAT, data);
    }

    /// Read the tile that includes pixels (*,y,z) into data, converting
    /// if necessary from the native data format of the file into the
    /// 'format' specified.  (z==0 for non-volume images.)  The stride
    /// values give the data spacing of adjacent pixels, scanlines, and
    /// volumetric slices (measured in bytes).  Strides set to
    /// AutoStride imply 'contiguous' data (i.e.  xstride ==
    /// spec.nchannels*ParamBaseTypeSize(format), ystride ==
    /// xstride*spec.tile_width, zstride == ystride*spec.tile_height).
    /// The reader is expected to give the appearance of random access
    /// -- in other words, if it can't randomly seek to the given tile,
    /// it should transparently close, reopen, and sequentially read
    /// through prior tiles.  The base ImageInput class has a default
    /// implementation that calls read_native_tile and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_tile (int x, int y, int z, ParamBaseType format,
                            void *data, stride_t xstride=AutoStride,
                            stride_t ystride=AutoStride, stride_t zstride=AutoStride);

    ///
    /// Simple read_tile reads to contiguous float pixels.
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, PT_FLOAT, data, 
                          AutoStride, AutoStride, AutoStride);
    }

    /// Read the entire image of spec.width x spec.height x spec.depth
    /// pixels into data (which must already be sized large enough for
    /// the entire image) with the given strides and in the desired
    /// format.  Read tiles or scanlines automatically.  Strides set to
    /// AutoStride imply 'contiguous' data (i.e. xstride ==
    /// spec.nchannels*ParamBaseTypeSize(format),
    /// ystride==xstride*spec.width, zstride=ystride*spec.height).
    virtual bool read_image (ParamBaseType format, void *data,
                             stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride,
                             ProgressCallback progress_callback=NULL,
                             void *progress_callback_data=NULL);

    ///
    /// Simple read_image reads to contiguous float pixels.
    bool read_image (float *data) {
        return read_image (PT_FLOAT, data);
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
    ImageIOFormatSpec m_spec;  //< format spec of the current open subimage

private:
    std::string m_errmessage;  //< private storage of error massage
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
                              const void *src, ParamBaseType src_type, 
                              stride_t src_xstride, stride_t src_ystride,
                              stride_t src_zstride,
                              void *dst, ParamBaseType dst_type,
                              stride_t dst_xstride, stride_t dst_ystride,
                              stride_t dst_zstride,
                              float gain=1, float gamma=1);

// to force correct linkage on some systems
DLLPUBLIC void _ImageIO_force_link ();

// Use privately only
DLLPUBLIC void error (const char *format, ...);

}; /* end namespace OpenImageIO */


#endif  // IMAGEIO_H
