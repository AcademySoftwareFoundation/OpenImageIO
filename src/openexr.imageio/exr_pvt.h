// Copyright 2021-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once


#include <OpenImageIO/Imath.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/typedesc.h>

#include <ImathBox.h>
#include <OpenEXR/IexThrowErrnoExc.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfRgbaFile.h>

#define OPENEXR_CODED_VERSION                                    \
    (OPENEXR_VERSION_MAJOR * 10000 + OPENEXR_VERSION_MINOR * 100 \
     + OPENEXR_VERSION_PATCH)

#define OPENEXR_HAS_FLOATVECTOR 1

#define ENABLE_EXR_DEBUG_PRINTS 0

OIIO_PLUGIN_NAMESPACE_BEGIN

// Lots of debugging printf turned on for DEBUG builds or if you define
// ENABLE_EXR_DEBUG_PRINTS above, *AND* the "OIIO_DEBUG_OPENEXR" environment
// variable is set to something numerically non-zero.
#if ENABLE_EXR_DEBUG_PRINTS || !defined(NDEBUG) /* allow debugging */
static bool exrdebug = Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_OPENEXR"))
                       || Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_ALL"));
#    define DBGEXR(...) \
        if (exrdebug)   \
        Strutil::print(__VA_ARGS__)
#else
#    define DBGEXR(...)
#endif


namespace pvt {

// Split a full channel name into layer and suffix.
void
split_name(string_view fullname, string_view& layer, string_view& suffix);

// Do the channels appear to be R, G, B (or known common aliases)?
bool
channels_are_rgb(const ImageSpec& spec);

}  // namespace pvt



// Custom file input stream, copying code from the class StdIFStream in OpenEXR,
// which would have been used if we just provided a filename. The difference is
// that this can handle UTF-8 file paths on all platforms.
class OpenEXRInputStream final : public Imf::IStream {
public:
    OpenEXRInputStream(const char* filename, Filesystem::IOProxy* io)
        : Imf::IStream(filename)
        , m_io(io)
    {
        if (!io || io->mode() != Filesystem::IOProxy::Read)
            throw Iex::IoExc("File input failed.");
    }
    bool read(char c[], int n) override
    {
        OIIO_DASSERT(m_io);
        if (m_io->read(c, n) != size_t(n))
            throw Iex::IoExc("Unexpected end of file.");
        return n;
    }
    uint64_t tellg() override { return m_io->tell(); }
    void seekg(uint64_t pos) override
    {
        if (!m_io->seek(pos))
            throw Iex::IoExc("File input failed.");
    }
    void clear() override {}

private:
    Filesystem::IOProxy* m_io = nullptr;
};



class OpenEXRInput final : public ImageInput {
public:
    OpenEXRInput();
    ~OpenEXRInput() override { close(); }
    const char* format_name(void) const override { return "openexr"; }
    int supports(string_view feature) const override
    {
        return (feature == "arbitrary_metadata"
                || feature == "exif"  // Because of arbitrary_metadata
                || feature == "ioproxy"
                || feature == "iptc"  // Because of arbitrary_metadata
                || feature == "multiimage");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool open(const std::string& name, ImageSpec& newspec) override
    {
        return open(name, newspec, ImageSpec());
    }
    bool close() override;
    int current_subimage(void) const override { return m_subimage; }
    int current_miplevel(void) const override { return m_miplevel; }
    bool seek_subimage(int subimage, int miplevel) override;
    ImageSpec spec(int subimage, int miplevel) override;
    ImageSpec spec_dimensions(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               int z, void* data) override;
    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               int z, int chbegin, int chend,
                               void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;
    bool read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                           int ybegin, int yend, int zbegin, int zend,
                           void* data) override;
    bool read_native_tiles(int subimage, int miplevel, int xbegin, int xend,
                           int ybegin, int yend, int zbegin, int zend,
                           int chbegin, int chend, void* data) override;
    bool read_native_deep_scanlines(int subimage, int miplevel, int ybegin,
                                    int yend, int z, int chbegin, int chend,
                                    DeepData& deepdata) override;
    bool read_native_deep_tiles(int subimage, int miplevel, int xbegin,
                                int xend, int ybegin, int yend, int zbegin,
                                int zend, int chbegin, int chend,
                                DeepData& deepdata) override;

    bool set_ioproxy(Filesystem::IOProxy* ioproxy) override
    {
        m_io = ioproxy;
        return true;
    }

private:
    struct PartInfo {
        std::atomic_bool initialized;
        ImageSpec spec;
        int topwidth;           ///< Width of top mip level
        int topheight;          ///< Height of top mip level
        int levelmode;          ///< The level mode
        int roundingmode;       ///< Rounding mode
        bool cubeface;          ///< It's a cubeface environment map
        bool luminance_chroma;  ///< It's a luminance chroma image
        int nmiplevels;         ///< How many MIP levels are there?
        Imath::Box2i top_datawindow;
        Imath::Box2i top_displaywindow;
        std::vector<Imf::PixelType> pixeltype;  ///< Imf pixel type for each chan
        std::vector<int> chanbytes;  ///< Size (in bytes) of each channel

        PartInfo()
            : initialized(false)
        {
        }
        PartInfo(const PartInfo& p)
            : initialized((bool)p.initialized)
            , spec(p.spec)
            , topwidth(p.topwidth)
            , topheight(p.topheight)
            , levelmode(p.levelmode)
            , roundingmode(p.roundingmode)
            , cubeface(p.cubeface)
            , luminance_chroma(p.luminance_chroma)
            , nmiplevels(p.nmiplevels)
            , top_datawindow(p.top_datawindow)
            , top_displaywindow(p.top_displaywindow)
            , pixeltype(p.pixeltype)
            , chanbytes(p.chanbytes)
        {
        }
        ~PartInfo() {}
        bool parse_header(OpenEXRInput* in, const Imf::Header* header);
        bool query_channels(OpenEXRInput* in, const Imf::Header* header);
        void compute_mipres(int miplevel, ImageSpec& spec) const;
    };
    friend struct PartInfo;

    std::vector<PartInfo> m_parts;               ///< Image parts
    OpenEXRInputStream* m_input_stream;          ///< Stream for input file
    Imf::MultiPartInputFile* m_input_multipart;  ///< Multipart input
    Imf::InputPart* m_scanline_input_part;
    Imf::TiledInputPart* m_tiled_input_part;
    Imf::DeepScanLineInputPart* m_deep_scanline_input_part;
    Imf::DeepTiledInputPart* m_deep_tiled_input_part;
    Imf::RgbaInputFile* m_input_rgba;
    Filesystem::IOProxy* m_io = nullptr;
    std::unique_ptr<Filesystem::IOProxy> m_local_io;
    int m_subimage;                     ///< What subimage are we looking at?
    int m_nsubimages;                   ///< How many subimages are there?
    int m_miplevel;                     ///< What MIP level are we looking at?
    std::vector<float> m_missingcolor;  ///< Color for missing tile/scanline

    void init()
    {
        m_input_stream             = NULL;
        m_input_multipart          = NULL;
        m_scanline_input_part      = NULL;
        m_tiled_input_part         = NULL;
        m_deep_scanline_input_part = NULL;
        m_deep_tiled_input_part    = NULL;
        m_input_rgba               = NULL;
        m_subimage                 = -1;
        m_miplevel                 = -1;
        m_io                       = nullptr;
        m_local_io.reset();
        m_missingcolor.clear();
    }

    bool read_native_tiles_individually(int subimage, int miplevel, int xbegin,
                                        int xend, int ybegin, int yend,
                                        int zbegin, int zend, int chbegin,
                                        int chend, void* data, stride_t xstride,
                                        stride_t ystride);

    // Fill in with 'missing' color/pattern.
    void fill_missing(int xbegin, int xend, int ybegin, int yend, int zbegin,
                      int zend, int chbegin, int chend, void* data,
                      stride_t xstride, stride_t ystride);

    // Prepare friend function for copyPixels
    friend class OpenEXROutput;
};



OIIO_PLUGIN_NAMESPACE_END
