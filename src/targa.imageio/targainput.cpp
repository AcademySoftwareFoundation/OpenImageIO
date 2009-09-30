/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "targa_pvt.h"
using namespace TGA_pvt;

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

using namespace OpenImageIO;



class TGAInput : public ImageInput {
public:
    TGAInput () { init(); }
    virtual ~TGAInput () { close(); }
    virtual const char * format_name (void) const { return "targa"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    tga_header m_tga;                 ///< Targa header
    tga_footer m_foot;                ///< Targa 2.0 footer
    unsigned int m_ofs_colcorr_tbl;   ///< Offset to colour correction table
    unsigned int m_ofs_thumbnail;     ///< Offset to thumbnail
    tga_alpha_type m_alpha;           ///< Alpha type
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels

    /// Reset everything to initial state
    ///
    void init () {
        m_file = NULL;
        m_buf.clear ();
        m_ofs_colcorr_tbl = 0;
        m_ofs_thumbnail = 0;
        m_alpha = TGA_ALPHA_NONE;
    }

    /// Helper function: read the image.
    ///
    bool readimg ();

    /// Helper function: decode a pixel.
    inline void decode_pixel (unsigned char *in, unsigned char *out,
                              unsigned char *palette, int& bytespp,
                              int& palbytespp, int& alphabits);
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageInput *targa_input_imageio_create () { return new TGAInput; }

DLLEXPORT int targa_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;

DLLEXPORT const char * targa_input_extensions[] = {
    "tga", NULL
};

};



bool
TGAInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;

    m_file = fopen (name.c_str(), "rb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // due to struct packing, we may get a corrupt header if we just load the
    // struct from file; to adress that, read every member individually
    // save some typing
#define RH(memb)    fread (&m_tga.memb, sizeof (m_tga.memb), 1, m_file)
    RH(idlen);
    RH(cmap_type);
    RH(type);
    RH(cmap_first);
    RH(cmap_length);
    RH(cmap_size);
    RH(x_origin);
    RH(y_origin);
    RH(width);
    RH(height);
    RH(bpp);
    RH(attr);
#undef RH
    if (bigendian()) {
        // TGAs are little-endian
        swap_endian (&m_tga.idlen);
        swap_endian (&m_tga.cmap_type);
        swap_endian (&m_tga.type);
        swap_endian (&m_tga.cmap_first);
        swap_endian (&m_tga.cmap_length);
        swap_endian (&m_tga.cmap_size);
        swap_endian (&m_tga.x_origin);
        swap_endian (&m_tga.y_origin);
        swap_endian (&m_tga.width);
        swap_endian (&m_tga.height);
        swap_endian (&m_tga.bpp);
        swap_endian (&m_tga.attr);
    }

    if (m_tga.bpp != 8 && m_tga.bpp != 15 && m_tga.bpp != 16
        && m_tga.bpp != 24 && m_tga.bpp != 32) {
        error ("Illegal pixel size: %d bits per pixel", m_tga.bpp);
        return false;
    }

    if (m_tga.type == TYPE_NODATA) {
        error ("Image with no data");
        return false;
    }
    if (m_tga.type != TYPE_PALETTED && m_tga.type != TYPE_RGB
        && m_tga.type != TYPE_GRAY && m_tga.type != TYPE_PALETTED_RLE
        && m_tga.type != TYPE_RGB_RLE && m_tga.type != TYPE_GRAY_RLE) {
        error ("Illegal image type: %d", m_tga.type);
        return false;
    }

    if (m_tga.cmap_type
        && (m_tga.type == TYPE_GRAY || m_tga.type == TYPE_GRAY_RLE)) {
        // it should be an error for TYPE_RGB* as well, but apparently some
        // *very* old TGAs can be this way, so we'll hack around it
        error ("Palette defined for grayscale image");
        return false;
    }

    if (m_tga.cmap_type && (m_tga.cmap_size != 15 && m_tga.cmap_size != 16
        && m_tga.cmap_size != 24 && m_tga.cmap_size != 32)) {
        error ("Illegal palette entry size: %d bits", m_tga.cmap_size);
        return false;
    }

    m_alpha = m_tga.attr & 0x0F > 0 ? TGA_ALPHA_USEFUL : TGA_ALPHA_NONE;

    m_spec = ImageSpec ((int)m_tga.width, (int)m_tga.height,
                        // colour channels
                        ((m_tga.type == TYPE_GRAY
                        || m_tga.type == TYPE_GRAY_RLE)
                        ? 1 : 3)
                        // have we got alpha?
                        + (m_tga.bpp == 32
                        || m_alpha >= TGA_ALPHA_UNDEFINED_RETAIN),
                        TypeDesc::UINT8);
    m_spec.attribute ("BitsPerSample", m_tga.bpp);
    m_spec.default_channel_names ();
    m_spec.linearity = ImageSpec::UnknownLinearity;
#if 0   // no one seems to adhere to this part of the spec...
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
        m_spec.attribute ("compression", "rle");

    /*std::cerr << "[tga] " << m_tga.width << "x" << m_tga.height << "@"
              << (int)m_tga.bpp << " (" << m_spec.nchannels
              << ") type " << (int)m_tga.type << "\n";*/

    // load image ID
    if (m_tga.idlen) {
        // TGA comments can be at most 255 bytes long, but we add 1 extra byte
        // in case the comment lacks null termination
        char id[256];
        memset (id, 0, sizeof (id));
        fread (id, m_tga.idlen, 1, m_file);
        m_spec.attribute ("targa:ImageID", id);
    }

    int ofs = ftell (m_file);
    // now try and see if it's a TGA 2.0 image
    fseek (m_file, -26, SEEK_END);
    fread (&m_foot.ofs_ext, sizeof (m_foot.ofs_ext), 1, m_file);
    fread (&m_foot.ofs_dev, sizeof (m_foot.ofs_dev), 1, m_file);
    fread (&m_foot.signature, sizeof (m_foot.signature), 1, m_file);
    // TGA 2.0 files are identified by a nifty "TRUEVISION-XFILE.\0" signature
    if (!strncmp (m_foot.signature, "TRUEVISION-XFILE.", 17)) {
        //std::cerr << "[tga] this is a TGA 2.0 file\n";
        if (bigendian()) {
            swap_endian (&m_foot.ofs_ext);
            swap_endian (&m_foot.ofs_dev);
        }

        // read the extension area
        fseek (m_file, m_foot.ofs_ext, SEEK_SET);
        // check if this is a TGA 2.0 extension area
        // according to the 2.0 spec, the size for valid 2.0 files is exactly
        // 495 bytes, and the reader should only read as much as it understands
        // for < 495, we ignore this section of the file altogether
        // for > 495, we only read what we know
        uint16_t s;
        fread (&s, 2, 1, m_file);
        if (bigendian())
            swap_endian (&s);
        //std::cerr << "[tga] extension area size: " << s << "\n";
        if (s >= 495) {
            union {
                char        c[324]; // so as to accomodate the comments
                uint16_t    s[6];
                uint32_t    l;
            } buf;
            
            // load image author
            fread (buf.c, 41, 1, m_file);
            if (buf.c[0])
                m_spec.attribute ("Artist", buf.c);
            
            // load image comments
            fread (buf.c, 324, 1, m_file);
            // concatenate the lines into a single string
            buf.c[80] = buf.c[161] = buf.c[242] = '\n';
            if (buf.c[0])
                m_spec.attribute ("ImageDescription", buf.c);

            // timestamp
            fread (buf.s, 2, 6, m_file);
            if (buf.s[0] || buf.s[1] || buf.s[2]
                || buf.s[3] || buf.s[4] || buf.s[5]) {
                if (bigendian()) {
                    swap_endian (&buf.s[0]);
                    swap_endian (&buf.s[1]);
                    swap_endian (&buf.s[2]);
                    swap_endian (&buf.s[3]);
                    swap_endian (&buf.s[4]);
                    swap_endian (&buf.s[5]);
                }
                sprintf (&buf.c[12], "%04u:%02u:%02u %02u:%02u:%02u",
                         buf.s[2], buf.s[0], buf.s[1],
                         buf.s[3], buf.s[4], buf.s[5]);
                m_spec.attribute ("DateTime", &buf.c[12]);
            }

            // job name/ID
            fread (buf.c, 41, 1, m_file);
            if (buf.c[0])
                m_spec.attribute ("DocumentName", buf.c);

            // job time
            fread (buf.s, 2, 3, m_file);
            if (buf.s[0] || buf.s[1] || buf.s[2]) {
                if (bigendian()) {
                    swap_endian (&buf.s[0]);
                    swap_endian (&buf.s[1]);
                    swap_endian (&buf.s[2]);
                }
                sprintf (&buf.c[6], "%02u:%02u:%02u",
                          buf.s[0], buf.s[1], buf.s[2]);
                m_spec.attribute ("targa:JobTime", &buf.c[6]);
            }

            // software
            fread (buf.c, 41, 1, m_file);
            if (buf.c[0]) {
                // tack on the version number and letter
                uint16_t n;
                char l;
                fread (&n, 2, 1, m_file);
                fread (&l, 1, 1, m_file);
                sprintf (&buf.c[strlen (buf.c)], " %u.%u%c", n / 100, n % 100,
                         l != ' ' ? l : 0);
                m_spec.attribute ("Software", buf.c);
            }

            // background (key) colour
            fread (buf.c, 4, 1, m_file);
            // FIXME: what do we do with it?

            // aspect ratio
            fread (buf.s, 2, 2, m_file);
            // if the denominator is zero, it's unused
            if (buf.s[1]) {
                if (bigendian()) {
                    swap_endian (&buf.s[0]);
                    swap_endian (&buf.s[1]);
                }
                m_spec.attribute ("PixelAspectRatio", (float)buf.s[0] / (float)buf.s[1]);
            }

            // gamma
            fread (buf.s, 2, 2, m_file);
            // if the denominator is zero, it's unused
            if (buf.s[1]) {
                if (bigendian()) {
                    swap_endian (&buf.s[0]);
                    swap_endian (&buf.s[1]);
                }
                m_spec.linearity = ImageSpec::GammaCorrected;
                m_spec.gamma = (float)buf.s[0] / (float)buf.s[1];
            }

            // offset to colour correction table
            fread (&buf.l, 4, 1, m_file);
            if (bigendian())
                swap_endian (&buf.l);
            m_ofs_colcorr_tbl = buf.l;

            // offset to thumbnail
            fread (&buf.l, 4, 1, m_file);
            if (bigendian())
                swap_endian (&buf.l);
            m_ofs_thumbnail = buf.l;

            // offset to scan-line table
            fread (&buf.l, 4, 1, m_file);
            // TODO: can we find any use for this? we can't advertise random
            // access anyway, because not all RLE-compressed files will have
            // this table

            // alpha type
            fread (buf.c, 1, 1, m_file);
            m_alpha = (tga_alpha_type)buf.c[0];
        }
    }

    fseek (m_file, ofs, SEEK_SET);

    newspec = spec ();
    return true;
}



inline void
TGAInput::decode_pixel (unsigned char *in, unsigned char *out,
                        unsigned char *palette, int& bytespp,
                        int& palbytespp, int& alphabits)
{
    unsigned int k = 0;
    // I hate nested switches...
    switch (m_tga.type) {
    case TYPE_PALETTED:
    case TYPE_PALETTED_RLE:
        switch (bytespp) {
        case 1:
            k = in[0];
            break;
        case 2:
            k = *((unsigned int *)in) & 0x0000FFFF;
            if (bigendian())
                swap_endian (&k);
            break;
        case 3:
            k = *((unsigned int *)in) & 0x00FFFFFF;
            if (bigendian())
                swap_endian (&k);
            break;
        case 4:
            k = *((unsigned int *)in);
            if (bigendian())
                swap_endian (&k);
            break;
        }
        k = (m_tga.cmap_first + k) * palbytespp;
        switch (palbytespp) {
        case 2:
            // see the comment for 16bpp RGB below for an explanation of this
            out[2] = (palette[k + 1] & 0x7C) << 1;
            out[1] = ((palette[k + 0] & 0xE0) >> 2)
                     | ((palette[k + 1] & 0x03) << 6);
            out[0] = (palette[k + 0] & 0x1F) << 3;
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
            // This format is pretty funky. It's a 1-5R-5G-5B layout, with the
            // first bit unused, but thanks to the little-endianness, the order
            // is pretty bizarre. The bits are non-contiguous, so we have to
            // extract the relevant ones and synthesize the colour values from
            // the two bytes.
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
            // the first one as the less significant ones. So much for the
            // funny bit masks.
            // As for the bit shifts - we start out with 5 bits and we need to
            // expand them to 8, so these just move them to the most
            // significant positions. We're losing a tiny bit of precision
            // here, though: 5 bits of 1s is 31 in decimal, shifted 3 spaces to
            // the left (= multiplied by 8) it's 31 * 8 = 248.
            // FIXME: we could do proper range expansion with floating point
            // calculations instead of bit hacking, but I'm not really
            // convinced it's worth it
            out[0] = (in[1] & 0x7C) << 1;
            out[1] = ((in[0] & 0xE0) >> 2) | ((in[1] & 0x03) << 6);
            out[2] = (in[0] & 0x1F) << 3;
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
        if (bigendian ()) {
            for (int i = bytespp - 1; i >= 0; i--)
                out[i] = in[bytespp - i - 1];
        } else
            memcpy (out, in, bytespp);
        break;
    }
}



bool
TGAInput::readimg ()
{
    // how many bytes we actually read
    // for 15-bit read 2 bytes and ignore the 16th bit
    int bytespp = (m_tga.bpp == 15) ? 2 : (m_tga.bpp / 8);
    int palbytespp = (m_tga.cmap_size == 15) ? 2 : (m_tga.cmap_size / 8);
    int alphabits = m_tga.attr & 0x0F;
    if (alphabits == 0 && m_tga.bpp == 32)
        alphabits = 8;

    /*std::cerr << "[tga] bytespp = " << bytespp
              << " palbytespp = " << palbytespp
              << " alphabits = " << alphabits
              << "\n";*/

    m_buf.resize (m_spec.image_bytes());

    // read palette, if there is any
    unsigned char *palette = NULL;
    if (m_tga.cmap_type) {
        palette = new unsigned char[palbytespp * m_tga.cmap_length];
        fread (palette, palbytespp, m_tga.cmap_length, m_file);
    }

    unsigned char pixel[4];
    if (m_tga.type < TYPE_PALETTED_RLE) {
        // uncompressed image data
        unsigned char in[4];
        for (int y = m_spec.height - 1; y >= 0; y--) {
            for (int x = 0; x < m_spec.width; x++) {
                fread (in, bytespp, 1, m_file);
                decode_pixel (in, pixel, palette,
                              bytespp, palbytespp, alphabits);
                memcpy (&m_buf[y * m_spec.width * m_spec.nchannels
                        + x * m_spec.nchannels],
                        pixel, m_spec.nchannels);
            }
        }
    } else {
        // Run Length Encoded image
        unsigned char in[5];
        int packet_size;
        for (int y = m_spec.height - 1; y >= 0; y--) {
            for (int x = 0; x < m_spec.width; x++) {
                fread (in, 1 + bytespp, 1, m_file);
                packet_size = 1 + (in[0] & 0x7f);
                decode_pixel (&in[1], pixel, palette,
                              bytespp, palbytespp, alphabits);
                if (in[0] & 0x80) {  // run length packet
                    /*std::cerr << "[tga] run length packet "
                              << packet_size << "\n";*/
                    for (int i = 0; i < packet_size; i++) {
                        memcpy (&m_buf[y * m_spec.width * m_spec.nchannels
                                + x * m_spec.nchannels],
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
                } else { // non-rle packet
                    /*std::cerr << "[tga] non-run length packet "
                              << packet_size << "\n";*/
                    for (int i = 0; i < packet_size; i++) {
                        memcpy (&m_buf[y * m_spec.width * m_spec.nchannels
                                + x * m_spec.nchannels],
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
                            fread (&in[1], bytespp, 1, m_file);
                            decode_pixel(&in[1], pixel, palette,
                                         bytespp, palbytespp, alphabits);
                        }
                    }
                }
            }
            loop_break:;
        }
    }

    delete [] palette;

    // flip the image, if necessary
    if (m_tga.cmap_type)
        bytespp = palbytespp;
    // Y-flipping is now done in read_native_scanline instead
    /*if (m_tga.attr & FLAG_Y_FLIP) {
        //std::cerr << "[tga] y flipping\n";

        std::vector<unsigned char> flip (m_spec.width * bytespp);
        unsigned char *src, *dst, *tmp = &flip[0];
        for (int y = 0; y < m_spec.height / 2; y++) {
            src = &m_buf[(m_spec.height - y - 1) * m_spec.width * bytespp];
            dst = &m_buf[y * m_spec.width * bytespp];

            memcpy(tmp, src, m_spec.width * bytespp);
            memcpy(src, dst, m_spec.width * bytespp);
            memcpy(dst, tmp, m_spec.width * bytespp);
        }
    }*/
    if (m_tga.attr & FLAG_X_FLIP) {
        //std::cerr << "[tga] x flipping\n";

        std::vector<unsigned char> flip (bytespp * m_spec.width / 2);
        unsigned char *src, *dst, *tmp = &flip[0];
        for (int y = 0; y < m_spec.height; y++) {
            src = &m_buf[y * m_spec.width * bytespp];
            dst = &m_buf[(y * m_spec.width + m_spec.width / 2) * bytespp];

            memcpy(tmp, src, bytespp * m_spec.width / 2);
            memcpy(src, dst, bytespp * m_spec.width / 2);
            memcpy(dst, tmp, bytespp * m_spec.width / 2);
        }
    }

    return true;
}



bool
TGAInput::close ()
{
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init();  // Reset to initial state
    return true;
}



bool
TGAInput::read_native_scanline (int y, int z, void *data)
{
    if (m_buf.empty ())
        readimg ();

    if (m_tga.attr & FLAG_Y_FLIP)
        y = m_spec.height - y - 1;
    size_t size = spec().scanline_bytes();
    memcpy (data, &m_buf[0] + y * size, size);
    return true;
}
