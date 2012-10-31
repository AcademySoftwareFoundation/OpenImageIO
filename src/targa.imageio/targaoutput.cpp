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
#include "strutil.h"
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace TGA_pvt;


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
                       OpenMode mode=Create);
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

    // Helper function to write the TGA 2.0 data fields, called by close()
    bool write_tga20_data_fields ();

    /// Helper function to flush a non-run-length packet
    ///
    inline void flush_rawp (unsigned char *& src, int size, int start);

    /// Helper function to flush a run-length packet
    ///
    inline void flush_rlp (unsigned char *buf, int size);

    /// Helper - write, with error detection (no byte swapping!)
    template <class T>
    bool fwrite (const T *buf, size_t itemsize=sizeof(T), size_t nitems=1) {
        if (itemsize*nitems == 0)
            return true;
        size_t n = std::fwrite (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Write error: wrote %d records of %d", (int)n, (int)nitems);
        return n == nitems;
    }

    /// Helper -- write a 'short' with byte swapping if necessary
    bool fwrite (uint16_t s) {
        if (bigendian())
            swap_endian (&s);
        return fwrite (&s, sizeof(s), 1);
    }
    bool fwrite (uint32_t i) {
        if (bigendian())
            swap_endian (&i);
        return fwrite (&i, sizeof(i), 1);
    }

    /// Helper -- pad with zeroes
    bool pad (size_t n=1) {
        while (n--)
            if (fputc (0, m_file))
                return false;
        return true;
    }

    /// Helper -- write string, with padding and/or truncation
    bool fwrite_padded (const std::string &s, size_t len) {
        size_t slen = std::min (s.length(), len-1);
        return fwrite (s.c_str(), slen)  &&  pad(len-slen);
    }
        
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput *targa_output_imageio_create () { return new TGAOutput; }

// OIIO_EXPORT int tga_imageio_version = OIIO_PLUGIN_VERSION;   // it's in tgainput.cpp

OIIO_EXPORT const char * targa_output_extensions[] = {
    "tga", "tpic", NULL
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
TGAOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

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

    m_file = Filesystem::fopen (name, "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // Force 8 bit integers
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
    if (!fwrite(&tga.idlen) ||
        !fwrite(&tga.cmap_type) ||
        !fwrite(&tga.type) ||
        !fwrite(&tga.cmap_first) ||
        !fwrite(&tga.cmap_length) ||
        !fwrite(&tga.cmap_size) ||
        !fwrite(&tga.x_origin) ||
        !fwrite(&tga.y_origin) ||
        !fwrite(&tga.width) ||
        !fwrite(&tga.height) ||
        !fwrite(&tga.bpp) ||
        !fwrite(&tga.attr)) {
        fclose (m_file);  m_file = NULL;
        return false;
    }


    // dump comment to file, don't bother about null termination
    if (tga.idlen) {
        if (!fwrite(id.c_str(), tga.idlen)) {
            fclose (m_file);  m_file = NULL;
            return false;
        }
    }

    return true;
}



bool
TGAOutput::write_tga20_data_fields ()
{
    if (m_file) {
        // write out the TGA 2.0 data fields

        // FIXME: write out the developer area; according to Larry,
        // it's probably safe to ignore it altogether until someone complains
        // that it's missing :)

        fseek (m_file, 0, SEEK_END);

        // write out the thumbnail, if there is one
        uint32_t ofs_thumb = 0;
        unsigned char tw = m_spec.get_int_attribute ("thumbnail_width", 0);
        unsigned char th = m_spec.get_int_attribute ("thumbnail_width", 0);
        int tc = m_spec.get_int_attribute ("thumbnail_nchannels", 0);
        if (tw && th && tc == m_spec.nchannels) {
            ImageIOParameter *p = m_spec.find_attribute ("thumbnail_image");
            if (p) {
                ofs_thumb = (uint32_t) ftell (m_file);
                // dump thumbnail size
                if (!fwrite (&tw) ||
                    !fwrite (&th) ||
                    !fwrite (p->data(), p->datasize())) {
                    return false;
                }
            }
        }
        
        // prepare the footer
        tga_footer foot = {(uint32_t)ftell (m_file), 0, "TRUEVISION-XFILE."};

        // write out the extension area

        // ext area size -- 2 bytes, always short(495)
        fwrite (uint16_t(495));

        // author - 41 bytes
        fwrite_padded (m_spec.get_string_attribute("Artist"), 41);

        // image comment - 324 bytes
        fwrite_padded (m_spec.get_string_attribute("ImageDescription"), 324);

        // timestamp - 6 shorts (month, day, year, hour, minute, second)
        {
            std::string dt = m_spec.get_string_attribute ("DateTime", "");
            uint16_t y, m, d, h, i, s;
            if (dt.length() > 0)
                sscanf (dt.c_str(), "%04hu:%02hu:%02hu %02hu:%02hu:%02hu",
                        &y, &m, &d, &h, &i, &s);
            else
                y = m = d = h = i = s = 0;
            if (!fwrite(m) || !fwrite(d) || !fwrite(y) ||
                !fwrite(h) || !fwrite(i) || !fwrite(s)) {
                return false;
            }
        }

        // job ID - 41 bytes
        fwrite_padded (m_spec.get_string_attribute("DocumentName"), 41);

        // job time - 3 shorts (hours, minutes, seconds)
        {
            std::string jt = m_spec.get_string_attribute ("targa:JobTime", "");
            uint16_t h, m, s;
            if (jt.length() > 0)
                sscanf (jt.c_str(), "%hu:%02hu:%02hu", &h, &m, &s);
            else
                h = m = s = 0;
            if (!fwrite(h) || !fwrite(m) || !fwrite(s))
                return false;
        }

        // software ID -- 41 bytes
        fwrite_padded (m_spec.get_string_attribute("Software"), 41);

        // software version - 3 bytes (first 2 bytes: version*100)
        if (!fwrite(uint16_t(OIIO_VERSION)))
            return false;
        pad (1);

        // key colour (ARGB) -- punt and write 0
        pad (4);

        // pixel aspect ratio -- two shorts, giving a ratio
        {
            float ratio = m_spec.get_float_attribute ("PixelAspectRatio", 1.f);
            float EPS = 1E-5f;
            if (ratio >= (0.f+EPS) && ((ratio <= (1.f-EPS))||(ratio >= (1.f+EPS)))) {
                // FIXME: invent a smarter way to convert to a vulgar fraction?
                // numerator
                fwrite (uint16_t(ratio * 10000.f));  // numerator
                fwrite (uint16_t(10000));            // denominator
            } else {
                // just dump two zeros in there
                fwrite (uint16_t(0));
                fwrite (uint16_t(0));
            }
        }

        // gamma -- two shorts, giving a ratio
        if (Strutil::iequals (m_spec.get_string_attribute("oiio:ColorSpace"), "GammaCorrected")) {
            float gamma = m_spec.get_float_attribute ("oiio:Gamma", 1.0);
            // FIXME: invent a smarter way to convert to a vulgar fraction?
            // NOTE: the spec states that only 1 decimal place of precision
            // is needed, thus the expansion by 10
            // numerator
            fwrite (uint16_t(gamma*10.0f));
            fwrite (uint16_t(10));
        } else {
            // just dump two zeros in there
            fwrite (uint16_t(0));
            fwrite (uint16_t(0));
        }

        // offset to colour correction table - 4 bytes
        // FIXME: support this once it becomes clear how it's actually supposed
        // to be used... the spec is very unclear about this
        // for the time being just dump four NULL bytes
        pad (4);

        // offset to thumbnail - 4 bytes
        if (!fwrite(ofs_thumb))
            return false;

        // offset to scanline table - 4 bytes
        // not used very widely, don't bother unless someone complains
        pad (4);

        // alpha type - one byte
        unsigned char at = (m_spec.nchannels % 2 == 0)
                             ? TGA_ALPHA_USEFUL : TGA_ALPHA_NONE;
        if (!fwrite(&at))
            return false;

        // write out the TGA footer
        if (!fwrite(foot.ofs_ext) ||
            !fwrite(foot.ofs_dev) ||
            !fwrite(&foot.signature, 1, sizeof(foot.signature))) {
            return false;
        }
    }

    return true;
}



bool
TGAOutput::close ()
{
    bool ok = true;
    if (m_file) {
        ok &= write_tga20_data_fields ();
        // close the stream
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return ok;
}



inline void
TGAOutput::flush_rlp (unsigned char *buf, int size)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    unsigned char h = (size - 1) | 0x80;
    // write packet pixel
    if (!fwrite(&h) || !fwrite (buf, m_spec.nchannels)) {
        // do something intelligent?
        return;
    }
}



inline void
TGAOutput::flush_rawp (unsigned char *& src, int size, int start)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    unsigned char h = (size - 1) & ~0x80;
    if (!fwrite (&h))
        return;
    // rewind the scanline and flush packet pixels
    unsigned char buf[4];
    int n = m_spec.nchannels;
    for (int i = 0; i < size; i++) {
        if (n <= 2) {
            // 1- and 2-channels can write directly
            if (!fwrite (src+start, n)) {
                return;
            }
        } else {
            // 3- and 4-channel must swap red and blue
            buf[0] = src[(start + i) * n + 2];
            buf[1] = src[(start + i) * n + 1];
            buf[2] = src[(start + i) * n + 0];
            if (n > 3)
                buf[3] = src[(start + i) * n + 3];
            if (!fwrite (buf, n)) {
                return;
            }
        }
    }
}



bool
TGAOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (m_scratch.empty() || data != &m_scratch[0]) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    ASSERT (m_spec.format == TypeDesc::UINT8 &&
            "Targa only supports 8 bit channels");
    //std::cerr << "[tga] writing scanline #" << y << "\n";

    unsigned char *bdata = (unsigned char *)data;

    if (m_want_rle) {
        // Run Length Encoding
        // it's only profitable if n * b > 1 + b, where:
        // n is the number of pixels in a run
        // b is the pixel size in bytes
        // FIXME: optimize runs spanning across multiple scanlines?
        unsigned char buf[4] = { 0, 0, 0, 0 };
        unsigned char buf2[4] = { 0, 0, 0, 0 };
        bool rlp = false;
        int rlcount = 0, rawcount = 0;

        for (int x = 0; x < m_spec.width; x++) {
            // save off previous pixel
            memcpy (buf2, buf, sizeof (buf2));
            // read the new one
            switch (m_spec.nchannels) {
#if 0
            case 1:
                buf[0] = bdata[x ];
                break;
            case 2:
                buf[0] = bdata[(x * 2 + 0)];
                buf[1] = bdata[(x * 2 + 1)];
                break;
#endif
            case 3:
                buf[0] = bdata[(x * 3 + 2)];
                buf[1] = bdata[(x * 3 + 1)];
                buf[2] = bdata[(x * 3 + 0)];
                break;
            case 4:
                buf[0] = bdata[(x * 4 + 2)];
                buf[1] = bdata[(x * 4 + 1)];
                buf[2] = bdata[(x * 4 + 0)];
                buf[3] = bdata[(x * 4 + 3)];
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
                        flush_rawp (bdata, 0x80, x - 0x80 + 1);
                    }
                }
                // check the encoding profitability condition
                //if (rlcount * m_spec.nchannels > 1 + m_spec.nchannels) {
                // NOTE: the condition below is valid, nchannels can be 1
                if (rlcount > 1 + 1 / m_spec.nchannels) {
                    // flush a packet of what we had so far
                    flush_rawp (bdata, rawcount, x - rawcount - rlcount + 1);
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
            flush_rawp (bdata, rawcount, m_spec.width - rawcount);
        }
    } else {
        // raw, non-compressed data
        // seek to the correct scanline
        int n = m_spec.nchannels;
        int w = m_spec.width;
        fseek(m_file, 18 + m_idlen + (m_spec.height - y - 1) * w * n, SEEK_SET);
        if (n <= 2) {
            // 1- and 2-channels can write directly
            if (!fwrite (bdata, n, w)) {
                return false;
            }
        } else {
            // 3- and 4-channels must swap R and B
            std::vector<unsigned char> buf;
            buf.assign (bdata, bdata + n*w);
            for (int x = 0; x < m_spec.width; x++)
                std::swap (buf[x*n], buf[x*n+2]);

            if (!fwrite (&buf[0], n, w)) {
                return false;
            }
        }
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

