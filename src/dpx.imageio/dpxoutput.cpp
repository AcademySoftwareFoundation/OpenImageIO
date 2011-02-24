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
#include <boost/algorithm/string.hpp>

#include "libdpx/DPX.h"
#include "libdpx/DPXColorConverter.h"

#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using boost::algorithm::iequals;



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
                       OpenMode mode=Create);
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
    int m_bytes;

    // Initialize private members to pre-opened state
    void init (void) {
        if (m_stream) {
            m_stream->Close ();
            delete m_stream;
            m_stream = NULL;
        }
        m_buf.clear ();
    }

    /// Helper function - retrieve libdpx descriptor for string
    ///
    dpx::Characteristic get_characteristic_from_string (std::string str);

    /// Helper function - retrieve libdpx descriptor for string
    ///
    dpx::Descriptor get_descriptor_from_string (std::string str);
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *dpx_output_imageio_create () { return new DPXOutput; }

// DLLEXPORT int dpx_imageio_version = OIIO_PLUGIN_VERSION;   // it's in dpxinput.cpp

DLLEXPORT const char * dpx_output_extensions[] = {
    "dpx", NULL
};

OIIO_PLUGIN_EXPORTS_END



DPXOutput::DPXOutput () : m_stream(NULL)
{
    init ();
}



DPXOutput::~DPXOutput ()
{
    // Close, if not already done.
    close ();
}



bool
DPXOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    close ();  // Close any already-opened file

    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

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
    else if (m_spec.format == TypeDesc::FLOAT
        || m_spec.format == TypeDesc::HALF) {
        m_spec.format = TypeDesc::FLOAT;
        m_datasize = dpx::kFloat;
    } else if (m_spec.format == TypeDesc::DOUBLE)
        m_datasize = dpx::kDouble;
    else {
        // use 16-bit unsigned integers as a failsafe
        m_spec.format = TypeDesc::UINT16;
        m_datasize = dpx::kWord;
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
        OIIO_INTRO_STRING,                                  // creator
        project.empty () ? NULL : project.c_str (),         // project
        copyright.empty () ? NULL : copyright.c_str ());    // copyright

    // image info
    m_dpx.SetImageInfo (m_spec.width, m_spec.height);

    // determine descriptor
    m_desc = get_descriptor_from_string
        (m_spec.get_string_attribute ("dpx:ImageDescriptor", ""));

    // transfer function
    dpx::Characteristic transfer;
    
    std::string colorspace = m_spec.get_string_attribute ("oiio:ColorSpace", "");
    if (iequals (colorspace, "Linear"))  transfer = dpx::kLinear;
    else if (iequals (colorspace, "GammaCorrected")) transfer = dpx::kUserDefined;
    else if (iequals (colorspace, "Rec709")) transfer = dpx::kITUR709;
    else if (iequals (colorspace, "KodakLog")) transfer = dpx::kLogarithmic;
    else {
        std::string dpxtransfer = m_spec.get_string_attribute ("dpx:Transfer", "");
        transfer = get_characteristic_from_string (dpxtransfer);
    }
    
    // colorimetric
    m_cmetr = get_characteristic_from_string
        (m_spec.get_string_attribute ("dpx:Colorimetric", "User defined"));

    // select packing method
    dpx::Packing packing;
    std::string tmpstr = m_spec.get_string_attribute ("dpx:ImagePacking", "Filled, method A");
    if (iequals (tmpstr, "Packed"))
        packing = dpx::kPacked;
    else if (iequals (tmpstr, "Filled, method B"))
        packing = dpx::kFilledMethodB;
    else
        packing = dpx::kFilledMethodA;

    // calculate target bit depth
    int bitDepth = m_spec.get_int_attribute ("oiio:BitsPerSample",
        m_spec.format.size () * 8);
    if (bitDepth % 8 != 0 && bitDepth != 10 && bitDepth != 12 && bitDepth != 16) {
        error ("Unsupported bit depth %d", bitDepth);
        return false;
    }
    
    // see if we'll need to convert or not
    if (m_desc == dpx::kRGB || m_desc == dpx::kRGBA) {
        // shortcut for RGB(A) that gets the job done
        m_bytes = m_spec.scanline_bytes ();
        m_wantRaw = true;
    } else {
        m_bytes = dpx::QueryNativeBufferSize (m_desc, m_datasize, m_spec.width, 1);
        if (m_bytes == 0 && !m_wantRaw) {
            error ("Unable to deliver native format data from source data");
            return false;
        } else if (m_bytes < 0) {
            // no need to allocate another buffer
            if (!m_wantRaw)
                m_bytes = m_spec.scanline_bytes ();
            else
                m_bytes = -m_bytes;
        }
    }

    if (m_bytes < 0)
        m_bytes = -m_bytes;

    m_dpx.SetElement (0, m_desc, bitDepth, transfer, m_cmetr,
        packing, dpx::kNone, (m_spec.format == TypeDesc::INT8
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
        // FIXME: write the missing libdpx code
        /*m_dpx.SetUserData (user->datasize ());
        if (!m_dpx.WriteUserData ((void *)user->data ())) {
            error ("Failed to write user data");
            return false;
        }*/
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
        m_spec.width, 1, data, dst))
        return false;
    
    return true;
}



dpx::Characteristic
DPXOutput::get_characteristic_from_string (std::string str)
{
    if (iequals (str, "User defined"))
        return dpx::kUserDefined;
    else if (iequals (str, "Printing density"))
        return dpx::kPrintingDensity;
    else if (iequals (str, "Linear"))
        return dpx::kLinear;
    else if (iequals (str, "Logarithmic"))
        return dpx::kLogarithmic;
    else if (iequals (str, "Unspecified video"))
        return dpx::kUnspecifiedVideo;
    else if (iequals (str, "SMPTE 274M"))
        return dpx::kSMPTE274M;
    else if (iequals (str, "ITU-R 709-4"))
        return dpx::kITUR709;
    else if (iequals (str, "ITU-R 601-5 system B or G"))
        return dpx::kITUR601;
    else if (iequals (str, "ITU-R 601-5 system M"))
        return dpx::kITUR602;
    else if (iequals (str, "NTSC composite video"))
        return dpx::kNTSCCompositeVideo;
    else if (iequals (str, "PAL composite video"))
        return dpx::kPALCompositeVideo;
    else if (iequals (str, "Z depth linear"))
        return dpx::kZLinear;
    else if (iequals (str, "Z depth homogeneous"))
        return dpx::kZHomogeneous;
    else
        return dpx::kUndefinedCharacteristic;
}



dpx::Descriptor
DPXOutput::get_descriptor_from_string (std::string str)
{
    if (str.empty ()) {
        // try to guess based on the image spec
        // FIXME: make this more robust (that is, if someone complains)
        switch (m_spec.nchannels) {
            case 1:
                return dpx::kLuma;
            case 3:
                return dpx::kRGB;
            case 4:
                return dpx::kRGBA;
            default:
                if (m_spec.nchannels <= 8)
                    return (dpx::Descriptor)((int)dpx::kUserDefined2Comp
                        + m_spec.nchannels - 2);
                return dpx::kUndefinedDescriptor;
        }
    } else if (iequals (str, "User defined")) {
        if (m_spec.nchannels >= 2 && m_spec.nchannels <= 8)
            return (dpx::Descriptor)((int)dpx::kUserDefined2Comp
                + m_spec.nchannels - 2);
        return dpx::kUserDefinedDescriptor;
    } else if (iequals (str, "Red"))
        return dpx::kRed;
    else if (iequals (str, "Green"))
        return dpx::kGreen;
    else if (iequals (str, "Blue"))
        return dpx::kBlue;
    else if (iequals (str, "Alpha"))
        return dpx::kAlpha;
    else if (iequals (str, "Luma"))
        return dpx::kLuma;
    else if (iequals (str, "Color difference"))
        return dpx::kColorDifference;
    else if (iequals (str, "Depth"))
        return dpx::kDepth;
    else if (iequals (str, "Composite video"))
        return dpx::kCompositeVideo;
    else if (iequals (str, "RGB"))
        return dpx::kRGB;
    else if (iequals (str, "RGBA"))
        return dpx::kRGBA;
    else if (iequals (str, "ABGR"))
        return dpx::kABGR;
    else if (iequals (str, "CbYCrY"))
        return dpx::kCbYCrY;
    else if (iequals (str, "CbYACrYA"))
        return dpx::kCbYACrYA;
    else if (iequals (str, "CbYCr"))
        return dpx::kCbYCr;
    else if (iequals (str, "CbYCrA"))
        return dpx::kCbYCrA;
    //else if (iequals (str, "Undefined"))
        return dpx::kUndefinedDescriptor;
}

OIIO_PLUGIN_NAMESPACE_END

