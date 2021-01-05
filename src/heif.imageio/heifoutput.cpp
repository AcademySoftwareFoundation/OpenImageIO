// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <libheif/heif_cxx.h>


OIIO_PLUGIN_NAMESPACE_BEGIN

class HeifOutput final : public ImageOutput {
public:
    HeifOutput() {}
    virtual ~HeifOutput() { close(); }
    virtual const char* format_name(void) const override { return "heif"; }
    virtual int supports(string_view feature) const override
    {
        return feature == "alpha" || feature == "exif";
    }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode) override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool close() override;

private:
    std::string m_filename;
    std::unique_ptr<heif::Context> m_ctx;
    heif::ImageHandle m_ihandle;
    heif::Image m_himage;
    heif::Encoder m_encoder { heif_compression_HEVC };
    std::vector<unsigned char> scratch;
    std::vector<unsigned char> m_tilebuffer;
};



namespace {

class MyHeifWriter : public heif::Context::Writer {
public:
    MyHeifWriter(Filesystem::IOProxy* ioproxy)
        : m_ioproxy(ioproxy)
    {
    }
    virtual heif_error write(const void* data, size_t size)
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

}  // namespace


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
heif_output_imageio_create()
{
    return new HeifOutput;
}

OIIO_EXPORT const char* heif_output_extensions[] = { "heif", "heic", "heics",
#if LIBHEIF_HAVE_VERSION(1, 7, 0)
                                                     "avif",
#endif
                                                     nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
HeifOutput::open(const std::string& name, const ImageSpec& newspec,
                 OpenMode mode)
{
    if (mode != Create) {
        errorf("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    m_filename = name;
    // Save spec for later used
    m_spec = newspec;
    // heif always behaves like floating point
    m_spec.set_format(TypeDesc::FLOAT);

    // Check for things heif can't support
    if (m_spec.nchannels != 1 && m_spec.nchannels != 3
        && m_spec.nchannels != 4) {
        errorf("heif can only support 1-, 3- or 4-channel images");
        return false;
    }
    if (m_spec.width < 1 || m_spec.height < 1) {
        errorf("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        errorf("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    m_spec.set_format(TypeUInt8);  // Only uint8 for now

    try {
        m_ctx.reset(new heif::Context);
        m_himage = heif::Image();
        static heif_chroma chromas[/*nchannels*/]
            = { heif_chroma_undefined, heif_chroma_monochrome,
                heif_chroma_undefined, heif_chroma_interleaved_RGB,
                heif_chroma_interleaved_RGBA };
        m_himage.create(newspec.width, newspec.height, heif_colorspace_RGB,
                        chromas[m_spec.nchannels]);
        m_himage.add_plane(heif_channel_interleaved, newspec.width,
                           newspec.height, 8 * m_spec.nchannels /*bit depth*/);

        m_encoder = heif::Encoder(heif_compression_HEVC);
#if LIBHEIF_HAVE_VERSION(1, 7, 0)
        auto compqual  = m_spec.decode_compression_metadata("", 75);
        auto extension = Filesystem::extension(m_filename);
        if (compqual.first == "avif"
            || (extension == ".avif" && compqual.first == "")) {
            m_encoder = heif::Encoder(heif_compression_AV1);
        }
#endif
    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
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
    data           = to_native_scanline(format, data, xstride, scratch);
    int hystride   = 0;
    uint8_t* hdata = m_himage.get_plane(heif_channel_interleaved, &hystride);
    hdata += hystride * (y - m_spec.y);
    memcpy(hdata, data, hystride);
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
    if (!m_ctx) {  // already closed
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
        encode_exif(m_spec, exifblob, endian::big);
        m_ihandle = m_ctx->encode_image(m_himage, m_encoder);
        std::vector<char> head { 'E', 'x', 'i', 'f', 0, 0 };
        exifblob.insert(exifblob.begin(), head.begin(), head.end());
        try {
            m_ctx->add_exif_metadata(m_ihandle, exifblob.data(),
                                     exifblob.size());
        } catch (const heif::Error& err) {
#ifdef DEBUG
            std::string e = err.get_message();
            Strutil::printf("%s", e.empty() ? "unknown exception" : e.c_str());
#endif
        }
        m_ctx->set_primary_image(m_ihandle);
        Filesystem::IOFile ioproxy(m_filename, Filesystem::IOProxy::Write);
        if (ioproxy.mode() != Filesystem::IOProxy::Write) {
            errorf("Could not open \"%s\"", m_filename);
            ok = false;
        } else {
            MyHeifWriter writer(&ioproxy);
            m_ctx->write(writer);
        }
    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    }

    m_ctx.reset();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
