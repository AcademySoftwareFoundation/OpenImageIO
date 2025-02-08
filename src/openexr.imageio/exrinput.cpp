// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/platform.h>

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfEnvmap.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgba.h>
#include <OpenEXR/ImfTestFile.h>
#include <OpenEXR/ImfTiledInputFile.h>

#include "exr_pvt.h"

// The way that OpenEXR uses dynamic casting for attributes requires
// temporarily suspending "hidden" symbol visibility mode.
OIIO_PRAGMA_VISIBILITY_PUSH
OIIO_PRAGMA_WARNING_PUSH
OIIO_GCC_PRAGMA(GCC diagnostic ignored "-Wunused-parameter")
#include <OpenEXR/IexBaseExc.h>
#include <OpenEXR/IexThrowErrnoExc.h>
#include <OpenEXR/ImfBoxAttribute.h>
#include <OpenEXR/ImfChromaticitiesAttribute.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfDeepFrameBuffer.h>
#include <OpenEXR/ImfDeepScanLineInputPart.h>
#include <OpenEXR/ImfDeepTiledInputPart.h>
#include <OpenEXR/ImfDoubleAttribute.h>
#include <OpenEXR/ImfEnvmapAttribute.h>
#include <OpenEXR/ImfFloatAttribute.h>
#include <OpenEXR/ImfHeader.h>
#if OPENEXR_HAS_FLOATVECTOR
#    include <OpenEXR/ImfFloatVectorAttribute.h>
#endif
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfIntAttribute.h>
#include <OpenEXR/ImfKeyCodeAttribute.h>
#include <OpenEXR/ImfLineOrderAttribute.h>
#include <OpenEXR/ImfMatrixAttribute.h>
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfRationalAttribute.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfStringVectorAttribute.h>
#include <OpenEXR/ImfTiledInputPart.h>
#include <OpenEXR/ImfTimeCodeAttribute.h>
#include <OpenEXR/ImfVecAttribute.h>
OIIO_PRAGMA_WARNING_POP
OIIO_PRAGMA_VISIBILITY_POP

#include <OpenEXR/ImfCRgbaFile.h>

#if OPENEXR_CODED_VERSION >= 30100 && defined(OIIO_USE_EXR_C_API)
#    define USE_OPENEXR_CORE
#endif

#include "imageio_pvt.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>


OIIO_PLUGIN_NAMESPACE_BEGIN



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
openexr_input_imageio_create()
{
#ifdef USE_OPENEXR_CORE
    if (pvt::openexr_core) {
        // Strutil::print("selecting core\n");
        extern ImageInput* openexrcore_input_imageio_create();
        return openexrcore_input_imageio_create();
    }
#endif
    return new OpenEXRInput;
}

// OIIO_EXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION; // it's in exroutput.cpp

OIIO_EXPORT const char* openexr_input_extensions[] = { "exr", "sxr", "mxr",
                                                       nullptr };

OIIO_PLUGIN_EXPORTS_END


static std::map<std::string, std::string> exr_tag_to_oiio_std {
    // Ones whose name we change to our convention
    { "cameraTransform", "worldtocamera" },
    { "capDate", "DateTime" },
    { "comments", "ImageDescription" },
    { "owner", "Copyright" },
    { "pixelAspectRatio", "PixelAspectRatio" },
    { "xDensity", "XResolution" },
    { "expTime", "ExposureTime" },
    // Ones we don't rename -- OpenEXR convention matches ours
    { "wrapmodes", "wrapmodes" },
    { "aperture", "FNumber" },
    // Ones to prefix with openexr:
    { "chunkCount", "openexr:chunkCount" },
    { "maxSamplesPerPixel", "openexr:maxSamplesPerPixel" },
    { "dwaCompressionLevel", "openexr:dwaCompressionLevel" },
    // Ones to skip because we handle specially or consider them irrelevant
    { "channels", "" },
    { "compression", "" },
    { "dataWindow", "" },
    { "displayWindow", "" },
    { "envmap", "" },
    { "tiledesc", "" },
    { "tiles", "" },
    { "type", "" },

    // FIXME: Things to consider in the future:
    // preview
    // screenWindowCenter
    // adoptedNeutral
    // renderingTransform, lookModTransform
    // utcOffset
    // longitude latitude altitude
    // focus isoSpeed
};



namespace pvt {

void
set_exr_threads();


// Split a full channel name into layer and suffix.
void
split_name(string_view fullname, string_view& layer, string_view& suffix)
{
    size_t dot = fullname.find_last_of('.');
    if (dot == string_view::npos) {
        suffix = fullname;
        layer  = string_view();
    } else {
        layer  = string_view(fullname.data(), dot + 1);
        suffix = string_view(fullname.data() + dot + 1,
                             fullname.size() - dot - 1);
    }
}


inline bool
str_equal_either(string_view str, string_view a, string_view b)
{
    return Strutil::iequals(str, a) || Strutil::iequals(str, b);
}


// Do the channels appear to be R, G, B (or known common aliases)?
bool
channels_are_rgb(const ImageSpec& spec)
{
    return spec.nchannels >= 3
           && str_equal_either(spec.channel_name(0), "R", "Red")
           && str_equal_either(spec.channel_name(1), "G", "Green")
           && str_equal_either(spec.channel_name(2), "B", "Blue");
}

}  // namespace pvt



OpenEXRInput::OpenEXRInput() { init(); }



bool
OpenEXRInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    try {
        OpenEXRInputStream IStream("", ioproxy);
        return Imf::isOpenExrFile(IStream);
    } catch (const std::exception& e) {
        return false;
    }
}



bool
OpenEXRInput::open(const std::string& name, ImageSpec& newspec,
                   const ImageSpec& config)
{
    // First thing's first. See if we're been given an IOProxy. We have to
    // do this before the check for non-exr files, that's why it's here and
    // not where the rest of the configuration hints are handled.
    const ParamValue* param = config.find_attribute("oiio:ioproxy",
                                                    TypeDesc::PTR);
    if (param)
        m_io = param->get<Filesystem::IOProxy*>();

    // Quick check to immediately reject nonexistent or non-exr files.
    if (!m_io && !Filesystem::is_regular(name)) {
        errorfmt("Could not open file \"{}\"", name);
        return false;
    }

    // If we weren't given an IOProxy, create one now that just reads from
    // the file.
    if (!m_io) {
        m_io = new Filesystem::IOFile(name, Filesystem::IOProxy::Read);
        m_local_io.reset(m_io);
    }
    OIIO_ASSERT(m_io);

    if (!valid_file(m_io)) {
        errorfmt("\"{}\" is not an OpenEXR file", name);
        return false;
    }

    // Check any other configuration hints

    // "missingcolor" gives fill color for missing scanlines or tiles.
    if (const ParamValue* m = config.find_attribute("oiio:missingcolor")) {
        if (m->type().basetype == TypeDesc::STRING) {
            // missingcolor as string
            m_missingcolor = Strutil::extract_from_list_string<float>(
                m->get_string());
        } else {
            // missingcolor as numeric array
            int n = m->type().basevalues();
            m_missingcolor.clear();
            m_missingcolor.reserve(n);
            for (int i = 0; i < n; ++i)
                m_missingcolor[i] = m->get_float(i);
        }
    } else {
        // If not passed explicit, is there a global setting?
        std::string mc = OIIO::get_string_attribute("missingcolor");
        if (mc.size())
            m_missingcolor = Strutil::extract_from_list_string<float>(mc);
    }

    // Before engaging further with OpenEXR, make sure it is using the right
    // number of threads.
    pvt::set_exr_threads();

    // Clear the spec with default constructor
    m_spec = ImageSpec();

    // Establish an input stream.
    try {
        if (m_io->mode() != Filesystem::IOProxy::Read) {
            // If the proxy couldn't be opened in write mode, try to
            // return an error.
            std::string e = m_io->error();
            errorfmt("Could not open \"{}\" ({})", name,
                     e.size() ? e : std::string("unknown error"));
            return false;
        }
        m_io->seek(0);
        m_input_stream = new OpenEXRInputStream(name.c_str(), m_io);
    } catch (const std::exception& e) {
        m_input_stream = NULL;
        errorfmt("OpenEXR exception: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        m_input_stream = NULL;
        errorfmt("OpenEXR exception: unknown");
        return false;
    }

    // Read the header by constructing a MultiPartInputFile from the input
    // stream.
    try {
        m_input_multipart = new Imf::MultiPartInputFile(*m_input_stream);
    } catch (const std::exception& e) {
        delete m_input_stream;
        m_input_stream = NULL;
        errorfmt("OpenEXR exception: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        m_input_stream = NULL;
        errorfmt("OpenEXR exception: unknown");
        return false;
    }

    m_nsubimages = m_input_multipart->parts();
    m_parts.resize(m_nsubimages);
    m_subimage = -1;
    m_miplevel = -1;

    // Set up for the first subimage ("part"). This will trigger reading
    // information about all the parts.
    bool ok = seek_subimage(0, 0);
    if (ok)
        newspec = m_spec;
    else
        close();
    return ok;
}



// Count number of MIPmap levels
inline int
numlevels(int width, int roundingmode)
{
    int nlevels = 1;
    for (; width > 1; ++nlevels) {
        if (roundingmode == Imf::ROUND_DOWN)
            width = width / 2;
        else
            width = (width + 1) / 2;
    }
    return nlevels;
}



bool
OpenEXRInput::PartInfo::parse_header(OpenEXRInput* in,
                                     const Imf::Header* header)
{
    bool ok = true;
    if (initialized)
        return ok;

    ImageInput::lock_guard lock(*in);
    OIIO_DASSERT(header);
    spec = ImageSpec();

    top_datawindow    = header->dataWindow();
    top_displaywindow = header->displayWindow();
    spec.x            = top_datawindow.min.x;
    spec.y            = top_datawindow.min.y;
    spec.z            = 0;
    spec.width        = top_datawindow.max.x - top_datawindow.min.x + 1;
    spec.height       = top_datawindow.max.y - top_datawindow.min.y + 1;
    spec.depth        = 1;
    topwidth          = spec.width;  // Save top-level mipmap dimensions
    topheight         = spec.height;
    spec.full_x       = top_displaywindow.min.x;
    spec.full_y       = top_displaywindow.min.y;
    spec.full_z       = 0;
    spec.full_width   = top_displaywindow.max.x - top_displaywindow.min.x + 1;
    spec.full_height  = top_displaywindow.max.y - top_displaywindow.min.y + 1;
    spec.full_depth   = 1;
    spec.tile_depth   = 1;

    if (header->hasTileDescription()
        && Strutil::icontains(header->type(), "tile")) {
        const Imf::TileDescription& td(header->tileDescription());
        spec.tile_width  = td.xSize;
        spec.tile_height = td.ySize;
        levelmode        = td.mode;
        roundingmode     = td.roundingMode;
        if (levelmode == Imf::MIPMAP_LEVELS)
            nmiplevels = numlevels(std::max(topwidth, topheight), roundingmode);
        else if (levelmode == Imf::RIPMAP_LEVELS)
            nmiplevels = numlevels(std::max(topwidth, topheight), roundingmode);
        else
            nmiplevels = 1;
    } else {
        spec.tile_width  = 0;
        spec.tile_height = 0;
        levelmode        = Imf::ONE_LEVEL;
        nmiplevels       = 1;
    }
    if (!query_channels(in, header))  // also sets format
        return false;

    spec.deep = Strutil::istarts_with(header->type(), "deep");

    // Unless otherwise specified, exr files are assumed to be linear Rec709
    // if the channels appear to be R, G, B.  I know this suspect, but I'm
    // betting that this heuristic will guess the right thing that users want
    // more often than if we pretending we have no idea what the color space
    // is.
    if (pvt::channels_are_rgb(spec))
        spec.set_colorspace("lin_rec709");

    if (levelmode != Imf::ONE_LEVEL)
        spec.attribute("openexr:roundingmode", roundingmode);

    const Imf::EnvmapAttribute* envmap;
    envmap = header->findTypedAttribute<Imf::EnvmapAttribute>("envmap");
    if (envmap) {
        cubeface = (envmap->value() == Imf::ENVMAP_CUBE);
        spec.attribute("textureformat", cubeface ? "CubeFace Environment"
                                                 : "LatLong Environment");
        // OpenEXR conventions for env maps
        if (!cubeface)
            spec.attribute("oiio:updirection", "y");
        spec.attribute("oiio:sampleborder", 1);
        // FIXME - detect CubeFace Shadow?
    } else {
        cubeface = false;
        if (spec.tile_width && levelmode == Imf::MIPMAP_LEVELS)
            spec.attribute("textureformat", "Plain Texture");
        // FIXME - detect Shadow
    }

    const Imf::CompressionAttribute* compressattr;
    compressattr = header->findTypedAttribute<Imf::CompressionAttribute>(
        "compression");
    if (compressattr) {
        const char* comp = NULL;
        switch (compressattr->value()) {
        case Imf::NO_COMPRESSION: comp = "none"; break;
        case Imf::RLE_COMPRESSION: comp = "rle"; break;
        case Imf::ZIPS_COMPRESSION: comp = "zips"; break;
        case Imf::ZIP_COMPRESSION: comp = "zip"; break;
        case Imf::PIZ_COMPRESSION: comp = "piz"; break;
        case Imf::PXR24_COMPRESSION: comp = "pxr24"; break;
        case Imf::B44_COMPRESSION: comp = "b44"; break;
        case Imf::B44A_COMPRESSION: comp = "b44a"; break;
        case Imf::DWAA_COMPRESSION: comp = "dwaa"; break;
        case Imf::DWAB_COMPRESSION: comp = "dwab"; break;
        default: break;
        }
        if (comp)
            spec.attribute("compression", comp);
    }

    for (auto hit = header->begin(); hit != header->end(); ++hit) {
        const Imf::IntAttribute* iattr;
        const Imf::FloatAttribute* fattr;
        const Imf::StringAttribute* sattr;
        const Imf::M33fAttribute* m33fattr;
        const Imf::M44fAttribute* m44fattr;
        const Imf::V3fAttribute* v3fattr;
        const Imf::V3iAttribute* v3iattr;
        const Imf::V2fAttribute* v2fattr;
        const Imf::V2iAttribute* v2iattr;
        const Imf::Box2iAttribute* b2iattr;
        const Imf::Box2fAttribute* b2fattr;
        const Imf::TimeCodeAttribute* tattr;
        const Imf::KeyCodeAttribute* kcattr;
        const Imf::ChromaticitiesAttribute* crattr;
        const Imf::RationalAttribute* rattr;
#if OPENEXR_HAS_FLOATVECTOR
        const Imf::FloatVectorAttribute* fvattr;
#endif
        const Imf::StringVectorAttribute* svattr;
        const Imf::DoubleAttribute* dattr;
        const Imf::V2dAttribute* v2dattr;
        const Imf::V3dAttribute* v3dattr;
        const Imf::M33dAttribute* m33dattr;
        const Imf::M44dAttribute* m44dattr;
        const Imf::LineOrderAttribute* lattr;
        const char* name = hit.name();
        auto found       = exr_tag_to_oiio_std.find(name);
        std::string oname(found != exr_tag_to_oiio_std.end() ? found->second
                                                             : name);
        if (oname.empty())  // Empty string means skip this attrib
            continue;
        //        if (oname == name)
        //            oname = std::string(format_name()) + "_" + oname;
        const Imf::Attribute& attrib = hit.attribute();
        std::string type             = attrib.typeName();
        if (type == "string"
            && (sattr = header->findTypedAttribute<Imf::StringAttribute>(
                    name))) {
            if (sattr->value().size())
                spec.attribute(oname, sattr->value().c_str());
        } else if (type == "int"
                   && (iattr = header->findTypedAttribute<Imf::IntAttribute>(
                           name)))
            spec.attribute(oname, iattr->value());
        else if (type == "float"
                 && (fattr = header->findTypedAttribute<Imf::FloatAttribute>(
                         name)))
            spec.attribute(oname, fattr->value());
        else if (type == "m33f"
                 && (m33fattr = header->findTypedAttribute<Imf::M33fAttribute>(
                         name)))
            spec.attribute(oname, TypeMatrix33, &(m33fattr->value()));
        else if (type == "m44f"
                 && (m44fattr = header->findTypedAttribute<Imf::M44fAttribute>(
                         name)))
            spec.attribute(oname, TypeMatrix44, &(m44fattr->value()));
        else if (type == "v3f"
                 && (v3fattr = header->findTypedAttribute<Imf::V3fAttribute>(
                         name)))
            spec.attribute(oname, TypeVector, &(v3fattr->value()));
        else if (type == "v3i"
                 && (v3iattr = header->findTypedAttribute<Imf::V3iAttribute>(
                         name))) {
            TypeDesc v3(TypeDesc::INT, TypeDesc::VEC3, TypeDesc::VECTOR);
            spec.attribute(oname, v3, &(v3iattr->value()));
        } else if (type == "v2f"
                   && (v2fattr = header->findTypedAttribute<Imf::V2fAttribute>(
                           name))) {
            TypeDesc v2(TypeDesc::FLOAT, TypeDesc::VEC2);
            spec.attribute(oname, v2, &(v2fattr->value()));
        } else if (type == "v2i"
                   && (v2iattr = header->findTypedAttribute<Imf::V2iAttribute>(
                           name))) {
            TypeDesc v2(TypeDesc::INT, TypeDesc::VEC2);
            spec.attribute(oname, v2, &(v2iattr->value()));
        } else if (type == "stringvector"
                   && (svattr
                       = header->findTypedAttribute<Imf::StringVectorAttribute>(
                           name))) {
            std::vector<std::string> strvec = svattr->value();
            std::vector<ustring> ustrvec(strvec.size());
            for (size_t i = 0, e = strvec.size(); i < e; ++i)
                ustrvec[i] = strvec[i];
            TypeDesc sv(TypeDesc::STRING, ustrvec.size());
            spec.attribute(oname, sv, &ustrvec[0]);
#if OPENEXR_HAS_FLOATVECTOR

        } else if (type == "floatvector"
                   && (fvattr
                       = header->findTypedAttribute<Imf::FloatVectorAttribute>(
                           name))) {
            std::vector<float> fvec = fvattr->value();
            TypeDesc fv(TypeDesc::FLOAT, fvec.size());
            spec.attribute(oname, fv, &fvec[0]);
#endif
        } else if (type == "double"
                   && (dattr = header->findTypedAttribute<Imf::DoubleAttribute>(
                           name))) {
            TypeDesc d(TypeDesc::DOUBLE);
            spec.attribute(oname, d, &(dattr->value()));
        } else if (type == "v2d"
                   && (v2dattr = header->findTypedAttribute<Imf::V2dAttribute>(
                           name))) {
            TypeDesc v2(TypeDesc::DOUBLE, TypeDesc::VEC2);
            spec.attribute(oname, v2, &(v2dattr->value()));
        } else if (type == "v3d"
                   && (v3dattr = header->findTypedAttribute<Imf::V3dAttribute>(
                           name))) {
            TypeDesc v3(TypeDesc::DOUBLE, TypeDesc::VEC3, TypeDesc::VECTOR);
            spec.attribute(oname, v3, &(v3dattr->value()));
        } else if (type == "m33d"
                   && (m33dattr = header->findTypedAttribute<Imf::M33dAttribute>(
                           name))) {
            TypeDesc m33(TypeDesc::DOUBLE, TypeDesc::MATRIX33);
            spec.attribute(oname, m33, &(m33dattr->value()));
        } else if (type == "m44d"
                   && (m44dattr = header->findTypedAttribute<Imf::M44dAttribute>(
                           name))) {
            TypeDesc m44(TypeDesc::DOUBLE, TypeDesc::MATRIX44);
            spec.attribute(oname, m44, &(m44dattr->value()));
        } else if (type == "box2i"
                   && (b2iattr = header->findTypedAttribute<Imf::Box2iAttribute>(
                           name))) {
            TypeDesc bx(TypeDesc::INT, TypeDesc::VEC2, 2);
            spec.attribute(oname, bx, &b2iattr->value());
        } else if (type == "box2f"
                   && (b2fattr = header->findTypedAttribute<Imf::Box2fAttribute>(
                           name))) {
            TypeDesc bx(TypeDesc::FLOAT, TypeDesc::VEC2, 2);
            spec.attribute(oname, bx, &b2fattr->value());
        } else if (type == "timecode"
                   && (tattr
                       = header->findTypedAttribute<Imf::TimeCodeAttribute>(
                           name))) {
            unsigned int timecode[2];
            timecode[0] = tattr->value().timeAndFlags(
                Imf::TimeCode::TV60_PACKING);  //TV60 returns unchanged _time
            timecode[1] = tattr->value().userData();

            // Elevate "timeCode" to smpte:TimeCode
            if (oname == "timeCode")
                oname = "smpte:TimeCode";
            spec.attribute(oname, TypeTimeCode, timecode);
        } else if (type == "keycode"
                   && (kcattr
                       = header->findTypedAttribute<Imf::KeyCodeAttribute>(
                           name))) {
            const Imf::KeyCode* k = &kcattr->value();
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
        } else if (type == "chromaticities"
                   && (crattr = header->findTypedAttribute<
                                Imf::ChromaticitiesAttribute>(name))) {
            const Imf::Chromaticities* chroma = &crattr->value();
            spec.attribute(oname, TypeDesc(TypeDesc::FLOAT, 8),
                           (const float*)chroma);
        } else if (type == "rational"
                   && (rattr
                       = header->findTypedAttribute<Imf::RationalAttribute>(
                           name))) {
            const Imf::Rational* rational = &rattr->value();
            int n                         = rational->n;
            unsigned int d                = rational->d;
            if (d < (1UL << 31)) {
                int r[2];
                r[0] = n;
                r[1] = static_cast<int>(d);
                spec.attribute(oname, TypeRational, r);
            } else {
                int f = static_cast<int>(std::gcd(int64_t(n), int64_t(d)));
                if (f > 1) {
                    int r[2];
                    r[0] = n / f;
                    r[1] = static_cast<int>(d / f);
                    spec.attribute(oname, TypeRational, r);
                } else {
                    // TODO: find a way to allow the client to accept "close" rational values
                    OIIO::debugfmt(
                        "Don't know what to do with OpenEXR Rational attribute {} with value {} / {} that we cannot represent exactly",
                        oname, n, d);
                }
            }
        } else if (type == "lineOrder"
                   && (lattr
                       = header->findTypedAttribute<Imf::LineOrderAttribute>(
                           name))) {
            const char* lineOrder = "increasingY";
            switch (lattr->value()) {
            case Imf::INCREASING_Y: lineOrder = "increasingY"; break;
            case Imf::DECREASING_Y: lineOrder = "decreasingY"; break;
            case Imf::RANDOM_Y: lineOrder = "randomY"; break;
            default: break;
            }
            spec.attribute("openexr:lineOrder", lineOrder);
        } else {
#if 0
            print(std::cerr, "  unknown attribute '{}' name '{}'\n",
                  type, name);
#endif
        }
    }

    float aspect   = spec.get_float_attribute("PixelAspectRatio", 0.0f);
    float xdensity = spec.get_float_attribute("XResolution", 0.0f);
    if (xdensity) {
        // If XResolution is found, supply the YResolution and unit.
        spec.attribute("YResolution", xdensity * (aspect ? aspect : 1.0f));
        spec.attribute("ResolutionUnit", "in");  // EXR is always pixels/inch
    }

    // EXR "name" also gets passed along as "oiio:subimagename".
    if (header->hasName())
        spec.attribute("oiio:subimagename", header->name());

    spec.attribute("oiio:subimages", in->m_nsubimages);

    // Squash some problematic texture metadata if we suspect it's wrong
    pvt::check_texture_metadata_sanity(spec);

    initialized = true;
    return ok;
}



namespace {


static TypeDesc
TypeDesc_from_ImfPixelType(Imf::PixelType ptype)
{
    switch (ptype) {
    case Imf::UINT: return TypeDesc::UINT; break;
    case Imf::HALF: return TypeDesc::HALF; break;
    case Imf::FLOAT: return TypeDesc::FLOAT; break;
    default:
        OIIO_ASSERT_MSG(0, "Unknown Imf::PixelType %d", int(ptype));
        return TypeUnknown;
    }
}



// Used to hold channel information for sorting into canonical order
struct ChanNameHolder {
    string_view fullname;    // layer.suffix
    string_view layer;       // just layer
    string_view suffix;      // just suffix (or the fillname, if no layer)
    int exr_channel_number;  // channel index in the exr (sorted by name)
    int special_index;       // sort order for special reserved names
    Imf::PixelType exr_data_type;
    TypeDesc datatype;
    int xSampling;
    int ySampling;

    ChanNameHolder(string_view fullname, int n, const Imf::Channel& exrchan)
        : fullname(fullname)
        , exr_channel_number(n)
        , special_index(10000)
        , exr_data_type(exrchan.type)
        , datatype(TypeDesc_from_ImfPixelType(exrchan.type))
        , xSampling(exrchan.xSampling)
        , ySampling(exrchan.ySampling)
    {
        pvt::split_name(fullname, layer, suffix);
    }

    // Compute canoninical channel list sort priority
    void compute_special_index()
    {
        static const char* special[]
            = { "R",    "Red",  "G",  "Green", "B",     "Blue",  "Y",
                "real", "imag", "A",  "Alpha", "AR",    "RA",    "AG",
                "GA",   "AB",   "BA", "Z",     "Depth", "Zback", nullptr };
        for (int i = 0; special[i]; ++i)
            if (Strutil::iequals(suffix, special[i])) {
                special_index = i;
                return;
            }
    }

    // Compute alternate channel sort priority for layers that contain
    // x,y,z.
    void compute_special_index_xyz()
    {
        static const char* special[]
            = { "R",  "Red", "G",  "Green", "B",    "Blue", /* "Y", */
                "X",  "Y",   "Z",  "real",  "imag", "A",     "Alpha", "AR",
                "RA", "AG",  "GA", "AB",    "BA",   "Depth", "Zback", nullptr };
        for (int i = 0; special[i]; ++i)
            if (Strutil::iequals(suffix, special[i])) {
                special_index = i;
                return;
            }
    }

    // Partial sort on layer only
    static bool compare_layer(const ChanNameHolder& a, const ChanNameHolder& b)
    {
        return (a.layer < b.layer);
    }

    // Full sort on layer name, special index, suffix
    static bool compare_cnh(const ChanNameHolder& a, const ChanNameHolder& b)
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


// Is the channel name (suffix only) in the list?
static bool
suffixfound(string_view name, span<ChanNameHolder> chans)
{
    for (auto& c : chans)
        if (Strutil::iequals(name, c.suffix))
            return true;
    return false;
}


// Returns the index of that channel name (suffix only) in the list, or -1 in case of failure.
static int
get_index_of_suffix(string_view name, span<ChanNameHolder> chans)
{
    for (size_t i = 0, n = chans.size(); i < n; ++i)
        if (Strutil::iequals(name, chans[i].suffix))
            return static_cast<int>(i);
    return -1;
}


// Is this a luminance-chroma image, i.e., Y/BY/RY or Y/BY/RY/A or Y/BY/RY/Alpha?
//
// Note that extra channels are not supported.
static bool
is_luminance_chroma(span<ChanNameHolder> chans)
{
    if (chans.size() < 3 || chans.size() > 4)
        return false;
    if (!suffixfound("Y", chans))
        return false;
    if (!suffixfound("BY", chans))
        return false;
    if (!suffixfound("RY", chans))
        return false;
    if (chans.size() == 4 && !suffixfound("A", chans)
        && !suffixfound("Alpha", chans))
        return false;
    return true;
}


}  // namespace



bool
OpenEXRInput::PartInfo::query_channels(OpenEXRInput* in,
                                       const Imf::Header* header)
{
    OIIO_DASSERT(!initialized);
    bool ok = true;
    const Imf::ChannelList& channels(header->channels());
    std::vector<ChanNameHolder> cnh;
    int c = 0;
    for (auto ci = channels.begin(); ci != channels.end(); ++c, ++ci)
        cnh.emplace_back(ci.name(), c, ci.channel());
    spec.nchannels = int(cnh.size());
    if (!spec.nchannels) {
        in->errorfmt("No channels found");
        return false;
    }

    // First, do a partial sort by layername. EXR should already be in that
    // order, but take no chances.
    std::sort(cnh.begin(), cnh.end(), ChanNameHolder::compare_layer);

    // Now, within each layer, sort by channel name
    for (auto layerbegin = cnh.begin(); layerbegin != cnh.end();) {
        // Identify the subrange that comprises a layer
        auto layerend = layerbegin + 1;
        while (layerend != cnh.end() && layerbegin->layer == layerend->layer)
            ++layerend;

        span<ChanNameHolder> layerspan(&(*layerbegin), layerend - layerbegin);
        // Strutil::printf("layerspan:\n");
        // for (auto& c : layerspan)
        //     Strutil::print("  {} = {} . {}\n", c.fullname, c.layer, c.suffix);
        if (suffixfound("X", layerspan)
            && (suffixfound("Y", layerspan) || suffixfound("Z", layerspan))) {
            // If "X", and at least one of "Y" and "Z", are found among the
            // channel names of this layer, it must encode some kind of
            // position or normal. The usual sort order will give a weird
            // result. Choose a different sort order to reflect this.
            for (auto& ch : layerspan)
                ch.compute_special_index_xyz();
        } else {
            // Use the usual sort order.
            for (auto& ch : layerspan)
                ch.compute_special_index();
        }
        std::sort(layerbegin, layerend, ChanNameHolder::compare_cnh);

        layerbegin = layerend;  // next set of layers
    }

    // Now we should have cnh sorted into the order that we want to present
    // to the OIIO client.

    // Limitations for luminance-chroma images: no tiling, no deep samples, no
    // miplevels/subimages, no extra channels.
    luminance_chroma = is_luminance_chroma(cnh);
    if (luminance_chroma) {
        spec.attribute("openexr:luminancechroma", 1);
        spec.format    = TypeDesc::HALF;
        spec.nchannels = cnh.size();
        if (spec.nchannels == 3) {
            spec.channelnames  = { "R", "G", "B" };
            spec.alpha_channel = -1;
            spec.z_channel     = -1;
        } else {
            OIIO_ASSERT(spec.nchannels == 4);
            int index_a = get_index_of_suffix("A", cnh);
            if (index_a != -1) {
                spec.channelnames  = { "R", "G", "B", "A" };
                spec.alpha_channel = index_a;
            } else {
                spec.channelnames  = { "R", "G", "B", "Alpha" };
                spec.alpha_channel = get_index_of_suffix("Alpha", cnh);
                OIIO_ASSERT(spec.alpha_channel != -1);
            }
            spec.z_channel = -1;
        }
        spec.channelformats.clear();
        return true;
    }

    spec.format         = TypeDesc::UNKNOWN;
    bool all_one_format = true;
    for (int c = 0; c < spec.nchannels; ++c) {
        spec.channelnames.push_back(cnh[c].fullname);
        spec.channelformats.push_back(cnh[c].datatype);
        spec.format = TypeDesc::basetype_merge(spec.format, cnh[c].datatype);
        pixeltype.push_back(cnh[c].exr_data_type);
        chanbytes.push_back(cnh[c].datatype.size());
        all_one_format &= (cnh[c].datatype == cnh[0].datatype);
        if (spec.alpha_channel < 0
            && (Strutil::iequals(cnh[c].suffix, "A")
                || Strutil::iequals(cnh[c].suffix, "Alpha")))
            spec.alpha_channel = c;
        if (spec.z_channel < 0
            && (Strutil::iequals(cnh[c].suffix, "Z")
                || Strutil::iequals(cnh[c].suffix, "Depth")))
            spec.z_channel = c;
        if (cnh[c].xSampling != 1 || cnh[c].ySampling != 1) {
            ok = false;
            in->errorfmt(
                "Subsampled channels are not supported (channel \"{}\" has sampling {},{}).",
                cnh[c].fullname, cnh[c].xSampling, cnh[c].ySampling);
            // FIXME: Some day, we should handle channel subsampling (beyond the luminance chroma
            // special case, possibly replacing it).
        }
    }
    OIIO_DASSERT((int)spec.channelnames.size() == spec.nchannels);
    OIIO_DASSERT(spec.format != TypeDesc::UNKNOWN);
    if (all_one_format)
        spec.channelformats.clear();
    return ok;
}



void
OpenEXRInput::PartInfo::compute_mipres(int miplevel, ImageSpec& spec) const
{
    // Compute the resolution of the requested mip level, and also adjust
    // the "full" size appropriately (based on the exr display window).

    if (levelmode == Imf::ONE_LEVEL)
        return;  // spec is already correct

    int w = topwidth;
    int h = topheight;
    if (levelmode == Imf::MIPMAP_LEVELS) {
        for (int m = miplevel; m; --m) {
            if (roundingmode == Imf::ROUND_DOWN) {
                w = w / 2;
                h = h / 2;
            } else {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
            }
            w = std::max(1, w);
            h = std::max(1, h);
        }
    } else if (levelmode == Imf::RIPMAP_LEVELS) {
        // FIXME
    } else {
        OIIO_ASSERT_MSG(0, "Unknown levelmode %d", int(levelmode));
    }

    spec.width  = w;
    spec.height = h;
    // N.B. OpenEXR doesn't support data and display windows per MIPmap
    // level.  So always take from the top level.
    Imath::Box2i datawindow    = top_datawindow;
    Imath::Box2i displaywindow = top_displaywindow;
    spec.x                     = datawindow.min.x;
    spec.y                     = datawindow.min.y;
    if (miplevel == 0) {
        spec.full_x      = displaywindow.min.x;
        spec.full_y      = displaywindow.min.y;
        spec.full_width  = displaywindow.max.x - displaywindow.min.x + 1;
        spec.full_height = displaywindow.max.y - displaywindow.min.y + 1;
    } else {
        spec.full_x      = spec.x;
        spec.full_y      = spec.y;
        spec.full_width  = spec.width;
        spec.full_height = spec.height;
    }
    if (cubeface) {
        spec.full_width  = w;
        spec.full_height = w;
    }
}



bool
OpenEXRInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage < 0 || subimage >= m_nsubimages)  // out of range
        return false;

    if (subimage == m_subimage && miplevel == m_miplevel) {  // no change
        return true;
    }

    PartInfo& part(m_parts[subimage]);
    if (!part.initialized) {
        const Imf::Header* header = NULL;
        if (m_input_multipart)
            header = &(m_input_multipart->header(subimage));
        if (!part.parse_header(this, header))
            return false;
        part.initialized = true;
    }

    if (subimage != m_subimage) {
        delete m_scanline_input_part;
        m_scanline_input_part = NULL;
        delete m_tiled_input_part;
        m_tiled_input_part = NULL;
        delete m_deep_scanline_input_part;
        m_deep_scanline_input_part = NULL;
        delete m_deep_tiled_input_part;
        m_deep_tiled_input_part = NULL;
        delete m_input_rgba;
        m_input_rgba = NULL;
        try {
            if (part.luminance_chroma) {
                if (subimage != 0 || miplevel != 0) {
                    errorfmt(
                        "Non-zero subimage or miplevel are not supported for luminance-chroma images.");
                    return false;
                }
                m_input_stream->seekg(0);
                m_input_rgba = new Imf::RgbaInputFile(*m_input_stream);
            } else if (part.spec.deep) {
                if (part.spec.tile_width)
                    m_deep_tiled_input_part
                        = new Imf::DeepTiledInputPart(*m_input_multipart,
                                                      subimage);
                else
                    m_deep_scanline_input_part
                        = new Imf::DeepScanLineInputPart(*m_input_multipart,
                                                         subimage);
            } else {
                if (part.spec.tile_width)
                    m_tiled_input_part
                        = new Imf::TiledInputPart(*m_input_multipart, subimage);
                else
                    m_scanline_input_part
                        = new Imf::InputPart(*m_input_multipart, subimage);
            }
        } catch (const std::exception& e) {
            errorfmt("OpenEXR exception: {}", e.what());
            m_scanline_input_part      = NULL;
            m_tiled_input_part         = NULL;
            m_deep_scanline_input_part = NULL;
            m_deep_tiled_input_part    = NULL;
            m_input_rgba               = NULL;
            return false;
        } catch (...) {  // catch-all for edge cases or compiler bugs
            errorfmt("OpenEXR exception: unknown");
            m_scanline_input_part      = NULL;
            m_tiled_input_part         = NULL;
            m_deep_scanline_input_part = NULL;
            m_deep_tiled_input_part    = NULL;
            m_input_rgba               = NULL;
            return false;
        }
    }

    m_subimage = subimage;

    if (miplevel < 0 || miplevel >= part.nmiplevels)  // out of range
        return false;

    m_miplevel = miplevel;
    m_spec     = part.spec;

    if (!check_open(m_spec, { 0, 1 << 20, 0, 1 << 20, 0, 1 << 16, 0, 1 << 12 }))
        return false;

    if (miplevel == 0 && part.levelmode == Imf::ONE_LEVEL) {
        return true;
    }

    // Compute the resolution of the requested mip level and adjust the
    // full size fields.
    part.compute_mipres(miplevel, m_spec);

    return true;
}



ImageSpec
OpenEXRInput::spec(int subimage, int miplevel)
{
    ImageSpec ret;
    if (subimage < 0 || subimage >= m_nsubimages)
        return ret;  // invalid
    const PartInfo& part(m_parts[subimage]);
    if (!part.initialized) {
        // Only if this subimage hasn't yet been inventoried do we need
        // to lock and seek.
        lock_guard lock(*this);
        if (!part.initialized) {
            if (!seek_subimage(subimage, miplevel))
                return ret;
        }
    }
    if (miplevel < 0 || miplevel >= part.nmiplevels)
        return ret;  // invalid
    ret = part.spec;
    part.compute_mipres(miplevel, ret);
    return ret;
}



ImageSpec
OpenEXRInput::spec_dimensions(int subimage, int miplevel)
{
    ImageSpec ret;
    if (subimage < 0 || subimage >= m_nsubimages)
        return ret;  // invalid
    const PartInfo& part(m_parts[subimage]);
    if (!part.initialized) {
        // Only if this subimage hasn't yet been inventoried do we need
        // to lock and seek.
        lock_guard lock(*this);
        if (!seek_subimage(subimage, miplevel))
            return ret;
    }
    if (miplevel < 0 || miplevel >= part.nmiplevels)
        return ret;  // invalid
    ret.copy_dimensions(part.spec);
    part.compute_mipres(miplevel, ret);
    return ret;
}



bool
OpenEXRInput::close()
{
    delete m_input_multipart;
    delete m_scanline_input_part;
    delete m_tiled_input_part;
    delete m_deep_scanline_input_part;
    delete m_deep_tiled_input_part;
    delete m_input_rgba;
    delete m_input_stream;
    init();  // Reset to initial state
    return true;
}



bool
OpenEXRInput::read_native_scanline(int subimage, int miplevel, int y, int z,
                                   void* data)
{
    return read_native_scanlines(subimage, miplevel, y, y + 1, z, 0,
                                 m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                    int yend, int z, void* data)
{
    return read_native_scanlines(subimage, miplevel, ybegin, yend, z, 0,
                                 m_spec.nchannels, data);
}


bool
OpenEXRInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                    int yend, int /*z*/, int chbegin, int chend,
                                    void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    chend = clamp(chend, chbegin + 1, m_spec.nchannels);
    DBGEXR("openexr rns {}-{}, channels {}-{}", ybegin, yend, chbegin,
           chend - 1);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    const PartInfo& part(m_parts[m_subimage]);
    size_t pixelbytes    = m_spec.pixel_bytes(chbegin, chend, true);
    size_t scanlinebytes = (size_t)m_spec.width * pixelbytes;
    char* buf            = (char*)data - m_spec.x * stride_t(pixelbytes)
                - ybegin * stride_t(scanlinebytes);

    try {
        if (part.luminance_chroma) {
            Imath::Box2i dw = m_input_rgba->dataWindow();
            if (dw.min.x != 0 || dw.min.y != 0
                || dw != m_input_rgba->displayWindow()) {
                errorfmt(
                    "Non-trivial data and/or display windows are not supported for luminance-chroma images.");
                return false;
            }
            int dw_width     = dw.max.x - dw.min.x + 1;
            int dw_height    = dw.max.y - dw.min.y + 1;
            int chunk_height = yend - ybegin;
            // FIXME Are these assumptions correct?
            OIIO_ASSERT(ybegin >= dw.min.y);
            OIIO_ASSERT(yend <= dw.max.y + 1);
            OIIO_ASSERT(chunk_height <= dw_height);

            Imf::Array2D<Imf::Rgba> pixels(chunk_height, dw_width);
            m_input_rgba->setFrameBuffer(&pixels[0][0] - dw.min.x
                                             - ybegin * dw_width,
                                         1, dw_width);
            m_input_rgba->readPixels(ybegin, yend - 1);

            // FIXME There is probably some optimized code for this somewhere.
            for (int c = chbegin; c < chend; ++c) {
                size_t chanbytes = m_spec.channelformat(c).size();
                half* src        = &pixels[0][0].r + c;
                half* dst        = (half*)((char*)data + c * chanbytes);
                for (int y = ybegin; y < yend; ++y) {
                    for (int x = 0; x < m_spec.width; ++x) {
                        *dst = *src;
                        src += 4;  // always advance 4 RGBA halfs
                        dst += m_spec.nchannels;
                    }
                }
            }

            return true;
        }

        if (!m_scanline_input_part) {
            errorfmt(
                "called OpenEXRInput::read_native_scanlines without an open file");
            return false;
        }

        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = chbegin; c < chend; ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert(m_spec.channelnames[c].c_str(),
                               Imf::Slice(part.pixeltype[c], buf + chanoffset,
                                          pixelbytes, scanlinebytes));
            chanoffset += chanbytes;
        }
        if (m_scanline_input_part) {
            m_scanline_input_part->setFrameBuffer(frameBuffer);
            m_scanline_input_part->readPixels(ybegin, yend - 1);
        } else {
            errorfmt("Attempted to read scanline from a non-scanline file.");
            return false;
        }
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR read: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR read: unknown exception");
        return false;
    }
    return true;
}



bool
OpenEXRInput::read_native_tile(int subimage, int miplevel, int x, int y, int z,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    return read_native_tiles(subimage, miplevel, x, x + m_spec.tile_width, y,
                             y + m_spec.tile_height, z, z + m_spec.tile_depth,
                             0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_tiles(int subimage, int miplevel, int xbegin,
                                int xend, int ybegin, int yend, int zbegin,
                                int zend, void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    return read_native_tiles(subimage, miplevel, xbegin, xend, ybegin, yend,
                             zbegin, zend, 0, m_spec.nchannels, data);
}



bool
OpenEXRInput::read_native_tiles(int subimage, int miplevel, int xbegin,
                                int xend, int ybegin, int yend, int zbegin,
                                int zend, int chbegin, int chend, void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    chend = clamp(chend, chbegin + 1, m_spec.nchannels);
    const PartInfo& part(m_parts[m_subimage]);
    if (part.luminance_chroma) {
        errorfmt(
            "OpenEXRInput::read_native_tiles is not supported for luminance-chroma images");
        return false;
    }
    DBGEXR("openexr rnt {} {}-{}, chans {}-{}", xbegin, xend, ybegin, yend,
           chend - 1);
    if (!m_tiled_input_part
        || !m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend)) {
        errorfmt("called OpenEXRInput::read_native_tiles without an open file");
        return false;
    }

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    size_t pixelbytes = m_spec.pixel_bytes(chbegin, chend, true);
    int firstxtile    = (xbegin - m_spec.x) / m_spec.tile_width;
    int firstytile    = (ybegin - m_spec.y) / m_spec.tile_height;
    // clamp to the image edge
    xend = std::min(xend, m_spec.x + m_spec.width);
    yend = std::min(yend, m_spec.y + m_spec.height);
    zend = std::min(zend, m_spec.z + m_spec.depth);
    // figure out how many tiles we need
    int nxtiles = (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width;
    int nytiles = (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height;
    int whole_width  = nxtiles * m_spec.tile_width;
    int whole_height = nytiles * m_spec.tile_height;

    std::unique_ptr<char[]> tmpbuf;
    void* origdata = data;
    if (whole_width != (xend - xbegin) || whole_height != (yend - ybegin)) {
        // Deal with the case of reading not a whole number of tiles --
        // OpenEXR will happily overwrite user memory in this case.
        tmpbuf.reset(new char[nxtiles * nytiles * m_spec.tile_bytes(true)]);
        data = &tmpbuf[0];
    }
    char* buf = (char*)data - xbegin * pixelbytes
                - ybegin * pixelbytes * m_spec.tile_width * nxtiles;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = chbegin; c < chend; ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert(m_spec.channelnames[c].c_str(),
                               Imf::Slice(part.pixeltype[c], buf + chanoffset,
                                          pixelbytes,
                                          pixelbytes * m_spec.tile_width
                                              * nxtiles));
            chanoffset += chanbytes;
        }
        if (m_tiled_input_part) {
            m_tiled_input_part->setFrameBuffer(frameBuffer);
            m_tiled_input_part->readTiles(firstxtile, firstxtile + nxtiles - 1,
                                          firstytile, firstytile + nytiles - 1,
                                          m_miplevel, m_miplevel);
        } else {
            errorfmt("Attempted to read tiles from a non-tiled file");
            return false;
        }
        if (data != origdata) {
            stride_t user_scanline_bytes = (xend - xbegin) * pixelbytes;
            stride_t scanline_stride = nxtiles * m_spec.tile_width * pixelbytes;
            for (int y = ybegin; y < yend; ++y)
                memcpy((char*)origdata + (y - ybegin) * scanline_stride,
                       (char*)data + (y - ybegin) * scanline_stride,
                       user_scanline_bytes);
        }
    } catch (const std::exception& e) {
        std::string err = e.what();
        if (m_missingcolor.size()) {
            // User said not to fail for bad or missing tiles. If we failed
            // reading a single tile, use the fill pattern. If we failed
            // reading many tiles, we don't know which ones, so go back and
            // read them individually for a second chance.
            stride_t xstride = pixelbytes;
            stride_t ystride = xstride * (xend - xbegin);
            if (nxtiles * nytiles == 1) {
                // Read of one tile -- use the fill pattern
                fill_missing(xbegin, xend, ybegin, yend, zbegin, zend, chbegin,
                             chend, data, xstride, ystride);
            } else {
                // Read of many tiles -- don't know which failed, so try
                // again to read them all individually.
                return read_native_tiles_individually(subimage, miplevel,
                                                      xbegin, xend, ybegin,
                                                      yend, zbegin, zend,
                                                      chbegin, chend, data,
                                                      xstride, ystride);
            }
        } else {
            errorfmt("Failed OpenEXR read: {}", err);
            return false;
        }
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXRInput::read_native_tiles_individually(int subimage, int miplevel,
                                             int xbegin, int xend, int ybegin,
                                             int yend, int zbegin, int zend,
                                             int chbegin, int chend, void* data,
                                             stride_t xstride, stride_t ystride)
{
    // Note: this is only called by read_tiles, which still holds the
    // mutex, so it's safe to directly access m_spec.
    bool ok = true;
    for (int y = ybegin; y < yend; y += m_spec.tile_height) {
        // int ye = std::min(y + m_spec.tile_height, m_spec.y + m_spec.height);
        int ye = y + m_spec.tile_height;
        for (int x = xbegin; x < xend; x += m_spec.tile_width) {
            // int xe = std::min(x + m_spec.tile_width, m_spec.x + m_spec.width);
            int xe  = x + m_spec.tile_width;
            char* d = (char*)data + (y - ybegin) * ystride
                      + (x - xbegin) * xstride;
            ok &= read_tiles(subimage, miplevel, x, xe, y, ye, zbegin, zend,
                             chbegin, chend, TypeUnknown, d, xstride, ystride);
        }
    }
    return ok;
}



void
OpenEXRInput::fill_missing(int xbegin, int xend, int ybegin, int yend,
                           int /*zbegin*/, int /*zend*/, int chbegin, int chend,
                           void* data, stride_t xstride, stride_t ystride)
{
    std::vector<float> missingcolor = m_missingcolor;
    missingcolor.resize(chend, m_missingcolor.back());
    bool stripe = missingcolor[0] < 0.0f;
    if (stripe)
        missingcolor[0] = fabsf(missingcolor[0]);
    for (int y = ybegin; y < yend; ++y) {
        for (int x = xbegin; x < xend; ++x) {
            char* d = (char*)data + (y - ybegin) * ystride
                      + (x - xbegin) * xstride;
            for (int ch = chbegin; ch < chend; ++ch) {
                float v = missingcolor[ch];
                if (stripe && ((x - y) & 8))
                    v = 0.0f;
                TypeDesc cf = m_spec.channelformat(ch);
                if (cf == TypeFloat)
                    *(float*)d = v;
                else if (cf == TypeHalf)
                    *(half*)d = v;
                d += cf.size();
            }
        }
    }
}



bool
OpenEXRInput::read_native_deep_scanlines(int subimage, int miplevel, int ybegin,
                                         int yend, int /*z*/, int chbegin,
                                         int chend, DeepData& deepdata)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    const PartInfo& part(m_parts[m_subimage]);
    if (part.luminance_chroma) {
        errorfmt(
            "OpenEXRInput::read_native_deep_scanlines is not supported for luminance-chroma images");
        return false;
    }
    if (m_deep_scanline_input_part == NULL) {
        errorfmt(
            "called OpenEXRInput::read_native_deep_scanlines without an open file");
        return false;
    }

    try {
        size_t npixels = (yend - ybegin) * m_spec.width;
        chend          = clamp(chend, chbegin + 1, m_spec.nchannels);
        size_t nchans  = chend - chbegin;

        // Set up the count and pointers arrays and the Imf framebuffer
        std::vector<TypeDesc> channeltypes;
        m_spec.get_channelformats(channeltypes);
        deepdata.init(npixels, nchans,
                      cspan<TypeDesc>(&channeltypes[chbegin], nchans),
                      m_spec.channelnames);
        std::vector<unsigned int> all_samples(npixels);
        std::vector<void*> pointerbuf(npixels * nchans);
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice(Imf::UINT,
                              (char*)(&all_samples[0] - m_spec.x
                                      - ybegin * m_spec.width),
                              sizeof(unsigned int),
                              sizeof(unsigned int) * m_spec.width);
        frameBuffer.insertSampleCountSlice(countslice);
        size_t slchans      = m_spec.width * nchans;
        size_t xstride      = sizeof(void*) * nchans;
        size_t ystride      = sizeof(void*) * slchans;
        size_t samplestride = deepdata.samplesize();

        for (int c = chbegin; c < chend; ++c) {
            Imf::DeepSlice slice(part.pixeltype[c],
                                 (char*)(&pointerbuf[0] + (c - chbegin)
                                         - m_spec.x * nchans
                                         - ybegin * slchans),
                                 xstride, ystride, samplestride);
            frameBuffer.insert(m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_scanline_input_part->setFrameBuffer(frameBuffer);

        // Get the sample counts for each pixel and compute the total
        // number of samples and resize the data area appropriately.
        m_deep_scanline_input_part->readPixelSampleCounts(ybegin, yend - 1);
        deepdata.set_all_samples(all_samples);
        deepdata.get_pointers(pointerbuf);

        // Read the pixels
        m_deep_scanline_input_part->readPixels(ybegin, yend - 1);
        // deepdata.import_chansamp (pointerbuf);
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR read: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXRInput::read_native_deep_tiles(int subimage, int miplevel, int xbegin,
                                     int xend, int ybegin, int yend,
                                     int /*zbegin*/, int /*zend*/, int chbegin,
                                     int chend, DeepData& deepdata)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    const PartInfo& part(m_parts[m_subimage]);
    if (part.luminance_chroma) {
        errorfmt(
            "OpenEXRInput::read_native_deep_tiles is not supported for luminance-chroma images");
        return false;
    }
    if (m_deep_tiled_input_part == NULL) {
        errorfmt(
            "called OpenEXRInput::read_native_deep_tiles without an open file");
        return false;
    }

    try {
        size_t width   = xend - xbegin;
        size_t height  = yend - ybegin;
        size_t npixels = width * height;
        chend          = clamp(chend, chbegin + 1, m_spec.nchannels);
        size_t nchans  = chend - chbegin;

        // Set up the count and pointers arrays and the Imf framebuffer
        std::vector<TypeDesc> channeltypes;
        m_spec.get_channelformats(channeltypes);
        deepdata.init(npixels, nchans,
                      cspan<TypeDesc>(&channeltypes[chbegin], nchans),
                      m_spec.channelnames);
        std::vector<unsigned int> all_samples(npixels);
        std::vector<void*> pointerbuf(npixels * nchans);
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice(
            Imf::UINT, (char*)(&all_samples[0] - xbegin - ybegin * width),
            sizeof(unsigned int), sizeof(unsigned int) * width);
        frameBuffer.insertSampleCountSlice(countslice);
        size_t slchans      = width * nchans;
        size_t xstride      = sizeof(void*) * nchans;
        size_t ystride      = sizeof(void*) * slchans;
        size_t samplestride = deepdata.samplesize();
        for (int c = chbegin; c < chend; ++c) {
            Imf::DeepSlice slice(part.pixeltype[c],
                                 (char*)(&pointerbuf[0] + (c - chbegin)
                                         - xbegin * nchans - ybegin * slchans),
                                 xstride, ystride, samplestride);
            frameBuffer.insert(m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_tiled_input_part->setFrameBuffer(frameBuffer);

        int xtiles = round_to_multiple(width, m_spec.tile_width)
                     / m_spec.tile_width;
        int ytiles = round_to_multiple(height, m_spec.tile_height)
                     / m_spec.tile_height;

        int firstxtile = (xbegin - m_spec.x) / m_spec.tile_width;
        int firstytile = (ybegin - m_spec.y) / m_spec.tile_height;

        // Get the sample counts for each pixel and compute the total
        // number of samples and resize the data area appropriately.
        m_deep_tiled_input_part->readPixelSampleCounts(firstxtile,
                                                       firstxtile + xtiles - 1,
                                                       firstytile,
                                                       firstytile + ytiles - 1);
        deepdata.set_all_samples(all_samples);
        deepdata.get_pointers(pointerbuf);

        // Read the pixels
        m_deep_tiled_input_part->readTiles(firstxtile, firstxtile + xtiles - 1,
                                           firstytile, firstytile + ytiles - 1,
                                           m_miplevel, m_miplevel);
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR read: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR read: unknown exception");
        return false;
    }

    return true;
}


OIIO_PLUGIN_NAMESPACE_END
