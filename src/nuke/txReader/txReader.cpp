#include "txReader.h"

#include "OpenImageIO/imagecache.h"


/*
 * TODO:
 * - Populate metadata in txReader constructor
 * - Implement planar Reader API for Nuke 8 (should map better to TIFF/OIIO)
 */


namespace TxReaderNS {
    txReader::txReader(Read* read) : Reader(read), channelCount_(0) {
        txFmt_ = dynamic_cast<TxReaderFormat*>(iop->handler());
        if (!txFmt_) {
            iop->error("Failed to cast Read handler to TxReaderFormat");
            return;
        }

        oiioInput_ = ImageInput::open(filename());
        ImageSpec& spec = oiioInput_->spec();

        channelCount_ = spec.nchannels;
        set_info(spec.width, spec.height, channelCount_);

        // Populate mip level pulldown
        std::vector<std::string> mipLabels;
        bool found = true;
        std::stringstream buf;
        for (int mipLevel = 0; found; mipLevel++, buf(std::string()), buf.clear()) {
            buf << mipLevel << '\t' << mipLevel << " - " << spec.width << 'x' << spec.height;
            mipLabels.push_back(std::string(buf));
            found = oiioInput_->seek_subimage(0, mipLevel, spec);
        }

        txFmt_->setMipLabels(mipLabels);
    }

    txReader::~txReader() {
        if (oiioInput_)
            oiioInput_->close();
        delete oiioInput_;
        oiioInput_ = NULL;
    }

    void txReader::engine(int y, int x, int r, ChannelMask channels, Row& row) {
        int mipLevel = txFmt_->mipLevel();
        ImageSpec& spec = oiioInput_->spec();

        // Seek to the correct mip level
        if (mipLevel != oiioInput_->current_miplevel()) {
            if (!oiioInput_->seek_subimage(0, mipLevel, spec)){
                iop->internalError("Failed to seek to mip level %d", mipLevel);
                return;
            }
        }

        row.erase(channels);

//        if (mipLevel) {
//            // Reading a mip level smaller than the full format
//            if (spec.tile_width && spec.tile_height) {
//                // Tiled image
//                // y - (y % spec.tile_height);  // First scanline of tile containing y
//            }
//            else {
//
//            }
//        }
//        else {
//            // Reading the whole image
//        }
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
