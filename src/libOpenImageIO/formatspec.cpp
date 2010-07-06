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
#include "fmath.h"

#include "imageio.h"

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
    case TypeDesc::INT64:
        set_default_quantize <long long> (quant_black, quant_white,
                                     quant_min, quant_max, quant_dither);
        break;
    case TypeDesc::UINT64:
        set_default_quantize <unsigned long long> (quant_black, quant_white,
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



size_t
ImageSpec::pixel_bytes () const
{
    if (nchannels < 0)
        return 0;
    return clamped_mult32 ((size_t)nchannels, channel_bytes());
}



imagesize_t
ImageSpec::scanline_bytes () const
{
    if (width < 0)
        return 0;
    return clamped_mult64 ((imagesize_t)width, (imagesize_t)pixel_bytes());
}



imagesize_t
ImageSpec::tile_pixels () const
{
    if (tile_width < 0 || tile_height < 0 || tile_depth < 0)
        return 0;
    imagesize_t r = clamped_mult64 ((imagesize_t)tile_width,
                                    (imagesize_t)tile_height);
    if (tile_depth > 1)
        r = clamped_mult64 (r, (imagesize_t)tile_depth);
    return r;
}



imagesize_t
ImageSpec::tile_bytes () const
{
    return clamped_mult64 (tile_pixels(), (imagesize_t)pixel_bytes());
}



imagesize_t
ImageSpec::image_pixels () const
{
    if (width < 0 || height < 0 || depth < 0)
        return 0;
    imagesize_t r = clamped_mult64 ((imagesize_t)width, (imagesize_t)height);
    if (depth > 1)
        r = clamped_mult64 (r, (imagesize_t)depth);
    return r;
}



imagesize_t
ImageSpec::image_bytes () const
{
    return clamped_mult64 (image_pixels(), (imagesize_t)pixel_bytes());
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



int
ImageSpec::get_int_attribute (const std::string &name, int val) const
{
    const ImageIOParameter *p = find_attribute (name);
    if (p) {
        if (p->type() == TypeDesc::INT)
            val = *(const int *)p->data();
        else if (p->type() == TypeDesc::UINT)
            val = (int) *(const unsigned int *)p->data();
        else if (p->type() == TypeDesc::INT16)
            val = *(const short *)p->data();
        else if (p->type() == TypeDesc::UINT16)
            val = *(const unsigned short *)p->data();
        else if (p->type() == TypeDesc::INT8)
            val = *(const char *)p->data();
        else if (p->type() == TypeDesc::UINT8)
            val = *(const unsigned char *)p->data();
        else if (p->type() == TypeDesc::INT64)
            val = *(const long long *)p->data();
        else if (p->type() == TypeDesc::UINT64)
            val = *(const unsigned long long *)p->data();
    }
    return val;
}



float
ImageSpec::get_float_attribute (const std::string &name, float val) const
{
    const ImageIOParameter *p = find_attribute (name);
    if (p) {
        if (p->type() == TypeDesc::FLOAT)
            val = *(const float *)p->data();
        else if (p->type() == TypeDesc::HALF)
            val = *(const half *)p->data();
        else if (p->type() == TypeDesc::DOUBLE)
            val = (float) *(const double *)p->data();
    }
    return val;
}



std::string
ImageSpec::get_string_attribute (const std::string &name,
                                 const std::string &val) const
{
    const ImageIOParameter *p = find_attribute (name, TypeDesc::STRING);
    if (p)
        return std::string (*(const char **)p->data());
    else return val;
}



namespace {  // make an anon namespace


static std::string
format_raw_metadata (const ImageIOParameter &p)
{
    std::string out;
    TypeDesc element = p.type().elementtype();
    int n = p.type().numelements() * p.nvalues();
    if (element == TypeDesc::STRING) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s\"%s\"", (i ? ", " : ""), ((const char **)p.data())[i]);
    } else if (element == TypeDesc::FLOAT) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%g", (i ? ", " : ""), ((const float *)p.data())[i]);
    } else if (element == TypeDesc::DOUBLE) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%g", (i ? ", " : ""), ((const double *)p.data())[i]);
    } else if (element == TypeDesc::INT) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%d", (i ? ", " : ""), ((const int *)p.data())[i]);
    } else if (element == TypeDesc::UINT) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%d", (i ? ", " : ""), ((const unsigned int *)p.data())[i]);
    } else if (element == TypeDesc::UINT16) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%u", (i ? ", " : ""), ((const unsigned short *)p.data())[i]);
    } else if (element == TypeDesc::INT16) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%d", (i ? ", " : ""), ((const short *)p.data())[i]);
    } else if (element == TypeDesc::UINT64) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%llu", (i ? ", " : ""), ((const unsigned long long *)p.data())[i]);
    } else if (element == TypeDesc::INT64) {
        for (int i = 0;  i < n;  ++i)
            out += Strutil::format ("%s%lld", (i ? ", " : ""), ((const long long *)p.data())[i]);
    } else if (element == TypeDesc::TypeMatrix) {
        const float *m = (const float *)p.data();
        for (int i = 0;  i < n;  ++i, m += 16)
            out += Strutil::format ("%s%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g",
                    (i ? ", " : ""), m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    }
    else {
        out += Strutil::format ("<unknown data type> (base %d, agg %d vec %d)",
                p.type().basetype, p.type().aggregate,
                p.type().vecsemantics);
    }
    return out;
}



struct LabelTable {
    int value;
    const char *label;
};

static std::string
explain_justprint (const ImageIOParameter &p, const void *extradata)
{
    return format_raw_metadata(p) + " " + std::string ((const char *)extradata);
}

static std::string
explain_labeltable (const ImageIOParameter &p, const void *extradata)
{
    int val;
    if (p.type() == TypeDesc::INT)
        val = *(const int *)p.data();
    else if (p.type() == TypeDesc::UINT)
        val = (int) *(const unsigned int *)p.data();
    else if (p.type() == TypeDesc::STRING)
        val = (int) **(const char **)p.data();
    else
        return std::string();
    for (const LabelTable *lt = (const LabelTable *)extradata; lt->label; ++lt)
        if (val == lt->value)
            return std::string (lt->label);
    return std::string();  // nothing
}

static std::string
explain_shutterapex (const ImageIOParameter &p, const void *extradata)
{
    if (p.type() == TypeDesc::FLOAT) {
        double val = pow (2.0, - (double)*(float *)p.data());
        if (val > 1)
            return Strutil::format ("%g s", val);
        else
            return Strutil::format ("1/%g s", floor(1.0/val));
    }
    return std::string();
}

static std::string
explain_apertureapex (const ImageIOParameter &p, const void *extradata)
{
    if (p.type() == TypeDesc::FLOAT)
        return Strutil::format ("f/%g", powf (2.0f, *(float *)p.data()/2.0f));
    return std::string();
}

static std::string
explain_ExifFlash (const ImageIOParameter &p, const void *extradata)
{
    if (p.type() == TypeDesc::UINT) {
        unsigned int val = *(unsigned int *)p.data();
        return Strutil::format ("%s%s%s%s%s%s%s%s",
                                (val&1) ? "flash fired" : "no flash",
                                (val&6) == 4 ? ", no strobe return" : "",
                                (val&6) == 6 ? ", strobe return" : "",
                                (val&24) == 8 ? ", compulsary flash" : "",
                                (val&24) == 16 ? ", flash supression" : "",
                                (val&24) == 24 ? ", auto flash" : "",
                                (val&32) ? ", no flash available" : "",
                                (val&64) ? ", red-eye reduction" : "");
    }
    return std::string();
}

static LabelTable ExifExposureProgram_table[] = {
    { 0, "" }, { 1, "manual" }, { 2, "normal program" },
    { 3, "aperture priority" }, { 4, "shutter priority" },
    { 5, "Creative program, biased toward DOF" },
    { 6, "Action program, biased toward fast shutter" },
    { 7, "Portrait mode, foreground in focus" },
    { 8, "Landscape mode, background in focus" },
    { -1, NULL }
};

static LabelTable ExifLightSource_table[] = {
    { 0, "unknown" }, { 1, "daylight" }, { 2, "tungsten/incandescent" },
    { 4, "flash" }, { 9, "fine weather" }, { 10, "cloudy" }, { 11, "shade" },
    { 12, "daylight fluorescent D 5700-7100K" },
    { 13, "day white fluorescent N 4600-5400K" },
    { 14, "cool white fluorescent W 3900-4500K" },
    { 15, "white fluorescent WW 3200-3700K" },
    { 17, "standard light A" }, { 18, "standard light B" },
    { 19, "standard light C" },
    { 20, "D55" }, { 21, "D65" }, { 22, "D75" }, { 23, "D50" },
    { 24, "ISO studio tungsten" }, { 255, "other" }, { -1, NULL }
};

static LabelTable ExifMeteringMode_table[] = {
    { 0, "" }, { 1, "average" }, { 2, "center-weighted average" },
    { 3, "spot" }, { 4, "multi-spot" }, { 5, "pattern" }, { 6, "partial" },
    { -1, NULL }
};

static LabelTable ExifSubjectDistanceRange_table[] = {
    { 0, "unknown" }, { 1, "macro" }, { 2, "close" }, { 3, "distant" },
    { -1, NULL }
};

static LabelTable ExifSceneCaptureType_table[] = {
    { 0, "standard" }, { 1, "landscape" }, { 2, "portrait" }, 
    { 3, "night scene" }, { -1, NULL }
};

static LabelTable orientation_table[] = {
    { 1, "normal" }, 
    { 2, "flipped horizontally" }, 
    { 3, "rotated 180 deg" }, 
    { 4, "flipped vertically" }, 
    { 5, "transposed top<->left" }, 
    { 6, "rotated 90 deg CW" }, 
    { 7, "transverse top<->right" }, 
    { 8, "rotated 90 deg CCW" }, 
    { -1, NULL }
};

static LabelTable resunit_table[] = {
    { 1, "none" }, { 2, "inches" }, { 3, "cm" }, { -1, NULL }
};

static LabelTable ExifSensingMethod_table[] = {
    { 1, "undefined" }, { 2, "1-chip color area" }, 
    { 3, "2-chip color area" }, { 4, "3-chip color area" }, 
    { 5, "color sequential area" }, { 7, "trilinear" }, 
    { 8, "color trilinear" }, { -1, NULL }
};

static LabelTable ExifFileSource_table[] = {
    { 3, "digital camera" }, { -1, NULL }
};

static LabelTable ExifSceneType_table[] = {
    { 1, "directly photographed" }, { -1, NULL }
};

static LabelTable ExifExposureMode_table[] = {
    { 0, "auto" }, { 1, "manual" }, { 2, "auto-bracket" }, { -1, NULL }
};

static LabelTable ExifWhiteBalance_table[] = {
    { 0, "auto" }, { 1, "manual" }, { -1, NULL }
};

static LabelTable ExifGainControl_table[] = {
    { 0, "none" }, { 1, "low gain up" }, { 2, "high gain up" }, 
    { 3, "low gain down" }, { 4, "high gain down" },
    { -1, NULL }
};

static LabelTable yesno_table[] = {
    { 0, "no" }, { 1, "yes" }, { -1, NULL }
};

static LabelTable softhard_table[] = {
    { 0, "normal" }, { 1, "soft" }, { 2, "hard" }, { -1, NULL }
};

static LabelTable lowhi_table[] = {
    { 0, "normal" }, { 1, "low" }, { 2, "high" }, { -1, NULL }
};

static LabelTable GPSAltitudeRef_table[] = {
    { 0, "above sea level" }, { 1, "below sea level" }, { -1, NULL }
};

static LabelTable GPSStatus_table[] = {
    { 'A', "measurement in progress" }, { 'V', "measurement interoperability" },
    { -1, NULL }
};

static LabelTable GPSMeasureMode_table[] = {
    { '2', "2-D" }, { '3', "3-D" }, { -1, NULL }
};

static LabelTable GPSSpeedRef_table[] = {
    { 'K', "km/hour" }, { 'M', "miles/hour" }, { 'N', "knots" }, 
    { -1, NULL }
};

static LabelTable GPSDestDistanceRef_table[] = {
    { 'K', "km" }, { 'M', "miles" }, { 'N', "knots" }, 
    { -1, NULL }
};

static LabelTable magnetic_table[] = {
    { 'T', "true direction" }, { 'M', "magnetic direction" }, { -1, NULL }
};

typedef std::string (*ExplainerFunc) (const ImageIOParameter &p, 
                                      const void *extradata);

struct ExplanationTableEntry {
    const char    *oiioname;
    ExplainerFunc  explainer;
    const void    *extradata;
};

static ExplanationTableEntry explanation[] = {
    { "ResolutionUnit", explain_labeltable, resunit_table },
    { "Orientation", explain_labeltable, orientation_table },
    { "Exif:ExposureProgram", explain_labeltable, ExifExposureProgram_table },
    { "Exif:ShutterSpeedValue", explain_shutterapex, NULL },
    { "Exif:ApertureValue", explain_apertureapex, NULL },
    { "Exif:MaxApertureValue", explain_apertureapex, NULL },
    { "Exif:SubjectDistance", explain_justprint, "m" },
    { "Exif:MeteringMode", explain_labeltable, ExifMeteringMode_table },
    { "Exif:LightSource", explain_labeltable, ExifLightSource_table },
    { "Exif:Flash", explain_ExifFlash, NULL },
    { "Exif:FocalLength", explain_justprint, "mm" },
    { "Exif:FlashEnergy", explain_justprint, "BCPS" },
    { "Exif:FocalPlaneResolutionUnit", explain_labeltable, resunit_table },
    { "Exif:SensingMethod", explain_labeltable, ExifSensingMethod_table },
    { "Exif:FileSource", explain_labeltable, ExifFileSource_table },
    { "Exif:SceneType", explain_labeltable, ExifSceneType_table },
    { "Exif:CustomRendered", explain_labeltable, yesno_table },
    { "Exif:ExposureMode", explain_labeltable, ExifExposureMode_table },
    { "Exif:WhiteBalance", explain_labeltable, ExifWhiteBalance_table },
    { "Exif:SceneCaptureType", explain_labeltable, ExifSceneCaptureType_table },
    { "Exif:GainControl", explain_labeltable, ExifGainControl_table },
    { "Exif:Contrast", explain_labeltable, softhard_table },
    { "Exif:Saturation", explain_labeltable, lowhi_table },
    { "Exif:Sharpness", explain_labeltable, softhard_table },
    { "Exif:SubjectDistanceRange", explain_labeltable, ExifSubjectDistanceRange_table },
    { "GPS:AltitudeRef", explain_labeltable, GPSAltitudeRef_table },
    { "GPS:Altitude", explain_justprint, "m" },
    { "GPS:Status", explain_labeltable, GPSStatus_table },
    { "GPS:MeasureMode", explain_labeltable, GPSMeasureMode_table },
    { "GPS:SpeedRef", explain_labeltable, GPSSpeedRef_table },
    { "GPS:TrackRef", explain_labeltable, magnetic_table },
    { "GPS:ImgDirectionRef", explain_labeltable, magnetic_table },
    { "GPS:DestBearingRef", explain_labeltable, magnetic_table },
    { "GPS:DestDistanceRef", explain_labeltable, GPSDestDistanceRef_table },
    { "GPS:Differential", explain_labeltable, yesno_table },
    { NULL, NULL, NULL }
}; 

}; // end anon namespace



std::string
ImageSpec::metadata_val (const ImageIOParameter &p, bool human) const
{
    std::string out = format_raw_metadata (p);

    if (human) {
        std::string nice;
        for (int e = 0;  explanation[e].oiioname;  ++e) {
            if (! strcmp (explanation[e].oiioname, p.name().c_str()) &&
                explanation[e].explainer) {
                nice = explanation[e].explainer (p, explanation[e].extradata);
                break;
            }
        }
        if (nice.length())
            out = out + " (" + nice + ")";
    }

    return out;
}
