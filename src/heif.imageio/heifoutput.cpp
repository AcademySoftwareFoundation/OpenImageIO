// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/tiffutils.h>

#include <libheif/heif_cxx.h>

#define MAKE_LIBHEIF_VERSION(a, b, c, d) \
    (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#if LIBHEIF_NUMERIC_VERSION >= MAKE_LIBHEIF_VERSION(1, 17, 0, 0)
#    include <libheif/heif_properties.h>
#endif


OIIO_PLUGIN_NAMESPACE_BEGIN

class HeifOutput final : public ImageOutput {
public:
    HeifOutput() {}
    ~HeifOutput() override { close(); }
    const char* format_name(void) const override { return "heif"; }
    int supports(string_view feature) const override
    {
        return feature == "alpha" || feature == "exif" || feature == "ioproxy"
               || feature == "tiles"
#if LIBHEIF_HAVE_VERSION(1, 9, 0)
               || feature == "cicp"
#endif
            ;
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;

private:
    std::string m_filename;
    std::unique_ptr<heif::Context> m_ctx;
    heif::ImageHandle m_ihandle;
    heif::Image m_himage;
    // Undefined until we know the specific requested encoder, because an
    // exception is thrown if libheif is built without support for it.
    heif::Encoder m_encoder { heif_compression_undefined };
    std::vector<unsigned char> scratch;
    std::vector<unsigned char> m_tilebuffer;
    int m_bitdepth = 0;
};


class HeifWriter final : public heif::Context::Writer {
public:
    HeifWriter(Filesystem::IOProxy* ioproxy)
        : m_ioproxy(ioproxy)
    {
    }
    heif_error write(const void* data, size_t size) override
    {
        heif_error herr { heif_error_Ok, heif_suberror_Unspecified, "" };
        if (m_ioproxy && m_ioproxy->mode() == Filesystem::IOProxy::Write
            && m_ioproxy->write(data, size) == size) {
            // ok
        } else {
            herr.code    = heif_error_Encoding_error;
            herr.message = "write error";
        }
        return herr;
    }

private:
    Filesystem::IOProxy* m_ioproxy = nullptr;
};


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
heif_output_imageio_create()
{
    extern void oiio_heif_init();
    oiio_heif_init();
    return new HeifOutput;
}

OIIO_EXPORT const char* heif_output_extensions[] = { "heif", "heic", "heics",
                                                     "hif",  "avif", nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
HeifOutput::open(const std::string& name, const ImageSpec& newspec,
                 OpenMode mode)
{
    if (!check_open(mode, newspec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow2Channel)))
        return false;

    m_filename = name;

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name)) {
        return false;
    }

    m_bitdepth = m_spec.format.size() > TypeUInt8.size() ? 10 : 8;
    m_bitdepth = m_spec.get_int_attribute("oiio:BitsPerSample", m_bitdepth);
    if (m_bitdepth == 10 || m_bitdepth == 12) {
        m_spec.set_format(TypeUInt16);
    } else if (m_bitdepth == 8) {
        m_spec.set_format(TypeUInt8);
    } else {
        errorfmt("Unsupported bit depth {}", m_bitdepth);
        return false;
    }

    try {
        m_ctx.reset(new heif::Context);
        m_himage = heif::Image();
        static heif_chroma chromas[/*nchannels*/]
            = { heif_chroma_undefined, heif_chroma_monochrome,
                heif_chroma_undefined,
                (m_bitdepth == 8) ? heif_chroma_interleaved_RGB
                : littleendian()  ? heif_chroma_interleaved_RRGGBB_LE
                                  : heif_chroma_interleaved_RRGGBB_BE,
                (m_bitdepth == 8) ? heif_chroma_interleaved_RGBA
                : littleendian()  ? heif_chroma_interleaved_RRGGBBAA_LE
                                  : heif_chroma_interleaved_RRGGBBAA_BE };
        m_himage.create(newspec.width, newspec.height, heif_colorspace_RGB,
                        chromas[m_spec.nchannels]);
        m_himage.add_plane(heif_channel_interleaved, newspec.width,
                           newspec.height, m_bitdepth);

        auto compqual  = m_spec.decode_compression_metadata("", 75);
        auto extension = Filesystem::extension(m_filename);
        if (compqual.first == "avif"
            || (extension == ".avif" && compqual.first == "")) {
            m_encoder = heif::Encoder(heif_compression_AV1);
        } else {
            m_encoder = heif::Encoder(heif_compression_HEVC);
        }
    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        return false;
    }

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



bool
HeifOutput::write_scanline(int y, int /*z*/, TypeDesc format, const void* data,
                           stride_t xstride)
{
    data = to_native_scanline(format, data, xstride, scratch);
#if LIBHEIF_NUMERIC_VERSION >= MAKE_LIBHEIF_VERSION(1, 20, 0, 0)
    size_t hystride = 0;
#else
    int hystride = 0;
#endif
#if LIBHEIF_NUMERIC_VERSION >= MAKE_LIBHEIF_VERSION(1, 20, 2, 0)
    uint8_t* hdata = m_himage.get_plane2(heif_channel_interleaved, &hystride);
#else
    uint8_t* hdata = m_himage.get_plane(heif_channel_interleaved, &hystride);
#endif
    hdata += hystride * (y - m_spec.y);
    if (m_bitdepth == 10 || m_bitdepth == 12) {
        const uint16_t* data16  = static_cast<const uint16_t*>(data);
        uint16_t* hdata16       = reinterpret_cast<uint16_t*>(hdata);
        const size_t num_values = m_spec.width * m_spec.nchannels;
        if (m_bitdepth == 10) {
            for (size_t i = 0; i < num_values; ++i) {
                hdata16[i] = bit_range_convert<16, 10>(data16[i]);
            }
        } else {
            for (size_t i = 0; i < num_values; ++i) {
                hdata16[i] = bit_range_convert<16, 12>(data16[i]);
            }
        }
    } else {
        memcpy(hdata, data, hystride);
    }
    return true;
}



bool
HeifOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
HeifOutput::close()
{
    if (!m_ctx || !ioproxy_opened()) {  // already closed
        m_ctx.reset();
        ioproxy_clear();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    std::vector<char> exifblob;
    try {
        auto compqual = m_spec.decode_compression_metadata("", 75);
        if (compqual.first == "heic" || compqual.first == "avif") {
            if (compqual.second >= 100)
                m_encoder.set_lossless(true);
            else {
                m_encoder.set_lossless(false);
                m_encoder.set_lossy_quality(compqual.second);
            }
        } else if (compqual.first == "none") {
            m_encoder.set_lossless(true);
        }
        heif::Context::EncodingOptions options;
#if LIBHEIF_HAVE_VERSION(1, 9, 0)
        // Write CICP. we can only set output_nclx_profile with the C API.
        std::unique_ptr<heif_color_profile_nclx,
                        void (*)(heif_color_profile_nclx*)>
            nclx(heif_nclx_color_profile_alloc(), heif_nclx_color_profile_free);
        const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
        const ParamValue* p    = m_spec.find_attribute("CICP",
                                                       TypeDesc(TypeDesc::INT, 4));
        string_view colorspace = m_spec.get_string_attribute("oiio:ColorSpace");
        cspan<int> cicp        = (p) ? p->as_cspan<int>()
                                     : colorconfig.get_cicp(colorspace);
        if (!cicp.empty()) {
            nclx->color_primaries          = heif_color_primaries(cicp[0]);
            nclx->transfer_characteristics = heif_transfer_characteristics(
                cicp[1]);
            nclx->matrix_coefficients   = heif_matrix_coefficients(cicp[2]);
            nclx->full_range_flag       = cicp[3];
            options.output_nclx_profile = nclx.get();
            // Chroma subsampling is incompatible with RGB.
            if (nclx->matrix_coefficients == heif_matrix_coefficients_RGB_GBR) {
                m_encoder.set_string_parameter("chroma", "444");
            }
        }
#endif
        encode_exif(m_spec, exifblob, endian::big);
        m_ihandle = m_ctx->encode_image(m_himage, m_encoder, options);
        std::vector<char> head { 'E', 'x', 'i', 'f', 0, 0 };
        exifblob.insert(exifblob.begin(), head.begin(), head.end());
        try {
            m_ctx->add_exif_metadata(m_ihandle, exifblob.data(),
                                     exifblob.size());
        } catch (const heif::Error& err) {
#ifdef DEBUG
            std::string e = err.get_message();
            Strutil::print("{}", e.empty() ? "unknown exception" : e.c_str());
#endif
        }
        m_ctx->set_primary_image(m_ihandle);
        HeifWriter writer(ioproxy());
        m_ctx->write(writer);
    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        ok = false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        ok = false;
    }

    m_ctx.reset();
    ioproxy_clear();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
