// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


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
//     http://local.wasp.uwa.edu.au/~pbourke/dataformats/pic/
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
    virtual ~HdrInput() { close(); }
    virtual const char* format_name(void) const override { return "hdr"; }
    virtual int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool open(const std::string& name, ImageSpec& newspec,
                      const ImageSpec& config) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;
    virtual bool close() override;
    virtual int current_subimage(void) const override { return m_subimage; }
    virtual bool seek_subimage(int subimage, int miplevel) override;

private:
    std::string m_filename;  // File name
    int m_subimage;          // What subimage are we looking at?
    int m_next_scanline;     // Next scanline to read
    std::vector<int64_t> m_scanline_offsets;  // Cached scanline offsets
    int64_t m_io_pos = 0;                     // current position

    void init()
    {
        m_subimage      = -1;
        m_next_scanline = 0;
        m_scanline_offsets.clear();
        ioproxy_clear();
    }

    bool RGBE_ReadHeader();
    bool RGBE_ReadPixels(float* data, int y, uint64_t numpixels);
    bool RGBE_ReadPixels_RLE(float* data, int y, int scanline_width,
                             int num_scanlines);

    // helper: fgets reads a "line" from the proxy, akin to std fgets. The
    // bytes go in the buffer, and part up to and including the new line is
    // returned as a string_view, and the file pointer is updated to the byte
    // right after the newline.
    string_view fgets(span<char> buf)
    {
        Filesystem::IOProxy* m_io = ioproxy();
        auto rdsize = m_io->pread(buf.data(), buf.size(), m_io_pos);
        string_view sv(buf.data(), rdsize);
        if (sv.size() == 0) {
            errorfmt(
                "RGBE read error -- early end of file at position {}, asked for {}, got {} bytes, file size {}",
                m_io_pos, buf.size(), rdsize, m_io->size());
            return sv;
        }
        sv = Strutil::parse_line(sv);
        m_io_pos += sv.size();
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



/* standard conversion from rgbe to float pixels */
/* note: Ward uses ldexp(col+0.5,exp-(128+8)).  However we wanted pixels */
/*       in the range [0,1] to map back into the range [0,1].            */
inline void
rgbe2float(float& red, float& green, float& blue, unsigned char rgbe[4])
{
    if (rgbe[3]) { /*nonzero pixel*/
        float f = ldexpf(1.0f, rgbe[3] - (int)(128 + 8));
        red     = rgbe[0] * f;
        green   = rgbe[1] * f;
        blue    = rgbe[2] * f;
    } else {
        red = green = blue = 0.0f;
    }
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

    m_io_pos = iotell();

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
    m_scanline_offsets.push_back(m_io_pos);

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

    // Skip the hard work if we're already on the requested subimage
    if (subimage == current_subimage()) {
        return true;
    }

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

    m_spec.attribute("oiio:ColorSpace", "linear");  // presume linear
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
            m_spec.attribute("oiio:Gamma", g);
            if (g == 1.0f)
                m_spec.attribute("oiio:ColorSpace", "linear");
            else
                m_spec.attribute("oiio:ColorSpace",
                                 Strutil::sprintf("Gamma%.2g", g));

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
    if (ioproxy()->pread(rgbe, size, m_io_pos) != size) {
        errorfmt("Read error reading pixels on scanline {}", y);
        return false;
    }
    m_io_pos += size;
    for (uint64_t i = 0; i < numpixels; ++i)
        rgbe2float(data[3 * i], data[3 * i + 1], data[3 * i + 2], &rgbe[4 * i]);
    return true;
}



bool
HdrInput::RGBE_ReadPixels_RLE(float* data, int y, int scanline_width,
                              int num_scanlines)
{
    if ((scanline_width < 8) || (scanline_width > 0x7fff))
        /* run length encoding is not allowed so read flat*/
        return RGBE_ReadPixels(data, y, scanline_width * num_scanlines);

    unsigned char rgbe[4], *ptr, *ptr_end;
    int i, count;
    unsigned char buf[2];
    std::vector<unsigned char> scanline_buffer;
    Filesystem::IOProxy* m_io = ioproxy();

    /* read in each successive scanline */
    while (num_scanlines > 0) {
        if (m_io->pread(rgbe, sizeof(rgbe), m_io_pos) < sizeof(rgbe)) {
            errorfmt("Read error on scanline {}", y);
            return false;
        }
        m_io_pos += sizeof(rgbe);
        if ((rgbe[0] != 2) || (rgbe[1] != 2) || (rgbe[2] & 0x80)) {
            /* this file is not run length encoded */
            rgbe2float(data[0], data[1], data[2], rgbe);
            data += 3;
            return RGBE_ReadPixels(data, y, scanline_width * num_scanlines - 1);
        }
        if ((((int)rgbe[2]) << 8 | rgbe[3]) != scanline_width) {
            errorfmt("wrong scanline width for scanline {}", y);
            return false;
        }
        scanline_buffer.resize(4 * scanline_width);

        ptr = &scanline_buffer[0];
        /* read each of the four channels for the scanline into the buffer */
        for (i = 0; i < 4; i++) {
            ptr_end = &scanline_buffer[(i + 1) * scanline_width];
            while (ptr < ptr_end) {
                if (m_io->pread(buf, 2, m_io_pos) < 2) {
                    errorfmt("Read error on scanline {}", y);
                    return false;
                }
                m_io_pos += 2;
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
                        if (m_io->pread(ptr, count, m_io_pos)
                            < uint64_t(count)) {
                            errorfmt("Read error on scanline {}", y);
                            return false;
                        }
                        m_io_pos += count;
                        ptr += count;
                    }
                }
            }
        }
        /* now convert data from buffer into floats */
        for (i = 0; i < scanline_width; i++) {
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
        m_io_pos        = m_scanline_offsets[m_next_scanline];
    }

    while (m_next_scanline <= y) {
        // Keep reading until we've read the scanline we really need
        bool ok = RGBE_ReadPixels_RLE((float*)data, y, m_spec.width, 1);
        ++m_next_scanline;
        if ((size_t)m_next_scanline == m_scanline_offsets.size()) {
            m_scanline_offsets.push_back(m_io_pos);
        }
        if (!ok)
            return false;
    }
    return true;
}



bool
HdrInput::close()
{
    // if (m_io_local) {
    //     // If we allocated our own ioproxy, close it.
    //     ioproxy_clear();
    // }

    init();  // Reset to initial state
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
