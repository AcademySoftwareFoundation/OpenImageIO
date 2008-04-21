/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation.  All Rights Reserved.
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


#ifndef GELATO_IMAGEIO_H
#define GELATO_IMAGEIO_H

#include <vector>

#include "export.h"
#include "paramtype.h"   /* Needed for ParamBaseType definition */


namespace Gelato {


// Each imageio DSO/DLL should include this statement:
//      GELATO_EXPORT int imageio_version = Gelato::IMAGEIO_VERSION;
// Applications using imageio DSO/DLL's should check this
// variable, to avoid using DSO/DLL's compiled against
// incompatible versions of this header file.
//
// Version 3 added supports_rectangles() and write_rectangle() to
// ImageOutput, and added stride parameters to the ImageInput read
// routines.
const int IMAGEIO_VERSION = 3;


struct ImageIOFormatSpec {
    int x, y, z;                // image origin (0,0,0)
    int width, height, depth;   // width, height, depth (depth>1 for volume)
    int full_width;             // width of entire image (not just cropwindow)
    int full_height;
    int full_depth;
    int tile_width;             // tile size (0 if tiles are not supported)
    int tile_height;
    int tile_depth;
    int nchannels;              // e.g., 4 for RGBA
    ParamBaseType format;       // format of data in each channel
    std::vector<const char*> channelnames;  // e.g., {"R","G","B","A"}
    char unused[256];           // for future expansion
};



struct GELATO_PUBLIC ImageIOParameter {
    const char *name;
    ParamBaseType type;         // data type
    int nvalues;                // number of elements
    const void *value;          // array of values

    ImageIOParameter () {};
    ImageIOParameter (const char *name, ParamBaseType type,
                      int nvalues, const void *value)
        : name(name), type(type), nvalues(nvalues), value(value) {}
};


class GELATO_PUBLIC ImageOutput {
 public:
    ImageOutput ();
    virtual ~ImageOutput ();

    // Overrride these functions in your derived output class
    // to inform the client which formats are supported
    
    // Does this format know how to write tiled images?
    virtual bool supports_tiles (void) const { return false; }

    // Does this format know how to write tiles/scanlines in any order?
    // (false means that the client must send scanlines/tiles in image order)
    virtual bool supports_random_access (void) const { return false; }

    // Does this format know how to write multiple subimages in a single file?
    virtual bool supports_multiimage (void) const { return false; }

    // Does this format know how to write volumetric images?
    virtual bool supports_volumes (void) const { return false; }

    // Does this format accept the same scanline/tile more than once?
    // The main use is for interactive output.  supports_rewrite implies
    // that supports_random_access must also be true.
    virtual bool supports_rewrite (void) const { return false; }

    // Does this format support the passing of a NULL data pointer
    // in the write_scanline or write_tile functions to indicate
    // that the entire data block is zero?
    virtual bool supports_empty (void) const { return false; }

    // Does this format plugin support write_rectangle calls giving
    // totally arbitrary rectangles of pixels?
    virtual bool supports_rectangles (void) const { return false; }
   
    
    // Open file with given name, with resolution and other format data
    // as given in spec.  Additional param[0..nparams] contains additional
    // params specific to the format/driver (valid parameters should be
    // enumerated in the documentation for the output plugin).  Most
    // plugins should understand, at a minimum:
    //      "quantize"  int[4]       quantization black, white, min, max
    //      "gain"      float        gain multiplier
    //      "gamma"     float        gamma correction before quant
    //      "dither"    float        dither amount
    // Open returns true for success, false for failure.
    // Note that it is legal to call open multiple times on the same file
    // without a call to close(), if it supports multiimage and the 
    // append flag is true -- this is interpreted as appending images
    // (such as for MIP-maps).
    virtual bool open (const char *name, const ImageIOFormatSpec &spec,
        int nparams, const ImageIOParameter *param, bool append=false) = 0;

    // Write the scanline that includes pixels (*,y,z).  (z==0 for
    // non-volume images.)  The stride value gives the data layout:
    // one pixel to the "right" is xstride floats away.
    // Return true for success, false for failure.  It is a failure to
    // call write_scanline with an out-of-order scanline if this format
    // driver does not support random access.
    virtual bool write_scanline (int y, int z, const float *data, int xstride)
        { return false; }

    // Write the tile with (x,y,z) as the upper left corner.
    // (z==0 for non-volume images.)  The three stride values give the
    // data layout: one pixel to the "right" is xstride floats away,
    // one pixel "down" is ystride floats away, one pixel "in" (the
    // next volumetric slice) is zstride floats away.  Return true for
    // success, false for failure.  It is a failure to call write_tile
    // with an out-of-order tile if this format driver does not
    // support random access.
    virtual bool write_tile (int x, int y, int z,
        const float *data, int xstride,int ystride,int zstride) {return false;}

    // Write pixels whose x coords range over xmin..xmax (inclusive),
    // y coords over ymin..ymax, and z coords over zmin...zmax.
    // Stride measures the distance (in floats) between successive
    // pixels in x, y, and z.  Return true for success, false for
    // failure.  It is a failure to call write_rectangle for a format
    // plugin that does not return true for supports_rectangles().
    virtual bool write_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax, const float *data,
                                  int xstride, int ystride, int zstride)
        { return false; }

    // Close an image that we are totally done with.
    virtual bool close () = 0;

    // General message passing between client and image output server
    virtual int send_to_output (const char *format, ...);
    int send_to_client (const char *format, ...);

    // Helper routines to compute quantization and exposure
    static int quantize (float value, int black, int white,
                         int clamp_min, int clamp_max, float ditheramp);
    static float exposure (float value, float gain, float invgamma);

    // Error reporting
    void error (const char *message, ...);
    const char *error_message () { return errmessage; }


 private:
    char *errmessage;
};



class GELATO_PUBLIC ImageInput {
 public:
    ImageInput ();
    virtual ~ImageInput ();

    // Open file with given name.  Various file attributes are put in
    // newspec and a copy is also saved in this->spec.  From these
    // attributes, you can discern the resolution, if it's tiled,
    // number of channels, and native data format.  Return true if the
    // file was found and opened okay.
    virtual bool open (const char *name, ImageIOFormatSpec &newspec,
        int nparams, const ImageIOParameter *param) = 0;

    // Return the subimage number of the subimage we're currently reading.
    // Obviously, this is always 0 if there is only one subimage in the file.
    virtual int current_subimage (void) const { return 0; }

    // Seek to the given subimage.  Return true on success, false on
    // failure (including that there is not a subimage with that
    // index).  The new subimage's vital statistics are put in newspec
    // (and also saved in this->spec).  The reader is expected to give
    // the appearance of random access to subimages -- in other words,
    // if it can't randomly seek to the given subimage, it should
    // transparently close, reopen, and sequentially read through
    // prior subimages.
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec) {
        return false;
    }

    // Read the scanline that includes pixels (*,y,z) into contiguous
    // floats beginning at data.  (z==0 for non-volume images.)  The
    // stride value gives the data layout: one pixel to the "right" is
    // xstride floats away.  The reader is expected to give the
    // appearance of random access -- in other words, if it can't
    // randomly seek to the given scanline, it should transparently
    // close, reopen, and sequentially read through prior scanlines.
    // A good default implementation exists that calls read_native_scanline
    // and then does appropriate format conversion, so there's no reason 
    // for each format plugin to override this method.
    virtual bool read_scanline (int y, int z, float *data, int xstride);

    // Read the tile that includes pixel (x,y,z) into contiguous
    // floats beginning at data.  (z==0 for non-volume images.)  The
    // three stride values give the data layout: one pixel to the
    // "right" is xstride floats away, one pixel "down" is ystride
    // floats away, one pixel "in" (the next volumetric slice) is
    // zstride floats away.  The reader is expected to give the
    // appearance of random access -- in other words, if it can't
    // randomly seek to the given tile, it should transparently close,
    // reopen, and sequentially read through prior tiles.
    // A good default implementation exists that calls read_native_tile
    // and then does appropriate format conversion, so there's no reason 
    // for each format plugin to override this method.
    virtual bool read_tile (int x, int y, int z, float *data,
                            int xstride, int ystride, int zstride);

    // No supplied stride implies contiguous pixels.
    bool read_scanline (int y, int z, float *data) {
        return read_scanline (y, z, data, spec.nchannels);
    }
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, data, spec.nchannels,
                          spec.nchannels*spec.tile_width,
                          spec.nchannels*spec.tile_width*spec.tile_height);
    }

    // The two read_native routines are just like read_scanline and
    // read_tile, except that they keep the data in the native data
    // format of the disk file, without conversion to float, and
    // always read into contiguous memory (no strides).  It's
    // up to the user to know what to do with the data.
    // THESE ARE THE MOST IMPORTANT ROUTINES FOR EACH FORMAT TO OVERRIDE.
    virtual bool read_native_scanline (int y, int z, void *data) = 0;
    virtual bool read_native_tile (int x, int y, int z, void *data) {
        return false;
    }

    // Try to find a parameter from the currently opened image (or
    // subimage) and store its value in *val.  The user is responsible
    // for making sure that val points to the right type and amount of
    // storage for the parameter requested.  Caveat emptor.  Return
    // true if the plugin knows about that parameter and it's in the
    // file (and of the right type).  Return false (and don't modify
    // *val) if the param name is unrecognized, or doesn't have an
    // entry in the file.
    virtual bool get_parameter (const char *name, ParamType t, void *val) {
        return false;
    }

    // Close an image that we are totally done with.
    virtual bool close () = 0;

    // General message passing between client and image input server
    virtual int send_to_input (const char *format, ...);
    int send_to_client (const char *format, ...);

    // Error reporting
    void error (const char *message, ...);
    const char *error_message () { return errmessage; }
    
 private:
    char *errmessage;
 protected:
    ImageIOFormatSpec spec;  // spec of current subimage
};



// Utility functions

// Create an ImageOutput or ImageInput for the given format.  The
// searchpath is the path to find the plugin DSO's, not the searchpath
// to find the images themselves.  If MakeImageOutput isn't given a
// format name, it assumes that the file extension is the format name.
// MakeImageInput will try all imagio plugins to find one that reads
// the format of the input file.
GELATO_PUBLIC ImageOutput *MakeImageOutput (const char *filename,
                                     const char *searchpath);
GELATO_PUBLIC ImageOutput *MakeImageOutput (const char *filename,
                                     const char *format,
                                     const char *searchpath);
GELATO_PUBLIC ImageInput *MakeImageInput (const char *filename,
                                   const char *searchpath);

// If MakeImageInput/Output fail, there's no ImageInput/Output to use to
// call error_message(), so call ImageIOErrorMessage().
GELATO_PUBLIC const char *ImageIOErrorMessage ();

// Helper routines, used mainly by image output plugins, to search for
// entries in an array of ImageIOParameter.
// If the param[] array contains a param with the given name, type, and
// nvalues, then store its index within param[], and return its value.
// Else, set index to -1 and return NULL.
GELATO_PUBLIC const void *IOParamFindValue (const char *name, ParamBaseType type,
                                     int count, int& index, int nparams,
                                     const ImageIOParameter *param);
// If the param[] array contains a param with the given name, and a
// single string value, return that value.  Else return NULL.
GELATO_PUBLIC const char *IOParamFindString (const char *name, int nparams,
                                      const ImageIOParameter *param);

// to force correct linkage on some systems
GELATO_PUBLIC void _ImageIO_force_link ();


}; /* end namespace Gelato */


#endif  // GELATO_IMAGEIO_H
