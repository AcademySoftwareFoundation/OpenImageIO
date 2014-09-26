#ifndef TXREADER_H
#define TXREADER_H

#include "DDImage/Enumeration_KnobI.h"
#include "DDImage/Reader.h"
#include "DDImage/Row.h"

//#include "OpenImageIO/imagecache.h"
#include "OpenImageIO/imageio.h"


using namespace DD::Image;


namespace TxReaderNS {

    OIIO_NAMESPACE_USING


    static const char* const EMPTY[] = {NULL};

    class TxReaderFormat : public ReaderFormat {
//        friend class txReader;
        int mipLevel_;
        Knob* mipLevelKnob_;

    public:
        TxReaderFormat() : mipLevel_(0), mipLevelKnob_(NULL) { }

        void knobs(Knob_Callback cb) {
            mipLevelKnob_ = Enumeration_knob(cb, &mipLevel_, EMPTY,
                                             "tx_mip_level", "mip level");
        }

        void append(Hash& hash) { hash.append(mipLevel_); }

        inline int mipLevel() { return mipLevel_; }

        void setMipLabels(std::vector<std::string> items) {
            mipLevelKnob_->enumerationKnob()->menu(items);
        }

        const char* help() { return "Tiled, mipmapped texture format"; }
    };


    class txReader : public Reader {
        int channelCount_;
        TxReaderFormat* txFmt_;
        ImageInput* oiioInput_;
        MetaData::Bundle meta_;

        static const Description d;

    public:
        txReader(Read* iop);
        virtual ~txReader();

        void engine(int y, int x, int r, ChannelMask channels, Row& row);

        const MetaData::Bundle& fetchMetaData(const char* key) { return meta_; }
    };

}  // ~TxReaderNS

#endif  // TXREADER_H
