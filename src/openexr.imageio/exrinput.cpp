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

#include <ImfTestFile.h>
#include <ImfInputFile.h>
#include <ImfTiledInputFile.h>
#include <ImfChannelList.h>
//using namespace Imf;

#include "dassert.h"
#include "paramtype.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"

using namespace OpenImageIO;



class OpenEXRInput : public ImageInput {
public:
    OpenEXRInput () { init(); }
    virtual ~OpenEXRInput () { close(); }
    virtual const char * format_name (void) const { return "OpenEXR"; }
    virtual bool open (const char *name, ImageIOFormatSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    const Imf::Header *m_exr_header;  ///< Ptr to image header
    Imf::InputFile *m_input_scanline; ///< Input for scanline files
    Imf::TiledInputFile *m_input_tiled; ///< Input for tiled files
    std::string m_filename;          ///< Stash the filename
    int m_levelmode;                 ///< The level mode of the file
    int m_roundingmode;              ///< Rounding mode of the file
    int m_subimage;                  ///< What subimage are we looking at?
    int m_nsubimages;                ///< How many subimages are there?
    int m_topwidth;                  ///< Width of top mip level
    int m_topheight;                 ///< Height of top mip level
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    void init () {
        m_exr_header = NULL;
        m_input_scanline = NULL;
        m_input_tiled = NULL;
    }

    // Read tags from m_tif and fill out spec
    void read ();

    // Get a string tiff tag field and put it into extra_params
    void get_string_field (const std::string &name, int tag) {
#if 0
        char *s = NULL;
        TIFFGetField (m_tif, tag, &s);
        if (s && *s)
            spec.add_parameter (name, PT_STRING, 1, &s);
#endif
    }

    // Get a float-oid tiff tag field and put it into extra_params
    void get_float_field (const std::string &name, int tag,
                          ParamBaseType type=PT_FLOAT) {
#if 0
        float f[16];
        if (TIFFGetField (m_tif, tag, f))
            spec.add_parameter (name, type, 1, &f);
#endif
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_field (const std::string &name, int tag) {
#if 0
        int i;
        if (TIFFGetField (m_tif, tag, &i))
            spec.add_parameter (name, PT_INT, 1, &i);
#endif
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_field (const std::string &name, int tag) {
#if 0
        unsigned short s;
        if (TIFFGetField (m_tif, tag, &s)) {
            int i = s;
            spec.add_parameter (name, PT_INT, 1, &i);
        }
#endif
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT OpenEXRInput *
openexr_input_imageio_create ()
{
    return new OpenEXRInput;
}

// DLLEXPORT int imageio_version = IMAGEIO_VERSION; // it's in tiffoutput.cpp

DLLEXPORT const char * openexr_input_extensions[] = {
    "exr", NULL
};

};



// Someplace to store an error message from the OpenEXR error handler
//static std::string lasterr;
//static mutex lasterr_mutex;


bool
OpenEXRInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    // Quick check to reject non-exr files
    bool tiled;
    if (! Imf::isOpenExrFile (name, tiled))
        return false;

    spec = ImageIOFormatSpec();
    try {
        if (tiled) {
            m_input_tiled = new Imf::TiledInputFile (name);
            m_exr_header = &(m_input_tiled->header());
        } else {
            m_input_scanline = new Imf::InputFile (name);
            m_exr_header = &(m_input_scanline->header());
        }
    }
    catch (const std::exception &e) {
        error ("OpenEXR exception: %s", e.what());
        return false;
    }
    if (! m_input_scanline && ! m_input_tiled) {
        error ("Unknown error opening EXR file");
        return false;
    }

    Imath::Box2i datawindow = m_exr_header->dataWindow();
    spec.x = datawindow.min.x;
    spec.y = datawindow.min.y;
    spec.z = 0;
    spec.width = datawindow.max.x - datawindow.min.x + 1;
    spec.height = datawindow.max.y - datawindow.min.y + 1;
    spec.depth = 1;
    m_topwidth = spec.width;      // Save top-level mipmap dimensions
    m_topheight = spec.height;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    spec.tile_width = 0;
    spec.tile_height = 0;
    spec.tile_depth = 0;
    spec.format = PT_HALF;  // FIXME: do the right thing for non-half
    spec.nchannels = 0;
    const Imf::ChannelList &channels (m_exr_header->channels());
    Imf::ChannelList::ConstIterator ci;
    int c;
    for (c = 0, ci = channels.begin();  ci != channels.end();  ++c, ++ci) {
        // const Imf::Channel &channel = ci.channel();
        std::cerr << "Channel " << ci.name() << '\n';
        spec.channelnames.push_back (ci.name());
        if (ci.name() == "Z")
            spec.z_channel = c;
        if (ci.name() == "A" || ci.name() == "Alpha")
            spec.alpha_channel = c;
        ++spec.nchannels;
    }
    // FIXME: should we also figure out the layers?
    
    if (tiled) {
        // FIXME: levelmode
        m_levelmode = m_input_tiled->levelMode();
        m_roundingmode = m_input_tiled->levelRoundingMode();
        if (m_levelmode == Imf::MIPMAP_LEVELS)
            m_nsubimages = m_input_tiled->numLevels();
        else if (m_levelmode == Imf::RIPMAP_LEVELS)
            m_nsubimages = std::max (m_input_tiled->numXLevels(),
                                     m_input_tiled->numYLevels());
        else
            m_nsubimages = 1;
    } else {
        m_levelmode = Imf::ONE_LEVEL;
        m_nsubimages = 1;
    }
    m_subimage = 0;
    newspec = spec;

    return true;
}



bool
OpenEXRInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
#if 0
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
#endif
}



void
OpenEXRInput::read ()
{
#if 0
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
#endif
}



bool
OpenEXRInput::close ()
{
#if 0
    if (m_tif)
        TIFFClose (m_tif);
    init();  // Reset to initial state
    return true;
#endif
}



bool
OpenEXRInput::read_native_scanline (int y, int z, void *data)
{
    y -= spec.y;
#if 0
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

    return true;
#endif
}



bool
OpenEXRInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= spec.x;
    y -= spec.y;
#if 0
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
#endif

    return true;
}
