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
#include <map>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;
using boost::algorithm::istarts_with;

#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfTiledOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfEnvmap.h>
#include <OpenEXR/ImfIntAttribute.h>
#include <OpenEXR/ImfFloatAttribute.h>
#include <OpenEXR/ImfMatrixAttribute.h>
#include <OpenEXR/ImfVecAttribute.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfEnvmapAttribute.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfCRgbaFile.h>   // JUST to get symbols to figure out version!
#ifdef IMF_B44_COMPRESSION
#define OPENEXR_VERSION_IS_1_6_OR_LATER
#endif

#include "dassert.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "sysutil.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


class OpenEXROutput : public ImageOutput {
public:
    OpenEXROutput ();
    virtual ~OpenEXROutput ();
    virtual const char * format_name (void) const { return "openexr"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_scanlines (int ybegin, int yend, int z,
                                  TypeDesc format, const void *data,
                                  stride_t xstride, stride_t ystride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
    virtual bool write_tiles (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              const void *data, stride_t xstride,
                              stride_t ystride, stride_t zstride);

private:
    Imf::Header *m_header;                ///< Ptr to image header
    Imf::OutputFile *m_output_scanline;   ///< Input for scanline files
    Imf::TiledOutputFile *m_output_tiled; ///< Input for tiled files
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage we're writing now
    int m_nsubimages;                     ///< How many subimages are there?
    int m_miplevel;                       ///< What miplevel we're writing now
    int m_nmiplevels;                     ///< How many mip levels are there?
    std::vector<Imf::PixelType> m_pixeltype; ///< Imf pixel type for each chan
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    // Initialize private members to pre-opened state
    void init (void) {
        m_header = NULL;
        m_output_scanline = NULL;
        m_output_tiled = NULL;
        m_subimage = -1;
        m_miplevel = -1;
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, TypeDesc type,
                        const void *data);
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *
openexr_output_imageio_create ()
{
    return new OpenEXROutput;
}

DLLEXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION;

DLLEXPORT const char * openexr_output_extensions[] = {
    "exr", NULL
};

OIIO_PLUGIN_EXPORTS_END



static std::string format_string ("openexr");
static std::string format_prefix ("openexr_");


namespace pvt {
    void set_exr_threads ();
}



OpenEXROutput::OpenEXROutput ()
{
    pvt::set_exr_threads ();
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
    if (feature == "mipmap")
        return true;
    if (feature == "channelformats")
        return true;
    if (feature == "displaywindow")
        return true;

    // EXR supports random write order iff lineOrder is set to 'random Y'
    if (feature == "random_access") {
        const ImageIOParameter *param = m_spec.find_attribute("openexr:lineOrder");
        const char *lineorder = param ? *(char **)param->data() : NULL;
        return (lineorder && iequals (lineorder, "randomY"));
    }

    // FIXME: we could support "empty"

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open (const std::string &name, const ImageSpec &userspec,
                     OpenMode mode)
{
    if (mode == AppendSubimage) {
        error ("%s does not support subimages", format_name());
        return false;
    }

    if (mode == AppendMIPLevel && (m_output_scanline || m_output_tiled)) {
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
            // Copy the new mip level size.  Keep everything else from the
            // original level.
            m_spec.width = userspec.width;
            m_spec.height = userspec.height;
            // N.B. do we need to copy anything else from userspec?
            ++m_miplevel;
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
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        m_spec.format = TypeDesc::FLOAT;
        break;
    default:
        // Everything else defaults to half
        m_spec.format = TypeDesc::HALF;
    }

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
        TypeDesc format = m_spec.channelformats.size() ?
                                  m_spec.channelformats[c] : m_spec.format;
        Imf::PixelType ptype;
        switch (format.basetype) {
        case TypeDesc::UINT:
            ptype = Imf::UINT;
            format = TypeDesc::UINT;
            break;
        case TypeDesc::FLOAT:
        case TypeDesc::DOUBLE:
            ptype = Imf::FLOAT;
            format = TypeDesc::FLOAT;
            break;
        default:
            // Everything else defaults to half
            ptype = Imf::HALF;
            format = TypeDesc::HALF;
        }
        
#ifdef OPENEXR_VERSION_IS_1_6_OR_LATER
        // Hint to lossy compression methods that indicates whether
        // human perception of the quantity represented by this channel
        // is closer to linear or closer to logarithmic.  Compression
        // methods may optimize image quality by adjusting pixel data
        // quantization acording to this hint.
        
        bool pLinear = iequals (m_spec.get_string_attribute ("oiio:ColorSpace", "Linear"), "Linear");
#endif
        m_pixeltype.push_back (ptype);
        if (m_spec.channelformats.size())
            m_spec.channelformats[c] = format;
        m_header->channels().insert (m_spec.channelnames[c].c_str(),
                                     Imf::Channel(ptype, 1, 1
#ifdef OPENEXR_VERSION_IS_1_6_OR_LATER
                                     , pLinear
#endif
                                     ));
    }
    ASSERT (m_pixeltype.size() == (size_t)m_spec.nchannels);

    // Default to ZIP compression if no request came with the user spec.
    if (! m_spec.find_attribute("compression"))
        m_spec.attribute ("compression", "zip");

    // Default to increasingY line order, same as EXR.
    if (! m_spec.find_attribute("openexr:lineOrder"))
        m_spec.attribute ("openexr:lineOrder", "increasingY");

    // Automatically set date field if the client didn't supply it.
    if (! m_spec.find_attribute("DateTime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        Sysutil::get_local_time (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        m_spec.attribute ("DateTime", date);
    }

    m_nsubimages = 1;
    m_subimage = 0;
    m_nmiplevels = 1;
    m_miplevel = 0;

    // Figure out if we are a mipmap or an environment map
    ImageIOParameter *param = m_spec.find_attribute ("textureformat");
    const char *textureformat = param ? *(char **)param->data() : NULL;
    m_levelmode = Imf::ONE_LEVEL;  // Default to no MIP-mapping
    m_roundingmode = m_spec.get_int_attribute ("openexr:roundingmode",
                                               Imf::ROUND_DOWN);

    if (textureformat) {
        if (iequals (textureformat, "Plain Texture")) {
            m_levelmode = m_spec.get_int_attribute ("openexr:levelmode",
                                                    Imf::MIPMAP_LEVELS);
        } else if (iequals (textureformat, "CubeFace Environment")) {
            m_levelmode = m_spec.get_int_attribute ("openexr:levelmode",
                                                    Imf::MIPMAP_LEVELS);
            m_header->insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_CUBE));
        } else if (iequals (textureformat, "LatLong Environment")) {
            m_levelmode = m_spec.get_int_attribute ("openexr:levelmode",
                                                    Imf::MIPMAP_LEVELS);
            m_header->insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_LATLONG));
        } else if (iequals (textureformat, "Shadow")) {
            m_levelmode = Imf::ONE_LEVEL;  // Force one level for shadow maps
        }

        if (m_levelmode == Imf::MIPMAP_LEVELS) {
            // Compute how many mip levels there will be
            int w = m_spec.width;
            int h = m_spec.height;
            while (w > 1 && h > 1) {
                if (m_roundingmode == Imf::ROUND_DOWN) {
                    w = w / 2;
                    h = h / 2;
                } else {
                    w = (w + 1) / 2;
                    h = (h + 1) / 2;
                }
                w = std::max (1, w);
                h = std::max (1, h);
                ++m_nmiplevels;
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
    std::string xname = name;
    if (istarts_with (xname, "oiio:"))
        return false;
    else if (iequals(xname, "worldtocamera"))
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
        const char *str = *(char **)data;
        m_header->compression() = Imf::ZIP_COMPRESSION;  // Default
        if (str) {
            if (iequals (str, "none"))
                m_header->compression() = Imf::NO_COMPRESSION;
            else if (iequals (str, "deflate") || iequals (str, "zip")) 
                m_header->compression() = Imf::ZIP_COMPRESSION;
            else if (iequals (str, "rle")) 
                m_header->compression() = Imf::RLE_COMPRESSION;
            else if (iequals (str, "zips")) 
                m_header->compression() = Imf::ZIPS_COMPRESSION;
            else if (iequals (str, "piz")) 
                m_header->compression() = Imf::PIZ_COMPRESSION;
            else if (iequals (str, "pxr24")) 
                m_header->compression() = Imf::PXR24_COMPRESSION;
#ifdef IMF_B44_COMPRESSION
            // The enum Imf::B44_COMPRESSION is not defined in older versions
            // of OpenEXR, and there are no explicit version numbers in the
            // headers.  BUT this other related #define is present only in
            // the newer version.
            else if (iequals (str, "b44"))
                m_header->compression() = Imf::B44_COMPRESSION;
            else if (iequals (str, "b44a"))
                m_header->compression() = Imf::B44A_COMPRESSION;
#endif
        }
        return true;
    }

    if (iequals (xname, "openexr:lineOrder") && type == TypeDesc::STRING) {
        const char *str = *(char **)data;
        m_header->lineOrder() = Imf::INCREASING_Y;   // Default
        if (str) {
            if (iequals (str, "randomY"))
                m_header->lineOrder() = Imf::RANDOM_Y;
            else if (iequals (str, "decreasingY"))
                m_header->lineOrder() = Imf::DECREASING_Y;
        }
        return true;
    }

    // Supress planarconfig!
    if (iequals (xname, "planarconfig") || iequals (xname, "tiff:planarconfig"))
        return true;

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
    if (type == TypeDesc::TypeMatrix) {
        m_header->insert (xname.c_str(), Imf::M44fAttribute (*(Imath::M44f*)data));
        return true;
    }
    if (type == TypeDesc::TypeString) {
        m_header->insert (xname.c_str(), Imf::StringAttribute (*(char**)data));
        return true;
    }
    if (type == TypeDesc::TypeVector) {
        m_header->insert (xname.c_str(), Imf::V3fAttribute (*(Imath::V3f*)data));
        return true;
    }

#ifdef DEBUG
    std::cerr << "Don't know what to do with " << type.c_str() << ' ' << xname << "\n";
#endif

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
    bool native = (format == TypeDesc::UNKNOWN);
    size_t pixel_bytes = m_spec.pixel_bytes (true);  // native
    if (native && xstride == AutoStride)
        xstride = (stride_t) pixel_bytes;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    data = to_native_scanline (format, data, xstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    imagesize_t scanlinebytes = m_spec.scanline_bytes (native);
    char *buf = (char *)data
              - m_spec.x * pixel_bytes
              - y * scanlinebytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            size_t chanbytes = m_spec.channelformats.size() 
                                  ? m_spec.channelformats[c].size() 
                                  : m_spec.format.size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype[c],
                                            buf + chanoffset,
                                            pixel_bytes, scanlinebytes));
            chanoffset += chanbytes;
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
OpenEXROutput::write_scanlines (int ybegin, int yend, int z,
                                TypeDesc format, const void *data,
                                stride_t xstride, stride_t ystride)
{
    yend = std::min (yend, spec().y+spec().height);
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t scanlinebytes = spec().scanline_bytes(native);
    size_t pixel_bytes = m_spec.pixel_bytes (native);
    if (native && xstride == AutoStride)
        xstride = (stride_t) pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        m_spec.width, m_spec.height);

    const imagesize_t limit = 16*1024*1024;   // Allocate 16 MB, or 1 scanline
    int chunk = std::max (1, int(limit / scanlinebytes));

    bool ok = true;
    for ( ;  ok && ybegin < yend;  ybegin += chunk) {
        int y1 = std::min (ybegin+chunk, yend);
        int nscanlines = y1 - ybegin;
        const void *d = to_native_rectangle (m_spec.x, m_spec.x+m_spec.width,
                                             ybegin, y1, z, z+1, format, data,
                                             xstride, ystride, zstride,
                                             m_scratch);

        // Compute where OpenEXR needs to think the full buffers starts.
        // OpenImageIO requires that 'data' points to where the client wants
        // to put the pixels being read, but OpenEXR's frameBuffer.insert()
        // wants where the address of the "virtual framebuffer" for the
        // whole image.
        char *buf = (char *)d
                  - m_spec.x * pixel_bytes
                  - ybegin * scanlinebytes;
        try {
            Imf::FrameBuffer frameBuffer;
            size_t chanoffset = 0;
            for (int c = 0;  c < m_spec.nchannels;  ++c) {
                size_t chanbytes = m_spec.channelformats.size() 
                                      ? m_spec.channelformats[c].size() 
                                      : m_spec.format.size();
                frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                    Imf::Slice (m_pixeltype[c],
                                                buf + chanoffset,
                                                pixel_bytes, scanlinebytes));
                chanoffset += chanbytes;
            }
            m_output_scanline->setFrameBuffer (frameBuffer);
            m_output_scanline->writePixels (nscanlines);
        }
        catch (const std::exception &e) {
            error ("Failed OpenEXR write: %s", e.what());
            return false;
        }

        data = (const char *)data + ystride*nscanlines;
    }

    // If we allocated more than 1M, free the memory.  It's not wasteful,
    // because it means we're writing big chunks at a time, and therefore
    // there will be few allocations and deletions.
    if (m_scratch.size() > 1*1024*1024) {
        std::vector<unsigned char> dummy;
        std::swap (m_scratch, dummy);
    }
    return true;
}



bool
OpenEXROutput::write_tile (int x, int y, int z,
                           TypeDesc format, const void *data,
                           stride_t xstride, stride_t ystride, stride_t zstride)
{
    return write_tiles (x, std::min (x+m_spec.tile_width, m_spec.x+m_spec.width),
                        y, std::min (y+m_spec.tile_height, m_spec.y+m_spec.height),
                        z, std::min (z+m_spec.tile_depth, m_spec.z+m_spec.depth),
                        format, data, xstride, ystride, zstride);
}



bool
OpenEXROutput::write_tiles (int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, TypeDesc format,
                            const void *data, stride_t xstride,
                            stride_t ystride, stride_t zstride)
{
//    std::cerr << "exr::write_tiles " << xbegin << ' ' << xend 
//              << ' ' << ybegin << ' ' << yend << "\n";
    if (! m_output_tiled ||
        ! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    bool native = (format == TypeDesc::UNKNOWN);
    size_t user_pixelbytes = m_spec.pixel_bytes (native);
    size_t pixelbytes = m_spec.pixel_bytes (true);
    if (native && xstride == AutoStride)
        xstride = (stride_t) user_pixelbytes;
    m_spec.auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        (xend-xbegin), (yend-ybegin));
    data = to_native_rectangle (xbegin, xend, ybegin, yend, zbegin, zend,
                                format, data, xstride, ystride, zstride,
                                m_scratch);

    // clamp to the image edge
    xend = std::min (xend, m_spec.x+m_spec.width);
    yend = std::min (yend, m_spec.y+m_spec.height);
    zend = std::min (zend, m_spec.z+m_spec.depth);
    int firstxtile = (xbegin-m_spec.x) / m_spec.tile_width;
    int firstytile = (ybegin-m_spec.y) / m_spec.tile_height;
    int nxtiles = (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width;
    int nytiles = (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height;

    std::vector<char> padded;
    int width = nxtiles*m_spec.tile_width;
    int height = nytiles*m_spec.tile_height;
    stride_t widthbytes = width * pixelbytes;
    if (width != (xend-xbegin) || height != (yend-ybegin)) {
        // If the image region is not an even multiple of the tile size,
        // we need to copy and add padding.
        padded.resize (pixelbytes * width * height, 0);
        OIIO_NAMESPACE::copy_image (m_spec.nchannels, xend-xbegin,
                                    yend-ybegin, 1, data, pixelbytes,
                                    pixelbytes, (xend-xbegin)*pixelbytes,
                                    (xend-xbegin)*(yend-ybegin)*pixelbytes,
                                    &padded[0], pixelbytes, widthbytes,
                                    height*widthbytes);
        data = &padded[0];
    }

    char *buf = (char *)data
              - xbegin * pixelbytes
              - ybegin * widthbytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            size_t chanbytes = m_spec.channelformats.size() 
                                  ? m_spec.channelformats[c].size() 
                                  : m_spec.format.size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype[c],
                                            buf + chanoffset, pixelbytes,
                                            widthbytes));
            chanoffset += chanbytes;
        }
        m_output_tiled->setFrameBuffer (frameBuffer);
        m_output_tiled->writeTiles (firstxtile, firstxtile+nxtiles-1,
                                    firstytile, firstytile+nytiles-1,
                                    m_miplevel, m_miplevel);
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    return true;
}


OIIO_PLUGIN_NAMESPACE_END

