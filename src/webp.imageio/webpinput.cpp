// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <webp/decode.h>
#include <webp/demux.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace webp_pvt {


class WebpInput final : public ImageInput {
public:
    WebpInput() {}
    virtual ~WebpInput() { close(); }
    virtual const char* format_name() const override { return "webp"; }
    virtual int supports(string_view feature) const override
    {
        return (feature == "exif");
    }
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool seek_subimage(int subimage, int miplevel) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;
    virtual bool close() override;
    virtual int current_subimage(void) const override { return m_subimage; }

private:
    std::string m_filename;
    std::unique_ptr<uint8_t[]> m_encoded_image;
    std::unique_ptr<uint8_t> m_decoded_image;
    uint64_t m_image_size    = 0;
    long int m_scanline_size = 0;
    uint32_t m_demux_flags   = 0;
    int m_frame_count        = 1;
    WebPDemuxer* m_demux     = nullptr;
    WebPIterator m_iter;
    int m_subimage      = -1;  // Subimage we're pointed to
    int m_subimage_read = -1;  // Subimage stored in decoded_image

    // Reposition the m_iter to the desired subimage, return true for
    // success and adjust m_subimage, false for failure.
    bool iter_to_subimage(int subimage)
    {
        if (WebPDemuxGetFrame(m_demux, subimage + 1, &m_iter)) {
            m_subimage = subimage;
            return true;
        }
        return false;
    }

    // Read the pixels of the current subimage (if they aren't already),
    // return true for  success, false for failure. It's a failure if the
    // last pixels read aren't either the requested subimage or the one
    // immediately preceding it.
    bool read_current_subimage();

    // Reposition to the desired subimage and also read the pixels if `read`
    // is true. Return true for success, false for failure. This is all the
    // hard logic about how to get to the right spot if it's not the next
    // sequential frame.
    bool read_subimage(int subimage, bool read);
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
    if (m_image_size > std::numeric_limits<size_t>::max()) {
        errorf("Image size (%d) is too big to read", m_image_size);
    }

    FILE* file = Filesystem::fopen(m_filename, "rb");
    if (!file) {
        errorf("Could not open file \"%s\"", m_filename);
        return false;
    }

    // Read header and verify we've got WebP image.
    std::vector<uint8_t> image_header;
    image_header.resize(std::min(m_image_size, (uint64_t)64), 0);
    size_t numRead = fread(&image_header[0], sizeof(uint8_t),
                           image_header.size(), file);
    if (numRead != image_header.size()) {
        errorf("Read failure for header of \"%s\" (expected %d bytes, read %d)",
               m_filename, image_header.size(), numRead);
        fclose(file);
        close();
        return false;
    }

    int width = 0, height = 0;
    if (!WebPGetInfo(&image_header[0], image_header.size(), &width, &height)) {
        errorf("%s is not a WebP image file", m_filename);
        fclose(file);
        close();
        return false;
    }

    // Read actual data and decode.
    m_encoded_image.reset(new uint8_t[m_image_size]);
    fseek(file, 0, SEEK_SET);
    numRead = fread(m_encoded_image.get(), sizeof(uint8_t), m_image_size, file);
    fclose(file);
    file = nullptr;
    if (numRead != m_image_size) {
        errorf("Read failure for \"%s\" (expected %d bytes, read %d)",
               m_filename, m_image_size, numRead);
        close();
        return false;
    }

    // WebPMuxError err;
    WebPData bitstream { m_encoded_image.get(), size_t(m_image_size) };
    m_demux = WebPDemux(&bitstream);
    if (!m_demux) {
        errorf("Couldn't decode");
        close();
        return false;
    }
    uint32_t w             = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_WIDTH);
    uint32_t h             = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_HEIGHT);
    uint32_t m_demux_flags = WebPDemuxGetI(m_demux, WEBP_FF_FORMAT_FLAGS);
#if 0
    if (m_demux_flags & XMP_FLAG)
        Strutil::print("  xmp\n");
    if (m_demux_flags & EXIF_FLAG)
        Strutil::print("  EXIF\n");
    if (m_demux_flags & ICCP_FLAG)
        Strutil::print("  ICCP\n");
#endif

    m_spec = ImageSpec(w, h, (m_demux_flags & ALPHA_FLAG) ? 4 : 3, TypeUInt8);
    m_scanline_size = m_spec.scanline_bytes();
    m_spec.attribute("oiio:ColorSpace", "sRGB");  // webp is always sRGB
    if (m_demux_flags & ANIMATION_FLAG) {
        m_spec.attribute("oiio:Movie", 1);
        m_frame_count       = (int)WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT);
        uint32_t loop_count = WebPDemuxGetI(m_demux, WEBP_FF_LOOP_COUNT);
        if (loop_count)
            m_spec.attribute("webp:LoopCount", (int)loop_count);
        // uint32_t bgcolor = WebPDemuxGetI(m_demux, WEBP_FF_BACKGROUND_COLOR    );
        // Strutil::print("  animated {} frames, loop {}, bgcolor={}\n",
        //                frame_count, loop_count, bgcolor);
    } else {
        m_frame_count = 1;
    }

    WebPChunkIterator chunk_iter;
    if (m_demux_flags & EXIF_FLAG
        && WebPDemuxGetChunk(m_demux, "EXIF", 1, &chunk_iter)) {
        decode_exif(string_view((const char*)chunk_iter.chunk.bytes + 6,
                                chunk_iter.chunk.size - 6),
                    m_spec);
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }
    if (m_demux_flags & XMP_FLAG
        && WebPDemuxGetChunk(m_demux, "XMP ", 1, &chunk_iter)) {
        // FIXME: This is where we would extract XMP. Come back to this when
        // I have found an example webp containing XMP that I can use as a
        // test case, otherwise I'm just guessing.
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }
    if (m_demux_flags & ICCP_FLAG
        && WebPDemuxGetChunk(m_demux, "ICCP", 1, &chunk_iter)) {
        // FIXME: This is where we would extract an ICC profile. Come back
        // to this when I have found an example webp containing an ICC
        // profile that I can use as a test case, otherwise I'm just
        // guessing.
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }

    // Make space for the decoded image
    m_decoded_image.reset(new uint8_t[m_spec.image_bytes()]);

    seek_subimage(0, 0);
    spec = m_spec;
    return true;
}



bool
WebpInput::seek_subimage(int subimage, int miplevel)
{
    lock_guard lock(m_mutex);
    if (miplevel != 0 || subimage < 0)
        return false;

    if (subimage == m_subimage)
        return true;  // Already seeked to the requested subimage

    return read_subimage(subimage, false);
}



// Seek to the requested subimage, if read==true also make sure we have a
// proper representation of the image in decoded_image.
bool
WebpInput::read_subimage(int subimage, bool read)
{
    // Already pointed to the right place? Done.
    if (m_subimage == subimage && (!read || m_subimage_read == subimage))
        return true;

    // If we're not reading, just do the seek and we're done
    if (!read)
        return iter_to_subimage(subimage);

    // If we're pointing to (and have read) the imediately previous frame,
    // catch up.
    if (m_subimage == subimage - 1 && m_subimage_read == subimage - 1) {
        if (!iter_to_subimage(subimage))
            return false;
    }

    // If we're pointing to the right subimage, read it if we need to, and
    // we're done.
    if (m_subimage == subimage && read_current_subimage())
        return true;

    // All other cases: backtrack to the beginning and read up to where
    // we need to be.
    m_subimage      = -1;
    m_subimage_read = -1;
    while (m_subimage < subimage) {
        if (iter_to_subimage(m_subimage + 1) && read_current_subimage())
            m_subimage_read = m_subimage;
        else
            return false;
    }

    return true;

    // FIXME: this covers the common cases efficiently: sequential or random
    // access for reading just subimage metadata, and sequential access to
    // subimages for reading pixels. Random access to subimages requiring
    // pixel reads incurs lots of backtracking. It would be straightforward
    // to add the case where we randomly seek forward (just keep reading
    // until we catch up), but we can leave that until it becomes apparent
    // that this is an important performance case.
}



// Read the current subimage, if we haven't already. This fails if we need
// to perform the pixel read but m_subimage_read is not the immediately
// prior frame.
bool
WebpInput::read_current_subimage()
{
    if (m_subimage_read == m_subimage) {
        return true;  // Already read this frame's pixels
    }

    if (m_subimage_read != m_subimage - 1)
        return false;  // fail -- last read is not merely one frame behind

    uint8_t* okptr = nullptr;
    if (m_subimage == 0 || !m_iter.has_alpha) {
        // No alpha supplied (or first image) -- full overwrite
        size_t offset = (m_iter.y_offset * m_spec.width + m_iter.x_offset)
                        * m_spec.pixel_bytes();
        if (m_spec.nchannels == 3) {
            okptr = WebPDecodeRGBInto(m_iter.fragment.bytes,
                                      m_iter.fragment.size,
                                      m_decoded_image.get() + offset,
                                      m_spec.image_bytes() - offset,
                                      m_spec.scanline_bytes());
        } else {
            OIIO_DASSERT(m_spec.nchannels == 4);
            okptr = WebPDecodeRGBAInto(m_iter.fragment.bytes,
                                       m_iter.fragment.size,
                                       m_decoded_image.get() + offset,
                                       m_spec.image_bytes() - offset,
                                       m_spec.scanline_bytes());
            // WebP requires unassociated alpha, and it's sRGB.
            // Handle this all by wrapping an IB around it.
            ImageBuf fullbuf(m_spec, m_decoded_image.get());
            ImageBufAlgo::premult(fullbuf, fullbuf);
        }
    } else {
        // This subimage writes *atop* the prior image, we must composite
        ImageSpec fullspec(m_spec.width, m_spec.height, m_spec.nchannels,
                           m_spec.format);
        ImageBuf fullbuf(fullspec, m_decoded_image.get());
        ImageSpec fragspec(m_iter.width, m_iter.height, 4, TypeUInt8);
        fragspec.x = m_iter.x_offset;
        fragspec.y = m_iter.y_offset;
        ImageBuf fragbuf(fragspec);
        okptr = WebPDecodeRGBAInto(m_iter.fragment.bytes, m_iter.fragment.size,
                                   (uint8_t*)fragbuf.localpixels(),
                                   fragspec.image_bytes(),
                                   fragspec.scanline_bytes());
        // WebP requires unassociated alpha, and it's sRGB.
        // Handle this all by wrapping an IB around it.
        ImageBufAlgo::premult(fragbuf, fragbuf);
        ImageBufAlgo::over(fullbuf, fragbuf, fullbuf);
    }

    if (!okptr) {
        errorfmt("Couldn't decode subimage {}", m_subimage);
        return false;
    }

    m_subimage_read = m_subimage;
    return true;
}



bool
WebpInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                                void* data)
{
    lock_guard lock(m_mutex);
    if (!read_subimage(subimage, true))
        return false;
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;
    memcpy(data, m_decoded_image.get() + (y * m_scanline_size),
           m_scanline_size);
    return true;
}



bool
WebpInput::close()
{
    if (m_demux) {
        WebPDemuxReleaseIterator(&m_iter);
        WebPDemuxDelete(m_demux);
        m_demux = nullptr;
    }
    m_decoded_image.reset();
    m_encoded_image.reset();
    m_subimage = -1;
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
