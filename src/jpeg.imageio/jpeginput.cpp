// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cassert>
#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/tiffutils.h>

#include "jpeg_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


// N.B. The class definition for JpgInput is in jpeg_pvt.h.


// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int jpeg_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
jpeg_imageio_library_version()
{
#ifdef LIBJPEG_TURBO_VERSION
    return "jpeg-turbo " OIIO_STRINGIZE(
        LIBJPEG_TURBO_VERSION) "/jp" OIIO_STRINGIZE(JPEG_LIB_VERSION);
#else
    return "jpeglib " OIIO_STRINGIZE(JPEG_LIB_VERSION_MAJOR) "." OIIO_STRINGIZE(
        JPEG_LIB_VERSION_MINOR);
#endif
}

OIIO_EXPORT ImageInput*
jpeg_input_imageio_create()
{
    return new JpgInput;
}

OIIO_EXPORT const char* jpeg_input_extensions[]
    = { "jpg", "jpe", "jpeg", "jif", "jfif", "jfi", nullptr };

OIIO_PLUGIN_EXPORTS_END


static const uint8_t JPEG_MAGIC1 = 0xff;
static const uint8_t JPEG_MAGIC2 = 0xd8;


// For explanations of the error handling, see the "example.c" in the
// libjpeg distribution.


static void
my_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    JpgInput::my_error_ptr myerr = (JpgInput::my_error_ptr)cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    //  (*cinfo->err->output_message) (cinfo);
    myerr->jpginput->jpegerror(myerr, true);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}



static void
my_output_message(j_common_ptr cinfo)
{
    JpgInput::my_error_ptr myerr = (JpgInput::my_error_ptr)cinfo->err;

    // Create the message
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buffer);
    myerr->jpginput->jpegerror(myerr, false);

    // This function is called only for non-fatal problems, so we don't
    // need to do the longjmp.
    // longjmp(myerr->setjmp_buffer, 1);
}



static std::string
comp_info_to_attr(const jpeg_decompress_struct& cinfo)
{
    // Compare the current 6 samples with our known definitions
    // to determine the corresponding subsampling attr
    std::vector<int> comp;
    comp.push_back(cinfo.comp_info[0].h_samp_factor);
    comp.push_back(cinfo.comp_info[0].v_samp_factor);
    comp.push_back(cinfo.comp_info[1].h_samp_factor);
    comp.push_back(cinfo.comp_info[1].v_samp_factor);
    comp.push_back(cinfo.comp_info[2].h_samp_factor);
    comp.push_back(cinfo.comp_info[2].v_samp_factor);
    size_t size = comp.size();

    if (std::equal(JPEG_444_COMP, JPEG_444_COMP + size, comp.begin()))
        return JPEG_444_STR;
    else if (std::equal(JPEG_422_COMP, JPEG_422_COMP + size, comp.begin()))
        return JPEG_422_STR;
    else if (std::equal(JPEG_420_COMP, JPEG_420_COMP + size, comp.begin()))
        return JPEG_420_STR;
    else if (std::equal(JPEG_411_COMP, JPEG_411_COMP + size, comp.begin()))
        return JPEG_411_STR;
    return "";
}



void
JpgInput::jpegerror(my_error_ptr /*myerr*/, bool fatal)
{
    // Send the error message to the ImageInput
    char errbuf[JMSG_LENGTH_MAX];
    (*m_cinfo.err->format_message)((j_common_ptr)&m_cinfo, errbuf);
    errorfmt("JPEG error: {} (\"{}\")", errbuf, filename());

    // Shut it down and clean it up
    if (fatal) {
        m_fatalerr = true;
        close();
        m_fatalerr = true;  // because close() will reset it
    }
}



bool
JpgInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    // Check magic number to assure this is a JPEG file
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Read)
        return false;

    uint8_t magic[2] {};
    const size_t numRead = ioproxy->pread(magic, sizeof(magic), 0);
    return numRead == sizeof(magic) && magic[0] == JPEG_MAGIC1
           && magic[1] == JPEG_MAGIC2;
}



bool
JpgInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    auto p = config.find_attribute("_jpeg:raw", TypeInt);
    m_raw  = p && *(int*)p->data();
    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



bool
JpgInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;

    if (!ioproxy_use_or_open(name))
        return false;
    // If an IOProxy was passed, it had better be a File or a
    // MemReader, that's all we know how to use with jpeg.
    Filesystem::IOProxy* m_io = ioproxy();
    std::string proxytype     = m_io->proxytype();
    if (proxytype != "file" && proxytype != "memreader") {
        errorfmt("JPEG reader can't handle proxy type {}", proxytype);
        return false;
    }

    // Check magic number to assure this is a JPEG file
    uint8_t magic[2] = { 0, 0 };
    if (m_io->pread(magic, sizeof(magic), 0) != sizeof(magic)) {
        errorfmt("Empty file \"{}\"", name);
        close_file();
        return false;
    }

    if (magic[0] != JPEG_MAGIC1 || magic[1] != JPEG_MAGIC2) {
        close_file();
        errorfmt(
            "\"{}\" is not a JPEG file, magic number doesn't match (was 0x{:x}{:x})",
            name, int(magic[0]), int(magic[1]));
        return false;
    }

    // Set up the normal JPEG error routines, then override error_exit and
    // output_message so we intercept all the errors.
    m_cinfo.err               = jpeg_std_error((jpeg_error_mgr*)&m_jerr);
    m_jerr.pub.error_exit     = my_error_exit;
    m_jerr.pub.output_message = my_output_message;
    if (setjmp(m_jerr.setjmp_buffer)) {
        // Jump to here if there's a libjpeg internal error
        // Prevent memory leaks, see example.c in jpeg distribution
        jpeg_destroy_decompress(&m_cinfo);
        close_file();
        return false;
    }

    // initialize decompressor
    jpeg_create_decompress(&m_cinfo);
    m_decomp_create = true;
    // specify the data source
    if (proxytype == "file") {
        auto fd = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        jpeg_stdio_src(&m_cinfo, fd);
    } else {
        auto buffer = reinterpret_cast<Filesystem::IOMemReader*>(m_io)->buffer();
        jpeg_mem_src(&m_cinfo, const_cast<unsigned char*>(buffer.data()),
                     buffer.size());
    }

    // Request saving of EXIF and other special tags for later spelunking
    for (int mark = 0; mark < 16; ++mark)
        jpeg_save_markers(&m_cinfo, JPEG_APP0 + mark, 0xffff);
    jpeg_save_markers(&m_cinfo, JPEG_COM, 0xffff);  // comment marker

    // read the file parameters
    if (jpeg_read_header(&m_cinfo, FALSE) != JPEG_HEADER_OK || m_fatalerr) {
        errorfmt("Bad JPEG header for \"{}\"", filename());
        return false;
    }

    int nchannels = m_cinfo.num_components;

    if (m_cinfo.jpeg_color_space == JCS_CMYK
        || m_cinfo.jpeg_color_space == JCS_YCCK) {
        // CMYK jpegs get converted by us to RGB
        m_cinfo.out_color_space = JCS_CMYK;  // pre-convert YCbCrK->CMYK
        nchannels               = 3;
        m_cmyk                  = true;
    }

    if (m_raw)
        m_coeffs = jpeg_read_coefficients(&m_cinfo);
    else
        jpeg_start_decompress(&m_cinfo);  // start working
    if (m_fatalerr)
        return false;
    m_next_scanline = 0;  // next scanline we'll read

    m_spec = ImageSpec(m_cinfo.output_width, m_cinfo.output_height, nchannels,
                       TypeDesc::UINT8);

    if (!check_open(m_spec, { 0, 1 << 16, 0, 1 << 16, 0, 1, 0, 3 }))
        return false;

    // Assume JPEG is in sRGB unless the Exif or XMP tags say otherwise.
    m_spec.set_colorspace("sRGB");

    if (m_cinfo.jpeg_color_space == JCS_CMYK)
        m_spec.attribute("jpeg:ColorSpace", "CMYK");
    else if (m_cinfo.jpeg_color_space == JCS_YCCK)
        m_spec.attribute("jpeg:ColorSpace", "YCbCrK");

    // If the chroma subsampling is detected and matches something
    // we expect, then set an attribute so that it can be preserved
    // in future operations.
    std::string subsampling = comp_info_to_attr(m_cinfo);
    if (!subsampling.empty())
        m_spec.attribute(JPEG_SUBSAMPLING_ATTR, subsampling);

    for (jpeg_saved_marker_ptr m = m_cinfo.marker_list; m; m = m->next) {
        if (m->marker == (JPEG_APP0 + 1)
            && !strcmp((const char*)m->data, "Exif")) {
            // The block starts with "Exif\0\0", so skip 6 bytes to get
            // to the start of the actual Exif data TIFF directory
            decode_exif(string_view((char*)m->data + 6, m->data_length - 6),
                        m_spec);
        } else if (m->marker == (JPEG_APP0 + 1)
                   && !strcmp((const char*)m->data,
                              "http://ns.adobe.com/xap/1.0/")) {  //NOSONAR
            std::string xml((const char*)m->data, m->data_length);
            decode_xmp(xml, m_spec);
        } else if (m->marker == (JPEG_APP0 + 13)
                   && !strcmp((const char*)m->data, "Photoshop 3.0"))
            jpeg_decode_iptc((unsigned char*)m->data);
        else if (m->marker == JPEG_COM) {
            std::string data((const char*)m->data, m->data_length);
            // Additional string metadata can be stored in JPEG files as
            // comment markers in the form "key:value" or "ident:key:value".
            // If the string contains a single colon, we assume key:value.
            // If there's multiple, we try splitting as ident:key:value and
            // check if ident and key are reasonable (in particular, whether
            // ident is a C-style identifier and key is not surrounded by
            // whitespace). If ident passes but key doesn't, assume key:value.
            auto separator = data.find(':');
            if (OIIO::get_int_attribute("jpeg:com_attributes")
                && (separator != std::string::npos && separator > 0)) {
                std::string left  = data.substr(0, separator);
                std::string right = data.substr(separator + 1);
                separator         = right.find(':');
                if (separator != std::string::npos && separator > 0) {
                    std::string mid   = right.substr(0, separator);
                    std::string value = right.substr(separator + 1);
                    if (Strutil::string_is_identifier(left)
                        && (mid == Strutil::trimmed_whitespace(mid))) {
                        // Valid parsing: left is ident, mid is key
                        std::string attribute = left + ":" + mid;
                        if (!m_spec.find_attribute(attribute, TypeDesc::STRING))
                            m_spec.attribute(attribute, value);
                        continue;
                    }
                }
                if (left == Strutil::trimmed_whitespace(left)) {
                    // Valid parsing: left is key, right is value
                    if (!m_spec.find_attribute(left, TypeDesc::STRING))
                        m_spec.attribute(left, right);
                    continue;
                }
            }
            // If we made it this far, treat the comment as a description
            if (!m_spec.find_attribute("ImageDescription", TypeDesc::STRING))
                m_spec.attribute("ImageDescription", data);
        }
    }

    // Handle density/pixelaspect. We need to do this AFTER the exif is
    // decoded, in case it contains useful information.
    float xdensity = m_spec.get_float_attribute("XResolution");
    float ydensity = m_spec.get_float_attribute("YResolution");
    if (m_cinfo.X_density && m_cinfo.Y_density) {
        xdensity = float(m_cinfo.X_density);
        ydensity = float(m_cinfo.Y_density);
        if (xdensity > 1 && ydensity > 1) {
            m_spec.attribute("XResolution", xdensity);
            m_spec.attribute("YResolution", ydensity);
            // We're kind of assuming that if either cinfo.X_density or
            // Y_density is 1, then those fields are only used to indicate
            // pixel aspect ratio, but don't override [XY]Resolution that may
            // have come from the Exif.
        }
    }
    if (xdensity && ydensity) {
        // Pixel aspect ratio SHOULD be computed like this:
        //     float aspect = ydensity / xdensity;
        // But Nuke and Photoshop do it backwards, and so we do, too, because
        // we are lemmings.
        float aspect = xdensity / ydensity;
        if (aspect != 1.0f)
            m_spec.attribute("PixelAspectRatio", aspect);
        if (m_spec.extra_attribs.contains("XResolution")) {
            switch (m_cinfo.density_unit) {
            case 0: m_spec.attribute("ResolutionUnit", "none"); break;
            case 1: m_spec.attribute("ResolutionUnit", "in"); break;
            case 2: m_spec.attribute("ResolutionUnit", "cm"); break;
            }
        }
    }

    read_icc_profile(&m_cinfo, m_spec);  /// try to read icc profile

    // Try to interpret as Ultra HDR image.
    // The libultrahdr API requires to load the whole file content in memory
    // therefore we first check for the presence of the "hdrgm:Version" metadata
    // to avoid this costly process when not necessary.
    // https://developer.android.com/media/platform/hdr-image-format#signal_of_the_format
    if (m_spec.find_attribute("hdrgm:Version"))
        m_is_uhdr = read_uhdr(m_io);

    newspec = m_spec;
    return true;
}



bool
JpgInput::read_icc_profile(j_decompress_ptr cinfo, ImageSpec& spec)
{
    int num_markers = 0;
    std::vector<uint8_t> icc_buf;
    unsigned int total_length = 0;
    const int MAX_SEQ_NO      = 255;
    unsigned char marker_present
        [MAX_SEQ_NO
         + 1];  // one extra is used to store the flag if marker is found, set to one if marker is found
    unsigned int data_length[MAX_SEQ_NO + 1];  // store the size of each marker
    unsigned int data_offset[MAX_SEQ_NO + 1];  // store the offset of each marker
    memset(marker_present, 0, (MAX_SEQ_NO + 1));

    for (jpeg_saved_marker_ptr m = cinfo->marker_list; m; m = m->next) {
        if (m->marker == (JPEG_APP0 + 2)
            && !strcmp((const char*)m->data, "ICC_PROFILE")) {
            if (num_markers == 0)
                num_markers = GETJOCTET(m->data[13]);
            else if (num_markers != GETJOCTET(m->data[13]))
                return false;
            int seq_no = GETJOCTET(m->data[12]);
            if (seq_no <= 0 || seq_no > num_markers)
                return false;
            if (marker_present[seq_no])  // duplicate marker
                return false;
            marker_present[seq_no] = 1;  // flag found marker
            data_length[seq_no]    = m->data_length - ICC_HEADER_SIZE;
        }
    }
    if (num_markers == 0)
        return false;

    // checking for missing markers
    for (int seq_no = 1; seq_no <= num_markers; seq_no++) {
        if (marker_present[seq_no] == 0)
            return false;  // missing sequence number
        data_offset[seq_no] = total_length;
        total_length += data_length[seq_no];
    }

    if (total_length == 0)
        return false;  // found only empty markers

    icc_buf.resize(total_length * sizeof(JOCTET));

    // and fill it in
    for (jpeg_saved_marker_ptr m = cinfo->marker_list; m; m = m->next) {
        if (m->marker == (JPEG_APP0 + 2)
            && !strcmp((const char*)m->data, "ICC_PROFILE")) {
            int seq_no = GETJOCTET(m->data[12]);
            if (data_offset[seq_no] + data_length[seq_no] > icc_buf.size()) {
                errorfmt("Possible corrupt file, invalid ICC profile\n");
                return false;
            }
            spancpy(make_span(icc_buf), data_offset[seq_no],
                    make_cspan(m->data + ICC_HEADER_SIZE, data_length[seq_no]),
                    0, data_length[seq_no]);
        }
    }
    spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, total_length),
                   icc_buf.data());

    std::string errormsg;
    bool ok = decode_icc_profile(icc_buf, spec, errormsg);
    if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
        errorfmt("Possible corrupt file, could not decode ICC profile: {}\n",
                 errormsg);
        return false;
    }

    return true;
}



bool
JpgInput::read_uhdr(Filesystem::IOProxy* ioproxy)
{
#if defined(USE_UHDR)
    // Read entire file content into buffer.
    const size_t buffer_size = ioproxy->size();
    std::vector<unsigned char> buffer(buffer_size);
    ioproxy->pread(buffer.data(), buffer_size, 0);

    // Check if this is an actual Ultra HDR image.
    const bool detect_uhdr = is_uhdr_image(buffer.data(), buffer.size());
    if (!detect_uhdr)
        return false;

    // Create Ultra HDR decoder.
    // Do not forget to release it once we don't need it,
    // i.e if this function returns false
    // or when we call close().
    m_uhdr_dec = uhdr_create_decoder();

    // Prepare decoder input.
    // Note: we currently do not override any of the
    // default settings.
    uhdr_compressed_image_t uhdr_compressed;
    uhdr_compressed.data     = buffer.data();
    uhdr_compressed.data_sz  = buffer.size();
    uhdr_compressed.capacity = buffer.size();
    uhdr_dec_set_image(m_uhdr_dec, &uhdr_compressed);

    // Decode Ultra HDR image
    // and check for decoding errors.
    uhdr_error_info_t err_info = uhdr_decode(m_uhdr_dec);

    if (err_info.error_code != UHDR_CODEC_OK) {
        errorfmt("Ultra HDR decoding failed with error code {}",
                 int(err_info.error_code));
        if (err_info.has_detail != 0)
            errorfmt("Additional error details: {}", err_info.detail);
        uhdr_release_decoder(m_uhdr_dec);
        return false;
    }

    // Update spec with decoded image properties.
    // Note: we currently only support a subset of all possible
    // Ultra HDR image formats.
    uhdr_raw_image_t* uhdr_raw = uhdr_get_decoded_image(m_uhdr_dec);

    int nchannels;
    TypeDesc desc;
    switch (uhdr_raw->fmt) {
    case UHDR_IMG_FMT_32bppRGBA8888:
        nchannels = 4;
        desc      = TypeDesc::UINT8;
        break;
    case UHDR_IMG_FMT_64bppRGBAHalfFloat:
        nchannels = 4;
        desc      = TypeDesc::HALF;
        break;
    case UHDR_IMG_FMT_24bppRGB888:
        nchannels = 3;
        desc      = TypeDesc::UINT8;
        break;
    default:
        errorfmt("Unsupported Ultra HDR image format: {}", int(uhdr_raw->fmt));
        uhdr_release_decoder(m_uhdr_dec);
        return false;
    }

    ImageSpec newspec = ImageSpec(uhdr_raw->w, uhdr_raw->h, nchannels, desc);
    newspec.extra_attribs = std::move(m_spec.extra_attribs);
    m_spec                = newspec;

    return true;
#else
    return false;
#endif
}



static void
cmyk_to_rgb(int n, const unsigned char* cmyk, size_t cmyk_stride,
            unsigned char* rgb, size_t rgb_stride)
{
    for (; n; --n, cmyk += cmyk_stride, rgb += rgb_stride) {
        // JPEG seems to store CMYK as 1-x
        float C = convert_type<unsigned char, float>(cmyk[0]);
        float M = convert_type<unsigned char, float>(cmyk[1]);
        float Y = convert_type<unsigned char, float>(cmyk[2]);
        float K = convert_type<unsigned char, float>(cmyk[3]);
        float R = C * K;
        float G = M * K;
        float B = Y * K;
        rgb[0]  = convert_type<float, unsigned char>(R);
        rgb[1]  = convert_type<float, unsigned char>(G);
        rgb[2]  = convert_type<float, unsigned char>(B);
    }
}



bool
JpgInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (m_raw)
        return false;
    if (y < 0 || y >= (int)m_cinfo.output_height)  // out of range scanline
        return false;
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        // Don't forget to save and restore any configuration settings.
        ImageSpec configsave;
        if (m_config)
            configsave = *m_config;
        ImageSpec dummyspec;
        int subimage = current_subimage();
        if (!close() || !open(m_filename, dummyspec, configsave)
            || !seek_subimage(subimage, 0))
            return false;  // Somehow, the re-open failed
        OIIO_DASSERT(m_next_scanline == 0 && current_subimage() == subimage);
    }

#if defined(USE_UHDR)
    if (m_is_uhdr) {
        uhdr_raw_image_t* uhdr_raw = uhdr_get_decoded_image(m_uhdr_dec);

        unsigned int nbytes;
        switch (uhdr_raw->fmt) {
        case UHDR_IMG_FMT_32bppRGBA8888: nbytes = 4; break;
        case UHDR_IMG_FMT_64bppRGBAHalfFloat: nbytes = 8; break;
        case UHDR_IMG_FMT_24bppRGB888: nbytes = 3; break;
        default: return false;
        }

        const size_t row_size   = uhdr_raw->stride[UHDR_PLANE_PACKED] * nbytes;
        unsigned char* top_left = static_cast<unsigned char*>(
            uhdr_raw->planes[UHDR_PLANE_PACKED]);
        unsigned char* row_data_start = top_left + row_size * y;
        memcpy(data, row_data_start, row_size);

        return true;
    }
#endif

    // Set up our custom error handler
    if (setjmp(m_jerr.setjmp_buffer)) {
        // Jump to here if there's a libjpeg internal error
        return false;
    }

    void* readdata = data;
    if (m_cmyk) {
        // If the file's data is CMYK, read into a 4-channel buffer, then
        // we'll have to convert.
        m_cmyk_buf.resize(m_spec.width * 4);
        readdata = &m_cmyk_buf[0];
        OIIO_DASSERT(m_spec.nchannels == 3);
    }

    for (; m_next_scanline <= y; ++m_next_scanline) {
        // Keep reading until we've read the scanline we really need
        if (jpeg_read_scanlines(&m_cinfo, (JSAMPLE**)&readdata, 1) != 1
            || m_fatalerr) {
            errorfmt("JPEG failed scanline read (\"{}\")", filename());
            return false;
        }
    }

    if (m_cmyk)
        cmyk_to_rgb(m_spec.width, (unsigned char*)readdata, 4,
                    (unsigned char*)data, 3);

    return true;
}



bool
JpgInput::close()
{
    if (ioproxy_opened()) {
        // unnecessary?  jpeg_abort_decompress (&m_cinfo);
        if (m_decomp_create)
            jpeg_destroy_decompress(&m_cinfo);
        m_decomp_create = false;
#if defined(USE_UHDR)
        if (m_is_uhdr)
            uhdr_release_decoder(m_uhdr_dec);
        m_is_uhdr = false;
#endif
        close_file();
    }
    init();  // Reset to initial state
    return true;
}



void
JpgInput::jpeg_decode_iptc(const unsigned char* buf)
{
    // APP13 blob doesn't have to be IPTC info.  Look for the IPTC marker,
    // which is the string "Photoshop 3.0" followed by a null character.
    if (strcmp((const char*)buf, "Photoshop 3.0"))
        return;
    buf += strlen("Photoshop 3.0") + 1;

    // Next are the 4 bytes "8BIM"
    if (strncmp((const char*)buf, "8BIM", 4))
        return;
    buf += 4;

    // Next two bytes are the segment type, in big endian.
    // We expect 1028 to indicate IPTC data block.
    if (((buf[0] << 8) + buf[1]) != 1028)
        return;
    buf += 2;

    // Next are 4 bytes of 0 padding, just skip it.
    buf += 4;

    // Next is 2 byte (big endian) giving the size of the segment
    int segmentsize = (buf[0] << 8) + buf[1];
    buf += 2;

    decode_iptc_iim(buf, segmentsize, m_spec);
}

OIIO_PLUGIN_NAMESPACE_END
