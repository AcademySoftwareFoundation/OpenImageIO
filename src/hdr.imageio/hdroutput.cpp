// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <cassert>
#include <cstdio>
#include <iostream>

#include "rgbe.h"
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

class HdrOutput final : public ImageOutput {
public:
    HdrOutput() { init(); }
    virtual ~HdrOutput() { close(); }
    virtual const char* format_name(void) const override { return "hdr"; }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode) override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool close() override;

private:
    FILE* m_fd;
    std::vector<unsigned char> scratch;
    std::string rgbe_error;  // Buffer for RGBE library error msgs
    std::vector<unsigned char> m_tilebuffer;

    void init(void)
    {
        m_fd = NULL;
        rgbe_error.clear();
    }
};


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
hdr_output_imageio_create()
{
    return new HdrOutput;
}

OIIO_EXPORT const char* hdr_output_extensions[] = { "hdr", "rgbe", nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
HdrOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    if (mode != Create) {
        errorf("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // Save spec for later use
    m_spec = newspec;
    // HDR always behaves like floating point
    m_spec.set_format(TypeDesc::FLOAT);

    // Check for things HDR can't support
    if (m_spec.nchannels != 3) {
        errorf("HDR can only support 3-channel images");
        return false;
    }
    if (m_spec.width < 1 || m_spec.height < 1) {
        errorf("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        errorf("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    m_spec.set_format(TypeDesc::FLOAT);  // Native rgbe is float32 only

    m_fd = Filesystem::fopen(name, "wb");
    if (m_fd == NULL) {
        errorf("Could not open \"%s\"", name);
        return false;
    }

    rgbe_header_info h;
    h.valid = 0;

    // Most readers seem to think that rgbe files are valid only if they
    // identify themselves as from "RADIANCE".
    h.valid |= RGBE_VALID_PROGRAMTYPE;
    Strutil::safe_strcpy(h.programtype, "RADIANCE", sizeof(h.programtype));

    ParamValue* p;
    p = m_spec.find_attribute("Orientation", TypeDesc::INT);
    if (p) {
        h.valid |= RGBE_VALID_ORIENTATION;
        h.orientation = *(int*)p->data();
    }

    // FIXME -- should we do anything about gamma, exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    int r = RGBE_WriteHeader(m_fd, m_spec.width, m_spec.height, &h, rgbe_error);
    if (r != RGBE_RETURN_SUCCESS)
        errorf("%s", rgbe_error);

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
    data  = to_native_scanline(format, data, xstride, scratch);
    int r = RGBE_WritePixels_RLE(m_fd, (float*)data, m_spec.width, 1,
                                 rgbe_error);
    if (r != RGBE_RETURN_SUCCESS)
        errorf("%s", rgbe_error);
    return (r == RGBE_RETURN_SUCCESS);
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
    if (!m_fd) {  // already closed
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

    fclose(m_fd);
    m_fd = NULL;
    init();

    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
