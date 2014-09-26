#include "txReader.h"


/*
 * TODO:
 * - Handle mip orientations ( ImageSpec.get_int_attribute("Orientation", default); )
 * - Populate metadata in txReader constructor
 * - Look into using the planar Reader API in Nuke 8, which may map better to
 *      TIFF/OIIO.
 * - Look into making use of DD::Image::Memory/MemoryHolder to make Nuke aware
 *      of buffer allocations.
 */


namespace TxReaderNS {
    txReader::txReader(Read* read) : Reader(read),
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

        channelCount_ = baseSpec.nchannels;
        set_info(baseSpec.width, baseSpec.height, channelCount_);

        // Populate mip level pulldown with labels in the form:
        //      "MIPLEVEL\tMIPLEVEL - WxH" (e.g. "0\t0 - 1920x1080")
        // The knob will split these values on tab characters, and only store
        // the first part (i.e. the index) when the knob is serialized. This
        // way, we don't have to maintain a separate knob for the mip index.
        std::vector<std::string> mipLabels;
        bool found = true;
        std::ostringstream buf;
        ImageSpec mipSpec(baseSpec);
        for (int mipLevel = 0; found; mipLevel++) {
            buf << mipLevel << '\t' << mipLevel << " - " << mipSpec.width << 'x' << mipSpec.height;
            mipLabels.push_back(buf.str());
            found = oiioInput_->seek_subimage(0, mipLevel + 1, mipSpec);
            if (found) {
                buf.str(std::string());
                buf.clear();
            }
        }

        txFmt_->setMipLabels(mipLabels);
    }

    txReader::~txReader() {
        if (oiioInput_)
            oiioInput_->close();
        delete oiioInput_;
        oiioInput_ = NULL;
    }

    void txReader::open() {
//        printf("open : thread ID = %d\n", Thread::thisIndex());
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
            // Because of the frequency with which Readers are created and
            // destroyed, I don't know if this test is really going to save any
            // reallocations. We could just say screw it and give the buffer
            // enough room for mip level 0 right off the bat.
            if (needSize > imageBuf_.size())
                imageBuf_.resize(needSize);
            oiioInput_->read_image(&imageBuf_[0]);  // Channel-interleaved
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

        const int mipW = oiioInput_->spec().width;
        const int mipH = oiioInput_->spec().height;

        if (lastMipLevel_) {
            const int bufY = (height() - y - 1) * mipH / height();
            const int bufX = x * mipW / width();
            const int bufR = r * mipH / width();
            // TODO
            row.erase(channels);
        }
        else {
            const int bufY = height() - y - 1;
            const float* alpha = channelCount_ > 3 ? &imageBuf_[3] : NULL;
            const int baseIndex = bufY * width() * channelCount_ + (x * channelCount_);
            // TODO: Make sure this channel logic is sound
            foreach(z, channels) {
                from_float(z, row.writable(z) + x, &imageBuf_[baseIndex + z - 1],
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
