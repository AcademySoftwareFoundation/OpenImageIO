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

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>

#include <OpenEXR/half.h>

#include <cmath>

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/imagebufalgo_util.h"
#include "OpenImageIO/dassert.h"
#include "OpenImageIO/sysutil.h"
#include "OpenImageIO/filter.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/filesystem.h"
#include "OpenImageIO/hash.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif


OIIO_NAMESPACE_ENTER {


template<typename T>
static bool
fill_ (ImageBuf &dst, const float *values, ROI roi=ROI(), int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fill_<T>, boost::ref(dst), values,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            p[c] = values[c];
    return true;
}


template<typename T>
static bool
fill_ (ImageBuf &dst, const float *top, const float *bottom,
       ROI origroi, ROI roi=ROI(), int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fill_<T>, boost::ref(dst), top, bottom,
                        origroi, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    float h = std::max (1, origroi.height() - 1);
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        float v = (p.y() - origroi.ybegin) / h;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            p[c] = lerp (top[c], bottom[c], v);
    }
    return true;
}


template<typename T>
static bool
fill_ (ImageBuf &dst, const float *topleft, const float *topright,
       const float *bottomleft, const float *bottomright,
       ROI origroi, ROI roi=ROI(), int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fill_<T>, boost::ref(dst), topleft, topright,
                        bottomleft, bottomright,
                        origroi, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    float w = std::max (1, origroi.width() - 1);
    float h = std::max (1, origroi.height() - 1);
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        float u = (p.x() - origroi.xbegin) / w;
        float v = (p.y() - origroi.ybegin) / h;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            p[c] = bilerp (topleft[c], topright[c],
                           bottomleft[c], bottomright[c], u, v);
    }
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi, int nthreads)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "fill", fill_, dst.spec().format,
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
    OIIO_DISPATCH_TYPES (ok, "fill", fill_, dst.spec().format,
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
    OIIO_DISPATCH_TYPES (ok, "fill", fill_, dst.spec().format,
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(checker_<T>, boost::ref(dst),
                        size, color1, color2, offset,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(noise_uniform_<T>, boost::ref(dst),
                        min, max, mono, seed,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        int x = p.x(), y = p.y(), z = p.z();
        float n = 0.0;
        for (int c = roi.chbegin;  c < roi.chend;  ++c) {
            if (c == roi.chbegin || !mono)
                n = lerp (min, max, hashrand (x, y, z, c, seed));
            p[c] = p[c] + n;
        }
    }
    return true;
}



template<typename T>
static bool
noise_gaussian_ (ImageBuf &dst, float mean, float stddev, bool mono,
                 int seed, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(noise_gaussian_<T>, boost::ref(dst),
                        mean, stddev, mono, seed,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        int x = p.x(), y = p.y(), z = p.z();
        float n = 0.0;
        for (int c = roi.chbegin;  c < roi.chend;  ++c) {
            if (c == roi.chbegin || !mono)
                n = mean + stddev * hashnormal (x, y, z, c, seed);
            p[c] = p[c] + n;
        }
    }
    return true;
}



template<typename T>
static bool
noise_salt_ (ImageBuf &dst, float saltval, float saltportion, bool mono,
             int seed, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(noise_salt_<T>, boost::ref(dst),
                        saltval, saltportion, mono, seed,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
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
static const char * default_font_name[] = {
        "cour", "Courier New", "FreeMono", NULL
     };
} // anon namespace
#endif


bool
ImageBufAlgo::render_text (ImageBuf &R, int x, int y, string_view text,
                           int fontsize, string_view font_,
                           const float *textcolor)
{
    if (R.spec().depth > 1) {
        R.error ("ImageBufAlgo::render_text does not support volume images");
        return false;
    }

#ifdef USE_FREETYPE
    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return false;

    // Thread safety
    lock_guard ft_lock (ft_mutex);
    int error = 0;

    // If FT not yet initialized, do it now.
    if (! ft_library) {
        error = FT_Init_FreeType (&ft_library);
        if (error) {
            ft_broken = true;
            R.error ("Could not initialize FreeType for font rendering");
            return false;
        }
    }

    // A set of likely directories for fonts to live, across several systems.
    std::vector<std::string> search_dirs;
    const char *home = getenv ("HOME");
    if (home && *home) {
        std::string h (home);
        search_dirs.push_back (h + "/fonts");
        search_dirs.push_back (h + "/Fonts");
        search_dirs.push_back (h + "/Library/Fonts");
    }
    const char *systemRoot = getenv ("SystemRoot");
    if (systemRoot && *systemRoot) {
        std::string sysroot (systemRoot);
        search_dirs.push_back (sysroot + "/Fonts");
    }
    search_dirs.push_back ("/usr/share/fonts");
    search_dirs.push_back ("/Library/Fonts");
    search_dirs.push_back ("C:/Windows/Fonts");
    search_dirs.push_back ("/opt/local/share/fonts");

    // Try to find the font.  Experiment with several extensions
    std::string font = font_;
    if (font.empty()) {
        // nothing specified -- look for something to use as a default.
        for (int j = 0;  default_font_name[j] && font.empty(); ++j) {
            static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
            for (int i = 0;  font.empty() && extensions[i];  ++i)
                font = Filesystem::searchpath_find (std::string(default_font_name[j])+extensions[i],
                                                 search_dirs, true, true);
        }
        if (font.empty()) {
            R.error ("Could not set default font face");
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
                                             search_dirs, true, true);
        if (f.empty()) {
            R.error ("Could not set font face to \"%s\"", font);
            return false;
        }
        font = f;
    }

    ASSERT (! font.empty());
    if (! Filesystem::is_regular (font)) {
        R.error ("Could not find font \"%s\"", font);
        return false;
    }

    FT_Face face;      // handle to face object
    error = FT_New_Face (ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.error ("Could not set font face to \"%s\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes (face,        // handle to face object
                                0,           // pixel_width
                                fontsize);   // pixel_heigh
    if (error) {
        FT_Done_Face (face);
        R.error ("Could not set font size to %d", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    int nchannels = R.spec().nchannels;
    float *pixelcolor = ALLOCA (float, nchannels);
    if (! textcolor) {
        float *localtextcolor = ALLOCA (float, nchannels);
        for (int c = 0;  c < nchannels;  ++c)
            localtextcolor[c] = 1.0f;
        textcolor = localtextcolor;
    }

    std::vector<uint32_t> utext;
    utext.reserve(text.size()); //Possible overcommit, but most text will be ascii
    Strutil::utf8_to_unicode(text, utext);

    for (size_t n = 0, e = utext.size();  n < e;  ++n) {
        error = FT_Load_Char (face, utext[n], FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        // now, draw to our target surface
        for (int j = 0;  j < static_cast<int>(slot->bitmap.rows); ++j) {
            int ry = y + j - slot->bitmap_top;
            for (int i = 0;  i < static_cast<int>(slot->bitmap.width); ++i) {
                int rx = x + i + slot->bitmap_left;
                float b = slot->bitmap.buffer[slot->bitmap.pitch*j+i] / 255.0f;
                R.getpixel (rx, ry, pixelcolor);
                for (int c = 0;  c < nchannels;  ++c)
                    pixelcolor[c] = b*textcolor[c] + (1.0f-b) * pixelcolor[c];
                R.setpixel (rx, ry, pixelcolor);
            }
        }
        // increment pen position
        x += slot->advance.x >> 6;
    }

    FT_Done_Face (face);
    return true;

#else
    R.error ("OpenImageIO was not compiled with FreeType for font rendering");
    return false;   // Font rendering not supported
#endif
}



} OIIO_NAMESPACE_EXIT
