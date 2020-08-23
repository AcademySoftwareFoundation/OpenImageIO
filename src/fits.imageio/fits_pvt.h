// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#pragma once

#include <cstdio>
#include <map>
#include <sstream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

// This represent the size of ONE header unit in FITS file.
#define HEADER_SIZE 2880
// This represent the size of ONE card unit. Card consist of
// keyname, value and optional comment
#define CARD_SIZE 80
// This represent the size of max number of cards in one header
#define CARDS_PER_HEADER 36


OIIO_PLUGIN_NAMESPACE_BEGIN


namespace fits_pvt {

// struct in which we store information about one subimage. This informations
// allow us to set up pointer at the beginning of given subimage
struct Subimage {
    int number;
    size_t offset;
};

}  // namespace fits_pvt



class FitsInput final : public ImageInput {
public:
    FitsInput() { init(); }
    virtual ~FitsInput() { close(); }
    virtual const char* format_name(void) const override { return "fits"; }
    virtual int supports(string_view feature) const override
    {
        return (feature == "arbitrary_metadata"
                || feature == "exif"    // Because of arbitrary_metadata
                || feature == "iptc");  // Because of arbitrary_metadata
    }
    virtual bool valid_file(const std::string& filename) const override;
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool close(void) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;
    virtual bool seek_subimage(int subimage, int miplevel) override;
    virtual int current_subimage() const override { return m_cur_subimage; }

private:
    FILE* m_fd;
    std::string m_filename;
    int m_cur_subimage;
    int m_bitpix;              // number of bits that represents data value;
    int m_naxes;               // number of axes of the image (e.g dimensions)
    std::vector<int> m_naxis;  // axis sizes of each dimension
    fpos_t m_filepos;          // current position in the file
    // here we store informations how many times COMMENT, HISTORY, HIERARCH
    // keywords have occurred
    std::map<std::string, int> keys;
    // here we store informations about subimages,
    // eg. subimage number and subimage offset
    std::vector<fits_pvt::Subimage> m_subimages;
    // here we stores content of COMMENT, HISTORY, HIERARCH keywords. Each line
    // of comment is separated by m_sep
    std::string m_comment, m_history, m_hierarch;
    std::string m_sep;

    void init(void)
    {
        m_fd = NULL;
        m_filename.clear();
        m_cur_subimage = 0;
        m_bitpix       = 0;
        m_naxes        = 0;
        m_subimages.clear();
        m_comment.clear();
        m_history.clear();
        m_hierarch.clear();
        m_sep = '\n';
    }

    // read keywords from FITS header and add them to the ImageSpec
    // sets some ImageSpec fields: width, height, depth.
    // Return true if all is ok, false if there was a read error.
    bool read_fits_header(void);

    // add keyword (with comment if exists) to the ImageSpec
    void add_to_spec(const std::string& keyname, const std::string& value);

    // search for subimages: in FITS subimage is a header with SIMPLE keyword
    // or with XTENSION keyword with value 'IMAGE   '. Information about found
    // subimages are stored in m_subimages
    void subimage_search();

    // set basic info (width, height) of subimage
    // add attributes to ImageSpec
    // return true if ok, false upon error reading the spec from the file.
    bool set_spec_info();

    // converts date in FITS format (YYYY-MM-DD or DD/MM/YY)
    // to DateTime format
    std::string convert_date(const std::string& date);
};



class FitsOutput final : public ImageOutput {
public:
    FitsOutput() { init(); }
    virtual ~FitsOutput() { close(); }
    virtual const char* format_name(void) const override { return "fits"; }
    virtual int supports(string_view feature) const override;
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual bool close(void) override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;

private:
    FILE* m_fd;
    std::string m_filename;
    int m_bitpix;      // number of bits that represents data value;
    fpos_t m_filepos;  // current position in the file
    bool m_simple;     // does the header with SIMPLE key was written?
    std::vector<unsigned char> m_scratch;
    std::string m_sep;
    std::vector<unsigned char> m_tilebuffer;

    void init(void)
    {
        m_fd = NULL;
        m_filename.clear();
        m_bitpix = 0;
        m_simple = true;
        m_scratch.clear();
        m_sep = '\n';
    }

    // save to FITS file all attributes from ImageSpace and after writing last
    // attribute writes END keyword
    void create_fits_header(void);

    // save to FITS file some mandatory keywords: SIMPLE, BITPIX, NAXIS, NAXIS1
    // and NAXIS2 with their values.
    void create_basic_header(std::string& header);
};



namespace fits_pvt {

// converts given number to string
std::string
num2str(float val);


// creates FITS card from given (keyname, value, comment) strings
std::string
create_card(std::string keyname, std::string value);


// retrieving keyname, value and comment from the given card
void
unpack_card(const std::string& card, std::string& keyname, std::string& value);

}  // namespace fits_pvt

OIIO_PLUGIN_NAMESPACE_END
