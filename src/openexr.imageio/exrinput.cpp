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

#include <OpenEXR/ImfTestFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfTiledInputFile.h>
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

#include "dassert.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


class OpenEXRInput : public ImageInput {
public:
    OpenEXRInput ();
    virtual ~OpenEXRInput () { close(); }
    virtual const char * format_name (void) const { return "openexr"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual int current_miplevel (void) const { return m_miplevel; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_scanlines (int ybegin, int yend, int z, void *data);
    virtual bool read_native_scanlines (int ybegin, int yend, int z,
                                        int firstchan, int nchans, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend, void *data);
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend,
                                    int firstchan, int nchans, void *data);

private:
    const Imf::Header *m_header;          ///< Ptr to image header
    Imf::InputFile *m_input_scanline;     ///< Input for scanline files
    Imf::TiledInputFile *m_input_tiled;   ///< Input for tiled files
    std::vector<Imf::PixelType> m_pixeltype; ///< Imf pixel type for each chan
    int m_levelmode;                      ///< The level mode of the file
    int m_roundingmode;                   ///< Rounding mode of the file
    int m_subimage;                       ///< What subimage are we looking at?
    int m_nsubimages;                     ///< How many subimages are there?
    int m_miplevel;                       ///< What MIP level are we looking at?
    int m_nmiplevels;                     ///< How many MIP levels are there?
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
        m_miplevel = -1;
    }

    // Helper for open(): set up m_spec.nchannels, m_spec.channelnames,
    // m_spec.alpha_channel, m_spec.z_channel, m_channelnames,
    // m_userchannels.
    void query_channels (void);
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *
openexr_input_imageio_create ()
{
    return new OpenEXRInput;
}

// DLLEXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION; // it's in exroutput.cpp

DLLEXPORT const char * openexr_input_extensions[] = {
    "exr", NULL
};

OIIO_PLUGIN_EXPORTS_END



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
        m_map["openexr:lineOrder"] = "";
        // Ones to skip because we consider them irrelevant

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


namespace pvt {

void set_exr_threads ()
{
    static int exr_threads = 0;  // lives in exrinput.cpp
    static spin_mutex exr_threads_mutex;  

    int oiio_threads = 1;
    OIIO::getattribute ("threads", oiio_threads);

    spin_lock lock (exr_threads_mutex);
    if (exr_threads != oiio_threads) {
        exr_threads = oiio_threads;
        Imf::setGlobalThreadCount (exr_threads == 1 ? 0 : exr_threads);
    }
}

} // namespace pvt



OpenEXRInput::OpenEXRInput ()
{
    init ();
}



bool
OpenEXRInput::valid_file (const std::string &filename) const
{
    return Imf::isOpenExrFile (filename.c_str());
}



bool
OpenEXRInput::open (const std::string &name, ImageSpec &newspec)
{
    // Quick check to reject non-exr files
    bool tiled;
    if (! Imf::isOpenExrFile (name.c_str(), tiled))
        return false;

    pvt::set_exr_threads ();

    m_spec = ImageSpec(); // Clear everything with default constructor
    
    // Unless otherwise specified, exr files are assumed to be linear.
    m_spec.attribute ("oiio:ColorSpace", "Linear");
    
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

    m_nsubimages = 1;
    if (tiled) {
        // FIXME: levelmode
        m_levelmode = m_input_tiled->levelMode();
        m_roundingmode = m_input_tiled->levelRoundingMode();
        if (m_levelmode == Imf::MIPMAP_LEVELS) {
            m_nmiplevels = m_input_tiled->numLevels();
            m_spec.attribute ("openexr:roundingmode", m_roundingmode);
        } else if (m_levelmode == Imf::RIPMAP_LEVELS) {
            m_nmiplevels = std::max (m_input_tiled->numXLevels(),
                                     m_input_tiled->numYLevels());
            m_spec.attribute ("openexr:roundingmode", m_roundingmode);
        } else {
            m_nmiplevels = 1;
        }
    } else {
        m_levelmode = Imf::ONE_LEVEL;
        m_nmiplevels = 1;
    }

    const Imf::EnvmapAttribute *envmap;
    envmap = m_header->findTypedAttribute<Imf::EnvmapAttribute>("envmap");
    if (envmap) {
        m_cubeface = (envmap->value() == Imf::ENVMAP_CUBE);
        m_spec.attribute ("textureformat", m_cubeface ? "CubeFace Environment" : "LatLong Environment");
        // OpenEXR conventions for env maps
        if (! m_cubeface)
            m_spec.attribute ("oiio:updirection", "y");
        m_spec.attribute ("oiio:sampleborder", 1);
        // FIXME - detect CubeFace Shadow?
    } else {
        m_cubeface = false;
        if (tiled && m_levelmode == Imf::MIPMAP_LEVELS)
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
#ifdef IMF_B44_COMPRESSION
            // The enum Imf::B44_COMPRESSION is not defined in older versions
            // of OpenEXR, and there are no explicit version numbers in the
            // headers.  BUT this other related #define is present only in
            // the newer version.
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
        const Imf::V3fAttribute *vattr;        
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
            m_spec.attribute (oname, TypeDesc::TypeMatrix, &(mattr->value()));
        else if (type == "v3f" &&
                 (vattr = m_header->findTypedAttribute<Imf::V3fAttribute> (name)))
            m_spec.attribute (oname, TypeDesc::TypeVector, &(vattr->value()));
        else {
#if 0
            std::cerr << "  unknown attribute " << type << ' ' << name << "\n";
#endif
        }
    }

    m_subimage = 0;
    m_miplevel = 0;
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
        const char* name = ci.name();
        m_channelnames.push_back (name);
        if (red < 0 && (Strutil::iequals(name, "R") || Strutil::iequals(name, "Red") ||
                        Strutil::iends_with(name,".R") || Strutil::iends_with(name,".Red")))
            red = c;
        if (green < 0 && (Strutil::iequals(name, "G") || Strutil::iequals(name, "Green") ||
                          Strutil::iends_with(name,".G") || Strutil::iends_with(name,".Green")))
            green = c;
        if (blue < 0 && (Strutil::iequals(name, "B") || Strutil::iequals(name, "Blue") ||
                         Strutil::iends_with(name,".B") || Strutil::iends_with(name,".Blue")))
            blue = c;
        if (alpha < 0 && (Strutil::iequals(name, "A") || Strutil::iequals(name, "Alpha") ||
                          Strutil::iends_with(name,".A") || Strutil::iends_with(name,".Alpha")))
            alpha = c;
        if (zee < 0 && (Strutil::iequals(name, "Z") || Strutil::iequals(name, "Depth") ||
                        Strutil::iends_with(name,".Z") || Strutil::iends_with(name,".Depth")))
            zee = c;
        ++m_spec.nchannels;
    }
    m_userchannels.resize (m_spec.nchannels);
    int nc = 0;
    if (red >= 0) {
        m_spec.channelnames.push_back (m_channelnames[red]);
        m_userchannels[red] = nc++;
    }
    if (green >= 0) {
        m_spec.channelnames.push_back (m_channelnames[green]);
        m_userchannels[green] = nc++;
    }
    if (blue >= 0) {
        m_spec.channelnames.push_back (m_channelnames[blue]);
        m_userchannels[blue] = nc++;
    }
    if (alpha >= 0) {
        m_spec.channelnames.push_back (m_channelnames[alpha]);
        m_spec.alpha_channel = nc;
        m_userchannels[alpha] = nc++;
    }
    if (zee >= 0) {
        m_spec.channelnames.push_back (m_channelnames[zee]);
        m_spec.z_channel = nc;
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
    std::vector<TypeDesc> chanformat;
    bool differing_chanformats = false;
    for (c = 0, ci = channels.begin();  ci != channels.end();  ++c, ++ci) {
        Imf::PixelType ptype = ci.channel().type;
        TypeDesc fmt = TypeDesc::HALF;
        switch (ptype) {
        case Imf::UINT :
            fmt = TypeDesc::UINT;
            if (m_spec.format == TypeDesc::UNKNOWN)
                m_spec.format = TypeDesc::UINT;
            break;
        case Imf::HALF :
            fmt = TypeDesc::HALF;
            if (m_spec.format != TypeDesc::FLOAT)
                m_spec.format = TypeDesc::HALF;
            break;
        case Imf::FLOAT :
            fmt = TypeDesc::FLOAT;
            m_spec.format = TypeDesc::FLOAT;
            break;
        default: ASSERT (0);
        }
        chanformat.push_back (fmt);
        m_pixeltype.push_back (ptype);
        if (fmt != chanformat[0])
            differing_chanformats = true;
    }
    ASSERT (m_spec.format != TypeDesc::UNKNOWN);
    if (differing_chanformats)
        m_spec.channelformats = chanformat;
}



bool
OpenEXRInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (subimage < 0 || subimage >= m_nsubimages)   // out of range
        return false;

    if (miplevel < 0 || miplevel >= m_nmiplevels)   // out of range
        return false;

    m_subimage = subimage;
    m_miplevel = miplevel;

    if (miplevel == 0 && m_levelmode == Imf::ONE_LEVEL) {
        newspec = m_spec;
        return true;
    }

    // Compute the resolution of the requested mip level.
    int w = m_topwidth, h = m_topheight;
    if (m_levelmode == Imf::MIPMAP_LEVELS) {
        while (miplevel--) {
            if (m_roundingmode == Imf::ROUND_DOWN) {
                w = w / 2;
                h = h / 2;
            } else {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
            }
            w = std::max (1, w);
            h = std::max (1, h);
        }
    } else if (m_levelmode == Imf::RIPMAP_LEVELS) {
        // FIXME
    } else {
        ASSERT(0);
    }

    m_spec.width = w;
    m_spec.height = h;
    // N.B. OpenEXR doesn't support data and display windows per MIPmap
    // level.  So always take from the top level.
    Imath::Box2i datawindow = m_header->dataWindow();
    Imath::Box2i displaywindow = m_header->displayWindow();
    m_spec.x = datawindow.min.x;
    m_spec.y = datawindow.min.y;
    if (miplevel == 0) {
        m_spec.full_x = displaywindow.min.x;
        m_spec.full_y = displaywindow.min.y;
        m_spec.full_width = displaywindow.max.x - displaywindow.min.x + 1;
        m_spec.full_height = displaywindow.max.y - displaywindow.min.y + 1;
    } else {
        m_spec.full_x = m_spec.x;
        m_spec.full_y = m_spec.y;
        m_spec.full_width = m_spec.width;
        m_spec.full_height = m_spec.height;
    }
    if (m_cubeface) {
        m_spec.full_width = w;
        m_spec.full_height = w;
    }
    newspec = m_spec;

    return true;
}



bool
OpenEXRInput::close ()
{
    delete m_input_scanline;
    delete m_input_tiled;
    init ();  // Reset to initial state
    return true;
}



bool
OpenEXRInput::read_native_scanline (int y, int z, void *data)
{
    return read_native_scanlines (y, y+1, z, 0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_scanlines (int ybegin, int yend, int z, void *data)
{
    return read_native_scanlines (ybegin, yend, z, 0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_scanlines (int ybegin, int yend, int z,
                                     int firstchan, int nchans, void *data)
{
//    std::cerr << "openexr rns " << ybegin << ' ' << yend << ", channels "
//              << firstchan << "-" << (firstchan+nchans-1) << "\n";
    if (m_input_scanline == NULL)
        return false;

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    size_t pixelbytes = m_spec.pixel_bytes (firstchan, nchans, true);
    size_t scanlinebytes = (size_t)m_spec.width * pixelbytes;
    char *buf = (char *)data
              - m_spec.x * pixelbytes
              - ybegin * scanlinebytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0;  c < nchans;  ++c) {
            size_t chanbytes = m_spec.channelformats.size() 
                                  ? m_spec.channelformats[c+firstchan].size() 
                                  : m_spec.format.size();
            frameBuffer.insert (m_spec.channelnames[c+firstchan].c_str(),
                                Imf::Slice (m_pixeltype[c+firstchan],
                                            buf + chanoffset,
                                            pixelbytes, scanlinebytes));
            chanoffset += chanbytes;
        }
        m_input_scanline->setFrameBuffer (frameBuffer);
        m_input_scanline->readPixels (ybegin, yend-1);
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
    return read_native_tiles (x, x+m_spec.tile_width, y, y+m_spec.tile_height,
                              z, z+m_spec.tile_depth,
                              0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                 int zbegin, int zend, void *data)
{
    return read_native_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                              0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                 int zbegin, int zend, 
                                 int firstchan, int nchans, void *data)
{
#if 0
    std::cerr << "openexr rnt " << xbegin << ' ' << xend << ' ' << ybegin 
              << ' ' << yend << ", chans " << firstchan 
              << "-" << (firstchan+nchans-1) << "\n";
#endif
    if (! m_input_tiled ||
        ! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    size_t pixelbytes = m_spec.pixel_bytes (firstchan, nchans, true);
    int firstxtile = (xbegin-m_spec.x) / m_spec.tile_width;
    int firstytile = (ybegin-m_spec.y) / m_spec.tile_height;
    // clamp to the image edge
    xend = std::min (xend, m_spec.x+m_spec.width);
    yend = std::min (yend, m_spec.y+m_spec.height);
    zend = std::min (zend, m_spec.z+m_spec.depth);
    // figure out how many tiles we need
    int nxtiles = (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width;
    int nytiles = (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height;
    int whole_width = nxtiles * m_spec.tile_width;
    int whole_height = nytiles * m_spec.tile_height;
    std::vector<char> tmpbuf;
    void *origdata = data;
    if (whole_width != (xend-xbegin) || whole_height != (yend-ybegin)) {
        // Deal with the case of reading not a whole number of tiles --
        // OpenEXR will happily overwrite user memory in this case.
        tmpbuf.resize (nxtiles * nytiles * m_spec.tile_bytes(true));
        data = &tmpbuf[0];
    }
    char *buf = (char *)data
              - xbegin * pixelbytes
              - ybegin * pixelbytes * m_spec.tile_width * nxtiles;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0;  c < nchans;  ++c) {
            size_t chanbytes = m_spec.channelformats.size() 
                                  ? m_spec.channelformats[c+firstchan].size()
                                  : m_spec.format.size();
            frameBuffer.insert (m_spec.channelnames[c+firstchan].c_str(),
                                Imf::Slice (m_pixeltype[c],
                                            buf + chanoffset, pixelbytes,
                                            pixelbytes*m_spec.tile_width*nxtiles));
            chanoffset += chanbytes;
        }
        m_input_tiled->setFrameBuffer (frameBuffer);
        m_input_tiled->readTiles (firstxtile, firstxtile+nxtiles-1,
                                  firstytile, firstytile+nytiles-1,
                                  m_miplevel, m_miplevel);
        if (data != origdata) {
            stride_t user_scanline_bytes = (xend-xbegin) * pixelbytes;
            stride_t scanline_stride = nxtiles*m_spec.tile_width*pixelbytes;
            for (int y = ybegin;  y < yend;  ++y)
                memcpy ((char *)origdata+(y-ybegin)*scanline_stride,
                        (char *)data+(y-ybegin)*scanline_stride,
                        user_scanline_bytes);
        }
    }
    catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

