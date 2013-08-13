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
// Written by Pixar, 2011-2012.
//-*****************************************************************************

#ifndef _PxOneChanDeepAlpha_h_
#define _PxOneChanDeepAlpha_h_

#include "PxDeepUtils.h"
#include "PxBaseDeepHelper.h"

namespace PxDeep {

//-*****************************************************************************
// ONE CHANNEL DEEP ALPHA CONTINUOUS
//-*****************************************************************************
template <typename RGBA_T>
class OneChanDeepAlphaContinuous
    : public BaseDeepHelper<RGBA_T,
                            OneChanDeepAlphaContinuous<RGBA_T>,Span>
{
public:
    typedef BaseDeepHelper<RGBA_T,
                           OneChanDeepAlphaContinuous<RGBA_T>,Span>
    super_type;
    typedef OneChanDeepAlphaContinuous<RGBA_T> this_type;
    typedef typename super_type::span_type span_type;

    OneChanDeepAlphaContinuous( DtexFile* i_dtexFile,
                                int i_numDtexChans,
                                const Parameters& i_params )
      : BaseDeepHelper<RGBA_T,
                       OneChanDeepAlphaContinuous<RGBA_T>,Span>
    ( i_dtexFile,
      i_numDtexChans,
      i_params ) {}

    void processDeepPixel( int i_numPts );
};

//-*****************************************************************************
// ONE CHANNEL DEEP ALPHA DISCRETE
//-*****************************************************************************
template <typename RGBA_T>
class OneChanDeepAlphaDiscrete
    : public BaseDeepHelper<RGBA_T,
                            OneChanDeepAlphaDiscrete<RGBA_T>,Span>
{
public:
    typedef BaseDeepHelper<RGBA_T,
                           OneChanDeepAlphaDiscrete<RGBA_T>,Span>
    super_type;
    typedef OneChanDeepAlphaDiscrete<RGBA_T> this_type;
    typedef typename super_type::span_type span_type;

    OneChanDeepAlphaDiscrete( DtexFile* i_dtexFile,
                              int i_numDtexChans,
                              const Parameters& i_params )
      : BaseDeepHelper<RGBA_T,
                       OneChanDeepAlphaDiscrete<RGBA_T>,Span>
    ( i_dtexFile,
      i_numDtexChans,
      i_params ) {}

    void processDeepPixel( int i_numPts );
};

//-*****************************************************************************
template <typename RGBA_T>
void OneChanDeepAlphaContinuous<RGBA_T>::processDeepPixel( int i_numPts )
{
    assert( i_numPts > 0 );
    
    // Loop over all the dtex points and get their deepAlphas
    // and their depths. Enforce the case that deepAlpha
    // is always between 0 and 1.
    // Also, find a good "best slope" for extraplolation.
    (this->m_spans).resize( ( size_t )i_numPts );

    for ( int j = 0; j < i_numPts; ++j )
    {
        float z;
        float pts[4];
        DtexPixelGetPoint( (this->m_pixel), j, &z, ( float * )pts );

        z = ClampDepth( z );
        
        double alpha = ClampAlpha( pts[0] );

        span_type& spanJ = (this->m_spans)[j];
        spanJ.clear();
        spanJ.in = z;
        spanJ.out = z;
        spanJ.viz = ClampViz( 1.0 - alpha );
        spanJ.index = j;
    }

    // Sort the spans.
    std::sort( (this->m_spans).begin(), (this->m_spans).end() );

    // Combine identical depths, gathering max density along
    // the way.
    double maxDensity = PXDU_MIN_NON_ZERO_DENSITY;
    {
        int activeBegin = 0;
        int activeEnd = 0;
        float interestingDepth = 0.0f;
        int numRemoved = 0;

        while ( activeBegin < i_numPts )
        {
            span_type& spanActiveBegin = (this->m_spans)[activeBegin];
            float nextInterestingDepth = spanActiveBegin.in;
            assert( nextInterestingDepth > interestingDepth );

            activeEnd = i_numPts;
            for ( int a = activeBegin + 1; a < i_numPts; ++a )
            {
                span_type& spanNext = (this->m_spans)[a];

                assert( spanNext.in > interestingDepth );
                assert( spanNext.in >= nextInterestingDepth );

                if ( spanNext.in > nextInterestingDepth )
                {
                    // This span is not active in this round,
                    // set activeEnd and get out.
                    activeEnd = a;
                    break;
                }
                else
                {
                    // This span has an identical depth to
                    // the previous one, so we must combine their
                    // vizs and eliminate the depth.
                    spanActiveBegin.viz *= spanNext.viz;
                    spanNext.in = FLT_MAX;
                    spanNext.out = FLT_MAX;
                    ++numRemoved;
                }
            }

            spanActiveBegin.viz = ClampViz( spanActiveBegin.viz );

            // Accumulate density from here to the next point.
            if ( activeEnd < i_numPts )
            {
                span_type& spanNext = (this->m_spans)[activeEnd];
                double dz = spanNext.in - spanActiveBegin.in;
                assert( spanNext.in > spanActiveBegin.in );
                assert( dz > 0.0 );

                double density = DensityFromVizDz( spanActiveBegin.viz,
                                                   dz );
                maxDensity = std::max( maxDensity, density );
            }
            
            activeBegin = activeEnd;
            interestingDepth = nextInterestingDepth;
        }

        // If any removed, re-sort the list and remove the end
        // points.
        if ( numRemoved > 0 )
        {
            assert( numRemoved < i_numPts );
            std::sort( (this->m_spans).begin(), (this->m_spans).end() );
            i_numPts -= numRemoved;
            (this->m_spans).resize( i_numPts );
        }
    }

    // Handle the single point case.
    if ( i_numPts == 1 )
    {
        span_type& span0 = (this->m_spans)[0];
        
        if ( (this->m_params).discardZeroAlphaSamples &&
             span0.viz >= 1.0 )
        {
            // Nothing!
            return;
        }

        span0.out = ClampDepth( IncrementPositiveFloat( span0.in ) );

        float alphaF = ClampAlpha( 1.0 - span0.viz );

        (this->m_deepOutPixel).push_back( span0.in,
                                  span0.out,
                                  alphaF );

        return;
    }

    // Put the spans back out.
    // If the last point has a non-zero alpha, extrapolate the
    // maximum density to create an end point.
    for ( int j = 0; j < i_numPts; ++j )
    {
        span_type& spanJ = (this->m_spans)[j];

        if ( (this->m_params).discardZeroAlphaSamples &&
             spanJ.viz >= 1.0 )
        {
            // This span is transparent, ignore it.
            continue;
        }

        // Set the out points.
        if ( j < i_numPts-1 )
        {
            spanJ.out = (this->m_spans)[j+1].in;
        }
        else
        {
            // This is the last point.
            // If it has non-zero alpha, it needs depth,
            // which we use the max density for.
            if ( spanJ.viz >= 1.0 )
            {
                // Don't need to worry about this last span!
                // It is at the end of the continuous span, and
                // is completely transparent.
                continue;
            }
            
            double dz = DzFromVizDensity( spanJ.viz, maxDensity );
            spanJ.out = ClampDepth( spanJ.in + dz );
            if ( spanJ.out <= spanJ.in )
            {
                spanJ.out = ClampDepth(
                    IncrementPositiveFloat( spanJ.in ) );
            }
        }

        float alphaF = ClampAlpha( 1.0 - spanJ.viz );

        (this->m_deepOutPixel).push_back( spanJ.in,
                                  spanJ.out,
                                  alphaF );
    }
}

//-*****************************************************************************
template <typename RGBA_T>
void OneChanDeepAlphaDiscrete<RGBA_T>::processDeepPixel( int i_numPts )
{
    assert( i_numPts > 0 );
    
    // Loop over all the dtex points and get their deepAlphas
    // and their depths. Enforce the case that deepAlpha
    // is always between 0 and 1.
    // Also, find a good "best slope" for extraplolation.
    (this->m_spans).resize( ( size_t )i_numPts );

    for ( int j = 0; j < i_numPts; ++j )
    {
        float z;
        float pts[4];
        DtexPixelGetPoint( (this->m_pixel), j, &z, ( float * )pts );

        z = ClampDepth( z );
        
        double alpha = ClampAlpha( pts[0] );

        span_type& spanJ = (this->m_spans)[j];
        spanJ.clear();
        spanJ.in = z;
        spanJ.viz = ClampViz( 1.0 - alpha );
        spanJ.index = j;
    }

    // Sort the spans.
    std::sort( (this->m_spans).begin(), (this->m_spans).end() );

    // Combine identical depths.
    {
        int activeBegin = 0;
        int activeEnd = 0;
        float interestingDepth = 0.0f;
        int numRemoved = 0;

        while ( activeBegin < i_numPts )
        {
            span_type& spanActiveBegin = (this->m_spans)[activeBegin];
            float nextInterestingDepth = spanActiveBegin.in;
            assert( nextInterestingDepth > interestingDepth );

            activeEnd = i_numPts;
            for ( int a = activeBegin + 1; a < i_numPts; ++a )
            {
                span_type& spanNext = (this->m_spans)[a];

                assert( spanNext.in > interestingDepth );
                assert( spanNext.in >= nextInterestingDepth );

                if ( spanNext.in > nextInterestingDepth )
                {
                    // This span is not active in this round,
                    // set activeEnd and get out.
                    activeEnd = a;
                    break;
                }
                else
                {
                    // This span has an identical depth to
                    // the previous one, so we must combine their
                    // alphas and eliminate the depth.
                    spanActiveBegin.viz *= spanNext.viz;
                    spanNext.in = FLT_MAX;
                    ++numRemoved;
                }
            }

            spanActiveBegin.viz = ClampViz( spanActiveBegin.viz );
            activeBegin = activeEnd;
            interestingDepth = nextInterestingDepth;
        }

        // If any removed, re-sort the list and remove the end
        // points.
        if ( numRemoved > 0 )
        {
            assert( numRemoved < i_numPts );
            std::sort( (this->m_spans).begin(), (this->m_spans).end() );
            i_numPts -= numRemoved;
            (this->m_spans).resize( i_numPts );
        }
    }

    // Put the spans back out.
    for ( int j = 0; j < i_numPts; ++j )
    {
        span_type& spanJ = (this->m_spans)[j];

        if ( (this->m_params).discardZeroAlphaSamples &&
             spanJ.viz >= 1.0 )
        {
            // This span is transparent, ignore it.
            continue;
        }

        float alphaF = ClampAlpha( 1.0 - spanJ.viz );
        
        // Set the channels!
        (this->m_deepOutPixel).push_back( spanJ.in,
                                  alphaF );
    }
}

} // End namespace PxDeep

#endif
