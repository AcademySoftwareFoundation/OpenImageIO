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

#include <tiffio.h>

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "fmath.h"


using namespace OpenImageIO;


// Helper struct for constructing tables of TIFF tags
struct TIFF_tag_info {
    int tifftag;       // TIFF tag used for this info
    const char *name;  // Attribute name we use, or NULL to ignore the tag
    TIFFDataType tifftype;  // Data type that TIFF wants
};



class TIFFInput : public ImageInput {
public:
    TIFFInput () { init(); }
    virtual ~TIFFInput () { close(); }
    virtual const char * format_name (void) const { return "tiff"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    TIFF *m_tif;                     ///< libtiff handle
    std::string m_filename;          ///< Stash the filename
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use
    int m_subimage;                  ///< What subimage are we looking at?
    unsigned short m_planarconfig;   ///< Planar config of the file
    unsigned short m_bitspersample;  ///< Of the *file*, not the client's view
    unsigned short m_photometric;    ///< Of the *file*, not the client's view
    std::vector<unsigned short> m_colormap;  ///< Color map for palette images

    // Reset everything to initial state
    void init () {
        m_tif = NULL;
        m_subimage = -1;
        m_colormap.clear();
    }

    void close_tif () {
        if (m_tif) {
            TIFFClose (m_tif);
            m_tif = NULL;
        }
    }

    // Read tags from the current directory of m_tif and fill out spec
    void readspec ();

    // Convert planar separate to contiguous data format
    void separate_to_contig (int n, const unsigned char *separate,
                             unsigned char *contig);

    // Convert palette to RGB
    void palette_to_rgb (int n, const unsigned char *palettepels,
                         unsigned char *rgb);

    // Convert nbits (1, 2, 4) bits to 8 bit
    void nbit_to_8bit (int n, const unsigned char *bits,
                       unsigned char *bytes, int nbits);

    void invert_photometric (int n, void *data);

    // Calling TIFFGetField (tif, tag, &dest) is supposed to work fine for
    // simple types... as long as the tag types in the file are the correct
    // advertised types.  But for some types -- which we never expect, but
    // it turns out can sometimes happen, TIFFGetField will try to pull
    // a second argument (a void**) off the stack, and that can crash the
    // program!  Ick.  So to avoid this, we always push a pointer, which
    // we expect NOT to be altered, and if it is, it's a danger sign (plus
    // we didn't crash).
    bool safe_tiffgetfield (const std::string &name, int tag, void *dest) {
        void *ptr = NULL;  // dummy -- expect it to stay NULL
        bool ok = TIFFGetField (m_tif, tag, dest, &ptr);
        if (ptr) {
#ifdef DEBUG
            std::cerr << "Error safe_tiffgetfield : did not expect ptr set on "
                      << name << " " << (void *)ptr << "\n";
#endif
//            return false;
        }
        return ok;
    }

    // Get a string tiff tag field and put it into extra_params
    void get_string_attribute (const std::string &name, int tag) {
        char *s = NULL;
        if (safe_tiffgetfield (name, tag, &s))
            if (s && *s)
                m_spec.attribute (name, s);
    }

    // Get a matrix tiff tag field and put it into extra_params
    void get_matrix_attribute (const std::string &name, int tag) {
        float *f = NULL;
        if (safe_tiffgetfield (name, tag, &f) && f)
            m_spec.attribute (name, TypeDesc::PT_MATRIX, f);
    }

    // Get a float tiff tag field and put it into extra_params
    void get_float_attribute (const std::string &name, int tag) {
        float f[16];
        if (safe_tiffgetfield (name, tag, f))
            m_spec.attribute (name, f[0]);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_attribute (const std::string &name, int tag) {
        int i;
        if (safe_tiffgetfield (name, tag, &i))
            m_spec.attribute (name, i);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_attribute (const std::string &name, int tag) {
        // Make room for two shorts, in case the tag is not the type we
        // expect, and libtiff writes a long instead.
        unsigned short s[2] = {0,0};
        if (safe_tiffgetfield (name, tag, &s)) {
            int i = s[0];
            m_spec.attribute (name, i);
        }
    }

    // Search for TIFF tag 'tagid' having type 'tifftype', and if found,
    // add it in the obvious way to m_spec under the name 'oiioname'.
    void find_tag (int tifftag, TIFFDataType tifftype, const char *oiioname) {
        const TIFFFieldInfo *info = TIFFFieldWithTag (m_tif, tifftag);
        if (info && info->field_type != tifftype) {
            // Something has gone wrong, libtiff doesn't think the field type
            // is the same as we do.
            // std::cerr << "Wow, " << oiioname << " " << info->field_type 
            //           << " versus " << tifftype << "\n";
            return;
        }
        if (tifftype == TIFF_ASCII)
            get_string_attribute (oiioname, tifftag);
        else if (tifftype == TIFF_SHORT)
            get_short_attribute (oiioname, tifftag);
        else if (tifftype == TIFF_LONG)
            get_int_attribute (oiioname, tifftag);
        else if (tifftype == TIFF_RATIONAL || tifftype == TIFF_SRATIONAL ||
                 tifftype == TIFF_FLOAT || tifftype == TIFF_DOUBLE)
            get_float_attribute (oiioname, tifftag);
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageInput *tiff_input_imageio_create () { return new TIFFInput; }

// DLLEXPORT int tiff_imageio_version = OPENIMAGEIO_PLUGIN_VERSION; // it's in tiffoutput.cpp

DLLEXPORT const char * tiff_input_extensions[] = {
    "tiff", "tif", "tx", "env", "sm", "vsm", NULL
};

};



// Someplace to store an error message from the TIFF error handler
static std::string lasterr;
static mutex lasterr_mutex;


static void
my_error_handler (const char *str, const char *format, va_list ap)
{
    lock_guard lock (lasterr_mutex);
    lasterr = Strutil::vformat (format, ap);
}



bool
TIFFInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = -1;
    return seek_subimage (0, newspec);
}



bool
TIFFInput::seek_subimage (int index, ImageSpec &newspec)
{
    if (index < 0)       // Illegal
        return false;
    if (index == m_subimage) {
        // We're already pointing to the right subimage
        newspec = m_spec;
        return true;
    }

    if (! m_tif) {
        // Use our own error handler to keep libtiff from spewing to stderr
        lock_guard lock (lasterr_mutex);
        TIFFSetErrorHandler (my_error_handler);
        TIFFSetWarningHandler (my_error_handler);
    }

    if (! m_tif) {
        m_tif = TIFFOpen (m_filename.c_str(), "rm");
        if (m_tif == NULL) {
            error ("Could not open file: %s", lasterr.c_str());
            return false;
        }
        m_subimage = 0;
    }
    
    if (TIFFSetDirectory (m_tif, index)) {
        m_subimage = index;
        readspec ();
        newspec = m_spec;
        if (newspec.format == TypeDesc::UNKNOWN) {
            error ("No support for data format of \"%s\"", m_filename.c_str());
            return false;
        }
        return true;
    } else {
        error ("%s", lasterr.c_str());
        m_subimage = -1;
        return false;
    }
}



// Tags we can handle in a totally automated fasion, just copying
// straight to an ImageSpec.
static const TIFF_tag_info tiff_tag_table[] = {
    { TIFFTAG_IMAGEDESCRIPTION,	"ImageDescription", TIFF_ASCII },
    { TIFFTAG_ORIENTATION,	"Orientation",	TIFF_SHORT },
    { TIFFTAG_XRESOLUTION,	"XResolution",	TIFF_RATIONAL },
    { TIFFTAG_YRESOLUTION,	"YResolution",	TIFF_RATIONAL },
    { TIFFTAG_RESOLUTIONUNIT,	"ResolutionUnit",TIFF_SHORT },
    { TIFFTAG_MAKE,	        "Make",	        TIFF_ASCII },
    { TIFFTAG_MODEL,	        "Model",	TIFF_ASCII },
    { TIFFTAG_SOFTWARE,	        "Software",	TIFF_ASCII },
    { TIFFTAG_ARTIST,	        "Artist",	TIFF_ASCII },
    { TIFFTAG_COPYRIGHT,	"Copyright",	TIFF_ASCII },
    { TIFFTAG_DATETIME,	        "DateTime",	TIFF_ASCII },
    { TIFFTAG_DOCUMENTNAME,	"DocumentName",	TIFF_ASCII },
    { TIFFTAG_PAGENAME,         "tiff:PageName", TIFF_ASCII },
    { TIFFTAG_PAGENUMBER,       "tiff:PageNumber", TIFF_SHORT },
    { TIFFTAG_HOSTCOMPUTER,	"HostComputer",	TIFF_ASCII },
    { TIFFTAG_PIXAR_TEXTUREFORMAT, "textureformat", TIFF_ASCII },
    { TIFFTAG_PIXAR_WRAPMODES,  "wrapmodes",    TIFF_ASCII },
    { TIFFTAG_PIXAR_FOVCOT,     "fovcot",       TIFF_FLOAT },
    { 0, NULL, TIFF_NOTYPE }
};


// Tags we may come across in the EXIF directory.
static const TIFF_tag_info exif_tag_table[] = {
    { EXIFTAG_EXPOSURETIME,	"ExposureTime",	TIFF_RATIONAL },
    { EXIFTAG_FNUMBER,	        "FNumber",	TIFF_RATIONAL },
    { EXIFTAG_EXPOSUREPROGRAM,	"Exif:ExposureProgram",	TIFF_SHORT }, // ?? translate to ascii names?
    { EXIFTAG_SPECTRALSENSITIVITY,	"Exif:SpectralSensitivity",	TIFF_ASCII },
    { EXIFTAG_ISOSPEEDRATINGS,	"Exif:ISOSpeedRatings",	TIFF_SHORT },
    { EXIFTAG_OECF,	        "Exif:OECF",	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_EXIFVERSION,	"Exif:ExifVersion",	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_DATETIMEORIGINAL,	"Exif:DateTimeOriginal",	TIFF_ASCII },
    { EXIFTAG_DATETIMEDIGITIZED,"Exif:DateTimeDigitized",	TIFF_ASCII },
    { EXIFTAG_COMPONENTSCONFIGURATION,	"Exif:ComponentsConfiguration",	TIFF_UNDEFINED },
    { EXIFTAG_COMPRESSEDBITSPERPIXEL,	"Exif:CompressedBitsPerPixel",	TIFF_RATIONAL },
    { EXIFTAG_SHUTTERSPEEDVALUE,"Exif:ShutterSpeedValue",	TIFF_SRATIONAL }, // APEX units
    { EXIFTAG_APERTUREVALUE,	"Exif:ApertureValue",	TIFF_RATIONAL },	// APEX units
    { EXIFTAG_BRIGHTNESSVALUE,	"Exif:BrightnessValue",	TIFF_SRATIONAL },
    { EXIFTAG_EXPOSUREBIASVALUE,"Exif:ExposureBiasValue",	TIFF_SRATIONAL },
    { EXIFTAG_MAXAPERTUREVALUE,	"Exif:MaxApertureValue",TIFF_RATIONAL },
    { EXIFTAG_SUBJECTDISTANCE,	"Exif:SubjectDistance",	TIFF_RATIONAL },
    { EXIFTAG_METERINGMODE,	"Exif:MeteringMode",	TIFF_SHORT },
    { EXIFTAG_LIGHTSOURCE,	"Exif:LightSource",	TIFF_SHORT },
    { EXIFTAG_FLASH,	        "Exif:Flash",	        TIFF_SHORT },
    { EXIFTAG_FOCALLENGTH,	"Exif:FocalLength",	TIFF_RATIONAL }, // mm
    { EXIFTAG_SUBJECTAREA,	"Exif:SubjectArea",	TIFF_NOTYPE }, // skip
    { EXIFTAG_MAKERNOTE,	"Exif:MakerNote",	TIFF_NOTYPE },	 // skip it
    { EXIFTAG_USERCOMMENT,	"Exif:UserComment",	TIFF_NOTYPE },	// skip it
    { EXIFTAG_SUBSECTIME,	"Exif:SubsecTime",	        TIFF_ASCII },
    { EXIFTAG_SUBSECTIMEORIGINAL,"Exif:SubsecTimeOriginal",	TIFF_ASCII },
    { EXIFTAG_SUBSECTIMEDIGITIZED,"Exif:SubsecTimeDigitized",	TIFF_ASCII },
    { EXIFTAG_FLASHPIXVERSION,	"Exif:FlashPixVersion",	TIFF_NOTYPE },	// skip "Exif:FlashPixVesion",	TIFF_NOTYPE },
    { EXIFTAG_COLORSPACE,	"Exif:ColorSpace",	TIFF_SHORT },
    { EXIFTAG_PIXELXDIMENSION,	"Exif:PixelXDimension",	TIFF_LONG },
    { EXIFTAG_PIXELYDIMENSION,	"Exif:PixelYDimension",	TIFF_LONG },
    { EXIFTAG_RELATEDSOUNDFILE,	"Exif:RelatedSoundFile", TIFF_NOTYPE },	// skip
    { EXIFTAG_FLASHENERGY,	"Exif:FlashEnergy",	TIFF_RATIONAL },
    { EXIFTAG_SPATIALFREQUENCYRESPONSE,	"Exif:SpatialFrequencyResponse",	TIFF_NOTYPE },
    { EXIFTAG_FOCALPLANEXRESOLUTION,	"Exif:FocalPlaneXResolution",	TIFF_RATIONAL },
    { EXIFTAG_FOCALPLANEYRESOLUTION,	"Exif:FocalPlaneYResolution",	TIFF_RATIONAL },
    { EXIFTAG_FOCALPLANERESOLUTIONUNIT,	"Exif:FocalPlaneResolutionUnit",	TIFF_SHORT }, // Symbolic?
    { EXIFTAG_SUBJECTLOCATION,	"Exif:SubjectLocation",	TIFF_SHORT }, // FIXME: short[2]
    { EXIFTAG_EXPOSUREINDEX,	"Exif:ExposureIndex",	TIFF_RATIONAL },
    { EXIFTAG_SENSINGMETHOD,	"Exif:SensingMethod",	TIFF_SHORT },
    { EXIFTAG_FILESOURCE,	"Exif:FileSource",	TIFF_NOTYPE },
    { EXIFTAG_SCENETYPE,	"Exif:SceneType",	TIFF_NOTYPE },
    { EXIFTAG_CFAPATTERN,	"Exif:CFAPattern",	TIFF_NOTYPE },
    { EXIFTAG_CUSTOMRENDERED,	"Exif:CustomRendered",	TIFF_SHORT },
    { EXIFTAG_EXPOSUREMODE,	"Exif:ExposureMode",	TIFF_SHORT },
    { EXIFTAG_WHITEBALANCE,	"Exif:WhiteBalance",	TIFF_SHORT },
    { EXIFTAG_DIGITALZOOMRATIO,	"Exif:DigitalZoomRatio",TIFF_RATIONAL },
    { EXIFTAG_FOCALLENGTHIN35MMFILM,	"Exif:FocalLengthIn35mmFilm",	TIFF_SHORT },
    { EXIFTAG_SCENECAPTURETYPE,	"Exif:SceneCaptureType",TIFF_SHORT },
    { EXIFTAG_GAINCONTROL,	"Exif:GainControl",	TIFF_RATIONAL },
    { EXIFTAG_CONTRAST,	        "Exif:Contrast",	TIFF_SHORT },
    { EXIFTAG_SATURATION,	"Exif:Saturation",	TIFF_SHORT },
    { EXIFTAG_SHARPNESS,	"Exif:Sharpness",	TIFF_SHORT },
    { EXIFTAG_DEVICESETTINGDESCRIPTION,	"Exif:DeviceSettingDescription",	TIFF_NOTYPE },
    { EXIFTAG_SUBJECTDISTANCERANGE,	"Exif:SubjectDistanceRange",	TIFF_SHORT },
    { EXIFTAG_IMAGEUNIQUEID,	"Exif:ImageUniqueID",	TIFF_ASCII },
    { 0, NULL, TIFF_NOTYPE }
};



void
TIFFInput::readspec ()
{
    uint32 width, height, depth;
    unsigned short nchans;
    TIFFGetField (m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetField (m_tif, TIFFTAG_SAMPLESPERPIXEL, &nchans);

    m_spec = ImageSpec ((int)width, (int)height, (int)nchans);

    float x = 0, y = 0;
    TIFFGetField (m_tif, TIFFTAG_XPOSITION, &x);
    TIFFGetField (m_tif, TIFFTAG_YPOSITION, &y);
    m_spec.x = (int)x;
    m_spec.y = (int)y;
    m_spec.z = 0;
    // FIXME? - TIFF spec describes the positions as in resolutionunit.
    // What happens if this is not unitless pixels?  Are we interpreting
    // it all wrong?

    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1) 
        m_spec.full_width = width;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1)
        m_spec.full_height = height;

    if (TIFFIsTiled (m_tif)) {
        TIFFGetField (m_tif, TIFFTAG_TILEWIDTH, &m_spec.tile_width);
        TIFFGetField (m_tif, TIFFTAG_TILELENGTH, &m_spec.tile_height);
        TIFFGetFieldDefaulted (m_tif, TIFFTAG_TILEDEPTH, &m_spec.tile_depth);
    } else {
        m_spec.tile_width = 0;
        m_spec.tile_height = 0;
        m_spec.tile_depth = 0;
    }

    m_bitspersample = 8;
    TIFFGetField (m_tif, TIFFTAG_BITSPERSAMPLE, &m_bitspersample);
    m_spec.attribute ("BitsPerSample", (int)m_bitspersample);

    unsigned short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (m_bitspersample) {
    case 1:
    case 2:
    case 4:
        // Make 1, 2, 4 bpp look like byte images
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (TypeDesc::UINT8);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (TypeDesc::INT8);
        else m_spec.set_format (TypeDesc::UINT8);  // punt
        break;
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (TypeDesc::UINT16);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (TypeDesc::INT16);
        break;
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format (TypeDesc::FLOAT);
        break;
    default:
        m_spec.set_format (TypeDesc::UNKNOWN);
        break;
    }

    // Use the table for all the obvious things that can be mindlessly
    // shoved into the image spec.
    for (int i = 0;  tiff_tag_table[i].name;  ++i)
        find_tag (tiff_tag_table[i].tifftag,
                  tiff_tag_table[i].tifftype, tiff_tag_table[i].name);

    // Now we need to get fields "by hand" for anything else that is less
    // straightforward...

    m_photometric = (m_spec.nchannels == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
    TIFFGetField (m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric);
    m_spec.attribute ("tiff:PhotometricInterpretation", (int)m_photometric);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Read the color map
        unsigned short *r = NULL, *g = NULL, *b = NULL;
        TIFFGetField (m_tif, TIFFTAG_COLORMAP, &r, &g, &b);
        ASSERT (r != NULL && g != NULL && b != NULL);
        m_colormap.clear ();
        m_colormap.insert (m_colormap.end(), r, r + (1 << m_bitspersample));
        m_colormap.insert (m_colormap.end(), g, g + (1 << m_bitspersample));
        m_colormap.insert (m_colormap.end(), b, b + (1 << m_bitspersample));
        // Palette TIFF images are always 3 channels (to the client)
        m_spec.nchannels = 3;
        m_spec.default_channel_names ();
    }

    TIFFGetFieldDefaulted (m_tif, TIFFTAG_PLANARCONFIG, &m_planarconfig);
    m_spec.attribute ("tiff:PlanarConfiguration", (int)m_planarconfig);
    if (m_planarconfig == PLANARCONFIG_SEPARATE)
        m_spec.attribute ("planarconfig", "separate");
    else
        m_spec.attribute ("planarconfig", "contig");

    short compress;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_COMPRESSION, &compress);
    m_spec.attribute ("tiff:Compression", (int)compress);
    switch (compress) {
    case COMPRESSION_NONE :
        m_spec.attribute ("compression", "none");
        break;
    case COMPRESSION_LZW :
        m_spec.attribute ("compression", "lzw");
        break;
    case COMPRESSION_CCITTRLE :
        m_spec.attribute ("compression", "ccittrle");
        break;
    case COMPRESSION_ADOBE_DEFLATE :
        m_spec.attribute ("compression", "zip");
        break;
    case COMPRESSION_PACKBITS :
        m_spec.attribute ("compression", "packbits");
        break;
    default:
        break;
    }

    short resunit = -1;
    TIFFGetField (m_tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
    switch (resunit) {
    case RESUNIT_NONE : m_spec.attribute ("ResolutionUnit", "none"); break;
    case RESUNIT_INCH : m_spec.attribute ("ResolutionUnit", "in"); break;
    case RESUNIT_CENTIMETER : m_spec.attribute ("ResolutionUnit", "cm"); break;
    }

    get_matrix_attribute ("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA);
    get_matrix_attribute ("worldtoscreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN);
    get_int_attribute ("tiff:subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly? 

    // FIXME: do we care about fillorder for 1-bit and 4-bit images?

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            m_spec.channelnames[c] = "z";
    }

    // N.B. we currently ignore the following TIFF fields:
    // ExtraSamples
    // GrayResponseCurve GrayResponseUnit
    // MaxSampleValue MinSampleValue
    // NewSubfileType RowsPerStrip SubfileType(deprecated)
    // Colorimetry fields

    // Search for an EXIF IFD in the TIFF file, and if found, rummage 
    // around for Exif fields.
#if TIFFLIB_VERSION > 20050912    /* compat with old TIFF libs - skip Exif */
    int exifoffset = 0;
    if (TIFFGetField (m_tif, TIFFTAG_EXIFIFD, &exifoffset) &&
            TIFFReadEXIFDirectory (m_tif, exifoffset)) {
        for (int i = 0;  exif_tag_table[i].name;  ++i)
            find_tag (exif_tag_table[i].tifftag, exif_tag_table[i].tifftype,
                      exif_tag_table[i].name);
        // I'm not sure what state TIFFReadEXIFDirectory leaves us.
        // So to be safe, close and re-seek.
        TIFFClose (m_tif);
        m_tif = TIFFOpen (m_filename.c_str(), "rm");
        TIFFSetDirectory (m_tif, m_subimage);

        // A few tidbits to look for
        ImageIOParameter *p;
        if ((p = m_spec.find_attribute ("Exif:ColorSpace", TypeDesc::INT))) {
            // Exif spec says that anything other than 0xffff==uncalibrated
            // should be interpreted to be sRGB.
            if (*(const int *)p->data() == 0xffff)
                m_spec.linearity = ImageSpec::UnknownLinearity;
            else
                m_spec.linearity = ImageSpec::sRGB;
        }
    }
#endif

#if TIFFLIB_VERSION >= 20051230
    // Search for IPTC metadata in IIM form -- but older versions of
    // libtiff botch the size, so ignore it for very old libtiff.
    int iptcsize = 0;
    const void *iptcdata = NULL;
    if (TIFFGetField (m_tif, TIFFTAG_RICHTIFFIPTC, &iptcsize, &iptcdata)) {
        std::vector<long> iptc ((long *)iptcdata, (long *)iptcdata+iptcsize);
        if (TIFFIsByteSwapped (m_tif))
            TIFFSwabArrayOfLong ((uint32*)&iptc[0], iptcsize);
        OpenImageIO::decode_iptc_iim (&iptc[0], iptcsize*4, m_spec);
    }
#endif

    // Search for an XML packet containing XMP (IPTC, Exif, etc.)
    int xmlsize = 0;
    const void *xmldata = NULL;
    if (TIFFGetField (m_tif, TIFFTAG_XMLPACKET, &xmlsize, &xmldata)) {
        // std::cerr << "Found XML data, size " << xmlsize << "\n";
        if (xmldata && xmlsize) {
            std::string xml ((const char *)xmldata, xmlsize);
            OpenImageIO::decode_xmp (xml, m_spec);
        }
    }

#if 0
    // Experimental -- look for photoshop data
    int photoshopsize = 0;
    const void *photoshopdata = NULL;
    if (TIFFGetField (m_tif, TIFFTAG_PHOTOSHOP, &photoshopsize, &photoshopdata)) {
        std::cerr << "Found PHOTOSHOP data, size " << photoshopsize << "\n";
        if (photoshopdata && photoshopsize) {
//            std::string photoshop ((const char *)photoshopdata, photoshopsize);
//            std::cerr << "PHOTOSHOP:\n" << photoshop << "\n---\n";
        }
    }
#endif
}



bool
TIFFInput::close ()
{
    close_tif ();
    init();  // Reset to initial state
    return true;
}



/// Helper: Convert n pixels from separate (RRRGGGBBB) to contiguous
/// (RGBRGBRGB) planarconfig.
void
TIFFInput::separate_to_contig (int n, const unsigned char *separate,
                               unsigned char *contig)
{
    int channelbytes = m_spec.channel_bytes();
    for (int p = 0;  p < n;  ++p)                     // loop over pixels
        for (int c = 0;  c < m_spec.nchannels;  ++c)    // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                contig[(p*m_spec.nchannels+c)*channelbytes+i] =
                    separate[(c*n+p)*channelbytes+i];
}



void
TIFFInput::palette_to_rgb (int n, const unsigned char *palettepels,
                           unsigned char *rgb)
{
    int vals_per_byte = 8 / m_bitspersample;
    int entries = 1 << m_bitspersample;
    int highest = entries-1;
    DASSERT (m_spec.nchannels == 3);
    DASSERT (m_colormap.size() == 3*entries);
    for (int x = 0;  x < n;  ++x) {
        int i = palettepels[x/vals_per_byte];
        i >>= (m_bitspersample * (vals_per_byte - 1 - (x % vals_per_byte)));
        i &= highest;
        *rgb++ = m_colormap[0*entries+i] / 257;
        *rgb++ = m_colormap[1*entries+i] / 257;
        *rgb++ = m_colormap[2*entries+i] / 257;
    }
}



void
TIFFInput::nbit_to_8bit (int n, const unsigned char *bits,
                         unsigned char *bytes, int nbits)
{
    int vals_per_byte = 8 / nbits;
    int highest = (1 << nbits) - 1;
    for (int i = 0;  i < n;  ++i) {
        int b = bits[i/vals_per_byte];
        b >>= (nbits * (vals_per_byte - 1 - (i % vals_per_byte)));
        b &= highest;
        bytes[i] = (unsigned char) ((b * 255) / highest);
    }
}



void
TIFFInput::invert_photometric (int n, void *data)
{
    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8: {
        unsigned char *d = (unsigned char *) data;
        for (int i = 0;  i < n;  ++i)
            d[i] = 255 - d[i];
        break;
        }
    default:
        break;
    }
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= m_spec.y;
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (m_spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        m_scratch.resize (m_spec.scanline_bytes());
        int plane_bytes = m_spec.width * m_spec.format.size();
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            if (TIFFReadScanline (m_tif, &m_scratch[plane_bytes*c], y, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else if (m_bitspersample == 1 || m_bitspersample == 2 || 
               m_bitspersample == 4) {
        // <8 bit images
        m_scratch.resize (m_spec.width);
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        nbit_to_8bit (m_spec.width, &m_scratch[0], (unsigned char *)data,
                      m_bitspersample);
    } else {
        // Contiguous, >= 8 bit per sample -- the "usual" case
        if (TIFFReadScanline (m_tif, data, y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (m_spec.width * m_spec.nchannels, data);

    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= m_spec.x;
    y -= m_spec.y;
    int tile_pixels = m_spec.tile_width * m_spec.tile_height 
                      * std::max (m_spec.tile_depth, 1);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        m_scratch.resize (tile_pixels);
        if (TIFFReadTile (m_tif, &m_scratch[0], x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (tile_pixels, &m_scratch[0], (unsigned char *)data);
    } else if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB)
        int plane_bytes = tile_pixels * m_spec.format.size();
        DASSERT (plane_bytes*m_spec.nchannels == m_spec.tile_bytes());
        m_scratch.resize (m_spec.tile_bytes());
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            if (TIFFReadTile (m_tif, &m_scratch[plane_bytes*c], x, y, z, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        separate_to_contig (tile_pixels, &m_scratch[0], (unsigned char *)data);
    } else {
        // Contiguous, >= 8 bit per sample -- the "usual" case
        if (TIFFReadTile (m_tif, data, x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (tile_pixels, data);

    return true;
}
