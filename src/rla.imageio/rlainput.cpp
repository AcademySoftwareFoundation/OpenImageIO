/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
#include <cassert>

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

#include "rla_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace RLA_pvt;


class RLAInput : public ImageInput {
public:
    RLAInput () { init(); }
    virtual ~RLAInput () { close(); }
    virtual const char * format_name (void) const { return "rla"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool close ();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    RLAHeader m_rla;                  ///< Wavefront RLA header
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels
    int m_subimage;                   ///< Current subimage index
    std::vector<uint32_t> m_sot;      ///< Scanline offsets table
    int m_stride;                     ///< Number of bytes a contig pixel takes

    /// Reset everything to initial state
    ///
    void init () {
        m_file = NULL;
        m_buf.clear ();
    }

    /// Helper: raw read, with error detection
    ///
    bool fread (void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fread (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Read error: read %d records but %d expected %s",
                   (int)n, (int)nitems, feof(m_file) ? " (hit EOF)" : "");
        return n == nitems;
    }

    /// Helper: read buf[0..nitems-1], swap endianness if necessary
    template<typename T>
    bool read (T *buf, size_t nitems=1) {
        if (! fread (buf, sizeof(T), nitems))
            return false;
        if (littleendian() &&
            (is_same<T,uint16_t>::value || is_same<T,int16_t>::value ||
             is_same<T,uint32_t>::value || is_same<T,int32_t>::value)) {
            swap_endian (buf, nitems);
        }
        return true;
    }

    /// Helper function: translate 3-letter month abbreviation to number.
    ///
    inline int get_month_number (const char *s);
    
    /// Helper: read the RLA header and scanline offset table.
    ///
    inline bool read_header ();
    
    /// Helper: read and decode a single channel group consisting of
    /// channels [first_channel .. first_channel+num_channels-1], which
    /// all share the same number of significant bits.
    bool decode_channel_group (int first_channel, short num_channels,
                               short num_bits, int y);

    /// Helper: decode a span of n RLE-encoded bytes from encoded[0..elen-1]
    /// into buf[0],buf[stride],buf[2*stride]...buf[(n-1)*stride].
    /// Return the number of encoded bytes we ate to fill buf.
    size_t decode_rle_span (unsigned char *buf, int n, int stride,
                            const char *encoded, size_t elen);
    
    /// Helper: determine channel TypeDesc
    inline TypeDesc get_channel_typedesc (short chan_type, short chan_bits);

    // debugging aid
    void preview (std::ostream &out) {
        ASSERT (!feof(m_file));
        long pos = ftell (m_file);
        out << "@" << pos << ", next 4 bytes are ";
        union {  // trickery to avoid punned pointer warnings
            unsigned char c[4];
            uint16_t s[2];
            uint32_t i;
        } u;
        read (&u.c, 4);   // because it's char, it didn't swap endian
        uint16_t s[2] = { u.s[0], u.s[1] };
        uint32_t i = u.i;
        if (littleendian()) {
            swap_endian (s, 2);
            swap_endian (&i);
        }
        out << Strutil::format ("%d/%u %d/%u %d/%u %d/%u (%d %d) (%u)\n",
                                u.c[0], ((char *)u.c)[0],
                                u.c[1], ((char *)u.c)[1],
                                u.c[2], ((char *)u.c)[2],
                                u.c[3], ((char *)u.c)[3],
                                s[0], s[1], i);
        fseek (m_file, pos, SEEK_SET);
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *rla_input_imageio_create () { return new RLAInput; }

DLLEXPORT int rla_imageio_version = OIIO_PLUGIN_VERSION;

DLLEXPORT const char * rla_input_extensions[] = {
    "rla", NULL
};

OIIO_PLUGIN_EXPORTS_END



bool
RLAInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;

    m_file = fopen (name.c_str(), "rb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }
    
    // set a bogus subimage index so that seek_subimage actually seeks
    m_subimage = 1;
    return seek_subimage (0, 0, newspec);
}



inline bool
RLAInput::read_header ()
{
    // Read the image header, which should have the same exact layout as
    // the m_rla structure (except for endianness issues).
    ASSERT (sizeof(m_rla) == 740 && "Bad RLA struct size");
    if (! read (&m_rla)) {
        error ("RLA could not read the image header");
        return false;
    }
    m_rla.rla_swap_endian ();  // fix endianness

    if (m_rla.Revision != (int16_t)0xFFFE &&
        m_rla.Revision != 0 /* for some reason, this can happen */) {
        error ("RLA header Revision number unrecognized: %d", m_rla.Revision);
        return false;   // unknown file revision
    }
    if (m_rla.NumOfChannelBits == 0)
        m_rla.NumOfChannelBits = 8;  // apparently, this can happen

    // Immediately following the header is the scanline offset table --
    // one uint32_t for each scanline, giving absolute offsets (from the
    // beginning of the file) where the RLE records start for each
    // scanline of this subimage.
    m_sot.resize (std::abs (m_rla.ActiveBottom - m_rla.ActiveTop) + 1, 0);
    if (! read (&m_sot[0], m_sot.size())) {
        error ("RLA could not read the scanline offset table");
        return false;
    }
    return true;
}



bool
RLAInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0 || subimage < 0)
        return false;

    if (subimage == current_subimage())
        return true;    // already on the right level

    // RLA images allow multiple subimages; they are simply concatenated
    // together, wth image N's header field NextOffset giving the
    // absolute offset of the start of image N+1.
    int diff = subimage - current_subimage ();
    if (subimage - current_subimage () < 0) {
        // If we are requesting an image earlier than the current one,
        // reset to the first subimage.
        fseek (m_file, 0, SEEK_SET);
        if (!read_header ())
            return false;  // read_header always calls error()
        diff = subimage;
    }
    // forward scrolling -- skip subimages until we're at the right place
    while (diff > 0 && m_rla.NextOffset != 0) {
        fseek (m_file, m_rla.NextOffset, SEEK_SET);
        if (!read_header ())
            return false;  // read_header always calls error()
        --diff;
    }
    if (diff > 0 && m_rla.NextOffset == 0) {  // no more subimages to read
        error ("Unknown subimage");
        return false;
    }

    // Now m_rla holds the header of the requested subimage.  Examine it
    // to fill out our ImageSpec.

    if (m_rla.ColorChannelType > CT_FLOAT) {
        error ("Illegal color channel type: %d", m_rla.ColorChannelType);
        return false;
    }
    if (m_rla.MatteChannelType > CT_FLOAT) {
        error ("Illegal matte channel type: %d", m_rla.MatteChannelType);
        return false;
    }
    if (m_rla.AuxChannelType > CT_FLOAT) {
        error ("Illegal auxiliary channel type: %d", m_rla.AuxChannelType);
        return false;
    }

    // pick maximum precision for the time being
    int maxbytes = (std::max (m_rla.NumOfChannelBits * (m_rla.NumOfColorChannels > 0 ? 1 : 0),
                             std::max (m_rla.NumOfMatteBits * (m_rla.NumOfMatteChannels > 0 ? 1 : 0),
                                       m_rla.NumOfAuxBits * (m_rla.NumOfAuxChannels > 0 ? 1 : 0)))
                   + 7) / 8;
    int nchannels = m_rla.NumOfColorChannels + m_rla.NumOfMatteChannels
                                             + m_rla.NumOfAuxChannels;
    TypeDesc maxtype = (maxbytes == 4) ? TypeDesc::UINT32
                     : (maxbytes == 2 ? TypeDesc::UINT16 : TypeDesc::UINT8);
    if (nchannels < 1 || nchannels > 16 ||
        (maxbytes != 1 && maxbytes != 2 && maxbytes != 4)) {
        error ("Failed channel bytes sanity check");
        return false;   // failed sanity check
    }

    m_spec = ImageSpec (m_rla.ActiveRight - m_rla.ActiveLeft + 1,
                        (m_rla.ActiveTop - m_rla.ActiveBottom + 1)
                            / (m_rla.FieldRendered ? 2 : 1), // interlaced image?
                        m_rla.NumOfColorChannels
                        + m_rla.NumOfMatteChannels
                        + m_rla.NumOfAuxChannels, maxtype);
    
    // set window dimensions etc.
    m_spec.x = m_rla.ActiveLeft;
    m_spec.y = m_spec.height - m_rla.ActiveTop - 1;
    m_spec.full_width = m_rla.WindowRight - m_rla.WindowLeft + 1;
    m_spec.full_height = m_rla.WindowTop - m_rla.WindowBottom + 1;
    m_spec.full_depth = 1;
    m_spec.full_x = m_rla.WindowLeft;
    m_spec.full_y = m_spec.full_height - m_rla.WindowTop - 1;

    // set channel formats and stride
    m_stride = 0;
    TypeDesc t = get_channel_typedesc (m_rla.ColorChannelType, m_rla.NumOfChannelBits);
    for (int i = 0; i < m_rla.NumOfColorChannels; ++i)
        m_spec.channelformats.push_back (t);
    m_stride += m_rla.NumOfColorChannels * t.size ();
    t = get_channel_typedesc (m_rla.MatteChannelType, m_rla.NumOfMatteBits);
    for (int i = 0; i < m_rla.NumOfMatteChannels; ++i)
        m_spec.channelformats.push_back (t);
    m_stride += m_rla.NumOfMatteChannels * t.size ();
    t = get_channel_typedesc (m_rla.AuxChannelType, m_rla.NumOfAuxBits);
    for (int i = 0; i < m_rla.NumOfAuxChannels; ++i)
        m_spec.channelformats.push_back (t);
    m_stride += m_rla.NumOfAuxChannels * t.size ();

    // But if all channels turned out the same, just use 'format' and don't
    // bother sending back channelformats at all.
    bool allsame = true;
    for (int c = 1;  c < m_spec.nchannels;  ++c)
        allsame &= (m_spec.channelformats[c] == m_spec.channelformats[0]);
    if (allsame) {
        m_spec.format = m_spec.channelformats[0];
        m_spec.channelformats.clear();
        m_spec.attribute ("oiio:BitsPerSample", m_rla.NumOfChannelBits);
        // N.B. don't set bps for mixed formats, it isn't well defined
    }

    // make a guess at channel names for the time being
    m_spec.default_channel_names ();
    // this is always true
    m_spec.attribute ("compression", "rle");
    
    if (m_rla.DateCreated[0]) {
        char month[4] = {0, 0, 0, 0};
        int d, h, M, m, y;
        if (sscanf (m_rla.DateCreated, "%c%c%c %d %d:%d %d",
            month + 0, month + 1, month + 2, &d, &h, &m, &y) == 7) {
            M = get_month_number (month);
            if (M > 0) {
                // construct a date/time marker in OIIO convention
                m_spec.attribute ("DateTime", 
                                  Strutil::format("%4d:%02d:%02d %02d:%02d:00", y, M, d, h, m));
            }
        }
    }
    
    // save some typing by using macros
#define FIELD(x,name)             if (m_rla.x > 0)              \
                                            m_spec.attribute (name, m_rla.x)
#define STRING_FIELD(x,name)      if (m_rla.x[0])               \
                                            m_spec.attribute (name, m_rla.x)
    STRING_FIELD (Description, "ImageDescription");
    FIELD (FrameNumber, "rla:FrameNumber");
    FIELD (Revision, "rla:Revision");
    FIELD (JobNumber, "rla:JobNumber");
    FIELD (FieldRendered, "rla:FieldRendered");
    STRING_FIELD (FileName, "rla:FileName");
    STRING_FIELD (ProgramName, "Software");
    STRING_FIELD (MachineName, "HostComputer");
    STRING_FIELD (UserName, "Artist");
    STRING_FIELD (Aspect, "rla:Aspect");
    STRING_FIELD (ColorChannel, "rla:ColorChannel");
    STRING_FIELD (Time, "rla:Time");
    STRING_FIELD (Filter, "rla:Filter");
    STRING_FIELD (AuxData, "rla:AuxData");
#undef STRING_FIELD
#undef FIELD

    float f[3]; // variable will be reused for chroma, thus the array
    f[0] = atof (m_rla.Gamma);
    if (f[0] > 0.f) {
        if (f[0] == 1.f)
            m_spec.attribute ("oiio:ColorSpace", "Linear");
        else {
            m_spec.attribute ("oiio:ColorSpace", "GammaCorrected");
            m_spec.attribute ("oiio:Gamma", f[0]);
        }
    }
    
    f[0] = atof (m_rla.AspectRatio);
    if (f[0] > 0.f)
        m_spec.attribute ("PixelAspectRatio", f[0]);
    
    // read chromaticity points
    if (m_rla.RedChroma[0]) {
        int num = sscanf(m_rla.RedChroma, "%f %f %f", f + 0, f + 1, f + 2);
        if (num >= 2)
            m_spec.attribute ("rla:RedChroma", TypeDesc(TypeDesc::FLOAT,
                              num == 2 ? TypeDesc::VEC2 : TypeDesc::VEC3,
                              TypeDesc::POINT), f);
    }
    if (m_rla.GreenChroma[0]) {
        int num = sscanf(m_rla.GreenChroma, "%f %f %f", f + 0, f + 1, f + 2);
        if (num >= 2)
            m_spec.attribute ("rla:GreenChroma", TypeDesc(TypeDesc::FLOAT,
                              num == 2 ? TypeDesc::VEC2 : TypeDesc::VEC3,
                              TypeDesc::POINT), f);
    }
    if (m_rla.BlueChroma[0]) {
        int num = sscanf(m_rla.BlueChroma, "%f %f %f", f + 0, f + 1, f + 2);
        if (num >= 2)
            m_spec.attribute ("rla:BlueChroma", TypeDesc(TypeDesc::FLOAT,
                              num == 2 ? TypeDesc::VEC2 : TypeDesc::VEC3,
                              TypeDesc::POINT), f);
    }
    if (m_rla.WhitePoint[0]) {
        int num = sscanf(m_rla.WhitePoint, "%f %f %f", f + 0, f + 1, f + 2);
        if (num >= 2)
            m_spec.attribute ("rla:WhitePoint", TypeDesc(TypeDesc::FLOAT,
                              num == 2 ? TypeDesc::VEC2 : TypeDesc::VEC3,
                              TypeDesc::POINT), f);
    }

    newspec = spec ();    
    m_subimage = subimage;
    
    // N.B. the file pointer is now immediately after the scanline
    // offset table for this subimage.
    return true;
}



bool
RLAInput::close ()
{
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init();  // Reset to initial state
    return true;
}



size_t
RLAInput::decode_rle_span (unsigned char *buf, int n, int stride,
                           const char *encoded, size_t elen)
{
    size_t e = 0;
    while (n > 0 && e < elen) {
        char count = encoded[e++];
        if (count >= 0) {
            // run count positive: value repeated count+1 times
            for (int i = 0;  i <= count && n;  ++i, buf += stride, --n)
                *buf = encoded[e];
            ++e;
        } else {
            // run count negative: repeat bytes literally
            count = -count;  // make it positive
            for ( ; count && n > 0 && e < elen; --count, buf += stride, --n)
                *buf = encoded[e++];
        }
    }
    if (n != 0) {
        error ("Read error: malformed RLE record");
        return 0;
    }
    return e;
}



bool
RLAInput::decode_channel_group (int first_channel, short num_channels,
                                short num_bits, int y)
{
    // Some preliminaries -- figure out various sizes and offsets
    int chsize;         // size of the channels in this group, in bytes
    int offset;         // buffer offset to first channel
    int pixelsize;      // spacing between pixels (in bytes) in the output
    TypeDesc chantype;  // data type for the channel
    if (! m_spec.channelformats.size()) {
        // No per-channel formats, they are all the same, so it's easy
        chantype = m_spec.format;
        chsize = chantype.size ();
        offset = first_channel * chsize;
        pixelsize = chsize * m_spec.nchannels;
    } else {
        // Per-channel formats differ, need to sum them up
        chantype = m_spec.channelformats[first_channel];
        chsize = chantype.size ();
        offset = 0;
        pixelsize = m_spec.pixel_bytes (true);
        for (int i = 0; i < first_channel; ++i)
            offset += m_spec.channelformats[i].size ();
    }

    // Read the big-endian values into the buffer.
    // The channels are simply contatenated together in order.
    // Each channel starts with a length, from which we know how many
    // bytes of encoded RLE data to read.  Then there are RLE
    // spans for each 8-bit slice of the channel.
    std::vector<char> encoded;
    for (int c = 0;  c < num_channels;  ++c) {
        // Read the length
        uint16_t length; // number of encoded bytes
        if (!read (&length)) {
            error ("Read error: couldn't read RLE record length");
            return false;
        }
        // Read the encoded RLE record
        encoded.resize (length);
        if (!read (&encoded[0], length)) {
            error ("Read error: couldn't read RLE data span");
            return false;
        }

        if (chantype == TypeDesc::FLOAT) {
            // Special case -- float data is just dumped raw, no RLE
            for (int x = 0;  x < m_spec.width;  ++x)
                *((float *)&m_buf[offset+c*chsize+x*pixelsize]) =
                    ((float *)&encoded[0])[x];
            continue;
        }

        // Decode RLE -- one pass for each significant byte of the file,
        // which we re-interleave properly by passing the right offsets
        // and strides to decode_rle_span.
        size_t eoffset = 0;
        for (int bytes = 0;  bytes < chsize;  ++bytes) {
            size_t e = decode_rle_span (&m_buf[offset+c*chsize+bytes],
                                        m_spec.width, pixelsize,
                                        &encoded[eoffset], length);
            if (! e)
                return false;
            eoffset += e;
        }
    }

    // If we're little endian, swap endianness in place for 2- and
    // 4-byte pixel data.
    if (littleendian()) {
        if (chsize == 2) {
            if (num_channels == m_spec.nchannels)
                swap_endian ((uint16_t *)&m_buf[0], num_channels*m_spec.width);
            else
                for (int x = 0;  x < m_spec.width;  ++x)
                    swap_endian ((uint16_t *)&m_buf[offset+x*pixelsize], num_channels);
        } else if (chsize == 4 && chantype != TypeDesc::FLOAT) {
            if (num_channels == m_spec.nchannels)
                swap_endian ((uint32_t *)&m_buf[0], num_channels*m_spec.width);
            else
                for (int x = 0;  x < m_spec.width;  ++x)
                    swap_endian ((uint32_t *)&m_buf[offset+x*pixelsize], num_channels);
        }
    }

    // If not 8*2^n bits, need to rescale.  For example, if num_bits is
    // 10, the data values run 0-1023, but are stored in uint16.  So we
    // now rescale to the full range of the output buffer range, per
    // OIIO conventions.
    if (num_bits == 8 || num_bits == 16 || num_bits == 32) {
        // ok -- no rescaling needed
    } else if (num_bits == 10) {
        // fast, common case -- use templated hard-code
        for (int x = 0;  x < m_spec.width;  ++x) {
            uint16_t *b = (uint16_t *)(&m_buf[offset+x*pixelsize]);
            for (int c = 0;  c < num_channels;  ++c)
                b[c] = bit_range_convert<10,16> (b[c]);
        }
    } else if (num_bits < 8) {
        // rare case, use slow code to make this clause short and simple
        for (int x = 0;  x < m_spec.width;  ++x) {
            uint8_t *b = (uint8_t *)&m_buf[offset+x*pixelsize];
            for (int c = 0;  c < num_channels;  ++c)
                b[c] = bit_range_convert (b[c], num_bits, 8);
        }
    } else if (num_bits > 8 && num_bits < 16) {
        // rare case, use slow code to make this clause short and simple
        for (int x = 0;  x < m_spec.width;  ++x) {
            uint16_t *b = (uint16_t *)&m_buf[offset+x*pixelsize];
            for (int c = 0;  c < num_channels;  ++c)
                b[c] = bit_range_convert (b[c], num_bits, 16);
        }
    } else if (num_bits > 16 && num_bits < 32) {
        // rare case, use slow code to make this clause short and simple
        for (int x = 0;  x < m_spec.width;  ++x) {
            uint32_t *b = (uint32_t *)&m_buf[offset+x*pixelsize];
            for (int c = 0;  c < num_channels;  ++c)
                b[c] = bit_range_convert (b[c], num_bits, 32);
        }
    }
    return true;
}



bool
RLAInput::read_native_scanline (int y, int z, void *data)
{
    // By convention, RLA images store their images bottom-to-top.
    y = m_spec.height - y - 1;

    // Seek to scanline start, based on the scanline offset table
    fseek (m_file, m_sot[y], SEEK_SET);

    // Now decode and interleave the channels.  
    // The channels are non-interleaved (i.e. rrrrrgggggbbbbb...).
    // Color first, then matte, then auxiliary channels.  We can't
    // decode all in one shot, though, because the data type and number
    // of significant bits may be may be different for each class of
    // channels, so we deal with them separately and interleave into
    // our buffer as we go.
    size_t size = m_spec.scanline_bytes(true);
    m_buf.resize (size);
    if (m_rla.NumOfColorChannels > 0)
        if (!decode_channel_group(0, m_rla.NumOfColorChannels, m_rla.NumOfChannelBits, y))
            return false;
    if (m_rla.NumOfMatteChannels > 0)
        if (!decode_channel_group(m_rla.NumOfColorChannels, m_rla.NumOfMatteChannels,
                                  m_rla.NumOfMatteBits, y))
            return false;
    if (m_rla.NumOfAuxChannels > 0)
        if (!decode_channel_group(m_rla.NumOfColorChannels + m_rla.NumOfMatteChannels,
                                  m_rla.NumOfAuxChannels, m_rla.NumOfAuxBits, y))
            return false;

    memcpy (data, &m_buf[0], size);
    return true;
}



inline int
RLAInput::get_month_number (const char *s)
{
    static const char *months[] = {
        "", "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };
    for (int i = 1;  i <= 12;  ++i)
        if (Strutil::iequals (s, months[i]))
            return i;
    return -1;
}



inline TypeDesc
RLAInput::get_channel_typedesc (short chan_type, short chan_bits)
{
    switch (chan_type) {
        case CT_BYTE:
            // some non-spec-compliant images > 8bpc will have it set to
            // byte anyway, so try guessing by bit depth instead
            if (chan_bits > 8) {
                switch ((chan_bits + 7) / 8) {
                    case 2:
                        return TypeDesc::UINT16;                        
                    case 3:
                    case 4:
                        return TypeDesc::UINT32;
                    default:
                        ASSERT(!"Invalid colour channel type");
                }
            } else
                return TypeDesc::UINT8;            
        case CT_WORD:
            return TypeDesc::UINT16;            
        case CT_DWORD:
            return TypeDesc::UINT32;            
        case CT_FLOAT:
            return TypeDesc::FLOAT;            
        default:
            ASSERT(!"Invalid colour channel type");
    }
    // shut up compiler
    return TypeDesc::UINT8;
}

OIIO_PLUGIN_NAMESPACE_END

