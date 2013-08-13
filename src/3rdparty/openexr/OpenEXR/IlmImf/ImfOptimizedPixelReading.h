///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012, Autodesk, Inc.
// 
// All rights reserved.
//
// Implementation of IIF-specific file format and speed optimizations 
// provided by Innobec Technologies inc on behalf of Autodesk.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
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
///////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef INCLUDED_IMF_OPTIMIZED_PIXEL_READING_H
#define INCLUDED_IMF_OPTIMIZED_PIXEL_READING_H


// GCC and Visual Studio SSE2 compiler flags
#if defined __SSE2__ || (_MSC_VER >= 1300 && !_M_CEE_PURE)
    #define IMF_HAVE_SSE2 1
#endif


extern "C"
{
#if IMF_HAVE_SSE2
    #include <emmintrin.h>
    #include <mmintrin.h>
#endif
}

#include "ImfSystemSpecific.h"
#include <iostream>
#include "ImfChannelList.h"
#include "ImfFrameBuffer.h"
#include "ImfStringVectorAttribute.h"

OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_ENTER

class OptimizationMode
{
public:

    enum PixelFormat
    {
        PIXELFORMAT_RGB    = 0,     // pixel contains components 'R', 'G' and 'B' only
        PIXELFORMAT_RGBA   = 1,     // pixel contains components 'R', 'G', 'B' and 'A' only
        PIXELFORMAT_OTHER  = 2,     // pixel contains any other components
        
        NUM_PIXELFORMATS
    };

    enum MultiView
    {
        MULTIVIEW_MONO      = 0,    // image is mono (only one view)
        MULTIVIEW_STEREO    = 1,    // image is stereo (right and left views)
        
        NUM_MULTIVIEW_TYPES 
    };

    struct ChannelsInfo
    {
        PixelFormat     _format;
        MultiView       _multiview;
        int             _xStride;
        int             _ySampling;
        float           _alphaFillValueLeft;
        float           _alphaFillValueRight;

        // Retrieve the number of channels in the image/framebuffer
        int getNbChannels()
        {
            int nbChannels = 0;

            if (_format == PIXELFORMAT_RGB)
            {
                nbChannels = 3;
            }
            else if (_format == PIXELFORMAT_RGBA)
            {
                nbChannels = 4;
            }

            if (_multiview == MULTIVIEW_STEREO)
            {
                nbChannels *= 2;
            }

            return nbChannels;
        }

        // Constructor
        ChannelsInfo()
            : _format (PIXELFORMAT_OTHER)
        {
        }
    };

    OptimizationMode (ChannelsInfo source, ChannelsInfo destination)
        : _source(source)
        , _destination(destination)
    {
    }

    OptimizationMode()
    {
    }

    bool                _fillAlpha;

    // Optimization is for reading images, so the source will be the image file
    ChannelsInfo        _source;

    // The destination will be the framebuffer
    ChannelsInfo        _destination;
};

class IIFOptimizable
{

  public:
    enum ChannelMask
    {
        CHANNELMASK_OTHER        = 0x1,   // Channel contains data that is neither R, G, B, nor A

        CHANNELMASK_A            = 0x2,   // Channel contains alpha data (right in stereo or mono)
        CHANNELMASK_G            = 0x4,   // Channel contains green data (right in stereo or mono)
        CHANNELMASK_B            = 0x8,   // Channel contains blue data (right in stereo or mono)
        CHANNELMASK_R            = 0x10,  // Channel contains red data (right in stereo or mono)

        CHANNELMASK_ALEFT        = 0x20,  // Channel contains alpha data (left in stereo / unused in mono)
        CHANNELMASK_GLEFT        = 0x40,  // Channel contains green data (left in stereo / unused in mono)
        CHANNELMASK_BLEFT        = 0x80,  // Channel contains blue data (left in stereo / unused in mono)
        CHANNELMASK_RLEFT        = 0x100, // Channel contains red data (left in stereo / unused in mono)

        //
        // The following are used as shortcuts in order to determine whether
        // a channel is conform to the IIF definition of RGB or RGBA data
        //
        CHANNELMASK_RGB          = CHANNELMASK_R   |
                                   CHANNELMASK_G   |
                                   CHANNELMASK_B,
                                   
        CHANNELMASK_RGBA         = CHANNELMASK_RGB | CHANNELMASK_A,
                                   
        CHANNELMASK_RGB_LEFT      = CHANNELMASK_RLEFT |
                                    CHANNELMASK_GLEFT |
                                    CHANNELMASK_BLEFT ,
                                   
        CHANNELMASK_RGBA_LEFT     = CHANNELMASK_RGB_LEFT | CHANNELMASK_ALEFT,


        CHANNELMASK_RGB_STEREO   = CHANNELMASK_RGB | CHANNELMASK_RGB_LEFT,

        CHANNELMASK_RGBA_STEREO  = CHANNELMASK_RGB_STEREO |
                                   CHANNELMASK_A          |
                                   CHANNELMASK_ALEFT,

        CHANNELMASK_INVALID      = 0xFFFFFFFF   // channel contains invalid data
    };

   public :
    static int getMaskFromChannelName (const std::string& channelName,const StringVector * views)
    {
        
        bool left_hero = false; // set to 0 for left, 1 for right, -1 for no hero
        
        
        if( views && views->size()>0 && (*views)[0]=="left" )
        {    
            left_hero = true;
        }
        
       
        
        int channelMask = 0;

        // channel is a RGBA if it is from the right view
        // or if it is from the default view if that view is NOT left
        // (implies any hero view is either 'left' or 'right')
        if ( (!left_hero && channelName == "R") || channelName == "right.R")
        {
            channelMask = IIFOptimizable::CHANNELMASK_R;
        }
        else if ( (!left_hero && channelName == "G") || channelName == "right.G")
        {
            channelMask = IIFOptimizable::CHANNELMASK_G;
        }
        else if ( (!left_hero && channelName == "B") || channelName == "right.B")
        {
            channelMask = IIFOptimizable::CHANNELMASK_B;
        }
        else if ( (!left_hero && channelName == "A") || channelName == "right.A")
        {
            channelMask = IIFOptimizable::CHANNELMASK_A;
        }
        
        
        //
        // channel is left view RGBA if the name implies it is from the left view
        // or if it is 'default' where that is left or right
        //
        else if ( (left_hero && channelName=="R" ) || channelName == "left.R")
        {
            channelMask = IIFOptimizable::CHANNELMASK_RLEFT;
        }
        else if ( (left_hero && channelName=="G") || channelName == "left.G")
        {
            channelMask = IIFOptimizable::CHANNELMASK_GLEFT;
        }
        else if( (left_hero && channelName=="B" ) || channelName == "left.B")
        {
            channelMask = IIFOptimizable::CHANNELMASK_BLEFT;
        }
        else if( (left_hero && channelName=="A" ) || channelName == "left.A")
        {
            channelMask = IIFOptimizable::CHANNELMASK_ALEFT;
        }
        else
        {
            channelMask = IIFOptimizable::CHANNELMASK_OTHER;
        }

        return channelMask;
    }
};


#if IMF_HAVE_SSE2


//------------------------------------------------------------------------
// Test for SSE pointer alignemnt
//------------------------------------------------------------------------
EXR_FORCEINLINE
bool
isPointerSSEAligned (const void* EXR_RESTRICT pPointer)
{
    unsigned long trailingBits = ((unsigned long)pPointer) & 15;
    return trailingBits == 0;
}

//------------------------------------------------------------------------
// Load SSE from address into register
//------------------------------------------------------------------------
template<bool IS_ALIGNED>
EXR_FORCEINLINE
__m128i loadSSE (__m128i*& loadAddress)
{
    // throw exception :: this is not accepted
    return _mm_loadu_si128 (loadAddress);
}

template<>
EXR_FORCEINLINE
__m128i loadSSE<false> (__m128i*& loadAddress)
{
    return _mm_loadu_si128 (loadAddress);
}

template<>
EXR_FORCEINLINE
__m128i loadSSE<true> (__m128i*& loadAddress)
{
    return _mm_load_si128 (loadAddress);
}

//------------------------------------------------------------------------
// Store SSE from register into address
//------------------------------------------------------------------------
template<bool IS_ALIGNED>
EXR_FORCEINLINE
void storeSSE (__m128i*& storeAddress, __m128i& dataToStore)
{

}

template<>
EXR_FORCEINLINE
void
storeSSE<false> (__m128i*& storeAddress, __m128i& dataToStore)
{
    _mm_storeu_si128 (storeAddress, dataToStore);
}

template<>
EXR_FORCEINLINE
void
storeSSE<true> (__m128i*& storeAddress, __m128i& dataToStore)
{
    _mm_stream_si128 (storeAddress, dataToStore);
}



//------------------------------------------------------------------------
//
// Write to RGBA
//
//------------------------------------------------------------------------

//
// Using SSE intrinsics
//
template<bool READ_PTR_ALIGNED, bool WRITE_PTR_ALIGNED>
EXR_FORCEINLINE 
void writeToRGBASSETemplate 
    (__m128i*& readPtrSSERed,
     __m128i*& readPtrSSEGreen,
     __m128i*& readPtrSSEBlue,
     __m128i*& readPtrSSEAlpha,
     __m128i*& writePtrSSE,
     const size_t& lPixelsToCopySSE)
{
    for (size_t i = 0; i < lPixelsToCopySSE; ++i)
    {
        __m128i redRegister   = loadSSE<READ_PTR_ALIGNED> (readPtrSSERed);
        __m128i greenRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSEGreen);
        __m128i blueRegister  = loadSSE<READ_PTR_ALIGNED> (readPtrSSEBlue);
        __m128i alphaRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSEAlpha);

        __m128i redGreenRegister  = _mm_unpacklo_epi16 (redRegister,
                                                        greenRegister);
        __m128i blueAlphaRegister = _mm_unpacklo_epi16 (blueRegister,
                                                        alphaRegister);

        __m128i pixel12Register   = _mm_unpacklo_epi32 (redGreenRegister,
                                                        blueAlphaRegister);
        __m128i pixel34Register   = _mm_unpackhi_epi32 (redGreenRegister,
                                                        blueAlphaRegister);

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel12Register);
        ++writePtrSSE;

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel34Register);
        ++writePtrSSE;

        redGreenRegister  = _mm_unpackhi_epi16 (redRegister, greenRegister);
        blueAlphaRegister = _mm_unpackhi_epi16 (blueRegister, alphaRegister);

        pixel12Register   = _mm_unpacklo_epi32 (redGreenRegister,
                                                blueAlphaRegister);
        pixel34Register   = _mm_unpackhi_epi32 (redGreenRegister,
                                                blueAlphaRegister);

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel12Register);
        ++writePtrSSE;
        
        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel34Register);
        ++writePtrSSE;

        ++readPtrSSEAlpha;
        ++readPtrSSEBlue;
        ++readPtrSSEGreen;
        ++readPtrSSERed;
    }
}

//
// Not using SSE intrinsics.  This is still faster than the alternative
// because we have multiple read pointers and therefore we are able to
// take advantage of data locality for write operations.
//
EXR_FORCEINLINE 
void writeToRGBANormal (unsigned short*& readPtrRed,
                        unsigned short*& readPtrGreen,
                        unsigned short*& readPtrBlue,
                        unsigned short*& readPtrAlpha,
                        unsigned short*& writePtr,
                        const size_t& lPixelsToCopy)
{
    for (size_t i = 0; i < lPixelsToCopy; ++i)
    {
        *(writePtr++) = *(readPtrRed++);
        *(writePtr++) = *(readPtrGreen++);
        *(writePtr++) = *(readPtrBlue++);
        *(writePtr++) = *(readPtrAlpha++);
    }
}

//
// Determine which (template) version to use by checking whether pointers
// are aligned
//
EXR_FORCEINLINE 
void optimizedWriteToRGBA (unsigned short*& readPtrRed,
                           unsigned short*& readPtrGreen,
                           unsigned short*& readPtrBlue,
                           unsigned short*& readPtrAlpha,
                           unsigned short*& writePtr,
                           const size_t& pixelsToCopySSE,
                           const size_t& pixelsToCopyNormal)
{
    bool readPtrAreAligned = true;

    readPtrAreAligned &= isPointerSSEAligned(readPtrRed);
    readPtrAreAligned &= isPointerSSEAligned(readPtrGreen);
    readPtrAreAligned &= isPointerSSEAligned(readPtrBlue);
    readPtrAreAligned &= isPointerSSEAligned(readPtrAlpha);

    bool writePtrIsAligned = isPointerSSEAligned(writePtr);

    if (!readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBASSETemplate<false, false> ((__m128i*&)readPtrRed,
                                              (__m128i*&)readPtrGreen,
                                              (__m128i*&)readPtrBlue,
                                              (__m128i*&)readPtrAlpha,
                                              (__m128i*&)writePtr,
                                              pixelsToCopySSE);
    }
    else if (!readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBASSETemplate<false, true> ((__m128i*&)readPtrRed,
                                             (__m128i*&)readPtrGreen,
                                             (__m128i*&)readPtrBlue,
                                             (__m128i*&)readPtrAlpha,
                                             (__m128i*&)writePtr,
                                             pixelsToCopySSE);
    }
    else if (readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBASSETemplate<true, false> ((__m128i*&)readPtrRed,
                                             (__m128i*&)readPtrGreen,
                                             (__m128i*&)readPtrBlue,
                                             (__m128i*&)readPtrAlpha,
                                             (__m128i*&)writePtr,
                                             pixelsToCopySSE);
    }
    else if(readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBASSETemplate<true, true> ((__m128i*&)readPtrRed,
                                            (__m128i*&)readPtrGreen,
                                            (__m128i*&)readPtrBlue,
                                            (__m128i*&)readPtrAlpha,
                                            (__m128i*&)writePtr,
                                            pixelsToCopySSE);
    }

    writeToRGBANormal (readPtrRed, readPtrGreen, readPtrBlue, readPtrAlpha,
                       writePtr, pixelsToCopyNormal);
}



//------------------------------------------------------------------------
//
// Write to RGBA Fill A
//
//------------------------------------------------------------------------

//
// Using SSE intrinsics
//
template<bool READ_PTR_ALIGNED, bool WRITE_PTR_ALIGNED>
EXR_FORCEINLINE 
void
writeToRGBAFillASSETemplate (__m128i*& readPtrSSERed,
                             __m128i*& readPtrSSEGreen,
                             __m128i*& readPtrSSEBlue,
                             const unsigned short& alphaFillValue,
                             __m128i*& writePtrSSE,
                             const size_t& pixelsToCopySSE)
{
    const __m128i dummyAlphaRegister = _mm_set_epi16 (alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue,
                                                      alphaFillValue);

    for (size_t pixelCounter = 0; pixelCounter < pixelsToCopySSE; ++pixelCounter)
    {
        __m128i redRegister   = loadSSE<READ_PTR_ALIGNED> (readPtrSSERed);
        __m128i greenRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSEGreen);
        __m128i blueRegister  = loadSSE<READ_PTR_ALIGNED> (readPtrSSEBlue);

        __m128i redGreenRegister  = _mm_unpacklo_epi16 (redRegister,
                                                        greenRegister);
        __m128i blueAlphaRegister = _mm_unpacklo_epi16 (blueRegister,
                                                        dummyAlphaRegister);

        __m128i pixel12Register   = _mm_unpacklo_epi32 (redGreenRegister,
                                                        blueAlphaRegister);
        __m128i pixel34Register   = _mm_unpackhi_epi32 (redGreenRegister,
                                                        blueAlphaRegister);

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel12Register);
        ++writePtrSSE;

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel34Register);
        ++writePtrSSE;

        redGreenRegister  = _mm_unpackhi_epi16 (redRegister,
                                                greenRegister);
        blueAlphaRegister = _mm_unpackhi_epi16 (blueRegister,
                                                dummyAlphaRegister);

        pixel12Register   = _mm_unpacklo_epi32 (redGreenRegister,
                                                blueAlphaRegister);
        pixel34Register   = _mm_unpackhi_epi32 (redGreenRegister,
                                                blueAlphaRegister);

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel12Register);
        ++writePtrSSE;

        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, pixel34Register);
        ++writePtrSSE;

        ++readPtrSSEBlue;
        ++readPtrSSEGreen;
        ++readPtrSSERed;
    }
}

//
// Not using SSE intrinsics.  This is still faster than the alternative
// because we have multiple read pointers and therefore we are able to
// take advantage of data locality for write operations.
//
EXR_FORCEINLINE
void
writeToRGBAFillANormal (unsigned short*& readPtrRed,
                        unsigned short*& readPtrGreen,
                        unsigned short*& readPtrBlue,
                        const unsigned short& alphaFillValue,
                        unsigned short*& writePtr,
                        const size_t& pixelsToCopy)
{
    for (size_t i = 0; i < pixelsToCopy; ++i)
    {
        *(writePtr++) = *(readPtrRed++);
        *(writePtr++) = *(readPtrGreen++);
        *(writePtr++) = *(readPtrBlue++);
        *(writePtr++) = alphaFillValue;
    }
}

//
// Determine which (template) version to use by checking whether pointers
// are aligned.
//
EXR_FORCEINLINE 
void
optimizedWriteToRGBAFillA (unsigned short*& readPtrRed,
                           unsigned short*& readPtrGreen,
                           unsigned short*& readPtrBlue,
                           const unsigned short& alphaFillValue,
                           unsigned short*& writePtr,
                           const size_t& pixelsToCopySSE,
                           const size_t& pixelsToCopyNormal)
{
    bool readPtrAreAligned = true;

    readPtrAreAligned &= isPointerSSEAligned (readPtrRed);
    readPtrAreAligned &= isPointerSSEAligned (readPtrGreen);
    readPtrAreAligned &= isPointerSSEAligned (readPtrBlue);

    bool writePtrIsAligned = isPointerSSEAligned (writePtr);

    if (!readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBAFillASSETemplate<false, false> ((__m128i*&)readPtrRed,
                                                   (__m128i*&)readPtrGreen,
                                                   (__m128i*&)readPtrBlue,
                                                   alphaFillValue,
                                                   (__m128i*&)writePtr,
                                                   pixelsToCopySSE);
    }
    else if (!readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBAFillASSETemplate<false, true> ((__m128i*&)readPtrRed,
                                                  (__m128i*&)readPtrGreen,
                                                  (__m128i*&)readPtrBlue,
                                                  alphaFillValue,
                                                  (__m128i*&)writePtr,
                                                  pixelsToCopySSE);
    }
    else if (readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBAFillASSETemplate<true, false> ((__m128i*&)readPtrRed,
                                                  (__m128i*&)readPtrGreen,
                                                  (__m128i*&)readPtrBlue,
                                                  alphaFillValue,
                                                  (__m128i*&)writePtr,
                                                  pixelsToCopySSE);
    }
    else if (readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBAFillASSETemplate<true, true> ((__m128i*&)readPtrRed,
                                                 (__m128i*&)readPtrGreen,
                                                 (__m128i*&)readPtrBlue,
                                                 alphaFillValue,
                                                 (__m128i*&)writePtr,
                                                 pixelsToCopySSE);
    }

    writeToRGBAFillANormal (readPtrRed,
                            readPtrGreen, readPtrBlue, alphaFillValue,
                            writePtr, pixelsToCopyNormal);
}



//------------------------------------------------------------------------
//
// Write to RGB
//
//------------------------------------------------------------------------

//
// Using SSE intrinsics
//
template<bool READ_PTR_ALIGNED, bool WRITE_PTR_ALIGNED>
EXR_FORCEINLINE 
void
writeToRGBSSETemplate (__m128i*& readPtrSSERed,
                       __m128i*& readPtrSSEGreen,
                       __m128i*& readPtrSSEBlue,
                       __m128i*& writePtrSSE,
                       const size_t& pixelsToCopySSE)
{

    for (size_t pixelCounter = 0; pixelCounter < pixelsToCopySSE; ++pixelCounter)
    {
        //
        // Need to shuffle and unpack pointers to obtain my first register
        // We must save 8 pixels at a time, so we must have the following three registers at the end:
        // 1) R1 G1 B1 R2 G2 B2 R3 G3
        // 2) B3 R4 G4 B4 R5 G5 B5 R6
        // 3) G6 B6 R7 G7 B7 R8 G8 B8
        //
        __m128i redRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSERed);
        __m128i greenRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSEGreen);
        __m128i blueRegister = loadSSE<READ_PTR_ALIGNED> (readPtrSSEBlue);

        //
        // First register: R1 G1 B1 R2 G2 B2 R3 G3
        // Construct 2 registers and then unpack them to obtain our final result:
        //
        __m128i redGreenRegister  = _mm_unpacklo_epi16 (redRegister,
                                                        greenRegister);
        __m128i redBlueRegister   = _mm_unpacklo_epi16 (redRegister,
                                                        blueRegister);
        __m128i greenBlueRegister = _mm_unpacklo_epi16 (greenRegister,
                                                        blueRegister);

        // Left Part (R1 G1 B1 R2)
        __m128i quarterRight = _mm_shufflelo_epi16 (redBlueRegister,
                                                    _MM_SHUFFLE(3,0,2,1));
        __m128i halfLeft     = _mm_unpacklo_epi32 (redGreenRegister,
                                                   quarterRight);

        // Right Part (G2 B2 R3 G3)
        __m128i quarterLeft  = _mm_shuffle_epi32 (greenBlueRegister,
                                                 _MM_SHUFFLE(3,2,0,1));
        quarterRight         = _mm_shuffle_epi32 (redGreenRegister,
                                                 _MM_SHUFFLE(3,0,1,2));
        __m128i halfRight    = _mm_unpacklo_epi32 (quarterLeft, quarterRight);

        __m128i fullRegister = _mm_unpacklo_epi64 (halfLeft, halfRight);
        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, fullRegister);
        ++writePtrSSE;

        //
        // Second register: B3 R4 G4 B4 R5 G5 B5 R6
        //

        // Left Part (B3, R4, G4, B4)
        quarterLeft  = _mm_shufflehi_epi16 (redBlueRegister,
                                            _MM_SHUFFLE(0, 3, 2, 1));
        quarterRight = _mm_shufflehi_epi16 (greenBlueRegister,
                                            _MM_SHUFFLE(1, 0, 3, 2));
        halfLeft     = _mm_unpackhi_epi32 (quarterLeft, quarterRight);

        // Update the registers
        redGreenRegister  = _mm_unpackhi_epi16 (redRegister, greenRegister);
        redBlueRegister   = _mm_unpackhi_epi16 (redRegister, blueRegister);
        greenBlueRegister = _mm_unpackhi_epi16 (greenRegister, blueRegister);

        // Right Part (R5 G5 B5 R6)
        quarterRight = _mm_shufflelo_epi16 (redBlueRegister,
                                            _MM_SHUFFLE(3,0,2,1));
        halfRight    = _mm_unpacklo_epi32 (redGreenRegister, quarterRight);

        fullRegister = _mm_unpacklo_epi64 (halfLeft, halfRight);
        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, fullRegister);
        ++writePtrSSE;

        //
        // Third register: G6 B6 R7 G7 B7 R8 G8 B8
        //

        // Left part (G6 B6 R7 G7)
        quarterLeft  = _mm_shuffle_epi32 (greenBlueRegister,
                                          _MM_SHUFFLE(3,2,0,1));
        quarterRight = _mm_shuffle_epi32 (redGreenRegister,
                                          _MM_SHUFFLE(3,0,1,2));
        halfLeft     = _mm_unpacklo_epi32 (quarterLeft, quarterRight);

        // Right part (B7 R8 G8 B8)
        quarterLeft  = _mm_shufflehi_epi16 (redBlueRegister,
                                            _MM_SHUFFLE(0, 3, 2, 1));
        quarterRight = _mm_shufflehi_epi16 (greenBlueRegister,
                                            _MM_SHUFFLE(1, 0, 3, 2));
        halfRight    = _mm_unpackhi_epi32 (quarterLeft, quarterRight);

        fullRegister = _mm_unpacklo_epi64 (halfLeft, halfRight);
        storeSSE<WRITE_PTR_ALIGNED> (writePtrSSE, fullRegister);
        ++writePtrSSE;

        //
        // Increment read pointers
        //
        ++readPtrSSEBlue;
        ++readPtrSSEGreen;
        ++readPtrSSERed;
    }
}

//
// Not using SSE intrinsics.  This is still faster than the alternative
// because we have multiple read pointers and therefore we are able to
// take advantage of data locality for write operations.
//
EXR_FORCEINLINE 
void
writeToRGBNormal (unsigned short*& readPtrRed,
                  unsigned short*& readPtrGreen,
                  unsigned short*& readPtrBlue,
                  unsigned short*& writePtr,
                  const size_t& pixelsToCopy)
{
    for (size_t i = 0; i < pixelsToCopy; ++i)
    {
        *(writePtr++) = *(readPtrRed++);
        *(writePtr++) = *(readPtrGreen++);
        *(writePtr++) = *(readPtrBlue++);
    }
}

//
// Determine which (template) version to use by checking whether pointers
// are aligned
//
EXR_FORCEINLINE 
void optimizedWriteToRGB (unsigned short*& readPtrRed,
                          unsigned short*& readPtrGreen,
                          unsigned short*& readPtrBlue,
                          unsigned short*& writePtr,
                          const size_t& pixelsToCopySSE,
                          const size_t& pixelsToCopyNormal)
{
    bool readPtrAreAligned = true;

    readPtrAreAligned &= isPointerSSEAligned(readPtrRed);
    readPtrAreAligned &= isPointerSSEAligned(readPtrGreen);
    readPtrAreAligned &= isPointerSSEAligned(readPtrBlue);

    bool writePtrIsAligned = isPointerSSEAligned(writePtr);

    if (!readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBSSETemplate<false, false> ((__m128i*&)readPtrRed,
                                             (__m128i*&)readPtrGreen,
                                             (__m128i*&)readPtrBlue,
                                             (__m128i*&)writePtr,
                                             pixelsToCopySSE);
    }
    else if (!readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBSSETemplate<false, true> ((__m128i*&)readPtrRed,
                                            (__m128i*&)readPtrGreen,
                                            (__m128i*&)readPtrBlue,
                                            (__m128i*&)writePtr,
                                            pixelsToCopySSE);
    }
    else if (readPtrAreAligned && !writePtrIsAligned)
    {
        writeToRGBSSETemplate<true, false> ((__m128i*&)readPtrRed,
                                            (__m128i*&)readPtrGreen,
                                            (__m128i*&)readPtrBlue,
                                            (__m128i*&)writePtr,
                                            pixelsToCopySSE);
    }
    else if (readPtrAreAligned && writePtrIsAligned)
    {
        writeToRGBSSETemplate<true, true> ((__m128i*&)readPtrRed,
                                           (__m128i*&)readPtrGreen,
                                           (__m128i*&)readPtrBlue,
                                           (__m128i*&)writePtr,
                                           pixelsToCopySSE);
    }


    writeToRGBNormal (readPtrRed, readPtrGreen, readPtrBlue,
                      writePtr, pixelsToCopyNormal);
}


EXR_FORCEINLINE 
OptimizationMode::ChannelsInfo
getOptimizationInfo(const ChannelList & list,const StringVector * views) 
{
    OptimizationMode::ChannelsInfo optimizationInfo;
    optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;

    int fullMask = 0;
    
    for (ChannelList::ConstIterator j = list.begin();
    j != list.end();
    ++j)
    {
        // convert the channel name into a string for easy manipulation
        // find out the last element after the last dot
        std::string channelName = j.name();
        fullMask |= IIFOptimizable::getMaskFromChannelName(channelName,views);
    }
    
    switch (fullMask)
    {
        case IIFOptimizable::CHANNELMASK_RGB : case IIFOptimizable::CHANNELMASK_RGB_LEFT:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGB;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_MONO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGBA : case IIFOptimizable::CHANNELMASK_RGBA_LEFT:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGBA;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_MONO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGB_STEREO:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGB;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_STEREO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGBA_STEREO:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGBA;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_STEREO;
            break;
            
        default:
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;   
            break;
    }
    
    return optimizationInfo;
}

EXR_FORCEINLINE 
OptimizationMode::ChannelsInfo
getOptimizationInfo(const FrameBuffer & framebuffer,const StringVector * views)
{
    OptimizationMode::ChannelsInfo optimizationInfo;    
    optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;

    int fullMask = 0;
    FrameBuffer::ConstIterator channelIterator = framebuffer.begin();
    
    if(channelIterator == framebuffer.end())
    {
        return optimizationInfo;
    }
    
    int globalXStride = channelIterator.slice().xStride;
    
    // Needs to be RGB or RGBA, Mono or Stereo
    if (globalXStride != (3 * sizeof(half)) &&
        globalXStride != (4 * sizeof(half)) &&
        globalXStride != (6 * sizeof(half)) &&
        globalXStride != (8 * sizeof(half)))
    {
        return optimizationInfo;
    }
    
    // Since we are saving everything contiguously, make sure
    // all the slices have the same ySampling.  We cannot use
    // a different ySampling from the 'R' channel than the 'A' channel
    // because we need the same number of pixels for every channel.
    int globalYSampling = channelIterator.slice().ySampling;
    
    optimizationInfo._xStride = globalXStride;
    optimizationInfo._ySampling = globalYSampling;
    
    for (FrameBuffer::ConstIterator channelIterator = framebuffer.begin(); channelIterator != framebuffer.end(); ++channelIterator)
    {        
        const Slice currentSlice = channelIterator.slice();
        
        
        // we support only channels RGB and RGBA for IIF optimizations.  
        // Those values should also be of type HALF.
        // We also support only an xSampling of 1 and the same ySampling
        // across the channels
        if (currentSlice.type != HALF || 
            currentSlice.xStride != size_t(globalXStride) ||
            currentSlice.ySampling != globalYSampling ||
            currentSlice.xSampling != 1)
        {
            return optimizationInfo;
        }
        
        // convert the channel name into a string for easy manipulation
        // find out the last element after the last dot
        std::string lChannelName = channelIterator.name();
        
        int maskForChannel = IIFOptimizable::getMaskFromChannelName (lChannelName,views);
        fullMask |= maskForChannel;
        
        if (maskForChannel == IIFOptimizable::CHANNELMASK_A)
        {
            optimizationInfo._alphaFillValueRight = currentSlice.fillValue;
        }
        else if (maskForChannel == IIFOptimizable::CHANNELMASK_ALEFT)
        {
            optimizationInfo._alphaFillValueLeft = currentSlice.fillValue;
        }
    } 
    
    switch (fullMask)
    {
        case IIFOptimizable::CHANNELMASK_RGB : case IIFOptimizable::CHANNELMASK_RGB_LEFT:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGB;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_MONO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGBA : case IIFOptimizable::CHANNELMASK_RGBA_LEFT:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGBA;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_MONO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGB_STEREO:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGB;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_STEREO;
            break;
            
        case IIFOptimizable::CHANNELMASK_RGBA_STEREO:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_RGBA;
            optimizationInfo._multiview = OptimizationMode::MULTIVIEW_STEREO;
            break;
            
        default:
            
            optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;
            break;
    }

    return optimizationInfo;
}


#else // ! defined IMF_HAVE_SSE2


EXR_FORCEINLINE
OptimizationMode::ChannelsInfo
getOptimizationInfo(const ChannelList & list,const StringVector * views)
{
    OptimizationMode::ChannelsInfo optimizationInfo;
    optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;

    return optimizationInfo;
}

EXR_FORCEINLINE 
OptimizationMode::ChannelsInfo
getOptimizationInfo(const FrameBuffer & framebuffer,const StringVector * views)
{
    OptimizationMode::ChannelsInfo optimizationInfo;
    optimizationInfo._format = OptimizationMode::PIXELFORMAT_OTHER;

    return optimizationInfo;
}

#endif // defined IMF_HAVE_SSE2


OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_EXIT

#endif
