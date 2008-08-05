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
#include <map>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;
using boost::algorithm::istarts_with;

#include <tiffio.h>

#include <ImfOutputFile.h>
#include <ImfTiledOutputFile.h>
#include <ImfChannelList.h>
#include <ImfEnvmap.h>
#include <ImfIntAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfCompressionAttribute.h>

#include "dassert.h"
#include "imageio.h"
#include "strutil.h"


using namespace OpenImageIO;


class OpenEXROutput : public ImageOutput {
public:
    OpenEXROutput ();
    virtual ~OpenEXROutput ();
    virtual const char * format_name (void) const { return "openexr"; }
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
    Imf::Header *m_header;                ///< Ptr to image header
    Imf::OutputFile *m_output_scanline;   ///< Input for scanline files
    Imf::TiledOutputFile *m_output_tiled; ///< Input for tiled files
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage we're writing now
    int m_nsubimages;                     ///< How many subimages are there?
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    // Initialize private members to pre-opened state
    void init (void) {
        m_header = NULL;
        m_output_scanline = NULL;
        m_output_tiled = NULL;
        m_subimage = -1;
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, ParamBaseType type,
                        const void *data);
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT OpenEXROutput *
openexr_output_imageio_create ()
{
    return new OpenEXROutput;
}

DLLEXPORT int imageio_version = IMAGEIO_VERSION;

DLLEXPORT const char * openexr_output_extensions[] = {
    "exr", NULL
};

};



static std::string format_string ("openexr");
static std::string format_prefix ("openexr_");



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

    // FIXME: we could support "empty"
    // Can EXR support "random"?

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open (const char *name, const ImageIOFormatSpec &userspec,
                     bool append)
{
    if (append && (m_output_scanline || m_output_tiled)) {
        // Special case for appending to an open file -- we don't need
        // to close and reopen
        if (m_spec.tile_width && m_levelmode != Imf::ONE_LEVEL) {
            // OpenEXR does not support differing tile sizes on different
            // MIP-map levels.  Reject the open() if not using the original
            // tile sizes.
            if (userspec.tile_width != m_spec.tile_width ||
                userspec.tile_height != m_spec.tile_height) {
                error ("OpenEXR tiles must have the same size on all MIPmap levels");
                return false;
            }
            // Copy the new subimage size.  Keep everything else from the
            // original level.
            m_spec.width = userspec.width;
            m_spec.height = userspec.height;
            // N.B. do we need to copy anything else from userspec?
            ++m_subimage;
            return true;
        }
    }

    m_spec = userspec;  // Stash the spec

    if (! m_spec.full_width)
        m_spec.full_width = m_spec.width;
    if (! m_spec.full_height)
        m_spec.full_height = m_spec.height;

    m_spec.format = PT_HALF;
    // FIXME: support float and uint32
    // Big FIXME: support per-channel formats?


    Imath::Box2i dataWindow (Imath::V2i (m_spec.x, m_spec.y),
                             Imath::V2i (m_spec.width + m_spec.x - 1,
                                         m_spec.height + m_spec.y - 1));
    m_header = new Imf::Header (m_spec.full_width, m_spec.full_height, dataWindow);

    // Insert channels into the header.  Also give the channels names if
    // the user botched it.
    static const char *default_chan_names[] = { "R", "G", "B", "A" };
    m_spec.channelnames.resize (m_spec.nchannels);
    for (int c = 0;  c < m_spec.nchannels;  ++c) {
        if (m_spec.channelnames[c].empty())
            m_spec.channelnames[c] = (c<4) ? default_chan_names[c]
                                           : Strutil::format ("unknown %d", c);
        m_header->channels().insert (m_spec.channelnames[c].c_str(),
                                     Imf::Channel(Imf::HALF, 1, 1, true));
    }
    
    // Default to ZIP compression if no request came with the user spec.
    if (! m_spec.find_attribute("compression"))
        m_spec.attribute ("compression", "zip");

    // Automatically set date field if the client didn't supply it.
    if (! m_spec.find_attribute("datetime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        localtime_r (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        m_spec.attribute ("datetime", date);
    }

    m_nsubimages = 1;
    m_subimage = 0;

    // Figure out if we are a mipmap or an environment map
    ImageIOParameter *param = m_spec.find_attribute ("textureformat");
    const char *textureformat = param ? *(char **)param->data() : NULL;
    m_levelmode = Imf::ONE_LEVEL;  // Default to no MIP-mapping
    m_roundingmode = Imf::ROUND_UP;     // Force rounding up mode
    if (textureformat) {
        if (! strcmp (textureformat, "Plain Texture")) {
            m_levelmode = Imf::MIPMAP_LEVELS;
        } else if (! strcmp (textureformat, "CubeFace Environment")) {
            m_levelmode = Imf::MIPMAP_LEVELS;
            m_header->insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_CUBE));
        } else if (! strcmp (textureformat, "LatLong Environment")) {
            m_levelmode = Imf::MIPMAP_LEVELS;
            m_header->insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_LATLONG));
        } else if (! strcmp (textureformat, "Shadow")) {
            m_levelmode = Imf::ONE_LEVEL;
        }

        if (m_levelmode == Imf::MIPMAP_LEVELS) {
            // Compute how many subimages there will be
            int w = m_spec.width;
            int h = m_spec.height;
            while (w > 1 && h > 1) {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
                ++m_nsubimages;
            }
        }
    }

    // Deal with all other params
    for (size_t p = 0;  p < m_spec.extra_attribs.size();  ++p)
        put_parameter (m_spec.extra_attribs[p].name, m_spec.extra_attribs[p].type,
                       m_spec.extra_attribs[p].data());

    try {
        if (m_spec.tile_width) {
            m_header->setTileDescription (
                Imf::TileDescription (m_spec.tile_width, m_spec.tile_height,
                                      Imf::LevelMode(m_levelmode),
                                      Imf::LevelRoundingMode(m_roundingmode)));
            m_output_tiled = new Imf::TiledOutputFile (name, *m_header);
        } else {
            m_output_scanline = new Imf::OutputFile (name, *m_header);
        }
    }
    catch (const std::exception &e) {
        error ("OpenEXR exception: %s", e.what());
        m_output_scanline = NULL;
        return false;
    }
    if (! m_output_scanline && ! m_output_tiled) {
        error ("Unknown error opening EXR file");
        return false;
    }

    return true;
}



bool
OpenEXROutput::put_parameter (const std::string &name, ParamBaseType type,
                              const void *data)
{
    // Translate
    std::string xname = name; // ooio_std_to_exr_tag[name];

    if (iequals(xname, "worldtocamera"))
        xname = "cameraTransform";
    else if (iequals(xname, "DateTime"))
        xname = "capDate";
    else if (iequals(xname, "description") || iequals(xname, "ImageDescription"))
        xname = "comments";
    else if (iequals(xname, "Copyright"))
        xname = "owner";
    else if (iequals(xname, "pixelaspectratio"))
        xname = "pixelAspectRatio";
    else if (iequals(xname, "ExposureTime"))
        xname = "expTime";
    else if (iequals(xname, "FNumber"))
        xname = "aperture";
    else if (istarts_with (xname, format_prefix))
        xname = std::string (xname.begin()+format_prefix.size(), xname.end());

//    std::cerr << "exr put '" << name << "' -> '" << xname << "'\n";

    // Special cases
    if (iequals(xname, "Compression") && type == PT_STRING) {
        int compress = COMPRESSION_LZW;  // default
        const char *str = *(char **)data;
        m_header->compression() = Imf::ZIP_COMPRESSION;  // Default
        if (str) {
            if (! strcmp (str, "none"))
                m_header->compression() = Imf::NO_COMPRESSION;
            else if (! strcmp (str, "deflate") || ! strcmp (str, "zip")) 
                m_header->compression() = Imf::ZIP_COMPRESSION;
            else if (! strcmp (str, "zips")) 
                m_header->compression() = Imf::ZIPS_COMPRESSION;
            else if (! strcmp (str, "piz")) 
                m_header->compression() = Imf::PIZ_COMPRESSION;
            else if (! strcmp (str, "pxr24")) 
                m_header->compression() = Imf::PXR24_COMPRESSION;
        }
        return true;
    }

    // General handling of attributes
    // FIXME -- police this if we ever allow arrays
    if (type == PT_INT || type == PT_UINT) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(int*)data));
        return true;
    }
    if (type == PT_INT16) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(short*)data));
        return true;
    }
    if (type == PT_UINT16) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(unsigned short*)data));
        return true;
    }
    if (type == PT_FLOAT) {
        m_header->insert (xname.c_str(), Imf::FloatAttribute (*(float*)data));
        return true;
    }
    if (type == PT_HALF) {
        m_header->insert (xname.c_str(), Imf::FloatAttribute ((float)*(half*)data));
        return true;
    }
    if (type == PT_MATRIX) {
        m_header->insert (xname.c_str(), Imf::M44fAttribute (*(Imath::M44f*)data));
        return true;
    }
    if (type == PT_STRING) {
        m_header->insert (xname.c_str(), Imf::StringAttribute (*(char**)data));
        return true;
    }
    std::cerr << "Don't know what to do with " << type << ' ' << xname << "\n";
    return false;
}



bool
OpenEXROutput::close ()
{
    // FIXME: if the use pattern for mipmaps is open(), open(append),
    // ... close(), then we don't have to leave the file open with this
    // trickery.  That's only necessary if it's open(), close(),
    // open(append), close(), ...

    if (m_levelmode != Imf::ONE_LEVEL) {
        // Leave MIP-map files open, since appending cannot be done via
        // a re-open like it can with TIFF files.
        return true;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
}



bool
OpenEXROutput::write_scanline (int y, int z, ParamBaseType format,
                               const void *data, stride_t xstride)
{
    // FIXME: hard-coded for HALF!

    m_spec.auto_stride (xstride, format, spec().nchannels);
    data = to_native_scanline (format, data, xstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - m_spec.x * m_spec.pixel_bytes() 
              - (y + m_spec.y) * m_spec.scanline_bytes();
    // FIXME: Should it be scanline_bytes, or full_width*pixelsize?

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (Imf::HALF,  // FIXME
                                            buf + c * m_spec.channel_bytes(),
                                            m_spec.pixel_bytes(),
                                            m_spec.scanline_bytes()));
            // FIXME - what if all channels aren't the same data type?
        }
        m_output_scanline->setFrameBuffer (frameBuffer);
        m_output_scanline->writePixels (1);  // FIXME - is this the right call?
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    // FIXME -- can we checkpoint the file?

    return true;
}



bool
OpenEXROutput::write_tile (int x, int y, int z,
                           ParamBaseType format, const void *data,
                           stride_t xstride, stride_t ystride, stride_t zstride)
{
    std::cerr << "write_tile " << x << ' ' << y << ' ' << z << "\n";
    m_spec.auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        spec().tile_width, spec().tile_height);
    data = to_native_tile (format, data, xstride, ystride, zstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - (x + m_spec.x) * m_spec.pixel_bytes() 
              - (y + m_spec.y) * m_spec.pixel_bytes() * m_spec.tile_width;

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (Imf::HALF,  // FIXME
                                            buf + c * m_spec.channel_bytes(),
                                            m_spec.pixel_bytes(),
                                            m_spec.pixel_bytes()*m_spec.tile_width));
            // FIXME - what if all channels aren't the same data type?
        }
        m_output_tiled->setFrameBuffer (frameBuffer);
        m_output_tiled->writeTile ((x - m_spec.x) / m_spec.tile_width,
                                   (y - m_spec.y) / m_spec.tile_height,
                                   m_subimage, m_subimage);
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    return true;
}
