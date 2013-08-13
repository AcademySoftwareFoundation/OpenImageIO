//-*****************************************************************************
// Copyright (C) Pixar. All rights reserved.                                   *
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

#include "PxDeepUtils.h"

namespace PxDeep {

//-*****************************************************************************
// Density/Viz/DZ calculations are always performed in double precision.
// We try to leave them alone as much as possible, but the logarithm can get
// weird for very very small numbers. The "isfinite" call basically rules
// out NaN and Infinity results, though it doesn't bother with subnormal
// numbers, since the error case we're worried about is log being too big.
// viz = exp( -dz * density )
// log( viz ) = -dz * density
// density = -log( viz ) / dz
double DensityFromVizDz( double i_viz, double i_dz )
{
    assert( i_viz >= 0.0 );
    assert( i_viz <= 1.0 );
    assert( i_dz >= 0.0 );

    if ( i_viz >= 1.0 )
    {
        // There's no attenuation at all, so there's no density!
        return 0.0;
    }
    else if ( i_viz <= 0.0 )
    {
        // There's total attenuation, so we use our max density.
        return PXDU_DENSITY_OF_VIZ_0;
    }
    else if ( i_dz <= 0.0 )
    {
        // There's no depth, and viz is greater than zero,
        // so we assume the density is as high as possible
        return PXDU_DENSITY_OF_VIZ_0;
    }
    else
    {
        double d = -log( i_viz ) / i_dz;
        if ( !isfinite( d ) )
        {
            return PXDU_DENSITY_OF_VIZ_0;
        }
        else
        {
            return d;
        }
    }
}

//-*****************************************************************************
// We can often treat "density times dz" as a single quantity without
// separating it.
// viz = exp( -densityTimesDz )
// log( viz ) = -densityTimesDz
// densityTimesDz = -log( viz )
double DensityTimesDzFromViz( double i_viz )
{
    assert( i_viz >= 0.0 );
    assert( i_viz <= 1.0 );

    if ( i_viz >= 1.0 )
    {
        // There's no attenuation at all, so there's no density!
        return 0.0;
    }
    else if ( i_viz <= 0.0 )
    {
        // There's total attenuation, so we use our max density.
        return PXDU_DENSITY_OF_VIZ_0 * PXDU_DZ_OF_VIZ_0;
    }
    else
    {
        double d = -log( i_viz );
        if ( !isfinite( d ) )
        {
            return PXDU_DENSITY_OF_VIZ_0 * PXDU_DZ_OF_VIZ_0;
        }
        else
        {
            return d;
        }
    }
}

//-*****************************************************************************
// viz = exp( -dz * density )
// log( viz ) = -dz * density
// dz = -log( viz ) / density
// Note that this is basically the same as the computation above.
double DzFromVizDensity( double i_viz, double i_density )
{
    assert( i_viz >= 0.0 );
    assert( i_viz <= 1.0 );
    assert( i_density >= 0.0 );

    if ( i_viz >= 1.0 )
    {
        // There's no attenuation, so there's no depth.
        return 0.0;
    }
    else if ( i_viz <= 0.0 )
    {
        // There's total attenuation, so we use the smallest depth
        // for our max density.
        return PXDU_DZ_OF_VIZ_0;
    }
    else if ( i_density <= 0.0 )
    {
        // Hmmm. There's no density, but there is some attenuation,
        // which basically implies an infinite depth.
        // We'll use the minimum density.
        // This whole part is hacky at best.
        double dz = -log( i_viz ) / PXDU_MIN_NON_ZERO_DENSITY;
        if ( !isfinite( dz ) )
        {
            return PXDU_MAX_DZ;
        }
        else
        {
            return dz;
        }
    }
    else
    {
        double dz = -log( i_viz ) / i_density;
        if ( !isfinite( dz ) )
        {
            return PXDU_MAX_DZ;
        }
        else
        {
            return dz;
        }
    }
}

} // End namespace PxDeep

