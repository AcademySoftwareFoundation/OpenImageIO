#include "DDImage/Enumeration_KnobI.h"
#include "DDImage/Reader.h"
#include "DDImage/Row.h"

#include "OpenImageIO/imageio.h"


using namespace DD::Image;


/*
 * TODO:
 * - Look into using the planar Reader API in Nuke 8, which may map better to
 *      TIFF/OIIO.
 * - Add support for actually changing the Nuke format to match the mip level?
 *      This might be a bad idea...
 * - Handle mip orientations ( ImageSpec.get_int_attribute("Orientation", default); )
 * - It would be nice to have a way to read in a region, rather than the whole
 *      image, but this hinges on getting access to the Read's request region.
 */


namespace TxReaderNS
{


OIIO_NAMESPACE_USING


static const char* const EMPTY[] = {NULL};


class TxReaderFormat : public ReaderFormat {
    int mipLevel_;
    Knob* mipLevelKnob_;

public:
    TxReaderFormat() : mipLevel_(0), mipLevelKnob_(NULL) { }

    void knobs(Knob_Callback cb) {
        mipLevelKnob_ = Enumeration_knob(cb, &mipLevel_, EMPTY,
                                         "tx_mip_level", "mip level");
        SetFlags(cb, Knob::EXPAND_TO_WIDTH);
        Tooltip(cb, "The mip level to read from the file. Currently, this will "
                "be resampled to fill the same resolution as the base image.");
    }

    void append(Hash& hash) { hash.append(mipLevel_); }

    inline int mipLevel() { return mipLevel_; }

    void setMipLabels(std::vector<std::string> items) {
        mipLevelKnob_->enumerationKnob()->menu(items);
    }

    const char* help() { return "Tiled, mipmapped texture format"; }
};


class txReader : public Reader {
    ImageInput* oiioInput_;
    TxReaderFormat* txFmt_;

    int fullW_, fullH_, chanCount_, lastMipLevel_;
    bool haveImage_;
    std::vector<float> imageBuf_;

    MetaData::Bundle meta_;

    static const Description d;

    void fillMetadata(const ImageSpec& spec) {
        switch (spec.format.basetype) {
            case TypeDesc::UINT8:
            case TypeDesc::INT8:
                meta_.setData(MetaData::DEPTH, MetaData::DEPTH_8);
                break;
            case TypeDesc::UINT16:
            case TypeDesc::INT16:
                meta_.setData(MetaData::DEPTH, MetaData::DEPTH_16);
                break;
            case TypeDesc::UINT32:
            case TypeDesc::INT32:
                meta_.setData(MetaData::DEPTH, MetaData::DEPTH_32);
                break;
            case TypeDesc::FLOAT:
                meta_.setData(MetaData::DEPTH, MetaData::DEPTH_FLOAT);
                break;
            case TypeDesc::DOUBLE:
                meta_.setData(MetaData::DEPTH, MetaData::DEPTH_DOUBLE);
                break;
            default:
                meta_.setData(MetaData::DEPTH, "Unknown");
                break;
        }

        meta_.setData("tx/tile_width", spec.tile_width);
        meta_.setData("tx/tile_height", spec.tile_height);

        string_view val;

        val = spec.get_string_attribute("ImageDescription");
        if (!val.empty())
            meta_.setData("tx/image_description", val);

        val = spec.get_string_attribute("DateTime");
        if (!val.empty())
            meta_.setData(MetaData::CREATED_TIME, val);

        val = spec.get_string_attribute("Software");
        if (!val.empty())
            meta_.setData(MetaData::CREATOR, val);

        val = spec.get_string_attribute("textureformat");
        if (!val.empty())
            meta_.setData("tx/texture_format", val);

        val = spec.get_string_attribute("wrapmodes");
        if (!val.empty())
            meta_.setData("tx/wrap_modes", val);

        val = spec.get_string_attribute("fovcot");
        if (!val.empty())
            meta_.setData("tx/fovcot", val);

        val = spec.get_string_attribute("compression");
        if (!val.empty())
            meta_.setData("tx/compression", val);

        val = spec.get_string_attribute("tiff:planarconfig");
        if (!val.empty())
            meta_.setData("tiff/planar_config", val);

        int v = spec.get_int_attribute("Orientation");
        if (v > 0)
            meta_.setData("tiff/orientation", v);
    }

public:
    txReader(Read* iop) : Reader(iop),
            fullW_(0),
            fullH_(0),
            chanCount_(0),
            lastMipLevel_(-1),
            haveImage_(false)
    {
        txFmt_ = dynamic_cast<TxReaderFormat*>(iop->handler());

        OIIO::attribute("threads", (int)Thread::numThreads / 2);

        oiioInput_ = ImageInput::open(filename());
        if (!oiioInput_) {
            iop->internalError("OIIO: Failed to open file %s: %s", filename(),
                               geterror().c_str());
            return;
        }

        const ImageSpec& baseSpec = oiioInput_->spec();

        fillMetadata(baseSpec);

        if (!baseSpec.width * baseSpec.height) {
            iop->internalError("tx file has one or more zero dimensions "
                               "(%d x %d)", baseSpec.width, baseSpec.height);
            return;
        }

        fullW_ = baseSpec.width;
        fullH_ = baseSpec.height;

        chanCount_ = baseSpec.nchannels;
        if (chanCount_ > 4) {
            iop->warning("%s contains more than 4 channels, but input will be "
                         "limited to the first 4.", filename());
            chanCount_ = 4;
        }
        set_info(fullW_, fullH_, chanCount_);

        // Populate mip level pulldown with labels in the form:
        //      "MIPLEVEL\tMIPLEVEL - WxH" (e.g. "0\t0 - 1920x1080")
        // The knob will split these on tab characters, and only store the
        // first part (i.e. the index) when the knob is serialized.
        std::vector<std::string> mipLabels;
        std::ostringstream buf;
        ImageSpec mipSpec(baseSpec);
        int mipLevel = 0;
        while (true) {
            buf << mipLevel << '\t' << mipLevel << " - " << mipSpec.width << 'x' << mipSpec.height;
            mipLabels.push_back(buf.str());
            if (oiioInput_->seek_subimage(0, mipLevel + 1, mipSpec)) {
                buf.str(std::string());
                buf.clear();
                mipLevel++;
            }
            else
                break;
        }

        meta_.setData("tx/mip_levels", mipLevel + 1);

        txFmt_->setMipLabels(mipLabels);
    }

    virtual ~txReader() {
        if (oiioInput_)
            oiioInput_->close();
        delete oiioInput_;
        oiioInput_ = NULL;
    }

    void open() {
        if (lastMipLevel_ != txFmt_->mipLevel()) {
            ImageSpec mipSpec;
            if (!oiioInput_->seek_subimage(0, txFmt_->mipLevel(), mipSpec)) {
                iop->internalError("Failed to seek to mip level %d: %s",
                        txFmt_->mipLevel(), oiioInput_->geterror().c_str());
                return;
            }

            if (txFmt_->mipLevel() && mipSpec.nchannels < chanCount_) {
                iop->internalError("txReader does not support mip levels with "
                                   "different channel counts");
                return;
            }

            lastMipLevel_ = txFmt_->mipLevel();
            haveImage_ = false;
        }

        if (!haveImage_) {
            const int needSize = oiioInput_->spec().width
                                * oiioInput_->spec().height
                                * oiioInput_->spec().nchannels;
            if (needSize > imageBuf_.size())
                imageBuf_.resize(needSize);
            oiioInput_->read_image(&imageBuf_[0]);
            haveImage_ = true;
        }
    }

    void engine(int y, int x, int r, ChannelMask channels, Row& row) {
        if (!haveImage_)
            iop->error("txReader engine called, but haveImage_ is false");

        if (aborted()) {
            row.erase(channels);
            return;
        }

        ChannelSet readChannels(chanCount_ > 3 ? Mask_RGBA : Mask_RGB);
        // Eventually we should probably support more than 4 channels...
        const int fileChans = oiioInput_->spec().nchannels;

        if (lastMipLevel_) {  // Mip level other than 0
            const int mipW = oiioInput_->spec().width;
            const int mipMult = fullW_ / mipW;

            const int bufY = (fullH_ - y - 1) * oiioInput_->spec().height / fullH_;
            const int bufX = x ? x / mipMult : 0;
            const int bufR = r / mipMult;
            const int bufW = bufR - bufX;

            std::vector<float> chanBuf(bufW);
            float* chanStart = &chanBuf[0];
            const int bufStart = bufY * mipW * fileChans + bufX * fileChans;
            const float* alpha = chanCount_ > 3 ? &imageBuf_[bufStart + 3] : NULL;
            foreach (z, readChannels) {
                from_float(z, &chanBuf[0], &imageBuf_[bufStart + z - 1], alpha,
                           bufW, fileChans);

                float* OUT = row.writable(z);
                for (int stride = 0, c = 0; stride < bufW; stride++, c = 0)
                    for (; c < mipMult; c++)
                        *OUT++ = *(chanStart + stride);
            }
        }
        else {  // Mip level 0
            const int bufY = fullH_ - y - 1;
            const int pixStart = bufY * fullW_ * fileChans + x * fileChans;
            const float* alpha = chanCount_ > 3 ? &imageBuf_[pixStart + 3] : NULL;

            foreach (z, readChannels)
                from_float(z, row.writable(z) + x, &imageBuf_[pixStart + z - 1],
                           alpha, r - x, fileChans);
        }
    }

    const MetaData::Bundle& fetchMetaData(const char* key) { return meta_; }
};


}  // ~TxReaderNS


static Reader* buildReader(Read* iop, int fd, const unsigned char* b, int n) {
    close(fd);
    return new TxReaderNS::txReader(iop);
}

static ReaderFormat* buildformat(Read* iop) {
    return new TxReaderNS::TxReaderFormat();
}

// Test code copied from tiffReader example plugin
static bool test(int fd, const unsigned char* block, int length) {
    // Big-endian
    if (block[0] == 'M' && block[1] == 'M' && block[2] == 0 && block[3] == 42)
        return true;
    // Little-endian
    if (block[0] == 'I' && block[1] == 'I' && block[2] == 42 && block[3] == 0)
        return true;
    return false;
}

const Reader::Description TxReaderNS::txReader::d("tx\0TX\0", buildReader, test,
                                                  buildformat);
