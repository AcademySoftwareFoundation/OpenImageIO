/*
  Copyright 2013 Larry Gritz and the other authors and contributors.
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
#include <OpenImageIO/color.h>
#include <OpenImageIO/imagebufalgo.h>
#include "py_oiio.h"


namespace PyOpenImageIO
{
using namespace boost::python;


class IBA_dummy { };   // dummy class to establish a scope



bool
IBA_zero (ImageBuf &dst, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::zero (dst, roi, nthreads);
}



bool
IBA_fill (ImageBuf &dst, tuple values_tuple,
          ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (dst.initialized())
        values.resize (dst.nchannels(), 0.0f);
    else if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::fill (dst, &values[0], roi, nthreads);
}


bool
IBA_fill2 (ImageBuf &dst, tuple top_tuple, tuple bottom_tuple,
          ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> top, bottom;
    py_to_stdvector (top, top_tuple);
    py_to_stdvector (bottom, bottom_tuple);
    if (dst.initialized()) {
        top.resize (dst.nchannels(), 0.0f);
        bottom.resize (dst.nchannels(), 0.0f);
    } else if (roi.defined()) {
        top.resize (roi.nchannels(), 0.0f);
        bottom.resize (roi.nchannels(), 0.0f);
    }
    else return false;
    ASSERT (top.size() > 0 && bottom.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::fill (dst, &top[0], &bottom[0], roi, nthreads);
}


bool
IBA_fill4 (ImageBuf &dst, tuple top_left_tuple, tuple top_right_tuple,
          tuple bottom_left_tuple, tuple bottom_right_tuple,
          ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> top_left, top_right, bottom_left, bottom_right;
    py_to_stdvector (top_left, top_left_tuple);
    py_to_stdvector (top_right, top_right_tuple);
    py_to_stdvector (bottom_left, bottom_left_tuple);
    py_to_stdvector (bottom_right, bottom_right_tuple);
    if (dst.initialized()) {
        top_left.resize (dst.nchannels(), 0.0f);
        top_right.resize (dst.nchannels(), 0.0f);
        bottom_left.resize (dst.nchannels(), 0.0f);
        bottom_right.resize (dst.nchannels(), 0.0f);
    } else if (roi.defined()) {
        top_left.resize (roi.nchannels(), 0.0f);
        top_right.resize (roi.nchannels(), 0.0f);
        bottom_left.resize (roi.nchannels(), 0.0f);
        bottom_right.resize (roi.nchannels(), 0.0f);
    }
    else return false;
    ASSERT (top_left.size() > 0 && top_right.size() > 0 &&
            bottom_left.size() > 0 && bottom_right.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::fill (dst, &top_left[0], &top_right[0],
                               &bottom_left[0], &bottom_right[0],
                               roi, nthreads);
}



bool
IBA_checker (ImageBuf &dst, int width, int height, int depth,
             tuple color1_tuple, tuple color2_tuple,
             int xoffset, int yoffset, int zoffset,
             ROI roi, int nthreads)
{
    std::vector<float> color1, color2;
    py_to_stdvector (color1, color1_tuple);
    py_to_stdvector (color2, color2_tuple);
    if (dst.initialized())
        color1.resize (dst.nchannels(), 0.0f);
    else if (roi.defined())
        color1.resize (roi.nchannels(), 0.0f);
    else return false;
    if (dst.initialized())
        color2.resize (dst.nchannels(), 0.0f);
    else if (roi.defined())
        color2.resize (roi.nchannels(), 0.0f);
    else return false;
    ScopedGILRelease gil;
    return ImageBufAlgo::checker (dst, width, height, depth,
                                  &color1[0], &color2[0],
                                  xoffset, yoffset, zoffset, roi, nthreads);
}



bool
IBA_noise (ImageBuf &dst, std::string type, float A, float B, bool mono, int seed,
           ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::noise (dst, type, A, B, mono, seed, roi, nthreads);
}



bool
IBA_channels (ImageBuf &dst, const ImageBuf &src,
              tuple channelorder_, tuple newchannelnames_,
              bool shuffle_channel_names, int nthreads)
{
    size_t nchannels = (size_t) len(channelorder_);
    if (nchannels < 1) {
        dst.error ("No channels selected");
        return false;
    }
    std::vector<int> channelorder (nchannels, -1);
    std::vector<float> channelvalues (nchannels, 0.0f);
    for (size_t i = 0;  i < nchannels;  ++i) {
        extract<int> chnum (channelorder_[i]);
        if (chnum.check()) {
            channelorder[i] = chnum();
            continue;
        }
        extract<float> chval (channelorder_[i]);
        if (chval.check()) {
            channelvalues[i] = chval();
            continue;
        }
        extract<std::string> chname (channelorder_[i]);
        if (chname.check()) {
            for (int c = 0;  c < src.nchannels(); ++c) {
                if (src.spec().channelnames[c] == chname())
                    channelorder[i] = c;
            }
            continue;
        }
    }
    std::vector<std::string> newchannelnames;
    py_to_stdvector (newchannelnames, newchannelnames_);
    if (newchannelnames.size() != 0 && newchannelnames.size() != nchannels) {
        dst.error ("Inconsistent number of channel arguments");
        return false;
    }
    ScopedGILRelease gil;
    return ImageBufAlgo::channels (dst, src, (int)nchannels, &channelorder[0],
                         channelvalues.size() ? &channelvalues[0] : NULL,
                         newchannelnames.size() ? &newchannelnames[0] : NULL,
                         shuffle_channel_names, nthreads);
}



bool
IBA_channel_append (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::channel_append (dst, A, B, roi, nthreads);
}



bool
IBA_deepen (ImageBuf &dst, const ImageBuf &src, float zvalue,
            ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::deepen (dst, src, zvalue, roi, nthreads);
}



bool
IBA_flatten (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::flatten (dst, src, roi, nthreads);
}



bool
IBA_deep_merge (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                 bool occlusion_cull, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::deep_merge (dst, A, B, occlusion_cull, roi, nthreads);
}



bool
IBA_deep_holdout (ImageBuf &dst, const ImageBuf &src, const ImageBuf &holdout,
                  ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::deep_holdout (dst, src, holdout, roi, nthreads);
}



bool
IBA_copy (ImageBuf &dst, const ImageBuf &src, TypeDesc::BASETYPE convert,
          ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::copy (dst, src, convert, roi, nthreads);
}



bool
IBA_crop (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::crop (dst, src, roi, nthreads);
}



bool
IBA_cut (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::cut (dst, src, roi, nthreads);
}



bool
IBA_paste (ImageBuf &dst, int xbegin, int ybegin, int zbegin, int chbegin,
           const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::paste (dst, xbegin, ybegin, zbegin, chbegin,
                                src, roi, nthreads);
}



bool
IBA_rotate90 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rotate90 (dst, src, roi, nthreads);
}



bool
IBA_rotate180 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rotate180 (dst, src, roi, nthreads);
}



bool
IBA_rotate270 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rotate270 (dst, src, roi, nthreads);
}



bool
IBA_flip (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::flip (dst, src, roi, nthreads);
}



bool
IBA_flop (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::flop (dst, src, roi, nthreads);
}



bool
IBA_reorient (ImageBuf &dst, const ImageBuf &src, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::reorient (dst, src, nthreads);
}



bool
IBA_transpose (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::transpose (dst, src, roi, nthreads);
}



bool
IBA_circular_shift (ImageBuf &dst, const ImageBuf &src,
                    int xshift, int yshift, int zshift,
                    ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::circular_shift (dst, src, xshift, yshift, zshift,
                                         roi, nthreads);
}



bool
IBA_add_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::add (dst, A, &values[0], roi, nthreads);
}

bool
IBA_add_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::add (dst, A, val, roi, nthreads);
}

bool
IBA_add_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::add (dst, A, B, roi, nthreads);
}



bool
IBA_sub_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::sub (dst, A, &values[0], roi, nthreads);
}

bool
IBA_sub_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::sub (dst, A, val, roi, nthreads);
}

bool
IBA_sub_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::sub (dst, A, B, roi, nthreads);
}



bool
IBA_absdiff_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::absdiff (dst, A, &values[0], roi, nthreads);
}

bool
IBA_absdiff_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::absdiff (dst, A, val, roi, nthreads);
}

bool
IBA_absdiff_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::absdiff (dst, A, B, roi, nthreads);
}



bool
IBA_abs (ImageBuf &dst, const ImageBuf &A,
         ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::abs (dst, A, roi, nthreads);
}



bool
IBA_mul_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::mul (dst, A, &values[0], roi, nthreads);
}

bool
IBA_mul_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
}

bool
IBA_mul_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
}



bool
IBA_div_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::div (dst, A, &values[0], roi, nthreads);
}

bool
IBA_div_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::div (dst, A, B, roi, nthreads);
}

bool
IBA_div_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::div (dst, A, B, roi, nthreads);
}



bool
IBA_mad_color (ImageBuf &dst, const ImageBuf &A,
               tuple Bvalues_tuple, tuple Cvalues_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> Bvalues, Cvalues;
    py_to_stdvector (Bvalues, Bvalues_tuple);
    if (roi.defined())
        Bvalues.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        Bvalues.resize (A.nchannels(), 0.0f);
    else return false;
    py_to_stdvector (Cvalues, Cvalues_tuple);
    if (roi.defined())
        Cvalues.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        Cvalues.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (Bvalues.size() > 0 && Cvalues.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::mad (dst, A, &Bvalues[0], &Cvalues[0], roi, nthreads);
}

bool
IBA_mad_float (ImageBuf &dst, const ImageBuf &A, float B, float C,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::mad (dst, A, B, C, roi, nthreads);
}

bool
IBA_mad_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                const ImageBuf &C, ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::mad (dst, A, B, C, roi, nthreads);
}



bool
IBA_invert (ImageBuf &dst, const ImageBuf &A,
             ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::invert (dst, A, roi, nthreads);
}



bool
IBA_pow_color (ImageBuf &dst, const ImageBuf &A, tuple values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    ScopedGILRelease gil;
    return ImageBufAlgo::pow (dst, A, &values[0], roi, nthreads);
}

bool
IBA_pow_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::pow (dst, A, B, roi, nthreads);
}




bool
IBA_clamp (ImageBuf &dst, const ImageBuf &src,
           tuple min_, tuple max_,
           bool clampalpha01 = false,
           ROI roi = ROI::All(), int nthreads=0)
{
    if (! src.initialized())
        return false;
    std::vector<float> min, max;
    py_to_stdvector (min, min_);
    py_to_stdvector (max, max_);
    min.resize (src.nchannels(), -std::numeric_limits<float>::max());
    max.resize (src.nchannels(), std::numeric_limits<float>::max());
    ScopedGILRelease gil;
    return ImageBufAlgo::clamp (dst, src, &min[0], &max[0],
                                clampalpha01, roi, nthreads);
}



bool
IBA_clamp_float (ImageBuf &dst, const ImageBuf &src,
                 float min_, float max_,
                 bool clampalpha01 = false,
                 ROI roi = ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    if (! src.initialized())
        return false;
    std::vector<float> min, max;
    min.resize (src.nchannels(), min_);
    max.resize (src.nchannels(), max_);
    return ImageBufAlgo::clamp (dst, src, &min[0], &max[0],
                                clampalpha01, roi, nthreads);
}



bool
IBA_channel_sum_weight (ImageBuf &dst, const ImageBuf &src, tuple weight_tuple,
                        ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> weight;
    py_to_stdvector (weight, weight_tuple);
    if (! src.initialized()) {
        dst.error ("Uninitialized source image for channel_sum");
        return false;
    }
    if (weight.size() == 0)
        weight.resize (src.nchannels(), 1.0f);  // no weights -> uniform
    else
        weight.resize (src.nchannels(), 0.0f);  // missing weights -> 0
    ScopedGILRelease gil;
    return ImageBufAlgo::channel_sum (dst, src, &weight[0], roi, nthreads);
}

bool
IBA_channel_sum (ImageBuf &dst, const ImageBuf &src,
                 ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::channel_sum (dst, src, NULL, roi, nthreads);
}


bool
IBA_color_map_values (ImageBuf &dst, const ImageBuf &src, int srcchannel,
                      int nknots, int channels, tuple knots_tuple,
                      ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> knots;
    py_to_stdvector (knots, knots_tuple);
    if (! src.initialized()) {
        dst.error ("Uninitialized source image for color_map");
        return false;
    }
    if (! knots.size()) {
        dst.error ("No knot values supplied");
        return false;
    }
    ScopedGILRelease gil;
    return ImageBufAlgo::color_map (dst, src, srcchannel, nknots, channels,
                                    knots, roi, nthreads);
}


bool
IBA_color_map_name (ImageBuf &dst, const ImageBuf &src, int srcchannel,
                    const std::string& mapname,
                    ROI roi=ROI::All(), int nthreads=0)
{
    if (! src.initialized()) {
        dst.error ("Uninitialized source image for color_map");
        return false;
    }
    ScopedGILRelease gil;
    return ImageBufAlgo::color_map (dst, src, srcchannel, mapname,
                                    roi, nthreads);
}



bool IBA_rangeexpand (ImageBuf &dst, const ImageBuf &src,
                      bool useluma = false,
                      ROI roi = ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rangeexpand (dst, src, useluma, roi, nthreads);
}


bool IBA_rangecompress (ImageBuf &dst, const ImageBuf &src,
                        bool useluma = false,
                        ROI roi = ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rangecompress (dst, src, useluma, roi, nthreads);
}



bool IBA_premult (ImageBuf &dst, const ImageBuf &src,
                  ROI roi = ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::premult (dst, src, roi, nthreads);
}


bool IBA_unpremult (ImageBuf &dst, const ImageBuf &src,
                    ROI roi = ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::unpremult (dst, src, roi, nthreads);
}



bool IBA_computePixelStats (const ImageBuf &src, ImageBufAlgo::PixelStats &stats,
                            ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::computePixelStats (stats, src, roi, nthreads);
}



bool IBA_compare (const ImageBuf &A, const ImageBuf &B,
                  float failthresh, float warnthresh,
                  ImageBufAlgo::CompareResults &result,
                  ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::compare (A, B, failthresh, warnthresh,
                                  result, roi, nthreads);
}



bool IBA_compare_Yee (const ImageBuf &A, const ImageBuf &B,
                      ImageBufAlgo::CompareResults &result,
                      float luminance, float fov, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::compare_Yee (A, B, result, luminance, fov,
                                      roi, nthreads);
}



std::string
IBA_computePixelHashSHA1 (const ImageBuf &src,
                          const std::string &extrainfo = std::string(),
                          ROI roi = ROI::All(),
                          int blocksize = 0, int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::computePixelHashSHA1 (src, extrainfo, roi,
                                               blocksize, nthreads);
}



bool
IBA_warp (ImageBuf &dst, const ImageBuf &src, tuple values_M,
          const std::string &filtername = "", float filterwidth = 0.0f,
          bool recompute_roi = false,
          ImageBuf::WrapMode wrap = ImageBuf::WrapDefault,
          ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> M;
    py_to_stdvector (M, values_M);
    if (M.size() != 9)
        return false;
    ScopedGILRelease gil;
    return ImageBufAlgo::warp (dst, src, *(Imath::M33f *)&M[0],
                               filtername, filterwidth, recompute_roi, wrap,
                               roi, nthreads);
}



bool
IBA_rotate (ImageBuf &dst, const ImageBuf &src, float angle,
            const std::string &filtername = "", float filterwidth = 0.0f,
            bool recompute_roi = false,
            ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rotate (dst, src, angle, filtername, filterwidth,
                                 recompute_roi, roi, nthreads);
}



bool
IBA_rotate2 (ImageBuf &dst, const ImageBuf &src, float angle,
             float center_x, float center_y,
             const std::string &filtername = "", float filterwidth = 0.0f,
             bool recompute_roi = false,
             ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::rotate (dst, src, angle, center_x, center_y,
                                 filtername, filterwidth, recompute_roi,
                                 roi, nthreads);
}



bool
IBA_resize (ImageBuf &dst, const ImageBuf &src,
            const std::string &filtername = "", float filterwidth = 0.0f,
            ROI roi=ROI::All(), int nthreads=0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::resize (dst, src, filtername, filterwidth,
                                 roi, nthreads);
}



bool
IBA_resample (ImageBuf &dst, const ImageBuf &src, bool interpolate,
              ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::resample (dst, src, interpolate, roi, nthreads);
}



bool
IBA_make_kernel (ImageBuf &dst, const std::string &name,
                 float width, float height, float depth, bool normalize)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::make_kernel (dst, name, width, height, depth,
                                      normalize);
}



bool
IBA_convolve (ImageBuf &dst, const ImageBuf &src, const ImageBuf &kernel,
              bool normalize, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::convolve (dst, src, kernel, normalize, roi, nthreads);
}



bool
IBA_unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                  const std::string &kernel, float width,
                  float contrast, float threshold, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::unsharp_mask (dst, src, kernel, width,
                                       contrast, threshold, roi, nthreads);
}



bool
IBA_median_filter (ImageBuf &dst, const ImageBuf &src,
                   int width, int height, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::median_filter (dst, src, width, height, roi, nthreads);
}



bool
IBA_dilate (ImageBuf &dst, const ImageBuf &src,
            int width, int height, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::dilate (dst, src, width, height, roi, nthreads);
}



bool
IBA_erode (ImageBuf &dst, const ImageBuf &src,
           int width, int height, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::erode (dst, src, width, height, roi, nthreads);
}



bool
IBA_laplacian (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::laplacian (dst, src, roi, nthreads);
}



bool
IBA_fft (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::fft (dst, src, roi, nthreads);
}



bool
IBA_ifft (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::ifft (dst, src, roi, nthreads);
}



bool
IBA_polar_to_complex (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::polar_to_complex (dst, src, roi, nthreads);
}



bool
IBA_complex_to_polar (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::complex_to_polar (dst, src, roi, nthreads);
}



bool
IBA_fillholes_pushpull (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::fillholes_pushpull (dst, src, roi, nthreads);
}



bool
IBA_over (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
           ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::over (dst, A, B, roi, nthreads);
}



bool
IBA_zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
           bool z_zeroisinf = false,
           ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::zover (dst, A, B, z_zeroisinf, roi, nthreads);
}



bool
IBA_colorconvert (ImageBuf &dst, const ImageBuf &src,
                  const std::string &from, const std::string &to,
                  bool unpremult = false,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::colorconvert (dst, src, from, to, unpremult,
                                       "", "", nullptr, roi, nthreads);
}



bool
IBA_colorconvert_colorconfig (ImageBuf &dst, const ImageBuf &src,
                  const std::string &from, const std::string &to,
                  bool unpremult = false,
                  const std::string &context_key="",
                  const std::string &context_value="",
                  const std::string &colorconfig="",
                  ROI roi = ROI::All(), int nthreads = 0)
{
    ColorConfig config (colorconfig);
    ScopedGILRelease gil;
    return ImageBufAlgo::colorconvert (dst, src, from, to, unpremult,
                                       context_key, context_value,
                                       &config, roi, nthreads);
}



bool
IBA_ociolook (ImageBuf &dst, const ImageBuf &src, const std::string &looks,
              const std::string &from, const std::string &to,
              bool inverse, bool unpremult,
              const std::string &context_key, const std::string &context_value,
              ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::ociolook (dst, src, looks, from, to,
                                   inverse, unpremult,
                                   context_key, context_value, NULL,
                                   roi, nthreads);
}



bool
IBA_ociolook_colorconfig (ImageBuf &dst, const ImageBuf &src, const std::string &looks,
              const std::string &from, const std::string &to,
              bool inverse, bool unpremult,
              const std::string &context_key, const std::string &context_value,
              const std::string &colorconfig="",
              ROI roi = ROI::All(), int nthreads = 0)
{
    ColorConfig config (colorconfig);
    ScopedGILRelease gil;
    return ImageBufAlgo::ociolook (dst, src, looks, from, to,
                                   inverse, unpremult,
                                   context_key, context_value,
                                   &config, roi, nthreads);
}



bool
IBA_ociodisplay (ImageBuf &dst, const ImageBuf &src,
                 const std::string &display, const std::string &view,
                 const object &from, const object &looks,
                 bool unpremult,
                 const std::string &context_key, const std::string &context_value,
                 ROI roi = ROI::All(), int nthreads = 0)
{
    std::string from_str, looks_str;
    if (from != object())
        from_str = extract<std::string>(from);
    if (looks != object())
        looks_str = extract<std::string>(looks);
    ScopedGILRelease gil;
    return ImageBufAlgo::ociodisplay (dst, src, display.c_str(), view.c_str(),
                                      from == object() ? NULL : from_str.c_str(),
                                      looks == object() ? NULL : looks_str.c_str(),
                                      unpremult,
                                      context_key, context_value, NULL,
                                      roi, nthreads);
}



bool
IBA_ociodisplay_colorconfig (ImageBuf &dst, const ImageBuf &src,
                 const std::string &display, const std::string &view,
                 const object &from, const object &looks,
                 bool unpremult,
                 const std::string &context_key, const std::string &context_value,
                 const std::string &colorconfig = "",
                 ROI roi = ROI::All(), int nthreads = 0)
{
    ColorConfig config (colorconfig);
    std::string from_str, looks_str;
    if (from != object())
        from_str = extract<std::string>(from);
    if (looks != object())
        looks_str = extract<std::string>(looks);
    ScopedGILRelease gil;
    return ImageBufAlgo::ociodisplay (dst, src, display.c_str(), view.c_str(),
                                      from == object() ? NULL : from_str.c_str(),
                                      looks == object() ? NULL : looks_str.c_str(),
                                      unpremult, context_key, context_value,
                                      &config, roi, nthreads);
}



bool
IBA_ociofiletransform (ImageBuf &dst, const ImageBuf &src,
                       const std::string &name,
                       bool inverse, bool unpremult,
                       ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::ociofiletransform (dst, src, name,
                                            inverse, unpremult, NULL,
                                            roi, nthreads);
}



bool
IBA_ociofiletransform_colorconfig (ImageBuf &dst, const ImageBuf &src,
                                   const std::string &name,
                                   bool inverse, bool unpremult,
                                   const std::string &colorconfig="",
                                   ROI roi = ROI::All(), int nthreads = 0)
{
    ColorConfig config (colorconfig);
    ScopedGILRelease gil;
    return ImageBufAlgo::ociofiletransform (dst, src, name,
                                            inverse, unpremult, &config,
                                            roi, nthreads);
}



object
IBA_isConstantColor (const ImageBuf &src,
                     ROI roi = ROI::All(), int nthreads = 0)
{
    std::vector<float> constcolor (src.nchannels());
    bool r;
    {
        ScopedGILRelease gil;
        r = ImageBufAlgo::isConstantColor (src, &constcolor[0], roi, nthreads);
    }
    if (r) {
        return C_to_tuple (&constcolor[0], (int)constcolor.size(),
                           PyFloat_FromDouble);
    } else {
        return object();
    }
}



bool IBA_isConstantChannel (const ImageBuf &src, int channel, float val,
                            ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::isConstantChannel (src, channel, val, roi, nthreads);
}



bool IBA_isMonochrome (const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::isMonochrome (src, roi, nthreads);
}



ROI IBA_nonzero_region(const ImageBuf &src, ROI roi, int nthreads)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::nonzero_region(src, roi, nthreads);
}



bool
IBA_fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                  ImageBufAlgo::NonFiniteFixMode mode=ImageBufAlgo::NONFINITE_BOX3,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::fixNonFinite (dst, src, mode, NULL, roi, nthreads);
}



bool
IBA_render_point (ImageBuf &dst, int x, int y, tuple color_ = tuple())
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    ScopedGILRelease gil;
    return ImageBufAlgo::render_point (dst, x, y, color);
}


bool
IBA_render_line (ImageBuf &dst, int x1, int y1, int x2, int y2,
                 tuple color_ = tuple(), bool skip_first_point=false)
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    ScopedGILRelease gil;
    return ImageBufAlgo::render_line (dst, x1, y1, x2, y2,
                                      color, skip_first_point);
}


bool
IBA_render_box (ImageBuf &dst, int x1, int y1, int x2, int y2,
                 tuple color_ = tuple(), bool fill=false)
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    ScopedGILRelease gil;
    return ImageBufAlgo::render_box (dst, x1, y1, x2, y2, color, fill);
}


bool
IBA_render_text (ImageBuf &dst, int x, int y,
                 const std::string &text,
                 int fontsize=16, const std::string &fontname="",
                 tuple textcolor_ = tuple(),
                 const std::string ax = "left",
                 const std::string ay = "baseline",
                 int shadow = 0, ROI roi = ROI::All(), int nthreads = 0)
{
    std::vector<float> textcolor;
    py_to_stdvector (textcolor, textcolor_);
    textcolor.resize (dst.nchannels(), 1.0f);
    ScopedGILRelease gil;
    using ImageBufAlgo::TextAlignX;
    using ImageBufAlgo::TextAlignY;
    TextAlignX alignx (TextAlignX::Left);
    TextAlignY aligny (TextAlignY::Baseline);
    if (Strutil::iequals(ax, "right") || Strutil::iequals(ax, "r"))
        alignx = TextAlignX::Right;
    if (Strutil::iequals(ax, "center") || Strutil::iequals(ax, "c"))
        alignx = TextAlignX::Center;
    if (Strutil::iequals(ay, "top") || Strutil::iequals(ay, "t"))
        aligny = TextAlignY::Top;
    if (Strutil::iequals(ay, "bottom") || Strutil::iequals(ay, "b"))
        aligny = TextAlignY::Bottom;
    if (Strutil::iequals(ay, "center") || Strutil::iequals(ay, "c"))
        aligny = TextAlignY::Center;
    return ImageBufAlgo::render_text (dst, x, y, text, fontsize, fontname,
                                      textcolor, alignx, aligny, shadow,
                                      roi, nthreads);
}


ROI
IBA_text_size (const std::string &text,
               int fontsize=16, const std::string &fontname="")
{
    ScopedGILRelease gil;
    return ImageBufAlgo::text_size (text, fontsize, fontname);
}



bool
IBA_capture_image (ImageBuf &dst, int cameranum,
                   TypeDesc::BASETYPE convert = TypeDesc::UNKNOWN)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::capture_image (dst, cameranum, convert);
}



bool
IBA_make_texture_ib (ImageBufAlgo::MakeTextureMode mode,
                     const ImageBuf &buf,
                     const std::string &outputfilename,
                     const ImageSpec &config)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::make_texture (mode, buf, outputfilename, config);
}


bool
IBA_make_texture_filename (ImageBufAlgo::MakeTextureMode mode,
                           const std::string &filename,
                           const std::string &outputfilename,
                           const ImageSpec &config)
{
    ScopedGILRelease gil;
    return ImageBufAlgo::make_texture (mode, filename, outputfilename,
                                       config);
}



#if PY_MAJOR_VERSION >= 3
# define PYLONG(x) PyLong_FromLong((long)x)
#else
# define PYLONG(x) PyInt_FromLong((long)x)
#endif


static object
PixelStats_get_min(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.min[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_max(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.max[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_avg(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.avg[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_stddev(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.stddev[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_nancount(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PYLONG((long)stats.nancount[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_infcount(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PYLONG((long)stats.infcount[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_finitecount(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PYLONG((long)stats.finitecount[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_sum(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.sum[i]));
    return object(handle<>(result));
}

static object
PixelStats_get_sum2(const ImageBufAlgo::PixelStats& stats)
{
    size_t size = stats.min.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
        PyTuple_SetItem(result, i, PyFloat_FromDouble(stats.sum2[i]));
    return object(handle<>(result));
}






void declare_imagebufalgo()
{
    enum_<ImageBufAlgo::NonFiniteFixMode>("NonFiniteFixMode")
        .value("NONFINITE_NONE",  ImageBufAlgo::NONFINITE_NONE)
        .value("NONFINITE_BLACK", ImageBufAlgo::NONFINITE_BLACK)
        .value("NONFINITE_BOX3",  ImageBufAlgo::NONFINITE_BOX3)
        .export_values()
    ;

    enum_<ImageBufAlgo::MakeTextureMode>("MakeTextureMode")
        .value("MakeTxTexture", ImageBufAlgo::MakeTxTexture)
        .value("MakeTxShadow",  ImageBufAlgo::MakeTxShadow)
        .value("MakeTxEnvLatl", ImageBufAlgo::MakeTxEnvLatl)
        .value("MakeTxEnvLatlFromLightProbe",
                                ImageBufAlgo::MakeTxEnvLatlFromLightProbe)
        .export_values()
    ;

    class_<ImageBufAlgo::PixelStats>("PixelStats")
        .add_property("min", &PixelStats_get_min)
        .add_property("max", &PixelStats_get_max)
        .add_property("avg", &PixelStats_get_avg)
        .add_property("stddev", &PixelStats_get_stddev)
        .add_property("nancount", &PixelStats_get_nancount)
        .add_property("infcount", &PixelStats_get_infcount)
        .add_property("finitecount", &PixelStats_get_finitecount)
        .add_property("sum", &PixelStats_get_sum)
        .add_property("sum2", &PixelStats_get_sum2)
    ;

    class_<ImageBufAlgo::CompareResults>("CompareResults")
        .def_readwrite("meanerror", &ImageBufAlgo::CompareResults::meanerror)
        .def_readwrite("rms_error", &ImageBufAlgo::CompareResults::rms_error)
        .def_readwrite("PSNR", &ImageBufAlgo::CompareResults::PSNR)
        .def_readwrite("maxerror", &ImageBufAlgo::CompareResults::maxerror)
        .def_readwrite("maxx", &ImageBufAlgo::CompareResults::maxx)
        .def_readwrite("maxy", &ImageBufAlgo::CompareResults::maxy)
        .def_readwrite("maxz", &ImageBufAlgo::CompareResults::maxz)
        .def_readwrite("maxc", &ImageBufAlgo::CompareResults::maxc)
        .def_readwrite("nwarn", &ImageBufAlgo::CompareResults::nwarn)
        .def_readwrite("nfail", &ImageBufAlgo::CompareResults::nfail)
    ;

    // Use a boost::python::scope to put this all inside "ImageBufAlgo"
    boost::python::scope IBA = class_<IBA_dummy>("ImageBufAlgo")
        .def("zero", &IBA_zero, 
             (arg("dst"), arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("zero")

        .def("fill", &IBA_fill,
             (arg("dst"), arg("values"), 
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .def("fill", &IBA_fill2,
             (arg("dst"), arg("top"), arg("bottom"), 
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .def("fill", &IBA_fill4,
             (arg("dst"), arg("topleft"), arg("topright"), 
              arg("bottomleft"), arg("bottomright"),
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("fill")

        .def("checker", &IBA_checker,
             (arg("dst"), arg("width"), arg("height"), arg("depth"),
              arg("color1"), arg("color2"),
              arg("xoffset")=0, arg("yoffset")=0, arg("zoffset")=0,
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("checker")

        .def("noise", &IBA_noise,
             (arg("dst"), arg("type")="gaussian", arg("A")=0.0f, arg("B")=0.1f,
              arg("mono")=false, arg("seed")=0, arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("noise")

        .def("channels", &IBA_channels,
             (arg("dst"), arg("src"), arg("channelorder"),
              arg("newchannelnames")=tuple(),
              arg("shuffle_channel_names")=false, arg("nthreads")=0) )
        .staticmethod("channels")

        .def("channel_append", IBA_channel_append,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("channel_append")

        .def("deepen", IBA_deepen,
             (arg("dst"), arg("src"), arg("zvalue")=1.0f,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("deepen")

        .def("flatten", IBA_flatten,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flatten")

        .def("deep_merge", IBA_deep_merge,
             (arg("dst"), arg("A"), arg("B"), arg("occlusion_cull")=true,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("deep_merge")

        .def("deep_holdout", IBA_deep_holdout,
             (arg("dst"), arg("src"), arg("holdout"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("deep_holdout")

        .def("copy", IBA_copy,
             (arg("dst"), arg("src"), arg("convert")=TypeDesc::UNKNOWN,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("copy")

        .def("crop", IBA_crop,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("crop")

        .def("cut", IBA_cut,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("cut")

        .def("paste", IBA_paste,
             (arg("dst"), arg("xbegin"), arg("ybegin"), arg("zbegin"),
              arg("chbegin"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("paste")

        .def("rotate90", IBA_rotate90,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rotate90")

        .def("rotate180", IBA_rotate180,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rotate180")

        .def("rotate270", IBA_rotate270,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rotate270")

        .def("flip", IBA_flip,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flip")

        .def("flop", IBA_flop,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flop")

        .def("reorient", IBA_reorient,
             (arg("dst"), arg("src"), arg("nthreads")=0))
        .staticmethod("reorient")

        .def("transpose", IBA_transpose,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("transpose")

        .def("circular_shift", &IBA_circular_shift,
             (arg("dst"), arg("src"),
              arg("xshift"), arg("yshift"), arg("zshift")=0,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("circular_shift")

        .def("add", &IBA_add_images,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("add", &IBA_add_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("add", IBA_add_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("add")

        .def("sub", &IBA_sub_images,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("sub", &IBA_sub_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("sub", IBA_sub_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("sub")

        .def("absdiff", &IBA_absdiff_images,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("absdiff", &IBA_absdiff_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("absdiff", IBA_absdiff_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("absdiff")

        .def("abs", &IBA_abs,
             (arg("dst"), arg("A"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("abs")

        .def("mul", &IBA_mul_images,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("mul", &IBA_mul_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("mul", &IBA_mul_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("mul")

        .def("div", &IBA_div_images,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("div", &IBA_div_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("div", &IBA_div_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("div")

        .def("mad", &IBA_mad_images,
             (arg("dst"), arg("A"), arg("B"), arg("C"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("mad", &IBA_mad_float,
             (arg("dst"), arg("A"), arg("B"), arg("C"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("mad", &IBA_mad_color,
             (arg("dst"), arg("A"), arg("B"), arg("C"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("mad")

        .def("invert", &IBA_invert,
             (arg("dst"), arg("A"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("invert")

        .def("pow", &IBA_pow_float,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("pow", &IBA_pow_color,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("pow")

        .def("channel_sum", &IBA_channel_sum,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("channel_sum", &IBA_channel_sum_weight,
             (arg("dst"), arg("src"), arg("weight"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("channel_sum")

        .def("color_map", &IBA_color_map_name,
             (arg("dst"), arg("src"), arg("srcchannel"), arg("mapname"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("color_map", &IBA_color_map_values,
             (arg("dst"), arg("src"), arg("srcchannel"),
              arg("nknots"), arg("channels"), arg("knots"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("color_map")

        .def("rangecompress", &IBA_rangecompress,
             (arg("dst"), arg("src"), arg("useluma")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rangecompress")

        .def("rangeexpand", &IBA_rangeexpand,
             (arg("dst"), arg("src"), arg("useluma")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rangeexpand")

        .def("premult", &IBA_premult,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("premult")

        .def("unpremult", &IBA_unpremult,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("unpremult")

        .def("clamp", &IBA_clamp,
             (arg("dst"), arg("src"),
              arg("min")=tuple(), arg("max")=tuple(),
              arg("clampalpha01")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("clamp", &IBA_clamp_float,
             (arg("dst"), arg("src"),
              arg("min")=-std::numeric_limits<float>::max(),
              arg("max")=std::numeric_limits<float>::max(),
              arg("clampalpha01")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("clamp")

        .def("colorconvert", &IBA_colorconvert,
             (arg("dst"), arg("src"),
              arg("from"), arg("to"), arg("unpremult")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("colorconvert", &IBA_colorconvert_colorconfig,
             (arg("dst"), arg("src"),
              arg("from"), arg("to"),
              arg("unpremult")=false,
              arg("context_key")="", arg("context_value")="",
              arg("colorconfig")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("colorconvert")

        .def("ociolook", &IBA_ociolook,
             (arg("dst"), arg("src"),
              arg("looks"), arg("from"), arg("to"),
              arg("unpremult")=false, arg("invert")=false,
              arg("context_key")="", arg("context_value")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("ociolook", &IBA_ociolook_colorconfig,
             (arg("dst"), arg("src"),
              arg("looks"), arg("from"), arg("to"),
              arg("unpremult")=false, arg("invert")=false,
              arg("context_key")="", arg("context_value")="",
              arg("colorconfig")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ociolook")

        .def("ociodisplay", &IBA_ociodisplay,
             (arg("dst"), arg("src"),
              arg("display"), arg("view"),
              arg("from")=object(), arg("looks")=object(),
              arg("unpremult")=false,
              arg("context_key")="", arg("context_value")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("ociodisplay", &IBA_ociodisplay_colorconfig,
             (arg("dst"), arg("src"),
              arg("display"), arg("view"),
              arg("from")=object(), arg("looks")=object(),
              arg("unpremult")=false,
              arg("context_key")="", arg("context_value")="",
              arg("colorconfig")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ociodisplay")

        .def("ociofiletransform", &IBA_ociofiletransform,
             (arg("dst"), arg("src"), arg("name"),
              arg("unpremult")=false, arg("invert")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("ociofiletransform", &IBA_ociofiletransform_colorconfig,
             (arg("dst"), arg("src"), arg("name"),
              arg("unpremult")=false, arg("invert")=false,
              arg("colorconfig")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ociofiletransform")

        .def("computePixelStats", &IBA_computePixelStats,
             (arg("src"), arg("stats"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("computePixelStats")

        .def("compare", &IBA_compare,
             (arg("A"), arg("B"), arg("failthresh"), arg("warnthresh"),
              arg("result"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("compare")

        .def("compare_Yee", &IBA_compare_Yee,
             (arg("A"), arg("B"), arg("result"),
              arg("luminance")=100, arg("fov")=45,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("compare_Yee")

        .def("isConstantColor", &IBA_isConstantColor,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isConstantColor")

        .def("isConstantChannel", &IBA_isConstantChannel,
             (arg("src"), arg("channel"), arg("val"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isConstantChannel")

        .def("isMonochrome", &IBA_isMonochrome,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isMonochrome")

        // color_count, color_range_check

        .def("nonzero_region", &IBA_nonzero_region,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("nonzero_region")

        .def("computePixelHashSHA1", &IBA_computePixelHashSHA1,
             (arg("src"), arg("extrainfo")="", arg("roi")=ROI::All(),
              arg("blocksize")=0, arg("nthreads")=0))
        .staticmethod("computePixelHashSHA1")

        .def("warp", &IBA_warp,
             (arg("dst"), arg("src"), arg("M"),
              arg("filtername")="", arg("filterwidth")=0.0f,
              arg("recompute_roi")=false, arg("wrap")=ImageBuf::WrapDefault,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("warp")

        .def("rotate", &IBA_rotate,
             (arg("dst"), arg("src"), arg("angle"),
              arg("filtername")="", arg("filterwidth")=0.0f,
              arg("recompute_roi")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("rotate", &IBA_rotate2,
             (arg("dst"), arg("src"), arg("angle"),
              arg("center_x"), arg("center_y"),
              arg("filtername")="", arg("filterwidth")=0.0f,
              arg("recompute_roi")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("rotate")

        .def("resize", &IBA_resize,
             (arg("dst"), arg("src"), arg("filtername")="",
              arg("filterwidth")=0.0f, arg("roi")=ROI::All(),
              arg("nthreads")=0))
        .staticmethod("resize")

        .def("resample", &IBA_resample,
             (arg("dst"), arg("src"), arg("interpolate")=true,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("resample")

        .def("make_kernel", &IBA_make_kernel,
             (arg("dst"), arg("name"), arg("width"), arg("height"),
              arg("depth")=1.0f, arg("normalize")=true))
        .staticmethod("make_kernel")

        .def("convolve", &IBA_convolve,
             (arg("dst"), arg("src"), arg("kernel"), arg("normalze")=true,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("convolve")

        .def("unsharp_mask", &IBA_unsharp_mask,
             (arg("dst"), arg("src"), arg("kernel")="gaussian",
              arg("width")=3.0f, arg("contrast")=1.0f,
              arg("threshold")=0.0f,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("unsharp_mask")

        .def("median_filter", &IBA_median_filter,
             (arg("dst"), arg("src"),
              arg("width")=3, arg("height")=-1,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("median_filter")

        .def("dilate", &IBA_dilate,
             (arg("dst"), arg("src"),
              arg("width")=3, arg("height")=-1,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("dilate")

        .def("erode", &IBA_erode,
             (arg("dst"), arg("src"),
              arg("width")=3, arg("height")=-1,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("erode")

        .def("laplacian", &IBA_laplacian,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("laplacian")

        .def("fft", &IBA_fft,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fft")

        .def("ifft", &IBA_ifft,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ifft")

        .def("polar_to_complex", &IBA_polar_to_complex,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("polar_to_complex")

        .def("complex_to_polar", &IBA_complex_to_polar,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("complex_to_polar")

        .def("fixNonFinite", &IBA_fixNonFinite,
             (arg("dst"), arg("src"), 
              arg("mode")=ImageBufAlgo::NONFINITE_BOX3,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fixNonFinite")

        .def("fillholes_pushpull", &IBA_fillholes_pushpull,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fillholes_pushpull")

        .def("capture_image", &IBA_capture_image,
             (arg("dst"), arg("cameranum")=0,
              arg("convert")=TypeDesc::UNKNOWN))
        .staticmethod("capture_image")

        .def("over", &IBA_over,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("over")

        .def("zover", &IBA_zover,
             (arg("dst"), arg("A"), arg("B"), arg("z_zeroisinf")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("zover")

        .def("render_point", &IBA_render_point,
             (arg("dst"), arg("x"), arg("y"), arg("color")=tuple()))
        .staticmethod("render_point")

        .def("render_line", &IBA_render_line,
             (arg("dst"), arg("x1"), arg("y1"), arg("x2"), arg("y2"),
              arg("color")=tuple(), arg("skip_first_point")=false))
        .staticmethod("render_line")

        .def("render_box", &IBA_render_box,
             (arg("dst"), arg("x1"), arg("y1"), arg("x2"), arg("y2"),
              arg("color")=tuple(), arg("fill")=false))
        .staticmethod("render_box")

        .def("render_text", &IBA_render_text,
             (arg("dst"), arg("x"), arg("y"), arg("text"),
              arg("fontsize")=16, arg("fontname")="",
              arg("textcolor")=tuple(), arg("alignx")="left",
              arg("aligny")="baseline", arg("shadow")=0,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("render_text")

        .def("text_size", &IBA_text_size,
             (arg("text"), arg("fontsize")=16, arg("fontname")=""))
        .staticmethod("text_size")

        // histogram, histogram_draw,

        .def("make_texture", &IBA_make_texture_filename,
             (arg("mode"), arg("filename"), arg("outputfilename"),
              arg("config")=ImageSpec()))
        .def("make_texture", &IBA_make_texture_ib,
             (arg("mode"), arg("buf"), arg("outputfilename"),
              arg("config")=ImageSpec()))
        .staticmethod("make_texture")

        ;
}

} // namespace PyOpenImageIO

