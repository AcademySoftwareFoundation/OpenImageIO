// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo demosaic algorithms

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace {

static const ustring algorithm_us("algorithm");
static const ustring pattern_us("pattern");

}  // namespace

template<class Rtype, class Atype, int size> class BayerDemosaicing {
public:
    struct Window {
        struct Row {
            ImageBuf::ConstIterator<Atype> iterator;
            Atype data[size];
        };

        std::vector<Row> rows;

        int col_mapping[size];

        int src_xbegin;
        int src_xend;
        int src_ybegin;
        int src_yend;
        int x;

        Window(int y, int xbegin, const ImageBuf& src)
        {
            const ImageSpec& spec = src.spec();
            src_xbegin            = spec.x;
            src_xend              = spec.x + spec.width;
            src_ybegin            = spec.y;
            src_yend              = spec.y + spec.height;
            x                     = xbegin;

            int central = size / 2;

            int skip = src_xbegin - xbegin + central;
            if (skip < 0)
                skip = 0;

            int xstart = xbegin - central + skip;

            for (int i = 0; i < size; i++) {
                col_mapping[i] = i;
            }

            for (int i = 0; i < size; i++) {
                int index  = i;
                int offset = i - central;

                if ((y + offset <= src_ybegin) || (y + offset > src_yend - 1)) {
                    index = central - offset;
                }

                int ystart = y - central + index;
                Row row
                    = { ImageBuf::ConstIterator<Atype>(src, xstart, ystart) };

                for (int j = skip; j < size; j++) {
                    row.data[j] = row.iterator[0];
                    row.iterator++;
                }

                for (int j = 0; j < skip; j++) {
                    row.data[j] = row.data[size - 1 - j];
                }

                rows.push_back(row);
            }
        }

        void update()
        {
            x++;

            int curr = col_mapping[0];
            for (int i = 0; i < size - 1; i++) {
                col_mapping[i] = col_mapping[i + 1];
            }
            col_mapping[size - 1] = curr;

            if (x + size / 2 < src_xend) {
                for (int i = 0; i < size; i++) {
                    Row& row       = rows[i];
                    row.data[curr] = row.iterator[0];
                    row.iterator++;
                }
            } else {
                int off = ((x + size / 2 - src_xend) + 1) * 2;
                off     = curr + size - off;

                for (int i = 0; i < size; i++) {
                    Row& row       = rows[i];
                    row.data[curr] = row.data[off];
                }
            }
        };

        Atype operator()(int row, int col)
        {
            int index = col_mapping[col];
            return rows[row].data[index];
        }
    };

    virtual void calc_red(Window& window, ImageBuf::Iterator<Rtype>& out,
                          int chbegin)
        = 0;
    virtual void calc_blue(Window& window, ImageBuf::Iterator<Rtype>& out,
                           int chbegin)
        = 0;
    virtual void calc_green_in_red(Window& window,
                                   ImageBuf::Iterator<Rtype>& out, int chbegin)
        = 0;
    virtual void calc_green_in_blue(Window& window,
                                    ImageBuf::Iterator<Rtype>& out, int chbegin)
        = 0;

    bool process(ImageBuf& dst, const ImageBuf& src,
                 const std::string& bayer_pattern, ROI roi, int nthreads)
    {
        int x_offset, y_offset;

        if (bayer_pattern == "RGGB") {
            x_offset = 0;
            y_offset = 0;
        } else if (bayer_pattern == "GRBG") {
            x_offset = 1;
            y_offset = 0;
        } else if (bayer_pattern == "GBBR") {
            x_offset = 0;
            y_offset = 1;
        } else if (bayer_pattern == "BGGR") {
            x_offset = 1;
            y_offset = 1;
        } else {
            dst.errorfmt("ImageBufAlgo::bayer_demosaic() invalid pattern");
            return false;
        }

        ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
            ImageBuf::Iterator<Rtype> it(dst, roi);

            for (int y = roi.ybegin; y < roi.yend; y++) {
                Window window(y, roi.xbegin, src);


                void (BayerDemosaicing::*calc_0)(Window(&),
                                                 ImageBuf::Iterator<Rtype>&,
                                                 int)
                    = nullptr;
                void (BayerDemosaicing::*calc_1)(Window(&),
                                                 ImageBuf::Iterator<Rtype>&,
                                                 int)
                    = nullptr;

                if ((roi.xbegin + x_offset) & 1) {
                    if ((y + y_offset) & 1) {
                        calc_0 = &BayerDemosaicing::calc_blue;
                        calc_1 = &BayerDemosaicing::calc_green_in_blue;
                    } else {
                        calc_0 = &BayerDemosaicing::calc_green_in_red;
                        calc_1 = &BayerDemosaicing::calc_red;
                    }
                } else {
                    if ((y + y_offset) & 1) {
                        calc_0 = &BayerDemosaicing::calc_green_in_blue;
                        calc_1 = &BayerDemosaicing::calc_blue;
                    } else {
                        calc_0 = &BayerDemosaicing::calc_red;
                        calc_1 = &BayerDemosaicing::calc_green_in_red;
                    }
                }

                int x = roi.xbegin;
                while (x < roi.xend - 1) {
                    if (x != roi.xbegin) {
                        window.update();
                    }

                    (this->*calc_0)(window, it, 0);
                    it++;
                    x++;

                    window.update();
                    (this->*calc_1)(window, it, 0);
                    it++;
                    x++;
                }

                if (x < roi.xend) {
                    window.update();
                    (this->*calc_0)(window, it, 0);
                    it++;
                    x++;
                }
            }
        });

        return true;
    };
};



template<class Rtype, class Atype, int size = 3>
class LinearBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, size> {
public:
    void calc_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
                  ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        out[chbegin + 0] = window(1, 1);

        out[chbegin + 1] = (window(0, 1) + window(2, 1) + window(1, 0)
                            + window(1, 2))
                           / (Rtype)4;

        out[chbegin + 2] = (window(0, 0) + window(0, 2) + window(2, 0)
                            + window(2, 2))
                           / (Rtype)4;
    }

    void calc_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
                   ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        out[chbegin + 0] = (window(0, 0) + window(0, 2) + window(2, 0)
                            + window(2, 2))
                           / (Rtype)4;

        out[chbegin + 1] = (window(0, 1) + window(2, 1) + window(1, 0)
                            + window(1, 2))
                           / (Rtype)4;

        out[chbegin + 2] = window(1, 1);
    }

    void calc_green_in_red(
        typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
        ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        out[chbegin + 0] = (window(1, 0) + window(1, 2)) / (Rtype)2;

        out[chbegin + 1] = window(1, 1);

        out[chbegin + 2] = (window(0, 1) + window(2, 1)) / (Rtype)2;
    }

    void calc_green_in_blue(
        typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
        ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        out[chbegin + 0] = (window(0, 1) + window(2, 1)) / (Rtype)2;

        out[chbegin + 1] = window(1, 1);

        out[chbegin + 2] = (window(1, 0) + window(1, 2)) / (Rtype)2;
    }
};

template<class Rtype, class Atype, int size = 5>
class MHCBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, size> {
private:
    inline void
    mix1(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
         Atype& out_mix1, Atype& out_mix2)
    {
        Atype tmp = window(0, 2) + window(4, 2) + window(2, 0) + window(2, 4);
        out_mix1
            = (8 * window(2, 2)
               + 4 * (window(1, 2) + window(3, 2) + window(2, 1) + window(2, 3))
               - 2 * tmp)
              / 16;
        out_mix2
            = (12 * window(2, 2)
               + 4 * (window(1, 1) + window(1, 3) + window(3, 1) + window(3, 3))
               - 3 * tmp)
              / 16;
    }

    inline void
    mix2(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
         Atype& out_mix1, Atype& out_mix2)
    {
        Atype tmp = window(1, 1) + window(1, 3) + window(3, 1) + window(3, 3);

        out_mix1 = (10 * window(2, 2) + 8 * (window(2, 1) + window(2, 3))
                    - 2 * (tmp + window(2, 0) + window(2, 4))
                    + 1 * (window(0, 2) + window(4, 2)))
                   / 16;
        out_mix2 = (10 * window(2, 2) + 8 * (window(1, 2) + window(3, 2))
                    - 2 * (tmp + window(0, 2) + window(4, 2))
                    + 1 * (window(2, 0) + window(2, 4)))
                   / 16;
    }

public:
    void calc_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
                  ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        Atype val1, val2;
        mix1(window, val1, val2);

        out[chbegin + 0] = window(2, 2);
        out[chbegin + 1] = val1;
        out[chbegin + 2] = val2;
    }

    void calc_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
                   ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        Atype val1, val2;
        mix1(window, val1, val2);

        out[chbegin + 0] = val2;
        out[chbegin + 1] = val1;
        out[chbegin + 2] = window(2, 2);
    }

    void calc_green_in_red(
        typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
        ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        Atype val1, val2;
        mix2(window, val1, val2);

        out[chbegin + 0] = val1;
        out[chbegin + 1] = window(2, 2);
        out[chbegin + 2] = val2;
    }

    void calc_green_in_blue(
        typename BayerDemosaicing<Rtype, Atype, size>::Window& window,
        ImageBuf::Iterator<Rtype>& out, int chbegin) override
    {
        Atype val1, val2;
        mix2(window, val1, val2);

        out[chbegin + 0] = val2;
        out[chbegin + 1] = window(2, 2);
        out[chbegin + 2] = val1;
    }
};



template<class Rtype, class Atype>
static bool
bayer_demosaic_linear_impl(ImageBuf& dst, const ImageBuf& src,
                           const std::string& bayer_pattern, ROI roi,
                           int nthreads)
{
    LinearBayerDemosaicing<Rtype, Atype> obj;
    return obj.process(dst, src, bayer_pattern, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
bayer_demosaic_MHC_impl(ImageBuf& dst, const ImageBuf& src,
                        const std::string& bayer_pattern, ROI roi, int nthreads)
{
    MHCBayerDemosaicing<Rtype, Atype> obj;
    return obj.process(dst, src, bayer_pattern, roi, nthreads);

    return true;
}


bool
ImageBufAlgo::bayer_demosaic(ImageBuf& dst, const ImageBuf& src, KWArgs options,
                             ROI roi, int nthreads)
{
    bool ok = false;
    pvt::LoggedTimer logtime("IBA::bayer_demosaic");

    std::string algorithm = "linear";
    std::string pattern   = "RGGB";

    for (auto&& pv : options) {
        if (pv.name() == algorithm_us) {
            if (pv.type() == TypeString) {
                algorithm = pv.get_string();
            } else {
                dst.errorfmt(
                    "ImageBufAlgo::bayer_demosaic() invalid algorithm");
            }
        } else if (pv.name() == pattern_us) {
            if (pv.type() == TypeString) {
                pattern = pv.get_string();
            } else {
                dst.errorfmt("ImageBufAlgo::bayer_demosaic() invalid pattern");
            }
        } else {
            dst.errorfmt("ImageBufAlgo::bayer_demosaic() unknown parameter {}",
                         pv.name());
        }
    }


    ROI dst_roi = roi;
    if (!dst_roi.defined()) {
        dst_roi = src.roi_full();
    }

    dst_roi.chbegin = 0;
    dst_roi.chend   = 2;

    ImageSpec dst_spec = src.spec();
    dst_spec.nchannels = 3;
    dst_spec.default_channel_names();
    dst_spec.channelformats.clear();
    dst_spec.alpha_channel = -1;
    dst_spec.z_channel     = -1;

    IBAprep(dst_roi, &dst, &src, nullptr, &dst_spec);



    if (algorithm == "linear") {
        OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_linear",
                                    bayer_demosaic_linear_impl,
                                    dst.spec().format, src.spec().format, dst,
                                    src, pattern, dst_roi, nthreads);
    } else if (algorithm == "MHC") {
        OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_MHC",
                                    bayer_demosaic_MHC_impl, dst.spec().format,
                                    src.spec().format, dst, src, pattern,
                                    dst_roi, nthreads);
    } else {
        dst.errorfmt("ImageBufAlgo::bayer_demosaic() invalid algorithm");
    }



    return true;
}

ImageBuf
ImageBufAlgo::bayer_demosaic(const ImageBuf& src, KWArgs options, ROI roi,
                             int nthreads)
{
    ImageBuf result;
    bool ok = bayer_demosaic(result, src, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::bayer_demosaic() error");
    return result;
}

OIIO_NAMESPACE_END
