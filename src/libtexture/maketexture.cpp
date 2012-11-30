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

#include "maketexture.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>

#include "dassert.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"
#include "color.h"
#include "timer.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "thread.h"


OIIO_NAMESPACE_ENTER
{


// Ugly global to track inf/nan pixels. Used inside of parallel_image
// and requires mutex. Consider moving into MaketxStats, which then
// need to be passed to parallel_image?
static int found_nonfinite = 0;
static spin_mutex maketx_mutex;   // for found_nonfinite



// forward decl
static bool write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
                          ImageOutput *out, TypeDesc outputdatatype,
                          bool mipmap, const Filter2D *filter,
                          const MaketxParams &param, MaketxStats *stat);



static std::string
datestring (time_t t)
{
    struct tm mytm;
    Sysutil::get_local_time (&t, &mytm);
    return Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                            mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                            mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
}



// Run func over all pixels of dst, but split into separate threads for
// bands of the image.  Assumes that the calling profile of func is:
//     func (dst, src, xbegin, xend, ybegin, yend, filter);
// Also assumes that every pixel processed is approximately the same
// cost, so it just divides the image space into equal-sized bands without
// worrying about any sophisticated load balancing.
template <class Func>
void
parallel_image (Func func, ImageBuf *dst, const ImageBuf *src,
                int xbegin, int xend, int ybegin, int yend, int nthreads,
                const MaketxParams *param)
{
    const ImageSpec &dstspec (dst->spec());
    
    // Don't parallelize with too few pixels
    if (dstspec.image_pixels() < 1000)
        nthreads = 1;
    // nthreads < 1 means try to make enough threads to fill all cores
    if (nthreads < 1) {
        nthreads = boost::thread::hardware_concurrency();
    }
    
    if (nthreads > 1) {
        boost::thread_group threads;
        int blocksize = std::max (1, ((xend-xbegin) + nthreads-1) / nthreads);
        for (int i = 0;  i < nthreads;  ++i) {
            int x0 = xbegin + i*blocksize;
            int x1 = std::min (xbegin + (i+1)*blocksize, xend);
            //            std::cerr << "  launching " << x0 << ' ' << x1 << ' '
            //                      << ybegin << ' ' << yend << "\n";
            threads.add_thread (new boost::thread (func, dst, src,
                                                   x0, x1,
                                                   ybegin, yend, param));
        }
        threads.join_all ();
    } else {
        func (dst, src, xbegin, xend, ybegin, yend, param);
    }
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static void
copy_block (ImageBuf *dst, const ImageBuf *src,
            int x0, int x1, int y0, int y1, const MaketxParams *param)
{
//    const ImageSpec &dstspec (dst->spec());
//    float *pel = (float *) alloca (dstspec.pixel_bytes());
    float pel[256];
    for (int y = y0;  y < y1;  ++y) {
        for (int x = x0;  x < x1;  ++x) {
            src->getpixel (x, y, pel);
            dst->setpixel (x, y, pel);
        }
    }
}



// Resize src into dst using a good quality filter,
// for the pixel range [x0,x1) x [y0,y1).
static void
resize_block_HQ (ImageBuf *dst, const ImageBuf *src,
                 int x0, int x1, int y0, int y1, const MaketxParams *param)
{
    ImageBufAlgo::resize (*dst, *src, x0, x1, y0, y1, param->filter);
}



static void
interppixel_NDC_clamped (const ImageBuf &buf, float x, float y,
                         MaketxParams::ConversionMode mode, float *pixel)
{
    
    int fx = buf.spec().full_x;
    int fy = buf.spec().full_y;
    int fw = buf.spec().full_width;
    int fh = buf.spec().full_height;
    x = static_cast<float>(fx) + x * static_cast<float>(fw);
    y = static_cast<float>(fy) + y * static_cast<float>(fh);
    
    const int maxchannels = 64;  // Reasonable guess
    float p[4][maxchannels];
    DASSERT (buf.spec().nchannels <= maxchannels &&
             "You need to increase maxchannels");
    int n = std::min (buf.spec().nchannels, maxchannels);
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    // Clamp
    int xnext = Imath::clamp (xtexel+1, buf.xmin(), buf.xmax());
    int ynext = Imath::clamp (ytexel+1, buf.ymin(), buf.ymax());
    xtexel = std::max (xtexel, buf.xmin());
    ytexel = std::max (ytexel, buf.ymin());
    // Get the four texels
    buf.getpixel (xtexel, ytexel, p[0], n);
    buf.getpixel (xnext, ytexel, p[1], n);
    buf.getpixel (xtexel, ynext, p[2], n);
    buf.getpixel (xnext, ynext, p[3], n);
    if (mode == MaketxParams::ENVLATLONG) {
        // For latlong environment maps, in order to conserve energy, we
        // must weight the pixels by sin(t*PI) because pixels closer to
        // the pole are actually less area on the sphere. Doing this
        // wrong will tend to over-represent the high latitudes in
        // low-res MIP levels.  We fold the area weighting into our
        // linear interpolation by adjusting yfrac.
        float w0 = (1.0f - yfrac) * sinf ((float)M_PI * (ytexel+0.5f)/(float)fh);
        float w1 = yfrac * sinf ((float)M_PI * (ynext+0.5f)/(float)fh);
        yfrac = w0 / (w0 + w1);
    }
    // Bilinearly interpolate
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
}



// Resize src into dst, relying on the linear interpolation of
// interppixel_NDC_full, for the pixel range [x0,x1) x [y0,y1).
static void
resize_block (ImageBuf *dst, const ImageBuf *src,
              int x0, int x1, int y0, int y1, const MaketxParams *param)
{
    const ImageSpec &dstspec (dst->spec());
//    float *pel = (float *) alloca (dstspec.pixel_bytes());
    float pel[256];
    float xoffset = dstspec.full_x;
    float yoffset = dstspec.full_y;
    float xscale = 1.0f / (float)dstspec.full_width;
    float yscale = 1.0f / (float)dstspec.full_height;
    for (int y = y0;  y < y1;  ++y) {
        float t = (y+0.5f)*yscale + yoffset;
        for (int x = x0;  x < x1;  ++x) {
            float s = (x+0.5f)*xscale + xoffset;
            interppixel_NDC_clamped (*src, s, t, param->conversionmode, pel);
            dst->setpixel (x, y, pel);
        }
    }
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static void
check_nan_block (ImageBuf* /*dst*/, const ImageBuf* src,
                 int x0, int x1, int y0, int y1, const MaketxParams *param)
{
    const ImageSpec &spec (src->spec());
//    float *pel = (float *) alloca (spec.pixel_bytes());
    float pel[256];
    for (int y = y0;  y < y1;  ++y) {
        for (int x = x0;  x < x1;  ++x) {
            src->getpixel (x, y, pel);
            for (int c = 0;  c < spec.nchannels;  ++c) {
                if (! isfinite(pel[c])) {
                    spin_lock lock (maketx_mutex);
                    if (found_nonfinite < 3)
                        std::cerr << "maketx ERROR: Found " << pel[c]
                        << " at (x=" << x << ", y=" << y << ")\n";
                    ++found_nonfinite;
                    break;  // skip other channels, there's no point
                }
            }
        }
    }
}



static void
fix_latl_edges (ImageBuf &buf)
{
    //  ASSERT (envlatlmode && "only call fix_latl_edges for latlong maps");
    int n = buf.nchannels();
//    float *left = ALLOCA (float, n);
//    float *right = ALLOCA (float, n);
    float left[256], right[256];
    
    // Make the whole first and last row be solid, since they are exactly
    // on the pole
    float wscale = 1.0f / (buf.spec().width);
    for (int j = 0;  j <= 1;  ++j) {
        int y = (j==0) ? buf.ybegin() : buf.yend()-1;
        // use left for the sum, right for each new pixel
        for (int c = 0;  c < n;  ++c)
            left[c] = 0.0f;
        for (int x = buf.xbegin();  x < buf.xend();  ++x) {
            buf.getpixel (x, y, right);
            for (int c = 0;  c < n;  ++c)
                left[c] += right[c];
        }
        for (int c = 0;  c < n;  ++c)
            left[c] += right[c];
        for (int c = 0;  c < n;  ++c)
            left[c] *= wscale;
        for (int x = buf.xbegin();  x < buf.xend();  ++x)
            buf.setpixel (x, y, left);
    }
    
    // Make the left and right match, since they are both right on the
    // prime meridian.
    for (int y = buf.ybegin();  y < buf.yend();  ++y) {
        buf.getpixel (buf.xbegin(), y, left);
        buf.getpixel (buf.xend()-1, y, right);
        for (int c = 0;  c < n;  ++c)
            left[c] = 0.5f * left[c] + 0.5f * right[c];
        buf.setpixel (buf.xbegin(), y, left);
        buf.setpixel (buf.xend()-1, y, left);
    }
}



static std::string
formatres (const ImageSpec &spec, bool extended=false)
{
    std::string s;
    s = Strutil::format("%dx%d", spec.width, spec.height);
    if (extended) {
        if (spec.x || spec.y)
            s += Strutil::format("%+d%+d", spec.x, spec.y);
        if (spec.width != spec.full_width || spec.height != spec.full_height ||
            spec.x != spec.full_x || spec.y != spec.full_y) {
            s += " (full/display window is ";
            s += Strutil::format("%dx%d", spec.full_width, spec.full_height);
            if (spec.full_x || spec.full_y)
                s += Strutil::format("%+d%+d", spec.full_x, spec.full_y);
            s += ")";
        }
    }
    return s;
}



bool
make_texturemap (ImageBuf &src, ImageOutput *out,
                 const MaketxParams &param, MaketxStats *stat)
{
    if (!out)
        return false;
    if (param.prman && param.oiio)
        return false;
    
    // The cache might mess with the apparent data format.  But for the
    // purposes of what we should output, figure it out now, before the
    // file has been read and cached.
    TypeDesc out_dataformat = src.spec().format;
    
    // Figure out which data format we want for output
    if (! param.dataformatname.empty()) {
        if (param.dataformatname == "uint8")
            out_dataformat = TypeDesc::UINT8;
        else if (param.dataformatname == "int8" ||
                 param.dataformatname == "sint8")
            out_dataformat = TypeDesc::INT8;
        else if (param.dataformatname == "uint16")
            out_dataformat = TypeDesc::UINT16;
        else if (param.dataformatname == "int16" ||
                 param.dataformatname == "sint16")
            out_dataformat = TypeDesc::INT16;
        else if (param.dataformatname == "half")
            out_dataformat = TypeDesc::HALF;
        else if (param.dataformatname == "float")
            out_dataformat = TypeDesc::FLOAT;
        else if (param.dataformatname == "double")
            out_dataformat = TypeDesc::DOUBLE;
    }
    
    // Potentially modified parameter values.
    // This also why we moved the prman & oiio value setting to this function.
    int tile_width = param.tile_width;
    int tile_height = param.tile_height;
    bool separate = param.separate;
    bool prman_metadata = param.prman_metadata;
    bool constant_color_detect = param.constant_color_detect;
    std::string swrap = param.swrap;
    std::string twrap = param.twrap;
    float fovcot = param.fovcot;
    Filter2D *filter = param.filter;
    bool local_filter = false;
    if (filter == NULL) {
        filter = Filter2D::create ("box", 1, 1);
        local_filter = true;
    }

    // We cannot compute the prman / oiio options until after out_dataformat
    // has been determined, as it's required (and can potentially change
    // out_dataformat too!)
    assert(!(param.prman && param.oiio));
    if (param.prman) {
        // Force planar image handling, and also emit prman metadata
        separate = true;
        prman_metadata = true;
        
        // 8-bit : 64x64
        if (out_dataformat == TypeDesc::UINT8 ||
            out_dataformat == TypeDesc::INT8) {
            tile_width = 64;
            tile_height = 64;
        }
        
        // 16-bit : 64x32
        // Force u16 -> s16
        // In prman's txmake (last tested in 15.0)
        // specifying -short creates a signed int representation
        if (out_dataformat == TypeDesc::UINT16) {
            out_dataformat = TypeDesc::INT16;
        }
        
        if (out_dataformat == TypeDesc::UINT16 ||
            out_dataformat == TypeDesc::INT16) {
            tile_width = 64;
            tile_height = 32;
        }
        
        // Float: 32x32
        // In prman's txmake (last tested in 15.0)
        // specifying -half or -float make 32x32 tile size
        if (out_dataformat == TypeDesc::HALF ||
            out_dataformat == TypeDesc::FLOAT ||
            out_dataformat == TypeDesc::DOUBLE) {
            tile_width = 32;
            tile_height = 32;
        }
    } else if (param.oiio) {
        // Interleaved channels are faster to read
        separate = false;
        
        // Enable constant color optimizations
        constant_color_detect = true;
        
        // Force fixed tile-size across the board
        tile_width = 64;
        tile_height = 64;
    }
    
    // Read the full file locally if it's less than 1 GB, otherwise
    // allow the ImageBuf to use ImageCache to manage memory.
    bool read_local = (src.spec().image_bytes() < param.readlocalbytes);
    
    if (param.verbose)
        std::cout << "Reading file: " << src.name() << std::endl;
    Timer readtimer;
    if (! src.read (0, 0, read_local)) {
        std::cerr
        << "maketx ERROR: Could not read \""
        << src.name() << "\" : " << src.geterror() << "\n";
        return false;
    }
    if (stat)
        stat->readtime += readtimer();
    
    // If requested - and we're a constant color - make a tiny texture instead
    std::vector<float> constantColor(src.nchannels());
    bool isConstantColor = ImageBufAlgo::isConstantColor (src, &constantColor[0]);
    
    if (isConstantColor && constant_color_detect) {
        ImageSpec newspec = src.spec();
        
        // Reset the image, to a new image, at the new size
        std::string name = src.name() + ".constant_color";
        src.reset(name, newspec);
        
        ImageBufAlgo::fill (src, &constantColor[0]);
        
        if (param.verbose) {
            std::cout << "  Constant color image detected. ";
            std::cout << "Creating " << newspec.width << "x" << newspec.height << " texture instead.\n";
        }
    }
    
    // If requested -- and alpha is 1.0 everywhere -- drop it.
    if (param.opaque_detect && src.spec().alpha_channel == src.nchannels()-1 &&
        param.nchannels < 0 &&
        ImageBufAlgo::isConstantChannel(src,src.spec().alpha_channel,1.0f)) {
        ImageBuf newsrc(src.name() + ".noalpha", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, src.nchannels()-1);
        src.copy (newsrc);
        if (param.verbose) {
            std::cout << "  Alpha==1 image detected. Dropping the alpha channel.\n";
        }
    }
    
    // If requested - and we're a monochrome image - drop the extra channels
    if (param.monochrome_detect && (src.nchannels() > 1) &&
        param.nchannels < 0 && ImageBufAlgo::isMonochrome(src)) {
        ImageBuf newsrc(src.name() + ".monochrome", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, 1);
        src.copy (newsrc);
        if (param.verbose) {
            std::cout << "  Monochrome image detected. Converting to single channel texture.\n";
        }
    }
    
    // If we've otherwise explicitly requested to write out a
    // specific number of channels, do it.
    if (param.nchannels > 0 && param.nchannels != src.nchannels()) {
        ImageBuf newsrc(src.name() + ".channels", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, param.nchannels);
        src.copy (newsrc);
        if (param.verbose) {
            std::cout << "  Overriding number of channels to "
                      << param.nchannels << "\n";
        }
    }
    
    if (param.conversionmode == MaketxParams::SHADOW) {
        // Some special checks for shadow maps
        if (src.spec().nchannels != 1) {
            std::cerr << "maketx ERROR: shadow maps require 1-channel images,\n"
                      << "\t\"" << src.name() << "\" is "
                      << src.spec().nchannels << " channels\n";
            return false;
        }
        // Shadow maps only make sense for floating-point data.
        if (out_dataformat != TypeDesc::FLOAT &&
            out_dataformat != TypeDesc::HALF &&
            out_dataformat != TypeDesc::DOUBLE)
            out_dataformat = TypeDesc::FLOAT;
    }
    
    // Copy the input spec
    const ImageSpec &srcspec = src.spec();
    ImageSpec dstspec = srcspec;
    bool orig_was_volume = srcspec.depth > 1 || srcspec.full_depth > 1;
    bool orig_was_crop = (srcspec.x > srcspec.full_x ||
                          srcspec.y > srcspec.full_y ||
                          srcspec.z > srcspec.full_z ||
                          srcspec.x+srcspec.width < srcspec.full_x+srcspec.full_width ||
                          srcspec.y+srcspec.height < srcspec.full_y+srcspec.full_height ||
                          srcspec.z+srcspec.depth < srcspec.full_z+srcspec.full_depth);
    bool orig_was_overscan = (srcspec.x < srcspec.full_x &&
                              srcspec.y < srcspec.full_y &&
                              srcspec.x+srcspec.width > srcspec.full_x+srcspec.full_width &&
                              srcspec.y+srcspec.height > srcspec.full_y+srcspec.full_height &&
                              (!orig_was_volume || (srcspec.z < srcspec.full_z &&
                                                    srcspec.z+srcspec.depth > srcspec.full_z+srcspec.full_depth)));
    // Make the output not a crop window
    if (orig_was_crop) {
        dstspec.x = 0;
        dstspec.y = 0;
        dstspec.z = 0;
        dstspec.width = srcspec.full_width;
        dstspec.height = srcspec.full_height;
        dstspec.depth = srcspec.full_depth;
        dstspec.full_x = 0;
        dstspec.full_y = 0;
        dstspec.full_z = 0;
        dstspec.full_width = dstspec.width;
        dstspec.full_height = dstspec.height;
        dstspec.full_depth = dstspec.depth;
    }
    if (orig_was_overscan) {
        swrap = "black";
        twrap = "black";
    }
    
    if ((dstspec.x < 0 || dstspec.y < 0 || dstspec.z < 0) &&
        (out && !out->supports("negativeorigin"))) {
        // User passed negative origin but the output format doesn't
        // support it.  Try to salvage the situation by shifting the
        // image into the positive range.
        if (dstspec.x < 0) {
            dstspec.full_x -= dstspec.x;
            dstspec.x = 0;
        }
        if (dstspec.y < 0) {
            dstspec.full_y -= dstspec.y;
            dstspec.y = 0;
        }
        if (dstspec.z < 0) {
            dstspec.full_z -= dstspec.z;
            dstspec.z = 0;
        }
    }
    
    // Make the output tiled, regardless of input
    dstspec.tile_width  = tile_width;
    dstspec.tile_height = tile_height;
    dstspec.tile_depth  = param.tile_depth;
    
    if (param.forcecompress) {
        dstspec.attribute ("compression", "zip");
        // Ugh, the line above seems to trigger a bug in the tiff library.
        // Maybe a bug in libtiff zip compression for tiles?  So let's
        // stick to the default compression.
    }
    
    // Put a DateTime in the out file, either now, or matching the date
    // stamp of the input file (if update mode).
    time_t date;
    time (&date);    // current time. In update mode this should be src time!
    dstspec.attribute ("DateTime", datestring(date));
    
    if (param.conversionmode == MaketxParams::SHADOW) {
        dstspec.attribute ("textureformat", "Shadow");
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Shadow");
    } else if (param.conversionmode == MaketxParams::ENVLATLONG) {
        dstspec.attribute ("textureformat", "LatLong Environment");
        swrap = "periodic";
        twrap = "clamp";
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Latlong Environment");
    } else {
        dstspec.attribute ("textureformat", "Plain Texture");
        if(prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Plain Texture");
    }
    
    if (param.Mcam != Imath::M44f(0.0f))
        dstspec.attribute ("worldtocamera", TypeDesc::TypeMatrix, &param.Mcam);
    if (param.Mscr != Imath::M44f(0.0f))
        dstspec.attribute ("worldtoscreen", TypeDesc::TypeMatrix, &param.Mscr);
    
    // FIXME - check for valid strings in the wrap mode
    if (param.conversionmode != MaketxParams::SHADOW) {
        std::string wrapmodes = (swrap.size() ? swrap : param.wrap) + ',' +
                                (twrap.size() ? twrap : param.wrap);
        dstspec.attribute ("wrapmodes", wrapmodes);
    }
    
    if(fovcot == 0.0f) {
        fovcot = static_cast<float>(srcspec.full_width) /
        static_cast<float>(srcspec.full_height);
    }
    dstspec.attribute ("fovcot", fovcot);
    
    if (separate)
        dstspec.attribute ("planarconfig", "separate");
    else {
        dstspec.erase_attribute("planarconfig");
        dstspec.erase_attribute("tiff:planarconfig");
    }
    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?
    
    // If --checknan was used and it's a floating point image, check for
    // nonfinite (NaN or Inf) values and abort if they are found.
    if (param.checknan && (srcspec.format.basetype == TypeDesc::FLOAT ||
                           srcspec.format.basetype == TypeDesc::HALF ||
                           srcspec.format.basetype == TypeDesc::DOUBLE)) {
        found_nonfinite = false;
        parallel_image (check_nan_block, &src, &src,
                        dstspec.x, dstspec.x+dstspec.width,
                        dstspec.y, dstspec.y+dstspec.height,
                        param.nthreads, &param);
        if (found_nonfinite) {
            if (found_nonfinite > 3)
                std::cerr << "maketx ERROR: ...and Nan/Inf at "
                << (found_nonfinite-3) << " other pixels\n";
            return false;
        }
    }
    
    // Fix nans/infs (if requested
    ImageBufAlgo::NonFiniteFixMode fixmode = ImageBufAlgo::NONFINITE_NONE;
    if (param.fixnan.empty() || param.fixnan == "none") { }
    else if (param.fixnan == "black") { fixmode = ImageBufAlgo::NONFINITE_BLACK; }
    else if (param.fixnan == "box3") { fixmode = ImageBufAlgo::NONFINITE_BOX3; }
    else {
        std::cerr << "maketx ERROR: Unknown --fixnan mode " << " fixnan\n";
        return false;
    }
    
    int pixelsFixed = 0;
    if (!ImageBufAlgo::fixNonFinite (src, src, fixmode, &pixelsFixed)) {
        std::cerr << "maketx ERROR: Error fixing nans/infs.\n";
        return false;
    }
    
    if (param.verbose && pixelsFixed > 0) {
        std::cout << "  Warning: " << pixelsFixed << " nan/inf pixels fixed.\n";
    }
    
    
    
    // Color convert the pixels, if needed, in place.  If a color
    // conversion is required we will promote the src to floating point
    // (or there wont be enough precision potentially).  Also,
    // independently color convert the constant color metadata
    ImageBuf * ccSrc = &src;    // Ptr to cc'd src image
    ImageBuf colorBuffer;
    ColorConfig colorconfig;
    if (!param.incolorspace.empty() && !param.outcolorspace.empty() &&
        param.incolorspace != param.outcolorspace) {
        if (src.spec().format != TypeDesc::FLOAT) {
            ImageSpec floatSpec = src.spec();
            floatSpec.set_format(TypeDesc::FLOAT);
            colorBuffer.reset("bitdepth promoted", floatSpec);
            ccSrc = &colorBuffer;
        }
        
        Timer colorconverttimer;
        if (param.verbose) {
            std::cout << "  Converting from colorspace " << param.incolorspace
            << " to colorspace " << param.outcolorspace << std::endl;
        }
        
        if (colorconfig.error()) {
            std::cerr << "Error Creating ColorConfig\n";
            std::cerr << colorconfig.geterror() << std::endl;
            return false;
        }
        
        ColorProcessor * processor = colorconfig.createColorProcessor (
               param.incolorspace.c_str(), param.outcolorspace.c_str());
        
        if (!processor || colorconfig.error()) {
            std::cerr << "Error Creating Color Processor." << std::endl;
            std::cerr << colorconfig.geterror() << std::endl;
            return false;
        }
        
        if (param.unpremult && param.verbose)
            std::cout << "  Unpremulting image..." << std::endl;
        
        if (!ImageBufAlgo::colorconvert (*ccSrc, src, processor,
                                         param.unpremult)) {
            std::cerr << "Error applying color conversion to image.\n";
            return false;
        }
        
        if (isConstantColor) {
            const int colorSize = static_cast<int>(constantColor.size());
            if (!ImageBufAlgo::colorconvert (&constantColor[0], colorSize,
                                             processor, param.unpremult)) {
                std::cerr << "Error applying color conversion to constant color.\n";
                return false;
            }
        }
        
        ColorConfig::deleteColorProcessor(processor);
        processor = NULL;
        if (stat)
            stat->colorconverttime += colorconverttimer();
    }
    
    // Force float for the sake of the ImageBuf math if requested
    if (param.forcefloat)
        dstspec.set_format (TypeDesc::FLOAT);
  
    // Handle resize to power of two, if called for
    if (param.pow2resize  &&  param.conversionmode != MaketxParams::SHADOW) {
        dstspec.width = pow2roundup (dstspec.width);
        dstspec.height = pow2roundup (dstspec.height);
        dstspec.full_width = dstspec.width;
        dstspec.full_height = dstspec.height;
    }
    
    bool do_resize = false;
    // Resize if we're up-resing for pow2
    if (dstspec.width != srcspec.width || dstspec.height != srcspec.height ||
        dstspec.full_depth != srcspec.full_depth)
        do_resize = true;
    // resize if the original was a crop
    if (orig_was_crop)
        do_resize = true;
    // resize if we're converting from non-border sampling to border sampling
    // Note: src_samples_border is always false at this point.
    if (param.conversionmode == MaketxParams::ENVLATLONG &&
        (Strutil::iequals(out->format_name(),"openexr") ||
         Strutil::iends_with(param.outputfilename,".exr")))
        do_resize = true;

    if (do_resize && orig_was_overscan && !out->supports("displaywindow")) {
        std::cerr << "maketx ERROR: format " << out->format_name()
        << " does not support separate display windows,\n"
        << "              which is necessary when combining resizing"
        << " and an input image with overscan.";
        return false;
    }
    
    Timer resizetimer;
    ImageBuf dst ("temp");
    ImageBuf *toplevel = &dst;    // Ptr to top level of mipmap
    if (! do_resize) {
        // Don't need to resize
        if (dstspec.format == ccSrc->spec().format) {
            // Even more special case, no format change -- just use
            // the original copy.
            toplevel = ccSrc;
        } else {
            dst.alloc(dstspec);
            parallel_image (copy_block, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height,
                            param.nthreads, &param);
        }
    } else {
        // Resize
        dst.alloc(dstspec);
        if (param.verbose)
            std::cout << "  Resizing image to " << dstspec.width
            << " x " << dstspec.height << std::endl;
        if (filter->name() == "box" && filter->width() == 1.0f)
            parallel_image (resize_block, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height,
                            param.nthreads, &param);
        else
            parallel_image (resize_block_HQ, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height,
                            param.nthreads, &param);
    }
    
    if (stat)
        stat->resizetime += resizetimer();
    
    
    // Update the toplevel ImageDescription with the sha1 pixel hash and
    // constant color
    std::string desc = dstspec.get_string_attribute ("ImageDescription");
    bool updatedDesc = false;
    
    // FIXME: We need to do real dictionary style partial updates on the
    //        ImageDescription. I.e., set one key without affecting the
    //        other keys. But in the meantime, just clear it out if
    //        it appears the incoming image was a maketx style texture.
    
    if ((desc.find ("SHA-1=") != std::string::npos) ||
        (desc.find ("ConstantColor=") != std::string::npos)) {
        desc = "";
    }
    
    if (param.computesha1) {
        // The hash is only computed for the top mipmap level of pixel data.
        // Thus, any additional information that will effect the lower levels
        // (such as filtering information) needs to be manually added into the
        // hash.
        std::ostringstream addlHashData;
        addlHashData << filter->name() << " ";
        addlHashData << filter->width() << " ";
        
        printf("Computing SHA1\n");
        
        std::string hash_digest = ImageBufAlgo::computePixelHashSHA1 (*toplevel,
                                                           addlHashData.str());
        if (hash_digest.length()) {
            if (desc.length())
                desc += " ";
            desc += "SHA-1=";
            desc += hash_digest;
            if (param.verbose)
                std::cout << "  SHA-1: " << hash_digest << std::endl;
            updatedDesc = true;
            dstspec.attribute ("oiio:SHA-1", hash_digest);
        }
        printf("Computing SHA1 -- FINISHED\n");
    }
  
  
    if (isConstantColor) {
        std::ostringstream os; // Emulate a JSON array
        os << "[";
        for (unsigned int i=0; i<constantColor.size(); ++i) {
            if (i!=0) os << ",";
            os << constantColor[i];
        }
        os << "]";
        
        if (desc.length())
            desc += " ";
        desc += "ConstantColor=";
        desc += os.str();
        if (param.verbose)
            std::cout << "  ConstantColor: " << os.str() << std::endl;
        updatedDesc = true;
        dstspec.attribute ("oiio:ConstantColor", os.str());
    }
    
    if (updatedDesc) {
        dstspec.attribute ("ImageDescription", desc);
    }
    
    // Write out, and compute, the mipmap levels for the speicifed image
    bool domip = param.conversionmode != MaketxParams::SHADOW &&
                 !param.nomipmap;
    if (!write_mipmap (*toplevel, dstspec, out, out_dataformat, domip,
                       filter, param, stat))
        return false;
    
    if (local_filter)
      delete filter;
    
    return true;
}



static bool
write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
              ImageOutput *out, TypeDesc outputdatatype, bool mipmap,
              const Filter2D *filter, const MaketxParams &param,
              MaketxStats *stat)
{    
    ImageSpec outspec = outspec_template;
    outspec.set_format (outputdatatype);
    
    if (mipmap && !out->supports ("multiimage") && !out->supports ("mipmap")) {
        std::cerr << "maketx ERROR: \"" << param.outputfilename
        << "\" format does not support multires images\n";
        return false;
    }
    
    if (! mipmap && ! strcmp (out->format_name(), "openexr")) {
        // Send hint to OpenEXR driver that we won't specify a MIPmap
        outspec.attribute ("openexr:levelmode", 0 /* ONE_LEVEL */);
    }
    
    if (mipmap && ! strcmp (out->format_name(), "openexr")) {
        outspec.attribute ("openexr:roundingmode", 0 /* ROUND_DOWN */);
    }
    
    // OpenEXR always uses border sampling for environment maps
    bool src_samples_border = false;
    if (param.conversionmode == MaketxParams::ENVLATLONG &&
        !strcmp(out->format_name(), "openexr")) {
        src_samples_border = true;
        outspec.attribute ("oiio:updirection", "y");
        outspec.attribute ("oiio:sampleborder", 1);
    }
    if (param.conversionmode == MaketxParams::ENVLATLONG && src_samples_border)
        fix_latl_edges (img);
    
    Timer writetimer;
    if (! out->open (param.outputfilename.c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << param. outputfilename
        << "\" : " << out->geterror() << "\n";
        return false;
    }
    
    // Write out the image
    if (param.verbose) {
        std::cout << "  Writing file: " << param.outputfilename << std::endl;
        std::cout << "  Filter \"" << filter->name() << "\" width = "
        << filter->width() << "\n";
        std::cout << "  Top level is " << formatres(outspec) << std::endl;
    }
    
    if (! img.write (out)) {
        // ImageBuf::write transfers any errors from the ImageOutput to
        // the ImageBuf.
        std::cerr << "maketx ERROR: Write failed \" : " << img.geterror() << "\n";
        out->close ();
        exit (EXIT_FAILURE);
    }
    if (stat)
        stat->writetime += writetimer();

    if (mipmap) {  // Mipmap levels:
        if (param.verbose)
            std::cout << "  Mipmapping...\n" << std::flush;
        ImageBuf tmp;
        ImageBuf *big = &img, *small = &tmp;
        std::vector<std::string> mipimages = param.mipimages; // mutable copy
        while (outspec.width > 1 || outspec.height > 1) {
            Timer miptimer;
            ImageSpec smallspec;
            
            if (param.mipimages.size()) {
                // Special case -- the user specified a custom MIP level
                small->reset (mipimages[0]);
                small->read (0, 0, true, TypeDesc::FLOAT);
                smallspec = small->spec();
                if (smallspec.nchannels != outspec.nchannels) {
                    std::cout << "WARNING: Custom mip level \""
                              << param.mipimages[0]
                              << " had the wrong number of channels.\n";
                    ImageBuf *t = new ImageBuf (mipimages[0], smallspec);
                    ImageBufAlgo::setNumChannels(*t, *small, outspec.nchannels);
                    std::swap (t, small);
                    delete t;
                }
                smallspec.tile_width = outspec.tile_width;
                smallspec.tile_height = outspec.tile_height;
                smallspec.tile_depth = outspec.tile_depth;
                mipimages.erase (mipimages.begin());
            } else {
                // Resize a factor of two smaller
                smallspec = outspec;
                smallspec.width = big->spec().width;
                smallspec.height = big->spec().height;
                smallspec.depth = big->spec().depth;
                if (smallspec.width > 1)
                    smallspec.width /= 2;
                if (smallspec.height > 1)
                    smallspec.height /= 2;
                smallspec.full_width = smallspec.width;
                smallspec.full_height = smallspec.height;
                smallspec.full_depth = smallspec.depth;
                if (param.forcefloat)
                    smallspec.set_format (TypeDesc::FLOAT);
                
                // Trick: to get the resize working properly, we reset
                // both display and pixel windows to match, and have 0
                // offset, AND doctor the big image to have its display
                // and pixel windows match.  Don't worry, the texture
                // engine doesn't care what the upper MIP levels have
                // for the window sizes, it uses level 0 to determine
                // the relatinship between texture 0-1 space (display
                // window) and the pixels.
                smallspec.x = 0;
                smallspec.y = 0;
                smallspec.full_x = 0;
                smallspec.full_y = 0;
                small->alloc (smallspec);  // Realocate with new size
                big->set_full (big->xbegin(), big->xend(), big->ybegin(),
                               big->yend(), big->zbegin(), big->zend());
                
                if (filter->name() == "box" &&
                    filter->width() == 1.0f)
                    parallel_image (resize_block, small, big,
                                    small->xbegin(), small->xend(),
                                    small->ybegin(), small->yend(),
                                    param.nthreads, &param);
                else
                    parallel_image (resize_block_HQ, small, big,
                                    small->xbegin(), small->xend(),
                                    small->ybegin(), small->yend(),
                                    param.nthreads, &param);
            }
            if (stat)
                stat->miptime += miptimer();
            outspec = smallspec;
            outspec.set_format (outputdatatype);
            if (param.conversionmode == MaketxParams::ENVLATLONG &&
                src_samples_border)
                fix_latl_edges (*small);
            
            Timer writetimer;
            // If the format explicitly supports MIP-maps, use that,
            // otherwise try to simulate MIP-mapping with multi-image.
            ImageOutput::OpenMode mode = out->supports ("mipmap") ?
            ImageOutput::AppendMIPLevel : ImageOutput::AppendSubimage;
            if (! out->open (param.outputfilename.c_str(), outspec, mode)) {
                std::cerr << "maketx ERROR: Could not append \""
                          << param.outputfilename
                          << "\" : " << out->geterror() << "\n";
                return false;
            }
            if (! small->write (out)) {
                // ImageBuf::write transfers any errors from the
                // ImageOutput to the ImageBuf.
                std::cerr << "maketx ERROR writing \"" << param.outputfilename
                          << "\" : " << small->geterror() << "\n";
                out->close ();
                exit (EXIT_FAILURE);
            }
            if (stat)
                stat->writetime += writetimer();
            if (param.verbose) {
                std::cout << "    " << formatres(smallspec) << std::endl;
            }
            std::swap (big, small);
        }
    }
    
    if (param.verbose)
        std::cout << "  Wrote file: " << param.outputfilename << std::endl;
    writetimer.reset ();
    writetimer.start ();
    if (! out->close ()) {
        std::cerr << "maketx ERROR writing \"" << param.outputfilename
        << "\" : " << out->geterror() << "\n";
        return false;
    }
    if (stat)
        stat->writetime += writetimer ();
    
    return true;
}


}
OIIO_NAMESPACE_EXIT
