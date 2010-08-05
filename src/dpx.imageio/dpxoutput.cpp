/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include "libdpx/DPX.h"
#include "libdpx/DPXColorConverter.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace OpenImageIO;



class DPXOutput : public ImageOutput {
public:
    DPXOutput ();
    virtual ~DPXOutput ();
    virtual const char * format_name (void) const { return "dpx"; }
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
    OutStream *m_stream;
    dpx::Writer m_dpx;
    std::vector<unsigned char> m_buf;
    std::vector<unsigned char> m_scratch;
    dpx::DataSize m_datasize;
    dpx::Descriptor m_desc;
    dpx::Characteristic m_cmetr;
    bool m_wantRaw;
    unsigned char *m_dataPtr;
    int m_bytes;

    // Initialize private members to pre-opened state
    void init (void) {
        if (m_stream) {
            m_stream->Close ();
            delete m_stream;
            m_stream = NULL;
        }
        delete m_dataPtr;
        m_dataPtr = NULL;
        m_buf.clear ();
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *dpx_output_imageio_create () { return new DPXOutput; }

// DLLEXPORT int dpx_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;   // it's in dpxinput.cpp

DLLEXPORT const char * dpx_output_extensions[] = {
    "dpx", NULL
};

OIIO_PLUGIN_EXPORTS_END



DPXOutput::DPXOutput () : m_stream(NULL), m_dataPtr(NULL)
{
    init ();
}



DPXOutput::~DPXOutput ()
{
    // Close, if not already done.
    close ();
}



bool
DPXOutput::open (const std::string &name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    // open the image
    m_stream = new OutStream();
    if (! m_stream->Open(name.c_str ())) {
        error ("Could not open file \"%s\"", name.c_str ());
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
        error ("DPX does not support volume images (depth > 1)");
        return false;
    }

    if (m_spec.format == TypeDesc::UINT8
        || m_spec.format == TypeDesc::INT8)
        m_datasize = dpx::kByte;
    else if (m_spec.format == TypeDesc::UINT16
        || m_spec.format == TypeDesc::INT16)
        m_datasize = dpx::kWord;
    else if (m_spec.format == TypeDesc::FLOAT)
        m_datasize = dpx::kFloat;
    else if (m_spec.format == TypeDesc::DOUBLE)
        m_datasize = dpx::kDouble;
    else {
        error ("DPX does not support data of this format");
        return false;
    }

    // check if the client is giving us raw data to write
    m_wantRaw = m_spec.get_int_attribute ("dpx:RawData", 0) != 0;

    m_dpx.SetOutStream (m_stream);

    // start out the file
    m_dpx.Start ();

    // some metadata
    std::string project = m_spec.get_string_attribute ("DocumentName", "");
    std::string copyright = m_spec.get_string_attribute ("Copyright", "");
    m_dpx.SetFileInfo (name.c_str (),                       // filename
        NULL,                                               // TODO: cr. date
        OPENIMAGEIO_INTRO_STRING,                           // creator
        project.length () > 0 ? project.c_str () : NULL,    // project
        copyright.length () > 0 ? copyright.c_str () : NULL); // copyright

    // image info
    m_dpx.SetImageInfo (m_spec.width, m_spec.height);

    // determine descriptor
    dpx::Descriptor desc;
    switch (m_spec.nchannels) {
        case 1:
            if (m_spec.alpha_channel == 0)
                desc = dpx::kAlpha;
            else
                desc = dpx::kLuma;
            break;
        case 2:
            desc = dpx::kUserDefined2Comp;
            break;
        case 3:
            desc = dpx::kRGB;
            break;
        case 4:
            desc = dpx::kRGBA;
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            desc = dpx::Descriptor((int)dpx::kUserDefined5Comp + m_spec.nchannels - 5);
            break;
        default:
            desc = dpx::kUndefinedDescriptor;
            break;
    }

    // transfer function
    dpx::Characteristic transfer;
    switch (m_spec.linearity) {
        case ImageSpec::Linear:
            transfer = dpx::kLinear;
            break;
        case ImageSpec::GammaCorrected:
            transfer = dpx::kUserDefined;
            break;
        case ImageSpec::sRGB: // HACK: sRGB is close to Rec709
        case ImageSpec::Rec709:
            transfer = dpx::kITUR709;
            break;
        case ImageSpec::KodakLog:
            transfer = dpx::kLogarithmic;
        default:
            transfer = dpx::kUserDefined;
            break;
    }

    dpx::Characteristic cmetr = dpx::kUserDefined;

    // FIXME: clean up after the merger of diffs
    m_desc = desc;
    m_cmetr = cmetr;
    
    // see if we'll need to convert or not
    m_bytes = dpx::QueryNativeBufferSize (desc, m_datasize, m_spec.width, 1);
    if (m_bytes == 0 && !m_wantRaw) {
        error ("Unable to deliver native format data from source data");
        return false;
    } else if (!m_wantRaw && m_bytes > 0)
        m_dataPtr = new unsigned char[m_spec.scanline_bytes ()];
    else {
        // no need to allocate another buffer
        m_dataPtr = NULL;
        if (!m_wantRaw)
            m_bytes = m_spec.scanline_bytes ();
    }

    if (m_bytes < 0)
        m_bytes = -m_bytes;

    m_dpx.SetElement (0, desc, m_spec.format.size () * 8, transfer, cmetr,
        dpx::kFilledMethodA, dpx::kNone, (m_spec.format == TypeDesc::INT8
            || m_spec.format == TypeDesc::INT16) ? 1 : 0);

    // commit!
    if (!m_dpx.WriteHeader ()) {
        error ("Failed to write DPX header");
        return false;
    }

    // user data
    ImageIOParameter *user = m_spec.find_attribute ("dpx:UserData");
    if (user && user->datasize () > 0) {
        if (user->datasize () > 1024 * 1024) {
            error ("User data block size exceeds 1 MB");
            return false;
        }
        m_dpx.SetUserData (user->datasize ());
        if (!m_dpx.WriteUserData ((void *)user->data ())) {
            error ("Failed to write user data");
            return false;
        }
    }

    // reserve space for the image data buffer
    m_buf.reserve (m_bytes * m_spec.height);

    return true;
}



bool
DPXOutput::close ()
{
    if (m_stream) {
        m_dpx.WriteElement (0, &m_buf[0], m_datasize);
        m_dpx.Finish ();
    }
        
    init();  // Reset to initial state
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



bool
DPXOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    m_spec.auto_stride (xstride, format, m_spec.nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
                          data = &m_scratch[0];
    }

    unsigned char *dst = &m_buf[y * m_bytes];
    if (m_wantRaw)
        // fast path - just dump the scanline into the buffer
        memcpy (dst, data, m_spec.scanline_bytes ());
    else if (!dpx::ConvertToNative (m_desc, m_datasize, m_cmetr,
        m_spec.width, m_spec.height, data, dst))
        return false;
    
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

