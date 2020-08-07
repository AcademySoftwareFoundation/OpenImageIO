// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#include <boost/container/flat_map.hpp>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/tiffutils.h>

#include "exif.h"

OIIO_NAMESPACE_BEGIN

using namespace pvt;


class TagMap::Impl {
public:
    typedef boost::container::flat_map<int, const TagInfo*> tagmap_t;
    typedef boost::container::flat_map<std::string, const TagInfo*> namemap_t;
    // Name map is lower case so it's effectively case-insensitive

    Impl(string_view mapname, cspan<TagInfo> tag_table)
        : m_mapname(mapname)
    {
        for (const auto& tag : tag_table) {
            m_tagmap[tag.tifftag] = &tag;
            if (tag.name) {
                std::string lowername(tag.name);
                Strutil::to_lower(lowername);
                m_namemap[lowername] = &tag;
            }
        }
    }

    tagmap_t m_tagmap;
    namemap_t m_namemap;
    std::string m_mapname;
};



TagMap::TagMap(string_view mapname, cspan<TagInfo> tag_table)
    : m_impl(new Impl(mapname, tag_table))
{
}



TagMap::~TagMap() {}


const TagInfo*
TagMap::find(int tag) const
{
    auto i = m_impl->m_tagmap.find(tag);
    return i == m_impl->m_tagmap.end() ? NULL : i->second;
}


const TagInfo*
TagMap::find(string_view name) const
{
    std::string lowername(name);
    Strutil::to_lower(lowername);
    auto i = m_impl->m_namemap.find(lowername);
    return i == m_impl->m_namemap.end() ? NULL : i->second;
}


const char*
TagMap::name(int tag) const
{
    auto i = m_impl->m_tagmap.find(tag);
    return i == m_impl->m_tagmap.end() ? NULL : i->second->name;
}


TIFFDataType
TagMap::tifftype(int tag) const
{
    auto i = m_impl->m_tagmap.find(tag);
    return i == m_impl->m_tagmap.end() ? TIFF_NOTYPE : i->second->tifftype;
}


int
TagMap::tiffcount(int tag) const
{
    auto i = m_impl->m_tagmap.find(tag);
    return i == m_impl->m_tagmap.end() ? 0 : i->second->tiffcount;
}


int
TagMap::tag(string_view name) const
{
    std::string lowername(name);
    Strutil::to_lower(lowername);
    auto i = m_impl->m_namemap.find(lowername);
    return i == m_impl->m_namemap.end() ? -1 : i->second->tifftag;
}


string_view
TagMap::mapname() const
{
    return m_impl->m_mapname;
}



const TagInfo*
tag_lookup(string_view domain, int tag)
{
    const TagMap* tm = nullptr;
    if (domain == "Exif")
        tm = &exif_tagmap_ref();
    else if (domain == "GPS")
        tm = &gps_tagmap_ref();
    else
        tm = &tiff_tagmap_ref();
    return tm ? tm->find(tag) : nullptr;
}



const TagInfo*
tag_lookup(string_view domain, string_view tag)
{
    const TagMap* tm = nullptr;
    if (domain == "Exif")
        tm = &exif_tagmap_ref();
    else if (domain == "GPS")
        tm = &gps_tagmap_ref();
    else
        tm = &tiff_tagmap_ref();
    return tm ? tm->find(tag) : nullptr;
}



size_t
tiff_data_size(TIFFDataType tifftype)
{
    // Sizes of TIFFDataType members
    static size_t sizes[]    = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4 };
    const int num_data_sizes = sizeof(sizes) / sizeof(*sizes);
    int dir_index            = (int)tifftype;
    if (dir_index < 0 || dir_index >= num_data_sizes) {
        // Inform caller about corrupted entry.
        return -1;
    }
    return sizes[dir_index];
}



size_t
tiff_data_size(const TIFFDirEntry& dir)
{
    return tiff_data_size(TIFFDataType(dir.tdir_type)) * dir.tdir_count;
}



TypeDesc
tiff_datatype_to_typedesc(TIFFDataType tifftype, size_t tiffcount)
{
    if (tiffcount == 1)
        tiffcount = 0;  // length 1 == not an array
    switch (tifftype) {
    case TIFF_NOTYPE: return TypeUnknown;
    case TIFF_BYTE: return TypeDesc(TypeDesc::UINT8, tiffcount);
    case TIFF_ASCII: return TypeString;
    case TIFF_SHORT: return TypeDesc(TypeDesc::UINT16, tiffcount);
    case TIFF_LONG: return TypeDesc(TypeDesc::UINT32, tiffcount);
    case TIFF_RATIONAL:
        return TypeDesc(TypeDesc::INT32, TypeDesc::VEC2, TypeDesc::RATIONAL,
                        tiffcount);
    case TIFF_SBYTE: return TypeDesc(TypeDesc::INT8, tiffcount);
    case TIFF_UNDEFINED:
        return TypeDesc(TypeDesc::UINT8, tiffcount);  // 8-bit untyped data
    case TIFF_SSHORT: return TypeDesc(TypeDesc::INT16, tiffcount);
    case TIFF_SLONG: return TypeDesc(TypeDesc::INT32, tiffcount);
    case TIFF_SRATIONAL:
        return TypeDesc(TypeDesc::INT32, TypeDesc::VEC2, TypeDesc::RATIONAL,
                        tiffcount);
    case TIFF_FLOAT: return TypeDesc(TypeDesc::FLOAT, tiffcount);
    case TIFF_DOUBLE: return TypeDesc(TypeDesc::DOUBLE, tiffcount);
    case TIFF_IFD: return TypeUnknown;
#ifdef TIFF_VERSION_BIG
    case TIFF_LONG8: return TypeDesc(TypeDesc::UINT64, tiffcount);
    case TIFF_SLONG8: return TypeDesc(TypeDesc::INT64, tiffcount);
    case TIFF_IFD8: return TypeUnknown;
#endif
    }
    return TypeUnknown;
}



cspan<uint8_t>
tiff_dir_data(const TIFFDirEntry& td, cspan<uint8_t> data)
{
    size_t len = tiff_data_size(td);
    if (len <= 4) {
        // Short data are stored in the offset field itself
        return cspan<uint8_t>((const uint8_t*)&td.tdir_offset, len);
    }
    // Long data
    size_t begin = td.tdir_offset;
    if (begin + len > size_t(data.size())) {
        // Invalid span -- it is not entirely contained in the data window.
        // Signal error by returning an empty span.
        return cspan<uint8_t>();
    }
    return cspan<uint8_t>(data.data() + begin, len);
}



#if DEBUG_EXIF_READ || DEBUG_EXIF_WRITE
static bool
print_dir_entry(std::ostream& out, const TagMap& tagmap,
                const TIFFDirEntry& dir, cspan<uint8_t> buf,
                int offset_adjustment)
{
    int len = tiff_data_size(dir);
    if (len < 0) {
        out << "Ignoring bad directory entry\n";
        return false;
    }
    const char* mydata = (const char*)dataptr(dir, buf, offset_adjustment);
    if (!mydata)
        return false;  // bogus! overruns the buffer
    mydata += offset_adjustment;
    const char* name = tagmap.name(dir.tdir_tag);
    Strutil::fprintf(out,
                     "  Tag %d/0x%s (%s) type=%d (%s) count=%d offset=%d = ",
                     dir.tdir_tag, dir.tdir_tag, (name ? name : "unknown"),
                     dir.tdir_type, tiff_datatype_to_typedesc(dir),
                     dir.tdir_count, dir.tdir_offset);

    switch (dir.tdir_type) {
    case TIFF_ASCII:
        out << "'" << string_view(mydata, dir.tdir_count) << "'";
        break;
    case TIFF_RATIONAL: {
        const unsigned int* u = (unsigned int*)mydata;
        for (size_t i = 0; i < dir.tdir_count; ++i)
            out << u[2 * i] << "/" << u[2 * i + 1] << " = "
                << (double)u[2 * i] / (double)u[2 * i + 1] << " ";
    } break;
    case TIFF_SRATIONAL: {
        const int* u = (int*)mydata;
        for (size_t i = 0; i < dir.tdir_count; ++i)
            out << u[2 * i] << "/" << u[2 * i + 1] << " = "
                << (double)u[2 * i] / (double)u[2 * i + 1] << " ";
    } break;
    case TIFF_SHORT: out << ((unsigned short*)mydata)[0]; break;
    case TIFF_LONG: out << ((unsigned int*)mydata)[0]; break;
    case TIFF_BYTE:
    case TIFF_UNDEFINED:
    case TIFF_NOTYPE:
    default:
        if (len <= 4 && dir.tdir_count > 4) {
            // Request more data than is stored.
            out << "Ignoring buffer with too much count of short data.\n";
            return false;
        }
        for (size_t i = 0; i < dir.tdir_count; ++i)
            out << (int)((unsigned char*)mydata)[i] << ' ';
        break;
    }
    out << "\n";
    return true;
}



// debugging
static std::string
dumpdata(cspan<uint8_t> blob, cspan<size_t> ifdoffsets, size_t start,
         int offset_adjustment)
{
    std::stringstream out;
    for (size_t pos = 0; pos < blob.size(); ++pos) {
        bool at_ifd = (std::find(ifdoffsets.cbegin(), ifdoffsets.cend(), pos)
                       != ifdoffsets.end());
        if (pos == 0 || pos == start || at_ifd || (pos % 10) == 0) {
            out << "\n@" << pos << ": ";
            if (at_ifd) {
                uint16_t n = *(uint16_t*)&blob[pos];
                out << "\nNew IFD: " << n
                    << " tags:  [offset_adjustment=" << offset_adjustment
                    << "\n";
                TIFFDirEntry* td = (TIFFDirEntry*)&blob[pos + 2];
                for (int i = 0; i < n; ++i, ++td)
                    print_dir_entry(out, tiff_tagmap_ref(), *td, blob,
                                    offset_adjustment);
            }
        }
        unsigned char c = (unsigned char)blob[pos];
        if (c >= ' ' && c < 127)
            out << c << ' ';
        out << "(" << (int)c << ") ";
    }
    out << "\n";
    return out.str();
}
#endif



static void
version4char_handler(const TagInfo& taginfo, const TIFFDirEntry& dir,
                     cspan<uint8_t> buf, ImageSpec& spec,
                     bool /*swapendian*/ = false, int offset_adjustment = 0)
{
    const char* data = (const char*)dataptr(dir, buf, offset_adjustment);
    if (tiff_data_size(dir) == 4 && data != nullptr)  // sanity check
        spec.attribute(taginfo.name, std::string(data, data + 4));
}


static void
version4uint8_handler(const TagInfo& taginfo, const TIFFDirEntry& dir,
                      cspan<uint8_t> buf, ImageSpec& spec,
                      bool /*swapendian*/ = false, int offset_adjustment = 0)
{
    const char* data = (const char*)dataptr(dir, buf, offset_adjustment);
    if (tiff_data_size(dir) == 4 && data != nullptr)  // sanity check
        spec.attribute(taginfo.name, TypeDesc(TypeDesc::UINT8, 4),
                       (const char*)data);
}


static void
makernote_handler(const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                  cspan<uint8_t> buf, ImageSpec& spec, bool swapendian = false,
                  int offset_adjustment = 0)
{
    if (tiff_data_size(dir) <= 4)
        return;  // sanity check

    if (spec.get_string_attribute("Make") == "Canon") {
        std::vector<size_t> ifdoffsets { 0 };
        std::set<size_t> offsets_seen;
        decode_ifd((unsigned char*)buf.data() + dir.tdir_offset, buf, spec,
                   pvt::canon_maker_tagmap_ref(), offsets_seen, swapendian,
                   offset_adjustment);
    } else {
        // Maybe we just haven't parsed the Maker metadata yet?
        // Allow a second try later by just stashing the maker note offset.
        spec.attribute("oiio:MakerNoteOffset", int(dir.tdir_offset));
    }
}



static const TagInfo tiff_tag_table[] = {
    // clang-format off
    { TIFFTAG_IMAGEDESCRIPTION, "ImageDescription", TIFF_ASCII, 0 },
    { TIFFTAG_ORIENTATION,      "Orientation",      TIFF_SHORT, 1 },
    { TIFFTAG_XRESOLUTION,      "XResolution",      TIFF_RATIONAL, 1 },
    { TIFFTAG_YRESOLUTION,      "YResolution",      TIFF_RATIONAL, 1 },
    { TIFFTAG_RESOLUTIONUNIT,   "ResolutionUnit",   TIFF_SHORT, 1 },
    { TIFFTAG_MAKE,             "Make",             TIFF_ASCII, 0 },
    { TIFFTAG_MODEL,            "Model",            TIFF_ASCII, 0 },
    { TIFFTAG_SOFTWARE,         "Software",         TIFF_ASCII, 0 },
    { TIFFTAG_ARTIST,           "Artist",           TIFF_ASCII, 0 },
    { TIFFTAG_COPYRIGHT,        "Copyright",        TIFF_ASCII, 0 },
    { TIFFTAG_DATETIME,         "DateTime",         TIFF_ASCII, 0 },
    { TIFFTAG_DOCUMENTNAME,     "DocumentName",     TIFF_ASCII, 0 },
    { TIFFTAG_PAGENAME,         "tiff:PageName",    TIFF_ASCII, 0 },
    { TIFFTAG_PAGENUMBER,       "tiff:PageNumber",  TIFF_SHORT, 1 },
    { TIFFTAG_HOSTCOMPUTER,     "HostComputer",     TIFF_ASCII, 0 },
    { TIFFTAG_PIXAR_TEXTUREFORMAT, "textureformat", TIFF_ASCII, 0 },
    { TIFFTAG_PIXAR_WRAPMODES,  "wrapmodes",        TIFF_ASCII, 0 },
    { TIFFTAG_PIXAR_FOVCOT,     "fovcot",           TIFF_FLOAT, 1 },
    { TIFFTAG_JPEGQUALITY,      "CompressionQuality", TIFF_LONG, 1 },
    { TIFFTAG_ZIPQUALITY,       "tiff:zipquality",  TIFF_LONG, 1 },
    { TIFFTAG_XMLPACKET,        "tiff:XMLPacket",   TIFF_ASCII, 0 },
    // clang-format on
};

const TagMap&
pvt::tiff_tagmap_ref()
{
    static TagMap T("TIFF", tiff_tag_table);
    return T;
}



static const TagInfo exif_tag_table[] = {
    // clang-format off
    // Skip ones handled by the usual JPEG code
    // N.B. We use TIFF_NOTYPE to indicate an item that should be skipped.
    { TIFFTAG_IMAGEWIDTH,	"Exif:ImageWidth",	TIFF_NOTYPE, 1 },
    { TIFFTAG_IMAGELENGTH,	"Exif:ImageLength",	TIFF_NOTYPE, 1 },
    { TIFFTAG_BITSPERSAMPLE,	"Exif:BitsPerSample",	TIFF_NOTYPE, 0 },
    { TIFFTAG_COMPRESSION,	"Exif:Compression",	TIFF_NOTYPE, 1 },
    { TIFFTAG_PHOTOMETRIC,	"Exif:Photometric",	TIFF_NOTYPE, 1 },
    { TIFFTAG_SAMPLESPERPIXEL,	"Exif:SamplesPerPixel",	TIFF_NOTYPE, 1 },
    { TIFFTAG_PLANARCONFIG,	"Exif:PlanarConfig",	TIFF_NOTYPE, 1 },
    { TIFFTAG_YCBCRSUBSAMPLING,	"Exif:YCbCrSubsampling",TIFF_SHORT, 1 },
    { TIFFTAG_YCBCRPOSITIONING,	"Exif:YCbCrPositioning",TIFF_SHORT, 1 },
    // TIFF tags we may come across
    { TIFFTAG_ORIENTATION,	"Orientation",	TIFF_SHORT, 1 },
    { TIFFTAG_XRESOLUTION,	"XResolution",	TIFF_RATIONAL, 1 },
    { TIFFTAG_YRESOLUTION,	"YResolution",	TIFF_RATIONAL, 1 },
    { TIFFTAG_RESOLUTIONUNIT,	"ResolutionUnit",TIFF_SHORT, 1 },
    { TIFFTAG_IMAGEDESCRIPTION,	"ImageDescription",	TIFF_ASCII, 0 },
    { TIFFTAG_MAKE,	        "Make",	        TIFF_ASCII, 0 },
    { TIFFTAG_MODEL,	        "Model",	TIFF_ASCII, 0 },
    { TIFFTAG_SOFTWARE,	        "Software",	TIFF_ASCII, 0 },
    { TIFFTAG_ARTIST,	        "Artist",	TIFF_ASCII, 0 },
    { TIFFTAG_COPYRIGHT,	"Copyright",	TIFF_ASCII, 0 },
    { TIFFTAG_DATETIME,	        "DateTime",	TIFF_ASCII, 0 },
    { TIFFTAG_EXIFIFD,          "Exif:ExifIFD", TIFF_NOTYPE, 1 },
    { TIFFTAG_INTEROPERABILITYIFD, "Exif:InteroperabilityIFD", TIFF_NOTYPE, 1 },
    { TIFFTAG_GPSIFD,           "Exif:GPSIFD",  TIFF_NOTYPE, 1 },

    // EXIF tags we may come across
    { EXIF_EXPOSURETIME,	"ExposureTime",	TIFF_RATIONAL, 1 },
    { EXIF_FNUMBER,	        "FNumber",	TIFF_RATIONAL, 1 },
    { EXIF_EXPOSUREPROGRAM,	"Exif:ExposureProgram",	TIFF_SHORT, 1 }, // ?? translate to ascii names?
    { EXIF_SPECTRALSENSITIVITY,"Exif:SpectralSensitivity",	TIFF_ASCII, 0 },
    { EXIF_ISOSPEEDRATINGS,	"Exif:ISOSpeedRatings",	TIFF_SHORT, 1 },
    { EXIF_OECF,	        "Exif:OECF",	TIFF_NOTYPE, 1 },	 // skip it
    { EXIF_EXIFVERSION,	"Exif:ExifVersion",	TIFF_UNDEFINED, 1, version4char_handler },	 // skip it
    { EXIF_DATETIMEORIGINAL,	"Exif:DateTimeOriginal",	TIFF_ASCII, 0 },
    { EXIF_DATETIMEDIGITIZED,"Exif:DateTimeDigitized",   TIFF_ASCII, 0 },
    { EXIF_OFFSETTIME,"Exif:OffsetTime",   TIFF_ASCII, 0 },
    { EXIF_OFFSETTIMEORIGINAL,"Exif:OffsetTimeOriginal",   TIFF_ASCII, 0 },
    { EXIF_OFFSETTIMEDIGITIZED,"Exif:OffsetTimeDigitized",	TIFF_ASCII, 0 },
    { EXIF_COMPONENTSCONFIGURATION, "Exif:ComponentsConfiguration",	TIFF_UNDEFINED, 1 },
    { EXIF_COMPRESSEDBITSPERPIXEL,  "Exif:CompressedBitsPerPixel",	TIFF_RATIONAL, 1 },
    { EXIF_SHUTTERSPEEDVALUE,"Exif:ShutterSpeedValue",	TIFF_SRATIONAL, 1 }, // APEX units
    { EXIF_APERTUREVALUE,	"Exif:ApertureValue",	TIFF_RATIONAL, 1 },	// APEX units
    { EXIF_BRIGHTNESSVALUE,	"Exif:BrightnessValue",	TIFF_SRATIONAL, 1 },
    { EXIF_EXPOSUREBIASVALUE,"Exif:ExposureBiasValue",	TIFF_SRATIONAL, 1 },
    { EXIF_MAXAPERTUREVALUE,	"Exif:MaxApertureValue",TIFF_RATIONAL, 1 },
    { EXIF_SUBJECTDISTANCE,	"Exif:SubjectDistance",	TIFF_RATIONAL, 1 },
    { EXIF_METERINGMODE,	"Exif:MeteringMode",	TIFF_SHORT, 1 },
    { EXIF_LIGHTSOURCE,	"Exif:LightSource",	TIFF_SHORT, 1 },
    { EXIF_FLASH,	        "Exif:Flash",	        TIFF_SHORT, 1 },
    { EXIF_FOCALLENGTH,	"Exif:FocalLength",	TIFF_RATIONAL, 1 }, // mm
    { EXIF_SECURITYCLASSIFICATION, "Exif:SecurityClassification", TIFF_ASCII, 1 },
    { EXIF_IMAGEHISTORY,     "Exif:ImageHistory",    TIFF_ASCII, 1 },
    { EXIF_SUBJECTAREA,	"Exif:SubjectArea",	TIFF_NOTYPE, 1 }, // FIXME
    { EXIF_MAKERNOTE,	"Exif:MakerNote",	TIFF_BYTE, 0, makernote_handler },
    { EXIF_USERCOMMENT,	"Exif:UserComment",	TIFF_BYTE, 0 },
    { EXIF_SUBSECTIME,	"Exif:SubsecTime",	        TIFF_ASCII, 0 },
    { EXIF_SUBSECTIMEORIGINAL,"Exif:SubsecTimeOriginal",	TIFF_ASCII, 0 },
    { EXIF_SUBSECTIMEDIGITIZED,"Exif:SubsecTimeDigitized",	TIFF_ASCII, 0 },
    { EXIF_FLASHPIXVERSION,	"Exif:FlashPixVersion",	TIFF_UNDEFINED, 1, version4char_handler },	// skip "Exif:FlashPixVesion",	TIFF_NOTYPE, 1 },
    { EXIF_COLORSPACE,	"Exif:ColorSpace",	TIFF_SHORT, 1 },
    { EXIF_PIXELXDIMENSION,	"Exif:PixelXDimension",	TIFF_LONG, 1 },
    { EXIF_PIXELYDIMENSION,	"Exif:PixelYDimension",	TIFF_LONG, 1 },
    { EXIF_RELATEDSOUNDFILE,	"Exif:RelatedSoundFile", TIFF_ASCII, 0 },
    { EXIF_FLASHENERGY,	"Exif:FlashEnergy",	TIFF_RATIONAL, 1 },
    { EXIF_SPATIALFREQUENCYRESPONSE,	"Exif:SpatialFrequencyResponse",	TIFF_NOTYPE, 1 },
    { EXIF_FOCALPLANEXRESOLUTION,	"Exif:FocalPlaneXResolution",	TIFF_RATIONAL, 1 },
    { EXIF_FOCALPLANEYRESOLUTION,	"Exif:FocalPlaneYResolution",	TIFF_RATIONAL, 1 },
    { EXIF_FOCALPLANERESOLUTIONUNIT,	"Exif:FocalPlaneResolutionUnit",	TIFF_SHORT, 1 }, // Symbolic?
    { EXIF_SUBJECTLOCATION,	"Exif:SubjectLocation",	TIFF_SHORT, 2 },
    { EXIF_EXPOSUREINDEX,	"Exif:ExposureIndex",	TIFF_RATIONAL, 1 },
    { EXIF_SENSINGMETHOD,	"Exif:SensingMethod",	TIFF_SHORT, 1 },
    { EXIF_FILESOURCE,	"Exif:FileSource",	TIFF_UNDEFINED, 1 },
    { EXIF_SCENETYPE,	"Exif:SceneType",	TIFF_UNDEFINED, 1 },
    { EXIF_CFAPATTERN,	"Exif:CFAPattern",	TIFF_NOTYPE, 1 }, // FIXME
    { EXIF_CUSTOMRENDERED,	"Exif:CustomRendered",	TIFF_SHORT, 1 },
    { EXIF_EXPOSUREMODE,	"Exif:ExposureMode",	TIFF_SHORT, 1 },
    { EXIF_WHITEBALANCE,	"Exif:WhiteBalance",	TIFF_SHORT, 1 },
    { EXIF_DIGITALZOOMRATIO,	"Exif:DigitalZoomRatio", TIFF_RATIONAL, 1 },
    { EXIF_FOCALLENGTHIN35MMFILM, "Exif:FocalLengthIn35mmFilm",	TIFF_SHORT, 1 },
    { EXIF_SCENECAPTURETYPE,	"Exif:SceneCaptureType", TIFF_SHORT, 1 },
    { EXIF_GAINCONTROL,	"Exif:GainControl",	TIFF_RATIONAL, 1 },
    { EXIF_CONTRAST,	        "Exif:Contrast",	TIFF_SHORT, 1 },
    { EXIF_SATURATION,	"Exif:Saturation",	TIFF_SHORT, 1 },
    { EXIF_SHARPNESS,	"Exif:Sharpness",	TIFF_SHORT, 1 },
    { EXIF_DEVICESETTINGDESCRIPTION,	"Exif:DeviceSettingDescription",	TIFF_NOTYPE, 1 }, // FIXME
    { EXIF_SUBJECTDISTANCERANGE,	"Exif:SubjectDistanceRange",	TIFF_SHORT, 1 },
    { EXIF_IMAGEUNIQUEID,	"Exif:ImageUniqueID",   TIFF_ASCII, 0 },
    { EXIF_PHOTOGRAPHICSENSITIVITY,  "Exif:PhotographicSensitivity",  TIFF_SHORT, 1 },
    { EXIF_SENSITIVITYTYPE,  "Exif:SensitivityType",  TIFF_SHORT, 1 },
    { EXIF_STANDARDOUTPUTSENSITIVITY,  "Exif:StandardOutputSensitivity", TIFF_LONG, 1 },
    { EXIF_RECOMMENDEDEXPOSUREINDEX,  "Exif:RecommendedExposureIndex", TIFF_LONG, 1 },
    { EXIF_ISOSPEED,  "Exif:ISOSpeed", TIFF_LONG, 1 },
    { EXIF_ISOSPEEDLATITUDEYYY,  "Exif:ISOSpeedLatitudeyyy", TIFF_LONG, 1 },
    { EXIF_ISOSPEEDLATITUDEZZZ,  "Exif:ISOSpeedLatitudezzz", TIFF_LONG, 1 },
    { EXIF_TEMPERATURE,  "Exif:Temperature", TIFF_SRATIONAL, 1 },
    { EXIF_HUMIDITY,  "Exif:Humidity", TIFF_RATIONAL, 1 },
    { EXIF_PRESSURE,  "Exif:Pressure", TIFF_RATIONAL, 1 },
    { EXIF_WATERDEPTH,  "Exif:WaterDepth", TIFF_SRATIONAL, 1 },
    { EXIF_ACCELERATION,  "Exif:Acceleration", TIFF_RATIONAL, 1 },
    { EXIF_CAMERAELEVATIONANGLE,  "Exif:CameraElevationAngle", TIFF_SRATIONAL, 1 },
    { EXIF_CAMERAOWNERNAME,  "Exif:CameraOwnerName",  TIFF_ASCII, 0 },
    { EXIF_BODYSERIALNUMBER,  "Exif:BodySerialNumber", TIFF_ASCII, 0 },
    { EXIF_LENSSPECIFICATION,  "Exif:LensSpecification", TIFF_RATIONAL, 4 },
    { EXIF_LENSMAKE,  "Exif:LensMake",         TIFF_ASCII, 0 },
    { EXIF_LENSMODEL,  "Exif:LensModel",        TIFF_ASCII, 0 },
    { EXIF_LENSSERIALNUMBER,  "Exif:LensSerialNumber", TIFF_ASCII, 0 },
    { EXIF_GAMMA,  "Exif:Gamma", TIFF_RATIONAL, 0 }
    // clang-format on
};

const TagMap&
pvt::exif_tagmap_ref()
{
    static TagMap T("EXIF", exif_tag_table);
    return T;
}



// libtiff > 4.1.0 defines these in tiff.h. For older libtiff, let's define
// them ourselves.
#ifndef GPSTAG_VERSIONID
enum GPSTag {
    GPSTAG_VERSIONID            = 0,
    GPSTAG_LATITUDEREF          = 1,
    GPSTAG_LATITUDE             = 2,
    GPSTAG_LONGITUDEREF         = 3,
    GPSTAG_LONGITUDE            = 4,
    GPSTAG_ALTITUDEREF          = 5,
    GPSTAG_ALTITUDE             = 6,
    GPSTAG_TIMESTAMP            = 7,
    GPSTAG_SATELLITES           = 8,
    GPSTAG_STATUS               = 9,
    GPSTAG_MEASUREMODE          = 10,
    GPSTAG_DOP                  = 11,
    GPSTAG_SPEEDREF             = 12,
    GPSTAG_SPEED                = 13,
    GPSTAG_TRACKREF             = 14,
    GPSTAG_TRACK                = 15,
    GPSTAG_IMGDIRECTIONREF      = 16,
    GPSTAG_IMGDIRECTION         = 17,
    GPSTAG_MAPDATUM             = 18,
    GPSTAG_DESTLATITUDEREF      = 19,
    GPSTAG_DESTLATITUDE         = 20,
    GPSTAG_DESTLONGITUDEREF     = 21,
    GPSTAG_DESTLONGITUDE        = 22,
    GPSTAG_DESTBEARINGREF       = 23,
    GPSTAG_DESTBEARING          = 24,
    GPSTAG_DESTDISTANCEREF      = 25,
    GPSTAG_DESTDISTANCE         = 26,
    GPSTAG_PROCESSINGMETHOD     = 27,
    GPSTAG_AREAINFORMATION      = 28,
    GPSTAG_DATESTAMP            = 29,
    GPSTAG_DIFFERENTIAL         = 30,
    GPSTAG_GPSHPOSITIONINGERROR = 31
};
#endif

static const TagInfo gps_tag_table[] = {
    // clang-format off
    { GPSTAG_VERSIONID,		"GPS:VersionID",	TIFF_BYTE, 4, version4uint8_handler }, 
    { GPSTAG_LATITUDEREF,	"GPS:LatitudeRef",	TIFF_ASCII, 2 },
    { GPSTAG_LATITUDE,		"GPS:Latitude",		TIFF_RATIONAL, 3 },
    { GPSTAG_LONGITUDEREF,	"GPS:LongitudeRef",	TIFF_ASCII, 2 },
    { GPSTAG_LONGITUDE,		"GPS:Longitude",	TIFF_RATIONAL, 3 }, 
    { GPSTAG_ALTITUDEREF,	"GPS:AltitudeRef",	TIFF_BYTE, 1 },
    { GPSTAG_ALTITUDE,		"GPS:Altitude",		TIFF_RATIONAL, 1 },
    { GPSTAG_TIMESTAMP,		"GPS:TimeStamp",	TIFF_RATIONAL, 3 },
    { GPSTAG_SATELLITES,	"GPS:Satellites",	TIFF_ASCII, 0 },
    { GPSTAG_STATUS,		"GPS:Status",		TIFF_ASCII, 2 },
    { GPSTAG_MEASUREMODE,	"GPS:MeasureMode",	TIFF_ASCII, 2 },
    { GPSTAG_DOP,		"GPS:DOP",		TIFF_RATIONAL, 1 },
    { GPSTAG_SPEEDREF,		"GPS:SpeedRef",		TIFF_ASCII, 2 },
    { GPSTAG_SPEED,		"GPS:Speed",		TIFF_RATIONAL, 1 },
    { GPSTAG_TRACKREF,		"GPS:TrackRef",		TIFF_ASCII, 2 },
    { GPSTAG_TRACK,		"GPS:Track",		TIFF_RATIONAL, 1 },
    { GPSTAG_IMGDIRECTIONREF,	"GPS:ImgDirectionRef",	TIFF_ASCII, 2 },
    { GPSTAG_IMGDIRECTION,	"GPS:ImgDirection",	TIFF_RATIONAL, 1 },
    { GPSTAG_MAPDATUM,		"GPS:MapDatum",		TIFF_ASCII, 0 },
    { GPSTAG_DESTLATITUDEREF,	"GPS:DestLatitudeRef",	TIFF_ASCII, 2 },
    { GPSTAG_DESTLATITUDE,	"GPS:DestLatitude",	TIFF_RATIONAL, 3 },
    { GPSTAG_DESTLONGITUDEREF,	"GPS:DestLongitudeRef",	TIFF_ASCII, 2 },
    { GPSTAG_DESTLONGITUDE,	"GPS:DestLongitude",	TIFF_RATIONAL, 3 }, 
    { GPSTAG_DESTBEARINGREF,	"GPS:DestBearingRef",	TIFF_ASCII, 2 },
    { GPSTAG_DESTBEARING,	"GPS:DestBearing",	TIFF_RATIONAL, 1 },
    { GPSTAG_DESTDISTANCEREF,	"GPS:DestDistanceRef",	TIFF_ASCII, 2 },
    { GPSTAG_DESTDISTANCE,	"GPS:DestDistance",	TIFF_RATIONAL, 1 },
    { GPSTAG_PROCESSINGMETHOD,	"GPS:ProcessingMethod",	TIFF_UNDEFINED, 1 },
    { GPSTAG_AREAINFORMATION,	"GPS:AreaInformation",	TIFF_UNDEFINED, 1 },
    { GPSTAG_DATESTAMP,		"GPS:DateStamp",	TIFF_ASCII, 0 },
    { GPSTAG_DIFFERENTIAL,	"GPS:Differential",	TIFF_SHORT, 1 },
    { GPSTAG_GPSHPOSITIONINGERROR,	"GPS:HPositioningError",TIFF_RATIONAL, 1 }
    // clang-format on
};


const TagMap&
pvt::gps_tagmap_ref()
{
    static TagMap T("GPS", gps_tag_table);
    return T;
}



cspan<TagInfo>
tag_table(string_view tablename)
{
    if (tablename == "Exif")
        return cspan<TagInfo>(exif_tag_table);
    if (tablename == "GPS")
        return cspan<TagInfo>(gps_tag_table);
    // if (tablename == "TIFF")
    return cspan<TagInfo>(tiff_tag_table);
}



/// Add one EXIF directory entry's data to spec under the given 'name'.
/// The directory entry is in *dirp, buf points to the beginning of the
/// TIFF "file", i.e. all TIFF tag offsets are relative to buf.  If swab
/// is true, the endianness of the file doesn't match the endianness of
/// the host CPU, therefore all integer and float data embedded in buf
/// needs to be byte-swapped.  Note that *dirp HAS already been swapped,
/// if necessary, so no byte swapping on *dirp is necessary.
static void
add_exif_item_to_spec(ImageSpec& spec, const char* name,
                      const TIFFDirEntry* dirp, cspan<uint8_t> buf, bool swab,
                      int offset_adjustment = 0)
{
    OIIO_ASSERT(dirp);
    const uint8_t* dataptr = (const uint8_t*)pvt::dataptr(*dirp, buf,
                                                          offset_adjustment);
    if (!dataptr)
        return;
    TypeDesc type = tiff_datatype_to_typedesc(*dirp);
    if (dirp->tdir_type == TIFF_SHORT) {
        std::vector<uint16_t> d((const uint16_t*)dataptr,
                                (const uint16_t*)dataptr + dirp->tdir_count);
        if (swab)
            swap_endian(d.data(), d.size());
        spec.attribute(name, type, d.data());
        return;
    }
    if (dirp->tdir_type == TIFF_LONG) {
        std::vector<uint32_t> d((const uint32_t*)dataptr,
                                (const uint32_t*)dataptr + dirp->tdir_count);
        if (swab)
            swap_endian(d.data(), d.size());
        spec.attribute(name, type, d.data());
        return;
    }
    if (dirp->tdir_type == TIFF_RATIONAL) {
        int n    = dirp->tdir_count;  // How many
        float* f = OIIO_ALLOCA(float, n);
        for (int i = 0; i < n; ++i) {
            unsigned int num, den;
            num = ((const unsigned int*)dataptr)[2 * i + 0];
            den = ((const unsigned int*)dataptr)[2 * i + 1];
            if (swab) {
                swap_endian(&num);
                swap_endian(&den);
            }
            f[i] = (float)((double)num / (double)den);
        }
        if (dirp->tdir_count == 1)
            spec.attribute(name, *f);
        else
            spec.attribute(name, TypeDesc(TypeDesc::FLOAT, n), f);
        return;
    }
    if (dirp->tdir_type == TIFF_SRATIONAL) {
        int n    = dirp->tdir_count;  // How many
        float* f = OIIO_ALLOCA(float, n);
        for (int i = 0; i < n; ++i) {
            int num, den;
            num = ((const int*)dataptr)[2 * i + 0];
            den = ((const int*)dataptr)[2 * i + 1];
            if (swab) {
                swap_endian(&num);
                swap_endian(&den);
            }
            f[i] = (float)((double)num / (double)den);
        }
        if (dirp->tdir_count == 1)
            spec.attribute(name, *f);
        else
            spec.attribute(name, TypeDesc(TypeDesc::FLOAT, n), f);
        return;
    }
    if (dirp->tdir_type == TIFF_ASCII) {
        int len         = tiff_data_size(*dirp);
        const char* ptr = (const char*)dataptr;
        while (len && ptr[len - 1] == 0)  // Don't grab the terminating null
            --len;
        std::string str(ptr, len);
        if (strlen(str.c_str()) < str.length())  // Stray \0 in the middle
            str = std::string(str.c_str());
        spec.attribute(name, str);
        return;
    }
    if (dirp->tdir_type == TIFF_BYTE && dirp->tdir_count == 1) {
        // Not sure how to handle "bytes" generally, but certainly for just
        // one, add it as an int.
        unsigned char d;
        d = *dataptr;  // byte stored in offset itself
        spec.attribute(name, (int)d);
        return;
    }

#if 0
    if (dirp->tdir_type == TIFF_UNDEFINED || dirp->tdir_type == TIFF_BYTE) {
        // Add it as bytes
        const void *addr = dirp->tdir_count <= 4 ? (const void *) &dirp->tdir_offset 
                                                 : (const void *) &buf[dirp->tdir_offset];
        spec.attribute (name, TypeDesc(TypeDesc::UINT8, dirp->tdir_count), addr);
    }
#endif

#if !defined(NDEBUG) || DEBUG_EXIF_UNHANDLED
    std::cerr << "add_exif_item_to_spec: didn't know how to process " << name
              << ", type " << dirp->tdir_type << " x " << dirp->tdir_count
              << "\n";
#endif
}



/// Process a single TIFF directory entry embedded in the JPEG 'APP1'
/// data.  The directory entry is in *dirp, buf points to the beginning
/// of the TIFF "file", i.e. all TIFF tag offsets are relative to buf.
/// The goal is to decode the tag and put the data into appropriate
/// attribute slots of spec.  If swab is true, the endianness of the
/// file doesn't match the endianness of the host CPU, therefore all
/// integer and float data embedded in buf needs to be byte-swapped.
/// Note that *dirp has not been swapped, and so is still in the native
/// endianness of the file.
static void
read_exif_tag(ImageSpec& spec, const TIFFDirEntry* dirp, cspan<uint8_t> buf,
              bool swab, int offset_adjustment,
              std::set<size_t>& ifd_offsets_seen, const TagMap& tagmap)
{
    if ((const uint8_t*)dirp < buf.data()
        || (const uint8_t*)dirp + sizeof(TIFFDirEntry)
               >= buf.data() + buf.size()) {
#if DEBUG_EXIF_READ
        std::cerr << "Ignoring directory outside of the buffer.\n";
#endif
        return;
    }

    const TagMap& exif_tagmap(exif_tagmap_ref());
    const TagMap& gps_tagmap(gps_tagmap_ref());

    // Make a copy of the pointed-to TIFF directory, swab the components
    // if necessary.
    TIFFDirEntry dir = *dirp;
    if (swab) {
        swap_endian(&dir.tdir_tag);
        swap_endian(&dir.tdir_type);
        swap_endian(&dir.tdir_count);
        // only swab true offsets, not data embedded in the offset field
        if (tiff_data_size(dir) > 4)
            swap_endian(&dir.tdir_offset);
    }

#if DEBUG_EXIF_READ
    std::cerr << "Read " << tagmap.mapname() << " ";
    print_dir_entry(std::cerr, tagmap, dir, buf, offset_adjustment);
#endif

    if (dir.tdir_tag == TIFFTAG_EXIFIFD || dir.tdir_tag == TIFFTAG_GPSIFD) {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian(&offset);
        if (offset >= size_t(buf.size())) {
#if DEBUG_EXIF_READ
            unsigned int off2 = offset;
            swap_endian(&off2);
            std::cerr << "Bad Exif block? ExifIFD has offset " << offset
                      << " inexplicably greater than exif buffer length "
                      << buf.size() << " (byte swapped = " << off2 << ")\n";
#endif
            return;
        }
        // Don't recurse if we've already visited this IFD
        if (ifd_offsets_seen.find(offset) != ifd_offsets_seen.end()) {
#if DEBUG_EXIF_READ
            std::cerr << "Early ifd exit\n";
#endif
            return;
        }
        ifd_offsets_seen.insert(offset);
#if DEBUG_EXIF_READ
        std::cerr << "Now we've seen offset " << offset << "\n";
#endif
        const unsigned char* ifd = ((const unsigned char*)buf.data() + offset);
        unsigned short ndirs     = *(const unsigned short*)ifd;
        if (swab)
            swap_endian(&ndirs);
        if (dir.tdir_tag == TIFFTAG_GPSIFD && ndirs > 32) {
            // We have encountered JPEG files that inexplicably have the
            // directory count for the GPS data using the wrong byte order.
            // In this case, since there are only 32 possible GPS related
            // tags, we use that as a sanity check and skip the corrupted
            // data block. This isn't a very general solution, but it's a
            // rare case and clearly a broken file. We're just trying not to
            // crash in this case.
            return;
        }

#if DEBUG_EXIF_READ
        std::cerr << "exifid has type " << dir.tdir_type << ", offset "
                  << dir.tdir_offset << "\n";
        std::cerr << "EXIF Number of directory entries = " << ndirs << "\n";
#endif
        for (int d = 0; d < ndirs; ++d)
            read_exif_tag(
                spec, (const TIFFDirEntry*)(ifd + 2 + d * sizeof(TIFFDirEntry)),
                buf, swab, offset_adjustment, ifd_offsets_seen,
                dir.tdir_tag == TIFFTAG_EXIFIFD ? exif_tagmap : gps_tagmap);
#if DEBUG_EXIF_READ
        std::cerr << "> End EXIF\n";
#endif
    } else if (dir.tdir_tag == TIFFTAG_INTEROPERABILITYIFD) {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian(&offset);
        // Don't recurse if we've already visited this IFD
        if (ifd_offsets_seen.find(offset) != ifd_offsets_seen.end())
            return;
        ifd_offsets_seen.insert(offset);
#if DEBUG_EXIF_READ
        std::cerr << "Now we've seen offset " << offset << "\n";
#endif
        const unsigned char* ifd = ((const unsigned char*)buf.data() + offset);
        unsigned short ndirs     = *(const unsigned short*)ifd;
        if (swab)
            swap_endian(&ndirs);
#if DEBUG_EXIF_READ
        std::cerr << "\n\nInteroperability has type " << dir.tdir_type
                  << ", offset " << dir.tdir_offset << "\n";
        std::cerr << "Interoperability Number of directory entries = " << ndirs
                  << "\n";
#endif
        for (int d = 0; d < ndirs; ++d)
            read_exif_tag(
                spec, (const TIFFDirEntry*)(ifd + 2 + d * sizeof(TIFFDirEntry)),
                buf, swab, offset_adjustment, ifd_offsets_seen, exif_tagmap);
#if DEBUG_EXIF_READ
        std::cerr << "> End Interoperability\n\n";
#endif
    } else {
        // Everything else -- use our table to handle the general case
        const TagInfo* taginfo = tagmap.find(dir.tdir_tag);
        if (taginfo) {
            if (taginfo->handler)
                taginfo->handler(*taginfo, dir, buf, spec, swab,
                                 offset_adjustment);
            else if (taginfo->tifftype != TIFF_NOTYPE)
                add_exif_item_to_spec(spec, taginfo->name, &dir, buf, swab,
                                      offset_adjustment);
        } else {
#if DEBUG_EXIF_READ || DEBUG_EXIF_UNHANDLED
            Strutil::fprintf(
                stderr,
                "read_exif_tag: Unhandled %s tag=%d (0x%x), type=%d count=%d (%s), offset=%d\n",
                tagmap.mapname(), dir.tdir_tag, dir.tdir_tag, dir.tdir_type,
                dir.tdir_count, tiff_datatype_to_typedesc(dir),
                dir.tdir_offset);
#endif
        }
    }
}



/// Convert to the desired integer type and then append_tiff_dir_entry it.
///
template<class T>
static bool
append_tiff_dir_entry_integer(const ParamValue& p,
                              std::vector<TIFFDirEntry>& dirs,
                              std::vector<char>& data, int tag,
                              TIFFDataType type, size_t offset_correction,
                              OIIO::endian endianreq)
{
    T i;
    switch (p.type().basetype) {
    case TypeDesc::UINT: i = (T) * (unsigned int*)p.data(); break;
    case TypeDesc::INT: i = (T) * (int*)p.data(); break;
    case TypeDesc::UINT16: i = (T) * (unsigned short*)p.data(); break;
    case TypeDesc::INT16: i = (T) * (short*)p.data(); break;
    default: return false;
    }
    append_tiff_dir_entry(dirs, data, tag, type, 1, &i, offset_correction, 0,
                          endianreq);
    return true;
}


/// Helper: For param that needs to be added as a tag, create a TIFF
/// directory entry for it in dirs and add its data in data.  Set the
/// directory's offset just to the position within data where it will
/// reside.  Don't worry about it being relative to the start of some
/// TIFF structure.
static void
encode_exif_entry(const ParamValue& p, int tag, std::vector<TIFFDirEntry>& dirs,
                  std::vector<char>& data, const TagMap& tagmap,
                  size_t offset_correction, OIIO::endian endianreq)
{
    if (tag < 0)
        return;
    TIFFDataType type = tagmap.tifftype(tag);
    size_t count      = (size_t)tagmap.tiffcount(tag);
    TypeDesc element  = p.type().elementtype();

    switch (type) {
    case TIFF_ASCII:
        if (p.type() == TypeDesc::STRING) {
            const char* s = *(const char**)p.data();
            int len       = strlen(s) + 1;
            append_tiff_dir_entry(dirs, data, tag, type, len, s,
                                  offset_correction, 0, endianreq);
            return;
        }
        break;
    case TIFF_RATIONAL:
        if (element == TypeDesc::FLOAT) {
            unsigned int* rat = OIIO_ALLOCA(unsigned int, 2 * count);
            const float* f    = (const float*)p.data();
            for (size_t i = 0; i < count; ++i)
                float_to_rational(f[i], rat[2 * i], rat[2 * i + 1]);
            append_tiff_dir_entry(dirs, data, tag, type, count, rat,
                                  offset_correction, 0, endianreq);
            return;
        }
        break;
    case TIFF_SRATIONAL:
        if (element == TypeDesc::FLOAT) {
            int* rat       = OIIO_ALLOCA(int, 2 * count);
            const float* f = (const float*)p.data();
            for (size_t i = 0; i < count; ++i)
                float_to_rational(f[i], rat[2 * i], rat[2 * i + 1]);
            append_tiff_dir_entry(dirs, data, tag, type, count, rat,
                                  offset_correction, 0, endianreq);
            return;
        }
        break;
    case TIFF_SHORT:
        if (append_tiff_dir_entry_integer<unsigned short>(
                p, dirs, data, tag, type, offset_correction, endianreq))
            return;
        break;
    case TIFF_LONG:
        if (append_tiff_dir_entry_integer<unsigned int>(p, dirs, data, tag,
                                                        type, offset_correction,
                                                        endianreq))
            return;
        break;
    case TIFF_BYTE:
        if (append_tiff_dir_entry_integer<unsigned char>(
                p, dirs, data, tag, type, offset_correction, endianreq))
            return;
        break;
    default: break;
    }
#if DEBUG_EXIF_WRITE || DEBUG_EXIF_UNHANDLED
    std::cerr << "encode_exif_entry: Don't know how to add " << p.name()
              << ", tag " << tag << ", type " << type << ' ' << p.type().c_str()
              << "\n";
#endif
}



// Decode a raw Exif data block and save all the metadata in an
// ImageSpec.  Return true if all is ok, false if the exif block was
// somehow malformed.
void
pvt::decode_ifd(const unsigned char* ifd, cspan<uint8_t> buf, ImageSpec& spec,
                const TagMap& tag_map, std::set<size_t>& ifd_offsets_seen,
                bool swab, int offset_adjustment)
{
    // Read the directory that the header pointed to.  It should contain
    // some number of directory entries containing tags to process.
    unsigned short ndirs = *(const unsigned short*)ifd;
    if (swab)
        swap_endian(&ndirs);
    for (int d = 0; d < ndirs; ++d)
        read_exif_tag(spec,
                      (const TIFFDirEntry*)(ifd + 2 + d * sizeof(TIFFDirEntry)),
                      buf, swab, offset_adjustment, ifd_offsets_seen, tag_map);
}



void
pvt::append_tiff_dir_entry(std::vector<TIFFDirEntry>& dirs,
                           std::vector<char>& data, int tag, TIFFDataType type,
                           size_t count, const void* mydata,
                           size_t offset_correction, size_t offset_override,
                           OIIO::endian endianreq)
{
    TIFFDirEntry dir;
    dir.tdir_tag        = tag;
    dir.tdir_type       = type;
    dir.tdir_count      = count;
    size_t len          = tiff_data_size(dir);
    char* ptr           = nullptr;
    bool data_in_offset = false;
    if (len <= 4) {
        dir.tdir_offset = 0;
        data_in_offset  = true;
        if (mydata) {
            ptr = (char*)&dir.tdir_offset;
            memcpy(ptr, mydata, len);
        }
    } else {
        if (mydata) {
            // Add to the data vector and use its offset
            size_t oldsize  = data.size();
            dir.tdir_offset = data.size() - offset_correction;
            data.insert(data.end(), (char*)mydata, (char*)mydata + len);
            ptr = &data[oldsize];
        } else {
            // An offset override was given, use that, it means that data
            // ALREADY contains what we want.
            dir.tdir_offset = uint32_t(offset_override);
        }
    }
    if (endianreq != endian::native) {
        OIIO::swap_endian(&dir.tdir_tag);
        OIIO::swap_endian(&dir.tdir_type);
        OIIO::swap_endian(&dir.tdir_count);
        if (!data_in_offset)
            OIIO::swap_endian(&dir.tdir_offset);
        if (ptr) {
            if (type == TIFF_SHORT || type == TIFF_SSHORT)
                OIIO::swap_endian((uint16_t*)ptr, count);
            if (type == TIFF_LONG || type == TIFF_SLONG || type == TIFF_FLOAT
                || type == TIFF_IFD)
                OIIO::swap_endian((uint32_t*)ptr, count);
            if (type == TIFF_LONG8 || type == TIFF_SLONG8 || type == TIFF_DOUBLE
                || type == TIFF_IFD8)
                OIIO::swap_endian((uint64_t*)ptr, count);
            if (type == TIFF_RATIONAL || type == TIFF_SRATIONAL)
                OIIO::swap_endian((uint32_t*)ptr, count * 2);
        }
    }
    // Don't double-add
    for (TIFFDirEntry& d : dirs) {
        if (d.tdir_tag == dir.tdir_tag) {
            d = dir;
            return;
        }
    }
    dirs.push_back(dir);
}



bool
decode_exif(string_view exif, ImageSpec& spec)
{
    return decode_exif(cspan<uint8_t>((const uint8_t*)exif.data(), exif.size()),
                       spec);
}



bool
decode_exif(cspan<uint8_t> exif, ImageSpec& spec)
{
#if DEBUG_EXIF_READ
    std::cerr << "Exif dump:\n";
    for (size_t i = 0; i < exif.size(); ++i) {
        if (exif[i] >= ' ')
            std::cerr << (char)exif[i] << ' ';
        std::cerr << "(" << (int)exif[i] << ") ";
    }
    std::cerr << "\n";
#endif

    // The first item should be a standard TIFF header.  Note that HERE,
    // not the start of the Exif blob, is where all TIFF offsets are
    // relative to.  The header should have the right magic number (which
    // also tells us the endianness of the data) and an offset to the
    // first TIFF directory.
    //
    // N.B. Just read libtiff's "tiff.h" for info on the structure
    // layout of TIFF headers and directory entries.  The TIFF spec
    // itself is also helpful in this area.
    TIFFHeader head = *(const TIFFHeader*)exif.data();
    if (head.tiff_magic != 0x4949 && head.tiff_magic != 0x4d4d)
        return false;
    bool host_little = littleendian();
    bool file_little = (head.tiff_magic == 0x4949);
    bool swab        = (host_little != file_little);
    if (swab)
        swap_endian(&head.tiff_diroff);

    const unsigned char* ifd = ((const unsigned char*)exif.data()
                                + head.tiff_diroff);
    // keep track of IFD offsets we've already seen to avoid infinite
    // recursion if there are circular references.
    std::set<size_t> ifd_offsets_seen;
    decode_ifd(ifd, exif, spec, exif_tagmap_ref(), ifd_offsets_seen, swab);

    // A few tidbits to look for
    ParamValue* p;
    if ((p = spec.find_attribute("Exif:ColorSpace"))
        || (p = spec.find_attribute("ColorSpace"))) {
        int cs = -1;
        if (p->type() == TypeDesc::UINT)
            cs = *(const unsigned int*)p->data();
        else if (p->type() == TypeDesc::INT)
            cs = *(const int*)p->data();
        else if (p->type() == TypeDesc::UINT16)
            cs = *(const unsigned short*)p->data();
        else if (p->type() == TypeDesc::INT16)
            cs = *(const short*)p->data();
        // Exif spec says that anything other than 0xffff==uncalibrated
        // should be interpreted to be sRGB.
        if (cs != 0xffff)
            spec.attribute("oiio:ColorSpace", "sRGB");
    }

    // Look for a maker note offset, now that we have seen all the metadata
    // and therefore are sure we know the camera Make. See the comments in
    // makernote_handler for why this needs to come at the end.
    int makernote_offset = spec.get_int_attribute("oiio:MakerNoteOffset");
    if (makernote_offset > 0) {
        if (Strutil::iequals(spec.get_string_attribute("Make"), "Canon")) {
            decode_ifd((unsigned char*)exif.data() + makernote_offset, exif,
                       spec, pvt::canon_maker_tagmap_ref(), ifd_offsets_seen,
                       swab);
        }
        // Now we can erase the attrib we used to pass the message about
        // the maker note offset.
        spec.erase_attribute("oiio:MakerNoteOffset");
    }

    return true;
}



// DEPRECATED (1.8)
bool
decode_exif(const void* exif, int length, ImageSpec& spec)
{
    return decode_exif(cspan<uint8_t>((const uint8_t*)exif, length), spec);
}



template<class T>
inline void
append(std::vector<char>& blob, T v, endian endianreq = endian::native)
{
    if (endianreq != endian::native)
        swap_endian(&v);
    blob.insert(blob.end(), (const char*)&v, (const char*)&v + sizeof(T));
}

template<class T>
inline void
appendvec(std::vector<char>& blob, const std::vector<T>& v)
{
    blob.insert(blob.end(), (const char*)v.data(),
                (const char*)(v.data() + v.size()));
}



// DEPRECATED(2.1)
void
encode_exif(const ImageSpec& spec, std::vector<char>& blob)
{
    encode_exif(spec, blob, endian::native);
}



// Construct an Exif data block from the ImageSpec, appending the Exif
// data as a big blob to the char vector.
void
encode_exif(const ImageSpec& spec, std::vector<char>& blob,
            OIIO::endian endianreq)
{
    const TagMap& exif_tagmap(exif_tagmap_ref());
    const TagMap& gps_tagmap(gps_tagmap_ref());
    // const TagMap& canon_tagmap (pvt::canon_maker_tagmap_ref());

    // Reserve maximum space that an APP1 can take in a JPEG file, so
    // we can push_back to our heart's content and know that no offsets
    // or pointers to the exif vector's memory will change due to
    // reallocation.
    blob.reserve(0xffff);

    // Layout:
    //                     .-----------------------------------------
    //    (tiffstart) ---->|  TIFFHeader
    //                     |    magic
    //                     |    version
    //                  .--+--  diroff
    //                  |  |-----------------------------------------
    //            .-----+->|  d
    //            |     |  |   a
    //            |  .--+->|    t
    //            |  |  |  |     a
    //        .---+--+--+->|  d
    //        |   |  |  |  |   a
    //      .-+---+--+--+->|    t
    //      | |   |  |  |  |     a
    //      | |   |  |  |  +-----------------------------------------
    //      | |   |  |  `->|  number of top dir entries
    //      | |   `--+-----+- top dir entry 0
    //      | |      |     |  ...
    //      | |      | .---+- top dir Exif entry (point to Exif IFD)
    //      | |      | |   |  ...
    //      | |      | |   +------------------------------------------
    //      | |      | `-->|  number of Exif IFD dir entries (n)
    //      | |      `-----+- Exif IFD entry 0
    //      | |            |  ...
    //      | |        .---+- Exif entry for maker note
    //      | |        |   |  ...
    //      | `--------+---+- Exif IFD entry n-1
    //      |          |   +------------------------------------------
    //      |           `->|  number of makernote IFD dir entries
    //      `--------------+- Makernote IFD entry 0
    //                     |  ...
    //                     `------------------------------------------

    // Put a TIFF header
    size_t tiffstart = blob.size();  // store initial size
    TIFFHeader head;
    head.tiff_magic   = (endianreq == endian::little) ? 0x4949 : 0x4d4d;
    head.tiff_version = 42;
    // N.B. need to swap_endian head.tiff_diroff  below, once we know the sizes
    append(blob, head);

    // Accumulate separate tag directories for TIFF, Exif, GPS, and Interop.
    std::vector<TIFFDirEntry> tiffdirs, exifdirs, gpsdirs;
    std::vector<TIFFDirEntry> makerdirs;

    // Go through all spec attribs, add them to the appropriate tag
    // directory (tiff, gps, or exif), adding their data to the main blob.
    for (const ParamValue& p : spec.extra_attribs) {
        // Which tag domain are we using?
        if (Strutil::starts_with(p.name(), "GPS:")) {
            int tag = gps_tagmap.tag(p.name());
            if (tag >= 0)
                encode_exif_entry(p, tag, gpsdirs, blob, gps_tagmap, tiffstart,
                                  endianreq);
        } else {
            // Not GPS
            int tag = exif_tagmap.tag(p.name());
            if (tag < EXIF_EXPOSURETIME || tag > EXIF_IMAGEUNIQUEID) {
                // This range of Exif tags go in the main TIFF directories,
                // not the Exif IFD. Whatever.
                encode_exif_entry(p, tag, tiffdirs, blob, exif_tagmap,
                                  tiffstart, endianreq);
            } else {
                encode_exif_entry(p, tag, exifdirs, blob, exif_tagmap,
                                  tiffstart, endianreq);
            }
        }
    }

    // If we're a canon camera, construct the dirs for the Makernote,
    // with the data adding to the main blob.
    if (Strutil::iequals(spec.get_string_attribute("Make"), "Canon"))
        pvt::encode_canon_makernote(blob, makerdirs, spec, tiffstart);

#if DEBUG_EXIF_WRITE
    std::cerr << "Blob header size " << blob.size() << "\n";
    std::cerr << "tiff tags: " << tiffdirs.size() << "\n";
    std::cerr << "exif tags: " << exifdirs.size() << "\n";
    std::cerr << "gps tags: " << gpsdirs.size() << "\n";
    std::cerr << "canon makernote tags: " << makerdirs.size() << "\n";
#endif

    // If any legit Exif info was found (including if there's a maker note),
    // add some extra required Exif fields.
    if (exifdirs.size() || makerdirs.size()) {
        // Add some required Exif tags that wouldn't be in the spec
        append_tiff_dir_entry(exifdirs, blob, EXIF_EXIFVERSION, TIFF_UNDEFINED,
                              4, "0230", tiffstart, 0, endianreq);
        append_tiff_dir_entry(exifdirs, blob, EXIF_FLASHPIXVERSION,
                              TIFF_UNDEFINED, 4, "0100", tiffstart, 0,
                              endianreq);
        static char componentsconfig[] = { 1, 2, 3, 0 };
        append_tiff_dir_entry(exifdirs, blob, EXIF_COMPONENTSCONFIGURATION,
                              TIFF_UNDEFINED, 4, componentsconfig, tiffstart, 0,
                              endianreq);
    }

    // If any GPS info was found, add a version tag to the GPS fields.
    if (gpsdirs.size()) {
        // Add some required Exif tags that wouldn't be in the spec
        static char ver[] = { 2, 2, 0, 0 };
        append_tiff_dir_entry(gpsdirs, blob, GPSTAG_VERSIONID, TIFF_BYTE, 4,
                              &ver, tiffstart, 0, endianreq);
    }

    // Compute offsets:
    // TIFF dirs will start after the data
    size_t tiffdirs_offset = blob.size() - tiffstart;
    size_t tiffdirs_size   = sizeof(uint16_t)  // ndirs
                           + sizeof(TIFFDirEntry) * tiffdirs.size()
                           + (exifdirs.size() ? sizeof(TIFFDirEntry) : 0)
                           + (gpsdirs.size() ? sizeof(TIFFDirEntry) : 0)
                           + sizeof(uint32_t);  // zero pad for next IFD offset
    // Exif dirs will start after the TIFF dirs.
    size_t exifdirs_offset = tiffdirs_offset + tiffdirs_size;
    size_t exifdirs_size
        = exifdirs.empty()
              ? 0
              : (sizeof(uint16_t)  // ndirs
                 + sizeof(TIFFDirEntry) * exifdirs.size()
                 + (makerdirs.size() ? sizeof(TIFFDirEntry) : 0)
                 + sizeof(uint32_t));  // zero pad for next IFD offset
    // GPS dirs will start after Exif
    size_t gpsdirs_offset = exifdirs_offset + exifdirs_size;
    size_t gpsdirs_size
        = gpsdirs.empty()
              ? 0
              : (sizeof(uint16_t)  // ndirs
                 + sizeof(TIFFDirEntry) * gpsdirs.size()
                 + sizeof(uint32_t));  // zero pad for next IFD offset
    // MakerNote is after GPS
    size_t makerdirs_offset = gpsdirs_offset + gpsdirs_size;
    size_t makerdirs_size
        = makerdirs.empty()
              ? 0
              : (sizeof(uint16_t)  // ndirs
                 + sizeof(TIFFDirEntry) * makerdirs.size()
                 + sizeof(uint32_t));  // zero pad for next IFD offset

    // If any Maker info was found, add a MakerNote tag to the Exif dirs
    if (makerdirs.size()) {
        OIIO_ASSERT(exifdirs.size());
        // unsigned int size = (unsigned int) makerdirs_offset;
        append_tiff_dir_entry(exifdirs, blob, EXIF_MAKERNOTE, TIFF_BYTE,
                              makerdirs_size, nullptr, 0, makerdirs_offset,
                              endianreq);
    }

    // If any Exif info was found, add a Exif IFD tag to the TIFF dirs
    if (exifdirs.size()) {
        unsigned int size = (unsigned int)exifdirs_offset;
        append_tiff_dir_entry(tiffdirs, blob, TIFFTAG_EXIFIFD, TIFF_LONG, 1,
                              &size, tiffstart, 0, endianreq);
    }

    // If any GPS info was found, add a GPS IFD tag to the TIFF dirs
    if (gpsdirs.size()) {
        unsigned int size = (unsigned int)gpsdirs_offset;
        append_tiff_dir_entry(tiffdirs, blob, TIFFTAG_GPSIFD, TIFF_LONG, 1,
                              &size, tiffstart, 0, endianreq);
    }

    // All the tag dirs need to be sorted
    // Create a lambda that tests for order, accounting for endianness
    auto tagcompare = [=](const TIFFDirEntry& a, const TIFFDirEntry& b) {
        auto atag = a.tdir_tag;
        auto btag = b.tdir_tag;
        if (endianreq != endian::native) {
            swap_endian(&atag);
            swap_endian(&btag);
        }
        return (atag < btag);
    };

    std::sort(exifdirs.begin(), exifdirs.end(), tagcompare);
    std::sort(gpsdirs.begin(), gpsdirs.end(), tagcompare);
    std::sort(makerdirs.begin(), makerdirs.end(), tagcompare);

    // Now mash everything together
    size_t tiffdirstart = blob.size();
    append(blob, uint16_t(tiffdirs.size()), endianreq);  // ndirs for tiff
    appendvec(blob, tiffdirs);                           // tiff dirs
    append(blob, uint32_t(0));  // addr of next IFD (none)
    if (exifdirs.size()) {
        OIIO_ASSERT(blob.size() == exifdirs_offset + tiffstart);
        append(blob, uint16_t(exifdirs.size()), endianreq);  // ndirs for exif
        appendvec(blob, exifdirs);                           // exif dirs
        append(blob, uint32_t(0));  // addr of next IFD (none)
    }
    if (gpsdirs.size()) {
        OIIO_ASSERT(blob.size() == gpsdirs_offset + tiffstart);
        append(blob, uint16_t(gpsdirs.size()), endianreq);  // ndirs for gps
        appendvec(blob, gpsdirs);                           // gps dirs
        append(blob, uint32_t(0));  // addr of next IFD (none)
    }
    if (makerdirs.size()) {
        OIIO_ASSERT(blob.size() == makerdirs_offset + tiffstart);
        append(blob, uint16_t(makerdirs.size()), endianreq);  // ndirs for canon
        appendvec(blob, makerdirs);                           // canon dirs
        append(blob, uint32_t(0));  // addr of next IFD (none)
    }

    // Now go back and patch the header with the offset of the first TIFF
    // directory.
    uint32_t* diroff = &(((TIFFHeader*)(blob.data() + tiffstart))->tiff_diroff);
    *diroff          = tiffdirstart - tiffstart;
    if (endianreq != endian::native)
        swap_endian(diroff);

#if DEBUG_EXIF_WRITE
    std::cerr << "resulting exif block is a total of " << blob.size() << "\n";
#    if 1
    std::vector<size_t> ifdoffsets { tiffdirs_offset + tiffstart,
                                     exifdirs_offset + tiffstart,
                                     gpsdirs_offset + tiffstart,
                                     makerdirs_offset + tiffstart };
    std::cerr << "APP1 dump:" << dumpdata(blob, ifdoffsets, tiffstart);
#    endif
#endif
}



bool
exif_tag_lookup(string_view name, int& tag, int& tifftype, int& count)
{
    const TagInfo* e = exif_tagmap_ref().find(name);
    if (!e)
        return false;  // not found

    tag      = e->tifftag;
    tifftype = e->tifftype;
    count    = e->tiffcount;
    return true;
}


OIIO_NAMESPACE_END
