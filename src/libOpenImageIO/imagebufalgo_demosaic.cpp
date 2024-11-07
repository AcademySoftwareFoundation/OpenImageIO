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

static const ustring pattern_us("pattern");
static const ustring algorithm_us("algorithm");
static const ustring layout_us("layout");
static const ustring white_balance_us("white_balance");

}  // namespace

template<class Rtype, class Atype, int size> class BayerDemosaicing {
protected:
    /// Sliding window, holds `size*size` pixel values to filter over.
    /// The `size` is expected to be an odd number, so the pixel being
    /// processed is always in the middle.
    struct Window {
        /// A single row of the sliding window, holds `size` pixel values and
        /// the source iterator.
        struct Row {
            ImageBuf::ConstIterator<Atype> iterator;
            float white_balance[2];
            int x_offset;
            float data[size];

            float fetch()
            {
                float result = iterator[0] * white_balance[x_offset];
                iterator++;
                x_offset = 1 - x_offset;
                return result;
            }
        };

        std::vector<Row> rows;

        /// Column mapping. Insead of shifting every pixel value as the sliding
        /// window advances, we just rotate the indices in this table.
        int column_mapping[size];

        int src_xbegin;
        int src_xend;
        int src_ybegin;
        int src_yend;
        int x;

        Window(int y, int xbegin, const ImageBuf& src, int x_offset,
               int y_offset, const float white_balance[4])
        {
            assert(size >= 3);
            assert(size % 2 == 1);

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
                column_mapping[i] = i;
            }

            for (int i = 0; i < size; i++) {
                int ystart = y - central + i;
                if (ystart < src_ybegin) {
                    ystart = src_ybegin + (src_ybegin - ystart) % 2;
                } else if (ystart > src_yend - 1) {
                    ystart = src_yend - 1 - (ystart - src_yend + 1) % 2;
                }

                int x_off = (xstart + x_offset) % 2;
                int y_off = ((ystart + y_offset) % 2) * 2;

                Row row = { ImageBuf::ConstIterator<Atype>(src, xstart, ystart),
                            { white_balance[y_off], white_balance[y_off + 1] },
                            x_off };

                for (int j = skip; j < size; j++) {
                    row.data[j] = row.fetch();
                }

                for (int j = 0; j < skip; j++) {
                    row.data[j] = row.data[skip + (skip - j) % 2];
                }

                rows.push_back(row);
            }
        }

        /// Advances the sliding window to the right by one pixel. Rotates the
        /// indices in the `column_mapping`. Fetches the rightmost column
        /// from the source, if available. If we have reached the right border
        /// and there are no more pixels to fetch, duplicates the values we
        /// have fetched 2 steps ago, as the Bayer pattern repeats every 2
        /// columns.
        void update()
        {
            x++;

            int curr = column_mapping[0];
            for (int i = 0; i < size - 1; i++) {
                column_mapping[i] = column_mapping[i + 1];
            }
            column_mapping[size - 1] = curr;

            if (x + size / 2 < src_xend) {
                for (int i = 0; i < size; i++) {
                    Row& row       = rows[i];
                    row.data[curr] = row.fetch();
                }
            } else {
                int src = column_mapping[size - 3];
                for (int i = 0; i < size; i++) {
                    Row& row       = rows[i];
                    row.data[curr] = row.data[src];
                }
            }
        };

        float operator()(int row, int col)
        {
            int index = column_mapping[col];
            return rows[row].data[index];
        }
    };

    typedef void (*Decoder)(Window& window, ImageBuf::Iterator<Rtype>& out,
                            int chbegin);

    /// The decoder function pointers in RGGB order.
    /// All subclasses must initialize this table.
    Decoder decoders[2][2] = { { nullptr, nullptr }, { nullptr, nullptr } };

public:
    bool process(ImageBuf& dst, const ImageBuf& src, const std::string& layout,
                 const float white_balance[4], ROI roi, int nthreads)
    {
        int x_offset, y_offset;

        if (layout == "RGGB") {
            x_offset = 0;
            y_offset = 0;
        } else if (layout == "GRBG") {
            x_offset = 1;
            y_offset = 0;
        } else if (layout == "GBRG") {
            x_offset = 0;
            y_offset = 1;
        } else if (layout == "BGGR") {
            x_offset = 1;
            y_offset = 1;
        } else {
            dst.errorfmt("BayerDemosaicing::process() invalid Bayer layout");
            return false;
        }

        ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
            ImageBuf::Iterator<Rtype> it(dst, roi);

            for (int y = roi.ybegin; y < roi.yend; y++) {
                typename BayerDemosaicing<Rtype, Atype, size>::Window window(
                    y, roi.xbegin, src, x_offset, y_offset, white_balance);

                int r          = (y_offset + y) % 2;
                Decoder calc_0 = decoders[r][(x_offset + roi.xbegin + 0) % 2];
                Decoder calc_1 = decoders[r][(x_offset + roi.xbegin + 1) % 2];

                assert(calc_0 != nullptr);
                assert(calc_1 != nullptr);


                int x = roi.xbegin;

                // Process the leftmost pixel first.
                if (x < roi.xend) {
                    (calc_0)(window, it, 0);
                    it++;
                    x++;
                }

                // Now, process two pixels at a time.
                while (x < roi.xend - 1) {
                    window.update();
                    (calc_1)(window, it, 0);
                    it++;
                    x++;

                    window.update();
                    (calc_0)(window, it, 0);
                    it++;
                    x++;
                }

                // Process the rightmost pixel if needed.
                if (x < roi.xend) {
                    window.update();
                    (calc_1)(window, it, 0);
                    it++;
                }
            }
        });

        return true;
    };
};


template<class Rtype, class Atype, int size = 3>
class LinearBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, size> {
private:
    static void
    calc_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
             ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        out[chbegin + 0] = w(1, 1);
        out[chbegin + 1] = (w(0, 1) + w(2, 1) + w(1, 0) + w(1, 2)) / 4.0f;
        out[chbegin + 2] = (w(0, 0) + w(0, 2) + w(2, 0) + w(2, 2)) / 4.0f;
    }

    static void
    calc_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
              ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        out[chbegin + 0] = (w(0, 0) + w(0, 2) + w(2, 0) + w(2, 2)) / 4.0f;
        out[chbegin + 1] = (w(0, 1) + w(2, 1) + w(1, 0) + w(1, 2)) / 4.0f;
        out[chbegin + 2] = w(1, 1);
    }

    static void
    calc_green_in_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
                      ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        out[chbegin + 0] = (w(1, 0) + w(1, 2)) / 2.0f;
        out[chbegin + 1] = w(1, 1);
        out[chbegin + 2] = (w(0, 1) + w(2, 1)) / 2.0f;
    }

    static void
    calc_green_in_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
                       ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        out[chbegin + 0] = (w(0, 1) + w(2, 1)) / 2.0f;
        out[chbegin + 1] = w(1, 1);
        out[chbegin + 2] = (w(1, 0) + w(1, 2)) / 2.0f;
    }

public:
    LinearBayerDemosaicing()
    {
        BayerDemosaicing<Rtype, Atype, size>::decoders[0][0] = calc_red;
        BayerDemosaicing<Rtype, Atype, size>::decoders[0][1] = calc_green_in_red;
        BayerDemosaicing<Rtype, Atype, size>::decoders[1][0]
            = calc_green_in_blue;
        BayerDemosaicing<Rtype, Atype, size>::decoders[1][1] = calc_blue;
    };
};

template<class Rtype, class Atype, int size = 5>
class MHCBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, size> {
private:
    inline static void
    mix1(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
         float& out_mix1, float& out_mix2)
    {
        float tmp = w(0, 2) + w(4, 2) + w(2, 0) + w(2, 4);
        out_mix1  = (8.0f * w(2, 2)
                    + 4.0f * (w(1, 2) + w(3, 2) + w(2, 1) + w(2, 3))
                    - 2.0f * tmp)
                   / 16.0f;
        out_mix2 = (12.0f * w(2, 2)
                    + 4.0f * (w(1, 1) + w(1, 3) + w(3, 1) + w(3, 3))
                    - 3.0f * tmp)
                   / 16.0f;
    }

    inline static void
    mix2(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
         float& out_mix1, float& out_mix2)
    {
        float tmp = w(1, 1) + w(1, 3) + w(3, 1) + w(3, 3);

        out_mix1 = (10.0f * w(2, 2) + 8.0f * (w(2, 1) + w(2, 3))
                    - 2.0f * (tmp + w(2, 0) + w(2, 4))
                    + 1.0f * (w(0, 2) + w(4, 2)))
                   / 16.0f;
        out_mix2 = (10.0f * w(2, 2) + 8.0f * (w(1, 2) + w(3, 2))
                    - 2.0f * (tmp + w(0, 2) + w(4, 2))
                    + 1.0f * (w(2, 0) + w(2, 4)))
                   / 16.0f;
    }

    static void
    calc_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
             ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        float val1, val2;
        mix1(w, val1, val2);

        out[chbegin + 0] = w(2, 2);
        out[chbegin + 1] = val1;
        out[chbegin + 2] = val2;
    }

    static void
    calc_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
              ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        float val1, val2;
        mix1(w, val1, val2);

        out[chbegin + 0] = val2;
        out[chbegin + 1] = val1;
        out[chbegin + 2] = w(2, 2);
    }

    static void
    calc_green_in_red(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
                      ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        float val1, val2;
        mix2(w, val1, val2);

        out[chbegin + 0] = val1;
        out[chbegin + 1] = w(2, 2);
        out[chbegin + 2] = val2;
    }

    static void
    calc_green_in_blue(typename BayerDemosaicing<Rtype, Atype, size>::Window& w,
                       ImageBuf::Iterator<Rtype>& out, int chbegin)
    {
        float val1, val2;
        mix2(w, val1, val2);

        out[chbegin + 0] = val2;
        out[chbegin + 1] = w(2, 2);
        out[chbegin + 2] = val1;
    }

public:
    MHCBayerDemosaicing()
    {
        BayerDemosaicing<Rtype, Atype, size>::decoders[0][0] = calc_red;
        BayerDemosaicing<Rtype, Atype, size>::decoders[0][1] = calc_green_in_red;
        BayerDemosaicing<Rtype, Atype, size>::decoders[1][0]
            = calc_green_in_blue;
        BayerDemosaicing<Rtype, Atype, size>::decoders[1][1] = calc_blue;
    };
};


template<class Rtype, class Atype>
static bool
bayer_demosaic_linear_impl(ImageBuf& dst, const ImageBuf& src,
                           const std::string& bayer_pattern,
                           const float white_balance[4], ROI roi, int nthreads)
{
    LinearBayerDemosaicing<Rtype, Atype> obj;
    return obj.process(dst, src, bayer_pattern, white_balance, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
bayer_demosaic_MHC_impl(ImageBuf& dst, const ImageBuf& src,
                        const std::string& bayer_pattern,
                        const float white_balance[4], ROI roi, int nthreads)
{
    MHCBayerDemosaicing<Rtype, Atype> obj;
    return obj.process(dst, src, bayer_pattern, white_balance, roi, nthreads);

    return true;
}


bool
ImageBufAlgo::demosaic(ImageBuf& dst, const ImageBuf& src, KWArgs options,
                       ROI roi, int nthreads)
{
    bool ok = false;
    pvt::LoggedTimer logtime("IBA::demosaic");

    std::string pattern;
    std::string algorithm;
    std::string layout;
    float white_balance_RGGB[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    for (auto&& pv : options) {
        if (pv.name() == pattern_us) {
            if (pv.type() == TypeString) {
                pattern = pv.get_string();
            } else {
                dst.errorfmt("ImageBufAlgo::demosaic() invalid pattern");
            }
        } else if (pv.name() == algorithm_us) {
            if (pv.type() == TypeString) {
                algorithm = pv.get_string();
            } else {
                dst.errorfmt("ImageBufAlgo::demosaic() invalid algorithm");
            }
        } else if (pv.name() == layout_us) {
            if (pv.type() == TypeString) {
                layout = pv.get_string();
            } else {
                dst.errorfmt("ImageBufAlgo::demosaic() invalid layout");
            }
        } else if (pv.name() == white_balance_us) {
            if (pv.type() == TypeFloat && pv.nvalues() == 4) {
                // The order in the options is always (R,G1,B,G2)
                white_balance_RGGB[0] = pv.get_float_indexed(0);
                white_balance_RGGB[1] = pv.get_float_indexed(1);
                white_balance_RGGB[2] = pv.get_float_indexed(3);
                white_balance_RGGB[3] = pv.get_float_indexed(2);

                if (white_balance_RGGB[2] == 0)
                    white_balance_RGGB[2] = white_balance_RGGB[1];
            } else if (pv.type() == TypeFloat && pv.nvalues() == 3) {
                // The order in the options is always (R,G,B)
                white_balance_RGGB[0] = pv.get_float_indexed(0);
                white_balance_RGGB[1] = pv.get_float_indexed(1);
                white_balance_RGGB[2] = white_balance_RGGB[1];
                white_balance_RGGB[3] = pv.get_float_indexed(2);
            } else {
                dst.errorfmt("ImageBufAlgo::demosaic() invalid white balance");
            }
        } else {
            dst.errorfmt("ImageBufAlgo::demosaic() unknown parameter {}",
                         pv.name());
        }
    }


    ROI dst_roi = roi;
    if (!dst_roi.defined()) {
        dst_roi = src.roi();
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

    if (pattern.length() == 0)
        pattern = "bayer";

    if (pattern == "bayer") {
        if (algorithm.length() == 0) {
            algorithm = "linear";
        }

        if (layout.length() == 0) {
            layout = "RGGB";
        }

        if (algorithm == "linear") {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_linear",
                                        bayer_demosaic_linear_impl,
                                        dst.spec().format, src.spec().format,
                                        dst, src, layout, white_balance_RGGB,
                                        dst_roi, nthreads);
        } else if (algorithm == "MHC") {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_MHC",
                                        bayer_demosaic_MHC_impl,
                                        dst.spec().format, src.spec().format,
                                        dst, src, layout, white_balance_RGGB,
                                        dst_roi, nthreads);
        } else {
            dst.errorfmt("ImageBufAlgo::demosaic() invalid algorithm");
        }
    } else {
        dst.errorfmt("ImageBufAlgo::demosaic() invalid pattern");
    }

    return true;
}

ImageBuf
ImageBufAlgo::demosaic(const ImageBuf& src, KWArgs options, ROI roi,
                       int nthreads)
{
    ImageBuf result;
    bool ok = demosaic(result, src, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::demosaic() error");
    return result;
}

OIIO_NAMESPACE_END
