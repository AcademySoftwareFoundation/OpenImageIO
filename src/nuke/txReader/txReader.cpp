// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#ifndef _WIN32
#    include <unistd.h>
#endif

#include "DDImage/Enumeration_KnobI.h"
#include "DDImage/Reader.h"
#include "DDImage/Row.h"

#include <OpenImageIO/imageio.h>


using namespace DD::Image;


/*
 * TODO:
 * - Look into using the planar Reader API in Nuke 8, which may map better to
 *      TIFF/OIIO.
 * - It would be nice to have a way to read in a region, rather than the whole
 *      image, but this would require access to the Read's request region,
 *      which isn't currently possible. A feature request for this is logged
 *      with The Foundry as Bug 46237.
 */


namespace TxReaderNS {


using namespace OIIO;


static const char* const EMPTY[] = { NULL };


class TxReaderFormat final : public ReaderFormat {
    int mipLevel_;
    int mipEnumIndex_;
    Knob* mipLevelKnob_;
    Knob* mipLevelEnumKnob_;

public:
    TxReaderFormat()
        : mipLevel_(0)
        , mipEnumIndex_(0)
        , mipLevelKnob_(NULL)
        , mipLevelEnumKnob_(NULL)
    {
    }

    void knobs(Knob_Callback cb)
    {
        // The "real" mip level knob that controls the level read by the Reader
        // class, and whose value is stored when the Read is serialized.
        mipLevelKnob_ = Int_knob(cb, &mipLevel_, "tx_mip_level", "mip index");
        SetFlags(cb, Knob::INVISIBLE);

        // The user-facing mip level dropdown. This is populated lazily by the
        // Reader when it opens a file, and does not directly contribute to the
        // op hash or get stored when the Read is serialized.
        mipLevelEnumKnob_ = Enumeration_knob(cb, &mipEnumIndex_, EMPTY,
                                             "tx_user_mip_level", "mip level");
        SetFlags(cb, Knob::EXPAND_TO_WIDTH | Knob::DO_NOT_WRITE
                         | Knob::NO_RERENDER);
        Tooltip(cb,
                "The mip level to read from the file. Currently, this will "
                "be resampled to fill the same resolution as the base image.");
    }

    int knob_changed(Knob* k)
    {
        if (k == mipLevelEnumKnob_)
            mipLevelKnob_->set_value(mipEnumIndex_);
        return 1;
    }

    void append(Hash& hash) { hash.append(mipLevel_); }

    inline int mipLevel() { return mipLevel_; }

    void setMipLabels(std::vector<std::string> items)
    {
        if (mipLevelEnumKnob_) {
            mipLevelEnumKnob_->set_flag(Knob::NO_KNOB_CHANGED);
            mipLevelEnumKnob_->enumerationKnob()->menu(items);
            mipLevelEnumKnob_->set_value(
                std::min((int)items.size() - 1, mipLevel_));
            mipLevelEnumKnob_->clear_flag(Knob::NO_KNOB_CHANGED);
        }
    }

    const char* help() { return "Tiled, mipmapped texture format"; }
};


class txReader final : public Reader {
    std::unique_ptr<ImageInput> oiioInput_;
    TxReaderFormat* txFmt_;

    int chanCount_, lastMipLevel_;
    bool haveImage_, flip_;
    std::vector<float> imageBuf_;
    std::map<Channel, int> chanMap_;

    MetaData::Bundle meta_;

    static const Description d;

    void fillMetadata(const ImageSpec& spec, bool isEXR)
    {
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
        case TypeDesc::HALF:
            meta_.setData(MetaData::DEPTH, MetaData::DEPTH_HALF);
            break;
        case TypeDesc::FLOAT:
            meta_.setData(MetaData::DEPTH, MetaData::DEPTH_FLOAT);
            break;
        case TypeDesc::DOUBLE:
            meta_.setData(MetaData::DEPTH, MetaData::DEPTH_DOUBLE);
            break;
        default: meta_.setData(MetaData::DEPTH, "Unknown"); break;
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

        if (isEXR) {
            val = spec.get_string_attribute("openexr:lineOrder");
            if (!val.empty())
                meta_.setData("exr/line_order", val);

            float cl = spec.get_float_attribute("openexr:dwaCompressionLevel",
                                                0.0f);
            if (val > 0)
                meta_.setData("exr/dwa_compression_level", cl);
        } else {
            val = spec.get_string_attribute("tiff:planarconfig");
            if (!val.empty())
                meta_.setData("tiff/planar_config", val);
        }
    }

    void setChannels(const ImageSpec& spec)
    {
        ChannelSet mask;
        Channel chan;
        int chanIndex = 0;

        for (std::vector<std::string>::const_iterator it
             = spec.channelnames.begin();
             it != spec.channelnames.end(); it++) {
            chan = Reader::channel(it->c_str());
            mask += chan;
            chanMap_[chan] = chanIndex++;
        }

        info_.channels(mask);
    }

public:
    txReader(Read* iop)
        : Reader(iop)
        , oiioInput_(ImageInput::open(filename()))
        , chanCount_(0)
        , lastMipLevel_(-1)
        , haveImage_(false)
        , flip_(false)
    {
        txFmt_ = dynamic_cast<TxReaderFormat*>(iop->handler());

        OIIO::attribute("threads", (int)Thread::numThreads / 2);

        if (!oiioInput_) {
            iop->internalError("OIIO: Failed to open file %s: %s", filename(),
                               geterror().c_str());
            return;
        }

        const ImageSpec& baseSpec = oiioInput_->spec();

        if (!(baseSpec.width * baseSpec.height)) {
            iop->internalError("tx file has one or more zero dimensions "
                               "(%d x %d)",
                               baseSpec.width, baseSpec.height);
            return;
        }

        chanCount_       = baseSpec.nchannels;
        const bool isEXR = strcmp(oiioInput_->format_name(), "openexr") == 0;

        if (isEXR) {
            float pixAspect = baseSpec.get_float_attribute("PixelAspectRatio",
                                                           0);
            set_info(baseSpec.width, baseSpec.height, 1, pixAspect);
            meta_.setData(MetaData::PIXEL_ASPECT,
                          pixAspect > 0 ? pixAspect : 1.0f);
            setChannels(baseSpec);  // Fills chanMap_
            flip_ = true;
        } else {
            set_info(baseSpec.width, baseSpec.height, chanCount_);
            int orientation = baseSpec.get_int_attribute("Orientation", 1);
            meta_.setData("tiff/orientation", orientation);
            flip_ = !((orientation - 1) & 2);

            int chanIndex = 0;
            foreach (z, info_.channels())
                chanMap_[z] = chanIndex++;
        }

        fillMetadata(baseSpec, isEXR);

        // Populate mip level pulldown with labels in the form:
        //      "MIPLEVEL - WxH" (e.g. "0 - 1920x1080")
        std::vector<std::string> mipLabels;
        std::ostringstream buf;
        ImageSpec mipSpec(baseSpec);
        int mipLevel = 0;
        while (true) {
            buf << mipLevel << " - " << mipSpec.width << 'x' << mipSpec.height;
            mipLabels.push_back(buf.str());
            if (oiioInput_->seek_subimage(0, mipLevel + 1, mipSpec)) {
                buf.str(std::string());
                buf.clear();
                mipLevel++;
            } else
                break;
        }

        meta_.setData("tx/mip_levels", mipLevel + 1);

        txFmt_->setMipLabels(mipLabels);
    }

    virtual ~txReader()
    {
        if (oiioInput_)
            oiioInput_->close();
    }

    void open()
    {
        if (lastMipLevel_ != txFmt_->mipLevel()) {
            ImageSpec mipSpec;
            if (!oiioInput_->seek_subimage(0, txFmt_->mipLevel(), mipSpec)) {
                iop->internalError("Failed to seek to mip level %d: %s",
                                   txFmt_->mipLevel(),
                                   oiioInput_->geterror().c_str());
                return;
            }

            if (txFmt_->mipLevel() && mipSpec.nchannels != chanCount_) {
                iop->internalError("txReader does not support mip levels with "
                                   "different channel counts");
                return;
            }

            lastMipLevel_ = txFmt_->mipLevel();
            haveImage_    = false;
        }

        if (!haveImage_) {
            const int needSize = oiioInput_->spec().width
                                 * oiioInput_->spec().height
                                 * oiioInput_->spec().nchannels;
            if (size_t(needSize) > imageBuf_.size())
                imageBuf_.resize(needSize);
            oiioInput_->read_image(&imageBuf_[0]);
            haveImage_ = true;
        }
    }

    void engine(int y, int x, int r, ChannelMask channels, Row& row)
    {
        if (!haveImage_)
            iop->internalError("engine called, but haveImage_ is false");

        if (aborted()) {
            row.erase(channels);
            return;
        }

        const bool doAlpha = channels.contains(Chan_Alpha);

        if (flip_)
            y = height() - y - 1;

        if (lastMipLevel_) {  // Mip level other than 0
            const int mipW    = oiioInput_->spec().width;
            const int mipMult = width() / mipW;

            y              = y * oiioInput_->spec().height / height();
            const int bufX = x ? x / mipMult : 0;
            const int bufR = r / mipMult;
            const int bufW = bufR - bufX;

            std::vector<float> chanBuf(bufW);
            float* chanStart   = &chanBuf[0];
            const int bufStart = y * mipW * chanCount_ + bufX * chanCount_;
            const float* alpha
                = doAlpha ? &imageBuf_[bufStart + chanMap_[Chan_Alpha]] : NULL;
            foreach (z, channels) {
                from_float(z, &chanBuf[0], &imageBuf_[bufStart + chanMap_[z]],
                           alpha, bufW, chanCount_);

                float* OUT = row.writable(z);
                for (int stride = 0, c = 0; stride < bufW; stride++, c = 0)
                    for (; c < mipMult; c++)
                        *OUT++ = *(chanStart + stride);
            }
        } else {  // Mip level 0
            const int pixStart = y * width() * chanCount_ + x * chanCount_;
            const float* alpha
                = doAlpha ? &imageBuf_[pixStart + chanMap_[Chan_Alpha]] : NULL;

            foreach (z, channels) {
                from_float(z, row.writable(z) + x,
                           &imageBuf_[pixStart + chanMap_[z]], alpha, r - x,
                           chanCount_);
            }
        }
    }

    const MetaData::Bundle& fetchMetaData(const char* key) { return meta_; }
};


}  // namespace TxReaderNS


static Reader*
buildReader(Read* iop, int fd, const unsigned char* b, int n)
{
    // FIXME: I expect that this close() may be problematic on Windows.
    // For Linux/gcc, we needed to #include <unistd.h> at the top of
    // this file. If this is a problem for Windows, a different #include
    // or a different close call here may be necessary.
    close(fd);
    return new TxReaderNS::txReader(iop);
}

static ReaderFormat*
buildformat(Read* iop)
{
    return new TxReaderNS::TxReaderFormat();
}

static bool
test(int fd, const unsigned char* block, int length)
{
    // Big-endian TIFF
    if (block[0] == 'M' && block[1] == 'M' && block[2] == 0 && block[3] == 42)
        return true;
    // Little-endian TIFF
    if (block[0] == 'I' && block[1] == 'I' && block[2] == 42 && block[3] == 0)
        return true;
    // EXR
    return block[0] == 0x76 && block[1] == 0x2f && block[2] == 0x31
           && block[3] == 0x01;
}

const Reader::Description TxReaderNS::txReader::d("tx\0TX\0", buildReader, test,
                                                  buildformat);
