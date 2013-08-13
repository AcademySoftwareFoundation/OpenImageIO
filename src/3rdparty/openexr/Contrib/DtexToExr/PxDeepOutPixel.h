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

#ifndef _PxDeepOutPixel_h_
#define _PxDeepOutPixel_h_

#include "PxDeepUtils.h"

namespace PxDeep {

//-*****************************************************************************
//-*****************************************************************************
// DEEP OUT PIXEL
//-*****************************************************************************
//-*****************************************************************************
// While constructing a deep out pixel from a dtex pixel, we reuse some
// temporary storage.
template <typename RGBA_T>
struct DeepOutPixel
{
    size_t size() const
    {
        return deepFront.size();
    }
    
    void clear()
    {
        deepFront.clear();
        deepBack.clear();
        red.clear();
        green.clear();
        blue.clear();
        alpha.clear();
    }

    void reserve( size_t N )
    {
        deepFront.reserve( N );
        deepBack.reserve( N );
        red.reserve( N );
        green.reserve( N );
        blue.reserve( N );
        alpha.reserve( N );
    }

    void push_back( float i_depth,
                    RGBA_T i_alpha )
    {
        deepFront.push_back( i_depth );
        deepBack.push_back( i_depth );
        red.push_back( 0.0f );
        green.push_back( 0.0f );
        blue.push_back( 0.0f );
        alpha.push_back( i_alpha );
    }

    void push_back( float i_deepFront,
                    float i_deepBack,
                    RGBA_T i_alpha )
    {
        deepFront.push_back( i_deepFront );
        deepBack.push_back( i_deepBack );
        red.push_back( 0.0f );
        green.push_back( 0.0f );
        blue.push_back( 0.0f );
        alpha.push_back( i_alpha );
    }
    
    void push_back( float i_depth,
                    RGBA_T i_red,
                    RGBA_T i_green,
                    RGBA_T i_blue,
                    RGBA_T i_alpha )
    {
        deepFront.push_back( i_depth );
        deepBack.push_back( i_depth );
        red.push_back( i_red );
        green.push_back( i_green );
        blue.push_back( i_blue );
        alpha.push_back( i_alpha );
    }

    void push_back( float i_deepFront,
                    float i_deepBack,
                    RGBA_T i_red,
                    RGBA_T i_green,
                    RGBA_T i_blue,
                    RGBA_T i_alpha )
    {
        deepFront.push_back( i_deepFront );
        deepBack.push_back( i_deepBack );
        red.push_back( i_red );
        green.push_back( i_green );
        blue.push_back( i_blue );
        alpha.push_back( i_alpha );
    }
    
    std::vector<float> deepFront;
    std::vector<float> deepBack;
    std::vector<RGBA_T> red;
    std::vector<RGBA_T> green;
    std::vector<RGBA_T> blue;
    std::vector<RGBA_T> alpha;
};

}

#endif
