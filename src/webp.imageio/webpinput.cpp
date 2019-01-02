/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
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
        error("Not a regular file \"%s\"", m_filename.c_str());
        return false;
    }

    // Get file size and check we've got enough data to decode WebP.
    m_image_size = Filesystem::file_size(name);
    if (m_image_size == uint64_t(-1)) {
        error("Failed to get size for \"%s\"", m_filename);
        return false;
    }
    if (m_image_size < 12) {
        error("File size is less than WebP header for file \"%s\"", m_filename);
        return false;
    }

    m_file = Filesystem::fopen(m_filename, "rb");
    if (!m_file) {
        error("Could not open file \"%s\"", m_filename.c_str());
        return false;
    }

    // Read header and verify we've got WebP image.
    std::vector<uint8_t> image_header;
    image_header.resize(std::min(m_image_size, (uint64_t)64), 0);
    size_t numRead = fread(&image_header[0], sizeof(uint8_t),
                           image_header.size(), m_file);
    if (numRead != image_header.size()) {
        error("Read failure for header of \"%s\" (expected %d bytes, read %d)",
              m_filename, image_header.size(), numRead);
        close();
        return false;
    }

    int width = 0, height = 0;
    if (!WebPGetInfo(&image_header[0], image_header.size(), &width, &height)) {
        error("%s is not a WebP image file", m_filename.c_str());
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
        error("Read failure for \"%s\" (expected %d bytes, read %d)",
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
        error("Couldn't decode %s", m_filename.c_str());
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
WebpInput::read_native_scanline(int subimage, int miplevel, int y, int z,
                                void* data)
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
