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

#include <boost/regex.hpp>
#include <boost/thread/tss.hpp>

#include <tiffio.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/filesystem.h"
#include "OpenImageIO/fmath.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


// General TIFF information:
// TIFF 6.0 spec: 
//     http://partners.adobe.com/public/developer/en/tiff/TIFF6.pdf
// Other Adobe TIFF docs:
//     http://partners.adobe.com/public/developer/tiff/index.html
// Adobe extensions to allow 16 (and 24) bit float in TIFF (ugh, not on
// their developer page, only on Chris Cox's web site?):
//     http://chriscox.org/TIFFTN3d1.pdf
// Libtiff:
//     http://remotesensing.org/libtiff/



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
    TIFFInput ();
    virtual ~TIFFInput ();
    virtual const char * format_name (void) const { return "tiff"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual int supports (string_view feature) const {
        return (feature == "exif"
             || feature == "iptc");
        // N.B. No support for arbitrary metadata.
    }
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
    virtual bool read_scanline (int y, int z, TypeDesc format, void *data,
                                stride_t xstride);
    virtual bool read_scanlines (int ybegin, int yend, int z,
                                 int chbegin, int chend,
                                 TypeDesc format, void *data,
                                 stride_t xstride, stride_t ystride);
    virtual bool read_tile (int x, int y, int z, TypeDesc format, void *data,
                            stride_t xstride, stride_t ystride, stride_t zstride);
    virtual bool read_tiles (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, int chbegin, int chend,
                             TypeDesc format, void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);

private:
    TIFF *m_tif;                     ///< libtiff handle
    std::string m_filename;          ///< Stash the filename
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use
    std::vector<unsigned char> m_scratch2; ///< More scratch
    int m_subimage;                  ///< What subimage are we looking at?
    int m_next_scanline;             ///< Next scanline we'll read
    bool m_no_random_access;         ///< Should we avoid random access?
    bool m_emulate_mipmap;           ///< Should we emulate mip with subimage?
    bool m_keep_unassociated_alpha;  ///< If the image is unassociated, please
                                     ///  try to keep it that way!
    bool m_convert_alpha;            ///< Do we need to associate alpha?
    bool m_separate;                 ///< Separate planarconfig?
    bool m_testopenconfig;           ///< Debug aid to test open-with-config
    bool m_use_rgba_interface;       ///< Sometimes we punt
    unsigned short m_planarconfig;   ///< Planar config of the file
    unsigned short m_bitspersample;  ///< Of the *file*, not the client's view
    unsigned short m_photometric;    ///< Of the *file*, not the client's view
    unsigned short m_compression;    ///< TIFF compression tag
    unsigned short m_inputchannels;  ///< Channels in the file (careful with CMYK)
    std::vector<unsigned short> m_colormap;  ///< Color map for palette images
    std::vector<uint32_t> m_rgbadata; ///< Sometimes we punt

    // Reset everything to initial state
    void init () {
        m_tif = NULL;
        m_subimage = -1;
        m_emulate_mipmap = false;
        m_keep_unassociated_alpha = false;
        m_convert_alpha = false;
        m_separate = false;
        m_inputchannels = 0;
        m_testopenconfig = false;
        m_colormap.clear();
        m_use_rgba_interface = false;
    }

    void close_tif () {
        if (m_tif) {
            TIFFClose (m_tif);
            m_tif = NULL;
            if (m_rgbadata.size())
                std::vector<uint32_t>().swap(m_rgbadata); // release
        }
    }

    // Read tags from the current directory of m_tif and fill out spec.
    // If read_meta is false, assume that m_spec already contains valid
    // metadata and should not be cleared or rewritten.
    void readspec (bool read_meta=true);

    // Convert planar separate to contiguous data format
    void separate_to_contig (int nplanes, int nvals,
                             const unsigned char *separate,
                             unsigned char *contig);

    // Convert palette to RGB
    void palette_to_rgb (int n, const unsigned char *palettepels,
                         unsigned char *rgb);

    // Convert in-bits to out-bits (outbits must be 8, 16, 32, and
    // inbits < outbits)
    void bit_convert (int n, const unsigned char *in, int inbits,
                      void *out, int outbits);

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
#ifndef NDEBUG
            std::cerr << "Error safe_tiffgetfield : did not expect ptr set on "
                      << name << " " << (void *)ptr << "\n";
#endif
            return false;
        }
        return ok;
    }

    // Get a string tiff tag field and put it into extra_params
    void get_string_attribute (const std::string &name, int tag) {
        char *s = NULL;
        void *ptr = NULL;  // dummy -- expect it to stay NULL
        bool ok = TIFFGetField (m_tif, tag, &s, &ptr);
        if (ok && ptr) {
            // Oy, some tags need 2 args, which are count, then ptr.
            // There's no way to know ahead of time which ones, so we send
            // a second pointer. If it gets overwritten, then we understand
            // and try it again with 2 args, first one is count.
            unsigned short count;
            ok = TIFFGetField (m_tif, tag, &count, &s);
            m_spec.attribute (name, string_view(s,count));
        }
        else if (ok && s && *s)
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

OIIO_EXPORT ImageInput *tiff_input_imageio_create () { return new TIFFInput; }

// OIIO_EXPORT int tiff_imageio_version = OIIO_PLUGIN_VERSION; // it's in tiffoutput.cpp

OIIO_EXPORT const char * tiff_input_extensions[] = {
    "tiff", "tif", "tx", "env", "sm", "vsm", NULL
};

OIIO_PLUGIN_EXPORTS_END



// Someplace to store an error message from the TIFF error handler
// To avoid thread oddities, we have the storage area buffering error
// messages for seterror()/geterror() be thread-specific.
static boost::thread_specific_ptr<std::string> thread_error_msg;
static atomic_int handler_set;
static spin_mutex handler_mutex;



std::string &
oiio_tiff_last_error ()
{
    std::string *e = thread_error_msg.get();
    if (! e) {
        e = new std::string;
        thread_error_msg.reset (e);
    }
    return *e;
}



static void
my_error_handler (const char *str, const char *format, va_list ap)
{
    oiio_tiff_last_error() = Strutil::vformat (format, ap);
}



void
oiio_tiff_set_error_handler ()
{
    if (! handler_set) {
        spin_lock lock (handler_mutex);
        if (! handler_set) {
            TIFFSetErrorHandler (my_error_handler);
            TIFFSetWarningHandler (my_error_handler);
            handler_set = 1;
        }
    }
}



struct CompressionCode {
    int code;
    const char *name;
};

static CompressionCode tiff_compressions[] = {
    { COMPRESSION_NONE,          "none" },        // no compression
    { COMPRESSION_LZW,           "lzw" },         // LZW
    { COMPRESSION_ADOBE_DEFLATE, "zip" },         // deflate / zip
    { COMPRESSION_DEFLATE,       "zip" },         // deflate / zip
    { COMPRESSION_CCITTRLE,      "ccittrle" },    // CCITT RLE
    { COMPRESSION_CCITTFAX3,     "ccittfax3" },   // CCITT group 3 fax
    { COMPRESSION_CCITT_T4,      "ccitt_t4" },    // CCITT T.4
    { COMPRESSION_CCITTFAX4,     "ccittfax4" },   // CCITT group 4 fax
    { COMPRESSION_CCITT_T6,      "ccitt_t6" },    // CCITT T.6
    { COMPRESSION_OJPEG,         "ojpeg" },       // old (pre-TIFF6.0) JPEG
    { COMPRESSION_JPEG,          "jpeg" },        // JPEG
    { COMPRESSION_NEXT,          "next" },        // NeXT 2-bit RLE
    { COMPRESSION_CCITTRLEW,     "ccittrle2" },   // #1 w/ word alignment
    { COMPRESSION_PACKBITS,      "packbits" },    // Macintosh RLE
    { COMPRESSION_THUNDERSCAN,   "thunderscan" }, // ThundeScan RLE
    { COMPRESSION_IT8CTPAD,      "IT8CTPAD" },    // IT8 CT w/ patting
    { COMPRESSION_IT8LW,         "IT8LW" },       // IT8 linework RLE
    { COMPRESSION_IT8MP,         "IT8MP" },       // IT8 monochrome picture
    { COMPRESSION_IT8BL,         "IT8BL" },       // IT8 binary line art
    { COMPRESSION_PIXARFILM,     "pixarfilm" },   // Pixar 10 bit LZW
    { COMPRESSION_PIXARLOG,      "pixarlog" },    // Pixar 11 bit ZIP
    { COMPRESSION_DCS,           "dcs" },         // Kodak DCS encoding
    { COMPRESSION_JBIG,          "isojbig" },     // ISO JBIG
    { COMPRESSION_SGILOG,        "sgilog" },      // SGI log luminance RLE
    { COMPRESSION_SGILOG24,      "sgilog24" },    // SGI log 24bit
    { COMPRESSION_JP2000,        "jp2000" },      // Leadtools JPEG2000
#if defined(TIFF_VERSION_BIG) && TIFFLIB_VERSION >= 20120922
    // Others supported in more recent TIFF library versions.
    { COMPRESSION_T85,           "T85" },         // TIFF/FX T.85 JBIG
    { COMPRESSION_T43,           "T43" },         // TIFF/FX T.43 color layered JBIG
    { COMPRESSION_LZMA,          "lzma" },        // LZMA2
#endif
    { -1, NULL }
};

static const char *
tiff_compression_name (int code)
{
    for (int i = 0; tiff_compressions[i].name; ++i)
        if (code == tiff_compressions[i].code)
            return tiff_compressions[i].name;
    return NULL;
}



TIFFInput::TIFFInput ()
{
    oiio_tiff_set_error_handler ();
    init ();
}



TIFFInput::~TIFFInput ()
{
    // Close, if not already done.
    close ();
}



bool
TIFFInput::valid_file (const std::string &filename) const
{
    FILE *file = Filesystem::fopen (filename, "r");
    if (! file)
        return false;  // needs to be able to open
    unsigned short magic[2] = { 0, 0 };
    size_t numRead = fread (magic, sizeof(unsigned short), 2, file);
    fclose (file);
    if (numRead != 2)  // fread failed
    	return false;
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
    oiio_tiff_set_error_handler ();
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
    // This configuration hint has no function other than as a debugging aid
    // for testing whether configurations are received properly from other
    // OIIO components.
    if (config.get_int_attribute("oiio:DebugOpenConfig!", 0))
        m_testopenconfig = true;
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
#ifdef _WIN32
        std::wstring wfilename = Strutil::utf8_to_utf16 (m_filename);
        m_tif = TIFFOpenW (wfilename.c_str(), "rm");
#else
        m_tif = TIFFOpen (m_filename.c_str(), "rm");
#endif
        if (m_tif == NULL) {
            std::string e = oiio_tiff_last_error();
            error ("Could not open file: %s", e.length() ? e : m_filename);
            return false;
        }
        m_subimage = 0;
    }
    
    m_next_scanline = 0;   // next scanline we'll read
    if (TIFFSetDirectory (m_tif, subimage)) {
        m_subimage = subimage;
        readspec (read_meta);
        // OK, some edge cases we just don't handle. For those, fall back on
        // the TIFFRGBA interface.
        if (m_compression == COMPRESSION_JPEG || m_compression == COMPRESSION_OJPEG ||
            m_photometric == PHOTOMETRIC_YCBCR || m_photometric == PHOTOMETRIC_CIELAB ||
            m_photometric == PHOTOMETRIC_ICCLAB || m_photometric == PHOTOMETRIC_ITULAB ||
            m_photometric == PHOTOMETRIC_LOGL || m_photometric == PHOTOMETRIC_LOGLUV) {
            char emsg[1024];
            m_use_rgba_interface = true;
            if (! TIFFRGBAImageOK (m_tif, emsg)) {
                error ("No support for this flavor of TIFF file");
                return false;
            }
            // This falls back to looking like uint8 images
            m_spec.format = TypeDesc::UINT8;
            m_spec.channelformats.clear ();
            m_photometric = PHOTOMETRIC_RGB;
        }
        newspec = m_spec;
        if (newspec.format == TypeDesc::UNKNOWN) {
            error ("No support for data format of \"%s\"", m_filename.c_str());
            return false;
        }
        return true;
    } else {
        std::string e = oiio_tiff_last_error();
        error ("%s", e.length() ? e : m_filename);
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
    { TIFFTAG_JPEGQUALITY,  "CompressionQuality", TIFF_LONG },
    { TIFFTAG_ZIPQUALITY,   "tiff:zipquality",    TIFF_LONG },
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


#define ICC_PROFILE_ATTR "ICCProfile"


void
TIFFInput::readspec (bool read_meta)
{
    uint32 width = 0, height = 0, depth = 0;
    TIFFGetField (m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_SAMPLESPERPIXEL, &m_inputchannels);

    if (read_meta) {
        // clear the whole m_spec and start fresh
        m_spec = ImageSpec ((int)width, (int)height, (int)m_inputchannels);
    } else {
        // assume m_spec is valid, except for things that might differ
        // between MIP levels
        m_spec.width = (int)width;
        m_spec.height = (int)height;
        m_spec.depth = (int)depth;
        m_spec.nchannels = (int)m_inputchannels;
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

    // Start by assuming the "full" (aka display) window is the same as the
    // data window. That's what we'll stick to if there is no further
    // information in the file. But if the file has tags for hte "full"
    // size, assume a display window with origin (0,0) and those dimensions.
    // (Unfortunately, there are no TIFF tags for "full" origin.)
    m_spec.full_x = m_spec.x;
    m_spec.full_y = m_spec.y;
    m_spec.full_z = m_spec.z;
    m_spec.full_width  = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth  = m_spec.depth;
    if (TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1 &&
        TIFFGetField (m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1 &&
          width > 0 && height > 0) {
        m_spec.full_width = width;
        m_spec.full_height = height;
        m_spec.full_x = 0;
        m_spec.full_y = 0;
    }

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
        else if (sampleformat == SAMPLEFORMAT_IEEEFP) {
            m_spec.set_format (TypeDesc::HALF);
            // Adobe extension, see http://chriscox.org/TIFFTN3d1.pdf
        }
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

    // Use the table for all the obvious things that can be mindlessly
    // shoved into the image spec.
    if (read_meta) {
        for (int i = 0;  tiff_tag_table[i].name;  ++i)
            find_tag (tiff_tag_table[i].tifftag,
                      tiff_tag_table[i].tifftype, tiff_tag_table[i].name);
        for (int i = 0;  tiff_tag_table[i].name;  ++i)
            find_tag (exif_tag_table[i].tifftag,
                      exif_tag_table[i].tifftype, exif_tag_table[i].name);
    }

    // Now we need to get fields "by hand" for anything else that is less
    // straightforward...

    m_photometric = (m_spec.nchannels == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
    TIFFGetField (m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric);
    m_spec.attribute ("tiff:PhotometricInterpretation", (int)m_photometric);
    switch (m_photometric) {
    case PHOTOMETRIC_SEPARATED :
        m_spec.attribute ("tiff:ColorSpace", "CMYK");
        m_spec.nchannels = 3;   // Silently convert to RGB
        break;
    case PHOTOMETRIC_YCBCR  :
        m_spec.attribute ("tiff:ColorSpace", "YCbCr");
        break;
    case PHOTOMETRIC_CIELAB :
        m_spec.attribute ("tiff:ColorSpace", "CIELAB");
        break;
    case PHOTOMETRIC_ICCLAB :
        m_spec.attribute ("tiff:ColorSpace", "ICCLAB");
        break;
    case PHOTOMETRIC_ITULAB :
        m_spec.attribute ("tiff:ColorSpace", "ITULAB");
        break;
    case PHOTOMETRIC_LOGL   :
        m_spec.attribute ("tiff:ColorSpace", "LOGL");
        break;
    case PHOTOMETRIC_LOGLUV :
        m_spec.attribute ("tiff:ColorSpace", "LOGLUV");
        break;
    case PHOTOMETRIC_PALETTE : {
        m_spec.attribute ("tiff:ColorSpace", "palette");
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
        if (m_bitspersample != m_spec.format.size()*8) {
            // For palette images with unusual bits per sample, set
            // oiio:BitsPerSample to the "full" version, to avoid problems
            // when copying the file back to a TIFF file (we don't write
            // palette images), but do leave "tiff:BitsPerSample" to reflect
            // the original file.
            m_spec.attribute ("tiff:BitsPerSample", (int)m_bitspersample);
            m_spec.attribute ("oiio:BitsPerSample", (int)m_spec.format.size()*8);
        }
        // FIXME - what about palette + extra (alpha?) channels?  Is that
        // allowed?  And if so, ever encountered in the wild?
        break;
        }
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

    m_compression = 0;
    TIFFGetFieldDefaulted (m_tif, TIFFTAG_COMPRESSION, &m_compression);
    m_spec.attribute ("tiff:Compression", (int)m_compression);
    if (const char *compressname = tiff_compression_name(m_compression))
        m_spec.attribute ("compression", compressname);

    int rowsperstrip = -1;
    if (! m_spec.tile_width) {
        TIFFGetField (m_tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
        if (rowsperstrip > 0)
            m_spec.attribute ("tiff:RowsPerStrip", rowsperstrip);
    }

    // The libtiff docs say that only uncompressed images, or those with
    // rowsperstrip==1, support random access to scanlines.
    m_no_random_access = (m_compression != COMPRESSION_NONE && rowsperstrip != 1);

    // Do we care about fillorder?  No, the TIFF spec says, "We
    // recommend that FillOrder=2 (lsb-to-msb) be used only in
    // special-purpose applications".  So OIIO will assume msb-to-lsb
    // convention until somebody finds a TIFF file in the wild that
    // breaks this assumption.

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
             i < extrasamples && c < m_inputchannels;  ++i, ++c) {
            // std::cerr << "   extra " << i << " " << sampleinfo[i] << "\n";
            if (sampleinfo[i] == EXTRASAMPLE_ASSOCALPHA) {
                // This is the alpha channel, associated as usual
                m_spec.alpha_channel = c;
            } else if (sampleinfo[i] == EXTRASAMPLE_UNASSALPHA) {
                // This is the alpha channel, but color is unassociated
                m_spec.alpha_channel = c;
                alpha_is_unassociated = true;
                if (m_keep_unassociated_alpha)
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

    // If we've been instructed to skip reading metadata, because it is
    // assumed to be identical to what we already have in m_spec,
    // skip everything following.
    if (! read_meta)
        return;

    short resunit = -1;
    TIFFGetField (m_tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
    switch (resunit) {
    case RESUNIT_NONE : m_spec.attribute ("ResolutionUnit", "none"); break;
    case RESUNIT_INCH : m_spec.attribute ("ResolutionUnit", "in"); break;
    case RESUNIT_CENTIMETER : m_spec.attribute ("ResolutionUnit", "cm"); break;
    }
    float xdensity = m_spec.get_float_attribute ("XResolution", 0.0f);
    float ydensity = m_spec.get_float_attribute ("YResolution", 0.0f);
    if (xdensity && ydensity)
        m_spec.attribute ("PixelAspectRatio", ydensity/xdensity);

    get_matrix_attribute ("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA);
    get_matrix_attribute ("worldtoscreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN);
    get_int_attribute ("tiff:subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly? 

    // Special names for shadow maps
    char *s = NULL;
    TIFFGetField (m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s)
        m_emulate_mipmap = true;
    if (s && ! strcmp (s, "Shadow")) {
        for (int c = 0;  c < m_spec.nchannels;  ++c)
            m_spec.channelnames[c] = "z";
    }

    /// read color profile
    unsigned int icc_datasize = 0;
    unsigned char *icc_buf = NULL;
    TIFFGetField (m_tif, TIFFTAG_ICCPROFILE, &icc_datasize, &icc_buf);
    if (icc_datasize && icc_buf)
        m_spec.attribute (ICC_PROFILE_ATTR, TypeDesc(TypeDesc::UINT8, icc_datasize), icc_buf);

    // Search for an EXIF IFD in the TIFF file, and if found, rummage 
    // around for Exif fields.
#if TIFFLIB_VERSION > 20050912    /* compat with old TIFF libs - skip Exif */
    toff_t exifoffset = 0;
    if (TIFFGetField (m_tif, TIFFTAG_EXIFIFD, &exifoffset) &&
            TIFFReadEXIFDirectory (m_tif, exifoffset)) {
        for (int i = 0;  exif_tag_table[i].name;  ++i)
            find_tag (exif_tag_table[i].tifftag, exif_tag_table[i].tifftype,
                      exif_tag_table[i].name);
        // I'm not sure what state TIFFReadEXIFDirectory leaves us.
        // So to be safe, close and re-seek.
        TIFFClose (m_tif);
#ifdef _WIN32
        std::wstring wfilename = Strutil::utf8_to_utf16 (m_filename);
        m_tif = TIFFOpenW (wfilename.c_str(), "rm");
#else
        m_tif = TIFFOpen (m_filename.c_str(), "rm");
#endif
        TIFFSetDirectory (m_tif, m_subimage);

        // A few tidbits to look for
        ImageIOParameter *p;
        if ((p = m_spec.find_attribute ("Exif:ColorSpace", TypeDesc::INT))) {
            // Exif spec says that anything other than 0xffff==uncalibrated
            // should be interpreted to be sRGB.
            if (*(const int *)p->data() != 0xffff)
                m_spec.attribute ("oiio:ColorSpace", "sRGB");
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

    // If Software and IPTC:OriginatingProgram are identical, kill the latter
    if (m_spec.get_string_attribute("Software") == m_spec.get_string_attribute("IPTC:OriginatingProgram"))
        m_spec.erase_attribute ("IPTC:OriginatingProgram");

    std::string desc = m_spec.get_string_attribute ("ImageDescription");
    // If ImageDescription and IPTC:Caption are identical, kill the latter
    if (desc == m_spec.get_string_attribute("IPTC:Caption"))
        m_spec.erase_attribute ("IPTC:Caption");

    // Because TIFF doesn't support arbitrary metadata, we look for certain
    // hints in the ImageDescription and turn them into metadata.
    bool updatedDesc = false;
    static const char *fp_number_pattern =
            "([+-]?((?:(?:[[:digit:]]*\\.)?[[:digit:]]+(?:[eE][+-]?[[:digit:]]+)?)))";
    size_t found;
    found = desc.rfind ("oiio:ConstantColor=");
    if (found != std::string::npos) {
        size_t begin = desc.find_first_of ('=', found) + 1;
        size_t end = desc.find_first_of (' ', begin);
        string_view s = string_view (desc.data()+begin, end-begin);
        m_spec.attribute ("oiio:ConstantColor", s);
        const std::string constcolor_pattern =
            std::string ("oiio:ConstantColor=(\\[?") + fp_number_pattern + ",?)+\\]?[ ]*";
        desc = boost::regex_replace (desc, boost::regex(constcolor_pattern), "");
        updatedDesc = true;
    }
    found = desc.rfind ("oiio:AverageColor=");
    if (found != std::string::npos) {
        size_t begin = desc.find_first_of ('=', found) + 1;
        size_t end = desc.find_first_of (' ', begin);
        string_view s = string_view (desc.data()+begin, end-begin);
        m_spec.attribute ("oiio:AverageColor", s);
        const std::string average_pattern =
            std::string ("oiio:AverageColor=(\\[?") + fp_number_pattern + ",?)+\\]?[ ]*";
        desc = boost::regex_replace (desc, boost::regex(average_pattern), "");
        updatedDesc = true;
    }
    found = desc.rfind ("oiio:SHA-1=");
    if (found == std::string::npos)  // back compatibility with < 1.5
        found = desc.rfind ("SHA-1=");
    if (found != std::string::npos) {
        size_t begin = desc.find_first_of ('=', found) + 1;
        size_t end = begin+40;
        string_view s = string_view (desc.data()+begin, end-begin);
        m_spec.attribute ("oiio:SHA-1", s);
        desc = boost::regex_replace (desc, boost::regex("oiio:SHA-1=[[:xdigit:]]*[ ]*"), "");
        desc = boost::regex_replace (desc, boost::regex("SHA-1=[[:xdigit:]]*[ ]*"), "");
        updatedDesc = true;
    }
    if (updatedDesc) {
        if (desc.size())
            m_spec.attribute ("ImageDescription", desc);
        else
            m_spec.erase_attribute ("ImageDescription");
    }

    if (m_testopenconfig)  // open-with-config debugging
        m_spec.attribute ("oiio:DebugOpenConfig!", 42);
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
TIFFInput::separate_to_contig (int nplanes, int nvals,
                               const unsigned char *separate,
                               unsigned char *contig)
{
    int channelbytes = m_spec.channel_bytes();
    for (int p = 0;  p < nvals;  ++p)                     // loop over pixels
        for (int c = 0;  c < nplanes;  ++c)   // loop over channels
            for (int i = 0;  i < channelbytes;  ++i)  // loop over data bytes
                contig[(p*nplanes+c)*channelbytes+i] =
                    separate[(c*nvals+p)*channelbytes+i];
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



template <typename T>
static void
cmyk_to_rgb (int n, const T *cmyk, size_t cmyk_stride,
             T *rgb, size_t rgb_stride)
{
    for ( ; n; --n, cmyk += cmyk_stride, rgb += rgb_stride) {
        float C = convert_type<T,float>(cmyk[0]);
        float M = convert_type<T,float>(cmyk[1]);
        float Y = convert_type<T,float>(cmyk[2]);
        float K = convert_type<T,float>(cmyk[3]);
        float R = (1.0f - C) * (1.0f - K);
        float G = (1.0f - M) * (1.0f - K);
        float B = (1.0f - Y) * (1.0f - K);
        rgb[0] = convert_type<float,T>(R);
        rgb[1] = convert_type<float,T>(G);
        rgb[2] = convert_type<float,T>(B);
    }
}



bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
    y -= m_spec.y;

    if (m_use_rgba_interface) {
        // We punted and used the RGBA image interface -- copy from buffer.
        // libtiff has no way to read just one scanline as RGBA. So we
        // buffer the whole image.
        if (! m_rgbadata.size()) { // first time through: allocate & read
            m_rgbadata.resize (m_spec.width * m_spec.height * m_spec.depth);
            bool ok = TIFFReadRGBAImageOriented (m_tif, m_spec.width, m_spec.height,
                                       &m_rgbadata[0], ORIENTATION_TOPLEFT, 0);
            if (! ok) {
                error ("Unknown error trying to read TIFF as RGBA");
                return false;
            }
        }
        copy_image (m_spec.nchannels, m_spec.width, 1, 1,
                    &m_rgbadata[y*m_spec.width], m_spec.nchannels,
                    4, 4*m_spec.width, AutoStride,
                    data, m_spec.nchannels, m_spec.width*m_spec.nchannels, AutoStride);
        return true;
    }

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
                error ("%s", oiio_tiff_last_error());
                return false;
            }
            ++m_next_scanline;
        }
    }
    m_next_scanline = y+1;

    int nvals = m_spec.width * m_inputchannels;
    m_scratch.resize (nvals * m_spec.format.size());
    bool need_bit_convert = (m_bitspersample != 8 && m_bitspersample != 16 &&
                             m_bitspersample != 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadScanline (m_tif, &m_scratch[0], y) < 0) {
            error ("%s", oiio_tiff_last_error());
            return false;
        }
        palette_to_rgb (m_spec.width, &m_scratch[0], (unsigned char *)data);
        return true;
    }
    // Not palette...

    int plane_bytes = m_spec.width * m_spec.format.size();
    int planes = m_separate ? m_inputchannels : 1;
    int input_bytes = plane_bytes * m_inputchannels;
    // Where to read?  Directly into user data if no channel shuffling, bit
    // shifting, or CMYK conversion is needed, otherwise into scratch space.
    unsigned char *readbuf = (unsigned char *)data;
    if (need_bit_convert || m_separate || m_photometric == PHOTOMETRIC_SEPARATED)
        readbuf = &m_scratch[0];
    // Perform the reads.  Note that for contig, planes==1, so it will
    // only do one TIFFReadScanline.
    for (int c = 0;  c < planes;  ++c) { /* planes==1 for contig */
        if (TIFFReadScanline (m_tif, &readbuf[plane_bytes*c], y, c) < 0) {
            error ("%s", oiio_tiff_last_error());
            return false;
        }
    }

    // Handle less-than-full bit depths
    if (m_bitspersample < 8) {
        // m_scratch now holds nvals n-bit values, contig or separate
        m_scratch2.resize (input_bytes);
        m_scratch.swap (m_scratch2);
        for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
            bit_convert (m_separate ? m_spec.width : nvals,
                         &m_scratch2[plane_bytes*c], m_bitspersample,
                         m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 8);
    } else if (m_bitspersample > 8 && m_bitspersample < 16) {
        // m_scratch now holds nvals n-bit values, contig or separate
        m_scratch2.resize (input_bytes);
        m_scratch.swap (m_scratch2);
        for (int c = 0;  c < planes;  ++c)  /* planes==1 for contig */
            bit_convert (m_separate ? m_spec.width : nvals,
                         &m_scratch2[plane_bytes*c], m_bitspersample,
                         m_separate ? &m_scratch[plane_bytes*c] : (unsigned char *)data+plane_bytes*c, 16);
    }

    // Handle "separate" planarconfig
    if (m_separate) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB).
        // We know the data is in m_scratch at this point, so 
        // contiguize it into the user data area.
        if (m_photometric == PHOTOMETRIC_SEPARATED) {
            // CMYK->RGB means we need temp storage.
            m_scratch2.resize (input_bytes);
            separate_to_contig (planes, m_spec.width, &m_scratch[0], &m_scratch2[0]);
            m_scratch.swap (m_scratch2);
        } else {
            // If no CMYK->RGB conversion is necessary, we can "separate"
            // straight into the data area.
            separate_to_contig (planes, m_spec.width, &m_scratch[0], (unsigned char *)data);
        }
    }

    // Handle CMYK
    if (m_photometric == PHOTOMETRIC_SEPARATED) {
        // The CMYK will be in m_scratch.
        if (spec().format == TypeDesc::UINT8) {
            cmyk_to_rgb (m_spec.width, (unsigned char *)&m_scratch[0], m_inputchannels,
                         (unsigned char *)data, m_spec.nchannels);
        } else if (spec().format == TypeDesc::UINT16) {
            cmyk_to_rgb (m_spec.width, (unsigned short *)&m_scratch[0], m_inputchannels,
                         (unsigned short *)data, m_spec.nchannels);
        } else {
            error ("CMYK only supported for UINT8, UINT16");
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (nvals, data);

    return true;
}



bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
    x -= m_spec.x;
    y -= m_spec.y;

    if (m_use_rgba_interface) {
        // We punted and used the RGBA image interface
        // libtiff has a call to read just one tile as RGBA. So that's all
        // we need to do, not buffer the whole image.
        m_rgbadata.resize (m_spec.tile_pixels() * 4);
        bool ok = TIFFReadRGBATile (m_tif, x, y, &m_rgbadata[0]);
        if (!ok) {
            error ("Unknown error trying to read TIFF as RGBA");
            return false;
        }
        // Copy, and use stride magic to reverse top-to-bottom
        int tw = std::min (m_spec.tile_width, m_spec.width-x);
        int th = std::min (m_spec.tile_height, m_spec.height-y);
        copy_image (m_spec.nchannels, tw, th, 1,
                    &m_rgbadata[(th-1)*m_spec.tile_width], m_spec.nchannels,
                    4, -m_spec.tile_width*4, AutoStride,
                    data, m_spec.nchannels, m_spec.nchannels*m_spec.tile_width,
                    AutoStride);
        return true;
    }

    imagesize_t tile_pixels = m_spec.tile_pixels();
    imagesize_t nvals = tile_pixels * m_spec.nchannels;
    m_scratch.resize (m_spec.tile_bytes());
    bool no_bit_convert = (m_bitspersample == 8 || m_bitspersample == 16 ||
                           m_bitspersample == 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadTile (m_tif, &m_scratch[0], x, y, z, 0) < 0) {
            error ("%s", oiio_tiff_last_error());
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
                error ("%s", oiio_tiff_last_error());
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
            separate_to_contig (planes, tile_pixels, &m_scratch[0], (unsigned char *)data);
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric (nvals, data);

    return true;
}



bool TIFFInput::read_scanline (int y, int z, TypeDesc format, void *data,
                               stride_t xstride)
{
    bool ok = ImageInput::read_scanline (y, z, format, data, xstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        OIIO::premult (m_spec.nchannels, m_spec.width, 1, 1,
                       0 /*chbegin*/, m_spec.nchannels /*chend*/,
                       format, data, xstride, AutoStride, AutoStride,
                       m_spec.alpha_channel, m_spec.z_channel);
    }
    return ok;
}



bool TIFFInput::read_scanlines (int ybegin, int yend, int z,
                                int chbegin, int chend,
                                TypeDesc format, void *data,
                                stride_t xstride, stride_t ystride)
{
    bool ok = ImageInput::read_scanlines (ybegin, yend, z, chbegin, chend,
                                          format, data, xstride, ystride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        OIIO::premult (m_spec.nchannels, m_spec.width, yend-ybegin, 1,
                       chbegin, chend, format, data,
                       xstride, ystride, AutoStride,
                       m_spec.alpha_channel, m_spec.z_channel);
    }
    return ok;
}



bool TIFFInput::read_tile (int x, int y, int z, TypeDesc format, void *data,
                           stride_t xstride, stride_t ystride, stride_t zstride)
{
    bool ok = ImageInput::read_tile (x, y, z, format, data,
                                     xstride, ystride, zstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        OIIO::premult (m_spec.nchannels, m_spec.tile_width, m_spec.tile_height,
                       std::max (1, m_spec.tile_depth),
                       0, m_spec.nchannels, format, data,
                       xstride, AutoStride, AutoStride,
                       m_spec.alpha_channel, m_spec.z_channel);
    }
    return ok;
}



bool TIFFInput::read_tiles (int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend,
                            int chbegin, int chend,
                            TypeDesc format, void *data,
                            stride_t xstride, stride_t ystride, stride_t zstride)
{
    bool ok = ImageInput::read_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                      chbegin, chend, format, data,
                                      xstride, ystride, zstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        OIIO::premult (m_spec.nchannels, m_spec.tile_width, m_spec.tile_height,
                       std::max (1, m_spec.tile_depth),
                       chbegin, chend, format, data,
                       xstride, AutoStride, AutoStride,
                       m_spec.alpha_channel, m_spec.z_channel);
    }
    return ok;
}





OIIO_PLUGIN_NAMESPACE_END

