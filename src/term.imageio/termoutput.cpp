// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/sysutil.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace term_pvt {


class TermOutput : public ImageOutput {
public:
    TermOutput() { init(); }
    virtual ~TermOutput() { close(); }
    virtual const char* format_name() const { return "term"; }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create);
    virtual int supports(string_view feature) const;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride);
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride);
    virtual bool close();

private:
    ImageBuf m_buf;
    std::string m_method;
    bool m_fit = true;  // automatically fit to window size

    void init() { m_buf.clear(); }

    // Actually output the stored buffer to the console
    bool output();
};



int
TermOutput::supports(string_view feature) const
{
    return feature == "tiles" || feature == "alpha"
           || feature == "random_access" || feature == "rewrite"
           || feature == "procedural";
}



bool
TermOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    if (mode != Create) {
        error("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    if (spec.nchannels != 3 && spec.nchannels != 4) {
        error("%s does not support %d-channel images\n", format_name(),
              m_spec.nchannels);
        return false;
    }

    m_spec = spec;

    // Retrieve config hints giving special instructions
    m_method = Strutil::lower(m_spec["term:method"].get());
    m_fit    = m_spec["term:fit"].get<int>(1);

    // Store temp buffer in HALF format
    ImageSpec spec2 = m_spec;
    spec2.set_format(TypeDesc::HALF);
    m_buf.reset(spec2);
    ImageBufAlgo::zero(m_buf);

    return true;
}



bool
TermOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                           stride_t xstride)
{
    if (y > m_spec.height) {
        error("Attempt to write too many scanlines to terminal");
        close();
        return false;
    }
    ROI roi(m_spec.x, m_spec.x + m_spec.width, y, y + 1, z, z + 1, 0,
            m_spec.nchannels);
    return m_buf.set_pixels(roi, format, data, xstride);
}



bool
TermOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    ROI roi(x, std::min(x + m_spec.tile_width, m_spec.x + m_spec.width), y,
            std::min(y + m_spec.tile_height, m_spec.y + m_spec.height), z,
            std::min(z + m_spec.tile_depth, m_spec.z + m_spec.depth), 0,
            m_spec.nchannels);
    return m_buf.set_pixels(roi, format, data, xstride, ystride, zstride);
}



bool
TermOutput::close()
{
    if (!m_buf.initialized())
        return true;  // already closed

    output();

    init();  // clear everything
    return true;
}


bool
TermOutput::output()
{
    // Color convert in place to sRGB, or it won't look right
    std::string cspace = m_buf.spec()["oiio:colorspace"].get();
    ImageBufAlgo::colorconvert(m_buf, m_buf, cspace, "sRGB");

    string_view TERM(Sysutil::getenv("TERM"));
    string_view TERM_PROGRAM(Sysutil::getenv("TERM_PROGRAM"));
    string_view TERM_PROGRAM_VERSION(Sysutil::getenv("TERM_PROGRAM_VERSION"));
    Sysutil::Term term;

    string_view method(m_method);
    if (method.empty()) {
        if (TERM_PROGRAM == "iTerm.app"
            && Strutil::from_string<float>(TERM_PROGRAM_VERSION) >= 2.9) {
            method = "iterm2";
        } else if (TERM == "xterm" || TERM == "xterm-256color") {
            method = "24bit";
        } else {
            method = "256color";
        }
    }

    // Try to figure out how big an image we can display
    int w = m_buf.spec().width;
    int h = m_buf.spec().height;
    // iTerm2 is special, see bellow
    int maxw = (method == "iterm2") ? Sysutil::terminal_columns() * 16
                                    : Sysutil::terminal_columns();
    float yscale = (method == "iterm2" || method == "24bit") ? 1.0f : 0.5f;
    // Resize the image as needed
    if (w > maxw && m_fit) {
        ROI newsize(0, maxw, 0, int(std::round(yscale * float(maxw) / w * h)));
        m_buf = ImageBufAlgo::resize(m_buf, /*filter=*/nullptr, newsize);
        w     = newsize.width();
        h     = newsize.height();
    }

    if (method == "iterm2") {
        // iTerm2.app can display entire images in the window, if you use a
        // special escape sequence that lets you transmit a base64-encoded
        // image file, so we convert to just a simple PPM and do so.
        std::ostringstream s;
        s << "P3\n" << m_buf.spec().width << ' ' << m_buf.spec().height << "\n";
        s << "255\n";
        for (int y = m_buf.ybegin(), ye = m_buf.yend(); y < ye; y += 1) {
            for (int x = m_buf.xbegin(), xe = m_buf.xend(); x < xe; ++x) {
                unsigned char rgb[3];
                m_buf.get_pixels(ROI(x, x + 1, y, y + 1, 0, 1, 0, 3),
                                 TypeDesc::UINT8, &rgb);
                s << int(rgb[0]) << ' ' << int(rgb[1]) << ' ' << int(rgb[2])
                  << '\n';
            }
        }
        std::cout << "\033]"
                  << "1337;"
                  << "File=inline=1"
                  << ";width=auto"
                  << ":" << Strutil::base64_encode(s.str()) << '\007'
                  << std::endl;
        return true;
    }

    if (method == "24bit") {
        // Print two vertical pixels per character cell using the Unicode
        // "upper half block" glyph U+2580, with fg color set to the 24 bit
        // RGB value of the upper pixel, and bg color set to the 24-bit RGB
        // value the lower pixel.
        int z = m_buf.spec().z;
        for (int y = m_buf.ybegin(), ye = m_buf.yend(); y < ye; y += 2) {
            for (int x = m_buf.xbegin(), xe = m_buf.xend(); x < xe; ++x) {
                unsigned char rgb[2][3];
                m_buf.get_pixels(ROI(x, x + 1, y, y + 2, z, z + 1, 0, 3),
                                 TypeDesc::UINT8, &rgb);
                std::cout << term.ansi_fgcolor(rgb[0][0], rgb[0][1], rgb[0][2]);
                std::cout << term.ansi_bgcolor(rgb[1][0], rgb[1][1], rgb[1][2])
                          << "\u2580";
            }
            std::cout << term.ansi("default") << "\n";
        }
        return true;
    }

    if (method == "24bit-space") {
        // Print as space, with bg color set to the 24-bit RGB value of each
        // pixel.
        int z = m_buf.spec().z;
        for (int y = m_buf.ybegin(), ye = m_buf.yend(); y < ye; ++y) {
            for (int x = m_buf.xbegin(), xe = m_buf.xend(); x < xe; ++x) {
                unsigned char rgb[3];
                m_buf.get_pixels(ROI(x, x + 1, y, y + 1, z, z + 1, 0, 3),
                                 TypeDesc::UINT8, &rgb);
                std::cout << term.ansi_bgcolor(rgb[0], rgb[1], rgb[2]) << " ";
            }
            std::cout << term.ansi("default") << "\n";
        }
        return true;
    }

    if (method == "dither") {
        // Print as space, with bg color set to the 6x6x6 RGB value of each
        // pixels. Try to make it better with horizontal dithering. But...
        // it still looks bad. Room for future improvement?
        int z = m_buf.spec().z;
        for (int y = m_buf.ybegin(), ye = m_buf.yend(); y < ye; ++y) {
            simd::vfloat4 leftover(0.0f);
            for (int x = m_buf.xbegin(), xe = m_buf.xend(); x < xe; ++x) {
                simd::vfloat4 rgborig;
                m_buf.get_pixels(ROI(x, x + 1, y, y + 1, z, z + 1, 0, 3),
                                 TypeDesc::FLOAT, &rgborig);
                rgborig += leftover;
                simd::vfloat4 rgb = 5.0f * rgborig;
                simd::vint4 rgbi;
                OIIO_MAYBE_UNUSED simd::vfloat4 frac = floorfrac(rgb, &rgbi);
                leftover = rgborig - 0.2f * simd::vfloat4(rgbi);
                rgbi     = clamp(rgbi, simd::vint4(0), simd::vint4(5));
                std::cout << "\033[48;5;"
                          << (0x10 + 36 * rgbi[0] + 6 * rgbi[1] + rgbi[2])
                          << "m ";
            }
            std::cout << term.ansi("default") << "\n";
        }
        return true;
    }
    {
        // Print as space, with bg color set to the 6x6x6 RGB value of each
        // pixels. This looks awful!
        int z = m_buf.spec().z;
        for (int y = m_buf.ybegin(), ye = m_buf.yend(); y < ye; ++y) {
            for (int x = m_buf.xbegin(), xe = m_buf.xend(); x < xe; ++x) {
                simd::vfloat4 rgborig;
                m_buf.get_pixels(ROI(x, x + 1, y, y + 1, z, z + 1, 0, 3),
                                 TypeDesc::FLOAT, &rgborig);
                simd::vfloat4 rgb = 5.0f * rgborig;
                simd::vint4 rgbi;
                OIIO_MAYBE_UNUSED simd::vfloat4 frac = floorfrac(rgb, &rgbi);
                rgbi = clamp(rgbi, simd::vint4(0), simd::vint4(5));
                std::cout << "\033[48;5;"
                          << (0x10 + 36 * rgbi[0] + 6 * rgbi[1] + rgbi[2])
                          << "m ";
            }
            std::cout << term.ansi("default") << "\n";
        }
        return true;
    }

    return false;
}


}  // namespace term_pvt


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
term_output_imageio_create()
{
    return new term_pvt::TermOutput;
}

OIIO_EXPORT int term_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
term_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT const char* term_output_extensions[] = { "term", nullptr };

OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
