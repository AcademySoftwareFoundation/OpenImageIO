/*
  Copyright 2017 Larry Gritz et al. All Rights Reserved.

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

/////////////////////////////////////////////////////////////////////////////
/// \file
///
/// Utilities for dealing with TIFF tags and data structures (common to
/// plugins that have to deal with TIFF itself, Exif data blocks, and other
/// miscellaneous stuff that piggy-backs off TIFF format).
///
/////////////////////////////////////////////////////////////////////////////


#pragma once

#ifndef OIIO_TIFFUTILS_H
#define OIIO_TIFFUTILS_H

extern "C" {
#include "tiff.h"
}

#include <OpenImageIO/imageio.h>


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




OIIO_NAMESPACE_BEGIN

// Define EXIF constants
enum TIFFTAG {
    EXIF_EXPOSURETIME               = 33434,
    EXIF_FNUMBER                    = 33437,
    EXIF_EXPOSUREPROGRAM            = 34850,
    EXIF_SPECTRALSENSITIVITY        = 34852,
    EXIF_PHOTOGRAPHICSENSITIVITY    = 34855,
    EXIF_ISOSPEEDRATINGS            = 34855, // old nme for PHOTOGRAPHICSENSITIVITY
    EXIF_OECF                       = 34856,
    EXIF_SENSITIVITYTYPE            = 34864,
    EXIF_STANDARDOUTPUTSENSITIVITY  = 34865,
    EXIF_RECOMMENDEDEXPOSUREINDEX   = 34866,
    EXIF_ISOSPEED                   = 34867,
    EXIF_ISOSPEEDLATITUDEYYY        = 34868,
    EXIF_ISOSPEEDLATITUDEZZZ        = 34869,
    EXIF_EXIFVERSION                = 36864,
    EXIF_DATETIMEORIGINAL           = 36867,
    EXIF_DATETIMEDIGITIZED          = 36868,
    EXIF_OFFSETTIME                 = 36880,
    EXIF_OFFSETTIMEORIGINAL         = 36881,
    EXIF_OFFSETTIMEDIGITIZED        = 36882,
    EXIF_COMPONENTSCONFIGURATION    = 37121,
    EXIF_COMPRESSEDBITSPERPIXEL     = 37122,
    EXIF_SHUTTERSPEEDVALUE          = 37377,
    EXIF_APERTUREVALUE              = 37378,
    EXIF_BRIGHTNESSVALUE            = 37379,
    EXIF_EXPOSUREBIASVALUE          = 37380,
    EXIF_MAXAPERTUREVALUE           = 37381,
    EXIF_SUBJECTDISTANCE            = 37382,
    EXIF_METERINGMODE               = 37383,
    EXIF_LIGHTSOURCE                = 37384,
    EXIF_FLASH                      = 37385,
    EXIF_FOCALLENGTH                = 37386,
    EXIF_SECURITYCLASSIFICATION     = 37394,
    EXIF_IMAGEHISTORY               = 37395,
    EXIF_SUBJECTAREA                = 37396,
    EXIF_MAKERNOTE                  = 37500,
    EXIF_USERCOMMENT                = 37510,
    EXIF_SUBSECTIME                 = 37520,
    EXIF_SUBSECTIMEORIGINAL         = 37521,
    EXIF_SUBSECTIMEDIGITIZED        = 37522,
    EXIF_TEMPERATURE                = 37888,
    EXIF_HUMIDITY                   = 37889,
    EXIF_PRESSURE                   = 37890,
    EXIF_WATERDEPTH                 = 37891,
    EXIF_ACCELERATION               = 37892,
    EXIF_CAMERAELEVATIONANGLE       = 37893,
    EXIF_FLASHPIXVERSION            = 40960,
    EXIF_COLORSPACE                 = 40961,
    EXIF_PIXELXDIMENSION            = 40962,
    EXIF_PIXELYDIMENSION            = 40963,
    EXIF_RELATEDSOUNDFILE           = 40964,
    EXIF_FLASHENERGY                = 41483,
    EXIF_SPATIALFREQUENCYRESPONSE   = 41484,
    EXIF_FOCALPLANEXRESOLUTION      = 41486,
    EXIF_FOCALPLANEYRESOLUTION      = 41487,
    EXIF_FOCALPLANERESOLUTIONUNIT   = 41488,
    EXIF_SUBJECTLOCATION            = 41492,
    EXIF_EXPOSUREINDEX              = 41493,
    EXIF_SENSINGMETHOD              = 41495,
    EXIF_FILESOURCE                 = 41728,
    EXIF_SCENETYPE                  = 41729,
    EXIF_CFAPATTERN                 = 41730,
    EXIF_CUSTOMRENDERED             = 41985,
    EXIF_EXPOSUREMODE               = 41986,
    EXIF_WHITEBALANCE               = 41987,
    EXIF_DIGITALZOOMRATIO           = 41988,
    EXIF_FOCALLENGTHIN35MMFILM      = 41989,
    EXIF_SCENECAPTURETYPE           = 41990,
    EXIF_GAINCONTROL                = 41991,
    EXIF_CONTRAST                   = 41992,
    EXIF_SATURATION                 = 41993,
    EXIF_SHARPNESS                  = 41994,
    EXIF_DEVICESETTINGDESCRIPTION   = 41995,
    EXIF_SUBJECTDISTANCERANGE       = 41996,
    EXIF_IMAGEUNIQUEID              = 42016,
    EXIF_CAMERAOWNERNAME            = 42032,
    EXIF_BODYSERIALNUMBER           = 42033,
    EXIF_LENSSPECIFICATION          = 42034,
    EXIF_LENSMAKE                   = 42035,
    EXIF_LENSMODEL                  = 42036,
    EXIF_LENSSERIALNUMBER           = 42037,
    EXIF_GAMMA                      = 42240,
};



/// Given a TIFF data type code (defined in tiff.h) and a count, return the
/// equivalent TypeDesc where one exists. .Return TypeUndefined if there
/// is no obvious equivalent.
OIIO_API TypeDesc tiff_datatype_to_typedesc (TIFFDataType tifftype, size_t tiffcount=1);

inline TypeDesc tiff_datatype_to_typedesc (const TIFFDirEntry& dir) {
    return tiff_datatype_to_typedesc (TIFFDataType(dir.tdir_type), dir.tdir_count);
}

/// Return the data size (in bytes) of the TIFF type.
OIIO_API size_t tiff_data_size (TIFFDataType tifftype);

/// Return the data size (in bytes) of the data for the given TIFFDirEntry.
OIIO_API size_t tiff_data_size (const TIFFDirEntry &dir);

/// Given a TIFFDirEntry and a data arena (represented by an array view
/// of unsigned bytes), return an array_view of where the values for the
/// tiff dir lives. Return an empty array_view if there is an error, which
/// could include a nonsensical situation where the TIFFDirEntry seems to
/// point outside the data arena.
OIIO_API array_view<const uint8_t>
tiff_dir_data (const TIFFDirEntry &td, array_view<const uint8_t> data);

/// Decode a raw Exif data block and save all the metadata in an
/// ImageSpec.  Return true if all is ok, false if the exif block was
/// somehow malformed.  The binary data pointed to by 'exif' should
/// start with a TIFF directory header.
OIIO_API bool decode_exif (array_view<const uint8_t> exif, ImageSpec &spec);
OIIO_API bool decode_exif (string_view exif, ImageSpec &spec);
OIIO_API bool decode_exif (const void *exif, int length, ImageSpec &spec); // DEPRECATED (1.8)

/// Construct an Exif data block from the ImageSpec, appending the Exif 
/// data as a big blob to the char vector.
OIIO_API void encode_exif (const ImageSpec &spec, std::vector<char> &blob);

/// Helper: For the given OIIO metadata attribute name, look up the Exif tag
/// ID, TIFFDataType (expressed as an int), and count. Return true and fill
/// in the fields if found, return false if not found.
OIIO_API bool exif_tag_lookup (string_view name, int &tag,
                               int &tifftype, int &count);

/// Add metadata to spec based on raw IPTC (International Press
/// Telecommunications Council) metadata in the form of an IIM
/// (Information Interchange Model).  Return true if all is ok, false if
/// the iptc block was somehow malformed.  This is a utility function to
/// make it easy for multiple format plugins to support embedding IPTC
/// metadata without having to duplicate functionality within each
/// plugin.  Note that IIM is actually considered obsolete and is
/// replaced by an XML scheme called XMP.
OIIO_API bool decode_iptc_iim (const void *iptc, int length, ImageSpec &spec);

/// Find all the IPTC-amenable metadata in spec and assemble it into an
/// IIM data block in iptc.  This is a utility function to make it easy
/// for multiple format plugins to support embedding IPTC metadata
/// without having to duplicate functionality within each plugin.  Note
/// that IIM is actually considered obsolete and is replaced by an XML
/// scheme called XMP.
OIIO_API void encode_iptc_iim (const ImageSpec &spec, std::vector<char> &iptc);

/// Add metadata to spec based on XMP data in an XML block.  Return true
/// if all is ok, false if the xml was somehow malformed.  This is a
/// utility function to make it easy for multiple format plugins to
/// support embedding XMP metadata without having to duplicate
/// functionality within each plugin.
OIIO_API bool decode_xmp (const std::string &xml, ImageSpec &spec);

/// Find all the relavant metadata (IPTC, Exif, etc.) in spec and
/// assemble it into an XMP XML string.  This is a utility function to
/// make it easy for multiple format plugins to support embedding XMP
/// metadata without having to duplicate functionality within each
/// plugin.  If 'minimal' is true, then don't encode things that would
/// be part of ordinary TIFF or exif tags.
OIIO_API std::string encode_xmp (const ImageSpec &spec, bool minimal=false);


/// Handy structure to hold information mapping TIFF/EXIF tags to their
/// names and actions.
struct TagInfo {
    typedef void (*HandlerFunc)(const TagInfo& taginfo, const TIFFDirEntry& dir,
                                array_view<const uint8_t> buf, ImageSpec& spec,
                                bool swapendian, int offset_adjustment);

    TagInfo (int tag, const char *name, TIFFDataType type,
             int count, HandlerFunc handler = nullptr)
        : tifftag(tag), name(name), tifftype(type), tiffcount(count),
          handler(handler) {}

    int tifftag = -1;                     // TIFF tag used for this info
    const char *name = nullptr;           // OIIO attribute name we use
    TIFFDataType tifftype = TIFF_NOTYPE;  // Data type that TIFF wants
    int tiffcount = 0;                    // Number of items
    HandlerFunc handler = nullptr;        // Special decoding handler
};

/// Return an array_view of a TagInfo array for the corresponding table.
/// Valid names are "Exif", "GPS", and "TIFF". This can be handy for
/// iterating through all possible tags for each category.
OIIO_API array_view<const TagInfo> tag_table (string_view tablename);

/// Look up the TagInfo of a numbered or named tag from a named domain
/// ("TIFF", "Exif", or "GPS"). Return nullptr if it is not known.
OIIO_API const TagInfo* tag_lookup (string_view domain, int tag);
OIIO_API const TagInfo* tag_lookup (string_view domain, string_view tagname);



OIIO_NAMESPACE_END

#endif  // OIIO_TIFFUTILS_H
