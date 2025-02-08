// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/typedesc.h>

#include "rla_pvt.h"



OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace RLA_pvt;


class RLAOutput final : public ImageOutput {
public:
    RLAOutput();
    ~RLAOutput() override;
    const char* format_name(void) const override { return "rla"; }
    int supports(string_view feature) const override;
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::vector<unsigned char> m_scratch;
    RLAHeader m_rla;                   ///< Wavefront RLA header
    std::vector<uint32_t> m_sot;       ///< Scanline offset table
    std::vector<unsigned char> m_rle;  ///< Run record buffer for RLE
    std::vector<unsigned char> m_tilebuffer;
    unsigned int m_dither;

    // Initialize private members to pre-opened state
    void init(void)
    {
        ioproxy_clear();
        m_sot.clear();
    }

    /// Helper - sets a chromaticity from attribute
    inline void set_chromaticity(const ParamValue* p, char* dst,
                                 size_t field_size, const char* default_val);

    // Helper - handles the repetitive work of encoding and writing a
    // channel. The data is guaranteed to be in the scratch area and need
    // not be preserved.
    bool encode_channel(unsigned char* data, stride_t xstride,
                        TypeDesc chantype, int bits);


    /// Helper: write buf[0..nitems-1], swap endianness if necessary
    template<typename T> bool write(const T* buf, size_t nitems = 1)
    {
        if (littleendian()
            && (std::is_same<T, uint16_t>::value
                || std::is_same<T, int16_t>::value
                || std::is_same<T, uint32_t>::value
                || std::is_same<T, int32_t>::value)) {
            T* newbuf = OIIO_ALLOCA(T, nitems);
            memcpy(newbuf, buf, nitems * sizeof(T));
            swap_endian(newbuf, nitems);
            buf = newbuf;
        }
        return iowrite(buf, sizeof(T), nitems);
    }
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
rla_output_imageio_create()
{
    return new RLAOutput;
}

// OIIO_EXPORT int rla_imageio_version = OIIO_PLUGIN_VERSION;   // it's in rlainput.cpp

OIIO_EXPORT const char* rla_output_extensions[] = { "rla", nullptr };

OIIO_PLUGIN_EXPORTS_END


RLAOutput::RLAOutput() { init(); }



RLAOutput::~RLAOutput()
{
    // Close, if not already done.
    close();
}



int
RLAOutput::supports(string_view feature) const
{
    if (feature == "random_access")
        return true;
    if (feature == "displaywindow")
        return true;
    if (feature == "origin")
        return true;
    if (feature == "negativeorigin")
        return true;
    if (feature == "alpha")
        return true;
    if (feature == "nchannels")
        return true;
    if (feature == "channelformats")
        return true;
    if (feature == "ioproxy")
        return true;
    // Support nothing else nonstandard
    return false;
}



bool
RLAOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (!check_open(mode, userspec, { 0, 65535, 0, 65535, 0, 1, 0, 256 }))
        return false;
    // FIXME -- the RLA format supports subimages, but our writer doesn't.
    // I'm not sure if it's worth worrying about for an old format that is so
    // rarely used.  We'll come back to it if anybody actually encounters a
    // multi-subimage RLA in the wild.

    if (m_spec.format == TypeDesc::UNKNOWN)
        m_spec.format = TypeDesc::UINT8;  // Default to uint8 if unknown

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    // prepare and write the RLA header
    memset(&m_rla, 0, sizeof(m_rla));
    // frame and window coordinates
    m_rla.WindowLeft   = m_spec.full_x;
    m_rla.WindowRight  = m_spec.full_x + m_spec.full_width - 1;
    m_rla.WindowTop    = m_spec.full_height - 1 - m_spec.full_y;
    m_rla.WindowBottom = m_rla.WindowTop - m_spec.full_height + 1;

    m_rla.ActiveLeft   = m_spec.x;
    m_rla.ActiveRight  = m_spec.x + m_spec.width - 1;
    m_rla.ActiveTop    = m_spec.height - 1 - m_spec.y;
    m_rla.ActiveBottom = m_rla.ActiveTop - m_spec.height + 1;

    m_rla.FrameNumber = m_spec.get_int_attribute("rla:FrameNumber", 0);

    // figure out what's going on with the channels
    int remaining = m_spec.nchannels;
    if (m_spec.channelformats.size()) {
        int streak;
        // accommodate first 3 channels of the same type as colour ones
        for (streak = 1; streak <= 3 && remaining > 0; ++streak, --remaining)
            if (m_spec.channelformats[streak] != m_spec.channelformats[0]
                || m_spec.alpha_channel == streak || m_spec.z_channel == streak)
                break;
        m_rla.ColorChannelType = rla_type(m_spec.channelformats[0]);
        int bits = m_spec.get_int_attribute("oiio:BitsPerSample", 0);
        m_rla.NumOfChannelBits = bits ? bits
                                      : m_spec.channelformats[0].size() * 8;
        // limit to 3 in case the loop went further
        m_rla.NumOfColorChannels = std::min(streak, 3);
        // if we have anything left and it looks like alpha, treat it as alpha
        if (remaining && m_spec.z_channel != m_rla.NumOfColorChannels) {
            for (streak = 1; remaining > 0; ++streak, --remaining)
                if (m_spec.channelformats[m_rla.NumOfColorChannels + streak]
                    != m_spec.channelformats[m_rla.NumOfColorChannels])
                    break;
            m_rla.MatteChannelType = rla_type(
                m_spec.channelformats[m_rla.NumOfColorChannels]);
            m_rla.NumOfMatteBits
                = bits ? bits
                       : m_spec.channelformats[m_rla.NumOfColorChannels].size()
                             * 8;
            m_rla.NumOfMatteChannels = streak;
        } else {
            m_rla.MatteChannelType   = CT_BYTE;
            m_rla.NumOfMatteBits     = 8;
            m_rla.NumOfMatteChannels = 0;
        }
        // and if there's something more left, put it in auxiliary
        if (remaining) {
            for (streak = 1; remaining > 0; ++streak, --remaining)
                if (m_spec.channelformats[m_rla.NumOfColorChannels
                                          + m_rla.NumOfMatteChannels + streak]
                    != m_spec.channelformats[m_rla.NumOfColorChannels
                                             + m_rla.NumOfMatteChannels])
                    break;
            m_rla.AuxChannelType = rla_type(
                m_spec.channelformats[m_rla.NumOfColorChannels
                                      + m_rla.NumOfMatteChannels]);
            m_rla.NumOfAuxBits = m_spec
                                     .channelformats[m_rla.NumOfColorChannels
                                                     + m_rla.NumOfMatteChannels]
                                     .size()
                                 * 8;
            m_rla.NumOfAuxChannels = streak;
        }
    } else {
        m_rla.ColorChannelType = m_rla.MatteChannelType = m_rla.AuxChannelType
            = rla_type(m_spec.format);
        int bits = m_spec.get_int_attribute("oiio:BitsPerSample", 0);
        if (bits) {
            m_rla.NumOfChannelBits = bits;
        } else {
            if (m_spec.channelformats.size())
                m_rla.NumOfChannelBits = m_spec.channelformats[0].size() * 8;
            else
                m_rla.NumOfChannelBits = m_spec.format.size() * 8;
        }
        m_rla.NumOfMatteBits = m_rla.NumOfAuxBits = m_rla.NumOfChannelBits;
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
        if (remaining && m_spec.z_channel != m_rla.NumOfColorChannels) {
            --remaining;
            ++m_rla.NumOfMatteChannels;
        }
        // anything left is auxiliary
        if (remaining > 0)
            m_rla.NumOfAuxChannels = remaining;
    }
    // std::cout << "color chans " << m_rla.NumOfColorChannels << " a "
    //           << m_rla.NumOfMatteChannels << " z " << m_rla.NumOfAuxChannels << "\n";
    m_rla.Revision = 0xFFFE;

    const ColorConfig& colorconfig = ColorConfig::default_colorconfig();
    string_view colorspace = m_spec.get_string_attribute("oiio:ColorSpace");
    if (colorconfig.equivalent(colorspace, "linear")
        || colorconfig.equivalent(colorspace, "scene_linear"))
        Strutil::safe_strcpy(m_rla.Gamma, "1.0", sizeof(m_rla.Gamma));
    else if (colorconfig.equivalent(colorspace, "g22_rec709"))
        Strutil::safe_strcpy(m_rla.Gamma, "2.2", sizeof(m_rla.Gamma));
    else if (colorconfig.equivalent(colorspace, "g18_rec709"))
        Strutil::safe_strcpy(m_rla.Gamma, "1.8", sizeof(m_rla.Gamma));
    else if (Strutil::istarts_with(colorspace, "Gamma")) {
        Strutil::parse_word(colorspace);
        float g = Strutil::from_string<float>(colorspace);
        if (!(g >= 0.01f && g <= 10.0f /* sanity check */))
            g = m_spec.get_float_attribute("oiio:Gamma", 1.f);
        safe_format_to(m_rla.Gamma, "{:.10}", g);
    }

    const ParamValue* p;
    // default NTSC chromaticities
    p = m_spec.find_attribute("rla:RedChroma");
    set_chromaticity(p, m_rla.RedChroma, sizeof(m_rla.RedChroma), "0.67 0.08");
    p = m_spec.find_attribute("rla:GreenChroma");
    set_chromaticity(p, m_rla.GreenChroma, sizeof(m_rla.GreenChroma),
                     "0.21 0.71");
    p = m_spec.find_attribute("rla:BlueChroma");
    set_chromaticity(p, m_rla.BlueChroma, sizeof(m_rla.BlueChroma),
                     "0.14 0.33");
    p = m_spec.find_attribute("rla:WhitePoint");
    set_chromaticity(p, m_rla.WhitePoint, sizeof(m_rla.WhitePoint),
                     "0.31 0.316");

#define STRING_FIELD(rlafield, name)                                    \
    {                                                                   \
        std::string s = m_spec.get_string_attribute(name);              \
        if (s.length()) {                                               \
            strncpy(m_rla.rlafield, s.c_str(), sizeof(m_rla.rlafield)); \
            m_rla.rlafield[sizeof(m_rla.rlafield) - 1] = 0;             \
        } else {                                                        \
            m_rla.rlafield[0] = 0;                                      \
        }                                                               \
    }

    m_rla.JobNumber = m_spec.get_int_attribute("rla:JobNumber", 0);
    STRING_FIELD(FileName, "rla:FileName");
    STRING_FIELD(Description, "ImageDescription");
    STRING_FIELD(ProgramName, "Software");
    STRING_FIELD(MachineName, "HostComputer");
    STRING_FIELD(UserName, "Artist");

    // the month number will be replaced with the 3-letter abbreviation
    time_t t = time(NULL);
    struct tm localtm;
    Sysutil::get_local_time(&t, &localtm);
    strftime(m_rla.DateCreated, sizeof(m_rla.DateCreated), "%b %d %H:%M %Y",
             &localtm);

    // FIXME: it appears that Wavefront have defined a set of aspect names;
    // I think it's safe not to care until someone complains
    STRING_FIELD(Aspect, "rla:Aspect");

    float aspect = m_spec.get_float_attribute("PixelAspectRatio", 1.f);
    safe_format_to(m_rla.AspectRatio, "{:.6}", aspect);
    Strutil::safe_strcpy(m_rla.ColorChannel,
                         m_spec.get_string_attribute("rla:ColorChannel", "rgb"),
                         sizeof(m_rla.ColorChannel));
    m_rla.FieldRendered = m_spec.get_int_attribute("rla:FieldRendered", 0);

    STRING_FIELD(Time, "rla:Time");
    STRING_FIELD(Filter, "rla:Filter");
    STRING_FIELD(AuxData, "rla:AuxData");

    m_rla.rla_swap_endian();  // RLAs are big-endian
    write(&m_rla);
    m_rla.rla_swap_endian();  // flip back the endianness to native

    // write placeholder values - not all systems may expand the file with
    // zeroes upon seek
    m_sot.resize(m_spec.height, (int32_t)0);
    write(&m_sot[0], m_sot.size());

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



inline void
RLAOutput::set_chromaticity(const ParamValue* p, char* dst, size_t field_size,
                            const char* default_val)
{
    if (p && p->type().basetype == TypeDesc::FLOAT) {
        switch (p->type().aggregate) {
        case TypeDesc::VEC2:
            safe_format_to(dst, field_size, "{:.4} {:.4}",
                           ((const float*)p->data())[0],
                           ((const float*)p->data())[1]);
            break;
        case TypeDesc::VEC3:
            safe_format_to(dst, field_size, "{:.4} {:.4} {:.4}",
                           ((const float*)p->data())[0],
                           ((const float*)p->data())[1],
                           ((const float*)p->data())[2]);
            break;
        }
    } else
        Strutil::safe_strcpy(dst, default_val, field_size);
}



bool
RLAOutput::close()
{
    if (!ioproxy_opened()) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_DASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    // Now that all scanlines have been output, return to write the
    // correct scanline offset table to file and close the stream.
    ioseek(sizeof(RLAHeader));
    write(m_sot.data(), m_sot.size());

    init();  // re-initialize
    return ok;
}



bool
RLAOutput::encode_channel(unsigned char* data, stride_t xstride,
                          TypeDesc chantype, int bits)
{
    if (chantype == TypeDesc::FLOAT) {
        // Special case -- float data is just dumped raw, no RLE
        uint16_t size = m_spec.width * sizeof(float);
        write(&size);
        for (int x = 0; x < m_spec.width; ++x)
            write((const float*)&data[x * xstride]);
        return true;
    }

    if (chantype == TypeDesc::UINT16 && bits != 16) {
        // Need to do bit scaling. Safe to overwrite data in place.
        for (int x = 0; x < m_spec.width; ++x) {
            unsigned short* s = (unsigned short*)(data + x * xstride);
            *s                = bit_range_convert(*s, 16, bits);
        }
    }

    m_rle.resize(2);  // reserve t bytes for the encoded size

    // multi-byte data types are sliced to MSB, nextSB, ..., LSB
    int chsize = (int)chantype.size();
    for (int byte = 0; byte < chsize; ++byte) {
        int lastval    = -1;     // last value
        int count      = 0;      // count of raw or repeats
        bool repeat    = false;  // if true, we're repeating
        int runbegin   = 0;      // where did the run begin
        int byteoffset = bigendian() ? byte : (chsize - byte - 1);
        for (int x = 0; x < m_spec.width; ++x) {
            int newval = data[x * xstride + byteoffset];
            if (count == 0) {  // beginning of a run.
                count    = 1;
                repeat   = true;  // presumptive
                runbegin = x;
            } else if (repeat) {  // We've seen one or more repeating characters
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
                        m_rle.push_back(count - 1);
                        m_rle.push_back(lastval);
                        count    = 1;
                        runbegin = x;
                    }
                }
            } else {  // Have not been repeating
                if (newval == lastval) {
                    // starting a repetition?  Output previous
                    OIIO_DASSERT(count > 1);
                    // write everything but the last char
                    --count;
                    m_rle.push_back(-count);
                    for (int i = 0; i < count; ++i)
                        m_rle.push_back(
                            data[(runbegin + i) * xstride + byteoffset]);
                    count    = 2;
                    runbegin = x - 1;
                    repeat   = true;
                } else {
                    ++count;  // another non-repeat
                }
            }

            // If the run is too long or we're at the scanline end, write
            if (count == 127 || x == m_spec.width - 1) {
                if (repeat) {
                    m_rle.push_back(count - 1);
                    m_rle.push_back(lastval);
                } else {
                    m_rle.push_back(-count);
                    for (int i = 0; i < count; ++i)
                        m_rle.push_back(
                            data[(runbegin + i) * xstride + byteoffset]);
                }
                count = 0;
            }
            lastval = newval;
        }
        OIIO_ASSERT(count == 0);
    }

    // Now that we know the size of the encoded buffer, save it at the
    // beginning
    uint16_t size = uint16_t(m_rle.size() - 2);
    m_rle[0]      = size >> 8;
    m_rle[1]      = size & 255;

    // And write the channel to the file
    return write(m_rle.data(), m_rle.size());
}



bool
RLAOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    OIIO_DASSERT(data != nullptr);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // store the offset to the scanline.  We'll swap_endian if necessary
    // when we go to actually write it.
    m_sot[m_spec.height - 1 - (y - m_spec.y)] = (uint32_t)iotell();

    size_t pixelsize = m_spec.pixel_bytes(true /*native*/);
    int offset       = 0;
    for (int c = 0; c < m_spec.nchannels; ++c) {
        TypeDesc chantype = m_spec.channelformats.size()
                                ? m_spec.channelformats[c]
                                : m_spec.format;
        int bits = (c < m_rla.NumOfColorChannels) ? m_rla.NumOfChannelBits
                   : (c < (m_rla.NumOfColorChannels + m_rla.NumOfMatteBits))
                       ? m_rla.NumOfMatteBits
                       : m_rla.NumOfAuxBits;
        if (!encode_channel((unsigned char*)data + offset, pixelsize, chantype,
                            bits))
            return false;
        offset += chantype.size();
    }

    return true;
}



bool
RLAOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END
