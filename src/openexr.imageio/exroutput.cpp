/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

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
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z,
                             TypeDesc format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);

private:
    Imf::Header *m_header;                ///< Ptr to image header
    Imf::OutputFile *m_output_scanline;   ///< Input for scanline files
    Imf::TiledOutputFile *m_output_tiled; ///< Input for tiled files
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage we're writing now
    int m_nsubimages;                     ///< How many subimages are there?
    Imf::PixelType m_pixeltype;           ///< Imf pixel type
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    // Initialize private members to pre-opened state
    void init (void) {
        m_header = NULL;
        m_output_scanline = NULL;
        m_output_tiled = NULL;
        m_subimage = -1;
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, TypeDesc type,
                        const void *data);
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageOutput *
openexr_output_imageio_create ()
{
    return new OpenEXROutput;
}

DLLEXPORT int openexr_imageio_version = IMAGEIO_VERSION;

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

    delete m_output_scanline;  m_output_scanline = NULL;
    delete m_output_tiled;  m_output_tiled = NULL;
    delete m_header;    m_header = NULL;
}



bool
OpenEXROutput::supports (const std::string &feature) const
{
    if (feature == "tiles")
        return true;
    if (feature == "multiimage")
        return true;

    // FIXME: we could support "empty"
    // Can EXR support "random"?

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open (const std::string &name, const ImageSpec &userspec, bool append)
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

    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               userspec.width, userspec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    if (m_spec.full_width <= 0)
        m_spec.full_width = m_spec.width;
    if (m_spec.full_height <= 0)
        m_spec.full_height = m_spec.height;

    // Force use of one of the three data types that OpenEXR supports
    switch (m_spec.format.basetype) {
    case TypeDesc::UINT:
        m_spec.format = TypeDesc::UINT;
        m_pixeltype = Imf::UINT;
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        m_spec.format = TypeDesc::FLOAT;
        m_pixeltype = Imf::FLOAT;
        break;
    default:
        // Everything else defaults to half
        m_spec.format = TypeDesc::HALF;
        m_pixeltype = Imf::HALF;
    }

    // Big FIXME: support per-channel formats?

    Imath::Box2i dataWindow (Imath::V2i (m_spec.x, m_spec.y),
                             Imath::V2i (m_spec.width + m_spec.x - 1,
                                         m_spec.height + m_spec.y - 1));
    Imath::Box2i displayWindow (Imath::V2i (m_spec.full_x, m_spec.full_y),
                                Imath::V2i (m_spec.full_width+m_spec.full_x-1,
                                            m_spec.full_height+m_spec.full_y-1));
    m_header = new Imf::Header (displayWindow, dataWindow);

    // Insert channels into the header.  Also give the channels names if
    // the user botched it.
    static const char *default_chan_names[] = { "R", "G", "B", "A" };
    m_spec.channelnames.resize (m_spec.nchannels);
    for (int c = 0;  c < m_spec.nchannels;  ++c) {
        if (m_spec.channelnames[c].empty())
            m_spec.channelnames[c] = (c<4) ? default_chan_names[c]
                                           : Strutil::format ("unknown %d", c);
        m_header->channels().insert (m_spec.channelnames[c].c_str(),
                                     Imf::Channel(m_pixeltype, 1, 1
#if OPENEXR_VERSION >= 010601
                                     , true
#endif
                                     ));
    }
    
    // Default to ZIP compression if no request came with the user spec.
    if (! m_spec.find_attribute("compression"))
        m_spec.attribute ("compression", "zip");

    // Automatically set date field if the client didn't supply it.
    if (! m_spec.find_attribute("DateTime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        localtime_r (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        m_spec.attribute ("DateTime", date);
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
        put_parameter (m_spec.extra_attribs[p].name().string(),
                       m_spec.extra_attribs[p].type(),
                       m_spec.extra_attribs[p].data());

    try {
        if (m_spec.tile_width) {
            m_header->setTileDescription (
                Imf::TileDescription (m_spec.tile_width, m_spec.tile_height,
                                      Imf::LevelMode(m_levelmode),
                                      Imf::LevelRoundingMode(m_roundingmode)));
            m_output_tiled = new Imf::TiledOutputFile (name.c_str(), *m_header);
        } else {
            m_output_scanline = new Imf::OutputFile (name.c_str(), *m_header);
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
OpenEXROutput::put_parameter (const std::string &name, TypeDesc type,
                              const void *data)
{
    // Translate
    std::string xname = name; // ooio_std_to_exr_tag[name];

    if (iequals(xname, "worldtocamera"))
        xname = "worldToCamera";
    else if (iequals(xname, "worldtoscreen"))
        xname = "worldToNDC";
    else if (iequals(xname, "DateTime"))
        xname = "capDate";
    else if (iequals(xname, "description") || iequals(xname, "ImageDescription"))
        xname = "comments";
    else if (iequals(xname, "Copyright"))
        xname = "owner";
    else if (iequals(xname, "PixelAspectRatio"))
        xname = "pixelAspectRatio";
    else if (iequals(xname, "ExposureTime"))
        xname = "expTime";
    else if (iequals(xname, "FNumber"))
        xname = "aperture";
    else if (istarts_with (xname, format_prefix))
        xname = std::string (xname.begin()+format_prefix.size(), xname.end());

//    std::cerr << "exr put '" << name << "' -> '" << xname << "'\n";

    // Special cases
    if (iequals(xname, "Compression") && type == TypeDesc::STRING) {
        int compress = Imf::ZIP_COMPRESSION;  // default
        const char *str = *(char **)data;
        m_header->compression() = Imf::ZIP_COMPRESSION;  // Default
        if (str) {
            if (! strcmp (str, "none"))
                m_header->compression() = Imf::NO_COMPRESSION;
            else if (! strcmp (str, "deflate") || ! strcmp (str, "zip")) 
                m_header->compression() = Imf::ZIP_COMPRESSION;
            else if (! strcmp (str, "rle")) 
                m_header->compression() = Imf::RLE_COMPRESSION;
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
    if (type == TypeDesc::INT || type == TypeDesc::UINT) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(int*)data));
        return true;
    }
    if (type == TypeDesc::INT16) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(short*)data));
        return true;
    }
    if (type == TypeDesc::UINT16) {
        m_header->insert (xname.c_str(), Imf::IntAttribute (*(unsigned short*)data));
        return true;
    }
    if (type == TypeDesc::FLOAT) {
        m_header->insert (xname.c_str(), Imf::FloatAttribute (*(float*)data));
        return true;
    }
    if (type == TypeDesc::HALF) {
        m_header->insert (xname.c_str(), Imf::FloatAttribute ((float)*(half*)data));
        return true;
    }
    if (type == PT_MATRIX) {
        m_header->insert (xname.c_str(), Imf::M44fAttribute (*(Imath::M44f*)data));
        return true;
    }
    if (type == TypeDesc::STRING) {
        m_header->insert (xname.c_str(), Imf::StringAttribute (*(char**)data));
        return true;
    }
    std::cerr << "Don't know what to do with " << type.c_str() << ' ' << xname << "\n";
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

    delete m_output_scanline;  m_output_scanline = NULL;
    delete m_output_tiled;  m_output_tiled = NULL;
    delete m_header;    m_header = NULL;

    init ();      // re-initialize
    return true;  // How can we fail?
}



bool
OpenEXROutput::write_scanline (int y, int z, TypeDesc format,
                               const void *data, stride_t xstride)
{
    m_spec.auto_stride (xstride, format, spec().nchannels);
    data = to_native_scanline (format, data, xstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - m_spec.x * m_spec.pixel_bytes() 
              - y * m_spec.scanline_bytes();
    // FIXME: Should it be scanline_bytes, or full_width*pixelsize?

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype,
                                            buf + c * m_spec.channel_bytes(),
                                            m_spec.pixel_bytes(),
                                            m_spec.scanline_bytes()));
            // FIXME - what if all channels aren't the same data type?
        }
        m_output_scanline->setFrameBuffer (frameBuffer);
        m_output_scanline->writePixels (1);
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
                           TypeDesc format, const void *data,
                           stride_t xstride, stride_t ystride, stride_t zstride)
{
    m_spec.auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        spec().tile_width, spec().tile_height);
    data = to_native_tile (format, data, xstride, ystride, zstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - x * m_spec.pixel_bytes() 
              - y * m_spec.pixel_bytes() * m_spec.tile_width;

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype,
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
