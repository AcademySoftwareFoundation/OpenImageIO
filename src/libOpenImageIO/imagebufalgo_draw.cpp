// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

#include "imageio_pvt.h"

#ifdef USE_FREETYPE
#    include <ft2build.h>
#    include FT_FREETYPE_H
#endif

#if OIIO_GNUC_VERSION >= 80000 /* gcc 8+ */
// gcc8 complains about memcpy (in fill_const_) of half values because it
// has no trivial copy assignment.
#    pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif


OIIO_NAMESPACE_BEGIN


template<typename T>
static bool
fill_const_(ImageBuf& dst, const float* values, ROI roi = ROI(),
            int nthreads = 1)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [=, &dst](ROI roi) {
        // Do the data conversion just once, store locally.
        T* tvalues = OIIO_ALLOCA(T, roi.chend);
        for (int i = roi.chbegin; i < roi.chend; ++i)
            tvalues[i] = convert_type<float, T>(values[i]);
        int nchannels = roi.nchannels();
        for (ImageBuf::Iterator<T, T> p(dst, roi); !p.done(); ++p)
            memcpy((T*)p.rawptr() + roi.chbegin, tvalues + roi.chbegin,
                   nchannels * sizeof(T));
    });
    return true;
}


template<typename T>
static bool
fill_tb_(ImageBuf& dst, const float* top, const float* bottom, ROI origroi,
         ROI roi = ROI(), int nthreads = 1)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        float h = std::max(1, origroi.height() - 1);
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            float v = (p.y() - origroi.ybegin) / h;
            for (int c = roi.chbegin; c < roi.chend; ++c)
                p[c] = lerp(top[c], bottom[c], v);
        }
    });
    return true;
}


template<typename T>
static bool
fill_corners_(ImageBuf& dst, const float* topleft, const float* topright,
              const float* bottomleft, const float* bottomright, ROI origroi,
              ROI roi = ROI(), int nthreads = 1)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        float w = std::max(1, origroi.width() - 1);
        float h = std::max(1, origroi.height() - 1);
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            float u = (p.x() - origroi.xbegin) / w;
            float v = (p.y() - origroi.ybegin) / h;
            for (int c = roi.chbegin; c < roi.chend; ++c)
                p[c] = bilerp(topleft[c], topright[c], bottomleft[c],
                              bottomright[c], u, v);
        }
    });
    return true;
}


bool
ImageBufAlgo::fill(ImageBuf& dst, cspan<float> pixel, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fill");
    if (!IBAprep(roi, &dst))
        return false;
    bool ok;
    IBA_FIX_PERCHAN_LEN_DEF(pixel, dst.nchannels());
    OIIO_DISPATCH_TYPES(ok, "fill", fill_const_, dst.spec().format, dst,
                        pixel.data(), roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::fill(ImageBuf& dst, cspan<float> top, cspan<float> bottom,
                   ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fill");
    if (!IBAprep(roi, &dst))
        return false;
    bool ok;
    IBA_FIX_PERCHAN_LEN_DEF(top, dst.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(bottom, dst.nchannels());
    OIIO_DISPATCH_TYPES(ok, "fill", fill_tb_, dst.spec().format, dst,
                        top.data(), bottom.data(), roi, roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::fill(ImageBuf& dst, cspan<float> topleft, cspan<float> topright,
                   cspan<float> bottomleft, cspan<float> bottomright, ROI roi,
                   int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fill");
    if (!IBAprep(roi, &dst))
        return false;
    bool ok;
    IBA_FIX_PERCHAN_LEN_DEF(topleft, dst.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(topright, dst.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(bottomleft, dst.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(bottomright, dst.nchannels());
    OIIO_DISPATCH_TYPES(ok, "fill", fill_corners_, dst.spec().format, dst,
                        topleft.data(), topright.data(), bottomleft.data(),
                        bottomright.data(), roi, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::fill(cspan<float> pixel, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fill(result, pixel, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("fill error");
    return result;
}


ImageBuf
ImageBufAlgo::fill(cspan<float> top, cspan<float> bottom, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fill(result, top, bottom, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("fill error");
    return result;
}


ImageBuf
ImageBufAlgo::fill(cspan<float> topleft, cspan<float> topright,
                   cspan<float> bottomleft, cspan<float> bottomright, ROI roi,
                   int nthreads)
{
    ImageBuf result;
    bool ok = fill(result, topleft, topright, bottomleft, bottomright, roi,
                   nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("fill error");
    return result;
}


bool
ImageBufAlgo::zero(ImageBuf& dst, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::zero");
    if (!IBAprep(roi, &dst))
        return false;
    OIIO_ASSERT(dst.localpixels());
    if (dst.contiguous() && roi == dst.roi() && !dst.deep()) {
        // Special case: we're zeroing out an entire contiguous buffer -- safe
        // to use use memset.
        ImageBufAlgo::parallel_image(roi, nthreads, [=, &dst](ROI roi) {
            auto size = dst.spec().pixel_bytes() * imagesize_t(roi.width());
            for (int z = roi.zbegin; z < roi.zend; ++z) {
                for (int y = roi.ybegin; y < roi.yend; ++y) {
                    memset(dst.pixeladdr(roi.xbegin, y, z), 0, size);
                }
            }
        });
        return true;
    }

    // More general case -- fall back on fill_const
    float* zero = OIIO_ALLOCA(float, roi.chend);
    memset(zero, 0, roi.chend * sizeof(float));
    bool ok;
    OIIO_DISPATCH_TYPES(ok, "zero", fill_const_, dst.spec().format, dst, zero,
                        roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::zero(ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = zero(result, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("zero error");
    return result;
}



template<typename T>
static bool
render_point_(ImageBuf& dst, int x, int y, const float* color, float alpha,
              ROI roi, int /*nthreads*/)
{
    ImageBuf::Iterator<T> r(dst, x, y);
    for (int c = roi.chbegin; c < roi.chend; ++c)
        r[c] = color[c] + r[c] * (1.0f - alpha);  // "over"
    return true;
}



bool
ImageBufAlgo::render_point(ImageBuf& dst, int x, int y, cspan<float> color,
                           ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::render_point");
    if (!IBAprep(roi, &dst))
        return false;
    IBA_FIX_PERCHAN_LEN_DEF(color, dst.nchannels());
    if (x < roi.xbegin || x >= roi.xend || y < roi.ybegin || y >= roi.yend)
        return true;  // outside of bounds
    const ImageSpec& spec(dst.spec());

    // Alpha: if the image's spec designates an alpha channel, use it if
    // it's within the range specified by color. Otherwise, if color
    // includes more values than the highest channel roi says we should
    // modify, assume the first extra value is alpha. If all else fails,
    // make the line opaque (alpha=1.0).
    float alpha = 1.0f;
    if (spec.alpha_channel >= 0 && spec.alpha_channel < int(color.size()))
        alpha = color[spec.alpha_channel];
    else if (int(color.size()) == roi.chend + 1)
        alpha = color[roi.chend];

    bool ok;
    OIIO_DISPATCH_TYPES(ok, "render_point", render_point_, dst.spec().format,
                        dst, x, y, color.data(), alpha, roi, nthreads);
    return ok;
}



// Basic Bresenham 2D line drawing algorithm. Call func(x,y) for each x,y
// along the line from (x1,y1) to (x2,y2). If skip_first is true, don't draw
// the very first point.
template<typename FUNC>
static bool
bresenham2d(FUNC func, int x1, int y1, int x2, int y2, bool skip_first = false)
{
    // Basic Bresenham
    int dx   = abs(x2 - x1);
    int dy   = abs(y2 - y1);
    int xinc = (x1 > x2) ? -1 : 1;
    int yinc = (y1 > y2) ? -1 : 1;
    if (dx >= dy) {
        int dpr   = dy << 1;
        int dpru  = dpr - (dx << 1);
        int delta = dpr - dx;
        for (; dx >= 0; --dx) {
            if (skip_first)
                skip_first = false;
            else
                func(x1, y1);
            x1 += xinc;
            if (delta > 0) {
                y1 += yinc;
                delta += dpru;
            } else {
                delta += dpr;
            }
        }
    } else {
        int dpr   = dx << 1;
        int dpru  = dpr - (dy << 1);
        int delta = dpr - dy;
        for (; dy >= 0; dy--) {
            if (skip_first)
                skip_first = false;
            else
                func(x1, y1);
            y1 += yinc;
            if (delta > 0) {
                x1 += xinc;
                delta += dpru;
            } else {
                delta += dpr;
            }
        }
    }
    return true;
}



// Helper function that holds an IB::Iterator and composites a point into
// it every time drawer(x,y) is called.
template<typename T> struct IB_drawer {
    IB_drawer(ImageBuf::Iterator<T, float>& r_, cspan<float> color_,
              float alpha_, ROI roi_)
        : r(r_)
        , color(color_)
        , alpha(alpha_)
        , roi(roi_)
    {
    }

    void operator()(int x, int y)
    {
        r.pos(x, y);
        if (r.valid())
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = color[c] + r[c] * (1.0f - alpha);  // "over"
    }

    ImageBuf::Iterator<T, float>& r;
    cspan<float> color;
    float alpha;
    ROI roi;
};



template<typename T>
static bool
render_line_(ImageBuf& dst, int x1, int y1, int x2, int y2, cspan<float> color,
             float alpha, bool skip_first, ROI roi, int /*nthreads*/)
{
    ImageBuf::Iterator<T> r(dst, roi);
    IB_drawer<T> draw(r, color, alpha, roi);
    bresenham2d(draw, x1, y1, x2, y2, skip_first);
    return true;
}



bool
ImageBufAlgo::render_line(ImageBuf& dst, int x1, int y1, int x2, int y2,
                          cspan<float> color, bool skip_first_point, ROI roi,
                          int nthreads)
{
    pvt::LoggedTimer logtime("IBA::render_line");
    if (!IBAprep(roi, &dst))
        return false;
    IBA_FIX_PERCHAN_LEN_DEF(color, dst.nchannels());
    const ImageSpec& spec(dst.spec());

    // Alpha: if the image's spec designates an alpha channel, use it if
    // it's within the range specified by color. Otherwise, if color
    // includes more values than the highest channel roi says we should
    // modify, assume the first extra value is alpha. If all else fails,
    // make the line opaque (alpha=1.0).
    float alpha = 1.0f;
    if (spec.alpha_channel >= 0 && spec.alpha_channel < int(color.size()))
        alpha = color[spec.alpha_channel];
    else if (int(color.size()) == roi.chend + 1)
        alpha = color[roi.chend];

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "render_line", render_line_,
                               dst.spec().format, dst, x1, y1, x2, y2, color,
                               alpha, skip_first_point, roi, nthreads);
    return ok;
}



template<typename T>
static bool
render_box_(ImageBuf& dst, cspan<float> color, ROI roi = ROI(),
            int nthreads = 1)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        float alpha = 1.0f;
        if (dst.spec().alpha_channel >= 0
            && dst.spec().alpha_channel < int(color.size()))
            alpha = color[dst.spec().alpha_channel];
        else if (int(color.size()) == roi.chend + 1)
            alpha = color[roi.chend];

        if (alpha == 1.0f) {
            for (ImageBuf::Iterator<T> r(dst, roi); !r.done(); ++r)
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    r[c] = color[c];
        } else {
            for (ImageBuf::Iterator<T> r(dst, roi); !r.done(); ++r)
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    r[c] = color[c] + r[c] * (1.0f - alpha);  // "over"
        }
    });
    return true;
}



bool
ImageBufAlgo::render_box(ImageBuf& dst, int x1, int y1, int x2, int y2,
                         cspan<float> color, bool fill, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::render_box");
    if (!IBAprep(roi, &dst))
        return false;
    IBA_FIX_PERCHAN_LEN_DEF(color, dst.nchannels());

    if (x1 == x2 && y1 == y2) {
        // degenerate 1-point rectangle
        return render_point(dst, x1, y1, color, roi, nthreads);
    }

    // Filled case
    if (fill) {
        roi = roi_intersection(roi,
                               ROI(x1, x2 + 1, y1, y2 + 1, 0, 1, 0, roi.chend));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES(ok, "render_box", render_box_,
                                   dst.spec().format, dst, color, roi,
                                   nthreads);
        return ok;
    }

    // Unfilled case: use IBA::render_line
    return ImageBufAlgo::render_line(dst, x1, y1, x2, y1, color, true, roi,
                                     nthreads)
           && ImageBufAlgo::render_line(dst, x2, y1, x2, y2, color, true, roi,
                                        nthreads)
           && ImageBufAlgo::render_line(dst, x2, y2, x1, y2, color, true, roi,
                                        nthreads)
           && ImageBufAlgo::render_line(dst, x1, y2, x1, y1, color, true, roi,
                                        nthreads);
}



// Convenient helper struct to bundle a 3-int describing a block size.
struct Dim3 {
    int x, y, z;
    Dim3(int x, int y = 1, int z = 1)
        : x(x)
        , y(y)
        , z(z)
    {
    }
};



template<typename T>
static bool
checker_(ImageBuf& dst, Dim3 size, const float* color1, const float* color2,
         Dim3 offset, ROI roi, int nthreads = 1)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            int xtile = (p.x() - offset.x) / size.x;
            xtile += (p.x() < offset.x);
            int ytile = (p.y() - offset.y) / size.y;
            ytile += (p.y() < offset.y);
            int ztile = (p.z() - offset.z) / size.z;
            ztile += (p.z() < offset.z);
            int v = xtile + ytile + ztile;
            if (v & 1)
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    p[c] = color2[c];
            else
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    p[c] = color1[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::checker(ImageBuf& dst, int width, int height, int depth,
                      cspan<float> color1, cspan<float> color2, int xoffset,
                      int yoffset, int zoffset, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::checker");
    if (!IBAprep(roi, &dst))
        return false;
    IBA_FIX_PERCHAN_LEN_DEF(color1, dst.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(color2, dst.nchannels());
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "checker", checker_, dst.spec().format, dst,
                               Dim3(width, height, depth), color1.data(),
                               color2.data(), Dim3(xoffset, yoffset, zoffset),
                               roi, nthreads);
    return ok;
}


ImageBuf
ImageBufAlgo::checker(int width, int height, int depth, cspan<float> color1,
                      cspan<float> color2, int xoffset, int yoffset,
                      int zoffset, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = checker(result, width, height, depth, color1, color2, xoffset,
                      yoffset, zoffset, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("checker error");
    return result;
}



// Return a repeatable hash-based pseudo-random value uniform on [0,1).
// It's a hash, so it's completely deterministic, based on x,y,z,c,seed.
// But it can be used in similar ways to a PRNG.
OIIO_FORCEINLINE float
hashrand(int x, int y, int z, int c, int seed)
{
    const uint32_t magic = 0xfffff;
    uint32_t xu(x), yu(y), zu(z), cu(c), seedu(seed);
    using bjhash::bjfinal;
    uint32_t h = bjfinal(bjfinal(xu, yu, zu), cu, seedu) & magic;
    return h * (1.0f / (magic + 1));
}


// Return a hash-based normal-distributed pseudorandom value.
// We use the Marsaglia polar method, and hashrand to
OIIO_FORCEINLINE float
hashnormal(int x, int y, int z, int c, int seed)
{
    float xr, yr, r2;
    int s = seed - 1;
    do {
        s += 1;
        xr = 2.0 * hashrand(x, y, z, c, s) - 1.0;
        yr = 2.0 * hashrand(x, y, z, c, s + 139) - 1.0;
        r2 = xr * xr + yr * yr;
    } while (r2 > 1.0 || r2 == 0.0);
    float M = sqrt(-2.0 * log(r2) / r2);
    return xr * M;
}



template<typename T>
static bool
noise_uniform_(ImageBuf& dst, float min, float max, bool mono, int seed,
               ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == roi.chbegin || !mono)
                    n = lerp(min, max, hashrand(x, y, z, c, seed));
                p[c] = p[c] + n;
            }
        }
    });
    return true;
}



template<typename T>
static bool
noise_gaussian_(ImageBuf& dst, float mean, float stddev, bool mono, int seed,
                ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == roi.chbegin || !mono)
                    n = mean + stddev * hashnormal(x, y, z, c, seed);
                p[c] = p[c] + n;
            }
        }
    });
    return true;
}



template<typename T>
static bool
noise_salt_(ImageBuf& dst, float saltval, float saltportion, bool mono,
            int seed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == roi.chbegin || !mono)
                    n = hashrand(x, y, z, c, seed);
                if (n < saltportion)
                    p[c] = saltval;
            }
        }
    });
    return true;
}



template<typename T>
static bool
noise_blue_(ImageBuf& dst, float min, float max, bool mono, int seed, ROI roi,
            int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<T> p(dst, roi); !p.done(); ++p) {
            float n         = 0.0;
            const float* bn = nullptr;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == roi.chbegin || !mono) {
                    if (!bn || !(c & 3))
                        bn = pvt::bluenoise_4chan_ptr(p.x(), p.y(), p.z(),
                                                      roi.chbegin & ~3, seed);
                    n = lerp(min, max, bn[c & 3]);
                }
                p[c] = p[c] + n;
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::noise(ImageBuf& dst, string_view noisetype, float A, float B,
                    bool mono, int seed, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::noise");
    if (!IBAprep(roi, &dst))
        return false;
    bool ok;
    if (noisetype == "gaussian" || noisetype == "normal") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_gaussian", noise_gaussian_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else if (noisetype == "white" || noisetype == "uniform") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_uniform", noise_uniform_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else if (noisetype == "blue") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_blue", noise_blue_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else if (noisetype == "salt") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_salt", noise_salt_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else {
        ok = false;
        dst.errorfmt("unknown noise type \"{}\"", noisetype);
    }
    return ok;
}



ImageBuf
ImageBufAlgo::noise(string_view noisetype, float A, float B, bool mono,
                    int seed, ROI roi, int nthreads)
{
    ImageBuf result = ImageBufAlgo::zero(roi, nthreads);
    bool ok         = true;
    ok              = noise(result, noisetype, A, B, mono, seed, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("noise error");
    return result;
}



namespace {
// Helper: make an ImageSpec in the shape of the bluenosie_table, but don't
// let it look like it's RGB, because it's just data.
inline ImageSpec
bnspec()
{
    ImageSpec spec(pvt::bntable_res, pvt::bntable_res, 4, TypeFloat);
    spec.channelnames  = { "X", "Y", "Z", "W" };
    spec.alpha_channel = -1;
    return spec;
}
}  // namespace

const ImageBuf&
ImageBufAlgo::bluenoise_image()
{
    // This ImageBuf "wraps" the table.
    using namespace pvt;
    static ImageBuf img(bnspec(), make_cspan(&bluenoise_table[0][0][0],
                                             bntable_res * bntable_res * 4));
    return img;
}



static std::vector<std::string> font_search_dirs;
static std::vector<std::string> all_font_files;
static std::vector<std::string> all_fonts;
static std::unordered_map<std::string, std::string> font_file_map;
static std::mutex font_search_mutex;
static bool fonts_are_enumerated      = false;
static const char* font_dir_envvars[] = { "OPENIMAGEIO_FONTS",
                                          "OpenImageIO_ROOT" };
static const char* font_dir_suffixes[]
    = { "fonts",       "Fonts",       "Library/Fonts",
        "share/fonts", "share/Fonts", "share/fonts/OpenImageIO" };
// static const char* font_extensions[]   = { "", ".ttf", ".ttc", ".pfa", ".pfb" };

// list of available font families
static std::vector<std::string> s_font_families;
// available font styles per families
static std::unordered_map<std::string, std::vector<std::string>> s_font_styles;
// font filenames per family and style (e.g. "Arial Italic")
static std::unordered_map<std::string, std::string> s_font_filename_per_family;


// Add one dir to font_search_dirs, if the dir exists.
static void
fontpath_add_one_dir(string_view dir, int recursion = 0)
{
    if (dir.size() && Filesystem::is_directory(dir)) {
        font_search_dirs.emplace_back(dir);
        if (recursion) {
            std::vector<std::string> files;
            if (Filesystem::get_directory_entries(dir, files, false)) {
                for (auto&& subdir : files)
                    fontpath_add_one_dir(subdir, recursion - 1);
            }
        }
    }
}


// Add all the dirs in a searchpath to font_search_dirs.
static void
fontpath_add_from_searchpath(string_view searchpath)
{
    if (searchpath.size()) {
        for (auto& dir : Filesystem::searchpath_split(searchpath, true)) {
            fontpath_add_one_dir(dir);
            for (auto s : font_dir_suffixes)
                fontpath_add_one_dir(Strutil::fmt::format("{}/{}", dir, s));
        }
    }
}


// Add dir/{common_font_subdirs} to font_search_dirs.
static void
fontpath_add_from_dir(const std::string& dir)
{
    if (dir.size() && Filesystem::is_directory(dir)) {
        fontpath_add_one_dir(dir);
        for (auto s : font_dir_suffixes)
            fontpath_add_one_dir(Strutil::fmt::format("{}/{}", dir, s));
    }
}


static void
enumerate_fonts()
{
    std::lock_guard<std::mutex> lock(font_search_mutex);
    if (fonts_are_enumerated)
        return;  // already done

    // Find all the existing dirs from the font search path to populate
    // font_search_dirs.
    fontpath_add_from_searchpath(pvt::font_searchpath);
    // Find all the existing dirs from specific environment variables.
    for (auto s : font_dir_envvars)
        fontpath_add_from_searchpath(Sysutil::getenv(s));

        // Add system font directories
#ifdef _WIN32
    fontpath_add_one_dir(std::string(Sysutil::getenv("SystemRoot")) + "/Fonts");
    fontpath_add_one_dir(std::string(Sysutil::getenv("LOCALAPPDATA"))
                         + "/Microsoft/Windows/Fonts");
#endif
#ifdef __APPLE__
    fontpath_add_one_dir("/Library/Fonts");
    fontpath_add_one_dir("/System/Library/Fonts");
    fontpath_add_one_dir("/System/Library/Fonts/Supplemental");
    fontpath_add_one_dir(std::string(Sysutil::getenv("HOME"))
                         + "/Library/Fonts");
#endif
#ifdef __linux__
    fontpath_add_one_dir("/usr/share/fonts", 1);
    fontpath_add_one_dir("/usr/local/share/fonts", 1);
    fontpath_add_one_dir(std::string(Sysutil::getenv("HOME")) + "/.fonts", 1);
    fontpath_add_one_dir(std::string(Sysutil::getenv("HOME"))
                             + "/.local/share/fonts",
                         1);
#endif
    // Find font directories one level up from the place
    // where the currently running binary lives.
    std::string this_program = OIIO::Sysutil::this_program_path();
    if (this_program.size()) {
        std::string path = Filesystem::parent_path(this_program);
        path             = Filesystem::parent_path(path);
        fontpath_add_from_dir(path);
    }

    // Make sure folders are not duplicated
    std::vector<std::string> tmp_font_search_dirs = font_search_dirs;
    font_search_dirs.clear();
    std::unordered_set<std::string> font_search_dir_set;
    for (const std::string& dir : tmp_font_search_dirs) {
        std::string target_dir = dir;
#ifdef _WIN32
        // Windows is not case-sensitive, compare lower case paths
        target_dir = Strutil::lower(target_dir);
        // unify path separators
        target_dir = Strutil::replace(target_dir, "/", "\\", true);
#endif
        if (font_search_dir_set.find(target_dir) != font_search_dir_set.end())
            continue;

        font_search_dirs.push_back(dir);
        font_search_dir_set.insert(target_dir);
    }

    // Look for all the font files in dirs, populate font_file_set and font_set
    std::set<std::string> font_set;
    std::set<std::string> font_file_set;
    for (auto& dir : font_search_dirs) {
        std::vector<std::string> filenames;
        Filesystem::get_directory_entries(dir, filenames, false);
        for (auto& f : filenames) {
            if (Strutil::iends_with(f, ".ttf") || Strutil::iends_with(f, ".ttc")
                || Strutil::iends_with(f, ".pfa")
                || Strutil::iends_with(f, ".pfb")) {
                std::string fontname
                    = Filesystem::replace_extension(Filesystem::filename(f),
                                                    "");
                font_file_set.insert(f);
                font_set.insert(fontname);
                if (font_file_map.find(fontname) == font_file_map.end())
                    font_file_map[fontname] = f;
            }
        }
    }
    for (auto& f : font_file_set)
        all_font_files.push_back(f);
    for (auto& f : font_set)
        all_fonts.push_back(f);

    // Don't need to do that again
    fonts_are_enumerated = true;
}



const std::vector<std::string>&
pvt::font_dirs()
{
    enumerate_fonts();
    return font_search_dirs;
}



const std::vector<std::string>&
pvt::font_file_list()
{
    enumerate_fonts();
    return all_font_files;
}



const std::vector<std::string>&
pvt::font_list()
{
    enumerate_fonts();
    return all_fonts;
}



#ifdef USE_FREETYPE
namespace {  // anon
static mutex ft_mutex;
static FT_Library ft_library = NULL;
static bool ft_broken        = false;

static const char* default_font_name[] = { "DroidSans", "cour", "Courier New",
                                           "FreeMono" };



// Helper: given unicode and a font face, compute its size
static ROI
text_size_from_unicode(cspan<uint32_t> utext, FT_Face face, int fontsize)
{
    int y = 0;
    int x = 0;
    ROI size;
    size.xbegin = size.ybegin = std::numeric_limits<int>::max();
    size.xend = size.yend = std::numeric_limits<int>::min();
    FT_GlyphSlot slot     = face->glyph;
    for (auto ch : utext) {
        if (ch == '\n') {
            x = 0;
            y += fontsize;
            continue;
        }
        int error = FT_Load_Char(face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        size.ybegin = std::min(size.ybegin, y - slot->bitmap_top);
        size.yend   = std::max(size.yend, y + int(slot->bitmap.rows)
                                              - int(slot->bitmap_top) + 1);
        size.xbegin = std::min(size.xbegin, x + int(slot->bitmap_left));
        size.xend   = std::max(size.xend, x + int(slot->bitmap.width)
                                              + int(slot->bitmap_left) + 1);
        // increment pen position
        x += slot->advance.x >> 6;
    }
    return size;  // Font rendering not supported
}


// Read available font families and styles.
static void
init_font_families()
{
    // skip if already initialized
    if (!s_font_families.empty())
        return;

    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return;

    // If FT not yet initialized, do it now.
    if (!ft_library) {
        if (FT_Init_FreeType(&ft_library)) {
            ft_broken = true;
            return;
        }
    }

    // read available fonts
    std::unordered_set<std::string> font_family_set;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        font_style_set;
    const std::vector<std::string>& font_files = pvt::font_file_list();
    for (const std::string& filename : font_files) {
        // Load the font.
        FT_Face face;
        int error = FT_New_Face(ft_library, filename.c_str(),
                                0 /* face index */, &face);
        if (error)
            continue;

        // Ignore if the font fmaily name is not defined.
        if (!face->family_name) {
            FT_Done_Face(face);
            continue;
        }

        // Store the font family.
        std::string family = std::string(face->family_name);
        font_family_set.insert(family);

        // Store the font style.
        std::string style = face->style_name ? std::string(face->style_name)
                                             : std::string();
        if (!style.empty()) {
            std::unordered_set<std::string>& styles = font_style_set[family];
            styles.insert(style);
        }

        // Store the filename. Use the family and style as the key (e.g. "Arial Italic").
        std::string font_name = family;
        if (!style.empty())
            font_name += " " + style;
        s_font_filename_per_family[font_name] = filename;

        // Store regular fonts also with the family name only (e.g. "Arial Regular" as "Arial").
        if (style == "Regular")
            s_font_filename_per_family[family] = filename;

        FT_Done_Face(face);
    }

    // Sort font families.
    s_font_families = std::vector<std::string>(font_family_set.begin(),
                                               font_family_set.end());
    std::sort(s_font_families.begin(), s_font_families.end());

    // Sort font styles.
    for (auto it : font_style_set) {
        const std::string& family                   = it.first;
        std::unordered_set<std::string>& styles_set = it.second;
        std::vector<std::string> styles(styles_set.begin(), styles_set.end());
        std::sort(styles.begin(), styles.end());
        s_font_styles[family] = styles;
    }
}


// Given font name, resolve it to an existing font filename.
// If found, return true and put the resolved filename in result.
// If not found, return false and put an error message in result.
// Not thread-safe! The caller must use the mutex.
static bool
resolve_font(string_view font_, std::string& result)
{
    result.clear();

    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return false;

    // If FT not yet initialized, do it now.
    if (!ft_library) {
        if (FT_Init_FreeType(&ft_library)) {
            ft_broken = true;
            result    = "Could not initialize FreeType for font rendering";
            return false;
        }
    }

    // Try to find the font.
    enumerate_fonts();
    std::string font = font_;
    if (font.empty()) {
        // nothing specified -- look for something to use as a default.
        for (auto fontname : default_font_name) {
            auto f = font_file_map.find(fontname);
            if (f != font_file_map.end()) {
                font = f->second;
                break;
            }
        }
        if (font.empty()) {
            result = "Could not set default font face";
            return false;
        }
    } else if (Filesystem::is_regular(font)) {
        // directly specified filename -- use it
    } else {
        // A font name was specified but it's not a full path, look for it
        std::string f;

        // first look for a font with the given family and style
        init_font_families();
        if (s_font_filename_per_family.find(font)
            != s_font_filename_per_family.end())
            f = s_font_filename_per_family[font];

        // then look for a font with the given filename
        if (f.empty()) {
            if (font_file_map.find(font) != font_file_map.end())
                f = font_file_map[font];
        }

        if (!f.empty()) {
            font = f;
        } else {
            result = Strutil::fmt::format("Could not find font \"{}\"", font);
            return false;
        }
    }

    if (!Filesystem::is_regular(font)) {
        result = Strutil::fmt::format("Could not find font \"{}\"", font);
        return false;
    }

    // Success
    result = font;
    return true;
}

}  // namespace
#endif



ROI
ImageBufAlgo::text_size(string_view text, int fontsize, string_view font_)
{
    pvt::LoggedTimer logtime("IBA::text_size");
    ROI size;
#ifdef USE_FREETYPE
    // Thread safety
    lock_guard ft_lock(ft_mutex);

    std::string font;
    bool ok = resolve_font(font_, font);
    if (!ok) {
        return size;
    }

    int error = 0;
    FT_Face face;  // handle to face object
    error = FT_New_Face(ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        return size;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes(face /*handle*/, 0 /*width*/,
                               fontsize /*height*/);
    if (error) {
        FT_Done_Face(face);
        return size;  // couldn't set the character size
    }

    std::vector<uint32_t> utext;
    utext.reserve(text.size());
    Strutil::utf8_to_unicode(text, utext);
    size = text_size_from_unicode(utext, face, fontsize);

    FT_Done_Face(face);
#endif

    return size;  // Font rendering not supported
}



bool
ImageBufAlgo::render_text(ImageBuf& R, int x, int y, string_view text,
                          int fontsize, string_view font_,
                          cspan<float> textcolor, TextAlignX alignx,
                          TextAlignY aligny, int shadow, ROI roi,
                          int /*nthreads*/)
{
    pvt::LoggedTimer logtime("IBA::render_text");
    if (R.spec().depth > 1) {
        R.errorfmt("ImageBufAlgo::render_text does not support volume images");
        return false;
    }

#ifdef USE_FREETYPE
    // Thread safety
    lock_guard ft_lock(ft_mutex);

    std::string font;
    bool ok = resolve_font(font_, font);
    if (!ok) {
        std::string err = font.size() ? font : "Font error";
        R.errorfmt("{}", err);
        return false;
    }

    int error = 0;
    FT_Face face;  // handle to face object
    error = FT_New_Face(ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.errorfmt("Could not set font face to \"{}\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes(face /*handle*/, 0 /*width*/,
                               fontsize /*height*/);
    if (error) {
        FT_Done_Face(face);
        R.errorfmt("Could not set font size to {}", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    int nchannels(R.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(textcolor, nchannels);

    // Take into account the alpha of the requested text color. Slightly
    // complicated logic to try to make our best guess.
    int alpha_channel = R.spec().alpha_channel;
    float textalpha   = 1.0f;
    if (alpha_channel >= 0 && alpha_channel < int(textcolor.size())) {
        // If the image we're writing into has a designated alpha, use it.
        textalpha = textcolor[alpha_channel];
    } else if (alpha_channel < 0 && nchannels <= 4 && textcolor.size() == 4) {
        // If the buffer doesn't have an alpha, but the text color passed
        // has 4 values, assume the last value is supposed to be alpha.
        textalpha = textcolor[3];
    }

    // Convert the UTF to 32 bit unicode
    std::vector<uint32_t> utext;
    utext.reserve(text.size());
    Strutil::utf8_to_unicode(text, utext);

    // Compute the size that the text will render as, into an ROI
    ROI textroi     = text_size_from_unicode(utext, face, fontsize);
    textroi.zbegin  = 0;
    textroi.zend    = 1;
    textroi.chbegin = 0;
    textroi.chend   = 1;

    // Adjust position for alignment requests
    if (alignx == TextAlignX::Right)
        x -= textroi.width();
    if (alignx == TextAlignX::Center)
        x -= (textroi.width() / 2 + textroi.xbegin);
    if (aligny == TextAlignY::Top)
        y -= textroi.ybegin;
    if (aligny == TextAlignY::Bottom)
        y -= textroi.yend;
    if (aligny == TextAlignY::Center)
        y -= (textroi.height() / 2 + textroi.ybegin);

    // Pad bounds for shadowing
    textroi.xbegin += x - shadow;
    textroi.xend += x + shadow;
    textroi.ybegin += y - shadow;
    textroi.yend += y + shadow;

    // Create a temp buffer of the right size and render the text into it.
    ImageBuf textimg(ImageSpec(textroi, TypeDesc::FLOAT));
    ImageBufAlgo::zero(textimg);

    // Glyph by glyph, fill in our textimg buffer
    int origx = x;
    for (auto ch : utext) {
        // on Windows a newline is encoded as '\r\n'
        // we simply ignore carriage return here
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            x = origx;
            y += fontsize;
            continue;
        }
        int error = FT_Load_Char(face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        // now, draw to our target surface
        for (int j = 0; j < static_cast<int>(slot->bitmap.rows); ++j) {
            int ry = y + j - slot->bitmap_top;
            for (int i = 0; i < static_cast<int>(slot->bitmap.width); ++i) {
                int rx  = x + i + slot->bitmap_left;
                float b = slot->bitmap.buffer[slot->bitmap.pitch * j + i]
                          / 255.0f;
                textimg.setpixel(rx, ry, b);
            }
        }
        // increment pen position
        x += slot->advance.x >> 6;
    }

    // Generate the alpha image -- if drop shadow is requested, dilate,
    // otherwise it's just a copy of the text image
    ImageBuf alphaimg;
    if (shadow)
        dilate(alphaimg, textimg, 2 * shadow + 1);
    else
        alphaimg.copy(textimg);

    if (!roi.defined())
        roi = textroi;
    if (!IBAprep(roi, &R))
        return false;
    roi = roi_intersection(textroi, R.roi());

    // Now fill in the pixels of our destination image
    span<float> pixelcolor = OIIO_ALLOCA_SPAN(float, nchannels);
    ImageBuf::ConstIterator<float> t(textimg, roi, ImageBuf::WrapBlack);
    ImageBuf::ConstIterator<float> a(alphaimg, roi, ImageBuf::WrapBlack);
    ImageBuf::Iterator<float> r(R, roi);
    for (; !r.done(); ++r, ++t, ++a) {
        float val   = t[0];
        float alpha = a[0] * textalpha;
        R.getpixel(r.x(), r.y(), pixelcolor);
        for (int c = 0; c < nchannels; ++c)
            pixelcolor[c] = val * textcolor[c] + (1.0f - alpha) * pixelcolor[c];
        R.setpixel(r.x(), r.y(), pixelcolor);
    }

    FT_Done_Face(face);
    return true;

#else
    R.errorfmt("OpenImageIO was not compiled with FreeType for font rendering");
    return false;  // Font rendering not supported
#endif
}



const std::vector<std::string>&
pvt::font_family_list()
{
#ifdef USE_FREETYPE
    lock_guard ft_lock(ft_mutex);
    init_font_families();
#endif
    return s_font_families;
}


const std::vector<std::string>
pvt::font_style_list(string_view family)
{
#ifdef USE_FREETYPE
    lock_guard ft_lock(ft_mutex);
    init_font_families();
#endif
    auto it = s_font_styles.find(family);
    if (it != s_font_styles.end())
        return it->second;
    else
        return std::vector<std::string>();
}


const std::string
pvt::font_filename(string_view family, string_view style)
{
    if (family.empty())
        return std::string();

#ifdef USE_FREETYPE
    lock_guard ft_lock(ft_mutex);
    init_font_families();
#endif

    std::string font = family;
    if (!style.empty())
        font = Strutil::fmt::format("{} {}", family, style);

    auto it = s_font_filename_per_family.find(font);
    if (it != s_font_filename_per_family.end())
        return it->second;
    else
        return std::string();
}



OIIO_NAMESPACE_END
