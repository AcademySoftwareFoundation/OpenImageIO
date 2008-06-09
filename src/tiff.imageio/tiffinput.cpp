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
    virtual const char * format_name (void) const { return "TIFF"; }
    virtual bool open (const char *name, ImageIOFormatSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);
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

    void init () {
        m_tif = NULL;
        m_colormap.clear();
    }

    // Read tags from m_tif and fill out spec
    void read ();

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
    void get_string_field (const std::string &name, int tag) {
        char *s = NULL;
        TIFFGetField (m_tif, tag, &s);
        if (s && *s)
            spec.add_parameter (name, PT_STRING, 1, &s);
    }

    // Get a float-oid tiff tag field and put it into extra_params
    void get_float_field (const std::string &name, int tag,
                          ParamBaseType type=PT_FLOAT) {
        float f[16];
        if (TIFFGetField (m_tif, tag, f))
            spec.add_parameter (name, type, 1, &f);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_field (const std::string &name, int tag) {
        int i;
        if (TIFFGetField (m_tif, tag, &i))
            spec.add_parameter (name, PT_INT, 1, &i);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_field (const std::string &name, int tag) {
        unsigned short s;
        if (TIFFGetField (m_tif, tag, &s)) {
            int i = s;
            spec.add_parameter (name, PT_INT, 1, &i);
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
TIFFInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    m_filename = name;
    m_subimage = -1;
    return seek_subimage (0, newspec);
}



bool
TIFFInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
    if (index < 0)       // Illegal
        return false;
    if (index == m_subimage) {
        // We're already pointing to the right subimage
        newspec = spec;
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
        read ();
        newspec = spec;
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
TIFFInput::read ()
{
    spec = ImageIOFormatSpec();
    float x = 0, y = 0;
    TIFFGetField (m_tif, TIFFTAG_XPOSITION, &x);
    TIFFGetField (m_tif, TIFFTAG_YPOSITION, &y);
    spec.x = (int)x;
    spec.y = (int)y;
    spec.z = 0;
    // FIXME? - TIFF spec describes the positions as in resolutionunit.
    // What happens if this is not unitless pixels?  Are we interpreting
    // it all wrong?

    uint32 width, height, depth;
    TIFFGetField (m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    spec.full_width  = spec.width  = width;
    spec.full_height = spec.height = height;
    spec.full_depth  = spec.depth  = depth;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1) 
        spec.full_width = width;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1)
        spec.full_height = height;

    if (TIFFIsTiled (m_tif)) {
        TIFFGetField (m_tif, TIFFTAG_TILEWIDTH, &spec.tile_width);
        TIFFGetField (m_tif, TIFFTAG_TILELENGTH, &spec.tile_height);
        TIFFGetFieldDefaulted (m_tif, TIFFTAG_TILEDEPTH, &spec.tile_depth);
    } else {
        spec.tile_width = 0;
        spec.tile_height = 0;
        spec.tile_depth = 0;
    }

    m_bitspersample = 8;
    TIFFGetField (m_tif, TIFFTAG_BITSPERSAMPLE, &m_bitspersample);
    spec.add_parameter ("bitspersample", m_bitspersample);

    unsigned short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (m_bitspersample) {
    case 1:
        // Make 1bpp look like byte images
    case 4:
        // Make 4 bpp look like byte images
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            spec.set_format (PT_UINT8);
        else if (sampleformat == SAMPLEFORMAT_INT)
            spec.set_format (PT_INT8);
        else spec.set_format (PT_UINT8);  // punt
        break;
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            spec.set_format (PT_UINT16);
        else if (sampleformat == SAMPLEFORMAT_INT)
            spec.set_format (PT_INT16);
        break;
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            spec.set_format (PT_FLOAT);
        break;
    default:
        spec.set_format (PT_UNKNOWN);
        break;
    }

    unsigned short nchans;
    TIFFGetField (m_tif, TIFFTAG_SAMPLESPERPIXEL, &nchans);
    spec.nchannels = nchans;

    spec.channelnames.clear();
    switch (spec.nchannels) {
    case 1:
        spec.channelnames.push_back ("a");
        break;
    case 2:
        spec.channelnames.push_back ("i");
        spec.channelnames.push_back ("a");
        spec.alpha_channel = 1;  // Is this a safe bet?
        break;
    case 3:
        spec.channelnames.push_back ("r");
        spec.channelnames.push_back ("g");
        spec.channelnames.push_back ("b");
        break;
    case 4:
        spec.channelnames.push_back ("r");
        spec.channelnames.push_back ("g");
        spec.channelnames.push_back ("b");
        spec.channelnames.push_back ("a");
        spec.alpha_channel = 3;  // Is this a safe bet?
        break;
    default:
        for (int c = 0;  c < spec.nchannels;  ++c)
            spec.channelnames.push_back ("");
    }

    m_photometric = (spec.nchannels == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
    TIFFGetField (m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric);
    spec.add_parameter ("tiff_PhotometricInterpretation", m_photometric);
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
        spec.nchannels = 3;
    }

    TIFFGetFieldDefaulted (m_tif, TIFFTAG_PLANARCONFIG, &m_planarconfig);
    spec.add_parameter ("tiff_PlanarConfiguration", m_planarconfig);
    if (m_planarconfig == PLANARCONFIG_SEPARATE)
        spec.add_parameter ("planarconfig", "separate");
    else
        spec.add_parameter ("planarconfig", "contig");

    short compress;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_COMPRESSION, &compress);
    spec.add_parameter ("tiff_Compression", compress);
    switch (compress) {
    case COMPRESSION_NONE :
        spec.add_parameter ("compression", "none");
        break;
    case COMPRESSION_LZW :
        spec.add_parameter ("compression", "lzw");
        break;
    case COMPRESSION_CCITTRLE :
        spec.add_parameter ("compression", "ccittrle");
        break;
    case COMPRESSION_ADOBE_DEFLATE :
        spec.add_parameter ("compression", "deflate");  // zip?
        break;
    case COMPRESSION_PACKBITS :
        spec.add_parameter ("compression", "packbits");  // zip?
        break;
    default:
        break;
    }

    short resunit = -1;
    TIFFGetField (m_tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
    switch (resunit) {
    case RESUNIT_NONE : spec.add_parameter ("resolutionunit", "none"); break;
    case RESUNIT_INCH : spec.add_parameter ("resolutionunit", "in"); break;
    case RESUNIT_CENTIMETER : spec.add_parameter ("resolutionunit", "cm"); break;
    }
    get_float_field ("xresolution", TIFFTAG_XRESOLUTION);
    get_float_field ("yresolution", TIFFTAG_YRESOLUTION);
    // FIXME: xresolution, yresolution -- N.B. they are rational

    get_string_field ("artist", TIFFTAG_ARTIST);
    get_string_field ("description", TIFFTAG_IMAGEDESCRIPTION);
    get_string_field ("copyright", TIFFTAG_COPYRIGHT);
    get_string_field ("datetime", TIFFTAG_DATETIME);
    get_string_field ("name", TIFFTAG_DOCUMENTNAME);
    get_float_field ("fovcot", TIFFTAG_PIXAR_FOVCOT);
    get_string_field ("host", TIFFTAG_HOSTCOMPUTER);
    get_string_field ("software", TIFFTAG_SOFTWARE);
    get_string_field ("textureformat", TIFFTAG_PIXAR_TEXTUREFORMAT);
    get_float_field ("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, PT_MATRIX);
    get_float_field ("worldtosreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, PT_MATRIX);
    get_string_field ("wrapmodes", TIFFTAG_PIXAR_WRAPMODES);
    get_string_field ("tiff_PageName", TIFFTAG_PAGENAME);
    get_short_field ("tiff_PageNumber", TIFFTAG_PAGENUMBER);
    get_int_field ("tiff_subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly? 

    // FIXME: Others to consider adding: 
    // Orientation ExtraSamples? NewSubfileType?
    // Optional EXIF tags (exposuretime, fnumber, etc)?
    // FIXME: do we care about fillorder for 1-bit and 4-bit images?

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < spec.nchannels;  ++c)
            spec.channelnames[c] = "z";
    }

    // N.B. we currently ignore the following TIFF fields:
    // Orientation ExtraSamples
    // GrayResponseCurve GrayResponseUnit
    // Make MaxSampleValue MinSampleValue
    // Model NewSubfileType RowsPerStrip SubfileType(deprecated)
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
    int channelbytes = spec.channel_bytes();
    for (int p = 0;  p < n;  ++p)                     // loop over pixels
        for (int c = 0;  c < spec.nchannels;  ++c)    // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                contig[(p*spec.nchannels+c)*channelbytes+i] =
                    separate[(c*n+p)*channelbytes+i];
}



void
TIFFInput::palette_to_rgb (int n, const unsigned char *palettepels,
                           unsigned char *rgb)
{
    DASSERT (spec.nchannels == 3);
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
    switch (spec.format) {
    case PT_UINT8:
        unsigned char *d = (unsigned char *) data;
        for (int i = 0;  i < n;  ++i)
            d[i] = 255 - d[i];
        break;
    default:
        break;
    }
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= spec.y;
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        m_scratch.resize (spec.scanline_bytes());
        int plane_bytes = spec.width * ParamBaseTypeSize(spec.format);
        for (int c = 0;  c < spec.nchannels;  ++c)
            if (TIFFReadScanline (m_tif, &m_scratch[plane_bytes*c], y, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_bitspersample == 1 || m_bitspersample == 4) {
        // Bilevel images
        m_scratch.resize (spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        if (m_bitspersample == 1)
            bilevel_to_8bit (spec.width, &m_scratch[0], (unsigned char *)data);
        else
            fourbit_to_8bit (spec.width, &m_scratch[0], (unsigned char *)data);
    } else {
        // Contiguous, >= bit per sample -- the "usual" case
        if (TIFFReadScanline (m_tif, data, y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (spec.width * spec.nchannels, data);

    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= spec.x;
    y -= spec.y;
    int tile_pixels = spec.tile_width * spec.tile_height 
                      * std::max (spec.tile_depth, 1);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (tile_pixels);
        if (TIFFReadTile (m_tif, &m_scratch[0], x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (tile_pixels, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        int plane_bytes = tile_pixels * ParamBaseTypeSize(spec.format);
        DASSERT (plane_bytes*spec.nchannels == spec.tile_bytes());
        m_scratch.resize (spec.tile_bytes());
        for (int c = 0;  c < spec.nchannels;  ++c)
            if (TIFFReadTile (m_tif, &m_scratch[plane_bytes*c], x, y, z, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (spec.width, &m_scratch[0], (unsigned char *)data);
    } else {
        // Contiguous, >= bit per sample -- the "usual" case
        if (TIFFReadTile (m_tif, data, x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (tile_pixels, data);

    return true;
}
