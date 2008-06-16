/////////////////////////////////////////////////////////////////////////////
// Copyright 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <ctype.h>
#include <cstdio>
#include <iostream>
#include <vector>
#include <map>

#include <boost/scoped_array.hpp>

extern "C" {
#include "jpeglib.h"
#include "tiff.h"
}

#include "imageio.h"
using namespace OpenImageIO;
#include "fmath.h"



// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


struct EXIF_tag_info {
    int tifftag;       // TIFF tag used for this info
    const char *name;  // Attribute name we use, or NULL to ignore the tag
    TIFFDataType tifftype;  // Data type that TIFF wants
};

static const EXIF_tag_info exif_tag_table[] = {
    // Skip ones handled by the usual JPEG code
    { TIFFTAG_IMAGEWIDTH,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_IMAGELENGTH,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_BITSPERSAMPLE,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_COMPRESSION,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_PHOTOMETRIC,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_SAMPLESPERPIXEL,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_PLANARCONFIG,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_YCBCRSUBSAMPLING,	NULL,	TIFF_NOTYPE },
    { TIFFTAG_YCBCRPOSITIONING,	NULL,	TIFF_NOTYPE },

    // TIFF tags we may come across
    { TIFFTAG_ORIENTATION,	"orientation",	TIFF_SHORT },
    { TIFFTAG_XRESOLUTION,	"xresolution",	TIFF_RATIONAL },
    { TIFFTAG_YRESOLUTION,	"yresolution",	TIFF_RATIONAL },
    { TIFFTAG_RESOLUTIONUNIT,	"resolutionunit",TIFF_SHORT },
    { TIFFTAG_IMAGEDESCRIPTION,	"description",	TIFF_ASCII },
    { TIFFTAG_MAKE,	        "make",	        TIFF_ASCII },
    { TIFFTAG_MODEL,	        "model",	TIFF_ASCII },
    { TIFFTAG_SOFTWARE,	        "software",	TIFF_ASCII },
    { TIFFTAG_ARTIST,	        "artist",	TIFF_ASCII },
    { TIFFTAG_COPYRIGHT,	"copyright",	TIFF_ASCII },
    { TIFFTAG_DATETIME,	        "datetime",	TIFF_ASCII },

    // EXIF tags we may come across
    { EXIFTAG_EXPOSURETIME,	"exposuretime",	TIFF_NOTYPE },
    { EXIFTAG_FNUMBER,	        "fstop",	TIFF_NOTYPE },  // exif_FNumber?
    { EXIFTAG_EXPOSUREPROGRAM,	"exif_ExposureProgram",	TIFF_NOTYPE }, // ?? translate to ascii names?
    { EXIFTAG_SPECTRALSENSITIVITY,	"exif_SpectralSensitivity",	TIFF_NOTYPE },
    { EXIFTAG_ISOSPEEDRATINGS,	"exif_IFOSpeedRatings",	TIFF_NOTYPE },
    { EXIFTAG_OECF,	        NULL,	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_EXIFVERSION,	NULL,	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_DATETIMEORIGINAL,	"exif_DateTimeOriginal",	TIFF_NOTYPE },
    { EXIFTAG_DATETIMEDIGITIZED,"exif_DateTimeDigitized",	TIFF_NOTYPE },
    { EXIFTAG_COMPONENTSCONFIGURATION,	"exif_ComponentsConfiguration",	TIFF_NOTYPE },
    { EXIFTAG_COMPRESSEDBITSPERPIXEL,	"exif_CompressedBitsPerPixel",	TIFF_NOTYPE },
    { EXIFTAG_SHUTTERSPEEDVALUE,"exif_ShutterSpeedValue",	TIFF_NOTYPE }, // APEX units
    { EXIFTAG_APERTUREVALUE,	"exif_ApertureValue",	TIFF_NOTYPE },	// APEX units
    { EXIFTAG_BRIGHTNESSVALUE,	"exif_BrightnessValue",	TIFF_NOTYPE },
    { EXIFTAG_EXPOSUREBIASVALUE,"exif_ExposureBiasValue",	TIFF_NOTYPE },
    { EXIFTAG_MAXAPERTUREVALUE,	"exif_MaxApertureValue",	TIFF_NOTYPE },
    { EXIFTAG_SUBJECTDISTANCE,	"exif_SubjectDistance",	TIFF_NOTYPE },
    { EXIFTAG_METERINGMODE,	"exif_MeteringMode",	TIFF_NOTYPE },	// translate to tokens?
    { EXIFTAG_LIGHTSOURCE,	"exif_LightSource",	TIFF_NOTYPE },	// translate to tokens?
    { EXIFTAG_FLASH,	        "exif_Flash",	TIFF_NOTYPE },	 // translate to tokens?
    { EXIFTAG_FOCALLENGTH,	"focallength",	TIFF_NOTYPE },
    { EXIFTAG_SUBJECTAREA,	"exif_SubjectArea",	TIFF_NOTYPE },
    { EXIFTAG_MAKERNOTE,	NULL,	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_USERCOMMENT,	NULL,	TIFF_NOTYPE },	// skip it
    { EXIFTAG_SUBSECTIME,	"exif_SubsecTime",	TIFF_NOTYPE },
    { EXIFTAG_SUBSECTIMEORIGINAL,"exif_SubsecTimeOriginal",	TIFF_NOTYPE },
    { EXIFTAG_SUBSECTIMEDIGITIZED,"exif_SubsecTimeDigitized",	TIFF_NOTYPE },
    { EXIFTAG_FLASHPIXVERSION,	NULL,	TIFF_NOTYPE },	// skip "exif_FlashPixVesion",	TIFF_NOTYPE },
    { EXIFTAG_COLORSPACE,	"exif_ColorSpace",	TIFF_NOTYPE },
    { EXIFTAG_PIXELXDIMENSION,	"exif_PixelXDimension",	TIFF_NOTYPE },
    { EXIFTAG_PIXELYDIMENSION,	"exif_PixelTDimension",	TIFF_NOTYPE },
    { EXIFTAG_RELATEDSOUNDFILE,	NULL,	TIFF_NOTYPE },	// skip
    { EXIFTAG_FLASHENERGY,	"exif_FlashEnergy",	TIFF_NOTYPE },
    { EXIFTAG_SPATIALFREQUENCYRESPONSE,	"exif_SpatialFrequencyResponse",	TIFF_NOTYPE },
    { EXIFTAG_FOCALPLANEXRESOLUTION,	"exif_FocalPlaneXResolution",	TIFF_NOTYPE },
    { EXIFTAG_FOCALPLANEYRESOLUTION,	"exif_FocalPlaneYResolution",	TIFF_NOTYPE },
    { EXIFTAG_FOCALPLANERESOLUTIONUNIT,	"exif_FocalPlaneResolutionUnit",	TIFF_NOTYPE }, // Symbolic?
    { EXIFTAG_SUBJECTLOCATION,	"exif_SubjectLocation",	TIFF_NOTYPE },
    { EXIFTAG_EXPOSUREINDEX,	"exif_ExposureIndex",	TIFF_NOTYPE },
    { EXIFTAG_SENSINGMETHOD,	"exif_SensingMethod",	TIFF_NOTYPE },
    { EXIFTAG_FILESOURCE,	"exif_FileSource",	TIFF_NOTYPE },
    { EXIFTAG_SCENETYPE,	"exif_SceneType",	TIFF_NOTYPE },
    { EXIFTAG_CFAPATTERN,	"exif_CFAPattern",	TIFF_NOTYPE },
    { EXIFTAG_CUSTOMRENDERED,	"exif_CustomRendered",	TIFF_NOTYPE },
    { EXIFTAG_EXPOSUREMODE,	"exif_ExposureMode",	TIFF_NOTYPE },
    { EXIFTAG_WHITEBALANCE,	"exif_WhiteBalance",	TIFF_NOTYPE },
    { EXIFTAG_DIGITALZOOMRATIO,	"exif_DigitalZoomRatio",	TIFF_NOTYPE },
    { EXIFTAG_FOCALLENGTHIN35MMFILM,	"exif_FocalLengthIn35mmFilm",	TIFF_NOTYPE },
    { EXIFTAG_SCENECAPTURETYPE,	"exif_SceneCaptureType",	TIFF_NOTYPE },
    { EXIFTAG_GAINCONTROL,	"exif_GainControl",	TIFF_NOTYPE },
    { EXIFTAG_CONTRAST,	        "exif_Contrast",	TIFF_NOTYPE },
    { EXIFTAG_SATURATION,	"exif_Saturation",	TIFF_NOTYPE },
    { EXIFTAG_SHARPNESS,	"exif_Sharpness",	TIFF_NOTYPE },
    { EXIFTAG_DEVICESETTINGDESCRIPTION,	"exif_DeviceSettingDescription",	TIFF_NOTYPE },
    { EXIFTAG_SUBJECTDISTANCERANGE,	"exif_SubjectDistanceRange",	TIFF_NOTYPE },
    { EXIFTAG_IMAGEUNIQUEID,	"exif_ImageUniqueID",	TIFF_NOTYPE }
};



class TagMap {
    typedef std::map<int, const EXIF_tag_info *> tagmap_t;
    typedef std::map<std::string, const EXIF_tag_info *> namemap_t;
public:
    TagMap (void) {
        int n = sizeof(exif_tag_table)/sizeof(exif_tag_table[0]);
        for (int i = 0;  i < n;  ++i) {
            const EXIF_tag_info *eti = &exif_tag_table[i];
            m_tagmap[eti->tifftag] = eti;
            if (eti->name)
                m_namemap[eti->name] = eti;
        }
    }

    const EXIF_tag_info * find (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second;
    }

    const char * name (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second->name;
    }

    TIFFDataType tifftype (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? TIFF_NOTYPE : i->second->tifftype;
    }

    int tag (const std::string &name) const {
        namemap_t::const_iterator i = m_namemap.find (name);
        return i == m_namemap.end() ? -1 : i->second->tifftag;
    }

private:
    tagmap_t m_tagmap;
    namemap_t m_namemap;
};

static TagMap tagmap;




/// Add one EXIF directory entry's data to spec under the given 'name'.
/// The directory entry is in *dirp, buf points to the beginning of the
/// TIFF "file", i.e. all TIFF tag offsets are relative to buf.  If swab
/// is true, the endianness of the file doesn't match the endianness of
/// the host CPU, therefore all integer and float data embedded in buf
/// needs to be byte-swapped.  Note that *dirp HAS already been swapped,
/// if necessary, so no byte swapping on *dirp is necessary.
void
add_exif_item_to_spec (ImageIOFormatSpec &spec, const char *name,
                       TIFFDirEntry *dirp, const char *buf, bool swab)
{
    if (dirp->tdir_type == TIFF_SHORT && dirp->tdir_count == 1) {
        unsigned short d;
        d = * (unsigned short *) &dirp->tdir_offset;  // short stored in offset itself
        if (swab)
            swap_endian (&d);
        spec.add_parameter (name, (int)d);
    } else if (dirp->tdir_type == TIFF_LONG && dirp->tdir_count == 1) {
        unsigned int d;
        d = * (unsigned int *) &dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&d);
        spec.add_parameter (name, (int)d);
    } else if (dirp->tdir_type == TIFF_RATIONAL && dirp->tdir_count == 1) {
        unsigned int num, den;
        num = ((unsigned int *) &(buf[dirp->tdir_offset]))[0];
        den = ((unsigned int *) &(buf[dirp->tdir_offset]))[1];
        if (swab) {
            swap_endian (&num);
            swap_endian (&den);
        }
        double db = (double)num / (double)den;
        spec.add_parameter (name, (float)db);
    } else if (dirp->tdir_type == TIFF_SRATIONAL && dirp->tdir_count == 1) {
        unsigned int num, den;
        num = ((unsigned int *) &(buf[dirp->tdir_offset]))[0];
        den = ((unsigned int *) &(buf[dirp->tdir_offset]))[1];
        if (swab) {
            swap_endian (&num);
            swap_endian (&den);
        }
        double db = (double)num / (double)den;
        spec.add_parameter (name, (float)db);
    } else if (dirp->tdir_type == TIFF_ASCII) {
        spec.add_parameter (name, buf+dirp->tdir_offset);
    } else {
        std::cerr << "didn't know how to process type " 
                  << dirp->tdir_type << " x " << dirp->tdir_count << "\n";
    }
}



#if 0
static std::string
resunit_tag (TIFFDirEntry *dirp, const char *buf, bool swab)
{
    if (dirp->tdir_type != TIFF_SHORT)
        return "none";
    short s = add_exif_item_to_spec (spec, dirp, buf, swab);
    if (s == RESUNIT_INCH)
        return "in";
    else if (s == RESUNIT_CENTIMETER)
        return "cm";
    return "none";
}
#endif



/// Process a single TIFF directory entry embedded in the JPEG 'APP1'
/// data.  The directory entry is in *dirp, buf points to the beginning
/// of the TIFF "file", i.e. all TIFF tag offsets are relative to buf.
/// The goal is to decode the tag and put the data into appropriate
/// attribute slots of spec.  If swab is true, the endianness of the
/// file doesn't match the endianness of the host CPU, therefore all
/// integer and float data embedded in buf needs to be byte-swapped.
/// Note that *dirp has not been swapped, and so is still in the native
/// endianness of the file.
void
read_exif_tag (ImageIOFormatSpec &spec, TIFFDirEntry *dirp,
               const char *buf, bool swab)
{
    // Make a copy of the pointed-to TIFF directory, swab the components
    // if necessary.
    TIFFDirEntry dir = *dirp;
    if (swab) {
        swap_endian (&dir.tdir_tag);
        swap_endian (&dir.tdir_type);
        swap_endian (&dir.tdir_count);
        swap_endian (&dir.tdir_offset);
    }

    if (dir.tdir_tag == TIFFTAG_EXIFIFD) {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&offset);
        // std::cerr << "exifid has type " << dir.tdir_type << "\n";
        unsigned char *ifd = ((unsigned char *)buf + offset);
        unsigned short ndirs = *(unsigned short *)ifd;
        if (swab)
            swap_endian (&ndirs);
        std::cerr << "EXIF Number of directory entries = " << ndirs << "\n";
        for (int d = 0;  d < ndirs;  ++d)
            read_exif_tag (spec, (TIFFDirEntry *)(ifd+2+d*sizeof(TIFFDirEntry)),
                           (char *)buf, swab);
    } else {
        // Everything else -- use our table to handle the general case
        const char *name = tagmap.name (dir.tdir_tag);
        if (name) {
            add_exif_item_to_spec (spec, name, &dir, buf, swab);
        } else {
            std::cerr << "Dir : tag=" << dir.tdir_tag
                      << ", type=" << dir.tdir_type
                      << ", count=" << dir.tdir_count
                      << ", offset=" << dir.tdir_offset << "\n";
        }
    }
}



/// Rummage through the JPEG "APP1" marker pointed to by buf, decoding
/// EXIF information and adding attributes to spec.
void
exif_from_APP1 (ImageIOFormatSpec &spec, unsigned char *buf)
{
    // APP1 blob doesn't have to be exif info.  Look for the exif marker,
    // which near as I can tell is just the letters "Exif" at the start.
    if (strncmp ((char *)buf, "Exif", 5))
        return;
    buf += 6;   // ...and two nulls follow the "Exif"

    // The next item should be a standard TIFF header.  Note that HERE,
    // not the start of the Exif blob, is where all TIFF offsets are
    // relative to.  The header should have the right magic number (which
    // also tells us the endianness of the data) and an offset to the
    // first TIFF directory.
    //
    // N.B. Just read libtiff's "tiff.h" for info on the structure 
    // layout of TIFF headers and directory entries.  The TIFF spec
    // itself is also helpful in this area.
    TIFFHeader *head = (TIFFHeader *)buf;
    if (head->tiff_magic != 0x4949 && head->tiff_magic != 0x4d4d)
        return;
    bool host_little = littleendian();
    bool file_little = (head->tiff_magic == 0x4949);
    bool swab = (host_little != file_little);
    if (swab)
        swap_endian (&head->tiff_diroff);

    // Read the directory that the header pointed to.  It should contain
    // some number of directory entries containing tags to process.
    unsigned char *ifd = (buf + head->tiff_diroff);
    unsigned short ndirs = *(unsigned short *)ifd;
    if (swab)
        swap_endian (&ndirs);
    for (int d = 0;  d < ndirs;  ++d)
        read_exif_tag (spec, (TIFFDirEntry *) (ifd+2+d*sizeof(TIFFDirEntry)),
                       (char *)buf, swab);
}



static void
float_to_rational (float fv, unsigned int &num, unsigned int &den)
{
    int sign = 1;
    if (fv < 0) {
#if 0
        if (dir->tdir_type == TIFF_RATIONAL) {
            TIFFWarningExt(tif->tif_clientdata, tif->tif_name,
                           "\"%s\": Information lost writing value (%g) as (unsigned) RATIONAL",
                           _TIFFFieldWithTag(tif,dir->tdir_tag)->field_name,
                           fv);
            fv = 0;
        } else
#endif
            fv = -fv, sign = -1;
    }
    den = 1L;
    if (fv > 0) {
        while (fv < 1L<<(31-3) && den < 1L<<(31-3))
            fv *= 1<<3, den *= 1L<<3;
    }
    num = (unsigned int) (sign * (fv + 0.5));
    std::cerr << fv << " -> " << num << "/" << den << "\n";
}



/// Take all the stuff in spec that should be expressed as EXIF tags in
/// a JPEG, and construct a huge blob of an APP1 marker in exif.
void
APP1_exif_from_spec (ImageIOFormatSpec &spec, std::vector<char> &exif)
{
    // Clear the buffer and reserve maximum space that an APP1 can take
    // in a JPEG file, so we can push_back to our heart's content and
    // know that no offsets or pointers to the exif vector's memory will
    // change due to reallocation.
    exif.clear ();
    exif.reserve (0xffff);

    // Start the exif blob with "Exif" and two nulls.  That's how it
    // always is in the JPEG files I've examined.
    exif.push_back ('E');
    exif.push_back ('x');
    exif.push_back ('i');
    exif.push_back ('f');
    exif.push_back (0);
    exif.push_back (0);

    // Here is where the TIFF info starts.  All TIFF tag offsets are
    // relative to this position within the blob.
    int tiffstart = exif.size();

    // Handy macro -- declare a variable positioned at the current end of
    // exif, and grow exif to accommodate it.
#define EXIF_ADD(vartype, varname)                          \
    vartype & varname (* (vartype *) &exif[exif.size()]);   \
    exif.resize (exif.size() + sizeof(vartype));
    
    // Put a TIFF header
    EXIF_ADD (TIFFHeader, head);
    bool host_little = littleendian();
    head.tiff_magic = host_little ? 0x4949 : 0x4d4d;
    head.tiff_version = 42;
    head.tiff_diroff = exif.size() - tiffstart;

    // Placeholder for number of directories
    EXIF_ADD (unsigned short, ndirs);
    ndirs = 0;

    EXIF_ADD (TIFFDirEntry, dir);
    ++ndirs;
    dir.tdir_tag = TIFFTAG_EXIFIFD;
    dir.tdir_type = TIFF_IFD;
    dir.tdir_count = 1;
    dir.tdir_offset = exif.size() - tiffstart; // placeholder -- should point to
                                     //    where the data will live

    std::vector<TIFFDirEntry> dirs;
    std::vector<char> data;    // Put data here
    typedef std::pair<int, int> fixup_t;
    std::vector<fixup_t> offset_fixups;

    // Add TIFF, non-EXIF info here, then go back
    // ...
    std::cerr << "Non-exif tags\n";
    for (size_t i = 0;  i < spec.extra_params.size();  ++i) {
        const ImageIOParameter &p (spec.extra_params[i]);
        int tag = tagmap.tag (p.name);
        TIFFDataType type = tagmap.tifftype (tag);
        if (tag >= EXIFTAG_EXPOSURETIME && tag <= EXIFTAG_IMAGEUNIQUEID)
            continue;
        std::cerr << "  Do I add " << spec.extra_params[i].name << ' ' << tag << "\n";
        switch (type) {
        case TIFF_ASCII :
            if (p.type == PT_STRING) {
                const char *s = (const char *) p.data();
                int len = strlen(s) + 1;
                TIFFDirEntry dir;
                dir.tdir_tag = tag;
                dir.tdir_type = TIFF_ASCII;
                dir.tdir_count = len;
                dir.tdir_offset = data.size();  // Don't forget to correct
                dirs.push_back (dir);
                data.insert (data.end(), s, s+len+1);
            }
            break;
        default:
            std::cerr << "  Don't know how to add " << spec.extra_params[i].name << ' ' << tag << "\n";
            break;
            
        }
    }

    for (size_t i = 0;  i < dirs.size();  ++i) {
    }

#if 0
    // Here's the EXIF tag info
    EXIF_ADD (unsigned short, nsubdirs);
    nsubdirs = 0;

    std::cerr << "exif tags\n";
    for (size_t i = 0;  i < spec.extra_params.size();  ++i) {
        int tag = tagmap.tag (spec.extra_params[i].name);
        if (tag < EXIFTAG_EXPOSURETIME || tag > EXIFTAG_IMAGEUNIQUEID)
            continue;
        std::cerr << "  Do I add " << spec.extra_params[i].name << ' ' << tag << "\n";
    }
#if 0
    TIFFDirEntry *dir = (TIFFDirEntry *) &exif[exif.size()];
    exif.resize (exif.size() + sizeof(*dir));
    dir->tdir_tag = TIFFTAG_EXIFIFD;
    dir->tdir_type = TIFFTAG_IFD;  // ?? right?
    dir->tdir_count = 1;
    dir->tdir_offset = exif.size();
#endif

    // Append the data and fix up offsets
    int size = exif.size() - tiffstart;
    exif.insert (exif.end(), data.begin(), data.end());
    for (size_t i = 0;  i < offset_fixups.size();  ++i) {
        std::pair<int,int> &fixup (offset_fixups[i]);
        std::cerr << "fixup " << fixup.first << " to " << fixup.second << "\n";
        *(int *)&exif[fixup.first] = fixup.second + size;
    }
#endif

    exif.clear ();
}
