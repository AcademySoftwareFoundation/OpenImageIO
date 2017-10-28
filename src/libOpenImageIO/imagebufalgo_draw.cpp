/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <OpenEXR/half.h>

#include <cmath>
#include <limits>
#include <algorithm>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/hash.h>

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif


OIIO_NAMESPACE_BEGIN


template<typename T>
static bool
fill_const_ (ImageBuf &dst, const float *values, ROI roi=ROI(), int nthreads=1)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = values[c];
    });
    return true;
}


template<typename T>
static bool
fill_tb_ (ImageBuf &dst, const float *top, const float *bottom,
       ROI origroi, ROI roi=ROI(), int nthreads=1)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        float h = std::max (1, origroi.height() - 1);
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            float v = (p.y() - origroi.ybegin) / h;
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = lerp (top[c], bottom[c], v);
        }
    });
    return true;
}


template<typename T>
static bool
fill_corners_ (ImageBuf &dst, const float *topleft, const float *topright,
       const float *bottomleft, const float *bottomright,
       ROI origroi, ROI roi=ROI(), int nthreads=1)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        float w = std::max (1, origroi.width() - 1);
        float h = std::max (1, origroi.height() - 1);
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            float u = (p.x() - origroi.xbegin) / w;
            float v = (p.y() - origroi.ybegin) / h;
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = bilerp (topleft[c], topright[c],
                               bottomleft[c], bottomright[c], u, v);
        }
    });
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi, int nthreads)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "fill", fill_const_, dst.spec().format,
                         dst, pixel, roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *top, const float *bottom,
                    ROI roi, int nthreads)
{
    ASSERT (top && bottom && "fill must have a non-NULL pixel value pointers");
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "fill", fill_tb_, dst.spec().format,
                         dst, top, bottom, roi, roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *topleft, const float *topright,
                    const float *bottomleft, const float *bottomright,
                    ROI roi, int nthreads)
{
    ASSERT (topleft && topright && bottomleft && bottomright &&
            "fill must have a non-NULL pixel value pointers");
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "fill", fill_corners_, dst.spec().format,
                         dst, topleft, topright, bottomleft, bottomright,
                         roi, roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::zero (ImageBuf &dst, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    float *zero = ALLOCA(float,roi.chend);
    memset (zero, 0, roi.chend*sizeof(float));
    return fill (dst, zero, roi, nthreads);
}



template<typename T>
static bool
render_point_ (ImageBuf &dst, int x, int y,
               array_view<const float> color, float alpha,
               ROI roi, int nthreads)
{
    ImageBuf::Iterator<T> r (dst, x, y);
    for (int c = roi.chbegin; c < roi.chend; ++c)
        r[c] = color[c] + r[c] * (1.0f-alpha);  // "over"
    return true;
}



bool
ImageBufAlgo::render_point (ImageBuf &dst, int x, int y,
                            array_view<const float> color,
                            ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;

    if (int(color.size()) < roi.chend) {
        dst.error ("Not enough channels for the color (needed %d)", roi.chend);
        return false;   // Not enough color channels specified
    }
    if (x < roi.xbegin || x >= roi.xend || y < roi.ybegin || y >= roi.yend)
        return true;   // outside of bounds
    const ImageSpec &spec (dst.spec());

    // Alpha: if the image's spec designates an alpha channel, use it if
    // it's within the range specified by color. Otherwise, if color
    // includes more values than the highest channel roi says we should
    // modify, assume the first extra value is alpha. If all else fails,
    // make the line opaque (alpha=1.0).
    float alpha = 1.0f;
    if (spec.alpha_channel >= 0 && spec.alpha_channel < int(color.size()))
        alpha = color[spec.alpha_channel];
    else if (int(color.size()) == roi.chend+1)
        alpha = color[roi.chend];

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "render_point", render_point_, dst.spec().format,
                         dst, x, y, color, alpha, roi, nthreads);
    return ok;
}



// Basic Bresenham 2D line drawing algorithm. Call func(x,y) for each x,y
// along the line from (x1,y1) to (x2,y2). If skip_first is true, don't draw
// the very first point.
template<typename FUNC>
static bool
bresenham2d (FUNC func, int x1, int y1, int x2, int y2,
             bool skip_first=false)
{
    // Basic Bresenham
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int xinc = (x1 > x2) ? -1 : 1;
    int yinc = (y1 > y2) ? -1 : 1;
    if (dx >= dy) {
        int dpr = dy << 1;
        int dpru = dpr - (dx << 1);
        int delta = dpr - dx;
        for (; dx >= 0; --dx) {
            if (skip_first)
                skip_first = false;
            else
                func (x1, y1);
            x1 += xinc;
            if (delta > 0) {
                y1 += yinc;
                delta += dpru;
            } else {
                delta += dpr;
            }
        }
    } else {
        int dpr = dx << 1;
        int dpru = dpr - (dy << 1);
        int delta = dpr - dy;
        for (; dy >= 0; dy--) {
            if (skip_first)
                skip_first = false;
            else
                func (x1, y1);
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
template<typename T>
struct IB_drawer {
    IB_drawer (ImageBuf::Iterator<T,float> &r_,
               array_view<const float> color_, float alpha_, ROI roi_)
        : r(r_), color(color_), alpha(alpha_), roi(roi_) {}

    void operator() (int x, int y) {
        r.pos (x, y);
        if (r.valid())
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = color[c] + r[c] * (1.0f-alpha);  // "over"
    }

    ImageBuf::Iterator<T,float> &r;
    array_view<const float> color;
    float alpha;
    ROI roi;
};



template<typename T>
static bool
render_line_ (ImageBuf &dst, int x1, int y1, int x2, int y2,
              array_view<const float> color, float alpha, bool skip_first,
              ROI roi, int nthreads)
{
    ImageBuf::Iterator<T> r (dst, roi);
    IB_drawer<T> draw (r, color, alpha, roi);
    bresenham2d (draw, x1, y1, x2, y2, skip_first);
    return true;
}



bool
ImageBufAlgo::render_line (ImageBuf &dst, int x1, int y1, int x2, int y2,
                           array_view<const float> color,
                           bool skip_first_point,
                           ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;

    if (int(color.size()) < roi.chend) {
        dst.error ("Not enough channels for the color (needed %d)", roi.chend);
        return false;   // Not enough color channels specified
    }
    const ImageSpec &spec (dst.spec());

    // Alpha: if the image's spec designates an alpha channel, use it if
    // it's within the range specified by color. Otherwise, if color
    // includes more values than the highest channel roi says we should
    // modify, assume the first extra value is alpha. If all else fails,
    // make the line opaque (alpha=1.0).
    float alpha = 1.0f;
    if (spec.alpha_channel >= 0 && spec.alpha_channel < int(color.size()))
        alpha = color[spec.alpha_channel];
    else if (int(color.size()) == roi.chend+1)
        alpha = color[roi.chend];

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "render_line", render_line_, dst.spec().format,
                         dst, x1, y1, x2, y2, color, alpha, skip_first_point,
                         roi, nthreads);
    return ok;
}



template<typename T>
static bool
render_box_ (ImageBuf &dst, array_view<const float> color,
             ROI roi=ROI(), int nthreads=1)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        float alpha = 1.0f;
        if (dst.spec().alpha_channel >= 0 && dst.spec().alpha_channel < int(color.size()))
            alpha = color[dst.spec().alpha_channel];
        else if (int(color.size()) == roi.chend+1)
            alpha = color[roi.chend];

        if (alpha == 1.0f) {
            for (ImageBuf::Iterator<T> r (dst, roi);  !r.done();  ++r)
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    r[c] = color[c];
        } else {
            for (ImageBuf::Iterator<T> r (dst, roi);  !r.done();  ++r)
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    r[c] = color[c] + r[c] * (1.0f-alpha);  // "over"
        }
    });
    return true;
}



bool
ImageBufAlgo::render_box (ImageBuf &dst, int x1, int y1, int x2, int y2,
                          array_view<const float> color, bool fill,
                          ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    if (int(color.size()) < roi.chend) {
        dst.error ("Not enough channels for the color (needed %d)", roi.chend);
        return false;   // Not enough color channels specified
    }

    if (x1 == x2 && y1 == y2) {
        // degenerate 1-point rectangle
        return render_point (dst, x1, y1, color, roi, nthreads);
    }

    // Filled case
    if (fill) {
        roi = roi_intersection (roi, ROI(x1, x2+1, y1, y2+1, 0, 1, 0, roi.chend));
        bool ok;
        OIIO_DISPATCH_TYPES (ok, "render_box", render_box_, dst.spec().format,
                             dst, color, roi, nthreads);
        return ok;
    }

    // Unfilled case: use IBA::render_line
    return ImageBufAlgo::render_line (dst, x1, y1, x2, y1, color,true, roi, nthreads)
        && ImageBufAlgo::render_line (dst, x2, y1, x2, y2, color,true, roi, nthreads)
        && ImageBufAlgo::render_line (dst, x2, y2, x1, y2, color,true, roi, nthreads)
        && ImageBufAlgo::render_line (dst, x1, y2, x1, y1, color,true, roi, nthreads);
}



// Convenient helper struct to bundle a 3-int describing a block size.
struct Dim3 {
    int x, y, z;
    Dim3 (int x, int y=1, int z=1) : x(x), y(y), z(z) { }
};



template<typename T>
static bool
checker_ (ImageBuf &dst, Dim3 size,
          const float *color1, const float *color2,
          Dim3 offset,
          ROI roi, int nthreads=1)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            int xtile = (p.x()-offset.x)/size.x;  xtile += (p.x()<offset.x);
            int ytile = (p.y()-offset.y)/size.y;  ytile += (p.y()<offset.y);
            int ztile = (p.z()-offset.z)/size.z;  ztile += (p.z()<offset.z);
            int v = xtile + ytile + ztile;
            if (v & 1)
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    p[c] = color2[c];
            else
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    p[c] = color1[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::checker (ImageBuf &dst, int width, int height, int depth,
                       const float *color1, const float *color2,
                       int xoffset, int yoffset, int zoffset,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "checker", checker_, dst.spec().format,
                         dst, Dim3(width, height, depth), color1, color2,
                         Dim3(xoffset, yoffset, zoffset), roi, nthreads);
    return ok;
}



// Return a repeatable hash-based pseudo-random value uniform on [0,1).
// It's a hash, so it's completely deterministic, based on x,y,z,c,seed.
// But it can be used in similar ways to a PRNG.
OIIO_FORCEINLINE float
hashrand (int x, int y, int z, int c, int seed)
{
    const uint32_t magic = 0xfffff;
    uint32_t xu(x), yu(y), zu(z), cu(c), seedu(seed);
    using bjhash::bjfinal;
    uint32_t h = bjfinal (bjfinal(xu,yu,zu), cu, seedu) & magic;
    return h * (1.0f/(magic+1));
}


// Return a hash-based normal-distributed pseudorandom value.
// We use the Marsaglia polar method, and hashrand to
OIIO_FORCEINLINE float
hashnormal (int x, int y, int z, int c, int seed)
{
    float xr, yr, r2;
    int s = seed-1;
    do {
        s += 1;
        xr = 2.0 * hashrand(x,y,z,c,s) - 1.0;
        yr = 2.0 * hashrand(x,y,z,c,s+139) - 1.0;
        r2 = xr*xr + yr*yr;
    } while (r2 > 1.0 || r2 == 0.0);
    float M = sqrt(-2.0 * log(r2) / r2);
    return xr * M;
}



template<typename T>
static bool
noise_uniform_ (ImageBuf &dst, float min, float max, bool mono,
                int seed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                if (c == roi.chbegin || !mono)
                    n = lerp (min, max, hashrand (x, y, z, c, seed));
                p[c] = p[c] + n;
            }
        }
    });
    return true;
}



template<typename T>
static bool
noise_gaussian_ (ImageBuf &dst, float mean, float stddev, bool mono,
                 int seed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                if (c == roi.chbegin || !mono)
                    n = mean + stddev * hashnormal (x, y, z, c, seed);
                p[c] = p[c] + n;
            }
        }
    });
    return true;
}



template<typename T>
static bool
noise_salt_ (ImageBuf &dst, float saltval, float saltportion, bool mono,
             int seed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
            int x = p.x(), y = p.y(), z = p.z();
            float n = 0.0;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                if (c == roi.chbegin || !mono)
                    n = hashrand (x, y, z, c, seed);
                if (n < saltportion)
                    p[c] = saltval;
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::noise (ImageBuf &dst, string_view noisetype,
                     float A, float B, bool mono, int seed,
                     ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    if (noisetype == "gaussian" || noisetype == "normal") {
        OIIO_DISPATCH_TYPES (ok, "noise_gaussian", noise_gaussian_, dst.spec().format,
                             dst, A, B, mono, seed, roi, nthreads);
    } else if (noisetype == "uniform") {
        OIIO_DISPATCH_TYPES (ok, "noise_uniform", noise_uniform_, dst.spec().format,
                             dst, A, B, mono, seed, roi, nthreads);
    } else if (noisetype == "salt") {
        OIIO_DISPATCH_TYPES (ok, "noise_salt", noise_salt_, dst.spec().format,
                             dst, A, B, mono, seed, roi, nthreads);
    } else {
        ok = false;
        dst.error ("noise", "unknown noise type \"%s\"", noisetype);
    }
    return ok;
}




#ifdef USE_FREETYPE
namespace { // anon
static mutex ft_mutex;
static FT_Library ft_library = NULL;
static bool ft_broken = false;
static std::vector<std::string> font_search_dirs;
static const char * default_font_name[] = {
        "DroidSans", "cour", "Courier New", "FreeMono", NULL
     };

// Helper: given unicode and a font face, compute its size
static ROI
text_size_from_unicode (std::vector<uint32_t> &utext, FT_Face face)
{
    ROI size;
    size.xbegin = size.ybegin = std::numeric_limits<int>::max();
    size.xend = size.yend = std::numeric_limits<int>::min();
    FT_GlyphSlot slot = face->glyph;
    int x = 0;
    for (auto ch : utext) {
        int error = FT_Load_Char (face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        size.ybegin = std::min (size.ybegin, -slot->bitmap_top);
        size.yend = std::max (size.yend, int(slot->bitmap.rows) - int(slot->bitmap_top) + 1);
        size.xbegin = std::min (size.xbegin, x + int(slot->bitmap_left));
        size.xend = std::max (size.xend, x + int(slot->bitmap.width) + int(slot->bitmap_left) + 1);
        // increment pen position
        x += slot->advance.x >> 6;
    }
    return size;   // Font rendering not supported
}

} // anon namespace
#endif


// Given font name, resolve it to an existing font filename.
// If found, return true and put the resolved filename in result.
// If not found, return false and put an error message in result.
// Not thread-safe! The caller must use the mutex.
static bool
resolve_font (int fontsize, string_view font_, std::string &result)
{
    result.clear ();
#ifdef USE_FREETYPE
    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return false;

    // If FT not yet initialized, do it now.
    if (! ft_library) {
        if (FT_Init_FreeType (&ft_library)) {
            ft_broken = true;
            result = "Could not initialize FreeType for font rendering";
            return false;
        }
    }

    // A set of likely directories for fonts to live, across several systems.
    // Fill out the list of search dirs if not yet done.
    if (font_search_dirs.size() == 0) {
        string_view home = Sysutil::getenv ("HOME");
        if (home.size()) {
            std::string h (home);
            font_search_dirs.push_back (h + "/fonts");
            font_search_dirs.push_back (h + "/Fonts");
            font_search_dirs.push_back (h + "/Library/Fonts");
        }
        string_view systemRoot = Sysutil::getenv ("SystemRoot");
        if (systemRoot.size())
            font_search_dirs.push_back (std::string(systemRoot) + "/Fonts");
        font_search_dirs.emplace_back ("/usr/share/fonts");
        font_search_dirs.emplace_back ("/usr/share/fonts/OpenImageIO");
        font_search_dirs.emplace_back ("/Library/Fonts");
        font_search_dirs.emplace_back ("/Library/Fonts/OpenImageIO");
        font_search_dirs.emplace_back ("C:/Windows/Fonts");
        font_search_dirs.emplace_back ("C:/Windows/Fonts/OpenImageIO");
        font_search_dirs.emplace_back ("/usr/local/share/fonts");
        font_search_dirs.emplace_back ("/usr/local/share/fonts/OpenImageIO");
        font_search_dirs.emplace_back ("/opt/local/share/fonts");
        font_search_dirs.emplace_back ("/opt/local/share/fonts/OpenImageIO");
        // Try $OPENIMAGEIOHOME/fonts
        string_view oiiohomedir = Sysutil::getenv ("OPENIMAGEIOHOME");
        if (oiiohomedir.size()) {
            font_search_dirs.push_back (std::string(oiiohomedir) + "/fonts");
            font_search_dirs.push_back (std::string(oiiohomedir) + "/share/fonts/OpenImageIO");
        }
        // Try ../fonts relative to where this executing binary came from
        std::string this_program = OIIO::Sysutil::this_program_path ();
        if (this_program.size()) {
            std::string path = Filesystem::parent_path (this_program);
            path = Filesystem::parent_path (path);
            font_search_dirs.push_back (path+"/fonts");
            font_search_dirs.push_back (path+"/shared/fonts/OpenImageIO");
        }
    }

    // Try to find the font.  Experiment with several extensions
    std::string font = font_;
    if (font.empty()) {
        // nothing specified -- look for something to use as a default.
        for (int j = 0;  default_font_name[j] && font.empty(); ++j) {
            static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
            for (int i = 0;  font.empty() && extensions[i];  ++i)
                font = Filesystem::searchpath_find (std::string(default_font_name[j])+extensions[i],
                                                 font_search_dirs, true, true);
        }
        if (font.empty()) {
            result = "Could not set default font face";
            return false;
        }
    } else if (Filesystem::is_regular (font)) {
        // directly specified a filename -- use it
    } else {
        // A font name was specified but it's not a full path, look for it
        std::string f;
        static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
        for (int i = 0;  f.empty() && extensions[i];  ++i)
            f = Filesystem::searchpath_find (font+extensions[i],
                                             font_search_dirs, true, true);
        if (f.empty()) {
            result = Strutil::format ("Could not set font face to \"%s\"", font);
            return false;
        }
        font = f;
    }

    ASSERT (! font.empty());
    if (! Filesystem::is_regular (font)) {
        result = Strutil::format ("Could not find font \"%s\"", font);
        return false;
    }

    // Success
    result = font;
    return true;
#else
    return false;
#endif
}



ROI
ImageBufAlgo::text_size (string_view text, int fontsize, string_view font_)
{
    ROI size;
#ifdef USE_FREETYPE
    // Thread safety
    lock_guard ft_lock (ft_mutex);

    std::string font;
    bool ok = resolve_font (fontsize, font_, font);
    if (! ok) {
        return size;
    }

    int error = 0;
    FT_Face face;      // handle to face object
    error = FT_New_Face (ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        return size;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes (face /*handle*/, 0 /*width*/, fontsize/*height*/);
    if (error) {
        FT_Done_Face (face);
        return size;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut

    std::vector<uint32_t> utext;
    utext.reserve(text.size()); //Possible overcommit, but most text will be ascii
    Strutil::utf8_to_unicode(text, utext);

    size.xbegin = size.ybegin = std::numeric_limits<int>::max();
    size.xend = size.yend = std::numeric_limits<int>::min();
    int x = 0;
    for (auto ch : utext) {
        error = FT_Load_Char (face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        size.ybegin = std::min (size.ybegin, -slot->bitmap_top);
        size.yend = std::max (size.yend, int(slot->bitmap.rows) - int(slot->bitmap_top) + 1);
        size.xbegin = std::min (size.xbegin, x + int(slot->bitmap_left));
        size.xend = std::max (size.xend, x + int(slot->bitmap.width) + int(slot->bitmap_left) + 1);
        // increment pen position
        x += slot->advance.x >> 6;
    }

    FT_Done_Face (face);
#endif

    return size;   // Font rendering not supported
}



// Deprecated version
bool
ImageBufAlgo::render_text (ImageBuf &R, int x, int y, string_view text,
                           int fontsize, string_view font_,
                           const float *textcolor)
{
    array_view<const float> color;
    if (textcolor)
        color = array_view<const float>(textcolor,R.nchannels());
    return render_text (R, x, y, text, fontsize, font_, color);
}



bool
ImageBufAlgo::render_text (ImageBuf &R, int x, int y, string_view text,
                           int fontsize, string_view font_,
                           array_view<const float> textcolor,
                           TextAlignX alignx, TextAlignY aligny,
                           int shadow, ROI roi, int nthreads)
{
    if (R.spec().depth > 1) {
        R.error ("ImageBufAlgo::render_text does not support volume images");
        return false;
    }

#ifdef USE_FREETYPE
    // Thread safety
    lock_guard ft_lock (ft_mutex);

    std::string font;
    bool ok = resolve_font (fontsize, font_, font);
    if (! ok) {
        std::string err = font.size() ? font : "Font error";
        R.error ("%s", err);
        return false;
    }

    int error = 0;
    FT_Face face;      // handle to face object
    error = FT_New_Face (ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.error ("Could not set font face to \"%s\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes (face /*handle*/, 0 /*width*/, fontsize/*height*/);
    if (error) {
        FT_Done_Face (face);
        R.error ("Could not set font size to %d", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    size_t nchannels (R.spec().nchannels);
    if (textcolor.size() <= nchannels) {
        float *localtextcolor = ALLOCA (float, nchannels);
        for (size_t c = 0;  c < nchannels;  ++c)
            localtextcolor[c] = c < textcolor.size() ? textcolor[c] : 1.0f;
        textcolor = array_view<const float>(localtextcolor, nchannels);
    }

    // Convert the UTF to 32 bit unicode
    std::vector<uint32_t> utext;
    utext.reserve(text.size()); //Possible overcommit, but most text will be ascii
    Strutil::utf8_to_unicode(text, utext);

    // Compute the size that the text will render as, into an ROI
    ROI textroi = text_size_from_unicode (utext, face);
    textroi.zbegin = 0; textroi.zend = 1;
    textroi.chbegin = 0; textroi.chend = 1;

    // Adjust position for alignment requests
    if (alignx == TextAlignX::Right)
        x -= textroi.width();
    if (alignx == TextAlignX::Center)
        x -= (textroi.width()/2 + textroi.xbegin);
    if (aligny == TextAlignY::Top)
        y += textroi.height();
    if (aligny == TextAlignY::Bottom)
        y -= textroi.height();
    if (aligny == TextAlignY::Center)
        y -= (textroi.height()/2 + textroi.ybegin);

    // Pad bounds for shadowing
    textroi.xbegin += x - shadow;  textroi.xend += x + shadow;
    textroi.ybegin += y - shadow;  textroi.yend += y + shadow;

    // Create a temp buffer of the right size and render the text into it.
    ImageBuf textimg (ImageSpec(textroi, TypeDesc::FLOAT));
    ImageBufAlgo::zero (textimg);

    // Glyph by glyph, fill in our txtimg buffer
    for (auto ch : utext) {
        int error = FT_Load_Char (face, ch, FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        // now, draw to our target surface
        for (int j = 0;  j < static_cast<int>(slot->bitmap.rows); ++j) {
            int ry = y + j - slot->bitmap_top;
            for (int i = 0;  i < static_cast<int>(slot->bitmap.width); ++i) {
                int rx = x + i + slot->bitmap_left;
                float b = slot->bitmap.buffer[slot->bitmap.pitch*j+i] / 255.0f;
                textimg.setpixel (rx, ry, &b, 1);
            }
        }
        // increment pen position
        x += slot->advance.x >> 6;
    }

    // Generate the alpha image -- if drop shadow is requested, dilate,
    // otherwise it's just a copy of the text image
    ImageBuf alphaimg;
    if (shadow)
        dilate (alphaimg, textimg, 2*shadow+1);
    else
        alphaimg.copy (textimg);

    if (! roi.defined())
        roi = textroi;
    if (! IBAprep (roi, &R))
        return false;
    roi = roi_intersection (textroi, R.roi());

    // Now fill in the pixels of our destination image
    float *pixelcolor = ALLOCA (float, nchannels);
    ImageBuf::ConstIterator<float> t (textimg, roi, ImageBuf::WrapBlack);
    ImageBuf::ConstIterator<float> a (alphaimg, roi, ImageBuf::WrapBlack);
    ImageBuf::Iterator<float> r (R, roi);
    for ( ;  !r.done();  ++r, ++t, ++a) {
        float val = t[0];
        float alpha = a[0];
        R.getpixel (r.x(), r.y(), pixelcolor);
        for (size_t c = 0;  c < nchannels;  ++c)
            pixelcolor[c] = val*textcolor[c] + (1.0f-alpha) * pixelcolor[c];
        R.setpixel (r.x(), r.y(), pixelcolor);
    }

    FT_Done_Face (face);
    return true;

#else
    R.error ("OpenImageIO was not compiled with FreeType for font rendering");
    return false;   // Font rendering not supported
#endif
}



OIIO_NAMESPACE_END
