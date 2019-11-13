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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>

// Note: libdpx originally from: https://github.com/PatrickPalmer/dpx
// But that seems not to be actively maintained.
#include "libdpx/DPX.h"
#include "libdpx/DPXColorConverter.h"

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


static const int MAX_DPX_IMAGE_ELEMENTS = 8;  // max subimages in DPX spec



class DPXOutput final : public ImageOutput {
public:
    DPXOutput();
    virtual ~DPXOutput();
    virtual const char* format_name(void) const override { return "dpx"; }
    virtual int supports(string_view feature) const override
    {
        if (feature == "multiimage" || feature == "alpha"
            || feature == "nchannels" || feature == "random_access"
            || feature == "rewrite" || feature == "displaywindow"
            || feature == "origin")
            return true;
        return false;
    }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual bool open(const std::string& name, int subimages,
                      const ImageSpec* specs) override;
    virtual bool close() override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;

private:
    OutStream* m_stream;
    dpx::Writer m_dpx;
    std::vector<unsigned char> m_buf;
    std::vector<unsigned char> m_scratch;
    dpx::DataSize m_datasize;
    dpx::Descriptor m_desc;
    dpx::Characteristic m_cmetr;
    dpx::Characteristic m_transfer;
    dpx::Packing m_packing;
    int m_bitdepth;
    bool m_rawcolor;
    bool m_wantSwap;
    int64_t m_bytes;
    int m_subimage;
    int m_subimages_to_write;
    std::vector<ImageSpec> m_subimage_specs;
    bool m_write_pending;  // subimage buffer needs to be written
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;

    // Initialize private members to pre-opened state
    void init(void)
    {
        if (m_stream) {
            m_stream->Close();
            delete m_stream;
            m_stream = NULL;
        }
        m_buf.clear();
        m_subimage           = 0;
        m_subimages_to_write = 0;
        m_subimage_specs.clear();
        m_write_pending = false;
    }

    // Is the output file currently opened?
    bool is_opened() const { return (m_stream != NULL); }

    // flush the pending buffer
    bool write_buffer();

    bool prep_subimage(int s, bool allocate);

    /// Helper function - retrieve libdpx descriptor for string
    ///
    dpx::Characteristic get_characteristic_from_string(const std::string& str);

    /// Helper function - retrieve libdpx descriptor given nchannels and
    /// the channel names.
    dpx::Descriptor get_image_descriptor();

    /// Helper function - set keycode values from int array
    ///
    void set_keycode_values(int* array);
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
dpx_output_imageio_create()
{
    return new DPXOutput;
}

// OIIO_EXPORT int dpx_imageio_version = OIIO_PLUGIN_VERSION;   // it's in dpxinput.cpp

OIIO_EXPORT const char* dpx_output_extensions[] = { "dpx", nullptr };

OIIO_PLUGIN_EXPORTS_END



DPXOutput::DPXOutput()
    : m_stream(NULL)
{
    init();
}



DPXOutput::~DPXOutput()
{
    // Close, if not already done.
    close();
}



bool
DPXOutput::open(const std::string& name, int subimages, const ImageSpec* specs)
{
    if (subimages > MAX_DPX_IMAGE_ELEMENTS) {
        errorf("DPX does not support more than %d subimages",
               MAX_DPX_IMAGE_ELEMENTS);
        return false;
    };
    m_subimages_to_write = subimages;
    m_subimage_specs.clear();
    m_subimage_specs.insert(m_subimage_specs.begin(), specs, specs + subimages);
    return open(name, m_subimage_specs[0], Create);
}



bool
DPXOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (mode == Create) {
        m_subimage = 0;
        if (m_subimage_specs.size() < 1) {
            m_subimage_specs.resize(1);
            m_subimage_specs[0]  = userspec;
            m_subimages_to_write = 1;
        }
    } else if (mode == AppendSubimage) {
        if (m_write_pending)
            write_buffer();
        ++m_subimage;
        if (m_subimage >= m_subimages_to_write) {
            errorf("Exceeded the pre-declared number of subimages (%d)",
                   m_subimages_to_write);
            return false;
        }
        return prep_subimage(m_subimage, true);
        // Nothing else to do, the header taken care of when we opened with
        // Create.
    } else if (mode == AppendMIPLevel) {
        error("DPX does not support MIP-maps");
        return false;
    }

    // From here out, all the heavy lifting is done for Create
    ASSERT(mode == Create);

    if (is_opened())
        close();  // Close any already-opened file
    m_stream = new OutStream();
    if (!m_stream->Open(name.c_str())) {
        error("Could not open file \"%s\"", name.c_str());
        delete m_stream;
        m_stream = nullptr;
        return false;
    }
    m_dpx.SetOutStream(m_stream);
    m_dpx.Start();
    m_subimage = 0;

    ImageSpec& m_spec(m_subimage_specs[m_subimage]);  // alias the spec

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        errorf("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }

    if (m_spec.depth < 1)
        m_spec.depth = 1;
    else if (m_spec.depth > 1) {
        error("DPX does not support volume images (depth > 1)");
        return false;
    }

    // some metadata
    std::string software  = m_spec.get_string_attribute("Software", "");
    std::string project   = m_spec.get_string_attribute("DocumentName", "");
    std::string copyright = m_spec.get_string_attribute("Copyright", "");
    std::string datestr   = m_spec.get_string_attribute("DateTime", "");
    if (datestr.size() >= 19) {
        // libdpx's date/time format is pretty close to OIIO's (libdpx uses
        // %Y:%m:%d:%H:%M:%S%Z)
        // NOTE: the following code relies on the DateTime attribute being properly
        // formatted!
        // assume UTC for simplicity's sake, fix it if someone complains
        datestr[10] = ':';
        datestr.replace(19, -1, "Z");
    }

    // check if the client wants endianness reverse to native
    // assume big endian per Jeremy's request, unless little endian is
    // explicitly specified
    std::string endian = m_spec.get_string_attribute("oiio:Endian",
                                                     littleendian() ? "little"
                                                                    : "big");
    m_wantSwap         = (littleendian() != Strutil::iequals(endian, "little"));

    m_dpx.SetFileInfo(
        name.c_str(),                                             // filename
        datestr.c_str(),                                          // cr. date
        software.empty() ? OIIO_INTRO_STRING : software.c_str(),  // creator
        project.empty() ? NULL : project.c_str(),                 // project
        copyright.empty() ? NULL : copyright.c_str(),             // copyright
        m_spec.get_int_attribute("dpx:EncryptKey", ~0),  // encryption key
        m_wantSwap);

    // image info
    m_dpx.SetImageInfo(m_spec.width, m_spec.height);

    for (int s = 0; s < m_subimages_to_write; ++s) {
        prep_subimage(s, false);
        m_dpx.header.SetBitDepth(s, m_bitdepth);
        ImageSpec& spec(m_subimage_specs[s]);
        bool datasign = (spec.format == TypeDesc::INT8
                         || spec.format == TypeDesc::INT16);
        m_dpx.SetElement(
            s, m_desc, m_bitdepth, m_transfer, m_cmetr, m_packing, dpx::kNone,
            datasign, spec.get_int_attribute("dpx:LowData", 0xFFFFFFFF),
            spec.get_float_attribute("dpx:LowQuantity",
                                     std::numeric_limits<float>::quiet_NaN()),
            spec.get_int_attribute("dpx:HighData", 0xFFFFFFFF),
            spec.get_float_attribute("dpx:HighQuantity",
                                     std::numeric_limits<float>::quiet_NaN()),
            spec.get_int_attribute("dpx:EndOfLinePadding", 0),
            spec.get_int_attribute("dpx:EndOfImagePadding", 0));
        std::string desc = spec.get_string_attribute("ImageDescription", "");
        m_dpx.header.SetDescription(s, desc.c_str());
        // TODO: Writing RLE compressed files seem to be broken.
        // if (Strutil::iequals(spec.get_string_attribute("compression"),"rle"))
        //     m_dpx.header.SetImageEncoding(s, dpx::kRLE);
    }

    m_dpx.header.SetXScannedSize(
        m_spec.get_float_attribute("dpx:XScannedSize",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetYScannedSize(
        m_spec.get_float_attribute("dpx:YScannedSize",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetFramePosition(
        m_spec.get_int_attribute("dpx:FramePosition", 0xFFFFFFFF));
    m_dpx.header.SetSequenceLength(
        m_spec.get_int_attribute("dpx:SequenceLength", 0xFFFFFFFF));
    m_dpx.header.SetHeldCount(
        m_spec.get_int_attribute("dpx:HeldCount", 0xFFFFFFFF));
    m_dpx.header.SetFrameRate(
        m_spec.get_float_attribute("dpx:FrameRate",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetShutterAngle(
        m_spec.get_float_attribute("dpx:ShutterAngle",
                                   std::numeric_limits<float>::quiet_NaN()));
    // FIXME: should we write the input version through or always default to 2.0?
    /*tmpstr = m_spec.get_string_attribute ("dpx:Version", "");
    if (tmpstr.size () > 0)
        m_dpx.header.SetVersion (tmpstr.c_str ());*/
    std::string tmpstr;
    tmpstr = m_spec.get_string_attribute("dpx:FrameId", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetFrameId(tmpstr.c_str());
    tmpstr = m_spec.get_string_attribute("dpx:SlateInfo", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetSlateInfo(tmpstr.c_str());
    tmpstr = m_spec.get_string_attribute("dpx:SourceImageFileName", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetSourceImageFileName(tmpstr.c_str());
    tmpstr = m_spec.get_string_attribute("dpx:InputDevice", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetInputDevice(tmpstr.c_str());
    tmpstr = m_spec.get_string_attribute("dpx:InputDeviceSerialNumber", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetInputDeviceSerialNumber(tmpstr.c_str());
    m_dpx.header.SetInterlace(m_spec.get_int_attribute("dpx:Interlace", 0xFF));
    m_dpx.header.SetFieldNumber(
        m_spec.get_int_attribute("dpx:FieldNumber", 0xFF));
    m_dpx.header.SetHorizontalSampleRate(
        m_spec.get_float_attribute("dpx:HorizontalSampleRate",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetVerticalSampleRate(
        m_spec.get_float_attribute("dpx:VerticalSampleRate",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetTemporalFrameRate(
        m_spec.get_float_attribute("dpx:TemporalFrameRate",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetTimeOffset(
        m_spec.get_float_attribute("dpx:TimeOffset",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBlackLevel(
        m_spec.get_float_attribute("dpx:BlackLevel",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBlackGain(
        m_spec.get_float_attribute("dpx:BlackGain",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBreakPoint(
        m_spec.get_float_attribute("dpx:BreakPoint",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetWhiteLevel(
        m_spec.get_float_attribute("dpx:WhiteLevel",
                                   std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetIntegrationTimes(
        m_spec.get_float_attribute("dpx:IntegrationTimes",
                                   std::numeric_limits<float>::quiet_NaN()));
    float aspect = m_spec.get_float_attribute("PixelAspectRatio", 1.0f);
    int aspect_num, aspect_den;
    float_to_rational(aspect, aspect_num, aspect_den);
    m_dpx.header.SetAspectRatio(0, aspect_num);
    m_dpx.header.SetAspectRatio(1, aspect_den);
    m_dpx.header.SetXOffset((unsigned int)std::max(0, m_spec.x));
    m_dpx.header.SetYOffset((unsigned int)std::max(0, m_spec.y));
    m_dpx.header.SetXOriginalSize((unsigned int)m_spec.full_width);
    m_dpx.header.SetYOriginalSize((unsigned int)m_spec.full_height);

    static int DpxOrientations[] = { 0,
                                     dpx::kLeftToRightTopToBottom,
                                     dpx::kRightToLeftTopToBottom,
                                     dpx::kLeftToRightBottomToTop,
                                     dpx::kRightToLeftBottomToTop,
                                     dpx::kTopToBottomLeftToRight,
                                     dpx::kTopToBottomRightToLeft,
                                     dpx::kBottomToTopLeftToRight,
                                     dpx::kBottomToTopRightToLeft };
    int orient                   = m_spec.get_int_attribute("Orientation", 0);
    orient                       = DpxOrientations[clamp(orient, 0, 8)];
    m_dpx.header.SetImageOrientation((dpx::Orientation)orient);

    ParamValue* tc = m_spec.find_attribute("smpte:TimeCode", TypeTimeCode,
                                           false);
    if (tc) {
        unsigned int* timecode = (unsigned int*)tc->data();
        m_dpx.header.timeCode  = timecode[0];
        m_dpx.header.userBits  = timecode[1];
    } else {
        std::string timecode = m_spec.get_string_attribute("dpx:TimeCode", "");
        int tmpint           = m_spec.get_int_attribute("dpx:TimeCode", ~0);
        if (timecode.size() > 0)
            m_dpx.header.SetTimeCode(timecode.c_str());
        else if (tmpint != ~0)
            m_dpx.header.timeCode = tmpint;
        m_dpx.header.userBits = m_spec.get_int_attribute("dpx:UserBits", ~0);
    }

    ParamValue* kc = m_spec.find_attribute("smpte:KeyCode", TypeKeyCode, false);
    if (kc) {
        int* array = (int*)kc->data();
        set_keycode_values(array);

        // See if there is an overloaded dpx:Format
        std::string format = m_spec.get_string_attribute("dpx:Format", "");
        if (format.size() > 0)
            m_dpx.header.SetFormat(format.c_str());
    }

    std::string srcdate = m_spec.get_string_attribute("dpx:SourceDateTime", "");
    if (srcdate.size() >= 19) {
        // libdpx's date/time format is pretty close to OIIO's (libdpx uses
        // %Y:%m:%d:%H:%M:%S%Z)
        // NOTE: the following code relies on the DateTime attribute being properly
        // formatted!
        // assume UTC for simplicity's sake, fix it if someone complains
        srcdate[10] = ':';
        srcdate.replace(19, -1, "Z");
        m_dpx.header.SetSourceTimeDate(srcdate.c_str());
    }

    // set the user data size
    ParamValue* user = m_spec.find_attribute("dpx:UserData");
    if (user && user->datasize() > 0 && user->datasize() <= 1024 * 1024) {
        m_dpx.SetUserData(user->datasize());
    }

    // commit!
    if (!m_dpx.WriteHeader()) {
        error("Failed to write DPX header");
        return false;
    }

    // write the user data
    if (user && user->datasize() > 0 && user->datasize() <= 1024 * 1024) {
        if (!m_dpx.WriteUserData((void*)user->data())) {
            error("Failed to write user data");
            return false;
        }
    }

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return prep_subimage(m_subimage, true);
}



bool
DPXOutput::prep_subimage(int s, bool allocate)
{
    m_spec = m_subimage_specs[s];  // stash the spec

    // determine descriptor
    m_desc = get_image_descriptor();

    // transfer function
    std::string colorspace = m_spec.get_string_attribute("oiio:ColorSpace", "");
    if (Strutil::iequals(colorspace, "Linear"))
        m_transfer = dpx::kLinear;
    else if (Strutil::iequals(colorspace, "GammaCorrected"))
        m_transfer = dpx::kUserDefined;
    else if (Strutil::iequals(colorspace, "Rec709"))
        m_transfer = dpx::kITUR709;
    else if (Strutil::iequals(colorspace, "KodakLog"))
        m_transfer = dpx::kLogarithmic;
    else {
        std::string dpxtransfer = m_spec.get_string_attribute("dpx:Transfer",
                                                              "");
        m_transfer              = get_characteristic_from_string(dpxtransfer);
    }

    // colorimetric
    m_cmetr = get_characteristic_from_string(
        m_spec.get_string_attribute("dpx:Colorimetric", "User defined"));

    // select packing method
    std::string pck = m_spec.get_string_attribute("dpx:Packing",
                                                  "Filled, method A");
    if (Strutil::iequals(pck, "Packed"))
        m_packing = dpx::kPacked;
    else if (Strutil::iequals(pck, "Filled, method B"))
        m_packing = dpx::kFilledMethodB;
    else
        m_packing = dpx::kFilledMethodA;

    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8:
    case TypeDesc::UINT16:
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        // supported, fine
        break;
    case TypeDesc::HALF:
        // Turn half into float
        m_spec.format.basetype = TypeDesc::FLOAT;
        break;
    default:
        // Turn everything else into UINT16
        m_spec.format.basetype = TypeDesc::UINT16;
        break;
    }

    // calculate target bit depth
    m_bitdepth = m_spec.format.size() * 8;
    if (m_spec.format == TypeDesc::UINT16) {
        m_bitdepth = m_spec.get_int_attribute("oiio:BitsPerSample", 16);
        if (m_bitdepth != 10 && m_bitdepth != 12 && m_bitdepth != 16) {
            errorf("Unsupported bit depth %d", m_bitdepth);
            return false;
        }
    }

    // Bug workaround: libDPX doesn't appear to correctly support
    // "filled method A" for 12 bit data.  Does anybody care what
    // packing/filling we use?  Punt and just use "packed".
    if (m_bitdepth == 12)
        m_packing = dpx::kPacked;
    // I've also seen problems with 10 bits, but only for 1-channel images.
    if (m_bitdepth == 10 && m_spec.nchannels == 1)
        m_packing = dpx::kPacked;

    if (m_spec.format == TypeDesc::UINT8 || m_spec.format == TypeDesc::INT8)
        m_datasize = dpx::kByte;
    else if (m_spec.format == TypeDesc::UINT16
             || m_spec.format == TypeDesc::INT16)
        m_datasize = dpx::kWord;
    else if (m_spec.format == TypeDesc::FLOAT
             || m_spec.format == TypeDesc::HALF) {
        m_spec.format = TypeDesc::FLOAT;
        m_datasize    = dpx::kFloat;
    } else if (m_spec.format == TypeDesc::DOUBLE)
        m_datasize = dpx::kDouble;
    else {
        // use 16-bit unsigned integers as a failsafe
        m_spec.set_format(TypeDesc::UINT16);
        m_datasize = dpx::kWord;
    }

    // check if the client is giving us raw data to write
    m_rawcolor = m_spec.get_int_attribute("dpx:RawColor")
                 || m_spec.get_int_attribute("dpx:RawData")  // deprecated
                 || m_spec.get_int_attribute("oiio:RawColor");

    // see if we'll need to convert color space or not
    if (m_desc == dpx::kRGB || m_desc == dpx::kRGBA || m_spec.nchannels == 1) {
        // shortcut for RGB/RGBA, and for 1-channel images that don't
        // need to decode color representations.
        m_bytes    = m_spec.scanline_bytes();
        m_rawcolor = true;
    } else {
        m_bytes = dpx::QueryNativeBufferSize(m_desc, m_datasize, m_spec.width,
                                             1);
        if (m_bytes == 0 && !m_rawcolor) {
            error("Unable to deliver native format data from source data");
            return false;
        } else if (m_bytes < 0) {
            // no need to allocate another buffer
            if (!m_rawcolor)
                m_bytes = m_spec.scanline_bytes();
            else
                m_bytes = -m_bytes;
        }
    }
    if (m_bytes < 0)
        m_bytes = -m_bytes;

    // allocate space for the image data buffer
    if (allocate)
        m_buf.resize(m_bytes * m_spec.height);

    return true;
}



bool
DPXOutput::write_buffer()
{
    bool ok = true;
    if (m_write_pending) {
        ok = m_dpx.WriteElement(m_subimage, &m_buf[0], m_datasize);
        if (!ok) {
            const char* err = strerror(errno);
            errorf("DPX write failed (%s)",
                   (err && err[0]) ? err : "unknown error");
        }
        m_write_pending = false;
    }
    return ok;
}



bool
DPXOutput::close()
{
    if (!m_stream) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    ok &= write_buffer();
    m_dpx.Finish();
    init();  // Reset to initial state
    return ok;
}



bool
DPXOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    m_write_pending = true;

    m_spec.auto_stride(xstride, format, m_spec.nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    unsigned char* dst = &m_buf[(y - m_spec.y) * m_bytes];
    if (m_rawcolor)
        // fast path - just dump the scanline into the buffer
        memcpy(dst, data, m_spec.scanline_bytes());
    else if (!dpx::ConvertToNative(m_desc, m_datasize, m_cmetr, m_spec.width, 1,
                                   data, dst))
        return false;

    return true;
}



bool
DPXOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



dpx::Characteristic
DPXOutput::get_characteristic_from_string(const std::string& str)
{
    if (Strutil::iequals(str, "User defined"))
        return dpx::kUserDefined;
    else if (Strutil::iequals(str, "Printing density"))
        return dpx::kPrintingDensity;
    else if (Strutil::iequals(str, "Linear"))
        return dpx::kLinear;
    else if (Strutil::iequals(str, "Logarithmic"))
        return dpx::kLogarithmic;
    else if (Strutil::iequals(str, "Unspecified video"))
        return dpx::kUnspecifiedVideo;
    else if (Strutil::iequals(str, "SMPTE 274M"))
        return dpx::kSMPTE274M;
    else if (Strutil::iequals(str, "ITU-R 709-4"))
        return dpx::kITUR709;
    else if (Strutil::iequals(str, "ITU-R 601-5 system B or G"))
        return dpx::kITUR601;
    else if (Strutil::iequals(str, "ITU-R 601-5 system M"))
        return dpx::kITUR602;
    else if (Strutil::iequals(str, "NTSC composite video"))
        return dpx::kNTSCCompositeVideo;
    else if (Strutil::iequals(str, "PAL composite video"))
        return dpx::kPALCompositeVideo;
    else if (Strutil::iequals(str, "Z depth linear"))
        return dpx::kZLinear;
    else if (Strutil::iequals(str, "Z depth homogeneous"))
        return dpx::kZHomogeneous;
    else if (Strutil::iequals(str, "ADX"))
        return dpx::kADX;
    else
        return dpx::kUndefinedCharacteristic;
}



dpx::Descriptor
DPXOutput::get_image_descriptor()
{
    switch (m_spec.nchannels) {
    case 1: {
        std::string name = m_spec.channelnames.size() ? m_spec.channelnames[0]
                                                      : "";
        if (m_spec.z_channel == 0 || name == "Z")
            return dpx::kDepth;
        else if (m_spec.alpha_channel == 0 || name == "A")
            return dpx::kAlpha;
        else if (name == "R")
            return dpx::kRed;
        else if (name == "B")
            return dpx::kBlue;
        else if (name == "G")
            return dpx::kGreen;
        else
            return dpx::kLuma;
    }
    case 3: return dpx::kRGB;
    case 4: return dpx::kRGBA;
    default:
        if (m_spec.nchannels <= 8)
            return (dpx::Descriptor)((int)dpx::kUserDefined2Comp
                                     + m_spec.nchannels - 2);
        return dpx::kUndefinedDescriptor;
    }
}



void
DPXOutput::set_keycode_values(int* array)
{
    // Manufacturer code
    {
        std::stringstream ss;
        ss << std::setfill('0');
        ss << std::setw(2) << array[0];
        memcpy(m_dpx.header.filmManufacturingIdCode, ss.str().c_str(), 2);
    }

    // Film type
    {
        std::stringstream ss;
        ss << std::setfill('0');
        ss << std::setw(2) << array[1];
        memcpy(m_dpx.header.filmType, ss.str().c_str(), 2);
    }

    // Prefix
    {
        std::stringstream ss;
        ss << std::setfill('0');
        ss << std::setw(6) << array[2];
        memcpy(m_dpx.header.prefix, ss.str().c_str(), 6);
    }

    // Count
    {
        std::stringstream ss;
        ss << std::setfill('0');
        ss << std::setw(4) << array[3];
        memcpy(m_dpx.header.count, ss.str().c_str(), 4);
    }

    // Perforation Offset
    {
        std::stringstream ss;
        ss << std::setfill('0');
        ss << std::setw(2) << array[4];
        memcpy(m_dpx.header.perfsOffset, ss.str().c_str(), 2);
    }

    // Format
    int& perfsPerFrame = array[5];
    int& perfsPerCount = array[6];

    if (perfsPerFrame == 15 && perfsPerCount == 120) {
        Strutil::safe_strcpy(m_dpx.header.format, "8kimax",
                             sizeof(m_dpx.header.format));
    } else if (perfsPerFrame == 8 && perfsPerCount == 64) {
        Strutil::safe_strcpy(m_dpx.header.format, "VistaVision",
                             sizeof(m_dpx.header.format));
    } else if (perfsPerFrame == 4 && perfsPerCount == 64) {
        Strutil::safe_strcpy(m_dpx.header.format, "Full Aperture",
                             sizeof(m_dpx.header.format));
    } else if (perfsPerFrame == 3 && perfsPerCount == 64) {
        Strutil::safe_strcpy(m_dpx.header.format, "3perf",
                             sizeof(m_dpx.header.format));
    } else {
        Strutil::safe_strcpy(m_dpx.header.format, "Unknown",
                             sizeof(m_dpx.header.format));
    }
}

OIIO_PLUGIN_NAMESPACE_END
