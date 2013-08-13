//-*****************************************************************************
// Copyright (c) 2012, Pixar. All rights reserved.                             *
//                                                                             *
// This license governs use of the accompanying software. If you               *
// use the software, you accept this license. If you do not accept             *
// the license, do not use the software.                                       *
//                                                                             *
// 1. Definitions                                                              *
// The terms "reproduce," "reproduction," "derivative works," and              *
// "distribution" have the same meaning here as under U.S.                     *
// copyright law.  A "contribution" is the original software, or               *
// any additions or changes to the software.                                   *
// A "contributor" is any person or entity that distributes its                *
// contribution under this license.                                            *
// "Licensed patents" are a contributor's patent claims that read              *
// directly on its contribution.                                               *
//                                                                             *
// 2. Grant of Rights                                                          *
// (A) Copyright Grant- Subject to the terms of this license,                  *
// including the license conditions and limitations in section 3,              *
// each contributor grants you a non-exclusive, worldwide,                     *
// royalty-free copyright license to reproduce its contribution,               *
// prepare derivative works of its contribution, and distribute                *
// its contribution or any derivative works that you create.                   *
// (B) Patent Grant- Subject to the terms of this license,                     *
// including the license conditions and limitations in section 3,              *
// each contributor grants you a non-exclusive, worldwide,                     *
// royalty-free license under its licensed patents to make, have               *
// made, use, sell, offer for sale, import, and/or otherwise                   *
// dispose of its contribution in the software or derivative works             *
// of the contribution in the software.                                        *
//                                                                             *
// 3. Conditions and Limitations                                               *
// (A) No Trademark License- This license does not grant you                   *
// rights to use any contributor's name, logo, or trademarks.                  *
// (B) If you bring a patent claim against any contributor over                *
// patents that you claim are infringed by the software, your                  *
// patent license from such contributor to the software ends                   *
// automatically.                                                              *
// (C) If you distribute any portion of the software, you must                 *
// retain all copyright, patent, trademark, and attribution                    *
// notices that are present in the software.                                   *
// (D) If you distribute any portion of the software in source                 *
// code form, you may do so only under this license by including a             *
// complete copy of this license with your distribution. If you                *
// distribute any portion of the software in compiled or object                *
// code form, you may only do so under a license that complies                 *
// with this license.                                                          *
// (E) The software is licensed "as-is." You bear the risk of                  *
// using it. The contributors give no express warranties,                      *
// guarantees or conditions. You may have additional consumer                  *
// rights under your local laws which this license cannot change.              *
// To the extent permitted under your local laws, the contributors             *
// exclude the implied warranties of merchantability, fitness for              *
// a particular purpose and non-infringement.                                  *
//-*****************************************************************************

//-*****************************************************************************
// Written by Pixar Animation Studios, 2011-2012.
//-*****************************************************************************

#ifndef _PxDeepOutRow_h_
#define _PxDeepOutRow_h_

#include "PxDeepUtils.h"
#include "PxDeepOutPixel.h"

#include <ImfDeepScanLineOutputFile.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfPartType.h>
#include <ImfChannelList.h>

namespace PxDeep {

//-*****************************************************************************
//-*****************************************************************************
// DEEP OUT ROW
//-*****************************************************************************
//-*****************************************************************************
template <typename RGBA_T>
class DeepOutRow
{
public:
    DeepOutRow( int i_width, bool i_doDeepBack, bool i_doRGB );

    void clear();
    
    void addHole( int i_x )
    {
        m_sampleCounts[i_x] = 0;
    }

    void addPixel( int i_x, const DeepOutPixel<RGBA_T>& i_pixel );

    void setFrameBuffer( Imf::DeepFrameBuffer& o_frameBuffer );

protected:
    // Width of the row.
    int m_width;

    // Whether or not to bother with deep back
    bool m_doDeepBack;

    // Whether or not to bother with RGB
    bool m_doRGB;
    
    // Scanline sample buffers.
    std::vector<uint> m_sampleCounts;

    // The pointers to data at each pixel
    std::vector<float*> m_deepFrontPtrs;
    std::vector<float*> m_deepBackPtrs;
    std::vector<RGBA_T*> m_redPtrs;
    std::vector<RGBA_T*> m_greenPtrs;
    std::vector<RGBA_T*> m_bluePtrs;
    std::vector<RGBA_T*> m_alphaPtrs;

    // The data itself.
    std::vector<float> m_deepFrontSamples;
    std::vector<float> m_deepBackSamples;
    std::vector<RGBA_T> m_redSamples;
    std::vector<RGBA_T> m_greenSamples;
    std::vector<RGBA_T> m_blueSamples;
    std::vector<RGBA_T> m_alphaSamples;
};

//-*****************************************************************************
template <typename T>
inline void VecAppend( T& i_dst, const T& i_src )
{
    i_dst.insert( i_dst.end(), i_src.begin(), i_src.end() );
}

//-*****************************************************************************
template <typename RGBA_T>
DeepOutRow<RGBA_T>::DeepOutRow( int i_width, bool i_doDeepBack, bool i_doRGB )
  : m_width( i_width )
  , m_doDeepBack( i_doDeepBack )
  , m_doRGB( i_doRGB )
{
    m_sampleCounts.resize( ( size_t )m_width );
    m_deepFrontPtrs.resize( ( size_t )m_width );
    if ( m_doDeepBack )
    {
        m_deepBackPtrs.resize( ( size_t )m_width );
    }
    if ( m_doRGB )
    {
        m_redPtrs.resize( ( size_t )m_width );
        m_greenPtrs.resize( ( size_t )m_width );
        m_bluePtrs.resize( ( size_t )m_width );
    }
    m_alphaPtrs.resize( ( size_t )m_width );
}

//-*****************************************************************************
template <typename RGBA_T>
void DeepOutRow<RGBA_T>::clear()
{
    std::fill( m_sampleCounts.begin(),
               m_sampleCounts.end(),
               ( uint )0 );
    m_deepFrontSamples.clear();
    m_deepBackSamples.clear();
    m_redSamples.clear();
    m_greenSamples.clear();
    m_blueSamples.clear();
    m_alphaSamples.clear();
}

//-*****************************************************************************
template <typename RGBA_T>
void DeepOutRow<RGBA_T>::addPixel( int i_x,
                                   const DeepOutPixel<RGBA_T>& i_pixel )
{
    int npoints = i_pixel.size();
    m_sampleCounts[i_x] = npoints;
    if ( npoints > 0 )
    {
        VecAppend( m_deepFrontSamples, i_pixel.deepFront );
        if ( m_doDeepBack )
        {
            VecAppend( m_deepBackSamples, i_pixel.deepBack );
        }
        if ( m_doRGB )
        {
            VecAppend( m_redSamples, i_pixel.red );
            VecAppend( m_greenSamples, i_pixel.green );
            VecAppend( m_blueSamples, i_pixel.blue );
        }
        VecAppend( m_alphaSamples, i_pixel.alpha );
    }
}

//-*****************************************************************************
template <typename RGBA_T>
void DeepOutRow<RGBA_T>::setFrameBuffer( Imf::DeepFrameBuffer& o_frameBuffer )
{
    // Set up the pointers.
    float *deepFrontLastPtr = m_deepFrontSamples.data();
    float *deepBackLastPtr = m_deepBackSamples.data();
    RGBA_T *redLastPtr = m_redSamples.data();
    RGBA_T *greenLastPtr = m_greenSamples.data();
    RGBA_T *blueLastPtr = m_blueSamples.data();
    RGBA_T *alphaLastPtr = m_alphaSamples.data();
    for ( int x = 0; x < m_width; ++x )
    {
        m_deepFrontPtrs[x] = deepFrontLastPtr;
        if ( m_doDeepBack )
        {    
            m_deepBackPtrs[x] = deepBackLastPtr;
        }
        if ( m_doRGB )
        {
            m_redPtrs[x] = redLastPtr;
            m_greenPtrs[x] = greenLastPtr;
            m_bluePtrs[x] = blueLastPtr;
        }
        m_alphaPtrs[x] = alphaLastPtr;

        int c = m_sampleCounts[x];
            
        deepFrontLastPtr += c;
        deepBackLastPtr += c;
        redLastPtr += c;
        greenLastPtr += c;
        blueLastPtr += c;
        alphaLastPtr += c;
    }
        
    // Sample counts
    o_frameBuffer.insertSampleCountSlice(
        Imf::Slice( Imf::UINT,
                    ( char * )m_sampleCounts.data(),
                    sizeof( uint ),    // x stride
                    0 ) );             // y stride

    // RGB
    if ( m_doRGB )
    {
        o_frameBuffer.insert(
            "R",
            Imf::DeepSlice( ImfPixelType<RGBA_T>(),
                            ( char * )m_redPtrs.data(),
                            sizeof( RGBA_T* ),     // xstride
                            0,                     // ystride
                            sizeof( RGBA_T ) ) );  // sample stride
    
        o_frameBuffer.insert(
            "G",
            Imf::DeepSlice( ImfPixelType<RGBA_T>(),
                            ( char * )m_greenPtrs.data(),
                            sizeof( RGBA_T* ),     // xstride
                            0,                     // ystride
                            sizeof( RGBA_T ) ) );  // sample stride
            
        o_frameBuffer.insert(
            "B",
            Imf::DeepSlice( ImfPixelType<RGBA_T>(),
                            ( char * )m_bluePtrs.data(),
                            sizeof( RGBA_T* ),     // xstride
                            0,                     // ystride
                            sizeof( RGBA_T ) ) );  // sample stride
    }

    // ALPHA
    o_frameBuffer.insert(
        "A",
        Imf::DeepSlice( ImfPixelType<RGBA_T>(),
                        ( char * )m_alphaPtrs.data(),
                        sizeof( RGBA_T* ),     // xstride
                        0,                     // ystride
                        sizeof( RGBA_T ) ) );  // sample stride
        
    // DEEP FRONT
    o_frameBuffer.insert(
        "Z",
        Imf::DeepSlice( Imf::FLOAT,
                        ( char * )m_deepFrontPtrs.data(),
                        sizeof( float* ),      // xstride
                        0,                     // ystride
                        sizeof( float ) ) );   // sample stride

    // DEEP BACK
    if ( m_doDeepBack )
    {
        o_frameBuffer.insert(
            "ZBack",
            Imf::DeepSlice( Imf::FLOAT,
                            ( char * )m_deepBackPtrs.data(),
                            sizeof( float* ),      // xstride
                            0,                     // ystride
                            sizeof( float ) ) );   // sample stride
    }
}

} // End namespace PxDeep

#endif
