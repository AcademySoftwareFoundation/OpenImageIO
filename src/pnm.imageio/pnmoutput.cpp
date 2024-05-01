// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <fstream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


class PNMOutput final : public ImageOutput {
public:
    PNMOutput() { init(); }
    ~PNMOutput() override { close(); }
    const char* format_name(void) const override { return "pnm"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;  // Stash the filename
    unsigned int m_max_val, m_pnm_type;
    unsigned int m_dither;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;

    void init(void) { ioproxy_clear(); }

    bool write_ascii_binary(const unsigned char* data, const stride_t stride);
    bool write_raw_binary(const unsigned char* data, const stride_t stride);

    template<class T>
    bool write_ascii(const T* data, const stride_t stride,
                     unsigned int max_val);
    template<class T>
    bool write_raw(const T* data, const stride_t stride, unsigned int max_val);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
pnm_output_imageio_create()
{
    return new PNMOutput;
}

OIIO_EXPORT const char* pnm_output_extensions[] = { "ppm", "pgm", "pbm", "pnm",
                                                    nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
PNMOutput::write_ascii_binary(const unsigned char* data, const stride_t stride)
{
    for (int x = 0; x < m_spec.width; x++)
        if (!iowritefmt("{}\n", data[x * stride] ? '0' : '1'))
            return false;
    return true;
}



bool
PNMOutput::write_raw_binary(const unsigned char* data, const stride_t stride)
{
    for (int x = 0; x < m_spec.width;) {
        unsigned char val = 0;
        for (int bit = 7; bit >= 0 && x < m_spec.width; x++, bit--)
            val += (data[x * stride] ? 0 : (1 << bit));
        if (!iowrite(&val, sizeof(val)))
            return false;
    }
    return true;
}



template<class T>
bool
PNMOutput::write_ascii(const T* data, const stride_t stride,
                       unsigned int max_val)
{
    int nc = m_spec.nchannels;
    for (int x = 0; x < m_spec.width; x++) {
        unsigned int pixel = x * stride;
        for (int c = 0; c < nc; c++) {
            unsigned int val = data[pixel + c];
            val              = val * max_val / std::numeric_limits<T>::max();
            if (!iowritefmt("{}\n", val))
                return false;
        }
    }
    return true;
}



template<class T>
bool
PNMOutput::write_raw(const T* data, const stride_t stride, unsigned int max_val)
{
    int nc = m_spec.nchannels;
    for (int x = 0; x < m_spec.width; x++) {
        unsigned int pixel = x * stride;
        for (int c = 0; c < nc; c++) {
            unsigned int val = data[pixel + c];
            val              = val * max_val / std::numeric_limits<T>::max();
            if (sizeof(T) == 2) {
                // Writing a 16bit ppm file
                // I'll adopt the practice of Netpbm and write the MSB first
                uint8_t byte[2] = { static_cast<uint8_t>(val >> 8),
                                    static_cast<uint8_t>(val & 0xff) };
                if (!iowrite(&byte, 2))
                    return false;
            } else {
                // This must be an 8bit ppm file
                uint8_t byte = static_cast<uint8_t>(val);
                if (!iowrite(&byte, 1))
                    return false;
            }
        }
    }
    return true;
}



bool
PNMOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (!check_open(mode, userspec, { 0, 65535, 0, 65535, 0, 1, 0, 3 },
                    uint64_t(OpenChecks::Disallow2Channel)))
        return false;

    m_spec.set_format(TypeDesc::UINT8);  // Force 8 bit output
    int bits_per_sample = m_spec.get_int_attribute("oiio:BitsPerSample", 8);
    m_dither            = (m_spec.format == TypeDesc::UINT8)
                              ? m_spec.get_int_attribute("oiio:dither", 0)
                              : 0;

    if (bits_per_sample == 1)
        m_pnm_type = 4;
    else if (m_spec.nchannels == 1)
        m_pnm_type = 5;
    else
        m_pnm_type = 6;
    if (!m_spec.get_int_attribute("pnm:binary", 1))
        m_pnm_type -= 3;

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    m_max_val = (1 << bits_per_sample) - 1;
    // Write header
    bool ok = true;
    ok &= iowritefmt("P{}\n", m_pnm_type);
    ok &= iowritefmt("{} {}\n", m_spec.width, m_spec.height);
    if (m_pnm_type != 1 && m_pnm_type != 4)  // only non-monochrome
        ok &= iowritefmt("{}\n", m_max_val);

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return ok;
}



bool
PNMOutput::close()
{
    if (!ioproxy_opened())  // already closed
        return true;

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_DASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        m_tilebuffer.shrink_to_fit();
    }

    init();
    return ok;
}



bool
PNMOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    if (!ioproxy_opened())
        return false;
    if (z)
        return false;

    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data != origdata)  // a conversion happened...
        xstride = spec().nchannels;

    switch (m_pnm_type) {
    case 1: return write_ascii_binary((unsigned char*)data, xstride);
    case 2:
    case 3:
        if (m_max_val > std::numeric_limits<unsigned char>::max())
            return write_ascii((unsigned short*)data, xstride, m_max_val);
        else
            return write_ascii((unsigned char*)data, xstride, m_max_val);
    case 4: return write_raw_binary((unsigned char*)data, xstride);
    case 5:
    case 6:
        if (m_max_val > std::numeric_limits<unsigned char>::max())
            return write_raw((unsigned short*)data, xstride, m_max_val);
        else
            return write_raw((unsigned char*)data, xstride, m_max_val);
    default: return false;
    }

    return false;
}



bool
PNMOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END
