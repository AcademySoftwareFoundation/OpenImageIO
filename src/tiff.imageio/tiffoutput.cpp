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
#include <time.h>

#include "tiffio.h"

#include "dassert.h"
#include "imageio.h"


using namespace OpenImageIO;


class DLLPUBLIC TIFFOutput : public ImageOutput {
public:
    TIFFOutput ();
    virtual ~TIFFOutput ();
    virtual bool supports (const char *feature) const;
    virtual bool open (const char *name, const ImageIOFormatSpec &spec,
        int nparams, const ImageIOParameter *param, bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, int xstride);
    virtual bool write_tile (int x, int y, int z,
                             ParamType format, const void *data,
                             int xstride, int ystride, int zstride);

private:
    TIFF *m_tif;
    std::vector<unsigned char> m_scratch;
    int planarconfig;

    void contig_to_separate (int n, const unsigned char *contig,
                             unsigned char *separate);
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT TIFFOutput *tiff_output_imageio_create () { return new TIFFOutput; }

DLLEXPORT int imageio_version = IMAGEIO_VERSION;

DLLEXPORT const char * tiff_output_extensions[] = {
    "tiff", "tif", "tx", "env", "sm", "vsm", NULL
  };

};



TIFFOutput::TIFFOutput ()
    : m_tif(NULL), planarconfig(PLANARCONFIG_CONTIG)
{
}



TIFFOutput::~TIFFOutput ()
{
    close ();
}



bool
TIFFOutput::supports (const char *feature) const
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
TIFFOutput::open (const char *name, const ImageIOFormatSpec &spec,
                 int nparams, const ImageIOParameter *param, bool append)
{
    close ();  // Close any already-opened file
    this->spec = spec;  // Stash the spec

    // Open the file
    m_tif = TIFFOpen (name, append ? "a" : "w");
    if (! m_tif) {
        error ("Can't open \"%s\" for output.", name);
        return false;
    }

    // Check for unsupported data formats
    if (spec.format != PT_INT8 && spec.format != PT_UINT8 &&
        spec.format != PT_INT16 && spec.format != PT_UINT16 &&
        spec.format != PT_FLOAT) {

    }

    // FIXME:  Go through params and set values appropriately
    TIFFSetField (m_tif, TIFFTAG_XPOSITION, (float)spec.x);
    TIFFSetField (m_tif, TIFFTAG_YPOSITION, (float)spec.y);
    TIFFSetField (m_tif, TIFFTAG_IMAGEWIDTH, spec.width);
    TIFFSetField (m_tif, TIFFTAG_IMAGELENGTH, spec.height);
    if ((spec.full_width != 0 || spec.full_height != 0) &&
        (spec.full_width != spec.width || spec.full_height != spec.height)) {
        TIFFSetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, spec.full_width);
        TIFFSetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, spec.full_height);
    }
    if (spec.tile_width) {
        TIFFSetField (m_tif, TIFFTAG_TILEWIDTH, spec.tile_width);
        TIFFSetField (m_tif, TIFFTAG_TILELENGTH, spec.tile_height);
    } else {
        // Scanline images must set rowsperstrip
        TIFFSetField (m_tif, TIFFTAG_ROWSPERSTRIP, 32);
        // FIXME -- check param for this
    }
    TIFFSetField (m_tif, TIFFTAG_SAMPLESPERPIXEL, spec.nchannels);
    TIFFSetField (m_tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // always
    TIFFSetField (m_tif, TIFFTAG_PLANARCONFIG, planarconfig);  // FIXME: user choice
    
    int bps, sampformat;
    switch (spec.format) {
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
    case PT_FLOAT:
        bps = 32;
        sampformat = SAMPLEFORMAT_IEEEFP;
        break;
    default:
        error ("TIFF doesn't support %s images (\"%s\")",
               ParamBaseTypeNameString(spec.format), name);
        close();
        return false;
    }
    TIFFSetField (m_tif, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField (m_tif, TIFFTAG_SAMPLEFORMAT, sampformat);

    int photo = (spec.nchannels > 1 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
    TIFFSetField (m_tif, TIFFTAG_PHOTOMETRIC, photo);

    if (spec.nchannels == 4 && spec.alpha_channel == spec.nchannels-1) {
        unsigned short s = EXTRASAMPLE_ASSOCALPHA;
        TIFFSetField (m_tif, TIFFTAG_EXTRASAMPLES, 1, &s);
    }

    int compress = COMPRESSION_LZW;  // FIXME: user choice
    TIFFSetField (m_tif, TIFFTAG_COMPRESSION, compress);
    // Use predictor for integer pixels and compression
    if (bps < 32 && (compress == COMPRESSION_LZW || compress == COMPRESSION_ADOBE_DEFLATE))
        TIFFSetField (m_tif, TIFFTAG_PREDICTOR, 2);

    // Set date
    time_t now;
    time (&now);
    struct tm mytm;
    char buf[100];
    localtime_r (&now, &mytm);
    sprintf (buf, "%4d:%02d:%02d %2d:%02d:%02d",
             mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
             mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
    TIFFSetField (m_tif, TIFFTAG_DATETIME, buf);

    // FIXME: Deal with all other params

    TIFFCheckpointDirectory (m_tif);  // Ensure the header is written early

    return true;
}



bool
TIFFOutput::close ()
{
    if (m_tif) {
        TIFFClose (m_tif);
        m_tif = NULL;
    }
    return true;  // How can we fail?
}



/// Helper: Convert n pixels from contiguous (RGBRGBRGB) to separate
/// (RRRGGGBBB) planarconfig.
void
TIFFOutput::contig_to_separate (int n, const unsigned char *contig,
                                unsigned char *separate)
{
    int channelbytes = spec.channel_bytes();
    for (int p = 0;  p < n;  ++p)                     // loop over pixels
        for (int c = 0;  c < spec.nchannels;  ++c)    // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                separate[(c*n+p)*channelbytes+i] =
                    contig[(p*spec.nchannels+c)*channelbytes+i];
}



bool
TIFFOutput::write_scanline (int y, int z, ParamBaseType format,
                            const void *data, int xstride)
{
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);

    y -= spec.y;
    if (planarconfig == PLANARCONFIG_SEPARATE && spec.nchannels > 1) {
        // Convert from contiguous (RGBRGBRGB) to separate (RRRGGGBBB)
        m_scratch.resize (spec.scanline_bytes());
        contig_to_separate (spec.width, (const unsigned char *)data, &m_scratch[0]);
        TIFFWriteScanline (m_tif, &m_scratch[0], y);
    } else {
        // No contig->separate is necessary.  But we still use scratch
        // space since TIFFWriteScanline is destructive
        // TIFFTAG_PREDICTOR is used.
        if (data == origdata) {
            m_scratch.assign ((unsigned char *)data,
                              (unsigned char *)data + spec.scanline_bytes());
            data = &m_scratch[0];
        }
        TIFFWriteScanline (m_tif, (tdata_t)data, y);
    }
    // Every 16 scanlines, checkpoint (write partial file)
    if ((y % 16) == 0)
        TIFFCheckpointDirectory (m_tif);

    return true;
}



bool
TIFFOutput::write_tile (int x, int y, int z,
                        ParamType format, const void *data,
                        int xstride, int ystride, int zstride)
{
    // FIXME
    return false;
}
