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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include "jpeg_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if(0)


// References:
//  * JPEG library documentation: /usr/share/doc/libjpeg-devel-6b
//  * JFIF spec: https://www.w3.org/Graphics/JPEG/jfif3.pdf
//  * ITU T.871 (aka ISO/IEC 10918-5):
//      https://www.itu.int/rec/T-REC-T.871-201105-I/en



class JpgOutput final : public ImageOutput {
 public:
    JpgOutput () { init(); }
    virtual ~JpgOutput () { close(); }
    virtual const char * format_name (void) const { return "jpeg"; }
    virtual int supports (string_view feature) const {
        return (feature == "exif"
             || feature == "iptc");
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
    virtual bool close ();
    virtual bool copy_image (ImageInput *in);

 private:
    FILE *m_fd;
    std::string m_filename;
    unsigned int m_dither;
    int m_next_scanline;             // Which scanline is the next to write?
    std::vector<unsigned char> m_scratch;
    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr c_jerr;
    jvirt_barray_ptr *m_copy_coeffs;
    struct jpeg_decompress_struct *m_copy_decompressor;
    std::vector<unsigned char> m_tilebuffer;

    void init (void) {
        m_fd = NULL;
        m_copy_coeffs = NULL;
        m_copy_decompressor = NULL;
    }
    
    void set_subsampling (const int components[]) {
        jpeg_set_colorspace (&m_cinfo, JCS_YCbCr);
        m_cinfo.comp_info[0].h_samp_factor = components[0];
        m_cinfo.comp_info[0].v_samp_factor = components[1];
        m_cinfo.comp_info[1].h_samp_factor = components[2];
        m_cinfo.comp_info[1].v_samp_factor = components[3];
        m_cinfo.comp_info[2].h_samp_factor = components[4];
        m_cinfo.comp_info[2].v_samp_factor = components[5];
    }
};



OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *jpeg_output_imageio_create () {
        return new JpgOutput;
    }
    OIIO_EXPORT const char *jpeg_output_extensions[] = {
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

    m_fd = Filesystem::fopen (name, "wb");
    if (m_fd == NULL) {
        error ("Unable to open file \"%s\"", name.c_str());
        return false;
    }

    m_cinfo.err = jpeg_std_error (&c_jerr);             // set error handler
    jpeg_create_compress (&m_cinfo);                    // create compressor
    jpeg_stdio_dest (&m_cinfo, m_fd);                   // set output stream

    // Set image and compression parameters
    m_cinfo.image_width = m_spec.width;
    m_cinfo.image_height = m_spec.height;

    // JFIF can only handle grayscale and RGB. Do the best we can with this
    // limited format by truncating to 3 channels if > 3 are requested,
    // truncating to 1 channel if 2 are requested.
    if (m_spec.nchannels >= 3) {
        m_cinfo.input_components = 3;
        m_cinfo.in_color_space = JCS_RGB;
    } else {
        m_cinfo.input_components = 1;
        m_cinfo.in_color_space = JCS_GRAYSCALE;
    }

    string_view resunit = m_spec.get_string_attribute ("ResolutionUnit");
    if (Strutil::iequals (resunit, "none"))
        m_cinfo.density_unit = 0;
    else if (Strutil::iequals (resunit, "in"))
        m_cinfo.density_unit = 1;
    else if (Strutil::iequals (resunit, "cm"))
        m_cinfo.density_unit = 2;
    else
        m_cinfo.density_unit = 0;

    m_cinfo.X_density = int (m_spec.get_float_attribute ("XResolution"));
    m_cinfo.Y_density = int (m_spec.get_float_attribute ("YResolution"));
    const float aspect = m_spec.get_float_attribute ("PixelAspectRatio", 1.0f);
    if (aspect != 1.0f && m_cinfo.X_density <= 1 && m_cinfo.Y_density <= 1) {
        // No useful [XY]Resolution, but there is an aspect ratio requested.
        // Arbitrarily pick 72 dots per undefined unit, and jigger it to
        // honor it as best as we can.
        //
        // Here's where things get tricky. By logic and reason, as well as
        // the JFIF spec and ITU T.871, the pixel aspect ratio is clearly
        // ydensity/xdensity (because aspect is xlength/ylength, and density
        // is 1/length). BUT... for reasons lost to history, a number of
        // apps get this exactly backwards, and these include PhotoShop,
        // Nuke, and RV. So, alas, we must replicate the mistake, or else
        // all these common applications will misunderstand the JPEG files
        // written by OIIO and vice versa.
        m_cinfo.Y_density = 72;
        m_cinfo.X_density = int (m_cinfo.Y_density * aspect + 0.5f);
        m_spec.attribute ("XResolution", float(m_cinfo.Y_density * aspect + 0.5f));
        m_spec.attribute ("YResolution", float(m_cinfo.Y_density));
    }

    m_cinfo.write_JFIF_header = TRUE;

    if (m_copy_coeffs) {
        // Back door for copy()
        jpeg_copy_critical_parameters (m_copy_decompressor, &m_cinfo);
        DBG std::cout << "out open: copy_critical_parameters\n";
        jpeg_write_coefficients (&m_cinfo, m_copy_coeffs);
        DBG std::cout << "out open: write_coefficients\n";
    } else {
        // normal write of scanlines
        jpeg_set_defaults (&m_cinfo);                 // default compression
        // Careful -- jpeg_set_defaults overwrites density
        m_cinfo.X_density = int (m_spec.get_float_attribute ("XResolution"));
        m_cinfo.Y_density = int (m_spec.get_float_attribute ("YResolution", m_cinfo.X_density));
        DBG std::cout << "out open: set_defaults\n";
        int quality = newspec.get_int_attribute ("CompressionQuality", 98);
        jpeg_set_quality (&m_cinfo, quality, TRUE);   // baseline values
        DBG std::cout << "out open: set_quality\n";
        
        if (m_cinfo.input_components == 3) {
            std::string subsampling = m_spec.get_string_attribute (JPEG_SUBSAMPLING_ATTR);
            if (subsampling == JPEG_444_STR)
                set_subsampling(JPEG_444_COMP);
            else if (subsampling == JPEG_422_STR)
                set_subsampling(JPEG_422_COMP);
            else if (subsampling == JPEG_420_STR)
                set_subsampling(JPEG_420_COMP);
            else if (subsampling == JPEG_411_STR)
                set_subsampling(JPEG_411_COMP);
        }
        DBG std::cout << "out open: set_colorspace\n";
                
        jpeg_start_compress (&m_cinfo, TRUE);         // start working
        DBG std::cout << "out open: start_compress\n";
    }
    m_next_scanline = 0;    // next scanline we'll write

    // Write JPEG comment, if sent an 'ImageDescription'
    ParamValue *comment = m_spec.find_attribute ("ImageDescription",
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
        block.insert (block.end(), xmp.c_str(), xmp.c_str()+xmp.length());
        jpeg_write_marker (&m_cinfo, JPEG_APP0+1, (JOCTET*)&block[0], block.size());
    }

    m_spec.set_format (TypeDesc::UINT8);  // JPG is only 8 bit

    // Write ICC profile, if we have anything
    const ParamValue* icc_profile_parameter = m_spec.find_attribute(ICC_PROFILE_ATTR);
    if (icc_profile_parameter != NULL) {
        unsigned char *icc_profile = (unsigned char*)icc_profile_parameter->data();
        unsigned int icc_profile_length = icc_profile_parameter->type().size();
        if (icc_profile && icc_profile_length){
            /* Calculate the number of markers we'll need, rounding up of course */
            int num_markers = icc_profile_length / MAX_DATA_BYTES_IN_MARKER;
            if ((unsigned int)(num_markers * MAX_DATA_BYTES_IN_MARKER) != icc_profile_length)
                num_markers++;
            int curr_marker = 1;  /* per spec, count strarts at 1*/
            std::vector<unsigned char> profile (MAX_DATA_BYTES_IN_MARKER + ICC_HEADER_SIZE);
            while (icc_profile_length > 0) {
                // length of profile to put in this marker
                unsigned int length = std::min (icc_profile_length, (unsigned int)MAX_DATA_BYTES_IN_MARKER);
                icc_profile_length -= length;
                // Write the JPEG marker header (APP2 code and marker length)
                strcpy ((char *)&profile[0], "ICC_PROFILE");
                profile[11] = 0;
                profile[12] = curr_marker;
                profile[13] = (unsigned char) num_markers;
                memcpy(&profile[0] + ICC_HEADER_SIZE, icc_profile+length*(curr_marker-1), length);
                jpeg_write_marker(&m_cinfo, JPEG_APP0 + 2, &profile[0], ICC_HEADER_SIZE+length);
                curr_marker++;
            }
        }
    }

    m_dither = m_spec.get_int_attribute ("oiio:dither", 0);

    // If user asked for tiles -- which JPEG doesn't support, emulate it by
    // buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

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

    // Here's where we do the dirty work of conforming to JFIF's limitation
    // of 1 or 3 channels, by temporarily doctoring the spec so that
    // to_native_scanline properly contiguizes the first 1 or 3 channels,
    // then we restore it.  The call to to_native_scanline below needs
    // m_spec.nchannels to be set to the true number of channels we're
    // writing, or it won't arrange the data properly.  But if we doctored
    // m_spec.nchannels permanently, then subsequent calls to write_scanline
    // (including any surrounding call to write_image) with
    // stride=AutoStride would screw up the strides since the user's stride
    // is actually not 1 or 3 channels.
    m_spec.auto_stride (xstride, format, m_spec.nchannels);
    int save_nchannels = m_spec.nchannels;
    m_spec.nchannels = m_cinfo.input_components;

    data = to_native_scanline (format, data, xstride, m_scratch,
                               m_dither, y, z);
    m_spec.nchannels = save_nchannels;

    jpeg_write_scanlines (&m_cinfo, (JSAMPLE**)&data, 1);
    ++m_next_scanline;

    return true;
}



bool
JpgOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}



bool
JpgOutput::close ()
{
    if (! m_fd) {         // Already closed
        return true;
        init();
    }

    bool ok = true;

    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);  // free it
    }

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
    
    return ok;
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

