// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
    ~DPXOutput() override;
    const char* format_name(void) const override { return "dpx"; }
    int supports(string_view feature) const override
    {
        if (feature == "multiimage" || feature == "alpha"
            || feature == "nchannels" || feature == "random_access"
            || feature == "rewrite" || feature == "displaywindow"
            || feature == "origin" || feature == "ioproxy")
            return true;
        return false;
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool open(const std::string& name, int subimages,
              const ImageSpec* specs) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    OutStream* m_stream = nullptr;
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
        ioproxy_clear();
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



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
dpx_output_imageio_create()
{
    return new DPXOutput;
}

// OIIO_EXPORT int dpx_imageio_version = OIIO_PLUGIN_VERSION;   // it's in dpxinput.cpp

OIIO_EXPORT const char* dpx_output_extensions[] = { "dpx", nullptr };

OIIO_PLUGIN_EXPORTS_END



DPXOutput::DPXOutput() { init(); }



DPXOutput::~DPXOutput()
{
    // Close, if not already done.
    close();
}



bool
DPXOutput::open(const std::string& name, int subimages, const ImageSpec* specs)
{
    if (subimages > MAX_DPX_IMAGE_ELEMENTS) {
        errorfmt("DPX does not support more than {} subimages",
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
    if (mode == AppendSubimage) {
        if (!is_opened()) {
            errorfmt("open() with AppendSubimage called but file is not open.");
            return false;
        }
        if (m_write_pending)
            write_buffer();
        ++m_subimage;
        if (m_subimage >= m_subimages_to_write) {
            errorfmt("Exceeded the pre-declared number of subimages ({})",
                     m_subimages_to_write);
            return false;
        }
        return prep_subimage(m_subimage, true);
        // Nothing else to do, the header taken care of when we opened with
        // Create.
    }

    if (is_opened())
        close();  // Close any already-opened file

    if (!check_open(mode, userspec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 256 }))
        return false;

    // From here out, all the heavy lifting is done for Create
    OIIO_DASSERT(mode == Create);

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    m_subimage = 0;
    if (m_subimage_specs.empty()) {
        m_subimage_specs.resize(1);
        m_subimage_specs[0]  = m_spec;
        m_subimages_to_write = 1;
    }

    m_stream = new OutStream(ioproxy());
    m_dpx.SetOutStream(m_stream);
    m_dpx.Start();
    m_subimage = 0;

    ImageSpec& spec0(m_subimage_specs[m_subimage]);  // alias the spec

    // some metadata
    std::string software  = spec0.get_string_attribute("Software", "");
    std::string project   = spec0.get_string_attribute("DocumentName", "");
    std::string copyright = spec0.get_string_attribute("Copyright", "");
    std::string datestr   = spec0.get_string_attribute("DateTime", "");
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
    std::string endian = spec0.get_string_attribute("oiio:Endian",
                                                    littleendian() ? "little"
                                                                   : "big");
    m_wantSwap         = (littleendian() != Strutil::iequals(endian, "little"));

    m_dpx.SetFileInfo(
        name.c_str(),                                             // filename
        datestr.c_str(),                                          // cr. date
        software.empty() ? OIIO_INTRO_STRING : software.c_str(),  // creator
        project.empty() ? NULL : project.c_str(),                 // project
        copyright.empty() ? NULL : copyright.c_str(),             // copyright
        spec0.get_int_attribute("dpx:EncryptKey", ~0),  // encryption key
        m_wantSwap);

    // image info
    m_dpx.SetImageInfo(spec0.width, spec0.height);

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
        spec0.get_float_attribute("dpx:XScannedSize",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetYScannedSize(
        spec0.get_float_attribute("dpx:YScannedSize",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetFramePosition(
        spec0.get_int_attribute("dpx:FramePosition", 0xFFFFFFFF));
    m_dpx.header.SetSequenceLength(
        spec0.get_int_attribute("dpx:SequenceLength", 0xFFFFFFFF));
    m_dpx.header.SetHeldCount(
        spec0.get_int_attribute("dpx:HeldCount", 0xFFFFFFFF));
    m_dpx.header.SetFrameRate(
        spec0.get_float_attribute("dpx:FrameRate",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetShutterAngle(
        spec0.get_float_attribute("dpx:ShutterAngle",
                                  std::numeric_limits<float>::quiet_NaN()));
    // FIXME: should we write the input version through or always default to 2.0?
    /*tmpstr = spec0.get_string_attribute ("dpx:Version", "");
    if (tmpstr.size () > 0)
        m_dpx.header.SetVersion (tmpstr.c_str ());*/
    std::string tmpstr;
    tmpstr = spec0.get_string_attribute("dpx:FrameId", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetFrameId(tmpstr.c_str());
    tmpstr = spec0.get_string_attribute("dpx:SlateInfo", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetSlateInfo(tmpstr.c_str());
    tmpstr = spec0.get_string_attribute("dpx:SourceImageFileName", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetSourceImageFileName(tmpstr.c_str());
    tmpstr = spec0.get_string_attribute("dpx:InputDevice", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetInputDevice(tmpstr.c_str());
    tmpstr = spec0.get_string_attribute("dpx:InputDeviceSerialNumber", "");
    if (tmpstr.size() > 0)
        m_dpx.header.SetInputDeviceSerialNumber(tmpstr.c_str());
    m_dpx.header.SetInterlace(spec0.get_int_attribute("dpx:Interlace", 0xFF));
    m_dpx.header.SetFieldNumber(
        spec0.get_int_attribute("dpx:FieldNumber", 0xFF));
    m_dpx.header.SetHorizontalSampleRate(
        spec0.get_float_attribute("dpx:HorizontalSampleRate",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetVerticalSampleRate(
        spec0.get_float_attribute("dpx:VerticalSampleRate",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetTemporalFrameRate(
        spec0.get_float_attribute("dpx:TemporalFrameRate",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetTimeOffset(
        spec0.get_float_attribute("dpx:TimeOffset",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBlackLevel(
        spec0.get_float_attribute("dpx:BlackLevel",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBlackGain(
        spec0.get_float_attribute("dpx:BlackGain",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetBreakPoint(
        spec0.get_float_attribute("dpx:BreakPoint",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetWhiteLevel(
        spec0.get_float_attribute("dpx:WhiteLevel",
                                  std::numeric_limits<float>::quiet_NaN()));
    m_dpx.header.SetIntegrationTimes(
        spec0.get_float_attribute("dpx:IntegrationTimes",
                                  std::numeric_limits<float>::quiet_NaN()));
    float aspect = spec0.get_float_attribute("PixelAspectRatio", 1.0f);
    int aspect_num, aspect_den;
    float_to_rational(aspect, aspect_num, aspect_den);
    m_dpx.header.SetAspectRatio(0, aspect_num);
    m_dpx.header.SetAspectRatio(1, aspect_den);
    m_dpx.header.SetXOffset((unsigned int)std::max(0, spec0.x));
    m_dpx.header.SetYOffset((unsigned int)std::max(0, spec0.y));
    m_dpx.header.SetXOriginalSize((unsigned int)spec0.full_width);
    m_dpx.header.SetYOriginalSize((unsigned int)spec0.full_height);

    static int DpxOrientations[] = { 0,
                                     dpx::kLeftToRightTopToBottom,
                                     dpx::kRightToLeftTopToBottom,
                                     dpx::kLeftToRightBottomToTop,
                                     dpx::kRightToLeftBottomToTop,
                                     dpx::kTopToBottomLeftToRight,
                                     dpx::kTopToBottomRightToLeft,
                                     dpx::kBottomToTopLeftToRight,
                                     dpx::kBottomToTopRightToLeft };
    int orient                   = spec0.get_int_attribute("Orientation", 0);
    orient                       = DpxOrientations[clamp(orient, 0, 8)];
    m_dpx.header.SetImageOrientation((dpx::Orientation)orient);

    ParamValue* tc = spec0.find_attribute("smpte:TimeCode", TypeTimeCode,
                                          false);
    if (tc) {
        unsigned int* timecode = (unsigned int*)tc->data();
        m_dpx.header.timeCode  = timecode[0];
        m_dpx.header.userBits  = timecode[1];
    } else {
        std::string timecode = spec0.get_string_attribute("dpx:TimeCode", "");
        int tmpint           = spec0.get_int_attribute("dpx:TimeCode", ~0);
        if (timecode.size() > 0)
            m_dpx.header.SetTimeCode(timecode.c_str());
        else if (tmpint != ~0)
            m_dpx.header.timeCode = tmpint;
        m_dpx.header.userBits = spec0.get_int_attribute("dpx:UserBits", ~0);
    }

    ParamValue* kc = spec0.find_attribute("smpte:KeyCode", TypeKeyCode, false);
    if (kc) {
        int* array = (int*)kc->data();
        set_keycode_values(array);

        // See if there is an overloaded dpx:Format
        std::string format = spec0.get_string_attribute("dpx:Format", "");
        if (format.size() > 0)
            m_dpx.header.SetFormat(format.c_str());
    }

    std::string srcdate = spec0.get_string_attribute("dpx:SourceDateTime", "");
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
    ParamValue* user = spec0.find_attribute("dpx:UserData");
    if (user && user->datasize() > 0 && user->datasize() <= 1024 * 1024) {
        m_dpx.SetUserData(user->datasize());
    }

    // commit!
    if (!m_dpx.WriteHeader()) {
        errorfmt("Failed to write DPX header");
        close();
        return false;
    }

    // write the user data
    if (user && user->datasize() > 0 && user->datasize() <= 1024 * 1024) {
        if (!m_dpx.WriteUserData((void*)user->data())) {
            errorfmt("Failed to write user data");
            close();
            return false;
        }
    }

    m_dither = (spec0.format == TypeDesc::UINT8)
                   ? spec0.get_int_attribute("oiio:dither", 0)
                   : 0;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (spec0.tile_width && spec0.tile_height)
        m_tilebuffer.resize(spec0.image_bytes());

    return prep_subimage(m_subimage, true);
}



bool
DPXOutput::prep_subimage(int s, bool allocate)
{
    ImageSpec& spec_s(m_subimage_specs[s]);  // reference the spec

    // determine descriptor
    m_desc = get_image_descriptor();

    // transfer function
    std::string colorspace = spec_s.get_string_attribute("oiio:ColorSpace", "");
    if (Strutil::iequals(colorspace, "Linear"))
        m_transfer = dpx::kLinear;
    else if (Strutil::istarts_with(colorspace, "Gamma"))
        m_transfer = dpx::kUserDefined;
    else if (Strutil::iequals(colorspace, "Rec709"))
        m_transfer = dpx::kITUR709;
    else if (Strutil::iequals(colorspace, "KodakLog"))
        m_transfer = dpx::kLogarithmic;
    else {
        std::string dpxtransfer = spec_s.get_string_attribute("dpx:Transfer",
                                                              "");
        m_transfer              = get_characteristic_from_string(dpxtransfer);
    }

    // colorimetric
    m_cmetr = get_characteristic_from_string(
        spec_s.get_string_attribute("dpx:Colorimetric", "User defined"));

    // select packing method
    std::string pck = spec_s.get_string_attribute("dpx:Packing",
                                                  "Filled, method A");
    if (Strutil::iequals(pck, "Packed"))
        m_packing = dpx::kPacked;
    else if (Strutil::iequals(pck, "Filled, method B"))
        m_packing = dpx::kFilledMethodB;
    else
        m_packing = dpx::kFilledMethodA;

    switch (spec_s.format.basetype) {
    case TypeDesc::UINT8:
    case TypeDesc::UINT16:
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        // supported, fine
        break;
    case TypeDesc::HALF:
        // Turn half into float
        spec_s.format.basetype = TypeDesc::FLOAT;
        break;
    default:
        // Turn everything else into UINT16
        spec_s.format.basetype = TypeDesc::UINT16;
        break;
    }

    // calculate target bit depth
    m_bitdepth = spec_s.format.size() * 8;
    if (spec_s.format == TypeDesc::UINT16) {
        m_bitdepth = spec_s.get_int_attribute("oiio:BitsPerSample", 16);
        if (m_bitdepth != 10 && m_bitdepth != 12 && m_bitdepth != 16) {
            errorfmt("Unsupported bit depth {}", m_bitdepth);
            return false;
        }
    }

    // Bug workaround: libDPX doesn't appear to correctly support
    // "filled method A" for 12 bit data.  Does anybody care what
    // packing/filling we use?  Punt and just use "packed".
    if (m_bitdepth == 12)
        m_packing = dpx::kPacked;
    // I've also seen problems with 10 bits, but only for 1-channel images.
    if (m_bitdepth == 10 && spec_s.nchannels == 1)
        m_packing = dpx::kPacked;

    if (spec_s.format == TypeDesc::UINT8 || spec_s.format == TypeDesc::INT8)
        m_datasize = dpx::kByte;
    else if (spec_s.format == TypeDesc::UINT16
             || spec_s.format == TypeDesc::INT16)
        m_datasize = dpx::kWord;
    else if (spec_s.format == TypeDesc::FLOAT
             || spec_s.format == TypeDesc::HALF) {
        spec_s.format = TypeDesc::FLOAT;
        m_datasize    = dpx::kFloat;
    } else if (spec_s.format == TypeDesc::DOUBLE)
        m_datasize = dpx::kDouble;
    else {
        // use 16-bit unsigned integers as a failsafe
        spec_s.set_format(TypeDesc::UINT16);
        m_datasize = dpx::kWord;
    }

    // check if the client is giving us raw data to write
    m_rawcolor = spec_s.get_int_attribute("dpx:RawColor")
                 || spec_s.get_int_attribute("dpx:RawData")  // deprecated
                 || spec_s.get_int_attribute("oiio:RawColor");

    // see if we'll need to convert color space or not
    if (m_desc == dpx::kRGB || m_desc == dpx::kRGBA || spec_s.nchannels == 1) {
        // shortcut for RGB/RGBA, and for 1-channel images that don't
        // need to decode color representations.
        m_bytes    = spec_s.scanline_bytes();
        m_rawcolor = true;
    } else {
        OIIO_ASSERT(0 && "Unsupported color space");
        return false;
#if 0  /* NOT USED IN OIIO */
        m_bytes = dpx::QueryNativeBufferSize(m_desc, m_datasize, spec_s.width,
                                             1);
        if (m_bytes == 0 && !m_rawcolor) {
            errorfmt("Unable to deliver native format data from source data");
            return false;
        } else if (m_bytes < 0) {
            // no need to allocate another buffer
            if (!m_rawcolor)
                m_bytes = spec_s.scanline_bytes();
            else
                m_bytes = -m_bytes;
        }
#endif /* NOT USED IN OIIO */
    }
    if (m_bytes < 0)
        m_bytes = -m_bytes;

    // allocate space for the image data buffer
    if (allocate)
        m_buf.resize(m_bytes * spec_s.height);

    m_spec = spec_s;
    return true;
}



bool
DPXOutput::write_buffer()
{
    if (!is_opened())
        return false;

    bool ok = true;
    if (m_write_pending && m_buf.size()) {
        ok = m_dpx.WriteElement(m_subimage, m_buf.data(), m_datasize);
        if (!ok) {
            const char* err = strerror(errno);
            errorfmt("DPX write failed ({})",
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
    const ImageSpec& spec_s(m_subimage_specs[m_subimage]);
    if (spec_s.tile_width && m_tilebuffer.size()) {
        // Handle tile emulation -- output the buffered pixels
        ok &= write_scanlines(spec_s.y, spec_s.y + spec_s.height, 0,
                              spec_s.format, &m_tilebuffer[0]);
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
    if (!is_opened()) {
        errorfmt("write_scanline called but file is not open.");
        return false;
    }

    m_write_pending = true;

    const ImageSpec& spec_s(m_subimage_specs[m_subimage]);
    spec_s.auto_stride(xstride, format, spec_s.nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + spec_s.scanline_bytes());
        data = &m_scratch[0];
    }

    unsigned char* dst = &m_buf[(y - spec_s.y) * m_bytes];
    if (m_rawcolor)
        // fast path - just dump the scanline into the buffer
        memcpy(dst, data, spec_s.scanline_bytes());
    else if (!dpx::ConvertToNative(m_desc, m_datasize, m_cmetr, spec_s.width, 1,
                                   data, dst))
        return false;

    return true;
}



bool
DPXOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!is_opened()) {
        errorfmt("write_tile called but file is not open.");
        return false;
    }

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
    const ImageSpec& spec0(m_subimage_specs[0]);
    switch (spec0.nchannels) {
    case 1: {
        std::string name = spec0.channelnames.size() ? spec0.channelnames[0]
                                                     : "";
        if (spec0.z_channel == 0 || name == "Z")
            return dpx::kDepth;
        else if (spec0.alpha_channel == 0 || name == "A")
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
        if (spec0.nchannels <= 8)
            return (dpx::Descriptor)((int)dpx::kUserDefined2Comp
                                     + spec0.nchannels - 2);
        return dpx::kUndefinedDescriptor;
    }
}



void
DPXOutput::set_keycode_values(int* array)
{
    // Manufacturer code
    {
        std::string ss = Strutil::fmt::format("{:02d}", array[0]);
        memcpy(m_dpx.header.filmManufacturingIdCode, ss.c_str(), 2);
    }

    // Film type
    {
        std::string ss = Strutil::fmt::format("{:02d}", array[1]);
        memcpy(m_dpx.header.filmType, ss.c_str(), 2);
    }

    // Prefix
    {
        std::string ss = Strutil::fmt::format("{:06d}", array[2]);
        memcpy(m_dpx.header.prefix, ss.c_str(), 6);
    }

    // Count
    {
        std::string ss = Strutil::fmt::format("{:04d}", array[3]);
        memcpy(m_dpx.header.count, ss.c_str(), 4);
    }

    // Perforation Offset
    {
        std::string ss = Strutil::fmt::format("{:02d}", array[4]);
        memcpy(m_dpx.header.perfsOffset, ss.c_str(), 2);
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
