// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <libheif/heif_cxx.h>

#define MAKE_LIBHEIF_VERSION(a, b, c, d) \
    (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#if LIBHEIF_NUMERIC_VERSION >= MAKE_LIBHEIF_VERSION(1, 17, 0, 0)
#    include <libheif/heif_properties.h>
#endif



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
    bool valid_file(const std::string& filename) const override;
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
    bool m_reorient                = true;
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

OIIO_EXPORT const char* heif_input_extensions[] = { "heic", "heif", "heics",
                                                    "hif",  "avif", nullptr };

OIIO_PLUGIN_EXPORTS_END


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
    m_reorient = config.get_int_attribute("oiio:reorient", 1);

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
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
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

    auto id     = (subimage == 0) ? m_primary_id : m_item_ids[subimage - 1];
    m_ihandle   = m_ctx->get_image_handle(id);
    m_has_alpha = m_ihandle.has_alpha_channel();
    auto chroma = m_has_alpha ? heif_chroma_interleaved_RGBA
                              : heif_chroma_interleaved_RGB;
#if 0
    try {
        m_himage = m_ihandle.decode_image(heif_colorspace_RGB, chroma);
    } catch (const heif::Error& err) {
        std::string e = err.get_message();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        return false;
    } catch (const std::exception& err) {
        std::string e = err.what();
        errorfmt("{}", e.empty() ? "unknown exception" : e.c_str());
        return false;
    }
#else
    std::unique_ptr<heif_decoding_options, void (*)(heif_decoding_options*)>
        options(heif_decoding_options_alloc(), heif_decoding_options_free);
    options->ignore_transformations = !m_reorient;
    // print("Got decoding options version {}\n", options->version);
    struct heif_image* img_tmp = nullptr;
    struct heif_error herr = heif_decode_image(m_ihandle.get_raw_image_handle(),
                                               &img_tmp, heif_colorspace_RGB,
                                               chroma, options.get());
    if (img_tmp)
        m_himage = heif::Image(img_tmp);
    if (herr.code != heif_error_Ok || !img_tmp) {
        errorfmt("Could not decode image ({})", herr.message);
        m_ctx.reset();
        return false;
    }
#endif

    int bits = m_himage.get_bits_per_pixel(heif_channel_interleaved);
    m_spec   = ImageSpec(m_himage.get_width(heif_channel_interleaved),
                         m_himage.get_height(heif_channel_interleaved), bits / 8,
                         TypeUInt8);

    m_spec.set_colorspace("sRGB");

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
            print(
                "Don't know how to decode meta {} type='{}' contenttype='{}'\n",
                m, m_ihandle.get_metadata_type(m),
                m_ihandle.get_metadata_content_type(m));
            print("---\n{}\n---\n",
                  string_view((const char*)metacontents.data(),
                              metacontents.size()));
#endif
        }
    }

#if LIBHEIF_NUMERIC_VERSION >= MAKE_LIBHEIF_VERSION(1, 16, 0, 0)
    // Try to discover the orientation. The Exif is unreliable. We have to go
    // through the transformation properties ourselves. A tricky bit is that
    // the C++ API doesn't give us a direct way to get the context ptr, we
    // need to resort to some casting trickery, with knowledge that the C++
    // heif::Context class consists solely of a std::shared_ptr to a
    // heif_context.
    // NO int orientation = m_spec.get_int_attribute("Orientation", 1);
    int orientation = 1;
    const heif_context* raw_ctx
        = reinterpret_cast<std::shared_ptr<heif_context>*>(m_ctx.get())->get();
    int xpcount = heif_item_get_transformation_properties(raw_ctx, id, nullptr,
                                                          100);
    xpcount     = std::min(xpcount, 100);  // clamp to some reasonable limit
    std::vector<heif_property_id> xprops(xpcount);
    heif_item_get_transformation_properties(raw_ctx, id, xprops.data(),
                                            xpcount);
    for (int i = 0; i < xpcount; ++i) {
        auto type = heif_item_get_property_type(raw_ctx, id, xprops[i]);
        if (type == heif_item_property_type_transform_rotation) {
            int rot = heif_item_get_property_transform_rotation_ccw(raw_ctx, id,
                                                                    xprops[i]);
            // cw[] maps to one additional clockwise 90 degree turn
            static const int cw[] = { 0, 6, 7, 8, 5, 2, 3, 4, 1 };
            for (int i = 0; i < rot / 90; ++i)
                orientation = cw[orientation];
        } else if (type == heif_item_property_type_transform_mirror) {
            int mirror = heif_item_get_property_transform_mirror(raw_ctx, id,
                                                                 xprops[i]);
            //                                1  2  3  4  5  6  7  8
            static const int mirrorh[] = { 0, 2, 1, 4, 3, 6, 5, 8, 7 };
            static const int mirrorv[] = { 0, 4, 3, 2, 1, 8, 7, 6, 5 };
            if (mirror == heif_transform_mirror_direction_vertical) {
                orientation = mirrorv[orientation];
            } else if (mirror == heif_transform_mirror_direction_horizontal) {
                orientation = mirrorh[orientation];
            }
        }
    }
#else
    // Prior to libheif 1.16, the get_transformation_properties API was not
    // available, so we have to rely on the Exif orientation tag.
    int orientation = m_spec.get_int_attribute("Orientation", 1);
#endif

    // Erase the orientation metadata because libheif appears to be doing
    // the rotation-to-canonical-direction for us.
    if (orientation != 1) {
        if (m_reorient) {
            // If libheif auto-reoriented, record the original orientation in
            // "oiio:OriginalOrientation" and set the "Orientation" attribute
            // to 1 since we're presenting the image to the caller in the
            // usual orientation.
            m_spec.attribute("oiio:OriginalOrientation", orientation);
            m_spec.attribute("Orientation", 1);
        } else {
            // libheif supplies oriented width & height, so if we are NOT
            // auto-reorienting and it's one of the orientations that swaps
            // width and height, we need to do that swap ourselves.
            // Note: all the orientations that swap width and height are 5-8,
            // whereas 1-4 preserve aspect ratio.
            if (orientation >= 5) {
                std::swap(m_spec.width, m_spec.height);
                std::swap(m_spec.full_width, m_spec.full_height);
            }
        }
    }

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
        errorfmt("Unknown read error");
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
