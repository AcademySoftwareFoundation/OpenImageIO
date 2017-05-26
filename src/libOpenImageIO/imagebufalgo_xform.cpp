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

/// \file
/// ImageBufAlgo functions for filtered transformations


#include <OpenEXR/half.h>
#include <OpenEXR/ImathMatrix.h>
#include <OpenEXR/ImathBox.h>

#include <cmath>
#include <memory>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/thread.h>

OIIO_NAMESPACE_BEGIN


namespace {

// Poor man's Dual2<float> makes it easy to compute with differentials. For
// a rich man's implementation and full documentation, see
// OpenShadingLanguage (dual2.h).
class Dual2 {
public:
    float val() const { return m_val; }
    float dx() const { return m_dx; }
    float dy() const { return m_dy; }
    Dual2 (float val) : m_val(val), m_dx(0.0f), m_dy(0.0f) {}
    Dual2 (float val, float dx, float dy) : m_val(val), m_dx(dx), m_dy(dy) {}
    Dual2& operator= (float f) { m_val = f; m_dx = 0.0f; m_dy = 0.0f; return *this; }
    friend Dual2 operator+ (const Dual2 &a, const Dual2 &b) {
        return Dual2 (a.m_val+b.m_val, a.m_dx+b.m_dx, a.m_dy+b.m_dy);
    }
    friend Dual2 operator+ (const Dual2 &a, float b) {
        return Dual2 (a.m_val+b, a.m_dx, a.m_dy);
    }
    friend Dual2 operator* (const Dual2 &a, float b) {
        return Dual2 (a.m_val*b, a.m_dx*b, a.m_dy*b);
    }
    friend Dual2 operator* (const Dual2 &a, const Dual2 &b) {
        // Use the chain rule
        return Dual2 (a.m_val*b.m_val,
                      a.m_val*b.m_dx + a.m_dx*b.m_val,
                      a.m_val*b.m_dy + a.m_dy*b.m_val);
    }
    friend Dual2 operator/ (const Dual2 &a, const Dual2 &b) {
        float bvalinv = 1.0f / b.m_val;
        float aval_bval = a.m_val * bvalinv;
        return Dual2 (aval_bval,
                      bvalinv * (a.m_dx - aval_bval * b.m_dx),
                      bvalinv * (a.m_dy - aval_bval * b.m_dy));
    }
private:
    float m_val, m_dx, m_dy;
};

/// Transform a 2D point (x,y) with derivatives by a 3x3 affine matrix to
/// obtain a transformed point with derivatives.
inline void
robust_multVecMatrix (const Imath::M33f &M,
                      const Dual2 &x, const Dual2 &y,
                      Dual2 &outx, Dual2 &outy)
{
    Dual2 a = x * M[0][0] + y * M[1][0] + M[2][0];
    Dual2 b = x * M[0][1] + y * M[1][1] + M[2][1];
    Dual2 w = x * M[0][2] + y * M[1][2] + M[2][2];

    if (w.val() != 0.0f) {
       outx = a / w;
       outy = b / w;
    } else {
       outx = 0.0f;
       outy = 0.0f;
    }
}



// Transform an ROI by an affine matrix.
ROI
transform (const Imath::M33f &M, ROI roi)
{
    Imath::V2f ul (roi.xbegin+0.5f, roi.ybegin+0.5f);
    Imath::V2f ur (roi.xend-0.5f, roi.ybegin+0.5f);
    Imath::V2f ll (roi.xbegin+0.5f, roi.yend-0.5f);
    Imath::V2f lr (roi.xend-0.5f, roi.yend-0.5f);
    M.multVecMatrix (ul, ul);
    M.multVecMatrix (ur, ur);
    M.multVecMatrix (ll, ll);
    M.multVecMatrix (lr, lr);
    Imath::Box2f box (ul);
    box.extendBy (ll);
    box.extendBy (ur);
    box.extendBy (lr);
    int xmin = int (floorf(box.min.x));
    int ymin = int (floorf(box.min.y));
    int xmax = int (floorf(box.max.x)) + 1;
    int ymax = int (floorf(box.max.y)) + 1;
    return ROI (xmin, xmax, ymin, ymax, roi.zbegin, roi.zend, roi.chbegin, roi.chend);
}



// Given s,t image space coordinates and their derivatives, compute a 
// filtered sample using the derivatives to guide the size of the filter
// footprint.
template<typename SRCTYPE>
inline void
filtered_sample (const ImageBuf &src, float s, float t,
                 float dsdx, float dtdx, float dsdy, float dtdy,
                 const Filter2D *filter, ImageBuf::WrapMode wrap,
                 float *result)
{
    DASSERT (filter);
    // Just use isotropic filtering
    float ds = std::max (1.0f, std::max (fabsf(dsdx), fabsf(dsdy)));
    float dt = std::max (1.0f, std::max (fabsf(dtdx), fabsf(dtdy)));
    float ds_inv = 1.0f / ds;
    float dt_inv = 1.0f / dt;
    float filterrad_s = 0.5f * ds * filter->width();
    float filterrad_t = 0.5f * dt * filter->width();
    ImageBuf::ConstIterator<SRCTYPE> samp (src, 
                      (int)floorf(s-filterrad_s), (int)ceilf(s+filterrad_s),
                      (int)floorf(t-filterrad_t), (int)ceilf(t+filterrad_t),
                      0, 1, wrap);
    int nc = src.nchannels();
    float *sum = ALLOCA (float, nc);
    memset (sum, 0, nc*sizeof(float));
    float total_w = 0.0f;
    for ( ; ! samp.done(); ++samp) {
        float w = (*filter) (ds_inv*(samp.x()+0.5f-s), dt_inv*(samp.y()+0.5f-t));
        for (int c = 0; c < nc; ++c)
            sum[c] += w * samp[c];
        total_w += w;
    }
    if (total_w != 0.0f)
        for (int c = 0; c < nc; ++c)
            result[c] = sum[c] / total_w;
    else
        for (int c = 0; c < nc; ++c)
            result[c] = 0.0f;
}

} // end anon namespace




template<typename DSTTYPE, typename SRCTYPE>
static bool
resize_ (ImageBuf &dst, const ImageBuf &src,
         Filter2D *filter, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){

    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());
    int nchannels = dstspec.nchannels;

    // Local copies of the source image window, converted to float
    float srcfx = srcspec.full_x;
    float srcfy = srcspec.full_y;
    float srcfw = srcspec.full_width;
    float srcfh = srcspec.full_height;

    // Ratios of dst/src size.  Values larger than 1 indicate that we
    // are maximizing (enlarging the image), and thus want to smoothly
    // interpolate.  Values less than 1 indicate that we are minimizing
    // (shrinking the image), and thus want to properly filter out the
    // high frequencies.
    float xratio = float(dstspec.full_width) / srcfw; // 2 upsize, 0.5 downsize
    float yratio = float(dstspec.full_height) / srcfh;

    float dstfx = float (dstspec.full_x);
    float dstfy = float (dstspec.full_y);
    float dstfw = float (dstspec.full_width);
    float dstfh = float (dstspec.full_height);
    float dstpixelwidth = 1.0f / dstfw;
    float dstpixelheight = 1.0f / dstfh;
    float *pel = ALLOCA (float, nchannels);
    float filterrad = filter->width() / 2.0f;

    // radi,radj is the filter radius, as an integer, in source pixels.  We
    // will filter the source over [x-radi, x+radi] X [y-radj,y+radj].
    int radi = (int) ceilf (filterrad/xratio);
    int radj = (int) ceilf (filterrad/yratio);
    int xtaps = 2*radi + 1;
    int ytaps = 2*radj + 1;
    bool separable = filter->separable();
    float *yfiltval = ALLOCA (float, ytaps);
    float *xfiltval_all = NULL;
    if (separable) {
        // For separable filters, horizontal tap weights will be the same
        // for every column. So we precompute all the tap weights for every
        // x position we'll need. We do the same thing in y, but row by row
        // inside the loop (since we never revisit a y row). This
        // substantially speeds up resize.
        xfiltval_all = ALLOCA (float, xtaps * roi.width());
        for (int x = roi.xbegin;  x < roi.xend;  ++x) {
            float *xfiltval = xfiltval_all + (x-roi.xbegin) * xtaps;
            float s = (x-dstfx+0.5f)*dstpixelwidth;
            float src_xf = srcfx + s * srcfw;
            int src_x;
            float src_xf_frac = floorfrac (src_xf, &src_x);
            for (int c = 0;  c < nchannels;  ++c)
                pel[c] = 0.0f;
            float totalweight_x = 0.0f;
            for (int i = 0;  i < xtaps;  ++i) {
                float w = filter->xfilt (xratio * (i-radi-(src_xf_frac-0.5f)));
                xfiltval[i] = w;
                totalweight_x += w;
            }
            if (totalweight_x != 0.0f)
                for (int i = 0;  i < xtaps;  ++i)  // normalize x filter
                    xfiltval[i] /= totalweight_x;  // weights
        }
    }

#if 0
    std::cerr << "Resizing " << srcspec.full_width << "x" << srcspec.full_height
              << " to " << dstspec.full_width << "x" << dstspec.full_height << "\n";
    std::cerr << "ratios = " << xratio << ", " << yratio << "\n";
    std::cerr << "examining src filter " << filter->name()
              << " support radius of " << radi << " x " << radj << " pixels\n";
    std::cout << "  " << xtaps << "x" << ytaps << " filter taps\n";
    std::cerr << "dst range " << roi << "\n";
    std::cerr << "separable filter\n";
#endif

#define USE_SPECIAL 0
#if USE_SPECIAL
    // Special case: src and dst are local memory, float buffers, and we're
    // operating on all channels, <= 4.
    bool special = 
       (   (is_same<DSTTYPE,float>::value || is_same<DSTTYPE,half>::value)
        && (is_same<SRCTYPE,float>::value || is_same<SRCTYPE,half>::value)
        // && dst.localpixels() // has to be, because it's writeable
        && src.localpixels()
        // && R.contains_roi(roi)  // has to be, because IBAPrep
        && src.contains_roi(roi)
        && roi.chbegin == 0 && roi.chend == dst.nchannels()
        && roi.chend == src.nchannels() && roi.chend <= 4
        && separable);
#endif

    // We're going to loop over all output pixels we're interested in.
    //
    // (s,t) = NDC space coordinates of the output sample we are computing.
    //     This is the "sample point".
    // (src_xf, src_xf) = source pixel space float coordinates of the
    //     sample we're computing. We want to compute the weighted sum
    //     of all the source image pixels that fall under the filter when
    //     centered at that location.
    // (src_x, src_y) = image space integer coordinates of the floor,
    //     i.e., the closest pixel in the source image.
    // src_xf_frac and src_yf_frac are the position within that pixel
    //     of our sample.
    //
    // Separate cases for separable and non-separable filters.
    if (separable) {
        ImageBuf::Iterator<DSTTYPE> out (dst, roi);
        ImageBuf::ConstIterator<SRCTYPE> srcpel (src, ImageBuf::WrapClamp);
        for (int y = roi.ybegin;  y < roi.yend;  ++y) {
            float t = (y-dstfy+0.5f)*dstpixelheight;
            float src_yf = srcfy + t * srcfh;
            int src_y;
            float src_yf_frac = floorfrac (src_yf, &src_y);
            // If using separable filters, our vertical set of filter tap
            // weights will be the same for the whole scanline we're on.  Just
            // compute and normalize them once.
            float totalweight_y = 0.0f;
            for (int j = 0;  j < ytaps;  ++j) {
                float w = filter->yfilt (yratio * (j-radj-(src_yf_frac-0.5f)));
                yfiltval[j] = w;
                totalweight_y += w;
            }
            if (totalweight_y != 0.0f)
                for (int i = 0;  i < ytaps;  ++i)
                    yfiltval[i] /= totalweight_y;

            for (int x = roi.xbegin;  x < roi.xend;  ++x, ++out) {
                float s = (x-dstfx+0.5f)*dstpixelwidth;
                float src_xf = srcfx + s * srcfw;
                int src_x = ifloor (src_xf);
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] = 0.0f;
                const float *xfiltval = xfiltval_all + (x-roi.xbegin) * xtaps;
                float totalweight_x = 0.0f;
                for (int i = 0;  i < xtaps;  ++i)
                    totalweight_x += xfiltval[i];
                if (totalweight_x != 0.0f) {
                    srcpel.rerange (src_x-radi, src_x+radi+1,
                                    src_y-radj, src_y+radj+1,
                                    0, 1, ImageBuf::WrapClamp);
                    for (int j = -radj;  j <= radj;  ++j) {
                        float wy = yfiltval[j+radj];
                        if (wy == 0.0f) {
                            // 0 weight for this y tap -- move to next line
                            srcpel.pos (srcpel.x(), srcpel.y()+1, srcpel.z());
                            continue;
                        }
                        for (int i = 0;  i < xtaps; ++i, ++srcpel) {
                            float w = wy * xfiltval[i];
                            if (w)
                                for (int c = 0;  c < nchannels;  ++c)
                                    pel[c] += w * srcpel[c];
                        }
                    }
                }
                // Copy the pixel value (already normalized) to the output.
                DASSERT (out.x() == x && out.y() == y);
                if (totalweight_y == 0.0f) {
                    // zero it out
                    for (int c = 0;  c < nchannels;  ++c)
                        out[c] = 0.0f;
                } else {
                    for (int c = 0;  c < nchannels;  ++c)
                        out[c] = pel[c];
                }
            }
        }

    } else {
        // Non-separable filter
        ImageBuf::Iterator<DSTTYPE> out (dst, roi);
        ImageBuf::ConstIterator<SRCTYPE> srcpel (src, ImageBuf::WrapClamp);
        for (int y = roi.ybegin;  y < roi.yend;  ++y) {
            float t = (y-dstfy+0.5f)*dstpixelheight;
            float src_yf = srcfy + t * srcfh;
            int src_y;
            float src_yf_frac = floorfrac (src_yf, &src_y);
            for (int x = roi.xbegin;  x < roi.xend;  ++x, ++out) {
                float s = (x-dstfx+0.5f)*dstpixelwidth;
                float src_xf = srcfx + s * srcfw;
                int src_x;
                float src_xf_frac = floorfrac (src_xf, &src_x);
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] = 0.0f;
                float totalweight = 0.0f;
                srcpel.rerange (src_x-radi, src_x+radi+1,
                                src_y-radi, src_y+radi+1,
                                0, 1, ImageBuf::WrapClamp);
                for (int j = -radj;  j <= radj;  ++j) {
                    for (int i = -radi;  i <= radi;  ++i, ++srcpel) {
                        DASSERT (! srcpel.done());
                        float w = (*filter)(xratio * (i-(src_xf_frac-0.5f)),
                                            yratio * (j-(src_yf_frac-0.5f)));
                        if (w) {
                            totalweight += w;
                            for (int c = 0;  c < nchannels;  ++c)
                                pel[c] += w * srcpel[c];
                        }
                    }
                }
                DASSERT (srcpel.done());
                // Rescale pel to normalize the filter and write it to the
                // output image.
                DASSERT (out.x() == x && out.y() == y);
                if (totalweight == 0.0f) {
                    // zero it out
                    for (int c = 0;  c < nchannels;  ++c)
                        out[c] = 0.0f;
                } else {
                    for (int c = 0;  c < nchannels;  ++c)
                        out[c] = pel[c] / totalweight;
                }
            }
        }
    }

    });  // end of parallel_image
    return true;
}



bool
ImageBufAlgo::resize (ImageBuf &dst, const ImageBuf &src,
                      Filter2D *filter, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src,
            IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME |
            IBAprep_NO_COPY_ROI_FULL))
        return false;

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filterptr ((Filter2D*)NULL, Filter2D::destroy);
    bool allocfilter = (filter == NULL);
    if (allocfilter) {
        // If no filter was provided, punt and just linearly interpolate.
        const ImageSpec &srcspec (src.spec());
        const ImageSpec &dstspec (dst.spec());
        float wratio = float(dstspec.full_width) / float(srcspec.full_width);
        float hratio = float(dstspec.full_height) / float(srcspec.full_height);
        float w = 2.0f * std::max (1.0f, wratio);
        float h = 2.0f * std::max (1.0f, hratio);
        filter = Filter2D::create ("triangle", w, h);
        filterptr.reset (filter);
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "resize", resize_,
                          dst.spec().format, src.spec().format,
                          dst, src, filter, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::resize (ImageBuf &dst, const ImageBuf &src,
                      string_view filtername_, float fwidth,
                      ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src,
            IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME |
            IBAprep_NO_COPY_ROI_FULL))
        return false;
    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());

    // Resize ratios
    float wratio = float(dstspec.full_width) / float(srcspec.full_width);
    float hratio = float(dstspec.full_height) / float(srcspec.full_height);

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filter ((Filter2D*)NULL, Filter2D::destroy);
    std::string filtername = filtername_;
    if (filtername.empty()) {
        // No filter name supplied -- pick a good default
        if (wratio > 1.0f || hratio > 1.0f)
            filtername = "blackman-harris";
        else
            filtername = "lanczos3";
    }
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc fd;
        Filter2D::get_filterdesc (i, &fd);
        if (fd.name == filtername) {
            float w = fwidth > 0.0f ? fwidth : fd.width * std::max (1.0f, wratio);
            float h = fwidth > 0.0f ? fwidth : fd.width * std::max (1.0f, hratio);
            filter.reset (Filter2D::create (filtername, w, h));
            break;
        }
    }
    if (! filter) {
        dst.error ("Filter \"%s\" not recognized", filtername);
        return false;
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "resize", resize_,
                          dstspec.format, srcspec.format,
                          dst, src, filter.get(), roi, nthreads);
    return ok;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
resample_ (ImageBuf &dst, const ImageBuf &src, bool interpolate,
           ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        const ImageSpec &srcspec (src.spec());
        const ImageSpec &dstspec (dst.spec());
        int nchannels = src.nchannels();
        bool deep = src.deep();
        ASSERT (deep == dst.deep());

        // Local copies of the source image window, converted to float
        float srcfx = srcspec.full_x;
        float srcfy = srcspec.full_y;
        float srcfw = srcspec.full_width;
        float srcfh = srcspec.full_height;

        float dstfx = dstspec.full_x;
        float dstfy = dstspec.full_y;
        float dstfw = dstspec.full_width;
        float dstfh = dstspec.full_height;
        float dstpixelwidth = 1.0f / dstfw;
        float dstpixelheight = 1.0f / dstfh;
        float *pel = ALLOCA (float, nchannels);

        ImageBuf::Iterator<DSTTYPE> out (dst, roi);
        ImageBuf::ConstIterator<SRCTYPE> srcpel (src);
        for (int y = roi.ybegin;  y < roi.yend;  ++y) {
            // s,t are NDC space
            float t = (y-dstfy+0.5f)*dstpixelheight;
            // src_xf, src_xf are image space float coordinates
            float src_yf = srcfy + t * srcfh;
            // src_x, src_y are image space integer coordinates of the floor
            int src_y = ifloor (src_yf);
            for (int x = roi.xbegin;  x < roi.xend;  ++x, ++out) {
                float s = (x-dstfx+0.5f)*dstpixelwidth;
                float src_xf = srcfx + s * srcfw;
                int src_x = ifloor (src_xf);
                if (deep) {
                    srcpel.pos (src_x, src_y, 0);
                    int nsamps = srcpel.deep_samples();
                    ASSERT (nsamps == out.deep_samples());
                    if (! nsamps)
                        continue;
                    for (int c = 0; c < nchannels; ++c) {
                        if (dstspec.channelformat(c) == TypeDesc::UINT32)
                            for (int samp = 0; samp < nsamps; ++samp)
                                out.set_deep_value (c, samp, srcpel.deep_value_uint(c, samp));
                        else
                            for (int samp = 0; samp < nsamps; ++samp)
                                out.set_deep_value (c, samp, srcpel.deep_value(c, samp));
                    }
                } else if (interpolate) {
                    // Non-deep image, bilinearly interpolate
                    src.interppixel (src_xf, src_yf, pel);
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        out[c] = pel[c];
                } else {
                    // Non-deep image, just copy closest pixel
                    srcpel.pos (src_x, src_y, 0);
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        out[c] = srcpel[c];
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::resample (ImageBuf &dst, const ImageBuf &src,
                        bool interpolate, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src,
            IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME |
            IBAprep_NO_COPY_ROI_FULL | IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        const ImageSpec &srcspec (src.spec());
        const ImageSpec &dstspec (dst.spec());
        float srcfx = srcspec.full_x;
        float srcfy = srcspec.full_y;
        float srcfw = srcspec.full_width;
        float srcfh = srcspec.full_height;
        float dstpixelwidth = 1.0f / dstspec.full_width;
        float dstpixelheight = 1.0f / dstspec.full_height;
        ImageBuf::ConstIterator<float> srcpel (src, roi);
        ImageBuf::Iterator<float> dstpel (dst, roi);
        for ( ;  !dstpel.done();  ++dstpel, ++srcpel) {
            float s = (dstpel.x()-dstspec.full_x+0.5f)*dstpixelwidth;
            float t = (dstpel.y()-dstspec.full_y+0.5f)*dstpixelheight;
            int src_y = ifloor (srcfy + t * srcfh);
            int src_x = ifloor (srcfx + s * srcfw);
            srcpel.pos (src_x, src_y, 0);
            dstpel.set_deep_samples (srcpel.deep_samples ());
        }
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "resample", resample_,
                          dst.spec().format, src.spec().format,
                          dst, src, interpolate, roi, nthreads);
    return ok;
}



#if 0
template<typename DSTTYPE, typename SRCTYPE>
static bool
affine_resample_ (ImageBuf &dst, const ImageBuf &src, const Imath::M33f &Minv,
                  ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<DSTTYPE,DSTTYPE> d (dst, roi);
        ImageBuf::ConstIterator<SRCTYPE,DSTTYPE> s (src);
        for (  ;  ! d.done();  ++d) {
            Imath::V2f P (d.x()+0.5f, d.y()+0.5f);
            Minv.multVecMatrix (P, P);
            s.pos (int(floorf(P.x)), int(floorf(P.y)), d.z());
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                d[c] = s[c];
        }
    });
    return true;
}
#endif



template<typename DSTTYPE, typename SRCTYPE>
static bool
warp_ (ImageBuf &dst, const ImageBuf &src, const Imath::M33f &M,
       const Filter2D *filter, ImageBuf::WrapMode wrap,
       ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int nc = dst.nchannels();
        float *pel = ALLOCA (float, nc);
        memset (pel, 0, nc*sizeof(float));
        Imath::M33f Minv = M.inverse();
        ImageBuf::Iterator<DSTTYPE> out (dst, roi);
        for (  ;  ! out.done();  ++out) {
            Dual2 x (out.x()+0.5f, 1.0f, 0.0f);
            Dual2 y (out.y()+0.5f, 0.0f, 1.0f);
            robust_multVecMatrix (Minv, x, y, x, y);
            filtered_sample<SRCTYPE> (src, x.val(), y.val(),
                                      x.dx(), y.dx(), x.dy(), y.dy(),
                                      filter, wrap, pel);
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                out[c] = pel[c];

        }
    });
    return true;
}



bool
ImageBufAlgo::warp (ImageBuf &dst, const ImageBuf &src,
                    const Imath::M33f &M,
                    const Filter2D *filter,
                    bool recompute_roi, ImageBuf::WrapMode wrap,
                    ROI roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi, dst_roi_full;
    if (dst.initialized()) {
        dst_roi = roi.defined() ? roi : dst.roi();
        dst_roi_full = dst.roi_full();
    } else {
        dst_roi = roi.defined() ? roi : (recompute_roi ? transform (M, src.roi()) : src.roi());
        dst_roi_full = src_roi_full;
    }
    dst_roi.chend = std::min (dst_roi.chend, src.nchannels());
    dst_roi_full.chend = std::min (dst_roi_full.chend, src.nchannels());

    if (! IBAprep (dst_roi, &dst, &src, IBAprep_NO_SUPPORT_VOLUME))
        return false;

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filterptr ((Filter2D*)NULL, Filter2D::destroy);
    if (filter == NULL) {
        // If no filter was provided, punt and just linearly interpolate.
        filterptr.reset (Filter2D::create ("lanczos3", 6.0f, 6.0f));
        filter = filterptr.get();
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "warp", warp_,
                          dst.spec().format, src.spec().format,
                          dst, src, M, filter, wrap, dst_roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::warp (ImageBuf &dst, const ImageBuf &src,
                    const Imath::M33f &M,
                    string_view filtername_, float filterwidth,
                    bool recompute_roi, ImageBuf::WrapMode wrap,
                    ROI roi, int nthreads)
{
    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filter ((Filter2D*)NULL, Filter2D::destroy);
    std::string filtername = filtername_.size() ? filtername_ : "lanczos3";
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc fd;
        Filter2D::get_filterdesc (i, &fd);
        if (fd.name == filtername) {
            float w = filterwidth > 0.0f ? filterwidth : fd.width;
            float h = filterwidth > 0.0f ? filterwidth : fd.width;
            filter.reset (Filter2D::create (filtername, w, h));
            break;
        }
    }
    if (! filter) {
        dst.error ("Filter \"%s\" not recognized", filtername);
        return false;
    }

    return warp (dst, src, M, filter.get(), recompute_roi, wrap, roi, nthreads);
}



bool
ImageBufAlgo::rotate (ImageBuf &dst, const ImageBuf &src,
                      float angle, float center_x, float center_y,
                      Filter2D *filter, bool recompute_roi,
                      ROI roi, int nthreads)
{
    // Calculate the rotation matrix
    Imath::M33f M;
    M.translate(Imath::V2f(-center_x, -center_y));
    M.rotate(angle);
    M *= Imath::M33f().translate(Imath::V2f(center_x, center_y));
    return ImageBufAlgo::warp (dst, src, M, filter,
                               recompute_roi, ImageBuf::WrapBlack,
                               roi, nthreads);
}



bool
ImageBufAlgo::rotate (ImageBuf &dst, const ImageBuf &src,
                      float angle, float center_x, float center_y,
                      string_view filtername, float filterwidth,
                      bool recompute_roi, ROI roi, int nthreads)
{
    // Calculate the rotation matrix
    Imath::M33f M;
    M.translate(Imath::V2f(-center_x, -center_y));
    M.rotate(angle);
    M *= Imath::M33f().translate(Imath::V2f(center_x, center_y));
    return ImageBufAlgo::warp (dst, src, M, filtername, filterwidth,
                               recompute_roi, ImageBuf::WrapBlack,
                               roi, nthreads);
}



bool
ImageBufAlgo::rotate (ImageBuf &dst, const ImageBuf &src, float angle,
                      Filter2D *filter,
                      bool recompute_roi, ROI roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    float center_x = 0.5f * (src_roi_full.xbegin + src_roi_full.xend);
    float center_y = 0.5f * (src_roi_full.ybegin + src_roi_full.yend);
    return ImageBufAlgo::rotate (dst, src, angle, center_x, center_y,
                                 filter, recompute_roi, roi, nthreads);
}



bool
ImageBufAlgo::rotate (ImageBuf &dst, const ImageBuf &src, float angle,
                      string_view filtername, float filterwidth,
                      bool recompute_roi,
                      ROI roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    float center_x = 0.5f * (src_roi_full.xbegin + src_roi_full.xend);
    float center_y = 0.5f * (src_roi_full.ybegin + src_roi_full.yend);
    return ImageBufAlgo::rotate (dst, src, angle, center_x, center_y,
                                 filtername, filterwidth, recompute_roi,
                                 roi, nthreads);
}


OIIO_NAMESPACE_END
