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
#include <sstream>

#include <OpenEXR/half.h>
#include <OpenEXR/ImfTimeCode.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/fmath.h"
#include "OpenImageIO/imageio.h"
#include "imageio_pvt.h"

#if USE_EXTERNAL_PUGIXML
# include "pugixml.hpp"
#else
# include "OpenImageIO/pugixml.hpp"
#endif

OIIO_NAMESPACE_BEGIN

// Generate the default quantization parameters, templated on the data
// type.
template <class T>
inline void
get_default_quantize_ (long long &quant_min, long long &quant_max)
{
    if (std::numeric_limits <T>::is_integer) {
        quant_min    = (long long) std::numeric_limits <T>::min();
        quant_max    = (long long) std::numeric_limits <T>::max();
    } else {
        quant_min    = 0;
        quant_max    = 0;
    }
}



// Given the format, set the default quantization range.
// Rely on the template version to make life easy.
void
pvt::get_default_quantize (TypeDesc format,
                           long long &quant_min, long long &quant_max)
{
    switch (format.basetype) {
    case TypeDesc::UNKNOWN:
    case TypeDesc::UINT8:
        get_default_quantize_ <unsigned char> (quant_min, quant_max);
        break;
    case TypeDesc::UINT16:
        get_default_quantize_ <unsigned short> (quant_min, quant_max);
        break;
    case TypeDesc::HALF:
        get_default_quantize_ <half> (quant_min, quant_max);
        break;
    case TypeDesc::FLOAT:
        get_default_quantize_ <float> (quant_min, quant_max);
        break;
    case TypeDesc::INT8:
        get_default_quantize_ <char> (quant_min, quant_max);
        break;
    case TypeDesc::INT16:
        get_default_quantize_ <short> (quant_min, quant_max);
        break;
    case TypeDesc::INT:
        get_default_quantize_ <int> (quant_min, quant_max);
        break;
    case TypeDesc::UINT:
        get_default_quantize_ <unsigned int> (quant_min, quant_max);
        break;
    case TypeDesc::INT64:
        get_default_quantize_ <long long> (quant_min, quant_max);
        break;
    case TypeDesc::UINT64:
        get_default_quantize_ <unsigned long long> (quant_min, quant_max);
        break;
    case TypeDesc::DOUBLE:
        get_default_quantize_ <double> (quant_min, quant_max);
        break;
    default: ASSERT_MSG (0, "Unknown data format %d", format.basetype);
    }
}



ImageSpec::ImageSpec (TypeDesc format)
    : x(0), y(0), z(0), width(0), height(0), depth(1),
      full_x(0), full_y(0), full_z(0),
      full_width(0), full_height(0), full_depth(0),
      tile_width(0), tile_height(0), tile_depth(1),
      nchannels(0), format(format), alpha_channel(-1), z_channel(-1),
      deep(false)
{
}



ImageSpec::ImageSpec (int xres, int yres, int nchans, TypeDesc format)
    : x(0), y(0), z(0), width(xres), height(yres), depth(1),
      full_x(0), full_y(0), full_z(0),
      full_width(xres), full_height(yres), full_depth(1),
      tile_width(0), tile_height(0), tile_depth(1),
      nchannels(nchans), format(format), alpha_channel(-1), z_channel(-1),
      deep(false)
{
    default_channel_names ();
}



void
ImageSpec::set_format (TypeDesc fmt)
{
    format = fmt;
    channelformats.clear ();
}



void
ImageSpec::default_channel_names ()
{
    channelnames.clear();
    channelnames.reserve (nchannels);
    alpha_channel = -1;
    z_channel = -1;
    if (nchannels == 1) {   // Special case: 1-channel is named "Y"
        channelnames.push_back ("Y");
        return;
    }
    // General case: name channels R, G, B, A, channel4, channel5, ...
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
}



size_t
ImageSpec::channel_bytes (int chan, bool native) const
{
    if (chan >= nchannels)
        return 0;
    if (!native || channelformats.empty())
        return format.size();
    else
        return channelformats[chan].size();
}



size_t
ImageSpec::pixel_bytes (bool native) const
{
    if (nchannels < 0)
        return 0;
    if (!native || channelformats.empty())
        return clamped_mult32 ((size_t)nchannels, channel_bytes());
    else {
        size_t sum = 0;
        for (int i = 0;  i < nchannels;  ++i)
            sum += channelformats[i].size();
        return sum;
    }
}



size_t
ImageSpec::pixel_bytes (int chbegin, int chend, bool native) const
{
    if (chbegin < 0)
        return 0;
    chend = std::max (chend, chbegin);
    if (!native || channelformats.empty())
        return clamped_mult32 ((size_t)(chend-chbegin), channel_bytes());
    else {
        size_t sum = 0;
        for (int i = chbegin; i < chend;  ++i)
            sum += channelformats[i].size();
        return sum;
    }
}



imagesize_t
ImageSpec::scanline_bytes (bool native) const
{
    if (width < 0)
        return 0;
    return clamped_mult64 ((imagesize_t)width, (imagesize_t)pixel_bytes(native));
}



imagesize_t
ImageSpec::tile_pixels () const
{
    if (tile_width <= 0 || tile_height <= 0 || tile_depth <= 0)
        return 0;
    imagesize_t r = clamped_mult64 ((imagesize_t)tile_width,
                                    (imagesize_t)tile_height);
    if (tile_depth > 1)
        r = clamped_mult64 (r, (imagesize_t)tile_depth);
    return r;
}



imagesize_t
ImageSpec::tile_bytes (bool native) const
{
    return clamped_mult64 (tile_pixels(), (imagesize_t)pixel_bytes(native));
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
ImageSpec::image_bytes (bool native) const
{
    return clamped_mult64 (image_pixels(), (imagesize_t)pixel_bytes(native));
}



void
ImageSpec::attribute (string_view name, TypeDesc type, const void *value)
{
    // Don't allow duplicates
    ImageIOParameter *f = find_attribute (name);
    if (! f) {
        extra_attribs.resize (extra_attribs.size() + 1);
        f = &extra_attribs.back();
    }
    f->init (name, type, 1, value);
}



template <class T>
static void
parse_elements (string_view name, TypeDesc type, const char *type_code,
                string_view value, ImageIOParameter &param)
{
    int num_items = type.numelements() * type.aggregate;
    T *data = (T*) param.data();
    // Erase any leading whitespace
    value.remove_prefix (value.find_first_not_of (" \t"));
    for (int i = 0;  i < num_items;  ++i) {
        // Make a temporary copy so we for sure have a 0-terminated string.
        std::string temp = value;
        // Grab the first value from it
        sscanf (temp.c_str(), type_code, &data[i]);
        // Skip the value (eat until we find a delimiter -- space, comma, tab)
        value.remove_prefix (value.find_first_of (" ,\t"));
        // Skip the delimiter
        value.remove_prefix (value.find_first_not_of (" ,\t"));
        if (value.empty())
            break;   // done if nothing left to parse
    }
}



void
ImageSpec::attribute (string_view name, TypeDesc type, string_view value)
{
    ImageIOParameter param (name, type, 1, NULL);
    TypeDesc::BASETYPE basetype = (TypeDesc::BASETYPE)type.basetype;

    if (basetype == TypeDesc::INT) {
        parse_elements<int> (name, type, "%d", value, param);
    } else if (basetype == TypeDesc::UINT) {
        parse_elements<unsigned int> (name, type, "%u", value, param);
    } else if (basetype == TypeDesc::FLOAT) {
        parse_elements<float> (name, type, "%f", value, param);
    } else if (basetype == TypeDesc::DOUBLE) {
        parse_elements<double> (name, type, "%lf", value, param);
    } else if (basetype == TypeDesc::INT64) {
        parse_elements<long long> (name, type, "%lld", value, param);
    } else if (basetype == TypeDesc::UINT64) {
        parse_elements<unsigned long long> (name, type, "%llu", value, param);
    } else if (basetype == TypeDesc::INT16) {
        parse_elements<short> (name, type, "%hd", value, param);
    } else if (basetype == TypeDesc::UINT16) {
        parse_elements<unsigned short> (name, type, "%hu", value, param);
    } else if (type == TypeDesc::STRING) {
        ustring s (value);
        param.init (name, TypeDesc::TypeString, 1, &s);
    }

    // Don't allow duplicates
    ImageIOParameter *f = find_attribute (name);
    if (f) {
        *f = param;
    } else {
        extra_attribs.push_back (param);
    }
}



void
ImageSpec::erase_attribute (string_view name, TypeDesc searchtype,
                            bool casesensitive)
{
    ImageIOParameterList::iterator iter =
        extra_attribs.find (name, searchtype, casesensitive);
    if (iter != extra_attribs.end())
        extra_attribs.erase (iter);
}


ImageIOParameter *
ImageSpec::find_attribute (string_view name, TypeDesc searchtype,
                           bool casesensitive)
{
    ImageIOParameterList::iterator iter =
        extra_attribs.find (name, searchtype, casesensitive);
    if (iter != extra_attribs.end())
        return &(*iter);
    return NULL;
}



const ImageIOParameter *
ImageSpec::find_attribute (string_view name, TypeDesc searchtype,
                           bool casesensitive) const
{
    ImageIOParameterList::const_iterator iter =
        extra_attribs.find (name, searchtype, casesensitive);
    if (iter != extra_attribs.end())
        return &(*iter);
    return NULL;
}



const ImageIOParameter *
ImageSpec::find_attribute (string_view name, ImageIOParameter &tmpparam,
                           TypeDesc searchtype, bool casesensitive) const
{
    ImageIOParameterList::const_iterator iter =
        extra_attribs.find (name, searchtype, casesensitive);
    if (iter != extra_attribs.end())
        return &(*iter);
    // Check named items in the ImageSpec structs, not in extra_attrubs
#define MATCH(n,t) (((!casesensitive && Strutil::iequals(name,n)) || \
                     ( casesensitive && name == n)) && \
                    (searchtype == TypeDesc::UNKNOWN || searchtype == t))
#define GETINT(n) if (MATCH(#n,TypeDesc::TypeInt)) { \
                      tmpparam.init (#n, TypeDesc::TypeInt, 1, &this->n); \
                      return &tmpparam; \
                  }
    GETINT(nchannels);
    GETINT(width);
    GETINT(height);
    GETINT(depth);
    GETINT(x);
    GETINT(y);
    GETINT(z);
    GETINT(full_width);
    GETINT(full_height);
    GETINT(full_depth);
    GETINT(full_x);
    GETINT(full_y);
    GETINT(full_z);
    GETINT(tile_width);
    GETINT(tile_height);
    GETINT(tile_depth);
    GETINT(alpha_channel);
    GETINT(z_channel);
    // some special cases
    if (MATCH("geom", TypeDesc::TypeString)) {
        ustring s = (depth <= 1 && full_depth <= 1)
                    ? ustring::format ("%dx%d%+d%+d", width, height, x, y)
                    : ustring::format ("%dx%dx%d%+d%+d%+d", width, height, depth, x, y, z);
        tmpparam.init ("geom", TypeDesc::TypeString, 1, &s);
        return &tmpparam;
    }
    if (MATCH("full_geom", TypeDesc::TypeString)) {
        ustring s = (depth <= 1 && full_depth <= 1)
                    ? ustring::format ("%dx%d%+d%+d",
                                       full_width, full_height, full_x, full_y)
                    : ustring::format ("%dx%dx%d%+d%+d%+d",
                                       full_width, full_height, full_depth,
                                       full_x, full_y, full_z);
        tmpparam.init ("full_geom", TypeDesc::TypeString, 1, &s);
        return &tmpparam;
    }
#undef GETINT
#undef MATCH
    return NULL;
}



int
ImageSpec::get_int_attribute (string_view name, int val) const
{
    ImageIOParameter tmpparam;
    const ImageIOParameter *p = find_attribute (name, tmpparam);
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
ImageSpec::get_float_attribute (string_view name, float val) const
{
    ImageIOParameter tmpparam;
    const ImageIOParameter *p = find_attribute (name, tmpparam);
    if (p) {
        if (p->type() == TypeDesc::FLOAT)
            val = *(const float *)p->data();
        else if (p->type() == TypeDesc::HALF)
            val = *(const half *)p->data();
        else if (p->type() == TypeDesc::DOUBLE)
            val = (float) *(const double *)p->data();
        else if (p->type() == TypeDesc::INT)
            val = (float) *(const int *)p->data();
        else if (p->type() == TypeDesc::UINT)
            val = (float) *(const unsigned int *)p->data();
        else if (p->type() == TypeDesc::INT16)
            val = (float) *(const short *)p->data();
        else if (p->type() == TypeDesc::UINT16)
            val = (float) *(const unsigned short *)p->data();
        else if (p->type() == TypeDesc::INT8)
            val = (float) *(const char *)p->data();
        else if (p->type() == TypeDesc::UINT8)
            val = (float) *(const unsigned char *)p->data();
        else if (p->type() == TypeDesc::INT64)
            val = (float) *(const long long *)p->data();
        else if (p->type() == TypeDesc::UINT64)
            val = (float) *(const unsigned long long *)p->data();
    }
    return val;
}



string_view
ImageSpec::get_string_attribute (string_view name, string_view val) const
{
    ImageIOParameter tmpparam;
    const ImageIOParameter *p = find_attribute (name, tmpparam, TypeDesc::STRING);
    if (p)
        return *(ustring *)p->data();
    else return val;
}



namespace {  // make an anon namespace

template < typename T >
void formatType(const ImageIOParameter& p, const int n, const TypeDesc& element, const char* formatString, std::string& out) {
  const T *f = (const T *)p.data();
  for (int i = 0;  i < n;  ++i) {
      if (i)
          out += ", ";
      for (int c = 0;  c < (int)element.aggregate;  ++c, ++f)
          out += Strutil::format (formatString, (c ? " " : ""), f[0]);
  }
}

static std::string
format_raw_metadata (const ImageIOParameter &p, int maxsize=16)
{
    std::string out;
    TypeDesc element = p.type().elementtype();
    int nfull = int(p.type().numelements()) * p.nvalues();
    int n = std::min (nfull, maxsize);
    if (element.basetype == TypeDesc::STRING) {
        for (int i = 0;  i < n;  ++i) {
            const char *s = ((const char **)p.data())[i];
            out += Strutil::format ("%s\"%s\"", (i ? ", " : ""),
                                    s ? Strutil::escape_chars(s) : std::string());
        }
    } else if (element.basetype == TypeDesc::FLOAT) {
        formatType< float >(p, n, element, "%s%g", out);
    } else if (element.basetype == TypeDesc::DOUBLE) {
        formatType< double >(p, n, element, "%s%g", out);
    } else if (element.basetype == TypeDesc::HALF) {
        formatType< half >(p, n, element, "%s%g", out);
    } else if (element.basetype == TypeDesc::INT) {
        formatType< int >(p, n, element, "%s%d", out);
    } else if (element.basetype == TypeDesc::UINT) {
        formatType< unsigned int >(p, n, element, "%s%d", out);
    } else if (element.basetype == TypeDesc::UINT16) {
        formatType< unsigned short >(p, n, element, "%s%u", out);
    } else if (element.basetype == TypeDesc::INT16) {
        formatType< short >(p, n, element, "%s%d", out);
    } else if (element.basetype == TypeDesc::UINT64) {
        formatType< unsigned long long >(p, n, element, "%s%llu", out);
    } else if (element.basetype == TypeDesc::INT64) {
        formatType< long long >(p, n, element, "%s%lld", out);
    } else if (element.basetype == TypeDesc::UINT8) {
        formatType< unsigned char >(p, n, element, "%s%d", out);
    } else if (element.basetype == TypeDesc::INT8) {
        formatType< char >(p, n, element, "%s%d", out);
    } else {
        out += Strutil::format ("<unknown data type> (base %d, agg %d vec %d)",
                p.type().basetype, p.type().aggregate,
                p.type().vecsemantics);
    }
    if (n < nfull)
        out += ", ...";
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
    int val = 0;
    if (p.type() == TypeDesc::INT)
        val = *(int *)p.data();
    else if (p.type() == TypeDesc::UINT)
        val = *(unsigned int *)p.data();
    else return std::string();
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

static LabelTable ExifExposureProgram_table[] = {
    { 0, "" }, { 1, "manual" }, { 2, "normal program" },
    { 3, "aperture priority" }, { 4, "shutter priority" },
    { 5, "Creative program, biased toward DOF" },
    { 6, "Action program, biased toward fast shutter" },
    { 7, "Portrait mode, foreground in focus" },
    { 8, "Landscape mode, background in focus" },
    { 9, "bulb" },
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
    { 1, "none" }, { 2, "inches" }, { 3, "cm" },
    { 4, "mm" }, { 5, "um" }, { -1, NULL }
};

static LabelTable ExifSensingMethod_table[] = {
    { 1, "undefined" }, { 2, "1-chip color area" }, 
    { 3, "2-chip color area" }, { 4, "3-chip color area" }, 
    { 5, "color sequential area" }, { 7, "trilinear" }, 
    { 8, "color trilinear" }, { -1, NULL }
};

static LabelTable ExifFileSource_table[] = {
    { 1, "film scanner" }, { 2, "reflection print scanner" },
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
    { 'A', "measurement active" }, { 'V', "measurement void" },
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
    { 'K', "km" }, { 'M', "miles" }, { 'N', "nautical miles" },
    { -1, NULL }
};

static LabelTable magnetic_table[] = {
    { 'T', "true north" }, { 'M', "magnetic north" }, { -1, NULL }
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

} // end anon namespace



std::string
ImageSpec::metadata_val (const ImageIOParameter &p, bool human)
{
    std::string out = format_raw_metadata (p, human ? 16 : 1024);

    if (human) {
        std::string nice;
        for (int e = 0;  explanation[e].oiioname;  ++e) {
            if (! strcmp (explanation[e].oiioname, p.name().c_str()) &&
                explanation[e].explainer) {
                nice = explanation[e].explainer (p, explanation[e].extradata);
                break;
            }
        }
        if (p.type() == TypeDesc::TypeTimeCode) {
            Imf::TimeCode tc = *reinterpret_cast<const Imf::TimeCode *>(p.data());
            nice = Strutil::format ("%02d:%02d:%02d:%02d", tc.hours(),
                                    tc.minutes(), tc.seconds(), tc.frame());
        }
        if (nice.length())
            out = out + " (" + nice + ")";
    }

    return out;
}



namespace { // Helper functions for from_xml () and to_xml () methods.

using namespace pugi;

static xml_node
add_node (xml_node &node, string_view node_name, const char *val)
{
    xml_node newnode = node.append_child();
    newnode.set_name (node_name.c_str ());
    newnode.append_child (node_pcdata).set_value (val);
    return newnode;
}



static xml_node
add_node (xml_node &node, string_view node_name, const int val)
{
    char buf[64];
    sprintf (buf, "%d", val);
    return add_node (node, node_name, buf);
}



static void
add_channelnames_node (xml_document &doc, const std::vector<std::string> &channelnames)
{
    xml_node channel_node = doc.child ("ImageSpec").append_child();
    channel_node.set_name ("channelnames");
    for (auto&& name : channelnames)
        add_node (channel_node, "channelname", name.c_str ());
}



static void
get_channelnames (const xml_node &n, std::vector<std::string> &channelnames)
{
    xml_node channel_node = n.child ("channelnames");

    for (xml_node n = channel_node.child ("channelname"); n;
            n = n.next_sibling ("channelname")) {
        channelnames.push_back (n.child_value ());
    }
}

} // end of anonymous namespace



static const char *
extended_format_name (TypeDesc type, int bits)
{
    if (bits && bits < (int)type.size()*8) {
        // The "oiio:BitsPerSample" betrays a different bit depth in the
        // file than the data type we are passing.
        if (type == TypeDesc::UINT8 || type == TypeDesc::UINT16 ||
            type == TypeDesc::UINT32 || type == TypeDesc::UINT64)
            return ustring::format("uint%d", bits).c_str();
        if (type == TypeDesc::INT8 || type == TypeDesc::INT16 ||
            type == TypeDesc::INT32 || type == TypeDesc::INT64)
            return ustring::format("int%d", bits).c_str();
    }
    return type.c_str();  // use the name implied by type
}



inline std::string
format_res (const ImageSpec &spec, int w, int h, int d)
{
    using Strutil::format;
    return (spec.depth > 1) ? format("%d x %d x %d", w, h, d) : format("%d x %d", w, h);
}


inline std::string
format_offset (const ImageSpec &spec, int x, int y, int z)
{
    using Strutil::format;
    return (spec.depth > 1) ? format("%d, %d, %d", x, y, z) : format("%d, %d", x, y);
}



static std::string
spec_to_xml (const ImageSpec &spec, ImageSpec::SerialVerbose verbose)
{
    xml_document doc;

    doc.append_child ().set_name ("ImageSpec");
    doc.child ("ImageSpec").append_attribute ("version") = OIIO_PLUGIN_VERSION;
    xml_node node = doc.child ("ImageSpec");

    add_node (node, "x", spec.x);
    add_node (node, "y", spec.y);
    add_node (node, "z", spec.z);
    add_node (node, "width", spec.width);
    add_node (node, "height", spec.height);
    add_node (node, "depth", spec.depth);
    add_node (node, "full_x", spec.full_x);
    add_node (node, "full_y", spec.full_y);
    add_node (node, "full_z", spec.full_z);
    add_node (node, "full_width", spec.full_width);
    add_node (node, "full_height", spec.full_height);
    add_node (node, "full_depth", spec.full_depth);
    add_node (node, "tile_width", spec.tile_width);
    add_node (node, "tile_height", spec.tile_height);
    add_node (node, "tile_depth", spec.tile_depth);
    add_node (node, "format", spec.format.c_str ());
    add_node (node, "nchannels", spec.nchannels);
    add_channelnames_node (doc, spec.channelnames);
    add_node (node, "alpha_channel", spec.alpha_channel);
    add_node (node, "z_channel", spec.z_channel);
    add_node (node, "deep", int(spec.deep));

    if (verbose > ImageSpec::SerialBrief) {
        for (auto&& p : spec.extra_attribs) {
            std::string s = spec.metadata_val (p, false);  // raw data
            if (s == "1.#INF")
                s ="inf";
            if (p.type() == TypeDesc::STRING) {
                if (s.size() >= 2 && s[0] == '\"' && s[s.size()-1] == '\"')
                    s = s.substr (1, s.size()-2);
            }
            std::string desc;
            for (int e = 0;  explanation[e].oiioname;  ++e) {
                if (! strcmp (explanation[e].oiioname, p.name().c_str()) &&
                    explanation[e].explainer) {
                    desc = explanation[e].explainer (p, explanation[e].extradata);
                    break;
                }
            }
            if (p.type() == TypeDesc::TypeTimeCode) {
                Imf::TimeCode tc = *reinterpret_cast<const Imf::TimeCode *>(p.data());
                desc = Strutil::format ("%02d:%02d:%02d:%02d", tc.hours(),
                                        tc.minutes(), tc.seconds(), tc.frame());
            }
            xml_node n = add_node (node, "attrib", s.c_str());
            n.append_attribute("name").set_value (p.name().c_str());
            n.append_attribute("type").set_value (p.type().c_str());
            if (! desc.empty())
                n.append_attribute("description").set_value (desc.c_str());
        }
    }

    std::ostringstream result;
    doc.print (result, "");
    return result.str();
}




std::string
ImageSpec::serialize (SerialFormat fmt, SerialVerbose verbose) const
{
    if (fmt == SerialXML)
        return spec_to_xml (*this, verbose);

    // Text case:
    //

    using Strutil::format;
    std::stringstream out;

    out << ((depth > 1) ? format("%4d x %4d x %4d", width, height, depth)
                        : format("%4d x %4d", width, height));
    out << format (", %d channel, %s%s", nchannels,
                   deep ? "deep " : "", depth > 1 ? "volume " : "");
    if (channelformats.size()) {
        for (size_t c = 0;  c < channelformats.size();  ++c)
            out << format ("%s%s", c ? "/" : "", channelformats[c]);
    } else {
        int bits = get_int_attribute ("oiio:BitsPerSample", 0);
        out << extended_format_name (this->format, bits);
    }
    out << '\n';

    if (verbose >= SerialDetailed) {
        out << "    channel list: ";
        for (int i = 0;  i < nchannels;  ++i) {
            if (i < (int)channelnames.size())
                out << channelnames[i];
            else
                out << "unknown";
            if (i < (int)channelformats.size())
                out << format (" (%s)", channelformats[i]);
            if (i < nchannels-1)
                out << ", ";
        }
        out << '\n';
        if (x || y || z) {
            out << "    pixel data origin: "
                << ((depth > 1) ? format("x=%d, y=%d, z=%d", x, y, z)
                                : format("x=%d, y=%d", x, y)) << '\n';
        }
        if (full_x || full_y || full_z ||
              (full_width != width && full_width != 0) ||
              (full_height != height && full_height != 0) ||
              (full_depth != depth && full_depth != 0)) {
            out << "    full/display size: "
                << format_res (*this, full_width, full_height, full_depth) << '\n';
            out << "    full/display origin: "
                << format_offset (*this, full_x, full_y, full_z) << '\n';
        }
        if (tile_width) {
            out << "    tile size: "
                << format_res (*this, tile_width, tile_height, tile_depth) << '\n';
        }

        for (auto&& p : extra_attribs) {
            out << format ("    %s: ", p.name());
            std::string s = metadata_val (p, verbose == SerialDetailedHuman);
            if (s == "1.#INF")
                s ="inf";
            out << s << '\n';
        }
    }

    return out.str();
}



std::string
ImageSpec::to_xml () const
{
    return spec_to_xml (*this, SerialDetailedHuman);
}



void
ImageSpec::from_xml (const char *xml)
{
    xml_document doc;
    doc.load (xml);
    xml_node n = doc.child ("ImageSpec");

    //int version = n.attribute ("version").as_int();

    // Fields for version == 10 (current)
    x = atoi (n.child_value ("x"));
    y = atoi (n.child_value ("y"));
    z = atoi (n.child_value ("z"));
    width = atoi (n.child_value ("width"));
    height = atoi (n.child_value ("height"));
    depth = atoi (n.child_value ("depth"));
    full_x = atoi (n.child_value ("full_x"));
    full_y = atoi (n.child_value ("full_y"));
    full_z = atoi (n.child_value ("full_z"));
    full_width = atoi (n.child_value ("full_width"));
    full_height = atoi (n.child_value ("full_height"));
    full_depth = atoi (n.child_value ("full_depth"));
    tile_width = atoi (n.child_value ("tile_width"));
    tile_height = atoi (n.child_value ("tile_height"));
    tile_depth = atoi (n.child_value ("tile_depth"));
    format = TypeDesc (n.child_value ("format"));
    nchannels  = atoi (n.child_value ("nchannels"));
    get_channelnames (n, channelnames);
    alpha_channel = atoi (n.child_value ("alpha_channel"));
    z_channel = atoi (n.child_value ("z_channel"));
    deep = atoi (n.child_value ("deep"));
    
    // FIXME: What about extra attributes?
    
    // If version == 11 {fill new fields}
}


OIIO_NAMESPACE_END
