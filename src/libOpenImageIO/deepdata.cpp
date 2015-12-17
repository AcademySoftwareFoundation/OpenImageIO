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
#include <numeric>

#include <OpenEXR/half.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/deepdata.h"

OIIO_NAMESPACE_BEGIN



class DeepData::Impl {  // holds all the nontrivial stuff
public:
    std::vector<TypeDesc> m_channeltypes;  // for each channel [c]
    std::vector<size_t> m_channelsizes;    // for each channel [c]
    std::vector<size_t> m_channeloffsets;  // for each channel [c]
    std::vector<unsigned int> m_nsamples;  // for each pixel [p]
    std::vector<size_t> m_cumsamples;      // cumulative samples before pixel [p]
    std::vector<char> m_data;              // for each sample [p][s][c]
    size_t m_samplesize;
    bool m_allocated;

    Impl () : m_allocated(false) {}

    void clear () {
        m_channeltypes.clear();
        m_channelsizes.clear();
        m_channeloffsets.clear();
        m_nsamples.clear();
        m_cumsamples.clear();
        m_data.clear();
        m_samplesize = 0;
        m_allocated = false;
    }

    // If not already done, allocate data and cumsamples
    void alloc (size_t npixels) {
        if (! m_allocated) {
            m_cumsamples.resize (npixels);
            size_t totalsamples = 0;
            for (size_t i = 0; i < npixels; ++i) {
                m_cumsamples[i] = totalsamples;
                totalsamples += m_nsamples[i];
            }
            m_data.resize (totalsamples * m_samplesize);
            m_allocated = true;
        }
    }

    size_t data_offset (int pixel, int channel, int sample) {
        DASSERT (int(m_cumsamples.size()) > pixel);
        return (m_cumsamples[pixel] + sample) * m_samplesize
             + m_channeloffsets[channel];
    }

    void * data_ptr (int pixel, int channel, int sample) {
        size_t offset = data_offset (pixel, channel, sample);
        DASSERT (offset < m_data.size());
        return &m_data[offset];
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



TypeDesc
DeepData::channeltype (int c) const
{
    DASSERT (m_impl && c >= 0 && c < m_nchannels);
    return m_impl->m_channeltypes[c];
}



size_t
DeepData::channelsize (int c) const
{
    DASSERT (m_impl && c >= 0 && c < m_nchannels);
    return m_impl->m_channelsizes[c];
}



size_t
DeepData::samplesize () const
{
    return m_impl->m_samplesize;
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
    m_impl->m_channelsizes.resize (m_nchannels);
    m_impl->m_channeloffsets.resize (m_nchannels);
    m_impl->m_samplesize = 0;
    for (int i = 0; i < m_nchannels; ++i) {
        size_t size = m_impl->m_channeltypes[i].size();
        m_impl->m_channelsizes[i] = size;
        m_impl->m_channeloffsets[i] = m_impl->m_samplesize;
        m_impl->m_samplesize += size;
    }
    m_impl->m_nsamples.resize (m_npixels, 0);
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
    if (pixel < 0 || pixel >= m_npixels)
        return;
    ASSERT (m_impl);
    if (m_impl->m_allocated) {
        // Data already allocated. Turn it into an insert or delete
        int n = (int)samples(pixel);
        int diff = abs (samps - n);
        if (samps < n)
            insert_samples (pixel, n, diff);
        else
            erase_samples (pixel, n-diff, diff);
    } else {
        m_impl->m_nsamples[pixel] = samps;
    }
}



void
DeepData::insert_samples (int pixel, int samplepos, int n)
{
    if (m_impl->m_allocated) {
        // Move the data
        size_t offset = m_impl->data_offset (pixel, 0, samplepos);
        m_impl->m_data.insert (m_impl->m_data.begin() + offset,
                               n*samplesize(), 0);
        // Adjust the cumulative prefix sum of samples for subsequent pixels
        for (int p = pixel+1; p < m_npixels; ++p)
            m_impl->m_cumsamples[p] += size_t(n);
    }
    // Add to this pixel's sample count
    m_impl->m_nsamples[pixel] += n;
}



void
DeepData::erase_samples (int pixel, int samplepos, int n)
{
    n = std::min (n, int(m_impl->m_nsamples[pixel]));
    if (m_impl->m_allocated) {
        // Move the data
        size_t offset = m_impl->data_offset (pixel, 0, samplepos);
        m_impl->m_data.erase (m_impl->m_data.begin() + offset,
                              m_impl->m_data.begin() + offset + n*samplesize());
        // Adjust the cumulative prefix sum of samples for subsequent pixels
        for (int p = pixel+1; p < m_npixels; ++p)
            m_impl->m_cumsamples[p] -= size_t(n);
    }
    // Subtract from this pixel's sample count
    m_impl->m_nsamples[pixel] -= n;
}



void *
DeepData::data_ptr (int pixel, int channel, int sample)
{
    m_impl->alloc (m_npixels);
    if (pixel < 0 || pixel >= m_npixels ||
          channel < 0 || channel >= m_nchannels || !m_impl ||
          sample < 0 || sample >= int(m_impl->m_nsamples[pixel]))
        return NULL;
    return m_impl->data_ptr (pixel, channel, sample);
}



const void *
DeepData::data_ptr (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= m_npixels ||
            channel < 0 || channel >= m_nchannels ||
            !m_impl || !m_impl->m_data.size() ||
            sample < 0 || sample >= int(m_impl->m_nsamples[pixel]))
        return NULL;
    return m_impl->data_ptr (pixel, channel, sample);
}



float
DeepData::deep_value (int pixel, int channel, int sample) const
{
    const void *ptr = data_ptr (pixel, channel, sample);
    if (! ptr)
        return 0.0f;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        return ((const float *)ptr)[0];
    case TypeDesc::HALF  :
        return ((const half *)ptr)[0];
    case TypeDesc::UINT  :
        return ConstDataArrayProxy<unsigned int,float>((const unsigned int *)ptr)[0];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,float>((const unsigned char *)ptr)[0];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,float>((const char *)ptr)[0];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,float>((const unsigned short *)ptr)[0];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,float>((const short *)ptr)[0];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,float>((const int *)ptr)[0];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,float>((const unsigned long long *)ptr)[0];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,float>((const long long *)ptr)[0];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



uint32_t
DeepData::deep_value_uint (int pixel, int channel, int sample) const
{
    const void *ptr = data_ptr (pixel, channel, sample);
    if (! ptr)
        return 0;
    switch (channeltype(channel).basetype) {
    case TypeDesc::UINT  :
        return ((const unsigned int *)ptr)[0];
    case TypeDesc::FLOAT :
        return ConstDataArrayProxy<float,uint32_t>((const float *)ptr)[0];
    case TypeDesc::HALF  :
        return ConstDataArrayProxy<half,uint32_t>((const half *)ptr)[0];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,uint32_t>((const unsigned char *)ptr)[0];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,uint32_t>((const char *)ptr)[0];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,uint32_t>((const unsigned short *)ptr)[0];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,uint32_t>((const short *)ptr)[0];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,uint32_t>((const int *)ptr)[0];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,uint32_t>((const unsigned long long *)ptr)[0];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,uint32_t>((const long long *)ptr)[0];
    default:
        ASSERT (0);
        return 0;
    }
}



void
DeepData::set_deep_value (int pixel, int channel, int sample, float value)
{
    void *ptr = data_ptr (pixel, channel, sample);
    if (! ptr)
        return;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,float>((float *)ptr)[0] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,float>((half *)ptr)[0] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,float>((uint32_t *)ptr)[0] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,float>((unsigned char *)ptr)[0] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,float>((char *)ptr)[0] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,float>((unsigned short *)ptr)[0] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,float>((short *)ptr)[0] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,float>((int *)ptr)[0] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,float>((uint64_t *)ptr)[0] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,float>((int64_t *)ptr)[0] = value; break;
    default:
        ASSERT (0);
    }
}



void
DeepData::set_deep_value (int pixel, int channel, int sample, uint32_t value)
{
    void *ptr = data_ptr (pixel, channel, sample);
    if (! ptr)
        return;
    switch (channeltype(channel).basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,uint32_t>((float *)ptr)[0] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,uint32_t>((half *)ptr)[0] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,uint32_t>((unsigned char *)ptr)[0] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,uint32_t>((char *)ptr)[0] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,uint32_t>((unsigned short *)ptr)[0] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,uint32_t>((short *)ptr)[0] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,uint32_t>((uint32_t *)ptr)[0] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,uint32_t>((int *)ptr)[0] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,uint32_t>((uint64_t *)ptr)[0] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,uint32_t>((int64_t *)ptr)[0] = value; break;
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



array_view<const char>
DeepData::all_data () const
{
    ASSERT (m_impl);
    m_impl->alloc (m_npixels);
    return m_impl->m_data;
}



void
DeepData::get_pointers (std::vector<void*> &pointers) const
{
    ASSERT (m_impl);
    m_impl->alloc (m_npixels);
    pointers.resize (pixels()*channels());
    for (int i = 0;  i < m_npixels;  ++i) {
        for (int c = 0;  c < m_nchannels;  ++c)
            pointers[i*m_nchannels+c] = (void *)m_impl->data_ptr (i, c, 0);
    }
}



OIIO_NAMESPACE_END
