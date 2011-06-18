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
    long m_sot;                       ///< Scanline offset table offset in file
    int m_stride;                     ///< Number of bytes a contig pixel takes
    bool m_Yflip;                     ///< Some non fully spec-compliant files
                                      ///  will have their Y axis inverted

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
    bool decode_plane (short chan_type, short num_channels, int offset);
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
    RH(Field);
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
        swap_endian (&m_rla.Field);
        swap_endian (&m_rla.NumOfChannelBits);
        swap_endian (&m_rla.MatteChannelType);
        swap_endian (&m_rla.NumOfMatteBits);
        swap_endian (&m_rla.AuxChannelType);
        swap_endian (&m_rla.NumOfAuxBits);
        swap_endian (&m_rla.NextOffset);
    }
    // set offset to scanline offset table
    m_sot = ftell (m_file);
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
    
    // pick the highest-precision type
    int ct = std::max (m_rla.ColorChannelType, std::max (m_rla.MatteChannelType,
                                                         m_rla.AuxChannelType));
    int bits = std::max (m_rla.NumOfChannelBits, std::max (m_rla.NumOfMatteBits,
                                                           m_rla.NumOfAuxBits));
    m_stride = m_rla.NumOfColorChannels * std::min (1 << m_rla.ColorChannelType, 4)
        + m_rla.NumOfMatteChannels * std::min (1 << m_rla.MatteChannelType, 4)
        + m_rla.NumOfAuxChannels * std::min (1 << m_rla.AuxChannelType, 4);
    m_Yflip = m_rla.ActiveBottom - m_rla.ActiveTop < 0;
    
    m_spec = ImageSpec (m_rla.ActiveRight - m_rla.ActiveLeft + 1,
                        std::abs (m_rla.ActiveBottom - m_rla.ActiveTop) + 1,
                        m_rla.NumOfColorChannels
                            + m_rla.NumOfMatteChannels
                            + m_rla.NumOfAuxChannels,
                        ct == CT_BYTE ? TypeDesc::UINT8
                            : (ct == CT_WORD ? TypeDesc::UINT16
                                : (ct == CT_DWORD ? TypeDesc::UINT32
                                    : TypeDesc::FLOAT)));
    m_spec.attribute ("oiio:BitsPerSample", m_spec.nchannels * bits);
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
    // zeroes are perfectly fine values for these
    RLA_SET_ATTRIB_NOCHECK(WindowLeft);
    RLA_SET_ATTRIB_NOCHECK(WindowRight);
    RLA_SET_ATTRIB_NOCHECK(WindowBottom);
    RLA_SET_ATTRIB_NOCHECK(WindowTop);
    RLA_SET_ATTRIB_NOCHECK(ActiveLeft);
    RLA_SET_ATTRIB_NOCHECK(ActiveRight);
    RLA_SET_ATTRIB_NOCHECK(ActiveBottom);
    RLA_SET_ATTRIB_NOCHECK(ActiveTop);
    
    RLA_SET_ATTRIB(FrameNumber);
    RLA_SET_ATTRIB(Revision);
    RLA_SET_ATTRIB(JobNumber);
    RLA_SET_ATTRIB(Field);
    RLA_SET_ATTRIB_STR(FileName);
    RLA_SET_ATTRIB_STR(ProgramName);
    RLA_SET_ATTRIB_STR(MachineName);
    RLA_SET_ATTRIB_STR(UserName);
    RLA_SET_ATTRIB_STR(Aspect);
    RLA_SET_ATTRIB_STR(ColorChannel);
    RLA_SET_ATTRIB_STR(Time);
    RLA_SET_ATTRIB_STR(Filter);
    RLA_SET_ATTRIB_STR(AuxData);
#undef RLA_SET_ATTRIB_STR
#undef RLA_SET_ATTRIB

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
RLAInput::decode_plane (short chan_type, short num_channels, int offset)
{
    int chsize = std::min (1 << chan_type, 4);
    unsigned short eb; // number of encoded bytes
    char rc; // run count
    int k;
    std::vector<unsigned char> record;
    for (int i = 0; i < num_channels; ++i) {
        int x = 0; // index of pixel inside the scanline
        if (!fread (&eb, 2, 1))
            return false;
        if (littleendian ())
            swap_endian (&eb);
        record.resize (std::max (record.size (), (size_t)eb));
        if (!fread (&record[0], 1, eb))
            return false;
        k = 0;
        while (k < eb) {
            *(&rc) = *(&record[k++]);
            if (rc > 0) {
                // replicate value run count + 1 times
                for (; rc >= 0; --rc, ++x) {
                    assert(x * m_stride + i * chsize + offset < (int)m_buf.size ());
                    m_buf[x * m_stride + i * chsize + offset] = record[k];
                }
                ++k;
            } else {
                // copy raw values run count times
                for (rc = -rc; rc > 0; --rc, ++x, ++k) {
                    assert(x * m_stride + i * chsize + offset < (int)m_buf.size ());
                    m_buf[x * m_stride + i * chsize + offset] = record[k];
                }
            }
        }
        // make sure we haven't gone way off range
        assert(k - eb < 1);
    }
    return true;
}



bool
RLAInput::read_native_scanline (int y, int z, void *data)
{
    if (m_Yflip)
        y = m_spec.height - y - 1;
    m_buf.resize (m_spec.scanline_bytes());
    // seek to scanline offset table
    fseek (m_file, m_sot + y * 4, SEEK_SET);
    unsigned int ofs;
    if (!fread (&ofs, 4, 1))
        return false;
    if (littleendian ())
        swap_endian (&ofs);
    // seek to scanline start
    fseek (m_file, ofs, SEEK_SET);
    
    ofs = 0;
    // now decode and interleave the planes
    if (!decode_plane(m_rla.ColorChannelType, m_rla.NumOfColorChannels, ofs))
        return false;
    ofs += m_rla.NumOfColorChannels * std::min (1 << m_rla.ColorChannelType, 4);
    if (!decode_plane(m_rla.MatteChannelType, m_rla.NumOfMatteChannels, ofs))
        return false;
    ofs += m_rla.NumOfMatteChannels * std::min (1 << m_rla.MatteChannelType, 4);
    if (!decode_plane(m_rla.AuxChannelType, m_rla.NumOfAuxChannels, ofs))
        return false;

    size_t size = spec().scanline_bytes();
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

OIIO_PLUGIN_NAMESPACE_END

