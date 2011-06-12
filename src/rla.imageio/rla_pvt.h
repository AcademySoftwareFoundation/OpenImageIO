/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

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

#ifndef OPENIMAGEIO_RLA_PVT_H
#define OPENIMAGEIO_RLA_PVT_H

#include "fmath.h"



OIIO_PLUGIN_NAMESPACE_BEGIN

namespace RLA_pvt {

    // type mappings
    typedef char CHAR;
    typedef short SHORT;
    typedef int LONG; // always 32-bit

    // code below comes from http://www.fileformat.info/format/wavefrontrla/egff.htm
    typedef struct _WavefrontHeader
    {
        SHORT WindowLeft;         /* Left side of the full image */
        SHORT WindowRight;        /* Right side of the full image */
        SHORT WindowBottom;       /* Bottom of the full image */
        SHORT WindowTop;          /* Top of the full image */
        SHORT ActiveLeft;         /* Left side of the viewable image */
        SHORT ActiveRight;        /* Right side of viewable image */
        SHORT ActiveBottom;       /* Bottom of the viewable image */
        SHORT ActiveTop;          /* Top of the viewable image */
        SHORT FrameNumber;        /* Frame sequence number */
        SHORT ColorChannelType;   /* Data format of the image channels */
        SHORT NumOfColorChannels; /* Number of color channels in image */
        SHORT NumOfMatteChannels; /* Number of matte channels in image */
        SHORT NumOfAuxChannels;   /* Number of auxiliary channels in image */
        SHORT Revision;           /* File format revision number */
        CHAR  Gamma[16];          /* Gamma setting of image */
        CHAR  RedChroma[24];      /* Red chromaticity */
        CHAR  GreenChroma[24];    /* Green chromaticity */
        CHAR  BlueChroma[24];     /* Blue chromaticity */
        CHAR  WhitePoint[24];     /* White point chromaticity*/
        LONG  JobNumber;          /* Job number ID of the file */
        CHAR  FileName[128];      /* Image file name */
        CHAR  Description[128];   /* Description of the file contents */
        CHAR  ProgramName[64];    /* Name of the program that created the file */
        CHAR  MachineName[32];    /* Name of machine used to create the file */
        CHAR  UserName[32];       /* Name of user who created the file */
        CHAR  DateCreated[20];    /* Date the file was created */
        CHAR  Aspect[24];         /* Aspect format of the image */
        CHAR  AspectRatio[8];     /* Aspect ratio of the image */
        CHAR  ColorChannel[32];   /* Format of color channel data */
        SHORT Field;              /* Image contains field-rendered data */
        CHAR  Time[12];           /* Length of time used to create the image
                                     file */
        CHAR  Filter[32];         /* Name of post-processing filter */
        SHORT NumOfChannelBits;   /* Number of bits in each color channel pixel */
        SHORT MatteChannelType;   /* Data format of the matte channels */
        SHORT NumOfMatteBits;     /* Number of bits in each matte channel pixel */
        SHORT AuxChannelType;     /* Data format of the auxiliary channels */
        SHORT NumOfAuxBits;       /* Number of bits in each auxiliary channel
                                     pixel */
        CHAR  AuxData[32];        /* Auxiliary channel data description */
        CHAR  Reserved[36];       /* Unused */
        LONG  NextOffset;         /* Location of the next image header in the
                                     file */
    } WAVEFRONT;

    /// format of the colour data
    enum rla_colour_channel_type {
        CT_BYTE = 0,
        CT_WORD = 1,
        CT_DWORD = 2,
        CT_FLOAT = 3
    };

};  // namespace RLA_pvt



OIIO_PLUGIN_NAMESPACE_END

#endif // OPENIMAGEIO_RLA_PVT_H
