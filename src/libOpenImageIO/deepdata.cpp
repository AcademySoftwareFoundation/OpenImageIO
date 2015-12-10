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



class DeepData::Impl {  // holds all the nontrivial stuff
public:
    std::vector<TypeDesc> m_channeltypes;  // for each channel [c]
    std::vector<unsigned int> m_nsamples;// for each pixel [z][y][x]
    std::vector<void *> m_pointers;    // for each channel per pixel [z][y][x][c]
    std::vector<char> m_data;          // for each sample [z][y][x][c][s]

    void clear () {
        m_channeltypes.clear();
        m_nsamples.clear();
        m_pointers.clear();
        m_data.clear();
    }
};



DeepData::DeepData () : m_impl(NULL), m_npixels(0), m_nchannels(0)
{
}



DeepData::DeepData (const ImageSpec &spec) : m_impl(NULL)
{
    init (spec);
}



DeepData::~DeepData ()
{
    delete m_impl;
}



DeepData::DeepData (const DeepData &d) : m_impl(NULL)
{
    m_npixels = d.m_npixels;
    m_nchannels = d.m_nchannels;
    if (d.m_impl) {
        m_impl = new Impl;
        *m_impl = *(d.m_impl);
    }
}



const DeepData&
DeepData::operator= (const DeepData &d)
{
    if (this != &d) {
        m_npixels = d.m_npixels;
        m_nchannels = d.m_nchannels;
        if (! m_impl)
            m_impl = new Impl;
        if (d.m_impl)
            *m_impl = *(d.m_impl);
        else
            m_impl->clear ();
    }
    return *this;
}



int DeepData::pixels () const
{
    return m_npixels;
}



int DeepData::channels () const
{
    return m_nchannels;
}



TypeDesc DeepData::channeltype (int c) const
{
    ASSERT (m_impl);
    return m_impl->m_channeltypes[c];
}



void
DeepData::init (int npix, int nchan,
                array_view<const TypeDesc> channeltypes)
{
    clear ();
    m_npixels = npix;
    m_nchannels = nchan;
    ASSERT (channeltypes.size() == 1 || int(channeltypes.size()) == nchan);
    if (! m_impl)
        m_impl = new Impl;
    if (channeltypes.size() == 1) {
        m_impl->m_channeltypes.clear ();
        m_impl->m_channeltypes.resize (m_nchannels, channeltypes[0]);
    } else {
        m_impl->m_channeltypes.assign (channeltypes.data(), channeltypes.data()+channeltypes.size());
    }
    m_impl->m_nsamples.resize (m_npixels, 0);
    m_impl->m_pointers.resize (size_t(m_npixels)*size_t(m_nchannels), NULL);
}



void
DeepData::init (const ImageSpec &spec)
{
    if (int(spec.channelformats.size()) == spec.nchannels)
        init ((int) spec.image_pixels(), spec.nchannels, spec.channelformats);
    else
        init ((int) spec.image_pixels(), spec.nchannels, spec.format);
}



void
DeepData::alloc ()
{
    ASSERT (m_impl);
    // Calculate the total size we need, align each channel to 4 byte boundary
    size_t totalsamples = 0, totalbytes = 0;
    for (int i = 0;  i < m_npixels;  ++i) {
        if (int s = m_impl->m_nsamples[i]) {
            totalsamples += s;
            for (int c = 0;  c < m_nchannels;  ++c)
                totalbytes += round_to_multiple (channeltype(c).size() * s, 4);
        }
    }

    // Allocate a minimum of 4 bytes so that we can tell if alloc() was
    // called by whether m_impl->m_data.size() > 0.
    totalbytes = std::max (totalbytes, size_t(4));

    // Set all the data pointers to the right offsets within the
    // data block.  Leave the pointes NULL for pixels with no samples.
    m_impl->m_data.resize (totalbytes);
    char *p = &m_impl->m_data[0];
    for (int i = 0;  i < m_npixels;  ++i) {
        if (int s = m_impl->m_nsamples[i]) {
            for (int c = 0;  c < m_nchannels;  ++c) {
                m_impl->m_pointers[i*m_nchannels+c] = p;
                p += round_to_multiple (channeltype(c).size()*s, 4);
            }
        }
    }
}



void
DeepData::clear ()
{
    m_npixels = 0;
    m_nchannels = 0;
    if (m_impl)
        m_impl->clear ();
}



void
DeepData::free ()
{
    clear ();
    delete m_impl;
    m_impl = NULL;
}



int
DeepData::samples (int pixel) const
{
    if (pixel < 0 || pixel >= m_npixels)
        return 0;
    DASSERT (m_impl);
    return m_impl->m_nsamples[pixel];
}



void
DeepData::set_samples (int pixel, int samps)
{
    ASSERT (pixel >= 0 && pixel < m_npixels && "invalid pixel index");
    ASSERT (m_impl);
    ASSERT (m_impl->m_data.size() == 0 && "set_samples may not be called after alloc()");
    m_impl->m_nsamples[pixel] = samps;
}



void *
DeepData::channel_ptr (int pixel, int channel)
{
    if (pixel < 0 || pixel >= m_npixels ||
          channel < 0 || channel >= m_nchannels || !m_impl)
        return NULL;
    return m_impl->m_pointers[pixel*m_nchannels + channel];
}



const void *
DeepData::channel_ptr (int pixel, int channel) const
{
    if (pixel < 0 || pixel >= m_npixels ||
            channel < 0 || channel >= m_nchannels || !m_impl)
        return NULL;
    return m_impl->m_pointers[pixel*m_nchannels + channel];
}



float
DeepData::deep_value (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= m_npixels || channel < 0 ||
            channel >= m_nchannels || !m_impl)
        return 0.0f;
    int nsamps = m_impl->m_nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = m_impl->m_pointers[pixel*m_nchannels + channel];
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
    if (pixel < 0 || pixel >= m_npixels || channel < 0 ||
            channel >= m_nchannels || !m_impl)
        return 0.0f;
    int nsamps = m_impl->m_nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = m_impl->m_pointers[pixel*m_nchannels + channel];
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
    if (pixel < 0 || pixel >= m_npixels || channel < 0 ||
            channel >= m_nchannels || !m_impl)
        return;
    int nsamps = m_impl->m_nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    if (! m_impl->m_data.size())
        alloc();
    void *ptr = m_impl->m_pointers[pixel*m_nchannels + channel];
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
    if (pixel < 0 || pixel >= m_npixels || channel < 0 ||
            channel >= m_nchannels || !m_impl)
        return;
    int nsamps = m_impl->m_nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    if (! m_impl->m_data.size())
        alloc();
    void *ptr = m_impl->m_pointers[pixel*m_nchannels + channel];
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



array_view<const unsigned int>
DeepData::all_nsamples () const
{
    ASSERT (m_impl);
    return m_impl->m_nsamples;
}



void * const *
DeepData::all_pointers () const
{
    ASSERT (m_impl);
    return m_npixels ? &m_impl->m_pointers[0] : NULL;
}



array_view<const char>
DeepData::all_data () const
{
    ASSERT (m_impl);
    return m_impl->m_data;
}


OIIO_NAMESPACE_END
