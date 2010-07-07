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

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace TGA_pvt;
using namespace OpenImageIO;


class TGAOutput : public ImageOutput {
public:
    TGAOutput ();
    virtual ~TGAOutput ();
    virtual const char * format_name (void) const { return "targa"; }
    virtual bool supports (const std::string &feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    bool m_want_rle;                  ///< Whether the client asked for RLE
    std::vector<unsigned char> m_scratch;
    int m_idlen;                      ///< Length of the TGA ID block

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
    }

    /// Helper function to flush a non-run-length packet
    ///
    inline void flush_rawp (unsigned char *& src, int size, int start,
                               int ofs, int mult);

    /// Helper function to flush a run-length packet
    ///
    inline void flush_rlp (unsigned char *buf, int size);
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *targa_output_imageio_create () { return new TGAOutput; }

// DLLEXPORT int tga_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;   // it's in tgainput.cpp

DLLEXPORT const char * targa_output_extensions[] = {
    "tga", NULL
};

OIIO_PLUGIN_EXPORTS_END


TGAOutput::TGAOutput ()
{
    init ();
}



TGAOutput::~TGAOutput ()
{
    // Close, if not already done.
    close ();
}



bool
TGAOutput::open (const std::string &name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    m_file = fopen (name.c_str(), "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }

    if (m_spec.depth < 1)
        m_spec.depth = 1;
    else if (m_spec.depth > 1) {
        error ("TGA does not support volume images (depth > 1)");
        return false;
    }

    if (m_spec.nchannels < 1 || m_spec.nchannels > 4) {
        error ("TGA only supports 1-4 channels, not %d", m_spec.nchannels);
        return false;
    }

    // Force 8 bit integers
    if (m_spec.format != TypeDesc::UINT16)
        m_spec.set_format (TypeDesc::UINT8);

    // check if the client wants the image to be run length encoded
    // currently only RGB RLE is supported
    m_want_rle = (m_spec.get_string_attribute ("compression", "none")
                 != std::string("none")) && m_spec.nchannels >= 3;

    // prepare and write Targa header
    tga_header tga;
    memset (&tga, 0, sizeof (tga));
    tga.type = m_spec.nchannels <= 2 ? TYPE_GRAY
               : (m_want_rle ? TYPE_RGB_RLE : TYPE_RGB);
    tga.bpp = m_spec.nchannels * 8;
    tga.width = m_spec.width;
    tga.height = m_spec.height;
#if 0   // no one seems to adhere to this part of the spec...
    tga.x_origin = m_spec.x;
    tga.y_origin = m_spec.y;
#endif

    // handle image ID; save it to disk later on
    std::string id = m_spec.get_string_attribute ("targa:ImageID", "");
    // the format only allows for 255 bytes
    tga.idlen = std::min(id.length(), (size_t)255);
    m_idlen = tga.idlen;

    if (m_spec.nchannels % 2 == 0)  // if we have alpha
        tga.attr = 8;   // 8 bits of alpha
    // force Y flip when using RLE
    // for raw (non-RLE) images we can use random access, so we can dump the
    // image in the default top-bottom scanline order for maximum
    // compatibility (not all software supports the Y flip flag); however,
    // once RLE kicks in, we lose the ability to predict the byte offsets of
    // scanlines, so we just dump the data in the order it comes in and use
    // this flag instead
    if (m_want_rle)
        tga.attr |= FLAG_Y_FLIP;
    if (bigendian()) {
        // TGAs are little-endian
        swap_endian (&tga.cmap_type);
        swap_endian (&tga.type);
        swap_endian (&tga.cmap_first);
        swap_endian (&tga.cmap_length);
        swap_endian (&tga.cmap_size);
        swap_endian (&tga.x_origin);
        swap_endian (&tga.y_origin);
        swap_endian (&tga.width);
        swap_endian (&tga.height);
        swap_endian (&tga.bpp);
        swap_endian (&tga.attr);
    }
    // due to struct packing, we may get a corrupt header if we just dump the
    // struct to the file; to adress that, write every member individually
    // save some typing
#define WH(memb)    fwrite (&tga.memb, sizeof (tga.memb), 1, m_file)
    WH(idlen);
    WH(cmap_type);
    WH(type);
    WH(cmap_first);
    WH(cmap_length);
    WH(cmap_size);
    WH(x_origin);
    WH(y_origin);
    WH(width);
    WH(height);
    WH(bpp);
    WH(attr);
#undef WH

    // dump comment to file, don't bother about null termination
    if (tga.idlen)
        fwrite (id.c_str(), tga.idlen, 1, m_file);

    return true;
}



bool
TGAOutput::close ()
{
    if (m_file) {
        // write out the TGA 2.0 data fields

        // FIXME: write out the developer area; according to Larry,
        // it's probably safe to ignore it altogether until someone complains
        // that it's missing :)

        fseek (m_file, 0, SEEK_END);

        // write out the thumbnail, if there is one
        int ofs_thumb = 0;
        {
            unsigned char tw = m_spec.get_int_attribute ("thumbnail_width", 0);
            if (tw) {
                unsigned char th = m_spec.get_int_attribute ("thumbnail_width",
                                                             0);
                if (th) {
                    int tc = m_spec.get_int_attribute ("thumbnail_nchannels",
                                                       0);
                    if (tc == m_spec.nchannels) {
                        ImageIOParameter *p =
                            m_spec.find_attribute ("thumbnail_image");
                        if (p) {
                            ofs_thumb = ftell (m_file);
                            if (bigendian())
                                swap_endian (&ofs_thumb);
                            // dump thumbnail size
                            fwrite (&tw, 1, 1, m_file);
                            fwrite (&th, 1, 1, m_file);
                            // dump thumbnail data
                            fwrite (p->data(), p->datasize(), 1, m_file);
                        }
                    }
                }
            }
        }
        
        // prepare the footer
        tga_footer foot = {(uint32_t)ftell (m_file), 0, "TRUEVISION-XFILE."};
        if (bigendian()) {
            swap_endian (&foot.ofs_ext);
            swap_endian (&foot.ofs_dev);
        }

        // write out the extension area
        // ext area size
        short tmpint = 495;
        if (bigendian())
            swap_endian (&tmpint);
        fwrite (&tmpint, sizeof (tmpint), 1, m_file);

        tmpint = 0;

        // author
        std::string tmpstr = m_spec.get_string_attribute ("Artist", "");
        fwrite (tmpstr.c_str(), std::min (tmpstr.length (), size_t(40)),
                1, m_file);
        // fill the rest with zeros
        for (int i = 41 - std::min (tmpstr.length (), size_t(40)); i > 0; i--)
            fwrite (&tmpint, 1, 1, m_file);

        // image comment
        tmpstr = m_spec.get_string_attribute ("ImageDescription", "");
        {
            char *p = (char *)tmpstr.c_str ();
            int w = 0;  // number of bytes written
            for (int pos = 0; w < 324 && pos < (int)tmpstr.length ();
                 w++, pos++) {
                // on line breaks, fill the remainder of the line with zeros
                if (p[pos] == '\n') {
                    while ((w + 1) % 81 != 0) {
                        fwrite (&tmpint, 1, 1, m_file);
                        w++;
                    }
                    continue;
                }
                fwrite (&p[pos], 1, 1, m_file);
                // null-terminate each line
                if ((w + 1) % 81 == 0) {
                    fwrite (&tmpint, 1, 1, m_file);
                    w++;
                }
            }
            // fill the rest with zeros
            for (; w < 324; w++)
                fwrite (&tmpint, 1, 1, m_file);
        }

        // timestamp
        tmpstr = m_spec.get_string_attribute ("DateTime", "");
        {
            unsigned short y, m, d, h, i, s;
            if (tmpstr.length () > 0)
                sscanf (tmpstr.c_str (), "%04hu:%02hu:%02hu %02hu:%02hu:%02hu",
                        &y, &m, &d, &h, &i, &s);
            else
                y = m = d = h = i = s = 0;
            if (bigendian()) {
                swap_endian (&y);
                swap_endian (&m);
                swap_endian (&d);
                swap_endian (&h);
                swap_endian (&i);
                swap_endian (&s);
            }
            fwrite (&m, sizeof (m), 1, m_file);
            fwrite (&d, sizeof (d), 1, m_file);
            fwrite (&y, sizeof (y), 1, m_file);
            fwrite (&h, sizeof (h), 1, m_file);
            fwrite (&i, sizeof (i), 1, m_file);
            fwrite (&s, sizeof (s), 1, m_file);
        }

        // job ID
        tmpstr = m_spec.get_string_attribute ("DocumentName", "");
        fwrite (tmpstr.c_str(), std::min (tmpstr.length (), size_t(40)),
                1, m_file);
        // fill the rest with zeros
        for (int i = 41 - std::min (tmpstr.length (), size_t(40)); i > 0; i--)
            fwrite (&tmpint, 1, 1, m_file);

        // job time
        tmpstr = m_spec.get_string_attribute ("targa:JobTime", "");
        {
            unsigned short h, m, s;
            if (tmpstr.length () > 0)
                sscanf (tmpstr.c_str (), "%hu:%02hu:%02hu", &h, &m, &s);
            else
                h = m = s = 0;
            if (bigendian()) {
                swap_endian (&h);
                swap_endian (&m);
                swap_endian (&s);
            }
            fwrite (&h, sizeof (h), 1, m_file);
            fwrite (&m, sizeof (m), 1, m_file);
            fwrite (&s, sizeof (s), 1, m_file);
        }

        // software ID - we advertise ourselves
        tmpstr = OPENIMAGEIO_INTRO_STRING;
        fwrite (tmpstr.c_str(), std::min (tmpstr.length (), size_t(40)),
                1, m_file);
        // fill the rest with zeros
        for (int i = 41 - std::min (tmpstr.length (), size_t(40)); i > 0; i--)
            fwrite (&tmpint, 1, 1, m_file);

        // software version
        {
            short v = OPENIMAGEIO_VERSION_MAJOR * 100
                    + OPENIMAGEIO_VERSION_MINOR * 10
                    + OPENIMAGEIO_VERSION_PATCH;
            if (bigendian())
                swap_endian (&v);
            fwrite (&v, sizeof (v), 1, m_file);
            fwrite (&tmpint, 1, 1, m_file);
        }

        // key colour
        // FIXME: what do we save here?
        fwrite (&tmpint, 2, 1, m_file);
        fwrite (&tmpint, 2, 1, m_file);

        // pixel aspect ratio
        {
            float ratio = m_spec.get_float_attribute ("PixelAspectRatio", 1.f);
            // FIXME: use an epsilon here instead of an equality check?
            if (ratio != 0.f && ratio != 1.f) {
                // FIXME: invent a smarter way to convert to a vulgar fraction?
                // numerator
                tmpint = (unsigned short)(ratio * 10000.f);
                fwrite (&tmpint, 2, 1, m_file);
                // denominator
                tmpint = 10000;
                fwrite (&tmpint, 2, 1, m_file);
                // reset tmpint value
                tmpint = 0;
            } else {
                // just dump two zeros in there
                fwrite (&tmpint, 2, 1, m_file);
                fwrite (&tmpint, 2, 1, m_file);
            }
        }

        // gamma
        {
            if (m_spec.linearity == ImageSpec::GammaCorrected) {
                // FIXME: invent a smarter way to convert to a vulgar fraction?
                // NOTE: the spec states that only 1 decimal place of precision
                // is needed, thus the expansion by 10
                // numerator
                tmpint = (unsigned short)(m_spec.gamma * 10.f);
                fwrite (&tmpint, 2, 1, m_file);
                // denominator
                tmpint = 10;
                fwrite (&tmpint, 2, 1, m_file);
                // reset tmpint value
                tmpint = 0;
            } else {
                // just dump two zeros in there
                fwrite (&tmpint, 2, 1, m_file);
                fwrite (&tmpint, 2, 1, m_file);
            }
        }

        // offset to colour correction table
        // FIXME: support this once it becomes clear how it's actually supposed
        // to be used... the spec is very unclear about this
        // for the time being just dump four NULL bytes
        fwrite (&tmpint, 2, 1, m_file);
        fwrite (&tmpint, 2, 1, m_file);

        // offset to thumbnail (endiannes has already been accounted for)
        fwrite (&ofs_thumb, 4, 1, m_file);

        // offset to scanline table
        // not used very widely, don't bother unless someone complains
        fwrite (&tmpint, 2, 1, m_file);
        fwrite (&tmpint, 2, 1, m_file);

        // alpha type
        {
            unsigned char at = (m_spec.nchannels % 2 == 0)
                             ? TGA_ALPHA_USEFUL : TGA_ALPHA_NONE;
            fwrite (&at, 1, 1, m_file);
        }

        // write out the TGA footer
        fwrite (&foot.ofs_ext, 1, sizeof (foot.ofs_ext), m_file);
        fwrite (&foot.ofs_dev, 1, sizeof (foot.ofs_dev), m_file);
        fwrite (&foot.signature, 1, sizeof (foot.signature), m_file);

        // close the stream
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



inline void
TGAOutput::flush_rlp (unsigned char *buf, int size)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    unsigned char h = (size - 1) | 0x80;
    fwrite (&h, 1, 1, m_file);
    // write packet pixel
    fwrite (buf, m_spec.nchannels, 1, m_file);
}



inline void
TGAOutput::flush_rawp (unsigned char *& src, int size, int start,
                          int ofs, int mult)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    unsigned char h = (size - 1) & ~0x80;
    fwrite (&h, 1, 1, m_file);
    // rewind the scanline and flush packet pixels
    unsigned char buf[4];
    for (int i = 0; i < size; i++) {
        switch (m_spec.nchannels) {
#if 0 // FIXME
        case 1:
            buf[0] = src[(start + i) * mult + ofs];
            fwrite (buf, 1, 1, m_file);
            break;
        case 2:
            buf[0] = src[mult * ((start + i) * 2 + 0) + ofs];
            buf[1] = src[mult * ((start + i) * 2 + 1) + ofs];
            fwrite (buf, 2, 1, m_file);
            break;
#endif
        case 3:
            buf[0] = src[mult * ((start + i) * 3 + 2) + ofs];
            buf[1] = src[mult * ((start + i) * 3 + 1) + ofs];
            buf[2] = src[mult * ((start + i) * 3 + 0) + ofs];
            fwrite (buf, 3, 1, m_file);
            break;
        case 4:
            buf[0] = src[mult * ((start + i) * 4 + 2) + ofs];
            buf[1] = src[mult * ((start + i) * 4 + 1) + ofs];
            buf[2] = src[mult * ((start + i) * 4 + 0) + ofs];
            buf[3] = src[mult * ((start + i) * 4 + 3) + ofs];
            fwrite (buf, 4, 1, m_file);
            break;
        }
        //std::cerr << "[tga] put pixel\n";
    }
}



bool
TGAOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    //std::cerr << "[tga] writing scanline #" << y << "\n";

    unsigned char *bdata = (unsigned char *)data;
    unsigned char buf[4];
    // these are used to read the most significant 8 bits only (the
    // precision loss...), also accounting for byte order
    int mult = format == TypeDesc::UINT16 ? 2 : 1;
    int ofs = (mult > 1 && bigendian()) ? 1 : 0;

    if (m_want_rle) {
        // Run Length Encoding
        // it's only profitable if n * b > 1 + b, where:
        // n is the number of pixels in a run
        // b is the pixel size in bytes
        // FIXME: optimize runs spanning across multiple scanlines?
        unsigned char buf2[4];
        bool rlp = false;
        int rlcount = 0, rawcount = 0;

        for (int x = 0; x < m_spec.width; x++) {
            // save off previous pixel
            memcpy (buf2, buf, sizeof (buf2));
            // read the new one
            switch (m_spec.nchannels) {
#if 0
            case 1:
                buf[0] = bdata[x * mult + ofs];
                break;
            case 2:
                buf[0] = bdata[mult * (x * 2 + 0) + ofs];
                buf[1] = bdata[mult * (x * 2 + 1) + ofs];
                break;
#endif
            case 3:
                buf[0] = bdata[mult * (x * 3 + 2) + ofs];
                buf[1] = bdata[mult * (x * 3 + 1) + ofs];
                buf[2] = bdata[mult * (x * 3 + 0) + ofs];
                break;
            case 4:
                buf[0] = bdata[mult * (x * 4 + 2) + ofs];
                buf[1] = bdata[mult * (x * 4 + 1) + ofs];
                buf[2] = bdata[mult * (x * 4 + 0) + ofs];
                buf[3] = bdata[mult * (x * 4 + 3) + ofs];
                break;
            }

            //std::cerr << "[tga] x = " << x << "\n";

            if (x == 0) {   // initial encoder state
                rlp = false;
                rlcount = 0;
                rawcount = 1;
                continue; // nothing to work with yet (need 2 pixels)
            }

            if (rlp) {  // in the middle of a run-length packet
                // flush the packet if the run ends or max packet size is hit
                if (rlcount < 0x80
                    && buf[0] == buf2[0] && buf[1] == buf2[1]
                    && buf[2] == buf2[2] && buf[3] == buf2[3])
                    rlcount++;
                else {
                    // run broken or max size hit, flush RL packet and start
                    // a new raw one
                    flush_rlp (&buf2[0], rlcount);
                    // count raw pixel
                    rawcount++;
                    // reset state
                    rlcount -= 0x80;
                    if (rlcount < 0)
                        rlcount = 0;
                    rlp = false;
                }
            } else {    // in the middle of a raw data packet
                if (rawcount > 0    // make sure we have material to check
                    && buf[0] == buf2[0] && buf[1] == buf2[1]
                    && buf[2] == buf2[2] && buf[3] == buf2[3]) {
                    // run continues, possibly material for RLE
                    if (rlcount == 0) {
                        // join the previous pixel into the run
                        rawcount--;
                        rlcount++;
                    }
                    rlcount++;
                } else {
                    // run broken
                    // apart from the pixel we've just read, add any remaining
                    // ones we may have considered for RLE
                    rawcount += 1 + rlcount;
                    rlcount = 0;
                    // flush the packet if max packet size is hit
                    if (rawcount >= 0x80) {
                        // subtract 128 instead of setting to 0 because there
                        // is a chance that rawcount is now > 128; if so, we'll
                        // catch the remainder in the next iteration
                        rawcount -= 0x80;
                        flush_rawp (bdata, 0x80,
                                    x - 0x80 + 1,
                                    ofs, mult);
                    }
                }
                // check the encoding profitability condition
                //if (rlcount * m_spec.nchannels > 1 + m_spec.nchannels) {
                // NOTE: the condition below is valid, nchannels can be 1
                if (rlcount > 1 + 1 / m_spec.nchannels) {
                    // flush a packet of what we had so far
                    flush_rawp (bdata, rawcount,
                                x - rawcount - rlcount + 1,
                                ofs, mult);
                    // reset state
                    rawcount = 0;
                    // mark this as a run-length packet
                    rlp = true;
                }
            }
        }
        // flush anything that may be left
        if (rlp)
            flush_rlp (&buf2[0], rlcount);
        else {
            rawcount += rlcount;
            flush_rawp (bdata, rawcount,
                        m_spec.width - rawcount,
                        ofs, mult);
        }
    } else {
        // raw, non-compressed data
        // seek to the correct scanline
        fseek(m_file, 18 + m_idlen + (m_spec.height - y - 1)
              * m_spec.width * m_spec.nchannels, SEEK_SET);
        for (int x = 0; x < m_spec.width; x++) {
            switch (m_spec.nchannels) {
            case 1:
                buf[0] = bdata[x * mult + ofs];
                fwrite (buf, 1, 1, m_file);
                break;
            case 2:
                buf[0] = bdata[mult * (x * 2 + 0) + ofs];
                buf[1] = bdata[mult * (x * 2 + 1) + ofs];
                fwrite (buf, 2, 1, m_file);
                break;
            case 3:
                buf[0] = bdata[mult * (x * 3 + 2) + ofs];
                buf[1] = bdata[mult * (x * 3 + 1) + ofs];
                buf[2] = bdata[mult * (x * 3 + 0) + ofs];
                fwrite (buf, 3, 1, m_file);
                break;
            case 4:
                buf[0] = bdata[mult * (x * 4 + 2) + ofs];
                buf[1] = bdata[mult * (x * 4 + 1) + ofs];
                buf[2] = bdata[mult * (x * 4 + 0) + ofs];
                buf[3] = bdata[mult * (x * 4 + 3) + ofs];
                fwrite (buf, 4, 1, m_file);
                break;
            }
        }
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

