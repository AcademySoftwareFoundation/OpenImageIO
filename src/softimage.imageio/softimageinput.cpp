// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "softimage_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace softimage_pvt;



class SoftimageInput final : public ImageInput {
public:
    SoftimageInput() { init(); }
    ~SoftimageInput() override { close(); }
    const char* format_name(void) const override { return "softimage"; }
    bool open(const std::string& name, ImageSpec& spec) override;
    bool close() override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

private:
    /// Resets the core data members to defaults.
    ///
    void init();
    /// Read a scanline from m_fd.
    ///
    bool read_next_scanline(void* data);
    /// Read uncompressed pixel data from m_fd.
    ///
    bool read_pixels_uncompressed(const softimage_pvt::ChannelPacket& curPacket,
                                  void* data);
    /// Read pure run length encoded pixels.
    ///
    bool
    read_pixels_pure_run_length(const softimage_pvt::ChannelPacket& curPacket,
                                void* data);
    /// Read mixed run length encoded pixels.
    ///
    bool
    read_pixels_mixed_run_length(const softimage_pvt::ChannelPacket& curPacket,
                                 void* data);

    // Name for encoding
    const char* encoding_name(int encoding);

    FILE* m_fd;
    softimage_pvt::PicFileHeader m_pic_header;
    std::vector<softimage_pvt::ChannelPacket> m_channel_packets;
    std::string m_filename;
    std::vector<fpos_t> m_scanline_markers;
};



// symbols required for OpenImageIO plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
softimage_input_imageio_create()
{
    return new SoftimageInput;
}

OIIO_EXPORT const char* softimage_input_extensions[] = { "pic", nullptr };

OIIO_PLUGIN_EXPORTS_END



void
SoftimageInput::init()
{
    m_fd = NULL;
    m_filename.clear();
    m_channel_packets.clear();
    m_scanline_markers.clear();
}



bool
SoftimageInput::open(const std::string& name, ImageSpec& spec)
{
    // Remember the filename
    m_filename = name;

    m_fd = Filesystem::fopen(m_filename, "rb");
    if (!m_fd) {
        errorfmt("Could not open file \"{}\"", name);
        return false;
    }

    // Try read the header
    if (!m_pic_header.read_header(m_fd)) {
        errorfmt("\"{}\": failed to read header", m_filename);
        close();
        return false;
    }

    // Check whether it has the pic magic number
    if (m_pic_header.magic != 0x5380f634) {
        errorfmt(
            "\"{}\" is not a Softimage Pic file, magic number of 0x{:x} is not Pic",
            m_filename, m_pic_header.magic);
        close();
        return false;
    }

    // Get the ChannelPackets
    ChannelPacket curPacket;
    int nchannels = 0;
    std::vector<std::string> encodings;
    do {
        // Read the next packet into curPacket and store it off
        if (fread(&curPacket, 1, sizeof(ChannelPacket), m_fd)
            != sizeof(ChannelPacket)) {
            errorfmt("Unexpected end of file \"{}\".", m_filename);
            close();
            return false;
        }
        m_channel_packets.push_back(curPacket);

        // Add the number of channels in this packet to nchannels
        nchannels += curPacket.channels().size();
        encodings.push_back(encoding_name(m_channel_packets.back().type));
    } while (curPacket.chained);

    // Get the depth per pixel per channel
    TypeDesc chanType = TypeDesc::UINT8;
    if (curPacket.size == 16)
        chanType = TypeDesc::UINT16;

    // Set the details in the ImageSpec
    m_spec = ImageSpec(m_pic_header.width, m_pic_header.height, nchannels,
                       chanType);
    m_spec.attribute("BitsPerSample", (int)curPacket.size);

    m_spec.attribute("softimage:compression", Strutil::join(encodings, ","));

    if (m_pic_header.comment[0] != 0) {
        char comment[80];
        Strutil::safe_strcpy(comment, m_pic_header.comment, 80);
        m_spec.attribute("ImageDescription", comment);
    }

    // Build the scanline index
    fpos_t curPos;
    fgetpos(m_fd, &curPos);
    m_scanline_markers.push_back(curPos);

    spec = m_spec;
    return true;
}



bool
SoftimageInput::read_native_scanline(int subimage, int miplevel, int y,
                                     int /*z*/, void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    bool result = false;
    if (y == (int)m_scanline_markers.size() - 1) {
        // we're up to this scanline
        result = read_next_scanline(data);

        // save the marker for the next scanline if we haven't got the who images
        if (m_scanline_markers.size() < m_pic_header.height) {
            fpos_t curPos;
            fgetpos(m_fd, &curPos);
            m_scanline_markers.push_back(curPos);
        }
    } else if (y >= (int)m_scanline_markers.size()) {
        // we haven't yet read this far
        fpos_t curPos;
        // Store the ones before this without pulling the pixels
        do {
            if (!read_next_scanline(NULL))
                return false;

            fgetpos(m_fd, &curPos);
            m_scanline_markers.push_back(curPos);
        } while ((int)m_scanline_markers.size() <= y);

        result = read_next_scanline(data);
        fgetpos(m_fd, &curPos);
        m_scanline_markers.push_back(curPos);
    } else {
        // We've already got the index for this scanline and moved past

        // Let's seek to the scanline's data
        if (fsetpos(m_fd, &m_scanline_markers[y])) {
            errorfmt("Failed to seek to scanline {} in \"{}\"", y, m_filename);
            close();
            return false;
        }

        result = read_next_scanline(data);

        // If the index isn't complete let's shift the file pointer back to the latest readline
        if (m_scanline_markers.size() < m_pic_header.height) {
            if (fsetpos(m_fd,
                        &m_scanline_markers[m_scanline_markers.size() - 1])) {
                errorfmt("Failed to restore to scanline {} in \"{}\"",
                         m_scanline_markers.size() - 1, m_filename);
                close();
                return false;
            }
        }
    }

    return result;
}



bool
SoftimageInput::close()
{
    if (m_fd) {
        fclose(m_fd);
        m_fd = NULL;
    }
    init();
    return true;
}



const char*
SoftimageInput::encoding_name(int encoding)
{
    switch (encoding & 0x3) {
    case UNCOMPRESSED: return "none";
    case PURE_RUN_LENGTH: return "rle";
    case MIXED_RUN_LENGTH: return "mixed-rle";
    default: return "unknown";
    }
}



bool
SoftimageInput::read_next_scanline(void* data)
{
    // Each scanline is stored using one or more channel packets.
    // We go through each of those to pull the data
    for (auto& cp : m_channel_packets) {
        bool ok  = false;
        int type = int(cp.type) & 0x3;
        if (type == UNCOMPRESSED) {
            ok = read_pixels_uncompressed(cp, data);
        } else if (type == PURE_RUN_LENGTH) {
            ok = read_pixels_pure_run_length(cp, data);
        } else if (type == MIXED_RUN_LENGTH) {
            ok = read_pixels_mixed_run_length(cp, data);
        }
        if (!ok) {
            errorfmt("Failed to read channel packed type {:d} from \"{}\"",
                     int(cp.type), m_filename);
            close();
            return false;
        }
    }
    return true;
}



bool
SoftimageInput::read_pixels_uncompressed(
    const softimage_pvt::ChannelPacket& curPacket, void* data)
{
    // We're going to need to use the channels more than once
    std::vector<int> channels = curPacket.channels();
    // We'll need to use the pixelChannelSize a bit
    size_t pixelChannelSize = curPacket.size / 8;

    if (data) {
        // data pointer is set so we're supposed to write data there
        uint8_t* scanlineData = (uint8_t*)data;
        for (size_t pixelX = 0; pixelX < m_pic_header.width; pixelX++) {
            for (int channel : channels) {
                for (size_t byte = 0; byte < pixelChannelSize; byte++) {
                    // Get which byte we should be placing this in depending on endianness
                    size_t curByte = byte;
                    if (littleendian())
                        curByte = ((pixelChannelSize)-1) - curByte;

                    //read the data into the correct place
                    if (fread(&scanlineData[(pixelX * pixelChannelSize
                                             * m_spec.nchannels)
                                            + (channel * pixelChannelSize)
                                            + curByte],
                              1, 1, m_fd)
                        != 1)
                        return false;
                }
            }
        }
    } else {
        // data pointer is null so we should just seek to the next scanline
        // If the seek fails return false
        if (fseek(m_fd, m_pic_header.width * pixelChannelSize * channels.size(),
                  SEEK_CUR))
            return false;
    }
    return true;
}



bool
SoftimageInput::read_pixels_pure_run_length(
    const softimage_pvt::ChannelPacket& curPacket, void* data)
{
    // How many pixels we've read so far this line
    size_t linePixelCount = 0;
    // Number of repeats of this value
    uint8_t curCount = 0;
    // We'll need to use the pixelChannelSize a bit
    size_t pixelChannelSize = curPacket.size / 8;
    // We're going to need to use the channels more than once
    std::vector<int> channels = curPacket.channels();
    // Read the pixels until we've read them all
    while (linePixelCount < m_pic_header.width) {
        // Read the repeats for the run length - return false if read fails
        if (fread(&curCount, 1, 1, m_fd) != 1)
            return false;

        if (data) {
            // data pointer is set so we're supposed to write data there
            size_t pixelSize   = pixelChannelSize * channels.size();
            uint8_t* pixelData = new uint8_t[pixelSize];
            if (fread(pixelData, pixelSize, 1, m_fd) != pixelSize)
                return false;

            // Now we've got the pixel value we need to push it into the data
            uint8_t* scanlineData = (uint8_t*)data;
            for (size_t pixelX = linePixelCount;
                 pixelX < linePixelCount + curCount; pixelX++) {
                for (size_t curChan = 0; curChan < channels.size(); curChan++) {
                    for (size_t byte = 0; byte < pixelChannelSize; byte++) {
                        // Get which byte we should be placing this in depending on endianness
                        size_t curByte = byte;
                        if (littleendian())
                            curByte = ((pixelChannelSize)-1) - curByte;

                        //put the data into the correct place
                        scanlineData[(pixelX * pixelChannelSize
                                      * m_spec.nchannels)
                                     + (channels[curChan] * pixelChannelSize)
                                     + curByte]
                            = pixelData[(curChan * pixelChannelSize) + curByte];
                    }
                }
            }
            delete[] pixelData;
        } else {
            // data pointer is null so we should just seek to the next scanline
            // If the seek fails return false
            if (fseek(m_fd, pixelChannelSize * channels.size(), SEEK_CUR))
                return false;
        }

        // Add these pixels to the current pixel count
        linePixelCount += curCount;
    }
    return true;
}



bool
SoftimageInput::read_pixels_mixed_run_length(
    const softimage_pvt::ChannelPacket& curPacket, void* data)
{
    // How many pixels we've read so far this line
    size_t linePixelCount = 0;
    // Number of repeats of this value
    uint8_t curCount = 0;
    // We'll need to use the pixelChannelSize a bit
    size_t pixelChannelSize = curPacket.size / 8;
    // We're going to need to use the channels more than once
    std::vector<int> channels = curPacket.channels();
    // Read the pixels until we've read them all
    while (linePixelCount < m_pic_header.width) {
        // Read the repeats for the run length - return false if read fails
        if (fread(&curCount, 1, 1, m_fd) != 1)
            return false;

        if (curCount < 128) {
            // It's a raw packet - so this means the count is 1 less then the actual value
            curCount++;

            // Just to be safe let's make sure this wouldn't take us
            // past the end of this scanline
            if (curCount + linePixelCount > m_pic_header.width)
                curCount = m_pic_header.width - linePixelCount;

            if (data) {
                // data pointer is set so we're supposed to write data there
                uint8_t* scanlineData = (uint8_t*)data;
                for (size_t pixelX = linePixelCount;
                     pixelX < linePixelCount + curCount; pixelX++) {
                    for (int channel : channels) {
                        for (size_t byte = 0; byte < pixelChannelSize; byte++) {
                            // Get which byte we should be placing this in depending on endianness
                            size_t curByte = byte;
                            if (littleendian())
                                curByte = ((pixelChannelSize)-1) - curByte;

                            //read the data into the correct place
                            if (fread(
                                    &scanlineData[(pixelX * pixelChannelSize
                                                   * m_spec.nchannels)
                                                  + (channel * pixelChannelSize)
                                                  + curByte],
                                    1, 1, m_fd)
                                != 1)
                                return false;
                        }
                    }
                }
            } else {
                // data pointer is null so we should just seek to the
                // next scanline If the seek fails return false.
                if (fseek(m_fd, curCount * pixelChannelSize * channels.size(),
                          SEEK_CUR))
                    return false;
            }

            // Add these pixels to the current pixel count
            linePixelCount += curCount;
        } else {
            // It's a run length encoded packet
            uint16_t longCount = 0;

            if (curCount == 128) {
                // This is a long count so the next 16bits of the file
                // are an unsigned int containing the count.  If the
                // read fails we should return false.
                if (fread(&longCount, 1, 2, m_fd) != 2)
                    return false;

                // longCount is in big endian format - if we're not
                // let's swap it
                if (littleendian())
                    OIIO::swap_endian(&longCount);
            } else {
                longCount = curCount - 127;
            }

            if (data) {
                // data pointer is set so we're supposed to write data there
                size_t pixelSize   = pixelChannelSize * channels.size();
                uint8_t* pixelData = new uint8_t[pixelSize];
                if (fread(pixelData, 1, pixelSize, m_fd) != pixelSize)
                    return false;

                // Now we've got the pixel value we need to push it into
                // the data.
                uint8_t* scanlineData = (uint8_t*)data;
                for (size_t pixelX = linePixelCount;
                     pixelX < linePixelCount + longCount; pixelX++) {
                    for (size_t curChan = 0; curChan < channels.size();
                         curChan++) {
                        for (size_t byte = 0; byte < pixelChannelSize; byte++) {
                            // Get which byte we should be placing this
                            // in depending on endianness.
                            size_t curByte = byte;
                            if (littleendian())
                                curByte = ((pixelChannelSize)-1) - curByte;

                            //put the data into the correct place
                            scanlineData[(pixelX * pixelChannelSize
                                          * m_spec.nchannels)
                                         + (channels[curChan] * pixelChannelSize)
                                         + curByte]
                                = pixelData[(curChan * pixelChannelSize)
                                            + curByte];
                        }
                    }
                }
                delete[] pixelData;
            } else {
                // data pointer is null so we should just seek to the
                // next scanline.  If the seek fails return false.
                if (fseek(m_fd, pixelChannelSize * channels.size(), SEEK_CUR))
                    return false;
            }

            // Add these pixels to the current pixel count.
            linePixelCount += longCount;
        }
    }
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
