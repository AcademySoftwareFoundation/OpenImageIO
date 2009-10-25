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


#include <cstdlib>

#include "fits_pvt.h"



// Obligatory material to make this a recognizeable imageio plugin
extern "C" {
    DLLEXPORT int fits_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *fits_input_imageio_create () {
        return new FitsInput;
    }
    DLLEXPORT const char *fits_input_extensions[] = {
        "fits", NULL
    };
}



bool
FitsInput::open (const std::string &name, ImageSpec &spec)
{
    // saving 'name' for later use
    m_filename = name;

    // checking if the file exists and can be opened in READ mode
    m_fd = fopen (m_filename.c_str (), "rb");
    if (!m_fd) {
        error ("Could not open file \"%s\"", m_filename.c_str ());
        return false;
    }

    // checking if the file is FITS file
    char magic[6] = {0};
    fread(magic, 1, 6, m_fd);
    if(strncmp (magic, "SIMPLE", 6)) {
        error ("%s isn't a FITS file", m_filename.c_str ());
        close ();
        return false;
    }
    // moving back to the start of the file
    fseek (m_fd, 0, SEEK_SET);

    subimage_search ();

    set_spec_info ();

    spec = m_spec;
    return true;
};




bool
FitsInput::read_native_scanline (int y, int z, void *data)
{
    // we return true just to support 0x0 images
    if (!m_naxes)
        return true;

    std::vector<unsigned char> data_tmp (m_spec.scanline_bytes ());
    long scanline_off = (m_spec.height - y) * m_spec.scanline_bytes ();
    fseek (m_fd, scanline_off, SEEK_CUR);
    fread (&data_tmp[0], 1, m_spec.scanline_bytes(), m_fd);

    // in FITS image data is stored in big-endian so we have to switch to
    // little-endian on little-endian machines
    if (littleendian ()) {
        if (m_spec.format == TypeDesc::USHORT)
            swap_endian ((unsigned short*)&data_tmp[0],
                         data_tmp.size () / sizeof (unsigned short));
        else if (m_spec.format == TypeDesc::UINT)
            swap_endian ((unsigned int*)&data_tmp[0],
                         data_tmp.size () / sizeof (unsigned int));
        else if (m_spec.format == TypeDesc::FLOAT)
            swap_endian ((float*)&data_tmp[0],
                         data_tmp.size () / sizeof (float));
        else if (m_spec.format == TypeDesc::DOUBLE)
            swap_endian ((double*)&data_tmp[0],
                         data_tmp.size () / sizeof (double));
    }

    memcpy (data, &data_tmp[0], data_tmp.size ());

    // after reading scanline we set file pointer to the start of image data
    fsetpos (m_fd, &m_filepos);
    return true;
};



bool
FitsInput::seek_subimage (int index, ImageSpec &newspec)
{
    if (index < 0 || index >= (int)m_subimages.size ())
        return false;

    if (index == m_cur_subimage) {
        newspec = m_spec;
        return true;
    }

    // setting file pointer to the beginning of IMAGE extension
    m_cur_subimage = index;
    fseek (m_fd, m_subimages[m_cur_subimage].offset, SEEK_SET);

    set_spec_info ();

    newspec = m_spec;
    return true;
}



void
FitsInput::set_spec_info ()
{
    keys.clear ();
    // FITS spec doesn't say anything about color space or
    // number of channels, so we read all images as if they
    // all were one-channel images
    m_spec = ImageSpec(0, 0, 1, TypeDesc::UNKNOWN);

    // reading info about current subimage
    read_fits_header ();

    // we don't deal with one dimension images
    // it's some kind of spectral data
    if (! m_spec.width || ! m_spec.height) {
        m_spec.width = m_spec.full_width = 0;
        m_spec.height = m_spec.full_height = 0;
    }

    // now we can get the current position in the file
    // this is the start of the image data
    // we will need it in the read_native_scanline method
    fgetpos(m_fd, &m_filepos);

    if (m_bitpix == 8)
        m_spec.set_format (TypeDesc::UCHAR);
    else if (m_bitpix == 16)
        m_spec.set_format (TypeDesc::USHORT);
    else if (m_bitpix == 32)
        m_spec.set_format (TypeDesc::UINT);
    else if (m_bitpix == -32)
        m_spec.set_format (TypeDesc::FLOAT);
    else if (m_bitpix == -64)
        m_spec.set_format (TypeDesc::DOUBLE);
}
    
bool
FitsInput::close (void)
{
    if (m_fd)
        fclose (m_fd);
    init ();
    return true;
}



void
FitsInput::read_fits_header (void)
{
    std::string fits_header (HEADER_SIZE, 0);

    // we read whole header at once
    fread (&fits_header[0], 1, HEADER_SIZE, m_fd);

    for (int i = 0; i < CARDS_PER_HEADER; ++i) {
        std::string card (CARD_SIZE, 0);
        // reading card number i
        memcpy (&card[0], &fits_header[i*CARD_SIZE], CARD_SIZE);

        std::string keyname, value;
        fits_pvt::unpack_card (card, keyname, value);

        // END means that this is end of the FITS header
        if (keyname == "END")
            return;

        if (keyname == "SIMPLE" || keyname == "XTENSION")
            continue;

        // setting up some important fields
        // m_bitpix - format of the data (eg. bpp)
        // m_naxes - number of axes
        // width, height and depth of the image 
        if (keyname == "BITPIX") {
            m_bitpix = atoi (&card[10]);
            continue;
        }
        if (keyname == "NAXIS") {
            m_naxes = atoi (&card[10]);
            continue;
        }
        if (keyname == "NAXIS1") {
            m_spec.width = atoi (&card[10]);
            m_spec.full_width = m_spec.width;
            continue;
        }
        if (keyname == "NAXIS2") {
            m_spec.height = atoi (&card[10]);
            m_spec.full_height = m_spec.height;
            continue;
        }
        // ignoring other axis
        if (keyname.substr (0,5) == "NAXIS") {
            continue;
        }

        if (keyname == "ORIENTAT") {
            add_to_spec ("Orientation", value);
            continue;
        }

        if (keyname == "DATE") {
            add_to_spec ("DateTime", convert_date (value));
            continue;
        }

        // Some keywords can occure more than one time: COMMENT, HISTORY,
        // HIERARCH. We can't store more than one key with the same name in
        // ImageSpec. The workaround for this is to append a num to the keyname
        // that occurs more then one time
        if (keyname == "COMMENT" || keyname == "HISTORY"
            || keyname == "HIERARCH")
            keyname += Strutil::format ("%d", keys[keyname]++);

        add_to_spec (pystring::capitalize(keyname), value);
    }
    // if we didn't found END keyword in current header, we read next one
    read_fits_header ();
}



void
FitsInput::add_to_spec (const std::string &keyname, const std::string &value)
{
    // we don't add empty keys (or keys with empty values) to ImageSpec
    if (!keyname.size() || !value.size ())
        return;

    // COMMENT, HISTORY, HIERARCH keywords we save AS-IS
    if (keyname.substr (0, 7) == "Comment" || keyname.substr (0, 7) == "History"
        ||keyname.substr (0, 8) == "Hierarch" || keyname == "DateTime") {
        m_spec.attribute (keyname, value);
        return;
    }

    // converting string to float or integer
    bool isNumSign = (value[0] == '+' || value[0] == '-' || value[0] == '.');
    if (isdigit (value[0]) || isNumSign) {
        float val = atof (value.c_str ());
        if (val == (int)val)
            m_spec.attribute (keyname, (int)val);
        else
            m_spec.attribute (keyname, val);
    }
    else
        m_spec.attribute (keyname, value);
}



void
FitsInput::subimage_search ()
{
    // saving position of the file, just for safe)
    fpos_t fpos;
    fgetpos (m_fd, &fpos);

    // starting reading headers from the beginning of the file
    fseek (m_fd, 0, SEEK_SET);

    // we search for subimages by reading whole header and checking if it
    // starts by "SIMPLE" keyword (primary header is always image header)
    // or by "XTENSION= 'IMAGE   '" (it is image extensions)
    std::string hdu (HEADER_SIZE, 0);
    size_t offset = 0;
    while (fread (&hdu[0], 1, HEADER_SIZE, m_fd) == HEADER_SIZE) {
        if (!strncmp (&hdu[0], "SIMPLE", 6) ||
            !strncmp (&hdu[0], "XTENSION= 'IMAGE   '", 20)) {
            fits_pvt::Subimage newSub;
            newSub.number = m_subimages.size ();
            newSub.offset = offset;
            m_subimages.push_back (newSub);
        }
        offset += HEADER_SIZE;
    }
    fsetpos (m_fd, &fpos);
}



std::string
FitsInput::convert_date (const std::string &date)
{
    std::string ndate;
    if (date[4] == '-') {
        // YYYY-MM-DDThh:mm:ss convention is used since 1 January 2000
        ndate = Strutil::format ("%04u:%02u:%02u", atoi(&date[0]),
                                 atoi(&date[5]), atoi(&date[8]));
        if (date.size () >= 11 && date[10] == 'T')
            ndate += Strutil::format ("%02u:%02u:%02u", atoi (&date[11]),
                                      atoi (&date[14]), atoi (&date[17]));
        return ndate;
    }

    if (date[2] == '/') {
        // DD/MM/YY convention was used before 1 January 2000
        ndate = Strutil::format ("19%02u:%02u:%02u 00:00:00", atoi(&date[6]),
                                 atoi(&date[3]), atoi(&date[0]));
        return ndate;
    }
    // unrecognized format
    return date;
}
