/*
  Copyright 2008-2009 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_FITS_PVT_H
#define OPENIMAGEIO_FITS_PVT_H

#include <cstdio>
#include <sstream>
#include <map>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>

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

} // namespace fits_pvt



class FitsInput final : public ImageInput {
 public:
    FitsInput () { init (); }
    virtual ~FitsInput () { close (); }
    virtual const char *format_name (void) const { return "fits"; }
    virtual int supports (string_view feature) const {
        return (feature == "arbitrary_metadata"
             || feature == "exif"   // Because of arbitrary_metadata
             || feature == "iptc"); // Because of arbitrary_metadata
    }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual int current_subimage () const { return m_cur_subimage; }
 private:
    FILE *m_fd;
    std::string m_filename;
    int m_cur_subimage;
    int m_bitpix; // number of bits that represents data value;
    int m_naxes; // number of axsis of the image (e.g dimensions)
    fpos_t m_filepos; // current position in the file
    // here we store informations how many times COMMENT, HISTORY, HIERARCH
    // keywords has occured
    std::map<std::string, int> keys;
    // here we store informations about subimages,
    // eg. subimage number and subimage offset
    std::vector<fits_pvt::Subimage> m_subimages;
    // here we stores content of COMMENT, HISTORY, HIERARCH keywords. Each line
    // of comment is separated by m_sep
    std::string m_comment, m_history, m_hierarch;
    std::string m_sep;
    
    void init (void) {
        m_fd = NULL;
        m_filename.clear ();
        m_cur_subimage = 0;
        m_bitpix = 0;
        m_naxes = 0;
        m_subimages.clear ();
        m_comment.clear ();
        m_history.clear ();
        m_hierarch.clear ();
        m_sep = '\n';
    }

    // read keywords from FITS header and add them to the ImageSpec
    // sets some ImageSpec fields: width, height, depth.
    // Return true if all is ok, false if there was a read error.
    bool read_fits_header (void);

    // add keyword (with comment if exists) to the ImageSpec
    void add_to_spec (const std::string &keyname, const std::string &value);

    // search for subimages: in FITS subimage is a header with SIMPLE keyword
    // or with XTENSION keyword with value 'IMAGE   '. Information about found
    // subimages are stored in m_subimages
    void subimage_search ();

    // set basic info (width, height) of subimage
    // add attributes to ImageSpec
    // return true if ok, false upon error reading the spec from the file.
    bool set_spec_info ();

    // converts date in FITS format (YYYY-MM-DD or DD/MM/YY)
    // to DateTime format
    std::string convert_date (const std::string &date);
};



class FitsOutput final : public ImageOutput {
 public:
    FitsOutput () { init (); }
    virtual ~FitsOutput () { close (); }
    virtual const char *format_name (void) const { return "fits"; }
    virtual int supports (string_view feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close (void);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);

 private:
    FILE *m_fd;
    std::string m_filename;
    int m_bitpix; // number of bits that represents data value;
    fpos_t m_filepos; // current position in the file
    bool m_simple; // does the header with SIMPLE key was written?
    std::vector<unsigned char> m_scratch;
    std::string m_sep;
    std::vector<unsigned char> m_tilebuffer;

    void init (void) {
        m_fd = NULL;
        m_filename.clear ();
        m_bitpix = 0;
        m_simple = true;
        m_scratch.clear ();
        m_sep = '\n';
    }

    // save to FITS file all attributes from ImageSpace and after writing last
    // attribute writes END keyword
    void create_fits_header (void);

    // save to FITS file some mandatory keywords: SIMPLE, BITPIX, NAXIS, NAXIS1
    // and NAXIS2 with their values. 
    void create_basic_header (std::string &header);
};



namespace fits_pvt {

// converts given number to string
std::string num2str (float val);


// creates FITS card from given (keyname, value, comment) strings
std::string create_card (std::string keyname, std::string value);


// retrieving keyname, value and comment from the given card
void unpack_card (const std::string &card, std::string &keyname,
                  std::string &value);

} // namespace fits_pvt

OIIO_PLUGIN_NAMESPACE_END

#endif // OPENIMAGEIO_FITS_PVT_H

