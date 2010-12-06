/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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

#include <cassert>
#include <cstdio>

extern "C" {
#include "jpeglib.h"
}

#include "imageio.h"
#include "fmath.h"
#include "jpeg_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace OpenImageIO;
using namespace Jpeg_imageio_pvt;


// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b

// N.B. The class definition for JpgInput is in jpeg_pvt.h.


// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT int jpeg_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *jpeg_input_imageio_create () {
        return new JpgInput;
    }
    DLLEXPORT const char *jpeg_input_extensions[] = {
        "jpg", "jpe", "jpeg", NULL
    };

OIIO_PLUGIN_EXPORTS_END


bool
JpgInput::open (const std::string &name, ImageSpec &newspec,
                const ImageSpec &config)
{
    const ImageIOParameter *p = config.find_attribute ("_jpeg:raw",
                                                       TypeDesc::TypeInt);
    m_raw = p && *(int *)p->data();
    return open (name, newspec);
}



bool
JpgInput::open (const std::string &name, ImageSpec &newspec)
{
    // Check that file exists and can be opened
    m_filename = name;
    m_fd = fopen (name.c_str(), "rb");
    if (m_fd == NULL) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // Check magic number to assure this is a JPEG file
    int magic = 0;
    if (fread (&magic, 4, 1, m_fd) != 1) {
        error ("Empty file");
        return false;
    }

    rewind (m_fd);
    const int JPEG_MAGIC = 0xffd8ffe0, JPEG_MAGIC_OTHER_ENDIAN =  0xe0ffd8ff;
    const int JPEG_MAGIC2 = 0xffd8ffe1, JPEG_MAGIC2_OTHER_ENDIAN =  0xe1ffd8ff;
    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        fclose (m_fd);
        m_fd = NULL;
        error ("\"%s\" is a JPEG file, magic number doesn't match", name.c_str());
        return false;
    }

    m_cinfo.err = jpeg_std_error (&m_jerr);
    jpeg_create_decompress (&m_cinfo);          // initialize decompressor
    jpeg_stdio_src (&m_cinfo, m_fd);            // specify the data source

    // Request saving of EXIF and other special tags for later spelunking
    for (int mark = 0;  mark < 16;  ++mark)
        jpeg_save_markers (&m_cinfo, JPEG_APP0+mark, 0xffff);
    jpeg_save_markers (&m_cinfo, JPEG_COM, 0xffff);     // comment marker

    jpeg_read_header (&m_cinfo, FALSE);         // read the file parameters
    if (m_raw)
        m_coeffs = jpeg_read_coefficients (&m_cinfo);
    else
        jpeg_start_decompress (&m_cinfo);       // start working
    m_next_scanline = 0;                        // next scanline we'll read

    m_spec = ImageSpec (m_cinfo.output_width, m_cinfo.output_height,
                        m_cinfo.output_components, TypeDesc::UINT8);

    // Assume JPEG is in sRGB unless the Exif or XMP tags say otherwise.
    m_spec.linearity = ImageSpec::sRGB;

    for (jpeg_saved_marker_ptr m = m_cinfo.marker_list;  m;  m = m->next) {
        if (m->marker == (JPEG_APP0+1) &&
                ! strcmp ((const char *)m->data, "Exif"))
            decode_exif ((unsigned char *)m->data, m->data_length, m_spec);
        else if (m->marker == (JPEG_APP0+1) &&
                 ! strcmp ((const char *)m->data, "http://ns.adobe.com/xap/1.0/")) {
#ifdef DEBUG
            std::cerr << "Found APP1 XMP! length " << m->data_length << "\n";
#endif
            std::string xml ((const char *)m->data, m->data_length);
            OpenImageIO::decode_xmp (xml, m_spec);
        }
        else if (m->marker == (JPEG_APP0+13) &&
                ! strcmp ((const char *)m->data, "Photoshop 3.0"))
            jpeg_decode_iptc ((unsigned char *)m->data);
        else if (m->marker == JPEG_COM) {
            if (! m_spec.find_attribute ("ImageDescription", TypeDesc::STRING))
                m_spec.attribute ("ImageDescription",
                                  std::string ((const char *)m->data));
        }
    }

    newspec = m_spec;
    return true;
}



bool
JpgInput::read_native_scanline (int y, int z, void *data)
{
    if (m_raw)
        return false;
    if (y < 0 || y >= (int)m_cinfo.output_height)   // out of range scanline
        return false;
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        ImageSpec dummyspec;
        int subimage = current_subimage();
        if (! close ()  ||
            ! open (m_filename, dummyspec)  ||
            ! seek_subimage (subimage, dummyspec))
            return false;    // Somehow, the re-open failed
        assert (m_next_scanline == 0 && current_subimage() == subimage);
    }
    while (m_next_scanline <= y) {
        // Keep reading until we're read the scanline we really need
        jpeg_read_scanlines (&m_cinfo, (JSAMPLE **)&data, 1); // read one scanline
        ++m_next_scanline;
    }
    return true;
}



bool
JpgInput::close ()
{
    if (m_fd != NULL) {
        // N.B. don't call finish_decompress if we never read anything
        if (m_next_scanline > 0) {
            // But if we've only read some scanlines, read the rest to avoid
            // errors
            std::vector<char> buf (spec().scanline_bytes());
            char *data = &buf[0];
            while (m_next_scanline < spec().height) {
                jpeg_read_scanlines (&m_cinfo, (JSAMPLE **)&data, 1);
                ++m_next_scanline;
            }
        }
        if (m_next_scanline > 0 || m_raw)
            jpeg_finish_decompress (&m_cinfo);
        jpeg_destroy_decompress (&m_cinfo);
        fclose (m_fd);
        m_fd = NULL;
    }
    init ();   // Reset to initial state
    return true;
}



void
JpgInput::jpeg_decode_iptc (const unsigned char *buf)
{
    // APP13 blob doesn't have to be IPTC info.  Look for the IPTC marker,
    // which is the string "Photoshop 3.0" followed by a null character.
    if (strcmp ((const char *)buf, "Photoshop 3.0"))
        return;
    buf += strlen("Photoshop 3.0") + 1;

    // Next are the 4 bytes "8BIM"
    if (strncmp ((const char *)buf, "8BIM", 4))
        return;
    buf += 4;

    // Next two bytes are the segment type, in big endian.
    // We expect 1028 to indicate IPTC data block.
    if (((buf[0] << 8) + buf[1]) != 1028)
        return;
    buf += 2;

    // Next are 4 bytes of 0 padding, just skip it.
    buf += 4;

    // Next is 2 byte (big endian) giving the size of the segment
    int segmentsize = (buf[0] << 8) + buf[1];
    buf += 2;

    OpenImageIO::decode_iptc_iim (buf, segmentsize, m_spec);
}

OIIO_PLUGIN_NAMESPACE_END

