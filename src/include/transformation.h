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


#include "version.h"
#include "export.h"

OIIO_NAMESPACE_ENTER
{

class DLLPUBLIC Transformation {
public:
    Transformation () { }
    virtual ~Transformation (void) { };

    virtual void transform(int dstx, int dsty, float* srcx, float* srcy);

    static Transformation *create (const std::string &filtername, float width);

    // Destroy a filter that was created with create().
    static void destroy (Transformation *t);
};



class RotationTrans {
public:
    RotationTrans(float _rotangle, float _originx = 0, float _originy = 0)
    {
        rotangle = - _rotangle * M_PI / 180.0f;
        originx = _originx;
        originy = _originy;
    }

    void mapping(int x, int y, float* s, float* t, float *dsdx, float *dtdx, float *dsdy, float *dtdy)
    {
        *s = originx + (x+0.5f-originx) * cos(rotangle) - (y+0.5f-originy) * sin (rotangle);
        *t = originy + (x+0.5f-originx) * sin(rotangle)  +  (y+0.5f-originy)  * cos(rotangle);

        *dsdy = 1.0f;
        *dtdy = 1.0f;
        *dsdy = 0;
        *dtdx = 0;
    }

private:
    float rotangle;
    float originx, originy;
};




class ResizeTrans {
public:

    ResizeTrans(float _new_width, float _new_height, float orig_width, float orig_height)
    {
        new_width = _new_width;
        new_height = _new_height;
        xscale = new_width / orig_width;
        yscale = new_height / orig_height;
    }

    ResizeTrans(float _xscale, float _yscale)
    {
        xscale = _xscale;
        yscale = _yscale;
    }

    void mapping(int x, int y, float* s, float* t, float *dsdx, float *dtdx, float *dsdy, float *dtdy)
    {
        *s = (x + 0.5f) / xscale;
        *t = (y + 0.5f) / yscale;

        *dsdx = 1.0f / xscale;
        *dtdy = 1.0f / yscale;
        *dsdy = 0;
        *dtdx = 0;
    }

private:
    float new_width, new_height;
    float xscale, yscale;
};




class ShearTrans {
public:
    ShearTrans(float _m, float _n, float _originx = 0, float _originy = 0)
    {
        m = _m;
        n = _n;
        originx = _originx;
        originy = _originy;
    }

    void mapping(int x, int y, float* s, float* t, float *dsdx, float *dtdx, float *dsdy, float *dtdy)
    {
        if(1 - m*n == 0)
            return;

        *s = (x - m*y) / (1 - m*n);
        *t = y - n*(*s);

        *dsdy = 1.0f;
        *dtdy = 1.0f;
        *dsdy = 0;
        *dtdx = 0;
    }

private:
    float m, n;
    float originx, originy;
};





}
OIIO_NAMESPACE_EXIT