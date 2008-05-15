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
    TIFFInput () 
        : m_tif(NULL), m_planarconfig(PLANARCONFIG_CONTIG), m_subimage(-1)
    { }

    virtual ~TIFFInput () { close(); }

    virtual bool open (const char *name, ImageIOFormatSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    TIFF *m_tif;
    std::string m_filename;
    std::vector<char> m_scratch;
    int m_planarconfig;
    int m_subimage;

    // Read tags from m_tif and fill out spec
    void read ();

    // Convert planar separate to contiguous data format
    void separate_to_contig (int n, const char *separate, char *separate);

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
    void get_int_field (const std::string &name, int tag,
                              ParamBaseType type=PT_INT) {
        int i;
        if (TIFFGetField (m_tif, tag, &i))
            spec.add_parameter (name, type, 1, &i);
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
    float x = 0, y = 0;
    TIFFGetField (m_tif, TIFFTAG_XPOSITION, &x);
    TIFFGetField (m_tif, TIFFTAG_YPOSITION, &y);
    spec.x = (int)x;
    spec.y = (int)y;
    spec.z = 0;
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

    short bps = 8;
    TIFFGetField (m_tif, TIFFTAG_BITSPERSAMPLE, &bps);
    short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetField (m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (bps) {
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            spec.format = PT_UINT8;
        else if (sampleformat == SAMPLEFORMAT_INT)
            spec.format = PT_INT8;
        break;
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            spec.format = PT_UINT16;
        else if (sampleformat == SAMPLEFORMAT_INT)
            spec.format = PT_INT16;
        break;
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            spec.format = PT_FLOAT;
        break;
    default:
        spec.format = PT_UNKNOWN;
        break;
    }

    short nchans;
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

    get_string_field ("Artist", TIFFTAG_ARTIST);
    get_string_field ("comments", TIFFTAG_IMAGEDESCRIPTION); // Synonym for ImageDescription
    get_string_field ("copyright", TIFFTAG_COPYRIGHT);
    get_string_field ("DocumentName", TIFFTAG_DOCUMENTNAME);
    get_float_field ("fovcot", TIFFTAG_PIXAR_FOVCOT);
    get_string_field ("HostComputer", TIFFTAG_HOSTCOMPUTER);
    get_string_field ("ImageDescription", TIFFTAG_IMAGEDESCRIPTION);
    get_string_field ("software", TIFFTAG_SOFTWARE);
    get_int_field ("subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly? Also, should handle "NewSubfileType" that renders
    // the old SubfileType obsolete.
    get_string_field ("textureformat", TIFFTAG_PIXAR_TEXTUREFORMAT);
    get_float_field ("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, PT_MATRIX);
    get_float_field ("worldtosreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, PT_MATRIX);
    get_string_field ("wrapmodes", TIFFTAG_PIXAR_WRAPMODES);

    // FIXME: Others to consider adding: PhotometricInterpretation
    // XResolution YResolution ResolutionUnit Compression DateTime Orientation
    // PlanarConfiguration
    // Optional EXIF tags (exposuretime, fnumber, etc)?

    // FIXME: we probably do the entirely wrong thing for palette images

    // FIXME: look for ExtraSamples?

    short pc;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_PLANARCONFIG, &pc);
    m_planarconfig = pc;

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < spec.nchannels;  ++c)
            spec.channelnames[c] = "z";
    }
}



bool
TIFFInput::close ()
{
    if (m_tif) {
        TIFFClose (m_tif);
        m_tif = NULL;
    }
    m_subimage = -1;
    return true;
}



/// Helper: Convert n pixels from separate (RRRGGGBBB) to contiguous
/// (RGBRGBRGB) planarconfig.
void
TIFFInput::separate_to_contig (int n, const char *separate, char *contig)
{
    int channelbytes = spec.channel_bytes();
    for (int p = 0;  p < n;  ++p)                     // loop over pixels
        for (int c = 0;  c < spec.nchannels;  ++c)    // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                contig[(p*spec.nchannels+c)*channelbytes+i] =
                    separate[(c*n+p)*channelbytes+i];
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= spec.y;
    if (m_planarconfig == PLANARCONFIG_SEPARATE && spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        m_scratch.resize (spec.scanline_bytes());
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        separate_to_contig (spec.width, &m_scratch[0], (char *)data);
    } else {
        // No separate->contig is necessary.
        if (TIFFReadScanline (m_tif, data, y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }
    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= spec.x;
    y -= spec.y;
    if (m_planarconfig == PLANARCONFIG_SEPARATE && spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        int tile_pixels = spec.tile_width * spec.tile_height 
                            * std::max (spec.tile_depth, 1);
        int plane_bytes = tile_pixels * ParamBaseTypeSize(spec.format);
        DASSERT (plane_bytes*spec.nchannels == spec.tile_bytes());
        m_scratch.resize (spec.tile_bytes());
        for (int c = 0;  c < spec.nchannels;  ++c)
            if (TIFFReadTile (m_tif, &m_scratch[plane_bytes*c], x, y, z, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (spec.width, &m_scratch[0], (char *)data);
    } else {
        // No separate->contig is necessary.
        if (TIFFReadTile (m_tif, data, x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }
    return true;
}
