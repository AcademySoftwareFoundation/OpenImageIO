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


#include <cctype>
#include <cstdio>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

#include <boost/container/flat_map.hpp>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>

extern "C" {
#include "tiff.h"
}

// Some EXIF tags that don't seem to be in tiff.h
#ifndef EXIFTAG_SECURITYCLASSIFICATION
#define EXIFTAG_SECURITYCLASSIFICATION 37394
#endif
#ifndef EXIFTAG_IMAGEHISTORY
#define EXIFTAG_IMAGEHISTORY 37395
#endif

#ifdef TIFF_VERSION_BIG
// In old versions of TIFF, this was defined in tiff.h.  It's gone from
// "BIG TIFF" (libtiff 4.x), so we just define it here.

struct TIFFHeader {
    uint16_t tiff_magic;  /* magic number (defines byte order) */
    uint16_t tiff_version;/* TIFF version number */
    uint32_t tiff_diroff; /* byte offset to first directory */
};

struct TIFFDirEntry {
    uint16_t tdir_tag;    /* tag ID */
    uint16_t tdir_type;   /* data type -- see TIFFDataType enum */
    uint32_t tdir_count;  /* number of items; length in spec */
    uint32_t tdir_offset; /* byte offset to field data */
};
#endif

#include <OpenImageIO/imageio.h>


#define DEBUG_EXIF_READ  0
#define DEBUG_EXIF_WRITE 0

OIIO_NAMESPACE_BEGIN

namespace {


// Sizes of TIFFDataType members
static size_t tiff_data_sizes[] = {
    0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4
};

static int
tiff_data_size (const TIFFDirEntry &dir)
{
    const int num_data_sizes = sizeof(tiff_data_sizes) / sizeof(*tiff_data_sizes);
    int dir_index = (int)dir.tdir_type;
    if (dir_index < 0 || dir_index >= num_data_sizes) {
        // Inform caller about corrupted entry.
        return -1;
    }
    return tiff_data_sizes[dir_index] * dir.tdir_count;
}



struct EXIF_tag_info {
    int tifftag;            // TIFF tag used for this info
    const char *name;       // Attribute name we use
    TIFFDataType tifftype;  // Data type that TIFF wants
    int tiffcount;          // Number of items
};

static const EXIF_tag_info exif_tag_table[] = {
    // Skip ones handled by the usual JPEG code
    { TIFFTAG_IMAGEWIDTH,	"Exif:ImageWidth",	TIFF_NOTYPE, 1 },
    { TIFFTAG_IMAGELENGTH,	"Exif:ImageLength",	TIFF_NOTYPE, 1 },
    { TIFFTAG_BITSPERSAMPLE,	"Exif:BitsPerSample",	TIFF_NOTYPE, 1 },
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
    { EXIFTAG_EXPOSURETIME,	"ExposureTime",	TIFF_RATIONAL, 1 },
    { EXIFTAG_FNUMBER,	        "FNumber",	TIFF_RATIONAL, 1 },
    { EXIFTAG_EXPOSUREPROGRAM,	"Exif:ExposureProgram",	TIFF_SHORT, 1 }, // ?? translate to ascii names?
    { EXIFTAG_SPECTRALSENSITIVITY,"Exif:SpectralSensitivity",	TIFF_ASCII, 0 },
    { EXIFTAG_ISOSPEEDRATINGS,	"Exif:ISOSpeedRatings",	TIFF_SHORT, 1 },
    { EXIFTAG_OECF,	        "Exif:OECF",	TIFF_NOTYPE, 1 },	 // skip it
    { EXIFTAG_EXIFVERSION,	"Exif:ExifVersion",	TIFF_NOTYPE, 1 },	 // skip it
    { EXIFTAG_DATETIMEORIGINAL,	"Exif:DateTimeOriginal",	TIFF_ASCII, 0 },
    { EXIFTAG_DATETIMEDIGITIZED,"Exif:DateTimeDigitized",	TIFF_ASCII, 0 },
    { EXIFTAG_COMPONENTSCONFIGURATION, "Exif:ComponentsConfiguration",	TIFF_UNDEFINED, 1 },
    { EXIFTAG_COMPRESSEDBITSPERPIXEL,  "Exif:CompressedBitsPerPixel",	TIFF_RATIONAL, 1 },
    { EXIFTAG_SHUTTERSPEEDVALUE,"Exif:ShutterSpeedValue",	TIFF_SRATIONAL, 1 }, // APEX units
    { EXIFTAG_APERTUREVALUE,	"Exif:ApertureValue",	TIFF_RATIONAL, 1 },	// APEX units
    { EXIFTAG_BRIGHTNESSVALUE,	"Exif:BrightnessValue",	TIFF_SRATIONAL, 1 },
    { EXIFTAG_EXPOSUREBIASVALUE,"Exif:ExposureBiasValue",	TIFF_SRATIONAL, 1 },
    { EXIFTAG_MAXAPERTUREVALUE,	"Exif:MaxApertureValue",TIFF_RATIONAL, 1 },
    { EXIFTAG_SUBJECTDISTANCE,	"Exif:SubjectDistance",	TIFF_RATIONAL, 1 },
    { EXIFTAG_METERINGMODE,	"Exif:MeteringMode",	TIFF_SHORT, 1 },
    { EXIFTAG_LIGHTSOURCE,	"Exif:LightSource",	TIFF_SHORT, 1 },
    { EXIFTAG_FLASH,	        "Exif:Flash",	        TIFF_SHORT, 1 },
    { EXIFTAG_FOCALLENGTH,	"Exif:FocalLength",	TIFF_RATIONAL, 1 }, // mm
    { EXIFTAG_SECURITYCLASSIFICATION, "Exif:SecurityClassification", TIFF_ASCII, 1 },
    { EXIFTAG_IMAGEHISTORY,     "Exif:ImageHistory",    TIFF_ASCII, 1 },
    { EXIFTAG_SUBJECTAREA,	"Exif:SubjectArea",	TIFF_NOTYPE, 1 }, // skip
    { EXIFTAG_MAKERNOTE,	"Exif:MakerNote",	TIFF_NOTYPE, 1 },	 // skip it
    { EXIFTAG_USERCOMMENT,	"Exif:UserComment",	TIFF_NOTYPE, 1 },	// skip it
    { EXIFTAG_SUBSECTIME,	"Exif:SubsecTime",	        TIFF_ASCII, 0 },
    { EXIFTAG_SUBSECTIMEORIGINAL,"Exif:SubsecTimeOriginal",	TIFF_ASCII, 0 },
    { EXIFTAG_SUBSECTIMEDIGITIZED,"Exif:SubsecTimeDigitized",	TIFF_ASCII, 0 },
    { EXIFTAG_FLASHPIXVERSION,	"Exif:FlashPixVersion",	TIFF_NOTYPE, 1 },	// skip "Exif:FlashPixVesion",	TIFF_NOTYPE, 1 },
    { EXIFTAG_COLORSPACE,	"Exif:ColorSpace",	TIFF_SHORT, 1 },
    { EXIFTAG_PIXELXDIMENSION,	"Exif:PixelXDimension",	TIFF_LONG, 1 },
    { EXIFTAG_PIXELYDIMENSION,	"Exif:PixelYDimension",	TIFF_LONG, 1 },
    { EXIFTAG_RELATEDSOUNDFILE,	"Exif:RelatedSoundFile", TIFF_ASCII, 0 },
    { EXIFTAG_FLASHENERGY,	"Exif:FlashEnergy",	TIFF_RATIONAL, 1 },
    { EXIFTAG_SPATIALFREQUENCYRESPONSE,	"Exif:SpatialFrequencyResponse",	TIFF_NOTYPE, 1 },
    { EXIFTAG_FOCALPLANEXRESOLUTION,	"Exif:FocalPlaneXResolution",	TIFF_RATIONAL, 1 },
    { EXIFTAG_FOCALPLANEYRESOLUTION,	"Exif:FocalPlaneYResolution",	TIFF_RATIONAL, 1 },
    { EXIFTAG_FOCALPLANERESOLUTIONUNIT,	"Exif:FocalPlaneResolutionUnit",	TIFF_SHORT, 1 }, // Symbolic?
    { EXIFTAG_SUBJECTLOCATION,	"Exif:SubjectLocation",	TIFF_SHORT, 1 }, // FIXME: short[2]
    { EXIFTAG_EXPOSUREINDEX,	"Exif:ExposureIndex",	TIFF_RATIONAL, 1 },
    { EXIFTAG_SENSINGMETHOD,	"Exif:SensingMethod",	TIFF_SHORT, 1 },
    { EXIFTAG_FILESOURCE,	"Exif:FileSource",	TIFF_SHORT, 1 },
    { EXIFTAG_SCENETYPE,	"Exif:SceneType",	TIFF_SHORT, 1 },
    { EXIFTAG_CFAPATTERN,	"Exif:CFAPattern",	TIFF_NOTYPE, 1 },
    { EXIFTAG_CUSTOMRENDERED,	"Exif:CustomRendered",	TIFF_SHORT, 1 },
    { EXIFTAG_EXPOSUREMODE,	"Exif:ExposureMode",	TIFF_SHORT, 1 },
    { EXIFTAG_WHITEBALANCE,	"Exif:WhiteBalance",	TIFF_SHORT, 1 },
    { EXIFTAG_DIGITALZOOMRATIO,	"Exif:DigitalZoomRatio",TIFF_RATIONAL, 1 },
    { EXIFTAG_FOCALLENGTHIN35MMFILM, "Exif:FocalLengthIn35mmFilm",	TIFF_SHORT, 1 },
    { EXIFTAG_SCENECAPTURETYPE,	"Exif:SceneCaptureType",TIFF_SHORT, 1 },
    { EXIFTAG_GAINCONTROL,	"Exif:GainControl",	TIFF_RATIONAL, 1 },
    { EXIFTAG_CONTRAST,	        "Exif:Contrast",	TIFF_SHORT, 1 },
    { EXIFTAG_SATURATION,	"Exif:Saturation",	TIFF_SHORT, 1 },
    { EXIFTAG_SHARPNESS,	"Exif:Sharpness",	TIFF_SHORT, 1 },
    { EXIFTAG_DEVICESETTINGDESCRIPTION,	"Exif:DeviceSettingDescription",	TIFF_NOTYPE, 1 },
    { EXIFTAG_SUBJECTDISTANCERANGE,	"Exif:SubjectDistanceRange",	TIFF_SHORT, 1 },
    { EXIFTAG_IMAGEUNIQUEID,	"Exif:ImageUniqueID",   TIFF_ASCII, 0 },
    { 34855,                    "Exif:PhotographicSensitivity",  TIFF_SHORT, 1 },
    { 34864,                    "Exif:SensitivityType",  TIFF_SHORT, 1 },
    { 34865,                    "Exif:StandardOutputSensitivity", TIFF_LONG, 1 },
    { 34866,                    "Exif:RecommendedExposureIndex", TIFF_LONG, 1 },
    { 34867,                    "Exif:ISOSpeed", TIFF_LONG, 1 },
    { 34868,                    "Exif:ISOSpeedLatitudeyyy", TIFF_LONG, 1 },
    { 34869,                    "Exif:ISOSpeedLatitudezzz", TIFF_LONG, 1 },
    { 42032,                    "Exif:CameraOwnerName",  TIFF_ASCII, 0 },
    { 42033,                    "Exif:BodySerialNumber", TIFF_ASCII, 0 },
    { 42034,                    "Exif:LensSpecification",TIFF_RATIONAL, 4 },
    { 42035,                    "Exif:LensMake",         TIFF_ASCII, 0 },
    { 42036,                    "Exif:LensModel",        TIFF_ASCII, 0 },
    { 42037,                    "Exif:LensSerialNumber", TIFF_ASCII, 0 },
    { 42240,                    "Exif:Gamma", TIFF_RATIONAL, 0 },
    { -1, NULL }  // signal end of table
};



enum GPSTag {
    GPSTAG_VERSIONID = 0, 
    GPSTAG_LATITUDEREF = 1,  GPSTAG_LATITUDE = 2,
    GPSTAG_LONGITUDEREF = 3, GPSTAG_LONGITUDE = 4, 
    GPSTAG_ALTITUDEREF = 5,  GPSTAG_ALTITUDE = 6,
    GPSTAG_TIMESTAMP = 7,
    GPSTAG_SATELLITES = 8,
    GPSTAG_STATUS = 9,
    GPSTAG_MEASUREMODE = 10,
    GPSTAG_DOP = 11,
    GPSTAG_SPEEDREF = 12, GPSTAG_SPEED = 13,
    GPSTAG_TRACKREF = 14, GPSTAG_TRACK = 15,
    GPSTAG_IMGDIRECTIONREF = 16,  GPSTAG_IMGDIRECTION = 17,
    GPSTAG_MAPDATUM = 18,
    GPSTAG_DESTLATITUDEREF = 19,  GPSTAG_DESTLATITUDE = 20,
    GPSTAG_DESTLONGITUDEREF = 21, GPSTAG_DESTLONGITUDE = 22, 
    GPSTAG_DESTBEARINGREF = 23,   GPSTAG_DESTBEARING = 24,
    GPSTAG_DESTDISTANCEREF = 25,  GPSTAG_DESTDISTANCE = 26,
    GPSTAG_PROCESSINGMETHOD = 27,
    GPSTAG_AREAINFORMATION = 28,
    GPSTAG_DATESTAMP = 29,
    GPSTAG_DIFFERENTIAL = 30,
    GPSTAG_HPOSITIONINGERROR = 31
};

static const EXIF_tag_info gps_tag_table[] = {
    { GPSTAG_VERSIONID,		"GPS:VersionID",	TIFF_BYTE, 4 }, 
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
    { GPSTAG_HPOSITIONINGERROR,	"GPS:HPositioningError",TIFF_RATIONAL, 1 },
    { -1, NULL }  // signal end of table
};





class TagMap {
    typedef boost::container::flat_map<int, const EXIF_tag_info *> tagmap_t;
    typedef boost::container::flat_map<std::string, const EXIF_tag_info *> namemap_t;
    // Name map is lower case so it's effectively case-insensitive
public:
    TagMap (const EXIF_tag_info *tag_table) {
        for (int i = 0;  tag_table[i].tifftag >= 0;  ++i) {
            const EXIF_tag_info *eti = &tag_table[i];
            m_tagmap[eti->tifftag] = eti;
            if (eti->name) {
                std::string lowername (eti->name);
                Strutil::to_lower (lowername);
                m_namemap[lowername] = eti;
            }
        }
    }

    const EXIF_tag_info * find (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second;
    }

    const EXIF_tag_info * find (string_view name) const {
        std::string lowername (name);
        Strutil::to_lower (lowername);
        namemap_t::const_iterator i = m_namemap.find (lowername);
        return i == m_namemap.end() ? NULL : i->second;
    }

    const char * name (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second->name;
    }

    TIFFDataType tifftype (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? TIFF_NOTYPE : i->second->tifftype;
    }

    int tiffcount (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? 0 : i->second->tiffcount;
    }

    int tag (string_view name) const {
        std::string lowername (name);
        Strutil::to_lower (lowername);
        namemap_t::const_iterator i = m_namemap.find (lowername);
        return i == m_namemap.end() ? -1 : i->second->tifftag;
    }

private:
    tagmap_t m_tagmap;
    namemap_t m_namemap;
};

static TagMap& exif_tagmap_ref () {
    static TagMap T (exif_tag_table);
    return T;
}

static TagMap& gps_tagmap_ref () {
    static TagMap T (gps_tag_table);
    return T;
}




#if (DEBUG_EXIF_WRITE || DEBUG_EXIF_READ)
static bool
print_dir_entry (const TagMap &tagmap,
                 const TIFFDirEntry &dir, string_view buf)
{
    int len = tiff_data_size (dir);
    if (len < 0) {
        std::cerr << "Ignoring bad directory entry\n";
        return false;
    }
    const char *mydata = NULL;
    if (len <= 4) {  // short data is stored in the offset field
        mydata = (const char *)&dir.tdir_offset;
    } else {
        if (dir.tdir_offset >= buf.size() ||
           (dir.tdir_offset+tiff_data_size(dir)) >= buf.size())
            return false;    // bogus! overruns the buffer
        mydata = buf.data() + dir.tdir_offset;
    }
    const char *name = tagmap.name(dir.tdir_tag);
    std::cerr << "tag=" << dir.tdir_tag
              << " (" << (name ? name : "unknown") << ")"
              << ", type=" << dir.tdir_type
              << ", count=" << dir.tdir_count
              << ", offset=" << dir.tdir_offset << " = " ;
    switch (dir.tdir_type) {
    case TIFF_ASCII :
        std::cerr << "'" << (char *)mydata << "'";
        break;
    case TIFF_RATIONAL :
        {
            const unsigned int *u = (unsigned int *)mydata;
            for (size_t i = 0; i < dir.tdir_count;  ++i)
                std::cerr << u[2*i] << "/" << u[2*i+1] << " = "
                          << (double)u[2*i]/(double)u[2*i+1] << " ";
        }
        break;
    case TIFF_SRATIONAL :
        {
            const int *u = (int *)mydata;
            for (size_t i = 0; i < dir.tdir_count;  ++i)
                std::cerr << u[2*i] << "/" << u[2*i+1] << " = "
                          << (double)u[2*i]/(double)u[2*i+1] << " ";
        }
        break;
    case TIFF_SHORT :
        std::cerr << ((unsigned short *)mydata)[0];
        break;
    case TIFF_LONG :
        std::cerr << ((unsigned int *)mydata)[0];
        break;
    case TIFF_BYTE :
    case TIFF_UNDEFINED :
    case TIFF_NOTYPE :
    default:
        if (len <= 4 && dir.tdir_count > 4) {
            // Request more data than is stored.
            std::cerr << "Ignoring buffer with too much count of short data.\n";
            return false;
        }
        for (size_t i = 0;  i < dir.tdir_count;  ++i)
            std::cerr << (int)((unsigned char *)mydata)[i] << ' ';
        break;
    }
    std::cerr << "\n";
    return true;
}
#endif



/// Add one EXIF directory entry's data to spec under the given 'name'.
/// The directory entry is in *dirp, buf points to the beginning of the
/// TIFF "file", i.e. all TIFF tag offsets are relative to buf.  If swab
/// is true, the endianness of the file doesn't match the endianness of
/// the host CPU, therefore all integer and float data embedded in buf
/// needs to be byte-swapped.  Note that *dirp HAS already been swapped,
/// if necessary, so no byte swapping on *dirp is necessary.
static void
add_exif_item_to_spec (ImageSpec &spec, const char *name,
                       const TIFFDirEntry *dirp, string_view buf, bool swab)
{
    if (dirp->tdir_type == TIFF_SHORT && dirp->tdir_count == 1) {
        union { uint32_t i32; uint16_t i16[2]; } convert;
        convert.i32 = dirp->tdir_offset;
        unsigned short d = convert.i16[0];
        // N.B. The Exif spec says that for a 16 bit value, it's stored in
        // the *first* 16 bits of the offset area.
        if (swab)
            swap_endian (&d);
        spec.attribute (name, (unsigned int)d);
    } else if (dirp->tdir_type == TIFF_LONG && dirp->tdir_count == 1) {
        unsigned int d;
        d = * (const unsigned int *) &dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&d);
        spec.attribute (name, (unsigned int)d);
    } else if (dirp->tdir_type == TIFF_RATIONAL) {
        int n = dirp->tdir_count;  // How many
        float *f = (float *) alloca (n * sizeof(float));
        for (int i = 0;  i < n;  ++i) {
            unsigned int num, den;
            num = ((const unsigned int *) &(buf[dirp->tdir_offset]))[2*i+0];
            den = ((const unsigned int *) &(buf[dirp->tdir_offset]))[2*i+1];
            if (swab) {
                swap_endian (&num);
                swap_endian (&den);
            }
            f[i] = (float) ((double)num / (double)den);
        }
        if (dirp->tdir_count == 1)
            spec.attribute (name, *f);
        else
            spec.attribute (name, TypeDesc(TypeDesc::FLOAT, n), f);
    } else if (dirp->tdir_type == TIFF_SRATIONAL) {
        int n = dirp->tdir_count;  // How many
        float *f = (float *) alloca (n * sizeof(float));
        for (int i = 0;  i < n;  ++i) {
            int num, den;
            num = ((const int *) &(buf[dirp->tdir_offset]))[2*i+0];
            den = ((const int *) &(buf[dirp->tdir_offset]))[2*i+1];
            if (swab) {
                swap_endian (&num);
                swap_endian (&den);
            }
            f[i] = (float) ((double)num / (double)den);
        }
        if (dirp->tdir_count == 1)
            spec.attribute (name, *f);
        else
            spec.attribute (name, TypeDesc(TypeDesc::FLOAT, n), f);
    } else if (dirp->tdir_type == TIFF_ASCII) {
        int len = tiff_data_size (*dirp);
        const char *ptr = (len <= 4) ? (const char *)&dirp->tdir_offset 
                                     : (buf.data() + dirp->tdir_offset);
        while (len && ptr[len-1] == 0)  // Don't grab the terminating null
            --len;
        std::string str (ptr, len);
        if (strlen(str.c_str()) < str.length())  // Stray \0 in the middle
            str = std::string (str.c_str());
        spec.attribute (name, str);
    } else if (dirp->tdir_type == TIFF_BYTE && dirp->tdir_count == 1) {
        // Not sure how to handle "bytes" generally, but certainly for just
        // one, add it as an int.
        unsigned char d;
        d = * (const unsigned char *) &dirp->tdir_offset;  // byte stored in offset itself
        spec.attribute (name, (int)d);
    } else if (dirp->tdir_type == TIFF_UNDEFINED || dirp->tdir_type == TIFF_BYTE) {
        // Add it as bytes
#if 0
        const void *addr = dirp->tdir_count <= 4 ? (const void *) &dirp->tdir_offset 
                                                 : (const void *) &buf[dirp->tdir_offset];
        spec.attribute (name, TypeDesc::UINT8, dirp->tdir_count, addr);
#endif
    } else {
#ifndef NDEBUG
        std::cerr << "didn't know how to process " << name << ", type " 
                  << dirp->tdir_type << " x " << dirp->tdir_count << "\n";
#endif
    }
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
read_exif_tag (ImageSpec &spec, const TIFFDirEntry *dirp,
               string_view buf, bool swab,
               std::set<size_t> &ifd_offsets_seen,
               const TagMap &tagmap)
{
    if ((char*)dirp < buf.data() || (char*)dirp >= buf.data() + buf.size()) {
#if DEBUG_EXIF_READ
        std::cerr << "Ignoring directory outside of the buffer.\n";
#endif
        return;
    }

    TagMap& exif_tagmap (exif_tagmap_ref());
    TagMap& gps_tagmap (gps_tagmap_ref());

    // Make a copy of the pointed-to TIFF directory, swab the components
    // if necessary.
    TIFFDirEntry dir = *dirp;
    if (swab) {
        swap_endian (&dir.tdir_tag);
        swap_endian (&dir.tdir_type);
        swap_endian (&dir.tdir_count);
        // only swab true offsets, not data embedded in the offset field
        if (tiff_data_size (dir) > 4)
            swap_endian (&dir.tdir_offset);
    }

#if DEBUG_EXIF_READ
    std::cerr << "Read ";
    print_dir_entry (tagmap, dir, buf);
#endif

    if (dir.tdir_tag == TIFFTAG_EXIFIFD || dir.tdir_tag == TIFFTAG_GPSIFD) {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&offset);
        if (offset >= buf.size()) {
#if DEBUG_EXIF_READ
            unsigned int off2 = offset;
            swap_endian (&off2);
            std::cerr << "Bad Exif block? ExifIFD has offset " << offset
                      << " inexplicably greater than exif buffer length "
                      << buf.size() << " (byte swapped = " << off2 << ")\n";
#endif
            return;
        }
        // Don't recurse if we've already visited this IFD
        if (ifd_offsets_seen.find (offset) != ifd_offsets_seen.end()) {
#if DEBUG_EXIF_READ
            std::cerr << "Early ifd exit\n";
#endif
            return;
        }
        ifd_offsets_seen.insert (offset);
#if DEBUG_EXIF_READ
        std::cerr << "Now we've seen offset " << offset << "\n";
#endif
        const unsigned char *ifd = ((const unsigned char *)buf.data() + offset);
        unsigned short ndirs = *(const unsigned short *)ifd;
        if (swab)
            swap_endian (&ndirs);
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
        std::cerr << "exifid has type " << dir.tdir_type << ", offset " << dir.tdir_offset << "\n";
        std::cerr << "EXIF Number of directory entries = " << ndirs << "\n";
#endif
        for (int d = 0;  d < ndirs;  ++d)
            read_exif_tag (spec, (const TIFFDirEntry *)(ifd+2+d*sizeof(TIFFDirEntry)),
                           buf, swab, ifd_offsets_seen,
                           dir.tdir_tag == TIFFTAG_EXIFIFD ? exif_tagmap : gps_tagmap);
#if DEBUG_EXIF_READ
        std::cerr << "> End EXIF\n";
#endif
    } else if (dir.tdir_tag == TIFFTAG_INTEROPERABILITYIFD) {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&offset);
        // Don't recurse if we've already visited this IFD
        if (ifd_offsets_seen.find (offset) != ifd_offsets_seen.end())
            return;
        ifd_offsets_seen.insert (offset);
#if DEBUG_EXIF_READ
        std::cerr << "Now we've seen offset " << offset << "\n";
#endif
        const unsigned char *ifd = ((const unsigned char *)buf.data() + offset);
        unsigned short ndirs = *(const unsigned short *)ifd;
        if (swab)
            swap_endian (&ndirs);
#if DEBUG_EXIF_READ
        std::cerr << "\n\nInteroperability has type " << dir.tdir_type << ", offset " << dir.tdir_offset << "\n";
        std::cerr << "Interoperability Number of directory entries = " << ndirs << "\n";
#endif
        for (int d = 0;  d < ndirs;  ++d)
            read_exif_tag (spec, (const TIFFDirEntry *)(ifd+2+d*sizeof(TIFFDirEntry)),
                           buf, swab, ifd_offsets_seen, exif_tagmap);
#if DEBUG_EXIF_READ
        std::cerr << "> End Interoperability\n\n";
#endif
    } else {
        // Everything else -- use our table to handle the general case
        const char *name = tagmap.name (dir.tdir_tag);
        if (name) {
            add_exif_item_to_spec (spec, name, &dir, buf, swab);
        } else {
#if DEBUG_EXIF_READ
            std::cerr << "Dir : tag=" << dir.tdir_tag
                      << ", type=" << dir.tdir_type
                      << ", count=" << dir.tdir_count
                      << ", offset=" << dir.tdir_offset << "\n";
#endif
        }
    }
}



class tagcompare {
public:
    int operator() (const TIFFDirEntry &a, const TIFFDirEntry &b) {
        return (a.tdir_tag < b.tdir_tag);
    }
};




static void
append_dir_entry (const TagMap &tagmap,
                  std::vector<TIFFDirEntry> &dirs, std::vector<char> &data,
                  int tag, TIFFDataType type, size_t count, const void *mydata)
{
    TIFFDirEntry dir;
    dir.tdir_tag = tag;
    dir.tdir_type = type;
    dir.tdir_count = count;
    size_t len = tiff_data_sizes[(int)type] * count;
    if (len <= 4) {
        dir.tdir_offset = 0;
        memcpy (&dir.tdir_offset, mydata, len);
    } else {
        dir.tdir_offset = data.size();
        data.insert (data.end(), (char *)mydata, (char *)mydata + len);
    }
#if DEBUG_EXIF_WRITE
    std::cerr << "Adding ";
    print_dir_entry (tagmap, dir, string_view((const char *)mydata, len));
#endif
    // Don't double-add
    for (TIFFDirEntry &d : dirs) {
        if (d.tdir_tag == tag) {
            d = dir;
            return;
        }
    }
    dirs.push_back (dir);
}



/// Convert to the desired integer type and then append_dir_entry it.
///
template <class T>
bool
append_dir_entry_integer (const ParamValue &p, const TagMap &tagmap,
                          std::vector<TIFFDirEntry> &dirs,
                          std::vector<char> &data, int tag, TIFFDataType type)
{
    T i;
    switch (p.type().basetype) {
    case TypeDesc::UINT:
        i = (T) *(unsigned int *)p.data();
        break;
    case TypeDesc::INT:
        i = (T) *(int *)p.data();
        break;
    case TypeDesc::UINT16:
        i = (T) *(unsigned short *)p.data();
        break;
    case TypeDesc::INT16:
        i = (T) *(short *)p.data();
        break;
    default:
        return false;
    }
    append_dir_entry (tagmap, dirs, data, tag, type, 1, &i);
    return true;
}



/// Helper: For param that needs to be added as a tag, create a TIFF
/// directory entry for it in dirs and add its data in data.  Set the
/// directory's offset just to the position within data where it will
/// reside.  Don't worry about it being relative to the start of some
/// TIFF structure.
static void
encode_exif_entry (const ParamValue &p, int tag,
                   std::vector<TIFFDirEntry> &dirs,
                   std::vector<char> &data,
                   const TagMap &tagmap)
{
    TIFFDataType type = tagmap.tifftype (tag);
    size_t count = (size_t) tagmap.tiffcount (tag);
    TypeDesc element = p.type().elementtype();

    switch (type) {
    case TIFF_ASCII :
        if (p.type() == TypeDesc::STRING) {
            const char *s = *(const char **) p.data();
            int len = strlen(s) + 1;
            append_dir_entry (tagmap, dirs, data, tag, type, len, s);
            return;
        }
        break;
    case TIFF_RATIONAL :
        if (element == TypeDesc::FLOAT) {
            unsigned int *rat = (unsigned int *) alloca (2*count*sizeof(unsigned int));
            const float *f = (const float *)p.data();
            for (size_t i = 0;  i < count;  ++i)
                float_to_rational (f[i], rat[2*i], rat[2*i+1]);
            append_dir_entry (tagmap, dirs, data, tag, type, count, rat);
            return;
        }
        break;
    case TIFF_SRATIONAL :
        if (element == TypeDesc::FLOAT) {
            int *rat = (int *) alloca (2*count*sizeof(int));
            const float *f = (const float *)p.data();
            for (size_t i = 0;  i < count;  ++i)
                float_to_rational (f[i], rat[2*i], rat[2*i+1]);
            append_dir_entry (tagmap, dirs, data, tag, type, count, rat);
            return;
        }
        break;
    case TIFF_SHORT :
        if (append_dir_entry_integer<unsigned short> (p, tagmap, dirs, data, tag, type))
            return;
        break;
    case TIFF_LONG :
        if (append_dir_entry_integer<unsigned int> (p, tagmap, dirs, data, tag, type))
            return;
        break;
    case TIFF_BYTE :
        if (append_dir_entry_integer<unsigned char> (p, tagmap, dirs, data, tag, type))
            return;
        break;
    default:
        break;
    }
#if DEBUG_EXIF_WRITE
    std::cerr << "  Don't know how to add " << p.name() << ", tag " << tag << ", type " << type << ' ' << p.type().c_str() << "\n";
#endif
}



/// Given a list of directory entries, add 'offset' to their tdir_offset
/// fields (unless, of course, they are less than 4 bytes of data and are
/// therefore stored locally rather than having an offset at all).
static void
reoffset (std::vector<TIFFDirEntry> &dirs, const TagMap &tagmap,
          size_t offset)
{
    for (TIFFDirEntry &dir : dirs) {
        if (tiff_data_size (dir) <= 4 &&
            dir.tdir_tag != TIFFTAG_EXIFIFD && dir.tdir_tag != TIFFTAG_GPSIFD) {
#if DEBUG_EXIF_WRITE
            const char *name = tagmap.name (dir.tdir_tag);
            std::cerr << "    NO re-offset of exif entry " << " tag " << dir.tdir_tag << " " << (name ? name : "") << " to " << dir.tdir_offset << '\n';
#endif
            continue;
        }
        dir.tdir_offset += offset;
#if DEBUG_EXIF_WRITE
        const char *name = tagmap.name (dir.tdir_tag);
        std::cerr << "    re-offsetting entry " << " tag " << dir.tdir_tag << " " << (name ? name : "") << " to " << dir.tdir_offset << '\n';
#endif
    }
}


}  // anon namespace


// DEPRECATED (1.8)
bool
decode_exif (const void *exif, int length, ImageSpec &spec)
{
    return decode_exif (string_view ((const char *)exif, length), spec);
}



// Decode a raw Exif data block and save all the metadata in an
// ImageSpec.  Return true if all is ok, false if the exif block was
// somehow malformed.
bool
decode_exif (string_view exif, ImageSpec &spec)
{
    TagMap& exif_tagmap (exif_tagmap_ref());

#if DEBUG_EXIF_READ
    std::cerr << "Exif dump:\n";
    for (size_t i = 0;  i < exif.size();  ++i) {
        if (exif[i] >= ' ')
            std::cerr << (char)exif[i] << ' ';
        std::cerr << "(" << (int)(unsigned char)exif[i] << ") ";
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
    TIFFHeader head = *(const TIFFHeader *)exif.data();
    if (head.tiff_magic != 0x4949 && head.tiff_magic != 0x4d4d)
        return false;
    bool host_little = littleendian();
    bool file_little = (head.tiff_magic == 0x4949);
    bool swab = (host_little != file_little);
    if (swab)
        swap_endian (&head.tiff_diroff);

    // keep track of IFD offsets we've already seen to avoid infinite
    // recursion if there are circular references.
    std::set<size_t> ifd_offsets_seen;

    // Read the directory that the header pointed to.  It should contain
    // some number of directory entries containing tags to process.
    const unsigned char *ifd = ((const unsigned char *)exif.data() + head.tiff_diroff);
    unsigned short ndirs = *(const unsigned short *)ifd;
    if (swab)
        swap_endian (&ndirs);
    for (int d = 0;  d < ndirs;  ++d)
        read_exif_tag (spec, (const TIFFDirEntry *) (ifd+2+d*sizeof(TIFFDirEntry)),
                       exif, swab, ifd_offsets_seen, exif_tagmap);

    // A few tidbits to look for
    ParamValue *p;
    if ((p = spec.find_attribute ("Exif:ColorSpace")) ||
        (p = spec.find_attribute ("ColorSpace"))) {
        int cs = -1;
        if (p->type() == TypeDesc::UINT) 
            cs = *(const unsigned int *)p->data();
        else if (p->type() == TypeDesc::INT) 
            cs = *(const int *)p->data();
        else if (p->type() == TypeDesc::UINT16) 
            cs = *(const unsigned short *)p->data();
        else if (p->type() == TypeDesc::INT16) 
            cs = *(const short *)p->data();
        // Exif spec says that anything other than 0xffff==uncalibrated
        // should be interpreted to be sRGB.
        if (cs != 0xffff)
            spec.attribute ("oiio:ColorSpace", "sRGB");
    }
    return true;
}



// Construct an Exif data block from the ImageSpec, appending the Exif 
// data as a big blob to the char vector.
void
encode_exif (const ImageSpec &spec, std::vector<char> &blob)
{
    TagMap& exif_tagmap (exif_tagmap_ref());
    TagMap& gps_tagmap (gps_tagmap_ref());

    // Reserve maximum space that an APP1 can take in a JPEG file, so
    // we can push_back to our heart's content and know that no offsets
    // or pointers to the exif vector's memory will change due to
    // reallocation.
    blob.reserve (0xffff);

    // Layout:
    //    (tiffstart)      TIFFHeader
    //                     number of top dir entries 
    //                     top dir entry 0
    //                     ...
    //                     top dir entry (point to Exif IFD)
    //                     data for top dir entries (except Exif)
    //
    //                     Exif IFD number of dir entries (n)
    //                     Exif IFD entry 0
    //                     ...
    //                     Exif IFD entry n-1
    //                     ...More Data for Exif entries...

    // Here is where the TIFF info starts.  All TIFF tag offsets are
    // relative to this position within the blob.
    int tiffstart = blob.size();

    // Handy macro -- grow the blob to accommodate a new variable, which
    // we position at its end.
#define BLOB_ADD(vartype, varname)                          \
    blob.resize (blob.size() + sizeof(vartype));            \
    vartype & varname (* (vartype *) (&blob[0] + blob.size() - sizeof(vartype)));
    
    // Put a TIFF header
    BLOB_ADD (TIFFHeader, head);
    bool host_little = littleendian();
    head.tiff_magic = host_little ? 0x4949 : 0x4d4d;
    head.tiff_version = 42;
    head.tiff_diroff = blob.size() - tiffstart;

    // Placeholder for number of directories
    BLOB_ADD (unsigned short, ndirs);
    ndirs = 0;

    // Accumulate separate tag directories for TIFF, Exif, GPS, and Interop.
    std::vector<TIFFDirEntry> tiffdirs, exifdirs, gpsdirs, interopdirs;
    std::vector<char> data;    // Put data here
    int endmarker = 0;  // 4 bytes of 0's that marks the end of a directory

    // Go through all spec attribs, add them to the appropriate tag
    // directory (tiff, gps, or exif).
    for (const ParamValue &p : spec.extra_attribs) {
        // Which tag domain are we using?
        if (! strncmp (p.name().c_str(), "GPS:", 4)) {
            // GPS
            int tag = gps_tagmap.tag (p.name().string());
            encode_exif_entry (p, tag, gpsdirs, data, gps_tagmap);
        } else {
            // Not GPS
            int tag = exif_tagmap.tag (p.name().string());
            if (tag < EXIFTAG_EXPOSURETIME || tag > EXIFTAG_IMAGEUNIQUEID) {
                encode_exif_entry (p, tag, tiffdirs, data, exif_tagmap);
            } else {
                encode_exif_entry (p, tag, exifdirs, data, exif_tagmap);
            }
        }
    }

#if DEBUG_EXIF_WRITE
    std::cerr << "Blob header size " << blob.size() << "\n";
    std::cerr << "tiff tags: " << tiffdirs.size() << "\n";
    std::cerr << "exif tags: " << exifdirs.size() << "\n";
    std::cerr << "gps tags: " << gpsdirs.size() << "\n";
#endif

    // If any legit Exif info was found...
    if (exifdirs.size()) {
        // Add some required Exif tags that wouldn't be in the spec
        append_dir_entry (exif_tagmap, exifdirs, data, 
                          EXIFTAG_EXIFVERSION, TIFF_UNDEFINED, 4, "0220");
        append_dir_entry (exif_tagmap, exifdirs, data, 
                          EXIFTAG_FLASHPIXVERSION, TIFF_UNDEFINED, 4, "0100");
        char componentsconfig[] = { 1, 2, 3, 0 };
        append_dir_entry (exif_tagmap, exifdirs, data, 
                          EXIFTAG_COMPONENTSCONFIGURATION, TIFF_UNDEFINED, 4, componentsconfig);
        // Sort the exif tag directory
        std::sort (exifdirs.begin(), exifdirs.end(), tagcompare());

        // If we had exif info, add one more main dir entry to point to
        // the private exif tag directory.
        unsigned int size = (unsigned int) data.size();
        append_dir_entry (exif_tagmap, tiffdirs, data, TIFFTAG_EXIFIFD, TIFF_LONG, 1, &size);

        // Create interop directory boilerplate.
        // In all honesty, I have no idea what this is all about.
        append_dir_entry (exif_tagmap, interopdirs, data, 1, TIFF_ASCII, 4, "R98");
        append_dir_entry (exif_tagmap, interopdirs, data, 2, TIFF_UNDEFINED, 4, "0100");
        std::sort (interopdirs.begin(), interopdirs.end(), tagcompare());

#if 0
        // FIXME -- is this necessary?  If so, it's not completed.
        // Add the interop directory IFD entry to the main IFD
        size = (unsigned int) data.size();
        append_dir_entry (exif_tagmap, tiffdirs, data,
                          TIFFTAG_INTEROPERABILITYIFD, TIFF_LONG, 1, &size);
        std::sort (tiffdirs.begin(), tiffdirs.end(), tagcompare());
#endif
    }

    // If any GPS info was found...
    if (gpsdirs.size()) {
        // Add some required Exif tags that wouldn't be in the spec
        static char ver[] = { 2, 2, 0, 0 };
        append_dir_entry (gps_tagmap, gpsdirs, data,
                          GPSTAG_VERSIONID, TIFF_BYTE, 4, &ver);
        // Sort the gps tag directory
        std::sort (gpsdirs.begin(), gpsdirs.end(), tagcompare());

        // If we had gps info, add one more main dir entry to point to
        // the private gps tag directory.
        unsigned int size = (unsigned int) data.size();
        if (exifdirs.size())
            size += sizeof(unsigned short) + exifdirs.size()*sizeof(TIFFDirEntry) + 4;
        append_dir_entry (exif_tagmap, tiffdirs, data, TIFFTAG_GPSIFD, TIFF_LONG, 1, &size);
    }

    // Where will the data begin (we need this to adjust the directory
    // offsets once we append data to the exif blob)?
    size_t datastart = blob.size() - tiffstart + 
                       tiffdirs.size() * sizeof(TIFFDirEntry) +
                       4 /* end marker */;

    // Adjust the TIFF offsets, add the TIFF directory entries to the main
    // Exif block, followed by 4 bytes of 0's.
    reoffset (tiffdirs, exif_tagmap, datastart);
    ndirs = tiffdirs.size();
    if (ndirs)
	    blob.insert (blob.end(), (char *)&tiffdirs[0],
                 (char *)(&tiffdirs[0] + tiffdirs.size()));
    blob.insert (blob.end(), (char *)&endmarker, (char *)&endmarker + sizeof(int));

    // If legit Exif metadata was found, adjust the Exif directory offsets,
    // append the Exif tag directory entries onto the main data block,
    // followed by 4 bytes of 0's.
    if (exifdirs.size()) {
        reoffset (exifdirs, exif_tagmap, datastart);
        unsigned short nd = exifdirs.size();
        data.insert (data.end(), (char *)&nd, (char *)&nd + sizeof(nd));
        data.insert (data.end(), (char *)&exifdirs[0], (char *)(&exifdirs[0] + exifdirs.size()));
        data.insert (data.end(), (char *)&endmarker, (char *)&endmarker + sizeof(int));
    }

    // If legit GPS metadata was found, adjust the GPS directory offsets,
    // append the GPS tag directory entries onto the main data block,
    // followed by 4 bytes of 0's.
    if (gpsdirs.size()) {
        reoffset (gpsdirs, gps_tagmap, datastart);
        unsigned short nd = gpsdirs.size();
        data.insert (data.end(), (char *)&nd, (char *)&nd + sizeof(nd));
        data.insert (data.end(), (char *)&gpsdirs[0], (char *)(&gpsdirs[0] + gpsdirs.size()));
        data.insert (data.end(), (char *)&endmarker, (char *)&endmarker + sizeof(int));
    }

    // Now append the data block onto the end of the main exif block that
    // we're returning to the caller.
    blob.insert (blob.end(), data.begin(), data.end());

#if DEBUG_EXIF_WRITE
    std::cerr << "resulting exif block is a total of " << blob.size() << "\n";
#if 0
    std::cerr << "APP1 dump:\n";
    for (char c : blob) {
        if (c >= ' ')
            std::cerr << c << ' ';
        std::cerr << "(" << (int)c << ") ";
    }
#endif
#endif
}



bool
exif_tag_lookup (string_view name, int &tag, int &tifftype, int &count)
{
    const EXIF_tag_info *e = exif_tagmap_ref().find (name);
    if (! e)
        return false;  // not found

    tag = e->tifftag;
    tifftype = e->tifftype;
    count = e->tiffcount;
    return true;
}


OIIO_NAMESPACE_END

