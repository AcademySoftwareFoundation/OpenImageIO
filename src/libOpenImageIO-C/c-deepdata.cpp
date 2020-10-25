// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

#include <OpenImageIO/c-deepdata.h>

#include "util.h"

DEFINE_POINTER_CASTS(DeepData)
DEFINE_POINTER_CASTS(ImageSpec)
#undef DEFINE_POINTER_CASTS

using OIIO::bit_cast;
using OIIO::Strutil::safe_strcpy;

extern "C" {

OIIO_DeepData*
OIIO_DeepData_new()
{
    return to_c(new OIIO::DeepData);
}



OIIO_DeepData*
OIIO_DeepData_new_with_imagespec(const OIIO_ImageSpec* is)
{
    return to_c(new OIIO::DeepData(*to_cpp(is)));
}



OIIO_DeepData*
OIIO_DeepData_copy(const OIIO_DeepData* dd)
{
    return to_c(new OIIO::DeepData(*to_cpp(dd)));
}



void
OIIO_DeepData_delete(const OIIO_DeepData* dd)
{
    delete to_cpp(dd);
}



void
OIIO_DeepData_clear(OIIO_DeepData* dd)
{
    to_cpp(dd)->clear();
}



void
OIIO_DeepData_free(OIIO_DeepData* dd)
{
    to_cpp(dd)->free();
}



void
OIIO_DeepData_init(OIIO_DeepData* dd, int64_t npix, int nchan,
                   const OIIO_TypeDesc* channeltypes, int nchanneltypes,
                   const char** channelnames, int nchannelnames)
{
    /// FIXME: an awful lot of copying here
    std::vector<std::string> names(nchannelnames);
    for (int i = 0; i < nchannelnames; ++i) {
        names.emplace_back(channelnames[i]);
    }
    to_cpp(dd)->init(npix, nchan,
                     OIIO::cspan<OIIO::TypeDesc> {
                         (OIIO::TypeDesc*)channeltypes, nchanneltypes },
                     OIIO::cspan<std::string> { names });
}



void
OIIO_DeepData_init_with_imagespec(OIIO_DeepData* dd, const OIIO_ImageSpec* is)
{
    to_cpp(dd)->init(*to_cpp(is));
}



bool
OIIO_DeepData_initialized(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->initialized();
}



bool
OIIO_DeepData_allocated(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->allocated();
}



int64_t
OIIO_DeepData_pixels(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->pixels();
}



int
OIIO_DeepData_channels(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->channels();
}



int
OIIO_DeepData_Z_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->Z_channel();
}



int
OIIO_DeepData_Zback_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->Zback_channel();
}



int
OIIO_DeepData_A_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->A_channel();
}



int
OIIO_DeepData_AR_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->AR_channel();
}



int
OIIO_DeepData_AG_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->AG_channel();
}



int
OIIO_DeepData_AB_channel(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->AB_channel();
}



const char*
OIIO_DeepData_channelname(const OIIO_DeepData* dd, int c)
{
    return to_cpp(dd)->channelname(c).c_str();
}



OIIO_TypeDesc
OIIO_DeepData_channeltype(const OIIO_DeepData* dd, int c)
{
    return bit_cast<OIIO::TypeDesc, OIIO_TypeDesc>(to_cpp(dd)->channeltype(c));
}



size_t
OIIO_DeepData_channelsize(const OIIO_DeepData* dd, int c)
{
    return to_cpp(dd)->channelsize(c);
}



size_t
OIIO_DeepData_samplesize(const OIIO_DeepData* dd)
{
    return to_cpp(dd)->samplesize();
}



int
OIIO_DeepData_samples(const OIIO_DeepData* dd, int64_t pixel)
{
    return to_cpp(dd)->samples(pixel);
}



void
OIIO_DeepData_set_samples(OIIO_DeepData* dd, int64_t pixel, int samps)
{
    to_cpp(dd)->set_samples(pixel, samps);
}



void
OIIO_DeepData_set_all_samples(OIIO_DeepData* dd, const uint32_t* samples,
                              int nsamples)
{
    to_cpp(dd)->set_all_samples(OIIO::cspan<uint32_t>(samples, nsamples));
}



void
OIIO_DeepData_set_capacity(OIIO_DeepData* dd, int64_t pixel, int samps)
{
    to_cpp(dd)->set_capacity(pixel, samps);
}



void
OIIO_DeepData_insert_samples(OIIO_DeepData* dd, int64_t pixel, int samplepos,
                             int n)
{
    to_cpp(dd)->insert_samples(pixel, samplepos, n);
}



void
OIIO_DeepData_erase_samples(OIIO_DeepData* dd, int64_t pixel, int samplepos,
                            int n)
{
    to_cpp(dd)->erase_samples(pixel, samplepos, n);
}



float
OIIO_DeepData_deep_value(const OIIO_DeepData* dd, int64_t pixel, int channel,
                         int sample)
{
    return to_cpp(dd)->deep_value(pixel, channel, sample);
}



uint32_t
OIIO_DeepData_deep_value_uint(const OIIO_DeepData* dd, int64_t pixel,
                              int channel, int sample)
{
    return to_cpp(dd)->deep_value_uint(pixel, channel, sample);
}



void
OIIO_DeepData_set_deep_value(OIIO_DeepData* dd, int64_t pixel, int channel,
                             int sample, float value)
{
    to_cpp(dd)->set_deep_value(pixel, channel, sample, value);
}



void
OIIO_DeepData_set_deep_value_uint(OIIO_DeepData* dd, int64_t pixel, int channel,
                                  int sample, uint32_t value)
{
    to_cpp(dd)->set_deep_value(pixel, channel, sample, value);
}



void*
OIIO_DeepData_data_ptr(OIIO_DeepData* dd, int64_t pixel, int channel,
                       int sample)
{
    return to_cpp(dd)->data_ptr(pixel, channel, sample);
}



void
OIIO_DeepData_all_channeltypes(const OIIO_DeepData* dd,
                               const OIIO_TypeDesc** channeltypes,
                               int* nchanneltypes)
{
    auto span      = to_cpp(dd)->all_channeltypes();
    *nchanneltypes = span.size();
    *channeltypes  = (OIIO_TypeDesc*)span.data();
}



void
OIIO_DeepData_all_samples(const OIIO_DeepData* dd, const uint32_t** samples,
                          int* nsamples)
{
    auto span = to_cpp(dd)->all_samples();
    *nsamples = span.size();
    *samples  = span.data();
}



void
OIIO_DeepData_all_data(const OIIO_DeepData* dd, const char** bytes, int* nbytes)
{
    auto span = to_cpp(dd)->all_data();
    *nbytes   = span.size();
    *bytes    = span.data();
}



bool
OIIO_DeepData_copy_deep_sample(OIIO_DeepData* dd, int64_t pixel, int sample,
                               const OIIO_DeepData* src, int64_t srcpixel,
                               int srcsample)
{
    return to_cpp(dd)->copy_deep_sample(pixel, sample, *to_cpp(src), srcpixel,
                                        srcsample);
}



bool
OIIO_DeepData_copy_deep_pixel(OIIO_DeepData* dd, int64_t pixel,
                              const OIIO_DeepData* src, int64_t srcpixel)
{
    return to_cpp(dd)->copy_deep_pixel(pixel, *to_cpp(src), srcpixel);
}



bool
OIIO_DeepData_split(OIIO_DeepData* dd, int64_t pixel, float depth)
{
    return to_cpp(dd)->split(pixel, depth);
}



void
OIIO_DeepData_sort(OIIO_DeepData* dd, int64_t pixel)
{
    to_cpp(dd)->sort(pixel);
}



void
OIIO_DeepData_merge_overlaps(OIIO_DeepData* dd, int64_t pixel)
{
    to_cpp(dd)->merge_overlaps(pixel);
}



void
OIIO_DeepData_merge_deep_pixels(OIIO_DeepData* dd, int64_t pixel,
                                const OIIO_DeepData* src, int64_t srcpixel)
{
    return to_cpp(dd)->merge_deep_pixels(pixel, *to_cpp(src), srcpixel);
}



float
OIIO_DeepData_opaque_z(const OIIO_DeepData* dd, int64_t pixel)
{
    return to_cpp(dd)->opaque_z(pixel);
}



void
OIIO_DeepData_occlusion_cull(OIIO_DeepData* dd, int64_t pixel)
{
    to_cpp(dd)->occlusion_cull(pixel);
}
}
