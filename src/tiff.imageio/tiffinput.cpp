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

#include <tiffio.h>

#include "dassert.h"
#include "paramtype.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"


using namespace OpenImageIO;


class TIFFInput : public ImageInput {
public:
    TIFFInput () { init(); }
    virtual ~TIFFInput () { close(); }
    virtual const char * format_name (void) const { return "tiff"; }
    virtual bool open (const char *name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    TIFF *m_tif;                     ///< libtiff handle
    std::string m_filename;          ///< Stash the filename
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use
    int m_subimage;                  ///< What subimage are we looking at?
    unsigned short m_planarconfig;   ///< Planar config of the file
    unsigned short m_bitspersample;  ///< Of the *file*, not the client's view
    unsigned short m_photometric;    ///< Of the *file*, not the client's view
    std::vector<unsigned short> m_colormap;  ///< Color map for palette images

    // Reset everything to initial state
    void init () {
        m_tif = NULL;
        m_subimage = -1;
        m_colormap.clear();
    }

    // Read tags from the current directory of m_tif and fill out spec
    void readspec ();

    // Convert planar separate to contiguous data format
    void separate_to_contig (int n, const unsigned char *separate,
                             unsigned char *separate);

    // Convert palette to RGB
    void palette_to_rgb (int n, const unsigned char *palettepels,
                         unsigned char *rgb);

    // Convert bilevel (1-bit) to 8-bit
    void bilevel_to_8bit (int n, const unsigned char *bits,
                          unsigned char *bytes);

     // Convert bilevel (1-bit) to 8-bit
    void fourbit_to_8bit (int n, const unsigned char *bits,
                          unsigned char *bytes);

    void invert_photometric (int n, void *data);

    // Get a string tiff tag field and put it into extra_params
    void get_string_attribute (const std::string &name, int tag) {
        char *s = NULL;
        TIFFGetField (m_tif, tag, &s);
        if (s && *s)
            m_spec.attribute (name, PT_STRING, 1, &s);
    }

    // Get a float-oid tiff tag field and put it into extra_params
    void get_float_attribute (const std::string &name, int tag,
                          ParamBaseType type=PT_FLOAT) {
        float f[16];
        if (TIFFGetField (m_tif, tag, f))
            m_spec.attribute (name, type, 1, &f);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_attribute (const std::string &name, int tag) {
        int i;
        if (TIFFGetField (m_tif, tag, &i))
            m_spec.attribute (name, PT_INT, 1, &i);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_attribute (const std::string &name, int tag) {
        unsigned short s;
        if (TIFFGetField (m_tif, tag, &s)) {
            int i = s;
            m_spec.attribute (name, PT_INT, 1, &i);
        }
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT TIFFInput *tiff_input_imageio_create () { return new TIFFInput; }

// DLLEXPORT int imageio_version = IMAGEIO_VERSION; // it's in tiffoutput.cpp

DLLEXPORT const char * tiff_input_extensions[] = {
    "tiff", "tif", "tx", "env", "sm", "vsm", NULL
};

};



// Someplace to store an error message from the TIFF error handler
static std::string lasterr;
static mutex lasterr_mutex;


static void
my_error_handler (const char *str, const char *format, va_list ap)
{
    lock_guard lock (lasterr_mutex);
    lasterr = Strutil::vformat (format, ap);
}



bool
TIFFInput::open (const char *name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = -1;
    return seek_subimage (0, newspec);
}



bool
TIFFInput::seek_subimage (int index, ImageSpec &newspec)
{
    if (index < 0)       // Illegal
        return false;
    if (index == m_subimage) {
        // We're already pointing to the right subimage
        newspec = m_spec;
        return true;
    }

    // User our own error handler to keep libtiff from spewing to stderr
    TIFFSetErrorHandler (my_error_handler);
    TIFFSetWarningHandler (my_error_handler);

    if (! m_tif) {
        m_tif = TIFFOpen (m_filename.c_str(), "r");
        if (m_tif == NULL) {
            error ("%s", lasterr.c_str());
            return false;
        }
        m_subimage = 0;
    }
    
    if (TIFFSetDirectory (m_tif, index)) {
        m_subimage = index;
        readspec ();
        newspec = m_spec;
        if (newspec.format == PT_UNKNOWN) {
            error ("No support for data format of \"%s\"", m_filename.c_str());
            return false;
        }
        return true;
    } else {
        error ("%s", lasterr.c_str());
        m_subimage = -1;
        return false;
    }
}



void
TIFFInput::readspec ()
{
    uint32 width, height, depth;
    unsigned short nchans;
    TIFFGetField (m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetField (m_tif, TIFFTAG_SAMPLESPERPIXEL, &nchans);

    m_spec = ImageSpec ((int)width, (int)height, (int)nchans);

    float x = 0, y = 0;
    TIFFGetField (m_tif, TIFFTAG_XPOSITION, &x);
    TIFFGetField (m_tif, TIFFTAG_YPOSITION, &y);
    m_spec.x = (int)x;
    m_spec.y = (int)y;
    m_spec.z = 0;
    // FIXME? - TIFF spec describes the positions as in resolutionunit.
    // What happens if this is not unitless pixels?  Are we interpreting
    // it all wrong?

    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1) 
        m_spec.full_width = width;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1)
        m_spec.full_height = height;

    if (TIFFIsTiled (m_tif)) {
        TIFFGetField (m_tif, TIFFTAG_TILEWIDTH, &m_spec.tile_width);
        TIFFGetField (m_tif, TIFFTAG_TILELENGTH, &m_spec.tile_height);
        TIFFGetFieldDefaulted (m_tif, TIFFTAG_TILEDEPTH, &m_spec.tile_depth);
    } else {
        m_spec.tile_width = 0;
        m_spec.tile_height = 0;
        m_spec.tile_depth = 0;
    }

    m_bitspersample = 8;
    TIFFGetField (m_tif, TIFFTAG_BITSPERSAMPLE, &m_bitspersample);
    m_spec.attribute ("BitsPerSample", m_bitspersample);

    unsigned short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (m_bitspersample) {
    case 1:
        // Make 1bpp look like byte images
    case 4:
        // Make 4 bpp look like byte images
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (PT_UINT8);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (PT_INT8);
        else m_spec.set_format (PT_UINT8);  // punt
        break;
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (PT_UINT16);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (PT_INT16);
        break;
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format (PT_FLOAT);
        break;
    default:
        m_spec.set_format (PT_UNKNOWN);
        break;
    }

    m_photometric = (m_spec.nchannels == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
    TIFFGetField (m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric);
    m_spec.attribute ("tiff:PhotometricInterpretation", m_photometric);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Read the color map
        unsigned short *r = NULL, *g = NULL, *b = NULL;
        TIFFGetField (m_tif, TIFFTAG_COLORMAP, &r, &g, &b);
        ASSERT (r != NULL && g != NULL && b != NULL);
        m_colormap.clear ();
        m_colormap.insert (m_colormap.end(), r, r + (1 << m_bitspersample));
        m_colormap.insert (m_colormap.end(), g, g + (1 << m_bitspersample));
        m_colormap.insert (m_colormap.end(), b, b + (1 << m_bitspersample));
        // Palette TIFF images are always 3 channels (to the client)
        m_spec.nchannels = 3;
    }

    TIFFGetFieldDefaulted (m_tif, TIFFTAG_PLANARCONFIG, &m_planarconfig);
    m_spec.attribute ("tiff:PlanarConfiguration", m_planarconfig);
    if (m_planarconfig == PLANARCONFIG_SEPARATE)
        m_spec.attribute ("planarconfig", "separate");
    else
        m_spec.attribute ("planarconfig", "contig");

    short compress;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_COMPRESSION, &compress);
    m_spec.attribute ("tiff:Compression", compress);
    switch (compress) {
    case COMPRESSION_NONE :
        m_spec.attribute ("compression", "none");
        break;
    case COMPRESSION_LZW :
        m_spec.attribute ("compression", "lzw");
        break;
    case COMPRESSION_CCITTRLE :
        m_spec.attribute ("compression", "ccittrle");
        break;
    case COMPRESSION_ADOBE_DEFLATE :
        m_spec.attribute ("compression", "zip");
        break;
    case COMPRESSION_PACKBITS :
        m_spec.attribute ("compression", "packbits");
        break;
    default:
        break;
    }

    short resunit = -1;
    TIFFGetField (m_tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
    switch (resunit) {
    case RESUNIT_NONE : m_spec.attribute ("ResolutionUnit", "none"); break;
    case RESUNIT_INCH : m_spec.attribute ("ResolutionUnit", "in"); break;
    case RESUNIT_CENTIMETER : m_spec.attribute ("ResolutionUnit", "cm"); break;
    }

    get_string_attribute ("Artist", TIFFTAG_ARTIST);
    get_string_attribute ("ImageDescription", TIFFTAG_IMAGEDESCRIPTION);
    get_string_attribute ("Copyright", TIFFTAG_COPYRIGHT);
    get_string_attribute ("DateTime", TIFFTAG_DATETIME);
    get_string_attribute ("DocumentName", TIFFTAG_DOCUMENTNAME);
    get_float_attribute ("fovcot", TIFFTAG_PIXAR_FOVCOT);
    get_string_attribute ("HostComputer", TIFFTAG_HOSTCOMPUTER);
    get_string_attribute ("Make", TIFFTAG_MAKE);
    get_string_attribute ("Model", TIFFTAG_MODEL);
    get_short_attribute ("Orientation", TIFFTAG_ORIENTATION);
    get_string_attribute ("Software", TIFFTAG_SOFTWARE);
    get_string_attribute ("textureformat", TIFFTAG_PIXAR_TEXTUREFORMAT);
    get_float_attribute ("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, PT_MATRIX);
    get_float_attribute ("worldtoscreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, PT_MATRIX);
    get_string_attribute ("wrapmodes", TIFFTAG_PIXAR_WRAPMODES);
    get_float_attribute ("XResolution", TIFFTAG_XRESOLUTION);
    get_float_attribute ("YResolution", TIFFTAG_YRESOLUTION);

    get_string_attribute ("tiff:PageName", TIFFTAG_PAGENAME);
    get_short_attribute ("tiff:PageNumber", TIFFTAG_PAGENUMBER);
    get_int_attribute ("tiff:subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly? 

    // FIXME: Others to consider adding: 
    // Optional EXIF tags (exposuretime, fnumber, etc)?
    // FIXME: do we care about fillorder for 1-bit and 4-bit images?

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            m_spec.channelnames[c] = "z";
    }

    // N.B. we currently ignore the following TIFF fields:
    // ExtraSamples
    // GrayResponseCurve GrayResponseUnit
    // MaxSampleValue MinSampleValue
    // NewSubfileType RowsPerStrip SubfileType(deprecated)
    // Colorimetry fields
}



bool
TIFFInput::close ()
{
    if (m_tif)
        TIFFClose (m_tif);
    init();  // Reset to initial state
    return true;
}



/// Helper: Convert n pixels from separate (RRRGGGBBB) to contiguous
/// (RGBRGBRGB) planarconfig.
void
TIFFInput::separate_to_contig (int n, const unsigned char *separate,
                               unsigned char *contig)
{
    int channelbytes = m_spec.channel_bytes();
    for (int p = 0;  p < n;  ++p)                     // loop over pixels
        for (int c = 0;  c < m_spec.nchannels;  ++c)    // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                contig[(p*m_spec.nchannels+c)*channelbytes+i] =
                    separate[(c*n+p)*channelbytes+i];
}



void
TIFFInput::palette_to_rgb (int n, const unsigned char *palettepels,
                           unsigned char *rgb)
{
    DASSERT (m_spec.nchannels == 3);
    int entries = 1 << m_bitspersample;
    DASSERT (m_colormap.size() == 3*entries);
    if (m_bitspersample == 8) {
        for (int x = 0;  x < n;  ++x) {
            int i = (*palettepels++);
            *rgb++ = m_colormap[0*entries+i] / 257;
            *rgb++ = m_colormap[1*entries+i] / 257;
            *rgb++ = m_colormap[2*entries+i] / 257;
        }
    } else {
        // 4 bits per sample
        DASSERT (m_bitspersample == 4);
        for (int x = 0;  x < n;  ++x) {
            int i;
            if ((x & 1) == 0)
                i = (*palettepels >> 4);
            else
                i = (*palettepels++) & 0x0f;
            *rgb++ = m_colormap[0*entries+i] / 257;
            *rgb++ = m_colormap[1*entries+i] / 257;
            *rgb++ = m_colormap[2*entries+i] / 257;
        }
    }
}



void
TIFFInput::bilevel_to_8bit (int n, const unsigned char *bits,
                            unsigned char *bytes)
{
    for (int i = 0;  i < n;  ++i) {
        int b = bits[i/8] & (1 << (7 - (i&7)));
        bytes[i] = b ? 255 : 0;
    }
}



void
TIFFInput::fourbit_to_8bit (int n, const unsigned char *bits,
                            unsigned char *bytes)
{
    for (int i = 0;  i < n;  ++i) {
        int b = ((i & 1) == 0) ? (bits[i/2] >> 4) : (bits[i/2] & 15);
        bytes[i] = (unsigned char) ((b * 255) / 15);
    }
}



void
TIFFInput::invert_photometric (int n, void *data)
{
    switch (m_spec.format) {
    case PT_UINT8: {
        unsigned char *d = (unsigned char *) data;
        for (int i = 0;  i < n;  ++i)
            d[i] = 255 - d[i];
        break;
        }
    default:
        break;
    }
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= m_spec.y;
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (m_spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        m_scratch.resize (m_spec.scanline_bytes());
        int plane_bytes = m_spec.width * typesize(m_spec.format);
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            if (TIFFReadScanline (m_tif, &m_scratch[plane_bytes*c], y, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_bitspersample == 1 || m_bitspersample == 4) {
        // Bilevel images
        m_scratch.resize (m_spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        if (m_bitspersample == 1)
            bilevel_to_8bit (m_spec.width, &m_scratch[0], (unsigned char *)data);
        else
            fourbit_to_8bit (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else {
        // Contiguous, >= 8 bit per sample -- the "usual" case
        if (TIFFReadScanline (m_tif, data, y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (m_spec.width * m_spec.nchannels, data);

    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= m_spec.x;
    y -= m_spec.y;
    int tile_pixels = m_spec.tile_width * m_spec.tile_height 
                      * std::max (m_spec.tile_depth, 1);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (tile_pixels);
        if (TIFFReadTile (m_tif, &m_scratch[0], x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (tile_pixels, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        int plane_bytes = tile_pixels * typesize(m_spec.format);
        DASSERT (plane_bytes*m_spec.nchannels == m_spec.tile_bytes());
        m_scratch.resize (m_spec.tile_bytes());
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            if (TIFFReadTile (m_tif, &m_scratch[plane_bytes*c], x, y, z, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else {
        // Contiguous, >= 8 bit per sample -- the "usual" case
        if (TIFFReadTile (m_tif, data, x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (tile_pixels, data);

    return true;
}
