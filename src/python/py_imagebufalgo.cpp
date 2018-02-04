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


class IBA_dummy { };   // dummy class to establish a scope


bool
IBA_zero (ImageBuf &dst, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::zero (dst, roi, nthreads);
}



bool
IBA_fill (ImageBuf &dst, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::fill (dst, &values[0], roi, nthreads);
}


bool
IBA_fill2 (ImageBuf &dst, py::object top_tuple, py::object bottom_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::fill (dst, &top[0], &bottom[0], roi, nthreads);
}


bool
IBA_fill4 (ImageBuf &dst, py::object top_left_tuple, py::object top_right_tuple,
          py::object bottom_left_tuple, py::object bottom_right_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::fill (dst, &top_left[0], &top_right[0],
                               &bottom_left[0], &bottom_right[0],
                               roi, nthreads);
}



bool
IBA_checker (ImageBuf &dst, int width, int height, int depth,
             py::object color1_tuple, py::object color2_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::checker (dst, width, height, depth,
                                  &color1[0], &color2[0],
                                  xoffset, yoffset, zoffset, roi, nthreads);
}



bool
IBA_noise (ImageBuf &dst, std::string type, float A, float B, bool mono, int seed,
           ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::noise (dst, type, A, B, mono, seed, roi, nthreads);
}



bool
IBA_channels (ImageBuf &dst, const ImageBuf &src,
              py::tuple channelorder_, py::tuple newchannelnames_,
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
        auto orderi = channelorder_[i];
        if (py::isinstance<py::int_>(orderi)) {
            channelorder[i] = orderi.cast<py::int_>();
        }
        else if (py::isinstance<py::float_>(orderi)) {
            channelvalues[i] = orderi.cast<py::float_>();
        }
        else if (py::isinstance<py::str>(orderi)) {
            std::string chname = orderi.cast<py::str>();
            for (int c = 0;  c < src.nchannels(); ++c) {
                if (src.spec().channelnames[c] == chname)
                    channelorder[i] = c;
            }
        }
    }
    std::vector<std::string> newchannelnames;
    py_to_stdvector (newchannelnames, newchannelnames_);
    if (newchannelnames.size() != 0 && newchannelnames.size() != nchannels) {
        dst.error ("Inconsistent number of channel arguments");
        return false;
    }
    py::gil_scoped_release gil;
    return ImageBufAlgo::channels (dst, src, (int)nchannels, &channelorder[0],
                         channelvalues.size() ? &channelvalues[0] : nullptr,
                         newchannelnames.size() ? &newchannelnames[0] : nullptr,
                         shuffle_channel_names, nthreads);
}



bool
IBA_channel_append (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::channel_append (dst, A, B, roi, nthreads);
}


bool
IBA_deepen (ImageBuf &dst, const ImageBuf &src, float zvalue,
            ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::deepen (dst, src, zvalue, roi, nthreads);
}



bool
IBA_flatten (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::flatten (dst, src, roi, nthreads);
}



bool
IBA_deep_merge (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                 bool occlusion_cull, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::deep_merge (dst, A, B, occlusion_cull, roi, nthreads);
}



bool
IBA_deep_holdout (ImageBuf &dst, const ImageBuf &src, const ImageBuf &holdout,
                  ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::deep_holdout (dst, src, holdout, roi, nthreads);
}



bool
IBA_copy (ImageBuf &dst, const ImageBuf &src, TypeDesc convert,
          ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::copy (dst, src, convert, roi, nthreads);
}



bool
IBA_crop (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::crop (dst, src, roi, nthreads);
}



bool
IBA_cut (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::cut (dst, src, roi, nthreads);
}



bool
IBA_paste (ImageBuf &dst, int xbegin, int ybegin, int zbegin, int chbegin,
           const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::paste (dst, xbegin, ybegin, zbegin, chbegin,
                                src, roi, nthreads);
}



bool
IBA_rotate90 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::rotate90 (dst, src, roi, nthreads);
}



bool
IBA_rotate180 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::rotate180 (dst, src, roi, nthreads);
}



bool
IBA_rotate270 (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::rotate270 (dst, src, roi, nthreads);
}



bool
IBA_flip (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::flip (dst, src, roi, nthreads);
}



bool
IBA_flop (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::flop (dst, src, roi, nthreads);
}



bool
IBA_reorient (ImageBuf &dst, const ImageBuf &src, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::reorient (dst, src, nthreads);
}



bool
IBA_transpose (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::transpose (dst, src, roi, nthreads);
}



bool
IBA_circular_shift (ImageBuf &dst, const ImageBuf &src,
                    int xshift, int yshift, int zshift,
                    ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::circular_shift (dst, src, xshift, yshift, zshift,
                                         roi, nthreads);
}



bool
IBA_add_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::add (dst, A, &values[0], roi, nthreads);
}

bool
IBA_add_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::add (dst, A, val, roi, nthreads);
}

bool
IBA_add_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::add (dst, A, B, roi, nthreads);
}



bool
IBA_sub_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::sub (dst, A, &values[0], roi, nthreads);
}

bool
IBA_sub_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::sub (dst, A, val, roi, nthreads);
}

bool
IBA_sub_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::sub (dst, A, B, roi, nthreads);
}



bool
IBA_absdiff_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::absdiff (dst, A, &values[0], roi, nthreads);
}

bool
IBA_absdiff_float (ImageBuf &dst, const ImageBuf &A, float val,
                   ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::absdiff (dst, A, val, roi, nthreads);
}

bool
IBA_absdiff_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::absdiff (dst, A, B, roi, nthreads);
}



bool
IBA_abs (ImageBuf &dst, const ImageBuf &A,
         ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::abs (dst, A, roi, nthreads);
}



bool
IBA_mul_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::mul (dst, A, &values[0], roi, nthreads);
}

bool
IBA_mul_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
}

bool
IBA_mul_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
}



bool
IBA_div_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::div (dst, A, &values[0], roi, nthreads);
}

bool
IBA_div_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::div (dst, A, B, roi, nthreads);
}

bool
IBA_div_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::div (dst, A, B, roi, nthreads);
}



bool
IBA_mad_color (ImageBuf &dst, const ImageBuf &A,
               py::object Bvalues_tuple, py::object Cvalues_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::mad (dst, A, &Bvalues[0], &Cvalues[0], roi, nthreads);
}

bool
IBA_mad_float (ImageBuf &dst, const ImageBuf &A, float B, float C,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::mad (dst, A, B, C, roi, nthreads);
}

bool
IBA_mad_ici (ImageBuf &dst, const ImageBuf &A,
             py::object Bvalues_tuple, const ImageBuf &C,
             ROI roi=ROI::All(), int nthreads=0)
{
    std::vector<float> Bvalues, Cvalues;
    py_to_stdvector (Bvalues, Bvalues_tuple);
    if (roi.defined())
        Bvalues.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        Bvalues.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (Bvalues.size() > 0);
    py::gil_scoped_release gil;
    return ImageBufAlgo::mad (dst, A, &Bvalues[0], C, roi, nthreads);
}

bool
IBA_mad_cii (ImageBuf &dst, py::object Avalues_tuple,
             const ImageBuf &B, const ImageBuf &C,
             ROI roi=ROI::All(), int nthreads=0)
{
    return IBA_mad_ici (dst, B, Avalues_tuple, C, roi, nthreads);
}

bool
IBA_mad_ifi (ImageBuf &dst, const ImageBuf &A, float B, const ImageBuf &C,
             ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    std::vector<float> Bvalues (A.nchannels(), B);
    return ImageBufAlgo::mad (dst, A, &Bvalues[0], C, roi, nthreads);
}

bool
IBA_mad_fii (ImageBuf &dst, float A, const ImageBuf &B, const ImageBuf &C,
             ROI roi=ROI::All(), int nthreads=0)
{
    return IBA_mad_ifi (dst, B, A, C, roi, nthreads);
}

bool
IBA_mad_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                const ImageBuf &C, ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::mad (dst, A, B, C, roi, nthreads);
}



bool
IBA_invert (ImageBuf &dst, const ImageBuf &A,
             ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::invert (dst, A, roi, nthreads);
}



bool
IBA_pow_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::pow (dst, A, B, roi, nthreads);
}


bool
IBA_pow_color (ImageBuf &dst, const ImageBuf &A, py::object values_tuple,
               ROI roi=ROI::All(), int nthreads=0)
{
    if (py::isinstance<py::float_>(values_tuple))
        return IBA_pow_float (dst, A, values_tuple.cast<py::float_>(),
                              roi, nthreads);
    if (py::isinstance<py::int_>(values_tuple))
        return IBA_pow_float (dst, A, (float)int(values_tuple.cast<py::int_>()),
                              roi, nthreads);
    std::vector<float> values;
    py_to_stdvector (values, values_tuple);
    if (roi.defined())
        values.resize (roi.nchannels(), 0.0f);
    else if (A.initialized())
        values.resize (A.nchannels(), 0.0f);
    else return false;
    ASSERT (values.size() > 0);
    py::gil_scoped_release gil;
    return ImageBufAlgo::pow (dst, A, &values[0], roi, nthreads);
}



bool
IBA_clamp (ImageBuf &dst, const ImageBuf &src,
           py::object min_, py::object max_,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::clamp (dst, src, &min[0], &max[0],
                                clampalpha01, roi, nthreads);
}



bool
IBA_clamp_float (ImageBuf &dst, const ImageBuf &src,
                 float min_, float max_,
                 bool clampalpha01 = false,
                 ROI roi = ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    if (! src.initialized())
        return false;
    std::vector<float> min, max;
    min.resize (src.nchannels(), min_);
    max.resize (src.nchannels(), max_);
    return ImageBufAlgo::clamp (dst, src, &min[0], &max[0],
                                clampalpha01, roi, nthreads);
}



bool
IBA_channel_sum_weight (ImageBuf &dst, const ImageBuf &src,
                        py::object weight_tuple,
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::channel_sum (dst, src, &weight[0], roi, nthreads);
}

bool
IBA_channel_sum (ImageBuf &dst, const ImageBuf &src,
                 ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::channel_sum (dst, src, NULL, roi, nthreads);
}


bool
IBA_color_map_values (ImageBuf &dst, const ImageBuf &src, int srcchannel,
                      int nknots, int channels, py::object knots_tuple,
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
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::color_map (dst, src, srcchannel, mapname,
                                    roi, nthreads);
}



bool IBA_rangeexpand (ImageBuf &dst, const ImageBuf &src,
                      bool useluma = false,
                      ROI roi = ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::rangeexpand (dst, src, useluma, roi, nthreads);
}


bool IBA_rangecompress (ImageBuf &dst, const ImageBuf &src,
                        bool useluma = false,
                        ROI roi = ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::rangecompress (dst, src, useluma, roi, nthreads);
}



bool IBA_premult (ImageBuf &dst, const ImageBuf &src,
                  ROI roi = ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::premult (dst, src, roi, nthreads);
}


bool IBA_unpremult (ImageBuf &dst, const ImageBuf &src,
                    ROI roi = ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::unpremult (dst, src, roi, nthreads);
}



bool IBA_computePixelStats (const ImageBuf &src,
                            ImageBufAlgo::PixelStats &stats,
                            ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::computePixelStats (stats, src, roi, nthreads);
}



bool IBA_compare (const ImageBuf &A, const ImageBuf &B,
                  float failthresh, float warnthresh,
                  ImageBufAlgo::CompareResults &result,
                  ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::compare (A, B, failthresh, warnthresh,
                                  result, roi, nthreads);
}



bool IBA_compare_Yee (const ImageBuf &A, const ImageBuf &B,
                      ImageBufAlgo::CompareResults &result,
                      float luminance, float fov, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::compare_Yee (A, B, result, luminance, fov,
                                      roi, nthreads);
}



std::string
IBA_computePixelHashSHA1 (const ImageBuf &src, const std::string &extrainfo,
                          ROI roi = ROI::All(),
                          int blocksize = 0, int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::computePixelHashSHA1 (src, extrainfo, roi,
                                               blocksize, nthreads);
}



bool
IBA_warp (ImageBuf &dst, const ImageBuf &src, py::object values_M,
          const std::string &filtername = "", float filterwidth = 0.0f,
          bool recompute_roi = false,
          const std::string &wrapname="default",
          ROI roi=ROI::All(), int nthreads=0)
{
    ImageBuf::WrapMode wrap = ImageBuf::WrapMode_from_string (wrapname);
    std::vector<float> M;
    py_to_stdvector (M, values_M);
    if (M.size() != 9)
        return false;
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::rotate (dst, src, angle, center_x, center_y,
                                 filtername, filterwidth, recompute_roi,
                                 roi, nthreads);
}



bool
IBA_resize (ImageBuf &dst, const ImageBuf &src,
            const std::string &filtername = "", float filterwidth = 0.0f,
            ROI roi=ROI::All(), int nthreads=0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::resize (dst, src, filtername, filterwidth,
                                 roi, nthreads);
}



bool
IBA_resample (ImageBuf &dst, const ImageBuf &src, bool interpolate,
              ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::resample (dst, src, interpolate, roi, nthreads);
}



bool
IBA_make_kernel (ImageBuf &dst, const std::string &name,
                 float width, float height, float depth, bool normalize)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::make_kernel (dst, name, width, height, depth,
                                      normalize);
}



bool
IBA_convolve (ImageBuf &dst, const ImageBuf &src, const ImageBuf &kernel,
              bool normalize, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::convolve (dst, src, kernel, normalize, roi, nthreads);
}



bool
IBA_unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                  const std::string &kernel, float width,
                  float contrast, float threshold, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::unsharp_mask (dst, src, kernel, width,
                                       contrast, threshold, roi, nthreads);
}



bool
IBA_median_filter (ImageBuf &dst, const ImageBuf &src,
                   int width, int height, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::median_filter (dst, src, width, height, roi, nthreads);
}



bool
IBA_dilate (ImageBuf &dst, const ImageBuf &src,
            int width, int height, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::dilate (dst, src, width, height, roi, nthreads);
}



bool
IBA_erode (ImageBuf &dst, const ImageBuf &src,
           int width, int height, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::erode (dst, src, width, height, roi, nthreads);
}



bool
IBA_laplacian (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::laplacian (dst, src, roi, nthreads);
}



bool
IBA_fft (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::fft (dst, src, roi, nthreads);
}



bool
IBA_ifft (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::ifft (dst, src, roi, nthreads);
}



bool
IBA_polar_to_complex (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::polar_to_complex (dst, src, roi, nthreads);
}



bool
IBA_complex_to_polar (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::complex_to_polar (dst, src, roi, nthreads);
}



bool
IBA_fillholes_pushpull (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::fillholes_pushpull (dst, src, roi, nthreads);
}



bool
IBA_over (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
           ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::over (dst, A, B, roi, nthreads);
}



bool
IBA_zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
           bool z_zeroisinf = false,
           ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::zover (dst, A, B, z_zeroisinf, roi, nthreads);
}



bool
IBA_colorconvert (ImageBuf &dst, const ImageBuf &src,
                  const std::string &from, const std::string &to,
                  bool unpremult = false,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::ociolook (dst, src, looks, from, to,
                                   inverse, unpremult,
                                   context_key, context_value,
                                   &config, roi, nthreads);
}



bool
IBA_ociodisplay (ImageBuf &dst, const ImageBuf &src,
                 const std::string &display, const std::string &view,
                 const std::string &from, const std::string &looks,
                 bool unpremult,
                 const std::string &context_key, const std::string &context_value,
                 ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::ociodisplay (dst, src, display, view,
                                      from, looks, unpremult,
                                      context_key, context_value, NULL,
                                      roi, nthreads);
}



bool
IBA_ociodisplay_colorconfig (ImageBuf &dst, const ImageBuf &src,
                 const std::string &display, const std::string &view,
                 const std::string &from, const std::string &looks,
                 bool unpremult,
                 const std::string &context_key, const std::string &context_value,
                 const std::string &colorconfig = "",
                 ROI roi = ROI::All(), int nthreads = 0)
{
    ColorConfig config (colorconfig);
    py::gil_scoped_release gil;
    return ImageBufAlgo::ociodisplay (dst, src, display, view, from, looks,
                                      unpremult, context_key, context_value,
                                      &config, roi, nthreads);
}



bool
IBA_ociofiletransform (ImageBuf &dst, const ImageBuf &src,
                       const std::string &name,
                       bool inverse, bool unpremult,
                       ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::ociofiletransform (dst, src, name,
                                            inverse, unpremult, &config,
                                            roi, nthreads);
}



py::object
IBA_isConstantColor (const ImageBuf &src,
                     ROI roi = ROI::All(), int nthreads = 0)
{
    std::vector<float> constcolor (src.nchannels());
    bool r;
    {
        py::gil_scoped_release gil;
        r = ImageBufAlgo::isConstantColor (src, &constcolor[0], roi, nthreads);
    }
    if (r) {
        return C_to_tuple (&constcolor[0], (int)constcolor.size());
    } else {
        return py::none();
    }
}



bool IBA_isConstantChannel (const ImageBuf &src, int channel, float val,
                            ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::isConstantChannel (src, channel, val, roi, nthreads);
}



bool IBA_isMonochrome (const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::isMonochrome (src, roi, nthreads);
}



ROI IBA_nonzero_region(const ImageBuf &src, ROI roi, int nthreads)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::nonzero_region(src, roi, nthreads);
}



bool
IBA_fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                  ImageBufAlgo::NonFiniteFixMode mode=ImageBufAlgo::NONFINITE_BOX3,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::fixNonFinite (dst, src, mode, NULL, roi, nthreads);
}



bool
IBA_render_point (ImageBuf &dst, int x, int y, py::object color_)
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    py::gil_scoped_release gil;
    return ImageBufAlgo::render_point (dst, x, y, color);
}


bool
IBA_render_line (ImageBuf &dst, int x1, int y1, int x2, int y2,
                 py::object color_, bool skip_first_point=false)
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    py::gil_scoped_release gil;
    return ImageBufAlgo::render_line (dst, x1, y1, x2, y2,
                                      color, skip_first_point);
}


bool
IBA_render_box (ImageBuf &dst, int x1, int y1, int x2, int y2,
                 py::object color_, bool fill=false)
{
    std::vector<float> color;
    py_to_stdvector (color, color_);
    color.resize (dst.nchannels(), 1.0f);
    py::gil_scoped_release gil;
    return ImageBufAlgo::render_box (dst, x1, y1, x2, y2, color, fill);
}


bool
IBA_render_text (ImageBuf &dst, int x, int y,
                 const std::string &text,
                 int fontsize=16, const std::string &fontname="",
                 py::object textcolor_=py::none(),
                 const std::string ax = "left",
                 const std::string ay = "baseline",
                 int shadow = 0, ROI roi = ROI::All(), int nthreads = 0)
{
    std::vector<float> textcolor;
    py_to_stdvector (textcolor, textcolor_);
    textcolor.resize (dst.nchannels(), 1.0f);
    py::gil_scoped_release gil;
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
    py::gil_scoped_release gil;
    return ImageBufAlgo::text_size (text, fontsize, fontname);
}



bool
IBA_capture_image (ImageBuf &dst, int cameranum,
                   TypeDesc::BASETYPE convert = TypeDesc::UNKNOWN)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::capture_image (dst, cameranum, convert);
}



bool
IBA_make_texture_ib (ImageBufAlgo::MakeTextureMode mode,
                     const ImageBuf &buf,
                     const std::string &outputfilename,
                     const ImageSpec &config)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::make_texture (mode, buf, outputfilename, config);
}


bool
IBA_make_texture_filename (ImageBufAlgo::MakeTextureMode mode,
                           const std::string &filename,
                           const std::string &outputfilename,
                           const ImageSpec &config)
{
    py::gil_scoped_release gil;
    return ImageBufAlgo::make_texture (mode, filename, outputfilename,
                                       config);
}





void declare_imagebufalgo (py::module &m)
{
    using namespace pybind11::literals;
    using py::arg;

    py::enum_<ImageBufAlgo::NonFiniteFixMode>(m, "NonFiniteFixMode")
        .value("NONFINITE_NONE",  ImageBufAlgo::NONFINITE_NONE)
        .value("NONFINITE_BLACK", ImageBufAlgo::NONFINITE_BLACK)
        .value("NONFINITE_BOX3",  ImageBufAlgo::NONFINITE_BOX3)
        .export_values()
    ;

    py::enum_<ImageBufAlgo::MakeTextureMode>(m, "MakeTextureMode")
        .value("MakeTxTexture", ImageBufAlgo::MakeTxTexture)
        .value("MakeTxShadow",  ImageBufAlgo::MakeTxShadow)
        .value("MakeTxEnvLatl", ImageBufAlgo::MakeTxEnvLatl)
        .value("MakeTxEnvLatlFromLightProbe",
                                ImageBufAlgo::MakeTxEnvLatlFromLightProbe)
        .export_values()
    ;

    py::class_<ImageBufAlgo::PixelStats>(m, "PixelStats")
        .def(py::init<>())
        .def_readonly("min", &ImageBufAlgo::PixelStats::min)
        .def_readonly("max", &ImageBufAlgo::PixelStats::max)
        .def_readonly("avg", &ImageBufAlgo::PixelStats::avg)
        .def_readonly("stddev", &ImageBufAlgo::PixelStats::stddev)
        .def_readonly("nancount", &ImageBufAlgo::PixelStats::nancount)
        .def_readonly("infcount", &ImageBufAlgo::PixelStats::infcount)
        .def_readonly("finitecount", &ImageBufAlgo::PixelStats::finitecount)
        .def_readonly("sum", &ImageBufAlgo::PixelStats::sum)
        .def_readonly("sum2", &ImageBufAlgo::PixelStats::sum2)
    ;

    py::class_<ImageBufAlgo::CompareResults>(m, "CompareResults")
        .def(py::init<>())
        .def_readonly("meanerror", &ImageBufAlgo::CompareResults::meanerror)
        .def_readonly("rms_error", &ImageBufAlgo::CompareResults::rms_error)
        .def_readonly("PSNR", &ImageBufAlgo::CompareResults::PSNR)
        .def_readonly("maxerror", &ImageBufAlgo::CompareResults::maxerror)
        .def_readonly("maxx", &ImageBufAlgo::CompareResults::maxx)
        .def_readonly("maxy", &ImageBufAlgo::CompareResults::maxy)
        .def_readonly("maxz", &ImageBufAlgo::CompareResults::maxz)
        .def_readonly("maxc", &ImageBufAlgo::CompareResults::maxc)
        .def_readonly("nwarn", &ImageBufAlgo::CompareResults::nwarn)
        .def_readonly("nfail", &ImageBufAlgo::CompareResults::nfail)
    ;

    // Use a boost::python::scope to put this all inside "ImageBufAlgo"
    py::class_<IBA_dummy>(m, "ImageBufAlgo")
        .def_static("zero", &IBA_zero,
            "dst"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("fill", &IBA_fill,
            "dst"_a, "values"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("fill", &IBA_fill2,
            "dst"_a, "top"_a, "bottom"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("fill", &IBA_fill4,
            "dst"_a, "topleft"_a, "topright"_a, "bottomleft"_a,
            "bottomright"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("checker", &IBA_checker,
            "dst"_a, "width"_a, "height"_a, "depth"_a, "color1"_a, "color2"_a,
            "xoffset"_a=0, "yoffset"_a=0, "zoffset"_a=0,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("noise", &IBA_noise,
            "dst"_a, "type"_a="gaussian", "A"_a=0.0f, "B"_a=0.1f,
            "mono"_a=false, "seed"_a=0, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("channels", &IBA_channels,
            "dst"_a, "src"_a, "channelorder"_a,
            "newchannelnames"_a=py::tuple(),
            "shuffle_channel_names"_a=false, "nthreads"_a=0)

        .def_static("channel_append", IBA_channel_append,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("deepen", IBA_deepen,
            "dst"_a, "src"_a, "zvalue"_a=1.0f,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("flatten", IBA_flatten,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("deep_merge", IBA_deep_merge,
            "dst"_a, "A"_a, "B"_a, "occlusion_cull"_a=true,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("deep_holdout", IBA_deep_holdout,
            "dst"_a, "src"_a, "holdout"_a,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("copy", IBA_copy,
            "dst"_a, "src"_a, "convert"_a=TypeUnknown,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("crop", IBA_crop,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("cut", IBA_cut,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("paste", IBA_paste,
            "dst"_a, "xbegin"_a, "ybegin"_a, "zbegin"_a, "chbegin"_a, "src"_a,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rotate90", IBA_rotate90,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rotate180", IBA_rotate180,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rotate270", IBA_rotate270,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("flip", IBA_flip,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("flop", IBA_flop,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("reorient", IBA_reorient,
            "dst"_a, "src"_a, "nthreads"_a=0)

        .def_static("transpose", IBA_transpose,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("circular_shift", &IBA_circular_shift,
            "dst"_a, "src"_a,
            "xshift"_a, "yshift"_a, "zshift"_a=0,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("add", &IBA_add_images,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("add", &IBA_add_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("add", IBA_add_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("sub", &IBA_sub_images,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("sub", &IBA_sub_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("sub", IBA_sub_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("absdiff", &IBA_absdiff_images,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("absdiff", &IBA_absdiff_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("absdiff", IBA_absdiff_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("abs", &IBA_abs,
            "dst"_a, "A"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("mul", &IBA_mul_images,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mul", &IBA_mul_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mul", &IBA_mul_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("div", &IBA_div_images,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("div", &IBA_div_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("div", &IBA_div_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("mad", &IBA_mad_images,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_ifi,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_fii,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_ici,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_cii,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_float,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("mad", &IBA_mad_color,
            "dst"_a, "A"_a, "B"_a, "C"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("invert", &IBA_invert,
            "dst"_a, "A"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("pow", &IBA_pow_float,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("pow", &IBA_pow_color,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("channel_sum", &IBA_channel_sum,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("channel_sum", &IBA_channel_sum_weight,
            "dst"_a, "src"_a, "weight"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("color_map", &IBA_color_map_name,
            "dst"_a, "src"_a, "srcchannel"_a, "mapname"_a,
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("color_map", &IBA_color_map_values,
            "dst"_a, "src"_a, "srcchannel"_a,
            "nknots"_a, "channels"_a, "knots"_a,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rangecompress", &IBA_rangecompress,
            "dst"_a, "src"_a, "useluma"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rangeexpand", &IBA_rangeexpand,
            "dst"_a, "src"_a, "useluma"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("premult", &IBA_premult,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("unpremult", &IBA_unpremult,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("clamp", &IBA_clamp,
            "dst"_a, "src"_a,
            "min"_a=py::tuple(), "max"_a=py::tuple(),
            "clampalpha01"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("clamp", &IBA_clamp_float,
            "dst"_a, "src"_a,
            "min"_a=-std::numeric_limits<float>::max(),
            "max"_a=std::numeric_limits<float>::max(),
            "clampalpha01"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("colorconvert", &IBA_colorconvert,
            "dst"_a, "src"_a, "from"_a, "to"_a, "unpremult"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("colorconvert", &IBA_colorconvert_colorconfig,
            "dst"_a, "src"_a, "from"_a, "to"_a, "unpremult"_a=false,
            "context_key"_a="", "context_value"_a="", "colorconfig"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("ociolook", &IBA_ociolook,
            "dst"_a, "src"_a, "looks"_a, "from"_a, "to"_a,
            "unpremult"_a=false, "invert"_a=false,
            "context_key"_a="", "context_value"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("ociolook", &IBA_ociolook_colorconfig,
            "dst"_a, "src"_a, "looks"_a, "from"_a, "to"_a,
            "unpremult"_a=false, "invert"_a=false,
            "context_key"_a="", "context_value"_a="", "colorconfig"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("ociodisplay", &IBA_ociodisplay,
            "dst"_a, "src"_a, "display"_a, "view"_a,
            "from"_a="", "looks"_a="", "unpremult"_a=false,
            "context_key"_a="", "context_value"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("ociodisplay", &IBA_ociodisplay_colorconfig,
            "dst"_a, "src"_a, "display"_a, "view"_a,
            "from"_a="", "looks"_a="", "unpremult"_a=false,
            "context_key"_a="", "context_value"_a="", "colorconfig"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("ociofiletransform", &IBA_ociofiletransform,
            "dst"_a, "src"_a, "name"_a, "unpremult"_a=false, "invert"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("ociofiletransform", &IBA_ociofiletransform_colorconfig,
            "dst"_a, "src"_a, "name"_a,
            "unpremult"_a=false, "invert"_a=false, "colorconfig"_a="",
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("computePixelStats", &IBA_computePixelStats,
            "src"_a, "stats"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("compare", &IBA_compare,
            "A"_a, "B"_a, "failthresh"_a, "warnthresh"_a,
            "result"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("compare_Yee", &IBA_compare_Yee,
            "A"_a, "B"_a, "result"_a,
            "luminance"_a=100, "fov"_a=45,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("isConstantColor", &IBA_isConstantColor,
             "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("isConstantChannel", &IBA_isConstantChannel,
            "src"_a, "channel"_a, "val"_a,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("isMonochrome", &IBA_isMonochrome,
             "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        // color_count, color_range_check

        .def_static("nonzero_region", &IBA_nonzero_region,
             "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("computePixelHashSHA1", &IBA_computePixelHashSHA1,
            "src"_a, "extrainfo"_a="", "roi"_a=ROI::All(),
            "blocksize"_a=0, "nthreads"_a=0)

        .def_static("warp", &IBA_warp,
            "dst"_a, "src"_a, "M"_a, "filtername"_a="", "filterwidth"_a=0.0f,
            "recompute_roi"_a=false, "wrap"_a="default",
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("rotate", &IBA_rotate,
            "dst"_a, "src"_a, "angle"_a,
            "filtername"_a="", "filterwidth"_a=0.0f, "recompute_roi"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)
        .def_static("rotate", &IBA_rotate2,
            "dst"_a, "src"_a, "angle"_a, "center_x"_a, "center_y"_a,
            "filtername"_a="", "filterwidth"_a=0.0f, "recompute_roi"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("resize", &IBA_resize,
            "dst"_a, "src"_a, "filtername"_a="", "filterwidth"_a=0.0f,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("resample", &IBA_resample,
            "dst"_a, "src"_a, "interpolate"_a=true,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("make_kernel", &IBA_make_kernel,
            "dst"_a, "name"_a, "width"_a, "height"_a,
            "depth"_a=1.0f, "normalize"_a=true)

        .def_static("convolve", &IBA_convolve,
            "dst"_a, "src"_a, "kernel"_a, "normalze"_a=true,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("unsharp_mask", &IBA_unsharp_mask,
            "dst"_a, "src"_a, "kernel"_a="gaussian",
            "width"_a=3.0f, "contrast"_a=1.0f, "threshold"_a=0.0f,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("median_filter", &IBA_median_filter,
            "dst"_a, "src"_a, "width"_a=3, "height"_a=-1,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("dilate", &IBA_dilate,
            "dst"_a, "src"_a, "width"_a=3, "height"_a=-1,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("erode", &IBA_erode,
            "dst"_a, "src"_a, "width"_a=3, "height"_a=-1,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("laplacian", &IBA_laplacian,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("fft", &IBA_fft,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("ifft", &IBA_ifft,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("polar_to_complex", &IBA_polar_to_complex,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("complex_to_polar", &IBA_complex_to_polar,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("fixNonFinite", &IBA_fixNonFinite,
            "dst"_a, "src"_a, "mode"_a=ImageBufAlgo::NONFINITE_BOX3,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("fillholes_pushpull", &IBA_fillholes_pushpull,
            "dst"_a, "src"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("capture_image", &IBA_capture_image,
            "dst"_a, "cameranum"_a=0, "convert"_a=TypeDesc::UNKNOWN)

        .def_static("over", &IBA_over,
            "dst"_a, "A"_a, "B"_a, "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("zover", &IBA_zover,
            "dst"_a, "A"_a, "B"_a, "z_zeroisinf"_a=false,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("render_point", &IBA_render_point,
            "dst"_a, "x"_a, "y"_a, "color"_a=py::none())

        .def_static("render_line", &IBA_render_line,
            "dst"_a, "x1"_a, "y1"_a, "x2"_a, "y2"_a,
            "color"_a=py::none(), "skip_first_point"_a=false)

        .def_static("render_box", &IBA_render_box,
            "dst"_a, "x1"_a, "y1"_a, "x2"_a, "y2"_a,
            "color"_a=py::none(), "fill"_a=false)

        .def_static("render_text", &IBA_render_text,
            "dst"_a, "x"_a, "y"_a, "text"_a, "fontsize"_a=16,
            "fontname"_a="", "textcolor"_a=py::tuple(),"alignx"_a="left",
            "aligny"_a="baseline", "shadow"_a=0,
            "roi"_a=ROI::All(), "nthreads"_a=0)

        .def_static("text_size", &IBA_text_size,
            "text"_a, "fontsize"_a=16,"fontname"_a="")

        // histogram, histogram_draw,

        .def_static("make_texture", &IBA_make_texture_filename,
            "mode"_a, "filename"_a, "outputfilename"_a,
            "config"_a=ImageSpec())
        .def_static("make_texture", &IBA_make_texture_ib,
            "mode"_a, "buf"_a, "outputfilename"_a,
            "config"_a=ImageSpec())
        ;
}

} // namespace PyOpenImageIO

