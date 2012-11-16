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
#include <errno.h>
#include <fstream>
#include <iostream>
#include <map>

#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfTiledOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfEnvmap.h>

// The way that OpenEXR uses dynamic casting for attributes requires 
// temporarily suspending "hidden" symbol visibility mode.
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#include <OpenEXR/ImfIntAttribute.h>
#include <OpenEXR/ImfFloatAttribute.h>
#include <OpenEXR/ImfMatrixAttribute.h>
#include <OpenEXR/ImfVecAttribute.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfEnvmapAttribute.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfCRgbaFile.h>  // JUST to get symbols to figure out version!
#include <OpenEXR/IexBaseExc.h>
#include <OpenEXR/IexThrowErrnoExc.h>

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#ifdef IMF_B44_COMPRESSION
#define OPENEXR_VERSION_IS_1_6_OR_LATER
#endif
#ifdef USE_OPENEXR_VERSION2
#include <OpenEXR/ImfMultiPartOutputFile.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfOutputPart.h>
#include <OpenEXR/ImfTiledOutputPart.h>
#include <OpenEXR/ImfDeepScanlineOutputPart.h>
#include <OpenEXR/ImfDeepTiledOutputPart.h>
#endif

#include "dassert.h"
#include "imageio.h"
#include "filesystem.h"
#include "thread.h"
#include "strutil.h"
#include "sysutil.h"
#include "fmath.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


// Custom file output stream, copying code from the class StdOFStream in
// OpenEXR, which would have been used if we just provided a
// filename. The difference is that this can handle UTF-8 file paths on
// all platforms.
class OpenEXROutputStream : public Imf::OStream
{
public:
    OpenEXROutputStream (const char *filename) : Imf::OStream(filename) {
        // The reason we have this class is for this line, so that we
        // can correctly handle UTF-8 file paths on Windows
        Filesystem::open (ofs, filename, std::ios_base::binary);
        if (!ofs)
            Iex::throwErrnoExc ();
    }
    virtual void write (const char c[], int n) {
        errno = 0;
        ofs.write (c, n);
        check_error ();
    }
    virtual Imath::Int64 tellp () {
        return std::streamoff (ofs.tellp ());
    }
    virtual void seekp (Imath::Int64 pos) {
        ofs.seekp (pos);
        check_error ();
    }

private:
    void check_error () {
        if (!ofs) {
            if (errno)
                Iex::throwErrnoExc ();
            throw Iex::ErrnoExc ("File output failed.");
        }
    }
    std::ofstream ofs;
};



class OpenEXROutput : public ImageOutput {
public:
    OpenEXROutput ();
    virtual ~OpenEXROutput ();
    virtual const char * format_name (void) const { return "openexr"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool open (const std::string &name, int subimages,
                       const ImageSpec *specs);
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
    virtual bool write_deep_scanlines (int ybegin, int yend, int z,
                                       const DeepData &deepdata);
    virtual bool write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                   int zbegin, int zend,
                                   const DeepData &deepdata);

private:
    OpenEXROutputStream *m_output_stream; ///< Stream for output file
    Imf::OutputFile *m_output_scanline;   ///< Input for scanline files
    Imf::TiledOutputFile *m_output_tiled; ///< Input for tiled files
#ifdef USE_OPENEXR_VERSION2
    Imf::MultiPartOutputFile *m_output_multipart;
    Imf::OutputPart *m_scanline_output_part;
    Imf::TiledOutputPart *m_tiled_output_part;
    Imf::DeepScanLineOutputPart *m_deep_scanline_output_part;
    Imf::DeepTiledOutputPart *m_deep_tiled_output_part;
#else
    char *m_output_multipart;
    char *m_scanline_output_part;
    char *m_tiled_output_part;
    char *m_deep_scanline_output_part;
    char *m_deep_tiled_output_part;
#endif
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage we're writing now
    int m_nsubimages;                     ///< How many subimages are there?
    int m_miplevel;                       ///< What miplevel we're writing now
    int m_nmiplevels;                     ///< How many mip levels are there?
    std::vector<Imf::PixelType> m_pixeltype; ///< Imf pixel type for each chan
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use
    std::vector<ImageSpec> m_subimagespecs; ///< Saved subimage specs
    std::vector<Imf::Header> m_headers;

    // Initialize private members to pre-opened state
    void init (void) {
        m_output_stream = NULL;
        m_output_scanline = NULL;
        m_output_tiled = NULL;
        m_output_multipart = NULL;
        m_scanline_output_part = NULL;
        m_tiled_output_part = NULL;
        m_deep_scanline_output_part = NULL;
        m_deep_tiled_output_part = NULL;
        m_subimage = -1;
        m_miplevel = -1;
        std::vector<ImageSpec>().swap (m_subimagespecs);  // clear and free
        std::vector<Imf::Header>().swap (m_headers);
    }

    // Set up the header based on the given spec.  Also may doctor the 
    // spec a bit.
    bool spec_to_header (ImageSpec &spec, Imf::Header &header);

    // Add a parameter to the output
    static bool put_parameter (const std::string &name, TypeDesc type,
                               const void *data, Imf::Header &header);

    // Decode the IlmImf MIP parameters from the spec.
    static void figure_mip (const ImageSpec &spec, int &nmiplevels,
                            int &levelmode, int &roundingmode);
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput *
openexr_output_imageio_create ()
{
    return new OpenEXROutput;
}

OIIO_EXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char * openexr_output_extensions[] = {
    "exr", NULL
};

OIIO_PLUGIN_EXPORTS_END



static std::string format_string ("openexr");
static std::string format_prefix ("openexr_");


namespace pvt {
void set_exr_threads ();

// format-specific metadata prefixes
static std::vector<std::string> format_prefixes;
static atomic_int format_prefixes_initialized;
static spin_mutex format_prefixes_mutex;   // guard

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
    delete m_scanline_output_part;  m_scanline_output_part = NULL;
    delete m_tiled_output_part;  m_tiled_output_part = NULL;
    delete m_deep_scanline_output_part;  m_deep_scanline_output_part = NULL;
    delete m_deep_tiled_output_part;  m_deep_tiled_output_part = NULL;
    delete m_output_multipart;  m_output_multipart = NULL;
    delete m_output_stream;  m_output_stream = NULL;
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
    if (feature == "origin")
        return true;
    if (feature == "negativeorigin")
        return true;
#ifdef USE_OPENEXR_VERSION2
    if (feature == "multiimage")
        return true;  // N.B. But OpenEXR does not support "appendsubimage"
#endif

    // EXR supports random write order iff lineOrder is set to 'random Y'
    if (feature == "random_access") {
        const ImageIOParameter *param = m_spec.find_attribute("openexr:lineOrder");
        const char *lineorder = param ? *(char **)param->data() : NULL;
        return (lineorder && Strutil::iequals (lineorder, "randomY"));
    }

    // FIXME: we could support "empty"

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open (const std::string &name, const ImageSpec &userspec,
                     OpenMode mode)
{
    if (mode == Create) {
        if (userspec.deep)  // Fall back on multi-part OpenEXR for deep files
            return open (name, 1, &userspec);
        m_nsubimages = 1;
        m_subimage = 0;
        m_nmiplevels = 1;
        m_miplevel = 0;
        m_headers.resize (1);
        m_spec = userspec;  // Stash the spec

        if (! spec_to_header (m_spec, m_headers[m_subimage]))
            return false;

        try {
            m_output_stream = new OpenEXROutputStream (name.c_str());
            if (m_spec.tile_width) {
                m_output_tiled = new Imf::TiledOutputFile (*m_output_stream,
                                                           m_headers[m_subimage]);
            } else {
                m_output_scanline = new Imf::OutputFile (*m_output_stream,
                                                         m_headers[m_subimage]);
            }
        }
        catch (const std::exception &e) {
            error ("OpenEXR exception: %s", e.what());
            m_output_scanline = NULL;
            m_output_tiled = NULL;
            return false;
        }
        if (! m_output_scanline && ! m_output_tiled) {
            error ("Unknown error opening EXR file");
            return false;
        }

        return true;
    }

    if (mode == AppendSubimage) {
#ifdef USE_OPENEXR_VERSION2
        // OpenEXR 2.x supports subimages, but we only allow it to use the
        // open(name,subimages,specs[]) variety.
        if (m_subimagespecs.size() == 0 || ! m_output_multipart) {
            error ("%s not opened properly for subimages", format_name());
            return false;
        }
        // Move on to next subimage
        ++m_subimage;
        if (m_subimage >= m_nsubimages) {
            error ("More subimages than originally declared.");
            return false;
        }
        // Close the current subimage, open the next one
        try {
            if (m_tiled_output_part) {
                delete m_tiled_output_part;
                m_tiled_output_part = new Imf::TiledOutputPart (*m_output_multipart, m_subimage);
            } else if (m_scanline_output_part) {
                delete m_scanline_output_part;
                m_scanline_output_part = new Imf::OutputPart (*m_output_multipart, m_subimage);
            } else if (m_deep_tiled_output_part) {
                delete m_deep_tiled_output_part;
                m_deep_tiled_output_part = new Imf::DeepTiledOutputPart (*m_output_multipart, m_subimage);
            } else if (m_deep_scanline_output_part) {
                delete m_deep_scanline_output_part;
                m_deep_scanline_output_part = new Imf::DeepScanLineOutputPart (*m_output_multipart, m_subimage);
            } else {
                ASSERT (0);
            }
        }
        catch (const std::exception &e) {
            error ("OpenEXR exception: %s", e.what());
            m_scanline_output_part = NULL;
            m_tiled_output_part = NULL;
            m_deep_scanline_output_part = NULL;
            m_deep_tiled_output_part = NULL;
            return false;
        }
        m_spec = m_subimagespecs[m_subimage];
        return true;
#else
        // OpenEXR 1.x does not support subimages (multi-part)
        error ("%s does not support subimages", format_name());
        return false;
#endif
    }

    if (mode == AppendMIPLevel) {
        if (! m_output_scanline && ! m_output_tiled) {
            error ("Cannot append a MIP level if no file has been opened");
            return false;
        }
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
        } else {
            error ("Cannot add MIP level to a non-MIPmapped file");
            return false;
        }
    }

    ASSERTMSG (0, "Unknown open mode %d", int(mode));
    return false;
}



bool
OpenEXROutput::open (const std::string &name, int subimages,
                     const ImageSpec *specs)
{
    if (subimages < 1) {
        error ("OpenEXR does not support %d subimages.", subimages);
        return false;
    }

#ifdef USE_OPENEXR_VERSION2
    // Only one part and not deep?  Write an OpenEXR 1.x file
    if (subimages == 1 && ! specs[0].deep)
        return open (name, specs[0], Create);

    // Copy the passed-in subimages and turn into OpenEXR headers
    m_nsubimages = subimages;
    m_subimage = 0;
    m_nmiplevels = 1;
    m_miplevel = 0;
    m_subimagespecs.assign (specs, specs+subimages);
    m_headers.resize (subimages);
    std::string filetype;
    if (specs[0].deep)
        filetype = specs[0].tile_width ? "tiledimage" : "deepscanlineimage";
    else
        filetype = specs[0].tile_width ? "tiledimage" : "scanlineimage";
    bool deep = false;
    for (int s = 0;  s < subimages;  ++s) {
        if (! spec_to_header (m_subimagespecs[s], m_headers[s]))
            return false;
        deep |= m_subimagespecs[s].deep;
        if (m_subimagespecs[s].deep != m_subimagespecs[0].deep) {
            error ("OpenEXR does not support mixed deep/nondeep multi-part image files");
            return false;
        }
        if (subimages > 1 || deep) {
            bool tiled = m_subimagespecs[s].tile_width;
            m_headers[s].setType (deep ? (tiled ? Imf::DEEPTILE   : Imf::DEEPSCANLINE)
                                       : (tiled ? Imf::TILEDIMAGE : Imf::SCANLINEIMAGE));
        }
    }

    m_spec = m_subimagespecs[0];

    // Create an ImfMultiPartOutputFile
    try {
        // m_output_stream = new OpenEXROutputStream (name.c_str());
        // m_output_multipart = new Imf::MultiPartOutputFile (*m_output_stream,
        //                                          &m_headers[0], subimages);
        // FIXME: Oops, looks like OpenEXR 2.0 currently lacks a
        // MultiPartOutputFile ctr that takes an OStream, so we can't
        // do this quite yet.
        m_output_multipart = new Imf::MultiPartOutputFile (name.c_str(),
                                                 &m_headers[0], subimages);
    }
    catch (const std::exception &e) {
        delete m_output_stream;
        m_output_stream = NULL;
        error ("OpenEXR exception: %s", e.what());
        return false;
    }
    try {
        if (deep) {
            if (m_spec.tile_width) {
                m_deep_tiled_output_part = new Imf::DeepTiledOutputPart (*m_output_multipart, 0);
            } else {
                m_deep_scanline_output_part = new Imf::DeepScanLineOutputPart (*m_output_multipart, 0);
            }
        } else {
            if (m_spec.tile_width) {
                m_tiled_output_part = new Imf::TiledOutputPart (*m_output_multipart, 0);
            } else {
                m_scanline_output_part = new Imf::OutputPart (*m_output_multipart, 0);
            }
        }
    }
    catch (const std::exception &e) {
        error ("OpenEXR exception: %s", e.what());
        delete m_output_stream;  m_output_stream = NULL;
        m_scanline_output_part = NULL;
        m_tiled_output_part = NULL;
        m_deep_scanline_output_part = NULL;
        m_deep_tiled_output_part = NULL;
        return false;
    }

    return true;
#else
    // No support for OpenEXR 2.x -- one subimage only
    if (subimages != 1) {
        error ("OpenEXR 1.x does not support multiple subimages.");
        return false;
    }
    if (specs[0].deep) {
        error ("OpenEXR 1.x does not support deep data.");
        return false;
    }
    return open (name, specs[0], Create);
#endif
}



bool
OpenEXROutput::spec_to_header (ImageSpec &spec, Imf::Header &header)
{
    if (spec.width < 1 || spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               spec.width, spec.height);
        return false;
    }
    if (spec.depth < 1)
        spec.depth = 1;
    if (spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    if (spec.full_width <= 0)
        spec.full_width = spec.width;
    if (spec.full_height <= 0)
        spec.full_height = spec.height;

    // Force use of one of the three data types that OpenEXR supports
    switch (spec.format.basetype) {
    case TypeDesc::UINT:
        spec.format = TypeDesc::UINT;
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        spec.format = TypeDesc::FLOAT;
        break;
    default:
        // Everything else defaults to half
        spec.format = TypeDesc::HALF;
    }

    Imath::Box2i dataWindow (Imath::V2i (spec.x, spec.y),
                             Imath::V2i (spec.width + spec.x - 1,
                                         spec.height + spec.y - 1));
    Imath::Box2i displayWindow (Imath::V2i (spec.full_x, spec.full_y),
                                Imath::V2i (spec.full_width+spec.full_x-1,
                                            spec.full_height+spec.full_y-1));
    header = Imf::Header (displayWindow, dataWindow);

    // Insert channels into the header.  Also give the channels names if
    // the user botched it.
    m_pixeltype.clear ();
    static const char *default_chan_names[] = { "R", "G", "B", "A" };
    spec.channelnames.resize (spec.nchannels);
    for (int c = 0;  c < spec.nchannels;  ++c) {
        if (spec.channelnames[c].empty())
            spec.channelnames[c] = (c<4) ? default_chan_names[c]
                                           : Strutil::format ("unknown %d", c);
        TypeDesc format = spec.channelformat(c);
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
        
        bool pLinear = Strutil::iequals (spec.get_string_attribute ("oiio:ColorSpace", "Linear"), "Linear");
#endif
        m_pixeltype.push_back (ptype);
        if (spec.channelformats.size())
            spec.channelformats[c] = format;
        header.channels().insert (spec.channelnames[c].c_str(),
                                     Imf::Channel(ptype, 1, 1
#ifdef OPENEXR_VERSION_IS_1_6_OR_LATER
                                     , pLinear
#endif
                                     ));
    }
    ASSERT (m_pixeltype.size() == (size_t)spec.nchannels);

    // Default to ZIP compression if no request came with the user spec.
    if (! spec.find_attribute("compression"))
        spec.attribute ("compression", "zip");

    // Default to increasingY line order, same as EXR.
    if (! spec.find_attribute("openexr:lineOrder"))
        spec.attribute ("openexr:lineOrder", "increasingY");

    // Automatically set date field if the client didn't supply it.
    if (! spec.find_attribute("DateTime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        Sysutil::get_local_time (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        spec.attribute ("DateTime", date);
    }

    figure_mip (spec, m_nmiplevels, m_levelmode, m_roundingmode);

    std::string textureformat = spec.get_string_attribute ("textureformat", "");
    if (Strutil::iequals (textureformat, "CubeFace Environment")) {
        header.insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_CUBE));
    } else if (Strutil::iequals (textureformat, "LatLong Environment")) {
        header.insert ("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_LATLONG));
    }

    if (spec.tile_width)
        header.setTileDescription (
            Imf::TileDescription (spec.tile_width, spec.tile_height,
                                  Imf::LevelMode(m_levelmode),
                                  Imf::LevelRoundingMode(m_roundingmode)));

    // Deal with all other params
    for (size_t p = 0;  p < spec.extra_attribs.size();  ++p)
        put_parameter (spec.extra_attribs[p].name().string(),
                       spec.extra_attribs[p].type(),
                       spec.extra_attribs[p].data(), header);

    return true;
}



void
OpenEXROutput::figure_mip (const ImageSpec &spec, int &nmiplevels,
                           int &levelmode, int &roundingmode)
{
    nmiplevels = 1;
    levelmode = Imf::ONE_LEVEL;  // Default to no MIP-mapping
    roundingmode = spec.get_int_attribute ("openexr:roundingmode",
                                           Imf::ROUND_DOWN);

    std::string textureformat = spec.get_string_attribute ("textureformat", "");
    if (Strutil::iequals (textureformat, "Plain Texture")) {
        levelmode = spec.get_int_attribute ("openexr:levelmode",
                                            Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals (textureformat, "CubeFace Environment")) {
        levelmode = spec.get_int_attribute ("openexr:levelmode",
                                            Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals (textureformat, "LatLong Environment")) {
        levelmode = spec.get_int_attribute ("openexr:levelmode",
                                            Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals (textureformat, "Shadow")) {
        levelmode = Imf::ONE_LEVEL;  // Force one level for shadow maps
    }

    if (levelmode == Imf::MIPMAP_LEVELS) {
        // Compute how many mip levels there will be
        int w = spec.width;
        int h = spec.height;
        while (w > 1 && h > 1) {
            if (roundingmode == Imf::ROUND_DOWN) {
                w = w / 2;
                h = h / 2;
            } else {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
            }
            w = std::max (1, w);
            h = std::max (1, h);
            ++nmiplevels;
        }
    }
}



bool
OpenEXROutput::put_parameter (const std::string &name, TypeDesc type,
                              const void *data, Imf::Header &header)
{
    // Translate
    std::string xname = name;
    if (Strutil::iequals(xname, "worldtocamera"))
        xname = "worldToCamera";
    else if (Strutil::iequals(xname, "worldtoscreen"))
        xname = "worldToNDC";
    else if (Strutil::iequals(xname, "DateTime"))
        xname = "capDate";
    else if (Strutil::iequals(xname, "description") || Strutil::iequals(xname, "ImageDescription"))
        xname = "comments";
    else if (Strutil::iequals(xname, "Copyright"))
        xname = "owner";
    else if (Strutil::iequals(xname, "PixelAspectRatio"))
        xname = "pixelAspectRatio";
    else if (Strutil::iequals(xname, "ExposureTime"))
        xname = "expTime";
    else if (Strutil::iequals(xname, "FNumber"))
        xname = "aperture";
    else if (Strutil::istarts_with (xname, format_prefix))
        xname = std::string (xname.begin()+format_prefix.size(), xname.end());

//    std::cerr << "exr put '" << name << "' -> '" << xname << "'\n";

    // Special cases
    if (Strutil::iequals(xname, "Compression") && type == TypeDesc::STRING) {
        const char *str = *(char **)data;
        header.compression() = Imf::ZIP_COMPRESSION;  // Default
        if (str) {
            if (Strutil::iequals (str, "none"))
                header.compression() = Imf::NO_COMPRESSION;
            else if (Strutil::iequals (str, "deflate") || Strutil::iequals (str, "zip")) 
                header.compression() = Imf::ZIP_COMPRESSION;
            else if (Strutil::iequals (str, "rle")) 
                header.compression() = Imf::RLE_COMPRESSION;
            else if (Strutil::iequals (str, "zips")) 
                header.compression() = Imf::ZIPS_COMPRESSION;
            else if (Strutil::iequals (str, "piz")) 
                header.compression() = Imf::PIZ_COMPRESSION;
            else if (Strutil::iequals (str, "pxr24")) 
                header.compression() = Imf::PXR24_COMPRESSION;
#ifdef IMF_B44_COMPRESSION
            // The enum Imf::B44_COMPRESSION is not defined in older versions
            // of OpenEXR, and there are no explicit version numbers in the
            // headers.  BUT this other related #define is present only in
            // the newer version.
            else if (Strutil::iequals (str, "b44"))
                header.compression() = Imf::B44_COMPRESSION;
            else if (Strutil::iequals (str, "b44a"))
                header.compression() = Imf::B44A_COMPRESSION;
#endif
        }
        return true;
    }

    if (Strutil::iequals (xname, "openexr:lineOrder") && type == TypeDesc::STRING) {
        const char *str = *(char **)data;
        header.lineOrder() = Imf::INCREASING_Y;   // Default
        if (str) {
            if (Strutil::iequals (str, "randomY"))
                header.lineOrder() = Imf::RANDOM_Y;
            else if (Strutil::iequals (str, "decreasingY"))
                header.lineOrder() = Imf::DECREASING_Y;
        }
        return true;
    }

    // Supress planarconfig!
    if (Strutil::iequals (xname, "planarconfig") || Strutil::iequals (xname, "tiff:planarconfig"))
        return true;

    // Special handling of any remaining "oiio:*" metadata.
    if (Strutil::istarts_with (xname, "oiio:")) {
        // None currently supported
        return false;
    }

    // Before handling general named metadata, suppress non-openexr
    // format-specific metadata.
    if (const char *colon = strchr (xname.c_str(), ':')) {
        std::string prefix (xname.c_str(), colon);
        if (! Strutil::iequals (prefix, "openexr")) {
            if (! pvt::format_prefixes_initialized) {
                // Retrieve and split the list, only the first time
                spin_lock lock (pvt::format_prefixes_mutex);
                std::string format_list;
                OIIO::getattribute ("format_list", format_list);
                Strutil::split (format_list, pvt::format_prefixes, ",");
                pvt::format_prefixes_initialized = true;
            }
            for (size_t i = 0, e = pvt::format_prefixes.size();  i < e;  ++i)
                if (Strutil::iequals (prefix, pvt::format_prefixes[i]))
                    return false;
        }
    }

    // General handling of attributes
    // FIXME -- police this if we ever allow arrays
    if (type == TypeDesc::INT || type == TypeDesc::UINT) {
        header.insert (xname.c_str(), Imf::IntAttribute (*(int*)data));
        return true;
    }
    if (type == TypeDesc::INT16) {
        header.insert (xname.c_str(), Imf::IntAttribute (*(short*)data));
        return true;
    }
    if (type == TypeDesc::UINT16) {
        header.insert (xname.c_str(), Imf::IntAttribute (*(unsigned short*)data));
        return true;
    }
    if (type == TypeDesc::FLOAT) {
        header.insert (xname.c_str(), Imf::FloatAttribute (*(float*)data));
        return true;
    }
    if (type == TypeDesc::HALF) {
        header.insert (xname.c_str(), Imf::FloatAttribute ((float)*(half*)data));
        return true;
    }
    if (type == TypeDesc::TypeMatrix) {
        header.insert (xname.c_str(), Imf::M44fAttribute (*(Imath::M44f*)data));
        return true;
    }
    if (type == TypeDesc::TypeString) {
        header.insert (xname.c_str(), Imf::StringAttribute (*(char**)data));
        return true;
    }
    if (type == TypeDesc::TypeVector) {
        header.insert (xname.c_str(), Imf::V3fAttribute (*(Imath::V3f*)data));
        return true;
    }
    if (type == TypeDesc(TypeDesc::FLOAT,TypeDesc::VEC2) ||
        type == TypeDesc(TypeDesc::FLOAT,2) /* array float[2] */) {
        header.insert (xname.c_str(), Imf::V2fAttribute (*(Imath::V2f*)data));
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
    delete m_scanline_output_part;  m_scanline_output_part = NULL;
    delete m_tiled_output_part;  m_tiled_output_part = NULL;
    delete m_output_multipart;  m_output_multipart = NULL;
    delete m_output_stream;  m_output_stream = NULL;

    init ();      // re-initialize
    return true;  // How can we fail?
}



bool
OpenEXROutput::write_scanline (int y, int z, TypeDesc format,
                               const void *data, stride_t xstride)
{
    if (! (m_output_scanline || m_scanline_output_part)) {
        error ("called OpenEXROutput::write_scanline without an open file");
        return false;
    }

    bool native = (format == TypeDesc::UNKNOWN);
    size_t pixel_bytes = m_spec.pixel_bytes (true);  // native
    if (native && xstride == AutoStride)
        xstride = (stride_t) pixel_bytes;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    data = to_native_scanline (format, data, xstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where client stored
    // the bytes to be written, but OpenEXR's frameBuffer.insert() wants
    // where the address of the "virtual framebuffer" for the whole
    // image.
    imagesize_t scanlinebytes = m_spec.scanline_bytes (native);
    char *buf = (char *)data
              - m_spec.x * pixel_bytes
              - y * scanlinebytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype[c],
                                            buf + chanoffset,
                                            pixel_bytes, scanlinebytes));
            chanoffset += chanbytes;
        }
        if (m_output_scanline) {
            m_output_scanline->setFrameBuffer (frameBuffer);
            m_output_scanline->writePixels (1);
#ifdef USE_OPENEXR_VERSION2
        } else if (m_scanline_output_part) {
            m_scanline_output_part->setFrameBuffer (frameBuffer);
            m_scanline_output_part->writePixels (1);
#endif
        } else {
            ASSERT (0);
        }
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
    if (! (m_output_scanline || m_scanline_output_part)) {
        error ("called OpenEXROutput::write_scanlines without an open file");
        return false;
    }

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
        // OpenImageIO requires that 'data' points to where client stored
        // the bytes to be written, but OpenEXR's frameBuffer.insert() wants
        // where the address of the "virtual framebuffer" for the whole
        // image.
        char *buf = (char *)d
                  - m_spec.x * pixel_bytes
                  - ybegin * scanlinebytes;
        try {
            Imf::FrameBuffer frameBuffer;
            size_t chanoffset = 0;
            for (int c = 0;  c < m_spec.nchannels;  ++c) {
                size_t chanbytes = m_spec.channelformat(c).size();
                frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                    Imf::Slice (m_pixeltype[c],
                                                buf + chanoffset,
                                                pixel_bytes, scanlinebytes));
                chanoffset += chanbytes;
            }
            if (m_output_scanline) {
                m_output_scanline->setFrameBuffer (frameBuffer);
                m_output_scanline->writePixels (nscanlines);
#ifdef USE_OPENEXR_VERSION2
            } else if (m_scanline_output_part) {
                m_scanline_output_part->setFrameBuffer (frameBuffer);
                m_scanline_output_part->writePixels (nscanlines);
#endif
            } else {
                ASSERT (0);
            }
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
    if (! (m_output_tiled || m_tiled_output_part)) {
        error ("called OpenEXROutput::write_tiles without an open file");
        return false;
    }
    if (! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend)) {
        error ("called OpenEXROutput::write_tiles with an invalid tile range");
        return false;
    }

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
        OIIO::copy_image (m_spec.nchannels, xend-xbegin,
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
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype[c],
                                            buf + chanoffset, pixelbytes,
                                            widthbytes));
            chanoffset += chanbytes;
        }
        if (m_output_tiled) {
            m_output_tiled->setFrameBuffer (frameBuffer);
            m_output_tiled->writeTiles (firstxtile, firstxtile+nxtiles-1,
                                        firstytile, firstytile+nytiles-1,
                                        m_miplevel, m_miplevel);
#ifdef USE_OPENEXR_VERSION2
        } else if (m_tiled_output_part) {
            m_tiled_output_part->setFrameBuffer (frameBuffer);
            m_tiled_output_part->writeTiles (firstxtile, firstxtile+nxtiles-1,
                                             firstytile, firstytile+nytiles-1,
                                             m_miplevel, m_miplevel);
#endif
        } else {
            ASSERT (0);
        }
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    return true;
}



bool
OpenEXROutput::write_deep_scanlines (int ybegin, int yend, int z,
                                     const DeepData &deepdata)
{
    if (m_deep_scanline_output_part == NULL) {
        error ("called OpenEXROutput::write_deep_scanlines without an open file");
        return false;
    }
    if (m_spec.width*(yend-ybegin) != deepdata.npixels ||
        m_spec.nchannels != deepdata.nchannels) {
        error ("called OpenEXROutput::write_deep_scanlines with non-matching DeepData size");
        return false;
    }

#ifdef USE_OPENEXR_VERSION2
    int nchans = m_spec.nchannels;
    try {
        // Set up the count and pointers arrays and the Imf framebuffer
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice (Imf::UINT,
                               (char *)(&deepdata.nsamples[0]
                                        - m_spec.x
                                        - ybegin*m_spec.width),
                               sizeof(unsigned int),
                               sizeof(unsigned int) * m_spec.width);
        frameBuffer.insertSampleCountSlice (countslice);
        for (int c = 0;  c < nchans;  ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            Imf::DeepSlice slice (m_pixeltype[c],
                                  (char *)(&deepdata.pointers[c]
                                           - m_spec.x * nchans
                                           - ybegin*m_spec.width*nchans),
                                  sizeof(void*) * nchans, // xstride of pointer array
                                  sizeof(void*) * nchans*m_spec.width, // ystride of pointer array
                                  chanbytes); // stride of data sample
            frameBuffer.insert (m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_scanline_output_part->setFrameBuffer (frameBuffer);

        // Write the pixels
        m_deep_scanline_output_part->writePixels (yend-ybegin);
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    return true;

#else
    error ("deep data not supported with OpenEXR 1.x");
    return false;
#endif
}



bool
OpenEXROutput::write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                 int zbegin, int zend,
                                 const DeepData &deepdata)
{
    if (m_deep_tiled_output_part == NULL) {
        error ("called OpenEXROutput::write_deep_tiles without an open file");
        return false;
    }
    if ((xend-xbegin)*(yend-ybegin)*(zend-zbegin) != deepdata.npixels ||
        m_spec.nchannels != deepdata.nchannels) {
        error ("called OpenEXROutput::write_deep_tiles with non-matching DeepData size");
        return false;
    }

#ifdef USE_OPENEXR_VERSION2
    int nchans = m_spec.nchannels;
    try {
        size_t width = (xend - xbegin);

        // Set up the count and pointers arrays and the Imf framebuffer
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice (Imf::UINT,
                               (char *)(&deepdata.nsamples[0]
                                        - xbegin
                                        - ybegin*width),
                               sizeof(unsigned int),
                               sizeof(unsigned int) * width);
        frameBuffer.insertSampleCountSlice (countslice);
        for (int c = 0;  c < nchans;  ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            Imf::DeepSlice slice (m_pixeltype[c],
                                  (char *)(&deepdata.pointers[c]
                                           - xbegin*nchans
                                           - ybegin*width*nchans),
                                  sizeof(void*) * nchans, // xstride of pointer array
                                  sizeof(void*) * nchans*width, // ystride of pointer array
                                  chanbytes); // stride of data sample
            frameBuffer.insert (m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_tiled_output_part->setFrameBuffer (frameBuffer);

        int xtiles = round_to_multiple (xend-xbegin, m_spec.tile_width) / m_spec.tile_width;
        int ytiles = round_to_multiple (yend-ybegin, m_spec.tile_height) / m_spec.tile_height;

        // Write the pixels
        m_deep_tiled_output_part->writeTiles (0, xtiles-1, 0, ytiles-1,
                                                 m_miplevel, m_miplevel);
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR write: %s", e.what());
        return false;
    }

    return true;

#else
    error ("deep data not supported with OpenEXR 1.x");
    return false;
#endif
}


OIIO_PLUGIN_NAMESPACE_END

