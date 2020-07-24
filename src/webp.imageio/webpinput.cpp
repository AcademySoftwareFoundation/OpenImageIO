// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

#include <webp/decode.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace webp_pvt {


class WebpInput final : public ImageInput {
public:
    WebpInput() { init(); }
    virtual ~WebpInput() { close(); }
    virtual const char* format_name() const override { return "webp"; }
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;
    virtual bool close() override;

private:
    std::string m_filename;
    uint8_t* m_decoded_image;
    uint64_t m_image_size;
    long int m_scanline_size;
    FILE* m_file;

    void init()
    {
        m_image_size    = 0;
        m_scanline_size = 0;
        m_decoded_image = NULL;
        m_file          = NULL;
    }
};


bool
WebpInput::open(const std::string& name, ImageSpec& spec)
{
    m_filename = name;

    // Perform preliminary test on file type.
    if (!Filesystem::is_regular(m_filename)) {
        errorf("Not a regular file \"%s\"", m_filename);
        return false;
    }

    // Get file size and check we've got enough data to decode WebP.
    m_image_size = Filesystem::file_size(name);
    if (m_image_size == uint64_t(-1)) {
        errorf("Failed to get size for \"%s\"", m_filename);
        return false;
    }
    if (m_image_size < 12) {
        errorf("File size is less than WebP header for file \"%s\"",
               m_filename);
        return false;
    }

    m_file = Filesystem::fopen(m_filename, "rb");
    if (!m_file) {
        errorf("Could not open file \"%s\"", m_filename);
        return false;
    }

    // Read header and verify we've got WebP image.
    std::vector<uint8_t> image_header;
    image_header.resize(std::min(m_image_size, (uint64_t)64), 0);
    size_t numRead = fread(&image_header[0], sizeof(uint8_t),
                           image_header.size(), m_file);
    if (numRead != image_header.size()) {
        errorf("Read failure for header of \"%s\" (expected %d bytes, read %d)",
               m_filename, image_header.size(), numRead);
        close();
        return false;
    }

    int width = 0, height = 0;
    if (!WebPGetInfo(&image_header[0], image_header.size(), &width, &height)) {
        errorf("%s is not a WebP image file", m_filename);
        close();
        return false;
    }

    // Read actual data and decode.
    std::vector<uint8_t> encoded_image;
    encoded_image.resize(m_image_size, 0);
    fseek(m_file, 0, SEEK_SET);
    numRead = fread(&encoded_image[0], sizeof(uint8_t), encoded_image.size(),
                    m_file);
    if (numRead != encoded_image.size()) {
        errorf("Read failure for \"%s\" (expected %d bytes, read %d)",
               m_filename, encoded_image.size(), numRead);
        close();
        return false;
    }

    const int nchannels = 4;
    m_scanline_size     = width * nchannels;
    m_spec              = ImageSpec(width, height, nchannels, TypeDesc::UINT8);
    m_spec.attribute("oiio:ColorSpace", "sRGB");  // webp is always sRGB
    spec = m_spec;

    if (!(m_decoded_image = WebPDecodeRGBA(&encoded_image[0],
                                           encoded_image.size(), &m_spec.width,
                                           &m_spec.height))) {
        errorf("Couldn't decode %s", m_filename);
        close();
        return false;
    }


    // WebP requires unassociated alpha, and it's sRGB.
    // Handle this all by wrapping an IB around it.
    ImageSpec specwrap(m_spec.width, m_spec.height, 4, TypeUInt8);
    ImageBuf bufwrap(specwrap, m_decoded_image);
    ROI rgbroi(0, m_spec.width, 0, m_spec.height, 0, 1, 0, 3);
    ImageBufAlgo::pow(bufwrap, bufwrap, 2.2f, rgbroi);
    ImageBufAlgo::premult(bufwrap, bufwrap);
    ImageBufAlgo::pow(bufwrap, bufwrap, 1.0f / 2.2f, rgbroi);

    return true;
}


bool
WebpInput::read_native_scanline(int /*subimage*/, int /*miplevel*/, int y,
                                int /*z*/, void* data)
{
    // Not necessary to lock and seek -- no subimages in Webp, and since
    // the only thing we're doing here is a memcpy, it's already threadsafe.
    //
    // lock_guard lock (m_mutex);
    // if (! seek_subimage (subimage, miplevel))
    //     return false;

    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;
    memcpy(data, &m_decoded_image[y * m_scanline_size], m_scanline_size);
    return true;
}


bool
WebpInput::close()
{
    if (m_file) {
        fclose(m_file);
        m_file = NULL;
    }
    if (m_decoded_image) {
        free(m_decoded_image);
        m_decoded_image = NULL;
    }
    return true;
}

}  // namespace webp_pvt

// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int webp_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
webp_imageio_library_version()
{
    int v = WebPGetDecoderVersion();
    return ustring::sprintf("Webp %d.%d.%d", v >> 16, (v >> 8) & 255, v & 255)
        .c_str();
}

OIIO_EXPORT ImageInput*
webp_input_imageio_create()
{
    return new webp_pvt::WebpInput;
}

OIIO_EXPORT const char* webp_input_extensions[] = { "webp", nullptr };

OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
