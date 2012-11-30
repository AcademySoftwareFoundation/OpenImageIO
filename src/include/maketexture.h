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


#ifndef OPENIMAGEIO_MAKETEXTURE_H
#define OPENIMAGEIO_MAKETEXTURE_H

#include <string>

#include <OpenEXR/ImathMatrix.h>

#include "filter.h"
#include "typedesc.h"
#include "imagebuf.h"



OIIO_NAMESPACE_ENTER
{


/// Texture construction parameters.
///
struct MaketxParams {
    /// Conversion type between source and destination images
    enum ConversionMode { MIPMAP, SHADOW, ENVLATLONG };
    
    MaketxParams () : verbose(false), separate(false), nomipmap(false),
                      prman_metadata(false), constant_color_detect(false),
                      monochrome_detect(false), opaque_detect(false),
                      checknan(false), computesha1(true), forcecompress(true),
                      forcefloat(true), prman(false), oiio(false), nthreads(0),
                      tile_width(64), tile_height(64), tile_depth(1),
                      fixnan("none"), filter(NULL), conversionmode(MIPMAP),
                      readlocalbytes(1024*1024*1024),
                      fov(90), fovcot(0), wrap("black"), pow2resize(false),
                      Mcam(0.0f), Mscr(0.0f), nchannels(-1), unpremult(false) {}
    
    /// Output informational messages in addition to errors
    bool verbose;
    
    /// Use planarconfig separate (default: contiguous)
    bool separate;

    /// Do not make multiple MIP-map levels
    bool nomipmap;
    
    /// dd prman specific metadata
    bool prman_metadata;
    
    /// Create 1-tile textures from constant color inputs
    bool constant_color_detect;
    
    /// Create 1-channel textures from monochrome inputs
    bool monochrome_detect;
    
    /// Drop alpha channel that is always 1.0
    bool opaque_detect;
    
    /// Check for NaN/Inf values (abort if found)
    bool checknan;
  
    /// Compute SHA-1 (default to true)
    bool computesha1;
  
    /// Force compression when saving the output file (default to true)
    bool forcecompress;
    
    /// Force use of float buffers when resizing (default to true, slower!)
    bool forcefloat;
    
    /// Use PRMan-safe settings for tile size, planarconfig, and metadata
    bool prman;
    
    /// Use OIIO-optimized settings for tile size, planarconfig, metadata,
    /// and constant-color optimizations
    bool oiio;
    
    /// default: use #cores threads if available
    int nthreads;

    /// Tile dimensions, overriden by oiio and prman flags (default: 64x64x1)
    int tile_width, tile_height, tile_depth;
    
    /// Specify the output file name
    std::string outputfilename;
    
    /// Specify output file format (default: guess from extension)
    std::string fileformatname;
    
    /// Specify the number of output image channels
    std::string channellist;
    
    /// Set the output data format to one of:
    ///    uint8, sint8, uint16, sint16, half, float
    std::string dataformatname;
    
    /// Attempt to fix NaN/Inf values in the image (options: none, black, box3)
    std::string fixnan;
    
    /// Filter to use when resizing the image (NULL -> 1x1 Box)
    Filter2D *filter;
    
    /// Specify the type of output texture to create
    ConversionMode conversionmode;
    
    /// Specify the threshold above which we use an ImageCache
    size_t readlocalbytes;
    
    
    // Options controlling file metadata or mipmap creation
    
    float fov;           ///< Field of view for envcube/shadcube/twofish
    float fovcot;        ///< Override the frame aspect ratio. Default is w/h
    std::string wrap;    ///< Specify wrap mode (black, clamp, periodic, mirror)
    std::string swrap;   ///< Specific s wrap mode separately
    std::string twrap;   ///< Specific t wrap mode separately
    bool pow2resize;     ///< Do not resize textures to power of 2 (deprecated)
    //float opaquewidth = 0;  // should be volume shadow epsilon
    Imath::M44f Mcam, Mscr; ///< Initialize to 0
    int nchannels;       ///< Specify the number of output image channels
    
    /// Custom mipmap dimensions
    std::vector<std::string> mipimages;
    
    /// Unpremultiply before color conversion, then premultiply
    /// after the color conversion.  You'll probably want to use this flag
    /// if your image contains an alpha channel
    bool unpremult;
    
    /// Apply a color space conversion to the image.
    /// If the output color space is not the same bit depth
    /// as input color space, it is your responsibility to set the data format
    /// to the proper bit depth using dataformatname.
    std::string incolorspace;
    std::string outcolorspace;  ///< Name of the output colorspace
};



/// Optional statistics for profiling
///
struct MaketxStats {
    MaketxStats () : readtime(0), writetime(0), resizetime(0), miptime(0),
                     colorconverttime(0) {}
    double readtime;
    double writetime;
    double resizetime;
    double miptime;
    double colorconverttime;
};



/// Create a new texture map outputfilename reading from ImageBuf.
///
DLLPUBLIC bool make_texturemap (ImageBuf &src, ImageOutput *out,
                                const MaketxParams &param,
                                MaketxStats *stat=NULL);



}
OIIO_NAMESPACE_EXIT

#endif
