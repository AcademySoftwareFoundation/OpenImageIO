// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <OpenEXR/half.h>

#include <algorithm>
#include <cmath>
#include <limits>

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
        result.errorf("fill error");
    return result;
}


ImageBuf
ImageBufAlgo::fill(cspan<float> top, cspan<float> bottom, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fill(result, top, bottom, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorf("fill error");
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
        result.errorf("fill error");
    return result;
}


bool
ImageBufAlgo::zero(ImageBuf& dst, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::zero");
    if (!IBAprep(roi, &dst))
        return false;
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
        result.errorf("zero error");
    return result;
}



template<typename T>
static bool
render_point_(ImageBuf& dst, int x, int y, const float* color, float alpha,
              ROI roi, int nthreads)
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
             float alpha, bool skip_first, ROI roi, int nthreads)
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
        result.errorf("checker error");
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
    } else if (noisetype == "uniform") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_uniform", noise_uniform_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else if (noisetype == "salt") {
        OIIO_DISPATCH_COMMON_TYPES(ok, "noise_salt", noise_salt_,
                                   dst.spec().format, dst, A, B, mono, seed,
                                   roi, nthreads);
    } else {
        ok = false;
        dst.errorf("noise", "unknown noise type \"%s\"", noisetype);
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
        result.errorf("noise error");
    return result;
}



#ifdef USE_FREETYPE
namespace {  // anon
static mutex ft_mutex;
static FT_Library ft_library = NULL;
static bool ft_broken        = false;
static std::vector<std::string> font_search_dirs;
static const char* default_font_name[] = { "DroidSans", "cour", "Courier New",
                                           "FreeMono", nullptr };

// Helper: given unicode and a font face, compute its size
static ROI
text_size_from_unicode(std::vector<uint32_t>& utext, FT_Face face)
{
    ROI size;
    size.xbegin = size.ybegin = std::numeric_limits<int>::max();
    size.xend = size.yend = std::numeric_limits<int>::min();
    FT_GlyphSlot slot     = face->glyph;
    int x                 = 0;
    for (auto ch : utext) {
        int error = FT_Load_Char(face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        size.ybegin = std::min(size.ybegin, -slot->bitmap_top);
        size.yend   = std::max(size.yend, int(slot->bitmap.rows)
                                            - int(slot->bitmap_top) + 1);
        size.xbegin = std::min(size.xbegin, x + int(slot->bitmap_left));
        size.xend   = std::max(size.xend, x + int(slot->bitmap.width)
                                            + int(slot->bitmap_left) + 1);
        // increment pen position
        x += slot->advance.x >> 6;
    }
    return size;  // Font rendering not supported
}



// Given font name, resolve it to an existing font filename.
// If found, return true and put the resolved filename in result.
// If not found, return false and put an error message in result.
// Not thread-safe! The caller must use the mutex.
static bool
resolve_font(int fontsize, string_view font_, std::string& result)
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

    // A set of likely directories for fonts to live, across several systems.
    // Fill out the list of search dirs if not yet done.
    if (font_search_dirs.size() == 0) {
        string_view home = Sysutil::getenv("HOME");
        if (home.size()) {
            std::string h(home);
            font_search_dirs.push_back(h + "/fonts");
            font_search_dirs.push_back(h + "/Fonts");
            font_search_dirs.push_back(h + "/Library/Fonts");
        }
        string_view systemRoot = Sysutil::getenv("SystemRoot");
        if (systemRoot.size())
            font_search_dirs.push_back(std::string(systemRoot) + "/Fonts");
        font_search_dirs.emplace_back("/usr/share/fonts");
        font_search_dirs.emplace_back("/usr/share/fonts/OpenImageIO");
        font_search_dirs.emplace_back("/Library/Fonts");
        font_search_dirs.emplace_back("/Library/Fonts/OpenImageIO");
        font_search_dirs.emplace_back("C:/Windows/Fonts");
        font_search_dirs.emplace_back("C:/Windows/Fonts/OpenImageIO");
        font_search_dirs.emplace_back("/usr/local/share/fonts");
        font_search_dirs.emplace_back("/usr/local/share/fonts/OpenImageIO");
        font_search_dirs.emplace_back("/opt/local/share/fonts");
        font_search_dirs.emplace_back("/opt/local/share/fonts/OpenImageIO");
        // Try $OPENIMAGEIO_ROOT_DIR/fonts
        string_view OpenImageIOrootdir = Sysutil::getenv("OpenImageIO_ROOT");
        if (OpenImageIOrootdir.size()) {
            font_search_dirs.push_back(std::string(OpenImageIOrootdir)
                                       + "/fonts");
            font_search_dirs.push_back(std::string(OpenImageIOrootdir)
                                       + "/share/fonts/OpenImageIO");
        }
        string_view oiiorootdir = Sysutil::getenv("OPENIMAGEIO_ROOT_DIR");
        if (oiiorootdir.size()) {
            font_search_dirs.push_back(std::string(oiiorootdir) + "/fonts");
            font_search_dirs.push_back(std::string(oiiorootdir)
                                       + "/share/fonts/OpenImageIO");
        }
        // Try $OPENIMAGEIOHOME/fonts -- deprecated (1.9)
        string_view oiiohomedir = Sysutil::getenv("OPENIMAGEIOHOME");
        if (oiiohomedir.size()) {
            font_search_dirs.push_back(std::string(oiiohomedir) + "/fonts");
            font_search_dirs.push_back(std::string(oiiohomedir)
                                       + "/share/fonts/OpenImageIO");
        }
        // Try ../fonts relative to where this executing binary came from
        std::string this_program = OIIO::Sysutil::this_program_path();
        if (this_program.size()) {
            std::string path = Filesystem::parent_path(this_program);
            path             = Filesystem::parent_path(path);
            font_search_dirs.push_back(path + "/fonts");
            font_search_dirs.push_back(path + "/share/fonts/OpenImageIO");
        }
    }

    // Try to find the font.  Experiment with several extensions
    std::string font = font_;
    if (font.empty()) {
        // nothing specified -- look for something to use as a default.
        for (int j = 0; default_font_name[j] && font.empty(); ++j) {
            static const char* extensions[] = { "", ".ttf", ".pfa", ".pfb",
                                                NULL };
            for (int i = 0; font.empty() && extensions[i]; ++i)
                font = Filesystem::searchpath_find(
                    std::string(default_font_name[j]) + extensions[i],
                    font_search_dirs, true, true);
        }
        if (font.empty()) {
            result = "Could not set default font face";
            return false;
        }
    } else if (Filesystem::is_regular(font)) {
        // directly specified a filename -- use it
    } else {
        // A font name was specified but it's not a full path, look for it
        std::string f;
        static const char* extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
        for (int i = 0; f.empty() && extensions[i]; ++i)
            f = Filesystem::searchpath_find(font + extensions[i],
                                            font_search_dirs, true, true);
        if (f.empty()) {
            result = Strutil::sprintf("Could not set font face to \"%s\"",
                                      font);
            return false;
        }
        font = f;
    }

    if (!Filesystem::is_regular(font)) {
        result = Strutil::sprintf("Could not find font \"%s\"", font);
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
    bool ok = resolve_font(fontsize, font_, font);
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

    FT_GlyphSlot slot = face->glyph;  // a small shortcut

    std::vector<uint32_t> utext;
    utext.reserve(
        text.size());  //Possible overcommit, but most text will be ascii
    Strutil::utf8_to_unicode(text, utext);

    size.xbegin = size.ybegin = std::numeric_limits<int>::max();
    size.xend = size.yend = std::numeric_limits<int>::min();
    int x                 = 0;
    for (auto ch : utext) {
        error = FT_Load_Char(face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        size.ybegin = std::min(size.ybegin, -slot->bitmap_top);
        size.yend   = std::max(size.yend, int(slot->bitmap.rows)
                                            - int(slot->bitmap_top) + 1);
        size.xbegin = std::min(size.xbegin, x + int(slot->bitmap_left));
        size.xend   = std::max(size.xend, x + int(slot->bitmap.width)
                                            + int(slot->bitmap_left) + 1);
        // increment pen position
        x += slot->advance.x >> 6;
    }

    FT_Done_Face(face);
#endif

    return size;  // Font rendering not supported
}



bool
ImageBufAlgo::render_text(ImageBuf& R, int x, int y, string_view text,
                          int fontsize, string_view font_,
                          cspan<float> textcolor, TextAlignX alignx,
                          TextAlignY aligny, int shadow, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::render_text");
    if (R.spec().depth > 1) {
        R.errorf("ImageBufAlgo::render_text does not support volume images");
        return false;
    }

#ifdef USE_FREETYPE
    // Thread safety
    lock_guard ft_lock(ft_mutex);

    std::string font;
    bool ok = resolve_font(fontsize, font_, font);
    if (!ok) {
        std::string err = font.size() ? font : "Font error";
        R.errorf("%s", err);
        return false;
    }

    int error = 0;
    FT_Face face;  // handle to face object
    error = FT_New_Face(ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.errorf("Could not set font face to \"%s\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes(face /*handle*/, 0 /*width*/,
                               fontsize /*height*/);
    if (error) {
        FT_Done_Face(face);
        R.errorf("Could not set font size to %d", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    int nchannels(R.nchannels());
    IBA_FIX_PERCHAN_LEN_DEF(textcolor, nchannels);

    // Convert the UTF to 32 bit unicode
    std::vector<uint32_t> utext;
    utext.reserve(
        text.size());  //Possible overcommit, but most text will be ascii
    Strutil::utf8_to_unicode(text, utext);

    // Compute the size that the text will render as, into an ROI
    ROI textroi     = text_size_from_unicode(utext, face);
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
        y += textroi.height();
    if (aligny == TextAlignY::Bottom)
        y -= textroi.height();
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

    // Glyph by glyph, fill in our txtimg buffer
    for (auto ch : utext) {
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
                textimg.setpixel(rx, ry, &b, 1);
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
    float* pixelcolor = OIIO_ALLOCA(float, nchannels);
    ImageBuf::ConstIterator<float> t(textimg, roi, ImageBuf::WrapBlack);
    ImageBuf::ConstIterator<float> a(alphaimg, roi, ImageBuf::WrapBlack);
    ImageBuf::Iterator<float> r(R, roi);
    for (; !r.done(); ++r, ++t, ++a) {
        float val   = t[0];
        float alpha = a[0];
        R.getpixel(r.x(), r.y(), pixelcolor);
        for (int c = 0; c < nchannels; ++c)
            pixelcolor[c] = val * textcolor[c] + (1.0f - alpha) * pixelcolor[c];
        R.setpixel(r.x(), r.y(), pixelcolor);
    }

    FT_Done_Face(face);
    return true;

#else
    R.errorf("OpenImageIO was not compiled with FreeType for font rendering");
    return false;  // Font rendering not supported
#endif
}



OIIO_NAMESPACE_END
