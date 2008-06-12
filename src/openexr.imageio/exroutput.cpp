/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <time.h>

#include <tiffio.h>

#include "dassert.h"
#include "imageio.h"


using namespace OpenImageIO;


class OpenEXROutput : public ImageOutput {
public:
    OpenEXROutput ();
    virtual ~OpenEXROutput ();
    virtual const char * format_name (void) const { return "OpenEXR"; }
    virtual bool supports (const char *feature) const;
    virtual bool open (const char *name, const ImageIOFormatSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z,
                             ParamBaseType format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);

private:
    std::vector<unsigned char> m_scratch;

    // Initialize private members to pre-opened state
    void init (void) {
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, ParamBaseType type,
                        const void *data);
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT OpenEXROutput *openexr_output_imageio_create () { return new OpenEXROutput; }

DLLEXPORT int imageio_version = IMAGEIO_VERSION;

DLLEXPORT const char * openexr_output_extensions[] = {
    "exr", NULL
};

};



OpenEXROutput::OpenEXROutput ()
{
    init ();
}



OpenEXROutput::~OpenEXROutput ()
{
    // Close, if not already done.
    close ();
}



bool
OpenEXROutput::supports (const char *feature) const
{
    if (! strcmp (feature, "tiles"))
        return true;
    if (! strcmp (feature, "multiimage"))
        return true;

    // FIXME: we could support "volumes" and "empty"

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open (const char *name, const ImageIOFormatSpec &userspec,
                     bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

#if 0
    // Open the file
    m_tif = TIFFOpen (name, append ? "a" : "w");
    if (! m_tif) {
        error ("Can't open \"%s\" for output.", name);
        return false;
    }

    TIFFSetField (m_tif, TIFFTAG_XPOSITION, (float)m_spec.x);
    TIFFSetField (m_tif, TIFFTAG_YPOSITION, (float)m_spec.y);
    TIFFSetField (m_tif, TIFFTAG_IMAGEWIDTH, m_spec.width);
    TIFFSetField (m_tif, TIFFTAG_IMAGELENGTH, m_spec.height);
    if ((m_spec.full_width != 0 || m_spec.full_height != 0) &&
        (m_spec.full_width != m_spec.width || m_spec.full_height != m_spec.height)) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, m_spec.full_width);
        TIFFSetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, m_spec.full_height);
    }
    if (m_spec.tile_width) {
        TIFFSetField (m_tif, TIFFTAG_TILEWIDTH, m_spec.tile_width);
        TIFFSetField (m_tif, TIFFTAG_TILELENGTH, m_spec.tile_height);
    } else {
        // Scanline images must set rowsperstrip
        TIFFSetField (m_tif, TIFFTAG_ROWSPERSTRIP, 32);
    }
    TIFFSetField (m_tif, TIFFTAG_SAMPLESPERPIXEL, m_spec.nchannels);
    TIFFSetField (m_tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // always
    
    int bps, sampformat;
    switch (m_spec.format) {
    case PT_INT8:
        bps = 8;
        sampformat = SAMPLEFORMAT_INT;
        break;
    case PT_UINT8:
        bps = 8;
        sampformat = SAMPLEFORMAT_UINT;
        break;
    case PT_INT16:
        bps = 16;
        sampformat = SAMPLEFORMAT_INT;
        break;
    case PT_UINT16:
        bps = 16;
        sampformat = SAMPLEFORMAT_UINT;
        break;
    case PT_HALF:
        // Silently change requests for unsupported 'half' to 'float'
        m_spec.format = PT_FLOAT;
    case PT_FLOAT:
        bps = 32;
        sampformat = SAMPLEFORMAT_IEEEFP;
        break;
    default:
        error ("TIFF doesn't support %s images (\"%s\")",
               ParamBaseTypeNameString(m_spec.format), name);
        close();
        return false;
    }
    TIFFSetField (m_tif, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField (m_tif, TIFFTAG_SAMPLEFORMAT, sampformat);

    int photo = (m_spec.nchannels > 1 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
    TIFFSetField (m_tif, TIFFTAG_PHOTOMETRIC, photo);

    if (m_spec.nchannels == 4 && m_spec.alpha_channel == m_spec.nchannels-1) {
        unsigned short s = EXTRASAMPLE_ASSOCALPHA;
        TIFFSetField (m_tif, TIFFTAG_EXTRASAMPLES, 1, &s);
    }

    // Figure out if the user requested a specific compression in the
    // extra parameters.
    int compress = COMPRESSION_LZW;  // default
    ImageIOParameter *param;
    const char *str;
    if ((param = m_spec.find_parameter("compression"))  &&
            param->type == PT_STRING  &&  (str = *(char **)param->data())) {
        if (! strcmp (str, "none"))
            compress = COMPRESSION_NONE;
        else if (! strcmp (str, "lzw"))
            compress = COMPRESSION_LZW;
        else if (! strcmp (str, "zip") || ! strcmp (str, "deflate"))
            compress = COMPRESSION_ADOBE_DEFLATE;
    }
    TIFFSetField (m_tif, TIFFTAG_COMPRESSION, compress);
    // Use predictor when using compression
    if (compress == COMPRESSION_LZW || compress == COMPRESSION_ADOBE_DEFLATE) {
        if (m_spec.format == PT_FLOAT || m_spec.format == PT_DOUBLE || m_spec.format == PT_HALF)
            TIFFSetField (m_tif, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT);
        else
            TIFFSetField (m_tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    }

    // Did the user request separate planar configuration?
    m_planarconfig = PLANARCONFIG_CONTIG;
    if ((param = m_spec.find_parameter("planarconfig")) &&
            param->type == PT_STRING  &&  (str = *(char **)param->data())) {
        if (! strcmp (str, "separate"))
            m_planarconfig = PLANARCONFIG_SEPARATE;
    }
    TIFFSetField (m_tif, TIFFTAG_PLANARCONFIG, m_planarconfig);

    // Set date in the file headers to NOW
    time_t now;
    time (&now);
    struct tm mytm;
    char buf[100];
    localtime_r (&now, &mytm);
    sprintf (buf, "%4d:%02d:%02d %2d:%02d:%02d",
             mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
             mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
    TIFFSetField (m_tif, TIFFTAG_DATETIME, buf);

    // Deal with all other params
    for (size_t p = 0;  p < m_spec.extra_params.size();  ++p)
        put_parameter (m_spec.extra_params[p].name, m_spec.extra_params[p].type,
                       m_spec.extra_params[p].data());

    TIFFCheckpointDirectory (m_tif);  // Ensure the header is written early

    return true;
#endif
}



bool
OpenEXROutput::put_parameter (const std::string &name, ParamBaseType type,
                           const void *data)
{
#if 0
    if ((name == "artist" || name == "Artist") && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_ARTIST, *(char**)data);
        return true;
    }
    if ((name == "copyright" || name == "Copyright") && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_COPYRIGHT, *(char**)data);
        return true;
    }
    if ((name == "name" || name == "DocumentName") && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_DOCUMENTNAME, *(char**)data);
        return true;
    }
    if (name == "fovcot" && type == PT_FLOAT) {
        double d = *(float *)data;
        TIFFSetField (m_tif, TIFFTAG_PIXAR_FOVCOT, d);
        return true;
    }
    if ((name == "host" || name == "HostComputer") && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_HOSTCOMPUTER, *(char**)data);
        return true;
    }
    if ((name == "description" || name == "ImageDescription") &&
          type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_IMAGEDESCRIPTION, *(char**)data);
        return true;
    }
    if (name == "tiff_Predictor" && type == PT_INT) {
        TIFFSetField (m_tif, TIFFTAG_PREDICTOR, *(int *)data);
        return true;
    }
    if (name == "tiff_RowsPerStrip") {
        if (type == PT_INT) {
            TIFFSetField (m_tif, TIFFTAG_ROWSPERSTRIP, *(int*)data);
            return true;
        } else if (type == PT_STRING) {
            // Back-compatibility with Entropy and PRMan
            TIFFSetField (m_tif, TIFFTAG_ROWSPERSTRIP, atoi(*(char **)data));
            return true;
        }
    }
    if ((name == "software" || name == "Software") && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_SOFTWARE, *(char**)data);
        return true;
    }
    if (name == "tiff_SubFileType" && type == PT_INT) {
        TIFFSetField (m_tif, TIFFTAG_SUBFILETYPE, *(int*)data);
        return true;
    }
    if (name == "textureformat" && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, *(char**)data);
        return true;
    }
    if (name == "wrapmodes" && type == PT_STRING) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_WRAPMODES, *(char**)data);
        return true;
    }
    if (name == "worldtocamera" && type == PT_MATRIX) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, data);
        return true;
    }
    if (name == "worldtoscreen" && type == PT_MATRIX) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, data);
        return true;
    }
    return false;
#endif
}



bool
OpenEXROutput::close ()
{
#if 0
    if (m_tif)
        TIFFClose (m_tif);
#endif
    init ();      // re-initialize
    return true;  // How can we fail?
}



bool
OpenEXROutput::write_scanline (int y, int z, ParamBaseType format,
                               const void *data, stride_t xstride)
{
    m_spec.auto_stride (xstride);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);

    y -= m_spec.y;
#if 0
    if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from contiguous (RGBRGBRGB) to separate (RRRGGGBBB)
        m_scratch.resize (m_spec.scanline_bytes());
        contig_to_separate (m_spec.width, (const unsigned char *)data, &m_scratch[0]);
        TIFFWriteScanline (m_tif, &m_scratch[0], y);
    } else {
        // No contig->separate is necessary.  But we still use scratch
        // space since TIFFWriteScanline is destructive when
        // TIFFTAG_PREDICTOR is used.
        if (data == origdata) {
            m_scratch.assign ((unsigned char *)data,
                              (unsigned char *)data+m_spec.scanline_bytes());
            data = &m_scratch[0];
        }
        TIFFWriteScanline (m_tif, (tdata_t)data, y);
    }
    // Every 16 scanlines, checkpoint (write partial file)
    if ((y % 16) == 0)
        TIFFCheckpointDirectory (m_tif);
#endif
    return true;
}



bool
OpenEXROutput::write_tile (int x, int y, int z,
                        ParamBaseType format, const void *data,
                        stride_t xstride, stride_t ystride, stride_t zstride)
{
    m_spec.auto_stride (xstride, ystride, zstride);
    x -= m_spec.x;   // Account for offset, so x,y are file relative, not 
    y -= m_spec.y;   // image relative
    const void *origdata = data;   // Stash original pointer
    data = to_native_tile (format, data, xstride, ystride, zstride, m_scratch);

#if 0
    if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from contiguous (RGBRGBRGB) to separate (RRRGGGBBB)
        int tile_pixels = m_spec.tile_width * m_spec.tile_height 
                            * std::max (m_spec.tile_depth, 1);
        int plane_bytes = tile_pixels * ParamBaseTypeSize(m_spec.format);
        DASSERT (plane_bytes*m_spec.nchannels == m_spec.tile_bytes());
        m_scratch.resize (m_spec.tile_bytes());
        contig_to_separate (tile_pixels, (const unsigned char *)data, &m_scratch[0]);
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            TIFFWriteTile (m_tif, (tdata_t)&m_scratch[plane_bytes*c], x, y, z, c);
    } else {
        // No contig->separate is necessary.  But we still use scratch
        // space since TIFFWriteTile is destructive when
        // TIFFTAG_PREDICTOR is used.
        if (data == origdata) {
            m_scratch.assign ((unsigned char *)data,
                              (unsigned char *)data + m_spec.tile_bytes());
            data = &m_scratch[0];
        }
        TIFFWriteTile (m_tif, (tdata_t)data, x, y, z, 0);
    }

    // Every row of tiles, checkpoint (write partial file)
    if ((y % m_spec.tile_height) == 0)
        TIFFCheckpointDirectory (m_tif);
#endif

    return true;
}
