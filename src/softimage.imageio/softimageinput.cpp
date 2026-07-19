// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstring>

#include "softimage_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace softimage_pvt;



namespace {

// Decode n (1 or 2) on-disk big-endian bytes into a native integer.
inline uint16_t
assemble_be(const uint8_t* bytes, size_t n)
{
    return n == 2 ? uint16_t((uint16_t(bytes[0]) << 8) | bytes[1]) : bytes[0];
}


// Widen 8-bit to 16-bit via exact bit replication (v*257, since
// 255*257==65535); the only possible promotion, since packet depths are
// only ever 8 or 16 bits.
inline uint16_t
promote_depth(uint16_t value, size_t packetBytes, size_t storageBytes)
{
    return packetBytes == storageBytes ? value : uint16_t(value * 257);
}


// Store storageBytes of value at dst in native machine byte order.
inline void
store_native(uint8_t* dst, uint16_t value, size_t storageBytes)
{
    if (storageBytes == 1)
        dst[0] = uint8_t(value);
    else
        memcpy(dst, &value, sizeof(uint16_t));
}

}  // namespace



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
    // Maps absolute channel index (0=R,1=G,2=B,3=A) to sequential output
    // channel number in the ImageSpec.  Initialized to -1 (unused).
    int m_channel_map[4];
    // Byte offset of each absolute channel within a pixel.  Valid only
    // where m_channel_map is >= 0.
    size_t m_channel_byte_offset[4];
    // Uniform storage size (bytes) for all channels: the widest packet
    // depth in the file.  Narrower channels are promoted to it on read.
    size_t m_storage_bytes;
    // Native bytes per pixel (nchannels * m_storage_bytes).
    size_t m_pixel_bytes;
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
    std::fill(m_channel_map, m_channel_map + 4, -1);
    std::fill(m_channel_byte_offset, m_channel_byte_offset + 4, size_t(0));
    m_storage_bytes = 0;
    m_pixel_bytes   = 0;
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
    std::vector<std::string> encodings;
    do {
        // Read the next packet into curPacket and store it off
        if (fread(&curPacket, 1, sizeof(ChannelPacket), m_fd)
            != sizeof(ChannelPacket)) {
            errorfmt("Unexpected end of file \"{}\".", m_filename);
            close();
            return false;
        }
        // Some validity checking
        if (curPacket.size != 8 && curPacket.size != 16) {
            errorfmt("Unsupported bits per channel {}", curPacket.size);
            close();
            return false;
        }
        if (curPacket.channelCode == 0) {
            errorfmt("Channel packet with no channels");
            close();
            return false;
        }
        m_channel_packets.push_back(curPacket);

        encodings.push_back(encoding_name(m_channel_packets.back().type));

        if (m_channel_packets.size() > 4) {
            errorfmt("Too many channel packets");
            close();
            return false;
        }
    } while (curPacket.chained);

    // Build channel map: absolute RGBA index -> sequential output channel.
    // Packets may have different bit depths; rather than expose that as
    // per-channel formats (a legacy format isn't worth the resulting
    // non-uniform, potentially misaligned pixel layout), we always store
    // a single uniform format -- the widest depth present -- and promote
    // narrower channels (see promote_depth()) as they're read.
    int nchannels   = 0;
    TypeDesc widest = TypeDesc::UINT8;
    for (auto& cp : m_channel_packets) {
        if (cp.size == 16)
            widest = TypeDesc::UINT16;
        for (int ch : cp.channels())
            if (m_channel_map[ch] == -1)
                m_channel_map[ch] = nchannels++;
    }

    // Set the details in the ImageSpec
    m_spec = ImageSpec(m_pic_header.width, m_pic_header.height, nchannels,
                       widest);

    if (!check_open(m_spec, { 0, 65535, 0, 65535, 0, 1, 0, 4 })) {
        close();
        return false;
    }

    // Precompute the uniform, naturally-aligned byte layout.
    m_storage_bytes = widest.size();
    m_pixel_bytes   = nchannels * m_storage_bytes;
    for (int ch = 0; ch < 4; ++ch)
        if (m_channel_map[ch] >= 0)
            m_channel_byte_offset[ch] = m_channel_map[ch] * m_storage_bytes;

    m_spec.attribute("BitsPerSample", (int)(widest.size() * 8));

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
    if (y < 0 || y >= m_spec.height)
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
        } else {
            errorfmt("Unsupported channel packet encoding {:d} in \"{}\"",
                     int(cp.type), m_filename);
            close();
            return false;
        }
        if (!ok) {
            errorfmt("Failed to read channel packet type {:d} from \"{}\"",
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
                uint8_t raw[2] = { 0, 0 };
                if (fread(raw, 1, pixelChannelSize, m_fd) != pixelChannelSize)
                    return false;
                uint16_t value
                    = promote_depth(assemble_be(raw, pixelChannelSize),
                                    pixelChannelSize, m_storage_bytes);
                store_native(&scanlineData[(pixelX * m_pixel_bytes)
                                           + m_channel_byte_offset[channel]],
                             value, m_storage_bytes);
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
    // Allocate space for a pixel to read into
    size_t pixelSize   = pixelChannelSize * channels.size();
    uint8_t* pixelData = OIIO_ALLOCA(uint8_t, pixelSize);
    // Read the pixels until we've read them all
    while (linePixelCount < m_pic_header.width) {
        // Read the repeats for the run length - return false if read fails
        if (fread(&curCount, 1, 1, m_fd) != 1)
            return false;

        // Zero-length run is malformed
        if (curCount == 0) {
            errorfmt("Invalid RLE data");
            return false;
        }

        // Clamp to avoid writing past the end of the scanline buffer
        if (linePixelCount + curCount > m_pic_header.width)
            curCount = m_pic_header.width - linePixelCount;

        if (data) {
            // data pointer is set so we're supposed to write data there
            if (fread(pixelData, 1, pixelSize, m_fd) != pixelSize)
                return false;

            // Decode and promote each channel's value once per run, then
            // repeat it for each pixel in the run.
            uint16_t chanValues[4];
            for (size_t curChan = 0; curChan < channels.size(); curChan++)
                chanValues[curChan] = promote_depth(
                    assemble_be(pixelData + curChan * pixelChannelSize,
                                pixelChannelSize),
                    pixelChannelSize, m_storage_bytes);

            uint8_t* scanlineData = (uint8_t*)data;
            for (size_t pixelX = linePixelCount;
                 pixelX < linePixelCount + curCount; pixelX++) {
                for (size_t curChan = 0; curChan < channels.size(); curChan++)
                    store_native(
                        &scanlineData[(pixelX * m_pixel_bytes)
                                      + m_channel_byte_offset[channels[curChan]]],
                        chanValues[curChan], m_storage_bytes);
            }
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
    // Allocate space for a pixel to read into
    size_t pixelSize   = pixelChannelSize * channels.size();
    uint8_t* pixelData = OIIO_ALLOCA(uint8_t, pixelSize);
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
                        uint8_t raw[2] = { 0, 0 };
                        if (fread(raw, 1, pixelChannelSize, m_fd)
                            != pixelChannelSize)
                            return false;
                        uint16_t value
                            = promote_depth(assemble_be(raw, pixelChannelSize),
                                            pixelChannelSize, m_storage_bytes);
                        store_native(
                            &scanlineData[(pixelX * m_pixel_bytes)
                                          + m_channel_byte_offset[channel]],
                            value, m_storage_bytes);
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

            // Zero-length run is malformed
            if (longCount == 0) {
                errorfmt("Invalid RLE data");
                return false;
            }

            // Clamp to avoid writing past the end of the scanline buffer
            if (linePixelCount + longCount > m_pic_header.width)
                longCount = m_pic_header.width - linePixelCount;

            if (data) {
                // data pointer is set so we're supposed to write data there
                if (fread(pixelData, 1, pixelSize, m_fd) != pixelSize)
                    return false;

                // Decode and promote each channel's value once per run,
                // then repeat it for each pixel in the run.
                uint16_t chanValues[4];
                for (size_t curChan = 0; curChan < channels.size(); curChan++)
                    chanValues[curChan] = promote_depth(
                        assemble_be(pixelData + curChan * pixelChannelSize,
                                    pixelChannelSize),
                        pixelChannelSize, m_storage_bytes);

                uint8_t* scanlineData = (uint8_t*)data;
                for (size_t pixelX = linePixelCount;
                     pixelX < linePixelCount + longCount; pixelX++) {
                    for (size_t curChan = 0; curChan < channels.size();
                         curChan++)
                        store_native(
                            &scanlineData
                                [(pixelX * m_pixel_bytes)
                                 + m_channel_byte_offset[channels[curChan]]],
                            chanValues[curChan], m_storage_bytes);
                }
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
