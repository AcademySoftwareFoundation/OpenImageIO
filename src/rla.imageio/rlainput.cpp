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

#include "rla_pvt.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

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
    WAVEFRONT m_rla;                  ///< Wavefront RLA header
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels
    int m_subimage;                   ///< Current subimage index
    std::vector<long> m_sot;          ///< Scanline offsets table
    int m_stride;                     ///< Number of bytes a contig pixel takes

    /// Reset everything to initial state
    ///
    void init () {
        m_file = NULL;
        m_buf.clear ();
    }

    /// Helper: read, with error detection
    ///
    bool fread (void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fread (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Read error");
        return n == nitems;
    }
    
    /// Helper function: translate 3-letter month abbreviation to number.
    ///
    inline int get_month_number (const char *s);
    
    /// Helper: read the RLA header.
    ///
    inline bool read_header ();
    
    /// Helper: read and decode a single colour plane.
    bool decode_plane (int first_channel, short num_channels, short num_bits);
    
    /// Helper: determine channel TypeDesc
    inline TypeDesc get_channel_typedesc (short chan_type, short chan_bits);
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
    seek_subimage (0, 0, newspec);
    
    return true;
}



inline bool
RLAInput::read_header ()
{
    // due to struct packing, we may get a corrupt header if we just load the
    // struct from file; to adress that, read every member individually
    // save some typing
#define RH(memb)  if (! fread (&m_rla.memb, sizeof (m_rla.memb), 1)) \
                      return false
    RH(WindowLeft);
    RH(WindowRight);
    RH(WindowBottom);
    RH(WindowTop);
    RH(ActiveLeft);
    RH(ActiveRight);
    RH(ActiveBottom);
    RH(ActiveTop);
    RH(FrameNumber);
    RH(ColorChannelType);
    RH(NumOfColorChannels);
    RH(NumOfMatteChannels);
    RH(NumOfAuxChannels);
    RH(Revision);
    RH(Gamma);
    RH(RedChroma);
    RH(GreenChroma);
    RH(BlueChroma);
    RH(WhitePoint);
    RH(JobNumber);
    RH(FileName);
    RH(Description);
    RH(ProgramName);
    RH(MachineName);
    RH(UserName);
    RH(DateCreated);
    RH(Aspect);
    RH(AspectRatio);
    RH(ColorChannel);
    RH(FieldRendered);
    RH(Time);
    RH(Filter);
    RH(NumOfChannelBits);
    RH(MatteChannelType);
    RH(NumOfMatteBits);
    RH(AuxChannelType);
    RH(NumOfAuxBits);
    RH(AuxData);
    RH(Reserved);
    RH(NextOffset);
#undef RH
    if (littleendian()) {
        // RLAs are big-endian
        swap_endian (&m_rla.WindowLeft);
        swap_endian (&m_rla.WindowRight);
        swap_endian (&m_rla.WindowBottom);
        swap_endian (&m_rla.WindowTop);
        swap_endian (&m_rla.ActiveLeft);
        swap_endian (&m_rla.ActiveRight);
        swap_endian (&m_rla.ActiveBottom);
        swap_endian (&m_rla.ActiveTop);
        swap_endian (&m_rla.FrameNumber);
        swap_endian (&m_rla.ColorChannelType);
        swap_endian (&m_rla.NumOfColorChannels);
        swap_endian (&m_rla.NumOfMatteChannels);
        swap_endian (&m_rla.NumOfAuxChannels);
        swap_endian (&m_rla.Revision);
        swap_endian (&m_rla.JobNumber);
        swap_endian (&m_rla.FieldRendered);
        swap_endian (&m_rla.NumOfChannelBits);
        swap_endian (&m_rla.MatteChannelType);
        swap_endian (&m_rla.NumOfMatteBits);
        swap_endian (&m_rla.AuxChannelType);
        swap_endian (&m_rla.NumOfAuxBits);
        swap_endian (&m_rla.NextOffset);
    }
    // load the scanline offset table
    m_sot.clear ();
    m_sot.resize (std::abs (m_rla.ActiveBottom - m_rla.ActiveTop) + 1);
    long ofs;
    for (unsigned int y = 0; y < m_sot.size (); ++y) {
        if (!fread (&ofs, 4, 1))
            return false;
        if (littleendian ())
            swap_endian (&ofs);
        m_sot[y] = ofs;
    }
    return true;
}



bool
RLAInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0 || subimage < 0)
        return false;
    
    int diff = subimage - current_subimage ();
    
    if (diff == 0)
        // don't need to do anything
        return true;
    if (subimage - current_subimage () < 0) {
        // need to rewind to the beginning
        fseek (m_file, 0, SEEK_SET);
        if (!read_header ()) {
            error ("Corrupt RLA header");
            return false;
        }
        diff = subimage;
    }
    // forward scrolling
    while (diff > 0 && m_rla.NextOffset != 0) {
        fseek (m_file, m_rla.NextOffset, SEEK_SET);
        if (!read_header ()) {
            error ("Corrupt RLA header");
            return false;
        }
        --diff;
    }
    if (diff > 0 && m_rla.NextOffset == 0)
        // no more subimages to read
        return false;
    
    // now read metadata
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
    TypeDesc maxtype = (maxbytes == 4) ? TypeDesc::UINT32
                     : (maxbytes == 2 ? TypeDesc::UINT16 : TypeDesc::UINT8);
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
                char buf[20];
                sprintf(buf, "%4d:%02d:%02d %02d:%02d:00", y, M, d, h, m);
                m_spec.attribute ("DateTime", buf);
            }
        }
    }
    
    if (m_rla.Description[0])
        m_spec.attribute ("ImageDescription", m_rla.Description);
    
    // save some typing by using macros
#define RLA_SET_ATTRIB_NOCHECK(x)       m_spec.attribute ("rla:"#x, m_rla.x)
#define RLA_SET_ATTRIB(x)               if (m_rla.x > 0) \
                                            RLA_SET_ATTRIB_NOCHECK(x)
#define RLA_SET_ATTRIB_STR(x)           if (m_rla.x[0]) \
                                            RLA_SET_ATTRIB_NOCHECK(x)
    RLA_SET_ATTRIB(FrameNumber);
    RLA_SET_ATTRIB(Revision);
    RLA_SET_ATTRIB(JobNumber);
    RLA_SET_ATTRIB(FieldRendered);
    RLA_SET_ATTRIB_STR(FileName);
    RLA_SET_ATTRIB_STR(MachineName);
    RLA_SET_ATTRIB_STR(UserName);
    RLA_SET_ATTRIB_STR(Aspect);
    RLA_SET_ATTRIB_STR(ColorChannel);
    RLA_SET_ATTRIB_STR(Time);
    RLA_SET_ATTRIB_STR(Filter);
    RLA_SET_ATTRIB_STR(AuxData);
#undef RLA_SET_ATTRIB_STR
#undef RLA_SET_ATTRIB
#undef RLA_SET_ATTRIB_NOCHECK
    
    if (m_rla.ProgramName[0])
        m_spec.attribute ("Software", m_rla.ProgramName);
    if (m_rla.MachineName[0])
        m_spec.attribute ("HostComputer", m_rla.MachineName);

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
        m_spec.attribute ("rla:AspectRatio", f[0]);
    
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



bool
RLAInput::decode_plane (int first_channel, short num_channels, short num_bits)
{
    int chsize, offset;
    bool is_float; // float channels are not RLEd
    if (m_spec.channelformats.size()) {
        chsize = m_spec.channelformats[first_channel].size ();
        is_float = m_spec.channelformats[first_channel] == TypeDesc::FLOAT;
        offset = 0;
        for (int i = 0; i < first_channel; ++i)
            offset += m_spec.channelformats[i].size ();
    } else {
        chsize = m_spec.format.size ();
        is_float = m_spec.format == TypeDesc::FLOAT;
        offset = first_channel * chsize;
    }

    if (is_float) {
        // floats are not run-length encoded, but simply dumped, all of them
        unsigned short length; // number of encoded bytes
        std::vector<float> record;
        float *out;
        for (int i = 0; i < num_channels; ++i) {
            if (!fread (&length, 2, 1))
                return false;
            if (littleendian ())
                swap_endian (&length);
            record.resize (std::max (record.size (), (size_t)length / sizeof (float)));
            ASSERT(length <= m_buf.size ());
            if (!fread (&record[0], 1, length))
                return false;
            out = (float *)&m_buf[i * chsize + offset];
            for (std::vector<float>::iterator it = record.begin ();
                it != record.end ();
                ++it, out = (float *)((unsigned char *)(out) + m_stride)) {
                ASSERT((unsigned char *)out - &m_buf[0] < (int)m_buf.size ());
                *out = *it;
            }
        }
    } else {
        // integer values, run-length encoded
        unsigned short length; // number of encoded bytes
        char rc; // run count
        unsigned char *p; // pointer to current byte
        std::vector<unsigned char> record;
        for (int i = 0; i < num_channels * chsize; ++i) {
            int x = 0; // index of pixel inside the scanline
            if (!fread (&length, 2, 1))
                return false;
            if (littleendian ())
                swap_endian (&length);
            record.resize (std::max (record.size (), (size_t)length));
            if (!fread (&record[0], 1, length))
                return false;
            p = &record[0];
            while (p - &record[0] < length) {
                rc = *((char *)p++);
                if (rc >= 0) {
                    // replicate value run count + 1 times
                    for (; rc >= 0; --rc, ++x) {
                        ASSERT(x * m_stride + i + offset < (int)m_buf.size ());
                        m_buf[x * m_stride + i + offset] = *p;
                    }
                    // advance pointer by 1 datum
                    ++p;
                } else if (rc < 0) {
                    // copy raw values run count times
                    for (; rc < 0; ++rc, ++x) {
                        ASSERT(x * m_stride + i + offset < (int)m_buf.size ());
                        m_buf[x * m_stride + i + offset] = *p;
                        // advance pointer by 1 datum
                        ++p;
                    }
                }
                // exceeding the width while remaining inside the record means that
                // the less significant byte pass begins
                if (x >= m_spec.width && p - &record[0] < length) {
                    x = 0;
                    ++i;
                    if (i >= num_channels * chsize)
                        break;
                }
            }
            // make sure we haven't gone way off range
            ASSERT(p - &record[0] <= length);
        }
    }
    // reverse byte order (RLA is always big-endian) and expand bit range if needed
    if (chsize > 1 && littleendian () != is_float) {
        for (int i = 0; i < num_channels; ++i) {
            for (int x = 0; x < m_spec.width; ++x) {
                switch (chsize) {
                    case 2:
                        swap_endian ((short *)&m_buf[x * m_stride
                                                       + i * chsize + offset]);
                        if (!is_float && num_bits == 10)
                            *(short *)&m_buf[x * m_stride + i * chsize + offset] =
                                bit_range_convert<10, 16>(*(short *)&m_buf
                                    [x * m_stride + i * chsize + offset]);
                        break;
                    case 4:
                        swap_endian ((int *)&m_buf[x * m_stride
                                                     + i * chsize + offset]);
                        break;
                    default: ASSERT (!"Invalid channel size!");
                }
            }
        }
    }
    return true;
}



bool
RLAInput::read_native_scanline (int y, int z, void *data)
{
    y = m_spec.height - y - 1;
    m_buf.resize (m_spec.scanline_bytes (true));
    
    // seek to scanline start
    fseek (m_file, m_sot[y], SEEK_SET);
    
    // now decode and interleave the planes
    if (m_rla.NumOfColorChannels > 0)
        if (!decode_plane(0, m_rla.NumOfColorChannels, m_rla.NumOfChannelBits))
            return false;
    if (m_rla.NumOfMatteChannels > 0)
        if (!decode_plane(m_rla.NumOfColorChannels, m_rla.NumOfMatteChannels,
            m_rla.NumOfMatteBits))
            return false;
    if (m_rla.NumOfAuxChannels > 0)
        if (!decode_plane(m_rla.NumOfColorChannels + m_rla.NumOfMatteChannels,
            m_rla.NumOfAuxChannels, m_rla.NumOfAuxBits))
            return false;

    size_t size = spec().scanline_bytes(true);
    memcpy (data, &m_buf[0], size);
    return true;
}



inline int
RLAInput::get_month_number (const char *s)
{
    if (iequals (s, "jan"))
        return 1;
    if (iequals (s, "feb"))
        return 2;
    if (iequals (s, "mar"))
        return 3;
    if (iequals (s, "apr"))
        return 4;
    if (iequals (s, "may"))
        return 5;
    if (iequals (s, "jun"))
        return 6;
    if (iequals (s, "jul"))
        return 7;
    if (iequals (s, "aug"))
        return 8;
    if (iequals (s, "sep"))
        return 9;
    if (iequals (s, "oct"))
        return 10;
    if (iequals (s, "nov"))
        return 11;
    if (iequals (s, "dec"))
        return 12;
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

