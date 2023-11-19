// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cassert>
#include <cstdio>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <jxl/encode.h>
#include <jxl/resizable_parallel_runner.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)


// References:
//  * https://jpegxl.info



class JxlOutput final : public ImageOutput {
public:
    JxlOutput() { fprintf(stderr, "JxlOutput()\n"); init(); }
    ~JxlOutput() override { close(); }
    const char* format_name(void) const override { return "jxl"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "ioproxy");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;
    bool copy_image(ImageInput* in) override;

private:
    std::string m_filename;

    void init(void)
    {
        ioproxy_clear();
        clear_outbuffer();
    }

    void clear_outbuffer()
    {
    }

    void set_subsampling(const int components[])
    {
    }
};



OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
jxl_output_imageio_create()
{
    return new JxlOutput;
}

OIIO_EXPORT const char* jxl_output_extensions[]
    = { "jxl", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
JxlOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    // Save name and spec for later use
    m_filename = name;

    return true;
}



bool
JxlOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    return true;
}



bool
JxlOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    return true;
}



bool
JxlOutput::close()
{
    if (!ioproxy_opened()) {  // Already closed
        init();
        return true;
    }

    bool ok = true;

    init();
    return ok;
}



bool
JxlOutput::copy_image(ImageInput* in)
{
    if (in && !strcmp(in->format_name(), "jxl")) {
        return true;
    }

    return ImageOutput::copy_image(in);
}

OIIO_PLUGIN_NAMESPACE_END
