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

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "rla_pvt.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

#ifdef WIN32
# define snprintf _snprintf
#endif


OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace RLA_pvt;


class RLAOutput : public ImageOutput {
public:
    RLAOutput ();
    virtual ~RLAOutput ();
    virtual const char * format_name (void) const { return "rla"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    std::vector<unsigned char> m_scratch;
    WAVEFRONT m_rla;                  ///< Wavefront RLA header
    std::vector<int32_t> m_sot;       ///< Scanline offset table
    std::vector<unsigned char> m_buf; ///< Run record buffer for RLE

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_sot.clear ();
    }
    
    /// Helper - sets a chromaticity from attribute
    inline void set_chromaticity (const ImageIOParameter *p, char *dst,
                                  size_t field_size, const char *default_val);
    
    /// Helper - handles the repetitive work of encoding and writing a channel
    bool encode_plane (const unsigned char *data, stride_t xstride,
                       int chsize, bool is_float);
    
    /// Helper - flushes the current RLE run into the record buffer
    inline void flush_run (int& rawcount, int& rlecount,
                           std::vector<unsigned char>::iterator& it);
    
    /// Helper - flushes the current RLE record into file
    inline bool flush_record (int& rawcount, int& rlecount,
                              unsigned short& length, 
                              std::vector<unsigned char>::iterator& it);
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *rla_output_imageio_create () { return new RLAOutput; }

// DLLEXPORT int rla_imageio_version = OIIO_PLUGIN_VERSION;   // it's in rlainput.cpp

DLLEXPORT const char * rla_output_extensions[] = {
    "rla", NULL
};

OIIO_PLUGIN_EXPORTS_END


RLAOutput::RLAOutput ()
{
    init ();
}



RLAOutput::~RLAOutput ()
{
    // Close, if not already done.
    close ();
}



bool
RLAOutput::supports (const std::string &feature) const
{
    if (feature == "displaywindow")
        return true;
    // Support nothing else nonstandard
    return false;
}



bool
RLAOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

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
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    // prepare and write the RLA header
    memset (&m_rla, 0, sizeof (m_rla));
    // frame and window coordinates
    m_rla.WindowLeft = m_spec.full_x;
    m_rla.WindowRight = m_spec.full_x + m_spec.full_width - 1;
    m_rla.WindowBottom = -m_spec.full_y;
    m_rla.WindowTop = m_spec.full_height - m_spec.full_y - 1;
    
    m_rla.ActiveLeft = m_spec.x;
    m_rla.ActiveRight = m_spec.x + m_spec.width - 1;
    m_rla.ActiveBottom = -m_spec.y;
    m_rla.ActiveTop = m_spec.height - m_spec.y - 1;

    m_rla.FrameNumber = m_spec.get_int_attribute ("rla:FrameNumber", 0);

    // figure out what's going on with the channels
    int remaining = m_spec.nchannels;
    if (m_spec.channelformats.size ()) {
        int streak;
        // accomodate first 3 channels of the same type as colour ones
        for (streak = 1; streak <= 3 && remaining > 0; ++streak, --remaining)
            if (m_spec.channelformats[streak] != m_spec.channelformats[0])
                break;
        m_rla.ColorChannelType = m_spec.channelformats[0] == TypeDesc::FLOAT
            ? CT_FLOAT : CT_BYTE;
        m_rla.NumOfChannelBits = m_spec.channelformats[0].size () * 8;
        // limit to 3 in case the loop went further
        m_rla.NumOfColorChannels = std::min (streak, 3);
        // if we have anything left, treat it as alpha
        if (remaining) {
            for (streak = 1; remaining > 0; ++streak, --remaining)
                if (m_spec.channelformats[m_rla.NumOfColorChannels + streak]
                    != m_spec.channelformats[m_rla.NumOfColorChannels])
                    break;
            m_rla.MatteChannelType = m_spec.channelformats[m_rla.NumOfColorChannels]
                == TypeDesc::FLOAT ? CT_FLOAT : CT_BYTE;
            m_rla.NumOfMatteBits = m_spec.channelformats[m_rla.NumOfColorChannels].size () * 8;
            m_rla.NumOfMatteChannels = streak;
        }
        // and if there's something more left, put it in auxiliary
        if (remaining) {
            for (streak = 1; remaining > 0; ++streak, --remaining)
                if (m_spec.channelformats[m_rla.NumOfColorChannels
                        + m_rla.NumOfMatteChannels + streak]
                    != m_spec.channelformats[m_rla.NumOfColorChannels
                        + m_rla.NumOfMatteChannels])
                    break;
            m_rla.MatteChannelType = m_spec.channelformats[m_rla.NumOfColorChannels
                    + m_rla.NumOfMatteChannels]
                == TypeDesc::FLOAT ? CT_FLOAT : CT_BYTE;
            m_rla.NumOfAuxBits = m_spec.channelformats[m_rla.NumOfColorChannels
                + m_rla.NumOfMatteChannels].size () * 8;
            m_rla.NumOfAuxChannels = streak;
        }
    } else {
        m_rla.ColorChannelType = m_rla.MatteChannelType = m_rla.AuxChannelType =
            m_spec.format == TypeDesc::FLOAT ? CT_FLOAT : CT_BYTE;
        m_rla.NumOfChannelBits = m_rla.NumOfMatteBits = m_rla.NumOfAuxBits =
            m_spec.format.size () * 8;
        if (remaining >= 3) {
            // if we have at least 3 channels, treat them as colour
            m_rla.NumOfColorChannels = 3;
            remaining -= 3;
        } else {
            // otherwise let's say it's luminosity
            m_rla.NumOfColorChannels = 1;
            --remaining;
        }
        // if there's at least 1 more channel, it's alpha
        if (remaining-- > 0)
            ++m_rla.NumOfMatteChannels;
        // anything left is auxiliary
        if (remaining > 0)
            m_rla.NumOfAuxChannels = remaining;
    }
    
    m_rla.Revision = 0xFFFE;
    
    std::string s = m_spec.get_string_attribute ("oiio:ColorSpace", "Unknown");
    if (iequals(s, "Linear"))
        strcpy (m_rla.Gamma, "1.0");
    else if (iequals(s, "GammaCorrected"))
        snprintf (m_rla.Gamma, sizeof(m_rla.Gamma), "%.10f",
            m_spec.get_float_attribute ("oiio:Gamma", 1.f));
    
    const ImageIOParameter *p;
    // default NTSC chromaticities
    p = m_spec.find_attribute ("rla:RedChroma");
    set_chromaticity (p, m_rla.RedChroma, sizeof (m_rla.RedChroma), "0.67 0.08");
    p = m_spec.find_attribute ("rla:GreenChroma");
    set_chromaticity (p, m_rla.GreenChroma, sizeof (m_rla.GreenChroma), "0.21 0.71");
    p = m_spec.find_attribute ("rla:BlueChroma");
    set_chromaticity (p, m_rla.BlueChroma, sizeof (m_rla.BlueChroma), "0.14 0.33");
    p = m_spec.find_attribute ("rla:WhitePoint");
    set_chromaticity (p, m_rla.WhitePoint, sizeof (m_rla.WhitePoint), "0.31 0.316");

    m_rla.JobNumber = m_spec.get_int_attribute ("rla:JobNumber", 0);
    strncpy (m_rla.FileName, name.c_str (), sizeof (m_rla.FileName));
    
    s = m_spec.get_string_attribute ("ImageDescription", "");
    if (s.length ())
        strncpy (m_rla.Description, s.c_str (), sizeof (m_rla.Description));
    
    // yay for advertising!
    strcpy (m_rla.ProgramName, OIIO_INTRO_STRING);
    
    s = m_spec.get_string_attribute ("HostComputer", "");
    if (s.length ())
        strncpy (m_rla.MachineName, s.c_str (), sizeof (m_rla.MachineName));
    s = m_spec.get_string_attribute ("rla:UserName", "");
    if (s.length ())
        strncpy (m_rla.UserName, s.c_str (), sizeof (m_rla.UserName));
    
    // the month number will be replaced with the 3-letter abbreviation
    time_t t = time (NULL);
    strftime (m_rla.DateCreated, sizeof (m_rla.DateCreated), "%m  %d %H:%M %Y",
        localtime (&t));
    // nice little trick - atoi() will convert the month number to integer,
    // which we then use to index this array of constants, and copy the
    // abbreviation back into the date string
    static const char months[12][4] = {
        "JAN",
        "FEB",
        "MAR",
        "APR",
        "MAY",
        "JUN",
        "JUL",
        "AUG",
        "SEP",
        "OCT",
        "NOV",
        "DEC"
    };
    memcpy(m_rla.DateCreated, months[atoi (m_rla.DateCreated) - 1], 3);
    
    // FIXME: it appears that Wavefront have defined a set of aspect names;
    // I think it's safe not to care until someone complains
    s = m_spec.get_string_attribute ("rla:Aspect", "");
    if (s.length ())
        strncpy (m_rla.Aspect, s.c_str (), sizeof (m_rla.Aspect));

    snprintf (m_rla.AspectRatio, sizeof(m_rla.AspectRatio), "%.10f",
        m_spec.get_float_attribute ("PixelAspectRatio", 1.f));
    strcpy (m_rla.ColorChannel, m_spec.get_string_attribute ("rla:ColorChannel",
        "rgb").c_str ());
    m_rla.FieldRendered = m_spec.get_int_attribute ("rla:FieldRendered", 0);
    
    s = m_spec.get_string_attribute ("rla:Time", "");
    if (s.length ())
        strncpy (m_rla.Time, s.c_str (), sizeof (m_rla.Time));
        
    s = m_spec.get_string_attribute ("rla:Filter", "");
    if (s.length ())
        strncpy (m_rla.Filter, s.c_str (), sizeof (m_rla.Filter));
    
    s = m_spec.get_string_attribute ("rla:AuxData", "");
    if (s.length ())
        strncpy (m_rla.AuxData, s.c_str (), sizeof (m_rla.AuxData));
    
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
    // due to struct packing, we may get a corrupt header if we just dump the
    // struct to the file; to adress that, write every member individually
    // save some typing
#define WH(memb)    fwrite (&m_rla.memb, sizeof (m_rla.memb), 1, m_file)
    WH(WindowLeft);
    WH(WindowRight);
    WH(WindowBottom);
    WH(WindowTop);
    WH(ActiveLeft);
    WH(ActiveRight);
    WH(ActiveBottom);
    WH(ActiveTop);
    WH(FrameNumber);
    WH(ColorChannelType);
    WH(NumOfColorChannels);
    WH(NumOfMatteChannels);
    WH(NumOfAuxChannels);
    WH(Revision);
    WH(Gamma);
    WH(RedChroma);
    WH(GreenChroma);
    WH(BlueChroma);
    WH(WhitePoint);
    WH(JobNumber);
    WH(FileName);
    WH(Description);
    WH(ProgramName);
    WH(MachineName);
    WH(UserName);
    WH(DateCreated);
    WH(Aspect);
    WH(AspectRatio);
    WH(ColorChannel);
    WH(FieldRendered);
    WH(Time);
    WH(Filter);
    WH(NumOfChannelBits);
    WH(MatteChannelType);
    WH(NumOfMatteBits);
    WH(AuxChannelType);
    WH(NumOfAuxBits);
    WH(AuxData);
    WH(Reserved);
    WH(NextOffset);
#undef WH
    
    // write placeholder values - not all systems may expand the file with
    // zeroes upon seek
    m_sot.resize (m_spec.height, (int32_t)0);
    fwrite (&m_sot[0], sizeof(int32_t), m_sot.size (), m_file);
    
    // flip back the endianness of some things we will need again
    if (littleendian ()) {
        swap_endian (&m_rla.NumOfColorChannels);
        swap_endian (&m_rla.NumOfMatteChannels);
        swap_endian (&m_rla.NumOfAuxChannels);
        swap_endian (&m_rla.NumOfChannelBits);
        swap_endian (&m_rla.NumOfMatteBits);
        swap_endian (&m_rla.NumOfAuxBits);
    }
    
    // resize run record buffer to accomodate a worst-case scenario
    // 2 bytes for record length, 1 byte per 1 longest run
    m_buf.resize (2 + (size_t)ceil((1.0 + 1.0 / 128.0)
        * m_spec.scanline_bytes(true)));

    return true;
}



inline void
RLAOutput::set_chromaticity (const ImageIOParameter *p, char *dst,
                             size_t field_size, const char *default_val)
{
    if (p && p->type().basetype == TypeDesc::FLOAT) {
        switch (p->type().aggregate) {
            case TypeDesc::VEC2:
                snprintf (dst, field_size, "%.4f %.4f",
                    ((float *)p->data ())[0], ((float *)p->data ())[1]);
                break;
            case TypeDesc::VEC3:
                snprintf (dst, field_size, "%.4f %.4f %.4f",
                    ((float *)p->data ())[0], ((float *)p->data ())[1],
                        ((float *)p->data ())[2]);
                break;
        }
    } else
        strcpy (dst, default_val);
}



bool
RLAOutput::close ()
{
    if (m_file) {
        // dump the scanline offset table to file
        fseek (m_file, 740, SEEK_SET);
        fwrite (&m_sot[0], sizeof(int32_t), m_sot.size (), m_file);

        // close the stream
        fclose (m_file);
        m_file = NULL;
    }
    
    m_buf.clear ();

    init ();      // re-initialize
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



inline void
RLAOutput::flush_run (int& rawcount, int& rlecount,
                      std::vector<unsigned char>::iterator& it)
{
    if (rawcount > 0) {
        // take advantage of two's complement arithmetic
        *(it - rawcount - 1) = ~((unsigned char)rawcount) + 1;
        rawcount = 0;
    } else if (rlecount > 0) {
        *(it - 2) = (unsigned char)(rlecount - 1);
        rlecount = 0;
    }
}



inline bool
RLAOutput::flush_record (int& rawcount, int& rlecount, unsigned short& length,
                         std::vector<unsigned char>::iterator& it)
{
    flush_run (rawcount, rlecount, it);
    it = m_buf.begin ();
    if (littleendian ()) {
        *it++ = ((unsigned char *)&length)[1];
        *it++ = ((unsigned char *)&length)[0];
    } else {
        *it++ = ((unsigned char *)&length)[0];
        *it++ = ((unsigned char *)&length)[1];
    }
    length += 2;        // account for the record length
    if (fwrite (&m_buf[0], 1, length, m_file) != length)
        return false;
    // reserve 2 bytes for the record length
    it = m_buf.begin () + 2;
    length = 0;
    return true;
}



bool
RLAOutput::encode_plane (const unsigned char *data, stride_t xstride,
                         int chsize, bool is_float)
{
    unsigned short length;
    if (is_float) {
        // fast path for floats - just dump the plane into the file
        float f;            
        length = m_spec.width * sizeof(float);
        if (littleendian ())
            swap_endian (&length);
        fwrite (&length, sizeof (length), 1, m_file);
        for (int x = 0; x < m_spec.width; ++x) {
            f = *((float *)(data + x * xstride));
            if (bigendian ())
                swap_endian (&length);
            fwrite (&f, sizeof (float), 1, m_file);
        }
        return true;
    }
    // integer values - RLE
    unsigned char first = 0;
    const unsigned char *d = 0;
    int rawcount = 0, rlecount = 0;
    length = 0;
    // keeps track of the record buffer position
    // reserve 2 bytes for the record length
    std::vector<unsigned char>::iterator it = m_buf.begin () + 2;
    for (int i = 0; i < chsize; ++i) {
        // ensure correct byte order
        if (littleendian ())
            i = chsize - i - 1;
        for (int x = 0; x < m_spec.width; ++x) {
            ASSERT (it < m_buf.end ());
            d = data + x * xstride + i;
            bool contig = first == *d;
            
            // if the record has ended, flush it run and start anew
            if (length == (1 << (sizeof (length) * 8)) - 1
                && !flush_record (rawcount, rlecount, length, it))
                return false;
            
            if (rawcount == 0 && rlecount == 0) {
                // start of record
                ++it;   // run length placeholder
                *it++ = first = *d;
                rlecount = 1;
                length += 2;
            } else if (rawcount > 0) {
                // raw packet
                if (rawcount == 128) {
                    // max run length reached, flush
                    flush_run (rawcount, rlecount, it);
                    // start a new RLE run
                    ++it;       // run length placeholder
                    *it++ = first = *d;
                    rlecount = 1;
                    length += 2;
                } else {
                    ++rawcount;
                    if (contig) {
                        if (rlecount == 0)
                            rlecount = 2; // we wouldn't have noticed the 1st one
                        else
                            ++rlecount;
                        // we have 3 contiguous bytes, flush and start RLE
                        if (rlecount >= 3) {
                            // rewind the iterator and the counters
                            it -= rlecount - 1;
                            rawcount -= rlecount;
                            length -= rlecount - 2; // will be incremented below
                            // flush the raw run
                            flush_run (rawcount, rlecount, it);
                            // start the RLE run
                            // will be incremented below
                            ++it;
                        }
                    } else
                        // reset contiguity counter
                        rlecount = 0;
                    // also reset the comparison base
                    *it++ = first = *d;
                    ++length;
                }
            } else {
                // RLE packet
                if (rlecount == 128 || (!contig && rlecount >= 3)) {
                    // run ended, flush
                    flush_run (rawcount, rlecount, it);
                    // start a new RLE run
                    ++it;       // run length placeholder
                    *it++ = first = *d;
                    rlecount = 1;
                    length += 2;
                } else if (contig)
                    // another same byte
                    ++rlecount;
                else {
                    // not contiguous, turn the remainder into a raw run
                    for (int j = 1; j < rlecount; ++j, ++length)
                        *it++ = first;
                    rawcount = rlecount + 1;
                    rlecount = 0;
                    // also reset the comparison base
                    *it++ = first = *d;
                    ++length;
                }
            }
        }
        // if there's anything left in the run, flush it
        flush_run (rawcount, rlecount, it);
        // restore index for proper loop functioning
        if (littleendian ())
            i = chsize - i - 1;
    }
    // if there's anything left in the buffer, flush it
    if (length > 0)
        return flush_record (rawcount, rlecount, length, it);
    return true;
}



bool
RLAOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    
    // store the offset to the scanline
    m_sot[m_spec.height - y - 1] = (int32_t)ftell (m_file);
    if (littleendian ())
        swap_endian (&m_sot[m_spec.height - y - 1]);
    
    bool allsame = !m_spec.channelformats.size ();
    bool is_float = (allsame ? m_spec.format : m_spec.channelformats[0])
        == TypeDesc::FLOAT;        
    int offset = 0;
    // colour channels
    int chsize = allsame ? m_spec.format.size ()
                         : m_spec.channelformats[0].size ();
    for (int i = 0; i < m_rla.NumOfColorChannels; ++i, offset += chsize) {
        if (!encode_plane ((unsigned char *)data + offset, xstride, chsize,
            is_float))
            return false;
    }
    // alpha (matte) channels
    is_float = (allsame ? m_spec.format
                        : m_spec.channelformats[m_rla.NumOfColorChannels])
        == TypeDesc::FLOAT;
    chsize = allsame ? m_spec.format.size ()
                     : m_spec.channelformats[m_rla.NumOfColorChannels].size ();
    for (int i = 0; i < m_rla.NumOfMatteChannels; ++i, offset += chsize) {
        if (!encode_plane ((unsigned char *)data + offset, xstride, chsize,
            is_float))
            return false;
    }
    // aux (depth) channels
    is_float = (allsame ? m_spec.format
                        : m_spec.channelformats[m_rla.NumOfColorChannels
                            + m_rla.NumOfMatteChannels])
        == TypeDesc::FLOAT;
    chsize = allsame ? m_spec.format.size ()
                     : m_spec.channelformats[m_rla.NumOfColorChannels
                         + m_rla.NumOfMatteChannels].size ();
    for (int i = 0; i < m_rla.NumOfAuxChannels; ++i, offset += chsize) {
        if (!encode_plane ((unsigned char *)data + offset, xstride, chsize,
            is_float))
            return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

