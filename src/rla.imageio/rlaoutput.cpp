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
#include <ctime>

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"

#include "rla_pvt.h"

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
    RLAHeader m_rla;                  ///< Wavefront RLA header
    std::vector<uint32_t> m_sot;      ///< Scanline offset table
    std::vector<unsigned char> m_rle; ///< Run record buffer for RLE

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_sot.clear ();
    }
    
    /// Helper - sets a chromaticity from attribute
    inline void set_chromaticity (const ImageIOParameter *p, char *dst,
                                  size_t field_size, const char *default_val);
    
    /// Helper - handles the repetitive work of encoding and writing a channel
    bool encode_channel (const unsigned char *data, stride_t xstride,
                         TypeDesc chantype, int bits);
    
    /// Helper - write, with error detection
    bool fwrite (const void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fwrite (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Write error: wrote %d records of %d", (int)n, (int)nitems);
        return n == nitems;
    }

    /// Helper: write buf[0..nitems-1], swap endianness if necessary
    template<typename T>
    bool write (const T *buf, size_t nitems=1) {
        if (littleendian() &&
            (is_same<T,uint16_t>::value || is_same<T,int16_t>::value ||
             is_same<T,uint32_t>::value || is_same<T,int32_t>::value)) {
            T *newbuf = ALLOCA(T,nitems);
            memcpy (newbuf, buf, nitems*sizeof(T));
            swap_endian (newbuf, nitems);
            buf = newbuf;
        }
        return fwrite (buf, sizeof(T), nitems);
    }

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
    if (feature == "random_access")
        return true;
    if (feature == "displaywindow")
        return true;
    if (feature == "origin")
        return true;
    if (feature == "negativeorigin")
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
        // FIXME -- the RLA format supports subimages, but our writer
        // doesn't.  I'm not sure if it's worth worrying about for an
        // old format that is so rarely used.  We'll come back to it if
        // anybody actually encounters a multi-subimage RLA in the wild.
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
    if (m_spec.width > 65535 || m_spec.height > 65535) {
        error ("Image resolution %d x %d too large for RLA (maxiumum 65535x65535)",
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
        int bits = m_spec.get_int_attribute ("oiio:BitsPerSample", 0);
        m_rla.NumOfChannelBits = bits ? bits : m_spec.channelformats[0].size () * 8;
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
            m_rla.NumOfMatteBits = bits ? bits : m_spec.channelformats[m_rla.NumOfColorChannels].size () * 8;
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
    if (Strutil::iequals(s, "Linear"))
        strcpy (m_rla.Gamma, "1.0");
    else if (Strutil::iequals(s, "GammaCorrected"))
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

#define STRING_FIELD(rlafield,name)                                     \
    {                                                                   \
        std::string s = m_spec.get_string_attribute (name);             \
        if (s.length()) {                                               \
            strncpy (m_rla.rlafield, s.c_str(), sizeof(m_rla.rlafield));\
            m_rla.rlafield[sizeof(m_rla.rlafield)-1] = 0;               \
        } else {                                                        \
            m_rla.rlafield[0] = 0;                                      \
        }                                                               \
    }

    m_rla.JobNumber = m_spec.get_int_attribute ("rla:JobNumber", 0);
    STRING_FIELD (FileName, "rla:FileName");
    STRING_FIELD (Description, "ImageDescription");
    STRING_FIELD (ProgramName, "Software");
    STRING_FIELD (MachineName, "HostComputer");
    STRING_FIELD (UserName, "Artist");
    
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
    STRING_FIELD (Aspect, "rla:Aspect");

    snprintf (m_rla.AspectRatio, sizeof(m_rla.AspectRatio), "%.10f",
        m_spec.get_float_attribute ("PixelAspectRatio", 1.f));
    strcpy (m_rla.ColorChannel, m_spec.get_string_attribute ("rla:ColorChannel",
        "rgb").c_str ());
    m_rla.FieldRendered = m_spec.get_int_attribute ("rla:FieldRendered", 0);

    STRING_FIELD (Time, "rla:Time");
    STRING_FIELD (Filter, "rla:Filter");
    STRING_FIELD (AuxData, "rla:AuxData");

    m_rla.rla_swap_endian ();        // RLAs are big-endian
    write (&m_rla);
    m_rla.rla_swap_endian ();        // flip back the endianness to native
    
    // write placeholder values - not all systems may expand the file with
    // zeroes upon seek
    m_sot.resize (m_spec.height, (int32_t)0);
    write (&m_sot[0], m_sot.size());
    
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
        // Now that all scanlines ahve been output, return to write the
        // correct scanline offset table to file.
        fseek (m_file, sizeof(RLAHeader), SEEK_SET);
        write (&m_sot[0], m_sot.size());

        // close the stream
        fclose (m_file);
        m_file = NULL;
    }
    
    init ();      // re-initialize
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



bool
RLAOutput::encode_channel (const unsigned char *data, stride_t xstride,
                           TypeDesc chantype, int bits)
{
    if (chantype == TypeDesc::FLOAT) {
        // Special case -- float data is just dumped raw, no RLE
        uint16_t size = m_spec.width * sizeof(float);
        write (&size);
        for (int x = 0;  x < m_spec.width;  ++x)
            write ((const float *)&data[x*xstride]);
        return true;
    }

    m_rle.resize (2);   // reserve t bytes for the encoded size

    // multi-byte data types are sliced to MSB, nextSB, ..., LSB
    int chsize = (int)chantype.size();
    for (int byte = 0;  byte < chsize;  ++byte) {
        int lastval = -1;     // last value
        int count = 0;        // count of raw or repeats
        bool repeat = false;  // if true, we're repeating
        int runbegin = 0;     // where did the run begin
        int byteoffset = bigendian() ? byte : (chsize-byte-1);
        for (int x = 0;  x < m_spec.width;  ++x) {
            int newval = data[x*xstride+byteoffset];
            if (count == 0) {   // beginning of a run.
                count = 1;
                repeat = true;  // presumptive
                runbegin = x;
            } else if (repeat) { // We've seen one or more repeating characters
                if (newval == lastval) {
                    // another repeating value
                    ++count;
                } else {
                    // We stopped repeating.
                    if (count < 3) {
                        // If we didn't even have 3 in a row, just 
                        // retroactively treat it as a raw run.
                        ++count;
                        repeat = false;
                    } else {
                        // We are ending a 3+ repetition
                        m_rle.push_back (count-1);
                        m_rle.push_back (lastval);
                        count = 1;
                        runbegin = x;
                    }
                }
            } else {  // Have not been repeating
                if (newval == lastval) {
                    // starting a repetition?  Output previous
                    ASSERT (count > 1);
                    // write everything but the last char
                    --count;
                    m_rle.push_back (-count);
                    for (int i = 0;  i < count;  ++i)
                        m_rle.push_back (data[(runbegin+i)*xstride+byteoffset]);
                    count = 2;
                    runbegin = x - 1;
                    repeat = true;
                } else {
                    ++count;  // another non-repeat
                }
            }

            // If the run is too long or we're at the scanline end, write
            if (count == 127 || x == m_spec.width-1) {
                if (repeat) {
                    m_rle.push_back (count-1); 
                    m_rle.push_back (lastval);
                } else {
                    m_rle.push_back (-count);
                    for (int i = 0;  i < count;  ++i)
                        m_rle.push_back (data[(runbegin+i)*xstride+byteoffset]);
                }
                count = 0;
            }
            lastval = newval;
        }
        ASSERT (count == 0);
    }

    // Now that we know the size of the encoded buffer, save it at the
    // beginning
    uint16_t size = uint16_t (m_rle.size() - 2);
    m_rle[0] = size >> 8;
    m_rle[1] = size & 255;

    // And write the channel to the file
    return write (&m_rle[0], m_rle.size());
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
    
    // store the offset to the scanline.  We'll swap_endian if necessary
    // when we go to actually write it.
    m_sot[m_spec.height - y - 1] = (uint32_t)ftell (m_file);

    size_t pixelsize = m_spec.pixel_bytes (true /*native*/);
    int offset = 0;
    for (int c = 0;  c < m_spec.nchannels;  ++c) {
        TypeDesc chantype = m_spec.channelformats.size() ?
            m_spec.channelformats[c] : m_spec.format;
        int bits = (c < m_rla.NumOfColorChannels) ? m_rla.NumOfChannelBits
            : (c < (m_rla.NumOfColorChannels+m_rla.NumOfMatteBits)) ? m_rla.NumOfMatteBits
            : m_rla.NumOfAuxBits;
        if (!encode_channel ((unsigned char *)data + offset, pixelsize,
                             chantype, bits))
            return false;
        offset += chantype.size();
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

