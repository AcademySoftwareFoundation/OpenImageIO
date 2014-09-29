#include "txReader.h"


/*
 * TODO:
 * - Populate metadata in constructor
 * - Handle imperfect factors in mip dimensions
 * - Make sure channel logic is sound
 * - Add support for actually changing the Nuke format to match the mip level?
 *      This might be a bad idea...
 * - Handle mip orientations ( ImageSpec.get_int_attribute("Orientation", default); )
 * - Look into using the planar Reader API in Nuke 8, which may map better to
 *      TIFF/OIIO.
 */


namespace TxReaderNS {
    txReader::txReader(Read* read) : Reader(read),
            fullW_(0),
            fullH_(0),
            channelCount_(0),
            lastMipLevel_(-1),
            haveImage_(false)
    {
        txFmt_ = dynamic_cast<TxReaderFormat*>(iop->handler());
        if (!txFmt_) {
            iop->error("Failed to cast Read handler to TxReaderFormat");
            return;
        }

        OIIO::attribute("threads", (int)Thread::numThreads / 2);

        oiioInput_ = ImageInput::open(filename());
        if (!oiioInput_) {
            iop->internalError("Failed to create ImageInput for file %s");
            return;
        }

        const ImageSpec& baseSpec = oiioInput_->spec();

        if (!baseSpec.width * baseSpec.height) {
            iop->internalError("tx file has one or more zero dimensions "
                               "(%d x %d)", baseSpec.width, baseSpec.height);
            return;
        }

        fullW_ = baseSpec.width;
        fullH_ = baseSpec.height;

        channelCount_ = baseSpec.nchannels;
        set_info(fullW_, fullH_, channelCount_);

        // Populate mip level pulldown with labels in the form:
        //      "MIPLEVEL\tMIPLEVEL - WxH" (e.g. "0\t0 - 1920x1080")
        // The knob will split these values on tab characters, and only store
        // the first part (i.e. the index) when the knob is serialized.
        std::vector<std::string> mipLabels;
        std::ostringstream buf;
        ImageSpec mipSpec(baseSpec);
        for (int mipLevel = 0; ; mipLevel++) {
            buf << mipLevel << '\t' << mipLevel << " - " << mipSpec.width << 'x' << mipSpec.height;
            mipLabels.push_back(buf.str());
            if (oiioInput_->seek_subimage(0, mipLevel + 1, mipSpec)) {
                buf.str(std::string());
                buf.clear();
            }
            else
                break;
        }

        txFmt_->setMipLabels(mipLabels);
    }

    txReader::~txReader() {
        // The NDK docs mention this: "The destructor must close any files
        // (even though the Read may have opened them)." However, I think OIIO
        // takes care of this.
        if (oiioInput_)
            oiioInput_->close();
        delete oiioInput_;
        oiioInput_ = NULL;
    }

    void txReader::open() {
        // Seek to the correct mip level
        if (lastMipLevel_ != txFmt_->mipLevel()) {
            printf("Seeking to mip level %d\n", txFmt_->mipLevel());
            ImageSpec seekSpec;
            if (!oiioInput_->seek_subimage(0, txFmt_->mipLevel(), seekSpec)) {
                iop->internalError("Failed to seek to mip level %d",
                                   txFmt_->mipLevel());
                return;
            }

            // If the mip level is anything other than 0, make sure it has the
            // expected number of channels.
            if (txFmt_->mipLevel() && seekSpec.nchannels != channelCount_) {
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
                                * channelCount_;
            if (needSize > imageBuf_.size())
                imageBuf_.resize(needSize);
            oiioInput_->read_image(&imageBuf_[0]);
            haveImage_ = true;
        }
    }

    void txReader::engine(int y, int x, int r, ChannelMask channels, Row& row) {
        if (!haveImage_)
            iop->error("txReader engine called, but haveImage_ is false");

        if (aborted()) {
            row.erase(channels);
            return;
        }

        // I don't know if this is the right way to do this...
        ChannelSet readChannels(channels);
        readChannels &= (channelCount_ > 3 ? Mask_RGBA : Mask_RGB);

        if (lastMipLevel_) {  // Mip level other than 0
            const int mipW = oiioInput_->spec().width;
            const int mipMult = fullW_ / mipW;
            if (fullW_ % mipW) {
                iop->error("Mip width %d is not an exact factor of full image "
                           "width %d", mipW, fullW_);
                row.erase(channels);
                return;
            }

            // TODO: Orientation may change this
            const int bufY = (fullH_ - y - 1) * oiioInput_->spec().height / fullH_;
            const int bufX = x ? x / mipMult : 0;
            const int bufR = r / mipMult;
            const int copyW = bufR - bufX;

            std::vector<float> chanBuf(copyW);
            float* chanStart = &chanBuf[0];
            const int bufStart = bufY * mipW * channelCount_ + (bufX * channelCount_);
            const float* alpha = channelCount_ > 3 ? &imageBuf_[bufStart + 3] : NULL;
            foreach (z, readChannels) {
                from_float(z, &chanBuf[0], &imageBuf_[bufStart + z - 1], alpha,
                           copyW, channelCount_);
                float* OUT = row.writable(z);
                for (int xStride = 0, fillCount = 0; xStride < copyW; xStride++, fillCount = 0) {
                    for (; fillCount < mipMult; fillCount++)
                        *OUT++ = *(chanStart + xStride);
                }
            }
        }
        else {  // Mip level 0
            // TODO: Orientation may change this
            const int bufY = fullH_ - y - 1;
            const int pixStart = bufY * fullW_ * channelCount_ + (x * channelCount_);
            const float* alpha = channelCount_ > 3 ? &imageBuf_[pixStart + 3] : NULL;
            foreach (z, readChannels) {
                from_float(z, row.writable(z) + x, &imageBuf_[pixStart + z - 1],
                           alpha, r - x, channelCount_);
            }
        }
    }

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
