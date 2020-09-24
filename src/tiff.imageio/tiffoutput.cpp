// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>

#include <tiffio.h>
#include <zlib.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/tiffutils.h>
#include <OpenImageIO/timer.h>


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace {
// This is the default interval between checkpoints.
// While these are cheap, we need to throttle them
// so we don't checkpoint too often... (each checkpoint
// re-writes the tiff header and any new tiles / scanlines)

static double DEFAULT_CHECKPOINT_INTERVAL_SECONDS = 5.0;
static int MIN_SCANLINES_OR_TILES_PER_CHECKPOINT  = 64;
}  // namespace

class TIFFOutput final : public ImageOutput {
public:
    TIFFOutput();
    virtual ~TIFFOutput();
    virtual const char* format_name(void) const override { return "tiff"; }
    virtual int supports(string_view feature) const override;
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual bool close() override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                                 const void* data, stride_t xstride,
                                 stride_t ystride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool write_tiles(int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, TypeDesc format,
                             const void* data, stride_t xstride = AutoStride,
                             stride_t ystride = AutoStride,
                             stride_t zstride = AutoStride) override;

private:
    TIFF* m_tif;
    std::vector<unsigned char> m_scratch;
    Timer m_checkpointTimer;
    int m_checkpointItems;
    unsigned int m_dither;
    // The following fields are describing what we are writing to the file,
    // not what's in the client's view of the buffer.
    int m_planarconfig;
    int m_compression;
    int m_predictor;
    int m_photometric;
    int m_rowsperstrip;
    unsigned int m_bitspersample;  ///< Of the *file*, not the client's view
    int m_outputchans;             // Number of channels for the output
    bool m_convert_rgb_to_cmyk;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_tif                 = NULL;
        m_checkpointItems     = 0;
        m_compression         = COMPRESSION_ADOBE_DEFLATE;
        m_predictor           = PREDICTOR_NONE;
        m_photometric         = PHOTOMETRIC_RGB;
        m_rowsperstrip        = 32;
        m_outputchans         = 0;
        m_convert_rgb_to_cmyk = false;
    }

    // Convert planar contiguous to planar separate data format
    void contig_to_separate(int n, int nchans, const char* contig,
                            char* separate);

    // Convert RGB to CMYK.
    void* convert_to_cmyk(int npixels, const void* data,
                          std::vector<unsigned char>& cmyk);

    // Fix unusual bit depths
    void fix_bitdepth(void* data, int nvals);

    // Add a parameter to the output
    bool put_parameter(const std::string& name, TypeDesc type,
                       const void* data);
    bool write_exif_data();

    // Make our best guess about whether the spec is describing data that
    // is in true CMYK values.
    bool source_is_cmyk(const ImageSpec& spec);
    // Are we fairly certain that the spec is describing RGB values?
    bool source_is_rgb(const ImageSpec& spec);

    // If we're at scanline y, where does the next strip start?
    int next_strip_boundary(int y)
    {
        return round_to_multiple(y - m_spec.y, m_rowsperstrip) + m_spec.y;
    }

    bool is_strip_boundary(int y)
    {
        return y == next_strip_boundary(y) || y == m_spec.height;
    }

    // Copy a height x width x chans region of src to dst, applying a
    // horizontal predictor to each row. It is permitted for src and dst to
    // be the same.
    template<typename T>
    void horizontal_predictor(T* dst, const T* src, int chans, int width,
                              int height)
    {
        for (int y = 0; y < height;
             ++y, src += chans * width, dst += chans * width)
            for (int c = 0; c < chans; ++c) {
                for (int x = width - 1; x >= 1; --x)
                    dst[x * chans + c] = src[x * chans + c]
                                         - src[(x - 1) * chans + c];
                dst[c] = src[c];  // element 0
            }
    }

    void compress_one_strip(void* uncompressed_buf, size_t strip_bytes,
                            void* compressed_buf, unsigned long cbound,
                            int channels, int width, int height,
                            unsigned long* compressed_size, bool* ok);

    int tile_index(int x, int y, int z)
    {
        int xtile   = (x - m_spec.x) / m_spec.tile_width;
        int ytile   = (y - m_spec.y) / m_spec.tile_height;
        int ztile   = (z - m_spec.z) / m_spec.tile_depth;
        int nxtiles = (m_spec.width + m_spec.tile_width - 1)
                      / m_spec.tile_width;
        int nytiles = (m_spec.height + m_spec.tile_height - 1)
                      / m_spec.tile_height;
        return xtile + ytile * nxtiles + ztile * nxtiles * nytiles;
    }

    // Move data to scratch area if not already there.
    void* move_to_scratch(const void* data, size_t nbytes)
    {
        if (m_scratch.empty() || (const unsigned char*)data != m_scratch.data())
            m_scratch.assign((const unsigned char*)data,
                             (const unsigned char*)data + nbytes);
        return m_scratch.data();
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
tiff_output_imageio_create()
{
    return new TIFFOutput;
}

OIIO_EXPORT int tiff_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
tiff_imageio_library_version()
{
    std::string v(TIFFGetVersion());
    v = v.substr(0, v.find('\n'));
    v = Strutil::replace(v, ", ", " ");
    return ustring(v).c_str();
}

OIIO_EXPORT const char* tiff_output_extensions[]
    = { "tif", "tiff", "tx", "env", "sm", "vsm", nullptr };

OIIO_PLUGIN_EXPORTS_END



extern std::string&
oiio_tiff_last_error();
extern void
oiio_tiff_set_error_handler();



struct CompressionCode {
    int code;
    const char* name;
};

// We comment out a lot of these because we only support a subset for
// writing. These can be uncommented on a case by case basis as they are
// thoroughly tested and included in the testsuite.
static CompressionCode tiff_compressions[] = {
    { COMPRESSION_NONE, "none" },          // no compression
    { COMPRESSION_LZW, "lzw" },            // LZW
    { COMPRESSION_ADOBE_DEFLATE, "zip" },  // deflate / zip
    { COMPRESSION_DEFLATE, "zip" },        // deflate / zip
    { COMPRESSION_CCITTRLE, "ccittrle" },  // CCITT RLE
    //  { COMPRESSION_CCITTFAX3,     "ccittfax3" },   // CCITT group 3 fax
    //  { COMPRESSION_CCITT_T4,      "ccitt_t4" },    // CCITT T.4
    //  { COMPRESSION_CCITTFAX4,     "ccittfax4" },   // CCITT group 4 fax
    //  { COMPRESSION_CCITT_T6,      "ccitt_t6" },    // CCITT T.6
    //  { COMPRESSION_OJPEG,         "ojpeg" },       // old (pre-TIFF6.0) JPEG
    { COMPRESSION_JPEG, "jpeg" },  // JPEG
    //  { COMPRESSION_NEXT,          "next" },        // NeXT 2-bit RLE
    //  { COMPRESSION_CCITTRLEW,     "ccittrle2" },   // #1 w/ word alignment
    { COMPRESSION_PACKBITS, "packbits" },  // Macintosh RLE
//  { COMPRESSION_THUNDERSCAN,   "thunderscan" }, // ThundeScan RLE
//  { COMPRESSION_IT8CTPAD,      "IT8CTPAD" },    // IT8 CT w/ patting
//  { COMPRESSION_IT8LW,         "IT8LW" },       // IT8 linework RLE
//  { COMPRESSION_IT8MP,         "IT8MP" },       // IT8 monochrome picture
//  { COMPRESSION_IT8BL,         "IT8BL" },       // IT8 binary line art
//  { COMPRESSION_PIXARFILM,     "pixarfilm" },   // Pixar 10 bit LZW
//  { COMPRESSION_PIXARLOG,      "pixarlog" },    // Pixar 11 bit ZIP
//  { COMPRESSION_DCS,           "dcs" },         // Kodak DCS encoding
//  { COMPRESSION_JBIG,          "isojbig" },     // ISO JBIG
//  { COMPRESSION_SGILOG,        "sgilog" },      // SGI log luminance RLE
//  { COMPRESSION_SGILOG24,      "sgilog24" },    // SGI log 24bit
//  { COMPRESSION_JP2000,        "jp2000" },      // Leadtools JPEG2000
#if defined(TIFF_VERSION_BIG) && TIFFLIB_VERSION >= 20120922
// Others supported in more recent TIFF library versions.
//  { COMPRESSION_T85,           "T85" },         // TIFF/FX T.85 JBIG
//  { COMPRESSION_T43,           "T43" },         // TIFF/FX T.43 color layered JBIG
//  { COMPRESSION_LZMA,          "lzma" },        // LZMA2
#endif
    { -1, NULL }
};

static int
tiff_compression_code(string_view name)
{
    for (int i = 0; tiff_compressions[i].name; ++i)
        if (Strutil::iequals(name, tiff_compressions[i].name))
            return tiff_compressions[i].code;
    return COMPRESSION_ADOBE_DEFLATE;  // default
}



TIFFOutput::TIFFOutput()
{
    oiio_tiff_set_error_handler();
    init();
}



TIFFOutput::~TIFFOutput()
{
    // Close, if not already done.
    close();
}



int
TIFFOutput::supports(string_view feature) const
{
    if (feature == "tiles")
        return true;
    if (feature == "multiimage")
        return true;
    if (feature == "appendsubimage")
        return true;
    if (feature == "alpha")
        return true;
    if (feature == "nchannels")
        return true;
    // N.B. TIFF doesn't support "displaywindow", since it has no tags for
    // the equivalent of full_x, full_y.
    if (feature == "origin")
        return true;
    // N.B. TIFF doesn't support "negativeorigin"
    if (feature == "exif")
        return true;
    if (feature == "iptc")
        return true;
    // N.B. TIFF doesn't support arbitrary metadata.

    // FIXME: we could support "volumes" and "empty"

    // Everything else, we either don't support or don't know about
    return false;
}


#define ICC_PROFILE_ATTR "ICCProfile"


// Do all elements of vector d have value v?
template<typename T>
inline bool
allval(const std::vector<T>& d, T v = T(0))
{
    return std::all_of(d.begin(), d.end(), [&](const T& a) { return a == v; });
}



bool
TIFFOutput::open(const std::string& name, const ImageSpec& userspec,
                 OpenMode mode)
{
    if (mode == AppendMIPLevel) {
        errorf("%s does not support MIP levels", format_name());
        return false;
    }

    close();            // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        errorf("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.tile_width) {
        if (m_spec.tile_width % 16 != 0 || m_spec.tile_height % 16 != 0
            || m_spec.tile_height == 0) {
            errorf("Tile size must be a multiple of 16, you asked for %d x %d",
                   m_spec.tile_width, m_spec.tile_height);
            return false;
        }
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.channelformats.size()) {
        if (allval(m_spec.channelformats, m_spec.format))
            m_spec.channelformats.clear();
        else {
            errorf("%s does not support per-channel data formats",
                   format_name());
            return false;
        }
    }

    // Open the file
#ifdef _WIN32
    std::wstring wname = Strutil::utf8_to_utf16(name);
    m_tif = TIFFOpenW(wname.c_str(), mode == AppendSubimage ? "a" : "w");
#else
    m_tif = TIFFOpen(name.c_str(), mode == AppendSubimage ? "a" : "w");
#endif
    if (!m_tif) {
        errorf("Could not open \"%s\"", name);
        return false;
    }

    TIFFSetField(m_tif, TIFFTAG_IMAGEWIDTH, m_spec.width);
    TIFFSetField(m_tif, TIFFTAG_IMAGELENGTH, m_spec.height);

    // Handle display window or "full" size. Note that TIFF can't represent
    // nonzero offsets of the full size, so we may need to expand the
    // display window to encompass the origin.
    if ((m_spec.full_width != 0 || m_spec.full_height != 0)
        && (m_spec.full_width != m_spec.width
            || m_spec.full_height != m_spec.height || m_spec.full_x != 0
            || m_spec.full_y != 0)) {
        TIFFSetField(m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH,
                     m_spec.full_width + m_spec.full_x);
        TIFFSetField(m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH,
                     m_spec.full_height + m_spec.full_y);
    }

    if (m_spec.tile_width) {
        TIFFSetField(m_tif, TIFFTAG_TILEWIDTH, m_spec.tile_width);
        TIFFSetField(m_tif, TIFFTAG_TILELENGTH, m_spec.tile_height);
    } else {
        // Scanline images must set rowsperstrip
        m_rowsperstrip = 32;
        TIFFSetField(m_tif, TIFFTAG_ROWSPERSTRIP, m_rowsperstrip);
    }
    TIFFSetField(m_tif, TIFFTAG_SAMPLESPERPIXEL, m_spec.nchannels);
    int orientation = m_spec.get_int_attribute("Orientation", 1);
    TIFFSetField(m_tif, TIFFTAG_ORIENTATION, orientation);

    m_bitspersample = m_spec.get_int_attribute("oiio:BitsPerSample");
    int sampformat;
    switch (m_spec.format.basetype) {
    case TypeDesc::INT8:
        m_bitspersample = 8;
        sampformat      = SAMPLEFORMAT_INT;
        break;
    case TypeDesc::UINT8:
        if (m_bitspersample != 2 && m_bitspersample != 4 && m_bitspersample != 6
            && m_bitspersample != 1)
            m_bitspersample = 8;
        sampformat = SAMPLEFORMAT_UINT;
        break;
    case TypeDesc::INT16:
        m_bitspersample = 16;
        sampformat      = SAMPLEFORMAT_INT;
        break;
    case TypeDesc::UINT16:
        if (m_bitspersample != 10 && m_bitspersample != 12
            && m_bitspersample != 14)
            m_bitspersample = 16;
        sampformat = SAMPLEFORMAT_UINT;
        break;
    case TypeDesc::INT32:
        m_bitspersample = 32;
        sampformat      = SAMPLEFORMAT_INT;
        break;
    case TypeDesc::UINT32:
        if (m_bitspersample != 24)
            m_bitspersample = 32;
        sampformat = SAMPLEFORMAT_UINT;
        break;
    case TypeDesc::HALF:
        // Adobe extension, see http://chriscox.org/TIFFTN3d1.pdf
        // Unfortunately, Nuke 9.0, and probably many other apps we care
        // about, cannot read 16 bit float TIFFs correctly. Revisit this
        // again in future releases. (comment added Feb 2015)
        // For now, the default is to NOT write this (instead writing float)
        // unless the "tiff:half" attribute is nonzero -- use the global
        // OIIO attribute, but override with a specific attribute for this
        // file.
        if (m_spec.get_int_attribute("tiff:half",
                                     OIIO::get_int_attribute("tiff:half"))) {
            m_bitspersample = 16;
        } else {
            // Silently change requests for unsupported 'half' to 'float'
            m_bitspersample = 32;
            m_spec.set_format(TypeDesc::FLOAT);
        }
        sampformat = SAMPLEFORMAT_IEEEFP;
        break;
    case TypeDesc::FLOAT:
        m_bitspersample = 32;
        sampformat      = SAMPLEFORMAT_IEEEFP;
        break;
    case TypeDesc::DOUBLE:
        m_bitspersample = 64;
        sampformat      = SAMPLEFORMAT_IEEEFP;
        break;
    default:
        // Everything else, including UNKNOWN -- default to 8 bit
        m_bitspersample = 8;
        sampformat      = SAMPLEFORMAT_UINT;
        m_spec.set_format(TypeDesc::UINT8);
        break;
    }
    TIFFSetField(m_tif, TIFFTAG_BITSPERSAMPLE, m_bitspersample);
    TIFFSetField(m_tif, TIFFTAG_SAMPLEFORMAT, sampformat);

    m_photometric = (m_spec.nchannels >= 3 ? PHOTOMETRIC_RGB
                                           : PHOTOMETRIC_MINISBLACK);

    string_view comp;
    int qual;
    std::tie(comp, qual) = m_spec.decode_compression_metadata("zip");

    if (Strutil::iequals(comp, "jpeg")
        && (m_spec.format != TypeDesc::UINT8 || m_spec.nchannels != 3)) {
        comp = "zip";  // can't use JPEG for anything but 3xUINT8
        qual = -1;
    }
    m_compression = tiff_compression_code(comp);
    TIFFSetField(m_tif, TIFFTAG_COMPRESSION, m_compression);

    // Use predictor when using compression
    m_predictor = PREDICTOR_NONE;
    if (m_compression == COMPRESSION_LZW
        || m_compression == COMPRESSION_ADOBE_DEFLATE) {
        if (m_spec.format == TypeDesc::FLOAT
            || m_spec.format == TypeDesc::DOUBLE
            || m_spec.format == TypeDesc::HALF) {
            m_predictor = PREDICTOR_FLOATINGPOINT;
            // N.B. Very old versions of libtiff did not support this
            // predictor.  It's possible that certain apps can't read
            // floating point TIFFs with this set.  But since it's been
            // documented since 2005, let's take our chances.  Comment
            // out the above line if this is problematic.
        } else if (m_bitspersample == 8 || m_bitspersample == 16) {
            // predictors not supported for unusual bit depths (e.g. 10)
            m_predictor = PREDICTOR_HORIZONTAL;
        }
        if (m_predictor != PREDICTOR_NONE)
            TIFFSetField(m_tif, TIFFTAG_PREDICTOR, m_predictor);
        if (m_compression == COMPRESSION_ADOBE_DEFLATE) {
            qual = m_spec.get_int_attribute("tiff:zipquality", qual);
            if (qual > 0)
                TIFFSetField(m_tif, TIFFTAG_ZIPQUALITY,
                             OIIO::clamp(qual, 1, 9));
        }
    } else if (m_compression == COMPRESSION_JPEG) {
        if (qual <= 0)
            qual = 95;
        qual = OIIO::clamp(qual, 1, 100);
        TIFFSetField(m_tif, TIFFTAG_JPEGQUALITY, qual);
        m_rowsperstrip = 64;
        m_spec.attribute("tiff:RowsPerStrip", m_rowsperstrip);
        TIFFSetField(m_tif, TIFFTAG_ROWSPERSTRIP, m_rowsperstrip);
        if (m_photometric == PHOTOMETRIC_RGB) {
            // Compression works so much better when we ask the library to
            // auto-convert RGB to YCbCr.
            TIFFSetField(m_tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
            m_photometric = PHOTOMETRIC_YCBCR;
        }
    }
    m_outputchans = m_spec.nchannels;
    if (m_photometric == PHOTOMETRIC_RGB) {
        // There are a few ways in which we allow allow the user to specify
        // translation to different photometric types.
        string_view photo = m_spec.get_string_attribute("tiff:ColorSpace");
        if (Strutil::iequals(photo, "CMYK")
            || Strutil::iequals(photo, "color separated")) {
            // User has requested via the "tiff:ColorSpace" attribute that
            // the file be written as color separated channels.
            m_photometric = PHOTOMETRIC_SEPARATED;
            if (m_spec.format != TypeDesc::UINT8
                || m_spec.format != TypeDesc::UINT16) {
                m_spec.format   = TypeDesc::UINT8;
                m_bitspersample = 8;
                TIFFSetField(m_tif, TIFFTAG_BITSPERSAMPLE, m_bitspersample);
                TIFFSetField(m_tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
            }
            if (source_is_rgb(m_spec)) {
                // Case: RGB -> CMYK, do the conversions per pixel
                m_convert_rgb_to_cmyk = true;
                m_outputchans         = 4;  // output 4, not 3 chans
                TIFFSetField(m_tif, TIFFTAG_SAMPLESPERPIXEL, m_outputchans);
                TIFFSetField(m_tif, TIFFTAG_INKSET, INKSET_CMYK);
            } else if (source_is_cmyk(m_spec)) {
                // Case: CMYK -> CMYK (do not transform)
                m_convert_rgb_to_cmyk = false;
                TIFFSetField(m_tif, TIFFTAG_INKSET, INKSET_CMYK);
            } else {
                // Case: arbitrary inks
                m_convert_rgb_to_cmyk = false;
                TIFFSetField(m_tif, TIFFTAG_INKSET, INKSET_MULTIINK);
                std::string inknames;
                for (int i = 0; i < m_spec.nchannels; ++i) {
                    if (i)
                        inknames.insert(inknames.size(), 1, '\0');
                    if (i < (int)m_spec.channelnames.size())
                        inknames.insert(inknames.size(),
                                        m_spec.channelnames[i]);
                    else
                        inknames.insert(inknames.size(),
                                        Strutil::sprintf("ink%d", i));
                }
                TIFFSetField(m_tif, TIFFTAG_INKNAMES, int(inknames.size() + 1),
                             &inknames[0]);
                TIFFSetField(m_tif, TIFFTAG_NUMBEROFINKS, m_spec.nchannels);
            }
        }
    }

    TIFFSetField(m_tif, TIFFTAG_PHOTOMETRIC, m_photometric);

    // ExtraSamples tag
    if ((m_spec.alpha_channel >= 0 || m_spec.nchannels > 3)
        && m_photometric != PHOTOMETRIC_SEPARATED) {
        bool unass = m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);
        int defaultchans = m_spec.nchannels >= 3 ? 3 : 1;
        short e          = m_spec.nchannels - defaultchans;
        std::vector<unsigned short> extra(e);
        for (int c = 0; c < e; ++c) {
            if (m_spec.alpha_channel == (c + defaultchans))
                extra[c] = unass ? EXTRASAMPLE_UNASSALPHA
                                 : EXTRASAMPLE_ASSOCALPHA;
            else
                extra[c] = EXTRASAMPLE_UNSPECIFIED;
        }
        TIFFSetField(m_tif, TIFFTAG_EXTRASAMPLES, e, &extra[0]);
    }

    ParamValue* param;
    const char* str = NULL;

    // Did the user request separate planar configuration?
    m_planarconfig = PLANARCONFIG_CONTIG;
    if ((param = m_spec.find_attribute("planarconfig", TypeDesc::STRING))
        || (param = m_spec.find_attribute("tiff:planarconfig",
                                          TypeDesc::STRING))) {
        str = *(char**)param->data();
        if (str && Strutil::iequals(str, "separate"))
            m_planarconfig = PLANARCONFIG_SEPARATE;
    }
    // Can't deal with the headache of separate image planes when using
    // bit packing, or CMYK. Just punt by forcing contig in those cases.
    if (m_bitspersample != spec().format.size() * 8
        || m_photometric == PHOTOMETRIC_SEPARATED)
        m_planarconfig = PLANARCONFIG_CONTIG;
    if (m_planarconfig == PLANARCONFIG_SEPARATE) {
        if (!m_spec.tile_width) {
            // I can only seem to make separate planarconfig work when
            // rowsperstrip is 1.
            m_rowsperstrip = 1;
            TIFFSetField(m_tif, TIFFTAG_ROWSPERSTRIP, 1);
        }
    }
    TIFFSetField(m_tif, TIFFTAG_PLANARCONFIG, m_planarconfig);

    // Automatically set date field if the client didn't supply it.
    if (!m_spec.find_attribute("DateTime")) {
        time_t now;
        time(&now);
        struct tm mytm;
        Sysutil::get_local_time(&now, &mytm);
        std::string date = Strutil::sprintf("%4d:%02d:%02d %02d:%02d:%02d",
                                            mytm.tm_year + 1900,
                                            mytm.tm_mon + 1, mytm.tm_mday,
                                            mytm.tm_hour, mytm.tm_min,
                                            mytm.tm_sec);
        m_spec.attribute("DateTime", date);
    }

    // Write ICC profile, if we have anything
    const ParamValue* icc_profile_parameter = m_spec.find_attribute(
        ICC_PROFILE_ATTR);
    if (icc_profile_parameter != NULL) {
        unsigned char* icc_profile
            = (unsigned char*)icc_profile_parameter->data();
        uint32 length = icc_profile_parameter->type().size();
        if (icc_profile && length)
            TIFFSetField(m_tif, TIFFTAG_ICCPROFILE, length, icc_profile);
    }

    if (Strutil::iequals(m_spec.get_string_attribute("oiio:ColorSpace"), "sRGB"))
        m_spec.attribute("Exif:ColorSpace", 1);

    // Deal with missing XResolution or YResolution, or a PixelAspectRatio
    // that contradicts them.
    float X_density = m_spec.get_float_attribute("XResolution", 1.0f);
    float Y_density = m_spec.get_float_attribute("YResolution", 1.0f);
    // Eliminate nonsensical densities
    if (X_density <= 0.0f)
        X_density = 1.0f;
    if (Y_density <= 0.0f)
        Y_density = 1.0f;
    float aspect = m_spec.get_float_attribute("PixelAspectRatio", 1.0f);
    if (X_density < 1.0f || Y_density < 1.0f
        || aspect * X_density != Y_density) {
        if (X_density < 1.0f || Y_density < 1.0f) {
            X_density = Y_density = 1.0f;
            m_spec.attribute("ResolutionUnit", "none");
        }
        m_spec.attribute("XResolution", X_density);
        m_spec.attribute("YResolution", X_density * aspect);
    }

    if (m_spec.x || m_spec.y) {
        // The TIFF spec implies that the XPOSITION & YPOSITION are in the
        // resolution units. For a long time we just assumed they were whole
        // pixels. Beware! For the sake of old OIIO or other readers that
        // assume pixel units, it may be smart to not have non-1.0
        // XRESOLUTION or YRESOLUTION if you have a non-zero origin.
        //
        // TIFF is internally incapable of having negative origin, so we
        // have to clamp at 0.
        float x = m_spec.x / X_density;
        float y = m_spec.y / Y_density;
        TIFFSetField(m_tif, TIFFTAG_XPOSITION, std::max(0.0f, x));
        TIFFSetField(m_tif, TIFFTAG_YPOSITION, std::max(0.0f, y));
    }

    // Deal with all other params
    for (size_t p = 0; p < m_spec.extra_attribs.size(); ++p)
        put_parameter(m_spec.extra_attribs[p].name().string(),
                      m_spec.extra_attribs[p].type(),
                      m_spec.extra_attribs[p].data());

    std::vector<char> iptc;
    encode_iptc_iim(m_spec, iptc);
    if (iptc.size()) {
        iptc.resize((iptc.size() + 3) & (0xffff - 3));  // round up
        TIFFSetField(m_tif, TIFFTAG_RICHTIFFIPTC, iptc.size() / 4, &iptc[0]);
    }

    std::string xmp = encode_xmp(m_spec, true);
    if (!xmp.empty())
        TIFFSetField(m_tif, TIFFTAG_XMLPACKET, xmp.size(), xmp.c_str());

    TIFFCheckpointDirectory(m_tif);  // Ensure the header is written early
    m_checkpointTimer.reset();
    m_checkpointTimer.start();  // Initialize the to the fileopen time
    m_checkpointItems = 0;      // Number of tiles or scanlines we've written

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    return true;
}



bool
TIFFOutput::put_parameter(const std::string& name, TypeDesc type,
                          const void* data)
{
    if (Strutil::iequals(name, "Artist") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_ARTIST, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "Copyright") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_COPYRIGHT, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "DateTime") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_DATETIME, *(char**)data);
        return true;
    }
    if ((Strutil::iequals(name, "name")
         || Strutil::iequals(name, "DocumentName"))
        && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_DOCUMENTNAME, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "fovcot") && type == TypeDesc::FLOAT) {
        double d = *(float*)data;
        TIFFSetField(m_tif, TIFFTAG_PIXAR_FOVCOT, d);
        return true;
    }
    if ((Strutil::iequals(name, "host")
         || Strutil::iequals(name, "HostComputer"))
        && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_HOSTCOMPUTER, *(char**)data);
        return true;
    }
    if ((Strutil::iequals(name, "description")
         || Strutil::iequals(name, "ImageDescription"))
        && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_IMAGEDESCRIPTION, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "tiff:Predictor") && type == TypeDesc::INT) {
        m_predictor = *(int*)data;
        TIFFSetField(m_tif, TIFFTAG_PREDICTOR, m_predictor);
        return true;
    }
    if (Strutil::iequals(name, "ResolutionUnit") && type == TypeDesc::STRING) {
        const char* s = *(char**)data;
        bool ok       = true;
        if (Strutil::iequals(s, "none"))
            TIFFSetField(m_tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
        else if (Strutil::iequals(s, "in") || Strutil::iequals(s, "inch"))
            TIFFSetField(m_tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
        else if (Strutil::iequals(s, "cm"))
            TIFFSetField(m_tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        else
            ok = false;
        return ok;
    }
    if (Strutil::iequals(name, "tiff:RowsPerStrip")
        && !m_spec.tile_width /* don't set rps for tiled files */
        && m_planarconfig == PLANARCONFIG_CONTIG /* only for contig */) {
        if (type == TypeDesc::INT) {
            m_rowsperstrip = *(int*)data;
        } else if (type == TypeDesc::STRING) {
            // Back-compatibility with Entropy and PRMan
            m_rowsperstrip = Strutil::stoi(*(char**)data);
        } else {
            return false;
        }
        m_rowsperstrip = clamp(m_rowsperstrip, 1, m_spec.height);
        TIFFSetField(m_tif, TIFFTAG_ROWSPERSTRIP, m_rowsperstrip);
        return true;
    }
    if (Strutil::iequals(name, "Make") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_MAKE, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "Model") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_MODEL, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "Software") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_SOFTWARE, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "tiff:SubFileType") && type == TypeDesc::INT) {
        TIFFSetField(m_tif, TIFFTAG_SUBFILETYPE, *(int*)data);
        return true;
    }
    if (Strutil::iequals(name, "textureformat") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "wrapmodes") && type == TypeDesc::STRING) {
        TIFFSetField(m_tif, TIFFTAG_PIXAR_WRAPMODES, *(char**)data);
        return true;
    }
    if (Strutil::iequals(name, "worldtocamera") && type == TypeMatrix) {
        TIFFSetField(m_tif, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, data);
        return true;
    }
    if (Strutil::iequals(name, "worldtoscreen") && type == TypeMatrix) {
        TIFFSetField(m_tif, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, data);
        return true;
    }
    if (Strutil::iequals(name, "XResolution") && type == TypeDesc::FLOAT) {
        TIFFSetField(m_tif, TIFFTAG_XRESOLUTION, *(float*)data);
        return true;
    }
    if (Strutil::iequals(name, "YResolution") && type == TypeDesc::FLOAT) {
        TIFFSetField(m_tif, TIFFTAG_YRESOLUTION, *(float*)data);
        return true;
    }
    return false;
}



bool
TIFFOutput::write_exif_data()
{
#if defined(TIFF_VERSION_BIG) && TIFFLIB_VERSION >= 20120922
    // Older versions of libtiff do not support writing Exif directories

    if (m_spec.get_int_attribute("tiff:write_exif", 1) == 0) {
        // The special metadata "tiff:write_exif", if present and set to 0
        // (the default is 1), will cause us to skip outputting Exif data.
        // This is useful in cases where we think the TIFF file will need to
        // be read by an app that links against an old version of libtiff
        // that will have trouble reading the Exif directory.
        return true;
    }

    // First, see if we have any Exif data at all
    bool any_exif = false;
    for (size_t i = 0, e = m_spec.extra_attribs.size(); i < e; ++i) {
        const ParamValue& p(m_spec.extra_attribs[i]);
        int tag, tifftype, count;
        if (exif_tag_lookup(p.name(), tag, tifftype, count)
            && tifftype != TIFF_NOTYPE) {
            if (tag == EXIF_SECURITYCLASSIFICATION || tag == EXIF_IMAGEHISTORY
                || tag == EXIF_PHOTOGRAPHICSENSITIVITY)
                continue;  // libtiff doesn't understand these
            any_exif = true;
            break;
        }
    }
    if (!any_exif)
        return true;

    if (m_compression == COMPRESSION_JPEG) {
        // For reasons we don't understand, JPEG-compressed TIFF seems
        // to not output properly without a directory checkpoint here.
        TIFFCheckpointDirectory(m_tif);
    }

    // First, finish writing the current directory
    if (!TIFFWriteDirectory(m_tif)) {
        errorf("failed TIFFWriteDirectory()");
        return false;
    }

    // Create an Exif directory
    if (TIFFCreateEXIFDirectory(m_tif) != 0) {
        errorf("failed TIFFCreateEXIFDirectory()");
        return false;
    }

    for (size_t i = 0, e = m_spec.extra_attribs.size(); i < e; ++i) {
        const ParamValue& p(m_spec.extra_attribs[i]);
        int tag, tifftype, count;
        if (exif_tag_lookup(p.name(), tag, tifftype, count)
            && tifftype != TIFF_NOTYPE) {
            if (tag == EXIF_SECURITYCLASSIFICATION || tag == EXIF_IMAGEHISTORY
                || tag == EXIF_PHOTOGRAPHICSENSITIVITY)
                continue;  // libtiff doesn't understand these
            bool ok = false;
            if (tifftype == TIFF_ASCII) {
                ok = TIFFSetField(m_tif, tag, *(char**)p.data());
            } else if ((tifftype == TIFF_SHORT || tifftype == TIFF_LONG)
                       && p.type() == TypeDesc::SHORT) {
                ok = TIFFSetField(m_tif, tag, (int)*(short*)p.data());
            } else if ((tifftype == TIFF_SHORT || tifftype == TIFF_LONG)
                       && p.type() == TypeDesc::INT) {
                ok = TIFFSetField(m_tif, tag, *(int*)p.data());
            } else if ((tifftype == TIFF_RATIONAL || tifftype == TIFF_SRATIONAL)
                       && p.type() == TypeDesc::FLOAT) {
                ok = TIFFSetField(m_tif, tag, *(float*)p.data());
            } else if ((tifftype == TIFF_RATIONAL || tifftype == TIFF_SRATIONAL)
                       && p.type() == TypeDesc::DOUBLE) {
                ok = TIFFSetField(m_tif, tag, *(double*)p.data());
            }
            if (!ok) {
                // std::cout << "Unhandled EXIF " << p.name() << " " << p.type() << "\n";
            }
        }
    }

    // Now write the directory of Exif data
    uint64 dir_offset = 0;
    if (!TIFFWriteCustomDirectory(m_tif, &dir_offset)) {
        errorf("failed TIFFWriteCustomDirectory() of the Exif data");
        return false;
    }
    // Go back to the first directory, and add the EXIFIFD pointer.
    // std::cout << "diffdir = " << tiffdir << "\n";
    TIFFSetDirectory(m_tif, 0);
    TIFFSetField(m_tif, TIFFTAG_EXIFIFD, dir_offset);
#endif

    return true;  // all is ok
}



bool
TIFFOutput::close()
{
    if (m_tif) {
        write_exif_data();
        TIFFClose(m_tif);  // N.B. TIFFClose doesn't return a status code
    }
    init();       // re-initialize
    return true;  // How can we fail?
}



/// Helper: Convert n pixels from contiguous (RGBRGBRGB) to separate
/// (RRRGGGBBB) planarconfig.
void
TIFFOutput::contig_to_separate(int n, int nchans, const char* contig,
                               char* separate)
{
    int channelbytes = m_spec.channel_bytes();
    for (int p = 0; p < n; ++p)                     // loop over pixels
        for (int c = 0; c < nchans; ++c)            // loop over channels
            for (int i = 0; i < channelbytes; ++i)  // loop over data bytes
                separate[(c * n + p) * channelbytes + i]
                    = contig[(p * nchans + c) * channelbytes + i];
}



template<typename T>
static void
rgb_to_cmyk(int n, const T* rgb, size_t rgb_stride, T* cmyk, size_t cmyk_stride)
{
    for (; n; --n, cmyk += cmyk_stride, rgb += rgb_stride) {
        float R               = convert_type<T, float>(rgb[0]);
        float G               = convert_type<T, float>(rgb[1]);
        float B               = convert_type<T, float>(rgb[2]);
        float one_minus_K     = std::max(R, std::max(G, B));
        float one_minus_K_inv = (one_minus_K <= 1e-6) ? 0.0f
                                                      : 1.0f / one_minus_K;
        float C = (one_minus_K - R) * one_minus_K_inv;
        float M = (one_minus_K - G) * one_minus_K_inv;
        float Y = (one_minus_K - B) * one_minus_K_inv;
        float K = 1.0f - one_minus_K;
        cmyk[0] = convert_type<float, T>(C);
        cmyk[1] = convert_type<float, T>(M);
        cmyk[2] = convert_type<float, T>(Y);
        cmyk[3] = convert_type<float, T>(K);
    }
}



void*
TIFFOutput::convert_to_cmyk(int npixels, const void* data,
                            std::vector<unsigned char>& cmyk)
{
    cmyk.resize(m_outputchans * spec().format.size() * npixels);
    if (spec().format == TypeDesc::UINT8) {
        rgb_to_cmyk(npixels, (unsigned char*)data, m_spec.nchannels,
                    (unsigned char*)&cmyk[0], m_outputchans);
    } else if (spec().format == TypeDesc::UINT16) {
        rgb_to_cmyk(npixels, (unsigned short*)data, m_spec.nchannels,
                    (unsigned short*)&cmyk[0], m_outputchans);
    } else {
        OIIO_ASSERT(0 && "CMYK should be forced to UINT8 or UINT16");
    }
    return cmyk.data();
}



bool
TIFFOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                           stride_t xstride)
{
    m_spec.auto_stride(xstride, format, spec().nchannels);
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);

    // Handle weird photometric/color spaces
    std::vector<unsigned char> cmyk;
    if (m_photometric == PHOTOMETRIC_SEPARATED && m_convert_rgb_to_cmyk)
        data = convert_to_cmyk(spec().width, data, cmyk);
    size_t scanline_vals = spec().width * m_outputchans;

    // Handle weird bit depths
    if (spec().format.size() * 8 != m_bitspersample) {
        data = move_to_scratch(data, scanline_vals * spec().format.size());
        fix_bitdepth(m_scratch.data(), scanline_vals);
    }

    y -= m_spec.y;
    if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from contiguous (RGBRGBRGB) to separate (RRRGGGBBB)
        int plane_bytes = m_spec.width * m_spec.format.size();

        std::unique_ptr<char[]> separate_heap;
        char* separate            = nullptr;
        imagesize_t separate_size = plane_bytes * m_outputchans;
        if (separate_size <= (1 << 16))
            separate = OIIO_ALLOCA(char, separate_size);   // <=64k ? stack
        else {                                             // >64k ? heap
            separate_heap.reset(new char[separate_size]);  // will auto-free
            separate = separate_heap.get();
        }

        contig_to_separate(m_spec.width, m_outputchans, (const char*)data,
                           separate);
        for (int c = 0; c < m_outputchans; ++c) {
            if (TIFFWriteScanline(m_tif, (tdata_t)&separate[plane_bytes * c], y,
                                  c)
                < 0) {
                std::string err = oiio_tiff_last_error();
                errorf("TIFFWriteScanline failed writing line y=%d,z=%d (%s)",
                       y, z, err.size() ? err.c_str() : "unknown error");
                return false;
            }
        }
    } else {
        // No contig->separate is necessary.  But we still use scratch
        // space since TIFFWriteScanline is destructive when
        // TIFFTAG_PREDICTOR is used.
        data = move_to_scratch(data, scanline_vals * m_spec.format.size());
        if (TIFFWriteScanline(m_tif, (tdata_t)data, y) < 0) {
            std::string err = oiio_tiff_last_error();
            errorf("TIFFWriteScanline failed writing line y=%d,z=%d (%s)", y, z,
                   err.size() ? err.c_str() : "unknown error");
            return false;
        }
    }

    // Should we checkpoint? Only if we have enough scanlines and enough
    // time has passed (or if using JPEG compression, for which it seems
    // necessary).
    ++m_checkpointItems;
    if ((m_checkpointTimer() > DEFAULT_CHECKPOINT_INTERVAL_SECONDS
         || m_compression == COMPRESSION_JPEG)
        && m_checkpointItems >= MIN_SCANLINES_OR_TILES_PER_CHECKPOINT) {
        TIFFCheckpointDirectory(m_tif);
        m_checkpointTimer.lap();
        m_checkpointItems = 0;
    }

    return true;
}



void
TIFFOutput::compress_one_strip(void* uncompressed_buf, size_t strip_bytes,
                               void* compressed_buf, unsigned long cbound,
                               int channels, int width, int height,
                               unsigned long* compressed_size, bool* ok)
{
    if (m_spec.format == TypeUInt8)
        horizontal_predictor((unsigned char*)uncompressed_buf,
                             (unsigned char*)uncompressed_buf, channels, width,
                             height);
    else if (m_spec.format == TypeUInt16)
        horizontal_predictor((unsigned short*)uncompressed_buf,
                             (unsigned short*)uncompressed_buf, channels, width,
                             height);
    *compressed_size = cbound;
    auto zok         = compress((Bytef*)compressed_buf, compressed_size,
                        (const Bytef*)uncompressed_buf,
                        (unsigned long)strip_bytes);
    if (zok != Z_OK)
        *ok = false;
}



bool
TIFFOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride)
{
    // If the stars all align properly, try to write strips, and use the
    // thread pool to parallelize the compression. This can give a large
    // speedup (5x or more!) because the zip compression dwarfs the
    // actual raw I/O. But libtiff is totally serialized, so we can only
    // parallelize by making calls to zlib ourselves and then writing
    // "raw" (compressed) strips. Don't bother trying to handle any of
    // the uncommon cases with strips. This covers most real-world cases.
    thread_pool* pool = default_thread_pool();
    int nstrips       = (yend - ybegin + m_rowsperstrip - 1) / m_rowsperstrip;
    bool parallelize =
        // scanline range must be complete strips
        is_strip_boundary(ybegin)
        && is_strip_boundary(yend)
        // and more than one, or no point parallelizing
        && nstrips > 1
        // and not palette or cmyk color separated conversions
        && (m_photometric != PHOTOMETRIC_SEPARATED
            && m_photometric != PHOTOMETRIC_PALETTE)
        // no non-multiple-of-8 bits per sample
        && (spec().format.size() * 8 == m_bitspersample)
        // contig planarconfig only
        && m_planarconfig == PLANARCONFIG_CONTIG
        // only deflate/zip compression with horizontal predictor
        && m_compression == COMPRESSION_ADOBE_DEFLATE
        && m_predictor == PREDICTOR_HORIZONTAL
        // only uint8, uint16
        && (m_spec.format == TypeUInt8 || m_spec.format == TypeUInt16)
        // only if we're threading and don't enter the thread pool recursively!
        && pool->size() > 1
        && !pool->is_worker()
        // and not if the feature is turned off
        && m_spec.get_int_attribute("tiff:multithread",
                                    OIIO::get_int_attribute("tiff:multithread"));

    // If we're not parallelizing, just call the parent class default
    // implementaiton of write_scanlines, which will loop over the scanlines
    // and write each one individually.
    if (!parallelize) {
        return ImageOutput::write_scanlines(ybegin, yend, z, format, data,
                                            xstride, ystride);
    }

    // From here on, we're only dealing with the parallelizeable case...

    // First, do the native data type conversion and contiguization. By
    // doing the whole chunk, it will be parallelized.
    std::vector<unsigned char> nativebuf;
    data = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width, ybegin, yend,
                               z, z + 1, format, data, xstride, ystride,
                               AutoStride, nativebuf, m_dither, m_spec.x,
                               m_spec.y, m_spec.z);
    format  = TypeUnknown;  // native
    xstride = (stride_t)m_spec.pixel_bytes(true);
    ystride = xstride * m_spec.width;

    // Allocate various temporary space we need
    const void* origdata      = data;
    imagesize_t scratch_bytes = m_spec.scanline_bytes() * (yend - ybegin);
    // Because the predictor is destructive, we need to copy to temp space
    std::unique_ptr<char[]> scratch(new char[scratch_bytes]);
    memcpy(scratch.get(), data, scratch_bytes);
    data                    = scratch.get();
    imagesize_t strip_bytes = m_spec.scanline_bytes(true) * m_rowsperstrip;
    size_t cbound           = compressBound((uLong)strip_bytes);
    std::unique_ptr<char[]> compressed_scratch(new char[cbound * nstrips]);
    unsigned long* compressed_len = OIIO_ALLOCA(unsigned long, nstrips);
    int y                         = ybegin;
    int y_at_stripstart           = y;

    // Compress all the strips in parallel using the thread pool.
    task_set tasks(pool);
    bool ok = true;  // failed compression will stash a false here
    for (size_t stripidx = 0; y + m_rowsperstrip <= yend;
         y += m_rowsperstrip, ++stripidx) {
        char* cbuf = compressed_scratch.get() + stripidx * cbound;
        tasks.push(pool->push([=, &ok](int /*id*/) {
            memcpy((void*)data, origdata, strip_bytes);
            this->compress_one_strip((void*)data, strip_bytes, cbuf, cbound,
                                     this->m_spec.nchannels, this->m_spec.width,
                                     m_rowsperstrip, compressed_len + stripidx,
                                     &ok);
        }));
        data     = (char*)data + strip_bytes;
        origdata = (char*)origdata + strip_bytes;
    }
    // tasks.wait(); DON'T WAIT -- start writing as strips are done!

    // Now write those compressed strips as they come out of the queue.
    y = y_at_stripstart;
    for (size_t stripidx = 0; ok && y + m_rowsperstrip <= yend;
         y += m_rowsperstrip, ++stripidx) {
        char* cbuf        = compressed_scratch.get() + stripidx * cbound;
        tstrip_t stripnum = (y - m_spec.y) / m_rowsperstrip;
        // Wait for THIS strip to be done before writing. But ok if
        // others are still being compressed. And this is a non-blocking
        // wait, it will steal tasks from the queue if the next strip
        // it needs is not yet done.
        tasks.wait_for_task(stripidx);
        if (!ok) {
            errorf("Compression error");
            return false;
        }
        if (TIFFWriteRawStrip(m_tif, stripnum, (tdata_t)cbuf,
                              tmsize_t(compressed_len[stripidx]))
            < 0) {
            std::string err = oiio_tiff_last_error();
            errorf("TIFFWriteRawStrip failed writing line y=%d,z=%d: %s", y, z,
                   err.size() ? err.c_str() : "unknown error");
            return false;
        }
    }

    // Should we checkpoint? Only if we have enough scanlines and enough
    // time has passed (or if using JPEG compression, for which it seems
    // necessary).
    m_checkpointItems += m_rowsperstrip;
    if ((m_checkpointTimer() > DEFAULT_CHECKPOINT_INTERVAL_SECONDS
         || m_compression == COMPRESSION_JPEG)
        && m_checkpointItems >= MIN_SCANLINES_OR_TILES_PER_CHECKPOINT) {
        TIFFCheckpointDirectory(m_tif);
        m_checkpointTimer.lap();
        m_checkpointItems = 0;
    }

    if (y < yend && origdata != data)
        memcpy((void*)data, origdata, (yend - y) * m_spec.scanline_bytes(true));

    // Write the stray scanlines at the end that can't make a full strip.
    // Or all the scanlines if we weren't trying to write strips.
    for (; ok && y < yend; ++y) {
        ok &= write_scanline(y, z, format, data, xstride);
        data = (char*)data + ystride;
    }

    return ok;
}



bool
TIFFOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!m_spec.valid_tile_range(x, x, y, y, z, z))
        return false;
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       spec().tile_width, spec().tile_height);
    x -= m_spec.x;  // Account for offset, so x,y are file relative, not
    y -= m_spec.y;  // image relative
    z -= m_spec.z;
    data = to_native_tile(format, data, xstride, ystride, zstride, m_scratch,
                          m_dither, x, y, z);
    size_t tile_vals = spec().tile_pixels() * m_outputchans;

    // Handle weird photometric/color spaces
    std::vector<unsigned char> cmyk;
    if (m_photometric == PHOTOMETRIC_SEPARATED && m_convert_rgb_to_cmyk)
        data = convert_to_cmyk(spec().tile_pixels(), data, cmyk);

    // Handle weird bit depths
    if (spec().format.size() * 8 != m_bitspersample) {
        data = move_to_scratch(data, tile_vals * spec().format.size());
        fix_bitdepth(m_scratch.data(), int(tile_vals));
    }

    if (m_planarconfig == PLANARCONFIG_SEPARATE && m_spec.nchannels > 1) {
        // Convert from contiguous (RGBRGBRGB) to separate (RRRGGGBBB)
        imagesize_t tile_pixels = m_spec.tile_pixels();
        imagesize_t plane_bytes = tile_pixels * m_spec.format.size();
        OIIO_DASSERT(plane_bytes * m_spec.nchannels == m_spec.tile_bytes());

        std::unique_ptr<char[]> separate_heap;
        char* separate            = NULL;
        imagesize_t separate_size = plane_bytes * m_outputchans;
        if (separate_size <= (1 << 16))
            separate = OIIO_ALLOCA(char, separate_size);   // <=64k ? stack
        else {                                             // >64k ? heap
            separate_heap.reset(new char[separate_size]);  // will auto-free
            separate = separate_heap.get();
        }
        contig_to_separate(tile_pixels, m_outputchans, (const char*)data,
                           separate);
        for (int c = 0; c < m_outputchans; ++c) {
            if (TIFFWriteTile(m_tif, (tdata_t)&separate[plane_bytes * c], x, y,
                              z, c)
                < 0) {
                std::string err = oiio_tiff_last_error();
                errorf("TIFFWriteTile failed writing tile x=%d,y=%d,z=%d (%s)",
                       x + m_spec.x, y + m_spec.y, z + m_spec.z,
                       err.size() ? err.c_str() : "unknown error");
                return false;
            }
        }
    } else {
        // No contig->separate is necessary.  But we still use scratch
        // space since TIFFWriteTile is destructive when
        // TIFFTAG_PREDICTOR is used.
        data = move_to_scratch(data, tile_vals * m_spec.format.size());
        if (TIFFWriteTile(m_tif, (tdata_t)data, x, y, z, 0) < 0) {
            std::string err = oiio_tiff_last_error();
            errorf("TIFFWriteTile failed writing tile x=%d,y=%d,z=%d (%s)",
                   x + m_spec.x, y + m_spec.y, z + m_spec.z,
                   err.size() ? err.c_str() : "unknown error");
            return false;
        }
    }

    // Should we checkpoint? Only if we have enough tiles and enough
    // time has passed (or if using JPEG compression, for which it seems
    // necessary).
    ++m_checkpointItems;
    if ((m_checkpointTimer() > DEFAULT_CHECKPOINT_INTERVAL_SECONDS
         || m_compression == COMPRESSION_JPEG)
        && m_checkpointItems >= MIN_SCANLINES_OR_TILES_PER_CHECKPOINT) {
        TIFFCheckpointDirectory(m_tif);
        m_checkpointTimer.lap();
        m_checkpointItems = 0;
    }

    return true;
}



bool
TIFFOutput::write_tiles(int xbegin, int xend, int ybegin, int yend, int zbegin,
                        int zend, TypeDesc format, const void* data,
                        stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // If the stars all align properly, try to use the thread pool to
    // parallelize the compression of the tiles. This can give a large
    // speedup (5x or more!) because the zip compression dwarfs the actual
    // raw I/O. But libtiff is totally serialized, so we can only
    // parallelize by making calls to zlib ourselves and then writing "raw"
    // (compressed) strips. Don't bother trying to handle any of the
    // uncommon cases with strips. This covers most real-world cases.
    thread_pool* pool = default_thread_pool();
    OIIO_DASSERT(m_spec.tile_depth >= 1);
    size_t ntiles = size_t(
        (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width
        * (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height
        * (zend - zbegin + m_spec.tile_depth - 1) / m_spec.tile_depth);
    bool parallelize =
        // more than one tile, or no point parallelizing
        ntiles > 1
        // and not palette or cmyk color separated conversions
        && (m_photometric != PHOTOMETRIC_SEPARATED
            && m_photometric != PHOTOMETRIC_PALETTE)
        // no non-multiple-of-8 bits per sample
        && (spec().format.size() * 8 == m_bitspersample)
        // contig planarconfig only
        && m_planarconfig == PLANARCONFIG_CONTIG
        // only deflate/zip compression with horizontal predictor
        && m_compression == COMPRESSION_ADOBE_DEFLATE
        && m_predictor == PREDICTOR_HORIZONTAL
        // only uint8, uint16
        && (m_spec.format == TypeUInt8 || m_spec.format == TypeUInt16)
        // only if we're threading and don't enter the thread pool recursively!
        && pool->size() > 1
        && !pool->is_worker()
        // and not if the feature is turned off
        && m_spec.get_int_attribute("tiff:multithread",
                                    OIIO::get_int_attribute("tiff:multithread"));

    // If we're not parallelizing, just call the parent class default
    // implementaiton of write_tiles, which will loop over the tiles and
    // write each one individually.
    if (!parallelize) {
        return ImageOutput::write_tiles(xbegin, xend, ybegin, yend, zbegin,
                                        zend, format, data, xstride, ystride,
                                        zstride);
    }

    // From here on, we're only dealing with the parallelizeable case...

    // Allocate various temporary space we need
    stride_t tile_bytes = (stride_t)m_spec.tile_bytes(true);
    std::vector<std::vector<unsigned char>> tilebuf(ntiles);
    size_t cbound = compressBound((uLong)tile_bytes);
    std::unique_ptr<char[]> compressed_scratch(new char[ntiles * cbound]);
    unsigned long* compressed_len = OIIO_ALLOCA(unsigned long, ntiles);

    if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
        xstride = m_spec.pixel_bytes(true);
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       xend - xbegin, yend - ybegin);

    // Compress all the tiles in parallel using the thread pool.
    task_set tasks(pool);
    bool ok = true;  // failed compression will stash a false here
    for (int z = zbegin, tileno = 0; z < zend; z += m_spec.tile_depth) {
        for (int y = ybegin; y < yend; y += m_spec.tile_height) {
            for (int x = xbegin; ok && x < xend;
                 x += m_spec.tile_width, ++tileno) {
                tasks.push(pool->push([&, x, y, z, tileno](int /*id*/) {
                    const unsigned char* tilestart
                        = ((unsigned char*)data + (x - xbegin) * xstride
                           + (z - zbegin) * zstride + (y - ybegin) * ystride);
                    int xw = std::min(xend - x, m_spec.tile_width);
                    int yh = std::min(yend - y, m_spec.tile_height);
                    int zd = std::min(zend - z, m_spec.tile_depth);
                    stride_t tile_xstride = xstride;
                    stride_t tile_ystride = ystride;
                    stride_t tile_zstride = zstride;
                    // Partial tiles at the edge need to be padded to the
                    // full tile size.
                    std::unique_ptr<unsigned char[]> padded_tile;
                    if (xw < m_spec.tile_width || yh < m_spec.tile_height
                        || zd < m_spec.tile_depth) {
                        stride_t pixelsize = format.size() * m_spec.nchannels;
                        padded_tile.reset(
                            new unsigned char[pixelsize * m_spec.tile_pixels()]);
                        OIIO::copy_image(m_spec.nchannels, xw, yh, zd,
                                         tilestart, pixelsize, xstride, ystride,
                                         zstride, padded_tile.get(), pixelsize,
                                         pixelsize * m_spec.tile_width,
                                         pixelsize * m_spec.tile_pixels());
                        tilestart    = padded_tile.get();
                        tile_xstride = pixelsize;
                        tile_ystride = tile_xstride * m_spec.tile_width;
                        tile_zstride = tile_ystride * m_spec.tile_height;
                    }
                    const void* buf
                        = to_native_tile(format, tilestart, tile_xstride,
                                         tile_ystride, tile_zstride,
                                         tilebuf[tileno], m_dither, x, y, z);
                    if (buf == (const void*)tilestart) {
                        // Ugly detail: if to_native_rectangle did not allocate
                        // scratch space and copy to it, we need to do it now,
                        // because the horizontal predictor is destructive.
                        tilebuf[tileno].assign((char*)buf,
                                               ((char*)buf)
                                                   + m_spec.tile_bytes(true));
                        buf = tilebuf[tileno].data();
                    }
                    char* cbuf = compressed_scratch.get() + tileno * cbound;
                    compress_one_strip((void*)buf, tile_bytes, cbuf, cbound,
                                       m_spec.nchannels, m_spec.tile_width,
                                       m_spec.tile_height * m_spec.tile_depth,
                                       compressed_len + tileno, &ok);
                }));
            }
        }
    }
    // tasks.wait(); DON'T WAIT -- start writing as tiles are done!

    for (int z = zbegin, tileno = 0; z < zend; z += m_spec.tile_depth) {
        for (int y = ybegin; y < yend; y += m_spec.tile_height) {
            for (int x = xbegin; ok && x < xend;
                 x += m_spec.tile_width, ++tileno) {
                // Wait for THIS tile to be done before writing. But ok if
                // others are still being compressed. And this is a non-
                // blocking wait, it will steal tasks from the queue if the
                // next tile it needs is not yet done.
                tasks.wait_for_task(tileno);
                char* cbuf = compressed_scratch.get() + tileno * cbound;
                if (!ok) {
                    errorf("Compression error");
                    return false;
                }
                if (TIFFWriteRawTile(m_tif, uint32_t(tile_index(x, y, z)), cbuf,
                                     compressed_len[tileno])
                    < 0) {
                    std::string err = oiio_tiff_last_error();
                    errorf(
                        "TIFFWriteRawTile failed writing tile %d (x=%d,y=%d,z=%d): %s",
                        tile_index(x, y, z), x, y, z,
                        err.size() ? err.c_str() : "unknown error");
                    return false;
                }
            }
        }
    }
    return ok;
}



bool
TIFFOutput::source_is_cmyk(const ImageSpec& spec)
{
    if (spec.nchannels != 4) {
        return false;  // Can't be CMYK if it's not 4 channels
    }
    if (Strutil::iequals(spec.channelnames[0], "C")
        && Strutil::iequals(spec.channelnames[1], "M")
        && Strutil::iequals(spec.channelnames[2], "Y")
        && Strutil::iequals(spec.channelnames[3], "K"))
        return true;
    if (Strutil::iequals(spec.channelnames[0], "Cyan")
        && Strutil::iequals(spec.channelnames[1], "Magenta")
        && Strutil::iequals(spec.channelnames[2], "Yellow")
        && Strutil::iequals(spec.channelnames[3], "Black"))
        return true;
    string_view oiiocs = spec.get_string_attribute("oiio:ColorSpace");
    if (Strutil::iequals(oiiocs, "CMYK"))
        return true;
    return false;
}



bool
TIFFOutput::source_is_rgb(const ImageSpec& spec)
{
    string_view oiiocs = spec.get_string_attribute("oiio:ColorSpace");
    if (Strutil::iequals(oiiocs, "CMYK")
        || Strutil::iequals(oiiocs, "color separated"))
        return false;  // It's a color space mode that means something else
    if (spec.nchannels != 3)
        return false;  // Can't be RGB if it's not 3 channels
    if (Strutil::iequals(spec.channelnames[0], "R")
        && Strutil::iequals(spec.channelnames[1], "G")
        && Strutil::iequals(spec.channelnames[2], "B"))
        return true;
    if (Strutil::iequals(spec.channelnames[0], "Red")
        && Strutil::iequals(spec.channelnames[1], "Green")
        && Strutil::iequals(spec.channelnames[2], "Blue"))
        return true;
    return false;
}



void
TIFFOutput::fix_bitdepth(void* data, int nvals)
{
    OIIO_DASSERT(spec().format.size() * 8 != m_bitspersample);

    if (spec().format == TypeDesc::UINT16 && m_bitspersample == 10) {
        unsigned short* v = (unsigned short*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<16, 10>(v[i]);
        bit_pack(cspan<unsigned short>(v, v + nvals), v, 10);
    } else if (spec().format == TypeDesc::UINT16 && m_bitspersample == 12) {
        unsigned short* v = (unsigned short*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<16, 12>(v[i]);
        bit_pack(cspan<unsigned short>(v, v + nvals), v, 12);
    } else if (spec().format == TypeDesc::UINT16 && m_bitspersample == 14) {
        unsigned short* v = (unsigned short*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<16, 14>(v[i]);
        bit_pack(cspan<unsigned short>(v, v + nvals), v, 14);
    } else if (spec().format == TypeDesc::UINT8 && m_bitspersample == 4) {
        unsigned char* v = (unsigned char*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<8, 4>(v[i]);
        bit_pack(cspan<unsigned char>(v, v + nvals), v, 4);
    } else if (spec().format == TypeDesc::UINT8 && m_bitspersample == 2) {
        unsigned char* v = (unsigned char*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<8, 2>(v[i]);
        bit_pack(cspan<unsigned char>(v, v + nvals), v, 2);
    } else if (spec().format == TypeDesc::UINT8 && m_bitspersample == 6) {
        unsigned char* v = (unsigned char*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<8, 6>(v[i]);
        bit_pack(cspan<unsigned char>(v, v + nvals), v, 6);
    } else if (spec().format == TypeDesc::UINT8 && m_bitspersample == 1) {
        unsigned char* v = (unsigned char*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<8, 1>(v[i]);
        bit_pack(cspan<unsigned char>(v, v + nvals), v, 1);
    } else if (spec().format == TypeDesc::UINT32 && m_bitspersample == 24) {
        unsigned int* v = (unsigned int*)data;
        for (int i = 0; i < nvals; ++i)
            v[i] = bit_range_convert<32, 24>(v[i]);
        bit_pack(cspan<unsigned int>(v, v + nvals), v, 24);
    } else {
        OIIO_ASSERT(0 && "unsupported bit conversion -- shouldn't reach here");
    }
}


OIIO_PLUGIN_NAMESPACE_END
