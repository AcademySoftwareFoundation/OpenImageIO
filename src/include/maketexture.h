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


struct MaketxStats {
    MaketxStats() : writetime(0), resizetime(0), miptime(0), colorconverttime(0) {}
    double readtime;
    double writetime;
    double resizetime;
    double miptime;
    double colorconverttime;
};


struct MaketxParams {
    enum ConversionMode { MIPMAP, SHADOW, ENVLATLONG };
    
    MaketxParams() : verbose(false), nthreads(0), checknan(false), fixnan("none"), filter(NULL), conversionmode(MIPMAP), fov(90), fovcot(0), wrap("black"), pow2resize(false), Mcam(0.0f), Mscr(0.0f), separate(false), nomipmap(false), prman_metadata(false), constant_color_detect(false), monochrome_detect(false), opaque_detect(false), nchannels(-1), prman(false), oiio(false), unpremult(false) {}
    
    bool verbose;
    int nthreads;    // default: use #cores threads if available
    std::string channellist;
    bool checknan;
    std::string dataformatname;
    std::string fixnan; // none, black, box3
    Filter2D *filter;
    
    ConversionMode conversionmode;
    
    // Options controlling file metadata or mipmap creation
    float fov;
    float fovcot;
    std::string wrap;
    std::string swrap;
    std::string twrap;
    bool pow2resize;
    //float opaquewidth = 0;  // should be volume shadow epsilon
    Imath::M44f Mcam, Mscr;  // Initialize to 0
    bool separate;
    bool nomipmap;
    bool prman_metadata;
    bool constant_color_detect;
    bool monochrome_detect;
    bool opaque_detect;
    int nchannels;
    bool prman;
    bool oiio;
    
    bool unpremult;
    std::string incolorspace;
    std::string outcolorspace;
};


bool make_texturemap(ImageBuf &src, const char *dst_filename,
                     const MaketxParams &param, MaketxStats *stat=NULL);

}
OIIO_NAMESPACE_EXIT

#endif
