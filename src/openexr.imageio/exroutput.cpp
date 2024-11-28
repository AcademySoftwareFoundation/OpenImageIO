// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/platform.h>

#include <OpenEXR/IlmThreadPool.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfEnvmap.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfTiledOutputFile.h>

#include "exr_pvt.h"

// The way that OpenEXR uses dynamic casting for attributes requires
// temporarily suspending "hidden" symbol visibility mode.
OIIO_PRAGMA_VISIBILITY_PUSH
OIIO_PRAGMA_WARNING_PUSH
OIIO_GCC_PRAGMA(GCC diagnostic ignored "-Wunused-parameter")
#include <OpenEXR/IexBaseExc.h>
#include <OpenEXR/ImfBoxAttribute.h>
#include <OpenEXR/ImfCRgbaFile.h>  // JUST to get symbols to figure out version!
#include <OpenEXR/ImfChromaticitiesAttribute.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfEnvmapAttribute.h>
#include <OpenEXR/ImfFloatAttribute.h>
#include <OpenEXR/ImfFloatVectorAttribute.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfIntAttribute.h>
#include <OpenEXR/ImfKeyCodeAttribute.h>
#include <OpenEXR/ImfMatrixAttribute.h>
#include <OpenEXR/ImfRationalAttribute.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfTimeCodeAttribute.h>
#include <OpenEXR/ImfVecAttribute.h>

#include <OpenEXR/ImfDeepFrameBuffer.h>
#include <OpenEXR/ImfDeepScanLineOutputPart.h>
#include <OpenEXR/ImfDeepTiledOutputPart.h>
#include <OpenEXR/ImfDoubleAttribute.h>
#include <OpenEXR/ImfMultiPartOutputFile.h>
#include <OpenEXR/ImfOutputPart.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfStringVectorAttribute.h>
#include <OpenEXR/ImfTiledOutputPart.h>
OIIO_PRAGMA_WARNING_POP
OIIO_PRAGMA_VISIBILITY_POP

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


// Custom file output stream that uses IOProxy for output.
class OpenEXROutputStream final : public Imf::OStream {
public:
    OpenEXROutputStream(const char* filename, Filesystem::IOProxy* io)
        : Imf::OStream(filename)
        , m_io(io)
    {
        if (!io || io->mode() != Filesystem::IOProxy::Write)
            throw Iex::IoExc("File output failed.");
    }
    void write(const char c[], int n) override
    {
        if (m_io->write(c, n) != size_t(n))
            throw Iex::IoExc("File output failed.");
    }
    uint64_t tellp() override { return m_io->tell(); }
    void seekp(uint64_t pos) override
    {
        if (!m_io->seek(pos))
            throw Iex::IoExc("File output failed.");
    }

private:
    Filesystem::IOProxy* m_io = nullptr;
};



class OpenEXROutput final : public ImageOutput {
public:
    OpenEXROutput();
    ~OpenEXROutput() override;
    const char* format_name(void) const override { return "openexr"; }
    int supports(string_view feature) const override;
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool open(const std::string& name, int subimages,
              const ImageSpec* specs) override;
    bool close() override;
    bool copy_image(ImageInput* in) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride,
                         stride_t ystride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool write_tiles(int xbegin, int xend, int ybegin, int yend, int zbegin,
                     int zend, TypeDesc format, const void* data,
                     stride_t xstride, stride_t ystride,
                     stride_t zstride) override;
    bool write_deep_scanlines(int ybegin, int yend, int z,
                              const DeepData& deepdata) override;
    bool write_deep_tiles(int xbegin, int xend, int ybegin, int yend,
                          int zbegin, int zend,
                          const DeepData& deepdata) override;
    bool set_ioproxy(Filesystem::IOProxy* ioproxy) override
    {
        m_io = ioproxy;
        return true;
    }

private:
    std::unique_ptr<OpenEXROutputStream>
        m_output_stream;  ///< Stream for output file
    std::unique_ptr<Imf::OutputFile>
        m_output_scanline;  ///< Input for scanline files
    std::unique_ptr<Imf::TiledOutputFile>
        m_output_tiled;  ///< Input for tiled files
    std::unique_ptr<Imf::MultiPartOutputFile> m_output_multipart;
    std::unique_ptr<Imf::OutputPart> m_scanline_output_part;
    std::unique_ptr<Imf::TiledOutputPart> m_tiled_output_part;
    std::unique_ptr<Imf::DeepScanLineOutputPart> m_deep_scanline_output_part;
    std::unique_ptr<Imf::DeepTiledOutputPart> m_deep_tiled_output_part;
    int m_levelmode;     ///< The level mode of the file
    int m_roundingmode;  ///< Rounding mode of the file
    int m_subimage;      ///< What subimage we're writing now
    int m_nsubimages;    ///< How many subimages are there?
    int m_miplevel;      ///< What miplevel we're writing now
    int m_nmiplevels;    ///< How many mip levels are there?
    std::vector<Imf::PixelType> m_pixeltype;  ///< Imf pixel type for each
                                              ///<   channel of current subimage
    std::vector<unsigned char> m_scratch;     ///< Scratch space for us to use
    std::vector<ImageSpec> m_subimagespecs;   ///< Saved subimage specs
    std::vector<Imf::Header> m_headers;
    Filesystem::IOProxy* m_io = nullptr;
    std::unique_ptr<Filesystem::IOProxy> m_local_io;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_output_stream   = NULL;
        m_output_scanline = NULL;
        m_output_tiled    = NULL;
        m_output_multipart.reset();
        m_scanline_output_part.reset();
        m_tiled_output_part.reset();
        m_deep_scanline_output_part.reset();
        m_deep_tiled_output_part.reset();
        m_levelmode = Imf::ONE_LEVEL;
        m_subimage  = -1;
        m_miplevel  = -1;
        m_subimagespecs.clear();
        m_subimagespecs.shrink_to_fit();
        m_headers.clear();
        m_headers.shrink_to_fit();
        m_io = nullptr;
        m_local_io.reset();
    }

    // Set up the header based on the given spec.  Also may doctor the
    // spec a bit.
    bool spec_to_header(ImageSpec& spec, int subimage, Imf::Header& header);

    // Compute an OpenEXR PixelType from an OIIO TypeDesc
    Imf::PixelType imfpixeltype(TypeDesc type);

    // Fill in m_pixeltype based on the spec
    void compute_pixeltypes(const ImageSpec& spec);

    // Add a parameter to the output
    bool put_parameter(const std::string& name, TypeDesc type, const void* data,
                       Imf::Header& header);

    // Decode the IlmImf MIP parameters from the spec.
    static void figure_mip(const ImageSpec& spec, int& nmiplevels,
                           int& levelmode, int& roundingmode);

    // Helper: if the channel names are nonsensical, fix them to keep the
    // app from shooting itself in the foot.
    void sanity_check_channelnames();

    bool copy_and_check_spec(const ImageSpec& srcspec, ImageSpec& dstspec)
    {
        // Arbitrarily limit res to 1M x 1M and 4k channels, assuming anything
        // beyond that is more likely to be a mistake than a legit request. We
        // may have to come back to this if these assumptions are wrong.
        if (!check_open(Create, srcspec,
                        { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 1 << 12 }))
            return false;
        if (&dstspec != &m_spec)
            dstspec = m_spec;
        return true;
    }
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
openexr_output_imageio_create()
{
    return new OpenEXROutput;
}

OIIO_EXPORT int openexr_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
openexr_imageio_library_version()
{
    return "OpenEXR " OPENEXR_VERSION_STRING;
}

OIIO_EXPORT const char* openexr_output_extensions[] = { "exr", "sxr", "mxr",
                                                        nullptr };

OIIO_PLUGIN_EXPORTS_END



namespace pvt {

void
set_exr_threads()
{
    static int exr_threads = 0;  // lives in exrinput.cpp
    static spin_mutex exr_threads_mutex;

    int oiio_threads = 1;
    OIIO::getattribute("exr_threads", oiio_threads);

    // 0 means all threads in OIIO, but single-threaded in OpenEXR
    // -1 means single-threaded in OIIO
    if (oiio_threads == 0) {
        oiio_threads = Sysutil::hardware_concurrency();
    } else if (oiio_threads == -1) {
        oiio_threads = 0;
    }
    spin_lock lock(exr_threads_mutex);
    if (exr_threads != oiio_threads) {
        exr_threads = oiio_threads;
        Imf::setGlobalThreadCount(exr_threads);
    }

#if OPENEXR_CODED_VERSION < 30108 && defined(_WIN32)
    // If we're ever in this function, which we would be any time we use
    // openexr threads, also proactively ensure that we exit the application,
    // we force the OpenEXR threadpool to shut down because their destruction
    // might cause us to hang on Windows when it tries to communicate with
    // threads that would have already been terminated without releasing any
    // held mutexes.
    // Addendum: But only for OpenEXR < 3.1.8 (beyond that we think it's
    // fixed on the OpenEXR side), and also only on Windows (the only platform
    // where we've seen this be symptomatic).
    static std::once_flag set_atexit_once;
    std::call_once(set_atexit_once, []() {
        std::atexit([]() {
            // print("EXITING and setting ilmthreads = 0\n");
            IlmThread::ThreadPool::globalThreadPool().setNumThreads(0);
        });
    });
#endif
}

}  // namespace pvt



OpenEXROutput::OpenEXROutput()
{
    pvt::set_exr_threads();
    init();
}



OpenEXROutput::~OpenEXROutput()
{
    // Close, if not already done.
    close();

    m_output_scanline.reset();
    m_output_tiled.reset();
    m_scanline_output_part.reset();
    m_tiled_output_part.reset();
    m_deep_scanline_output_part.reset();
    m_deep_tiled_output_part.reset();
    m_output_multipart.reset();
    m_output_stream.reset();
}



int
OpenEXROutput::supports(string_view feature) const
{
    if (feature == "tiles")
        return true;
    if (feature == "mipmap")
        return true;
    if (feature == "alpha")
        return true;
    if (feature == "nchannels")
        return true;
    if (feature == "channelformats")
        return true;
    if (feature == "displaywindow")
        return true;
    if (feature == "origin")
        return true;
    if (feature == "negativeorigin")
        return true;
    if (feature == "arbitrary_metadata")
        return true;
    if (feature == "exif")  // Because of arbitrary_metadata
        return true;
    if (feature == "iptc")  // Because of arbitrary_metadata
        return true;
    if (feature == "multiimage")
        return true;  // N.B. But OpenEXR does not support "appendsubimage"
    if (feature == "deepdata")
        return true;
    if (feature == "ioproxy")
        return true;

    // EXR supports random write order iff lineOrder is set to 'random Y'
    // and it's a tiled file.
    if (feature == "random_access" && m_spec.tile_width != 0) {
        const ParamValue* param = m_spec.find_attribute("openexr:lineOrder");
        const char* lineorder   = param ? *(char**)param->data() : NULL;
        return (lineorder && Strutil::iequals(lineorder, "randomY"));
    }

    // FIXME: we could support "empty"

    // Everything else, we either don't support or don't know about
    return false;
}



bool
OpenEXROutput::open(const std::string& name, const ImageSpec& userspec,
                    OpenMode mode)
{
    if (mode == Create) {
        if (userspec.deep)  // Fall back on multi-part OpenEXR for deep files
            return open(name, 1, &userspec);
        m_nsubimages = 1;
        m_subimage   = 0;
        m_nmiplevels = 1;
        m_miplevel   = 0;
        m_headers.resize(1);
        copy_and_check_spec(userspec, m_spec);
        sanity_check_channelnames();
        const ParamValue* param = m_spec.find_attribute("oiio:ioproxy",
                                                        TypeDesc::PTR);
        if (param)
            m_io = param->get<Filesystem::IOProxy*>();

        if (!spec_to_header(m_spec, m_subimage, m_headers[m_subimage]))
            return false;

        try {
            if (!m_io) {
                m_io = new Filesystem::IOFile(name, Filesystem::IOProxy::Write);
                m_local_io.reset(m_io);
            }
            if (m_io->mode() != Filesystem::IOProxy::Write) {
                // If the proxy couldn't be opened in write mode, try to
                // return an error.
                std::string e = m_io->error();
                errorfmt("Could not open \"{}\" ({})", name,
                         e.size() ? e : std::string("unknown error"));
                return false;
            }
            m_output_stream.reset(new OpenEXROutputStream(name.c_str(), m_io));
            if (m_spec.tile_width) {
                m_output_tiled.reset(
                    new Imf::TiledOutputFile(*m_output_stream,
                                             m_headers[m_subimage]));
            } else {
                m_output_scanline.reset(
                    new Imf::OutputFile(*m_output_stream,
                                        m_headers[m_subimage]));
            }
        } catch (const std::exception& e) {
            errorfmt("Could not open \"{}\" ({})", name, e.what());
            m_output_scanline = NULL;
            m_output_tiled    = NULL;
            return false;
        } catch (...) {  // catch-all for edge cases or compiler bugs
            errorfmt("Could not open \"{}\" (unknown exception)", name);
            m_output_scanline = NULL;
            m_output_tiled    = NULL;
            return false;
        }
        if (!m_output_scanline && !m_output_tiled) {
            errorfmt("Unknown error opening EXR file");
            return false;
        }

        return true;
    }

    if (mode == AppendSubimage) {
        // OpenEXR 2.x supports subimages, but we only allow it to use the
        // open(name,subimages,specs[]) variety.
        if (m_subimagespecs.size() == 0 || !m_output_multipart) {
            errorfmt("{} not opened properly for subimages", format_name());
            return false;
        }
        // Move on to next subimage
        ++m_subimage;
        if (m_subimage >= m_nsubimages) {
            errorfmt("More subimages than originally declared.");
            return false;
        }
        // Close the current subimage, open the next one
        try {
            if (m_tiled_output_part) {
                m_tiled_output_part.reset(
                    new Imf::TiledOutputPart(*m_output_multipart, m_subimage));
            } else if (m_scanline_output_part) {
                m_scanline_output_part.reset(
                    new Imf::OutputPart(*m_output_multipart, m_subimage));
            } else if (m_deep_tiled_output_part) {
                m_deep_tiled_output_part.reset(
                    new Imf::DeepTiledOutputPart(*m_output_multipart,
                                                 m_subimage));
            } else if (m_deep_scanline_output_part) {
                m_deep_scanline_output_part.reset(
                    new Imf::DeepScanLineOutputPart(*m_output_multipart,
                                                    m_subimage));
            } else {
                errorfmt(
                    "Called open with AppendSubimage mode, but no appropriate part is found. Application bug?");
                return false;
            }
        } catch (const std::exception& e) {
            errorfmt("OpenEXR exception: {}", e.what());
            m_scanline_output_part.reset();
            m_tiled_output_part.reset();
            m_deep_scanline_output_part.reset();
            m_deep_tiled_output_part.reset();
            return false;
        } catch (...) {  // catch-all for edge cases or compiler bugs
            errorfmt("OpenEXR exception: unknown exception");
            m_scanline_output_part.reset();
            m_tiled_output_part.reset();
            m_deep_scanline_output_part.reset();
            m_deep_tiled_output_part.reset();
            return false;
        }
        m_spec = m_subimagespecs[m_subimage];
        sanity_check_channelnames();
        compute_pixeltypes(m_spec);
        return true;
    }

    if (mode == AppendMIPLevel) {
        if (!m_output_scanline && !m_output_tiled) {
            errorfmt("Cannot append a MIP level if no file has been opened");
            return false;
        }
        if (m_spec.tile_width && m_levelmode != Imf::ONE_LEVEL) {
            // OpenEXR does not support differing tile sizes on different
            // MIP-map levels.  Reject the open() if not using the original
            // tile sizes.
            if (userspec.tile_width != m_spec.tile_width
                || userspec.tile_height != m_spec.tile_height) {
                errorfmt(
                    "OpenEXR tiles must have the same size on all MIPmap levels");
                return false;
            }
            // Copy the new mip level size.  Keep everything else from the
            // original level.
            m_spec.width  = userspec.width;
            m_spec.height = userspec.height;
            // N.B. do we need to copy anything else from userspec?
            ++m_miplevel;
            return true;
        } else {
            errorfmt("Cannot add MIP level to a non-MIPmapped file");
            return false;
        }
    }

    errorfmt("Unknown open mode {}", int(mode));
    return false;
}



bool
OpenEXROutput::open(const std::string& name, int subimages,
                    const ImageSpec* specs)
{
    if (subimages < 1) {
        errorfmt("OpenEXR does not support {} subimages.", subimages);
        return false;
    }

    // Only one part and not deep?  Write an OpenEXR 1.x file
    if (subimages == 1 && !specs[0].deep)
        return open(name, specs[0], Create);

    // Copy the passed-in subimages and turn into OpenEXR headers
    m_nsubimages = subimages;
    m_subimage   = 0;
    m_nmiplevels = 1;
    m_miplevel   = 0;
    m_subimagespecs.resize(subimages);
    for (int i = 0; i < subimages; ++i)
        if (!copy_and_check_spec(specs[i], m_subimagespecs[i]))
            return false;

    m_headers.resize(subimages);
    std::string filetype;
    if (specs[0].deep)
        filetype = specs[0].tile_width ? "tiledimage" : "deepscanlineimage";
    else
        filetype = specs[0].tile_width ? "tiledimage" : "scanlineimage";
    bool deep = false;
    for (int s = 0; s < subimages; ++s) {
        if (!spec_to_header(m_subimagespecs[s], s, m_headers[s]))
            return false;
        deep |= m_subimagespecs[s].deep;
        if (m_subimagespecs[s].deep != m_subimagespecs[0].deep) {
            errorfmt(
                "OpenEXR does not support mixed deep/nondeep multi-part image files");
            return false;
        }
        if (subimages > 1 || deep) {
            bool tiled = m_subimagespecs[s].tile_width;
            m_headers[s].setType(
                deep ? (tiled ? Imf::DEEPTILE : Imf::DEEPSCANLINE)
                     : (tiled ? Imf::TILEDIMAGE : Imf::SCANLINEIMAGE));
        }
    }

    m_spec = m_subimagespecs[0];
    sanity_check_channelnames();
    compute_pixeltypes(m_spec);

    // Create an ImfMultiPartOutputFile
    try {
        if (!m_io) {
            m_io = new Filesystem::IOFile(name, Filesystem::IOProxy::Write);
            m_local_io.reset(m_io);
        }
        if (m_io->mode() != Filesystem::IOProxy::Write) {
            // If the proxy couldn't be opened in write mode, try to
            // return an error.
            std::string e = m_io->error();
            errorfmt("Could not open \"{}\" ({})", name,
                     e.size() ? e : std::string("unknown error"));
            return false;
        }
        m_output_stream.reset(new OpenEXROutputStream(name.c_str(), m_io));
        m_output_multipart.reset(new Imf::MultiPartOutputFile(*m_output_stream,
                                                              &m_headers[0],
                                                              subimages));
    } catch (const std::exception& e) {
        m_output_stream.reset();
        errorfmt("OpenEXR exception: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        m_output_stream.reset();
        errorfmt("OpenEXR exception: unknown exception");
        return false;
    }
    try {
        if (deep) {
            if (m_spec.tile_width) {
                m_deep_tiled_output_part.reset(
                    new Imf::DeepTiledOutputPart(*m_output_multipart, 0));
            } else {
                m_deep_scanline_output_part.reset(
                    new Imf::DeepScanLineOutputPart(*m_output_multipart, 0));
            }
        } else {
            if (m_spec.tile_width) {
                m_tiled_output_part.reset(
                    new Imf::TiledOutputPart(*m_output_multipart, 0));
            } else {
                m_scanline_output_part.reset(
                    new Imf::OutputPart(*m_output_multipart, 0));
            }
        }
    } catch (const std::exception& e) {
        errorfmt("OpenEXR exception: {}", e.what());
        m_output_stream.reset();
        m_scanline_output_part.reset();
        m_tiled_output_part.reset();
        m_deep_scanline_output_part.reset();
        m_deep_tiled_output_part.reset();
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("OpenEXR exception: unknown exception");
        m_output_stream.reset();
        m_scanline_output_part.reset();
        m_tiled_output_part.reset();
        m_deep_scanline_output_part.reset();
        m_deep_tiled_output_part.reset();
        return false;
    }

    return true;
}



Imf::PixelType
OpenEXROutput::imfpixeltype(TypeDesc type)
{
    Imf::PixelType ptype;
    switch (type.basetype) {
    case TypeDesc::UINT: ptype = Imf::UINT; break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE: ptype = Imf::FLOAT; break;
    default:
        // Everything else defaults to half
        ptype = Imf::HALF;
        break;
    }
    return ptype;
}



void
OpenEXROutput::compute_pixeltypes(const ImageSpec& spec)
{
    m_pixeltype.clear();
    m_pixeltype.reserve(spec.nchannels);
    for (int c = 0; c < spec.nchannels; ++c) {
        m_pixeltype.push_back(imfpixeltype(spec.channelformat(c)));
    }
    OIIO_ASSERT(m_pixeltype.size() == size_t(spec.nchannels));
}



bool
OpenEXROutput::spec_to_header(ImageSpec& spec, int subimage,
                              Imf::Header& header)
{
    // Force use of one of the three data types that OpenEXR supports
    switch (spec.format.basetype) {
    case TypeDesc::UINT: spec.format = TypeDesc::UINT; break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE: spec.format = TypeDesc::FLOAT; break;
    default:
        // Everything else defaults to half
        spec.format = TypeDesc::HALF;
    }

    Imath::Box2i dataWindow(Imath::V2i(spec.x, spec.y),
                            Imath::V2i(spec.width + spec.x - 1,
                                       spec.height + spec.y - 1));
    Imath::Box2i displayWindow(Imath::V2i(spec.full_x, spec.full_y),
                               Imath::V2i(spec.full_width + spec.full_x - 1,
                                          spec.full_height + spec.full_y - 1));
    header = Imf::Header(displayWindow, dataWindow);

    // Insert channels into the header.  Also give the channels names if
    // the user botched it.
    compute_pixeltypes(spec);
    static const char* default_chan_names[] = { "R", "G", "B", "A" };
    spec.channelnames.resize(spec.nchannels);
    for (int c = 0; c < spec.nchannels; ++c) {
        if (spec.channelnames[c].empty())
            spec.channelnames[c] = (c < 4)
                                       ? default_chan_names[c]
                                       : Strutil::fmt::format("unknown {}", c);
        // Hint to lossy compression methods that indicates whether
        // human perception of the quantity represented by this channel
        // is closer to linear or closer to logarithmic.  Compression
        // methods may optimize image quality by adjusting pixel data
        // quantization according to this hint.
        // Note: This is not the same as data having come from a linear
        // colorspace.  It is meant for data that is perceived by humans
        // in a linear fashion.
        // e.g Cb & Cr components in YCbCr images
        //     a* & b* components in L*a*b* images
        //     H & S components in HLS images
        // We ignore this for now, but we should fix it if we ever commonly
        // work with non-perceptual/non-color image data.
        bool pLinear = false;
        header.channels().insert(spec.channelnames[c].c_str(),
                                 Imf::Channel(m_pixeltype[c], 1, 1, pLinear));
    }

    string_view comp;
    int qual;
    std::tie(comp, qual) = spec.decode_compression_metadata("zip", -1);
    // It seems that zips is the only compression that can reliably work
    // on deep files (but allow "none" as well)
    if (spec.deep && comp != "none")
        comp = "zips";
    // For single channel tiled images, dwaa/b compression only seems to work
    // reliably when tile size > 16 and size is a power of two.
    if (spec.nchannels == 1 && spec.tile_width > 0
        && Strutil::istarts_with(comp, "dwa")
        && ((spec.tile_width < 16 && spec.tile_height < 16)
            || !ispow2(spec.tile_width) || !ispow2(spec.tile_height))) {
        comp = "zip";
    }
    spec.attribute("compression", comp);

    // Zip and DWA compression have additional ways to set the levels
#if OPENEXR_CODED_VERSION >= 30103
    // OpenEXR 3.1.3 and later allow us to pick the quality level. We've found
    // that 4 is a great tradeoff between size and speed, so that is our
    // default.
    if (Strutil::istarts_with(comp, "zip")) {
        header.zipCompressionLevel() = (qual >= 1 && qual <= 9) ? qual : 4;
    }
#endif
    if (Strutil::istarts_with(comp, "dwa") && qual > 0) {
#if OPENEXR_CODED_VERSION >= 30103
        // OpenEXR 3.1.3 and later have an API for setting the quality level
        // in the Header object. Older ones do it by setting an attribute, as
        // below.
        header.dwaCompressionLevel() = float(qual);
#endif
        // We set this attribute even for older openexr, because even if we
        // set in the header (above), it gets saved as metadata in the file so
        // that when we re-read it, we know what the compression level was.
        spec.attribute("openexr:dwaCompressionLevel", float(qual));
    } else {
        // If we're not compressing via dwaa/dwab, clear this attrib so we
        // aren't incorrectly carrying it around.
        spec.erase_attribute("openexr:dwaCompressionLevel");
    }

    // Default to increasingY line order
    if (!spec.find_attribute("openexr:lineOrder"))
        spec.attribute("openexr:lineOrder", "increasingY");

    // Automatically set date field if the client didn't supply it.
    if (!spec.find_attribute("DateTime")) {
        time_t now;
        time(&now);
        struct tm mytm;
        Sysutil::get_local_time(&now, &mytm);
        std::string date
            = Strutil::fmt::format("{:4d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                                   mytm.tm_year + 1900, mytm.tm_mon + 1,
                                   mytm.tm_mday, mytm.tm_hour, mytm.tm_min,
                                   mytm.tm_sec);
        spec.attribute("DateTime", date);
    }

    figure_mip(spec, m_nmiplevels, m_levelmode, m_roundingmode);

    std::string textureformat = spec.get_string_attribute("textureformat", "");
    if (Strutil::iequals(textureformat, "CubeFace Environment")) {
        header.insert("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_CUBE));
    } else if (Strutil::iequals(textureformat, "LatLong Environment")) {
        header.insert("envmap", Imf::EnvmapAttribute(Imf::ENVMAP_LATLONG));
    }

    // Fix up density and aspect to be consistent
    float aspect   = spec.get_float_attribute("PixelAspectRatio", 0.0f);
    float xdensity = spec.get_float_attribute("XResolution", 0.0f);
    float ydensity = spec.get_float_attribute("YResolution", 0.0f);
    if (!aspect && xdensity && ydensity) {
        // No aspect ratio. Compute it from density, if supplied.
        spec.attribute("PixelAspectRatio", xdensity / ydensity);
    }
    if (xdensity && ydensity
        && spec.get_string_attribute("ResolutionUnit") == "cm") {
        // OpenEXR only supports pixels per inch, so fix the values if they
        // came to us in cm.
        spec.attribute("XResolution", xdensity / 2.54f);
        spec.attribute("YResolution", ydensity / 2.54f);
    }

    // We must setTileDescription here before the put_parameter calls below,
    // since put_parameter will check the header to ensure this is a tiled
    // image before setting lineOrder to randomY.
    if (spec.tile_width)
        header.setTileDescription(
            Imf::TileDescription(spec.tile_width, spec.tile_height,
                                 Imf::LevelMode(m_levelmode),
                                 Imf::LevelRoundingMode(m_roundingmode)));

    // Deal with all other params
    for (const auto& p : spec.extra_attribs)
        put_parameter(p.name().string(), p.type(), p.data(), header);

    // Multi-part EXR files required to have a name. Make one up if not
    // supplied.
    if (m_nsubimages > 1 && !header.hasName()) {
        std::string n = Strutil::fmt::format("subimage{:02d}", subimage);
        header.insert("name", Imf::StringAttribute(n));
    }

    return true;
}



void
OpenEXROutput::figure_mip(const ImageSpec& spec, int& nmiplevels,
                          int& levelmode, int& roundingmode)
{
    nmiplevels   = 1;
    levelmode    = Imf::ONE_LEVEL;  // Default to no MIP-mapping
    roundingmode = spec.get_int_attribute("openexr:roundingmode",
                                          Imf::ROUND_DOWN);

    std::string textureformat = spec.get_string_attribute("textureformat", "");
    if (Strutil::iequals(textureformat, "Plain Texture")) {
        levelmode = spec.get_int_attribute("openexr:levelmode",
                                           Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals(textureformat, "CubeFace Environment")) {
        levelmode = spec.get_int_attribute("openexr:levelmode",
                                           Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals(textureformat, "LatLong Environment")) {
        levelmode = spec.get_int_attribute("openexr:levelmode",
                                           Imf::MIPMAP_LEVELS);
    } else if (Strutil::iequals(textureformat, "Shadow")) {
        levelmode = Imf::ONE_LEVEL;  // Force one level for shadow maps
    }

    if (levelmode == Imf::MIPMAP_LEVELS) {
        // Compute how many mip levels there will be
        int w = spec.width;
        int h = spec.height;
        while (w > 1 && h > 1) {
            if (roundingmode == Imf::ROUND_DOWN) {
                w = w / 2;
                h = h / 2;
            } else {
                w = (w + 1) / 2;
                h = (h + 1) / 2;
            }
            w = std::max(1, w);
            h = std::max(1, h);
            ++nmiplevels;
        }
    }
}



struct ExrMeta {
    const char *oiioname, *exrname;
    TypeDesc exrtype;

    ExrMeta(const char* oiioname = NULL, const char* exrname = NULL,
            TypeDesc exrtype = TypeDesc::UNKNOWN)
        : oiioname(oiioname)
        , exrname(exrname)
        , exrtype(exrtype)
    {
    }
};

static ExrMeta exr_meta_translation[] = {
    // Translate OIIO standard metadata names to OpenEXR standard names
    ExrMeta("worldtocamera", "worldToCamera", TypeMatrix),
    ExrMeta("worldtoNDC", "worldToNDC", TypeMatrix),
    ExrMeta("worldtoscreen", "worldToScreen", TypeMatrix),
    ExrMeta("DateTime", "capDate", TypeString),
    ExrMeta("ImageDescription", "comments", TypeString),
    ExrMeta("description", "comments", TypeString),
    ExrMeta("Copyright", "owner", TypeString),
    ExrMeta("PixelAspectRatio", "pixelAspectRatio", TypeFloat),
    ExrMeta("XResolution", "xDensity", TypeFloat),
    ExrMeta("ExposureTime", "expTime", TypeFloat),
    ExrMeta("FNumber", "aperture", TypeFloat),
    ExrMeta("oiio:subimagename", "name", TypeString),
    ExrMeta("openexr:dwaCompressionLevel", "dwaCompressionLevel", TypeFloat),
    ExrMeta("smpte:TimeCode", "timeCode", TypeTimeCode),
    ExrMeta("smpte:KeyCode", "keyCode", TypeKeyCode),
    // Empty exrname means that we silently drop this metadata.
    // Often this is because they have particular meaning to OpenEXR and we
    // don't want to mess it up by inadvertently copying it wrong from the
    // user or from a file we read.
    ExrMeta("YResolution"), ExrMeta("planarconfig"), ExrMeta("type"),
    ExrMeta("tiles"), ExrMeta("chunkCount"), ExrMeta("maxSamplesPerPixel"),
    ExrMeta("openexr:roundingmode")
};



bool
OpenEXROutput::put_parameter(const std::string& name, TypeDesc type,
                             const void* data, Imf::Header& header)
{
    // Translate
    if (name.empty())
        return false;
    if (!data)
        return false;
    std::string xname = name;
    TypeDesc exrtype  = TypeUnknown;

    for (const auto& e : exr_meta_translation) {
        if (Strutil::iequals(xname, e.oiioname)
            || (e.exrname && Strutil::iequals(xname, e.exrname))) {
            xname   = std::string(e.exrname ? e.exrname : "");
            exrtype = e.exrtype;
            // std::cerr << "exr put '" << name << "' -> '" << xname << "'\n";
            break;
        }
    }

    // Special cases
    if (Strutil::iequals(xname, "Compression") && type == TypeString) {
        const char* str      = *(char**)data;
        header.compression() = Imf::ZIP_COMPRESSION;  // Default
        if (str) {
            if (Strutil::iequals(str, "none"))
                header.compression() = Imf::NO_COMPRESSION;
            else if (Strutil::iequals(str, "deflate")
                     || Strutil::iequals(str, "zip"))
                header.compression() = Imf::ZIP_COMPRESSION;
            else if (Strutil::iequals(str, "rle"))
                header.compression() = Imf::RLE_COMPRESSION;
            else if (Strutil::iequals(str, "zips"))
                header.compression() = Imf::ZIPS_COMPRESSION;
            else if (Strutil::iequals(str, "piz"))
                header.compression() = Imf::PIZ_COMPRESSION;
            else if (Strutil::iequals(str, "pxr24"))
                header.compression() = Imf::PXR24_COMPRESSION;
            else if (Strutil::iequals(str, "b44"))
                header.compression() = Imf::B44_COMPRESSION;
            else if (Strutil::iequals(str, "b44a"))
                header.compression() = Imf::B44A_COMPRESSION;
            else if (Strutil::iequals(str, "dwaa"))
                header.compression() = Imf::DWAA_COMPRESSION;
            else if (Strutil::iequals(str, "dwab"))
                header.compression() = Imf::DWAB_COMPRESSION;
        }
        return true;
    }

    if (Strutil::iequals(xname, "openexr:lineOrder") && type == TypeString) {
        const char* str    = *(char**)data;
        header.lineOrder() = Imf::INCREASING_Y;  // Default
        if (str) {
            if (Strutil::iequals(str, "randomY")
                && header
                       .hasTileDescription() /* randomY is only for tiled files */)
                header.lineOrder() = Imf::RANDOM_Y;
            else if (Strutil::iequals(str, "decreasingY"))
                header.lineOrder() = Imf::DECREASING_Y;
        }
        return true;
    }

    // Special handling of any remaining "oiio:*" metadata.
    if (Strutil::istarts_with(xname, "oiio:")) {
        if (Strutil::iequals(xname, "oiio:ConstantColor")
            || Strutil::iequals(xname, "oiio:AverageColor")
            || Strutil::iequals(xname, "oiio:SHA-1")) {
            // let these fall through and get stored as metadata
        } else {
            // Other than the listed exceptions, suppress any other custom
            // oiio: directives.
            return false;
        }
    }

    // Before handling general named metadata, suppress format-specific
    // metadata meant for other formats.
    if (const char* colon = strchr(xname.c_str(), ':')) {
        std::string prefix(xname.c_str(), colon);
        Strutil::to_lower(prefix);
        if (prefix != format_name() && is_imageio_format_name(prefix))
            return false;
    }

    // The main "ICCProfile" byte array should translate, but the individual
    // "ICCProfile:*" attributes are suppressed because they merely duplicate
    // what's in the byte array.
    if (Strutil::istarts_with(xname, "ICCProfile:")) {
        return false;
    }

    if (!xname.length())
        return false;  // Skip suppressed names

    // Handle some cases where the user passed a type different than what
    // OpenEXR expects, and we can make a good guess about how to translate.
    float tmpfloat;
    int tmpint;
    if (exrtype == TypeFloat && type == TypeInt) {
        tmpfloat = float(*(const int*)data);
        data     = &tmpfloat;
        type     = TypeFloat;
    } else if (exrtype == TypeInt && type == TypeFloat) {
        tmpfloat = int(*(const float*)data);
        data     = &tmpint;
        type     = TypeInt;
    } else if (exrtype == TypeMatrix && type == TypeDesc(TypeDesc::FLOAT, 16)) {
        // Automatically translate float[16] to Matrix when expected
        type = TypeMatrix;
    }

    // Now if we still don't match a specific type OpenEXR is looking for,
    // skip it.
    if (exrtype != TypeDesc() && !exrtype.equivalent(type)) {
        OIIO::debugfmt(
            "OpenEXR output metadata \"{}\" type mismatch: expected {}, got {}\n",
            name, exrtype, type);
        return false;
    }

    // General handling of attributes
    try {
        // Scalar
        if (type.arraylen == 0) {
            if (type.aggregate == TypeDesc::SCALAR) {
                if (type == TypeDesc::INT || type == TypeDesc::UINT) {
                    header.insert(xname.c_str(),
                                  Imf::IntAttribute(*(int*)data));
                    return true;
                }
                if (type == TypeDesc::INT16) {
                    header.insert(xname.c_str(),
                                  Imf::IntAttribute(*(short*)data));
                    return true;
                }
                if (type == TypeDesc::UINT16) {
                    header.insert(xname.c_str(),
                                  Imf::IntAttribute(*(unsigned short*)data));
                    return true;
                }
                if (type == TypeDesc::FLOAT) {
                    header.insert(xname.c_str(),
                                  Imf::FloatAttribute(*(float*)data));
                    return true;
                }
                if (type == TypeDesc::HALF) {
                    header.insert(xname.c_str(),
                                  Imf::FloatAttribute((float)*(half*)data));
                    return true;
                }
                if (type == TypeString && *(const char**)data) {
                    header.insert(xname.c_str(),
                                  Imf::StringAttribute(
                                      *(const char**)data));  //NOSONAR
                    return true;
                }
                if (type == TypeDesc::DOUBLE) {
                    header.insert(xname.c_str(),
                                  Imf::DoubleAttribute(*(double*)data));
                    return true;
                }
            }
            // Single instance of aggregate type
            if (type.aggregate == TypeDesc::VEC2) {
                switch (type.basetype) {
                case TypeDesc::UINT:
                case TypeDesc::INT:
                    // TODO could probably handle U/INT16 here too
                    if (type.vecsemantics == TypeDesc::RATIONAL) {
                        // It's a floor wax AND a dessert topping
                        const int* intArray = reinterpret_cast<const int*>(
                            data);
                        const unsigned int* uIntArray
                            = reinterpret_cast<const unsigned int*>(data);
                        header.insert(xname.c_str(),
                                      Imf::RationalAttribute(
                                          Imf::Rational(intArray[0],
                                                        uIntArray[1])));
                        return true;
                    }
                    header.insert(xname.c_str(),
                                  Imf::V2iAttribute(*(Imath::V2i*)data));
                    return true;
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::V2fAttribute(*(Imath::V2f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::V2dAttribute(*(Imath::V2d*)data));
                    return true;
                case TypeDesc::STRING:
                    Imf::StringVector v;
                    v.emplace_back(((const char**)data)[0]);
                    v.emplace_back(((const char**)data)[1]);
                    header.insert(xname.c_str(), Imf::StringVectorAttribute(v));
                    return true;
                }
            }
            if (type.aggregate == TypeDesc::VEC3) {
                switch (type.basetype) {
                case TypeDesc::UINT:
                case TypeDesc::INT:
                    // TODO could probably handle U/INT16 here too
                    header.insert(xname.c_str(),
                                  Imf::V3iAttribute(*(Imath::V3i*)data));
                    return true;
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::V3fAttribute(*(Imath::V3f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::V3dAttribute(*(Imath::V3d*)data));
                    return true;
                case TypeDesc::STRING:
                    Imf::StringVector v;
                    v.emplace_back(((const char**)data)[0]);
                    v.emplace_back(((const char**)data)[1]);
                    v.emplace_back(((const char**)data)[2]);
                    header.insert(xname.c_str(), Imf::StringVectorAttribute(v));
                    return true;
                }
            }
            if (type.aggregate == TypeDesc::MATRIX33) {
                switch (type.basetype) {
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::M33fAttribute(*(Imath::M33f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::M33dAttribute(*(Imath::M33d*)data));
                    return true;
                }
            }
            if (type.aggregate == TypeDesc::MATRIX44) {
                switch (type.basetype) {
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::M44fAttribute(*(Imath::M44f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::M44dAttribute(*(Imath::M44d*)data));
                    return true;
                }
            }
        }
        // Unknown length arrays (Don't know how to handle these yet)
        else if (type.arraylen < 0) {
            return false;
        }
        // Arrays
        else {
            // TimeCode
            if (type == TypeTimeCode) {
                header.insert(xname.c_str(),
                              Imf::TimeCodeAttribute(*(Imf::TimeCode*)data));
                return true;
            }
            // KeyCode
            else if (type == TypeKeyCode) {
                header.insert(xname.c_str(),
                              Imf::KeyCodeAttribute(*(Imf::KeyCode*)data));
                return true;
            }

            // 2 Vec2's are treated as a Box
            if (type.arraylen == 2 && type.aggregate == TypeDesc::VEC2) {
                switch (type.basetype) {
                case TypeDesc::UINT:
                case TypeDesc::INT: {
                    int* a = (int*)data;
                    header.insert(xname.c_str(),
                                  Imf::Box2iAttribute(
                                      Imath::Box2i(Imath::V2i(a[0], a[1]),
                                                   Imath::V2i(a[2], a[3]))));
                    return true;
                }
                case TypeDesc::FLOAT: {
                    float* a = (float*)data;
                    header.insert(xname.c_str(),
                                  Imf::Box2fAttribute(
                                      Imath::Box2f(Imath::V2f(a[0], a[1]),
                                                   Imath::V2f(a[2], a[3]))));
                    return true;
                }
                }
            }
            // Vec 2
            if (type.arraylen == 2 && type.aggregate == TypeDesc::SCALAR) {
                switch (type.basetype) {
                case TypeDesc::UINT:
                case TypeDesc::INT:
                    // TODO could probably handle U/INT16 here too
                    header.insert(xname.c_str(),
                                  Imf::V2iAttribute(*(Imath::V2i*)data));
                    return true;
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::V2fAttribute(*(Imath::V2f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::V2dAttribute(*(Imath::V2d*)data));
                    return true;
                }
            }
            // Vec3
            if (type.arraylen == 3 && type.aggregate == TypeDesc::SCALAR) {
                switch (type.basetype) {
                case TypeDesc::UINT:
                case TypeDesc::INT:
                    // TODO could probably handle U/INT16 here too
                    header.insert(xname.c_str(),
                                  Imf::V3iAttribute(*(Imath::V3i*)data));
                    return true;
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::V3fAttribute(*(Imath::V3f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::V3dAttribute(*(Imath::V3d*)data));
                    return true;
                }
            }
            // Matrix
            if (type.arraylen == 9 && type.aggregate == TypeDesc::SCALAR) {
                switch (type.basetype) {
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::M33fAttribute(*(Imath::M33f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::M33dAttribute(*(Imath::M33d*)data));
                    return true;
                }
            }
            if (type.arraylen == 16 && type.aggregate == TypeDesc::SCALAR) {
                switch (type.basetype) {
                case TypeDesc::FLOAT:
                    header.insert(xname.c_str(),
                                  Imf::M44fAttribute(*(Imath::M44f*)data));
                    return true;
                case TypeDesc::DOUBLE:
                    header.insert(xname.c_str(),
                                  Imf::M44dAttribute(*(Imath::M44d*)data));
                    return true;
                }
            }
            if (type.basetype == TypeDesc::FLOAT
                && type.aggregate * type.arraylen == 8
                && Strutil::iequals(xname, "chromaticities")) {
                const float* f = (const float*)data;
                Imf::Chromaticities c(Imath::V2f(f[0], f[1]),
                                      Imath::V2f(f[2], f[3]),
                                      Imath::V2f(f[4], f[5]),
                                      Imath::V2f(f[6], f[7]));
                header.insert("chromaticities",
                              Imf::ChromaticitiesAttribute(c));
                return true;
            }
            // String Vector
            if (type.basetype == TypeDesc::STRING) {
                Imf::StringVector v((const char**)data,
                                    (const char**)data + type.basevalues());
                header.insert(xname.c_str(), Imf::StringVectorAttribute(v));
                return true;
            }
            // float Vector
            if (type.basetype == TypeDesc::FLOAT) {
                Imf::FloatVector v((const float*)data,
                                   (const float*)data + type.basevalues());
                header.insert(xname.c_str(), Imf::FloatVectorAttribute(v));
                return true;
            }
        }
    } catch (const std::exception& e) {
        OIIO::debugfmt("Caught OpenEXR exception: {}\n", e.what());
    } catch (...) {  // catch-all for edge cases or compiler bugs
        OIIO::debug("Caught unknown OpenEXR exception\n");
    }

    OIIO::debugfmt("Don't know what to do with {} {}\n", type, xname);
    return false;
}



void
OpenEXROutput::sanity_check_channelnames()
{
    m_spec.channelnames.resize(m_spec.nchannels, "");
    for (int c = 1; c < m_spec.nchannels; ++c) {
        for (int i = 0; i < c; ++i) {
            if (m_spec.channelnames[c].empty()
                || m_spec.channelnames[c] == m_spec.channelnames[i]) {
                // Duplicate or missing channel name! We don't want
                // libIlmImf to drop the channel (as it will do for
                // duplicates), so rename it and hope for the best.
                m_spec.channelnames[c] = Strutil::fmt::format("channel{}", c);
                break;
            }
        }
    }
}



bool
OpenEXROutput::close()
{
    // FIXME: if the use pattern for mipmaps is open(), open(append),
    // ... close(), then we don't have to leave the file open with this
    // trickery.  That's only necessary if it's open(), close(),
    // open(append), close(), ...

    if (m_levelmode != Imf::ONE_LEVEL) {
        // Leave MIP-map files open, since appending cannot be done via
        // a re-open like it can with TIFF files.
        return true;
    }

    m_output_scanline.reset();
    m_output_tiled.reset();
    m_scanline_output_part.reset();
    m_tiled_output_part.reset();
    m_output_multipart.reset();
    m_output_stream.reset();

    init();       // re-initialize
    return true;  // How can we fail?
}



bool
OpenEXROutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                              stride_t xstride)
{
#if 1
    return write_scanlines(y, y + 1, z, format, data, xstride, AutoStride);
#else
    if (!(m_output_scanline || m_scanline_output_part)) {
        errorfmt("called OpenEXROutput::write_scanline without an open file");
        return false;
    }

    bool native        = (format == TypeDesc::UNKNOWN);
    size_t pixel_bytes = m_spec.pixel_bytes(true);  // native
    if (native && xstride == AutoStride)
        xstride = (stride_t)pixel_bytes;
    m_spec.auto_stride(xstride, format, spec().nchannels);
    data = to_native_scanline(format, data, xstride, m_scratch);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where client stored
    // the bytes to be written, but OpenEXR's frameBuffer.insert() wants
    // where the address of the "virtual framebuffer" for the whole
    // image.
    imagesize_t scanlinebytes = m_spec.scanline_bytes(native);
    char* buf = (char*)data - m_spec.x * pixel_bytes - y * scanlinebytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0; c < m_spec.nchannels; ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert(m_spec.channelnames[c].c_str(),
                               Imf::Slice(m_pixeltype[c], buf + chanoffset,
                                          pixel_bytes, scanlinebytes));
            chanoffset += chanbytes;
        }
        if (m_output_scanline) {
            m_output_scanline->setFrameBuffer(frameBuffer);
            m_output_scanline->writePixels(1);
        } else if (m_scanline_output_part) {
            m_scanline_output_part->setFrameBuffer(frameBuffer);
            m_scanline_output_part->writePixels(1);
        } else {
            errorfmt("Attempt to write scanline to a non-scanline file.");
            return false;
        }
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR write: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR write: unknown exception");
        return false;
    }

    // FIXME -- can we checkpoint the file?

    return true;
#endif
}



bool
OpenEXROutput::copy_image(ImageInput* in)
{
    if (in && !strcmp(in->format_name(), "openexr")) {
        if (OpenEXRInput* exr_in = dynamic_cast<OpenEXRInput*>(in)) {
            // Copy over pixels without decompression.
            try {
                if (m_output_scanline && exr_in->m_scanline_input_part) {
                    m_output_scanline->copyPixels(
                        *exr_in->m_scanline_input_part);
                    return true;
                } else if (m_output_tiled && exr_in->m_tiled_input_part
                           && m_nmiplevels == 0) {
                    m_output_tiled->copyPixels(*exr_in->m_tiled_input_part);
                    return true;
                } else if (m_scanline_output_part
                           && exr_in->m_scanline_input_part) {
                    m_scanline_output_part->copyPixels(
                        *exr_in->m_scanline_input_part);
                    return true;
                } else if (m_tiled_output_part && exr_in->m_tiled_input_part
                           && m_nmiplevels == 0) {
                    m_tiled_output_part->copyPixels(
                        *exr_in->m_tiled_input_part);
                    return true;
                } else if (m_deep_scanline_output_part
                           && exr_in->m_deep_scanline_input_part) {
                    m_deep_scanline_output_part->copyPixels(
                        *exr_in->m_deep_scanline_input_part);
                    return true;
                } else if (m_deep_tiled_output_part
                           && exr_in->m_deep_tiled_input_part
                           && m_nmiplevels == 0) {
                    m_deep_tiled_output_part->copyPixels(
                        *exr_in->m_deep_tiled_input_part);
                    return true;
                }
            } catch (const std::exception& e) {
                errorfmt(
                    "Failed OpenEXR copy: {}, falling back to the default image copy routine.",
                    e.what());
                return false;
            } catch (...) {  // catch-all for edge cases or compiler bugs
                errorfmt(
                    "Failed OpenEXR copy: unknown exception, falling back to the default image copy routine.");
                return false;
            }
        }
    }
    return ImageOutput::copy_image(in);
}



bool
OpenEXROutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                               const void* data, stride_t xstride,
                               stride_t ystride)
{
    if (!(m_output_scanline || m_scanline_output_part)) {
        errorfmt("called OpenEXROutput::write_scanlines without an open file");
        return false;
    }

    yend                      = std::min(yend, spec().y + spec().height);
    bool native               = (format == TypeDesc::UNKNOWN);
    imagesize_t scanlinebytes = spec().scanline_bytes(true);
    size_t pixel_bytes        = m_spec.pixel_bytes(true);
    if (native && xstride == AutoStride)
        xstride = (stride_t)pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       m_spec.width, m_spec.height);

    const imagesize_t limit = 16 * 1024
                              * 1024;  // Allocate 16 MB, or 1 scanline
    int chunk = std::max(1, int(limit / scanlinebytes));

    bool ok                  = true;
    const bool isDecreasingY = m_spec.get_string_attribute("openexr:lineOrder")
                               == "decreasingY";
    const int nAvailableScanLines = yend - ybegin;
    const int numChunks           = nAvailableScanLines > 0
                                        ? 1 + ((nAvailableScanLines - 1) / chunk)
                                        : 0;
    const int yLoopStart = isDecreasingY ? ybegin + (numChunks - 1) * chunk
                                         : ybegin;
    const int yDelta     = isDecreasingY ? -chunk : chunk;
    const int yLoopEnd   = yLoopStart + numChunks * yDelta;
    for (int y = yLoopStart; ok && y != yLoopEnd; y += yDelta) {
        int y1         = std::min(y + chunk, yend);
        int nscanlines = y1 - y;

        const void* dataStart = (const char*)data + (y - ybegin) * ystride;
        const void* d = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width,
                                            y, y1, z, z + 1, format, dataStart,
                                            xstride, ystride, zstride,
                                            m_scratch);

        // Compute where OpenEXR needs to think the full buffers starts.
        // OpenImageIO requires that 'data' points to where client stored
        // the bytes to be written, but OpenEXR's frameBuffer.insert() wants
        // where the address of the "virtual framebuffer" for the whole
        // image.
        char* buf = (char*)d - m_spec.x * stride_t(pixel_bytes)
                    - y * stride_t(scanlinebytes);
        try {
            Imf::FrameBuffer frameBuffer;
            size_t chanoffset = 0;
            for (int c = 0; c < m_spec.nchannels; ++c) {
                size_t chanbytes = m_spec.channelformat(c).size();
                frameBuffer.insert(m_spec.channelnames[c].c_str(),
                                   Imf::Slice(m_pixeltype[c], buf + chanoffset,
                                              pixel_bytes, scanlinebytes));
                chanoffset += chanbytes;
            }
            if (m_output_scanline) {
                m_output_scanline->setFrameBuffer(frameBuffer);
                m_output_scanline->writePixels(nscanlines);
            } else if (m_scanline_output_part) {
                m_scanline_output_part->setFrameBuffer(frameBuffer);
                m_scanline_output_part->writePixels(nscanlines);
            } else {
                errorfmt("Attempt to write scanlines to a non-scanline file.");
                return false;
            }
        } catch (const std::exception& e) {
            errorfmt("Failed OpenEXR write: {}", e.what());
            return false;
        } catch (...) {  // catch-all for edge cases or compiler bugs
            errorfmt("Failed OpenEXR write: unknown exception");
            return false;
        }
    }

    // If we allocated more than 1M, free the memory.  It's not wasteful,
    // because it means we're writing big chunks at a time, and therefore
    // there will be few allocations and deletions.
    if (m_scratch.size() > 1 * 1024 * 1024) {
        std::vector<unsigned char> dummy;
        std::swap(m_scratch, dummy);
    }
    return true;
}



bool
OpenEXROutput::write_tile(int x, int y, int z, TypeDesc format,
                          const void* data, stride_t xstride, stride_t ystride,
                          stride_t zstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    if (native && xstride == AutoStride)
        xstride = (stride_t)m_spec.pixel_bytes(native);
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       m_spec.tile_width, m_spec.tile_height);
    return write_tiles(
        x, std::min(x + m_spec.tile_width, m_spec.x + m_spec.width), y,
        std::min(y + m_spec.tile_height, m_spec.y + m_spec.height), z,
        std::min(z + m_spec.tile_depth, m_spec.z + m_spec.depth), format, data,
        xstride, ystride, zstride);
}



bool
OpenEXROutput::write_tiles(int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride,
                           stride_t zstride)
{
    //    std::cerr << "exr::write_tiles " << xbegin << ' ' << xend
    //              << ' ' << ybegin << ' ' << yend << "\n";
    if (!(m_output_tiled || m_tiled_output_part)) {
        errorfmt("called OpenEXROutput::write_tiles without an open file");
        return false;
    }
    if (!m_spec.valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend)) {
        errorfmt(
            "called OpenEXROutput::write_tiles with an invalid tile range");
        return false;
    }

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    bool native            = (format == TypeDesc::UNKNOWN);
    size_t user_pixelbytes = m_spec.pixel_bytes(native);
    size_t pixelbytes      = m_spec.pixel_bytes(true);
    if (native && xstride == AutoStride)
        xstride = (stride_t)user_pixelbytes;
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       (xend - xbegin), (yend - ybegin));
    data = to_native_rectangle(xbegin, xend, ybegin, yend, zbegin, zend, format,
                               data, xstride, ystride, zstride, m_scratch);

    // clamp to the image edge
    xend           = std::min(xend, m_spec.x + m_spec.width);
    yend           = std::min(yend, m_spec.y + m_spec.height);
    zend           = std::min(zend, m_spec.z + m_spec.depth);
    int firstxtile = (xbegin - m_spec.x) / m_spec.tile_width;
    int firstytile = (ybegin - m_spec.y) / m_spec.tile_height;
    int nxtiles = (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width;
    int nytiles = (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height;

    std::vector<char> padded;
    stride_t width      = nxtiles * m_spec.tile_width;
    stride_t height     = nytiles * m_spec.tile_height;
    stride_t widthbytes = width * pixelbytes;
    if (width != (xend - xbegin) || height != (yend - ybegin)) {
        // If the image region is not an even multiple of the tile size,
        // we need to copy and add padding.
        padded.resize(pixelbytes * width * height, 0);
        OIIO::copy_image(m_spec.nchannels, xend - xbegin, yend - ybegin, 1,
                         data, pixelbytes, pixelbytes,
                         (xend - xbegin) * pixelbytes,
                         (xend - xbegin) * (yend - ybegin) * pixelbytes,
                         &padded[0], pixelbytes, widthbytes,
                         height * widthbytes);
        data = &padded[0];
    }

    char* buf = (char*)data - xbegin * pixelbytes - ybegin * widthbytes;

    try {
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        for (int c = 0; c < m_spec.nchannels; ++c) {
            size_t chanbytes = m_spec.channelformat(c).size();
            frameBuffer.insert(m_spec.channelnames[c].c_str(),
                               Imf::Slice(m_pixeltype[c], buf + chanoffset,
                                          pixelbytes, widthbytes));
            chanoffset += chanbytes;
        }
        if (m_output_tiled) {
            m_output_tiled->setFrameBuffer(frameBuffer);
            m_output_tiled->writeTiles(firstxtile, firstxtile + nxtiles - 1,
                                       firstytile, firstytile + nytiles - 1,
                                       m_miplevel, m_miplevel);
        } else if (m_tiled_output_part) {
            m_tiled_output_part->setFrameBuffer(frameBuffer);
            m_tiled_output_part->writeTiles(firstxtile,
                                            firstxtile + nxtiles - 1,
                                            firstytile,
                                            firstytile + nytiles - 1,
                                            m_miplevel, m_miplevel);
        } else {
            errorfmt("Attempt to write tiles for a non-tiled file.");
            return false;
        }
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR write: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR write: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXROutput::write_deep_scanlines(int ybegin, int yend, int /*z*/,
                                    const DeepData& deepdata)
{
    if (m_deep_scanline_output_part == NULL) {
        errorfmt(
            "called OpenEXROutput::write_deep_scanlines without an open file");
        return false;
    }
    if (m_spec.width * (yend - ybegin) != deepdata.pixels()
        || m_spec.nchannels != deepdata.channels()) {
        errorfmt(
            "called OpenEXROutput::write_deep_scanlines with non-matching DeepData size");
        return false;
    }

    size_t nchans(m_spec.nchannels);
    const DeepData* dd = &deepdata;
    std::unique_ptr<DeepData> dd_local;  // In case we need a copy
    bool same_chantypes = true;
    for (size_t c = 0; c < nchans; ++c)
        same_chantypes &= (m_spec.channelformat(c) == deepdata.channeltype(c));
    if (!same_chantypes) {
        // If the channel types don't match, we need to make a copy of the
        // DeepData and convert the channels to the spec's channel types.
        std::vector<TypeDesc> chantypes;
        if (m_spec.channelformats.size() == nchans)
            chantypes = m_spec.channelformats;
        else
            chantypes.resize(nchans, m_spec.format);
        dd_local.reset(new DeepData(deepdata, chantypes));
        dd = dd_local.get();
    }

    try {
        // Set up the count and pointers arrays and the Imf framebuffer
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice(Imf::UINT,
                              (char*)(dd->all_samples().data() - m_spec.x
                                      - ybegin * m_spec.width),
                              sizeof(unsigned int),
                              sizeof(unsigned int) * m_spec.width);
        frameBuffer.insertSampleCountSlice(countslice);
        std::vector<void*> pointerbuf;
        dd->get_pointers(pointerbuf);
        size_t slchans      = size_t(m_spec.width) * nchans;
        size_t xstride      = sizeof(void*) * nchans;
        size_t ystride      = sizeof(void*) * slchans;
        size_t samplestride = dd->samplesize();
        for (size_t c = 0; c < nchans; ++c) {
            Imf::DeepSlice slice(m_pixeltype[c],
                                 (char*)(&pointerbuf[c] - m_spec.x * nchans
                                         - ybegin * slchans),
                                 xstride, ystride, samplestride);
            frameBuffer.insert(m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_scanline_output_part->setFrameBuffer(frameBuffer);

        // Write the pixels
        m_deep_scanline_output_part->writePixels(yend - ybegin);
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR write: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR write: unknown exception");
        return false;
    }

    return true;
}



bool
OpenEXROutput::write_deep_tiles(int xbegin, int xend, int ybegin, int yend,
                                int zbegin, int zend, const DeepData& deepdata)
{
    if (m_deep_tiled_output_part == NULL) {
        errorfmt("called OpenEXROutput::write_deep_tiles without an open file");
        return false;
    }
    if ((xend - xbegin) * (yend - ybegin) * (zend - zbegin) != deepdata.pixels()
        || m_spec.nchannels != deepdata.channels()) {
        errorfmt(
            "called OpenEXROutput::write_deep_tiles with non-matching DeepData size");
        return false;
    }

    size_t nchans      = size_t(m_spec.nchannels);
    const DeepData* dd = &deepdata;
    std::unique_ptr<DeepData> dd_local;  // In case we need a copy
    bool same_chantypes = true;
    for (size_t c = 0; c < nchans; ++c)
        same_chantypes &= (m_spec.channelformat(c) == deepdata.channeltype(c));
    if (!same_chantypes) {
        // If the channel types don't match, we need to make a copy of the
        // DeepData and convert the channels to the spec's channel types.
        std::vector<TypeDesc> chantypes;
        if (m_spec.channelformats.size() == nchans)
            chantypes = m_spec.channelformats;
        else
            chantypes.resize(nchans, m_spec.format);
        dd_local.reset(new DeepData(deepdata, chantypes));
        dd = dd_local.get();
    }

    try {
        size_t width = (xend - xbegin);

        // Set up the count and pointers arrays and the Imf framebuffer
        Imf::DeepFrameBuffer frameBuffer;
        Imf::Slice countslice(Imf::UINT,
                              (char*)(dd->all_samples().data() - xbegin
                                      - ybegin * width),
                              sizeof(unsigned int),
                              sizeof(unsigned int) * width);
        frameBuffer.insertSampleCountSlice(countslice);
        std::vector<void*> pointerbuf;
        dd->get_pointers(pointerbuf);
        size_t slchans      = width * nchans;
        size_t xstride      = sizeof(void*) * nchans;
        size_t ystride      = sizeof(void*) * slchans;
        size_t samplestride = dd->samplesize();
        for (size_t c = 0; c < nchans; ++c) {
            Imf::DeepSlice slice(m_pixeltype[c],
                                 (char*)(&pointerbuf[c] - xbegin * nchans
                                         - ybegin * slchans),
                                 xstride, ystride, samplestride);
            frameBuffer.insert(m_spec.channelnames[c].c_str(), slice);
        }
        m_deep_tiled_output_part->setFrameBuffer(frameBuffer);

        int firstxtile = (xbegin - m_spec.x) / m_spec.tile_width;
        int firstytile = (ybegin - m_spec.y) / m_spec.tile_height;
        int xtiles     = round_to_multiple(xend - xbegin, m_spec.tile_width)
                     / m_spec.tile_width;
        int ytiles = round_to_multiple(yend - ybegin, m_spec.tile_height)
                     / m_spec.tile_height;

        // Write the pixels
        m_deep_tiled_output_part->writeTiles(firstxtile,
                                             firstxtile + xtiles - 1,
                                             firstytile,
                                             firstytile + ytiles - 1,
                                             m_miplevel, m_miplevel);
    } catch (const std::exception& e) {
        errorfmt("Failed OpenEXR write: {}", e.what());
        return false;
    } catch (...) {  // catch-all for edge cases or compiler bugs
        errorfmt("Failed OpenEXR write: unknown exception");
        return false;
    }

    return true;
}


OIIO_PLUGIN_NAMESPACE_END
