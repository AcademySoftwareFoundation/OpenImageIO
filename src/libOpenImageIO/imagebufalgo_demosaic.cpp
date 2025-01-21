// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo demosaic algorithms

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#include "imagebufalgo_demosaic_prv.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace {

static const ustring pattern_us("pattern");
static const ustring algorithm_us("algorithm");
static const ustring layout_us("layout");
static const ustring white_balance_us("white_balance");

}  // namespace

namespace ImageBufAlgo {

template<class Rtype, class Atype, int pattern_size, int window_size,
         const size_t channel_map[pattern_size][pattern_size]>
class DemosaicingBase {
protected:
    /// Sliding window, holds `size*size` pixel values to filter over.
    /// The `size` is expected to be an odd number, so the pixel being
    /// processed is always in the middle.
    struct Window {
        /// A single row of the sliding window, holds `size` pixel values and
        /// the source iterator.
        struct Row {
            ImageBuf::ConstIterator<Atype> iterator;

            int x_offset;
            const int y_offset;
            const float (&white_balance_values)[4];

            float data[window_size];

            float fetch()
            {
                float white_balance
                    = white_balance_values[channel_map[y_offset][x_offset]];
                float result = iterator[0] * white_balance;

                if (iterator.x() == iterator.range().xend - 1) {
                    // When we have reached the rightmost pixel, jump back by
                    // `pattern_size` pixels to re-fetch the last available
                    // column of the pixels with the required channel layout.
                    iterator.pos(iterator.x() - pattern_size + 1, iterator.y());
                } else {
                    iterator++;
                }

                x_offset++;
                if (x_offset == pattern_size)
                    x_offset = 0;
                return result;
            }
        };

        std::vector<Row> rows;

        /// Column mapping. Insead of shifting every pixel value as the sliding
        /// window advances, we just rotate the indices in this table.
        int column_mapping[window_size];

        int src_xbegin;
        int src_xend;
        int src_ybegin;
        int src_yend;

        Window(int y, int xbegin, const ImageBuf& src, int x_offset,
               int y_offset, const float (&white_balance_values)[4])
        {
            OIIO_DASSERT(window_size >= 3);
            OIIO_DASSERT(window_size % 2 == 1);

            const ImageSpec& spec = src.spec();
            src_xbegin            = spec.x;
            src_xend              = spec.x + spec.width;
            src_ybegin            = spec.y;
            src_yend              = spec.y + spec.height;

            int central = window_size / 2;

            int skip = src_xbegin - xbegin + central;
            if (skip < 0)
                skip = 0;

            int xstart = xbegin - central + skip;

            for (int i = 0; i < window_size; i++) {
                column_mapping[i] = i;
            }

            for (int i = 0; i < window_size; i++) {
                int ystart = y - central + i;
                while (ystart < src_ybegin) {
                    ystart += pattern_size;
                }
                while (ystart > src_yend - 1) {
                    ystart -= pattern_size;
                }

                int x_off = (xstart + x_offset) % pattern_size;
                int y_off = (ystart + y_offset) % pattern_size;

                Row row = { ImageBuf::ConstIterator<Atype>(src, xstart, ystart),
                            x_off, y_off, white_balance_values };

                // Fill the window with the values needed to process the first
                // (leftmost) pixel. First fetch the pixels which are directly
                // available in the image. We may need to skip a few columns,
                // as the image doesn't have any pixels to the left of the first
                // pixel of the row.
                for (int j = skip; j < window_size; j++) {
                    row.data[j] = row.fetch();
                }

                // Now fill in the columns we had skipped. First, check if the
                // rows we have already filled in have one with the same channel
                // layout we need. If so, just copy the values. Otherwise
                // calculate which column of the image would have such layout
                // and read the pixels.
                for (int j = 0; j < skip; j++) {
                    int k = j - skip;
                    while (k < 0)
                        k += pattern_size;

                    if (k + skip < window_size) {
                        row.data[j] = row.data[k + skip];
                    } else {
                        float v;
                        src.getpixel(xstart + k, ystart, 0, &v, 1);
                        int x_off2  = (x_off + k) % pattern_size;
                        int chan    = channel_map[y_off][x_off2];
                        row.data[j] = v * white_balance_values[chan];
                    }
                }

                rows.push_back(row);
            }
        }

        /// Advances the sliding window to the right by one pixel. Rotates the
        /// indices in the `column_mapping`.
        void update()
        {
            int curr = column_mapping[0];
            for (int i = 0; i < window_size - 1; i++) {
                column_mapping[i] = column_mapping[i + 1];
            }
            column_mapping[window_size - 1] = curr;

            for (int i = 0; i < window_size; i++) {
                Row& row       = rows[i];
                row.data[curr] = row.fetch();
            }
        };

        float operator()(int row, int col)
        {
            int index = column_mapping[col];
            return rows[row].data[index];
        }
    };

    struct Context {
        Window& window;
        ImageBuf::Iterator<Rtype>& out;
        int chbegin;
        int skip;
        int count;
    };

    /// Check the boundaries and process the pixel. We only need to check the
    /// boundaries for the first and the last few pixels of each line. As soon
    /// as we have reached the pixel aligned with the default layout, we can
    /// process the full stride without needing to check the boundaries
    /// (2 pixels for Bayer and 6 pixels for XTrans).
    template<bool check, typename Func>
    inline static bool check_and_decode(Context& context, const Func& func)
    {
        if constexpr (check) {
            if (context.skip > 0) {
                context.skip--;
            } else if (context.count == 0) {
                return true;
            } else {
                func();
                context.out++;
                context.count--;
                context.window.update();
            }
            return false;
        }

        func();
        context.out++;
        context.count--;
        context.window.update();
        return false;
    }

    /// All subclasses must initialize this table.
    typedef void (*Decoder)(Context& context);
    Decoder fast_decoders[pattern_size];
    Decoder slow_decoders[pattern_size];

    int x_offset = 0;
    int y_offset = 0;

    std::string error;

public:
    bool process(ImageBuf& dst, const ImageBuf& src,
                 const float (&white_balance)[4], ROI roi, int nthreads)
    {
        if (error.length() > 0) {
            dst.errorfmt("Demosaic::process() {}", error);
            return false;
        }

        ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
            ImageBuf::Iterator<Rtype> it(dst, roi);

            for (int y = roi.ybegin; y < roi.yend; y++) {
                typename DemosaicingBase<Rtype, Atype, pattern_size,
                                         window_size, channel_map>::Window
                    window(y, roi.xbegin, src, x_offset, y_offset,
                           white_balance);

                int r = (y_offset + y) % pattern_size;

                Decoder& fast_decoder = fast_decoders[r];
                Decoder& slow_decoder = slow_decoders[r];

                int skip        = (x_offset + roi.xbegin) % pattern_size;
                int count       = roi.width();
                Context context = { window, it, roi.chbegin, skip, count };

                if (skip > 0) {
                    slow_decoder(context);
                }

                size_t cc = context.count / pattern_size;
                for (size_t i = 0; i < cc; i++) {
                    fast_decoder(context);
                }

                slow_decoder(context);
            }
        });

        return true;
    };

    inline static size_t channel_at_offset(int x_offset, int y_offset)
    {
        return channel_map[y_offset % pattern_size][x_offset % pattern_size];
    }

    static void layout_from_offset(int x_offset, int y_offset,
                                   std::string& layout, bool whitespaces)
    {
        const char* channels = "RGBG";

        size_t length = pattern_size * pattern_size;
        if (whitespaces)
            length += pattern_size - 1;

        layout.resize(length);

        size_t i = 0;
        for (size_t y = 0; y < pattern_size; y++) {
            for (size_t x = 0; x < pattern_size; x++) {
                int c = channel_at_offset(x + x_offset, y + y_offset);
                OIIO_DASSERT(c < 4);

                layout[i] = channels[c];
                i++;
            }
            if (whitespaces) {
                layout[i] = ' ';
                i++;
            }
        }
    }

    bool init_offsets(const std::string& layout)
    {
        if (layout.length() == 0) {
            x_offset = 0;
            y_offset = 0;
            return true;
        }

        std::string stripped_layout = layout;
        if (layout.size() == pattern_size * pattern_size + pattern_size - 1) {
            stripped_layout.erase(std::remove(stripped_layout.begin(),
                                              stripped_layout.end(), ' '),
                                  stripped_layout.end());
        }

        std::string current_layout;

        for (size_t y = 0; y < pattern_size; y++) {
            for (size_t x = 0; x < pattern_size; x++) {
                layout_from_offset(x, y, current_layout, false);
                if (stripped_layout == current_layout) {
                    x_offset = x;
                    y_offset = y;
                    return true;
                }
            }
        }

        x_offset = 0;
        y_offset = 0;
        error    = "unrecognised layout \"" + layout + "\"";
        return false;
    }

    DemosaicingBase(const std::string& layout) { this->init_offsets(layout); }
};

constexpr size_t bayer_channel_map[2][2] = {
    { 0, 1 },  // RG
    { 3, 2 }   // GB
};

template<class Rtype, class Atype, int window_size>
class BayerDemosaicing
    : public DemosaicingBase<Rtype, Atype, 2, window_size, bayer_channel_map> {
public:
    using DemosaicingBase<Rtype, Atype, 2, window_size,
                          bayer_channel_map>::DemosaicingBase;
};

template<class Rtype, class Atype>
class LinearBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, 3> {
private:
    using Window  = typename LinearBayerDemosaicing<Rtype, Atype>::Window;
    using Context = typename LinearBayerDemosaicing<Rtype, Atype>::Context;

    template<bool check> static void calc_RG(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                         ch]() {
                c.out[ch + 0] = w(1, 1);
                c.out[ch + 1] = (w(0, 1) + w(2, 1) + w(1, 0) + w(1, 2)) / 4.0f;
                c.out[ch + 2] = (w(0, 0) + w(0, 2) + w(2, 0) + w(2, 2)) / 4.0f;
            }))
            return;

        if (LinearBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                         ch]() {
                c.out[ch + 0] = (w(1, 0) + w(1, 2)) / 2.0f;
                c.out[ch + 1] = w(1, 1);
                c.out[ch + 2] = (w(0, 1) + w(2, 1)) / 2.0f;
            }))
            return;
    }

    template<bool check> static void calc_GB(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                         ch]() {
                c.out[ch + 0] = (w(0, 1) + w(2, 1)) / 2.0f;
                c.out[ch + 1] = w(1, 1);
                c.out[ch + 2] = (w(1, 0) + w(1, 2)) / 2.0f;
            }))
            return;

        if (LinearBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                         ch]() {
                c.out[ch + 0] = (w(0, 0) + w(0, 2) + w(2, 0) + w(2, 2)) / 4.0f;
                c.out[ch + 1] = (w(0, 1) + w(2, 1) + w(1, 0) + w(1, 2)) / 4.0f;
                c.out[ch + 2] = w(1, 1);
            }))
            return;
    }

public:
    LinearBayerDemosaicing(const std::string& layout)
        : BayerDemosaicing<Rtype, Atype, 3>(layout)
    {
        this->fast_decoders[0] = calc_RG<false>;
        this->fast_decoders[1] = calc_GB<false>;

        this->slow_decoders[0] = calc_RG<true>;
        this->slow_decoders[1] = calc_GB<true>;
    };
};

template<class Rtype, class Atype>
class MHCBayerDemosaicing : public BayerDemosaicing<Rtype, Atype, 5> {
private:
    using Window = typename MHCBayerDemosaicing<Rtype, Atype>::Window;

    inline static void mix1(Window& w, float& out_mix1, float& out_mix2)
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

    inline static void mix2(Window& w, float& out_mix1, float& out_mix2)
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

    using Context = typename MHCBayerDemosaicing<Rtype, Atype>::Context;


    template<bool check> static void calc_RG(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (MHCBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                      ch]() {
                float val1, val2;
                mix1(w, val1, val2);

                c.out[ch + 0] = w(2, 2);
                c.out[ch + 1] = val1;
                c.out[ch + 2] = val2;
            }))
            return;

        if (MHCBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                      ch]() {
                float val1, val2;
                mix2(w, val1, val2);

                c.out[ch + 0] = val1;
                c.out[ch + 1] = w(2, 2);
                c.out[ch + 2] = val2;
            }))
            return;
    }

    template<bool check> static void calc_GB(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (MHCBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                      ch]() {
                float val1, val2;
                mix2(w, val1, val2);

                c.out[ch + 0] = val2;
                c.out[ch + 1] = w(2, 2);
                c.out[ch + 2] = val1;
            }))
            return;

        if (MHCBayerDemosaicing::template check_and_decode<check>(c, [&c, &w,
                                                                      ch]() {
                float val1, val2;
                mix1(w, val1, val2);

                c.out[ch + 0] = val2;
                c.out[ch + 1] = val1;
                c.out[ch + 2] = w(2, 2);
            }))
            return;
    }

public:
    MHCBayerDemosaicing(const std::string& layout)
        : BayerDemosaicing<Rtype, Atype, 5>(layout)
    {
        this->fast_decoders[0] = calc_RG<false>;
        this->fast_decoders[1] = calc_GB<false>;

        this->slow_decoders[0] = calc_RG<true>;
        this->slow_decoders[1] = calc_GB<true>;
    };
};

constexpr size_t xtrans_channel_map[6][6] = {
    { 1, 0, 2, 1, 2, 0 },  // GRBGBR
    { 2, 1, 1, 0, 1, 1 },  // BGGRGG
    { 0, 1, 1, 2, 1, 1 },  // RGGBGG
    { 1, 2, 0, 1, 0, 2 },  // GBRGRB
    { 0, 1, 1, 2, 1, 1 },  // RGGBGG
    { 2, 1, 1, 0, 1, 1 },  // BGGRGG
};

template<class Rtype, class Atype, int window_size>
class XTransDemosaicing
    : public DemosaicingBase<Rtype, Atype, 6, window_size, xtrans_channel_map> {
public:
    using DemosaicingBase<Rtype, Atype, 6, window_size,
                          xtrans_channel_map>::DemosaicingBase;
};

template<class Rtype, class Atype>
class LinearXTransDemosaicing : public XTransDemosaicing<Rtype, Atype, 5> {
private:
    using Context = typename LinearXTransDemosaicing<Rtype, Atype>::Context;

    // ..b..
    // a.X.d
    // ..c..
    inline static float cross(float a, float b, float c, float d)
    {
        return (a + d + (b + c) * 2.0) / 6.0;
    }

    // ...b.
    // .aX..
    // ...c.
    inline static float triangle(float a, float b, float c)
    {
        return (a + (b + c) * M_SQRT1_2) / (1.0 + M_SQRT1_2 + M_SQRT1_2);
    }

    // ..bd.
    // .aX..
    // ..ce.
    inline static float pentagon(float a, float b, float c, float d, float e)
    {
        return (a + b + c + (d + e) * M_SQRT1_2)
               / (3.0 + M_SQRT1_2 + M_SQRT1_2);
    }

    // ...b.
    // .aX..
    // ....d
    // ..c..
    inline static float square(float a, float b, float c, float d)
    {
        return (a + b * M_SQRT1_2 + c * 0.5 + d / sqrt(5.0))
               / (1.5 + M_SQRT1_2 + 1.0 / sqrt(5.0));
    }

    template<bool check> inline static bool calc_GRB_bgg(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // ggrgg
                    // ggbgg
                    // brGrb
                    // ggbgg
                    // ggrgg
                    c.out[ch + 0] = cross(w(0, 2), w(2, 1), w(2, 3), w(4, 2));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = cross(w(2, 0), w(1, 2), w(3, 2), w(2, 4));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // grggb
                    // gbggr
                    // rgRbg
                    // gbggr
                    // grggb
                    c.out[ch + 0] = w(2, 2);
                    c.out[ch + 1] = pentagon(w(2, 1), w(1, 2), w(3, 2), w(1, 3),
                                             w(3, 3));
                    c.out[ch + 2] = triangle(w(2, 3), w(1, 1), w(3, 1));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // rggbg
                    // bggrg
                    // grBgb
                    // bggrg
                    // rggbg
                    c.out[ch + 0] = triangle(w(2, 1), w(1, 3), w(3, 3));
                    c.out[ch + 1] = pentagon(w(2, 3), w(1, 2), w(3, 2), w(1, 1),
                                             w(3, 1));
                    c.out[ch + 2] = w(2, 2);
                }))
            return true;

        return false;
    }

    template<bool check> inline static bool calc_GBR_rgg(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // ggbgg
                    // ggrgg
                    // rbGbr
                    // ggrgg
                    // ggbgg
                    c.out[ch + 0] = cross(w(2, 0), w(1, 2), w(3, 2), w(2, 4));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = cross(w(0, 2), w(2, 1), w(2, 3), w(4, 2));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // gbggr
                    // grggb
                    // bgBrg
                    // grggb
                    // gbggr
                    c.out[ch + 0] = triangle(w(2, 3), w(1, 1), w(3, 1));
                    c.out[ch + 1] = pentagon(w(2, 1), w(1, 2), w(3, 2), w(1, 3),
                                             w(3, 3));
                    c.out[ch + 2] = w(2, 2);
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // bggrg
                    // rggbg
                    // gbRgr
                    // rggbg
                    // bggrg
                    c.out[ch + 0] = w(2, 2);
                    c.out[ch + 1] = pentagon(w(2, 3), w(1, 2), w(3, 2), w(1, 1),
                                             w(3, 1));
                    c.out[ch + 2] = triangle(w(2, 1), w(1, 3), w(3, 3));
                }))
            return true;

        return false;
    }

    template<bool check> inline static bool calc_BGG_rgg(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // ggbgg
                    // brgrb
                    // ggBgg
                    // ggrgg
                    // rbgbr
                    c.out[ch + 0] = triangle(w(3, 2), w(1, 1), w(1, 3));
                    c.out[ch + 1] = pentagon(w(1, 2), w(2, 1), w(2, 3), w(3, 1),
                                             w(3, 3));
                    c.out[ch + 2] = w(2, 2);
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // gbggr
                    // rgrbg
                    // gbGgr
                    // grggb
                    // bgbrg
                    c.out[ch + 0] = square(w(1, 2), w(3, 1), w(2, 4), w(4, 3));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(2, 1), w(1, 3), w(4, 2), w(3, 4));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // bggrg
                    // grbgb
                    // bgGrg
                    // rggbg
                    // gbrgr
                    c.out[ch + 0] = square(w(2, 3), w(1, 1), w(4, 2), w(3, 0));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(1, 2), w(3, 3), w(2, 0), w(4, 1));
                }))
            return true;

        return false;
    }

    template<bool check> inline static bool calc_RGG_bgg(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // ggrgg
                    // rbgbr
                    // ggRgg
                    // ggbgg
                    // brgrb
                    c.out[ch + 0] = w(2, 2);
                    c.out[ch + 1] = pentagon(w(1, 2), w(2, 1), w(2, 3), w(3, 1),
                                             w(3, 3));
                    c.out[ch + 2] = triangle(w(3, 2), w(1, 1), w(1, 3));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // ggrggb
                    // rbgbrg
                    // ggrGgb
                    // ggbggr
                    // brgrbg
                    c.out[ch + 0] = square(w(2, 1), w(1, 3), w(4, 2), w(3, 4));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(1, 2), w(3, 1), w(2, 4), w(4, 3));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // grggbg
                    // bgbrgr
                    // grgGbg
                    // gbggrg
                    // rgrbgb
                    c.out[ch + 0] = square(w(1, 2), w(3, 3), w(2, 0), w(4, 1));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(2, 3), w(1, 1), w(4, 2), w(3, 0));
                }))
            return true;

        return false;
    }

    template<bool check> inline static bool calc_RGG_gbr(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // brgrb
                    // ggbgg
                    // ggRgg
                    // rbgbr
                    // ggrgg
                    c.out[ch + 0] = w(2, 2);
                    c.out[ch + 1] = pentagon(w(3, 2), w(2, 1), w(2, 3), w(1, 1),
                                             w(1, 3));
                    c.out[ch + 2] = triangle(w(1, 2), w(3, 1), w(3, 3));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // rgrbg
                    // gbggr
                    // grGgb
                    // bgbrg
                    // grggb
                    c.out[ch + 0] = square(w(2, 1), w(3, 3), w(0, 2), w(1, 4));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(3, 2), w(1, 1), w(2, 4), w(0, 3));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // grbgb
                    // bggrg
                    // rgGbg
                    // gbrgr
                    // rggbg
                    c.out[ch + 0] = square(w(3, 2), w(1, 3), w(2, 0), w(3, 4));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(2, 3), w(3, 1), w(0, 2), w(1, 0));
                }))
            return true;

        return false;
    }

    template<bool check> inline static bool calc_BGG_grb(Context& c)
    {
        auto& w = c.window;
        auto ch = c.chbegin;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // rbgbr
                    // ggrgg
                    // ggBgg
                    // brgrb
                    // ggbgg
                    c.out[ch + 0] = triangle(w(1, 2), w(3, 1), w(3, 3));
                    c.out[ch + 1] = pentagon(w(3, 2), w(2, 1), w(2, 3), w(1, 1),
                                             w(1, 3));
                    c.out[ch + 2] = w(2, 2);
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // bgbrg
                    // grggb
                    // gbGgr
                    // rgrbg
                    // gbggr
                    c.out[ch + 0] = square(w(3, 2), w(1, 1), w(2, 4), w(0, 3));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(2, 1), w(3, 3), w(0, 2), w(1, 4));
                }))
            return true;

        if (LinearXTransDemosaicing::template check_and_decode<check>(
                c, [&c, &w, ch]() {
                    // gbrgr
                    // rggbg
                    // bgGrg
                    // grbgb
                    // bggrg
                    c.out[ch + 0] = square(w(2, 3), w(3, 1), w(0, 2), w(1, 0));
                    c.out[ch + 1] = w(2, 2);
                    c.out[ch + 2] = square(w(3, 2), w(1, 3), w(2, 0), w(3, 4));
                }))
            return true;

        return false;
    }

    template<bool check> static void calc_GRBGBR_bggrgg(Context& c)
    {
        if (calc_GRB_bgg<check>(c))
            return;
        if (calc_GBR_rgg<check>(c))
            return;
    }

    template<bool check> static void calc_BGGRGG_rggbgg(Context& c)
    {
        if (calc_BGG_rgg<check>(c))
            return;
        if (calc_RGG_bgg<check>(c))
            return;
    }

    template<bool check> static void calc_RGGBGG_gbrgrb(Context& c)
    {
        if (calc_RGG_gbr<check>(c))
            return;
        if (calc_BGG_grb<check>(c))
            return;
    }

    template<bool check> static void calc_GBRGRB_rggbgg(Context& c)
    {
        if (calc_GBR_rgg<check>(c))
            return;
        if (calc_GRB_bgg<check>(c))
            return;
    }

    template<bool check> static void calc_RGGBGG_bggrgg(Context& c)
    {
        if (calc_RGG_bgg<check>(c))
            return;
        if (calc_BGG_rgg<check>(c))
            return;
    }

    template<bool check> static void calc_BGGRGG_grbgbr(Context& c)
    {
        if (calc_BGG_grb<check>(c))
            return;
        if (calc_RGG_gbr<check>(c))
            return;
    }

public:
    LinearXTransDemosaicing(const std::string& layout)
        : XTransDemosaicing<Rtype, Atype, 5>(layout)
    {
        this->slow_decoders[0] = calc_GRBGBR_bggrgg<true>;
        this->slow_decoders[1] = calc_BGGRGG_rggbgg<true>;
        this->slow_decoders[2] = calc_RGGBGG_gbrgrb<true>;
        this->slow_decoders[3] = calc_GBRGRB_rggbgg<true>;
        this->slow_decoders[4] = calc_RGGBGG_bggrgg<true>;
        this->slow_decoders[5] = calc_BGGRGG_grbgbr<true>;

        this->fast_decoders[0] = calc_GRBGBR_bggrgg<false>;
        this->fast_decoders[1] = calc_BGGRGG_rggbgg<false>;
        this->fast_decoders[2] = calc_RGGBGG_gbrgrb<false>;
        this->fast_decoders[3] = calc_GBRGRB_rggbgg<false>;
        this->fast_decoders[4] = calc_RGGBGG_bggrgg<false>;
        this->fast_decoders[5] = calc_BGGRGG_grbgbr<false>;
    }
};


template<class Rtype, class Atype>
static bool
bayer_demosaic_linear_impl(ImageBuf& dst, const ImageBuf& src,
                           const std::string& layout,
                           const float (&white_balance)[4], ROI roi,
                           int nthreads)
{
    LinearBayerDemosaicing<Rtype, Atype> obj(layout);
    return obj.process(dst, src, white_balance, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
bayer_demosaic_MHC_impl(ImageBuf& dst, const ImageBuf& src,
                        const std::string& layout,
                        const float (&white_balance)[4], ROI roi, int nthreads)
{
    MHCBayerDemosaicing<Rtype, Atype> obj(layout);
    return obj.process(dst, src, white_balance, roi, nthreads);

    return true;
}

template<class Rtype, class Atype>
static bool
xtrans_demosaic_linear_impl(ImageBuf& dst, const ImageBuf& src,
                            const std::string& layout,
                            const float (&white_balance)[4], ROI roi,
                            int nthreads)
{
    LinearXTransDemosaicing<Rtype, Atype> obj(layout);
    return obj.process(dst, src, white_balance, roi, nthreads);
}

bool
demosaic(ImageBuf& dst, const ImageBuf& src, KWArgs options, ROI roi,
         int nthreads)
{
    bool ok = false;
    pvt::LoggedTimer logtime("IBA::demosaic");

    std::string pattern;
    std::string algorithm;
    std::string layout;
    float white_balance_RGBG[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string error;

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
                white_balance_RGBG[0] = pv.get_float_indexed(0);
                white_balance_RGBG[1] = pv.get_float_indexed(1);
                white_balance_RGBG[2] = pv.get_float_indexed(2);
                white_balance_RGBG[3] = pv.get_float_indexed(3);

                if (white_balance_RGBG[3] == 0)
                    white_balance_RGBG[3] = white_balance_RGBG[1];
            } else if (pv.type() == TypeFloat && pv.nvalues() == 3) {
                // The order in the options is always (R,G,B)
                white_balance_RGBG[0] = pv.get_float_indexed(0);
                white_balance_RGBG[1] = pv.get_float_indexed(1);
                white_balance_RGBG[2] = pv.get_float_indexed(2);
                white_balance_RGBG[3] = white_balance_RGBG[2];
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
        dst_roi         = src.roi();
        dst_roi.chbegin = 0;
        dst_roi.chend   = 3;
    }

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

        if (algorithm == "linear") {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_linear",
                                        bayer_demosaic_linear_impl,
                                        dst.spec().format, src.spec().format,
                                        dst, src, layout, white_balance_RGBG,
                                        dst_roi, 1);
        } else if (algorithm == "MHC") {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic_MHC",
                                        bayer_demosaic_MHC_impl,
                                        dst.spec().format, src.spec().format,
                                        dst, src, layout, white_balance_RGBG,
                                        dst_roi, 1);
        } else {
            dst.errorfmt("ImageBufAlgo::demosaic() invalid algorithm");
        }
    } else if (pattern == "xtrans") {
        if (algorithm.length() == 0) {
            algorithm = "linear";
        }

        OIIO_DISPATCH_COMMON_TYPES2(ok, "xtrans_demosaic_linear",
                                    xtrans_demosaic_linear_impl,
                                    dst.spec().format, src.spec().format, dst,
                                    src, layout, white_balance_RGBG, dst_roi,
                                    1);
    } else {
        dst.errorfmt("ImageBufAlgo::demosaic() invalid pattern");
    }

    return ok;
}

ImageBuf
demosaic(const ImageBuf& src, KWArgs options, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = demosaic(result, src, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::demosaic() error");
    return result;
}

/// Creates a mosaiced version of the input image using the provided Demosaic
/// class's pattern, layout, and white balancing weights. Used for testing.
template<class Demosaic, class Rtype, class Atype>
void
mosaic(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
       const float (&wb)[4], int nthreads)
{
    ImageSpec src_spec = src.spec();

    ROI roi = src.roi_full();

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Atype> s(src, roi);
        ImageBuf::Iterator<Rtype> d(dst, roi);

        for (int y = roi.ybegin; y < roi.yend; y++) {
            for (int x = roi.xbegin; x < roi.xend; x++) {
                size_t chan = Demosaic::channel_at_offset(x_offset + x,
                                                          y_offset + y);
                size_t c    = chan;
                if (c == 3)
                    c = 1;
                d[0] = s[c] / wb[chan];
                s++;
                d++;
            }
        }
    });
}

/// Creates a mosaiced version of the input image using the provided pattern,
/// layout, and white balancing weights. Returns the layout string calculated
/// from the given offsets. Used for testing.
template<class T>
std::string
mosaic(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
       const std::string& pattern, const float (&white_balance)[4],
       int nthreads)
{
    std::string layout;

    if (pattern == "bayer") {
        BayerDemosaicing<float, float, 3>::layout_from_offset(x_offset,
                                                              y_offset, layout,
                                                              false);
        mosaic<BayerDemosaicing<float, float, 3>, T, T>(dst, src, x_offset,
                                                        y_offset, white_balance,
                                                        nthreads);
    } else if (pattern == "xtrans") {
        XTransDemosaicing<float, float, 3>::layout_from_offset(x_offset,
                                                               y_offset, layout,
                                                               false);
        mosaic<XTransDemosaicing<float, float, 3>, T, T>(dst, src, x_offset,
                                                         y_offset,
                                                         white_balance,
                                                         nthreads);
    } else {
        return "";
    }


    return layout;
}

std::string
mosaic_float(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
             const std::string& pattern, const float (&white_balance)[4],
             int nthreads)
{
    return mosaic<float>(dst, src, x_offset, y_offset, pattern, white_balance,
                         nthreads);
}

std::string
mosaic_half(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
            const std::string& pattern, const float (&white_balance)[4],
            int nthreads)
{
    return mosaic<half>(dst, src, x_offset, y_offset, pattern, white_balance,
                        nthreads);
}

std::string
mosaic_uint16(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
              const std::string& pattern, const float (&white_balance)[4],
              int nthreads)
{
    return mosaic<uint16_t>(dst, src, x_offset, y_offset, pattern,
                            white_balance, nthreads);
}

std::string
mosaic_uint8(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
             const std::string& pattern, const float (&white_balance)[4],
             int nthreads)
{
    return mosaic<uint8_t>(dst, src, x_offset, y_offset, pattern, white_balance,
                           nthreads);
}

}  // namespace ImageBufAlgo

OIIO_NAMESPACE_END
