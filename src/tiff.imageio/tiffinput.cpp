// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define AVOID_WIN32_FILEIO
#include <tiffio.h>
#include <zlib.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/tiffutils.h>
#include <OpenImageIO/typedesc.h>

#include "imageio_pvt.h"


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


// clang-format off
#ifdef TIFFLIB_MAJOR_VERSION
// libtiff >= 4.5 defines versions by number -- use them.
#    define OIIO_TIFFLIB_VERSION (TIFFLIB_MAJOR_VERSION * 10000 \
                                  + TIFFLIB_MINOR_VERSION * 100 \
                                  + TIFFLIB_MICRO_VERSION)
// For older libtiff, we need to figure it out by date.
#elif TIFFLIB_VERSION >= 20220520
#    define OIIO_TIFFLIB_VERSION 40400
#elif TIFFLIB_VERSION >= 20210416
#    define OIIO_TIFFLIB_VERSION 40300
#elif TIFFLIB_VERSION >= 20201219
#    define OIIO_TIFFLIB_VERSION 40200
#elif TIFFLIB_VERSION >= 20191103
#    define OIIO_TIFFLIB_VERSION 40100
#elif TIFFLIB_VERSION >= 20120922
#    define OIIO_TIFFLIB_VERSION 40003
#elif TIFFLIB_VERSION >= 20111221
#    define OIIO_TIFFLIB_VERSION 40000
#else
#    error "libtiff 4.0.0 or later is required"
#endif
// clang-format on


OIIO_PLUGIN_NAMESPACE_BEGIN


// Helper struct for constructing tables of TIFF tags
struct TIFF_tag_info {
    int tifftag;            // TIFF tag used for this info
    const char* name;       // Attribute name we use, or NULL to ignore the tag
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



class TIFFInput final : public ImageInput {
public:
    TIFFInput();
    ~TIFFInput() override;
    const char* format_name(void) const override { return "tiff"; }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "iptc" || feature == "ioproxy"
                || feature == "multiimage" || feature == "mipmap");
        // N.B. No support for arbitrary metadata.
    }
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override;
    int current_subimage(void) const override
    {
        // If m_emulate_mipmap is true, pretend subimages are mipmap levels
        lock_guard lock(*this);
        return m_subimage;
    }
    int current_miplevel(void) const override
    {
        // If m_emulate_mipmap is true, pretend subimages are mipmap levels
        lock_guard lock(*this);
        return m_miplevel;
    }
    bool seek_subimage(int subimage, int miplevel) override;
    ImageSpec spec(int subimage, int miplevel) override;
    ImageSpec spec_dimensions(int subimage, int miplevel) override;
    const ImageSpec& spec(void) const override { return m_spec; }
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               int z, void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;
    bool read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                           int ybegin, int yend, int zbegin, int zend,
                           void* data) override;

    // Helper: already having locked the ImageInput and done a seek_subimage
    // to the right subimage & miplevel, read the designated scanline.
    bool read_native_scanline_locked(int subimage, int miplevel, int y,
                                     span<std::byte> data);
    bool read_native_tile_locked(int subimage, int miplevel, int x, int y,
                                 int z, span<std::byte> data);
    bool read_native_tiles_locked(int subimage, int miplevel, int xbegin,
                                  int xend, int ybegin, int yend, int zbegin,
                                  int zend, size_t ntiles,
                                  span<std::byte> data);

    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               span<std::byte> data) override;
    bool read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                           int ybegin, int yend, span<std::byte> data) override;
    bool read_native_volumetric_tiles(int subimage, int miplevel, int xbegin,
                                      int xend, int ybegin, int yend,
                                      int zbegin, int zend,
                                      span<std::byte> data) override;

    bool read_scanline(int y, int z, TypeDesc format, void* data,
                       stride_t xstride) override;
    bool read_scanlines(int subimage, int miplevel, int ybegin, int yend, int z,
                        int chbegin, int chend, TypeDesc format, void* data,
                        stride_t xstride, stride_t ystride) override;
    bool read_tile(int x, int y, int z, TypeDesc format, void* data,
                   stride_t xstride, stride_t ystride,
                   stride_t zstride) override;
    bool read_tiles(int subimage, int miplevel, int xbegin, int xend,
                    int ybegin, int yend, int zbegin, int zend, int chbegin,
                    int chend, TypeDesc format, void* data, stride_t xstride,
                    stride_t ystride, stride_t zstride) override;

private:
    TIFF* m_tif;                            ///< libtiff handle
    std::string m_filename;                 ///< Stash the filename
    std::vector<unsigned char> m_scratch;   ///< Scratch space for us to use
    std::vector<unsigned char> m_scratch2;  ///< More scratch
    int m_subimage;           ///< What subimage do we think we're on?
    int m_miplevel;           ///< Which mip level do we think we're on?
    int m_actual_subimage;    ///< Actual subimage we're on
    int m_next_scanline;      ///< Next scanline we'll read, relative to ymin
    bool m_no_random_access;  ///< Should we avoid random access?
    bool m_emulate_mipmap;    ///< Should we emulate mip with subimage?
    bool m_keep_unassociated_alpha;  ///< If the image is unassociated, please
                                     ///<   try to keep it that way!
    bool m_raw_color;                ///< If the image is not RGB, don't
                                     ///<   transform the color.
    bool m_convert_alpha;            ///< Do we need to associate alpha?
    bool m_separate;                 ///< Separate planarconfig?
    bool m_testopenconfig;           ///< Debug aid to test open-with-config
    bool m_use_rgba_interface;       ///< Sometimes we punt
    bool m_is_byte_swapped;          ///< Is the file opposite our endian?
    int m_rowsperstrip;              ///< For scanline imgs, rows per strip
    unsigned short m_planarconfig;   ///< Planar config of the file
    unsigned short m_bitspersample;  ///< Of the *file*, not the client's view
    unsigned short m_photometric;    ///< Of the *file*, not the client's view
    unsigned short m_compression;    ///< TIFF compression tag
    unsigned short m_predictor;      ///< TIFF compression predictor tag
    unsigned short m_inputchannels;  ///< Channels in the file (careful with CMYK)
    std::vector<unsigned short> m_colormap;   ///< Color map for palette images
    std::vector<uint32_t> m_rgbadata;         ///< Sometimes we punt
    std::vector<ImageSpec> m_subimage_specs;  ///< Cached subimage specs

    // Reset everything to initial state
    void init()
    {
        m_tif                     = NULL;
        m_subimage                = -1;
        m_miplevel                = -1;
        m_actual_subimage         = -1;
        m_emulate_mipmap          = false;
        m_keep_unassociated_alpha = false;
        m_raw_color               = false;
        m_convert_alpha           = false;
        m_separate                = false;
        m_inputchannels           = 0;
        m_testopenconfig          = false;
        m_colormap.clear();
        m_use_rgba_interface = false;
        m_subimage_specs.clear();
        ioproxy_clear();
    }

    // Just close the TIFF file handle, but don't forget anything we
    // learned about the contents of the file or any configuration hints.
    void close_tif()
    {
        if (m_tif) {
            TIFFClose(m_tif);
            m_tif = NULL;
            m_rgbadata.clear();
            m_rgbadata.shrink_to_fit();
        }
    }

    // Read tags from the current directory of m_tif and fill out spec.
    // If read_meta is false, assume that m_spec already contains valid
    // metadata and should not be cleared or rewritten.
    // Return true if all is fine, false if something really bad happens,
    // like we think the file is hopelessly corrupted.
    bool readspec(bool read_meta = true);

    // Figure out all the photometric-related aspects of the header
    void readspec_photometric();

    // Convert planar separate to contiguous data format
    void separate_to_contig(size_t nplanes, size_t nvals,
                            cspan<std::byte> separate, span<std::byte> contig);

    // Convert palette to RGB
    void palette_to_rgb(size_t n, cspan<uint8_t> palettepels,
                        span<uint8_t> rgb);
    void palette_to_rgb(size_t n, cspan<uint16_t> palettepels,
                        span<uint8_t> rgb);

    // Convert in-bits to out-bits (outbits must be 8, 16, 32, and
    // inbits < outbits)
    // FIXME: should change to be span-based
    void bit_convert(int n, const unsigned char* in, int inbits, void* out,
                     int outbits);

    void invert_photometric(int n, void* data);

    const TIFFField* find_field(int tifftag, TIFFDataType tifftype = TIFF_ANY)
    {
        return TIFFFindField(m_tif, tifftag, tifftype);
    }

    OIIO_NODISCARD
    TypeDesc tiffgetfieldtype(int tag)
    {
        auto field = find_field(tag);
        if (!field)
            return TypeUnknown;
        TIFFDataType tiffdatatype = TIFFFieldDataType(field);
        int passcount             = TIFFFieldPassCount(field);
        int readcount             = TIFFFieldReadCount(field);
        if (!passcount && readcount > 0)
            return tiff_datatype_to_typedesc(tiffdatatype, readcount);
        return TypeUnknown;
    }

    OIIO_NODISCARD
    bool safe_tiffgetfield(string_view name OIIO_MAYBE_UNUSED, int tag,
                           TypeDesc expected, void* dest)
    {
        TypeDesc type = tiffgetfieldtype(tag);
        // Caller expects a specific type and the tag doesn't match? Punt.
        if (expected != TypeUnknown && !equivalent(expected, type))
            return false;
        auto field = find_field(tag);
        if (!field)
            return false;

        // TIFFDataType tiffdatatype = TIFFFieldDataType(field);
        int passcount = TIFFFieldPassCount(field);
        int readcount = TIFFFieldReadCount(field);
        if (!passcount && readcount > 0) {
            return TIFFGetField(m_tif, tag, dest);
        }
        // OIIO::debugfmt(" stgf {} tag {} {} datatype {} passcount {} readcount {}\n",
        //                name, tag, type, int(TIFFFieldDataType(field)), passcount, readcount);
        return false;
    }

    // Get a string tiff tag field and save it it as a string_view. The
    // return value will be true if the tag was found, otherwise false.
    OIIO_NODISCARD
    bool tiff_get_string_field(int tag, string_view name OIIO_MAYBE_UNUSED,
                               string_view& result)
    {
        auto field = find_field(tag);
        if (!field)
            return false;
        TIFFDataType tiffdatatype = TIFFFieldDataType(field);
        int passcount             = TIFFFieldPassCount(field);
        int readcount             = TIFFFieldReadCount(field);
        // Strutil::printf(" tgsf %s tag %d datatype %d passcount %d readcount %d\n",
        //                 name, tag, int(tiffdatatype), passcount, readcount);
        char* s        = nullptr;
        uint32_t count = 0;
        bool ok        = false;
        if (tiffdatatype == TIFF_ASCII && passcount
            && readcount == TIFF_VARIABLE) {
            uint16_t shortcount = 0;
            ok                  = TIFFGetField(m_tif, tag, &shortcount, &s);
            count               = shortcount;
        } else if (tiffdatatype == TIFF_ASCII && passcount
                   && readcount == TIFF_VARIABLE2) {
            ok = TIFFGetField(m_tif, tag, &count, &s);
        } else if (readcount > 0) {
            ok    = TIFFGetField(m_tif, tag, &s);
            count = readcount;
        } else if (tiffdatatype == TIFF_ASCII) {
            ok = TIFFGetField(m_tif, tag, &s);
            if (ok && s && *s)
                count = Strutil::safe_strlen(s, 64 * 1024);
        } else {
            // Some other type, we should not have been asking for this
            // as ASCII, or maybe the tag is just the wrong data type in
            // the file. Punt.
        }
        if (ok && s && *s) {
            result = Strutil::safe_string_view(s, count);
        }
        return ok;
    }

    // Get a string tiff tag field and put it into extra_params
    void get_string_attribute(string_view name, int tag)
    {
        string_view s;
        if (tiff_get_string_field(tag, name, s)) {
            if (s.size())
                m_spec.attribute(name, s);
            else
                m_spec.erase_attribute(name);
        }
    }

    // Get a matrix tiff tag field and put it into extra_params
    void get_matrix_attribute(string_view name, int tag)
    {
        float* f = nullptr;
        if (safe_tiffgetfield(name, tag, TypeUnknown, &f) && f)
            m_spec.attribute(name, TypeMatrix, f);
    }

    // Get a float tiff tag field and put it into extra_params
    void get_float_attribute(string_view name, int tag)
    {
        float f[16];
        if (safe_tiffgetfield(name, tag, TypeUnknown, f))
            m_spec.attribute(name, f[0]);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_attribute(string_view name, int tag)
    {
        int i = 0;
        if (safe_tiffgetfield(name, tag, TypeUnknown, &i))
            m_spec.attribute(name, i);
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_attribute(string_view name, int tag)
    {
        // Make room for two shorts, in case the tag is not the type we
        // expect, and libtiff writes a long instead.
        unsigned short s[2] = { 0, 0 };
        if (safe_tiffgetfield(name, tag, TypeUInt16, &s)) {
            int i = s[0];
            m_spec.attribute(name, i);
        }
    }

    // Search for TIFF tag having type 'tifftype', and if found,
    // add it in the obvious way to m_spec under the name 'oiioname'.
    void find_tag(int tifftag, TIFFDataType tifftype, string_view oiioname)
    {
        auto info = find_field(tifftag, tifftype);
        if (!info) {
            // Something has gone wrong, libtiff doesn't think the field type
            // is the same as we do.
            return;
        }
        if (tifftype == TIFF_ASCII)
            get_string_attribute(oiioname, tifftag);
        else if (tifftype == TIFF_SHORT)
            get_short_attribute(oiioname, tifftag);
        else if (tifftype == TIFF_LONG)
            get_int_attribute(oiioname, tifftag);
        else if (tifftype == TIFF_RATIONAL || tifftype == TIFF_SRATIONAL
                 || tifftype == TIFF_FLOAT || tifftype == TIFF_DOUBLE)
            get_float_attribute(oiioname, tifftag);
    }

    // If we're at scanline y, where does the next strip start?
    int next_strip_boundary(int y)
    {
        return round_to_multiple(y - m_spec.y, m_rowsperstrip) + m_spec.y;
    }

    bool is_strip_boundary(int y)
    {
        return y == next_strip_boundary(y) || y == m_spec.height;
    }

    // Copy a height x width x chans region of src to dst, un-applying a
    // horizontal predictor to each row. It is permitted for src and dst to
    // be the same.
    template<typename T>
    void undo_horizontal_predictor(T* dst, const T* src, int chans, int width,
                                   int height)
    {
        for (int y = 0; y < height;
             ++y, src += chans * width, dst += chans * width)
            for (int c = 0; c < chans; ++c) {
                dst[c] = src[c];  // element 0
                for (int x = 1; x < width; ++x)
                    dst[x * chans + c] = src[(x - 1) * chans + c]
                                         + src[x * chans + c];
            }
    }

    void uncompress_one_strip(const void* compressed_buf, unsigned long csize,
                              void* uncompressed_buf, size_t strip_bytes,
                              int channels, int width, int height, bool* ok)
    {
        OIIO_DASSERT (m_compression == COMPRESSION_ADOBE_DEFLATE /*||
                      m_compression == COMPRESSION_NONE*/);
        size_t nvals = size_t(width) * size_t(height) * size_t(channels);
        if (m_compression == COMPRESSION_NONE) {
            // just copy if there's no compression
            memcpy(uncompressed_buf, compressed_buf, csize);
            if (m_is_byte_swapped && m_spec.format == TypeUInt16)
                TIFFSwabArrayOfShort((unsigned short*)uncompressed_buf, nvals);
            return;
        }
        uLong uncompressed_size = (uLong)strip_bytes;
        auto zok = uncompress((Bytef*)uncompressed_buf, &uncompressed_size,
                              (const Bytef*)compressed_buf, csize);
        if (zok != Z_OK || uncompressed_size != strip_bytes) {
            *ok = false;
            return;
        }
        if (m_is_byte_swapped && m_spec.format == TypeUInt16)
            TIFFSwabArrayOfShort((unsigned short*)uncompressed_buf, nvals);
        if (m_predictor == PREDICTOR_HORIZONTAL) {
            if (m_spec.format == TypeUInt8)
                undo_horizontal_predictor((unsigned char*)uncompressed_buf,
                                          (unsigned char*)uncompressed_buf,
                                          channels, width, height);
            else if (m_spec.format == TypeUInt16)
                undo_horizontal_predictor((unsigned short*)uncompressed_buf,
                                          (unsigned short*)uncompressed_buf,
                                          channels, width, height);
        }
    }

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

#if OIIO_TIFFLIB_VERSION >= 40500
    std::string m_last_error;
    spin_mutex m_last_error_mutex;

    std::string oiio_tiff_last_error()
    {
        spin_lock lock(m_last_error_mutex);
        return m_last_error;
    }

    // TIFF 4.5+ has a mechanism for per-file thread-safe error handlers.
    // Use it.
    static int my_error_handler(TIFF* tif, void* user_data,
                                const char* /*module*/, const char* fmt,
                                va_list ap)
    {
        TIFFInput* self = (TIFFInput*)user_data;
        spin_lock lock(self->m_last_error_mutex);
        OIIO_PRAGMA_WARNING_PUSH
        OIIO_GCC_PRAGMA(GCC diagnostic ignored "-Wformat-nonliteral")
        self->m_last_error = Strutil::vsprintf(fmt, ap);
        OIIO_PRAGMA_WARNING_POP
        return 1;
    }

    static int my_warning_handler(TIFF* tif, void* user_data,
                                  const char* /*module*/, const char* fmt,
                                  va_list ap)
    {
        TIFFInput* self = (TIFFInput*)user_data;
        spin_lock lock(self->m_last_error_mutex);
        OIIO_PRAGMA_WARNING_PUSH
        OIIO_GCC_PRAGMA(GCC diagnostic ignored "-Wformat-nonliteral")
        self->m_last_error = Strutil::vsprintf(fmt, ap);
        OIIO_PRAGMA_WARNING_POP
        return 1;
    }
#endif
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
tiff_input_imageio_create()
{
    return new TIFFInput;
}

// OIIO_EXPORT int tiff_imageio_version = OIIO_PLUGIN_VERSION; // it's in tiffoutput.cpp

OIIO_EXPORT const char* tiff_input_extensions[]
    = { "tif", "tiff", "tx", "env", "sm", "vsm", nullptr };

OIIO_PLUGIN_EXPORTS_END


#if OIIO_TIFFLIB_VERSION < 40500
// For TIFF 4.4 and earlier, we need someplace to store an error message from
// the global TIFF error handler. To avoid thread oddities, we have the
// storage area buffering error messages be thread-specific.
static thread_local std::string thread_error_msg;
static atomic_int handler_set;
static spin_mutex handler_mutex;



std::string&
oiio_tiff_last_error()
{
    return thread_error_msg;
}



static void
my_error_handler(const char* /*str*/, const char* format, va_list ap)
{
    OIIO_PRAGMA_WARNING_PUSH
    OIIO_GCC_PRAGMA(GCC diagnostic ignored "-Wformat-nonliteral")
    oiio_tiff_last_error() = Strutil::vsprintf(format, ap);
    OIIO_PRAGMA_WARNING_POP
}



void
oiio_tiff_set_error_handler()
{
    if (!handler_set) {
        spin_lock lock(handler_mutex);
        if (!handler_set) {
            TIFFSetErrorHandler(my_error_handler);
            TIFFSetWarningHandler(my_error_handler);
            handler_set = 1;
        }
    }
}
#endif


static tsize_t
reader_readproc(thandle_t handle, tdata_t data, tsize_t size)
{
    auto io = static_cast<Filesystem::IOProxy*>(handle);
    // Strutil::print("iop read {} {} @ {}\n",
    //                io->filename(), size, io->tell());
    auto r = io->read(data, size);
    // for (size_t i = 0; i < r; ++i)
    //     Strutil::print(" {:02x}",
    //                    int(((unsigned char *)data)[i]));
    // Strutil::print("\n");
    return tsize_t(r);
}

static tsize_t
reader_writeproc(thandle_t, tdata_t, tsize_t size)
{
    return tsize_t(0);
}

static toff_t
reader_seekproc(thandle_t handle, toff_t offset, int origin)
{
    auto io = static_cast<Filesystem::IOProxy*>(handle);
    // Strutil::print("iop seek {} {} ({})\n",
    //                io->filename(), offset, origin);
    return (io->seek(offset, origin)) ? toff_t(io->tell()) : toff_t(-1);
}

static int
reader_closeproc(thandle_t handle)
{
    // auto io = static_cast<Filesystem::IOProxy*>(handle);
    // if (io && io->opened()) {
    //     // Strutil::print("iop close {}\n\n",
    //     //                io->filename());
    //     // io->seek(0);
    //     io->close();
    // }
    return 0;
}

static toff_t
reader_sizeproc(thandle_t handle)
{
    auto io = static_cast<Filesystem::IOProxy*>(handle);
    // Strutil::print("iop size\n");
    return toff_t(io->size());
}

static int
reader_mapproc(thandle_t, tdata_t*, toff_t*)
{
    return 0;
}

static void
reader_unmapproc(thandle_t, tdata_t, toff_t)
{
}



// clang-format off

static std::pair<int, const char*>  tiff_input_compressions[] = {
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
#if defined(TIFF_VERSION_BIG) && OIIO_TIFFLIB_VERSION >= 40003
    // Others supported in more recent TIFF library versions.
    { COMPRESSION_T85,           "T85" },         // TIFF/FX T.85 JBIG
    { COMPRESSION_T43,           "T43" },         // TIFF/FX T.43 color layered JBIG
    { COMPRESSION_LZMA,          "lzma" },        // LZMA2
#endif
};

// clang-format on

static const char*
tiff_compression_name(int code)
{
    for (const auto& c : tiff_input_compressions)
        if (c.first == code)
            return c.second;
    return nullptr;
}



TIFFInput::TIFFInput()
{
#if OIIO_TIFFLIB_VERSION < 40500
    oiio_tiff_set_error_handler();
#endif
    init();
}



TIFFInput::~TIFFInput()
{
    // Close, if not already done.
    close();
}



bool
TIFFInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    uint16_t magic[2] {};
    const size_t numRead = ioproxy->pread(magic, sizeof(magic), 0);
    if (numRead != sizeof(magic))  // read failed
        return false;
    if (magic[0] != TIFF_LITTLEENDIAN && magic[0] != TIFF_BIGENDIAN)
        return false;  // not the right byte order
    if ((magic[0] == TIFF_LITTLEENDIAN) != littleendian())
        swap_endian(&magic[1], 1);
    return (magic[1] == 42 /* Classic TIFF */ || magic[1] == 43 /* Big TIFF */);
    // local_io, if used, will automatically close and free. A passed in
    // proxy will remain in its prior state.
}



bool
TIFFInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename        = name;
    m_subimage        = -1;
    m_miplevel        = -1;
    m_actual_subimage = -1;

    bool ok = seek_subimage(0, 0);
    newspec = spec();
    return ok;
}



bool
TIFFInput::open(const std::string& name, ImageSpec& newspec,
                const ImageSpec& config)
{
    // Check 'config' for any special requests
    ioproxy_retrieve_from_config(config);
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    if (config.get_int_attribute("oiio:RawColor", 0) == 1)
        m_raw_color = true;
    // This configuration hint has no function other than as a debugging aid
    // for testing whether configurations are received properly from other
    // OIIO components.
    if (config.get_int_attribute("oiio:DebugOpenConfig!", 0))
        m_testopenconfig = true;
    return open(name, newspec);
}



bool
TIFFInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage == m_subimage && miplevel == m_miplevel) {
        // We're already pointing to the right subimage
        return true;
    }

    if (subimage < 0 || miplevel < 0)  // Illegal
        return false;
    int orig_subimage = subimage;  // the original request
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

    // If we're emulating a MIPmap, only resolution is allowed to change
    // between MIP levels, so if we already have a valid level in m_spec,
    // we don't need to re-parse metadata, it's guaranteed to be the same.
    bool read_meta = true;
    if (m_emulate_mipmap && m_tif && m_miplevel >= 0)
        read_meta = false;

    if (!m_tif) {
#if OIIO_TIFFLIB_VERSION >= 40500
        auto openopts = TIFFOpenOptionsAlloc();
        TIFFOpenOptionsSetErrorHandlerExtR(openopts, my_error_handler, this);
        TIFFOpenOptionsSetWarningHandlerExtR(openopts, my_warning_handler,
                                             this);
#endif
        if (ioproxy_opened()) {
            static_assert(sizeof(thandle_t) == sizeof(void*),
                          "thandle_t must be same size as void*");
            // Strutil::print("\n\nOpening client \"{}\"\n", m_filename);
            ioseek(0);
#if OIIO_TIFFLIB_VERSION >= 40500
            m_tif = TIFFClientOpen(m_filename.c_str(), "rm", ioproxy(),
                                   reader_readproc, reader_writeproc,
                                   reader_seekproc, reader_closeproc,
                                   reader_sizeproc, reader_mapproc,
                                   reader_unmapproc);
#else
            m_tif = TIFFClientOpen(m_filename.c_str(), "rm", ioproxy(),
                                   reader_readproc, reader_writeproc,
                                   reader_seekproc, reader_closeproc,
                                   reader_sizeproc, reader_mapproc,
                                   reader_unmapproc);
#endif
        } else {
#if OIIO_TIFFLIB_VERSION >= 40500
#    ifdef _WIN32
            std::wstring wfilename = Strutil::utf8_to_utf16wstring(m_filename);
            m_tif = TIFFOpenWExt(wfilename.c_str(), "rm", openopts);
#    else
            m_tif = TIFFOpenExt(m_filename.c_str(), "rm", openopts);
#    endif
#else
#    ifdef _WIN32
            std::wstring wfilename = Strutil::utf8_to_utf16wstring(m_filename);
            m_tif                  = TIFFOpenW(wfilename.c_str(), "rm");
#    else
            m_tif = TIFFOpen(m_filename.c_str(), "rm");
#    endif
#endif
        }
#if OIIO_TIFFLIB_VERSION >= 40500
        TIFFOpenOptionsFree(openopts);
#endif
        if (m_tif == NULL) {
            std::string e = oiio_tiff_last_error();
            errorfmt("Could not open file: {}", e.length() ? e : m_filename);
            close_tif();
            return false;
        }
        m_is_byte_swapped = TIFFIsByteSwapped(m_tif);
        m_actual_subimage = 0;
    }

    m_next_scanline = 0;  // next scanline we'll read
    if (subimage == m_actual_subimage || TIFFSetDirectory(m_tif, subimage)) {
        m_actual_subimage = subimage;
        if (!readspec(read_meta))
            return false;

        char emsg[1024];
        if (m_use_rgba_interface && !TIFFRGBAImageOK(m_tif, emsg)) {
            errorfmt("No support for this flavor of TIFF file ({})", emsg);
            return false;
        }
        if (size_t(subimage) >= m_subimage_specs.size())  // make room
            m_subimage_specs.resize(
                subimage > 0 ? round_to_multiple(subimage + 1, 4) : 1);
        if (m_subimage_specs[subimage].undefined()) {
            // haven't cached this spec yet
            m_subimage_specs[subimage] = m_spec;
        }
        if (m_spec.format == TypeDesc::UNKNOWN) {
            errorfmt("No support for data format of \"{}\"", m_filename);
            return false;
        }
        if (!check_open(m_spec,
                        { 0, 1 << 20, 0, 1 << 20, 0, 1 << 16, 0, 1 << 16 }))
            return false;
        m_subimage = orig_subimage;
        m_miplevel = miplevel;
        return true;
    } else {
        std::string e = oiio_tiff_last_error();
        errorfmt("could not seek to {} {}",
                 m_emulate_mipmap ? "miplevel" : "subimage", subimage,
                 e.length() ? ": " : "", e.length() ? e : std::string(""));
        m_subimage        = -1;
        m_miplevel        = -1;
        m_actual_subimage = -1;
        return false;
    }
}



ImageSpec
TIFFInput::spec(int subimage, int miplevel)
{
    ImageSpec ret;

    // s == index of the spec list to retrieve. Start by presuming it's
    // the sublevel number.
    int s = subimage;
    if (m_emulate_mipmap) {
        // This is the kind of TIFF file where we are emulating MIPmap
        // levels with TIFF subimages.
        if (subimage != 0)
            return ret;  // Invalid subimage request, return the empty spec
        // Index into the spec list by miplevel instead, because that's
        // what it really contains.
        s = miplevel;
    } else {
        // Not emulating MIP levels -> there are none
        if (miplevel)
            return ret;
    }

    lock_guard lock(*this);
    if (s >= 0 && s < int(m_subimage_specs.size())
        && !m_subimage_specs[s].undefined()) {
        // If we've cached this spec, we don't need to seek and read
        ret = m_subimage_specs[s];
    } else {
        if (seek_subimage(subimage, miplevel))
            ret = m_spec;
    }
    return ret;
}



ImageSpec
TIFFInput::spec_dimensions(int subimage, int miplevel)
{
    ImageSpec ret;

    // s == index of the spec list to retrieve. Start by presuming it's
    // the sublevel number.
    int s = subimage;
    if (m_emulate_mipmap) {
        // This is the kind of TIFF file where we are emulating MIPmap
        // levels with TIFF subimages.
        if (subimage != 0)
            return ret;  // Invalid subimage request, return the empty spec
        // Index into the spec list by miplevel instead, because that's
        // what it really contains.
        s = miplevel;
    } else {
        // Not emulating MIP levels -> there are none
        if (miplevel)
            return ret;
    }

    lock_guard lock(*this);
    if (s >= 0 && s < int(m_subimage_specs.size())
        && !m_subimage_specs[s].undefined()) {
        // If we've cached this spec, we don't need to seek and read
        ret.copy_dimensions(m_subimage_specs[s]);
    } else {
        if (seek_subimage(subimage, miplevel))
            ret.copy_dimensions(m_spec);
    }
    return ret;
}



#define ICC_PROFILE_ATTR "ICCProfile"


bool
TIFFInput::readspec(bool read_meta)
{
    uint32_t width = 0, height = 0, depth = 0;
    TIFFGetField(m_tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(m_tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(m_tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetFieldDefaulted(m_tif, TIFFTAG_SAMPLESPERPIXEL, &m_inputchannels);

    if (read_meta) {
        // clear the whole m_spec and start fresh
        m_spec = ImageSpec((int)width, (int)height, (int)m_inputchannels);
    } else {
        // assume m_spec is valid, except for things that might differ
        // between MIP levels
        m_spec.width     = (int)width;
        m_spec.height    = (int)height;
        m_spec.depth     = (int)depth;
        m_spec.nchannels = (int)m_inputchannels;
    }

    float xpos = 0, ypos = 0;
    TIFFGetField(m_tif, TIFFTAG_XPOSITION, &xpos);
    TIFFGetField(m_tif, TIFFTAG_YPOSITION, &ypos);
    if (xpos || ypos) {
        // In the TIFF files, the positions are in resolutionunit. But we
        // didn't used to interpret it that way, hence the mess below.
        float xres = 1, yres = 1;
        TIFFGetField(m_tif, TIFFTAG_XRESOLUTION, &xres);
        TIFFGetField(m_tif, TIFFTAG_YRESOLUTION, &yres);
        // See if the 'Software' field has a clue about what version of OIIO
        // wrote the TIFF file. This can save us from embarrassing mistakes
        // misinterpreting the image offset.
        int oiio_write_version = 0;
        string_view software;
        if (tiff_get_string_field(TIFFTAG_SOFTWARE, "Software", software)
            && Strutil::parse_prefix(software, "OpenImageIO")) {
            int major = 0, minor = 0, patch = 0;
            if (Strutil::parse_int(software, major)
                && Strutil::parse_char(software, '.')
                && Strutil::parse_int(software, minor)
                && Strutil::parse_char(software, '.')
                && Strutil::parse_int(software, patch)) {  // NOSONAR
                oiio_write_version = major * 10000 + minor * 100 + patch;
            }
        }
        // Old version of OIIO did not write the field correctly, so try
        // to compensate.
        if (oiio_write_version && oiio_write_version < 10803) {
            xres = yres = 1.0f;
        }
        m_spec.x = (int)(xpos * xres);
        m_spec.y = (int)(ypos * yres);
    } else {
        m_spec.x = 0;
        m_spec.y = 0;
    }
    m_spec.z = 0;

    // Start by assuming the "full" (aka display) window is the same as the
    // data window. That's what we'll stick to if there is no further
    // information in the file. But if the file has tags for the "full"
    // size, assume a display window with origin (0,0) and those dimensions.
    // (Unfortunately, there are no TIFF tags for "full" origin.)
    m_spec.full_x      = m_spec.x;
    m_spec.full_y      = m_spec.y;
    m_spec.full_z      = m_spec.z;
    m_spec.full_width  = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth  = m_spec.depth;
    if (TIFFGetField(m_tif, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1
        && TIFFGetField(m_tif, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1
        && width > 0 && height > 0) {
        m_spec.full_width  = width;
        m_spec.full_height = height;
        m_spec.full_x      = 0;
        m_spec.full_y      = 0;
    }

    if (TIFFIsTiled(m_tif)) {
        TIFFGetField(m_tif, TIFFTAG_TILEWIDTH, &m_spec.tile_width);
        TIFFGetField(m_tif, TIFFTAG_TILELENGTH, &m_spec.tile_height);
        TIFFGetFieldDefaulted(m_tif, TIFFTAG_TILEDEPTH, &m_spec.tile_depth);
    } else {
        m_spec.tile_width  = 0;
        m_spec.tile_height = 0;
        m_spec.tile_depth  = 0;
    }

    TIFFGetFieldDefaulted(m_tif, TIFFTAG_BITSPERSAMPLE, &m_bitspersample);
    m_spec.attribute("oiio:BitsPerSample", (int)m_bitspersample);

    unsigned short sampleformat = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(m_tif, TIFFTAG_SAMPLEFORMAT, &sampleformat);
    switch (m_bitspersample) {
    case 1:
    case 2:
    case 4:
    case 6:
        // Make 1, 2, 4, 6 bpp look like byte images
    case 8:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format(TypeDesc::UINT8);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format(TypeDesc::INT8);
        else
            m_spec.set_format(TypeDesc::UINT8);  // punt
        break;
    case 10:
    case 12:
    case 14:
        // Make 10, 12, 14 bpp look like 16 bit images
    case 16:
        if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format(TypeDesc::UINT16);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format(TypeDesc::INT16);
        else if (sampleformat == SAMPLEFORMAT_IEEEFP) {
            m_spec.set_format(TypeDesc::HALF);
            // Adobe extension, see http://chriscox.org/TIFFTN3d1.pdf
        } else
            m_spec.set_format(TypeDesc::UNKNOWN);
        break;
    case 24:
        // Make 24 bit look like 32 bit
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format(TypeDesc::FLOAT);
        else if (sampleformat == SAMPLEFORMAT_UINT)
            m_spec.set_format(TypeDesc::UINT32);
        else if (sampleformat == SAMPLEFORMAT_INT)
            m_spec.set_format(TypeDesc::INT32);
        else
            m_spec.set_format(TypeDesc::UNKNOWN);
        break;
    case 64:
        if (sampleformat == SAMPLEFORMAT_IEEEFP)
            m_spec.set_format(TypeDesc::DOUBLE);
        else
            m_spec.set_format(TypeDesc::UNKNOWN);
        break;
    default: m_spec.set_format(TypeDesc::UNKNOWN); break;
    }

    // Use the table for all the obvious things that can be mindlessly
    // shoved into the image spec.
    if (read_meta) {
        for (const auto& tag : tag_table("TIFF"))
            find_tag(tag.tifftag, tag.tifftype, tag.name);
        for (const auto& tag : tag_table("Exif"))
            find_tag(tag.tifftag, tag.tifftype, tag.name);
    }

    // Now we need to get fields "by hand" for anything else that is less
    // straightforward...

    m_compression = 0;
    TIFFGetFieldDefaulted(m_tif, TIFFTAG_COMPRESSION, &m_compression);
    m_spec.attribute("tiff:Compression", (int)m_compression);

    m_photometric = (m_spec.nchannels == 1 ? PHOTOMETRIC_MINISBLACK
                                           : PHOTOMETRIC_RGB);
    TIFFGetField(m_tif, TIFFTAG_PHOTOMETRIC, &m_photometric);
    m_spec.attribute("tiff:PhotometricInterpretation", (int)m_photometric);

    readspec_photometric();

    TIFFGetFieldDefaulted(m_tif, TIFFTAG_PLANARCONFIG, &m_planarconfig);
    m_separate = (m_planarconfig == PLANARCONFIG_SEPARATE
                  && m_spec.nchannels > 1
                  && m_photometric != PHOTOMETRIC_PALETTE);
    m_spec.attribute("tiff:PlanarConfiguration", (int)m_planarconfig);
    if (m_planarconfig == PLANARCONFIG_SEPARATE)
        m_spec.attribute("planarconfig", "separate");
    else
        m_spec.attribute("planarconfig", "contig");

    if (const char* compressname = tiff_compression_name(m_compression))
        m_spec.attribute("compression", compressname);
    m_predictor = PREDICTOR_NONE;
    if (!safe_tiffgetfield("Predictor", TIFFTAG_PREDICTOR, TypeUInt16,
                           &m_predictor))
        m_predictor = PREDICTOR_NONE;

    m_rowsperstrip = -1;
    if (!m_spec.tile_width) {
        TIFFGetField(m_tif, TIFFTAG_ROWSPERSTRIP, &m_rowsperstrip);
        if (m_rowsperstrip > 0)
            m_spec.attribute("tiff:RowsPerStrip", m_rowsperstrip);
    }

    // The libtiff docs say that only uncompressed images, or those with
    // rowsperstrip==1, support random access to scanlines.
    m_no_random_access = (m_compression != COMPRESSION_NONE
                          && m_rowsperstrip != 1);

    // Do we care about fillorder?  No, the TIFF spec says, "We
    // recommend that FillOrder=2 (lsb-to-msb) be used only in
    // special-purpose applications".  So OIIO will assume msb-to-lsb
    // convention until somebody finds a TIFF file in the wild that
    // breaks this assumption.

    unsigned short* sampleinfo  = NULL;
    unsigned short extrasamples = 0;
    TIFFGetField(m_tif, TIFFTAG_EXTRASAMPLES, &extrasamples, &sampleinfo);
    // std::cerr << "Extra samples = " << extrasamples << "\n";
    bool alpha_is_unassociated = false;  // basic assumption
    if (extrasamples) {
        // If the TIFF ExtraSamples tag was specified, use that to figure
        // out the meaning of alpha.
        int colorchannels = 3;
        if (m_photometric == PHOTOMETRIC_MINISWHITE
            || m_photometric == PHOTOMETRIC_MINISBLACK
            || m_photometric == PHOTOMETRIC_PALETTE
            || m_photometric == PHOTOMETRIC_MASK)
            colorchannels = 1;
        for (int i = 0, c = colorchannels;
             i < extrasamples && c < m_inputchannels; ++i, ++c) {
            // std::cerr << "   extra " << i << " " << sampleinfo[i] << "\n";
            if (sampleinfo[i] == EXTRASAMPLE_ASSOCALPHA) {
                // This is the alpha channel, associated as usual
                m_spec.alpha_channel = c;
            } else if (sampleinfo[i] == EXTRASAMPLE_UNASSALPHA) {
                // This is the alpha channel, but color is unassociated
                m_spec.alpha_channel  = c;
                alpha_is_unassociated = true;
                if (m_keep_unassociated_alpha)
                    m_spec.attribute("oiio:UnassociatedAlpha", 1);
            } else {
                OIIO_DASSERT(sampleinfo[i] == EXTRASAMPLE_UNSPECIFIED);
                // This extra channel is not alpha at all.  Undo any
                // assumptions we previously made about this channel.
                if (m_spec.alpha_channel == c) {
                    m_spec.channelnames[c] = Strutil::fmt::format("channel{}",
                                                                  c);
                    m_spec.alpha_channel   = -1;
                }
            }
        }
        if (m_photometric == PHOTOMETRIC_SEPARATED)
            m_spec.alpha_channel = -1;  // ignore alpha in CMYK
        if (m_spec.alpha_channel >= 0
            && m_spec.alpha_channel < m_spec.nchannels) {
            while (m_spec.channelnames.size() < size_t(m_spec.nchannels))
                m_spec.channelnames.push_back(
                    Strutil::fmt::format("channel{}", m_spec.nchannels));
            m_spec.channelnames[m_spec.alpha_channel] = "A";
            // Special case: "R","A" should really be named "Y","A", since
            // the first channel is luminance, not red.
            if (m_spec.nchannels == 2 && m_spec.alpha_channel == 1)
                m_spec.channelnames[0] = "Y";
        }
    }
    if (alpha_is_unassociated)
        m_spec.attribute("tiff:UnassociatedAlpha", 1);
    // Will we need to do alpha conversions?
    m_convert_alpha = (m_spec.alpha_channel >= 0 && alpha_is_unassociated
                       && !m_keep_unassociated_alpha);

    // N.B. we currently ignore the following TIFF fields:
    // GrayResponseCurve GrayResponseUnit
    // MaxSampleValue MinSampleValue
    // NewSubfileType SubfileType(deprecated)
    // Colorimetry fields

    // If we've been instructed to skip reading metadata, because it is
    // assumed to be identical to what we already have in m_spec,
    // skip everything following.
    if (!read_meta)
        return true;

    short resunit = -1;
    TIFFGetField(m_tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
    switch (resunit) {
    case RESUNIT_NONE: m_spec.attribute("ResolutionUnit", "none"); break;
    case RESUNIT_INCH: m_spec.attribute("ResolutionUnit", "in"); break;
    case RESUNIT_CENTIMETER: m_spec.attribute("ResolutionUnit", "cm"); break;
    }
    float xdensity = m_spec.get_float_attribute("XResolution", 0.0f);
    float ydensity = m_spec.get_float_attribute("YResolution", 0.0f);
    if (xdensity && ydensity)
        m_spec.attribute("PixelAspectRatio", ydensity / xdensity);

    get_matrix_attribute("worldtocamera", TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA);
    get_matrix_attribute("worldtoscreen", TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN);
    get_int_attribute("tiff:subfiletype", TIFFTAG_SUBFILETYPE);
    // FIXME -- should subfiletype be "conventionized" and used for all
    // plugins uniformly?

    // Special names for shadow maps
    char* s = NULL;
    TIFFGetField(m_tif, TIFFTAG_PIXAR_TEXTUREFORMAT, &s);
    if (s)
        m_emulate_mipmap = true;
    if (s && !strcmp(s, "Shadow")) {
        for (int c = 0; c < m_spec.nchannels; ++c)
            m_spec.channelnames[c] = "z";
    }

    /// read color profile
    unsigned int icc_datasize = 0;
    uint8_t* icc_buf          = NULL;
    TIFFGetField(m_tif, TIFFTAG_ICCPROFILE, &icc_datasize, &icc_buf);
    if (icc_datasize && icc_buf) {
        m_spec.attribute(ICC_PROFILE_ATTR,
                         TypeDesc(TypeDesc::UINT8, icc_datasize), icc_buf);
        std::string errormsg;
        bool ok = decode_icc_profile(cspan<uint8_t>(icc_buf, icc_datasize),
                                     m_spec, errormsg);
        if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
            errorfmt("Possible corrupt file, could not decode ICC profile: {}\n",
                     errormsg);
            return false;
        }
    }

    // Search for an EXIF IFD in the TIFF file, and if found, rummage
    // around for Exif fields.
    toff_t exifoffset = 0;
    if (TIFFGetField(m_tif, TIFFTAG_EXIFIFD, &exifoffset)) {
        if (TIFFReadEXIFDirectory(m_tif, exifoffset)) {
            for (const auto& tag : tag_table("Exif"))
                find_tag(tag.tifftag, tag.tifftype, tag.name);
            // Look for a Makernote
            auto makerfield = find_field(EXIF_MAKERNOTE, TIFF_UNDEFINED);
            // std::unique_ptr<uint32_t[]> buf (new uint32_t[]);
            if (makerfield) {
                // bool ok = TIFFGetField (m_tif, tag, dest, &ptr);
                unsigned int mn_datasize = 0;
                unsigned char* mn_buf    = NULL;
                TIFFGetField(m_tif, EXIF_MAKERNOTE, &mn_datasize, &mn_buf);
            }
            // Exif spec says that anything other than 0xffff==uncalibrated
            // should be interpreted to be sRGB.
            if (m_spec.get_int_attribute("Exif:ColorSpace") != 0xffff)
                m_spec.attribute("oiio:ColorSpace", "sRGB");
            // NOTE: We must set "oiio:ColorSpace" explicitly, not call
            // set_colorspace, or it will erase several other TIFF attribs we
            // need to preserve.
        }
        // TIFFReadEXIFDirectory seems to do something to the internal state
        // that requires a TIFFSetDirectory to set things straight again.
        TIFFSetDirectory(m_tif, m_actual_subimage);
    }

    // Search for IPTC metadata in IIM form -- but older versions of
    // libtiff botch the size, so ignore it for very old libtiff.
    int iptcsize         = 0;
    const char* iptcdata = nullptr;
    TypeDesc iptctype    = tiffgetfieldtype(TIFFTAG_RICHTIFFIPTC);
    if (TIFFGetField(m_tif, TIFFTAG_RICHTIFFIPTC, &iptcsize, &iptcdata)
        && iptcsize > 0) {
        std::vector<char> iptc;
        if (iptctype.size() == 4) {
            // Some TIFF files in the wild inexplicably think their IPTC
            // data are stored as longs, and we have to undo any byte
            // swapping that may have occurred.
            iptcsize *= 4;
            iptc.assign(iptcdata, iptcdata + iptcsize);
            if (TIFFIsByteSwapped(m_tif))
                TIFFSwabArrayOfLong((uint32_t*)&iptc[0], iptcsize / 4);
        } else {
            iptc.assign(iptcdata, iptcdata + iptcsize);
        }
        decode_iptc_iim(&iptc[0], iptcsize, m_spec);
    }

    // Search for an XML packet containing XMP (IPTC, Exif, etc.)
    int xmlsize         = 0;
    const void* xmldata = NULL;
    if (TIFFGetField(m_tif, TIFFTAG_XMLPACKET, &xmlsize, &xmldata)) {
        // std::cerr << "Found XML data, size " << xmlsize << "\n";
        if (xmldata && xmlsize) {
            std::string xml((const char*)xmldata, xmlsize);
            decode_xmp(xml, m_spec);
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
    if (m_spec.get_string_attribute("Software")
        == m_spec.get_string_attribute("IPTC:OriginatingProgram"))
        m_spec.erase_attribute("IPTC:OriginatingProgram");

    std::string desc = m_spec.get_string_attribute("ImageDescription");
    // If ImageDescription and IPTC:Caption are identical, kill the latter
    if (desc == m_spec.get_string_attribute("IPTC:Caption"))
        m_spec.erase_attribute("IPTC:Caption");

    // Because TIFF doesn't support arbitrary metadata, we look for certain
    // hints in the ImageDescription and turn them into metadata, also
    // removing them from the ImageDescrption.
    bool updatedDesc = false;
    auto cc = Strutil::excise_string_after_head(desc, "oiio:ConstantColor=");
    if (cc.size()) {
        m_spec.attribute("oiio:ConstantColor", cc);
        updatedDesc = true;
    }
    auto ac = Strutil::excise_string_after_head(desc, "oiio:AverageColor=");
    if (ac.size()) {
        m_spec.attribute("oiio:AverageColor", ac);
        updatedDesc = true;
    }
    std::string sha = Strutil::excise_string_after_head(desc, "oiio:SHA-1=");
    if (sha.empty())  // back compatibility with OIIO < 1.5
        sha = Strutil::excise_string_after_head(desc, "SHA-1=");
    if (sha.size()) {
        m_spec.attribute("oiio:SHA-1", sha);
        updatedDesc = true;
    }
    std::string handed = Strutil::excise_string_after_head(desc,
                                                           "oiio:handed=");
    if (handed.size() && (handed == "left" || handed == "right")) {
        m_spec.attribute("handed", handed);
        updatedDesc = true;
    }

    if (updatedDesc) {
        string_view d(desc);
        Strutil::skip_whitespace(d);  // erase if it's only whitespace
        if (d.size())
            m_spec.attribute("ImageDescription", desc);
        else
            m_spec.erase_attribute("ImageDescription");
    }

    // Squash some problematic texture metadata if we suspect it's wrong
    pvt::check_texture_metadata_sanity(m_spec);

    if (m_testopenconfig)  // open-with-config debugging
        m_spec.attribute("oiio:DebugOpenConfig!", 42);

    return true;
}



void
TIFFInput::readspec_photometric()
{
    switch (m_photometric) {
    case PHOTOMETRIC_SEPARATED: {
        // Photometric "separated" is "usually CMYK".
        m_spec.channelnames.clear();
        short inkset       = INKSET_CMYK;
        short numberofinks = 0;
        TIFFGetFieldDefaulted(m_tif, TIFFTAG_INKSET, &inkset);
        TIFFGetFieldDefaulted(m_tif, TIFFTAG_NUMBEROFINKS, &numberofinks);
        if (inkset == INKSET_CMYK && m_spec.nchannels == 4) {
            // True CMYK
            m_spec.attribute("tiff:ColorSpace", "CMYK");
            if (m_raw_color) {
                m_spec.channelnames.resize(4);
                m_spec.channelnames[0] = "C";
                m_spec.channelnames[1] = "M";
                m_spec.channelnames[2] = "Y";
                m_spec.channelnames[3] = "K";
                m_spec.attribute("oiio:ColorSpace", "CMYK");
            } else {
                // Silently convert to RGB
                m_spec.nchannels = 3;
                m_spec.default_channel_names();
            }
        } else {
            // Non-CMYK ink set
            m_spec.attribute("tiff:ColorSpace", "color separated");
            m_spec.attribute("oiio:ColorSpace", "color separated");
            m_raw_color = true;  // Conversion to RGB doesn't make sense
            const char* inknames = NULL;
            if (safe_tiffgetfield("tiff:InkNames", TIFFTAG_INKNAMES,
                                  TypeUnknown, &inknames)
                && inknames && inknames[0] && numberofinks) {
                m_spec.channelnames.clear();
                // Decode the ink names, which are all concatenated together.
                for (int i = 0; i < int(numberofinks); ++i) {
                    string_view ink(inknames);
                    if (ink.size()) {
                        m_spec.channelnames.emplace_back(ink);
                        inknames += ink.size() + 1;
                    } else {
                        // Run out of road
                        numberofinks = i;
                    }
                }
            } else {
                numberofinks = 0;
            }
            // No ink names. Make it up.
            for (int i = numberofinks; i < m_spec.nchannels; ++i)
                m_spec.channelnames.emplace_back(
                    Strutil::fmt::format("ink{}", i));
        }
        break;
    }
    case PHOTOMETRIC_YCBCR: m_spec.attribute("tiff:ColorSpace", "YCbCr"); break;
    case PHOTOMETRIC_CIELAB:
        m_spec.attribute("tiff:ColorSpace", "CIELAB");
        break;
    case PHOTOMETRIC_ICCLAB:
        m_spec.attribute("tiff:ColorSpace", "ICCLAB");
        break;
    case PHOTOMETRIC_ITULAB:
        m_spec.attribute("tiff:ColorSpace", "ITULAB");
        break;
    case PHOTOMETRIC_LOGL: m_spec.attribute("tiff:ColorSpace", "LOGL"); break;
    case PHOTOMETRIC_LOGLUV:
        m_spec.attribute("tiff:ColorSpace", "LOGLUV");
        break;
    case PHOTOMETRIC_PALETTE: {
        m_spec.attribute("tiff:ColorSpace", "palette");
        // Read the color map
        unsigned short *r = NULL, *g = NULL, *b = NULL;
        TIFFGetField(m_tif, TIFFTAG_COLORMAP, &r, &g, &b);
        OIIO_ASSERT(r != NULL && g != NULL && b != NULL);
        m_colormap.clear();
        m_colormap.reserve(3 * (1 << m_bitspersample));
        m_colormap.insert(m_colormap.end(), r, r + (1 << m_bitspersample));
        m_colormap.insert(m_colormap.end(), g, g + (1 << m_bitspersample));
        m_colormap.insert(m_colormap.end(), b, b + (1 << m_bitspersample));
        // Palette TIFF images are always 3 channels, uint8 (to the client)
        m_spec.nchannels = 3;
        m_spec.set_format(TypeUInt8);
        m_spec.default_channel_names();
        if (m_bitspersample < m_spec.format.size() * 8) {
            // For palette images with unusual bits per sample, set
            // oiio:BitsPerSample to the "full" version, to avoid problems
            // when copying the file back to a TIFF file (we don't write
            // palette images), but do leave "tiff:BitsPerSample" to reflect
            // the original file.
            m_spec.attribute("tiff:BitsPerSample", (int)m_bitspersample);
            m_spec.attribute("oiio:BitsPerSample",
                             (int)m_spec.format.size() * 8);
        }
        // FIXME - what about palette + extra (alpha?) channels?  Is that
        // allowed?  And if so, ever encountered in the wild?
        break;
    }
    }

    // For some PhotometricInterpretation modes that are both rare and hairy
    // to handle, we use libtiff's TIFFRGBA interface and have it give us 8
    // bit RGB values.
    bool is_jpeg         = (m_compression == COMPRESSION_JPEG
                    || m_compression == COMPRESSION_OJPEG);
    bool is_nonspectral  = (m_photometric == PHOTOMETRIC_YCBCR
                           || m_photometric == PHOTOMETRIC_CIELAB
                           || m_photometric == PHOTOMETRIC_ICCLAB
                           || m_photometric == PHOTOMETRIC_ITULAB
                           || m_photometric == PHOTOMETRIC_LOGL
                           || m_photometric == PHOTOMETRIC_LOGLUV);
    m_use_rgba_interface = false;
    m_rgbadata.clear();
    if ((is_jpeg && m_spec.nchannels != 3)
        || (is_nonspectral && !m_raw_color)) {
        m_use_rgba_interface = true;
        // This falls back to looking like uint8 images
        m_spec.format = TypeDesc::UINT8;
        m_spec.channelformats.clear();
        m_photometric = PHOTOMETRIC_RGB;
    }

    // If we're not using the RGBA interface, but we have one of these
    // non-spectral color spaces, set the OIIO color space attribute to
    // the tiff:colorspace value.
    if (is_nonspectral && !m_use_rgba_interface) {
        m_spec.attribute("oiio:ColorSpace",
                         m_spec.get_string_attribute("tiff:ColorSpace"));
    }
}



bool
TIFFInput::close()
{
    close_tif();
    init();  // Reset to initial state
    return true;
}



/// Helper: Convert n pixels from separate (RRRGGGBBB) to contiguous
/// (RGBRGBRGB) planarconfig.
void
TIFFInput::separate_to_contig(size_t nplanes, size_t nvals,
                              cspan<std::byte> separate, span<std::byte> contig)
{
    size_t channelbytes = m_spec.channel_bytes();
    OIIO_DASSERT(nplanes * nvals * channelbytes <= separate.size()
                 && nplanes * nvals * channelbytes <= contig.size());
    for (size_t p = 0; p < nvals; ++p)                 // loop over pixels
        for (size_t c = 0; c < nplanes; ++c)           // loop over channels
            for (size_t i = 0; i < channelbytes; ++i)  // loop over data bytes
                contig[(p * nplanes + c) * channelbytes + i]
                    = separate[(c * nvals + p) * channelbytes + i];
}



// palette_to_rgb, for either a uint8 or uint16 valued palette
void
TIFFInput::palette_to_rgb(size_t n, cspan<uint8_t> palettepels,
                          span<uint8_t> rgb)
{
    size_t vals_per_byte = 8 / m_bitspersample;
    size_t entries       = 1 << m_bitspersample;
    size_t highest       = entries - 1;
    OIIO_DASSERT(m_spec.nchannels == 3);
    OIIO_DASSERT(m_colormap.size() == 3 * entries);
    OIIO_DASSERT(palettepels.size() == n && rgb.size() == n * 3);
    for (size_t x = 0; x < n; ++x) {
        uint32_t i = palettepels[x / vals_per_byte];
        i >>= (m_bitspersample * (vals_per_byte - 1 - (x % vals_per_byte)));
        i &= highest;
        rgb[3 * x + 0] = m_colormap[0 * entries + i] / 257;
        rgb[3 * x + 1] = m_colormap[1 * entries + i] / 257;
        rgb[3 * x + 2] = m_colormap[2 * entries + i] / 257;
    }
}



void
TIFFInput::palette_to_rgb(size_t n, cspan<uint16_t> palettepels,
                          span<uint8_t> rgb)
{
    // palette_to_rgb(int(n), palettepels.data(), rgb.data());
    // return;
    size_t entries = 1 << m_bitspersample;
    size_t highest = entries - 1;
    OIIO_DASSERT(m_spec.nchannels == 3);
    OIIO_DASSERT(m_colormap.size() == 3 * entries);
    OIIO_DASSERT(palettepels.size() == n && rgb.size() == n * 3);
    for (size_t x = 0; x < n; ++x) {
        uint32_t i = palettepels[x];
        i &= highest;
        rgb[3 * x + 0] = m_colormap[0 * entries + i] / 257;
        rgb[3 * x + 1] = m_colormap[1 * entries + i] / 257;
        rgb[3 * x + 2] = m_colormap[2 * entries + i] / 257;
    }
}



void
TIFFInput::bit_convert(int n, const unsigned char* in, int inbits, void* out,
                       int outbits)
{
    OIIO_DASSERT(inbits >= 1 && inbits < 32);  // surely bugs if not
    uint32_t highest = (1 << inbits) - 1;
    int B = 0, b = 0;
    // Invariant:
    // So far, we have used in[0..B-1] and the high b bits of in[B].
    for (int i = 0; i < n; ++i) {
        long long val = 0;
        int valbits   = 0;  // bits so far we've accumulated in val
        while (valbits < inbits) {
            // Invariant: we have already accumulated valbits of the next
            // needed value (of a total of inbits), living in the valbits
            // low bits of val.
            int out_left = inbits - valbits;  // How much more we still need
            int in_left  = 8 - b;             // Bits still available in in[B].
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
            ((unsigned char*)out)[i] = (unsigned char)((val * 0xff) / highest);
        else if (outbits == 16)
            ((unsigned short*)out)[i] = (unsigned short)((val * 0xffff)
                                                         / highest);
        else
            ((unsigned int*)out)[i] = (unsigned int)((val * 0xffffffff)
                                                     / highest);
    }
}



void
TIFFInput::invert_photometric(int n, void* data)
{
    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8: {
        unsigned char* d = (unsigned char*)data;
        for (int i = 0; i < n; ++i)
            d[i] = 255 - d[i];
        break;
    }
    default: break;
    }
}



template<typename T>
static void
cmyk_to_rgb(int n, const T* cmyk, size_t cmyk_stride, T* rgb, size_t rgb_stride)
{
    for (; n; --n, cmyk += cmyk_stride, rgb += rgb_stride) {
        float C = convert_type<T, float>(cmyk[0]);
        float M = convert_type<T, float>(cmyk[1]);
        float Y = convert_type<T, float>(cmyk[2]);
        float K = convert_type<T, float>(cmyk[3]);
        float R = (1.0f - C) * (1.0f - K);
        float G = (1.0f - M) * (1.0f - K);
        float B = (1.0f - Y) * (1.0f - K);
        rgb[0]  = convert_type<float, T>(R);
        rgb[1]  = convert_type<float, T>(G);
        rgb[2]  = convert_type<float, T>(B);
    }
}



bool
TIFFInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                                void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    auto native_sl_bytes = m_spec.scanline_bytes(true);
    return read_native_scanline_locked(subimage, miplevel, y,
                                       as_writable_bytes(data, native_sl_bytes));
}



bool
TIFFInput::read_native_scanline_locked(int subimage, int miplevel, int y,
                                       span<std::byte> data)
{
    // Not necessary, we assume this has already been done:
    //
    // lock_guard lock(*this);
    // if (!seek_subimage(subimage, miplevel))
    //     return false;
#ifndef NDEBUG /* double check in debug mode */
    if (!valid_raw_span_size(data, m_spec, m_spec.x, m_spec.x + m_spec.width, y,
                             y + 1))
        return false;
#endif

    y -= m_spec.y;

    if (m_use_rgba_interface) {
        // We punted and used the RGBA image interface -- copy from buffer.
        // libtiff has no way to read just one scanline as RGBA. So we
        // buffer the whole image.
        if (!m_rgbadata.size()) {  // first time through: allocate & read
            m_rgbadata.resize(m_spec.image_pixels());
            bool ok = TIFFReadRGBAImageOriented(m_tif, m_spec.width,
                                                m_spec.height,
                                                m_rgbadata.data(),
                                                ORIENTATION_TOPLEFT, 0);
            if (!ok) {
                std::string err = oiio_tiff_last_error();
                errorfmt("Unknown error trying to read TIFF as RGBA ({})",
                         err.size() ? err.c_str() : "unknown error");
                return false;
            }
        }
        copy_image(m_spec.nchannels, m_spec.width, 1, 1,
                   &m_rgbadata[y * size_t(m_spec.width)], m_spec.nchannels, 4,
                   4 * m_spec.width, AutoStride, data.data(), m_spec.nchannels,
                   m_spec.width * m_spec.nchannels, AutoStride);
        return true;
    }

    // Make sure there's enough scratch space
    int nvals = m_spec.width * m_inputchannels;
    if (m_photometric == PHOTOMETRIC_PALETTE && m_bitspersample > 8)
        m_scratch.resize(nvals * 2);  // special case for 16 bit palette
    else
        m_scratch.resize(nvals * m_spec.format.size());

    // How many color planes to read
    int planes = m_separate ? m_inputchannels : 1;

    // For compression modes that don't support random access to scanlines
    // (which I *think* is only LZW), we need to emulate random access by
    // re-seeking.
    if (m_no_random_access && m_next_scanline != y) {
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
            // We need to close the TIFF file s that we can re-open and
            // seek back to the beginning of this subimage. The close_tif()
            // accomplishes that. It's important not to do a full close()
            // here, because that would also call init() to fully reset
            // to a fresh ImageInput, thus forgetting any configuration
            // settings such as raw_color or keep_unassociated_alpha.
            close_tif();
            if (!open(m_filename, dummyspec)
                || !seek_subimage(old_subimage, old_miplevel)) {
                return false;  // Somehow, the re-open failed
            }
            OIIO_DASSERT(m_next_scanline == 0
                         && current_subimage() == old_subimage
                         && current_miplevel() == old_miplevel);
        }
        while (m_next_scanline < y) {
            // Keep reading until we're read the scanline we really need
            for (int c = 0; c < planes; ++c) { /* planes==1 for contig */
                if (TIFFReadScanline(m_tif, &m_scratch[0], m_next_scanline, c)
                    < 0) {
#if OIIO_TIFFLIB_VERSION < 40500
                    errorfmt("{}", oiio_tiff_last_error());
#endif
                    return false;
                }
            }
            ++m_next_scanline;
        }
    }
    m_next_scanline = y + 1;

    bool need_bit_convert = (m_bitspersample != 8 && m_bitspersample != 16
                             && m_bitspersample != 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadScanline(m_tif, m_scratch.data(), y) < 0) {
#if OIIO_TIFFLIB_VERSION < 40500
            errorfmt("{}", oiio_tiff_last_error());
#endif
            return false;
        }
        size_t n(m_spec.width);
        if (m_bitspersample <= 8)
            palette_to_rgb(n,
                           make_cspan(reinterpret_cast<const uint8_t*>(
                                          m_scratch.data()),
                                      n),
                           span_cast<uint8_t>(data));
        else if (m_bitspersample == 16)
            palette_to_rgb(n,
                           make_cspan(reinterpret_cast<const uint16_t*>(
                                          m_scratch.data()),
                                      n),
                           span_cast<uint8_t>(data));
        return true;
    }
    // Not palette...

    size_t plane_bytes = m_spec.width * m_spec.format.size();
    size_t input_bytes = plane_bytes * m_inputchannels;
    // Where to read?  Directly into user data if no channel shuffling, bit
    // shifting, or CMYK conversion is needed, otherwise into scratch space.
    unsigned char* readbuf = reinterpret_cast<unsigned char*>(data.data());
    if (need_bit_convert || m_separate
        || (m_photometric == PHOTOMETRIC_SEPARATED && !m_raw_color))
        readbuf = &m_scratch[0];
    // Perform the reads.  Note that for contig, planes==1, so it will
    // only do one TIFFReadScanline.
    for (int c = 0; c < planes; ++c) { /* planes==1 for contig */
        if (TIFFReadScanline(m_tif, &readbuf[plane_bytes * c], y, c) < 0) {
#if OIIO_TIFFLIB_VERSION < 40500
            errorfmt("{}", oiio_tiff_last_error());
#endif
            return false;
        }
    }

    // Handle less-than-full bit depths
    if (m_bitspersample < 8) {
        // m_scratch now holds nvals n-bit values, contig or separate
        m_scratch2.resize(input_bytes);
        m_scratch.swap(m_scratch2);
        for (int c = 0; c < planes; ++c) /* planes==1 for contig */
            bit_convert(m_separate ? m_spec.width : nvals,
                        &m_scratch2[plane_bytes * c], m_bitspersample,
                        m_separate
                            ? &m_scratch[plane_bytes * c]
                            : (unsigned char*)data.data() + plane_bytes * c,
                        8);
    } else if (m_bitspersample > 8 && m_bitspersample < 16) {
        // m_scratch now holds nvals n-bit values, contig or separate
        m_scratch2.resize(input_bytes);
        m_scratch.swap(m_scratch2);
        for (int c = 0; c < planes; ++c) /* planes==1 for contig */
            bit_convert(m_separate ? m_spec.width : nvals,
                        &m_scratch2[plane_bytes * c], m_bitspersample,
                        m_separate
                            ? &m_scratch[plane_bytes * c]
                            : (unsigned char*)data.data() + plane_bytes * c,
                        16);
    } else if (m_bitspersample > 16 && m_bitspersample < 32) {
        // m_scratch now holds nvals n-bit values, contig or separate
        m_scratch2.resize(input_bytes);
        m_scratch.swap(m_scratch2);
        for (int c = 0; c < planes; ++c) /* planes==1 for contig */
            bit_convert(m_separate ? m_spec.width : nvals,
                        &m_scratch2[plane_bytes * c], m_bitspersample,
                        m_separate
                            ? &m_scratch[plane_bytes * c]
                            : (unsigned char*)data.data() + plane_bytes * c,
                        32);
    }

    // Handle "separate" planarconfig
    if (m_separate) {
        // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB).
        // We know the data is in m_scratch at this point, so
        // contiguize it into the user data area.
        if (m_photometric == PHOTOMETRIC_SEPARATED && !m_raw_color) {
            // CMYK->RGB means we need temp storage.
            m_scratch2.resize(input_bytes);
            separate_to_contig(planes, m_spec.width,
                               as_bytes(make_span(m_scratch)),
                               as_writable_bytes(make_span(m_scratch2)));
            m_scratch.swap(m_scratch2);
        } else {
            // If no CMYK->RGB conversion is necessary, we can "separate"
            // straight into the data area.
            separate_to_contig(planes, m_spec.width,
                               as_bytes(make_span(m_scratch)), data);
        }
    }

    // Handle CMYK
    if (m_photometric == PHOTOMETRIC_SEPARATED && !m_raw_color) {
        // The CMYK will be in m_scratch.
        if (spec().format == TypeDesc::UINT8) {
            cmyk_to_rgb(m_spec.width, (unsigned char*)&m_scratch[0],
                        m_inputchannels, (unsigned char*)data.data(),
                        m_spec.nchannels);
        } else if (spec().format == TypeDesc::UINT16) {
            cmyk_to_rgb(m_spec.width, (unsigned short*)&m_scratch[0],
                        m_inputchannels, (unsigned short*)data.data(),
                        m_spec.nchannels);
        } else {
            errorfmt("CMYK only supported for UINT8, UINT16");
            return false;
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric(nvals, data.data());

    return true;
}



bool
TIFFInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                 int yend, int z, void* data)
{
    // Implement the raw pointer version of read_native_scanlines for
    // tiff by calling the span version, with the assumed size.
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    ybegin      = clamp(ybegin, m_spec.y, m_spec.y + m_spec.height);
    yend        = clamp(yend, m_spec.y, m_spec.y + m_spec.height);
    size_t size = m_spec.scanline_bytes(true) * size_t(yend - ybegin);

    return TIFFInput::read_native_scanlines(subimage, miplevel, ybegin, yend,
                                            as_writable_bytes(data, size));
}



bool
TIFFInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                 int yend, span<std::byte> data)
{
    // If the stars all align properly, try to read strips, and use the
    // thread pool to parallelize the decompression. This can give a large
    // speedup (5x or more!) because the zip decompression dwarfs the
    // actual raw I/O. But libtiff is totally serialized, so we can only
    // parallelize by reading raw (compressed) strips then making calls to
    // zlib ourselves to decompress. Don't bother trying to handle any of
    // the uncommon cases with strips. This covers most real-world cases.
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    yend        = std::min(yend, spec().y + spec().height);
    int nstrips = (yend - ybegin + m_rowsperstrip - 1) / m_rowsperstrip;
    auto native_sl_bytes = m_spec.scanline_bytes(true);

    // Validate that the span provided can hold the requested scanlines.
    if (!valid_raw_span_size(data, m_spec, m_spec.x, m_spec.x + m_spec.width,
                             ybegin, yend))
        return false;

    // See if it's easy to read this scanline range as strips. For edge
    // cases we don't with to deal with here, we just call the base class
    // read_native_scanlines, which in turn will loop over each scanline in
    // the range to call our read_native_scanline, which does handle the
    // full range of cases.
    bool read_as_strips =
        // we're reading more than one scanline
        (yend - ybegin) > 1  // no advantage to strips for single scanlines
        // scanline range must be complete strips
        && is_strip_boundary(ybegin)
        && is_strip_boundary(yend)
        // and not palette or cmyk color separated conversions
        && (m_photometric != PHOTOMETRIC_SEPARATED
            && m_photometric != PHOTOMETRIC_PALETTE)
        // no non-multiple-of-8 bits per sample
        && (spec().format.size() * 8 == m_bitspersample)
        // No other unusual cases
        && !m_use_rgba_interface;
    if (!read_as_strips) {
        // Punt and read one scanline at a time
        bool ok = true;
        for (int y = ybegin; ok && y < yend; ++y)
            ok &= read_native_scanline_locked(
                subimage, miplevel, y,
                data.subspan(native_sl_bytes * (y - ybegin), native_sl_bytes));
        return ok;
    }

    // Are we reading raw (compressed) strips and doing the decompression
    // ourselves?
    bool read_raw_strips =
        // only deflate/zip compression
        (m_compression
         == COMPRESSION_ADOBE_DEFLATE /*|| m_compression == COMPRESSION_NONE*/)
        // only horizontal predictor (or none)
        && (m_predictor == PREDICTOR_HORIZONTAL
            || m_predictor == PREDICTOR_NONE)
        // contig planarconfig only (for now?)
        && !m_separate
        // only uint8, uint16
        && (m_spec.format == TypeUInt8 || m_spec.format == TypeUInt16);

    // We know we wish to read as strips. But additionally, there are some
    // circumstances in which we want to read RAW strips, and do the
    // decompression ourselves, which we can feed to the thread pool to
    // perform in parallel.
    thread_pool* pool = default_thread_pool();
    bool parallelize =
        // and more than one, or no point parallelizing
        nstrips > 1
        // only if we are reading scanlines in order
        && ybegin == (m_next_scanline + m_spec.y)
        // only if we're threading and don't enter the thread pool recursively!
        && pool->size() > 1
        && !pool->is_worker()
        // and not if the feature is turned off
        && m_spec.get_int_attribute("tiff:multithread",
                                    OIIO::get_int_attribute("tiff:multithread"));

    // Make room for, and read the raw (still compressed) strips. As each
    // one is read, kick off the decompress and any other extras, to execute
    // in parallel.
    task_set tasks(pool);
    bool ok        = true;  // failed compression will stash a false here
    int y          = ybegin;
    size_t ystride = m_spec.scanline_bytes(true);
    int stripchans = m_separate ? 1 : m_spec.nchannels;  // chans in each strip
    int planes     = m_separate ? m_spec.nchannels : 1;  // color planes
    // N.B. "separate" planarconfig stores only one channel in a strip
    int stripvals = m_spec.width * stripchans
                    * m_rowsperstrip;  // values in a strip
    imagesize_t strip_bytes = stripvals * m_spec.format.size();
    size_t cbound           = compressBound((uLong)strip_bytes);
    std::unique_ptr<char[]> compressed_scratch;
    std::unique_ptr<char[]> separate_tmp(
        m_separate ? new char[strip_bytes * nstrips * planes] : nullptr);

    if (read_raw_strips) {
        // Make room for, and read the raw (still compressed) strips. As each
        // one is read, kick off the decompress and any other extras, to execute
        // in parallel.
        compressed_scratch.reset(new char[cbound * nstrips * planes]);
        for (size_t stripidx = 0; y + m_rowsperstrip <= yend;
             y += m_rowsperstrip, ++stripidx) {
            char* cbuf        = compressed_scratch.get() + stripidx * cbound;
            tstrip_t stripnum = (y - m_spec.y) / m_rowsperstrip;
            tsize_t csize     = TIFFReadRawStrip(m_tif, stripnum, cbuf,
                                                 tmsize_t(cbound));
            if (csize < 0) {
                std::string err = oiio_tiff_last_error();
                errorfmt("TIFFRead{}Strip failed reading line y={}: {}",
                         read_raw_strips ? "Raw" : "Encoded", y,
                         err.size() ? err.c_str() : "unknown error");
                ok = false;
            }
            auto out            = this;
            auto uncompress_etc = [=, &ok](int /*id*/) {
                out->uncompress_one_strip(cbuf, (unsigned long)csize,
                                          data.data(), strip_bytes,
                                          out->m_spec.nchannels,
                                          out->m_spec.width,
                                          out->m_rowsperstrip, &ok);
                if (out->m_photometric == PHOTOMETRIC_MINISWHITE)
                    out->invert_photometric(stripvals * stripchans,
                                            data.data());
            };
            if (parallelize) {
                // Push the rest of the work onto the thread pool queue
                tasks.push(pool->push(uncompress_etc));
            } else {
                uncompress_etc(0);
            }
            data = data.subspan(strip_bytes * planes);
        }

    } else {
        // One of the cases where we don't bother reading raw, we read
        // encoded strips. Still can be a lot more efficient than reading
        // individual scanlines. This is the clause that has to handle
        // "separate" planarconfig.
        int strips_in_file = (m_spec.height + m_rowsperstrip - 1)
                             / m_rowsperstrip;
        for (size_t stripidx = 0; y < yend; y += m_rowsperstrip, ++stripidx) {
            int myrps       = std::min(yend - y, m_rowsperstrip);
            int strip_endy  = std::min(y + m_rowsperstrip, yend);
            int mystripvals = m_spec.width * stripchans * (strip_endy - y);
            imagesize_t mystrip_bytes = mystripvals * m_spec.format.size();
            for (int c = 0; c < planes; ++c) {
                tstrip_t stripnum = ((y - m_spec.y) / m_rowsperstrip)
                                    + c * strips_in_file;
                tsize_t csize = TIFFReadEncodedStrip(m_tif, stripnum,
                                                     (char*)data.data()
                                                         + c * mystrip_bytes,
                                                     tmsize_t(mystrip_bytes));
                if (csize < 0) {
                    std::string err = oiio_tiff_last_error();
                    errorfmt("TIFFReadEncodedStrip failed reading line y={}: {}",
                             y, err.size() ? err.c_str() : "unknown error");
                    ok = false;
                }
            }
            if (m_photometric == PHOTOMETRIC_MINISWHITE)
                invert_photometric(mystripvals * planes, data.data());
            if (m_separate) {
                // handle "separate" planarconfig: copy to temp area, then
                // separate_to_contig it back.
                char* sepbuf = separate_tmp.get()
                               + stripidx * mystrip_bytes * planes;
                memcpy(sepbuf, data.data(), mystrip_bytes * planes);
                separate_to_contig(planes, m_spec.width * myrps,
                                   make_span((const std::byte*)sepbuf,
                                             mystrip_bytes * planes),
                                   data);
            }
            data = data.subspan(mystrip_bytes * planes);
        }
    }

    // If we have left over scanlines, read them serially
    m_next_scanline = y - m_spec.y;
    for (; y < yend; ++y) {
        bool ok = read_native_scanline_locked(subimage, miplevel, y, data);
        if (!ok)
            return false;
        data = data.subspan(ystride);
    }
    tasks.wait();
    return true;
}



bool
TIFFInput::read_native_tile_locked(int subimage, int miplevel, int x, int y,
                                   int z, span<std::byte> data)
{
    // Not necessary, we assume this has already been done:
    //
    // lock_guard lock(*this);
    // if (!seek_subimage(subimage, miplevel))
    //     return false;

#ifndef NDEBUG /* double check in debug mode */
    if (!valid_raw_span_size(data, m_spec, x, x + m_spec.tile_width, y,
                             y + m_spec.tile_height, z, z + m_spec.tile_depth))
        return false;
#endif

    x -= m_spec.x;
    y -= m_spec.y;

    if (m_use_rgba_interface) {
        // We punted and used the RGBA image interface
        // libtiff has a call to read just one tile as RGBA. So that's all
        // we need to do, not buffer the whole image.
        m_rgbadata.resize(m_spec.tile_pixels());
        bool ok = TIFFReadRGBATile(m_tif, x, y, m_rgbadata.data());
        if (!ok) {
            std::string err = oiio_tiff_last_error();
            errorfmt("Unknown error trying to read TIFF as RGBA ({})",
                     err.size() ? err.c_str() : "unknown error");
            return false;
        }
        // Copy, and use stride magic to reverse top-to-bottom, because
        // TIFFReadRGBATile always returns data in bottom-to-top order.
        int tw = std::min(m_spec.tile_width, m_spec.width - x);
        int th = std::min(m_spec.tile_height, m_spec.height - y);

        // When the vertical read size is smaller that the tile size
        // the actual data is in the bottom end of the tile
        // so copy_image should start from tile_height - read_height.
        // (Again, because TIFFReadRGBATile reverses the scanline order.)
        int vert_offset = m_spec.tile_height - th;

        copy_image(m_spec.nchannels, tw, th, 1,
                   &m_rgbadata[vert_offset * m_spec.tile_width
                               + (th - 1) * m_spec.tile_width],
                   m_spec.nchannels, 4, -m_spec.tile_width * 4, AutoStride,
                   data.data(), m_spec.nchannels,
                   m_spec.nchannels * imagesize_t(m_spec.tile_width),
                   AutoStride);
        return true;
    }

    imagesize_t tile_pixels = m_spec.tile_pixels();
    imagesize_t nvals       = tile_pixels * m_inputchannels;
    if (m_photometric == PHOTOMETRIC_PALETTE && m_bitspersample > 8)
        m_scratch.resize(nvals * 2);  // special case for 16 bit palette
    else
        m_scratch.resize(nvals * m_spec.format.size());
    bool no_bit_convert = (m_bitspersample == 8 || m_bitspersample == 16
                           || m_bitspersample == 32);
    if (m_photometric == PHOTOMETRIC_PALETTE) {
        // Convert from palette to RGB
        if (TIFFReadTile(m_tif, m_scratch.data(), x, y, z, 0) < 0) {
#if OIIO_TIFFLIB_VERSION < 40500
            errorfmt("{}", oiio_tiff_last_error());
#endif
            return false;
        }
        if (m_bitspersample <= 8)
            palette_to_rgb(tile_pixels,
                           make_cspan((uint8_t*)m_scratch.data(), tile_pixels),
                           span_cast<uint8_t>(data));
        else if (m_bitspersample == 16)
            palette_to_rgb(tile_pixels,
                           make_cspan((uint16_t*)m_scratch.data(), tile_pixels),
                           span_cast<uint8_t>(data));
    } else {
        // Not palette
        imagesize_t plane_bytes = m_spec.tile_pixels() * m_spec.format.size();
        int planes              = m_separate ? m_inputchannels : 1;
        std::vector<unsigned char> scratch2(m_separate ? m_spec.tile_bytes()
                                                       : 0);
        // Where to read?  Directly into user data if no channel shuffling
        // or bit shifting is needed, otherwise into scratch space.
        unsigned char* readbuf = (no_bit_convert && !m_separate)
                                     ? (unsigned char*)data.data()
                                     : m_scratch.data();
        // Perform the reads.  Note that for contig, planes==1, so it will
        // only do one TIFFReadTile.
        for (int c = 0; c < planes; ++c) /* planes==1 for contig */
            if (TIFFReadTile(m_tif, &readbuf[plane_bytes * c], x, y, z, c)
                < 0) {
#if OIIO_TIFFLIB_VERSION < 40500
                errorfmt("{}", oiio_tiff_last_error());
#endif
                return false;
            }
        if (m_bitspersample < 8) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap(m_scratch, scratch2);
            for (int c = 0; c < planes; ++c) /* planes==1 for contig */
                bit_convert(m_separate ? tile_pixels : nvals,
                            &scratch2[plane_bytes * c], m_bitspersample,
                            m_separate
                                ? m_scratch.data() + plane_bytes * c
                                : (unsigned char*)data.data() + plane_bytes * c,
                            8);
        } else if (m_bitspersample > 8 && m_bitspersample < 16) {
            // m_scratch now holds nvals n-bit values, contig or separate
            std::swap(m_scratch, scratch2);
            for (int c = 0; c < planes; ++c) /* planes==1 for contig */
                bit_convert(m_separate ? tile_pixels : nvals,
                            &scratch2[plane_bytes * c], m_bitspersample,
                            m_separate
                                ? m_scratch.data() + plane_bytes * c
                                : (unsigned char*)data.data() + plane_bytes * c,
                            16);
        }
        if (m_separate) {
            // Convert from separate (RRRGGGBBB) to contiguous (RGBRGBRGB).
            // We know the data is in m_scratch at this point, so
            // contiguize it into the user data area.
            separate_to_contig(planes, tile_pixels,
                               as_bytes(make_span(m_scratch)), data);
        }
    }

    if (m_photometric == PHOTOMETRIC_MINISWHITE)
        invert_photometric(nvals, data.data());

    return true;
}



bool
TIFFInput::read_native_tile(int subimage, int miplevel, int x, int y, int z,
                            void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    auto native_tile_bytes = m_spec.tile_bytes(true);
    return read_native_tile_locked(subimage, miplevel, x, y, z,
                                   as_writable_bytes(data, native_tile_bytes));
}



bool
TIFFInput::read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    OIIO_DASSERT(m_spec.tile_depth >= 1);
    size_t ntiles = size_t(
        (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width
        * (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height
        * (zend - zbegin + m_spec.tile_depth - 1) / m_spec.tile_depth);
    auto native_tile_bytes = m_spec.tile_bytes(true);
    return read_native_tiles_locked(
        subimage, miplevel, xbegin, xend, ybegin, yend, zbegin, zend, ntiles,
        as_writable_bytes(data, ntiles * native_tile_bytes));
}



bool
TIFFInput::read_native_tiles_locked(int subimage, int miplevel, int xbegin,
                                    int xend, int ybegin, int yend, int zbegin,
                                    int zend, size_t ntiles,
                                    span<std::byte> data)
{
    // Not necessary, we assume this has already been done:
    //
    // lock_guard lock(*this);
    // if (!seek_subimage(subimage, miplevel))
    //     return false;
    // if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend))
    //     return false;
#ifndef NDEBUG /* double check in debug mode */
    if (!valid_raw_span_size(data, m_spec, xbegin, xend, ybegin, yend, zbegin,
                             zend))
        return false;
#endif

    stride_t pixel_bytes   = (stride_t)m_spec.pixel_bytes(true);
    stride_t tileystride   = pixel_bytes * m_spec.tile_width;
    stride_t tilezstride   = tileystride * m_spec.tile_height;
    stride_t ystride       = (xend - xbegin) * pixel_bytes;
    stride_t zstride       = (yend - ybegin) * ystride;
    imagesize_t tile_bytes = m_spec.tile_bytes(true);

    // If the stars all align properly, use the thread pool to parallelize
    // the decompression. This can give a large speedup (5x or more!)
    // because the zip decompression dwarfs the actual raw I/O. But libtiff
    // is totally serialized, so we can only parallelize by making calls to
    // zlib ourselves and then writing "raw" (compressed) strips. Don't
    // bother trying to handle any of the uncommon cases with strips. This
    // covers most real-world cases.
    thread_pool* pool = default_thread_pool();
    bool parallelize =
        // more than one tile, or no point parallelizing
        ntiles > 1
        // and not palette or cmyk color separated conversions
        && (m_photometric != PHOTOMETRIC_SEPARATED
            && m_photometric != PHOTOMETRIC_PALETTE)
        // no non-multiple-of-8 bits per sample
        && (spec().format.size() * 8 == m_bitspersample)
        // contig planarconfig only (for now?)
        && !m_separate
        // only deflate/zip compression with horizontal predictor
        && m_compression == COMPRESSION_ADOBE_DEFLATE
        && m_predictor == PREDICTOR_HORIZONTAL
        // only uint8, uint16
        && (m_spec.format == TypeUInt8 || m_spec.format == TypeUInt16)
        // No other unusual cases
        && !m_use_rgba_interface
        // only if we're threading and don't enter the thread pool recursively!
        && pool->size() > 1
        && !pool->is_worker()
        // only if this ImageInput wasn't asked to be single-threaded
        && this->threads() != 1
        // and not if the feature is turned off
        && m_spec.get_int_attribute("tiff:multithread",
                                    OIIO::get_int_attribute("tiff:multithread"));

    if (!parallelize) {
        // If we're not parallelizing, just loop over the tiles and read each
        // one individually.
        std::unique_ptr<std::byte[]> pels(new std::byte[tile_bytes]);
        for (int z = zbegin; z < zend; z += m_spec.tile_depth) {
            for (int y = ybegin; y < yend; y += m_spec.tile_height) {
                for (int x = xbegin; x < xend; x += m_spec.tile_width) {
                    bool ok = read_native_tile_locked(subimage, miplevel, x, y,
                                                      z,
                                                      make_span(pels.get(),
                                                                tile_bytes));
                    if (!ok)
                        return false;
                    copy_image(m_spec.nchannels, m_spec.tile_width,
                               m_spec.tile_height, m_spec.tile_depth,
                               pels.get(), size_t(pixel_bytes), pixel_bytes,
                               tileystride, tilezstride,
                               data.data() + (z - zbegin) * zstride
                                   + (y - ybegin) * ystride
                                   + (x - xbegin) * pixel_bytes,
                               pixel_bytes, ystride, zstride);
                }
            }
        }
        return true;
    }

    // Make room for, and read the raw (still compressed) tiles. As each one
    // is read, kick off the decompress and any other extras, to execute in
    // parallel.
    int tilevals  = m_spec.tile_pixels() * m_spec.nchannels;
    size_t cbound = compressBound((uLong)tile_bytes);
    std::unique_ptr<char[]> compressed_scratch(new char[cbound * ntiles]);
    std::unique_ptr<char[]> scratch(new char[tile_bytes * ntiles]);
    task_set tasks(pool);
    bool ok = true;  // failed compression will stash a false here

    // Strutil::printf ("Parallel tile case %d %d  %d %d  %d %d\n",
    //                  xbegin, xend, ybegin, yend, zbegin, zend);
    size_t tileidx = 0;
    for (int z = zbegin; z < zend; z += m_spec.tile_depth) {
        for (int y = ybegin; ok && y < yend; y += m_spec.tile_height) {
            for (int x = xbegin; ok && x < xend;
                 x += m_spec.tile_width, ++tileidx) {
                char* cbuf = compressed_scratch.get() + tileidx * cbound;
                char* ubuf = scratch.get() + tileidx * tile_bytes;
                auto csize = TIFFReadRawTile(m_tif, tile_index(x, y, z), cbuf,
                                             tmsize_t(cbound));
                if (csize < 0) {
                    std::string err = oiio_tiff_last_error();
                    errorfmt(
                        "TIFFReadRawTile failed reading tile x={},y={},z={}: {}",
                        x, y, z, err.size() ? err.c_str() : "unknown error");
                    ok = false;
                    break;
                }
                // Push the rest of the work onto the thread pool queue
                auto out = this;
                tasks.push(pool->push([=, &ok](int /*id*/) {
                    out->uncompress_one_strip(cbuf, (unsigned long)csize, ubuf,
                                              tile_bytes, out->m_spec.nchannels,
                                              out->m_spec.tile_width,
                                              out->m_spec.tile_height
                                                  * out->m_spec.tile_depth,
                                              &ok);
                    if (out->m_photometric == PHOTOMETRIC_MINISWHITE)
                        out->invert_photometric(tilevals, ubuf);
                    copy_image(out->m_spec.nchannels, out->m_spec.tile_width,
                               out->m_spec.tile_height, out->m_spec.tile_depth,
                               ubuf, size_t(pixel_bytes), pixel_bytes,
                               tileystride, tilezstride,
                               data.data() + (z - zbegin) * zstride
                                   + (y - ybegin) * ystride
                                   + (x - xbegin) * pixel_bytes,
                               pixel_bytes, ystride, zstride);
                }));
            }
        }
    }
    tasks.wait();
    return ok;
}



bool
TIFFInput::read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                             int ybegin, int yend, span<std::byte> data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend))
        return false;
    if (!valid_raw_span_size(data, m_spec, m_spec.x, m_spec.x + m_spec.width,
                             ybegin, yend))
        return false;

    OIIO_DASSERT(m_spec.tile_depth == 1);
    size_t ntiles = size_t(
        (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width
        * (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height);
    return read_native_tiles_locked(subimage, miplevel, xbegin, xend, ybegin,
                                    yend, 0, 1, ntiles, data);
}



bool
TIFFInput::read_native_volumetric_tiles(int subimage, int miplevel, int xbegin,
                                        int xend, int ybegin, int yend,
                                        int zbegin, int zend,
                                        span<std::byte> data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend))
        return false;
    if (!valid_raw_span_size(data, m_spec, m_spec.x, m_spec.x + m_spec.width,
                             ybegin, yend, zbegin, zend))
        return false;

    OIIO_DASSERT(m_spec.tile_depth >= 1);
    size_t ntiles = size_t(
        (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width
        * (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height
        * (zend - zbegin + m_spec.tile_depth - 1) / m_spec.tile_depth);
    return read_native_tiles_locked(subimage, miplevel, xbegin, xend, ybegin,
                                    yend, zbegin, zend, ntiles, data);
}



bool
TIFFInput::read_scanline(int y, int z, TypeDesc format, void* data,
                         stride_t xstride)
{
    bool ok = ImageInput::read_scanline(y, z, format, data, xstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        {
            lock_guard lock(*this);
            if (format
                == TypeUnknown)  // unknown means retrieve the native type
                format = m_spec.format;
        }
        OIIO::premult(m_spec.nchannels, m_spec.width, 1, 1, 0 /*chbegin*/,
                      m_spec.nchannels /*chend*/, format, data, xstride,
                      AutoStride, AutoStride, m_spec.alpha_channel,
                      m_spec.z_channel);
    }
    return ok;
}



bool
TIFFInput::read_scanlines(int subimage, int miplevel, int ybegin, int yend,
                          int z, int chbegin, int chend, TypeDesc format,
                          void* data, stride_t xstride, stride_t ystride)
{
    bool ok = ImageInput::read_scanlines(subimage, miplevel, ybegin, yend, z,
                                         chbegin, chend, format, data, xstride,
                                         ystride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        int nchannels, alpha_channel, z_channel, width;
        {
            lock_guard lock(*this);
            seek_subimage(subimage, miplevel);
            nchannels     = m_spec.nchannels;
            alpha_channel = m_spec.alpha_channel;
            z_channel     = m_spec.z_channel;
            width         = m_spec.width;
            if (format == TypeUnknown)  // unknown == native type
                format = m_spec.format;
        }
        // NOTE: if the channel range we read doesn't include the alpha
        // channel, we don't have the alpha data to do the premult, so skip
        // this. Pity the hapless soul who tries to read only the first
        // three channels of an RGBA file with unassociated alpha. They will
        // not get the premultiplication they deserve.
        if (alpha_channel >= chbegin && alpha_channel < chend)
            OIIO::premult(nchannels, width, yend - ybegin, 1, chbegin, chend,
                          format, data, xstride, ystride, AutoStride,
                          alpha_channel, z_channel);
    }
    return ok;
}



bool
TIFFInput::read_tile(int x, int y, int z, TypeDesc format, void* data,
                     stride_t xstride, stride_t ystride, stride_t zstride)
{
    bool ok = ImageInput::read_tile(x, y, z, format, data, xstride, ystride,
                                    zstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        {
            lock_guard lock(*this);
            if (format
                == TypeUnknown)  // unknown means retrieve the native type
                format = m_spec.format;
        }
        OIIO::premult(m_spec.nchannels, m_spec.tile_width, m_spec.tile_height,
                      std::max(1, m_spec.tile_depth), 0, m_spec.nchannels,
                      format, data, xstride, ystride, zstride,
                      m_spec.alpha_channel, m_spec.z_channel);
    }
    return ok;
}



bool
TIFFInput::read_tiles(int subimage, int miplevel, int xbegin, int xend,
                      int ybegin, int yend, int zbegin, int zend, int chbegin,
                      int chend, TypeDesc format, void* data, stride_t xstride,
                      stride_t ystride, stride_t zstride)
{
    bool ok = ImageInput::read_tiles(subimage, miplevel, xbegin, xend, ybegin,
                                     yend, zbegin, zend, chbegin, chend, format,
                                     data, xstride, ystride, zstride);
    if (ok && m_convert_alpha) {
        // If alpha is unassociated and we aren't requested to keep it that
        // way, multiply the colors by alpha per the usual OIIO conventions
        // to deliver associated color & alpha.  Any auto-premultiplication
        // by alpha should happen after we've already done data format
        // conversions. That's why we do it here, rather than in
        // read_native_blah.
        int nchannels, alpha_channel, z_channel;
        {
            lock_guard lock(*this);
            seek_subimage(subimage, miplevel);
            nchannels     = m_spec.nchannels;
            alpha_channel = m_spec.alpha_channel;
            z_channel     = m_spec.z_channel;
            if (format == TypeUnknown)  // unknown == native type
                format = m_spec.format;
        }
        // NOTE: if the channel range we read doesn't include the alpha
        // channel, we don't have the alpha data to do the premult, so skip
        // this. Pity the hapless soul who tries to read only the first
        // three channels of an RGBA file with unassociated alpha. They will
        // not get the premultiplication they deserve.
        if (alpha_channel >= chbegin && alpha_channel < chend)
            OIIO::premult(nchannels, xend - xbegin, yend - ybegin,
                          zend - zbegin, chbegin, chend, format, data, xstride,
                          ystride, zstride, alpha_channel, z_channel);
    }
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
