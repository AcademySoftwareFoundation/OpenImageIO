#include "txReader.h"


/*
 * TODO:
 * - Handle mip orientations ( ImageSpec.get_int_attribute("Orientation", default); )
 * - Populate metadata in txReader constructor
 * - Implement planar Reader API for Nuke 8 (should map better to TIFF/OIIO)
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

        // Populate mip level pulldown
        std::vector<std::string> mipLabels;
        bool found = true;
        std::ostringstream buf;
        ImageSpec mipSpec(baseSpec);
        for (int mipLevel = 0; found; mipLevel++) {
            // e.g. "0\t0 - 1920x1080"
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
        // Seek to the correct mip level
        if (lastMipLevel_ != txFmt_->mipLevel()) {
            printf("Seeking to mip level %d\n", txFmt_->mipLevel());
            ImageSpec seekSpec;
            if (!oiioInput_->seek_subimage(0, txFmt_->mipLevel(), seekSpec)) {
                iop->internalError("Failed to seek to mip level %d",
                                   txFmt_->mipLevel());
                return;
            }

            // Make sure the current mip level has the same number of channels we
            // were expecting
            if (seekSpec.nchannels != channelCount_) {
                iop->internalError("txReader does not support mip levels with "
                                   "different channel counts");
                return;
            }

            lastMipLevel_ = txFmt_->mipLevel();
            haveImage_ = false;
        }
    }

    void txReader::engine(int y, int x, int r, ChannelMask channels, Row& row) {
        if (aborted()) {
            row.erase(channels);
            return;
        }

        const ImageSpec& spec = oiioInput_->spec();

        if (!haveImage_) {
            Guard g(syncLock_);
            if (!haveImage_) {
                imageBuf_.resize(spec.width * spec.height * channelCount_);
                OIIO::attribute("threads", (int)Thread::numThreads / 2);
                oiioInput_->read_image(&imageBuf_[0]);  // Channel-interleaved
                haveImage_ = true;
            }
        }

        if (aborted()) {
            row.erase(channels);
            return;
        }

        if (lastMipLevel_) {
            const int bufY = (height() - y - 1) * spec.height / height();
            const int bufX = x * spec.width / width();
            const int bufR = r * spec.width / width();
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
