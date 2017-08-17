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

#include <OpenImageIO/oiioversion.h>
#include "fits_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace fits_pvt;


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *fits_output_imageio_create () {
        return new FitsOutput;
    }
    OIIO_EXPORT const char *fits_output_extensions[] = {
        "fits", NULL
    };

OIIO_PLUGIN_EXPORTS_END



int
FitsOutput::supports (string_view feature) const
{
    return (feature == "multiimage"
         || feature == "alpha"
         || feature == "nchannels"
         || feature == "random_access"
         || feature == "arbitrary_metadata"
         || feature == "exif"   // Because of arbitrary_metadata
         || feature == "iptc"); // Because of arbitrary_metadata
}



bool
FitsOutput::open (const std::string &name, const ImageSpec &spec,
                  OpenMode mode)
{
    if (mode == AppendMIPLevel) {
        error ("%s does not support MIP levels", format_name());
        return false;
    }

    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;
    if (m_spec.format == TypeDesc::UNKNOWN)  // if unknown, default to float
        m_spec.set_format (TypeDesc::FLOAT);

    // checking if the file exists and can be opened in WRITE mode
    m_fd = Filesystem::fopen (m_filename, mode == AppendSubimage ? "r+b" : "wb");
    if (!m_fd) {
        error ("Unable to open file \"%s\"", m_filename.c_str());
        return false;
    }

    create_fits_header ();

    // now we can get the current position in the file
    // we will need it int the write_native_scanline method
    fgetpos(m_fd, &m_filepos);

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    return true;
}



bool
FitsOutput::write_scanline (int y, int z, TypeDesc format, const void *data,
                            stride_t xstride)
{
    if (m_spec.width == 0 && m_spec.height == 0)
        return true;
    if (y > m_spec.height) {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        close ();
        return false;
    }

    data = to_native_scanline (format, data, xstride, m_scratch);

    std::vector<unsigned char> data_tmp (m_spec.scanline_bytes (), 0);
    memcpy (&data_tmp[0], data, m_spec.scanline_bytes ());

    // computing scanline offset
    long scanline_off = (m_spec.height - y) * m_spec.scanline_bytes ();
    fseek (m_fd, scanline_off, SEEK_CUR);

    // in FITS image data is stored in big-endian so we have to switch to
    // big-endian on little-endian machines
    if (littleendian ()) {
        if (m_bitpix == 16)
            swap_endian ((unsigned short*)&data_tmp[0],
                          data_tmp.size () / sizeof (unsigned short));
        else if (m_bitpix == 32)
            swap_endian ((unsigned int*)&data_tmp[0],
                         data_tmp.size () / sizeof (unsigned int));
        else if (m_bitpix == -32)
            swap_endian ((float*)&data_tmp[0],
                         data_tmp.size () / sizeof (float));
        else if (m_bitpix == -64)
            swap_endian ((double*)&data_tmp[0],
                         data_tmp.size () / sizeof (double));
    }

    size_t byte_count = fwrite (&data_tmp[0], 1, data_tmp.size (), m_fd);

    fsetpos (m_fd, &m_filepos);

    //byte_count == data.size --> all written
    return byte_count == data_tmp.size();
}



bool
FitsOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}



bool
FitsOutput::close (void)
{
    if (! m_fd) {   // already closed
        init ();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);
    }

    fclose (m_fd);
    init ();
    return ok;
}



void
FitsOutput::create_fits_header (void)
{
    std::string header;
    create_basic_header (header);

    //we add all keywords stored in ImageSpec to the FITS file
    for (size_t i = 0; i < m_spec.extra_attribs.size (); ++i) {

        std::string keyname = m_spec.extra_attribs[i].name().string();

        std::string value;
        TypeDesc attr_format = m_spec.extra_attribs[i].type();
        if (attr_format == TypeDesc::STRING) {
            value = *(const char**)m_spec.extra_attribs[i].data();
        }
        else if (attr_format == TypeDesc::INT) {
            int val = (*(int*)m_spec.extra_attribs[i].data());
            value = num2str ((float)val);
        }
        else if (attr_format == TypeDesc::FLOAT) {
            float val = (*(float*)m_spec.extra_attribs[i].data());
            value = num2str (val);
        }

        // Comment, History and Hierarch attributes contains multiple line of
        // COMMENT, HISTORY and HIERARCH keywords, so we have to split them before
        // adding to the file
        std::vector<std::string> values;
        if (keyname == "Comment" || keyname == "History" || keyname == "Hierarch") {
            Strutil::split (value, values, m_sep);
            for (const auto& value : values)
                header += create_card (keyname, value);
            continue;
        }

        // FITS use Date keyword for dates so we convert our DateTime attribute
        // to Date format before adding it to the FITS file
        if (keyname == "DateTime") {
            keyname = "Date";
            value = Strutil::format ("%04u-%02u-%02uT%02u:%02u:%02u",
                                     atoi(&value[0]), atoi(&value[5]),
                                     atoi(&value[8]), atoi(&value[11]),
                                     atoi(&value[14]), atoi(&value[17]));
        }

        header += create_card (keyname, value);
    }

    header += "END";
    // header size must be multiple of HEADER_SIZE
    const int hsize = HEADER_SIZE - header.size () % HEADER_SIZE;
    if (hsize)
        header.resize (header.size () + hsize, ' ');

    size_t byte_count = fwrite (&header[0], 1, header.size (), m_fd);
    if (byte_count != header.size ()) {
    	// FIXME Bad Write
    	error ("Bad header write (err %d)", byte_count);
    }
}



void
FitsOutput::create_basic_header (std::string &header)
{
    // the first word in the header is SIMPLE, that informs if given
    // file is standard FITS file (T) or isn't (F)
    // we always set this value for T 
    std::string key;
    if (m_simple) {
        header += create_card ("SIMPLE", "T");
        m_simple = false;
    }
    else
        header += create_card ("XTENSION", "IMAGE   ");

    // next, we add BITPIX value that represent how many bpp we need
    switch (m_spec.format.basetype) {
        case TypeDesc::CHAR:
        case TypeDesc::UCHAR:
            m_bitpix = 8;
            break;
        case TypeDesc::SHORT:
        case TypeDesc::USHORT:
            m_bitpix = 16;
            break;
        case TypeDesc::INT:
        case TypeDesc::UINT:
            m_bitpix = 32;
            break;
        case TypeDesc::HALF:
        case TypeDesc::FLOAT:
            m_bitpix = -32;
            break;
        case TypeDesc::DOUBLE:
            m_bitpix = -64;
            break;
        default:
            m_bitpix = -32;  // punt: default to 32 bit float
            break;
    }
    header += create_card ("BITPIX", num2str (m_bitpix));

    // NAXIS inform how many dimension have the image.
    // we deal only with 2D images so this value is always set to 2
    int axes = 0;
    if (m_spec.width != 0 || m_spec.height != 0)
        axes = 2;
    header += create_card ("NAXIS", num2str (axes));

    // now we save NAXIS1 and NAXIS2
    // this keywords represents width and height
    header += create_card ("NAXIS1", num2str (m_spec.width));
    header += create_card ("NAXIS2", num2str (m_spec.height));
}

OIIO_PLUGIN_NAMESPACE_END

