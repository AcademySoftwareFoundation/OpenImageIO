/*
  Copyright 2018 Larry Gritz and the other authors and contributors.
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

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/timer.h>
#include "imagebufalgo_cuda.h"


OIIO_NAMESPACE_BEGIN
namespace pvt {


__global__
void add_cuda (float *R, const float *A, const float *B, ROI roi)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    int n = int(roi.npixels());
    int nc = roi.nchannels();
    for (int p = index; p < n; p += stride) {
        int i = p*nc;
        for (int c = roi.chbegin; c < roi.chend; ++c)
            R[i+c] = A[i+c] + B[i+c];
    }
}



bool
add_impl_cuda (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
               ROI roi)
{
    Timer timer;
    int blockSize = 1024;
    int numBlocks = (int(roi.npixels()) + blockSize - 1) / blockSize;
    add_cuda<<<numBlocks, blockSize>>>((float *)R.localpixels(),
                                       (const float *)A.localpixels(),
                                       (const float *)B.localpixels(), roi);
    cudaDeviceSynchronize();
    OIIO::debug ("Running cuda ImageBufAlgo::add, %d blocks of %d: %gms\n",
                 numBlocks, blockSize, timer()*1000.0f);
    return true;
}




__global__
void sub_cuda (float *R, const float *A, const float *B, ROI roi)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    int n = int(roi.npixels());
    int nc = roi.nchannels();
    for (int p = index; p < n; p += stride) {
        int i = p*nc;
        for (int c = roi.chbegin; c < roi.chend; ++c)
            R[i+c] = A[i+c] - B[i+c];
    }
}



bool
sub_impl_cuda (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
               ROI roi)
{
    Timer timer;
    int blockSize = 1024;
    int numBlocks = (int(roi.npixels()) + blockSize - 1) / blockSize;
    sub_cuda<<<numBlocks, blockSize>>>((float *)R.localpixels(),
                                       (const float *)A.localpixels(),
                                       (const float *)B.localpixels(), roi);
    cudaDeviceSynchronize();
    OIIO::debug ("Running cuda ImageBufAlgo::sub, %d blocks of %d: %gms\n",
                 numBlocks, blockSize, timer()*1000.0f);
    return true;
}


}  // end namespace pvt
OIIO_NAMESPACE_END

