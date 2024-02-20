// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <libheif/heif_cxx.h>


// This plugin utilises libheif:
//   https://github.com/strukturag/libheif
//
// General information about HEIF/HEIC/AVIF:
//
// Sources of sample images:
//     https://github.com/nokiatech/heif/tree/gh-pages/content


OIIO_PLUGIN_NAMESPACE_BEGIN

class HeifInput final : public ImageInput {
public:
    HeifInput() {}
    ~HeifInput() override { close(); }
    const char* format_name(void) const override { return "heif"; }
    int supports(string_view feature) const override
    {
        return feature == "exif";
    }
#if LIBHEIF_HAVE_VERSION(1, 4, 0)
    bool valid_file(const std::string& filename) const override;
#endif
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override;
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_scanline(int y, int z, TypeDesc format, void* data,
                       stride_t xstride) override;

private:
    std::string m_filename;
    int m_subimage                 = -1;
    int m_num_subimages            = 0;
    int m_has_alpha                = false;
    bool m_associated_alpha        = true;
    bool m_keep_unassociated_alpha = false;
    bool m_do_associate            = false;
    std::unique_ptr<heif::Context> m_ctx;
    heif_item_id m_primary_id;             // id of primary image
    std::vector<heif_item_id> m_item_ids;  // ids of all other images
    heif::ImageHandle m_ihandle;
    heif::Image m_himage;
};



void
oiio_heif_init()
{
#if LIBHEIF_HAVE_VERSION(1, 16, 0)
    static std::once_flag flag;
    std::call_once(flag, []() { heif_init(nullptr); });
#endif
}



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int heif_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
heif_imageio_library_version()
{
    return "libheif " LIBHEIF_VERSION;
}

OIIO_EXPORT ImageInput*
heif_input_imageio_create()
{
    oiio_heif_init();
    return new HeifInput;
}

OIIO_EXPORT const char* heif_input_extensions[] = { "heic",  "heif",
                                                    "heics", "hif",
#if LIBHEIF_HAVE_VERSION(1, 7, 0)
                                                    "avif",
#endif
                                                    nullptr };

OIIO_PLUGIN_EXPORTS_END


#if LIBHEIF_HAVE_VERSION(1, 4, 0)
bool
HeifInput::valid_file(const std::string& filename) const
{
    uint8_t magic[12];
    if (Filesystem::read_bytes(filename, magic, sizeof(magic)) != sizeof(magic))
        return false;
    heif_filetype_result filetype_check = heif_check_filetype(magic,
                                                              sizeof(magic));
    return filetype_check != heif_filetype_no
           && filetype_check != heif_filetype_yes_unsupported;
}
#endif



bool
HeifInput::open(const std::string& name, ImageSpec& newspec)
{
    // If user doesn't want to provide any config, just use an empty spec.
    ImageSpec config;
    return open(name, newspec, config);
}



bool
HeifInput::open(const std::string& name, ImageSpec& newspec,
                const ImageSpec& config)
{
    m_filename = name;
    m_subimage = -1;

    m_ctx.reset(new heif::Context);
    m_himage  = heif::Image();
    m_ihandle = heif::ImageHandle();

    m_keep_unassociated_alpha
        = (config.get_int_attribute("oiio:UnassociatedAlpha") != 0);

    try {
        m_ctx->read_from_file(name);
        // FIXME: should someday be read_from_reader to give full flexibility

        m_item_ids   = m_ctx->get_list_of_top_level_image_IDs();
        m_primary_id = m_ctx->get_primary_image_ID();
        for (size_t i = 0; i < m_item_ids.size(); ++i)
            if (m_item_ids[i] == m_primary_id) {
                m_item_ids.erase(m_item_ids.begin() + i);
                break;
            }
        // std::cout << " primary id: " << m_primary_id << "\n";
        // std::cout << " item ids: " << Strutil::join(m_item_ids, ", ") << "\n";
        m_num_subimages = 1 + int(m_item_ids.size());

    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    }

    bool ok = seek_subimage(0, 0);
    newspec = spec();
    return ok;
}



bool
HeifInput::close()
{
    m_himage  = heif::Image();
    m_ihandle = heif::ImageHandle();
    m_ctx.reset();
    m_subimage                = -1;
    m_num_subimages           = 0;
    m_associated_alpha        = true;
    m_keep_unassociated_alpha = false;
    m_do_associate            = false;
    return true;
}



bool
HeifInput::seek_subimage(int subimage, int miplevel)
{
    if (miplevel != 0)
        return false;

    if (subimage == m_subimage) {
        return true;  // already there
    }

    if (subimage >= m_num_subimages) {
        return false;
    }

    try {
        auto id     = (subimage == 0) ? m_primary_id : m_item_ids[subimage - 1];
        m_ihandle   = m_ctx->get_image_handle(id);
        m_has_alpha = m_ihandle.has_alpha_channel();
        auto chroma = m_has_alpha ? heif_chroma_interleaved_RGBA
                                  : heif_chroma_interleaved_RGB;
        m_himage    = m_ihandle.decode_image(heif_colorspace_RGB, chroma);

    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorf("%s", e.empty() ? "unknown exception" : e.c_str());
        return false;
    }

    int bits = m_himage.get_bits_per_pixel(heif_channel_interleaved);
    m_spec = ImageSpec(m_ihandle.get_width(), m_ihandle.get_height(), bits / 8,
                       TypeUInt8);

    m_spec.attribute("oiio:ColorSpace", "sRGB");

#if LIBHEIF_HAVE_VERSION(1, 12, 0)
    // Libheif >= 1.12 added API call to find out if the image is associated
    // alpha (i.e. colors are premultiplied).
    m_associated_alpha = m_himage.is_premultiplied_alpha();
    m_do_associate     = (!m_associated_alpha && m_spec.alpha_channel >= 0
                      && !m_keep_unassociated_alpha);
    if (!m_associated_alpha && m_spec.nchannels >= 4) {
        // Indicate that file stored unassociated alpha data
        m_spec.attribute("heif:UnassociatedAlpha", 1);
        // If we don't have 4 chans, we need not consider
        m_keep_unassociated_alpha &= (m_spec.nchannels >= 4);
        if (m_keep_unassociated_alpha) {
            // Indicate that we are returning unassociated data if the file
            // had associated and we were asked to keep it that way.
            m_spec.attribute("oiio:UnassociatedAlpha", 1);
        }
    }
#else
    m_associated_alpha = true;  // assume/hope
#endif

    auto meta_ids = m_ihandle.get_list_of_metadata_block_IDs();
    // std::cout << "nmeta? " << meta_ids.size() << "\n";
    for (auto m : meta_ids) {
        std::vector<uint8_t> metacontents;
        try {
            metacontents = m_ihandle.get_metadata(m);
        } catch (const heif::Error& err) {
            if (err.get_code() == heif_error_Usage_error
                && err.get_subcode() == heif_suberror_Null_pointer_argument) {
                // a bug in heif_cxx.h means a 0 byte metadata causes a null
                // ptr error code, which we ignore
                continue;
            }
        }
        if (Strutil::iequals(m_ihandle.get_metadata_type(m), "Exif")
            && metacontents.size() >= 10) {
            cspan<uint8_t> s(&metacontents[10], metacontents.size() - 10);
            decode_exif(s, m_spec);
        } else if (0  // For now, skip this, I haven't seen anything useful
                   && Strutil::iequals(m_ihandle.get_metadata_type(m), "mime")
                   && Strutil::iequals(m_ihandle.get_metadata_content_type(m),
                                       "application/rdf+xml")) {
            decode_xmp(metacontents, m_spec);
        } else {
#ifdef DEBUG
            std::cout << "Don't know how to decode meta " << m
                      << " type=" << m_ihandle.get_metadata_type(m)
                      << " contenttype='"
                      << m_ihandle.get_metadata_content_type(m) << "'\n";
            std::cout << "---\n"
                      << string_view((const char*)&metacontents[0],
                                     metacontents.size())
                      << "\n---\n";
#endif
        }
    }

    // Erase the orientation metadata because libheif appears to be doing
    // the rotation-to-canonical-direction for us.
    m_spec.erase_attribute("Orientation");

    m_subimage = subimage;
    return true;
}



bool
HeifInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                                void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;

    int ystride          = 0;
    const uint8_t* hdata = m_himage.get_plane(heif_channel_interleaved,
                                              &ystride);
    if (!hdata) {
        errorf("Unknown read error");
        return false;
    }
    hdata += (y - m_spec.y) * ystride;
    memcpy(data, hdata, m_spec.width * m_spec.pixel_bytes());
    return true;
}



bool
HeifInput::read_scanline(int y, int z, TypeDesc format, void* data,
                         stride_t xstride)
{
    bool ok = ImageInput::read_scanline(y, z, format, data, xstride);
    if (ok && m_do_associate) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        {
            lock_guard lock(*this);
            if (format == TypeUnknown)  // unknown -> retrieve native type
                format = m_spec.format;
        }
        OIIO::premult(m_spec.nchannels, m_spec.width, 1, 1, 0 /*chbegin*/,
                      m_spec.nchannels /*chend*/, format, data, xstride,
                      AutoStride, AutoStride, m_spec.alpha_channel);
    }
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
