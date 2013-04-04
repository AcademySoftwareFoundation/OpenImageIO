/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <iostream>
#include <limits>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
#include "dassert.h"
#include <stdexcept>

OIIO_NAMESPACE_ENTER
{
namespace
{

template<class D, class S>
bool flip_(ImageBuf &Rib, const ImageBuf &Aib)
{
    const int nchans = Rib.nchannels();
    const int firstscanline = Rib.ymin();
    const int lastscanline = Rib.ymax();
    ImageBuf::ConstIterator<S, D> a(Aib);
    ImageBuf::Iterator<D, D> r(Rib);

    for( ; ! r.done(); ++r) {
        a.pos(r.x(), lastscanline - (r.y() - firstscanline));
        for(int c = 0; c < nchans; ++c) {
            r[c] = a[c];
        }
    }
    return true;
}


template<class D, class S>
static
bool flop_(ImageBuf &Rib, const ImageBuf &Aib)
{
    const int nchans = Rib.nchannels();
    const int firstcolumn = Rib.xmin();
    const int lastcolumn = Rib.xmax();
    ImageBuf::ConstIterator<S, D> a(Aib);
    ImageBuf::Iterator<D, D> r(Rib);

    for( ; ! r.done(); ++r) {
        a.pos(lastcolumn - (r.x() - firstcolumn), r.y());
        for(int c = 0; c < nchans; ++c)
            r[c] = a[c];
    }
    return true;
}


template<class D, class S>
static
bool flipflop_(ImageBuf &Rib, const ImageBuf &Aib)
{
    const int nchans = Rib.nchannels();
    const int firstscanline = Rib.ymin();
    const int lastscanline = Rib.ymax();
    const int firstcolumn = Rib.xmin();
    const int lastcolumn = Rib.xmax();
    ImageBuf::ConstIterator<S, D> a(Aib);
    ImageBuf::Iterator<D, D> r(Rib);

    for( ; !r.done(); ++r) {
        a.pos (lastcolumn - (r.x() - firstcolumn),
               lastscanline - (r.y() - firstscanline));
        for(int c = 0; c < nchans; ++c)
        {
            r[c] = a[c];
        }
    }
    return true;
}

} // Anonymous Namespace


bool ImageBufAlgo::flip(ImageBuf &Rib, const ImageBuf &Aib)
{
    OIIO_DISPATCH_TYPES2("flip", flip_, Rib.spec().format, Aib.spec().format,
                         Rib, Aib);
    return false;
}


bool ImageBufAlgo::flop(ImageBuf &Rib, const ImageBuf &Aib)
{
    OIIO_DISPATCH_TYPES2("flop", flop_, Rib.spec().format, Aib.spec().format,
                         Rib, Aib);
    return false;
}


bool ImageBufAlgo::flipflop(ImageBuf &Rib, const ImageBuf &Aib)
{
    OIIO_DISPATCH_TYPES2("flipflop", flipflop_, Rib.spec().format, Aib.spec().format,
                         Rib, Aib);
    return false;
}


}
OIIO_NAMESPACE_EXIT
