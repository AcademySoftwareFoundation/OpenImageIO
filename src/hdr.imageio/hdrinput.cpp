// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cassert>
#include <cstdio>
#include <iostream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>


OIIO_PLUGIN_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
// .hdr / .rgbe files - HDR files from Radiance
//
// General info on the hdr/rgbe format can be found at:
//     http://paulbourke.net/dataformats/pic/
//
// Also see Greg Ward's "Real Pixels" chapter in Graphics Gems II for an
// explanation of the encoding that's used in Radiance rgba files.
//
// Based on source code that originally came from:
//     http://www.graphics.cornell.edu/~bjw/rgbe.html
// But it's been modified very heavily, and little of the original remains.
// Nonetheless, it bore this original notice:
//     written by Bruce Walter  (bjw@graphics.cornell.edu)  5/26/95
//     based on code written by Greg Ward
/////////////////////////////////////////////////////////////////////////////



class HdrInput final : public ImageInput {
public:
    HdrInput() { init(); }
    ~HdrInput() override { close(); }
    const char* format_name(void) const override { return "hdr"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool close() override;
    int current_subimage(void) const override { return m_subimage; }
    bool seek_subimage(int subimage, int miplevel) override;

private:
    std::string m_filename;  // File name
    int m_subimage;          // What subimage are we looking at?
    int m_next_scanline;     // Next scanline to read
    std::vector<int64_t> m_scanline_offsets;  // Cached scanline offsets

    void init()
    {
        m_subimage      = -1;
        m_next_scanline = 0;
        m_scanline_offsets.clear();
        ioproxy_clear();
    }

    bool RGBE_ReadHeader();
    bool RGBE_ReadPixels(float* data, int y, uint64_t numpixels);
    bool RGBE_ReadPixels_RLE(float* data, int y, uint64_t scanline_width,
                             int num_scanlines);

    // helper: fgets reads a "line" from the proxy, akin to std fgets. The
    // bytes go in the buffer, and part up to and including the new line is
    // returned as a string_view, and the file pointer is updated to the byte
    // right after the newline.
    string_view fgets(span<char> buf)
    {
        Filesystem::IOProxy* m_io = ioproxy();
        int64_t pos               = m_io->tell();
        auto rdsize               = m_io->read(buf.data(), buf.size());
        string_view sv(buf.data(), rdsize);
        if (sv.size() == 0) {
            errorfmt(
                "RGBE read error -- early end of file at position {}, asked for {}, got {} bytes, file size {}",
                pos, buf.size(), rdsize, m_io->size());
            return sv;
        }
        sv = Strutil::parse_line(sv);
        m_io->seek(pos + sv.size());
        return sv;
    }
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int hdr_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
hdr_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT ImageInput*
hdr_input_imageio_create()
{
    return new HdrInput;
}

OIIO_EXPORT const char* hdr_input_extensions[] = { "hdr", "rgbe", nullptr };

OIIO_PLUGIN_EXPORTS_END


// Table of ldexpf(1.0f, i - (int)(128 + 8)) values.
// Note: Ward uses ldexp(col+0.5,exp-(128+8)).  However we wanted pixels
//       in the range [0,1] to map back into the range [0,1].
static const float exponent_table[256] = {
    1.1479437e-41f, 2.2958874e-41f, 4.5917748e-41f, 9.1835496e-41f,
    1.8367099e-40f, 3.6734198e-40f, 7.3468397e-40f, 1.4693679e-39f,
    2.9387359e-39f, 5.8774718e-39f, 1.1754944e-38f, 2.3509887e-38f,
    4.7019774e-38f, 9.4039548e-38f, 1.880791e-37f,  3.7615819e-37f,
    7.5231638e-37f, 1.5046328e-36f, 3.0092655e-36f, 6.0185311e-36f,
    1.2037062e-35f, 2.4074124e-35f, 4.8148249e-35f, 9.6296497e-35f,
    1.9259299e-34f, 3.8518599e-34f, 7.7037198e-34f, 1.540744e-33f,
    3.0814879e-33f, 6.1629758e-33f, 1.2325952e-32f, 2.4651903e-32f,
    4.9303807e-32f, 9.8607613e-32f, 1.9721523e-31f, 3.9443045e-31f,
    7.8886091e-31f, 1.5777218e-30f, 3.1554436e-30f, 6.3108872e-30f,
    1.2621774e-29f, 2.5243549e-29f, 5.0487098e-29f, 1.009742e-28f,
    2.0194839e-28f, 4.0389678e-28f, 8.0779357e-28f, 1.6155871e-27f,
    3.2311743e-27f, 6.4623485e-27f, 1.2924697e-26f, 2.5849394e-26f,
    5.1698788e-26f, 1.0339758e-25f, 2.0679515e-25f, 4.1359031e-25f,
    8.2718061e-25f, 1.6543612e-24f, 3.3087225e-24f, 6.6174449e-24f,
    1.323489e-23f,  2.646978e-23f,  5.2939559e-23f, 1.0587912e-22f,
    2.1175824e-22f, 4.2351647e-22f, 8.4703295e-22f, 1.6940659e-21f,
    3.3881318e-21f, 6.7762636e-21f, 1.3552527e-20f, 2.7105054e-20f,
    5.4210109e-20f, 1.0842022e-19f, 2.1684043e-19f, 4.3368087e-19f,
    8.6736174e-19f, 1.7347235e-18f, 3.469447e-18f,  6.9388939e-18f,
    1.3877788e-17f, 2.7755576e-17f, 5.5511151e-17f, 1.110223e-16f,
    2.220446e-16f,  4.4408921e-16f, 8.8817842e-16f, 1.7763568e-15f,
    3.5527137e-15f, 7.1054274e-15f, 1.4210855e-14f, 2.8421709e-14f,
    5.6843419e-14f, 1.1368684e-13f, 2.2737368e-13f, 4.5474735e-13f,
    9.094947e-13f,  1.8189894e-12f, 3.6379788e-12f, 7.2759576e-12f,
    1.4551915e-11f, 2.910383e-11f,  5.8207661e-11f, 1.1641532e-10f,
    2.3283064e-10f, 4.6566129e-10f, 9.3132257e-10f, 1.8626451e-09f,
    3.7252903e-09f, 7.4505806e-09f, 1.4901161e-08f, 2.9802322e-08f,
    5.9604645e-08f, 1.1920929e-07f, 2.3841858e-07f, 4.7683716e-07f,
    9.5367432e-07f, 1.9073486e-06f, 3.8146973e-06f, 7.6293945e-06f,
    1.5258789e-05f, 3.0517578e-05f, 6.1035156e-05f, 0.00012207031f,
    0.00024414062f, 0.00048828125f, 0.0009765625f,  0.001953125f,
    0.00390625f,    0.0078125f,     0.015625f,      0.03125f,
    0.0625f,        0.125f,         0.25f,          0.5f,
    1.0f,           2.0f,           4.0f,           8.0f,
    16.0f,          32.0f,          64.0f,          128.0f,
    256.0f,         512.0f,         1024.0f,        2048.0f,
    4096.0f,        8192.0f,        16384.0f,       32768.0f,
    65536.0f,       131072.0f,      262144.0f,      524288.0f,
    1048576.0f,     2097152.0f,     4194304.0f,     8388608.0f,
    16777216.0f,    33554432.0f,    67108864.0f,    1.3421773e+08f,
    2.6843546e+08f, 5.3687091e+08f, 1.0737418e+09f, 2.1474836e+09f,
    4.2949673e+09f, 8.5899346e+09f, 1.7179869e+10f, 3.4359738e+10f,
    6.8719477e+10f, 1.3743895e+11f, 2.7487791e+11f, 5.4975581e+11f,
    1.0995116e+12f, 2.1990233e+12f, 4.3980465e+12f, 8.796093e+12f,
    1.7592186e+13f, 3.5184372e+13f, 7.0368744e+13f, 1.4073749e+14f,
    2.8147498e+14f, 5.6294995e+14f, 1.1258999e+15f, 2.2517998e+15f,
    4.5035996e+15f, 9.0071993e+15f, 1.8014399e+16f, 3.6028797e+16f,
    7.2057594e+16f, 1.4411519e+17f, 2.8823038e+17f, 5.7646075e+17f,
    1.1529215e+18f, 2.305843e+18f,  4.611686e+18f,  9.223372e+18f,
    1.8446744e+19f, 3.6893488e+19f, 7.3786976e+19f, 1.4757395e+20f,
    2.9514791e+20f, 5.9029581e+20f, 1.1805916e+21f, 2.3611832e+21f,
    4.7223665e+21f, 9.444733e+21f,  1.8889466e+22f, 3.7778932e+22f,
    7.5557864e+22f, 1.5111573e+23f, 3.0223145e+23f, 6.0446291e+23f,
    1.2089258e+24f, 2.4178516e+24f, 4.8357033e+24f, 9.6714066e+24f,
    1.9342813e+25f, 3.8685626e+25f, 7.7371252e+25f, 1.547425e+26f,
    3.0948501e+26f, 6.1897002e+26f, 1.23794e+27f,   2.4758801e+27f,
    4.9517602e+27f, 9.9035203e+27f, 1.9807041e+28f, 3.9614081e+28f,
    7.9228163e+28f, 1.5845633e+29f, 3.1691265e+29f, 6.338253e+29f,
    1.2676506e+30f, 2.5353012e+30f, 5.0706024e+30f, 1.0141205e+31f,
    2.028241e+31f,  4.0564819e+31f, 8.1129638e+31f, 1.6225928e+32f,
    3.2451855e+32f, 6.4903711e+32f, 1.2980742e+33f, 2.5961484e+33f,
    5.1922969e+33f, 1.0384594e+34f, 2.0769187e+34f, 4.1538375e+34f,
    8.307675e+34f,  1.661535e+35f,  3.32307e+35f,   6.64614e+35f,
};


/* standard conversion from rgbe to float pixels */
inline void
rgbe2float(float& red, float& green, float& blue, unsigned char rgbe[4])
{
    if (rgbe[3]) { /*nonzero pixel*/
        float f = exponent_table[rgbe[3]];
        red     = rgbe[0] * f;
        green   = rgbe[1] * f;
        blue    = rgbe[2] * f;
    } else {
        red = green = blue = 0.0f;
    }
}



bool
HdrInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    char magic[2] {};
    const size_t numRead = ioproxy->pread(magic, sizeof(magic), 0);
    return numRead == sizeof(magic) && memcmp(magic, "#?", sizeof(magic)) == 0;
}



bool
HdrInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // Check 'config' for any special requests
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}



bool
HdrInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;

    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    // hdr always makes a 3 channel float image.
    // RGBE_ReadHeader will set the width and height.
    m_spec = ImageSpec(1, 1, 3, TypeDesc::FLOAT);

    if (!RGBE_ReadHeader()) {
        close();
        return false;
    }
    m_spec.full_width  = m_spec.width;
    m_spec.full_height = m_spec.height;

    // FIXME -- should we do anything about exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    m_next_scanline = 0;
    m_scanline_offsets.clear();
    m_scanline_offsets.push_back(iotell());

    m_subimage = 0;
    newspec    = spec();
    return true;
}



bool
HdrInput::seek_subimage(int subimage, int miplevel)
{
    // HDR doesn't support multiple subimages or mipmaps
    if (subimage != 0 || miplevel != 0)
        return false;

    return true;
}



bool
HdrInput::RGBE_ReadHeader()
{
    char buffer[128];
    string_view line = fgets(buffer);
    if (!line.size())
        return false;
    if (!Strutil::parse_prefix(line, "#?")) {
        // if you want to require the magic token then uncomment the next lines
        // errorfmt("RGBE header read error (line = '{}')", line);
        // return false;
    }
    /* string_view programtype = */ Strutil::parse_until(line);

    line = fgets(buffer);
    if (!line.size())
        return false;

    m_spec.set_colorspace("lin_rec709");
    // presume linear w/ srgb primaries -- seems like the safest assumption
    // for this old file format.

    bool found_FORMAT_line = false;
    for (int nlines = 0; nlines < 100 /* safety */; ++nlines) {
        if (line.size() == 0 || line[0] == '\n')  // stop at blank line
            break;
        float tempf;
        if (line == "FORMAT=32-bit_rle_rgbe\n") {
            found_FORMAT_line = true;
            /* LG says no:    break;       // format found so break out of loop */
        } else if (Strutil::parse_values(line, "GAMMA=", span<float>(tempf))) {
            // Round gamma to the nearest hundredth to prevent stupid
            // precision choices and make it easier for apps to make
            // decisions based on known gamma values. For example, you want
            // 2.2, not 2.19998.
            float g = float(1.0 / tempf);
            g       = roundf(100.0 * g) / 100.0f;
            set_colorspace_rec709_gamma(m_spec, g);
        } else if (Strutil::parse_values(line,
                                         "EXPOSURE=", span<float>(tempf))) {
            m_spec.attribute("hdr:exposure", tempf);
        }

        line = fgets(buffer);
        if (!line.size())
            return false;
    }
    if (!found_FORMAT_line) {
        errorfmt("no FORMAT specifier found");
        return false;
    }
    if (line != "\n") {
        errorfmt("missing blank line after FORMAT specifier");
        return false;
    }
    line = fgets(buffer);
    if (!line.size())
        return false;

    int hw[2];
    int orientation = 1;
    if (Strutil::parse_values(line, "-Y", hw, "+X")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 1;
    } else if (Strutil::parse_values(line, "-Y", hw, "-X")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 2;
    } else if (Strutil::parse_values(line, "+Y", hw, "-X")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 3;
    } else if (Strutil::parse_values(line, "+Y", hw, "+X")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 4;
    } else if (Strutil::parse_values(line, "+X", hw, "-Y")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 5;
    } else if (Strutil::parse_values(line, "+X", hw, "+Y")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 6;
    } else if (Strutil::parse_values(line, "-X", hw, "+Y")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 7;
    } else if (Strutil::parse_values(line, "-X", hw, "-Y")) {
        m_spec.height = hw[0], m_spec.width = hw[1];
        orientation = 8;
    } else {
        errorfmt("missing image size specifier");
        return false;
    }
    m_spec.attribute("Orientation", orientation);

    return true;
}



/* simple read routine.  will not correctly handle run length encoding */
bool
HdrInput::RGBE_ReadPixels(float* data, int y, uint64_t numpixels)
{
    size_t size = 4 * numpixels;
    unsigned char* rgbe;
    OIIO_ALLOCATE_STACK_OR_HEAP(rgbe, unsigned char, size);
    if (ioproxy()->read(rgbe, size) != size) {
        errorfmt("Read error reading pixels on scanline {}", y);
        return false;
    }
    for (uint64_t i = 0; i < numpixels; ++i)
        rgbe2float(data[3 * i], data[3 * i + 1], data[3 * i + 2], &rgbe[4 * i]);
    return true;
}



bool
HdrInput::RGBE_ReadPixels_RLE(float* data, int y, uint64_t scanline_width,
                              int num_scanlines)
{
    if ((scanline_width < 8) || (scanline_width > 0x7fff))
        /* run length encoding is not allowed so read flat*/
        return RGBE_ReadPixels(data, y, scanline_width * num_scanlines);

    unsigned char rgbe[4], *ptr, *ptr_end;
    int count;
    unsigned char buf[2];
    std::vector<unsigned char> scanline_buffer;
    Filesystem::IOProxy* m_io = ioproxy();

    /* read in each successive scanline */
    while (num_scanlines > 0) {
        if (m_io->read(rgbe, sizeof(rgbe)) < sizeof(rgbe)) {
            errorfmt("Read error on scanline {}", y);
            return false;
        }
        if ((rgbe[0] != 2) || (rgbe[1] != 2) || (rgbe[2] & 0x80)) {
            /* this file is not run length encoded */
            rgbe2float(data[0], data[1], data[2], rgbe);
            data += 3;
            return RGBE_ReadPixels(data, y, scanline_width * num_scanlines - 1);
        }
        if ((((uint64_t)rgbe[2]) << 8 | rgbe[3]) != scanline_width) {
            errorfmt("wrong scanline width for scanline {}", y);
            return false;
        }
        scanline_buffer.resize(4 * scanline_width);

        ptr = scanline_buffer.data();
        /* read each of the four channels for the scanline into the buffer */
        for (int i = 0; i < 4; i++) {
            ptr_end = scanline_buffer.data() + (i + 1) * scanline_width;
            while (ptr < ptr_end) {
                if (m_io->read(buf, 2) < 2) {
                    errorfmt("Read error on scanline {}", y);
                    return false;
                }
                if (buf[0] > 128) {
                    /* a run of the same value */
                    count = buf[0] - 128;
                    if ((count == 0) || (count > ptr_end - ptr)) {
                        errorfmt("bad scanline {} data", y);
                        return false;
                    }
                    while (count-- > 0)
                        *ptr++ = buf[1];
                } else {
                    /* a non-run */
                    count = buf[0];
                    if ((count == 0) || (count > ptr_end - ptr)) {
                        errorfmt("bad scanline {} data", y);
                        return false;
                    }
                    *ptr++ = buf[1];
                    if (--count > 0) {
                        if (m_io->read(ptr, count) < uint64_t(count)) {
                            errorfmt("Read error on scanline {}", y);
                            return false;
                        }
                        ptr += count;
                    }
                }
            }
        }
        /* now convert data from buffer into floats */
        for (uint64_t i = 0; i < scanline_width; i++) {
            rgbe[0] = scanline_buffer[i];
            rgbe[1] = scanline_buffer[i + scanline_width];
            rgbe[2] = scanline_buffer[i + 2 * scanline_width];
            rgbe[3] = scanline_buffer[i + 3 * scanline_width];
            rgbe2float(data[0], data[1], data[2], rgbe);
            data += 3;
        }
        num_scanlines--;
    }
    return true;
}



bool
HdrInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (m_next_scanline != y) {
        // For random access, use cached file offsets of scanlines. This avoids
        // re-reading the same pixels many times over.
        m_next_scanline = std::min((size_t)y, m_scanline_offsets.size() - 1);
        ioseek(m_scanline_offsets[m_next_scanline]);
    }

    while (m_next_scanline <= y) {
        // Keep reading until we've read the scanline we really need
        bool ok = RGBE_ReadPixels_RLE((float*)data, y, m_spec.width, 1);
        ++m_next_scanline;
        if ((size_t)m_next_scanline == m_scanline_offsets.size()) {
            m_scanline_offsets.push_back(iotell());
        }
        if (!ok)
            return false;
    }
    return true;
}



bool
HdrInput::close()
{
    init();  // Reset to initial state
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
