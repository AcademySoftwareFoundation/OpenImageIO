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

#include "rla_pvt.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace RLA_pvt;


class RLAInput : public ImageInput {
public:
    RLAInput () { init(); }
    virtual ~RLAInput () { close(); }
    virtual const char * format_name (void) const { return "rla"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    WAVEFRONT m_rla;                  ///< Wavefront RLA header
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels

    /// Reset everything to initial state
    ///
    void init () {
        m_file = NULL;
        m_buf.clear ();
    }

    /// Helper function: read the image.
    ///
    bool readimg ();

    /// Helper: read, with error detection
    ///
    bool fread (void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fread (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Read error");
        return n == nitems;
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
    
    m_spec = ImageSpec (std::abs (m_rla.ActiveRight - m_rla.ActiveLeft) + 1,
                        std::abs (m_rla.ActiveBottom - m_rla.ActiveTop) + 1,
                        m_rla.NumOfColorChannels
                            + m_rla.NumOfMatteChannels
                            + m_rla.NumOfAuxChannels,
                        ct == CT_BYTE ? TypeDesc::UINT8
                            : (ct == CT_WORD ? TypeDesc::UINT16
                                : (ct == CT_DWORD ? TypeDesc::UINT32
                                    : TypeDesc::FLOAT)));
    m_spec.attribute ("oiio:BitsPerSample", m_spec.nchannels
                                            * 8 * std::min (32, 1 << ct));
    // make a guess at channel names for the time being
    m_spec.default_channel_names ();
    // this is always true
    m_spec.attribute ("compression", "rle");

    newspec = spec ();
    return true;
}



bool
RLAInput::readimg ()
{
    return false;
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
RLAInput::read_native_scanline (int y, int z, void *data)
{
    if (m_buf.empty ())
        readimg ();

    size_t size = spec().scanline_bytes();
    memcpy (data, &m_buf[0] + y * size, size);
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

