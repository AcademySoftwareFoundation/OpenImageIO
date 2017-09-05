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

#include "libdpx/DPX.h"
#include "libdpx/DPXColorConverter.h"
#include <OpenEXR/ImfTimeCode.h> //For TimeCode support

#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <iomanip>

OIIO_PLUGIN_NAMESPACE_BEGIN


class DPXInput final : public ImageInput {
public:
    DPXInput () : m_stream(NULL), m_dataPtr(NULL) { init(); }
    virtual ~DPXInput () { close(); }
    virtual const char * format_name (void) const { return "dpx"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    int m_subimage;
    InStream *m_stream;
    dpx::Reader m_dpx;
    std::vector<unsigned char> m_userBuf;
    bool m_wantRaw;
    unsigned char *m_dataPtr;

    /// Reset everything to initial state
    ///
    void init () {
        if (m_stream) {
            m_stream->Close ();
            delete m_stream;
            m_stream = NULL;
        }
        delete m_dataPtr;
        m_dataPtr = NULL;
        m_userBuf.clear ();
    }

    /// Helper function - retrieve string for libdpx characteristic
    ///
    std::string get_characteristic_string (dpx::Characteristic c);

    /// Helper function - retrieve string for libdpx descriptor
    ///
    std::string get_descriptor_string (dpx::Descriptor c);

    /// Helper function - fill int array with KeyCode values
    ///
    void get_keycode_values (int *array);

    /// Helper function - convert Imf::TimeCode to string;
    ///
    std::string get_timecode_string (Imf::TimeCode &tc);
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput *dpx_input_imageio_create () { return new DPXInput; }

OIIO_EXPORT int dpx_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char* dpx_imageio_library_version () { return NULL; }

OIIO_EXPORT const char * dpx_input_extensions[] = {
    "dpx", NULL
};

OIIO_PLUGIN_EXPORTS_END



bool
DPXInput::valid_file (const std::string &filename) const
{
    InStream *stream = new InStream();
    if (! stream)
        return false;
    bool ok = false;
    if (stream->Open(filename.c_str())) {
        dpx::Reader dpx;
        dpx.SetInStream(stream);
        ok = dpx.ReadHeader();
        stream->Close();
    }
    delete stream;
    return ok;
}



bool
DPXInput::open (const std::string &name, ImageSpec &newspec)
{
    // open the image
    m_stream = new InStream();
    if (! m_stream->Open(name.c_str())) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }
    
    m_dpx.SetInStream(m_stream);
    if (! m_dpx.ReadHeader()) {
        error ("Could not read header");
        return false;
    }

    bool ok = seek_subimage (0, 0, newspec);
    newspec = spec ();
    return ok;
}



bool
DPXInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0)
        return false;
    if (subimage < 0 || subimage >= m_dpx.header.ImageElementCount ())
        return false;

    m_subimage = subimage;

    // check if the client asked us for raw data
    m_wantRaw = newspec.get_int_attribute ("dpx:RawData", 0) != 0;

    // create imagespec
    TypeDesc typedesc;
    switch (m_dpx.header.ComponentDataSize(subimage)) {
        case dpx::kByte:
            typedesc = m_dpx.header.DataSign (subimage)
                ? TypeDesc::INT8 : TypeDesc::UINT8;
            break;
        case dpx::kWord:
            typedesc = m_dpx.header.DataSign (subimage)
                ? TypeDesc::INT16 : TypeDesc::UINT16;
            break;
        case dpx::kInt:
            typedesc = m_dpx.header.DataSign (subimage)
                ? TypeDesc::INT32 : TypeDesc::UINT32;
            break;
        case dpx::kFloat:
            typedesc = TypeDesc::FLOAT;
            break;
        case dpx::kDouble:
            typedesc = TypeDesc::DOUBLE;
            break;
        default:
            error ("Invalid component data size");
            return false;
    }
    m_spec = ImageSpec (m_dpx.header.Width(), m_dpx.header.Height(),
        m_dpx.header.ImageElementComponentCount(subimage), typedesc);

    // xOffset/yOffset are defined as unsigned 32-bit integers, but m_spec.x/y are signed
    // avoid casts that would result in negative values
    if (m_dpx.header.xOffset <= (unsigned int)std::numeric_limits<int>::max())
        m_spec.x = m_dpx.header.xOffset;
    if (m_dpx.header.yOffset <= (unsigned int)std::numeric_limits<int>::max())
        m_spec.y = m_dpx.header.yOffset;
    if ((int)m_dpx.header.xOriginalSize > 0)
        m_spec.full_width = m_dpx.header.xOriginalSize;
    if ((int)m_dpx.header.yOriginalSize > 0)
        m_spec.full_height = m_dpx.header.yOriginalSize;

    // fill channel names
    m_spec.channelnames.clear ();
    switch (m_dpx.header.ImageDescriptor(subimage)) {
        /*case dpx::kUserDefinedDescriptor:
            break;*/
        case dpx::kRed:
            m_spec.channelnames.emplace_back("R");
            break;
        case dpx::kGreen:
            m_spec.channelnames.emplace_back("G");
            break;
        case dpx::kBlue:
            m_spec.channelnames.emplace_back("B");
            break;
        case dpx::kAlpha:
            m_spec.channelnames.emplace_back("A");
            m_spec.alpha_channel = 0;
            break;
        case dpx::kLuma:
            // FIXME: do we treat this as intensity or do we use Y' as per
            // convention to differentiate it from linear luminance?
            m_spec.channelnames.emplace_back("Y'");
            break;
        case dpx::kDepth:
            m_spec.channelnames.emplace_back("Z");
            m_spec.z_channel = 0;
            break;
        /*case dpx::kCompositeVideo:
            break;*/
        case dpx::kRGB:
        case dpx::kRGBA:
        case dpx::kABGR:    // colour converter will swap the bytes for us
            m_spec.default_channel_names ();
            break;
        case dpx::kCbYCrY:
            if (m_wantRaw) {
                m_spec.channelnames.emplace_back("CbCr");
                m_spec.channelnames.emplace_back("Y");
            } else {
                m_spec.nchannels = 3;
                m_spec.default_channel_names ();
            }
            break;
        case dpx::kCbYACrYA:
            if (m_wantRaw) {
                m_spec.channelnames.emplace_back("CbCr");
                m_spec.channelnames.emplace_back("Y");
                m_spec.channelnames.emplace_back("A");
                m_spec.alpha_channel = 2;
            } else {
                m_spec.nchannels = 4;
                m_spec.default_channel_names ();
            }
            break;
        case dpx::kCbYCr:
            if (m_wantRaw) {
                m_spec.channelnames.emplace_back("Cb");
                m_spec.channelnames.emplace_back("Y");
                m_spec.channelnames.emplace_back("Cr");
            } else
                m_spec.default_channel_names ();
            break;
        case dpx::kCbYCrA:
            if (m_wantRaw) {
                m_spec.channelnames.emplace_back("Cb");
                m_spec.channelnames.emplace_back("Y");
                m_spec.channelnames.emplace_back("Cr");
                m_spec.channelnames.emplace_back("A");
                m_spec.alpha_channel = 3;
            } else {
                m_spec.default_channel_names ();
            }
            break;
        default:
            {
                for (int i = 0;
                    i < m_dpx.header.ImageElementComponentCount(subimage); i++) {
                    std::string ch = Strutil::format("channel%d", i);
                    m_spec.channelnames.push_back(ch);
                }
            }
    }
    // bits per pixel
    m_spec.attribute ("oiio:BitsPerSample", m_dpx.header.BitDepth(subimage));
    // image orientation - see appendix B.2 of the OIIO documentation
    int orientation;
    switch (m_dpx.header.ImageOrientation ()) {
        case dpx::kLeftToRightTopToBottom:
            orientation = 1;
            break;
        case dpx::kRightToLeftTopToBottom:
            orientation = 2;
            break;
        case dpx::kLeftToRightBottomToTop:
            orientation = 4;
            break;
        case dpx::kRightToLeftBottomToTop:
            orientation = 3;
            break;
        case dpx::kTopToBottomLeftToRight:
            orientation = 5;
            break;
        case dpx::kTopToBottomRightToLeft:
            orientation = 6;
            break;
        case dpx::kBottomToTopLeftToRight:
            orientation = 8;
            break;
        case dpx::kBottomToTopRightToLeft:
            orientation = 7;
            break;
        default:
            orientation = 0;
            break;
    }
    m_spec.attribute ("Orientation", orientation);

    // image linearity
    switch (m_dpx.header.Transfer (subimage)) {
        case dpx::kLinear:
            m_spec.attribute ("oiio:ColorSpace", "Linear");
            break;
        case dpx::kLogarithmic:
            m_spec.attribute ("oiio:ColorSpace", "KodakLog");
            break;
        case dpx::kITUR709:
            m_spec.attribute ("oiio:ColorSpace", "Rec709");
            break;
        case dpx::kUserDefined:
            if (! isnan (m_dpx.header.Gamma ()) && m_dpx.header.Gamma () != 0) {
                float g = float(m_dpx.header.Gamma());
                m_spec.attribute ("oiio:ColorSpace",
                                  Strutil::format("GammaCorrected%.2g", g));
                m_spec.attribute ("oiio:Gamma", g);
                break;
            }
            // intentional fall-through
        /*case dpx::kPrintingDensity:
        case dpx::kUnspecifiedVideo:
        case dpx::kSMPTE274M:
        case dpx::kITUR601:
        case dpx::kITUR602:
        case dpx::kNTSCCompositeVideo:
        case dpx::kPALCompositeVideo:
        case dpx::kZLinear:
        case dpx::kZHomogeneous:
        case dpx::kUndefinedCharacteristic:*/
        default:
            break;
    }
    m_spec.attribute ("dpx:Transfer",
        get_characteristic_string (m_dpx.header.Transfer (subimage)));
    // colorimetric characteristic
    m_spec.attribute ("dpx:Colorimetric",
        get_characteristic_string (m_dpx.header.Colorimetric (subimage)));

    // general metadata
    // some non-compliant writers will dump a field filled with 0xFF rather
    // than a NULL string termination on the first character, so take that
    // into account, too
    if (m_dpx.header.copyright[0] && m_dpx.header.copyright[0] != (char)0xFF)
        m_spec.attribute ("Copyright", m_dpx.header.copyright);
    if (m_dpx.header.creator[0] && m_dpx.header.creator[0] != (char)0xFF)
        m_spec.attribute ("Software", m_dpx.header.creator);
    if (m_dpx.header.project[0] && m_dpx.header.project[0] != (char)0xFF)
        m_spec.attribute ("DocumentName", m_dpx.header.project);
    if (m_dpx.header.creationTimeDate[0]) {
        // libdpx's date/time format is pretty close to OIIO's (libdpx uses
        // %Y:%m:%d:%H:%M:%S%Z)
        char date[24];
        Strutil::safe_strcpy(date, m_dpx.header.creationTimeDate, sizeof(date));
        date[10] = ' ';
        date[19] = 0;
        m_spec.attribute ("DateTime", date);
    }
    if (m_dpx.header.ImageEncoding (subimage) == dpx::kRLE)
        m_spec.attribute ("compression", "rle");
    char buf[32 + 1];
    m_dpx.header.Description (subimage, buf);
    if (buf[0] && buf[0] != char(-1))
        m_spec.attribute ("ImageDescription", buf);
    m_spec.attribute ("PixelAspectRatio",
        m_dpx.header.AspectRatio(1) ? (m_dpx.header.AspectRatio(0) /
                (float)m_dpx.header.AspectRatio(1)) : 1.0f);

    // DPX-specific metadata
    m_spec.attribute ("dpx:ImageDescriptor",
        get_descriptor_string (m_dpx.header.ImageDescriptor (subimage)));
    // save some typing by using macros
    // "internal" macros
#define DPX_SET_ATTRIB_S(x, n, s)   m_spec.attribute (s,                      \
                                        m_dpx.header.x (n))
#define DPX_SET_ATTRIB(x, n)        DPX_SET_ATTRIB_S(x, n, "dpx:" #x)
    // set without checking for bogus attributes
#define DPX_SET_ATTRIB_N(x)         DPX_SET_ATTRIB(x, subimage)
    // set with checking for bogus attributes
#define DPX_SET_ATTRIB_BYTE(x)      if (m_dpx.header.x () != 0xFF)      \
                                        DPX_SET_ATTRIB(x, )
#define DPX_SET_ATTRIB_INT_N(x)     if (m_dpx.header.x (subimage) != 0xFFFFFFFF) \
                                        DPX_SET_ATTRIB(x, subimage)
#define DPX_SET_ATTRIB_INT(x)       if (m_dpx.header.x () != 0xFFFFFFFF)      \
                                        DPX_SET_ATTRIB(x, )
#define DPX_SET_ATTRIB_FLOAT_N(x)   if (! isnan(m_dpx.header.x (subimage)))      \
                                        DPX_SET_ATTRIB(x, subimage)
#define DPX_SET_ATTRIB_FLOAT(x)     if (! isnan(m_dpx.header.x ()))           \
                                        DPX_SET_ATTRIB(x, )
    // see comment above Copyright, Software and DocumentName
#define DPX_SET_ATTRIB_STR(X, x)    if (m_dpx.header.x[0]                     \
                                        && m_dpx.header.x[0] != char(-1))     \
                                        m_spec.attribute ("dpx:" #X,          \
                                            m_dpx.header.x)

    DPX_SET_ATTRIB_INT(EncryptKey);
    DPX_SET_ATTRIB_INT(DittoKey);
    DPX_SET_ATTRIB_INT_N(LowData);
    DPX_SET_ATTRIB_FLOAT_N(LowQuantity);
    DPX_SET_ATTRIB_INT_N(HighData);
    DPX_SET_ATTRIB_FLOAT_N(HighQuantity);
    DPX_SET_ATTRIB_INT_N(EndOfLinePadding);
    DPX_SET_ATTRIB_INT_N(EndOfImagePadding);
    DPX_SET_ATTRIB_FLOAT(XScannedSize);
    DPX_SET_ATTRIB_FLOAT(YScannedSize);
    DPX_SET_ATTRIB_INT(FramePosition);
    DPX_SET_ATTRIB_INT(SequenceLength);
    DPX_SET_ATTRIB_INT(HeldCount);
    DPX_SET_ATTRIB_FLOAT(FrameRate);
    DPX_SET_ATTRIB_FLOAT(ShutterAngle);
    DPX_SET_ATTRIB_STR(Version, version);
    DPX_SET_ATTRIB_STR(Format, format);
    DPX_SET_ATTRIB_STR(FrameId, frameId);
    DPX_SET_ATTRIB_STR(SlateInfo, slateInfo);
    DPX_SET_ATTRIB_STR(SourceImageFileName, sourceImageFileName);
    DPX_SET_ATTRIB_STR(InputDevice, inputDevice);
    DPX_SET_ATTRIB_STR(InputDeviceSerialNumber, inputDeviceSerialNumber);
    DPX_SET_ATTRIB_BYTE(Interlace);
    DPX_SET_ATTRIB_BYTE(FieldNumber);
    DPX_SET_ATTRIB_FLOAT(HorizontalSampleRate);
    DPX_SET_ATTRIB_FLOAT(VerticalSampleRate);
    DPX_SET_ATTRIB_FLOAT(TemporalFrameRate);
    DPX_SET_ATTRIB_FLOAT(TimeOffset);
    DPX_SET_ATTRIB_FLOAT(BlackLevel);
    DPX_SET_ATTRIB_FLOAT(BlackGain);
    DPX_SET_ATTRIB_FLOAT(BreakPoint);
    DPX_SET_ATTRIB_FLOAT(WhiteLevel);
    DPX_SET_ATTRIB_FLOAT(IntegrationTimes);

#undef DPX_SET_ATTRIB_STR
#undef DPX_SET_ATTRIB_FLOAT
#undef DPX_SET_ATTRIB_FLOAT_N
#undef DPX_SET_ATTRIB_INT
#undef DPX_SET_ATTRIB_INT_N
#undef DPX_SET_ATTRIB_N
#undef DPX_SET_ATTRIB
#undef DPX_SET_ATTRIB_S

    std::string tmpstr;
    switch (m_dpx.header.ImagePacking (subimage)) {
        case dpx::kPacked:
            tmpstr = "Packed";
            break;
        case dpx::kFilledMethodA:
            tmpstr = "Filled, method A";
            break;
        case dpx::kFilledMethodB:
            tmpstr = "Filled, method B";
            break;
    }
    if (!tmpstr.empty ())
        m_spec.attribute ("dpx:Packing", tmpstr);

    if (m_dpx.header.filmManufacturingIdCode[0] != 0) {

        int kc[7];
        get_keycode_values (kc);
        m_spec.attribute("smpte:KeyCode", TypeKeyCode, kc);
    }

    if (m_dpx.header.timeCode != 0xFFFFFFFF) {

        unsigned int timecode[2] = {m_dpx.header.timeCode, m_dpx.header.userBits};
        m_spec.attribute("smpte:TimeCode", TypeTimeCode, timecode);

        // This attribute is dpx specific and is left in for backwards compatability.
        // Users should utilise the new smpte:TimeCode attribute instead
        Imf::TimeCode tc(m_dpx.header.timeCode, m_dpx.header.userBits);
        m_spec.attribute ("dpx:TimeCode", get_timecode_string(tc));
    }

    // This attribute is dpx specific and is left in for backwards compatability.
    // Users should utilise the new smpte:TimeCode attribute instead
    if (m_dpx.header.userBits != 0xFFFFFFFF)
        m_spec.attribute ("dpx:UserBits", m_dpx.header.userBits);

    if (m_dpx.header.sourceTimeDate[0]) {
        // libdpx's date/time format is pretty close to OIIO's (libdpx uses
        // %Y:%m:%d:%H:%M:%S%Z)
        char date[24];
        Strutil::safe_strcpy(date, m_dpx.header.sourceTimeDate, sizeof(date));
        date[10] = ' ';
        date[19] = 0;
        m_spec.attribute ("dpx:SourceDateTime", date);
    }
    m_dpx.header.FilmEdgeCode(buf);
    if (buf[0])
        m_spec.attribute ("dpx:FilmEdgeCode", buf);

    tmpstr.clear ();
    switch (m_dpx.header.Signal ()) {
        case dpx::kUndefined:
            tmpstr = "Undefined";
            break;
        case dpx::kNTSC:
            tmpstr = "NTSC";
            break;
        case dpx::kPAL:
            tmpstr = "PAL";
            break;
        case dpx::kPAL_M:
            tmpstr = "PAL-M";
            break;
        case dpx::kSECAM:
            tmpstr = "SECAM";
            break;
        case dpx::k525LineInterlace43AR:
            tmpstr = "YCbCr ITU-R 601-5 525i, 4:3";
            break;
        case dpx::k625LineInterlace43AR:
            tmpstr = "YCbCr ITU-R 601-5 625i, 4:3";
            break;
        case dpx::k525LineInterlace169AR:
            tmpstr = "YCbCr ITU-R 601-5 525i, 16:9";
            break;
        case dpx::k625LineInterlace169AR:
            tmpstr = "YCbCr ITU-R 601-5 625i, 16:9";
            break;
        case dpx::k1050LineInterlace169AR:
            tmpstr = "YCbCr 1050i, 16:9";
            break;
        case dpx::k1125LineInterlace169AR_274:
            tmpstr = "YCbCr 1125i, 16:9 (SMPTE 274M)";
            break;
        case dpx::k1250LineInterlace169AR:
            tmpstr = "YCbCr 1250i, 16:9";
            break;
        case dpx::k1125LineInterlace169AR_240:
            tmpstr = "YCbCr 1125i, 16:9 (SMPTE 240M)";
            break;
        case dpx::k525LineProgressive169AR:
            tmpstr = "YCbCr 525p, 16:9";
            break;
        case dpx::k625LineProgressive169AR:
            tmpstr = "YCbCr 625p, 16:9";
            break;
        case dpx::k750LineProgressive169AR:
            tmpstr = "YCbCr 750p, 16:9 (SMPTE 296M)";
            break;
        case dpx::k1125LineProgressive169AR:
            tmpstr = "YCbCr 1125p, 16:9 (SMPTE 274M)";
            break;
        case dpx::k255:
            // don't set the attribute at all
            break;
        default:
            tmpstr = Strutil::format ("Undefined %d",
                (int)m_dpx.header.Signal ());
            break;
    }
    if (!tmpstr.empty ())
        m_spec.attribute ("dpx:Signal", tmpstr);

    // read in user data; don't bother if the buffer is already filled (user
    // data is per-file, not per-element)
    if (m_userBuf.empty () && m_dpx.header.UserSize () != 0
        && m_dpx.header.UserSize () != 0xFFFFFFFF) {
        m_userBuf.resize (m_dpx.header.UserSize ());
        m_dpx.ReadUserData (&m_userBuf[0]);
    }
    if (!m_userBuf.empty ())
        m_spec.attribute ("dpx:UserData", TypeDesc (TypeDesc::UCHAR,
            m_dpx.header.UserSize ()), &m_userBuf[0]);

    dpx::Block block(0, 0, m_dpx.header.Width () - 1, 0);
    int bufsize = dpx::QueryRGBBufferSize (m_dpx.header, subimage, block);
    if (bufsize == 0 && !m_wantRaw) {
        error ("Unable to deliver RGB data from source data");
        return false;
    } else if (!m_wantRaw && bufsize > 0)
        m_dataPtr = new unsigned char[bufsize];
    else
        // no need to allocate another buffer
        m_dataPtr = NULL;
    
    newspec = m_spec;
    return true;
}



bool
DPXInput::close ()
{
    init();  // Reset to initial state
    return true;
}



bool
DPXInput::read_native_scanline (int y, int z, void *data)
{
    dpx::Block block(0, y-m_spec.y, m_dpx.header.Width () - 1, y-m_spec.y);

    if (m_wantRaw) {
        // fast path - just read the scanline in
        if (!m_dpx.ReadBlock (m_subimage, (unsigned char *)data, block))
            return false;
    } else {
        // read the scanline and convert to RGB
        void *ptr = m_dataPtr == NULL ? data : (void *)m_dataPtr;

        if (!m_dpx.ReadBlock (m_subimage, (unsigned char *)ptr, block))
            return false;

        if (!dpx::ConvertToRGB (m_dpx.header, m_subimage, ptr, data, block))
            return false;
    }
    
    return true;
}



std::string
DPXInput::get_characteristic_string (dpx::Characteristic c)
{
    switch (c) {
        case dpx::kUserDefined:
            return "User defined";
        case dpx::kPrintingDensity:
            return "Printing density";
        case dpx::kLinear:
            return "Linear";
        case dpx::kLogarithmic:
            return "Logarithmic";
        case dpx::kUnspecifiedVideo:
            return "Unspecified video";
        case dpx::kSMPTE274M:
            return "SMPTE 274M";
        case dpx::kITUR709:
            return "ITU-R 709-4";
        case dpx::kITUR601:
            return "ITU-R 601-5 system B or G";
        case dpx::kITUR602:
            return "ITU-R 601-5 system M";
        case dpx::kNTSCCompositeVideo:
            return "NTSC composite video";
        case dpx::kPALCompositeVideo:
            return "PAL composite video";
        case dpx::kZLinear:
            return "Z depth linear";
        case dpx::kZHomogeneous:
            return "Z depth homogeneous";
        case dpx::kUndefinedCharacteristic:
        default:
            return "Undefined";
    }
}



std::string
DPXInput::get_descriptor_string (dpx::Descriptor c)
{
    switch (c) {
        case dpx::kUserDefinedDescriptor:
        case dpx::kUserDefined2Comp:
        case dpx::kUserDefined3Comp:
        case dpx::kUserDefined4Comp:
        case dpx::kUserDefined5Comp:
        case dpx::kUserDefined6Comp:
        case dpx::kUserDefined7Comp:
        case dpx::kUserDefined8Comp:
            return "User defined";
        case dpx::kRed:
            return "Red";
        case dpx::kGreen:
            return "Green";
        case dpx::kBlue:
            return "Blue";
        case dpx::kAlpha:
            return "Alpha";
        case dpx::kLuma:
            return "Luma";
        case dpx::kColorDifference:
            return "Color difference";
        case dpx::kDepth:
            return "Depth";
        case dpx::kCompositeVideo:
            return "Composite video";
        case dpx::kRGB:
            return "RGB";
        case dpx::kRGBA:
            return "RGBA";
        case dpx::kABGR:
            return "ABGR";
        case dpx::kCbYCrY:
            return "CbYCrY";
        case dpx::kCbYACrYA:
            return "CbYACrYA";
        case dpx::kCbYCr:
            return "CbYCr";
        case dpx::kCbYCrA:
            return "CbYCrA";
        //case dpx::kUndefinedDescriptor:
        default:
            return "Undefined";
    }
}



void
DPXInput::get_keycode_values (int *array)
{
    std::stringstream ss;

    // Manufacturer code
    ss << std::string(m_dpx.header.filmManufacturingIdCode, 2);
    ss >> array[0];
    ss.clear(); ss.str("");

    // Film type
    ss << std::string(m_dpx.header.filmType, 2);
    ss >> array[1];
    ss.clear(); ss.str("");

    // Prefix
    ss << std::string(m_dpx.header.prefix, 6);
    ss >> array[2];
    ss.clear(); ss.str("");

    // Count
    ss << std::string(m_dpx.header.count, 4);
    ss >> array[3];
    ss.clear(); ss.str("");

    // Perforation Offset
    ss << std::string(m_dpx.header.perfsOffset, 2);
    ss >> array[4];
    ss.clear(); ss.str("");

    // Format
    std::string format(m_dpx.header.format, 32);
    int &perfsPerFrame = array[5];
    int &perfsPerCount = array[6];

    // default values
    perfsPerFrame = 4;
    perfsPerCount = 64;

    if ( format == "8kimax" ) {
        perfsPerFrame = 15;
        perfsPerCount = 120;
    }
    else if ( format.substr(0,4) == "2kvv" || format.substr(0,4) == "4kvv" ) {
        perfsPerFrame = 8;
    }
    else if ( format == "VistaVision" ) {
        perfsPerFrame = 8;
    }
    else if ( format.substr(0,4) == "2k35" || format.substr(0,4) == "4k35") {
        perfsPerFrame = 4;
    }
    else if ( format == "Full Aperture" ) {
        perfsPerFrame = 4;
    }
    else if ( format == "Academy" ) {
        perfsPerFrame = 4;
    }
    else if ( format.substr(0,7) == "2k3perf" || format.substr(0,7) == "4k3perf" ) {
        perfsPerFrame = 3;
    }
    else if ( format == "3perf" ) {
        perfsPerFrame = 3;
    }
}



std::string
DPXInput::get_timecode_string (Imf::TimeCode &tc)
{
    int values[] = {tc.hours(), tc.minutes(), tc.seconds(), tc.frame()};
    std::stringstream ss;
    for (int i=0; i<4; i++) {
        std::ostringstream padded;
        padded << std::setw(2) << std::setfill('0') << values[i];
        ss << padded.str();
        if (i != 3) {
            if (i == 2) {
                tc.dropFrame() ? ss << ';' : ss << ':';
            }
            else {
                ss << ':';
            }
        }
    }
    return ss.str();
}

OIIO_PLUGIN_NAMESPACE_END

