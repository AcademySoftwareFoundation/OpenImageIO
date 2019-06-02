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


#pragma once

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/string_view.h>

OIIO_NAMESPACE_BEGIN


struct TypeDesc;
class ImageSpec;



/// A `DeepData` holds the contents of an image of ``deep'' pixels (multiple
/// depth samples per pixel).

class OIIO_API DeepData {
public:
    /// Construct an empty DeepData.
    DeepData();

    /// Construct and init from an ImageSpec.
    DeepData(const ImageSpec& spec);

    /// Copy constructor
    DeepData(const DeepData& d);

    ~DeepData();

    /// Copy assignment
    const DeepData& operator=(const DeepData& d);

    /// Reset the `DeepData` to be equivalent to its empty initial state.
    void clear();
    /// In addition to performing the tasks of `clear()`, also ensure that
    /// all allocated memory has been truly freed.
    void free();

    /// Initialize the `DeepData` with the specified number of pixels,
    /// channels, channel types, and channel names, and allocate memory for
    /// all the data.
    void init(int npix, int nchan, cspan<TypeDesc> channeltypes,
              cspan<std::string> channelnames);

    /// Initialize the `DeepData` based on the `ImageSpec`'s total number of
    /// pixels, number and types of channels. At this stage, all pixels are
    /// assumed to have 0 samples, and no sample data is allocated.
    void init(const ImageSpec& spec);

    /// Is the DeepData initialized?
    bool initialized() const;

    /// Has the DeepData fully allocated? If no, it is still very
    /// inexpensive to call set_capacity().
    bool allocated() const;

    /// Retrieve the total number of pixels.
    int pixels() const;
    /// Retrieve the number of channels.
    int channels() const;

    // Retrieve the index of the Z channel.
    int Z_channel() const;
    // Retrieve the index of the Zback channel, which will return the
    // Z channel if no Zback exists.
    int Zback_channel() const;
    // Retrieve the index of the A (alpha) channel.
    int A_channel() const;
    // Retrieve the index of the AR channel. If it does not exist, the A
    // channel (which always exists) will be returned.
    int AR_channel() const;
    // Retrieve the index of the AG channel. If it does not exist, the A
    // channel (which always exists) will be returned.
    int AG_channel() const;
    // Retrieve the index of the AB channel. If it does not exist, the A
    // channel (which always exists) will be returned.
    int AB_channel() const;

    /// Return the name of channel c.
    string_view channelname(int c) const;

    /// Retrieve the data type of channel `c`.
    TypeDesc channeltype(int c) const;
    /// Return the size (in bytes) of one sample dadum of channel `c`.
    size_t channelsize(int c) const;
    /// Return the size (in bytes) for all channels of one sample.
    size_t samplesize() const;

    /// Retrieve the number of samples for the given pixel index.
    int samples(int pixel) const;

    /// Set the number of samples for the given pixel. This must be called
    /// after init().
    void set_samples(int pixel, int samps);

    /// Set the number of samples for all pixels. The samples.size() is
    /// required to match pixels().
    void set_all_samples(cspan<unsigned int> samples);

    /// Set the capacity of samples for the given pixel. This must be called
    /// after init().
    void set_capacity(int pixel, int samps);

    /// Retrieve the capacity (number of allocated samples) for the given
    /// pixel index.
    int capacity(int pixel) const;

    /// Insert `n` samples of the specified pixel, betinning at the sample
    /// position index. After insertion, the new samples will have
    /// uninitialized values.
    void insert_samples(int pixel, int samplepos, int n = 1);

    /// Erase `n` samples of the specified pixel, betinning at the sample
    /// position index.
    void erase_samples(int pixel, int samplepos, int n = 1);

    /// Retrieve the value of the given pixel, channel, and sample index,
    /// cast to a `float`.
    float deep_value(int pixel, int channel, int sample) const;
    /// Retrieve the value of the given pixel, channel, and sample index,
    /// cast to a `uint32`.
    uint32_t deep_value_uint(int pixel, int channel, int sample) const;

    /// Set the value of the given pixel, channel, and sample index, for
    /// floating-point channels.
    void set_deep_value(int pixel, int channel, int sample, float value);

    /// Set the value of the given pixel, channel, and sample index, for
    /// integer channels.
    void set_deep_value(int pixel, int channel, int sample, uint32_t value);

    /// Retrieve the pointer to a given pixel/channel/sample, or NULL if
    /// there are no samples for that pixel. Use with care, and note that
    /// calls to insert_samples and erase_samples can invalidate pointers
    /// returend by prior calls to data_ptr.
    void* data_ptr(int pixel, int channel, int sample);
    const void* data_ptr(int pixel, int channel, int sample) const;

    cspan<TypeDesc> all_channeltypes() const;
    cspan<unsigned int> all_samples() const;
    cspan<char> all_data() const;

    /// Fill in the vector with pointers to the start of the first
    /// channel for each pixel.
    void get_pointers(std::vector<void*>& pointers) const;

    /// Copy a deep sample from `src` to this `DeepData`. They must have the
    /// same channel layout. Return `true` if ok, `false` if the operation
    /// could not be performed.
    bool copy_deep_sample(int pixel, int sample, const DeepData& src,
                          int srcpixel, int srcsample);

    /// Copy an entire deep pixel from `src` to this `DeepData`, completely
    /// eplacing any pixel data for that pixel. They must have the same
    /// channel ayout. Return `true` if ok, `false` if the operation could
    /// not be erformed.
    bool copy_deep_pixel(int pixel, const DeepData& src, int srcpixel);

    /// Split all samples of that pixel at the given depth zsplit. Samples
    /// that span z (i.e. z < zsplit < zback) will be split into two samples
    /// with depth ranges [z,zsplit] and [zsplit,zback] with appropriate
    /// changes to their color and alpha values. Samples not spanning zsplit
    /// will remain intact. This operation will have no effect if there are
    /// not Z and Zback channels present. Return true if any splits
    /// occurred, false if the pixel was not modified.
    bool split(int pixel, float depth);

    /// Sort the samples of the pixel by their `Z` depth.
    void sort(int pixel);

    /// Merge any adjacent samples in the pixel that exactly overlap in z
    /// range. This is only useful if the pixel has previously been split at
    /// all sample starts and ends, and sorted by Z.  Note that this may
    /// change the number of samples in the pixel.
    void merge_overlaps(int pixel);

    /// Merge the samples of `src`'s pixel into this `DeepData`'s pixel.
    /// Return `true` if ok, `false` if the operation could not be
    /// performed.
    void merge_deep_pixels(int pixel, const DeepData& src, int srcpixel);

    /// Return the z depth at which the pixel reaches full opacity.
    float opaque_z(int pixel) const;

    /// Remvove any samples hidden behind opaque samples.
    void occlusion_cull(int pixel);

private:
    class Impl;
    Impl* m_impl;  // holds all the nontrivial stuff
    int m_npixels, m_nchannels;
};


OIIO_NAMESPACE_END
