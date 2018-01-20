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
#include <cerrno>
#include <fstream>
#include <map>
#include <numeric>
#include <memory>

#include <boost/math/common_factor_rt.hpp>

#include <OpenEXR/ImfTestFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfTiledInputFile.h>
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
#include <OpenEXR/ImfBoxAttribute.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfTimeCodeAttribute.h>
#include <OpenEXR/ImfKeyCodeAttribute.h>
#include <OpenEXR/ImfEnvmapAttribute.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfChromaticitiesAttribute.h>
#include <OpenEXR/ImfRationalAttribute.h>
#include <OpenEXR/IexBaseExc.h>
#include <OpenEXR/IexThrowErrnoExc.h>
#include <OpenEXR/ImfStringVectorAttribute.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfTiledInputPart.h>
#include <OpenEXR/ImfDeepScanLineInputPart.h>
#include <OpenEXR/ImfDeepTiledInputPart.h>
#include <OpenEXR/ImfDeepFrameBuffer.h>
#include <OpenEXR/ImfDoubleAttribute.h>

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#include <OpenEXR/ImfCRgbaFile.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/sysutil.h>
#include "imageio_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


// Custom file input stream, copying code from the class StdIFStream in OpenEXR,
// which would have been used if we just provided a filename. The difference is
// that this can handle UTF-8 file paths on all platforms.
class OpenEXRInputStream : public Imf::IStream
{
public:
    OpenEXRInputStream (const char *filename) : Imf::IStream (filename) {
        // The reason we have this class is for this line, so that we
        // can correctly handle UTF-8 file paths on Windows
        Filesystem::open (ifs, filename, std::ios_base::binary);
        if (!ifs) 
            Iex::throwErrnoExc ();
    }
    virtual bool read (char c[], int n) {
        if (!ifs) 
            throw Iex::InputExc ("Unexpected end of file.");
		
        errno = 0;
        ifs.read (c, n);
        return check_error ();
    }
    virtual Imath::Int64 tellg () {
        return std::streamoff (ifs.tellg ());
    }
    virtual void seekg (Imath::Int64 pos) {
        ifs.seekg (pos);
        check_error ();
    }
    virtual void clear () {
        ifs.clear ();
    }

private:
    bool check_error () {
        if (!ifs) {
            if (errno) 
                Iex::throwErrnoExc ();
			
            return false;
        }
        return true;
    }
    OIIO::ifstream ifs;
};



class OpenEXRInput final : public ImageInput {
public:
    OpenEXRInput ();
    virtual ~OpenEXRInput () { close(); }
    virtual const char * format_name (void) const { return "openexr"; }
    virtual int supports (string_view feature) const {
        return (feature == "arbitrary_metadata"
             || feature == "exif"   // Because of arbitrary_metadata
             || feature == "iptc"); // Because of arbitrary_metadata
    }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual int current_miplevel (void) const { return m_miplevel; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_scanlines (int ybegin, int yend, int z, void *data);
    virtual bool read_native_scanlines (int ybegin, int yend, int z,
                                        int chbegin, int chend, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend, void *data);
    virtual bool read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend,
                                    int chbegin, int chend, void *data);
    virtual bool read_native_deep_scanlines (int ybegin, int yend, int z,
                                             int chbegin, int chend,
                                             DeepData &deepdata);
    virtual bool read_native_deep_tiles (int xbegin, int xend,
                                         int ybegin, int yend,
                                         int zbegin, int zend,
                                         int chbegin, int chend,
                                         DeepData &deepdata);

private:
    struct PartInfo {
        bool initialized;
        ImageSpec spec;
        int topwidth;                     ///< Width of top mip level
        int topheight;                    ///< Height of top mip level
        int levelmode;                    ///< The level mode
        int roundingmode;                 ///< Rounding mode
        bool cubeface;                    ///< It's a cubeface environment map
        int nmiplevels;                   ///< How many MIP levels are there?
        Imath::Box2i top_datawindow;
        Imath::Box2i top_displaywindow;
        std::vector<Imf::PixelType> pixeltype; ///< Imf pixel type for each chan
        std::vector<int> chanbytes;       ///< Size (in bytes) of each channel

        PartInfo () : initialized(false) { }
        ~PartInfo () { }
        bool parse_header (OpenEXRInput *in, const Imf::Header *header);
        bool query_channels (OpenEXRInput *in, const Imf::Header *header);
    };

    std::vector<PartInfo> m_parts;        ///< Image parts
    OpenEXRInputStream *m_input_stream;   ///< Stream for input file
    Imf::MultiPartInputFile *m_input_multipart;   ///< Multipart input
    Imf::InputPart *m_scanline_input_part;
    Imf::TiledInputPart *m_tiled_input_part;
    Imf::DeepScanLineInputPart *m_deep_scanline_input_part;
    Imf::DeepTiledInputPart *m_deep_tiled_input_part;
    Imf::InputFile *m_input_scanline;     ///< Input for scanline files
    Imf::TiledInputFile *m_input_tiled;   ///< Input for tiled files
    int m_subimage;                       ///< What subimage are we looking at?
    int m_nsubimages;                     ///< How many subimages are there?
    int m_miplevel;                       ///< What MIP level are we looking at?

    void init () {
        m_input_stream = NULL;
        m_input_multipart = NULL;
        m_scanline_input_part = NULL;
        m_tiled_input_part = NULL;
        m_deep_scanline_input_part = NULL;
        m_deep_tiled_input_part = NULL;
        m_input_scanline = NULL;
        m_input_tiled = NULL;
        m_subimage = -1;
        m_miplevel = -1;
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput *
openexr_input_imageio_create ()
{
    return new OpenEXRInput;
}

// OIIO_EXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION; // it's in exroutput.cpp

OIIO_EXPORT const char * openexr_input_extensions[] = {
    "exr", "sxr", "mxr", NULL
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
        m_map["xDensity"] = "XResolution";
        m_map["expTime"] = "ExposureTime";
        // Ones we don't rename -- OpenEXR convention matches ours
        m_map["wrapmodes"] = "wrapmodes";
        m_map["aperture"] = "FNumber";
        // Ones to prefix with openexr:
        m_map["version"] = "openexr:version";
        m_map["chunkCount"] = "openexr:chunkCount";
        m_map["maxSamplesPerPixel"] = "openexr:maxSamplesPerPixel";
        m_map["dwaCompressionLevel"] = "openexr:dwaCompressionLevel";
        // Ones to skip because we handle specially
        m_map["channels"] = "";
        m_map["compression"] = "";
        m_map["dataWindow"] = "";
        m_map["displayWindow"] = "";
        m_map["envmap"] = "";
        m_map["tiledesc"] = "";
        m_map["tiles"] = "";
        m_map["openexr:lineOrder"] = "";
        m_map["type"] = "";
        // Ones to skip because we consider them irrelevant

//        m_map[""] = "";
        // FIXME: Things to consider in the future:
        // preview
        // screenWindowCenter
        // adoptedNeutral
        // renderingTransform, lookModTransform
        // utcOffset
        // longitude latitude altitude
        // focus isoSpeed
    }
};

static StringMap exr_tag_to_oiio_std;


namespace pvt {

void set_exr_threads ()
{
    static int exr_threads = 0;  // lives in exrinput.cpp
    static spin_mutex exr_threads_mutex;  

    int oiio_threads = 1;
    OIIO::getattribute ("exr_threads", oiio_threads);
    
    // 0 means all threads in OIIO, but single-threaded in OpenEXR
    // -1 means single-threaded in OIIO
    if (oiio_threads == 0) {
        oiio_threads = Sysutil::hardware_concurrency();
    } else if (oiio_threads == -1) {
        oiio_threads = 0;
    }
    spin_lock lock (exr_threads_mutex);
    if (exr_threads != oiio_threads) {
        exr_threads = oiio_threads;
        Imf::setGlobalThreadCount (exr_threads);
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
    if (! Filesystem::is_regular (name)) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }
    bool tiled;
    if (! Imf::isOpenExrFile (name.c_str(), tiled)) {
        error ("\"%s\" is not an OpenEXR file", name.c_str());
        return false;
    }

    pvt::set_exr_threads ();

    m_spec = ImageSpec(); // Clear everything with default constructor
    
    try {
        m_input_stream = new OpenEXRInputStream (name.c_str());
    } catch (const std::exception &e) {
        m_input_stream = NULL;
        error ("OpenEXR exception: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        m_input_stream = NULL;
        error ("OpenEXR exception: unknown");
        return false;
    }

    try {
        m_input_multipart = new Imf::MultiPartInputFile (*m_input_stream);
    } catch (const std::exception &e) {
        delete m_input_stream;
        m_input_stream = NULL;
        error ("OpenEXR exception: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        m_input_stream = NULL;
        error ("OpenEXR exception: unknown");
        return false;
    }

    m_nsubimages = m_input_multipart->parts();
    m_parts.resize (m_nsubimages);
    m_subimage = -1;
    m_miplevel = -1;
    bool ok = seek_subimage (0, 0, newspec);
    if (! ok)
        close ();
    return ok;
}



// Count number of MIPmap levels
inline int
numlevels (int width, int roundingmode)
{
    int nlevels = 1;
    for (  ;  width > 1;  ++nlevels) {
        if (roundingmode == Imf::ROUND_DOWN)
            width = width / 2;
        else
            width = (width + 1) / 2;
    }
    return nlevels;
}



bool
OpenEXRInput::PartInfo::parse_header (OpenEXRInput *in,
                                      const Imf::Header *header)
{
    bool ok = true;
    if (initialized)
        return ok;

    ASSERT (header);
    spec = ImageSpec();

    top_datawindow = header->dataWindow();
    top_displaywindow = header->displayWindow();
    spec.x = top_datawindow.min.x;
    spec.y = top_datawindow.min.y;
    spec.z = 0;
    spec.width  = top_datawindow.max.x - top_datawindow.min.x + 1;
    spec.height = top_datawindow.max.y - top_datawindow.min.y + 1;
    spec.depth = 1;
    topwidth = spec.width;      // Save top-level mipmap dimensions
    topheight = spec.height;
    spec.full_x = top_displaywindow.min.x;
    spec.full_y = top_displaywindow.min.y;
    spec.full_z = 0;
    spec.full_width  = top_displaywindow.max.x - top_displaywindow.min.x + 1;
    spec.full_height = top_displaywindow.max.y - top_displaywindow.min.y + 1;
    spec.full_depth = 1;
    spec.tile_depth = 1;

    if (header->hasTileDescription()
          && Strutil::icontains(header->type(), "tile")) {
        const Imf::TileDescription &td (header->tileDescription());
        spec.tile_width = td.xSize;
        spec.tile_height = td.ySize;
        levelmode = td.mode;
        roundingmode = td.roundingMode;
        if (levelmode == Imf::MIPMAP_LEVELS)
            nmiplevels = numlevels (std::max(topwidth,topheight), roundingmode);
        else if (levelmode == Imf::RIPMAP_LEVELS)
            nmiplevels = numlevels (std::max(topwidth,topheight), roundingmode);
        else
            nmiplevels = 1;
    } else {
        spec.tile_width = 0;
        spec.tile_height = 0;
        levelmode = Imf::ONE_LEVEL;
        nmiplevels = 1;
    }
    if (! query_channels (in, header))   // also sets format
        return false;

    spec.deep = Strutil::istarts_with (header->type(), "deep");

    // Unless otherwise specified, exr files are assumed to be linear.
    spec.attribute ("oiio:ColorSpace", "Linear");

    if (levelmode != Imf::ONE_LEVEL)
        spec.attribute ("openexr:roundingmode", roundingmode);

    const Imf::EnvmapAttribute *envmap;
    envmap = header->findTypedAttribute<Imf::EnvmapAttribute>("envmap");
    if (envmap) {
        cubeface = (envmap->value() == Imf::ENVMAP_CUBE);
        spec.attribute ("textureformat", cubeface ? "CubeFace Environment" : "LatLong Environment");
        // OpenEXR conventions for env maps
        if (! cubeface)
            spec.attribute ("oiio:updirection", "y");
        spec.attribute ("oiio:sampleborder", 1);
        // FIXME - detect CubeFace Shadow?
    } else {
        cubeface = false;
        if (spec.tile_width && levelmode == Imf::MIPMAP_LEVELS)
            spec.attribute ("textureformat", "Plain Texture");
        // FIXME - detect Shadow
    }

    const Imf::CompressionAttribute *compressattr;
    compressattr = header->findTypedAttribute<Imf::CompressionAttribute>("compression");
    if (compressattr) {
        const char *comp = NULL;
        switch (compressattr->value()) {
        case Imf::NO_COMPRESSION    : comp = "none"; break;
        case Imf::RLE_COMPRESSION   : comp = "rle"; break;
        case Imf::ZIPS_COMPRESSION  : comp = "zips"; break;
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
#if defined(OPENEXR_VERSION_MAJOR) && \
    (OPENEXR_VERSION_MAJOR*10000+OPENEXR_VERSION_MINOR*100+OPENEXR_VERSION_PATCH) >= 20200
        case Imf::DWAA_COMPRESSION  : comp = "dwaa"; break;
        case Imf::DWAB_COMPRESSION  : comp = "dwab"; break;
#endif
        default:
            break;
        }
        if (comp)
            spec.attribute ("compression", comp);
    }

    for (auto hit = header->begin(); hit != header->end();  ++hit) {
        const Imf::IntAttribute *iattr;
        const Imf::FloatAttribute *fattr;
        const Imf::StringAttribute *sattr;
        const Imf::M33fAttribute *m33fattr;
        const Imf::M44fAttribute *m44fattr;
        const Imf::V3fAttribute *v3fattr;
        const Imf::V3iAttribute *v3iattr;
        const Imf::V2fAttribute *v2fattr;
        const Imf::V2iAttribute *v2iattr;
        const Imf::Box2iAttribute *b2iattr;
        const Imf::Box2fAttribute *b2fattr;
        const Imf::TimeCodeAttribute *tattr;
        const Imf::KeyCodeAttribute *kcattr;
        const Imf::ChromaticitiesAttribute *crattr;
        const Imf::RationalAttribute *rattr;
        const Imf::StringVectorAttribute *svattr;
        const Imf::DoubleAttribute *dattr;
        const Imf::V2dAttribute *v2dattr;
        const Imf::V3dAttribute *v3dattr;
        const Imf::M33dAttribute *m33dattr;
        const Imf::M44dAttribute *m44dattr;
        const char *name = hit.name();
        std::string oname = exr_tag_to_oiio_std[name];
        if (oname.empty())   // Empty string means skip this attrib
            continue;
//        if (oname == name)
//            oname = std::string(format_name()) + "_" + oname;
        const Imf::Attribute &attrib = hit.attribute();
        std::string type = attrib.typeName();
        if (type == "string" &&
            (sattr = header->findTypedAttribute<Imf::StringAttribute> (name)))
            spec.attribute (oname, sattr->value().c_str());
        else if (type == "int" &&
            (iattr = header->findTypedAttribute<Imf::IntAttribute> (name)))
            spec.attribute (oname, iattr->value());
        else if (type == "float" &&
            (fattr = header->findTypedAttribute<Imf::FloatAttribute> (name)))
            spec.attribute (oname, fattr->value());
        else if (type == "m33f" &&
            (m33fattr = header->findTypedAttribute<Imf::M33fAttribute> (name)))
            spec.attribute (oname, TypeMatrix33, &(m33fattr->value()));
        else if (type == "m44f" &&
            (m44fattr = header->findTypedAttribute<Imf::M44fAttribute> (name)))
            spec.attribute (oname, TypeMatrix44, &(m44fattr->value()));
        else if (type == "v3f" &&
                 (v3fattr = header->findTypedAttribute<Imf::V3fAttribute> (name)))
            spec.attribute (oname, TypeVector, &(v3fattr->value()));
        else if (type == "v3i" &&
                 (v3iattr = header->findTypedAttribute<Imf::V3iAttribute> (name))) {
            TypeDesc v3 (TypeDesc::INT, TypeDesc::VEC3, TypeDesc::VECTOR);
            spec.attribute (oname, v3, &(v3iattr->value()));
        }
        else if (type == "v2f" &&
                 (v2fattr = header->findTypedAttribute<Imf::V2fAttribute> (name))) {
            TypeDesc v2 (TypeDesc::FLOAT,TypeDesc::VEC2);
            spec.attribute (oname, v2, &(v2fattr->value()));
        }
        else if (type == "v2i" &&
                 (v2iattr = header->findTypedAttribute<Imf::V2iAttribute> (name))) {
            TypeDesc v2 (TypeDesc::INT,TypeDesc::VEC2);
            spec.attribute (oname, v2, &(v2iattr->value()));
        }
        else if (type == "stringvector" &&
            (svattr = header->findTypedAttribute<Imf::StringVectorAttribute> (name))) {
            std::vector<std::string> strvec = svattr->value();
            std::vector<ustring> ustrvec (strvec.size());
            for (size_t i = 0, e = strvec.size();  i < e;  ++i)
                ustrvec[i] = strvec[i];
            TypeDesc sv (TypeDesc::STRING, ustrvec.size());
            spec.attribute(oname, sv, &ustrvec[0]);
        }
        else if (type == "double" &&
            (dattr = header->findTypedAttribute<Imf::DoubleAttribute> (name))) {
            TypeDesc d (TypeDesc::DOUBLE);
            spec.attribute (oname, d, &(dattr->value()));
        }
        else if (type == "v2d" &&
                 (v2dattr = header->findTypedAttribute<Imf::V2dAttribute> (name))) {
            TypeDesc v2 (TypeDesc::DOUBLE,TypeDesc::VEC2);
            spec.attribute (oname, v2, &(v2dattr->value()));
        }
        else if (type == "v3d" &&
                 (v3dattr = header->findTypedAttribute<Imf::V3dAttribute> (name))) {
            TypeDesc v3 (TypeDesc::DOUBLE,TypeDesc::VEC3, TypeDesc::VECTOR);
            spec.attribute (oname, v3, &(v3dattr->value()));
        }
        else if (type == "m33d" &&
            (m33dattr = header->findTypedAttribute<Imf::M33dAttribute> (name))) {
            TypeDesc m33 (TypeDesc::DOUBLE, TypeDesc::MATRIX33);
            spec.attribute (oname, m33, &(m33dattr->value()));
        }
        else if (type == "m44d" &&
            (m44dattr = header->findTypedAttribute<Imf::M44dAttribute> (name))) {
            TypeDesc m44 (TypeDesc::DOUBLE, TypeDesc::MATRIX44);
            spec.attribute (oname, m44, &(m44dattr->value()));
        }
        else if (type == "box2i" &&
                 (b2iattr = header->findTypedAttribute<Imf::Box2iAttribute> (name))) {
            TypeDesc bx (TypeDesc::INT, TypeDesc::VEC2, 2);
            spec.attribute (oname, bx, &b2iattr->value());
        }
        else if (type == "box2f" &&
                 (b2fattr = header->findTypedAttribute<Imf::Box2fAttribute> (name))) {
            TypeDesc bx (TypeDesc::FLOAT, TypeDesc::VEC2, 2);
            spec.attribute (oname, bx, &b2fattr->value());
        }
        else if (type == "timecode" &&
                 (tattr = header->findTypedAttribute<Imf::TimeCodeAttribute> (name))) {
            unsigned int timecode[2];
            timecode[0] = tattr->value().timeAndFlags(Imf::TimeCode::TV60_PACKING); //TV60 returns unchanged _time
            timecode[1] = tattr->value().userData();

            // Elevate "timeCode" to smpte:TimeCode
            if (oname == "timeCode")
                oname = "smpte:TimeCode";
            spec.attribute(oname, TypeTimeCode, timecode);
        }
        else if (type == "keycode" &&
                 (kcattr = header->findTypedAttribute<Imf::KeyCodeAttribute> (name))) {
            const Imf::KeyCode *k = &kcattr->value();
            unsigned int keycode[7];
            keycode[0] = k->filmMfcCode();
            keycode[1] = k->filmType();
            keycode[2] = k->prefix();
            keycode[3] = k->count();
            keycode[4] = k->perfOffset();
            keycode[5] = k->perfsPerFrame();
            keycode[6] = k->perfsPerCount();

            // Elevate "keyCode" to smpte:KeyCode
            if (oname == "keyCode")
                oname = "smpte:KeyCode";
            spec.attribute(oname, TypeKeyCode, keycode);
        } else if (type == "chromaticities" &&
                   (crattr = header->findTypedAttribute<Imf::ChromaticitiesAttribute> (name))) {
            const Imf::Chromaticities *chroma = &crattr->value();
            spec.attribute (oname, TypeDesc(TypeDesc::FLOAT,8),
                            (const float *)chroma);
        } 
        else if (type == "rational" &&
                   (rattr = header->findTypedAttribute<Imf::RationalAttribute> (name))) {
            const Imf::Rational *rational = &rattr->value();
            int n = rational->n;
            unsigned int d = rational->d;
            if (d < (1UL << 31)) {
                int r[2];
                r[0] = n;
                r[1] = static_cast<int>(d);
                spec.attribute (oname, TypeRational, r);    
            } else if (int f = static_cast<int>(boost::math::gcd<long int>(rational[0], rational[1])) > 1) {
                int r[2];
                r[0] = n / f;
                r[1] = static_cast<int>(d / f);
               spec.attribute (oname, TypeRational, r);
            } else {
                // TODO: find a way to allow the client to accept "close" rational values
                OIIO::debug ("Don't know what to do with OpenEXR Rational attribute %s with value %d / %u that we cannot represent exactly", oname, n, d);
            }
        }
        else {
#if 0
            std::cerr << "  unknown attribute " << type << ' ' << name << "\n";
#endif
        }
    }

    float aspect = spec.get_float_attribute ("PixelAspectRatio", 0.0f);
    float xdensity = spec.get_float_attribute ("XResolution", 0.0f);
    if (xdensity) {
        // If XResolution is found, supply the YResolution and unit.
        spec.attribute ("YResolution", xdensity * (aspect ? aspect : 1.0f));
        spec.attribute ("ResolutionUnit", "in"); // EXR is always pixels/inch
    }

    // EXR "name" also gets passed along as "oiio:subimagename".
    if (header->hasName())
        spec.attribute ("oiio:subimagename", header->name());

    // Squash some problematic texture metadata if we suspect it's wrong
    pvt::check_texture_metadata_sanity (spec);

    initialized = true;
    return ok;
}



namespace {


static TypeDesc
TypeDesc_from_ImfPixelType (Imf::PixelType ptype)
{
    switch (ptype) {
    case Imf::UINT  : return TypeDesc::UINT;  break;
    case Imf::HALF  : return TypeDesc::HALF;  break;
    case Imf::FLOAT : return TypeDesc::FLOAT; break;
    default: ASSERT_MSG (0, "Unknown Imf::PixelType %d", int(ptype));
    }
}



// Used to hold channel information for sorting into canonical order
struct ChanNameHolder {
    string_view fullname;
    int exr_channel_number;  // channel index in the exr (sorted by name)
    string_view layer;
    string_view suffix;
    int special_index;
    Imf::PixelType exr_data_type;
    TypeDesc datatype;
    int xSampling;
    int ySampling;

    ChanNameHolder (string_view fullname, int n, const Imf::Channel &exrchan)
        : fullname(fullname), exr_channel_number(n), exr_data_type(exrchan.type),
          datatype(TypeDesc_from_ImfPixelType(exrchan.type)),
          xSampling(exrchan.xSampling), ySampling(exrchan.ySampling)
    {
        size_t dot = fullname.find_last_of ('.');
        if (dot == string_view::npos) {
            suffix = fullname;
        } else {
            layer = string_view (fullname.data(), dot+1);
            suffix = string_view (fullname.data()+dot+1, fullname.size()-dot-1);
        }
        static const char * special[] = {
            "R", "Red", "G", "Green", "B", "Blue", "Y", "real", "imag",
            "A", "Alpha", "AR", "RA", "AG", "GA", "AB", "BA",
            "Z", "Depth", "Zback", NULL
        };
        special_index = 10000;
        for (int i = 0; special[i]; ++i)
            if (Strutil::iequals (suffix, special[i])) {
                special_index = i;
                break;
            }
    }

    static bool compare_cnh (const ChanNameHolder &a, const ChanNameHolder &b)
    {
        if (a.layer < b.layer)
            return true;
        if (a.layer > b.layer)
            return false;
        // Within the same layer
        if (a.special_index < b.special_index)
            return true;
        if (a.special_index > b.special_index)
            return false;
        return a.suffix < b.suffix;
    }
};

} // anon namespace



bool
OpenEXRInput::PartInfo::query_channels (OpenEXRInput *in,
                                        const Imf::Header *header)
{
    ASSERT (! initialized);
    bool ok = true;
    spec.nchannels = 0;
    const Imf::ChannelList &channels (header->channels());
    std::vector<std::string> channelnames;  // Order of channels in file
    std::vector<ChanNameHolder> cnh;
    int c = 0;
    for (auto ci = channels.begin(); ci != channels.end();  ++c, ++ci)
        cnh.emplace_back (ci.name(), c, ci.channel());
    spec.nchannels = int(cnh.size());
    std::sort (cnh.begin(), cnh.end(), ChanNameHolder::compare_cnh);
    // Now we should have cnh sorted into the order that we want to present
    // to the OIIO client.
    spec.format = TypeDesc::UNKNOWN;
    bool all_one_format = true;
    for (int c = 0; c < spec.nchannels; ++c) {
        spec.channelnames.push_back (cnh[c].fullname);
        spec.channelformats.push_back (cnh[c].datatype);
        spec.format = TypeDesc(ImageBufAlgo::type_merge (TypeDesc::BASETYPE(spec.format.basetype),
                                                         TypeDesc::BASETYPE(cnh[c].datatype.basetype)));
        pixeltype.push_back (cnh[c].exr_data_type);
        chanbytes.push_back (cnh[c].datatype.size());
        all_one_format &= (cnh[c].datatype == cnh[0].datatype);
        if (spec.alpha_channel < 0 && (Strutil::iequals (cnh[c].suffix, "A") ||
                                       Strutil::iequals (cnh[c].suffix, "Alpha")))
            spec.alpha_channel = c;
        if (spec.z_channel < 0 && (Strutil::iequals (cnh[c].suffix, "Z") ||
                                   Strutil::iequals (cnh[c].suffix, "Depth")))
            spec.z_channel = c;
        if (cnh[c].xSampling != 1 || cnh[c].ySampling != 1) {
            ok = false;
            in->error ("Subsampled channels are not supported (channel \"%s\" has sampling %d,%d).",
                       cnh[c].fullname, cnh[c].xSampling, cnh[c].ySampling);
            // FIXME: Some day, we should handle channel subsampling.
        }
    }
    ASSERT ((int)spec.channelnames.size() == spec.nchannels);
    ASSERT (spec.format != TypeDesc::UNKNOWN);
    if (all_one_format)
        spec.channelformats.clear();
    return ok;
}



bool
OpenEXRInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (subimage < 0 || subimage >= m_nsubimages)   // out of range
        return false;

    if (subimage == m_subimage && miplevel == m_miplevel) {  // no change
        newspec = m_spec;
        return true;
    }

    PartInfo &part (m_parts[subimage]);
    if (! part.initialized) {
        const Imf::Header *header = NULL;
        if (m_input_multipart)
            header = &(m_input_multipart->header(subimage));
        if (! part.parse_header (this, header))
            return false;
        part.initialized = true;
    }

    if (subimage != m_subimage) {
        delete m_scanline_input_part;  m_scanline_input_part = NULL;
        delete m_tiled_input_part;  m_tiled_input_part = NULL;
        delete m_deep_scanline_input_part;  m_deep_scanline_input_part = NULL;
        delete m_deep_tiled_input_part;  m_deep_tiled_input_part = NULL;
        try {
            if (part.spec.deep) {
                if (part.spec.tile_width)
                    m_deep_tiled_input_part = new Imf::DeepTiledInputPart (*m_input_multipart, subimage);
                else
                    m_deep_scanline_input_part = new Imf::DeepScanLineInputPart (*m_input_multipart, subimage);
            } else {
                if (part.spec.tile_width)
                    m_tiled_input_part = new Imf::TiledInputPart (*m_input_multipart, subimage);
                else
                    m_scanline_input_part = new Imf::InputPart (*m_input_multipart, subimage);
            }
        } catch (const std::exception &e) {
            error ("OpenEXR exception: %s", e.what());
            m_scanline_input_part = NULL;
            m_tiled_input_part = NULL;
            m_deep_scanline_input_part = NULL;
            m_deep_tiled_input_part = NULL;
            return false;
        } catch (...) {   // catch-all for edge cases or compiler bugs
            error ("OpenEXR exception: unknown");
            m_scanline_input_part = NULL;
            m_tiled_input_part = NULL;
            m_deep_scanline_input_part = NULL;
            m_deep_tiled_input_part = NULL;
            return false;
        }
    }

    m_subimage = subimage;

    if (miplevel < 0 || miplevel >= part.nmiplevels)   // out of range
        return false;

    m_miplevel = miplevel;
    m_spec = part.spec;

    if (miplevel == 0 && part.levelmode == Imf::ONE_LEVEL) {
        newspec = m_spec;
        return true;
    }

    // Compute the resolution of the requested mip level.
    int w = part.topwidth, h = part.topheight;
    if (part.levelmode == Imf::MIPMAP_LEVELS) {
        while (miplevel--) {
            if (part.roundingmode == Imf::ROUND_DOWN) {
                w = w / 2;
                h = h / 2;
            } else {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
            }
            w = std::max (1, w);
            h = std::max (1, h);
        }
    } else if (part.levelmode == Imf::RIPMAP_LEVELS) {
        // FIXME
    } else {
        ASSERT_MSG (0, "Unknown levelmode %d", int(part.levelmode));
    }

    m_spec.width = w;
    m_spec.height = h;
    // N.B. OpenEXR doesn't support data and display windows per MIPmap
    // level.  So always take from the top level.
    Imath::Box2i datawindow = part.top_datawindow;
    Imath::Box2i displaywindow = part.top_displaywindow;
    m_spec.x = datawindow.min.x;
    m_spec.y = datawindow.min.y;
    if (m_miplevel == 0) {
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
    if (part.cubeface) {
        m_spec.full_width = w;
        m_spec.full_height = w;
    }
    newspec = m_spec;

    return true;
}



bool
OpenEXRInput::close ()
{
    delete m_input_multipart;
    delete m_scanline_input_part;
    delete m_tiled_input_part;
    delete m_deep_scanline_input_part;
    delete m_deep_tiled_input_part;
    delete m_input_scanline;
    delete m_input_tiled;
    delete m_input_stream;
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
                                     int chbegin, int chend, void *data)
{
    chend = clamp (chend, chbegin+1, m_spec.nchannels);
//    std::cerr << "openexr rns " << ybegin << ' ' << yend << ", channels "
//              << chbegin << "-" << (chend-1) << "\n";
    if (m_input_scanline == NULL && m_scanline_input_part == NULL) {
        error ("called OpenEXRInput::read_native_scanlines without an open file");
        return false;
    }

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    const PartInfo &part (m_parts[m_subimage]);
    size_t pixelbytes = m_spec.pixel_bytes (chbegin, chend, true);
    size_t scanlinebytes = (size_t)m_spec.width * pixelbytes;
    char *buf = (char *)data
              - m_spec.x * pixelbytes
              - ybegin * scanlinebytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = chbegin;  c < chend;  ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (part.pixeltype[c],
                                            buf + chanoffset,
                                            pixelbytes, scanlinebytes));
            chanoffset += chanbytes;
        }
        if (m_input_scanline) {
            m_input_scanline->setFrameBuffer (frameBuffer);
            m_input_scanline->readPixels (ybegin, yend-1);
        } else if (m_scanline_input_part) {
            m_scanline_input_part->setFrameBuffer (frameBuffer);
            m_scanline_input_part->readPixels (ybegin, yend-1);
        } else {
            error ("Attempted to read scanline from a non-scanline file.");
            return false;
        }
    } catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        error ("Failed OpenEXR read: unknown exception");
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
                                 int chbegin, int chend, void *data)
{
    chend = clamp (chend, chbegin+1, m_spec.nchannels);
#if 0
    std::cerr << "openexr rnt " << xbegin << ' ' << xend << ' ' << ybegin 
              << ' ' << yend << ", chans " << chbegin
              << "-" << (chend-1) << "\n";
#endif
    if (! (m_input_tiled || m_tiled_input_part) ||
        ! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend)) {
        error ("called OpenEXRInput::read_native_tiles without an open file");
        return false;
    }

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    const PartInfo &part (m_parts[m_subimage]);
    size_t pixelbytes = m_spec.pixel_bytes (chbegin, chend, true);
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

    std::unique_ptr<char[]> tmpbuf;
    void *origdata = data;
    if (whole_width != (xend-xbegin) || whole_height != (yend-ybegin)) {
        // Deal with the case of reading not a whole number of tiles --
        // OpenEXR will happily overwrite user memory in this case.
        tmpbuf.reset (new char [nxtiles * nytiles * m_spec.tile_bytes(true)]);
        data = &tmpbuf[0];
    }
    char *buf = (char *)data
              - xbegin * pixelbytes
              - ybegin * pixelbytes * m_spec.tile_width * nxtiles;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = chbegin;  c < chend;  ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert (m_spec.channelnames[c].c_str(),
                                Imf::Slice (part.pixeltype[c],
                                            buf + chanoffset, pixelbytes,
                                            pixelbytes*m_spec.tile_width*nxtiles));
            chanoffset += chanbytes;
        }
        if (m_input_tiled) {
            m_input_tiled->setFrameBuffer (frameBuffer);
            m_input_tiled->readTiles (firstxtile, firstxtile+nxtiles-1,
                                      firstytile, firstytile+nytiles-1,
                                      m_miplevel, m_miplevel);
        } else if (m_tiled_input_part) {
            m_tiled_input_part->setFrameBuffer (frameBuffer);
            m_tiled_input_part->readTiles (firstxtile, firstxtile+nxtiles-1,
                                           firstytile, firstytile+nytiles-1,
                                           m_miplevel, m_miplevel);
        } else {
            error ("Attempted to read tiles from a non-tiled file");
            return false;
        }
        if (data != origdata) {
            stride_t user_scanline_bytes = (xend-xbegin) * pixelbytes;
            stride_t scanline_stride = nxtiles*m_spec.tile_width*pixelbytes;
            for (int y = ybegin;  y < yend;  ++y)
                memcpy ((char *)origdata+(y-ybegin)*scanline_stride,
                        (char *)data+(y-ybegin)*scanline_stride,
                        user_scanline_bytes);
        }
    } catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        error ("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXRInput::read_native_deep_scanlines (int ybegin, int yend, int z,
                                          int chbegin, int chend,
                                          DeepData &deepdata)
{
    if (m_deep_scanline_input_part == NULL) {
        error ("called OpenEXRInput::read_native_deep_scanlines without an open file");
        return false;
    }

    try {
        const PartInfo &part (m_parts[m_subimage]);
        size_t npixels = (yend - ybegin) * m_spec.width;
        chend = clamp (chend, chbegin+1, m_spec.nchannels);
        int nchans = chend - chbegin;

        // Set up the count and pointers arrays and the Imf framebuffer
        std::vector<TypeDesc> channeltypes;
        m_spec.get_channelformats (channeltypes);
        deepdata.init (npixels, nchans,
                       array_view<const TypeDesc>(&channeltypes[chbegin], chend-chbegin),
                       spec().channelnames);
        std::vector<unsigned int> all_samples (npixels);
        std::vector<void*> pointerbuf (npixels*nchans);
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice (Imf::UINT,
                               (char *)(&all_samples[0]
                                        - m_spec.x
                                        - ybegin*m_spec.width),
                               sizeof(unsigned int),
                               sizeof(unsigned int) * m_spec.width);
        frameBuffer.insertSampleCountSlice (countslice);

        for (int c = chbegin;  c < chend;  ++c) {
            Imf::DeepSlice slice (part.pixeltype[c],
                                  (char *)(&pointerbuf[0]+(c-chbegin)
                                           - m_spec.x * nchans
                                           - ybegin*m_spec.width*nchans),
                                  sizeof(void*) * nchans, // xstride of pointer array
                                  sizeof(void*) * nchans*m_spec.width, // ystride of pointer array
                                  deepdata.samplesize()); // stride of data sample
            frameBuffer.insert (m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_scanline_input_part->setFrameBuffer (frameBuffer);

        // Get the sample counts for each pixel and compute the total
        // number of samples and resize the data area appropriately.
        m_deep_scanline_input_part->readPixelSampleCounts (ybegin, yend-1);
        deepdata.set_all_samples (all_samples);
        deepdata.get_pointers (pointerbuf);

        // Read the pixels
        m_deep_scanline_input_part->readPixels (ybegin, yend-1);
        // deepdata.import_chansamp (pointerbuf);
    } catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        error ("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXRInput::read_native_deep_tiles (int xbegin, int xend,
                                      int ybegin, int yend,
                                      int zbegin, int zend,
                                      int chbegin, int chend,
                                      DeepData &deepdata)
{
    if (m_deep_tiled_input_part == NULL) {
        error ("called OpenEXRInput::read_native_deep_tiles without an open file");
        return false;
    }

    try {
        const PartInfo &part (m_parts[m_subimage]);
        size_t width = (xend - xbegin);
        size_t npixels = width * (yend - ybegin) * (zend - zbegin);
        chend = clamp (chend, chbegin+1, m_spec.nchannels);
        int nchans = chend - chbegin;

        // Set up the count and pointers arrays and the Imf framebuffer
        std::vector<TypeDesc> channeltypes;
        m_spec.get_channelformats (channeltypes);
        deepdata.init (npixels, nchans,
                       array_view<const TypeDesc>(&channeltypes[chbegin], chend-chbegin),
                       spec().channelnames);
        std::vector<unsigned int> all_samples (npixels);
        std::vector<void*> pointerbuf (npixels * nchans);
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice (Imf::UINT,
                               (char *)(&all_samples[0]
                                        - xbegin
                                        - ybegin*width),
                               sizeof(unsigned int),
                               sizeof(unsigned int) * width);
        frameBuffer.insertSampleCountSlice (countslice);
        for (int c = chbegin;  c < chend;  ++c) {
            Imf::DeepSlice slice (part.pixeltype[c],
                                  (char *)(&pointerbuf[0]+(c-chbegin)
                                           - xbegin*nchans
                                           - ybegin*width*nchans),
                                  sizeof(void*) * nchans, // xstride of pointer array
                                  sizeof(void*) * nchans*width, // ystride of pointer array
                                  deepdata.samplesize()); // stride of data sample
            frameBuffer.insert (m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_tiled_input_part->setFrameBuffer (frameBuffer);

        int xtiles = round_to_multiple (xend-xbegin, m_spec.tile_width) / m_spec.tile_width;
        int ytiles = round_to_multiple (yend-ybegin, m_spec.tile_height) / m_spec.tile_height;

        int firstxtile = (xbegin - m_spec.x) / m_spec.tile_width;
        int firstytile = (ybegin - m_spec.y) / m_spec.tile_height;

        // Get the sample counts for each pixel and compute the total
        // number of samples and resize the data area appropriately.
        m_deep_tiled_input_part->readPixelSampleCounts (
                firstxtile, firstxtile+xtiles-1,
                firstytile, firstytile+ytiles-1);
        deepdata.set_all_samples (all_samples);
        deepdata.get_pointers (pointerbuf);

        // Read the pixels
        m_deep_tiled_input_part->readTiles (
                firstxtile, firstxtile+xtiles-1,
                firstytile, firstytile+ytiles-1,
                m_miplevel, m_miplevel);
    } catch (const std::exception &e) {
        error ("Failed OpenEXR read: %s", e.what());
        return false;
    } catch (...) {   // catch-all for edge cases or compiler bugs
        error ("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}


OIIO_PLUGIN_NAMESPACE_END
