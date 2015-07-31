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

#include <cstdio>
#include <cstdlib>

#include <OpenEXR/half.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/imageio.h"

OIIO_NAMESPACE_BEGIN


void
DeepData::init (int npix, int nchan,
                const TypeDesc *chbegin, const TypeDesc *chend)
{
    clear ();
    npixels = npix;
    nchannels = nchan;
    channeltypes.assign (chbegin, chend);
    nsamples.resize (npixels, 0);
    pointers.resize (size_t(npixels)*size_t(nchannels), NULL);
}



void
DeepData::init (const ImageSpec &spec)
{
    clear ();
    npixels = (int) spec.image_pixels();
    nchannels = spec.nchannels;
    channeltypes.reserve (nchannels);
    spec.get_channelformats (channeltypes);
    nsamples.resize (npixels, 0);
    pointers.resize (size_t(npixels)*size_t(nchannels), NULL);
}



void
DeepData::alloc ()
{
    // Calculate the total size we need, align each channel to 4 byte boundary
    size_t totalsamples = 0, totalbytes = 0;
    for (int i = 0;  i < npixels;  ++i) {
        if (int s = nsamples[i]) {
            totalsamples += s;
            for (int c = 0;  c < nchannels;  ++c)
                totalbytes += round_to_multiple (channeltype(c).size() * s, 4);
        }
    }

    // Allocate a minimum of 4 bytes so that we can tell if alloc() was
    // called by whether data.size() > 0.
    totalbytes = std::max (totalbytes, size_t(4));

    // Set all the data pointers to the right offsets within the
    // data block.  Leave the pointes NULL for pixels with no samples.
    data.resize (totalbytes);
    char *p = &data[0];
    for (int i = 0;  i < npixels;  ++i) {
        if (int s = nsamples[i]) {
            for (int c = 0;  c < nchannels;  ++c) {
                pointers[i*nchannels+c] = p;
                p += round_to_multiple (channeltype(c).size()*s, 4);
            }
        }
    }
}



void
DeepData::clear ()
{
    npixels = 0;
    nchannels = 0;
    channeltypes.clear();
    nsamples.clear();
    pointers.clear();
    data.clear();
}



void
DeepData::free ()
{
    clear ();
    std::vector<unsigned int>().swap (nsamples);
    std::vector<void *>().swap (pointers);
    std::vector<char>().swap (data);
}



int
DeepData::samples (int pixel) const
{
    if (pixel < 0 || pixel >= npixels || data.size() == 0)
        return 0;
    return nsamples[pixel];
}



void
DeepData::set_samples (int pixel, int samps)
{
    ASSERT (pixel >= 0 && pixel < npixels && "invalid pixel index");
    ASSERT (data.size() == 0 && "set_samples may not be called after alloc()");
    nsamples[pixel] = samps;
}



void *
DeepData::channel_ptr (int pixel, int channel)
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return NULL;
    return pointers[pixel*nchannels + channel];
}



const void *
DeepData::channel_ptr (int pixel, int channel) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return NULL;
    return pointers[pixel*nchannels + channel];
}



float
DeepData::deep_value (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return 0.0f;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return 0.0f;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        return ((const float *)ptr)[sample];
    case TypeDesc::HALF  :
        return ((const half *)ptr)[sample];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,float>((const unsigned char *)ptr)[sample];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,float>((const char *)ptr)[sample];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,float>((const unsigned short *)ptr)[sample];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,float>((const short *)ptr)[sample];
    case TypeDesc::UINT  :
        return ConstDataArrayProxy<unsigned int,float>((const unsigned int *)ptr)[sample];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,float>((const int *)ptr)[sample];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,float>((const unsigned long long *)ptr)[sample];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,float>((const long long *)ptr)[sample];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



uint32_t
DeepData::deep_value_uint (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return 0.0f;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return 0.0f;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        return ConstDataArrayProxy<float,uint32_t>((const float *)ptr)[sample];
    case TypeDesc::HALF  :
        return ConstDataArrayProxy<half,uint32_t>((const half *)ptr)[sample];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,uint32_t>((const unsigned char *)ptr)[sample];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,uint32_t>((const char *)ptr)[sample];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,uint32_t>((const unsigned short *)ptr)[sample];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,uint32_t>((const short *)ptr)[sample];
    case TypeDesc::UINT  :
        return ((const unsigned int *)ptr)[sample];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,uint32_t>((const int *)ptr)[sample];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,uint32_t>((const unsigned long long *)ptr)[sample];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,uint32_t>((const long long *)ptr)[sample];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



void
DeepData::set_deep_value (int pixel, int channel, int sample, float value)
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    if (! data.size())
        alloc();
    void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,float>((float *)ptr)[sample] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,float>((half *)ptr)[sample] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,float>((unsigned char *)ptr)[sample] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,float>((char *)ptr)[sample] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,float>((unsigned short *)ptr)[sample] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,float>((short *)ptr)[sample] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,float>((uint32_t *)ptr)[sample] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,float>((int *)ptr)[sample] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,float>((uint64_t *)ptr)[sample] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,float>((int64_t *)ptr)[sample] = value; break;
    default:
        ASSERT (0);
    }
}



void
DeepData::set_deep_value (int pixel, int channel, int sample, uint32_t value)
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    if (! data.size())
        alloc();
    void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,uint32_t>((float *)ptr)[sample] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,uint32_t>((half *)ptr)[sample] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,uint32_t>((unsigned char *)ptr)[sample] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,uint32_t>((char *)ptr)[sample] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,uint32_t>((unsigned short *)ptr)[sample] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,uint32_t>((short *)ptr)[sample] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,uint32_t>((uint32_t *)ptr)[sample] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,uint32_t>((int *)ptr)[sample] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,uint32_t>((uint64_t *)ptr)[sample] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,uint32_t>((int64_t *)ptr)[sample] = value; break;
    default:
        ASSERT (0);
    }
}



OIIO_NAMESPACE_END
