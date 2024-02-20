// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#define USE_GIFH 1

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>

// IOProxy equivalent of fputc
inline int
fputc(int i, OIIO::Filesystem::IOProxy* f)
{
    char c(i);
    if (f->write(&c, 1) == 1) {
        return c;
    } else {
        return EOF;
    }
}

// IOProxy equivalent of fwrite
inline size_t
fwrite(const void* ptr, size_t size, size_t nitems,
       OIIO::Filesystem::IOProxy* f)
{
    return f->write(ptr, size * nitems);
}

// IOProxy equivalent of fputs
inline int
fputs(OIIO::string_view s, OIIO::Filesystem::IOProxy* f)
{
    return f->write(s) == s.size() ? 0 : EOF;
}

// IOProxy equivalent of fclose (do nothing)
inline int
fclose(OIIO::Filesystem::IOProxy* f)
{
    // f->close();   // not necessary
    return 0;
}

// namespace {
// #define GIF_TEMP_MALLOC malloc
// #define GIF_TEMP_FREE free
// #define GIF_MALLOC malloc
// #define GIF_FREE free
#include "gif.h"
// }  // namespace


OIIO_PLUGIN_NAMESPACE_BEGIN

class GIFOutput final : public ImageOutput {
public:
    GIFOutput() { init(); }
    ~GIFOutput() override { close(); }
    const char* format_name(void) const override { return "gif"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "random_access"
                || feature == "multiimage" || feature == "appendsubimage"
                || feature == "ioproxy");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool open(const std::string& name, int subimages,
              const ImageSpec* specs) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool close() override;

private:
    std::string m_filename;
    int m_subimage;  // Current subimage index
    int m_nsubimages;
    bool m_pending_write;                    // Do we have an image buffered?
    std::vector<ImageSpec> m_subimagespecs;  // Saved subimage specs
    GifWriter<Filesystem::IOProxy> m_gifwriter;
    std::vector<uint8_t> m_canvas;  // Image canvas, accumulating output
    int m_delay;

    void init(void)
    {
        m_filename.clear();
        m_subimage = 0;
        m_canvas.clear();
        m_pending_write = false;
        ioproxy_clear();
    }

    bool start_subimage(const ImageSpec& spec);
    bool finish_subimage();
};



OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
gif_output_imageio_create()
{
    return new GIFOutput;
}

OIIO_EXPORT const char* gif_output_extensions[] = { "gif", NULL };

OIIO_PLUGIN_EXPORTS_END



bool
GIFOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    if (mode == Create) {
        return open(name, 1, &newspec);
    }

    if (mode == AppendMIPLevel) {
        errorfmt("{} does not support MIP levels", format_name());
        return false;
    }

    if (mode == AppendSubimage) {
        if (m_pending_write)
            finish_subimage();
        ++m_subimage;
        return start_subimage(newspec);
    }

    OIIO_ASSERT_MSG(0, "Unknown open mode %d", int(mode));
    return false;
}



bool
GIFOutput::open(const std::string& name, int subimages, const ImageSpec* specs)
{
    if (subimages < 1) {
        errorfmt("{} does not support {} subimages.", format_name(), subimages);
        return false;
    }

    m_filename   = name;
    m_subimage   = 0;
    m_nsubimages = subimages;
    m_subimagespecs.assign(specs, specs + subimages);
    float fps = m_spec.get_float_attribute("FramesPerSecond", 1.0f);
    m_delay   = (fps == 0.0f ? 0 : (int)(100.0f / fps));

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    return start_subimage(specs[0]);
}



bool
GIFOutput::close()
{
    bool ok = true;
    if (m_pending_write) {
        ok &= finish_subimage();
        GifEnd(&m_gifwriter);
    }
    init();
    return ok;
}



bool
GIFOutput::start_subimage(const ImageSpec& spec)
{
    if (!check_open(Create, spec, { 0, 65535, 0, 65535, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow1or2Channel)))
        return false;

    m_spec.set_format(TypeDesc::UINT8);  // GIF is only 8 bit

    if (m_subimage == 0) {
        m_gifwriter.f = ioproxy();
        bool ok       = GifBegin(&m_gifwriter, m_filename.c_str(), m_spec.width,
                                 m_spec.height, m_delay, 8 /*bit depth*/,
                                 true /*dither*/);
        if (!ok) {
            errorfmt("Could not open \"{}\"", m_filename);
            return false;
        }
    }
    m_canvas.clear();
    m_canvas.resize(size_t(m_spec.image_pixels() * 4), 255);

    m_pending_write = true;
    return true;
}



bool
GIFOutput::finish_subimage()
{
    if (!m_pending_write)
        return true;

    bool ok         = GifWriteFrame(&m_gifwriter, &m_canvas[0], spec().width,
                                    spec().height, m_delay, 8 /*bitdepth*/,
                                    true /*dither*/);
    m_pending_write = false;
    return ok;
}



bool
GIFOutput::write_scanline(int y, int /*z*/, TypeDesc format, const void* data,
                          stride_t xstride)
{
    return convert_image(spec().nchannels, spec().width, 1 /*1 scanline*/, 1,
                         data, format, xstride, AutoStride, AutoStride,
                         &m_canvas[y * spec().width * 4], TypeDesc::UINT8, 4,
                         AutoStride, AutoStride);
}


OIIO_PLUGIN_NAMESPACE_END
