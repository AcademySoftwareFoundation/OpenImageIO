// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/typedesc.h>

#include "imageio_pvt.h"
#include "targa_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace TGA_pvt;

#if 0 /* allow tga debugging */
static bool tgadebug = Strutil::stoi(Sysutil::getenv("OIIO_TARGA_DEBUG"));
#    define DBG(...)  \
        if (tgadebug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif



class TGAInput final : public ImageInput {
public:
    TGAInput() { init(); }
    ~TGAInput() override { close(); }
    const char* format_name(void) const override { return "targa"; }
    int supports(string_view feature) const override
    {
        return (feature == "thumbnail" || feature == "ioproxy");
    }
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool get_thumbnail(ImageBuf& thumb, int subimage) override;

private:
    std::string m_filename;            // Stash the filename
    tga_header m_tga;                  // Targa header
    tga_footer m_foot;                 // Targa 2.0 footer
    tga_alpha_type m_alpha_type;       // Alpha type
    int64_t m_ofs_thumb       = 0;     // Offset of thumbnail info
    int64_t m_ofs_palette     = 0;     // Offset of palette
    int64_t m_ofs_colcorr_tbl = 0;     // Offset to colour correction table
    bool m_keep_unassociated_alpha;    // Do not convert unassociated alpha
    short m_tga_version = 1;           // TGA version (1 or 2)
    std::unique_ptr<uint8_t[]> m_buf;  // Buffer the image pixels

    // Is this a palette image, i.e. it has a color map?
    bool is_palette() const { return m_tga.cmap_type != 0; }

    /// Reset everything to initial state
    ///
    void init()
    {
        m_buf.reset();
        m_ofs_thumb               = 0;
        m_ofs_palette             = 0;
        m_ofs_colcorr_tbl         = 0;
        m_alpha_type              = TGA_ALPHA_NONE;
        m_keep_unassociated_alpha = false;
        m_tga_version             = 1;
        ioproxy_clear();
    }

    // Helper: read the tga 2.0 specific parts of the header
    bool read_tga2_header();

    // Helper function: read the image.
    bool readimg();

    /// Helper function: decode a pixel.
    inline bool decode_pixel(unsigned char* in, unsigned char* out,
                             unsigned char* palette, int bytespp,
                             int palbytespp, size_t palette_alloc_size);

    // Read one byte
    bool read(uint8_t& buf) { return ioread(&buf, sizeof(buf), 1); }

    // Read one byte
    bool read(char& buf) { return ioread(&buf, sizeof(buf), 1); }

    // Read a short, byte swap as necessary.
    bool read(uint16_t& buf)
    {
        bool ok = ioread(&buf, sizeof(buf), 1);
        if (bigendian())  // TGAs are always little-endian
            swap_endian(&buf);
        return ok;
    }

    // Read an int, byte swap as necessary.
    bool read(uint32_t& buf)
    {
        bool ok = ioread(&buf, sizeof(buf), 1);
        if (bigendian())  // TGAs are always little-endian
            swap_endian(&buf);
        return ok;
    }

    // Read maxlen bytes from the file, if it doesn't start with 0, add the
    // contents as attribute `name`. Return true if the read was successful,
    // false if there was an error reading from the file. The attribute is
    // only added if the string is non-empty.
    bool read_bytes_for_string_attribute(string_view name, size_t maxlen)
    {
        char* buf = OIIO_ALLOCA(char, maxlen);
        OIIO_DASSERT(buf != nullptr);
        if (!ioread(buf, maxlen))
            return false;
        if (buf[0])
            m_spec.attribute(name, Strutil::safe_string_view(buf, maxlen));
        return true;
    }
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
targa_input_imageio_create()
{
    return new TGAInput;
}

OIIO_EXPORT int targa_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char* targa_input_extensions[] = { "tga", "tpic", nullptr };

OIIO_EXPORT const char*
targa_imageio_library_version()
{
    return nullptr;
}

OIIO_PLUGIN_EXPORTS_END



bool
TGAInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;

    DBG("TGA opening {}\n", name);
    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    // Due to struct packing, we may get a corrupt header if we just load the
    // struct from file; to address that, read every member individually save
    // some typing. Byte swapping is done automatically. If any fail, the file
    // handle is closed and we return false from open().
    if (!(read(m_tga.idlen) && read(m_tga.cmap_type) && read(m_tga.type)
          && read(m_tga.cmap_first) && read(m_tga.cmap_length)
          && read(m_tga.cmap_size) && read(m_tga.x_origin)
          && read(m_tga.y_origin) && read(m_tga.width) && read(m_tga.height)
          && read(m_tga.bpp) && read(m_tga.attr))) {
        errorfmt("Could not read full header");
        return false;
    }

    if (m_tga.cmap_type != 0 && m_tga.cmap_type != 1) {
        errorfmt("Illegal cmap_type value {} in header", m_tga.cmap_type);
        return false;
    }
    if (m_tga.type == TYPE_NODATA) {
        errorfmt("Image with no data");
        return false;
    }
    if (m_tga.type != TYPE_PALETTED && m_tga.type != TYPE_RGB
        && m_tga.type != TYPE_GRAY && m_tga.type != TYPE_PALETTED_RLE
        && m_tga.type != TYPE_RGB_RLE && m_tga.type != TYPE_GRAY_RLE) {
        errorfmt("Illegal image type: {}", m_tga.type);
        return false;
    }
    if (m_tga.bpp != 8 && m_tga.bpp != 15 && m_tga.bpp != 16 && m_tga.bpp != 24
        && m_tga.bpp != 32) {
        errorfmt("Illegal pixel size: {} bits per pixel", m_tga.bpp);
        return false;
    }

    if ((m_tga.type == TYPE_PALETTED || m_tga.type == TYPE_PALETTED_RLE)
        && !is_palette()) {
        errorfmt("Palette image with no palette");
        return false;
    }

    if (is_palette()) {
        if (m_tga.type == TYPE_GRAY || m_tga.type == TYPE_GRAY_RLE) {
            // it should be an error for TYPE_RGB* as well, but apparently some
            // *very* old TGAs can be this way, so we'll hack around it
            errorfmt("Palette defined for grayscale image");
            return false;
        }
        if (m_tga.cmap_size != 15 && m_tga.cmap_size != 16
            && m_tga.cmap_size != 24 && m_tga.cmap_size != 32) {
            errorfmt("Illegal palette entry size: {} bits", m_tga.cmap_size);
            return false;
        }
    }

    m_alpha_type = TGA_ALPHA_NONE;
    if (((m_tga.type == TYPE_RGB || m_tga.type == TYPE_RGB_RLE)
         && m_tga.bpp == 32)
        || ((m_tga.type == TYPE_GRAY || m_tga.type == TYPE_GRAY_RLE)
            && m_tga.bpp > 8)) {
        m_alpha_type = (m_tga.attr & 0x08) > 0 ? TGA_ALPHA_USEFUL
                                               : TGA_ALPHA_NONE;
    }

    m_spec = ImageSpec(
        (int)m_tga.width, (int)m_tga.height,
        // colour channels
        ((m_tga.type == TYPE_GRAY || m_tga.type == TYPE_GRAY_RLE) ? 1 : 3)
            // have we got alpha?
            + (m_tga.bpp == 32 || m_alpha_type >= TGA_ALPHA_UNDEFINED_RETAIN),
        TypeDesc::UINT8);
    m_spec.attribute("oiio:BitsPerSample", m_tga.bpp / m_spec.nchannels);
    m_spec.default_channel_names();
#if 0  // no one seems to adhere to this part of the spec...
    if (m_tga.attr & FLAG_X_FLIP)
        m_spec.x = m_spec.width - m_tga.x_origin - 1;
    else
        m_spec.x = m_tga.x_origin;
    if (m_tga.attr & FLAG_Y_FLIP)
        m_spec.y = m_tga.y_origin;
    else
        m_spec.y = m_spec.width - m_tga.y_origin - 1;
#endif
    if (m_tga.type >= TYPE_PALETTED_RLE)
        m_spec.attribute("compression", "rle");

    /*std::cerr << "[tga] " << m_tga.width << "x" << m_tga.height << "@"
              << (int)m_tga.bpp << " (" << m_spec.nchannels
              << ") type " << (int)m_tga.type << "\n";*/

    // load image ID
    if (m_tga.idlen) {
        // TGA comments can be at most 255 bytes long, but we add 1 extra byte
        // in case the comment lacks null termination
        char id[256];
        memset(id, 0, sizeof(id));
        if (!ioread(id, m_tga.idlen, 1))
            return false;
        m_spec.attribute("targa:ImageID", id);
    }

    int64_t ofs   = iotell();
    m_ofs_palette = ofs;

    // now try and see if it's a TGA 2.0 image
    // TGA 2.0 files are identified by a nifty "TRUEVISION-XFILE.\0" signature
    bool check_for_tga2 = (ioproxy()->size() > 26 + 18);
    if (check_for_tga2 && !ioseek(-26, SEEK_END)) {
        errorfmt("Could not seek to find the TGA 2.0 signature.");
        return false;
    }
    if (check_for_tga2 && read(m_foot.ofs_ext) && read(m_foot.ofs_dev)
        && ioread(&m_foot.signature, sizeof(m_foot.signature), 1)
        && !strncmp(m_foot.signature, "TRUEVISION-XFILE.", 17)) {
        //std::cerr << "[tga] this is a TGA 2.0 file\n";
        m_tga_version = 2;
        if (!read_tga2_header())
            return false;
    } else {
        m_tga_version = 1;
    }
    m_spec.attribute("targa:version", int(m_tga_version));

    if (!check_open(m_spec))
        return false;

    if (m_spec.alpha_channel != -1 && m_alpha_type == TGA_ALPHA_USEFUL
        && m_keep_unassociated_alpha)
        m_spec.attribute("oiio:UnassociatedAlpha", 1);

    // Reposition back to where the palette starts
    if (!ioseek(ofs)) {
        return false;
    }

    newspec = spec();
    DBG("TGA completed opening {}\n", name);
    return true;
}



bool
TGAInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}



bool
TGAInput::read_tga2_header()
{
    // read the extension area
    if (!ioseek(m_foot.ofs_ext)) {
        return false;
    }
    // check if this is a TGA 2.0 extension area
    // according to the 2.0 spec, the size for valid 2.0 files is exactly
    // 495 bytes, and the reader should only read as much as it understands
    // for < 495, we ignore this section of the file altogether
    // for > 495, we only read what we know
    uint16_t s;
    if (!read(s))
        return false;
    //std::cerr << "[tga] extension area size: " << s << "\n";
    if (s >= 495) {
        union {
            unsigned char c[324];  // so as to accommodate the comments
            uint16_t s[6];
            uint32_t l;
        } buf;

        // load image author
        if (!read_bytes_for_string_attribute("Artist", 41))
            return false;

        // load image comments
        if (!ioread(buf.c, 324, 1))
            return false;

        // concatenate the lines into a single string
        std::string tmpstr = Strutil::safe_string((const char*)buf.c, 81);
        if (buf.c[81]) {
            tmpstr += "\n";
            tmpstr += Strutil::safe_string((const char*)&buf.c[81], 81);
        }
        if (buf.c[162]) {
            tmpstr += "\n";
            tmpstr += Strutil::safe_string((const char*)&buf.c[162], 81);
        }
        if (buf.c[243]) {
            tmpstr += "\n";
            tmpstr += Strutil::safe_string((const char*)&buf.c[243], 81);
        }
        if (tmpstr.length() > 0)
            m_spec.attribute("ImageDescription", tmpstr);

        // timestamp
        if (!ioread(buf.s, 2, 6))
            return false;
        if (buf.s[0] || buf.s[1] || buf.s[2] || buf.s[3] || buf.s[4]
            || buf.s[5]) {
            if (bigendian())
                swap_endian(&buf.s[0], 6);
            m_spec.attribute(
                "DateTime",
                Strutil::fmt::format("{:04}:{:02}:{:02} {:02}:{:02}:{:02}",
                                     buf.s[2], buf.s[0], buf.s[1], buf.s[3],
                                     buf.s[4], buf.s[5]));
        }

        // job name/ID
        if (!read_bytes_for_string_attribute("DocumentName", 41))
            return false;

        // job time
        if (!ioread(buf.s, 2, 3))
            return false;
        if (buf.s[0] || buf.s[1] || buf.s[2]) {
            if (bigendian())
                swap_endian(&buf.s[0], 3);
            m_spec.attribute("targa:JobTime",
                             Strutil::fmt::format("{}:{:02}:{:02}", buf.s[0],
                                                  buf.s[1], buf.s[2]));
        }

        // software
        if (!ioread(buf.c, 41, 1))
            return false;
        uint16_t n;
        char l;
        if (!read(n) || !read(l))
            return false;
        if (buf.c[0]) {
            // tack on the version number and letter
            std::string soft = Strutil::safe_string((const char*)buf.c, 41);
            soft += Strutil::fmt::format(" {}.{}", n / 100, n % 100);
            if (l != ' ')
                soft += l;
            m_spec.attribute("Software", soft);
        }

        // background (key) colour
        if (!ioread(buf.c, 4, 1))
            return false;
        // FIXME: what do we do with it?

        // aspect ratio
        if (!ioread(buf.s, 2, 2))
            return false;
        // if the denominator is zero, it's unused
        if (buf.s[1]) {
            if (bigendian())
                swap_endian(&buf.s[0], 2);
            m_spec.attribute("PixelAspectRatio",
                             (float)buf.s[0] / (float)buf.s[1]);
        }

        // gamma
        if (!ioread(buf.s, 2, 2))
            return false;
        // if the denominator is zero, it's unused
        if (buf.s[1]) {
            if (bigendian())
                swap_endian(&buf.s[0], 2);
            float gamma = (float)buf.s[0] / (float)buf.s[1];
            // Round gamma to the nearest hundredth to prevent stupid
            // precision choices and make it easier for apps to make
            // decisions based on known gamma values. For example, you want
            // 2.2, not 2.19998.
            gamma = roundf(100.0 * gamma) / 100.0f;
            set_colorspace_rec709_gamma(m_spec, gamma);
        }

        // offset to colour correction table
        if (!read(buf.l))
            return false;
        m_ofs_colcorr_tbl = buf.l;
        /*std::cerr << "[tga] colour correction table offset: "
                      << (int)m_ofs_colcorr_tbl << "\n";*/

        // offset to thumbnail
        if (!read(buf.l))
            return false;
        m_ofs_thumb = buf.l;

        // offset to scan-line table
        if (!read(buf.l))
            return false;
        // TODO: can we find any use for this? we can't advertise random
        // access anyway, because not all RLE-compressed files will have
        // this table

        // alpha type
        if (!ioread(buf.c, 1, 1))
            return false;
        if (buf.c[0] >= TGA_ALPHA_INVALID) {
            errorfmt("Invalid alpha type {}. Corrupted header?", (int)buf.c[0]);
            return false;
        }
        m_alpha_type = (tga_alpha_type)buf.c[0];
        if (m_alpha_type)
            m_spec.attribute("targa:alpha_type", m_alpha_type);

        // Check for presence of a thumbnail and set the metadata that
        // says its dimensions, but don't read and decode it unless
        // thumbnail() is called.
        if (m_ofs_thumb > 0) {
            if (!ioseek(m_ofs_thumb)) {
                return false;
            }
            // Read the thumbnail dimensions -- sometimes it's 0x0 to
            // indicate no thumbnail.
            unsigned char res[2];
            if (!ioread(&res, 2, 1))
                return false;
            if (res[0] > 0 && res[1] > 0) {
                m_spec.attribute("thumbnail_width", (int)res[0]);
                m_spec.attribute("thumbnail_height", (int)res[1]);
                m_spec.attribute("thumbnail_nchannels", m_spec.nchannels);
            }
        }
    }
    // FIXME: provide access to the developer area; according to Larry,
    // it's probably safe to ignore it altogether until someone complains
    // that it's missing :)

    return true;
}



bool
TGAInput::get_thumbnail(ImageBuf& thumb, int subimage)
{
    if (m_ofs_thumb <= 0)
        return false;  // no thumbnail info

    lock_guard lock(*this);
    bool result         = false;
    int64_t save_offset = iotell();

    if (!ioseek(m_ofs_thumb))
        return false;

    // Read the thumbnail dimensions -- sometimes it's 0x0 to indicate no
    // thumbnail.
    unsigned char res[2];
    if (!ioread(&res, 2, 1))
        return false;
    if (res[0] > 0 && res[1] > 0) {
        // Most of this code is a dupe of readimg(); according to the spec,
        // the thumbnail is in the same format as the main image but
        // uncompressed.
        ImageSpec thumbspec(res[0], res[1], m_spec.nchannels, TypeUInt8);
        thumbspec.set_colorspace("sRGB");
        thumb.reset(thumbspec);
        int bytespp    = (m_tga.bpp == 15) ? 2 : (m_tga.bpp / 8);
        int palbytespp = (m_tga.cmap_size == 15) ? 2 : (m_tga.cmap_size / 8);
        int alphabits  = m_tga.attr & 0x0F;
        if (alphabits == 0 && m_tga.bpp == 32)
            alphabits = 8;
        // read palette, if there is any
        std::unique_ptr<unsigned char[]> palette;
        size_t palette_alloc_size = 0;
        if (is_palette()) {
            if (!ioseek(m_ofs_palette)) {
                return false;
            }
            palette_alloc_size = palbytespp * m_tga.cmap_length;
            palette.reset(new unsigned char[palette_alloc_size]);
            if (!ioread(palette.get(), palbytespp, m_tga.cmap_length))
                return false;
            if (!ioseek(m_ofs_thumb + 2)) {
                return false;
            }
        }
        // load pixel data
        unsigned char pixel[4];
        unsigned char in[4];
        for (int64_t y = thumbspec.height - 1; y >= 0; y--) {
            char* img = (char*)thumb.pixeladdr(0, y);
            for (int64_t x = 0; x < thumbspec.width;
                 x++, img += m_spec.nchannels) {
                if (!ioread(in, bytespp, 1))
                    return false;
                if (!decode_pixel(in, pixel, palette.get(), bytespp, palbytespp,
                                  palette_alloc_size))
                    return false;
                memcpy(img, pixel, m_spec.nchannels);
            }
        }
        result = true;
    }

    if (!ioseek(save_offset)) {
        return false;
    }
    return result;
}



inline bool
TGAInput::decode_pixel(unsigned char* in, unsigned char* out,
                       unsigned char* palette, int bytespp, int palbytespp,
                       size_t palette_alloc_size)
{
    unsigned int k = 0;
    // I hate nested switches...
    switch (m_tga.type) {
    case TYPE_PALETTED:
    case TYPE_PALETTED_RLE:
        for (int i = 0; i < bytespp; ++i)
            k |= in[i] << (8 * i);  // Assemble it in little endian order
        k = (m_tga.cmap_first + k) * palbytespp;
        if (k + palbytespp > palette_alloc_size) {
            errorfmt("Corrupt palette index");
            return false;
        }
        switch (palbytespp) {
        case 2:
            // see the comment for 16bpp RGB below for an explanation of this
            out[0] = bit_range_convert<5, 8>((palette[k + 1] & 0x7C) >> 2);
            out[1] = bit_range_convert<5, 8>(((palette[k + 0] & 0xE0) >> 5)
                                             | ((palette[k + 1] & 0x03) << 3));
            out[2] = bit_range_convert<5, 8>(palette[k + 0] & 0x1F);
            break;
        case 3:
            out[0] = palette[k + 2];
            out[1] = palette[k + 1];
            out[2] = palette[k + 0];
            break;
        case 4:
            out[0] = palette[k + 2];
            out[1] = palette[k + 1];
            out[2] = palette[k + 0];
            out[3] = palette[k + 3];
            break;
        }
        break;
    case TYPE_RGB:
    case TYPE_RGB_RLE:
        switch (bytespp) {
        case 2:
            // This format is pretty funky. It's a 1A-5R-5G-5B layout,
            // with the first bit alpha (or unused if only 3 channels),
            // but thanks to the little-endianness, the order is pretty
            // bizarre. The bits are non-contiguous, so we have to
            // extract the relevant ones and synthesize the colour
            // values from the two bytes.
            // NOTE: This way of handling the pixel as two independent bytes
            // (as opposed to a single short int) makes it independent from
            // endianness.
            // Here's what the layout looks like:
            // MSb       unused   LSb
            //  v           v      v
            //  GGGBBBBB     RRRRRGG
            // [||||||||]  [||||||||]
            // While red and blue channels are quite self-explanatory, the
            // green one needs a few words. The 5 bits are composed of the
            // 2 from the second byte as the more significant and the 3 from
            // the first one as the less significant ones.

            // extract the bits to valid 5-bit integers and expand to full range
            out[0] = bit_range_convert<5, 8>((in[1] & 0x7C) >> 2);
            out[1] = bit_range_convert<5, 8>(((in[0] & 0xE0) >> 5)
                                             | ((in[1] & 0x03) << 3));
            out[2] = bit_range_convert<5, 8>(in[0] & 0x1F);
            if (m_spec.nchannels > 3)
                out[3] = (in[0] & 0x80) ? 255 : 0;
            break;
        case 3:
            out[0] = in[2];
            out[1] = in[1];
            out[2] = in[0];
            break;
        case 4:
            out[0] = in[2];
            out[1] = in[1];
            out[2] = in[0];
            out[3] = in[3];
            break;
        }
        break;
    case TYPE_GRAY:
    case TYPE_GRAY_RLE:
        if (bigendian()) {
            for (int i = bytespp - 1; i >= 0; i--)
                out[i] = in[bytespp - i - 1];
        } else
            memcpy(out, in, bytespp);
        break;
    }
    return true;
}



template<class T>
static void
associateAlpha(T* data, int64_t size, int channels, int alpha_channel,
               float gamma)
{
    T max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int64_t x = 0; x < size; ++x, data += channels)
            for (int c = 0; c < channels; c++)
                if (c != alpha_channel) {
                    unsigned int f = data[c];
                    data[c]        = (f * data[alpha_channel]) / max;
                }
    } else {  //With gamma correction
        float inv_max = 1.0 / max;
        for (int64_t x = 0; x < size; ++x, data += channels) {
            float alpha_associate
                = OIIO::fast_pow_pos(data[alpha_channel] * inv_max, gamma);
            // We need to transform to linear space, associate the alpha, and
            // then transform back.  That is, if D = data[c], we want
            //
            // D' = max * ( (D/max)^(1/gamma) * (alpha/max) ) ^ gamma
            //
            // This happens to simplify to something which looks like
            // multiplying by a nonlinear alpha:
            //
            // D' = D * (alpha/max)^gamma
            for (int c = 0; c < channels; c++)
                if (c != alpha_channel)
                    data[c] = static_cast<T>(data[c] * alpha_associate);
        }
    }
}



bool
TGAInput::readimg()
{
    // how many bytes we actually read
    // for 15-bit read 2 bytes and ignore the 16th bit
    int bytespp    = (m_tga.bpp == 15) ? 2 : (m_tga.bpp / 8);
    int palbytespp = (m_tga.cmap_size == 15) ? 2 : (m_tga.cmap_size / 8);
    int alphabits  = m_tga.attr & 0x0F;
    if (alphabits == 0 && m_tga.bpp == 32)
        alphabits = 8;

    DBG("TGA readimg {}, bytespp = {} palbytespp = {} alphabits = {}\n",
        m_filename, bytespp, palbytespp, alphabits);

    try {
        DBG("TGA {} allocating for {}x{} {}-chan image = {}\n", m_filename,
            m_spec.width, m_spec.height, m_spec.nchannels,
            m_spec.image_bytes());
        m_buf.reset(new uint8_t[m_spec.image_bytes()]);
    } catch (const std::exception& e) {
        errorfmt("Cannot allocate enough memory for {}x{} {}-chan image {}",
                 m_spec.width, m_spec.height, m_spec.nchannels, m_filename);
        return false;
    }

    // read palette, if there is any
    std::unique_ptr<unsigned char[]> palette;
    size_t palette_alloc_size = 0;
    if (is_palette()) {
        palette_alloc_size = palbytespp * m_tga.cmap_length;
        palette.reset(new unsigned char[palette_alloc_size]);
        if (!ioread(palette.get(), palbytespp, m_tga.cmap_length))
            return false;
    }

    unsigned char pixel[4] = { 0, 0, 0, 0 };
    if (m_tga.type < TYPE_PALETTED_RLE) {
        // uncompressed image data
        DBG("TGA readimg, reading uncompressed image data\n");
        unsigned char in[4];
        for (int64_t y = m_spec.height - 1; y >= 0; y--) {
            for (int64_t x = 0; x < m_spec.width; x++) {
                if (!ioread(in, bytespp, 1))
                    return false;
                if (!decode_pixel(in, pixel, palette.get(), bytespp, palbytespp,
                                  palette_alloc_size))
                    return false;
                memcpy(m_buf.get() + y * m_spec.width * m_spec.nchannels
                           + x * m_spec.nchannels,
                       pixel, m_spec.nchannels);
            }
        }
    } else {
        // Run Length Encoded image
        unsigned char in[5];
        DBG("TGA readimg, reading RLE image data\n");
        int packet_size;
        for (int64_t y = m_spec.height - 1; y >= 0; y--) {
            for (int64_t x = 0; x < m_spec.width; x++) {
                if (!ioread(in, 1 + bytespp, 1)) {
                    DBG("Failed on scanline {}\n", y);
                    return false;
                }
                packet_size = 1 + (in[0] & 0x7f);
                if (!decode_pixel(&in[1], pixel, palette.get(), bytespp,
                                  palbytespp, palette_alloc_size))
                    return false;
                if (in[0] & 0x80) {  // run length packet
                    // DBG("[tga] run length packet size {} @ ({},{})\n",
                    //     packet_size, x, y);
                    for (int i = 0; i < packet_size; i++) {
                        memcpy(m_buf.get() + y * m_spec.width * m_spec.nchannels
                                   + x * m_spec.nchannels,
                               pixel, m_spec.nchannels);
                        if (i < packet_size - 1) {
                            x++;
                            if (x >= m_spec.width) {
                                // run spans across multiple scanlines
                                x = 0;
                                if (y > 0)
                                    y--;
                                else
                                    goto loop_break;
                            }
                        }
                    }
                } else {  // non-rle packet
                    // DBG("[tga] non-run length packet size {} @ ({},{}): [{:d} {:d} {:d} {:d}]\n",
                    //     packet_size, x, y, pixel[0], pixel[1], pixel[2],
                    //     pixel[3]);
                    for (int i = 0; i < packet_size; i++) {
                        memcpy(m_buf.get() + y * m_spec.width * m_spec.nchannels
                                   + x * m_spec.nchannels,
                               pixel, m_spec.nchannels);
                        if (i < packet_size - 1) {
                            x++;
                            if (x >= m_spec.width) {
                                // run spans across multiple scanlines
                                x = 0;
                                if (y > 0)
                                    y--;
                                else
                                    goto loop_break;
                            }
                            // skip the packet header byte
                            if (!ioread(&in[1], bytespp, 1)) {
                                DBG("Failed on scanline {}\n", y);
                                return false;
                            }
                            if (!decode_pixel(&in[1], pixel, palette.get(),
                                              bytespp, palbytespp,
                                              palette_alloc_size))
                                return false;
                            // DBG("\t\t@ ({},{}): [{:d} {:d} {:d} {:d}]\n", x, y,
                            //     pixel[0], pixel[1], pixel[2], pixel[3]);
                        }
                    }
                }
            }
        loop_break:;
        }
    }

    // flip the image, if necessary
    if (is_palette())
        bytespp = palbytespp;
    // Y-flipping is now done in read_native_scanline instead
    /*if (m_tga.attr & FLAG_Y_FLIP) {
        //std::cerr << "[tga] y flipping\n";

        std::vector<unsigned char> flip (m_spec.width * bytespp);
        unsigned char *src, *dst, *tmp = &flip[0];
        for (int y = 0; y < m_spec.height / 2; y++) {
            src = m_buf.get() + (m_spec.height - y - 1) * m_spec.width * bytespp;
            dst = m_buf.get() + y * m_spec.width * bytespp;

            memcpy(tmp, src, m_spec.width * bytespp);
            memcpy(src, dst, m_spec.width * bytespp);
            memcpy(dst, tmp, m_spec.width * bytespp);
        }
    }*/
    if (m_tga.attr & FLAG_X_FLIP) {
        //std::cerr << "[tga] x flipping\n";
        unsigned char flip[4];
        int nc             = m_spec.nchannels;
        int scanline_bytes = m_spec.width * m_spec.nchannels;
        for (int64_t y = 0; y < m_spec.height; y++) {
            unsigned char* line = m_buf.get() + y * scanline_bytes;
            for (int64_t x = 0; x < m_spec.width / 2; x++) {
                unsigned char* src = line + x * m_spec.nchannels;
                unsigned char* dst
                    = line + (m_spec.width - 1 - x) * m_spec.nchannels;
                memcpy(flip, src, nc);
                memcpy(src, dst, nc);
                memcpy(dst, flip, nc);
            }
        }
    }

    // Convert to associated unless we were requested not to do so.
    if (m_spec.alpha_channel != -1 && !m_keep_unassociated_alpha
        && m_alpha_type != TGA_ALPHA_PREMULTIPLIED) {
        // TGA 1.0 files don't have a way to indicate that the alpha is not
        // premultiplied. We presume unpremultiplied, but if alpha is zero
        // everywhere, ugh, it's probably meaningless.
        bool alpha0_everywhere = (m_tga_version == 1);
        int64_t size           = m_spec.image_pixels();
        for (int64_t i = 0; i < size; ++i) {
            if (m_buf[i * m_spec.nchannels + m_spec.alpha_channel]) {
                alpha0_everywhere = false;
                break;
            }
        }
        if (!alpha0_everywhere) {
            float gamma = m_spec.get_float_attribute("oiio:Gamma", 1.0f);
            associateAlpha((unsigned char*)m_buf.get(), size, m_spec.nchannels,
                           m_spec.alpha_channel, gamma);
        }
    }

    DBG("TGA completed readimg {}\n", m_filename);
    return true;
}



bool
TGAInput::close()
{
    init();  // Reset to initial state
    return true;
}



bool
TGAInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (!m_buf) {
        if (!readimg()) {
            return false;
        }
    }

    if (m_tga.attr & FLAG_Y_FLIP)
        y = m_spec.height - y - 1;
    size_t size = spec().scanline_bytes();
    memcpy(data, m_buf.get() + y * size, size);
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
