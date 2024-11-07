// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cassert>
#include <cstdio>
#include <set>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include "jpeg_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)


// References:
//  * JPEG library documentation: /usr/share/doc/libjpeg-devel-6b
//  * JFIF spec: https://www.w3.org/Graphics/JPEG/jfif3.pdf
//  * ITU T.871 (aka ISO/IEC 10918-5):
//      https://www.itu.int/rec/T-REC-T.871-201105-I/en



class JpgOutput final : public ImageOutput {
public:
    JpgOutput() { init(); }
    ~JpgOutput() override { close(); }
    const char* format_name(void) const override { return "jpeg"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "iptc" || feature == "ioproxy");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;
    bool copy_image(ImageInput* in) override;

private:
    std::string m_filename;
    unsigned int m_dither;
    int m_next_scanline;  // Which scanline is the next to write?
    std::vector<unsigned char> m_scratch;
    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr c_jerr;
    jvirt_barray_ptr* m_copy_coeffs;
    struct jpeg_decompress_struct* m_copy_decompressor;
    std::vector<unsigned char> m_tilebuffer;
    // m_outbuffer/m_outsize are used for jpeg-to-memory
    unsigned char* m_outbuffer = nullptr;
#if OIIO_JPEG_LIB_VERSION >= 94
    // libjpeg switched jpeg_mem_dest() from accepting a `unsigned long*`
    // to a `size_t*` in version 9d.
    size_t m_outsize = 0;
#else
    // libjpeg < 9d, and so far all libjpeg-turbo releases, have a
    // jpeg_mem_dest() declaration that needs this to be unsigned long.
    unsigned long m_outsize = 0;
#endif

    void init(void)
    {
        m_copy_coeffs       = NULL;
        m_copy_decompressor = NULL;
        ioproxy_clear();
        clear_outbuffer();
    }

    void clear_outbuffer()
    {
        if (m_outbuffer) {
            free(m_outbuffer);
            m_outbuffer = nullptr;
        }
        m_outsize = 0;
    }

    void set_subsampling(const int components[])
    {
        jpeg_set_colorspace(&m_cinfo, JCS_YCbCr);
        m_cinfo.comp_info[0].h_samp_factor = components[0];
        m_cinfo.comp_info[0].v_samp_factor = components[1];
        m_cinfo.comp_info[1].h_samp_factor = components[2];
        m_cinfo.comp_info[1].v_samp_factor = components[3];
        m_cinfo.comp_info[2].h_samp_factor = components[4];
        m_cinfo.comp_info[2].v_samp_factor = components[5];
    }

    // Read the XResolution/YResolution and PixelAspectRatio metadata, store
    // in density fields m_cinfo.X_density,Y_density.
    void resmeta_to_density();
};



OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
jpeg_output_imageio_create()
{
    return new JpgOutput;
}

OIIO_EXPORT const char* jpeg_output_extensions[]
    = { "jpg", "jpe", "jpeg", "jif", "jfif", "jfi", nullptr };

OIIO_PLUGIN_EXPORTS_END



static std::set<std::string> metadata_include { "oiio:ConstantColor",
                                                "oiio:AverageColor",
                                                "oiio:SHA-1" };
static std::set<std::string> metadata_exclude {
    "XResolution",    "YResolution", "PixelAspectRatio",
    "ResolutionUnit", "Orientation", "ImageDescription"
};

bool
JpgOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    // Save name and spec for later use
    m_filename = name;

    if (!check_open(mode, newspec,
                    { 0, JPEG_MAX_DIMENSION, 0, JPEG_MAX_DIMENSION, 0, 1, 0,
                      256 }))
        return false;
    // NOTE: we appear to let a large number of channels be allowed, but
    // that's only because we robustly truncate to only RGB no matter what we
    // are handed.

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    m_cinfo.err = jpeg_std_error(&c_jerr);  // set error handler
    jpeg_create_compress(&m_cinfo);         // create compressor
    Filesystem::IOProxy* m_io = ioproxy();
    if (!strcmp(m_io->proxytype(), "file")) {
        auto fd = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        jpeg_stdio_dest(&m_cinfo, fd);  // set output stream
    } else {
        clear_outbuffer();
        jpeg_mem_dest(&m_cinfo, &m_outbuffer, &m_outsize);
    }

    // Set image and compression parameters
    m_cinfo.image_width  = m_spec.width;
    m_cinfo.image_height = m_spec.height;

    // JFIF can only handle grayscale and RGB. Do the best we can with this
    // limited format by switching to 1 or 3 channels.
    if (m_spec.nchannels >= 3) {
        // For 3 or more channels, write the first 3 as RGB and drop any
        // additional channels.
        m_cinfo.input_components = 3;
        m_cinfo.in_color_space   = JCS_RGB;
    } else if (m_spec.nchannels == 2) {
        // Two channels are tricky. If the first channel name is "Y", assume
        // it's a luminance image and write it as a single-channel grayscale.
        // Otherwise, punt, write it as an RGB image with third channel black.
        if (m_spec.channel_name(0) == "Y") {
            m_cinfo.input_components = 1;
            m_cinfo.in_color_space   = JCS_GRAYSCALE;
        } else {
            m_cinfo.input_components = 3;
            m_cinfo.in_color_space   = JCS_RGB;
        }
    } else {
        // One channel, assume it's grayscale
        m_cinfo.input_components = 1;
        m_cinfo.in_color_space   = JCS_GRAYSCALE;
    }

    resmeta_to_density();

    m_cinfo.write_JFIF_header = TRUE;

    if (m_copy_coeffs) {
        // Back door for copy()
        jpeg_copy_critical_parameters(m_copy_decompressor, &m_cinfo);
        DBG std::cout << "out open: copy_critical_parameters\n";
        jpeg_write_coefficients(&m_cinfo, m_copy_coeffs);
        DBG std::cout << "out open: write_coefficients\n";
    } else {
        // normal write of scanlines
        jpeg_set_defaults(&m_cinfo);  // default compression
        // Careful -- jpeg_set_defaults overwrites density
        resmeta_to_density();
        DBG std::cout << "out open: set_defaults\n";

        auto compqual = m_spec.decode_compression_metadata("jpeg", 98);
        if (Strutil::iequals(compqual.first, "jpeg"))
            jpeg_set_quality(&m_cinfo, clamp(compqual.second, 1, 100), TRUE);
        else
            jpeg_set_quality(&m_cinfo, 98, TRUE);  // not jpeg? default qual

        if (m_cinfo.input_components == 3) {
            std::string subsampling = m_spec.get_string_attribute(
                JPEG_SUBSAMPLING_ATTR);
            if (subsampling == JPEG_444_STR)
                set_subsampling(JPEG_444_COMP);
            else if (subsampling == JPEG_422_STR)
                set_subsampling(JPEG_422_COMP);
            else if (subsampling == JPEG_420_STR)
                set_subsampling(JPEG_420_COMP);
            else if (subsampling == JPEG_411_STR)
                set_subsampling(JPEG_411_COMP);
        }
        DBG std::cout << "out open: set_colorspace\n";

        // Save as a progressive jpeg if requested by the user
        if (m_spec.get_int_attribute("jpeg:progressive")) {
            jpeg_simple_progression(&m_cinfo);
        }

        jpeg_start_compress(&m_cinfo, TRUE);  // start working
        DBG std::cout << "out open: start_compress\n";
    }
    m_next_scanline = 0;  // next scanline we'll write

    // Write JPEG comment, if sent an 'ImageDescription'
    std::string comment = m_spec.get_string_attribute("ImageDescription");
    if (comment.size()) {
        jpeg_write_marker(&m_cinfo, JPEG_COM, (JOCTET*)comment.c_str(),
                          comment.size() + 1);
    }

    // Write other metadata as JPEG comments if requested
    if (m_spec.get_int_attribute("jpeg:com_attributes")) {
        for (const auto& p : m_spec.extra_attribs) {
            std::string name = p.name().string();
            auto colon       = name.find(':');
            if (metadata_include.count(name)) {
                // Allow explicitly included metadata
            } else if (metadata_exclude.count(name))
                continue;  // Suppress metadata that is processed separately
            else if (Strutil::istarts_with(name, "ICCProfile"))
                continue;  // Suppress ICC profile, gets written separately
            else if (colon != ustring::npos) {
                auto prefix = p.name().substr(0, colon);
                if (Strutil::iequals(prefix, "oiio"))
                    continue;  // Suppress internal metadata
                else if (Strutil::iequals(prefix, "exif")
                         || Strutil::iequals(prefix, "GPS")
                         || Strutil::iequals(prefix, "XMP"))
                    continue;  // Suppress EXIF metadata, gets written separately
                else if (Strutil::iequals(prefix, "iptc"))
                    continue;  // Suppress IPTC metadata
                else if (is_imageio_format_name(prefix))
                    continue;  // Suppress format-specific metadata
            }
            auto data = p.name().string() + ":" + p.get_string();
            jpeg_write_marker(&m_cinfo, JPEG_COM, (JOCTET*)data.c_str(),
                              data.size());
        }
    }

    if (equivalent_colorspace(m_spec.get_string_attribute("oiio:ColorSpace"),
                              "sRGB"))
        m_spec.attribute("Exif:ColorSpace", 1);

    // Write EXIF info
    std::vector<char> exif;
    // Start the blob with "Exif" and two nulls.  That's how it
    // always is in the JPEG files I've examined.
    exif.push_back('E');
    exif.push_back('x');
    exif.push_back('i');
    exif.push_back('f');
    exif.push_back(0);
    exif.push_back(0);
    encode_exif(m_spec, exif);
    jpeg_write_marker(&m_cinfo, JPEG_APP0 + 1, (JOCTET*)exif.data(),
                      exif.size());

    // Write IPTC IIM metadata tags, if we have anything
    std::vector<char> iptc;
    if (m_spec.get_int_attribute("jpeg:iptc", 1)
        && encode_iptc_iim(m_spec, iptc)) {
        static char photoshop[] = "Photoshop 3.0";
        std::vector<char> head(photoshop, photoshop + strlen(photoshop) + 1);
        static char _8BIM[] = "8BIM";
        head.insert(head.end(), _8BIM, _8BIM + 4);
        head.push_back(4);  // 0x0404
        head.push_back(4);
        head.push_back(0);  // four bytes of zeroes
        head.push_back(0);
        head.push_back(0);
        head.push_back(0);
        head.push_back((char)(iptc.size() >> 8));  // size of block
        head.push_back((char)(iptc.size() & 0xff));
        iptc.insert(iptc.begin(), head.begin(), head.end());
        jpeg_write_marker(&m_cinfo, JPEG_APP0 + 13, (JOCTET*)iptc.data(),
                          iptc.size());
    }

    // Write XMP packet, if we have anything
    std::string xmp = encode_xmp(m_spec, true);
    if (!xmp.empty()) {
        static char prefix[] = "http://ns.adobe.com/xap/1.0/";  //NOSONAR
        std::vector<char> block(prefix, prefix + strlen(prefix) + 1);
        block.insert(block.end(), xmp.c_str(), xmp.c_str() + xmp.length());
        jpeg_write_marker(&m_cinfo, JPEG_APP0 + 1, (JOCTET*)&block[0],
                          block.size());
    }

    m_spec.set_format(TypeDesc::UINT8);  // JPG is only 8 bit

    // Write ICC profile, if we have anything
    if (auto icc_profile_parameter = m_spec.find_attribute(ICC_PROFILE_ATTR)) {
        cspan<unsigned char> icc_profile((unsigned char*)
                                             icc_profile_parameter->data(),
                                         icc_profile_parameter->type().size());
        if (icc_profile.size() && icc_profile.data()) {
            /* Calculate the number of markers we'll need, rounding up of course */
            size_t num_markers = icc_profile.size() / MAX_DATA_BYTES_IN_MARKER;
            if (num_markers * MAX_DATA_BYTES_IN_MARKER
                != std::size(icc_profile))
                num_markers++;
            int curr_marker = 1; /* per spec, count starts at 1*/
            std::vector<JOCTET> profile(MAX_DATA_BYTES_IN_MARKER
                                        + ICC_HEADER_SIZE);
            size_t icc_profile_length = icc_profile.size();
            while (icc_profile_length > 0) {
                // length of profile to put in this marker
                size_t length = std::min(icc_profile_length,
                                         size_t(MAX_DATA_BYTES_IN_MARKER));
                icc_profile_length -= length;
                // Write the JPEG marker header (APP2 code and marker length)
                strcpy((char*)profile.data(), "ICC_PROFILE");  // NOSONAR
                profile[11] = 0;
                profile[12] = curr_marker;
                profile[13] = (JOCTET)num_markers;
                OIIO_ASSERT(profile.size() >= ICC_HEADER_SIZE + length);
                spancpy(make_span(profile), ICC_HEADER_SIZE, icc_profile,
                        length * (curr_marker - 1), length);
                jpeg_write_marker(&m_cinfo, JPEG_APP0 + 2, profile.data(),
                                  ICC_HEADER_SIZE + length);
                curr_marker++;
            }
        }
    }

    m_dither = m_spec.get_int_attribute("oiio:dither", 0);

    // If user asked for tiles -- which JPEG doesn't support, emulate it by
    // buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



void
JpgOutput::resmeta_to_density()
{
    // Clear cruft from Exif that might confuse us
    m_spec.erase_attribute("exif:XResolution");
    m_spec.erase_attribute("exif:YResolution");
    m_spec.erase_attribute("exif:ResolutionUnit");

    string_view resunit = m_spec.get_string_attribute("ResolutionUnit");
    if (Strutil::iequals(resunit, "none"))
        m_cinfo.density_unit = 0;
    else if (Strutil::iequals(resunit, "in"))
        m_cinfo.density_unit = 1;
    else if (Strutil::iequals(resunit, "cm"))
        m_cinfo.density_unit = 2;
    else
        m_cinfo.density_unit = 0;

    // We want to use the metadata to set the X_density and Y_density fields in
    // the JPEG header, but the problem is over-constrained. What are the
    // possibilities?
    //
    // what is set?   xres yres par
    //                                assume 72,72 par=1
    //                  *             set yres = xres (par = 1.0)
    //                       *        set xres=yres (par = 1.0)
    //                  *    *        keep (par is implied)
    //                           *    set yres=72, xres based on par
    //                  *        *    set yres based on par
    //                       *   *    set xres based on par
    //                  *    *   *    par wins if they don't match
    //
    float XRes   = m_spec.get_float_attribute("XResolution");
    float YRes   = m_spec.get_float_attribute("YResolution");
    float aspect = m_spec.get_float_attribute("PixelAspectRatio");
    if (aspect <= 0.0f) {
        // PixelAspectRatio was not set in the ImageSpec. So just use the
        // "resolution" values and pass them without judgment. If only one was
        // set, make them equal and assume 1.0 aspect ratio. If neither were
        // set, punt and set the fields to 0.
        if (XRes <= 0.0f && YRes <= 0.0f) {
            // No clue, set the fields to 1,1 to be valid and 1.0 aspect.
            m_cinfo.X_density = 1;
            m_cinfo.Y_density = 1;
            return;
        }
        if (XRes <= 0.0f)
            XRes = YRes;
        if (YRes <= 0.0f)
            YRes = XRes;
        aspect = YRes / XRes;
    } else {
        // PixelAspectRatio was set in the ImageSpec. Let that trump the
        // "resolution" fields, if they contradict.
        //
        // Here's where things get tricky. By logic and reason, as well as
        // the JFIF spec and ITU T.871, the pixel aspect ratio is clearly
        // ydensity/xdensity (because aspect is xlength/ylength, and density
        // is 1/length). BUT... for reasons lost to history, a number of
        // apps get this exactly backwards, and these include PhotoShop,
        // Nuke, and RV. So, alas, we must replicate the mistake, or else
        // all these common applications will misunderstand the JPEG files
        // written by OIIO and vice versa. In other words, we must reverse
        // the sense of how aspect ratio relates to density, contradicting
        // the JFIF spec but conforming to Nuke/etc's behavior. Sigh.
        if (XRes <= 0.0f && YRes <= 0.0f) {
            // resolutions were not set
            if (aspect >= 1.0f) {
                XRes = 72.0f;
                YRes = XRes / aspect;
            } else {
                YRes = 72.0f;
                XRes = YRes * aspect;
            }
        } else if (XRes <= 0.0f) {
            // Xres not set, but Yres was and we know aspect
            // e.g., yres = 100, aspect = 2.0
            // This SHOULD be the right answer:
            //     XRes = YRes / aspect;
            // But because of the note above, reverse it:
            //     XRes = YRes * aspect;
            XRes = YRes * aspect;
        } else {
            // All other cases -- XRes is set, so reset Yres to conform to
            // the requested PixelAspectRatio.
            // This SHOULD be the right answer:
            //     YRes = XRes * aspect;
            // But because of the note above, reverse it:
            //     YRes = XRes / aspect;
            YRes = XRes / aspect;
        }
    }
    int X_density     = clamp(int(XRes + 0.5f), 1, 65535);
    int Y_density     = clamp(int(YRes + 0.5f), 1, 65535);
    m_cinfo.X_density = X_density;
    m_cinfo.Y_density = Y_density;
}



bool
JpgOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    y -= m_spec.y;
    if (y != m_next_scanline) {
        errorfmt("Attempt to write scanlines out of order to {}", m_filename);
        return false;
    }
    if (y >= (int)m_cinfo.image_height) {
        errorfmt("Attempt to write too many scanlines to {}", m_filename);
        return false;
    }
    assert(y == (int)m_cinfo.next_scanline);

    // Here's where we do the dirty work of conforming to JFIF's limitation
    // of 1 or 3 channels, by temporarily doctoring the spec so that
    // to_native_scanline properly contiguizes the first 1 or 3 channels,
    // then we restore it.  The call to to_native_scanline below needs
    // m_spec.nchannels to be set to the true number of channels we're
    // writing, or it won't arrange the data properly.  But if we doctored
    // m_spec.nchannels permanently, then subsequent calls to write_scanline
    // (including any surrounding call to write_image) with
    // stride=AutoStride would screw up the strides since the user's stride
    // is actually not 1 or 3 channels.
    m_spec.auto_stride(xstride, format, m_spec.nchannels);
    int save_nchannels = m_spec.nchannels;
    m_spec.nchannels   = m_cinfo.input_components;

    if (save_nchannels == 2 && m_spec.nchannels == 3) {
        // Edge case: expanding 2 channels to 3
        uint8_t* tmp = OIIO_ALLOCA(uint8_t, m_spec.width * 3);
        memset(tmp, 0, m_spec.width * 3);
        convert_image(2, m_spec.width, 1, 1, data, format, xstride, AutoStride,
                      AutoStride, tmp, TypeDesc::UINT8, 3 * sizeof(uint8_t),
                      AutoStride, AutoStride);
        data = tmp;
    } else {
        data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y,
                                  z);
    }
    m_spec.nchannels = save_nchannels;

    jpeg_write_scanlines(&m_cinfo, (JSAMPLE**)&data, 1);
    ++m_next_scanline;

    return true;
}



bool
JpgOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
JpgOutput::close()
{
    if (!ioproxy_opened()) {  // Already closed
        init();
        return true;
    }

    bool ok = true;

    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_DASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);  // free it
    }

    if (m_next_scanline < spec().height && m_copy_coeffs == NULL) {
        // But if we've only written some scanlines, write the rest to avoid
        // errors
        std::vector<char> buf(spec().scanline_bytes(), 0);
        char* data = &buf[0];
        while (m_next_scanline < spec().height) {
            jpeg_write_scanlines(&m_cinfo, (JSAMPLE**)&data, 1);
            ++m_next_scanline;
        }
    }

    if (m_next_scanline >= spec().height || m_copy_coeffs) {
        DBG std::cout << "out close: about to finish_compress\n";
        jpeg_finish_compress(&m_cinfo);
        DBG std::cout << "out close: finish_compress\n";
    } else {
        DBG std::cout << "out close: about to abort_compress\n";
        jpeg_abort_compress(&m_cinfo);
        DBG std::cout << "out close: abort_compress\n";
    }
    DBG std::cout << "out close: about to destroy_compress\n";
    jpeg_destroy_compress(&m_cinfo);

    if (m_outsize) {
        // We had an IOProxy of some type that was not IOFile. JPEG doesn't
        // have fully general IO overloads, but it can write to memory
        // buffers, we did that, so now we have to copy that in one big chunk
        // to IOProxy.
        ioproxy()->write(m_outbuffer, m_outsize);
    }

    init();
    return ok;
}



bool
JpgOutput::copy_image(ImageInput* in)
{
    if (in && !strcmp(in->format_name(), "jpeg")) {
        JpgInput* jpg_in    = dynamic_cast<JpgInput*>(in);
        std::string in_name = jpg_in->filename();
        DBG std::cout << "JPG copy_image from " << in_name << "\n";

        // Save the original input spec and close it
        ImageSpec orig_in_spec = in->spec();
        in->close();
        DBG std::cout << "Closed old file\n";

        // Re-open the input spec, with special request that the JpgInput
        // will recognize as a request to merely open, but not start the
        // decompressor.
        ImageSpec in_spec;
        ImageSpec config_spec;
        config_spec.attribute("_jpeg:raw", 1);
        in->open(in_name, in_spec, config_spec);

        // Re-open the output
        std::string out_name    = m_filename;
        ImageSpec orig_out_spec = spec();
        close();
        m_copy_coeffs       = (jvirt_barray_ptr*)jpg_in->coeffs();
        m_copy_decompressor = &jpg_in->m_cinfo;
        open(out_name, orig_out_spec);

        // Strangeness -- the write_coefficients somehow sets things up
        // so that certain writes only happen in close(), which MUST
        // happen while the input file is still open.  So we go ahead
        // and close() now, so that the caller of copy_image() doesn't
        // close the input file first and then wonder why they crashed.
        close();

        return true;
    }

    return ImageOutput::copy_image(in);
}

OIIO_PLUGIN_NAMESPACE_END
