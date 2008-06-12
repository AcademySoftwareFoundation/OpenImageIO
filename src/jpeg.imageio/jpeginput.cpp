/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation and Copyright 2008 Larry Gritz.
// All Rights Reserved.
//
// Extensions by Larry Gritz based on open-source code by NVIDIA.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of NVIDIA nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// (This is the Modified BSD License)
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <ctype.h>
#include <cstdio>
#include <iostream>

#include <boost/scoped_array.hpp>

#include "imageio.h"
using namespace OpenImageIO;

#include "fmath.h"
#include "thread.h"

extern "C" {
#include "jpeglib.h"
#include "tiff.h"
}



static mutex marker_mutex;   // Guard non-reentrant marker
static ImageIOFormatSpec *marker_spec;  // Spec that my_marker_handler mods



// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgInput : public ImageInput {
 public:
    JpgInput () { init(); }
    virtual ~JpgInput () { close(); }
    virtual const char * format_name (void) const { return "JPEG"; }
    virtual bool open (const char *name, ImageIOFormatSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
 private:
    FILE *fd;
    bool first_scanline;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    void init () { fd = NULL; }
};



// Export version number and create function symbols
extern "C" {
    DLLEXPORT int imageio_version = IMAGEIO_VERSION;
    DLLEXPORT JpgInput *jpeg_input_imageio_create () {
        return new JpgInput;
    }
    DLLEXPORT const char *jpeg_input_extensions[] = {
        "jpg", "jpe", "jpeg", NULL
    };
};



// FIXME -- need mutex for thread safety
// Read next byte from jpeg strem.
// Borrowed from source code of libjpeg's "djpeg", which I used as an example
// of how to read the APPx markers.
static unsigned int
jpeg_getc (j_decompress_ptr cinfo)
{
    struct jpeg_source_mgr * datasrc = cinfo->src;

    if (datasrc->bytes_in_buffer == 0) {
        if (! (*datasrc->fill_input_buffer) (cinfo)) {
            // ERREXIT (cinfo, JERR_CANT_SUSPEND);
            // FIXME - error handling
            return 0;
        }
    }
    datasrc->bytes_in_buffer--;
    return GETJOCTET(*datasrc->next_input_byte++);
}



// Borrowed from source code of libjpeg's "djpeg", which I used as an example
// of how to read the APPx markers.
static int
my_marker_handler (j_decompress_ptr cinfo)
{
    std::cerr << "my_marker\n";

    int length = jpeg_getc(cinfo) << 8;
    length += jpeg_getc(cinfo);
    length -= 2;			/* discount the length word itself */

#if 0
    if (cinfo->unread_marker == JPEG_COM)
        fprintf(stderr, "Comment, length %ld:\n", (long) length);
    else			/* assume it is an APPn otherwise */
        fprintf(stderr, "APP%d, length %ld:\n",
                cinfo->unread_marker - JPEG_APP0, (long) length);
#endif

    boost::scoped_array<unsigned char> blob (new unsigned char [length+1]);
    for (int i = 0;  i < length;  ++i)
        blob[i] = jpeg_getc (cinfo);
    blob[length] = 0;  // Just in case it's a string that didn't terminate

    if (cinfo->unread_marker == (JPEG_APP0+1)) {
        unsigned char *buf = &blob[0];
        if (strncmp ((char *)buf, "Exif", 5)) {
            std::cerr << "JPEG: saw APP1, but didn't start 'Exif'\n";
            return 1;
        }
        buf += 6;
        TIFFHeader *head = (TIFFHeader *)buf;
        if (head->tiff_magic != 0x4949 && head->tiff_magic != 0x4d4d) {
            std::cerr << "JPEG: saw Exif, didn't see TIFF magic\n";
            return 1;
        }
        bool host_little = littleendian();
        bool file_little = (head->tiff_magic == 0x4949);
        std::cerr << "little " << host_little << ' ' << file_little << "\n";
        std::cerr << "Found TIFF magic " << (char)buf[0] << '\n';
        if (host_little != file_little)
            swap_endian (&head->tiff_diroff);
        std::cerr << "  directory offset = " << head->tiff_diroff << "\n";
        unsigned char *ifd = (buf + head->tiff_diroff);
        unsigned short ndirs = *(unsigned short *)ifd;
        if (host_little != file_little)
            swap_endian (&ndirs);
        std::cerr << "Number of directory entries = " << ndirs << "\n";
        for (int d = 0;  d < ndirs;  ++d) {
            TIFFDirEntry dir = * (TIFFDirEntry *) (ifd + 2 + d*sizeof(TIFFDirEntry));
            if (host_little != file_little) {
                swap_endian (&dir.tdir_tag);
                swap_endian (&dir.tdir_type);
                swap_endian (&dir.tdir_count);
                swap_endian (&dir.tdir_offset);
            }
            std::cerr << "Dir " << d << ": tag=" << dir.tdir_tag
                      << ", type=" << dir.tdir_type
                      << ", count=" << dir.tdir_count
                      << ", offset=" << dir.tdir_offset << "\n";
        }
    }

#if 1
    unsigned int lastch = 0;
    for (int i = 0;  i < std::min(length,100);  ++i) {
        unsigned char ch = blob[i];
        /* Emit the character in a readable form.
         * Nonprintables are converted to \nnn form,
         * while \ is converted to \\.
         * Newlines in CR, CR/LF, or LF form will be printed as one newline.
         */
        if (ch == '\r') {
            fprintf(stderr, "\n");
        } else if (ch == '\n') {
            if (lastch != '\r')
                fprintf(stderr, "\n");
        } else if (ch == '\\') {
            fprintf(stderr, "\\\\");
        } else if (isprint(ch)) {
            putc(ch, stderr);
        } else {
            fprintf(stderr, "\\%03o", ch);
        }
        lastch = ch;
    }
#endif

    return 1;
}



bool
JpgInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    // Check that file exists and can be opened
    fd = fopen (name, "rb");
    if (fd == NULL)
        return false;

    // Check magic number to assure this is a JPEG file
    int magic = 0;
    fread (&magic, 4, 1, fd);
    rewind (fd);
    const int JPEG_MAGIC = 0xffd8ffe0, JPEG_MAGIC_OTHER_ENDIAN =  0xe0ffd8ff;
    const int JPEG_MAGIC2 = 0xffd8ffe1, JPEG_MAGIC2_OTHER_ENDIAN =  0xe1ffd8ff;
    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        fclose (fd);
        return false;
    }

    m_spec = ImageIOFormatSpec();

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);            // initialize decompressor
    jpeg_stdio_src (&cinfo, fd);                // specify the data source

    // EXIF and other special tags need to be extracted by our custom
    // marker handler.  Except, duh, there's no blind pointer or other
    // way for the marker handler to know which ImageIO it's associated
    // with.  So we lock this section to make it thread-safe.
    marker_mutex.lock ();
    assert (marker_spec == NULL);
    marker_spec = &m_spec;
    jpeg_set_marker_processor (&cinfo, JPEG_APP0+1, my_marker_handler);
    jpeg_set_marker_processor (&cinfo, JPEG_COM, my_marker_handler);

    jpeg_read_header (&cinfo, FALSE);           // read the file parameters

    jpeg_set_marker_processor (&cinfo, JPEG_APP0+1, NULL);
    jpeg_set_marker_processor (&cinfo, JPEG_COM, NULL);
    marker_spec = NULL;
    marker_mutex.unlock ();
    // End critical section for the marker processing

    jpeg_start_decompress (&cinfo);             // start working
    first_scanline = true;                      // start decompressor

    m_spec.x = 0;
    m_spec.y = 0;
    m_spec.z = 0;
    m_spec.width = cinfo.output_width;
    m_spec.height = cinfo.output_height;
    m_spec.nchannels = cinfo.output_components;
    m_spec.depth = 1;
    m_spec.full_width = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth = m_spec.depth;
    m_spec.set_format (PT_UINT8);
    m_spec.tile_width = 0;
    m_spec.tile_height = 0;
    m_spec.tile_depth = 0;

    m_spec.channelnames.clear();
    switch (m_spec.nchannels) {
    case 1:
        m_spec.channelnames.push_back("a");
        break;
    case 3:
        m_spec.channelnames.push_back("r");
        m_spec.channelnames.push_back("g");
        m_spec.channelnames.push_back("b");
        break;
    case 4:
        m_spec.channelnames.push_back("r");
        m_spec.channelnames.push_back("g");
        m_spec.channelnames.push_back("b");
        m_spec.channelnames.push_back("a");
        break;
    default:
        fclose (fd);
        return false;
    }

    newspec = m_spec;
    return true;
}



bool
JpgInput::read_native_scanline (int y, int z, void *data)
{
    first_scanline = false;
    assert (y == (int)cinfo.output_scanline);
    assert (y < (int)cinfo.output_height);
    jpeg_read_scanlines (&cinfo, (JSAMPLE **)&data, 1); // read one scanline
    return true;
}



bool
JpgInput::close ()
{
    if (fd != NULL) {
        if (!first_scanline)
            jpeg_finish_decompress (&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose (fd);
    }
    init ();   // Reset to initial state
    return true;
}

