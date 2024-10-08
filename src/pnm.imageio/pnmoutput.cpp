// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>
#include <fstream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)

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
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride = AutoStride,
                         stride_t ystride = AutoStride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;  // Stash the filename
    unsigned int m_max_val;
    unsigned int m_pnm_type;
    std::string m_pfn_type;

    unsigned int m_dither;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;

    void init(void) { ioproxy_clear(); }

    bool write_ascii_binary(const unsigned char* data, const stride_t stride);
    bool write_raw_binary(const unsigned char* data, const stride_t stride);
    bool write_float(const void* data, TypeDesc format, const stride_t stride);

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

OIIO_EXPORT const char* pnm_output_extensions[] = { "ppm", "pgm", "pbm",
                                                    "pnm", "pfm", nullptr };

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
    DBG std::cerr << "PNMOutput::write_ascii()\n";
    int nc   = m_spec.nchannels;
    bool big = m_spec.get_int_attribute("pnm:bigendian", 0) == 1;
    DBG std::cerr << "bigendian: " << big << "\n";
    stride_t m_stride = stride / sizeof(T);

    for (int x = 0; x < m_spec.width; x++) {
        unsigned int pixel = x * m_stride;
        for (int c = 0; c < nc; c++) {
            unsigned int val = data[pixel + c];
            val              = val * max_val / std::numeric_limits<T>::max();
            if (big)
                swap_endian(&val, 1);
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
    //DBG std::cerr << "PNMOutput::write_raw()\n";
    int nc            = m_spec.nchannels;
    bool big          = m_spec.get_int_attribute("pnm:bigendian", 0) == 1;
    stride_t m_stride = stride / sizeof(T);

    for (int x = 0; x < m_spec.width; x++) {
        unsigned int pixel = x * m_stride;
        for (int c = 0; c < nc; c++) {
            unsigned int val = data[pixel + c];
            val              = val * max_val / std::numeric_limits<T>::max();
            if (sizeof(T) == 2) {
                // Writing a 16bit ppm file
                uint8_t byte[2] = { big ? static_cast<uint8_t>(val & 0xff)
                                        : static_cast<uint8_t>(val >> 8),
                                    big ? static_cast<uint8_t>(val >> 8)
                                        : static_cast<uint8_t>(val & 0xff) };
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
PNMOutput::write_float(const void* data, TypeDesc format, const stride_t stride)
{
    int nc   = m_spec.nchannels;
    bool big = m_spec.get_int_attribute("pnm:bigendian", 0) == 1;
    stride_t m_stride;

    switch (format.basetype) {
    case TypeDesc::HALF:
        m_stride = stride / sizeof(half);
        for (int x = 0; x < m_spec.width; x++) {
            unsigned int pixel = x * m_stride;
            for (int c = 0; c < nc; c++) {
                half* d   = (half*)data;
                float val = static_cast<float>(d[pixel + c]);
                if (big)
                    swap_endian(&val, 1);
                if (!iowrite(&val, sizeof(val)))
                    return false;
            }
        }
        break;
    case TypeDesc::FLOAT:
        m_stride = stride / sizeof(float);
        for (int x = 0; x < m_spec.width; x++) {
            unsigned int pixel = x * m_stride;
            for (int c = 0; c < nc; c++) {
                float* d  = (float*)data;
                float val = d[pixel + c];
                if (big)
                    swap_endian(&val, 1);
                if (!iowrite(&val, sizeof(val)))
                    return false;
            }
        }
        break;
    case TypeDesc::DOUBLE:
        m_stride = stride / sizeof(double);
        for (int x = 0; x < m_spec.width; x++) {
            unsigned int pixel = x * m_stride;
            for (int c = 0; c < nc; c++) {
                double* d = (double*)data;
                float val = static_cast<float>(d[pixel + c]);
                if (big)
                    swap_endian(&val, 1);
                if (!iowrite(&val, sizeof(val)))
                    return false;
            }
        }
        break;
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

    DBG std::cerr << "Bit depth: " << m_spec.format << "\n";

    m_pnm_type = 0;
    m_pfn_type = "";

    int bits_per_sample = 0;
    if (m_spec.find_attribute("oiio:BitsPerSample")) {
        bits_per_sample = m_spec.get_int_attribute("oiio:BitsPerSample", 8);
    }

    bool p_binary = m_spec.get_int_attribute("pnm:binary", 1);
    DBG std::cerr << "p_binary: " << p_binary << "\n";

    if (bits_per_sample == 1) {
        // black and white
        m_pnm_type = p_binary ? 4 : 1;
    } else if (bits_per_sample == 8 || bits_per_sample == 16) {
        // 8 or 16 bit
        m_pnm_type = (m_spec.nchannels == 1) ? (p_binary ? 5 : 2)
                                             : (p_binary ? 6 : 3);
    } else if (bits_per_sample == 32) {
        // 32 bit float
        m_pfn_type = (m_spec.nchannels == 1) ? "f" : "F";
    } else if (bits_per_sample == 0) {
        switch (m_spec.format.basetype) {
        case TypeDesc::UINT8:
        case TypeDesc::UINT16:
            m_pnm_type = (m_spec.nchannels == 1) ? (p_binary ? 5 : 2)
                                                 : (p_binary ? 6 : 3);
            break;
        case TypeDesc::HALF:
        case TypeDesc::FLOAT:
        case TypeDesc::DOUBLE:
            m_pfn_type = (m_spec.nchannels == 1) ? "f" : "F";
            break;
        default:
            errorfmt("PNM does not support {}\n", m_spec.format.c_str());
            return false;
        }
    } else {
        errorfmt("PNM does not support {}\n", m_spec.format.c_str());
        return false;
    }

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // Write header
    bool ok = true;
    if (bits_per_sample != 0) {
        // user specified bits per sample
        if (m_pfn_type == "") {
            ok &= iowritefmt("P{}\n", m_pnm_type);
            m_max_val = (bits_per_sample == 16) ? 65535 : 255;
        } else {
            ok &= iowritefmt("P{}\n", m_pfn_type);
        }
    } else {
        // no bits per sample specified
        if (m_pfn_type == "") {
            m_max_val = (m_spec.format == TypeDesc::UINT16) ? 65535 : 255;
            ok &= iowritefmt("P{}\n", m_pnm_type);
        } else {
            ok &= iowritefmt("P{}\n", m_pfn_type);
        }
    }

    ok &= iowritefmt("{} {}\n", m_spec.width, m_spec.height);


    if (m_pnm_type != 1 && m_pnm_type != 4) {  // only non-monochrome
        if (m_pfn_type == "")
            ok &= iowritefmt("{}\n", m_max_val);
        else {
            bool big = m_spec.get_int_attribute("pnm:bigendian", 0) == 1;
            std::string scale = big ? "1.0000" : "-1.0000";
            ok &= iowritefmt("{}\n", scale);
        }
    }
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
        ok &= ImageOutput::write_scanlines(m_spec.y, m_spec.y + m_spec.height,
                                           0, m_spec.format, &m_tilebuffer[0]);
        m_tilebuffer.shrink_to_fit();
    }

    init();
    return ok;
}



bool
PNMOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    //DBG std::cerr << "PNMOutput::write_scanline()\n";
    if (!ioproxy_opened())
        return false;
    if (z)
        return false;

    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data != origdata) {  // a conversion happened...
        xstride = m_spec.pixel_bytes(true);
    }

    switch (m_pnm_type) {
    case 0:
        if (m_pfn_type != "") {
            return write_float(data, m_spec.format, xstride);
        } else
            return false;
    case 1: return write_ascii_binary((unsigned char*)data, xstride);
    case 2:
    case 3:
        if (m_max_val > std::numeric_limits<unsigned char>::max()) {
            return write_ascii((unsigned short*)data, xstride, m_max_val);
        } else {
            return write_ascii((unsigned char*)data, xstride, m_max_val);
        }
    case 4: return write_raw_binary((unsigned char*)data, xstride);
    case 5:
    case 6:
        if (m_max_val > std::numeric_limits<unsigned char>::max()) {
            return write_raw((unsigned short*)data, xstride, m_max_val);
        } else {
            return write_raw((unsigned char*)data, xstride, m_max_val);
        }
    default: return false;
    }

    return false;
}



bool
PNMOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
    DBG std::cerr << "PNMOutput::write_scanlines()\n";

    if (m_spec.get_int_attribute("pnm:pfmflip", 1) == 1
        && format == TypeDesc::FLOAT) {
        DBG std::cerr << "Flipping PFM vertically\n";

        // Default implementation: write each scanline individually
        stride_t native_pixel_bytes = (stride_t)m_spec.pixel_bytes(true);
        if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
            xstride = native_pixel_bytes;
        stride_t zstride = AutoStride;
        m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                           m_spec.width, yend - ybegin);

        // start at the last scanline
        data    = (char*)data + (yend - ybegin - 1) * ystride;
        bool ok = true;
        for (int y = ybegin; ok && y < yend; ++y) {
            ok &= write_scanline(y, z, format, data, xstride);
            data = (char*)data - ystride;
        }
        return ok;
    }

    return ImageOutput::write_scanlines(ybegin, yend, z, format, data, xstride,
                                        ystride);
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
