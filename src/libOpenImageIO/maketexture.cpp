/*
  Copyright 2013 Larry Gritz and the other authors and contributors.
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

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

#include <OpenEXR/ImathMatrix.h>
#include <OpenEXR/half.h>

#include "argparse.h"
#include "dassert.h"
#include "filesystem.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"
#include "timer.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "thread.h"
#include "filter.h"

OIIO_NAMESPACE_USING


static spin_mutex maketx_mutex;   // for anything that needs locking



static Filter2D *
setup_filter (const std::string &filtername)
{
    // Figure out the recommended filter width for the named filter
    float filterwidth = 1.0f;
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc d;
        Filter2D::get_filterdesc (i, &d);
        if (filtername == d.name) {
            filterwidth = d.width;
            break;
        }
    }

    Filter2D *filter = Filter2D::create (filtername, filterwidth, filterwidth);

    return filter;
}



static TypeDesc
set_prman_options(TypeDesc out_dataformat, ImageSpec &configspec)
{
    // Force separate planar image handling, and also emit prman metadata
    configspec.attribute ("planarconfig", "separate");
    configspec.attribute ("maketx:prman_metadata", 1);

    // 8-bit : 64x64
    if (out_dataformat == TypeDesc::UINT8 || out_dataformat == TypeDesc::INT8) {
        configspec.tile_width = 64;
        configspec.tile_height = 64;
    }

    // 16-bit : 64x32
    // Force u16 -> s16
    // In prman's txmake (last tested in 15.0)
    // specifying -short creates a signed int representation
    if (out_dataformat == TypeDesc::UINT16)
        out_dataformat = TypeDesc::INT16;

    if (out_dataformat == TypeDesc::INT16) {
        configspec.tile_width = 64;
        configspec.tile_height = 32;
    }

    // Float: 32x32
    // In prman's txmake (last tested in 15.0)
    // specifying -half or -float make 32x32 tile size
    if (out_dataformat == TypeDesc::DOUBLE)
        out_dataformat = TypeDesc::FLOAT;
    if (out_dataformat == TypeDesc::HALF || out_dataformat == TypeDesc::FLOAT) {
        configspec.tile_width = 32;
        configspec.tile_height = 32;
    }

    return out_dataformat;
}



static TypeDesc
set_oiio_options(TypeDesc out_dataformat, ImageSpec &configspec)
{
    // Interleaved channels are faster to read
    configspec.attribute ("planarconfig", "contig");
    
    // Force fixed tile-size across the board
    configspec.tile_width = 64;
    configspec.tile_height = 64;
    
    return out_dataformat;
}



static std::string
datestring (time_t t)
{
    struct tm mytm;
    Sysutil::get_local_time (&t, &mytm);
    return Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                            mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                            mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
template<typename SRCTYPE>
static bool
copy_block_ (ImageBuf &dst, const ImageBuf &src, ROI roi=ROI())
{
    int nchannels = dst.spec().nchannels;
    ASSERT (src.spec().nchannels == nchannels);
    ROI image_roi = get_roi (dst.spec());
    if (roi.defined())
        image_roi = roi_intersection (image_roi, roi);
    ImageBuf::Iterator<float> d (dst, image_roi);
    ImageBuf::ConstIterator<SRCTYPE> s (src, image_roi, ImageBuf::WrapBlack);
    for (  ; ! d.done();  ++d, ++s) {
        for (int c = 0;  c < nchannels;  ++c)
            d[c] = s[c];
    }
    return true;
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static bool
copy_block (ImageBuf &dst, const ImageBuf &src, ROI roi)
{
    ASSERT (dst.spec().format == TypeDesc::TypeFloat);
    OIIO_DISPATCH_TYPES ("copy_block", copy_block_, src.spec().format,
                         dst, src, roi);
}



// Resize src into dst using a good quality filter,
// for the pixel range [x0,x1) x [y0,y1).
static void
resize_block_HQ (ImageBuf &dst, const ImageBuf &src, ROI roi, Filter2D *filter)
{
    int x0 = roi.xbegin, x1 = roi.xend, y0 = roi.ybegin, y1 = roi.yend;
    ImageBufAlgo::resize (dst, src, x0, x1, y0, y1, filter);
}



template<class SRCTYPE>
static void
interppixel_NDC_clamped (const ImageBuf &buf, float x, float y, float *pixel,
                         bool envlatlmode)
{
    int fx = buf.spec().full_x;
    int fy = buf.spec().full_y;
    int fw = buf.spec().full_width;
    int fh = buf.spec().full_height;
    x = static_cast<float>(fx) + x * static_cast<float>(fw);
    y = static_cast<float>(fy) + y * static_cast<float>(fh);

    int n = buf.spec().nchannels;
    float *p0 = ALLOCA(float, 4*n), *p1 = p0+n, *p2 = p1+n, *p3 = p2+n;

    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);

    // Get the four texels
    ImageBuf::ConstIterator<SRCTYPE> it (buf, ROI(xtexel, xtexel+2, ytexel, ytexel+2), ImageBuf::WrapClamp);
    for (int c = 0; c < n; ++c) p0[c] = it[c];
    ++it;
    for (int c = 0; c < n; ++c) p1[c] = it[c];
    ++it;
    for (int c = 0; c < n; ++c) p2[c] = it[c];
    ++it;
    for (int c = 0; c < n; ++c) p3[c] = it[c];

    if (envlatlmode) {
        // For latlong environment maps, in order to conserve energy, we
        // must weight the pixels by sin(t*PI) because pixels closer to
        // the pole are actually less area on the sphere. Doing this
        // wrong will tend to over-represent the high latitudes in
        // low-res MIP levels.  We fold the area weighting into our
        // linear interpolation by adjusting yfrac.
        int ynext = Imath::clamp (ytexel+1, buf.ymin(), buf.ymax());
        ytexel = Imath::clamp (ytexel, buf.ymin(), buf.ymax());
        float w0 = (1.0f - yfrac) * sinf ((float)M_PI * (ytexel+0.5f)/(float)fh);
        float w1 = yfrac * sinf ((float)M_PI * (ynext+0.5f)/(float)fh);
        yfrac = w0 / (w0 + w1);
    }

    // Bilinearly interpolate
    bilerp (p0, p1, p2, p3, xfrac, yfrac, n, pixel);
}



// Resize src into dst, relying on the linear interpolation of
// interppixel_NDC_full or interppixel_NDC_clamped, for the pixel range.
template<class SRCTYPE>
static bool
resize_block_ (ImageBuf &dst, const ImageBuf &src, ROI roi, bool envlatlmode)
{
    int x0 = roi.xbegin, x1 = roi.xend, y0 = roi.ybegin, y1 = roi.yend;
    const ImageSpec &srcspec (src.spec());
    bool src_is_crop = (srcspec.x > srcspec.full_x ||
                        srcspec.y > srcspec.full_y ||
                        srcspec.z > srcspec.full_z ||
                        srcspec.x+srcspec.width < srcspec.full_x+srcspec.full_width ||
                        srcspec.y+srcspec.height < srcspec.full_y+srcspec.full_height ||
                        srcspec.z+srcspec.depth < srcspec.full_z+srcspec.full_depth);

    const ImageSpec &dstspec (dst.spec());
    float *pel = (float *) alloca (dstspec.pixel_bytes());
    float xoffset = dstspec.full_x;
    float yoffset = dstspec.full_y;
    float xscale = 1.0f / (float)dstspec.full_width;
    float yscale = 1.0f / (float)dstspec.full_height;
    int nchannels = dst.nchannels();
    ASSERT (dst.spec().format == TypeDesc::TypeFloat);
    ImageBuf::Iterator<float> d (dst, roi);
    for (int y = y0;  y < y1;  ++y) {
        float t = (y+0.5f)*yscale + yoffset;
        for (int x = x0;  x < x1;  ++x, ++d) {
            float s = (x+0.5f)*xscale + xoffset;
            if (src_is_crop)
                src.interppixel_NDC_full (s, t, pel);
            else
                interppixel_NDC_clamped<SRCTYPE> (src, s, t, pel, envlatlmode);
            for (int c = 0; c < nchannels; ++c)
                d[c] = pel[c];
        }
    }
    return true;
}


// Helper function to compute the first bilerp pass into a scanline buffer
template<class SRCTYPE>
static void
halve_scanline(const SRCTYPE *s, const int nchannels, size_t sw, float *dst)
{
    for (size_t i = 0; i < sw; i += 2, s += nchannels) {
        for (size_t j = 0; j < nchannels; ++j, ++dst, ++s)
            *dst = 0.5 * (*s + *(s + nchannels));
    }
}



// Bilinear resize performed as a 2-pass filter.
// Optimized to assume that the images are contiguous.
template<class SRCTYPE>
static bool
resize_block_2pass (ImageBuf &dst, const ImageBuf &src, ROI roi, bool allow_shift)
{
    // Two-pass filtering introduces a half-pixel shift for odd resolutions.
    // Revert to correct bilerp sampling unless shift is explicitly allowed.
    if (!allow_shift && (src.spec().width % 2 || src.spec().height % 2))
        return resize_block_<SRCTYPE>(dst, src, roi, false);
    
    DASSERT(dst.spec().width == roi.width());           // Full width ROI
    DASSERT(roi.xbegin == 0 && roi.xend == roi.width());// Not inset
    DASSERT(dst.spec().width == src.spec().width / 2);  // Src is 2x
    DASSERT(src.spec().format == dst.spec().format);    // Same formats
    DASSERT(src.nchannels() == dst.nchannels());        // Same channels
    DASSERT(roi.ybegin + roi.height() <= dst.spec().height);

    // Allocate two scanline buffers to hold the result of the first pass
    const int nchannels = dst.nchannels();
    const size_t row_elem = roi.width() * nchannels;    // # floats in scanline
    boost::scoped_array<float> S0 (new float [row_elem]);
    boost::scoped_array<float> S1 (new float [row_elem]);
    
    // We know that the buffers created for mipmapping are all contiguous,
    // so we can skip the iterators for a bilerp resize entirely along with
    // any NDC -> pixel math, and just directly traverse pixels.
    const SRCTYPE *s = (const SRCTYPE *)src.localpixels();
    SRCTYPE *d = (SRCTYPE *)dst.localpixels();
    ASSERT(s && d);                                     // Assume contig bufs
    d += roi.ybegin * dst.spec().width * nchannels;     // Top of dst ROI
    const size_t ystride = src.spec().width * nchannels;// Scanline offset
    s += 2 * roi.ybegin * ystride;                      // Top of src ROI
    
    // Run through destination rows, doing the two-pass bilerp filter
    const size_t dw = roi.width(), dh = roi.height();   // Loop invariants
    const size_t sw = dw * 2;                           // Handle odd res
    for (size_t y = 0; y < dh; ++y) {                   // For each dst ROI row
        halve_scanline<SRCTYPE>(s, nchannels, sw, &S0[0]);
        s += ystride;
        halve_scanline<SRCTYPE>(s, nchannels, sw, &S1[0]);
        s += ystride;
        const float *s0 = &S0[0], *s1 = &S1[0];
        for (size_t x = 0; x < dw; ++x) {               // For each dst ROI col
            for (size_t i = 0; i < nchannels; ++i, ++s0, ++s1, ++d)
                *d = 0.5 * (*s0 + *s1);                 // Average vertically
        }
    }
    
    return true;
}



static bool
resize_block (ImageBuf &dst, const ImageBuf &src, ROI roi, bool envlatlmode,
              bool allow_shift)
{
  if (!envlatlmode) {
      OIIO_DISPATCH_TYPES("resize_block_2pass", resize_block_2pass,
                          src.spec().format, dst, src, roi, allow_shift);
  }
  
  ASSERT (dst.spec().format == TypeDesc::TypeFloat);
  OIIO_DISPATCH_TYPES ("resize_block", resize_block_, src.spec().format,
                       dst, src, roi, envlatlmode);
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static void
check_nan_block (const ImageBuf &src, ROI roi, int &found_nonfinite)
{
    int x0 = roi.xbegin, x1 = roi.xend, y0 = roi.ybegin, y1 = roi.yend;
    const ImageSpec &spec (src.spec());
    float *pel = (float *) alloca (spec.pixel_bytes());
    for (int y = y0;  y < y1;  ++y) {
        for (int x = x0;  x < x1;  ++x) {
            src.getpixel (x, y, pel);
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
    int n = buf.nchannels();
    float *left = ALLOCA (float, n);
    float *right = ALLOCA (float, n);

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



static void
maketx_merge_spec (ImageSpec &dstspec, const ImageSpec &srcspec)
{
    for (size_t i = 0, e = srcspec.extra_attribs.size(); i < e; ++i) {
        const ImageIOParameter &p (srcspec.extra_attribs[i]);
        ustring name = p.name();
        if (Strutil::istarts_with (name.string(), "maketx:")) {
            // Special instruction -- don't copy it to the destination spec
        } else {
            // just an attribute that should be set upon output
            dstspec.attribute (name.string(), p.type(), p.data());
        }
    }
}



static bool
write_mipmap (ImageBufAlgo::MakeTextureMode mode,
              boost::shared_ptr<ImageBuf> &img,
              const ImageSpec &outspec_template,
              std::string outputfilename, ImageOutput *out,
              TypeDesc outputdatatype, bool mipmap,
              Filter2D *filter, const ImageSpec &configspec,
              std::ostream &outstream,
              double &stat_writetime, double &stat_miptime,
              size_t &peak_mem)
{
    bool envlatlmode = (mode == ImageBufAlgo::MakeTxEnvLatl);

    ImageSpec outspec = outspec_template;
    outspec.set_format (outputdatatype);

    if (mipmap && !out->supports ("multiimage") && !out->supports ("mipmap")) {
        outstream << "maketx ERROR: \"" << outputfilename
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
    bool src_samples_border;
    if (envlatlmode && !strcmp(out->format_name(), "openexr")) {
        src_samples_border = true;
        outspec.attribute ("oiio:updirection", "y");
        outspec.attribute ("oiio:sampleborder", 1);
    }
    if (envlatlmode && src_samples_border)
        fix_latl_edges (*img);

    Timer writetimer;
    if (! out->open (outputfilename.c_str(), outspec)) {
        outstream << "maketx ERROR: Could not open \"" << outputfilename
                  << "\" : " << out->geterror() << "\n";
        return false;
    }

    // Write out the image
    bool verbose = configspec.get_int_attribute ("maketx:verbose");
    if (verbose) {
        outstream << "  Writing file: " << outputfilename << std::endl;
        outstream << "  Filter \"" << filter->name() << "\" width = " 
                  << filter->width() << "\n";
        outstream << "  Top level is " << formatres(outspec) << std::endl;
    }

    if (! img->write (out)) {
        // ImageBuf::write transfers any errors from the ImageOutput to
        // the ImageBuf.
        outstream << "maketx ERROR: Write failed \" : " << img->geterror() << "\n";
        out->close ();
        return false;
    }

    stat_writetime += writetimer();

    if (mipmap) {  // Mipmap levels:
        if (verbose)
            outstream << "  Mipmapping...\n" << std::flush;
        std::vector<std::string> mipimages;
        std::string mipimages_unsplit = configspec.get_string_attribute ("maketx:mipimages");
        if (mipimages_unsplit.length())
            Strutil::split (mipimages_unsplit, mipimages, ";");
        
        boost::shared_ptr<ImageBuf> small (new ImageBuf);
        while (outspec.width > 1 || outspec.height > 1) {
            Timer miptimer;
            ImageSpec smallspec;

            if (mipimages.size()) {
                // Special case -- the user specified a custom MIP level
                small->reset (mipimages[0]);
                small->read (0, 0, true, TypeDesc::FLOAT);
                smallspec = small->spec();
                if (smallspec.nchannels != outspec.nchannels) {
                    outstream << "WARNING: Custom mip level \"" << mipimages[0]
                              << " had the wrong number of channels.\n";
                    boost::shared_ptr<ImageBuf> t (new ImageBuf (mipimages[0], smallspec));
                    ImageBufAlgo::setNumChannels(*t, *small, outspec.nchannels);
                    std::swap (t, small);
                }
                smallspec.tile_width = outspec.tile_width;
                smallspec.tile_height = outspec.tile_height;
                smallspec.tile_depth = outspec.tile_depth;
                mipimages.erase (mipimages.begin());
            } else {
                // Resize a factor of two smaller
                smallspec = outspec;
                smallspec.width = img->spec().width;
                smallspec.height = img->spec().height;
                smallspec.depth = img->spec().depth;
                if (smallspec.width > 1)
                    smallspec.width /= 2;
                if (smallspec.height > 1)
                    smallspec.height /= 2;
                smallspec.full_width = smallspec.width;
                smallspec.full_height = smallspec.height;
                smallspec.full_depth = smallspec.depth;
                bool allow_shift = configspec.get_int_attribute("maketx:allow_pixel_shift");
                if (!allow_shift ||
                    configspec.get_int_attribute("maketx:forcefloat", 1))
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
                img->set_full (img->xbegin(), img->xend(), img->ybegin(),
                               img->yend(), img->zbegin(), img->zend());

                if (filter->name() == "box" && filter->width() == 1.0f)
                    ImageBufAlgo::parallel_image (boost::bind(resize_block, boost::ref(*small), boost::cref(*img), _1, envlatlmode, allow_shift),
                                                  OIIO::get_roi(small->spec()));
                else
                    ImageBufAlgo::parallel_image (boost::bind(resize_block_HQ, boost::ref(*small), boost::cref(*img), _1, filter),
                                                  OIIO::get_roi(small->spec()));
            }

            stat_miptime += miptimer();
            outspec = smallspec;
            outspec.set_format (outputdatatype);
            if (envlatlmode && src_samples_border)
                fix_latl_edges (*small);

            Timer writetimer;
            // If the format explicitly supports MIP-maps, use that,
            // otherwise try to simulate MIP-mapping with multi-image.
            ImageOutput::OpenMode mode = out->supports ("mipmap") ?
                ImageOutput::AppendMIPLevel : ImageOutput::AppendSubimage;
            if (! out->open (outputfilename.c_str(), outspec, mode)) {
                outstream << "maketx ERROR: Could not append \"" << outputfilename
                          << "\" : " << out->geterror() << "\n";
                return false;
            }
            if (! small->write (out)) {
                // ImageBuf::write transfers any errors from the
                // ImageOutput to the ImageBuf.
                outstream << "maketx ERROR writing \"" << outputfilename
                          << "\" : " << small->geterror() << "\n";
                out->close ();
                return false;
            }
            stat_writetime += writetimer();
            if (verbose) {
                size_t mem = Sysutil::memory_used(true);
                peak_mem = std::max (peak_mem, mem);
                outstream << Strutil::format ("    %-15s (%s)", 
                                              formatres(smallspec),
                                              Strutil::memformat(mem))
                          << std::endl;
            }
            std::swap (img, small);
        }
    }

    if (verbose)
        outstream << "  Wrote file: " << outputfilename << "  ("
                  << Strutil::memformat(Sysutil::memory_used(true)) << ")\n";
    writetimer.reset ();
    writetimer.start ();
    if (! out->close ()) {
        outstream << "maketx ERROR writing \"" << outputfilename
                  << "\" : " << out->geterror() << "\n";
        return false;
    }
    stat_writetime += writetimer ();
    return true;
}



static bool
make_texture_impl (ImageBufAlgo::MakeTextureMode mode,
                   const ImageBuf *input,
                   const std::string filename,
                   std::string outputfilename,
                   const ImageSpec &_configspec,
                   std::ostream *outstream_ptr)
{
    ASSERT (mode >= 0 && mode < ImageBufAlgo::_MakeTxLast);
    double stat_readtime = 0;
    double stat_writetime = 0;
    double stat_resizetime = 0;
    double stat_miptime = 0;
    double stat_colorconverttime = 0;
    size_t peak_mem = 0;
    Timer alltime;

#define STATUS(task,timer)                                              \
    {                                                                   \
        size_t mem = Sysutil::memory_used(true);                        \
        peak_mem = std::max (peak_mem, mem);                            \
        if (verbose)                                                    \
            outstream << Strutil::format ("  %-25s %s   (%s)\n", task,  \
                                  Strutil::timeintervalformat(timer,2), \
                                  Strutil::memformat(mem));             \
    }

    ImageSpec configspec = _configspec;
    std::stringstream localstream; // catch output when user doesn't want it
    std::ostream &outstream (outstream_ptr ? *outstream_ptr : localstream);

    bool from_filename = (input == NULL);

    if (from_filename && ! Filesystem::exists (filename)) {
        outstream << "maketx ERROR: \"" << filename << "\" does not exist\n";
        return false;
    }

    ImageBuf *img = NULL;
    if (input == NULL) {
        // No buffer supplied -- create one to read the file
        img = new ImageBuf(filename);
        img->init_spec (filename, 0, 0); // force it to get the spec, not read
    } else if (input->cachedpixels()) {
        // Image buffer supplied that's backed by ImageCache -- create a
        // copy (very light weight, just another cache reference)
        img = new ImageBuf(*input);
    } else {
        // Image buffer supplied that has pixels -- wrap it
        img = new ImageBuf(input->name(), input->spec(),
                           (void *)input->localpixels());
    }

    boost::shared_ptr<ImageBuf> src (img); // transfers ownership to src

    if (! outputfilename.length()) {
        std::string fn = src->name();
        if (fn.length()) {
            if (Filesystem::extension(fn).length() > 1)
                outputfilename = Filesystem::replace_extension (fn, ".tx");
            else
                outputfilename = outputfilename + ".tx";
        } else {
            outstream << "maketx: no output filename supplied\n";
            return false;
        }
    }

    // When was the input file last modified?
    // This is only used when we're reading from a filename
    std::time_t in_time;
    bool updatemode = configspec.get_int_attribute ("maketx:updatemode");
    if (from_filename) {
        // When in update mode, skip making the texture if the output
        // already exists and has the same file modification time as the
        // input file.
        in_time = Filesystem::last_write_time (src->name());
        if (updatemode && Filesystem::exists (outputfilename) &&
            (in_time == Filesystem::last_write_time (outputfilename))) {
            outstream << "maketx: no update required for \"" 
                      << outputfilename << "\"\n";
            return true;
        }
    }

    bool shadowmode = (mode == ImageBufAlgo::MakeTxShadow);
    bool envlatlmode = (mode == ImageBufAlgo::MakeTxEnvLatl);

    // Find an ImageIO plugin that can open the output file, and open it
    std::string outformat = configspec.get_string_attribute ("maketx:fileformatname",
                                                             outputfilename);
    ImageOutput *out = ImageOutput::create (outformat.c_str());
    if (! out) {
        outstream 
            << "maketx ERROR: Could not find an ImageIO plugin to write " 
            << outformat << " files:" << geterror() << "\n";
        return false;
    }
    if (! out->supports ("tiles")) {
        outstream << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support tiled images\n";
        return false;
    }

    // The cache might mess with the apparent data format.  But for the 
    // purposes of what we should output, figure it out now, before the
    // file has been read and cached.
    TypeDesc out_dataformat = src->spec().format;

    if (configspec.format != TypeDesc::UNKNOWN)
        out_dataformat = configspec.format;
    
    // We cannot compute the prman / oiio options until after out_dataformat
    // has been determined, as it's required (and can potentially change 
    // out_dataformat too!)
    if (configspec.get_int_attribute("maketx:prman_options"))
        out_dataformat = set_prman_options (out_dataformat, configspec);
    else if (configspec.get_int_attribute("maketx:oiio_options"))
        out_dataformat = set_oiio_options (out_dataformat, configspec);

    // Read the full file locally if it's less than 1 GB, otherwise
    // allow the ImageBuf to use ImageCache to manage memory.
    int local_mb_thresh = configspec.get_int_attribute("maketx:read_local_MB",
                                                    1024);
    bool read_local = (src->spec().image_bytes() < local_mb_thresh * 1024*1024);

    bool verbose = configspec.get_int_attribute ("maketx:verbose");
    double misc_time_1 = alltime.lap();
    STATUS ("prep", misc_time_1);
    if (from_filename) {
        if (verbose)
            outstream << "Reading file: " << src->name() << std::endl;
        if (! src->read (0, 0, read_local)) {
            outstream  << "maketx ERROR: Could not read \"" 
                       << src->name() << "\" : " << src->geterror() << "\n";
            return false;
        }
    }
    stat_readtime += alltime.lap();
    STATUS (Strutil::format("read \"%s\"", src->name()), stat_readtime);
    
    // If requested - and we're a constant color - make a tiny texture instead
    // Only safe if the full/display window is the same as the data window.
    // Also note that this could affect the appearance when using "black"
    // wrap mode at runtime.
    std::vector<float> constantColor(src->nchannels());
    bool isConstantColor = false;
    if (configspec.get_int_attribute("maketx:constant_color_detect") &&
        src->spec().x == 0 && src->spec().y == 0 && src->spec().z == 0 &&
        src->spec().full_x == 0 && src->spec().full_y == 0 &&
        src->spec().full_z == 0 && src->spec().full_width == src->spec().width &&
        src->spec().full_height == src->spec().height &&
        src->spec().full_depth == src->spec().depth) {
        isConstantColor = ImageBufAlgo::isConstantColor (*src, &constantColor[0]);
        if (isConstantColor) {
            // Reset the image, to a new image, at the tile size
            ImageSpec newspec = src->spec();
            newspec.width  = std::min (configspec.tile_width, src->spec().width);
            newspec.height = std::min (configspec.tile_height, src->spec().height);
            newspec.depth  = std::min (configspec.tile_depth, src->spec().depth);
            newspec.full_width  = newspec.width;
            newspec.full_height = newspec.height;
            newspec.full_depth  = newspec.depth;
            std::string name = src->name() + ".constant_color";
            src->reset(name, newspec);
            ImageBufAlgo::fill (*src, &constantColor[0]);
            if (verbose) {
                outstream << "  Constant color image detected. ";
                outstream << "Creating " << newspec.width << "x" << newspec.height << " texture instead.\n";
            }
        }
    }
    
    int nchannels = configspec.get_int_attribute ("maketx:nchannels", -1);

    // If requested -- and alpha is 1.0 everywhere -- drop it.
    if (configspec.get_int_attribute("maketx:opaque_detect") &&
          src->spec().alpha_channel == src->nchannels()-1 &&
          nchannels <= 0 &&
          ImageBufAlgo::isConstantChannel(*src,src->spec().alpha_channel,1.0f)) {
        if (verbose)
            outstream << "  Alpha==1 image detected. Dropping the alpha channel.\n";
        boost::shared_ptr<ImageBuf> newsrc (new ImageBuf(src->name() + ".noalpha", src->spec()));
        ImageBufAlgo::setNumChannels (*newsrc, *src, src->nchannels()-1);
        std::swap (src, newsrc);   // N.B. the old src will delete
    }

    // If requested - and we're a monochrome image - drop the extra channels
    if (configspec.get_int_attribute("maketx:monochrome_detect") &&
          nchannels <= 0 &&
          src->nchannels() == 3 && src->spec().alpha_channel < 0 &&  // RGB only
          ImageBufAlgo::isMonochrome(*src)) {
        if (verbose)
            outstream << "  Monochrome image detected. Converting to single channel texture.\n";
        boost::shared_ptr<ImageBuf> newsrc (new ImageBuf(src->name() + ".monochrome", src->spec()));
        ImageBufAlgo::setNumChannels (*newsrc, *src, 1);
        std::swap (src, newsrc);
    }

    // If we've otherwise explicitly requested to write out a
    // specific number of channels, do it.
    if ((nchannels > 0) && (nchannels != src->nchannels())) {
        if (verbose)
            outstream << "  Overriding number of channels to " << nchannels << "\n";
        boost::shared_ptr<ImageBuf> newsrc (new ImageBuf(src->name() + ".channels", src->spec()));
        ImageBufAlgo::setNumChannels (*newsrc, *src, nchannels);
        std::swap (src, newsrc);
    }
    
    if (shadowmode) {
        // Some special checks for shadow maps
        if (src->spec().nchannels != 1) {
            outstream << "maketx ERROR: shadow maps require 1-channel images,\n"
                      << "\t\"" << src->name() << "\" is " 
                      << src->spec().nchannels << " channels\n";
            return false;
        }
        // Shadow maps only make sense for floating-point data.
        if (out_dataformat != TypeDesc::FLOAT &&
              out_dataformat != TypeDesc::HALF &&
              out_dataformat != TypeDesc::DOUBLE)
            out_dataformat = TypeDesc::FLOAT;
    }

    if (configspec.get_int_attribute("maketx:set_full_to_pixels")) {
        // User requested that we treat the image as uncropped or not
        // overscan
        ImageSpec &spec (src->specmod());
        spec.full_x = spec.x = 0;
        spec.full_y = spec.y = 0;
        spec.full_z = spec.z = 0;
        spec.full_width = spec.width;
        spec.full_height = spec.height;
        spec.full_depth = spec.depth;
    }

    // Copy the input spec
    ImageSpec srcspec = src->spec();
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
    if (orig_was_overscan)
        configspec.attribute ("wrapmodes", "black,black");

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
    dstspec.tile_width  = configspec.tile_width  ? configspec.tile_width  : 64;
    dstspec.tile_height = configspec.tile_height ? configspec.tile_height : 64;
    dstspec.tile_depth  = configspec.tile_depth  ? configspec.tile_depth  : 1;

    // Try to force zip (still can be overriden by configspec
    dstspec.attribute ("compression", "zip");
    // Always prefer contiguous channels, unless overridden by configspec
    dstspec.attribute ("planarconfig", "contig");
    // Default to black wrap mode, unless overridden by configspec
    dstspec.attribute ("wrapmodes", "black,black");

    if (configspec.get_int_attribute ("maketx:ignore_unassoc"))
        dstspec.erase_attribute ("oiio:UnassociatedAlpha");

    // Put a DateTime in the out file, either now, or matching the date
    // stamp of the input file (if update mode).
    time_t date;
    if (updatemode && from_filename)
        date = in_time;  // update mode: use the time stamp of the input
    else
        time (&date);    // not update: get the time now
    dstspec.attribute ("DateTime", datestring(date));

    std::string cmdline = configspec.get_string_attribute ("maketx:full_command_line");
    if (! cmdline.empty()) {
        // Append command to image history
        std::string history = dstspec.get_string_attribute ("Exif:ImageHistory");
        if (history.length() && ! Strutil::iends_with (history, "\n"))
            history += std::string("\n");
        history += cmdline;
        dstspec.attribute ("Exif:ImageHistory", history);
    }

    bool prman_metadata = configspec.get_int_attribute ("maketx:prman_metadata");
    if (shadowmode) {
        dstspec.attribute ("textureformat", "Shadow");
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Shadow");
    } else if (envlatlmode) {
        dstspec.attribute ("textureformat", "LatLong Environment");
        configspec.attribute ("wrapmodes", "periodic,clamp");
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Latlong Environment");
    } else {
        dstspec.attribute ("textureformat", "Plain Texture");
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Plain Texture");
    }

    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?

    // If --checknan was used and it's a floating point image, check for
    // nonfinite (NaN or Inf) values and abort if they are found.
    if (configspec.get_int_attribute("maketx:checknan") &&
                    (srcspec.format.basetype == TypeDesc::FLOAT ||
                     srcspec.format.basetype == TypeDesc::HALF ||
                     srcspec.format.basetype == TypeDesc::DOUBLE)) {
        int found_nonfinite = 0;
        ImageBufAlgo::parallel_image (boost::bind(check_nan_block, *src, _1, boost::ref(found_nonfinite)),
                                      OIIO::get_roi(dstspec));
        if (found_nonfinite) {
            if (found_nonfinite > 3)
                outstream << "maketx ERROR: ...and Nan/Inf at "
                          << (found_nonfinite-3) << " other pixels\n";
            return false;
        }
    }
    
    // Fix nans/infs (if requested)
    std::string fixnan = configspec.get_string_attribute("maketx:fixnan");
    ImageBufAlgo::NonFiniteFixMode fixmode = ImageBufAlgo::NONFINITE_NONE;
    if (fixnan.empty() || fixnan == "none") { }
    else if (fixnan == "black") { fixmode = ImageBufAlgo::NONFINITE_BLACK; }
    else if (fixnan == "box3") { fixmode = ImageBufAlgo::NONFINITE_BOX3; }
    else {
        outstream << "maketx ERROR: Unknown --fixnan mode " << " fixnan\n";
        return false;
    }
    int pixelsFixed = 0;
    if (!ImageBufAlgo::fixNonFinite (*src, *src, fixmode, &pixelsFixed)) {
        outstream << "maketx ERROR: Error fixing nans/infs.\n";
        return false;
    }
    if (verbose && pixelsFixed>0) {
        outstream << "  Warning: " << pixelsFixed << " nan/inf pixels fixed.\n";
    }
    // FIXME -- we'd like to not call fixNonFinite if fixnan mode is
    // "none", or if we did the checknan and found no NaNs.  But deep
    // inside fixNonFinite, it forces a full read into local mem of any
    // cached images, and that affects performance. Come back to this
    // and solve later.

    double misc_time_2 = alltime.lap();
    STATUS ("misc2", misc_time_2);

    // Color convert the pixels, if needed, in place.  If a color
    // conversion is required we will promote the src to floating point
    // (or there wont be enough precision potentially).  Also,
    // independently color convert the constant color metadata
    std::string incolorspace = configspec.get_string_attribute ("incolorspace");
    std::string outcolorspace = configspec.get_string_attribute ("outcolorspace");
    if (!incolorspace.empty() && !outcolorspace.empty() && incolorspace != outcolorspace) {
        if (verbose)
            outstream << "  Converting from colorspace " << incolorspace 
                      << " to colorspace " << outcolorspace << std::endl;

        // Buffer for the color-corrected version. Start by making it just
        // another pointer to the original source.
        boost::shared_ptr<ImageBuf> ccSrc (src);  // color-corrected buffer

        if (src->spec().format != TypeDesc::FLOAT) {
            // If the original src buffer isn't float, make a scratch space
            // that is float.
            ImageSpec floatSpec = src->spec();
            floatSpec.set_format (TypeDesc::FLOAT);
            ccSrc.reset (new ImageBuf ("bitdepth promoted", floatSpec));
        }

        ColorConfig colorconfig;
        if (colorconfig.error()) {
            outstream << "Error Creating ColorConfig\n";
            outstream << colorconfig.geterror() << std::endl;
            return false;
        }
        
        ColorProcessor * processor = colorconfig.createColorProcessor (
            incolorspace.c_str(), outcolorspace.c_str());
        if (!processor || colorconfig.error()) {
            outstream << "Error Creating Color Processor." << std::endl;
            outstream << colorconfig.geterror() << std::endl;
            return false;
        }
        
        bool unpremult = configspec.get_int_attribute ("maketx:unpremult");
        if (unpremult && verbose)
            outstream << "  Unpremulting image..." << std::endl;
        
        if (!ImageBufAlgo::colorconvert (*ccSrc, *src, processor, unpremult)) {
            outstream << "Error applying color conversion to image.\n";
            return false;
        }
        
        if (isConstantColor) {
            if (!ImageBufAlgo::colorconvert (&constantColor[0],
                static_cast<int>(constantColor.size()), processor, unpremult)) {
                outstream << "Error applying color conversion to constant color.\n";
                return false;
            }
        }

        ColorConfig::deleteColorProcessor(processor);
        processor = NULL;

        // swap the color-converted buffer and src (making src be the
        // working master that's color converted).
        std::swap (src, ccSrc);
        // N.B. at this point, ccSrc will go out of scope, freeing it if
        // it was a scratch buffer.
        stat_colorconverttime += alltime.lap();
        STATUS ("color convert", stat_colorconverttime);
    }

    // Force float for the sake of the ImageBuf math.
    // Also force float if we do not allow for the pixel shift,
    // since resize_block_ requires floating point buffers.
    const int allow_shift = configspec.get_int_attribute("maketx:allow_pixel_shift");
    if (configspec.get_int_attribute("maketx:forcefloat", 1) || !allow_shift)
        dstspec.set_format (TypeDesc::FLOAT);

    // Handle resize to power of two, if called for
    if (configspec.get_int_attribute("maketx:resize")  &&  ! shadowmode) {
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
    // (converting TO an OpenEXR environment map).
    if (envlatlmode && 
        (Strutil::iequals(configspec.get_string_attribute("maketx:fileformatname"),"openexr") ||
         Strutil::iends_with(outputfilename,".exr")))
        do_resize = true;

    if (do_resize && orig_was_overscan &&
        out && !out->supports("displaywindow")) {
        outstream << "maketx ERROR: format " << out->format_name()
                  << " does not support separate display windows,\n"
                  << "              which is necessary when combining resizing"
                  << " and an input image with overscan.";
        return false;
    }

    std::string filtername = configspec.get_string_attribute ("maketx:filtername", "box");
    Filter2D *filter = setup_filter (filtername);
    if (! filter) {
        outstream << "maketx ERROR: could not make filter '" << filtername << "\n";
        return false;
    }

    double misc_time_3 = alltime.lap(); 
    STATUS ("misc3", misc_time_3);

    boost::shared_ptr<ImageBuf> toplevel;  // Ptr to top level of mipmap
    if (! do_resize && dstspec.format == src->spec().format) {
        // No resize needed, no format conversion needed -- just stick to
        // the image we've already got
        toplevel = src;
    } else  if (! do_resize) {
        // Need format conversion, but no resize -- just copy the pixels
        toplevel.reset (new ImageBuf ("temp", dstspec));
        ImageBufAlgo::parallel_image (boost::bind(copy_block,boost::ref(*toplevel),boost::cref(*src),_1),
                                      OIIO::get_roi(dstspec));
    } else {
        // Resize
        if (verbose)
            outstream << "  Resizing image to " << dstspec.width 
                      << " x " << dstspec.height << std::endl;
        toplevel.reset (new ImageBuf ("temp", dstspec));
        if (filtername == "box" && filter->width() == 1.0f)
            ImageBufAlgo::parallel_image (boost::bind(resize_block, boost::ref(*toplevel), boost::cref(*src), _1, envlatlmode, allow_shift),
                                          OIIO::get_roi(dstspec));
        else
            ImageBufAlgo::parallel_image (boost::bind(resize_block_HQ, boost::ref(*toplevel), boost::cref(*src), _1, filter),
                                          OIIO::get_roi(dstspec));
    }
    stat_resizetime += alltime.lap();
    STATUS ("resize & data convert", stat_resizetime);

    // toplevel now holds the color converted, format converted, resized
    // master copy.  We can release src.
    src.reset ();


    // Update the toplevel ImageDescription with the sha1 pixel hash and
    // constant color
    std::string desc = dstspec.get_string_attribute ("ImageDescription");
    bool updatedDesc = false;
    
    // Eliminate any SHA-1 or ConstantColor hints in the ImageDescription.
    if (desc.size()) {
        desc = boost::regex_replace (desc, boost::regex("SHA-1=[[:xdigit:]]*[ ]*"), "");
        static const char *fp_number_pattern =
            "([+-]?((?:(?:[[:digit:]]*\\.)?[[:digit:]]+(?:[eE][+-]?[[:digit:]]+)?)))";
        const std::string color_pattern =
            std::string ("ConstantColor=(\\[?") + fp_number_pattern + ",?)+\\]?[ ]*";
        desc = boost::regex_replace (desc, boost::regex(color_pattern), "");
        updatedDesc = true;
    }
    
    // The hash is only computed for the top mipmap level of pixel data.
    // Thus, any additional information that will effect the lower levels
    // (such as filtering information) needs to be manually added into the
    // hash.
    std::ostringstream addlHashData;
    addlHashData << filter->name() << " ";
    addlHashData << filter->width() << " ";

    const int sha1_blocksize = 256;
    std::string hash_digest = configspec.get_int_attribute("maketx:hash", 1) ?
        ImageBufAlgo::computePixelHashSHA1 (*toplevel, addlHashData.str(),
                                            ROI::All(), sha1_blocksize) : "";
    if (hash_digest.length()) {
        if (desc.length())
            desc += " ";
        desc += "SHA-1=";
        desc += hash_digest;
        if (verbose)
            outstream << "  SHA-1: " << hash_digest << std::endl;
        updatedDesc = true;
        dstspec.attribute ("oiio:SHA-1", hash_digest);
    }
    double stat_hashtime = alltime.lap();
    STATUS ("SHA-1 hash", stat_hashtime);
  
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
        if (verbose)
            outstream << "  ConstantColor: " << os.str() << std::endl;
        updatedDesc = true;
        dstspec.attribute ("oiio:ConstantColor", os.str());
    }
    
    if (updatedDesc) {
        dstspec.attribute ("ImageDescription", desc);
    }


    if (configspec.get_float_attribute("fovcot") == 0.0f) {
        configspec.attribute("fovcot", float(srcspec.full_width) / 
                                       float(srcspec.full_height));
    }

    maketx_merge_spec (dstspec, configspec);

    double misc_time_4 = alltime.lap();
    STATUS ("misc4", misc_time_4);

    // Write out, and compute, the mipmap levels for the speicifed image
    bool nomipmap = configspec.get_int_attribute ("maketx:nomipmap");
    bool ok = write_mipmap (mode, toplevel, dstspec, outputfilename,
                            out, out_dataformat, !shadowmode && !nomipmap,
                            filter, configspec, outstream,
                            stat_writetime, stat_miptime, peak_mem);
    delete out;  // don't need it any more

    // If using update mode, stamp the output file with a modification time
    // matching that of the input file.
    if (ok && updatemode && from_filename)
        Filesystem::last_write_time (outputfilename, in_time);

    Filter2D::destroy (filter);

    if (verbose || configspec.get_int_attribute("maketx:stats")) {
        double all = alltime();
        outstream << Strutil::format ("maketx run time (seconds): %5.2f\n", all);;

        outstream << Strutil::format ("  file read:       %5.2f\n", stat_readtime);
        outstream << Strutil::format ("  file write:      %5.2f\n", stat_writetime);
        outstream << Strutil::format ("  initial resize:  %5.2f\n", stat_resizetime);
        outstream << Strutil::format ("  hash:            %5.2f\n", stat_hashtime);
        outstream << Strutil::format ("  mip computation: %5.2f\n", stat_miptime);
        outstream << Strutil::format ("  color convert:   %5.2f\n", stat_colorconverttime);
        outstream << Strutil::format ("  unaccounted:     %5.2f  (%5.2f %5.2f %5.2f %5.2f)\n",
                                      all-stat_readtime-stat_writetime-stat_resizetime-stat_hashtime-stat_miptime,
                                      misc_time_1, misc_time_2, misc_time_3, misc_time_4);
        outstream << Strutil::format ("maketx peak memory used: %s\n",
                                      Strutil::memformat(peak_mem));
    }

#undef STATUS
    return ok;
}



bool
ImageBufAlgo::make_texture (ImageBufAlgo::MakeTextureMode mode,
                            const std::string &filename,
                            const std::string &outputfilename,
                            const ImageSpec &configspec,
                            std::ostream *outstream)
{
    return make_texture_impl (mode, NULL, filename, outputfilename,
                              configspec, outstream);
}



bool
ImageBufAlgo::make_texture (ImageBufAlgo::MakeTextureMode mode,
                            const std::vector<std::string> &filenames,
                            const std::string &outputfilename,
                            const ImageSpec &configspec,
                            std::ostream *outstream_ptr)
{
    return make_texture_impl (mode, NULL, filenames[0], outputfilename,
                              configspec, outstream_ptr);
}



bool
ImageBufAlgo::make_texture (ImageBufAlgo::MakeTextureMode mode,
                            const ImageBuf &input,
                            const std::string &outputfilename,
                            const ImageSpec &configspec,
                            std::ostream *outstream)
{
    return make_texture_impl (mode, &input, "", outputfilename,
                              configspec, outstream);
}
