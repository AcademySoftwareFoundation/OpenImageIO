// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#pragma once


/*
  Brief documentation about the RLA format:

  * The file consists of multiple subimages, merely concatenated together.
    Each subimage starts with a RLAHeader, and within the header is a
    NextOffset field that gives the absolute offset (relative to the start
    of the file) of the beginning of the next subimage, or 0 if there
    is no next subimage.

  * Immediately following the header is the scanline offset table, which
    is one uint32 for each scanline, giving the absolute offset for the
    beginning of that scanline record.  By convention, RLA scanline 0 is
    displayed at the bottom of the image (the opposite of OIIO convention).

  * Each scanline consists of up to three channel groups, concatenated
    together: color, then matte, then auxiliary.  Each group may have a
    different data type and bit depth.

  * A channel group consists of its channels (separate, non-interleaved)
    concatenated together.

  * A channel is stored in an RLE record, which consists of a uint16
    given the length of encoded data, then the encoded data run.

  * The encoded data run consists of a signed "count" byte.  If the
    count >= 0, the next byte is the pixel value, which is be repeated
    count+1 times as output.  If count < 0, then the next abs(count)
    bytes should be copied directly to as output.

  * For SHORT (16 bit), LONG (32 bit), or FLOAT pixel data types, 
    the most significant 8 bits of each pixel come first, then the
    next less significant 8 bits, and so on.  For example, for 16 bit
    data (HL), the sequence will be H0 H1 H2 ... L0 L1 L2 ...
    Therefore, the bytes will need to be re-interleaved to form 
    contiguous 16 or 32 bit values in the output buffer.

  * But float data is not RLE compressed, instead just dumped raw after
    the RLE length.  Well, at least according to old code at SPI.  We
    have no original RLA specification that stipulates this to be the
    case.

  * RLA files are "big endian" for all 16 and 32 bit data: header fields,
    offsets, and pixel data.

 */


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace RLA_pvt {

// code below adapted from
// http://www.fileformat.info/format/wavefrontrla/egff.htm
struct RLAHeader {
    int16_t WindowLeft;          // Left side of the full image
    int16_t WindowRight;         // Right side of the full image
    int16_t WindowBottom;        // Bottom of the full image
    int16_t WindowTop;           // Top of the full image
    int16_t ActiveLeft;          // Left side of the viewable image
    int16_t ActiveRight;         // Right side of viewable image
    int16_t ActiveBottom;        // Bottom of the viewable image
    int16_t ActiveTop;           // Top of the viewable image
    int16_t FrameNumber;         // Frame sequence number
    int16_t ColorChannelType;    // Data format of the image channels
    int16_t NumOfColorChannels;  // Number of color channels in image
    int16_t NumOfMatteChannels;  // Number of matte channels in image
    int16_t NumOfAuxChannels;    // Number of auxiliary channels in image
    int16_t Revision;            // File format revision number
    char Gamma[16];              // Gamma setting of image
    char RedChroma[24];          // Red chromaticity
    char GreenChroma[24];        // Green chromaticity
    char BlueChroma[24];         // Blue chromaticity
    char WhitePoint[24];         // White point chromaticity*/
    int32_t JobNumber;           // Job number ID of the file
    char FileName[128];          // Image file name
    char Description[128];       // Description of the file contents
    char ProgramName[64];        // Name of the program that created the file
    char MachineName[32];        // Name of machine used to create the file
    char UserName[32];           // Name of user who created the file
    char DateCreated[20];        // Date the file was created
    char Aspect[24];             // Aspect format of the image
    char AspectRatio[8];         // Aspect ratio of the image
    char ColorChannel[32];       // Format of color channel data
    int16_t FieldRendered;       // Image contains field-rendered data
    char Time[12];               // Length of time used to create the image file
    char Filter[32];             // Name of post-processing filter
    int16_t NumOfChannelBits;    // Number of bits in each color channel pixel
    int16_t MatteChannelType;    // Data format of the matte channels
    int16_t NumOfMatteBits;      // Number of bits in each matte channel pixel
    int16_t AuxChannelType;      // Data format of the auxiliary channels
    int16_t NumOfAuxBits;  // Number of bits in each auxiliary channel pixel
    char AuxData[32];      // Auxiliary channel data description
    char Reserved[36];     // Unused
    int32_t NextOffset;    // Location of the next image header in the file

    void rla_swap_endian()
    {
        if (littleendian()) {
            // RLAs are big-endian
            swap_endian(&WindowLeft);
            swap_endian(&WindowRight);
            swap_endian(&WindowBottom);
            swap_endian(&WindowTop);
            swap_endian(&ActiveLeft);
            swap_endian(&ActiveRight);
            swap_endian(&ActiveBottom);
            swap_endian(&ActiveTop);
            swap_endian(&FrameNumber);
            swap_endian(&ColorChannelType);
            swap_endian(&NumOfColorChannels);
            swap_endian(&NumOfMatteChannels);
            swap_endian(&NumOfAuxChannels);
            swap_endian(&Revision);
            swap_endian(&JobNumber);
            swap_endian(&FieldRendered);
            swap_endian(&NumOfChannelBits);
            swap_endian(&MatteChannelType);
            swap_endian(&NumOfMatteBits);
            swap_endian(&AuxChannelType);
            swap_endian(&NumOfAuxBits);
            swap_endian(&NextOffset);
        }
    }
};

/// format of data
enum rla_channel_type { CT_BYTE = 0, CT_WORD = 1, CT_DWORD = 2, CT_FLOAT = 4 };

inline rla_channel_type
rla_type(TypeDesc t)
{
    if (t == TypeDesc::UINT16)
        return CT_WORD;
    if (t == TypeDesc::UINT32)
        return CT_DWORD;
    if (t == TypeDesc::FLOAT)
        return CT_FLOAT;
    return CT_BYTE;
}



/// Version of snprintf that is type safe and locale independent.
template<typename... Args>
inline int
safe_snprintf(char* str, size_t size, const char* fmt, const Args&... args)
{
    std::string s = Strutil::sprintf(fmt, args...);
    return snprintf(str, size, "%s", s.c_str());
}


}  // namespace RLA_pvt



OIIO_PLUGIN_NAMESPACE_END
