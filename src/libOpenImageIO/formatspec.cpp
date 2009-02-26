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

#include <half.h>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "dassert.h"
#include "typedesc.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;



// Generate the default quantization parameters, templated on the data
// type.
template <class T>
static void
set_default_quantize (int &quant_black, int &quant_white,
                      int &quant_min, int &quant_max, float &quant_dither)
{
    if (std::numeric_limits <T>::is_integer) {
        quant_black  = 0;
        quant_white  = (int) std::numeric_limits <T>::max();
        quant_min    = (int) std::numeric_limits <T>::min();
        quant_max    = (int) std::numeric_limits <T>::max();
        quant_dither = 0.5f;
    } else {
        quant_black  = 0;
        quant_white  = 0;
        quant_min    = 0;
        quant_max    = 0;
        quant_dither = 0.0f;
    }
}



// Given the format, set the default quantization parameters.
// Rely on the template version to make life easy.
static void
set_default_quantize (TypeDesc format,
                      int &quant_black, int &quant_white,
                      int &quant_min, int &quant_max, float &quant_dither)
{
    switch (format.basetype) {
    case TypeDesc::INT8:
        set_default_quantize <char> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::UNKNOWN:
    case TypeDesc::UINT8:
        set_default_quantize <unsigned char> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::INT16:
        set_default_quantize <short> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::UINT16:
        set_default_quantize <unsigned short> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::INT:
        set_default_quantize <int> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::UINT:
        set_default_quantize <unsigned int> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::HALF:
        set_default_quantize <half> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::FLOAT:
        set_default_quantize <float> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::DOUBLE:
        set_default_quantize <double> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    default: ASSERT(0);
    }
}



QuantizationSpec
    QuantizationSpec::quantize_default (std::numeric_limits<stride_t>::min(),
                                        std::numeric_limits<stride_t>::min(),
                                        std::numeric_limits<stride_t>::min(),
                                        std::numeric_limits<stride_t>::min(),
                                        0);



QuantizationSpec::QuantizationSpec (TypeDesc _type)
{
    set_default_quantize (_type, quant_black, quant_white,
                          quant_min, quant_max, quant_dither);
}



ImageSpec::ImageSpec (TypeDesc format)
    : x(0), y(0), z(0), width(0), height(0), depth(1),
      full_x(0), full_y(0), full_z(0),
      full_width(0), full_height(0), full_depth(0),
      tile_width(0), tile_height(0), tile_depth(1),
      format(format), nchannels(0), alpha_channel(-1), z_channel(-1),
      linearity(UnknownLinearity), gamma(1)
{
    set_format (format);
}



ImageSpec::ImageSpec (int xres, int yres, int nchans, TypeDesc format)
    : x(0), y(0), z(0), width(xres), height(yres), depth(1),
      full_x(0), full_y(0), full_z(0),
      full_width(xres), full_height(yres), full_depth(1),
      tile_width(0), tile_height(0), tile_depth(1),
      format(format), nchannels(nchans), alpha_channel(-1), z_channel(-1),
      linearity(UnknownLinearity), gamma(1)
{
    set_format (format);
    default_channel_names ();
}



void
ImageSpec::set_format (TypeDesc fmt)
{
    format = fmt;
    set_default_quantize (fmt, quant_black, quant_white,
                          quant_min, quant_max, quant_dither);
}



TypeDesc
ImageSpec::format_from_quantize (int quant_black, int quant_white,
                                 int quant_min, int quant_max)
{
    if (quant_black == 0 && quant_white == 0 && 
        quant_min == 0 && quant_max == 0) {
        // Per RenderMan and Gelato heuristics, if all quantization
        // values are zero, assume they want a float output.
        return TypeDesc::FLOAT;
    } else if (quant_min >= std::numeric_limits <unsigned char>::min() && 
               quant_max <= std::numeric_limits <unsigned char>::max()) {
        return TypeDesc::UINT8;
    } else if (quant_min >= std::numeric_limits <char>::min() && 
               quant_max <= std::numeric_limits <char>::max()) {
        return TypeDesc::INT8;
    } else if (quant_min >= std::numeric_limits <unsigned short>::min() && 
               quant_max <= std::numeric_limits <unsigned short>::max()) {
        return TypeDesc::UINT16;
    } else if (quant_min >= std::numeric_limits <short>::min() && 
               quant_max <= std::numeric_limits <short>::max()) {
        return TypeDesc::INT16;
    } else if (quant_min >= std::numeric_limits <int>::min() && 
               quant_max <= std::numeric_limits <int>::max()) {
        return TypeDesc::INT;
    } else if (quant_min >= 0 && 
               (unsigned int) quant_min >= std::numeric_limits <unsigned int>::min() && 
               quant_max >= 0 &&
               (unsigned int) quant_max <= std::numeric_limits <unsigned int>::max()) {
        return TypeDesc::UINT;
    } else {
        return TypeDesc::UNKNOWN;
    }
}



void
ImageSpec::default_channel_names ()
{
    channelnames.clear();
    alpha_channel = -1;
    z_channel = -1;
    switch (nchannels) {
    case 1:
        channelnames.push_back ("A");
        break;
    case 2:
        channelnames.push_back ("I");
        channelnames.push_back ("A");
        alpha_channel = 1;
        break;
    case 3:
        channelnames.push_back ("R");
        channelnames.push_back ("G");
        channelnames.push_back ("B");
        break;
    default:
        if (nchannels >= 1)
            channelnames.push_back ("R");
        if (nchannels >= 2)
            channelnames.push_back ("G");
        if (nchannels >= 3)
            channelnames.push_back ("B");
        if (nchannels >= 4) {
            channelnames.push_back ("A");
            alpha_channel = 3;
        }
        for (int c = 4;  c < nchannels;  ++c)
            channelnames.push_back (Strutil::format("channel%d", c));
        break;
    }
}



void
ImageSpec::attribute (const std::string &name, TypeDesc type, const void *value)
{
    // Don't allow duplicates
    ImageIOParameter *f = find_attribute (name);
    if (! f) {
        extra_attribs.resize (extra_attribs.size() + 1);
        f = &extra_attribs.back();
    }
    f->init (name, type, 1, value);
}



ImageIOParameter *
ImageSpec::find_attribute (const std::string &name, TypeDesc searchtype,
                           bool casesensitive)
{
    if (casesensitive) {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (extra_attribs[i].name() == name &&
                (searchtype == TypeDesc::UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    } else {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (iequals (extra_attribs[i].name().string(), name) &&
                (searchtype == TypeDesc::UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    }
    return NULL;
}



const ImageIOParameter *
ImageSpec::find_attribute (const std::string &name, TypeDesc searchtype,
                           bool casesensitive) const
{
    if (casesensitive) {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (extra_attribs[i].name() == name &&
                (searchtype == TypeDesc::UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    } else {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (iequals (extra_attribs[i].name().string(), name) &&
                (searchtype == TypeDesc::UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    }
    return NULL;
}
