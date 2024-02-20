// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cassert>
#include <cstdio>
#include <iostream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

class HdrOutput final : public ImageOutput {
public:
    HdrOutput() { init(); }
    ~HdrOutput() override { close(); }
    const char* format_name(void) const override { return "hdr"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;

private:
    std::vector<unsigned char> scratch;
    std::vector<unsigned char> m_tilebuffer;

    void init(void) { ioproxy_clear(); }

    bool RGBE_WritePixels(float* data, int64_t numpixels);
    bool RGBE_WriteBytes_RLE(unsigned char* data, int numbytes);
    bool RGBE_WritePixels_RLE(float* data, int scanline_width,
                              int num_scanlines);
};


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
hdr_output_imageio_create()
{
    return new HdrOutput;
}

OIIO_EXPORT const char* hdr_output_extensions[] = { "hdr", "rgbe", nullptr };

OIIO_PLUGIN_EXPORTS_END


// convert float[3] to rgbe-encoded pixels
inline void
float2rgbe(unsigned char* rgbe, float red, float green, float blue)
{
    float v = red;
    if (green > v)
        v = green;
    if (blue > v)
        v = blue;
    if (v < 1e-32) {
        rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
    } else {
        int e;
        v       = frexpf(v, &e) * 256.0f / v;
        rgbe[0] = (unsigned char)(red * v);
        rgbe[1] = (unsigned char)(green * v);
        rgbe[2] = (unsigned char)(blue * v);
        rgbe[3] = (unsigned char)(e + 128);
    }
}


inline void
float2rgbe(unsigned char* rgbe, const float* rgb)
{
    return float2rgbe(rgbe, rgb[0], rgb[1], rgb[2]);
}



// The code below is only needed for the run-length encoded files.
// Run length encoding adds considerable complexity but does
// save some space.  For each scanline, each channel (r,g,b,e) is
// encoded separately for better compression.
bool
HdrOutput::RGBE_WriteBytes_RLE(unsigned char* data, int numbytes)
{
    static const int MINRUNLENGTH = 4;
    int cur, beg_run, run_count, old_run_count, nonrun_count;
    unsigned char buf[2];

    cur = 0;
    while (cur < numbytes) {
        beg_run = cur;
        /* find next run of length at least 4 if one exists */
        run_count = old_run_count = 0;
        while ((run_count < MINRUNLENGTH) && (beg_run < numbytes)) {
            beg_run += run_count;
            old_run_count = run_count;
            run_count     = 1;
            while ((beg_run + run_count < numbytes) && (run_count < 127)
                   && (data[beg_run] == data[beg_run + run_count]))
                run_count++;
        }
        /* if data before next big run is a short run then write it as such */
        if ((old_run_count > 1) && (old_run_count == beg_run - cur)) {
            buf[0] = 128 + old_run_count; /*write short run*/
            buf[1] = data[cur];
            if (!iowrite(buf, 2))
                return false;
            cur = beg_run;
        }
        /* write out bytes until we reach the start of the next run */
        while (cur < beg_run) {
            nonrun_count = beg_run - cur;
            if (nonrun_count > 128)
                nonrun_count = 128;
            buf[0] = nonrun_count;
            if (!iowrite(buf, 1))
                return false;
            if (!iowrite(&data[cur], nonrun_count))
                return false;
            cur += nonrun_count;
        }
        /* write out next run if one was found */
        if (run_count >= MINRUNLENGTH) {
            buf[0] = 128 + run_count;
            buf[1] = data[beg_run];
            if (!iowrite(buf, 2))
                return false;
            cur += run_count;
        }
    }
    return true;
}



bool
HdrOutput::RGBE_WritePixels_RLE(float* data, int scanline_width,
                                int num_scanlines)
{
    if (scanline_width < 8 || scanline_width > 0x7fff)
        // run length encoding is not allowed so write flat
        return RGBE_WritePixels(data, scanline_width * num_scanlines);
    unsigned char* buffer;
    OIIO_ALLOCATE_STACK_OR_HEAP(buffer, unsigned char, scanline_width * 4);
    while (num_scanlines-- > 0) {
        unsigned char rgbe[4];
        rgbe[0] = 2;
        rgbe[1] = 2;
        rgbe[2] = scanline_width >> 8;
        rgbe[3] = scanline_width & 0xFF;
        if (!iowrite(rgbe, 4))
            return false;
        for (int i = 0; i < scanline_width; i++) {
            float2rgbe(rgbe, data);
            buffer[i]                      = rgbe[0];
            buffer[i + scanline_width]     = rgbe[1];
            buffer[i + 2 * scanline_width] = rgbe[2];
            buffer[i + 3 * scanline_width] = rgbe[3];
            data += 3;
        }
        // write out each of the four channels separately run length encoded
        // first red, then green, then blue, then exponent
        for (int i = 0; i < 4; i++) {
            if (!RGBE_WriteBytes_RLE(&buffer[i * scanline_width],
                                     scanline_width))
                return false;
        }
    }
    return true;
}



// Simple write routine that does not use run length encoding.
// These routines can be made faster by allocating a larger buffer and
// fread-ing and fwrite-ing the data in larger chunks.
bool
HdrOutput::RGBE_WritePixels(float* data, int64_t numpixels)
{
    unsigned char* rgbe;
    OIIO_ALLOCATE_STACK_OR_HEAP(rgbe, unsigned char, 4 * numpixels);
    for (int64_t i = 0; i < numpixels; ++i)
        float2rgbe(&rgbe[4 * i], data + 3 * i);
    if (!iowrite(rgbe, 4 * numpixels))
        return false;
    return true;
}



bool
HdrOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    if (!check_open(mode, newspec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 3 },
                    uint64_t(OpenChecks::Disallow1or2Channel)))
        return false;

    // HDR always behaves like floating point
    m_spec.set_format(TypeDesc::FLOAT);  // Native rgbe is float32 only

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    if (!iowritefmt("#?RADIANCE\n"))
        return false;

    // FIXME -- should we do anything about orientation, gamma, exposure,
    // software, pixaspect, primaries?

    if (!iowritefmt("FORMAT=32-bit_rle_rgbe\n\n"))
        return false;
    if (!iowritefmt("-Y {} +X {}\n", m_spec.height, m_spec.width))
        return false;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



bool
HdrOutput::write_scanline(int /*y*/, int /*z*/, TypeDesc format,
                          const void* data, stride_t xstride)
{
    data = to_native_scanline(format, data, xstride, scratch);
    return RGBE_WritePixels_RLE((float*)data, m_spec.width, 1);
}



bool
HdrOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
HdrOutput::close()
{
    if (!ioproxy_opened()) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    init();

    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
