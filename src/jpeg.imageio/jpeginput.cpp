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


// N.B. The class definition for JpgInput is in jpeg_pvt.h.


// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT int jpeg_imageio_version = OIIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *jpeg_input_imageio_create () {
        return new JpgInput;
    }
    DLLEXPORT const char *jpeg_input_extensions[] = {
        "jpg", "jpe", "jpeg", "jif", "jfif", ".jfi", NULL
    };

OIIO_PLUGIN_EXPORTS_END


static const uint32_t JPEG_MAGIC = 0xffd8ffe0, JPEG_MAGIC_OTHER_ENDIAN =  0xe0ffd8ff;
static const uint32_t JPEG_MAGIC2 = 0xffd8ffe1, JPEG_MAGIC2_OTHER_ENDIAN =  0xe1ffd8ff;


// For explanations of the error handling, see the "example.c" in the
// libjpeg distribution.


static void
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    JpgInput::my_error_ptr myerr = (JpgInput::my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
//  (*cinfo->err->output_message) (cinfo);
    myerr->jpginput->jpegerror (myerr, true);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}



static void
my_output_message (j_common_ptr cinfo)
{
    JpgInput::my_error_ptr myerr = (JpgInput::my_error_ptr) cinfo->err;
    myerr->jpginput->jpegerror (myerr, true);
}



void
JpgInput::jpegerror (my_error_ptr myerr, bool fatal)
{
    // Send the error message to the ImageInput
    char errbuf[JMSG_LENGTH_MAX];
    (*m_cinfo.err->format_message) ((j_common_ptr)&m_cinfo, errbuf);
    error ("JPEG error: %s (\"%s\")", errbuf, filename().c_str());

    // Shut it down and clean it up
    if (fatal) {
        m_fatalerr = true;
        close ();
        m_fatalerr = true;   // because close() will reset it
    }
}



bool
JpgInput::valid_file (const std::string &filename) const
{
    FILE *fd = fopen (filename.c_str(), "rb");
    if (! fd)
        return false;

    // Check magic number to assure this is a JPEG file
    uint32_t magic = 0;
    bool ok = (fread (&magic, sizeof(magic), 1, fd) == 1);
    fclose (fd);

    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        ok = false;
    }
    return ok;
}



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
    uint32_t magic = 0;
    if (fread (&magic, sizeof(magic), 1, m_fd) != 1) {
        error ("Empty file \"%s\"", name.c_str());
        close_file ();
        return false;
    }

    rewind (m_fd);
    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        close_file ();
        error ("\"%s\" is not a JPEG file, magic number doesn't match", name.c_str());
        return false;
    }

    // Set up the normal JPEG error routines, then override error_exit and
    // output_message so we intercept all the errors.
    m_cinfo.err = jpeg_std_error ((jpeg_error_mgr *)&m_jerr);
    m_jerr.pub.error_exit = my_error_exit;
    m_jerr.pub.output_message = my_output_message;
    if (setjmp (m_jerr.setjmp_buffer)) {
        // Jump to here if there's a libjpeg internal error
        // Prevent memory leaks, see example.c in jpeg distribution
        jpeg_destroy_decompress (&m_cinfo);
        close_file ();
        return false;
    }

    jpeg_create_decompress (&m_cinfo);          // initialize decompressor
    jpeg_stdio_src (&m_cinfo, m_fd);            // specify the data source

    // Request saving of EXIF and other special tags for later spelunking
    for (int mark = 0;  mark < 16;  ++mark)
        jpeg_save_markers (&m_cinfo, JPEG_APP0+mark, 0xffff);
    jpeg_save_markers (&m_cinfo, JPEG_COM, 0xffff);     // comment marker

    // read the file parameters
    if (jpeg_read_header (&m_cinfo, FALSE) != JPEG_HEADER_OK || m_fatalerr) {
        error ("Bad JPEG header for \"%s\"", filename().c_str());
        return false;
    }
    if (m_raw)
        m_coeffs = jpeg_read_coefficients (&m_cinfo);
    else
        jpeg_start_decompress (&m_cinfo);       // start working
    if (m_fatalerr)
        return false;
    m_next_scanline = 0;                        // next scanline we'll read

    m_spec = ImageSpec (m_cinfo.output_width, m_cinfo.output_height,
                        m_cinfo.output_components, TypeDesc::UINT8);

    // Assume JPEG is in sRGB unless the Exif or XMP tags say otherwise.
    m_spec.attribute ("oiio:ColorSpace", "sRGB");

    for (jpeg_saved_marker_ptr m = m_cinfo.marker_list;  m;  m = m->next) {
        if (m->marker == (JPEG_APP0+1) &&
                ! strcmp ((const char *)m->data, "Exif")) {
            // The block starts with "Exif\0\0", so skip 6 bytes to get
            // to the start of the actual Exif data TIFF directory
            decode_exif ((unsigned char *)m->data+6, m->data_length-6, m_spec);
        }
        else if (m->marker == (JPEG_APP0+1) &&
                 ! strcmp ((const char *)m->data, "http://ns.adobe.com/xap/1.0/")) {
#ifdef DEBUG
            std::cerr << "Found APP1 XMP! length " << m->data_length << "\n";
#endif
            std::string xml ((const char *)m->data, m->data_length);
            decode_xmp (xml, m_spec);
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
            ! seek_subimage (subimage, 0, dummyspec))
            return false;    // Somehow, the re-open failed
        assert (m_next_scanline == 0 && current_subimage() == subimage);
    }

    // Set up our custom error handler
    if (setjmp (m_jerr.setjmp_buffer)) {
        // Jump to here if there's a libjpeg internal error
        return false;
    }

    for (  ;  m_next_scanline <= y;  ++m_next_scanline) {
        // Keep reading until we've read the scanline we really need
        if (jpeg_read_scanlines (&m_cinfo, (JSAMPLE **)&data, 1) != 1
            || m_fatalerr) {
            error ("JPEG failed scanline read (\"%s\")", filename().c_str());
            return false;
        }
    }

    return true;
}



bool
JpgInput::close ()
{
    if (m_fd != NULL) {
        // unnecessary?  jpeg_abort_decompress (&m_cinfo);
        jpeg_destroy_decompress (&m_cinfo);
        close_file ();
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

    decode_iptc_iim (buf, segmentsize, m_spec);
}

OIIO_PLUGIN_NAMESPACE_END

