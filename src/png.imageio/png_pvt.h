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

#include "color_pvt.h"
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
#define CICP_ATTR "CICP"

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
        return decode_exif(decoded, spec);
    }
    return false;
}



/// Read information from a PNG file and fill the ImageSpec accordingly.
///
inline bool
read_info(png_structp& sp, png_infop& ip, int& bit_depth, int& color_type,
          int& interlace_type, Imath::Color3f& bg, ImageSpec& spec,
          bool keep_unassociated_alpha, const ImageSpec* config_hints,
          string_view filename = {})
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
        spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
    } else if (png_get_gAMA(sp, ip, &gamma) && gamma > 0.0) {
        float g = float(1.0 / gamma);
        set_colorspace_rec709_gamma(spec, g);
    } else {
        // If there's no info at all, assume sRGB.
        spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
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
                ImageInput* pnginput = (ImageInput*)png_get_io_ptr(sp);
                pnginput->errorfmt("Could not decode ICC profile: {}\n",
                                   errormsg);
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
            bool ok = decode_png_text_exif(text_ptr[i].text, spec);
            if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
                ImageInput* pnginput = (ImageInput*)png_get_io_ptr(sp);
                pnginput->errorfmt("Could not decode Exif");
                return false;
            }
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
            spec.attribute(CICP_ATTR, TypeDesc(TypeDesc::INT, 4), cicp);
            // The CICP -> color-space override is applied centrally below.
        }
    }
#endif

#ifdef PNG_mDCV_SUPPORTED
    // mDCV (SMPTE ST 2086) -> oicio's ST 2086 integer wire keys (spec 34):
    // chromaticity xy x50000, luminance x10000, min floored at 1. libpng
    // hands back WHITE-FIRST doubles; we reorder to the R,G,B,W attribute set.
    {
        double wx, wy, rx, ry, gx, gy, bx, by, maxl, minl;
        if (png_get_mDCV(sp, ip, &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by, &maxl,
                         &minl)) {
            spec.attribute("mdcv_red_x", (int)std::lround(rx * 50000.0));
            spec.attribute("mdcv_red_y", (int)std::lround(ry * 50000.0));
            spec.attribute("mdcv_green_x", (int)std::lround(gx * 50000.0));
            spec.attribute("mdcv_green_y", (int)std::lround(gy * 50000.0));
            spec.attribute("mdcv_blue_x", (int)std::lround(bx * 50000.0));
            spec.attribute("mdcv_blue_y", (int)std::lround(by * 50000.0));
            spec.attribute("mdcv_white_x", (int)std::lround(wx * 50000.0));
            spec.attribute("mdcv_white_y", (int)std::lround(wy * 50000.0));
            spec.attribute("mdcv_max_luminance",
                           (int)std::lround(maxl * 10000.0));
            int64_t mn = (int64_t)std::lround(minl * 10000.0);
            spec.attribute("mdcv_min_luminance", (int)(mn < 1 ? 1 : mn));
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
        bool ok = decode_exif(cspan<uint8_t>(exif_data, span_size_t(num_exif)),
                              spec);
        if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
            ImageInput* pnginput = (ImageInput*)png_get_io_ptr(sp);
            pnginput->errorfmt("Could not decode Exif");
            return false;
        }
    }
#endif

    // PNG files are always "unassociated alpha" but we convert to associated
    // unless requested otherwise
    if (keep_unassociated_alpha)
        spec.attribute("oiio:UnassociatedAlpha", (int)1);

    // FIXME -- look for an XMP packet in an iTXt chunk.

    // Hand the raw color attributes just deposited to the central color-
    // metadata reconciler, which applies the audited precedence cascade
    // (replacing this reader's former inline CICP -> color-space override).
    // Per-open config hints (if any) override the global policy tier. With
    // policy at its defaults the resolved color space is identical.
    pvt::reconcile_color_metadata(
        spec,
        pvt::ColorReadPolicy::snapshot(config_hints,
                                       pvt::ambient_color_config(), filename),
        "png");

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
           float& gamma, string_view filename = {})
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

    string_view colorspace = spec.get_string_attribute("oiio:ColorSpace",
                                                       "srgb_rec709_scene");
    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    OIIO_MAYBE_UNUSED bool wrote_colorspace = false;
    srgb                                    = false;
    if (colorconfig.equivalent(colorspace, "srgb_rec709_scene")) {
        srgb  = true;
        gamma = 1.0f;
    } else if (colorconfig.equivalent(colorspace, "g22_rec709_scene")) {
        gamma = 2.2f;
    } else if (colorconfig.equivalent(colorspace, "g24_rec709_scene")) {
        gamma = 2.4f;
    } else if (colorconfig.equivalent(colorspace, "g18_rec709_scene")) {
        gamma = 1.8f;
    } else {
        gamma = spec.get_float_attribute("oiio:Gamma", 1.0f);
        // obsolete "oiio:Gamma" attrib for back compatibility
    }

    if (colorconfig.equivalent(colorspace, "scene_linear")
        || colorconfig.equivalent(colorspace, "lin_rec709_scene")) {
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA chunk";
        png_set_gAMA(sp, ip, 1.0);
        srgb             = false;
        wrote_colorspace = true;
    } else if (Strutil::istarts_with(colorspace, "Gamma")) {
        // Back compatible, this is DEPRECATED(3.1)
        Strutil::parse_word(colorspace);
        float g = Strutil::from_string<float>(colorspace);
        if (g >= 0.01f && g <= 10.0f /* sanity check */)
            gamma = g;
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA chunk";
        png_set_gAMA(sp, ip, 1.0f / gamma);
        srgb             = false;
        wrote_colorspace = true;
    } else if (colorconfig.equivalent(colorspace, "g22_rec709_scene")) {
        gamma = 2.2f;
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA chunk";
        png_set_gAMA(sp, ip, 1.0f / gamma);
        srgb             = false;
        wrote_colorspace = true;
    } else if (colorconfig.equivalent(colorspace, "g18_rec709_scene")) {
        gamma = 1.8f;
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA chunk";
        png_set_gAMA(sp, ip, 1.0f / gamma);
        srgb             = false;
        wrote_colorspace = true;
    } else if (colorconfig.equivalent(colorspace, "srgb_rec709_scene")) {
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG gAMA and cHRM chunk";
        png_set_sRGB_gAMA_and_cHRM(sp, ip, PNG_sRGB_INTENT_ABSOLUTE);
        srgb             = true;
        wrote_colorspace = true;
    }

    // Write ICC profile, if we have anything
    const ParamValue* icc_profile_parameter = spec.find_attribute(
        ICC_PROFILE_ATTR);
    if (icc_profile_parameter != nullptr) {
        unsigned int length = icc_profile_parameter->type().size();
        if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
            return "Could not set PNG iCCP chunk";
        unsigned char* icc_profile
            = (unsigned char*)icc_profile_parameter->data();
        if (icc_profile && length) {
            png_set_iCCP(sp, ip, "Embedded Profile", 0, icc_profile, length);
            wrote_colorspace = true;
        }
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

    // Central write plan (spec 09): consume it instead of deriving inline.
    // Drives the CICP chunk (below) and, under the verbose policy, the
    // redundant cHRM/gAMA chunks (Feature 2).
    const pvt::ColorMetadataPlan color_plan = pvt::plan_color_metadata(
        nullptr, spec, pvt::color_write_caps_for_format("png"),
        pvt::ColorWritePolicy::snapshot(&spec, pvt::ambient_color_config(),
                                        filename));

#ifdef PNG_cICP_SUPPORTED
    // CICP: the plan emits an author-supplied CICP tuple verbatim, or one
    // derived from the color space. PNG only auto-derives a tuple when no other
    // color-space chunk was already written; an explicitly authored tuple is
    // always emitted.
    {
        const auto& cicp = color_plan.cicp;
        const bool emit  = cicp.action == pvt::ColorPlanAction::Write
                          || (cicp.action == pvt::ColorPlanAction::Derive
                              && !wrote_colorspace);
        if (emit && cicp.ints.size() == 4) {
            png_byte vals[4];
            for (int i = 0; i < 4; ++i)
                vals[i] = static_cast<png_byte>(cicp.ints[i]);
            if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
                return "Could not set PNG cICP chunk";
            // libpng will only write the chunk if the third byte is 0
            png_set_cICP(sp, ip, vals[0], vals[1], (png_byte)0, vals[3]);
        }
    }
#endif

    // Feature 2 (spec 09): verbose/redundant emission. When the plan derives a
    // chromaticities set and/or a pure-gamma value for a space that did not
    // already write a color-space chunk, emit the redundant cHRM/gAMA chunks so
    // every consumer finds a signal it understands. `floats` is R,G,B,W (x,y).
    if (!wrote_colorspace) {
        const auto& chrm = color_plan.chromaticities;
        if (chrm.emit() && chrm.floats.size() == 8) {
            if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
                return "Could not set PNG cHRM chunk";
            png_set_cHRM(sp, ip, chrm.floats[6], chrm.floats[7],  // white x,y
                         chrm.floats[0], chrm.floats[1],          // red x,y
                         chrm.floats[2], chrm.floats[3],          // green x,y
                         chrm.floats[4], chrm.floats[5]);         // blue x,y
        }
        const auto& g = color_plan.gamma;
        if (g.emit() && g.gamma > 0.0f) {
            if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
                return "Could not set PNG gAMA chunk";
            png_set_gAMA(sp, ip, 1.0f / g.gamma);  // PNG stores file gamma
        }
    }

#ifdef PNG_mDCV_SUPPORTED
    // mDCV (SMPTE ST 2086 mastering-display colour volume). Adopt the sibling
    // oicio project's exact wire keys (oicio spec 34, "Wire metadata keys"):
    //   mdcv_{red,green,blue,white}_{x,y} = chromaticity xy scaled x50000
    //   mdcv_{max,min}_luminance          = cd/m2 scaled x10000 (min floored 1)
    // Source priority: explicit mdcv_* ImageSpec attributes win; otherwise,
    // under the broadcast policy, bridge the plan's derived RGBW primaries
    // (color_plan.mdcv.floats, a flat R,G,B,W xy float[8]) into a volume.
    // libpng wants WHITE-FIRST doubles in [0,1] xy and nits luminance, so we
    // reorder RGBW->WRGB and unscale (xy/50000, luminance/10000).
    {
        static const char* xykeys[8]
            = { "mdcv_red_x",  "mdcv_red_y",  "mdcv_green_x", "mdcv_green_y",
                "mdcv_blue_x", "mdcv_blue_y", "mdcv_white_x", "mdcv_white_y" };
        int64_t xy[8];
        bool have_xy = true;
        for (int i = 0; i < 8; ++i) {
            if (spec.find_attribute(xykeys[i]))
                xy[i] = spec.get_int_attribute(xykeys[i]);
            else {
                have_xy = false;
                break;
            }
        }
        // Bridge the broadcast-derived primaries when no explicit xy present.
        const auto& md = color_plan.mdcv;
        if (!have_xy && md.emit() && md.floats.size() == 8) {
            for (int i = 0; i < 8; ++i)
                xy[i] = (int64_t)std::lround(md.floats[i] * 50000.0);
            have_xy = true;
        }
        // ponytail: no photometric luminance probe here. oicio derives peak/
        // black via a probe ladder (spec 34 tiers 2-4) not ported to OIIO; the
        // interim is supplied-or-default. Default = P3 broadcast mastering
        // display: 1000 nits peak, 0.0001 nit black (ST2086 min floor of 1).
        // Override via the mdcv_max_luminance / mdcv_min_luminance attributes.
        int64_t maxlum = spec.get_int_attribute("mdcv_max_luminance", 10000000);
        int64_t minlum = spec.get_int_attribute("mdcv_min_luminance", 1);
        if (have_xy) {
            if (setjmp(png_jmpbuf(sp)))  // NOLINT(cert-err52-cpp)
                return "Could not set PNG mDCV chunk";
            png_set_mDCV(sp, ip, xy[6] / 50000.0, xy[7] / 50000.0,  // white x,y
                         xy[0] / 50000.0, xy[1] / 50000.0,          // red x,y
                         xy[2] / 50000.0, xy[3] / 50000.0,          // green x,y
                         xy[4] / 50000.0, xy[5] / 50000.0,          // blue x,y
                         maxlum / 10000.0, minlum / 10000.0);
        }
    }
#endif

#ifdef PNG_eXIf_SUPPORTED
    std::vector<char> exifBlob;
    encode_exif(spec, exifBlob, endian::big);
    png_set_eXIf_1(sp, ip, static_cast<png_uint_32>(exifBlob.size()),
                   reinterpret_cast<png_bytep>(exifBlob.data()));
#endif

    // Feature 1 (spec 09): PNG has no native colorInteropID slot. Under the
    // force_interop_id policy, stamp the derived id so the tEXt emission below
    // carries it; otherwise strip it so the file stays untagged. Must run
    // BEFORE the generic loop -- that loop would otherwise emit an authored id
    // as a tEXt chunk verbatim, ignoring write:interop_id=never.
    pvt::apply_forced_interop_id(spec, "png", filename);

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
