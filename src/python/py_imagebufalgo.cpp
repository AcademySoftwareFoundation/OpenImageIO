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

#include "imagebufalgo.h"
#include "py_oiio.h"


namespace PyOpenImageIO
{
using namespace boost::python;


class IBA_dummy { };   // dummy class to establish a scope



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
    return ImageBufAlgo::fill (dst, &values[0], roi, nthreads);
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
    return ImageBufAlgo::checker (dst, width, height, depth,
                                  &color1[0], &color2[0],
                                  xoffset, yoffset, zoffset, roi, nthreads);
}



bool
IBA_channels (ImageBuf &dst, const ImageBuf &src,
              tuple channelorder_, tuple newchannelnames_,
              bool shuffle_channel_names)
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
    return ImageBufAlgo::channels (dst, src, (int)nchannels, &channelorder[0],
                         channelvalues.size() ? &channelvalues[0] : NULL,
                         newchannelnames.size() ? &newchannelnames[0] : NULL,
                         shuffle_channel_names);
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
    return ImageBufAlgo::add (dst, A, &values[0], roi, nthreads);
}

bool
IBA_add_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::add (dst, A, val, roi, nthreads);
}

bool
IBA_add_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
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
    return ImageBufAlgo::sub (dst, A, &values[0], roi, nthreads);
}

bool
IBA_sub_float (ImageBuf &dst, const ImageBuf &A, float val,
               ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::sub (dst, A, val, roi, nthreads);
}

bool
IBA_sub_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::sub (dst, A, B, roi, nthreads);
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
    return ImageBufAlgo::mul (dst, A, &values[0], roi, nthreads);
}

bool
IBA_mul_float (ImageBuf &dst, const ImageBuf &A, float B,
               ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
}

bool
IBA_mul_images (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::mul (dst, A, B, roi, nthreads);
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
    return ImageBufAlgo::clamp (dst, src, &min[0], &max[0],
                                clampalpha01, roi, nthreads);
}



bool
IBA_clamp_float (ImageBuf &dst, const ImageBuf &src,
                 float min_, float max_,
                 bool clampalpha01 = false,
                 ROI roi = ROI::All(), int nthreads=0)
{
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
    return ImageBufAlgo::channel_sum (dst, src, &weight[0], roi, nthreads);
}

bool
IBA_channel_sum (ImageBuf &dst, const ImageBuf &src,
                 ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::channel_sum (dst, src, NULL, roi, nthreads);
}


bool IBA_rangeexpand (ImageBuf &dst, const ImageBuf &src,
                      bool useluma = false,
                      ROI roi = ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::rangeexpand (dst, src, useluma, roi, nthreads);
}


bool IBA_rangecompress (ImageBuf &dst, const ImageBuf &src,
                        bool useluma = false,
                        ROI roi = ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::rangecompress (dst, src, useluma, roi, nthreads);
}



bool IBA_premult (ImageBuf &dst, const ImageBuf &src,
                  ROI roi = ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::premult (dst, src, roi, nthreads);
}


bool IBA_unpremult (ImageBuf &dst, const ImageBuf &src,
                    ROI roi = ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::unpremult (dst, src, roi, nthreads);
}



bool
IBA_resize (ImageBuf &dst, const ImageBuf &src,
            const std::string &filter = "", float filterwidth = 0.0f,
            ROI roi=ROI::All(), int nthreads=0)
{
    return ImageBufAlgo::resize (dst, src, filter, filterwidth,
                                 roi, nthreads);
}



bool
IBA_zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
           bool z_zeroisinf = false,
           ROI roi = ROI::All(), int nthreads = 0)
{
    return ImageBufAlgo::zover (dst, A, B, z_zeroisinf, roi, nthreads);
}



bool
IBA_colorconvert (ImageBuf &dst, const ImageBuf &src,
                  const std::string &from, const std::string &to,
                  bool unpremult = false,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    return ImageBufAlgo::colorconvert (dst, src, from.c_str(), to.c_str(),
                                       unpremult, roi, nthreads);
}



bool
IBA_ociolook (ImageBuf &dst, const ImageBuf &src, const std::string &looks,
              const std::string &from, const std::string &to,
              bool inverse, bool unpremult,
              const std::string &context_key, const std::string &context_value,
              ROI roi = ROI::All(), int nthreads = 0)
{
    return ImageBufAlgo::ociolook (dst, src, looks.c_str(),
                                   from.c_str(), to.c_str(),
                                   inverse, unpremult,
                                   context_key.c_str(), context_value.c_str(),
                                   roi, nthreads);
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

    return ImageBufAlgo::ociodisplay (dst, src, display.c_str(), view.c_str(),
                                      from == object() ? NULL : from_str.c_str(),
                                      looks == object() ? NULL : looks_str.c_str(),
                                      unpremult,
                                      context_key.c_str(), context_value.c_str(),
                                      roi, nthreads);
}



object
IBA_isConstantColor (const ImageBuf &src,
                     ROI roi = ROI::All(), int nthreads = 0)
{
    std::vector<float> constcolor (src.nchannels());
    bool r = ImageBufAlgo::isConstantColor (src, &constcolor[0],
                                            roi, nthreads);
    if (r) {
        return C_to_tuple (&constcolor[0], (int)constcolor.size(),
                           PyFloat_FromDouble);
    } else {
        return object();
    }
}



bool
IBA_fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                  ImageBufAlgo::NonFiniteFixMode mode=ImageBufAlgo::NONFINITE_BOX3,
                  ROI roi = ROI::All(), int nthreads = 0)
{
    return ImageBufAlgo::fixNonFinite (dst, src, mode, NULL, roi, nthreads);
}



bool
IBA_render_text (ImageBuf &dst, int x, int y,
                 const std::string &text,
                 int fontsize=16, const std::string &fontname="",
                 tuple textcolor_ = tuple())
{
    std::vector<float> textcolor;
    py_to_stdvector (textcolor, textcolor_);
    textcolor.resize (dst.nchannels(), 1.0f);
    return ImageBufAlgo::render_text (dst, x, y, text, fontsize, fontname,
                                      &textcolor[0]);
}



bool
IBA_capture_image (ImageBuf &dst, int cameranum,
                   TypeDesc::BASETYPE convert = TypeDesc::UNKNOWN)
{
    return ImageBufAlgo::capture_image (dst, cameranum, convert);
}



bool
IBA_make_texture_ib (ImageBufAlgo::MakeTextureMode mode,
                     const ImageBuf &buf,
                     const std::string &outputfilename,
                     const ImageSpec &config)
{
    return ImageBufAlgo::make_texture (mode, buf, outputfilename, config);
}


bool
IBA_make_texture_filename (ImageBufAlgo::MakeTextureMode mode,
                           const std::string &filename,
                           const std::string &outputfilename,
                           const ImageSpec &config)
{
    return ImageBufAlgo::make_texture (mode, filename, outputfilename,
                                       config);
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
        .def("zero", &ImageBufAlgo::zero, 
             (arg("dst"), arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("zero")

        .def("fill", &IBA_fill,
             (arg("dst"), arg("values"), 
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("fill")

        .def("checker", &IBA_checker,
             (arg("dst"), arg("width"), arg("height"), arg("depth"),
              arg("color1"), arg("color2"),
              arg("xoffset")=0, arg("yoffset")=0, arg("zoffset")=0,
              arg("roi")=ROI::All(), arg("nthreads")=0) )
        .staticmethod("checker")

        .def("channels", &IBA_channels,
             (arg("dst"), arg("src"), arg("channelorder"),
              arg("newchannelnames")=tuple(),
              arg("shuffle_channel_names")=false))
        .staticmethod("channels")

        .def("channel_append", &ImageBufAlgo::channel_append,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("channel_append")

        .def("flatten", &ImageBufAlgo::flatten,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flatten")

        .def("crop", &ImageBufAlgo::crop,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("crop")

        .def("paste", &ImageBufAlgo::paste,
             (arg("dst"), arg("xbegin"), arg("ybegin"), arg("zbegin"),
              arg("chbegin"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("paste")

        .def("flip", &ImageBufAlgo::flip,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flip")

        .def("flop", &ImageBufAlgo::flop,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flop")

        .def("flipflop", &ImageBufAlgo::flipflop,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("flipflop")

        .def("transpose", &ImageBufAlgo::transpose,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("transpose")

        .def("circular_shift", &ImageBufAlgo::circular_shift,
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

        .def("channel_sum", &IBA_channel_sum,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .def("channel_sum", &IBA_channel_sum_weight,
             (arg("dst"), arg("src"), arg("weight"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("channel_sum")

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
        .staticmethod("colorconvert")

        .def("ociolook", &IBA_ociolook,
             (arg("dst"), arg("src"),
              arg("looks"), arg("from"), arg("to"),
              arg("unpremult")=false, arg("invert")=false,
              arg("context_key")="", arg("context_value")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ociolook")

        .def("ociodisplay", &IBA_ociodisplay,
             (arg("dst"), arg("src"),
              arg("display"), arg("view"),
              arg("from")=object(), arg("looks")=object(),
              arg("unpremult")=false,
              arg("context_key")="", arg("context_value")="",
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ociodisplay")

        // computePixelStats, 

        .def("compare", &ImageBufAlgo::compare,
             (arg("A"), arg("B"), arg("failthresh"), arg("warnthresh"),
              arg("result"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("compare")

        .def("compare_Yee", &ImageBufAlgo::compare_Yee,
             (arg("A"), arg("B"), arg("result"),
              arg("luminance")=100, arg("fov")=45,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("compare_Yee")

        .def("isConstantColor", &IBA_isConstantColor,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isConstantColor")

        .def("isConstantChannel", &ImageBufAlgo::isConstantChannel,
             (arg("src"), arg("channel"), arg("val"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isConstantChannel")

        .def("isMonochrome", &ImageBufAlgo::isMonochrome,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("isMonochrome")

        // color_count, color_range_check

        .def("nonzero_region", &ImageBufAlgo::nonzero_region,
             (arg("src"), arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("nonzero_region")

        .def("computePixelHashSHA1", &ImageBufAlgo::computePixelHashSHA1,
             (arg("src"), arg("extrainfo")="", arg("roi")=ROI::All(),
              arg("blocksize")=0, arg("nthreads")=0))
        .staticmethod("computePixelHashSHA1")

        .def("resize", &IBA_resize,
             (arg("dst"), arg("src"), arg("filername")="",
              arg("filterwidth")=0.0f, arg("roi")=ROI::All(),
              arg("nthreads")=0))
        .staticmethod("resize")

        .def("resample", &ImageBufAlgo::resample,
             (arg("dst"), arg("src"), arg("interpolate")=true,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("resample")

        .def("make_kernel", &ImageBufAlgo::make_kernel,
             (arg("dst"), arg("name"), arg("width"), arg("height"),
              arg("depth")=1.0f, arg("normalize")=true))
        .staticmethod("make_kernel")

        .def("convolve", &ImageBufAlgo::convolve,
             (arg("dst"), arg("src"), arg("kernel"), arg("normalze")=true,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("convolve")

        .def("unsharp_mask", &ImageBufAlgo::unsharp_mask,
             (arg("dst"), arg("src"), arg("kernel")="gaussian",
              arg("width")=3.0f, arg("contrast")=1.0f,
              arg("threshold")=0.0f,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("unsharp_mask")

        .def("fft", &ImageBufAlgo::fft,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fft")

        .def("ifft", &ImageBufAlgo::ifft,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("ifft")

        .def("fixNonFinite", &IBA_fixNonFinite,
             (arg("dst"), arg("src"), 
              arg("mode")=ImageBufAlgo::NONFINITE_BOX3,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fixNonFinite")

        .def("fillholes_pushpull", &ImageBufAlgo::fillholes_pushpull,
             (arg("dst"), arg("src"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("fillholes_pushpull")

        .def("capture_image", &IBA_capture_image,
             (arg("dst"), arg("cameranum")=0,
              arg("convert")=TypeDesc::UNKNOWN))
        .staticmethod("capture_image")

        .def("over", &ImageBufAlgo::over,
             (arg("dst"), arg("A"), arg("B"),
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("over")

        .def("zover", &IBA_zover,
             (arg("dst"), arg("A"), arg("B"), arg("z_zeroisinf")=false,
              arg("roi")=ROI::All(), arg("nthreads")=0))
        .staticmethod("zover")

        .def("render_text", &IBA_render_text,
             (arg("dst"), arg("x"), arg("y"), arg("text"),
              arg("fontsize")=16, arg("fontname")="",
              arg("textcolor")=tuple()))
        .staticmethod("render_text")

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

