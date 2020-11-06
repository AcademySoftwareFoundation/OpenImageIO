// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once

#include <OpenImageIO/export.h>

#include <OpenImageIO/c-typedesc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OIIO_DeepData OIIO_DeepData;
typedef struct OIIO_ImageSpec OIIO_ImageSpec;

/// Construct an empty DeepData.
///
/// Equivalent C++: `new OIIO::DeepData`
OIIOC_API OIIO_DeepData*
OIIO_DeepData_new();

/// Construct and init from an ImageSpec.
///
/// Equivalent C++: `new OIIO::DeepData(is)`
OIIOC_API OIIO_DeepData*
OIIO_DeepData_new_with_imagespec(const OIIO_ImageSpec* is);

/// Copy constructor
///
/// Equivalent C++: `new OIIO::DeepData(dd)`
OIIOC_API OIIO_DeepData*
OIIO_DeepData_copy(const OIIO_DeepData* dd);

/// Delete
///
/// Equivalent C++: `delete dd`
OIIOC_API void
OIIO_DeepData_delete(const OIIO_DeepData* dd);

/// Reset the `DeepData` to be equivalent to its empty initial state.
///
/// Equivalent C++: `dd->clear()`
OIIOC_API void
OIIO_DeepData_clear(OIIO_DeepData* dd);

/// Reset the `DeepData` to be equivalent to its empty initial state and also
/// ensure that all allocated memory has been truly freed
///
/// Equivalent C++: `dd->free()`
OIIOC_API void
OIIO_DeepData_free(OIIO_DeepData* dd);

/// Initialize the `DeepData` with the specified number of pixels,
/// channels, channel types, and channel names, and allocate memory for
/// all the data.
///
/// Equivalent C++: `dd->init(npix, nchan, {nchanneltypes, channeltypes}, {nchannelnames, channelnames})`
///
OIIOC_API void
OIIO_DeepData_init(OIIO_DeepData* dd, int64_t npix, int nchan,
                   const OIIO_TypeDesc* channeltypes, int nchanneltypes,
                   const char** channelnames, int nchannelnames);

/// Initialize the `DeepData` based on the `ImageSpec`'s total number of
/// pixels, number and types of channels. At this stage, all pixels are
/// assumed to have 0 samples, and no sample data is allocated.
///
/// Equivalent C++: `dd->init(*is)`
///
OIIOC_API void
OIIO_DeepData_init_with_imagespec(OIIO_DeepData* dd, const OIIO_ImageSpec* is);

/// Is the DeepData initialized?
///
/// Equivalent C++: `dd->initialized()`
///
OIIOC_API bool
OIIO_DeepData_initialized(const OIIO_DeepData* dd);

/// Is the DeepData allocated
///
/// Equivalent C++: `dd->allocated()`
///
OIIOC_API bool
OIIO_DeepData_allocated(const OIIO_DeepData* dd);

/// Retrieve the total number of pixels.
///
/// Equivalent C++: `dd->pixels()`
///
OIIOC_API int64_t
OIIO_DeepData_pixels(const OIIO_DeepData* dd);

/// Retrieve the total number of channels
///
/// Equivalent C++: `dd->channels()`
///
OIIOC_API int
OIIO_DeepData_channels(const OIIO_DeepData* dd);

/// Retrieve the index of the Z channel
///
/// Equivalent C++: `dd->Z_channel()`
///
OIIOC_API int
OIIO_DeepData_Z_channel(const OIIO_DeepData* dd);

// Retrieve the index of the Zback channel, which will return the
// Z channel if no Zback exists.
///
/// Equivalent C++: `dd->Zback_channel()`
///
OIIOC_API int
OIIO_DeepData_Zback_channel(const OIIO_DeepData* dd);

/// Retrieve the index of the alpha (A) channel
///
/// Equivalent C++: `dd->A_channel()`
///
OIIOC_API int
OIIO_DeepData_A_channel(const OIIO_DeepData* dd);

// Retrieve the index of the AR channel. If it does not exist, the A
// channel (which always exists) will be returned.
///
/// Equivalent C++: `dd->AR_channel()`
///
OIIOC_API int
OIIO_DeepData_AR_channel(const OIIO_DeepData* dd);

// Retrieve the index of the AG channel. If it does not exist, the A
// channel (which always exists) will be returned.
///
/// Equivalent C++: `dd->AG_channel()`
///
OIIOC_API int
OIIO_DeepData_AG_channel(const OIIO_DeepData* dd);

// Retrieve the index of the AB channel. If it does not exist, the A
// channel (which always exists) will be returned.
///
/// Equivalent C++: `dd->AB_channel()`
///
OIIOC_API int
OIIO_DeepData_AB_channel(const OIIO_DeepData* dd);

/// Return the name of channel c.
///
/// Equivalent C++: `dd->channelname(c)`
///
OIIOC_API const char*
OIIO_DeepData_channelname(const OIIO_DeepData* dd, int c);

/// Return the type of channel c.
///
/// Equivalent C++: `dd->channeltype(c)`
///
OIIOC_API OIIO_TypeDesc
OIIO_DeepData_channeltype(const OIIO_DeepData* dd, int c);

/// Return the size (in bytes) of one sample datum of channel `c`.
///
/// Equivalent C++: `dd->channelsize(c)`
///
OIIOC_API size_t
OIIO_DeepData_channelsize(const OIIO_DeepData* dd, int c);

/// Return the size (in bytes) for all channels of one sample.
///
/// Equivalent C++: `dd->samplesize()`
///
OIIOC_API size_t
OIIO_DeepData_samplesize(const OIIO_DeepData* dd);

/// Retrieve the number of samples for the given pixel index.
///
/// Equivalent C++: `dd->samples(pixel)`
///
OIIOC_API int
OIIO_DeepData_samples(const OIIO_DeepData* dd, int64_t pixel);

/// Set the number of samples for the given pixel. This must be called
/// after init().
///
/// Equivalent C++: `dd->set_samples(pixel, samps)`
///
OIIOC_API void
OIIO_DeepData_set_samples(OIIO_DeepData* dd, int64_t pixel, int samps);

/// Set the number of samples for all pixels. nsamples is
/// required to match pixels().
///
/// Equivalent C++: `dd->set_all_samples(pixel, samples, nsamples)`
///
OIIOC_API void
OIIO_DeepData_set_all_samples(OIIO_DeepData* dd, const uint32_t* samples,
                              int nsamples);

/// Set the capacity of samples for the given pixel. This must be called
/// after init().
///
/// Equivalent C++: `dd->set_capacity(pixel, samps)`
///
OIIOC_API void
OIIO_DeepData_set_capacity(OIIO_DeepData* dd, int64_t pixel, int samps);

/// Insert `n` samples of the specified pixel, betinning at the sample
/// position index. After insertion, the new samples will have
/// uninitialized values.
///
/// Equivalent C++: `dd->insert_samples(pixel, samplepos, n)`
///
OIIOC_API void
OIIO_DeepData_insert_samples(OIIO_DeepData* dd, int64_t pixel, int samplepos,
                             int n);

/// Erase `n` samples of the specified pixel, betinning at the sample
/// position index.
///
/// Equivalent C++: `dd->insert_samples(pixel, samplepos, n)`
///
OIIOC_API void
OIIO_DeepData_erase_samples(OIIO_DeepData* dd, int64_t pixel, int samplepos,
                            int n);

/// Retrieve the value of the given pixel, channel, and sample index,
/// cast to a `float`.
///
/// Equivalent C++: `dd->deep_value(pixel, channel, sample)`
///
OIIOC_API float
OIIO_DeepData_deep_value(const OIIO_DeepData* dd, int64_t pixel, int channel,
                         int sample);

/// Retrieve the value of the given pixel, channel, and sample index,
/// cast to a `uint32`.
///
/// Equivalent C++: `dd->deep_value_uint(pixel, channel, sample)`
///
OIIOC_API uint32_t
OIIO_DeepData_deep_value_uint(const OIIO_DeepData* dd, int64_t pixel,
                              int channel, int sample);

/// Set the value of the given pixel, channel, and sample index, for
/// floating-point channels.
///
/// Equivalent C++: `dd->set_deep_value(pixel, channel, sample, value)`
///
OIIOC_API void
OIIO_DeepData_set_deep_value(OIIO_DeepData* dd, int64_t pixel, int channel,
                             int sample, float value);

/// Set the value of the given pixel, channel, and sample index, for
/// integer channels.
///
/// Equivalent C++: `dd->set_deep_value(pixel, channel, sample, value)`
///
OIIOC_API void
OIIO_DeepData_set_deep_value_uint(OIIO_DeepData* dd, int64_t pixel, int channel,
                                  int sample, uint32_t value);

/// Retrieve the pointer to a given pixel/channel/sample, or NULL if
/// there are no samples for that pixel. Use with care, and note that
/// calls to insert_samples and erase_samples can invalidate pointers
/// returned by prior calls to data_ptr.
///
/// Equivalent C++: `dd->data_ptr(pixel, channel, sample)`
///
OIIOC_API void*
OIIO_DeepData_data_ptr(OIIO_DeepData* dd, int64_t pixel, int channel,
                       int sample);

///
/// Equivalent C++: `dd->all_channeltypes()`
///
OIIOC_API void
OIIO_DeepData_all_channeltypes(const OIIO_DeepData* dd,
                               const OIIO_TypeDesc** channeltypes,
                               int* nchanneltypes);

///
/// Equivalent C++: `dd->all_samples()`
///
OIIOC_API void
OIIO_DeepData_all_samples(const OIIO_DeepData* dd, const uint32_t** samples,
                          int* nsamples);

///
/// Equivalent C++: `dd->all_data()`
///
OIIOC_API void
OIIO_DeepData_all_data(const OIIO_DeepData* dd, const char** bytes,
                       int* nbytes);

/// TODO: (AL) get_pointers

/// Copy a deep sample from `src` to this `DeepData`. They must have the
/// same channel layout. Return `true` if ok, `false` if the operation
/// could not be performed.
///
/// Equivalent C++: `dd->copy_deep_sample(pixel, sample, *src, srcpixel, srcsample)`
///
OIIOC_API bool
OIIO_DeepData_copy_deep_sample(OIIO_DeepData* dd, int64_t pixel, int sample,
                               const OIIO_DeepData* src, int64_t srcpixel,
                               int srcsample);

/// Copy an entire deep pixel from `src` to this `DeepData`, completely
/// replacing any pixel data for that pixel. They must have the same
/// channel layout. Return `true` if ok, `false` if the operation could
/// not be performed.
///
/// Equivalent C++: `dd->copy_deep_pixel(pixel, *src, srcpixel)`
///
OIIOC_API bool
OIIO_DeepData_copy_deep_pixel(OIIO_DeepData* dd, int64_t pixel,
                              const OIIO_DeepData* src, int64_t srcpixel);

/// Split all samples of that pixel at the given depth zsplit.
///
/// Equivalent C++: `dd->split(pixel, depth)`
///
OIIOC_API bool
OIIO_DeepData_split(OIIO_DeepData* dd, int64_t pixel, float depth);

/// Sort the samples of the pixel by their `Z` depth.
///
/// Equivalent C++: `dd->sort(pixel)`
///
OIIOC_API void
OIIO_DeepData_sort(OIIO_DeepData* dd, int64_t pixel);

/// Merge any adjacent samples in the pixel that exactly overlap in z
/// range.
///
/// Equivalent C++: `dd->merge_overlaps(pixel)`
///
OIIOC_API void
OIIO_DeepData_merge_overlaps(OIIO_DeepData* dd, int64_t pixel);

/// Merge the samples of `src`'s pixel into this `DeepData`'s pixel.
///
/// Equivalent C++: `dd->merge_deep_pixels(pixel, *src, srcpixel)`
///
OIIOC_API void
OIIO_DeepData_merge_deep_pixels(OIIO_DeepData* dd, int64_t pixel,
                                const OIIO_DeepData* src, int64_t srcpixel);

/// Return the z depth at which the pixel reaches full opacity.
///
/// Equivalent C++: `dd->opaque_z(pixel)`
///
OIIOC_API float
OIIO_DeepData_opaque_z(const OIIO_DeepData* dd, int64_t pixel);

/// Remove any samples hidden behind opaque samples.
///
/// Equivalent C++: `dd->occlusion_cull(pixel)`
///
OIIOC_API void
OIIO_DeepData_occlusion_cull(OIIO_DeepData* dd, int64_t pixel);

#ifdef __cplusplus
}
#endif
