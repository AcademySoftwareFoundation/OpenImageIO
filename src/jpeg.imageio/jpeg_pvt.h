// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the jpeg.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include <csetjmp>

#ifdef WIN32
#    undef FAR
#    define XMD_H
#endif

extern "C" {
#include "jpeglib.h"
}

#if JPEG_LIB_VERSION < 80
//#    error "Only libjpeg 8+ is supported (JPEG_LIB_VERSION >= 80)"
#endif

#ifdef JPEG_LIB_VERSION_MINOR
#    define OIIO_JPEG_LIB_VERSION \
        (JPEG_LIB_VERSION_MAJOR * 10 + JPEG_LIB_VERSION_MINOR)
#else
#    define OIIO_JPEG_LIB_VERSION JPEG_LIB_VERSION
#endif


OIIO_PLUGIN_NAMESPACE_BEGIN


#define MAX_DATA_BYTES_IN_MARKER 65519L
#define ICC_HEADER_SIZE 14
#define ICC_PROFILE_ATTR "ICCProfile"

// Chroma sub-sampling values for jpeg_compress_struct / jpeg_component_info
#define JPEG_SUBSAMPLING_ATTR "jpeg:subsampling"
#define JPEG_444_STR "4:4:4"
#define JPEG_422_STR "4:2:2"
#define JPEG_420_STR "4:2:0"
#define JPEG_411_STR "4:1:1"

static const int JPEG_444_COMP[6] = { 1, 1, 1, 1, 1, 1 };
static const int JPEG_422_COMP[6] = { 2, 1, 1, 1, 1, 1 };
static const int JPEG_420_COMP[6] = { 2, 2, 1, 1, 1, 1 };
static const int JPEG_411_COMP[6] = { 4, 1, 1, 1, 1, 1 };


class JpgInput final : public ImageInput {
public:
    JpgInput() { init(); }
    ~JpgInput() override { close(); }
    const char* format_name(void) const override { return "jpeg"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "iptc" || feature == "ioproxy");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;

    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool close() override;

    const std::string& filename() const { return m_filename; }
    void* coeffs() const { return m_coeffs; }
    struct my_error_mgr {
        struct jpeg_error_mgr pub; /* "public" fields */
        jmp_buf setjmp_buffer;     /* for return to caller */
        JpgInput* jpginput;        /* back pointer to *this */
    };
    typedef struct my_error_mgr* my_error_ptr;

    // Called by my_error_exit
    void jpegerror(my_error_ptr myerr, bool fatal = false);

private:
    std::string m_filename;
    int m_next_scanline;   // Which scanline is the next to read?
    bool m_raw;            // Read raw coefficients, not scanlines
    bool m_cmyk;           // The input file is cmyk
    bool m_fatalerr;       // JPEG reader hit a fatal error
    bool m_decomp_create;  // Have we created the decompressor?
    struct jpeg_decompress_struct m_cinfo;
    my_error_mgr m_jerr;
    jvirt_barray_ptr* m_coeffs;
    std::vector<unsigned char> m_cmyk_buf;  // For CMYK translation
    std::unique_ptr<ImageSpec> m_config;    // Saved copy of configuration spec

    void init()
    {
        m_raw           = false;
        m_cmyk          = false;
        m_fatalerr      = false;
        m_decomp_create = false;
        m_coeffs        = NULL;
        m_jerr.jpginput = this;
        ioproxy_clear();
        m_config.reset();
    }

    // Rummage through the JPEG "APP1" marker pointed to by buf, decoding
    // IPTC (International Press Telecommunications Council) metadata
    // information and adding attributes to spec.  This assumes it's in
    // the form of an IIM (Information Interchange Model), which is actually
    // considered obsolete and is replaced by an XML scheme called XMP.
    void jpeg_decode_iptc(const unsigned char* buf);

    bool read_icc_profile(j_decompress_ptr cinfo, ImageSpec& spec);

    void close_file() { init(); }

    friend class JpgOutput;
};



OIIO_PLUGIN_NAMESPACE_END
