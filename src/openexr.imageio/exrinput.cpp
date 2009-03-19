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
#include <map>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include <ImfTestFile.h>
#include <ImfInputFile.h>
#include <ImfTiledInputFile.h>
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
#include "thread.h"
#include "strutil.h"

using namespace OpenImageIO;



class OpenEXRInput : public ImageInput {
public:
    OpenEXRInput () { init(); }
    virtual ~OpenEXRInput () { close(); }
    virtual const char * format_name (void) const { return "openexr"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    const Imf::Header *m_header;          ///< Ptr to image header
    Imf::InputFile *m_input_scanline;     ///< Input for scanline files
    Imf::TiledInputFile *m_input_tiled;   ///< Input for tiled files
    Imf::PixelType m_pixeltype;           ///< Imf pixel type
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage are we looking at?
    int m_nsubimages;                     ///< How many subimages are there?
    int m_topwidth;                       ///< Width of top mip level
    int m_topheight;                      ///< Height of top mip level
    bool m_cubeface;                      ///< It's a cubeface environment map
    std::vector<std::string> m_channelnames;  ///< Order of channels in file
    std::vector<int> m_userchannels;      ///< Map file chans to user chans
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    void init () {
        m_header = NULL;
        m_input_scanline = NULL;
        m_input_tiled = NULL;
        m_subimage = -1;
    }

    // Helper for open(): set up m_spec.nchannels, m_spec.channelnames,
    // m_spec.alpha_channel, m_spec.z_channel, m_channelnames,
    // m_userchannels.
    void query_channels (void);
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageInput *
openexr_input_imageio_create ()
{
    return new OpenEXRInput;
}

// DLLEXPORT int openexr_imageio_version = OPENIMAGEIO_PLUGIN_VERSION; // it's in exroutput.cpp

DLLEXPORT const char * openexr_input_extensions[] = {
    "exr", NULL
};

};



class StringMap {
    typedef std::map<std::string, std::string> map_t;
public:
    StringMap (void) { init(); }

    const std::string & operator[] (const std::string &s) const {
        map_t::const_iterator i;
        i = m_map.find (s);
        return i == m_map.end() ? s : i->second;
    }
private:
    map_t m_map;

    void init (void) {
        // Ones whose name we change to our convention
        m_map["cameraTransform"] = "worldtocamera";
        m_map["worldToCamera"] = "worldtocamera";
        m_map["worldToNDC"] = "worldtoscreen";
        m_map["capDate"] = "DateTime";
        m_map["comments"] = "ImageDescription";
        m_map["owner"] = "Copyright";
        m_map["pixelAspectRatio"] = "PixelAspectRatio";
        m_map["expTime"] = "ExposureTime";
        // Ones we don't rename -- OpenEXR convention matches ours
        m_map["wrapmodes"] = "wrapmodes";
        m_map["aperture"] = "FNumber";
        // Ones to skip because we handle specially
        m_map["channels"] = "";
        m_map["compression"] = "";
        m_map["dataWindow"] = "";
        m_map["envmap"] = "";
        m_map["tiledesc"] = "";
        // Ones to skip because we consider them irrelevant
        m_map["lineOrder"] = "";

//        m_map[""] = "";
        // FIXME: Things to consider in the future:
        // preview
        // screenWindowCenter
        // chromaticities whiteLuminance adoptedNeutral
        // renderingTransform, lookModTransform
        // xDensity
        // utcOffset
        // longitude latitude altitude
        // focus isoSpeed
        // keyCode timeCode framesPerSecond
    }
};

static StringMap exr_tag_to_ooio_std;



bool
OpenEXRInput::open (const std::string &name, ImageSpec &newspec)
{
    // Quick check to reject non-exr files
    bool tiled;
    if (! Imf::isOpenExrFile (name.c_str(), tiled))
        return false;

    m_spec = ImageSpec(); // Clear everything with default constructor
    try {
        if (tiled) {
            m_input_tiled = new Imf::TiledInputFile (name.c_str());
            m_header = &(m_input_tiled->header());
        } else {
            m_input_scanline = new Imf::InputFile (name.c_str());
            m_header = &(m_input_scanline->header());
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

    Imath::Box2i datawindow = m_header->dataWindow();
    m_spec.x = datawindow.min.x;
    m_spec.y = datawindow.min.y;
    m_spec.z = 0;
    m_spec.width = datawindow.max.x - datawindow.min.x + 1;
    m_spec.height = datawindow.max.y - datawindow.min.y + 1;
    m_spec.depth = 1;
    m_topwidth = m_spec.width;      // Save top-level mipmap dimensions
    m_topheight = m_spec.height;
    Imath::Box2i displaywindow = m_header->displayWindow();
    m_spec.full_x = displaywindow.min.x;
    m_spec.full_y = displaywindow.min.y;
    m_spec.full_z = 0;
    m_spec.full_width = displaywindow.max.x - displaywindow.min.x + 1;
    m_spec.full_height = displaywindow.max.y - displaywindow.min.y + 1;
    m_spec.full_depth = 1;
    if (tiled) {
        m_spec.tile_width = m_input_tiled->tileXSize();
        m_spec.tile_height = m_input_tiled->tileYSize();
    } else {
        m_spec.tile_width = 0;
        m_spec.tile_height = 0;
    }
    m_spec.tile_depth = 1;
    query_channels ();   // also sets format

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

    const Imf::EnvmapAttribute *envmap;
    envmap = m_header->findTypedAttribute<Imf::EnvmapAttribute>("envmap");
    if (envmap) {
        m_cubeface = (envmap->value() == Imf::ENVMAP_CUBE);
        m_spec.attribute ("textureformat", m_cubeface ? "CubeFace Environment" : "LatLong Environment");
        // FIXME - detect CubeFace Shadow
        m_spec.attribute ("updirection", "y");  // OpenEXR convention
    } else {
        m_cubeface = false;
        if (tiled)
            m_spec.attribute ("textureformat", "Plain Texture");
        // FIXME - detect Shadow
    }

    const Imf::CompressionAttribute *compressattr;
    compressattr = m_header->findTypedAttribute<Imf::CompressionAttribute>("compression");
    if (compressattr) {
        const char *comp = NULL;
        switch (compressattr->value()) {
        case Imf::NO_COMPRESSION    : comp = "none"; break;
        case Imf::RLE_COMPRESSION   : comp = "rle"; break;
        case Imf::ZIPS_COMPRESSION  : comp = "zip"; break;
        case Imf::ZIP_COMPRESSION   : comp = "zip"; break;
        case Imf::PIZ_COMPRESSION   : comp = "piz"; break;
        case Imf::PXR24_COMPRESSION : comp = "pxr24"; break;
#if OPENEXR_VERSION >= 010601
        case Imf::B44_COMPRESSION   : comp = "b44"; break;
        case Imf::B44A_COMPRESSION  : comp = "b44a"; break;
#endif
        default:
            break;
        }
        if (comp)
            m_spec.attribute ("compression", comp);
    }

    for (Imf::Header::ConstIterator hit = m_header->begin();
             hit != m_header->end();  ++hit) {
        const Imf::IntAttribute *iattr;
        const Imf::FloatAttribute *fattr;
        const Imf::StringAttribute *sattr;
        const Imf::M44fAttribute *mattr;
        const char *name = hit.name();
        std::string oname = exr_tag_to_ooio_std[name];
        if (oname.empty())   // Empty string means skip this attrib
            continue;
//        if (oname == name)
//            oname = std::string(format_name()) + "_" + oname;
        const Imf::Attribute &attrib = hit.attribute();
        std::string type = attrib.typeName();
        if (type == "string" && 
            (sattr = m_header->findTypedAttribute<Imf::StringAttribute> (name)))
            m_spec.attribute (oname, sattr->value().c_str());
        else if (type == "int" && 
            (iattr = m_header->findTypedAttribute<Imf::IntAttribute> (name)))
            m_spec.attribute (oname, iattr->value());
        else if (type == "float" && 
            (fattr = m_header->findTypedAttribute<Imf::FloatAttribute> (name)))
            m_spec.attribute (oname, fattr->value());
        else if (type == "m44f" && 
            (mattr = m_header->findTypedAttribute<Imf::M44fAttribute> (name)))
            m_spec.attribute (oname, PT_MATRIX, &(mattr->value()));
        else {
#ifdef DEBUG
            std::cerr << "  unknown attribute " << type << ' ' << name << "\n";
#endif
        }
    }

    m_subimage = 0;
    newspec = m_spec;
    return true;
}



void
OpenEXRInput::query_channels (void)
{
    m_spec.nchannels = 0;
    const Imf::ChannelList &channels (m_header->channels());
    Imf::ChannelList::ConstIterator ci;
    int c;
    int red = -1, green = -1, blue = -1, alpha = -1, zee = -1;
    for (c = 0, ci = channels.begin();  ci != channels.end();  ++c, ++ci) {
        // std::cerr << "Channel " << ci.name() << '\n';
        std::string name = ci.name();
        m_channelnames.push_back (name);
        if (iequals(name, "R") || iequals(name, "Red"))
            red = c;
        if (iequals(name, "G") || iequals(name, "Green"))
            green = c;
        if (iequals(name, "B") || iequals(name, "Blue"))
            blue = c;
        if (iequals(name, "A") || iequals(name, "Alpha"))
            alpha = c;
        else if (iequals(name, "Z"))
            zee = c;
        ++m_spec.nchannels;
    }
    m_userchannels.resize (m_spec.nchannels);
    int nc = 0;
    if (red >= 0) {
        m_spec.channelnames.push_back ("R");
        m_userchannels[red] = nc++;
    }
    if (green >= 0) {
        m_spec.channelnames.push_back ("G");
        m_userchannels[green] = nc++;
    }
    if (blue >= 0) {
        m_spec.channelnames.push_back ("B");
        m_userchannels[blue] = nc++;
    }
    if (alpha >= 0) {
        m_spec.channelnames.push_back ("A");
        m_userchannels[alpha] = nc++;
    }
    if (zee >= 0) {
        m_spec.channelnames.push_back ("Z");
        m_userchannels[zee] = nc++;
    }
    for (c = 0, ci = channels.begin();  ci != channels.end();  ++c, ++ci) {
        if (red == c || green == c || blue == c || alpha == c || zee == c)
            continue;   // Already accounted for this channel
        m_userchannels[c] = nc;
        m_spec.channelnames.push_back (ci.name());
        ++nc;
    }
    ASSERT ((int)m_spec.channelnames.size() == m_spec.nchannels);
    // FIXME: should we also figure out the layers?

    // Figure out data types -- choose the highest range
    m_spec.format = TypeDesc::UNKNOWN;
    for (ci = channels.begin();  ci != channels.end();  ++ci) {
        Imf::PixelType ptype = ci.channel().type;
        switch (ptype) {
        case Imf::UINT :
            if (m_spec.format == TypeDesc::UNKNOWN) {
                m_spec.format = TypeDesc::UINT;
                m_pixeltype = Imf::UINT;
            }
            break;
        case Imf::HALF :
            if (m_spec.format != TypeDesc::FLOAT) {
                m_spec.format = TypeDesc::HALF;
                m_pixeltype = Imf::HALF;
            }
            break;
        case Imf::FLOAT :
            m_pixeltype = Imf::FLOAT;
            m_spec.format = TypeDesc::FLOAT;
            break;
        default: ASSERT (0);
        }
    }
    if (m_spec.format == TypeDesc::UNKNOWN) {
        m_spec.format = TypeDesc::HALF;
        m_pixeltype = Imf::HALF;
    }
}



bool
OpenEXRInput::seek_subimage (int index, ImageSpec &newspec)
{
    if (index < 0 || index >= m_nsubimages)   // out of range
        return false;

    m_subimage = index;

    if (index == 0 && m_levelmode == Imf::ONE_LEVEL) {
        newspec = m_spec;
        return true;
    }

    // Compute the resolution of the requested subimage.
    int w = m_topwidth, h = m_topheight;
    if (m_levelmode == Imf::MIPMAP_LEVELS) {
        while (index--) {
            if (w > 1) {
                if ((w & 1) && m_roundingmode == Imf::ROUND_UP)
                    w = w/2 + 1;
                else w /= 2;
            }
            if (h > 1) {
                if ((h & 1) && m_roundingmode == Imf::ROUND_UP)
                    h = h/2 + 1;
                else h /= 2;
            }
        }
    } else if (m_levelmode == Imf::RIPMAP_LEVELS) {
        // FIXME
    } else {
        ASSERT(0);
    }

    m_spec.width = w;
    m_spec.height = h;
    m_spec.full_width = w;
    m_spec.full_height = m_cubeface ? w : h;
    newspec = m_spec;

    return true;
}



bool
OpenEXRInput::close ()
{
    delete m_input_scanline;
    delete m_input_tiled;
    m_subimage = -1;
    init ();  // Reset to initial state
    return true;
}



bool
OpenEXRInput::read_native_scanline (int y, int z, void *data)
{
    ASSERT (m_input_scanline != NULL);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - m_spec.x * m_spec.pixel_bytes() 
              - y * m_spec.scanline_bytes();

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (m_pixeltype,
                                            buf + c * m_spec.channel_bytes(),
                                            m_spec.pixel_bytes(),
                                            m_spec.scanline_bytes()));
        }
        m_input_scanline->setFrameBuffer (frameBuffer);
        m_input_scanline->readPixels (y, y);
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    }
    return true;
}



bool
OpenEXRInput::read_native_tile (int x, int y, int z, void *data)
{
    ASSERT (m_input_tiled != NULL);

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
        }
        m_input_tiled->setFrameBuffer (frameBuffer);
        m_input_tiled->readTile ((x - m_spec.x) / m_spec.tile_width,
                                 (y - m_spec.y) / m_spec.tile_height,
                                 m_subimage, m_subimage);
    }
    catch (const std::exception &e) {
        error ("Filed OpenEXR read: %s", e.what());
        return false;
    }

    return true;
}
