/*
  Copyright 2015 Larry Gritz and the other authors and contributors.
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


#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/unittest.h>

#include <iostream>

using namespace OIIO;




void
test_get_pixels_cachechannels (int chbegin = 0, int chend = 4,
                               int cache_chbegin = 0, int cache_chend = -1)
{
    std::cout << "\nTesting IC get_pixels of chans [" << chbegin << "," << chend
              << ") with cache range [" << cache_chbegin << "," << cache_chend << "):\n";
    ImageCache *imagecache = ImageCache::create (false /*not shared*/);

    // Create a 10 channel file
    ustring filename ("tenchannels.tif");
    const int nchans = 10;
    ImageBuf A (ImageSpec (64, 64, nchans, TypeDesc::FLOAT));
    const float pixelvalue[nchans] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f,
                                       0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
    ImageBufAlgo::fill (A, pixelvalue);
    A.write (filename);

    // Retrieve 2 pixels of [chbegin,chend), make sure we got the right values
    float p[2*nchans] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    OIIO_CHECK_ASSERT (imagecache->get_pixels (filename, 0, 0,
                                               0, 2, 0, 1, 0, 1, // pixel range
                                               chbegin, chend,
                                               TypeDesc::FLOAT, p,
                                               AutoStride, AutoStride, AutoStride,
                                               cache_chbegin, cache_chend));
    int nc = chend - chbegin;
    for (int x = 0; x < 2; ++x) {
        for (int c = 0; c < nc; ++c) {
            std::cout << ' ' << p[x*nc+c];
            OIIO_CHECK_EQUAL (p[x*nc+c], pixelvalue[c+chbegin]);
        }
        std::cout << "\n";
    }
    for (int c = 2*nc; c < 2*nchans; ++c)
        OIIO_CHECK_EQUAL (p[c], -1.0f);

    ImageCache::destroy (imagecache);
}



int
main (int argc, char **argv)
{
    test_get_pixels_cachechannels (0, 10);
    test_get_pixels_cachechannels (0, 4);
    test_get_pixels_cachechannels (0, 4, 0, 6);
    test_get_pixels_cachechannels (0, 4, 0, 4);
    test_get_pixels_cachechannels (6, 9);
    test_get_pixels_cachechannels (6, 9, 6, 9);

    return unit_test_failures;
}
