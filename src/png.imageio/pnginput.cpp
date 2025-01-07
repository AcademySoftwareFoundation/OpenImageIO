// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "png_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


class PNGInput final : public ImageInput {
public:
    PNGInput() { init(); }
    ~PNGInput() override { close(); }
    const char* format_name(void) const override { return "png"; }
    int supports(string_view feature) const override
    {
        return (feature == "ioproxy" || feature == "exif");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override;
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

private:
    std::string m_filename;            ///< Stash the filename
    png_structp m_png;                 ///< PNG read structure pointer
    png_infop m_info;                  ///< PNG image info structure pointer
    int m_bit_depth;                   ///< PNG bit depth
    int m_color_type;                  ///< PNG color model type
    int m_interlace_type;              ///< PNG interlace type
    std::vector<unsigned char> m_buf;  ///< Buffer the image pixels
    int m_subimage;                    ///< What subimage are we looking at?
    Imath::Color3f m_bg;               ///< Background color
    int m_next_scanline;
    bool m_keep_unassociated_alpha;  ///< Do not convert unassociated alpha
    bool m_linear_premult;           ///< Do premult for sRGB images in linear
    bool m_srgb   = false;           ///< It's an sRGB image (not gamma)
    bool m_err    = false;
    float m_gamma = 1.0f;
    std::unique_ptr<ImageSpec> m_config;  // Saved copy of configuration spec

    /// Reset everything to initial state
    ///
    void init()
    {
        m_subimage = -1;
        m_png      = nullptr;
        m_info     = nullptr;
        m_buf.clear();
        m_next_scanline           = 0;
        m_keep_unassociated_alpha = false;
        m_linear_premult = OIIO::get_int_attribute("png:linear_premult");
        m_srgb           = false;
        m_err            = false;
        m_gamma          = 1.0;
        m_config.reset();
        ioproxy_clear();
    }

    /// Helper function: read the image.
    ///
    bool readimg();

    /// Extract the background color.
    ///
    bool get_background(float* red, float* green, float* blue);

    // Callback for PNG that reads from an IOProxy.
    static void PngReadCallback(png_structp png_ptr, png_bytep data,
                                png_size_t length)
    {
        PNGInput* pnginput = (PNGInput*)png_get_io_ptr(png_ptr);
        OIIO_DASSERT(pnginput);
        if (!pnginput->ioread(data, length)) {
            pnginput->m_err = true;
            png_chunk_error(png_ptr, pnginput->geterror(false).c_str());
        }
    }

    template<class T>
    void associateAlpha(T* data, int size, int channels, int alpha_channel,
                        bool srgb, float gamma);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
png_input_imageio_create()
{
    return new PNGInput;
}

OIIO_EXPORT int png_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
png_imageio_library_version()
{
    return "libpng " PNG_LIBPNG_VER_STRING;
}

OIIO_EXPORT const char* png_input_extensions[] = { "png", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
PNGInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    unsigned char sig[8] {};
    const size_t numRead = ioproxy->pread(sig, sizeof(sig), 0);
    return numRead == sizeof(sig) && png_sig_cmp(sig, 0, 8) == 0;
}



bool
PNGInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;
    m_subimage = 0;

    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    unsigned char sig[8];
    if (ioproxy()->pread(sig, sizeof(sig), 0) != sizeof(sig)
        || png_sig_cmp(sig, 0, 8)) {
        if (!has_error())
            errorfmt("Not a PNG file");
        return false;  // Read failed
    }

    std::string s = PNG_pvt::create_read_struct(m_png, m_info, this);
    if (s.length()) {
        close();
        if (!has_error())
            errorfmt("{}", s);
        return false;
    }

    // Tell libpng to use our read callback to read from the IOProxy
    png_set_read_fn(m_png, this, PngReadCallback);

    bool ok = PNG_pvt::read_info(m_png, m_info, m_bit_depth, m_color_type,
                                 m_interlace_type, m_bg, m_spec,
                                 m_keep_unassociated_alpha);
    if (!ok || m_err
        || !check_open(m_spec, { 0, 1 << 16, 0, 1 << 16, 0, 1, 0, 4 })) {
        close();
        return false;
    }

    m_gamma                = m_spec.get_float_attribute("oiio:Gamma", 1.0f);
    string_view colorspace = m_spec.get_string_attribute("oiio:ColorSpace",
                                                         "sRGB");
    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    m_srgb = colorconfig.equivalent(colorspace, "sRGB");

    newspec         = spec();
    m_next_scanline = 0;

    return ok;
}



bool
PNGInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    m_linear_premult = config.get_int_attribute("png:linear_premult",
                                                OIIO::get_int_attribute(
                                                    "png:linear_premult"));
    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



bool
PNGInput::readimg()
{
    std::string s = PNG_pvt::read_into_buffer(m_png, m_info, m_spec, m_buf);
    if (s.length() || m_err || has_error()) {
        close();
        if (!has_error())
            errorfmt("{}", s);
        return false;
    }

    return true;
}



bool
PNGInput::close()
{
    PNG_pvt::destroy_read_struct(m_png, m_info);
    init();  // Reset to initial state
    return true;
}



template<class T>
void
PNGInput::associateAlpha(T* data, int size, int channels, int alpha_channel,
                         bool srgb, float gamma)
{
    // We need to transform to linear space, associate the alpha, and then
    // transform back.
    if (srgb && m_linear_premult) {
        // sRGB with request to do premult in linear space
        for (int x = 0; x < size; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 1.0f) {
                for (int c = 0; c < channels; c++) {
                    if (c != alpha_channel) {
                        float f = sRGB_to_linear(val[c]);
                        val[c]  = linear_to_sRGB(f * alpha);
                    }
                }
            }
        }
    } else if (gamma != 1.0f && m_linear_premult) {
        // Gamma correction with request to do premult in linear space
        float inv_gamma = 1.0f / gamma;
        for (int x = 0; x < size; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 1.0f) {
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        val[c] = powf((powf(val[c], gamma)) * alpha, inv_gamma);
            }
        }
    } else {
        // Do the premult directly on the values. This is correct for the
        // "gamma=1" case, and is also commonly what is needed for many sRGB
        // images (even though it's technically wrong in that case).
        for (int x = 0; x < size; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 1.0f) {
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        val[c] = val[c] * alpha;
            }
        }
    }
}



bool
PNGInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    y -= m_spec.y;
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;

    if (m_interlace_type != 0) {
        // Interlaced.  Punt and read the whole image
        if (m_buf.empty()) {
            if (has_error() || !readimg())
                return false;
        }
        size_t size = spec().scanline_bytes();
        memcpy(data, &m_buf[0] + y * size, size);
    } else {
        // Not an interlaced image -- read just one row
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
                || !seek_subimage(subimage, miplevel))
                return false;  // Somehow, the re-open failed
            assert(m_next_scanline == 0 && current_subimage() == subimage);
        }
        while (m_next_scanline <= y) {
            // Keep reading until we're read the scanline we really need
            // std::cerr << "reading scanline " << m_next_scanline << "\n";
            std::string s = PNG_pvt::read_next_scanline(m_png, data);
            if (s.length()) {
                errorfmt("{}", s);
                return false;
            }
            if (m_err)
                return false;  // error is already registered
            ++m_next_scanline;
        }
    }

    // PNG specifically dictates unassociated (un-"premultiplied") alpha.
    // Convert to associated unless we were requested not to do so.
    if (m_spec.alpha_channel != -1 && !m_keep_unassociated_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            associateAlpha((unsigned short*)data, m_spec.width,
                           m_spec.nchannels, m_spec.alpha_channel, m_srgb,
                           m_gamma);
        else
            associateAlpha((unsigned char*)data, m_spec.width, m_spec.nchannels,
                           m_spec.alpha_channel, m_srgb, m_gamma);
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END
