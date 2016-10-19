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
#include <algorithm>

#include <OpenEXR/half.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/deepdata.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/strutil.h"

OIIO_NAMESPACE_BEGIN


// Each pixel has a capacity (number of samples allocated) and a number of
// samples currently used. Erasing samples only reduces the samples in the
// pixels without changing the capacity, so there is no reallocation or data
// movement except for that pixel. Samples can be added without any
// reallocation or copying data (other than that one pixel) unles the
// capacity of the pixel is exceeded. Furthermore, only changes in capacity
// need to lock the mutex. As long as capacity is not changing, threads may
// change number of samples (inserting or deleting) as well as altering
// data, simultaneously, as long as they are working on separate pixels.



class DeepData::Impl {  // holds all the nontrivial stuff
    friend class DeepData;
public:
    std::vector<TypeDesc> m_channeltypes;  // for each channel [c]
    std::vector<size_t> m_channelsizes;    // for each channel [c]
    std::vector<size_t> m_channeloffsets;  // for each channel [c]
    std::vector<unsigned int> m_nsamples;  // for each pixel [p]
    std::vector<unsigned int> m_capacity;  // for each pixel [p]
    std::vector<unsigned int> m_cumcapacity;  // cumulative capacity before pixel [p]
    std::vector<char> m_data;              // for each sample [p][s][c]
    std::vector<std::string> m_channelnames; // For each channel[c]
    std::vector<int> m_myalphachannel;     // For each channel[c], its alpha
      // myalphachannel[c] gives the alpha channel corresponding to channel
      // c, or c if it is itself an alpha, or -1 if it doesn't appear to
      // be a color channel at all.
    size_t m_samplesize;
    int m_z_channel, m_zback_channel;
    int m_alpha_channel;
    bool m_allocated;
    spin_mutex m_mutex;

    Impl () : m_allocated(false) { clear(); }

    void clear () {
        m_channeltypes.clear();
        m_channelsizes.clear();
        m_channeloffsets.clear();
        m_nsamples.clear();
        m_capacity.clear();
        m_cumcapacity.clear();
        m_data.clear();
        m_channelnames.clear ();
        m_myalphachannel.clear ();
        m_samplesize = 0;
        m_z_channel = -1;
        m_zback_channel = -1;
        m_alpha_channel = -1;
        m_allocated = false;
    }

    // If not already done, allocate data and cumcapacity
    void alloc (size_t npixels) {
        if (! m_allocated) {
            spin_lock lock (m_mutex);
            if (! m_allocated) {
                // m_cumcapacity.resize (npixels);
                size_t totalcapacity = 0;
                for (size_t i = 0; i < npixels; ++i) {
                    m_cumcapacity[i] = totalcapacity;
                    totalcapacity += m_capacity[i];
                }
                m_data.resize (totalcapacity * m_samplesize);
                m_allocated = true;
            }
        }
    }

    size_t data_offset (int pixel, int channel, int sample) {
        DASSERT (int(m_cumcapacity.size()) > pixel);
        DASSERT (m_capacity[pixel] >= m_nsamples[pixel]);
        return (m_cumcapacity[pixel] + sample) * m_samplesize
             + m_channeloffsets[channel];
    }

    void * data_ptr (int pixel, int channel, int sample) {
        size_t offset = data_offset (pixel, channel, sample);
        DASSERT (offset < m_data.size());
        return &m_data[offset];
    }

    size_t total_capacity () const {
        return m_cumcapacity.back() + m_capacity.back();
    }

    inline void sanity () const {
        // int nchannels = int (m_channeltypes.size());
        ASSERT (m_channeltypes.size() == m_channelsizes.size());
        ASSERT (m_channeltypes.size() == m_channeloffsets.size());
        int npixels = int(m_capacity.size());
        ASSERT (m_nsamples.size() == m_capacity.size());
        ASSERT (m_cumcapacity.size() == m_capacity.size());
        if (m_allocated) {
            size_t totalcapacity = 0;
            for (int p = 0; p < npixels; ++p) {
                ASSERT (m_cumcapacity[p] == totalcapacity);
                totalcapacity += m_capacity[p];
                ASSERT (m_capacity[p] >= m_nsamples[p]);
            }
            ASSERT (totalcapacity * m_samplesize == m_data.size());
        }
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



string_view
DeepData::channelname (int c) const
{
    DASSERT (m_impl);
    return (c >= 0 && c < m_nchannels) ? string_view(m_impl->m_channelnames[c])
                                       : string_view();
}



TypeDesc
DeepData::channeltype (int c) const
{
    DASSERT (m_impl);
    return (c >= 0 && c < m_nchannels) ? m_impl->m_channeltypes[c]
                                       : TypeDesc();
}



size_t
DeepData::channelsize (int c) const
{
    DASSERT (m_impl);
    return (c >= 0 && c < m_nchannels) ? m_impl->m_channelsizes[c] : 0;
}



size_t
DeepData::samplesize () const
{
    return m_impl->m_samplesize;
}



// Is name the same as suffix, or does it end in ".suffix"?
inline bool
is_or_endswithdot (string_view name, string_view suffix)
{
    return (Strutil::iequals (name, suffix) ||
            (name.size() > suffix.size() &&
             Strutil::iends_with (name, suffix) &&
             name[name.size()-suffix.size()-1] == '.'));
}



void
DeepData::init (int npix, int nchan,
                array_view<const TypeDesc> channeltypes,
                array_view<const std::string> channelnames)
{
    clear ();
    m_npixels = npix;
    m_nchannels = nchan;
    ASSERT (channeltypes.size() >= 1);
    if (! m_impl)
        m_impl = new Impl;
    if (int(channeltypes.size()) >= nchan) {
        m_impl->m_channeltypes.assign (channeltypes.data(), channeltypes.data()+nchan);
    } else {
        m_impl->m_channeltypes.clear ();
        m_impl->m_channeltypes.resize (m_nchannels, channeltypes[0]);
    }
    m_impl->m_channelsizes.resize (m_nchannels);
    m_impl->m_channeloffsets.resize (m_nchannels);
    m_impl->m_channelnames.resize (m_nchannels);
    m_impl->m_myalphachannel.resize (m_nchannels, -1);
    m_impl->m_samplesize = 0;
    m_impl->m_nsamples.resize (m_npixels, 0);
    m_impl->m_capacity.resize (m_npixels, 0);
    m_impl->m_cumcapacity.resize (m_npixels, 0);

    // Channel name hunt
    // First, find Z, Zback, A
    for (int c = 0; c < m_nchannels; ++c) {
        size_t size = m_impl->m_channeltypes[c].size();
        m_impl->m_channelsizes[c] = size;
        m_impl->m_channeloffsets[c] = m_impl->m_samplesize;
        m_impl->m_samplesize += size;
        m_impl->m_channelnames[c] = channelnames[c];
        if (m_impl->m_z_channel < 0 && is_or_endswithdot (channelnames[c], "Z"))
            m_impl->m_z_channel = c;
        else if (m_impl->m_zback_channel < 0 && is_or_endswithdot (channelnames[c], "Zback"))
            m_impl->m_zback_channel = c;
        else if (m_impl->m_alpha_channel < 0 && is_or_endswithdot (channelnames[c], "A"))
            m_impl->m_alpha_channel = c;
    }
    // Now try to find which alpha corresponds to each channel
    for (int c = 0; c < m_nchannels; ++c) {
        // Skip non-color channels
        if (c == m_impl->m_z_channel || c == m_impl->m_zback_channel ||
            m_impl->m_channeltypes[c] == TypeDesc::UINT32)
            continue;
        string_view name (channelnames[c]);
        // Alpha channels are their own alpha
        if (is_or_endswithdot (name, "A")  || is_or_endswithdot (name, "RA") ||
            is_or_endswithdot (name, "GA") || is_or_endswithdot (name, "BA")) {
            m_impl->m_myalphachannel[c] = c;
            continue;
        }
        // For anything else, try to find its channel
        string_view prefix = name, suffix = name;
        size_t dot = name.find_last_of ('.');
        if (dot == string_view::npos) { // No dot
            prefix.clear ();
        } else { // dot
            prefix = prefix.substr (0, dot+1);
            suffix = suffix.substr (dot+1);
        }
        std::string targetalpha = std::string(prefix) + std::string(suffix) + "A";
        for (int i = 0; i < m_nchannels; ++i) {
            if (Strutil::iequals (m_impl->m_channelnames[i], targetalpha)) {
                m_impl->m_myalphachannel[c] = i;
                break;
            }
        }
        if (m_impl->m_myalphachannel[c] < 0)
            m_impl->m_myalphachannel[c] = m_impl->m_alpha_channel;
    }
}



void
DeepData::init (const ImageSpec &spec)
{
    if (int(spec.channelformats.size()) == spec.nchannels)
        init ((int) spec.image_pixels(), spec.nchannels, spec.channelformats,
              spec.channelnames);
    else
        init ((int) spec.image_pixels(), spec.nchannels, spec.format,
              spec.channelnames);
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
DeepData::capacity (int pixel) const
{
    if (pixel < 0 || pixel >= m_npixels)
        return 0;
    DASSERT (m_impl && m_impl->m_capacity.size() > size_t(pixel));
    return m_impl->m_capacity[pixel];
}



void
DeepData::set_capacity (int pixel, int samps)
{
    if (pixel < 0 || pixel >= m_npixels)
        return;
    ASSERT (m_impl);
    spin_lock lock (m_impl->m_mutex);
    if (m_impl->m_allocated) {
        // Data already allocated. Expand capacity if necessary, don't
        // contract. (FIXME?)
        int n = (int)capacity(pixel);
        if (samps > n) {
            int toadd = samps-n;
            if (m_impl->m_data.empty()) {
                size_t newtotal = (m_impl->total_capacity()+toadd);
                m_impl->m_data.resize (newtotal*samplesize());
            } else {
                size_t offset = m_impl->data_offset (pixel, 0, n);
                m_impl->m_data.insert (m_impl->m_data.begin() + offset,
                                       toadd*samplesize(), 0);
            }
            // Adjust the cumulative prefix sum of samples for subsequent pixels
            for (int p = pixel+1; p < m_npixels; ++p)
                m_impl->m_cumcapacity[p] += toadd;
            m_impl->m_capacity[pixel] = samps;
        }
    } else {
        m_impl->m_capacity[pixel] = samps;
    }
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
        if (samps > n)
            insert_samples (pixel, n, samps - n);
        else if (samps < n)
            erase_samples (pixel, samps, n-samps);
    } else {
        m_impl->m_nsamples[pixel] = samps;
        m_impl->m_capacity[pixel] = std::max (unsigned(samps), m_impl->m_capacity[pixel]);
    }
}



void
DeepData::set_all_samples (array_view<const unsigned int> samples)
{
    if (samples.size() != size_t(m_npixels))
        return;
    ASSERT (m_impl);
    if (m_impl->m_allocated) {
        // Data already allocated: set pixels individually
        for (int p = 0; p < m_npixels; ++p)
            set_samples (p, int(samples[p]));
    } else {
        // Data not yet allocated: copy in one shot
        m_impl->m_nsamples.assign (&samples[0], &samples[m_npixels]);
        m_impl->m_capacity.assign (&samples[0], &samples[m_npixels]);
    }
}



void
DeepData::insert_samples (int pixel, int samplepos, int n)
{
    int oldsamps = samples(pixel);
    set_capacity (pixel, oldsamps + n);
    // set_capacity is thread-safe, it locks internalls. Once the acpacity
    // is adjusted, we can alter nsamples or copy the data around within
    // the pixel without a lock, we presume that if multiple threads are
    // in play, they are working on separate pixels.
    if (m_impl->m_allocated) {
        // Move the data
        if (samplepos < oldsamps) {
            size_t offset = m_impl->data_offset (pixel, 0, samplepos);
            size_t end = m_impl->data_offset (pixel, 0, oldsamps);
            std::copy_backward (m_impl->m_data.begin() + offset,
                                m_impl->m_data.begin() + end,
                                m_impl->m_data.begin() + end + n*samplesize());
        }
    }
    // Add to this pixel's sample count
    m_impl->m_nsamples[pixel] += n;
}



void
DeepData::erase_samples (int pixel, int samplepos, int n)
{
    // DON'T reduce the capacity! Just leave holes for speed.
    // Because erase_samples only moves data within a pixel and doesn't
    // change the capacity, no lock is needed.
    n = std::min (n, int(m_impl->m_nsamples[pixel]));
    if (m_impl->m_allocated) {
        // Move the data
        int oldsamps = samples(pixel);
        size_t offset = m_impl->data_offset (pixel, 0, samplepos);
        size_t end = m_impl->data_offset (pixel, 0, oldsamps);
        std::copy (m_impl->m_data.begin() + offset + n*samplesize(),
                   m_impl->m_data.begin() + end,
                   m_impl->m_data.begin() + offset);
    }
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
        ASSERT_MSG (0, "Unknown/unsupported data type %d",
                    channeltype(channel).basetype);
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
        ASSERT_MSG (0, "Unknown/unsupported data type %d",
                    channeltype(channel).basetype);
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
        ASSERT_MSG (0, "Unknown/unsupported data type %d",
                    channeltype(channel).basetype);
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
        ASSERT_MSG (0, "Unknown/unsupported data type %d",
                    channeltype(channel).basetype);
    }
}



array_view<const TypeDesc>
DeepData::all_channeltypes () const
{
    ASSERT (m_impl);
    return m_impl->m_channeltypes;
}



array_view<const unsigned int>
DeepData::all_samples () const
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
        if (m_impl->m_nsamples[i])
            for (int c = 0;  c < m_nchannels;  ++c)
                pointers[i*m_nchannels+c] = (void *)m_impl->data_ptr (i, c, 0);
        else
            for (int c = 0;  c < m_nchannels;  ++c)
                pointers[i*m_nchannels+c] = NULL;
    }
}



bool
DeepData::copy_deep_sample (int pixel, int sample,
                            const DeepData &src, int srcpixel, int srcsample)
{
    const void *srcdata = src.data_ptr (srcpixel, 0, srcsample);
    int nchans = channels();
    if (! srcdata || nchans != src.channels())
        return false;
    int nsamples = src.samples(srcpixel);
    set_samples (pixel, std::max (samples(pixel), nsamples));
    for (int c = 0; c < m_nchannels; ++c) {
        if (channeltype(c) == TypeDesc::UINT32 && src.channeltype(c) == TypeDesc::UINT32)
            set_deep_value (pixel, c, sample,
                            src.deep_value_uint (srcpixel, c, srcsample));
        else
            set_deep_value (pixel, c, sample,
                            src.deep_value (srcpixel, c, srcsample));
    }
    return true;
}



bool
DeepData::copy_deep_pixel (int pixel, const DeepData &src, int srcpixel)
{
    if (pixel < 0 || pixel >= pixels()) {
        // std::cout << "dst pixel was " << pixel << "\n";
        DASSERT (0 && "Out of range pixel index");
        return false;
    }
    if (srcpixel < 0 || srcpixel >= src.pixels()) {
        // Copying empty pixel -- set samples to 0 and we're done
        // std::cout << "Source pixel was " << srcpixel << "\n";
        set_samples (pixel, 0);
        return true;
    }
    int nchans = channels();
    if (nchans != src.channels()) {
        DASSERT (0 && "Number of channels don't match.");
        return false;
    }
    int nsamples = src.samples(srcpixel);
    set_samples (pixel, nsamples);
    if (nsamples == 0)
        return true;
    bool sametypes = samplesize() == src.samplesize();
    if (sametypes)
        for (int c = 0; c < nchans; ++c)
            sametypes &= (channeltype(c) == src.channeltype(c));
    if (sametypes)
        memcpy (data_ptr (pixel, 0, 0), src.data_ptr (srcpixel, 0, 0),
                samplesize()*nsamples);
    else {
        for (int c = 0; c < nchans; ++c) {
            if (channeltype(c) == TypeDesc::UINT32 && src.channeltype(c) == TypeDesc::UINT32)
                for (int s = 0; s < nsamples; ++s)
                    set_deep_value (pixel, c, s,
                                    src.deep_value_uint (srcpixel, c, s));
            else
                for (int s = 0; s < nsamples; ++s)
                    set_deep_value (pixel, c, s,
                                    src.deep_value (srcpixel, c, s));
        }
    }
    return true;
}



void
DeepData::split (int pixel, float depth)
{
#if OIIO_CPLUSPLUS_VERSION >= 11
    using std::log1p;
    using std::expm1;
#endif
    int zchan = m_impl->m_z_channel;
    int zbackchan = m_impl->m_zback_channel;
    if (zchan < 0)
        return;   // No channel labeled Z -- we don't know what to do
    if (zbackchan < 0)
        return;   // The samples are not extended -- nothing to split
    int nchans = channels();
    for (int s = 0; s < samples(pixel); ++s) {
        float zf = deep_value (pixel, zchan, s);     // z front
        float zb = deep_value (pixel, zbackchan, s); // z back
        if (zf < depth && zb > depth) {
            // The sample spans depth, so split it.
            // See http://www.openexr.com/InterpretingDeepPixels.pdf
            insert_samples (pixel, s+1);
            copy_deep_sample (pixel, s+1, *this, pixel, s);
            set_deep_value (pixel, zbackchan, s,   depth);
            set_deep_value (pixel, zchan,     s+1, depth);
            // We have to proceed in two passes, since we may reuse the
            // alpha values, we can't overwrite them yet.
            for (int c = 0; c < nchans; ++c) {
                int alphachan = m_impl->m_myalphachannel[c];
                if (alphachan < 0   // No alpha
                      || alphachan == c)  // This is an alpha!
                    continue;
                float a = clamp (deep_value (pixel, alphachan, s), 0.0f, 1.0f);
                if (a == 1.0f) // Opaque or channels without alpha, we're done.
                    continue;
                float xf = (depth - zf) / (zb - zf);
                float xb = (zb - depth) / (zb - zf);
                if (a > std::numeric_limits<float>::min()) {
                    float af = -expm1 (xf * log1p (-a));
                    float ab = -expm1 (xb * log1p (-a));
                    float val = deep_value (pixel, c, s);
                    set_deep_value (pixel, c, s,   (af/a) * val);
                    set_deep_value (pixel, c, s+1, (ab/a) * val);
                } else {
                    float val = deep_value (pixel, c, s);
                    set_deep_value (pixel, c, s,   val * xf);
                    set_deep_value (pixel, c, s+1, val * xb);
                }
            }
            // Now that we've adjusted the colors, do the alphas
            for (int c = 0; c < nchans; ++c) {
                int alphachan = m_impl->m_myalphachannel[c];
                if (alphachan != c)
                    continue;  // skip if not an alpha
                float a = clamp (deep_value (pixel, alphachan, s), 0.0f, 1.0f);
                if (a == 1.0f) // Opaque or channels without alpha, we're done.
                    continue;
                float xf = (depth - zf) / (zb - zf);
                float xb = (zb - depth) / (zb - zf);
                if (a > std::numeric_limits<float>::min()) {
                    float af = -expm1 (xf * log1p (-a));
                    float ab = -expm1 (xb * log1p (-a));
                    set_deep_value (pixel, c, s,   af);
                    set_deep_value (pixel, c, s+1, ab);
                } else {
                    set_deep_value (pixel, c, s,   a * xf);
                    set_deep_value (pixel, c, s+1, a * xb);
                }
            }
        }
    }
}



namespace {

// Comparitor functor for depth sorting sample indices of a deep pixel.
class SampleComparator {
public:
    SampleComparator (const DeepData &dd, int pixel, int zchan, int zbackchan)
        : deepdata(dd), pixel(pixel), zchan(zchan), zbackchan(zbackchan) { }
    bool operator() (int i, int j) const {
        // If either has a lower z, that's the lower
        float iz = deepdata.deep_value (pixel, zchan, i);
        float jz = deepdata.deep_value (pixel, zchan, j);
        if (iz < jz)
            return true;
        if (iz > jz)
            return false;
        // If both z's are equal, sort based on zback
        float izb = deepdata.deep_value (pixel, zbackchan, i);
        float jzb = deepdata.deep_value (pixel, zbackchan, j);
        return izb < jzb;
    }
private:
    const DeepData &deepdata;
    int pixel;
    int zchan, zbackchan;
};

}



void
DeepData::sort (int pixel)
{
    int zchan = m_impl->m_z_channel;
    if (zchan < 0)
        return;   // No channel labeled Z -- we don't know what to do
    int zbackchan = m_impl->m_z_channel;
    if (zbackchan < 0)
        zbackchan = zchan;
    int nsamples = samples(pixel);
    if (nsamples < 2)
        return;   // 0 or 1 samples -- no sort necessary

    // Ick, std::sort and friends take a custom comparator, but not a custom
    // swapper, so there's no way to std::sort a data type whose size is not
    // known at compile time. So we just sort the indices!
    int *sample_indices = OIIO_ALLOCA (int, nsamples);
#if OIIO_CPLUSPLUS_VERSION >= 11
    std::iota (sample_indices, sample_indices+nsamples, 0);
#else
    for (int i = 0; i < nsamples; ++i)
        sample_indices[i] = i;
#endif
    std::stable_sort (sample_indices, sample_indices+nsamples,
                      SampleComparator(*this, pixel, zchan, zbackchan));

    // Now copy around using a temp buffer
    size_t samplebytes = samplesize();
    char *tmppixel = OIIO_ALLOCA (char, samplebytes*nsamples);
    memcpy (tmppixel, data_ptr (pixel, 0, 0), samplebytes*nsamples);
    for (int i = 0; i < nsamples; ++i)
        memcpy (data_ptr (pixel, 0, i),
                tmppixel+samplebytes*sample_indices[i], samplebytes);
}



void
DeepData::merge_overlaps (int pixel)
{
#if OIIO_CPLUSPLUS_VERSION >= 11
    using std::log1p;
#endif
    int zchan = m_impl->m_z_channel;
    int zbackchan = m_impl->m_zback_channel;
    if (zchan < 0)
        return;   // No channel labeled Z -- we don't know what to do
    if (zbackchan < 0)
        zbackchan = zchan;  // Missing Zback -- use Z
    int nchans = channels();
    for (int s = 1 /* YES, 1 */; s < samples(pixel); ++s) {
        float zf = deep_value (pixel, zchan, s);     // z front
        float zb = deep_value (pixel, zbackchan, s); // z back
        if (zf == deep_value (pixel, zchan, s-1) &&
            zb == deep_value (pixel, zbackchan, s-1)) {
            // The samples overlap exactly, merge them per
            // See http://www.openexr.com/InterpretingDeepPixels.pdf
            for (int c = 0; c < nchans; ++c) {  // set the colors
                int alphachan = m_impl->m_myalphachannel[c];
                if (alphachan < 0)
                    continue;    // Not color or alpha
                if (alphachan == c)
                    continue;  // Adjust the alphas in a second pass below
                float a1 = (alphachan < 0) ? 1.0f :
                           clamp (deep_value (pixel, alphachan, s-1), 0.0f, 1.0f);
                float a2 = (alphachan < 0) ? 1.0f :
                           clamp (deep_value (pixel, alphachan, s), 0.0f, 1.0f);
                float c1 = deep_value (pixel, c, s-1);
                float c2 = deep_value (pixel, c, s);
                float am = a1 + a2 - a1 * a2;
                float cm;
                if (a1 == 1.0f && a2 == 1.0f)
                    cm = (c1 + c2) / 2.0f;
                else if (a1 == 1.0f)
                    cm = c1;
                else if (a2 == 1.0f)
                    cm = c2;
                else {
                    static const float MAX = std::numeric_limits<float>::max();
                    float u1 = -log1p (-a1);
                    float v1 = (u1 < a1 * MAX)? u1 / a1: 1.0f;
                    float u2 = -log1p (-a2);
                    float v2 = (u2 < a2 * MAX)? u2 / a2: 1.0f;
                    float u = u1 + u2;
                    float w = (u > 1.0f || am < u * MAX)? am / u: 1.0f;
                    cm = (c1 * v1 + c2 * v2) * w;
                }
                set_deep_value (pixel, c, s-1, cm); // setting color
            }
            for (int c = 0; c < nchans; ++c) {  // set the alphas
                int alphachan = m_impl->m_myalphachannel[c];
                if (alphachan != c)
                    continue;    // This pass is only for alphas
                float a1 = (alphachan < 0) ? 1.0f :
                           clamp (deep_value (pixel, alphachan, s-1), 0.0f, 1.0f);
                float a2 = (alphachan < 0) ? 1.0f :
                           clamp (deep_value (pixel, alphachan, s), 0.0f, 1.0f);
                float am = a1 + a2 - a1 * a2;
                set_deep_value (pixel, c, s-1, am); // setting alpha
            }
            // Now eliminate sample s and revisit again
            erase_samples (pixel, s, 1);
            --s;
        }
    }
}



void
DeepData::merge_deep_pixels (int pixel, const DeepData &src, int srcpixel)
{
    int srcsamples = src.samples(srcpixel);
    if (srcsamples == 0)
        return;   // No samples to merge
    int dstsamples = samples(pixel);
    if (dstsamples == 0) {
        // Nothing in our pixel yet, so just copy src's pixel
        copy_deep_pixel (pixel, src, srcpixel);
        return;
    }

    // Need to merge the pixels

    // First, merge all of src's samples into our pixel
    set_samples (pixel, dstsamples+srcsamples);
    for (int i = 0; i < srcsamples; ++i)
        copy_deep_sample (pixel, dstsamples+i, src, srcpixel, i);

    // Now ALL the samples from both images are in our pixel.
    // Mutually split the samples against each other.
    sort (pixel);  // sort first so we only loop once
    int zchan = m_impl->m_z_channel;
    int zbackchan = m_impl->m_zback_channel;
    for (int s = 0; s < samples(pixel); ++s) {
        float z = deep_value (pixel, zchan, s);
        float zback = deep_value (pixel, zbackchan, s);
        split (pixel, z);
        split (pixel, zback);
    }
    sort (pixel);

    // Now merge the overlaps
    merge_overlaps (pixel);
}



void
DeepData::occlusion_cull (int pixel)
{
    int alpha_channel = m_impl->m_alpha_channel;
    if (alpha_channel < 0)
        return;   // If there isn't a definitive alpha channel, never mind
    int nsamples = samples(pixel);
    for (int s = 0; s < nsamples; ++s) {
        if (deep_value (pixel, alpha_channel, s) >= 1.0f) {
            // We hit an opaque sample. Cull everything farther.
            set_samples (pixel, s+1);
            break;
        }
    }
}


OIIO_NAMESPACE_END
