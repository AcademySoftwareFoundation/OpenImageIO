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
#include "filesystem.h"
#include "fmath.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


// Helper struct for constructing tables of TIFF tags
struct TIFF_tag_info {
    int tifftag;       // TIFF tag used for this info
    const char *name;  // Attribute name we use, or NULL to ignore the tag
    TIFFDataType tifftype;  // Data type that TIFF wants
};



// Note about MIP-maps versus subimages: 
//
// TIFF files support subimages, but do not explicitly support
// multiresolution/MIP maps.  So we have always used subimages to store
// MIP levels.
//
// At present, TIFF is the only format people use for multires textures
// that don't explicitly support it, so rather than make the
// TextureSystem have to handle both cases, we choose instead to emulate
// MIP with subimage in a way that's purely within the TIFFInput class.
// To the outside world, it really does look MIP-mapped.  This only
// kicks in for TIFF files that have the "textureformat" metadata set.
//
// The internal m_subimage really does contain the subimage, but for the
// MIP emulation case, we report the subimage as the MIP level, and 0 as
// the subimage.  It is indeed a tangled web of deceit we weave.



class TIFFInput : public ImageInput {
public:
    TIFFInput () { init(); }
    virtual ~TIFFInput () { close(); }
    virtual const char * format_name (void) const { return "tiff"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec &config);
    virtual bool close ();
    virtual int current_subimage (void) const {
        // If m_emulate_mipmap is true, pretend subimages are mipmap levels
        return m_emulate_mipmap ? 0 : m_subimage;
    }
    virtual int current_miplevel (void) const {
        // If m_emulate_mipmap is true, pretend subimages are mipmap levels
        return m_emulate_mipmap ? m_subimage : 0;
    }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    TIFF *m_tif;                     ///< libtiff handle
    std::string m_filename;          ///< Stash the filename
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use
    int m_subimage;                  ///< What subimage are we looking at?
    int m_next_scanline;             ///< Next scanline we'll read
    bool m_no_random_access;         ///< Should we avoid random access?
    bool m_emulate_mipmap;           ///< Should we emulate mip with subimage?
    bool m_keep_unassociated_alpha;  ///< If the image is unassociated, please
                                     ///  try to keep it that way!
    bool m_convert_alpha;            ///< Do we need to associate alpha?
    bool m_separate;                 ///< Separate planarconfig?
    unsigned short m_planarconfig;   ///< Planar config of the file
    unsigned short m_bitspersample;  ///< Of the *file*, not the client's view
    unsigned short m_photometric;    ///< Of the *file*, not the client's view
    std::vector<unsigned short> m_colormap;  ///< Color map for palette images

    // Reset everything to initial state
    void init () {
        m_tif = NULL;
        m_subimage = -1;
        m_emulate_mipmap = false;
        m_keep_unassociated_alpha = false;
        m_convert_alpha = false;
        m_separate = false;
        m_colormap.clear();
    }

    void close_tif () {
        if (m_tif) {
            TIFFClose (m_tif);
            m_tif = NULL;
        }
    }

    // Read tags from the current directory of m_tif and fill out spec.
    // If read_meta is false, assume that m_spec already contains valid
    // metadata and should not be cleared or rewritten.
    void readspec (bool read_meta=true);

    // Convert planar separate to contiguous data format
    void separate_to_contig (int n, const unsigned char *separate,
                             unsigned char *contig);

    // Convert palette to RGB
    void palette_to_rgb (int n, const unsigned char *palettepels,
                         unsigned char *rgb);

    // Convert in-bits to out-bits (outbits must be 8, 16, 32, and
    // inbits < outbits)
    void bit_convert (int n, const unsigned char *in, int inbits,
                      void *out, int outbits);

    void invert_photometric (int n, void *data);

    // Convert from unassociated/non-premultiplied alpha to
    // associated/premultiplied
    template <class T>
    void unassociate (T *data, int size, int nchannels, int alpha_channel) {
        double scale = std::numeric_limits<T>::is_integer ?
            1.0/std::numeric_limits<T>::max() : 1.0;
        for ( ;  size;  --size, data += nchannels)
            for (int c = 0;  c < nchannels;  c++)
                if (c != alpha_channel) {
                    double f = data[c];
                    data[c] = T (f * (data[alpha_channel] * scale));
                }
    }

    void unassalpha_to_assocalpha (int n, void *data);

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
            m_spec.attribute (name, TypeDesc::TypeMatrix, f);
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
#ifdef TIFF_VERSION_BIG
        const TIFFField *info = TIFFFindField (m_tif, tifftag, tifftype);
#else
        const TIFFFieldInfo *info = TIFFFindFieldInfo (m_tif, tifftag, tifftype);
#endif
        if (! info) {
            // Something has gone wrong, libtiff doesn't think the field type
            // is the same as we do.
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
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *tiff_input_imageio_create () { return new TIFFInput; }

// DLLEXPORT int tiff_imageio_version = OIIO_PLUGIN_VERSION; // it's in tiffoutput.cpp

DLLEXPORT const char * tiff_input_extensions[] = {
    "tiff", "tif", "tx", "env", "sm", "vsm", NULL
};

OIIO_PLUGIN_EXPORTS_END



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
TIFFInput::valid_file (const std::string &filename) const
{
    FILE *file = fopen (filename.c_str(), "r");
    if (! file)
        return false;  // needs to be able to open
    unsigned short magic[2] = { 0, 0 };
    fread (magic, sizeof(unsigned short), 2, file);
    fclose (file);
    if (magic[0] != TIFF_LITTLEENDIAN && magic[0] != TIFF_BIGENDIAN)
        return false;  // not the right byte order
    if ((magic[0] == TIFF_LITTLEENDIAN) != littleendian())
        swap_endian (&magic[1], 1);
    return (magic[1] == 42 /* Classic TIFF */ ||
            magic[1] == 43 /* Big TIFF */);
}



bool
TIFFInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = -1;
    return seek_subimage (0, 0, newspec);
}



bool
TIFFInput::open (const std::string &name, ImageSpec &newspec,
                 const ImageSpec &config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    return open (name, newspec);
}



bool
TIFFInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (subimage < 0)       // Illegal
        return false;
    if (m_emulate_mipmap) {
        // Emulating MIPmap?  Pretend one subimage, many MIP levels.
        if (subimage != 0)
            return false;
        subimage = miplevel;
    } else {
        // No MIPmap emulation
        if (miplevel != 0)
            return false;
    }

    if (subimage == m_subimage) {
        // We're already pointing to the right subimage
        newspec = m_spec;
        return true;
    }

    // If we're emulating a MIPmap, only resolution is allowed to change
    // between MIP levels, so if we already have a valid level in m_spec,
    // we don't need to re-parse metadata, it's guaranteed to be the same.
    bool read_meta = !(m_emulate_mipmap && m_tif && m_subimage >= 0);

    if (! m_tif) {
        // Use our own error handler to keep libtiff from spewing to stderr
        lock_guard lock (lasterr_mutex);
        TIFFSetErrorHandler (my_error_handler);
        TIFFSetWarningHandler (my_error_handler);
    }

    if (! m_tif) {
        m_tif = TIFFOpen (m_filename.c_str(), "rm");
        if (m_tif == NULL) {
            error ("Could not open file: %s",
                   lasterr.length() ? lasterr.c_str() : m_filename.c_str());
            return false;
        }
        m_subimage = 0;
    }
    
    m_next_scanline = 0;   // next scanline we'll read
    if (TIFFSetDirectory (m_tif, subimage)) {
        m_subimage = subimage;
        readspec (read_meta);
        newspec = m_spec;
        if (newspec.format == TypeDesc::UNKNOWN) {
            error ("No support for data format of \"%s\"", m_filename.c_str());
            return false;
        }
        return true;
    } else {
        error ("%s", lasterr.length() ? lasterr.c_str() : m_filename.c_str());
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
TIFFInput::readspec (bool read_meta)
{
    uint32 width = 0, height = 0, depth = 0;
    unsigned short nchans = 1;
    TIFFGetField (m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLESPERPIXEL, &nchans);

    if (read_meta) {
        // clear the whole m_spec and start fresh
        m_spec = ImageSpec ((int)width, (int)height, (int)nchans);
    } else {
        // assume m_spec is valid, except for things that might differ
        // between MIP levels
        m_spec.width = (int)width;
        m_spec.height = (int)height;
        m_spec.depth = (int)depth;
        m_spec.full_x = 0;
        m_spec.full_y = 0;
        m_spec.full_z = 0;
        m_spec.full_width = (int)width;
        m_spec.full_height = (int)height;
        m_spec.full_depth = (int)depth;
        m_spec.nchannels = (int)nchans;
    }

    float x = 0, y = 0;
    TIFFGetField (m_tif, TIFFTAG_XPOSITION, &x);
    TIFFGetField (m_tif, TIFFTAG_YPOSITION, &y);
    m_spec.x = (int)x;
    m_spec.y = (int)y;
    m_spec.z = 0;
    // FIXME? - TIFF spec describes the positions as in resolutionunit.
    // What happens if this is not unitless pixels?  Are we interpreting
    // it all wrong?

    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1
          && width > 0)
        m_spec.full_width = width;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1
          && height > 0)
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
    m_spec.attribute ("oiio:BitsPerSample", (int)m_bitspersample);

    unsigned short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (m_bitspersample) {
    case 1:
    case 2:
    case 4:
    case 6:
        // Make 1, 2, 4, 6 bpp look like byte images
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (TypeDesc::UINT8);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (TypeDesc::INT8);
        else m_spec.set_format (TypeDesc::UINT8);  // punt
        break;
    case 10:
    case 12:
    case 14:
        // Make 10, 12, 14 bpp look like 16 bit images
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (TypeDesc::UINT16);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (TypeDesc::INT16);
        else if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format (TypeDesc::HALF); // not to spec, but why not?
        else
            m_spec.set_format (TypeDesc::UNKNOWN);
        break;
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format (TypeDesc::FLOAT);
        else if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format (TypeDesc::UINT32);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format (TypeDesc::INT32);
        else
            m_spec.set_format (TypeDesc::UNKNOWN);
        break;
    case 64:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format (TypeDesc::DOUBLE);
        else
            m_spec.set_format (TypeDesc::UNKNOWN);
        break;
    default:
        m_spec.set_format (TypeDesc::UNKNOWN);
        break;
    }

    // If we've been instructed to skip reading metadata, because it is
    // guaranteed to be identical to what we already have in m_spec,
    // skip everything following.
    if (! read_meta)
        return;

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
        // FIXME - what about palette + extra (alpha?) channels?  Is that
        // allowed?  And if so, ever encountered in the wild?
    }

    TIFFGetFieldDefaulted (m_tif, TIFFTAG_PLANARCONFIG, &m_planarconfig);
    m_separate = (m_planarconfig == PLANARCONFIG_SEPARATE &&
                  m_spec.nchannels > 1 &&
                  m_photometric != PHOTOMETRIC_PALETTE);
    m_spec.attribute ("tiff:PlanarConfiguration", (int)m_planarconfig);
    if (m_planarconfig == PLANARCONFIG_SEPARATE)
        m_spec.attribute ("planarconfig", "separate");
    else
        m_spec.attribute ("planarconfig", "contig");

    int compress = 0;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_COMPRESSION, &compress);
    m_spec.attribute ("tiff:Compression", compress);
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
    case COMPRESSION_DEFLATE :
    case COMPRESSION_ADOBE_DEFLATE :
        m_spec.attribute ("compression", "zip");
        break;
    case COMPRESSION_PACKBITS :
        m_spec.attribute ("compression", "packbits");
        break;
    default:
        break;
    }

    int rowsperstrip = -1;
    if (! m_spec.tile_width) {
        TIFFGetField (m_tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
        if (rowsperstrip > 0)
            m_spec.attribute ("tiff:RowsPerStrip", rowsperstrip);
    }

    // The libtiff docs say that only uncompressed images, or those with
    // rowsperstrip==1, support random access to scanlines.
    m_no_random_access = (compress != COMPRESSION_NONE && rowsperstrip != 1);

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

    // Do we care about fillorder?  No, the TIFF spec says, "We
    // recommend that FillOrder=2 (lsb-to-msb) be used only in
    // special-purpose applications".  So OIIO will assume msb-to-lsb
    // convention until somebody finds a TIFF file in the wild that
    // breaks this assumption.

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s)
        m_emulate_mipmap = true;
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            m_spec.channelnames[c] = "z";
    }

    unsigned short *sampleinfo = NULL;
    unsigned short extrasamples = 0;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_EXTRASAMPLES, &extrasamples, &sampleinfo);
    // std::cerr << "Extra samples = " << extrasamples << "\n";
    bool alpha_is_unassociated = false;  // basic assumption
    if (extrasamples) {
        // If the TIFF ExtraSamples tag was specified, use that to figure
        // out the meaning of alpha.
        int colorchannels = 3;
        if (m_photometric == PHOTOMETRIC_MINISWHITE ||
              m_photometric == PHOTOMETRIC_MINISBLACK ||
              m_photometric == PHOTOMETRIC_PALETTE ||
              m_photometric == PHOTOMETRIC_MASK)
            colorchannels = 1;
        for (int i = 0, c = colorchannels;
             i < extrasamples && c < m_spec.nchannels;  ++i, ++c) {
            // std::cerr << "   extra " << i << " " << sampleinfo[i] << "\n";
            if (sampleinfo[i] == EXTRASAMPLE_ASSOCALPHA) {
                // This is the alpha channel, associated as usual
                m_spec.alpha_channel = c;
            } else if (sampleinfo[i] == EXTRASAMPLE_UNASSALPHA) {
                // This is the alpha channel, but color is unassociated
                m_spec.alpha_channel = c;
                alpha_is_unassociated = true;
                m_spec.attribute ("oiio:UnassociatedAlpha", 1);
            } else {
                DASSERT (sampleinfo[i] == EXTRASAMPLE_UNSPECIFIED);
                // This extra channel is not alpha at all.  Undo any
                // assumptions we previously made about this channel.
                if (m_spec.alpha_channel == c) {
                    m_spec.channelnames[c] = Strutil::format("channel%d", c);
                    m_spec.alpha_channel = -1;
                }
            }
        }
        if (m_spec.alpha_channel >= 0)
            m_spec.channelnames[m_spec.alpha_channel] = "A";
    }
    // Will we need to do alpha conversions?
    m_convert_alpha = (m_spec.alpha_channel >= 0 && alpha_is_unassociated &&
                       ! m_keep_unassociated_alpha);

    // N.B. we currently ignore the following TIFF fields:
    // GrayResponseCurve GrayResponseUnit
    // MaxSampleValue MinSampleValue
    // NewSubfileType SubfileType(deprecated)
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
            if (*(const int *)p->data() != 0xffff)
                m_spec.attribute ("oiio::ColorSpace", "sRGB");
        }
    }
#endif

#if TIFFLIB_VERSION >= 20051230
    // Search for IPTC metadata in IIM form -- but older versions of
    // libtiff botch the size, so ignore it for very old libtiff.
    int iptcsize = 0;
    const void *iptcdata = NULL;
    if (TIFFGetField (m_tif, TIFFTAG_RICHTIFFIPTC, &iptcsize, &iptcdata)) {
        std::vector<uint32> iptc ((uint32 *)iptcdata, (uint32 *)iptcdata+iptcsize);
        if (TIFFIsByteSwapped (m_tif))
            TIFFSwabArrayOfLong ((uint32*)&iptc[0], iptcsize);
        decode_iptc_iim (&iptc[0], iptcsize*4, m_spec);
    }
#endif

    // Search for an XML packet containing XMP (IPTC, Exif, etc.)
    int xmlsize = 0;
    const void *xmldata = NULL;
    if (TIFFGetField (m_tif, TIFFTAG_XMLPACKET, &xmlsize, &xmldata)) {
        // std::cerr << "Found XML data, size " << xmlsize << "\n";
        if (xmldata && xmlsize) {
            std::string xml ((const char *)xmldata, xmlsize);
            decode_xmp (xml, m_spec);
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
    size_t vals_per_byte = 8 / m_bitspersample;
    size_t entries = 1 << m_bitspersample;
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
TIFFInput::bit_convert (int n, const unsigned char *in, int inbits,
                        void *out, int outbits)
{
    ASSERT (inbits >= 1 && inbits < 31);  // surely bugs if not
    int highest = (1 << inbits) - 1;
    int B = 0, b = 0;
    // Invariant:
    // So far, we have used in[0..B-1] and the high b bits of in[B].
    for (int i = 0;  i < n;  ++i) {
        long long val = 0;
        int valbits = 0;  // bits so far we've accumulated in val
        while (valbits < inbits) {
            // Invariant: we have already accumulated valbits of the next
            // needed value (of a total of inbits), living in the valbits
            // low bits of val.
            int out_left = inbits - valbits;  // How much more we still need
            int in_left = 8 - b; // Bits still available in in[B].
            if (in_left <= out_left) {
                // Eat the rest of this byte:
                //   |---------|--------|
                //        b      in_left
                val <<= in_left;
                val |= in[B] & ~(0xffffffff << in_left);
                ++B;
                b = 0;
                valbits += in_left;
            } else {
                // Eat just the bits we need:
                //   |--|---------|-----|
                //     b  out_left  extra
                val <<= out_left;
                int extra = 8 - b - out_left;
                val |= (in[B] >> extra) & ~(0xffffffff << out_left);
                b += out_left;
                valbits = inbits;
            }
        }
        if (outbits == 8)
            ((unsigned char *)out)[i] = (unsigned char) ((val * 0xff) / highest);
        else if (outbits == 16)
            ((unsigned short *)out)[i] = (unsigned short) ((val * 0xffff) / highest);
        else
            ((unsigned int *)out)[i] = (unsigned int) ((val * 0xffffffff) / highest);
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



void
TIFFInput::unassalpha_to_assocalpha (int n, void *data)
{
    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8:
        unassociate ((unsigned char *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    case TypeDesc::INT8:
        unassociate ((char *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    case TypeDesc::UINT16:
        unassociate ((unsigned short *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    case TypeDesc::INT16:
        unassociate ((short *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    case TypeDesc::FLOAT:
        unassociate ((float *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    case TypeDesc::DOUBLE:
        unassociate ((double *)data, n, m_spec.nchannels, m_spec.alpha_channel);
        break;
    default:
        break;
    }
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= m_spec.y;

    // For compression modes that don't support random access to scanlines
    // (which I *think* is only LZW), we need to emulate random access by
    // re-seeking.
    if (m_no_random_access) {
        if (m_next_scanline > y) {
            // User is trying to read an earlier scanline than the one we're
            // up to.  Easy fix: start over.
            // FIXME: I'm too tired to look into it now, but I wonder if
            // it is able to randomly seek to the first line in any
            // "strip", in which case we don't need to start from 0, just
            // start from the beginning of the strip we need.
            ImageSpec dummyspec;
            int old_subimage = current_subimage();
            int old_miplevel = current_miplevel();
            if (! close ()  ||
                ! open (m_filename, dummyspec)  ||
                ! seek_subimage (old_subimage, old_miplevel, dummyspec)) {
                return false;    // Somehow, the re-open failed
            }
            ASSERT (m_next_scanline == 0 &&
                    current_subimage() == old_subimage &&
                    current_miplevel() == old_miplevel);
        }
        while (m_next_scanline < y) {
            // Keep reading until we're read the scanline we really need
            m_scratch.resize (m_spec.scanline_bytes());
            if (TIFFReadScanline (m_tif, &m_scratch[0], m_next_scanline) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
            ++m_next_scanline;
        }
    }
    m_next_scanline = y+1;

    int nvals = m_spec.width * m_spec.nchannels;
    m_scratch.resize (m_spec.scanline_bytes());
    bool no_bit_convert = (m_bitspersample == 8 || m_bitspersample == 16 ||
                           m_bitspersample == 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (m_spec.width, &m_scratch[0], (unsigned char *)data);
    } else {
        // Not palette
        int plane_bytes = m_spec.width * m_spec.format.size();
        int planes = m_separate ? m_spec.nchannels : 1;
        std::vector<unsigned char> scratch2 (m_separate ? m_spec.scanline_bytes() : 0);
        // Where to read?  Directly into user data if no channel shuffling
        // or bit shifting is needed, otherwise into scratch space.
        unsigned char *readbuf = (no_bit_convert && !m_separate) ? (unsigned char *)data : &m_scratch[0];
        // Perform the reads.  Note that for contig, planes==1, so it will
        // only do one TIFFReadScanline.
        for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
            if (TIFFReadScanline (m_tif, &readbuf[plane_bytes*c], y, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        if (m_bitspersample < 8) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap (m_scratch, scratch2);
            for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
                bit_convert (m_separate ? m_spec.width : nvals,
                             &scratch2[plane_bytes*c], m_bitspersample,
                             m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 8);
        } else if (m_bitspersample > 8 && m_bitspersample < 16) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap (m_scratch, scratch2);
            for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
                bit_convert (m_separate ? m_spec.width : nvals,
                             &scratch2[plane_bytes*c], m_bitspersample,
                             m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 16);
        }
        if (m_separate) {
            // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB).
            // We know the data is in m_scratch at this point, so 
            // contiguize it into the user data area.
            separate_to_contig (m_spec.width, &m_scratch[0], (unsigned char *)data);
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (nvals, data);

    // If alpha is unassociated and we aren't requested to keep it that
    // way, multiply the colors by alpha per the usual OIIO conventions
    // to deliver associated color & alpha.
    if (m_convert_alpha)
        unassalpha_to_assocalpha (m_spec.width, data);

    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= m_spec.x;
    y -= m_spec.y;
    imagesize_t tile_pixels = m_spec.tile_pixels();
    imagesize_t nvals = tile_pixels * m_spec.nchannels;
    m_scratch.resize (m_spec.tile_bytes());
    bool no_bit_convert = (m_bitspersample == 8 || m_bitspersample == 16 ||
                           m_bitspersample == 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadTile (m_tif, &m_scratch[0], x, y, z, 0) < 0) {
            error ("%s", lasterr.c_str());
            return false;
        }
        palette_to_rgb (tile_pixels, &m_scratch[0], (unsigned char *)data);
    } else {
        // Not palette
        imagesize_t plane_bytes = m_spec.tile_pixels() * m_spec.format.size();
        int planes = m_separate ? m_spec.nchannels : 1;
        std::vector<unsigned char> scratch2 (m_separate ? m_spec.tile_bytes() : 0);
        // Where to read?  Directly into user data if no channel shuffling
        // or bit shifting is needed, otherwise into scratch space.
        unsigned char *readbuf = (no_bit_convert && !m_separate) ? (unsigned char *)data : &m_scratch[0];
        // Perform the reads.  Note that for contig, planes==1, so it will
        // only do one TIFFReadTile.
        for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
            if (TIFFReadTile (m_tif, &readbuf[plane_bytes*c], x, y, z, c) < 0) {
                error ("%s", lasterr.c_str());
                return false;
            }
        if (m_bitspersample < 8) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap (m_scratch, scratch2);
            for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
                bit_convert (m_separate ? tile_pixels : nvals,
                             &scratch2[plane_bytes*c], m_bitspersample,
                             m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 8);
        } else if (m_bitspersample > 8 && m_bitspersample < 16) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap (m_scratch, scratch2);
            for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
                bit_convert (m_separate ? tile_pixels : nvals,
                             &scratch2[plane_bytes*c], m_bitspersample,
                             m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 16);
        }
        if (m_separate) {
            // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB).
            // We know the data is in m_scratch at this point, so 
            // contiguize it into the user data area.
            separate_to_contig (tile_pixels, &m_scratch[0], (unsigned char *)data);
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (tile_pixels, data);

    // If alpha is unassociated and we aren't requested to keep it that
    // way, multiply the colors by alpha per the usual OIIO conventions
    // to deliver associated color & alpha.
    if (m_convert_alpha)
        unassalpha_to_assocalpha (tile_pixels, data);

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

