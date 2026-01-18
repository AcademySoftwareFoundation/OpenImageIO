// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <libpng16/png.h>
#include <zlib.h>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/tiffutils.h>
#include <OpenImageIO/typedesc.h>

#include "imageio_pvt.h"

#define OIIO_LIBPNG_VERSION                                    \
    (PNG_LIBPNG_VER_MAJOR * 10000 + PNG_LIBPNG_VER_MINOR * 100 \
     + PNG_LIBPNG_VER_RELEASE)


/*
This code has been extracted from the PNG plugin so as to provide access to PNG
images embedded within any container format, without redundant code
duplication.

It's been done in the course of development of the ICO plugin in order to allow
reading and writing Vista-style PNG icons.

For further information see the following mailing list threads:
http://lists.openimageio.org/pipermail/oiio-dev-openimageio.org/2009-April/000586.html
http://lists.openimageio.org/pipermail/oiio-dev-openimageio.org/2009-April/000656.html
*/

OIIO_PLUGIN_NAMESPACE_BEGIN

#define ICC_PROFILE_ATTR "ICCProfile"

namespace PNG_pvt {

static void
rderr_handler(png_structp png, png_const_charp data)
{
    ImageInput* inp = (ImageInput*)png_get_error_ptr(png);
    if (inp && data)
        inp->errorfmt("PNG read error: {}", data);
}


static void
wrerr_handler(png_structp png, png_const_charp data)
{
    ImageOutput* outp = (ImageOutput*)png_get_error_ptr(png);
    if (outp && data)
        outp->errorfmt("PNG write error: {}", data);
}


static void
null_png_handler(png_structp /*png*/, png_const_charp /*data*/)
{
}



/// Initializes a PNG read struct.
/// \return empty string on success, error message on failure.
///
inline const std::string
create_read_struct(png_structp& sp, png_infop& ip, ImageInput* inp = nullptr)
{
    sp = png_create_read_struct(PNG_LIBPNG_VER_STRING, inp, rderr_handler,
                                null_png_handler);
    if (!sp)
        return "Could not create PNG read structure";

    png_set_error_fn(sp, inp, rderr_handler, null_png_handler);
    ip = png_create_info_struct(sp);
    if (!ip)
        return "Could not create PNG info structure";

    // Must call this setjmp in every function that does PNG reads
    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "PNG library error";

    // success
    return "";
}



/// Helper function - reads background colour.
///
inline bool
get_background(png_structp& sp, png_infop& ip, ImageSpec& spec, int& bit_depth,
               float* red, float* green, float* blue)
{
    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return false;
    if (!png_get_valid(sp, ip, PNG_INFO_bKGD))
        return false;

    png_color_16p bg;
    png_get_bKGD(sp, ip, &bg);
    if (spec.format == TypeDesc::UINT16) {
        *red   = bg->red / 65535.0;
        *green = bg->green / 65535.0;
        *blue  = bg->blue / 65535.0;
    } else if (spec.nchannels < 3 && bit_depth < 8) {
        if (bit_depth == 1)
            *red = *green = *blue = (bg->gray ? 1 : 0);
        else if (bit_depth == 2)
            *red = *green = *blue = bg->gray / 3.0;
        else  // 4 bits
            *red = *green = *blue = bg->gray / 15.0;
    } else {
        *red   = bg->red / 255.0;
        *green = bg->green / 255.0;
        *blue  = bg->blue / 255.0;
    }
    return true;
}



inline int
hex2int(char a)
{
    return a <= '9' ? a - '0' : tolower(a) - 'a' + 10;
}



// Recent libpng (>= 1.6.32) supports direct Exif chunks. But the old way
// is more common, which is to embed it in a Text field (like a comment).
// This decodes that raw text data, which is a string,that looks like:
//
//     <whitespace> exif
//     <whitespace> <integer size>
//     <72 hex digits>
//     ...more lines of 72 hex digits...
//
static bool
decode_png_text_exif(string_view raw, ImageSpec& spec)
{
    // Strutil::print("Found exif raw len={} '{}{}'\n", raw.size(),
    //                raw.substr(0,200), raw.size() > 200 ? "..." : "");

    Strutil::skip_whitespace(raw);
    if (!Strutil::parse_prefix(raw, "exif"))
        return false;
    int rawlen = 0;
    if (!Strutil::parse_int(raw, rawlen) || !rawlen)
        return false;
    Strutil::skip_whitespace(raw);
    std::string decoded;  // Converted from hex to bytes goes here
    decoded.reserve(raw.size() / 2 + 1);
    while (raw.size() >= 2) {
        if (!isxdigit(raw.front())) {  // not hex digit? skip
            raw.remove_prefix(1);
            continue;
        }
        int c = (hex2int(raw[0]) << 4) | hex2int(raw[1]);
        decoded.append(1, char(c));
        raw.remove_prefix(2);
    }
    if (Strutil::istarts_with(decoded, "Exif")) {
        decode_exif(decoded, spec);
    }
    return false;
}



/// Read information from a PNG file and fill the ImageSpec accordingly.
///
inline bool
read_info(png_structp& sp, png_infop& ip, int& bit_depth, int& color_type,
          int& interlace_type, Imath::Color3f& bg, ImageSpec& spec,
          bool keep_unassociated_alpha, string_view image_state_default)
{
    // Must call this setjmp in every function that does PNG reads
    if (setjmp(png_jmpbuf(sp))) {  // NOLINT(cert-err52-cpp)
        ImageInput* pnginput = (ImageInput*)png_get_io_ptr(sp);
        if (!pnginput->has_error())
            pnginput->errorfmt("Could not read info from file");
        return false;
    }

    bool ok = true;
    png_read_info(sp, ip);

    // Auto-convert 1-, 2-, and 4- bit images to 8 bits, palette to RGB,
    // and transparency to alpha.
    png_set_expand(sp);

    // PNG files are naturally big-endian
    if (littleendian())
        png_set_swap(sp);

    png_read_update_info(sp, ip);

    png_uint_32 width, height;
    ok &= (bool)png_get_IHDR(sp, ip, &width, &height, &bit_depth, &color_type,
                             nullptr, nullptr, nullptr);

    spec = ImageSpec((int)width, (int)height, png_get_channels(sp, ip),
                     bit_depth == 16 ? TypeDesc::UINT16 : TypeDesc::UINT8);

    spec.default_channel_names();
    if (spec.nchannels == 2) {
        // Special case: PNG spec says 2-channel image is Gray & Alpha
        spec.channelnames[0] = "Y";
        spec.channelnames[1] = "A";
        spec.alpha_channel   = 1;
    }

    int srgb_intent;
    double gamma = 0.0;
    if (png_get_sRGB(sp, ip, &srgb_intent)) {
        const bool erase_other_attributes = false;
        pvt::set_colorspace_srgb(spec, image_state_default,
                                 erase_other_attributes);
    } else if (png_get_gAMA(sp, ip, &gamma) && gamma > 0.0) {
        // Round gamma to the nearest hundredth to prevent stupid
        // precision choices and make it easier for apps to make
        // decisions based on known gamma values. For example, you want
        // 2.2, not 2.19998.
        float g = float(1.0 / gamma);
        g       = roundf(100.0f * g) / 100.0f;
        pvt::set_colorspace_rec709_gamma(spec, g, image_state_default);
    } else {
        // If there's no info at all, assume sRGB.
        const bool erase_other_attributes = false;
        pvt::set_colorspace_srgb(spec, image_state_default,
                                 erase_other_attributes);
    }

    if (png_get_valid(sp, ip, PNG_INFO_iCCP)) {
        png_charp profile_name     = nullptr;
        png_bytep profile_data     = nullptr;
        png_uint_32 profile_length = 0;
        int compression_type;
        png_get_iCCP(sp, ip, &profile_name, &compression_type, &profile_data,
                     &profile_length);
        if (profile_length && profile_data) {
            spec.attribute("ICCProfile",
                           TypeDesc(TypeDesc::UINT8, profile_length),
                           profile_data);
            std::string errormsg;
            bool ok
                = decode_icc_profile(make_cspan(profile_data, profile_length),
                                     spec, errormsg);
            if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
                errorfmt("Could not decode ICC profile: {}\n", errormsg);
                return false;
            }
        }
    }

    png_timep mod_time;
    if (png_get_tIME(sp, ip, &mod_time)) {
        std::string date
            = Strutil::fmt::format("{:4d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                                   mod_time->year, mod_time->month,
                                   mod_time->day, mod_time->hour,
                                   mod_time->minute, mod_time->second);
        spec.attribute("DateTime", date);
    }

    png_textp text_ptr;
    int num_comments = png_get_text(sp, ip, &text_ptr, NULL);
    for (int i = 0; i < num_comments; ++i) {
        if (Strutil::iequals(text_ptr[i].key, "Description"))
            spec.attribute("ImageDescription", text_ptr[i].text);
        else if (Strutil::iequals(text_ptr[i].key, "Author"))
            spec.attribute("Artist", text_ptr[i].text);
        else if (Strutil::iequals(text_ptr[i].key, "Title"))
            spec.attribute("DocumentName", text_ptr[i].text);
        else if (Strutil::iequals(text_ptr[i].key, "XML:com.adobe.xmp"))
            decode_xmp(text_ptr[i].text, spec);
        else if (Strutil::iequals(text_ptr[i].key, "Raw profile type exif")) {
            // Most PNG files seem to encode Exif by cramming it into a text
            // field, with the key "Raw profile type exif" and then a special
            // text encoding that we handle with the following function:
            decode_png_text_exif(text_ptr[i].text, spec);
        } else {
            spec.attribute(text_ptr[i].key, text_ptr[i].text);
        }
    }
    spec.x = png_get_x_offset_pixels(sp, ip);
    spec.y = png_get_y_offset_pixels(sp, ip);

    int unit;
    png_uint_32 resx, resy;
    if (png_get_pHYs(sp, ip, &resx, &resy, &unit)) {
        if (unit == PNG_RESOLUTION_METER) {
            // Convert to inches, to match most other formats
            float scale = 2.54f / 100.0f;
            float rx    = resx * scale;
            float ry    = resy * scale;
            // Round to nearest 0.1
            rx = std::round(10.0f * rx) / 10.0f;
            ry = std::round(10.0f * ry) / 10.0f;
            spec.attribute("ResolutionUnit", "inch");
            spec.attribute("XResolution", rx);
            spec.attribute("YResolution", ry);
        } else {
            spec.attribute("ResolutionUnit", "none");
            spec.attribute("XResolution", (float)resx);
            spec.attribute("YResolution", (float)resy);
        }
    }

    float aspect = (float)png_get_pixel_aspect_ratio(sp, ip);
    if (aspect != 0 && aspect != 1)
        spec.attribute("PixelAspectRatio", aspect);

    float r, g, b;
    if (get_background(sp, ip, spec, bit_depth, &r, &g, &b)) {
        bg = Imath::Color3f(r, g, b);
        // FIXME -- should we do anything with the background color?
    }

    interlace_type = png_get_interlace_type(sp, ip);

#ifdef PNG_cICP_SUPPORTED
    {
        png_byte pri = 0, trc = 0, mtx = 0, vfr = 0;
        if (png_get_cICP(sp, ip, &pri, &trc, &mtx, &vfr)) {
            const int cicp[4] = { pri, trc, mtx, vfr };
            pvt::set_colorspace_cicp(spec, cicp, image_state_default);
        }
    }
#endif

#ifdef PNG_eXIf_SUPPORTED
    // Recent version of PNG and libpng (>= 1.6.32, I think) have direct
    // support for Exif chunks. Older versions don't support it, and I'm not
    // sure how common it is. Most files use the old way, which is the
    // text embedding of Exif we handle with decode_png_text_exif.
    png_uint_32 num_exif = 0;
    png_bytep exif_data  = nullptr;
    if (png_get_eXIf_1(sp, ip, &num_exif, &exif_data)) {
        decode_exif(cspan<uint8_t>(exif_data, span_size_t(num_exif)), spec);
    }
#endif

    // PNG files are always "unassociated alpha" but we convert to associated
    // unless requested otherwise
    if (keep_unassociated_alpha)
        spec.attribute("oiio:UnassociatedAlpha", (int)1);

    // FIXME -- look for an XMP packet in an iTXt chunk.

    return ok;
}



/// Reads from an open PNG file into the indicated buffer.
/// \return empty string on success, error message on failure.
///
inline const std::string
read_into_buffer(png_structp& sp, png_infop& ip, ImageSpec& spec,
                 std::vector<unsigned char>& buffer)
{
    // Temp space for the row pointers. Must be declared before the setjmp
    // to ensure it's destroyed if the jump is taken.
    std::vector<unsigned char*> row_pointers(spec.height);

    // Must call this setjmp in every function that does PNG reads
    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "PNG library error";

#if 0
    // ?? This doesn't seem necessary, but I don't know why
    // Make the library handle fewer significant bits
    // png_color_8p sig_bit;
    // if (png_get_sBIT (sp, ip, &sig_bit)) {
    //        png_set_shift (sp, sig_bit);
    // }
#endif

    OIIO_DASSERT(spec.scanline_bytes() == png_get_rowbytes(sp, ip));
    buffer.resize(spec.image_bytes());
    for (int i = 0; i < spec.height; ++i)
        row_pointers[i] = buffer.data() + i * spec.scanline_bytes();

    png_read_image(sp, row_pointers.data());
    png_read_end(sp, NULL);

    // success
    return "";
}



/// Reads the next scanline from an open PNG file into the indicated buffer.
/// \return empty string on success, error message on failure.
///
inline const std::string
read_next_scanline(png_structp& sp, void* buffer)
{
    // Must call this setjmp in every function that does PNG reads
    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "PNG library error";

    png_read_row(sp, (png_bytep)buffer, NULL);

    // success
    return "";
}



/// Destroys a PNG read struct.
///
inline void
destroy_read_struct(png_structp& sp, png_infop& ip)
{
    if (sp && ip) {
        png_destroy_read_struct(&sp, &ip, NULL);
        sp = NULL;
        ip = NULL;
    }
}



/// Initializes a PNG write struct.
/// \return empty string on success, C-string error message on failure.
///
inline const std::string
create_write_struct(png_structp& sp, png_infop& ip, int& color_type,
                    ImageSpec& spec, ImageOutput* outp = nullptr)
{
    // Check for things this format doesn't support
    if (spec.width < 1 || spec.height < 1)
        return Strutil::fmt::format("Image resolution must be at least 1x1, "
                                    "you asked for {} x {}",
                                    spec.width, spec.height);
    if (spec.depth < 1)
        spec.depth = 1;
    if (spec.depth > 1)
        return "PNG does not support volume images (depth > 1)";

    switch (spec.nchannels) {
    case 1:
        color_type         = PNG_COLOR_TYPE_GRAY;
        spec.alpha_channel = -1;
        break;
    case 2:
        color_type         = PNG_COLOR_TYPE_GRAY_ALPHA;
        spec.alpha_channel = 1;
        break;
    case 3:
        color_type         = PNG_COLOR_TYPE_RGB;
        spec.alpha_channel = -1;
        break;
    case 4:
        color_type         = PNG_COLOR_TYPE_RGB_ALPHA;
        spec.alpha_channel = 3;
        break;
    default:
        return Strutil::fmt::format("PNG only supports 1-4 channels, not {}",
                                    spec.nchannels);
    }
    // N.B. PNG is very rigid about the meaning of the channels, so enforce
    // which channel is alpha, that's the only way PNG can do it.

    sp = png_create_write_struct(PNG_LIBPNG_VER_STRING, outp, wrerr_handler,
                                 null_png_handler);
    if (!sp)
        return "Could not create PNG write structure";

    ip = png_create_info_struct(sp);
    if (!ip)
        return "Could not create PNG info structure";

    // Must call this setjmp in every function that does PNG writes
    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "PNG library error";

    // success
    return "";
}



/// Helper function - writes a single parameter.
///
inline bool
put_parameter(png_structp& sp, png_infop& ip, const std::string& _name,
              TypeDesc type, const void* data, std::vector<png_text>& text)
{
    std::string name = _name;

    // Things to skip
    if (Strutil::iequals(name, "planarconfig"))  // No choice for PNG files
        return false;
    if (Strutil::iequals(name, "compression"))
        return false;
    if (Strutil::iequals(name, "ResolutionUnit")
        || Strutil::iequals(name, "XResolution")
        || Strutil::iequals(name, "YResolution"))
        return false;

    // Remap some names to PNG conventions
    if (Strutil::iequals(name, "Artist") && type == TypeDesc::STRING)
        name = "Author";
    if ((Strutil::iequals(name, "name")
         || Strutil::iequals(name, "DocumentName"))
        && type == TypeDesc::STRING)
        name = "Title";
    if ((Strutil::iequals(name, "description")
         || Strutil::iequals(name, "ImageDescription"))
        && type == TypeDesc::STRING)
        name = "Description";

    if (Strutil::iequals(name, "DateTime") && type == TypeDesc::STRING) {
        png_time mod_time;
        int year, month, day, hour, minute, second;
        if (Strutil::scan_datetime(*(const char**)data, year, month, day, hour,
                                   minute, second)) {
            mod_time.year   = year;
            mod_time.month  = month;
            mod_time.day    = day;
            mod_time.hour   = hour;
            mod_time.minute = minute;
            mod_time.second = second;
            png_set_tIME(sp, ip, &mod_time);
            return true;
        } else {
            return false;
        }
    }

#if 0
    if (Strutil::iequals(name, "ResolutionUnit") && type == TypeDesc::STRING) {
        const char *s = *(char**)data;
        bool ok = true;
        if (Strutil::iequals (s, "none"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_NONE);
        else if (Strutil::iequals (s, "in") || Strutil::iequals (s, "inch"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_INCH);
        else if (Strutil::iequals (s, "cm"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        else ok = false;
        return ok;
    }
    if (Strutil::iequals(name, "ResolutionUnit") && type == TypeDesc::UINT) {
        PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, *(unsigned int *)data);
        return true;
    }
    if (Strutil::iequals(name, "XResolution") && type == TypeDesc::FLOAT) {
        PNGSetField (m_tif, PNGTAG_XRESOLUTION, *(float *)data);
        return true;
    }
    if (Strutil::iequals(name, "YResolution") && type == TypeDesc::FLOAT) {
        PNGSetField (m_tif, PNGTAG_YRESOLUTION, *(float *)data);
        return true;
    }
#endif

    // Before handling general named metadata, suppress format-specific
    // metadata hints meant for other formats that are not meant to be literal
    // metadata written to the file. This includes anything with a namespace
    // prefix of "oiio:" or the name of any other file format.
    auto colonpos = name.find(':');
    if (colonpos != std::string::npos) {
        std::string prefix = Strutil::lower(name.substr(0, colonpos));
        if (prefix != "png" && is_imageio_format_name(prefix))
            return false;
        if (prefix == "oiio")
            return false;
    }

    if (type == TypeDesc::STRING) {
        // We can save arbitrary string metadata in multiple png text entries.
        // Is that ok? Should we also do it for other types by converting to
        // string?
        png_text t;
        t.compression = PNG_TEXT_COMPRESSION_NONE;
        t.key         = (char*)ustring(name).c_str();
        t.text        = *(char**)data;  // Already uniquified
        text.push_back(t);
        return true;
    }

    return false;
}



/// Writes PNG header according to the ImageSpec.
/// \return empty string on success, error message on failure.
///
inline const std::string
write_info(png_structp& sp, png_infop& ip, int& color_type, ImageSpec& spec,
           std::vector<png_text>& text, bool& convert_alpha, bool& srgb,
           float& gamma)
{
    // Force either 16 or 8 bit integers
    if (spec.format == TypeDesc::UINT8 || spec.format == TypeDesc::INT8)
        spec.set_format(TypeDesc::UINT8);
    else
        spec.set_format(TypeDesc::UINT16);  // best precision available

    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "Could not set PNG IHDR chunk";
    png_set_IHDR(sp, ip, spec.width, spec.height, spec.format.size() * 8,
                 color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
        return "Could not set PNG oFFs chunk";
    png_set_oFFs(sp, ip, spec.x, spec.y, PNG_OFFSET_PIXEL);

    // PNG specifically dictates unassociated (un-"premultiplied") alpha
    convert_alpha = spec.alpha_channel != -1
                    && !spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    OIIO_MAYBE_UNUSED bool wrote_colorspace = false;
    srgb                                    = false;
    if (pvt::is_colorspace_srgb(spec)) {
        gamma = 1.0f;
        srgb  = true;
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA and cHRM chunk";
        png_set_sRGB_gAMA_and_cHRM(sp, ip, PNG_sRGB_INTENT_ABSOLUTE);
        wrote_colorspace = true;
    } else {
        gamma = pvt::get_colorspace_rec709_gamma(spec);
        if (gamma != 0.0f) {
            if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
                return "Could not set PNG gAMA chunk";
            png_set_gAMA(sp, ip, 1.0 / gamma);
            srgb             = false;
            wrote_colorspace = true;
        }
    }

    // Write ICC profile, if we have anything
    std::vector<uint8_t> icc_profile = pvt::get_colorspace_icc_profile(spec);
    if (icc_profile.size()) {
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG iCCP chunk";
        png_set_iCCP(sp, ip, "Embedded Profile", 0, icc_profile.data(),
                     icc_profile.size());
        wrote_colorspace = true;
    }

    if (false && !spec.find_attribute("DateTime")) {
        time_t now;
        time(&now);
        struct tm mytm;
        Sysutil::get_local_time(&now, &mytm);
        std::string date
            = Strutil::fmt::format("{:4d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                                   mytm.tm_year + 1900, mytm.tm_mon + 1,
                                   mytm.tm_mday, mytm.tm_hour, mytm.tm_min,
                                   mytm.tm_sec);
        spec.attribute("DateTime", date);
    }

    string_view unitname = spec.get_string_attribute("ResolutionUnit");
    float xres           = spec.get_float_attribute("XResolution");
    float yres           = spec.get_float_attribute("YResolution");
    float paspect        = spec.get_float_attribute("PixelAspectRatio");
    if (xres || yres || paspect || unitname.size()) {
        int unittype = PNG_RESOLUTION_UNKNOWN;
        float scale  = 1;
        if (Strutil::iequals(unitname, "meter")
            || Strutil::iequals(unitname, "m"))
            unittype = PNG_RESOLUTION_METER;
        else if (Strutil::iequals(unitname, "cm")) {
            unittype = PNG_RESOLUTION_METER;
            scale    = 100;
        } else if (Strutil::iequals(unitname, "inch")
                   || Strutil::iequals(unitname, "in")) {
            unittype = PNG_RESOLUTION_METER;
            scale    = 100.0 / 2.54;
        }
        if (paspect) {
            // If pixel aspect is given, allow resolution to be reset
            if (xres)
                yres = 0.0f;
            else
                xres = 0.0f;
        }
        if (xres == 0.0f && yres == 0.0f) {
            xres = 100.0f;
            yres = xres * (paspect ? paspect : 1.0f);
        } else if (xres == 0.0f) {
            xres = yres / (paspect ? paspect : 1.0f);
        } else if (yres == 0.0f) {
            yres = xres * (paspect ? paspect : 1.0f);
        }
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG pHYs chunk";
        png_set_pHYs(sp, ip, (png_uint_32)(xres * scale),
                     (png_uint_32)(yres * scale), unittype);
    }

#ifdef PNG_cICP_SUPPORTED
    // Only automatically determine CICP from oiio::ColorSpace if we didn't
    // write colorspace metadata yet.
    cspan<int> cicp = pvt::get_colorspace_cicp(spec, !wrote_colorspace);
    if (!cicp.empty()) {
        png_byte vals[4];
        for (int i = 0; i < 4; ++i)
            vals[i] = static_cast<png_byte>(cicp[i]);
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG cICP chunk";
        // libpng will only write the chunk if the third byte is 0
        png_set_cICP(sp, ip, vals[0], vals[1], (png_byte)0, vals[3]);
    }
#endif

#ifdef PNG_eXIf_SUPPORTED
    std::vector<char> exifBlob;
    encode_exif(spec, exifBlob, endian::big);
    png_set_eXIf_1(sp, ip, static_cast<png_uint_32>(exifBlob.size()),
                   reinterpret_cast<png_bytep>(exifBlob.data()));
#endif

    // Deal with all other params
    for (size_t p = 0; p < spec.extra_attribs.size(); ++p)
        put_parameter(sp, ip, spec.extra_attribs[p].name().string(),
                      spec.extra_attribs[p].type(),
                      spec.extra_attribs[p].data(), text);

    if (text.size())
        png_set_text(sp, ip, &text[0], text.size());

    png_write_info(sp, ip);
    png_set_packing(sp);  // Pack 1, 2, 4 bit into bytes

    return "";
}



/// Writes a scanline.
///
inline bool
write_row(png_structp& sp, png_byte* data)
{
    if (setjmp(png_jmpbuf(sp))) {  // NOLINT(cert-err52-cpp)
        //error ("PNG library error");
        return false;
    }
    png_write_row(sp, data);
    return true;
}



/// Write scanlines
inline bool
write_rows(png_structp& sp, png_byte* data, int nrows = 0, stride_t ystride = 0)
{
    if (setjmp(png_jmpbuf(sp))) {  // NOLINT(cert-err52-cpp)
        //error ("PNG library error");
        return false;
    }
    if (nrows == 1) {
        png_write_row(sp, data);
    } else {
        png_byte** ptrs = OIIO_ALLOCA(png_byte*, nrows);
        for (int i = 0; i < nrows; ++i)
            ptrs[i] = data + i * ystride;
        png_write_rows(sp, ptrs, png_uint_32(nrows));
    }
    return true;
}



/// Helper function - error-catching wrapper for png_write_end
inline void
write_end(png_structp& sp, png_infop& ip)
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp(png_jmpbuf(sp))) {  // NOLINT(cert-err52-cpp)
        return;
    }
    png_write_end(sp, ip);
}


/// Helper function - error-catching wrapper for png_destroy_write_struct
inline void
destroy_write_struct(png_structp& sp, png_infop& ip)
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp(png_jmpbuf(sp))) {  // NOLINT(cert-err52-cpp)
        return;
    }
    png_destroy_write_struct(&sp, &ip);
}


}  // namespace PNG_pvt

OIIO_PLUGIN_NAMESPACE_END
