// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

OIIO_PLUGIN_NAMESPACE_BEGIN


// Docs reminders:
// https://en.wikipedia.org/wiki/BMP_file_format
// https://web.archive.org/web/20150127132443/https://forums.adobe.com/message/3272950


namespace bmp_pvt {

// size of the BMP file header (the first header that occur in BMP file)
const int BMP_HEADER_SIZE = 14;

// sizes of various DIB headers
const int OS2_V1        = 12;
const int WINDOWS_V3    = 40;
const int UNDOCHEADER52 = 52;  // 0x34
const int UNDOCHEADER56 = 56;  // 0x38
const int WINDOWS_V4    = 108;
const int WINDOWS_V5    = 124;

// bmp magic numbers
const int16_t MAGIC_BM = 0x4D42;
const int16_t MAGIC_BA = 0x4142;
const int16_t MAGIC_CI = 0x4943;
const int16_t MAGIC_CP = 0x5043;
const int16_t MAGIC_PT = 0x5450;

const int32_t NO_COMPRESSION   = 0;  // BI_RGB
const int32_t RLE8_COMPRESSION = 1;  // BI_RLE8
const int32_t RLE4_COMPRESSION = 2;  // BI_RLE4

enum class CSType {
    CalibratedRGB       = 0,
    DeviceDependentRGB  = 1,
    DeviceDependentCMYK = 2
};



// store information about BMP file
class BmpFileHeader {
public:
    // reads information about BMP file
    bool read_header(Filesystem::IOProxy* fd);

    // writes information about bmp file to given file
    bool write_header(Filesystem::IOProxy* fd);

    // return true if given file is BMP file
    bool isBmp() const;

    int16_t magic;   // used to identify BMP file
    int32_t fsize;   // size of the BMP file
    int16_t res1;    // reserved
    int16_t res2;    // reserved
    int32_t offset;  // offset of image data (pixels)
private:
    void swap_endian(void);
};

// stores information about bitmap
class DibInformationHeader {
public:
    // reads information about bitmap
    bool read_header(Filesystem::IOProxy* fd);

    // writes information about bitmap
    bool write_header(Filesystem::IOProxy* fd);

    int32_t size;     // size of the header
    int32_t width;    // bitmap width in pixels
    int32_t height;   // bitmap height in pixels
    int16_t cplanes;  // number of color planes - always 1
    int16_t bpp;      // number of bits per pixel, image color depth

    // Added after Version 1 of the format
    int32_t compression = 0;  // compression used in file
    int32_t isize       = 0;  // size of the raw image data
    int32_t hres        = 0;  // horizontal resolution in pixels per meter
    int32_t vres        = 0;  // vertical resolutions in pixels per meter
    int32_t cpalete     = 0;  // number of entries in the color palette
    int32_t important   = 0;  // number of important color used,
                              // 0 - all colors are important,
                              // in most cases ignored

    // added in Version 4 of the format
    int32_t red_mask   = 0;
    int32_t blue_mask  = 0;
    int32_t green_mask = 0;
    int32_t alpha_mask = 0;
    int32_t cs_type    = 0;  // color space type
    int32_t red_x      = 0;
    int32_t red_y      = 0;
    int32_t red_z      = 0;
    int32_t green_x    = 0;
    int32_t green_y    = 0;
    int32_t green_z    = 0;
    int32_t blue_x     = 0;
    int32_t blue_y     = 0;
    int32_t blue_z     = 0;
    int32_t gamma_x    = 0;
    int32_t gamma_y    = 0;
    int32_t gamma_z    = 0;

    // added in Version 5 of the format
    int32_t intent;
    int32_t profile_data;
    int32_t profile_size;
    int32_t reserved;

private:
    void swap_endian(void);
};

struct color_table {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t unused;
};

}  //namespace bmp_pvt



OIIO_PLUGIN_NAMESPACE_END
