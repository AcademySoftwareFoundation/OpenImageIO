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

#include "export.h"
#include "paramtype.h"   /* Needed for ParamBaseType definition */


namespace OpenImageIO {


// Each imageio DSO/DLL should include this statement:
//      GELATO_EXPORT int imageio_version = Gelato::IMAGEIO_VERSION;
// Applications using imageio DSO/DLL's should check this
// variable, to avoid using DSO/DLL's compiled against
// incompatible versions of this header file.
//
// Version 3 added supports_rectangles() and write_rectangle() to
// ImageOutput, and added stride parameters to the ImageInput read
// routines.
// Version 10 represents forking from NVIDIA's open source version,
// with which we break backwards compatibility.
const int IMAGEIO_VERSION = 10;



/// ImageIOFormatSpec describes the data format of an image --
/// dimensions, layout, number and meanings of image channels.
struct DLLPUBLIC ImageIOFormatSpec {
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


    /// Constructor: given just the data format, set the default quantize
    /// and dither and set all other channels to something reasonable.
    ImageIOFormatSpec (ParamBaseType format = PT_UNKNOWN);

    /// Set the data format, and as a side effect set quantize & dither
    /// to good defaults for that format
    void set_format (ParamBaseType fmt);

    /// Given quantization parameters, deduce a ParamBaseType that can
    /// be used without unacceptable loss of significant bits.
    static ParamBaseType format_from_quantize (int quant_black, int quant_white,
                                               int quant_min, int quant_max);

    ///
    /// Return the number of bytes for each channel datum
    int channel_bytes() const { return ParamBaseTypeSize(format); }

    ///
    /// Return the number of bytes for each pixel (counting all channels)
    int pixel_bytes() const { return nchannels * channel_bytes(); }

    ///
    /// Return the number of bytes for each scanline
    int scanline_bytes() const { return width * pixel_bytes (); }

    ///
    /// Return the number of bytes for each scanline
    int tile_bytes() const { return tile_width * tile_height * pixel_bytes (); }
};



/// ImageIOParameter holds a parameter and a pointer to its value(s)
///
class DLLPUBLIC ImageIOParameter {
public:
    std::string name;           //< data name
    ParamBaseType type;         //< data type
    int nvalues;                //< number of elements
    const void *value;          //< array of values
    bool copy;                  //< make a copy instead of just a ptr

    ImageIOParameter () : type(PT_UNKNOWN), nvalues(0), value(NULL) {};
    ImageIOParameter (const std::string &_name, ParamBaseType _type,
                      int _nvalues, const void *_value, bool _copy=false) {
        init (_name, _type, _nvalues, _value, _copy);
    }
    ImageIOParameter (const ImageIOParameter &p) {
        init (p.name, p.type, p.nvalues, p.value, p.copy);
    }
    ~ImageIOParameter () { clear_value(); }
    const ImageIOParameter& operator= (const ImageIOParameter &p) {
        clear_value();
        init (p.name, p.type, p.nvalues, p.value, p.copy);
        return *this;
    }

private:
    bool m_copy;
    void init (const std::string &_name, ParamBaseType _type,
               int _nvalues, const void *_value, bool _copy=false);
    void clear_value();
};



/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
class DLLPUBLIC ImageOutput {
public:
    /// Create an ImageOutput that will write to a file in the given
    /// format.  The plugin_searchpath parameter is a colon-separated
    /// list of directories to search for ImageIO plugin DSO/DLL's.
    /// This just creates the ImageOutput, it does not open the file.
    static ImageOutput *create (const char *filename, const char *format,
                                const char *plugin_searchpath);

    /// Create an ImageOutput that will write to a file, with the format
    /// inferred from the extension of the file.  The plugin_searchpath
    /// parameter is a colon-separated list of directories to search for
    /// ImageIO plugin DSO/DLL's.  This just creates the ImageOutput, it
    /// does not open the file.
    static ImageOutput *create (const char *filename, 
                                const char *plugin_searchpath);

    
    ImageOutput () { }
    virtual ~ImageOutput () { }

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
    /// as given in spec.  Additional param[0..nparams-1] contains
    /// additional params specific to the format/driver (valid
    /// parameters should be enumerated in the documentation for the
    /// output plugin).  Open returns true for success, false for
    /// failure.  Note that it is legal to call open multiple times on
    /// the same file without a call to close(), if it supports
    /// multiimage and the append flag is true -- this is interpreted as
    /// appending images (such as for MIP-maps).
    virtual bool open (const char *name, const ImageIOFormatSpec &spec,
        int nparams, const ImageIOParameter *param, bool append=false) = 0;

    /// Close an image that we are totally done with.
    ///
    virtual bool close () = 0;

    /// Write a full scanline that includes pixels (*,y,z).  (z is
    /// ignored for 2D non-volume images.)  The stride value gives the
    /// data layout: one pixel to the "right" is xstride elements of the
    /// given format away.  The data are automatically converted from
    /// 'format' to the actual output format (as specified to open()) by
    /// this method.  Return true for success, false for failure.  It is
    /// a failure to call write_scanline with an out-of-order scanline
    /// if this format driver does not support random access.
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, int xstride)
        { return false; }

    /// Write the tile with (x,y,z) as the upper left corner.  (z is
    /// ignored for 2D non-volume images.)  The three stride values give
    /// the distance (in number of elements of the given format) between
    /// successive pixels, scanlines, and volumetric slices,
    /// respectively.  The data are automatically converted from
    /// 'format' to the actual output format (as specified to open()) by
    /// this method.  Return true for success, false for failure.  It is
    /// a failure to call write_tile with an out-of-order tile if this
    /// format driver does not support random access.
    virtual bool write_tile (int x, int y, int z,
                             ParamType format, const void *data,
                             int xstride, int ystride, int zstride)
        { return false; }

    /// Write pixels whose x coords range over xmin..xmax (inclusive), y
    /// coords over ymin..ymax, and z coords over zmin...zmax.  The
    /// three stride values give the distance (in number of elements of
    /// the given format) between successive pixels, scanlines, and
    /// volumetric slices, respectively.  The data are automatically
    /// converted from 'format' to the actual output format (as
    /// specified to open()) by this method.  Return true for success,
    /// false for failure.  It is a failure to call write_rectangle for
    /// a format plugin that does not return true for
    /// supports_rectangles().
    virtual bool write_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax,
                                  ParamType format, const void *data,
                                  int xstride, int ystride, int zstride)
        { return false; }

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
    void error (const char *message, ...);

    /// Helper routine: quantize a value to an integer given the 
    /// quantization parameters.
    static int quantize (float value, int quant_black, int quant_white,
                         int quant_min, int quant_max, float quant_dither);

    /// Helper routine: compute (gain*value)^invgamma
    ///
    static float exposure (float value, float gain, float invgamma);

    /// Helper routines used by write_* implementations: convert data (in
    /// the given format and stride) to the "native" format of the file
    /// (described by the 'spec' member variable), in contiguous order.
    /// This requires a scratch space to be passed in so that there are
    /// no memory leaks.  Returns a pointer to the native data, which may
    /// be the original data if it was already in native format and
    /// contiguous, or it may point to the scratch space if it needed to
    /// make a copy or do conversions.
    const void *to_native_scanline (ParamBaseType format,
                                    const void *data, int xstride,
                                    std::vector<unsigned char> &scratch);
    const void *to_native_tile (ParamBaseType format, const void *data,
                                int xstride, int ystride, int zstride,
                                std::vector<unsigned char> &scratch);
    const void *to_native_rectangle (int xmin, int xmax, int ymin, int ymax,
                                     int zmin, int zmax, 
                                     ParamBaseType format, const void *data,
                                     int xstride, int ystride, int zstride,
                                     std::vector<unsigned char> &scratch);

protected:
    ImageIOFormatSpec spec;     ///< format spec of the currently open image

private:
    std::string m_errmessage;   ///< private storage of error massage
};



class DLLPUBLIC ImageInput {
public:
    /// Create and return an ImageInput implementation that is willing
    /// to read the given format.  The plugin_searchpath parameter is a
    /// colon-separated list of directories to search for ImageIO plugin
    /// DSO/DLL's (not a searchpath for the image itself!).  First, it
    /// tries to find formatname.imageio.so (.dll on Windows).  If no
    /// such perfect match exists, it will try all imageio plugins it can
    /// find until it one reports that it can read the given format.
    static ImageInput *create_format (const char *formatname,
                                      const char *plugin_searchpath);

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

    /// Open file with given name.  Various file attributes are put in
    /// newspec and a copy is also saved in this->spec.  From these
    /// attributes, you can discern the resolution, if it's tiled,
    /// number of channels, and native data format.  Return true if the
    /// file was found and opened okay.
    virtual bool open (const char *name, ImageIOFormatSpec &newspec,
                       int nparams, const ImageIOParameter *param) = 0;

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
    /// into the 'format' specified.  (z==0 for non-volume images.)  The
    /// stride value gives the data spacing of adjacent pixels (measured
    /// in number of elements of 'format').  The reader is expected to
    /// give the appearance of random access -- in other words, if it
    /// can't randomly seek to the given scanline, it should
    /// transparently close, reopen, and sequentially read through prior
    /// scanlines.  The base ImageInput class has a default
    /// implementation that calls read_native_scanline and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_scanline (int y, int z, ParamBaseType format, void *data,
                                int xstride);

    /// Read the tile that includes pixels (*,y,z) into data, converting
    /// if necessary from the native data format of the file into the
    /// 'format' specified.  (z==0 for non-volume images.)  The stride
    /// values give the data spacing of adjacent pixels, scanlines, and
    /// volumetric slices (measured in number of elements of 'format').
    /// The reader is expected to give the appearance of random access
    /// -- in other words, if it can't randomly seek to the given tile,
    /// it should transparently close, reopen, and sequentially read
    /// through prior tiles.  The base ImageInput class has a default
    /// implementation that calls read_native_tile and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_tile (int x, int y, int z,
                            ParamBaseType format, float *data,
                            int xstride, int ystride, int zstride);

    ///
    /// Simple read_scanline reads to contiguous float pixels.
    bool read_scanline (int y, int z, float *data) {
        return read_scanline (y, z, PT_FLOAT, data, spec.nchannels);
    }

    ///
    /// Simple read_tile reads to contiguous float pixels.
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, PT_FLOAT, data, spec.nchannels,
                          spec.nchannels*spec.tile_width,
                          spec.nchannels*spec.tile_width*spec.tile_height);
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

    /// Try to find a parameter from the currently opened image (or
    /// subimage) and store its value in *val.  The user is responsible
    /// for making sure that val points to the right type and amount of
    /// storage for the parameter requested.  Caveat emptor.  Return
    /// true if the plugin knows about that parameter and it's in the
    /// file (and of the right type).  Return false (and don't modify
    /// *val) if the param name is unrecognized, or doesn't have an
    /// entry in the file.
    virtual bool get_parameter (std::string name, ParamType t, void *val) {
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
    void error (const char *message, ...);
    
protected:
    ImageIOFormatSpec spec;    //< format spec of the currently open image

private:
    std::string m_errmessage;  //< private storage of error massage
};



// Utility functions

/// If MakeImageInput/Output fail, there's no ImageInput/Output to use to
/// call error_message(), so call ImageIOErrorMessage().
DLLPUBLIC std::string ImageIOErrorMessage ();

/// Helper routines, used mainly by image output plugins, to search for
/// entries in an array of ImageIOParameter.
/// If the param[] array contains a param with the given name, type, and
/// nvalues, then store its index within param[], and return its value.
/// Else, set index to -1 and return NULL.
DLLPUBLIC const void *IOParamFindValue (const char *name, ParamBaseType type,
                                        int count, int& index, int nparams,
                                        const ImageIOParameter *param);
/// If the param[] array contains a param with the given name, and a
/// single string value, return that value.  Else return NULL.
DLLPUBLIC std::string IOParamFindString (const char *name, int nparams,
                                         const ImageIOParameter *param);

// to force correct linkage on some systems
DLLPUBLIC void _ImageIO_force_link ();


}; /* end namespace OpenImageIO */


#endif  // IMAGEIO_H
