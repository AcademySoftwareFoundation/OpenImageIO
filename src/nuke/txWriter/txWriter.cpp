// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <sstream>
#include <vector>

#include "DDImage/Row.h"
#include "DDImage/Thread.h"
#include "DDImage/Version.h"
#include "DDImage/Writer.h"

#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>


/*
 * TODO:
 * - Look into using an ImageBuf iterator to fill the source buffer.
 * - Support for more than 4 output channels is easy, but we can't currently
 *      set the output channel names in such a way that OIIO will store them in
 *      the output file.
 * - Could throw Nuke script name and/or tree hash into the metadata
 *      ( iop->getHashOfInputs() )
 */


using namespace DD::Image;


namespace TxWriterNS {


using namespace OIIO;

// Limit the available output datatypes (for now, at least).
static const TypeDesc::BASETYPE oiioBitDepths[]
    = { TypeDesc::INT8, TypeDesc::INT16, TypeDesc::INT32, TypeDesc::FLOAT,
        TypeDesc::DOUBLE };

// Knob values for above bit depths (keep them synced!)
static const char* const bitDepthValues[]
    = { "8-bit integer", "16-bit integer", "32-bit integer",
        "32-bit float",  "64-bit double",  NULL };

// Knob values for NaN fix modes
static const char* const nanFixValues[] = { "black\tblack", "box3\tbox3 filter",
                                            NULL };

// Knob values for "preset" modes
static const char* const presetValues[] = { "oiio", "prman", "custom", NULL };

// Knob values for planar configuration
static const char* const planarValues[] = { "contig\tcontiguous", "separate",
                                            NULL };

// Knob values for texture mode configuration
static const char* const txModeValues[] = {
    "Ordinary 2D texture", "Latitude-longitude environment map",
    "Latitude-longitude environment map (light probe)", "Shadow texture", NULL
};
static const ImageBufAlgo::MakeTextureMode oiiotxMode[]
    = { ImageBufAlgo::MakeTxTexture, ImageBufAlgo::MakeTxEnvLatl,
        ImageBufAlgo::MakeTxEnvLatlFromLightProbe, ImageBufAlgo::MakeTxShadow };


bool gTxFiltersInitialized = false;
static std::vector<const char*> gFilterNames;


class txWriter final : public Writer {
    int preset_;
    int tileW_, tileH_;
    int planarMode_;
    int txMode_;
    int bitDepth_;
    int filter_;
    bool highlightComp_;
    bool detectConstant_;
    bool detectMonochrome_;
    bool detectOpaque_;
    bool fixNan_;
    int nanFixType_;
    bool checkNan_;
    bool verbose_;


    void setChannelNames(ImageSpec& spec, const ChannelSet& channels)
    {
        if (channels == Mask_RGB || channels == Mask_RGBA)
            return;

        int index = 0;
        std::ostringstream buf;
        foreach (z, channels) {
            if (index > 0)
                buf << ",";
            switch (z) {
            case Chan_Red: buf << "R"; break;
            case Chan_Green: buf << "G"; break;
            case Chan_Blue: buf << "B"; break;
            case Chan_Alpha:
                buf << "A";
                spec.alpha_channel = index;
                break;
            case Chan_Z:
                buf << "Z";
                spec.z_channel = index;
                break;
            default: buf << getName(z); break;
            }
            index++;
        }
        spec.attribute("maketx:channelnames", buf.str());
    }

public:
    txWriter(Write* iop)
        : Writer(iop)
        , preset_(0)
        , tileW_(64)
        , tileH_(64)
        , planarMode_(0)  // contiguous
        , txMode_(0)      // ordinary 2d texture
        , bitDepth_(3)    // float
        , filter_(0)
        , highlightComp_(false)
        , detectConstant_(false)
        , detectMonochrome_(false)
        , detectOpaque_(false)
        , fixNan_(false)
        , nanFixType_(0)
        , checkNan_(true)
        , verbose_(false)
    {
        if (!gTxFiltersInitialized) {
            for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
                FilterDesc d;
                Filter2D::get_filterdesc(i, &d);
                gFilterNames.push_back(d.name);
            }
            gFilterNames.push_back(NULL);
            gTxFiltersInitialized = true;
        }
    }

    void knobs(Knob_Callback cb)
    {
        Enumeration_knob(cb, &preset_, &presetValues[0], "preset");
        Tooltip(cb,
                "Choose a preset for various output parameters.\n"
                "<b>oiio</b>: Tile and planar settings optimized for OIIO.\n"
                "<b>prman</b>: Tile and planar ettings and metadata safe for "
                "use with prman.");

        Knob* k;

        k = Int_knob(cb, &tileW_, "tile_width", "tile size");
        if (cb.makeKnobs())
            k->disable();
        else if (preset_ == 2)
            k->enable();
        Tooltip(cb, "Tile width");

        k = Int_knob(cb, &tileH_, "tile_height", "x");
        if (cb.makeKnobs())
            k->disable();
        else if (preset_ == 2)
            k->enable();
        Tooltip(cb, "Tile height");
        ClearFlags(cb, Knob::STARTLINE);

        k = Enumeration_knob(cb, &planarMode_, &planarValues[0],
                             "planar_config", "planar config");
        if (cb.makeKnobs())
            k->disable();
        else if (preset_ == 2)
            k->enable();
        Tooltip(cb, "Planar mode of the image channels.");
        SetFlags(cb, Knob::STARTLINE);

        Enumeration_knob(cb, &txMode_, &txModeValues[0], "tx_mode", "mode");
        Tooltip(cb, "What type of texture file we are creating.");

        Enumeration_knob(cb, &bitDepth_, &bitDepthValues[0], "tx_datatype",
                         "datatype");
        Tooltip(cb, "The datatype of the output image.");

        Enumeration_knob(cb, &filter_, &gFilterNames[0], "tx_filter", "filter");
        Tooltip(cb, "The filter used to resize the image when generating mip "
                    "levels.");

        Bool_knob(cb, &highlightComp_, "highlight_compensation",
                  "highlight compensation");
        Tooltip(cb, "Compress dynamic range before resampling for mip levels, "
                    "and re-expand it afterward, while also clamping negative "
                    "pixel values to zero. This can help avoid artifacts when "
                    "using filters with negative lobes.");
        SetFlags(cb, Knob::STARTLINE);

        Bool_knob(cb, &detectConstant_, "detect_constant", "detect constant");
        Tooltip(cb, "Detect whether the image is entirely a single color, and "
                    "write it as a single-tile output file if so.");
        SetFlags(cb, Knob::STARTLINE);

        Bool_knob(cb, &detectMonochrome_, "detect_monochrome",
                  "detect monochrome");
        Tooltip(cb, "Detect whether the image's R, G, and B values are equal "
                    "everywhere, and write it as a single-channel (grayscale) "
                    "image if so.");
        ClearFlags(cb, Knob::STARTLINE);

        Bool_knob(cb, &detectOpaque_, "detect_opaque", "detect opaque");
        Tooltip(cb, "Detect whether the image's alpha channel is 1.0 "
                    "everywhere, and drop it from the output file if so (write "
                    "RGB instead).");

        Bool_knob(cb, &fixNan_, "fix_nan", "fix NaN/Inf pixels");
        Tooltip(cb, "Attempt to fix NaN/Inf pixel values in the image.");
        SetFlags(cb, Knob::STARTLINE);

        k = Enumeration_knob(cb, &nanFixType_, &nanFixValues[0], "nan_fix_type",
                             "");
        if (cb.makeKnobs())
            k->disable();
        else if (fixNan_)
            k->enable();
        Tooltip(cb, "The method to use to fix NaN/Inf pixel values.");
        ClearFlags(cb, Knob::STARTLINE);

        Bool_knob(cb, &checkNan_, "check_nan", "error on NaN/Inf");
        Tooltip(cb,
                "Check for NaN/Inf pixel values in the output image, and "
                "error if any are found. If this is enabled, the check will be "
                "run <b>after</b> the NaN fix process.");
        SetFlags(cb, Knob::STARTLINE);

        Bool_knob(cb, &verbose_, "verbose");
        Tooltip(cb, "Toggle verbose OIIO output.");
        SetFlags(cb, Knob::STARTLINE);
    }

    int knob_changed(Knob* k)
    {
        if (k->is("fix_nan")) {
            iop->knob("nan_fix_type")->enable(fixNan_);
            return 1;
        }
        if (k->is("preset")) {
            const bool e = preset_ == 2;
            iop->knob("tile_width")->enable(e);
            iop->knob("tile_height")->enable(e);
            iop->knob("planar_config")->enable(e);
            return 1;
        }

        return Writer::knob_changed(k);
    }

    void execute()
    {
        const int chanCount = num_channels();
        ChannelSet channels = channel_mask(chanCount);
        const bool doAlpha  = channels.contains(Chan_Alpha);

        iop->progressMessage("Preparing image");
        input0().request(0, 0, width(), height(), channels, 1);

        if (aborted())
            return;

        ImageSpec srcSpec(width(), height(), chanCount, TypeDesc::FLOAT);
        ImageBuf srcBuffer(srcSpec);
        Row row(0, width());
        // Buffer for a channel-interleaved row after output LUT processing
        std::vector<float> lutBuffer(width() * chanCount);

        for (int y = 0; y < height(); y++) {
            iop->progressFraction(double(y) / height() * 0.85);
            get(height() - y - 1, 0, width(), channels, row);
            if (aborted())
                return;

            const float* alpha = doAlpha ? row[Chan_Alpha] : NULL;

            for (int i = 0; i < chanCount; i++)
                to_float(i, &lutBuffer[i], row[channel(i)], alpha, width(),
                         chanCount);
            for (int x = 0; x < width(); x++)
                srcBuffer.setpixel(x, y, &lutBuffer[x * chanCount]);
        }

        ImageSpec destSpec(width(), height(), chanCount,
                           oiioBitDepths[bitDepth_]);

        setChannelNames(destSpec, channels);

        destSpec.attribute("maketx:filtername", gFilterNames[filter_]);

        switch (preset_) {
        case 0: destSpec.attribute("maketx:oiio_options", 1); break;
        case 1: destSpec.attribute("maketx:prman_options", 1); break;
        default:
            destSpec.tile_width  = tileW_;
            destSpec.tile_height = tileH_;
            destSpec.attribute("planarconfig",
                               planarMode_ ? "separate" : "contig");
            break;
        }

        if (fixNan_) {
            if (nanFixType_)
                destSpec.attribute("maketx:fixnan", "box3");
            else
                destSpec.attribute("maketx:fixnan", "black");
        } else
            destSpec.attribute("maketx:fixnan", "none");

        destSpec.attribute("maketx:highlightcomp", (int)highlightComp_);
        destSpec.attribute("maketx:constant_color_detect",
                           (int)detectConstant_);
        destSpec.attribute("maketx:monochrome_detect", (int)detectMonochrome_);
        destSpec.attribute("maketx:opaque_detect", (int)detectOpaque_);
        destSpec.attribute("maketx:checknan", (int)checkNan_);
        destSpec.attribute("maketx:verbose", (int)verbose_);

        std::string software
            = Strutil::fmt::format("OpenImageIO {}, Nuke {}",
                                   OIIO_VERSION_STRING,
                                   applicationVersion().string());
        destSpec.attribute("Software", software);

        if (aborted())
            return;

        OIIO::attribute("threads", (int)Thread::numCPUs);

        iop->progressMessage("Writing %s", filename());
        std::stringstream errmsg;
        if (!ImageBufAlgo::make_texture(oiiotxMode[txMode_], srcBuffer,
                                        filename(), destSpec, &errmsg))
            iop->error("ImageBufAlgo::make_texture failed to write file %s (%s)",
                       filename(), errmsg.str().c_str());
    }

    const char* help() { return "Tiled, mipmapped texture format"; }

    static const Writer::Description d;
};


}  // namespace TxWriterNS


static Writer*
build(Write* iop)
{
    return new TxWriterNS::txWriter(iop);
}
const Writer::Description TxWriterNS::txWriter::d("tx\0TX\0", build);
