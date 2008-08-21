/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>

#include <half.h>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "dassert.h"
#include "paramtype.h"

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
set_default_quantize (ParamBaseType format,
                      int &quant_black, int &quant_white,
                      int &quant_min, int &quant_max, float &quant_dither)
{
    switch (format) {
    case PT_INT8:
        set_default_quantize <char> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_UNKNOWN:
    case PT_UINT8:
        set_default_quantize <unsigned char> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_INT16:
        set_default_quantize <short> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_UINT16:
        set_default_quantize <unsigned short> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_INT:
        set_default_quantize <int> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_UINT:
        set_default_quantize <unsigned int> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_HALF:
        set_default_quantize <half> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_FLOAT:
        set_default_quantize <float> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case PT_DOUBLE:
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



QuantizationSpec::QuantizationSpec (ParamBaseType _type)
{
    set_default_quantize (_type, quant_black, quant_white,
                          quant_min, quant_max, quant_dither);
}



ImageIOFormatSpec::ImageIOFormatSpec (ParamBaseType format)
    : x(0), y(0), z(0), width(0), height(0), depth(1),
      full_width(0), full_height(0), full_depth(0),
      tile_width(0), tile_height(0), tile_depth(1),
      format(format), nchannels(0), alpha_channel(-1), z_channel(-1),
      linearity(UnknownLinearity), gamma(1)
{
    set_format (format);
}



ImageIOFormatSpec::ImageIOFormatSpec (int xres, int yres, int nchans, 
                                      ParamBaseType format)
    : x(0), y(0), z(0), width(xres), height(yres), depth(1),
      full_width(0), full_height(0), full_depth(0),
      tile_width(0), tile_height(0), tile_depth(1),
      format(format), nchannels(nchans), alpha_channel(-1), z_channel(-1),
      linearity(UnknownLinearity), gamma(1)
{
    set_format (format);
    if (nchans == 4)
        alpha_channel = 3;
}



void
ImageIOFormatSpec::set_format (ParamBaseType fmt)
{
    format = fmt;
    set_default_quantize (fmt, quant_black, quant_white,
                          quant_min, quant_max, quant_dither);
}



ParamBaseType
ImageIOFormatSpec::format_from_quantize (int quant_black, int quant_white,
                                         int quant_min, int quant_max)
{
    if (quant_black == 0 && quant_white == 0 && 
        quant_min == 0 && quant_max == 0) {
        // Per RenderMan and Gelato heuristics, if all quantization
        // values are zero, assume they want a float output.
        return PT_FLOAT;
    } else if (quant_min >= std::numeric_limits <unsigned char>::min() && 
               quant_max <= std::numeric_limits <unsigned char>::max()) {
        return PT_UINT8;
    } else if (quant_min >= std::numeric_limits <char>::min() && 
               quant_max <= std::numeric_limits <char>::max()) {
        return PT_INT8;
    } else if (quant_min >= std::numeric_limits <unsigned short>::min() && 
               quant_max <= std::numeric_limits <unsigned short>::max()) {
        return PT_UINT16;
    } else if (quant_min >= std::numeric_limits <short>::min() && 
               quant_max <= std::numeric_limits <short>::max()) {
        return PT_INT16;
    } else if (quant_min >= std::numeric_limits <int>::min() && 
               quant_max <= std::numeric_limits <int>::max()) {
        return PT_INT;
    } else if (quant_min >= std::numeric_limits <unsigned int>::min() && 
               quant_max <= std::numeric_limits <unsigned int>::max()) {
        return PT_UINT;
    } else {
        return PT_UNKNOWN;
    }
}



void
ImageIOFormatSpec::attribute (const std::string &name, ParamBaseType type,
                              int nvalues, const void *value)
{
    // Don't allow duplicates
    ImageIOParameter *f = find_attribute (name);
    if (! f) {
        extra_attribs.resize (extra_attribs.size() + 1);
        f = &extra_attribs.back();
    }
    f->init (name, type, nvalues, value);
}



ImageIOParameter *
ImageIOFormatSpec::find_attribute (const std::string &name,
                                   ParamType searchtype,
                                   bool casesensitive)
{
    if (casesensitive) {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (extra_attribs[i].name() == name &&
                (searchtype == PT_UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    } else {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (iequals (extra_attribs[i].name().string(), name) &&
                (searchtype == PT_UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    }
    return NULL;
}



const ImageIOParameter *
ImageIOFormatSpec::find_attribute (const std::string &name,
                                   ParamType searchtype,
                                   bool casesensitive) const
{
    if (casesensitive) {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (extra_attribs[i].name() == name &&
                (searchtype == PT_UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    } else {
        for (size_t i = 0;  i < extra_attribs.size();  ++i)
            if (iequals (extra_attribs[i].name().string(), name) &&
                (searchtype == PT_UNKNOWN || searchtype == extra_attribs[i].type()))
                return &extra_attribs[i];
    }
    return NULL;
}
