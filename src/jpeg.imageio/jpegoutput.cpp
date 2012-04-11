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
#include <vector>

extern "C" {
#include "jpeglib.h"
}

#include "imageio.h"
#include "fmath.h"
#include "jpeg_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if(0)



// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgOutput : public ImageOutput {
 public:
    JpgOutput () { init(); }
    virtual ~JpgOutput () { close(); }
    virtual const char * format_name (void) const { return "jpeg"; }
    virtual bool supports (const std::string &property) const { return false; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool close ();
    virtual bool copy_image (ImageInput *in);

 private:
    FILE *m_fd;
    std::string m_filename;
    int m_next_scanline;             // Which scanline is the next to write?
    std::vector<unsigned char> m_scratch;
    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr c_jerr;
    jvirt_barray_ptr *m_copy_coeffs;
    struct jpeg_decompress_struct *m_copy_decompressor;

    void init (void) {
        m_fd = NULL;
        m_copy_coeffs = NULL;
        m_copy_decompressor = NULL;
    }
};



OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT ImageOutput *jpeg_output_imageio_create () {
        return new JpgOutput;
    }
    DLLEXPORT const char *jpeg_output_extensions[] = {
        "jpg", "jpe", "jpeg", "jif", "jfif", "jfi", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
JpgOutput::open (const std::string &name, const ImageSpec &newspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // Save name and spec for later use
    m_filename = name;
    m_spec = newspec;

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    if (m_spec.nchannels != 1 && m_spec.nchannels != 3 &&
            m_spec.nchannels != 4) {
        error ("%s does not support %d-channel images\n",
               format_name(), m_spec.nchannels);
        return false;
    }

    m_fd = fopen (name.c_str(), "wb");
    if (m_fd == NULL) {
        error ("Unable to open file \"%s\"", name.c_str());
        return false;
    }

    int quality = 98;
    const ImageIOParameter *qual = newspec.find_attribute ("CompressionQuality",
                                                           TypeDesc::INT);
    if (qual)
        quality = * (const int *)qual->data();

    m_cinfo.err = jpeg_std_error (&c_jerr);             // set error handler
    jpeg_create_compress (&m_cinfo);                    // create compressor
    jpeg_stdio_dest (&m_cinfo, m_fd);                   // set output stream

    // Set image and compression parameters
    m_cinfo.image_width = m_spec.width;
    m_cinfo.image_height = m_spec.height;

    if (m_spec.nchannels == 3 || m_spec.nchannels == 4) {
        m_cinfo.input_components = 3;
        m_cinfo.in_color_space = JCS_RGB;
    } else if (m_spec.nchannels == 1) {
        m_cinfo.input_components = 1;
        m_cinfo.in_color_space = JCS_GRAYSCALE;
    }
    m_cinfo.density_unit = 2; // RESUNIT_INCH;
    m_cinfo.X_density = 72;
    m_cinfo.Y_density = 72;
    m_cinfo.write_JFIF_header = true;

    if (m_copy_coeffs) {
        // Back door for copy()
        jpeg_copy_critical_parameters (m_copy_decompressor, &m_cinfo);
        DBG std::cout << "out open: copy_critical_parameters\n";
        jpeg_write_coefficients (&m_cinfo, m_copy_coeffs);
        DBG std::cout << "out open: write_coefficients\n";
    } else {
        // normal write of scanlines
        jpeg_set_defaults (&m_cinfo);                 // default compression
        DBG std::cout << "out open: set_defaults\n";
        jpeg_set_quality (&m_cinfo, quality, TRUE);   // baseline values
        DBG std::cout << "out open: set_quality\n";
        jpeg_start_compress (&m_cinfo, TRUE);         // start working
        DBG std::cout << "out open: start_compress\n";
    }
    m_next_scanline = 0;    // next scanline we'll write

    // Write JPEG comment, if sent an 'ImageDescription'
    ImageIOParameter *comment = m_spec.find_attribute ("ImageDescription",
                                                       TypeDesc::STRING);
    if (comment && comment->data()) {
        const char **c = (const char **) comment->data();
        jpeg_write_marker (&m_cinfo, JPEG_COM, (JOCTET*)*c, strlen(*c) + 1);
    }
    
    if (Strutil::iequals (m_spec.get_string_attribute ("oiio:ColorSpace"), "sRGB"))
        m_spec.attribute ("Exif:ColorSpace", 1);

    // Write EXIF info
    std::vector<char> exif;
    // Start the blob with "Exif" and two nulls.  That's how it
    // always is in the JPEG files I've examined.
    exif.push_back ('E');
    exif.push_back ('x');
    exif.push_back ('i');
    exif.push_back ('f');
    exif.push_back (0);
    exif.push_back (0);
    encode_exif (m_spec, exif);
    jpeg_write_marker (&m_cinfo, JPEG_APP0+1, (JOCTET*)&exif[0], exif.size());

    // Write IPTC IIM metadata tags, if we have anything
    std::vector<char> iptc;
    encode_iptc_iim (m_spec, iptc);
    if (iptc.size()) {
        static char photoshop[] = "Photoshop 3.0";
        std::vector<char> head (photoshop, photoshop+strlen(photoshop)+1);
        static char _8BIM[] = "8BIM";
        head.insert (head.end(), _8BIM, _8BIM+4);
        head.push_back (4);   // 0x0404
        head.push_back (4);
        head.push_back (0);   // four bytes of zeroes
        head.push_back (0);
        head.push_back (0);
        head.push_back (0);
        head.push_back ((char)(iptc.size() >> 8));  // size of block
        head.push_back ((char)(iptc.size() & 0xff));
        iptc.insert (iptc.begin(), head.begin(), head.end());
        jpeg_write_marker (&m_cinfo, JPEG_APP0+13, (JOCTET*)&iptc[0], iptc.size());
    }

    // Write XMP packet, if we have anything
    std::string xmp = encode_xmp (m_spec, true);
    if (! xmp.empty()) {
        static char prefix[] = "http://ns.adobe.com/xap/1.0/";
        std::vector<char> block (prefix, prefix+strlen(prefix)+1);
        block.insert (block.end(), xmp.c_str(), xmp.c_str()+xmp.length()+1);
        jpeg_write_marker (&m_cinfo, JPEG_APP0+1, (JOCTET*)&block[0], block.size());
    }

    m_spec.set_format (TypeDesc::UINT8);  // JPG is only 8 bit

    return true;
}



bool
JpgOutput::write_scanline (int y, int z, TypeDesc format,
                           const void *data, stride_t xstride)
{
    y -= m_spec.y;
    if (y != m_next_scanline) {
        error ("Attempt to write scanlines out of order to %s",
               m_filename.c_str());
        return false;
    }
    if (y >= (int)m_cinfo.image_height) {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        return false;
    }
    assert (y == (int)m_cinfo.next_scanline);

    // It's so common to want to write RGBA data out as JPEG (which only
    // supports RGB) than it would be too frustrating to reject it.
    // Instead, we just silently drop the alpha.  Here's where we do the
    // dirty work, temporarily doctoring the spec so that
    // to_native_scanline properly contiguizes the first three channels,
    // then we restore it.  The call to to_native_scanline below needs
    // m_spec.nchannels to be set to the true number of channels we're
    // writing, or it won't arrange the data properly.  But if we
    // doctored m_spec.nchannels = 3 permanently, then subsequent calls
    // to write_scanline (including any surrounding call to write_image)
    // with stride=AutoStride would screw up the strides since the
    // user's stride is actually not 3 channels.
    int save_nchannels = m_spec.nchannels;
    m_spec.nchannels = m_cinfo.input_components;

    data = to_native_scanline (format, data, xstride, m_scratch);
    m_spec.nchannels = save_nchannels;

    jpeg_write_scanlines (&m_cinfo, (JSAMPLE**)&data, 1);
    ++m_next_scanline;

    return true;
}



bool
JpgOutput::close ()
{
    if (! m_fd)          // Already closed
        return true;

    if (m_next_scanline < spec().height && m_copy_coeffs == NULL) {
        // But if we've only written some scanlines, write the rest to avoid
        // errors
        std::vector<char> buf (spec().scanline_bytes(), 0);
        char *data = &buf[0];
        while (m_next_scanline < spec().height) {
            jpeg_write_scanlines (&m_cinfo, (JSAMPLE **)&data, 1);
            // DBG std::cout << "out close: write_scanlines\n";
            ++m_next_scanline;
        }
    }

    if (m_next_scanline >= spec().height || m_copy_coeffs) {
        DBG std::cout << "out close: about to finish_compress\n";
        jpeg_finish_compress (&m_cinfo);
        DBG std::cout << "out close: finish_compress\n";
    } else {
        DBG std::cout << "out close: about to abort_compress\n";
        jpeg_abort_compress (&m_cinfo);
        DBG std::cout << "out close: abort_compress\n";
    }
    DBG std::cout << "out close: about to destroy_compress\n";
    jpeg_destroy_compress (&m_cinfo);
    fclose (m_fd);
    m_fd = NULL;
    init();
    
    return true;
}



bool
JpgOutput::copy_image (ImageInput *in)
{
    if (in && !strcmp(in->format_name(), "jpeg")) {
        JpgInput *jpg_in = dynamic_cast<JpgInput *> (in);
        std::string in_name = jpg_in->filename ();
        DBG std::cout << "JPG copy_image from " << in_name << "\n";

        // Save the original input spec and close it
        ImageSpec orig_in_spec = in->spec();
        in->close ();
        DBG std::cout << "Closed old file\n";

        // Re-open the input spec, with special request that the JpgInput
        // will recognize as a request to merely open, but not start the
        // decompressor.
        ImageSpec in_spec;
        ImageSpec config_spec;
        config_spec.attribute ("_jpeg:raw", 1);
        in->open (in_name, in_spec, config_spec);

        // Re-open the output
        std::string out_name = m_filename;
        ImageSpec orig_out_spec = spec();
        close ();
        m_copy_coeffs = (jvirt_barray_ptr *)jpg_in->coeffs();
        m_copy_decompressor = &jpg_in->m_cinfo;
        open (out_name, orig_out_spec);

        // Strangeness -- the write_coefficients somehow sets things up
        // so that certain writes only happen in close(), which MUST
        // happen while the input file is still open.  So we go ahead
        // and close() now, so that the caller of copy_image() doesn't
        // close the input file first and then wonder why they crashed.
        close ();

        return true;
    }

    return ImageOutput::copy_image (in);
}

OIIO_PLUGIN_NAMESPACE_END

