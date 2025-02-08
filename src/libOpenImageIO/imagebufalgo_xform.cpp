// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// ImageBufAlgo functions for filtered transformations


#include <cmath>
#include <memory>

#include <OpenImageIO/Imath.h>

#include "imageio_pvt.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/thread.h>

#include <Imath/ImathBox.h>

OIIO_NAMESPACE_BEGIN


namespace {

static const ustring edgeclamp_us("edgeclamp");
static const ustring exact_us("exact");
static const ustring fillmode_us("fillmode");
static const ustring filtername_us("filtername");
static const ustring filterptr_us("filterptr");
static const ustring filterwidth_us("filterwidth");
static const ustring recompute_roi_us("recompute_roi");
static const ustring wrap_us("wrap");


#if 0
// I'm not sure if I want to keep this and/or make it public. Just let it
// sit here for a while.

/// Helper class that is used for passing filtering options to IBA functions.
/// A `FilterOpt` may specify filtering options one of several ways:
///
/// * Default constructor / empty FilterOpt: The IBA function will pick a
///   reasonable default filter.
/// * Filter name and width: Use the named filter with the given width.
/// * Filter name only: Use the named filter with that filter's default width.
/// * shared_ptr<Filter2D>: Use the filter object directly with shared
///   ownership.
/// * Raw `const Filter2D*` pointer: Use the filter object directly, and
///   it is owned by the caller (i.e., nothing in FilterOpt nor the IBA
///   function will free it).
///
class FilterOpt {
public:
    using shared_filter = std::shared_ptr<const Filter2D>;

    std::string name;    ///< Name of the filter to use.
    float width = 0.0f;  ///< Width of the filter (0 = default width).

    /// Default constructor
    FilterOpt() = default;

    /// Copy constructor
    FilterOpt(const FilterOpt& f) = default;

    /// Construct from a filter name and optional width. If the width is not
    /// supplied, the default width for that filter will be used.
    FilterOpt(string_view name, float width = 0.0f)
        : name(name), width(width) {}

    /// Construct from a shared_ptr to a Filter2D object.
    FilterOpt(shared_filter& filter)
        : m_filter(filter) {}

    /// Construct from a pointer to a caller-owned Filter2D.
    FilterOpt(const Filter2D* filter)
        : m_filter(shared_filter(filter, [](const Filter2D*){ })) {}
    // Note: this works by constructing a shared_ptr with a custom deleter
    // that does nothing.

    /// Retrieve a shared_ptr holding the Filter2D.
    const shared_filter& filter() const { return m_filter; }
    /// Retrieve a raw pointer to the Filter2D, or nullptr if none has been
    /// assigned.
    const Filter2D* filterptr() const { return m_filter.get(); }

    FilterOpt& operator= (const FilterOpt& f) = default;

    // Helper: Fill in the additional fields: If name and width were
    // specified, set up the actual Filter2D; if the Filter2D was supplied,
    // make sure the name and width are set. If none were set, use the
    // defaults passed (or, if none is passed, pick a reasonable default).
    // Return true for success, false for error (such as unknown filter name).
    OIIO_API bool resolve(string_view default_filtername = "",
                          float default_width = 0.0f);

private:
    shared_filter m_filter;  ///< Shared ptr to Filter2D
};



bool
FilterOpt::resolve(string_view default_filtername, float default_width)
{
    if (m_filter) {
        // If the filter was specified, make sure the name+width match.
        name  = m_filter->name();
        width = m_filter->width();
    } else {
        // No filter was specfied. Get one.
        if (name.empty()) {
            // If no name was specified, use the default filtername as passed,
            // or blackman-harris is a good all-purpose guess.
            if (default_filtername.empty())
                name = "blackman-harris";
            else
                name = default_filtername;
            width = default_width;
        }
        // Look for a matching filter name. Use its preferred width if none
        // was specified by the user.
        for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
            const FilterDesc& fd(Filter2D::get_filterdesc(i));
            if (fd.name == name) {
                float w  = width > 0.0f ? width : fd.width;
                m_filter = Filter2D::create_shared(name, w, w);
                break;
            }
        }
    }
    return filterptr() ? true : false;
}
#endif


// Define a templated Accumulator type that's float, except in the case
// of double input, in which case it's double.
template<typename T> struct Accum_t {
    typedef float type;
};
template<> struct Accum_t<double> {
    typedef double type;
};



// Poor man's Dual2<float> makes it easy to compute with differentials. For
// a rich man's implementation and full documentation, see
// OpenShadingLanguage (dual2.h).
class Dual2 {
public:
    float val() const { return m_val; }
    float dx() const { return m_dx; }
    float dy() const { return m_dy; }
    Dual2(float val)
        : m_val(val)
        , m_dx(0.0f)
        , m_dy(0.0f)
    {
    }
    Dual2(float val, float dx, float dy)
        : m_val(val)
        , m_dx(dx)
        , m_dy(dy)
    {
    }
    Dual2& operator=(float f)
    {
        m_val = f;
        m_dx  = 0.0f;
        m_dy  = 0.0f;
        return *this;
    }
    friend Dual2 operator+(const Dual2& a, const Dual2& b)
    {
        return Dual2(a.m_val + b.m_val, a.m_dx + b.m_dx, a.m_dy + b.m_dy);
    }
    friend Dual2 operator+(const Dual2& a, float b)
    {
        return Dual2(a.m_val + b, a.m_dx, a.m_dy);
    }
    friend Dual2 operator*(const Dual2& a, float b)
    {
        return Dual2(a.m_val * b, a.m_dx * b, a.m_dy * b);
    }
    friend Dual2 operator*(const Dual2& a, const Dual2& b)
    {
        // Use the chain rule
        return Dual2(a.m_val * b.m_val, a.m_val * b.m_dx + a.m_dx * b.m_val,
                     a.m_val * b.m_dy + a.m_dy * b.m_val);
    }
    friend Dual2 operator/(const Dual2& a, const Dual2& b)
    {
        float bvalinv   = 1.0f / b.m_val;
        float aval_bval = a.m_val * bvalinv;
        return Dual2(aval_bval, bvalinv * (a.m_dx - aval_bval * b.m_dx),
                     bvalinv * (a.m_dy - aval_bval * b.m_dy));
    }

private:
    float m_val, m_dx, m_dy;
};

/// Transform a 2D point (x,y) with derivatives by a 3x3 affine matrix to
/// obtain a transformed point with derivatives.
inline void
robust_multVecMatrix(const Imath::M33f& M, const Dual2& x, const Dual2& y,
                     Dual2& outx, Dual2& outy)
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
static ROI
transform(const Imath::M33f& M, ROI roi)
{
    Imath::V2f ul(roi.xbegin + 0.5f, roi.ybegin + 0.5f);
    Imath::V2f ur(roi.xend - 0.5f, roi.ybegin + 0.5f);
    Imath::V2f ll(roi.xbegin + 0.5f, roi.yend - 0.5f);
    Imath::V2f lr(roi.xend - 0.5f, roi.yend - 0.5f);
    M.multVecMatrix(ul, ul);
    M.multVecMatrix(ur, ur);
    M.multVecMatrix(ll, ll);
    M.multVecMatrix(lr, lr);
    Imath::Box2f box(ul);
    box.extendBy(ll);
    box.extendBy(ur);
    box.extendBy(lr);
    int xmin = int(floorf(box.min.x));
    int ymin = int(floorf(box.min.y));
    int xmax = int(floorf(box.max.x)) + 1;
    int ymax = int(floorf(box.max.y)) + 1;
    return ROI(xmin, xmax, ymin, ymax, roi.zbegin, roi.zend, roi.chbegin,
               roi.chend);
}



// Given s,t image space coordinates and their derivatives, compute a
// filtered sample using the derivatives to guide the size of the filter
// footprint.
template<typename SRCTYPE>
inline void
filtered_sample(const ImageBuf& src, float s, float t, float dsdx, float dtdx,
                float dsdy, float dtdy, const Filter2D* filter,
                ImageBuf::WrapMode wrap, bool edgeclamp, float* result)
{
    OIIO_DASSERT(filter);
    // Just use isotropic filtering
    float ds          = std::max(1.0f, std::max(fabsf(dsdx), fabsf(dsdy)));
    float dt          = std::max(1.0f, std::max(fabsf(dtdx), fabsf(dtdy)));
    float ds_inv      = 1.0f / ds;
    float dt_inv      = 1.0f / dt;
    float filterrad_s = 0.5f * ds * filter->width();
    float filterrad_t = 0.5f * dt * filter->width();
    int smin          = (int)floorf(s - filterrad_s);
    int smax          = (int)ceilf(s + filterrad_s);
    int tmin          = (int)floorf(t - filterrad_t);
    int tmax          = (int)ceilf(t + filterrad_t);
    if (edgeclamp) {
        // Special case for black wrap mode -- clamp the filter shape so we
        // don't even look outside the image region. This prevents strange
        // image edge artifacts when using filters with negative lobes,
        // where the image boundary itself is a contrast edge that can
        // produce ringing. In theory, we probably only need to do this for
        // filters with negative lobes, but there isn't an easy way to know
        // at this point whether that's true of this passed-in filter.
        smin = clamp(smin, src.xbegin(), src.xend());
        smax = clamp(smax, src.xbegin(), src.xend());
        tmin = clamp(tmin, src.ybegin(), src.yend());
        tmax = clamp(tmax, src.ybegin(), src.yend());
        // wrap = ImageBuf::WrapClamp;
        if (s < src.xbegin() - 1 || s >= src.xend() || t < src.ybegin() - 1
            || t >= src.yend()) {
            // Also, when edgeclamp is true, to further reduce ringing that
            // shows up outside the image boundary, always be black when
            // sampling more than one pixel from the source edge.
            for (int c = 0, nc = src.nchannels(); c < nc; ++c)
                result[c] = 0.0f;
            return;
        }
    }
    ImageBuf::ConstIterator<SRCTYPE> samp(src, smin, smax, tmin, tmax, 0, 1,
                                          wrap);
    int nc     = src.nchannels();
    float* sum = OIIO_ALLOCA(float, nc);
    memset(sum, 0, nc * sizeof(float));
    float total_w = 0.0f;
    for (; !samp.done(); ++samp) {
        float w = (*filter)(ds_inv * (samp.x() + 0.5f - s),
                            dt_inv * (samp.y() + 0.5f - t));
        for (int c = 0; c < nc; ++c)
            sum[c] += w * samp[c];
        total_w += w;
    }
    if (total_w > 0.0f)
        for (int c = 0; c < nc; ++c)
            result[c] = sum[c] / total_w;
    else
        for (int c = 0; c < nc; ++c)
            result[c] = 0.0f;
}

}  // namespace



static std::shared_ptr<const Filter2D>
get_warp_filter(string_view filtername_, float filterwidth, ImageBuf& dst)
{
    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    Filter2D::ref filter((Filter2D*)nullptr, Filter2D::destroy);
    std::string filtername = filtername_.size() ? filtername_ : "lanczos3";
    for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
        FilterDesc fd;
        Filter2D::get_filterdesc(i, &fd);
        if (fd.name == filtername) {
            float w = filterwidth > 0.0f ? filterwidth : fd.width;
            filter.reset(Filter2D::create(filtername, w, w));
            break;
        }
    }
    if (!filter) {
        dst.errorfmt("Filter \"{}\" not recognized", filtername);
    }
    return filter;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
warp_(ImageBuf& dst, const ImageBuf& src, const Imath::M33f& M,
      const Filter2D* filter, ImageBuf::WrapMode wrap, bool edgeclamp, ROI roi,
      int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int nc     = dst.nchannels();
        float* pel = OIIO_ALLOCA(float, nc);
        memset(pel, 0, nc * sizeof(float));
        Imath::M33f Minv = M.inverse();
        ImageBuf::Iterator<DSTTYPE> out(dst, roi);
        for (; !out.done(); ++out) {
            Dual2 x(out.x() + 0.5f, 1.0f, 0.0f);
            Dual2 y(out.y() + 0.5f, 0.0f, 1.0f);
            robust_multVecMatrix(Minv, x, y, x, y);
            filtered_sample<SRCTYPE>(src, x.val(), y.val(), x.dx(), y.dx(),
                                     x.dy(), y.dy(), filter, wrap, edgeclamp,
                                     pel);
            for (int c = roi.chbegin; c < roi.chend; ++c)
                out[c] = pel[c];
        }
    });
    return true;
}



static bool
warp_impl(ImageBuf& dst, const ImageBuf& src, const Imath::M33f& M,
          const Filter2D* filter, bool recompute_roi, ImageBuf::WrapMode wrap,
          bool edgeclamp, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::warp");
    ROI src_roi_full = src.roi_full();
    ROI dst_roi, dst_roi_full;
    if (dst.initialized()) {
        dst_roi      = roi.defined() ? roi : dst.roi();
        dst_roi_full = dst.roi_full();
    } else {
        dst_roi      = roi.defined()
                           ? roi
                           : (recompute_roi ? transform(M, src.roi()) : src.roi());
        dst_roi_full = src_roi_full;
    }
    dst_roi.chend      = std::min(dst_roi.chend, src.nchannels());
    dst_roi_full.chend = std::min(dst_roi_full.chend, src.nchannels());

    if (!IBAprep(dst_roi, &dst, &src, ImageBufAlgo::IBAprep_NO_SUPPORT_VOLUME))
        return false;

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filterptr((Filter2D*)NULL, Filter2D::destroy);
    if (filter == NULL) {
        // If no filter was provided, punt and use lanczos3
        filterptr.reset(Filter2D::create("lanczos3", 6.0f, 6.0f));
        filter = filterptr.get();
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "warp", warp_, dst.spec().format,
                                src.spec().format, dst, src, M, filter, wrap,
                                edgeclamp, dst_roi, nthreads);
    return ok;
}



// Is the PV `option` in the `recognized` list? If so, return true.
// Is it in the `obsolete` list? If so, return false.
// Is it in neither? Return false.
static bool
IBA_find_optional(const ParamValue& option, cspan<ustring> recognized,
                  cspan<ustring> obsolete = {})
{
    for (auto&& r : recognized)
        if (option.name() == r)
            return true;
    for (auto&& o : obsolete)
        if (option.name() == o)
            return false;
    return false;
}



// Given list of `options`, check if any are unrecognized or obsolete.
// Return true if all are fine, false if not.
static bool
IBA_check_optional(ImageBufAlgo::KWArgs options, cspan<ustring> recognized,
                   cspan<ustring> obsolete = {})
{
    bool ok = true;
    for (auto&& pv : options) {
        ok &= IBA_find_optional(pv, recognized, obsolete);
    }
    return ok;
}



// Extract filterptr from the options of it exists
inline Filter2D::ref
get_filterptr_option(ImageBufAlgo::KWArgs options)
{
    Filter2D::ref filterptr;
    auto f = options.find(filterptr_us, TypePointer);
    if (f != options.end())
        filterptr = Filter2D::ref(f->get<const Filter2D*>(),
                                  Filter2D::no_destroy);
    return filterptr;
}



bool
ImageBufAlgo::warp(ImageBuf& dst, const ImageBuf& src, M33fParam M,
                   KWArgs options, ROI roi, int nthreads)
{
    static const ustring recognized[] = { filtername_us,    filterwidth_us,
                                          wrap_us,          edgeclamp_us,
                                          recompute_roi_us, filterptr_us };
    IBA_check_optional(options, recognized);

    Filter2D::ref filterptr = get_filterptr_option(options);
    if (!filterptr) {
        filterptr = get_warp_filter(options.get_string(filtername_us),
                                    options.get_float(filterwidth_us), dst);
        if (!filterptr)
            return false;  // error issued in get_warp_filter
    }
    if (!filterptr) {
        dst.errorfmt("Invalid filter");
        return false;
    }

    ImageBuf::WrapMode wrap = ImageBuf::WrapMode::WrapDefault;
    auto wrapparam          = options.find(wrap_us);
    if (wrapparam != options.end()) {
        if (wrapparam->type() == TypeString)
            wrap = ImageBuf::WrapMode_from_string(wrapparam->get_ustring());
        else
            wrap = (ImageBuf::WrapMode)wrapparam->get_int();
    }
    bool recompute_roi = options.get_int(recompute_roi_us, 0);
    bool edgeclamp     = options.get_int(edgeclamp_us, 0);

    return warp_impl(dst, src, M, filterptr.get(), recompute_roi, wrap,
                     edgeclamp, roi, nthreads);
}



ImageBuf
ImageBufAlgo::warp(const ImageBuf& src, M33fParam M, KWArgs options, ROI roi,
                   int nthreads)
{
    ImageBuf result;
    bool ok = warp(result, src, M, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::warp() error");
    return result;
}



bool
ImageBufAlgo::warp(ImageBuf& dst, const ImageBuf& src, M33fParam M,
                   const Filter2D* filter, bool recompute_roi,
                   ImageBuf::WrapMode wrap, ROI roi, int nthreads)
{
    return warp(dst, src, M,
                { make_pv(filterptr_us, filter),
                  { recompute_roi_us, int(recompute_roi) },
                  { wrap_us, int(wrap) } },
                roi, nthreads);
}



bool
ImageBufAlgo::warp(ImageBuf& dst, const ImageBuf& src, M33fParam M,
                   string_view filtername, float filterwidth,
                   bool recompute_roi, ImageBuf::WrapMode wrap, ROI roi,
                   int nthreads)
{
    return warp(dst, src, M,
                { { filtername_us, filtername },
                  { filterwidth_us, filterwidth },
                  { recompute_roi_us, int(recompute_roi) },
                  { wrap_us, int(wrap) } },
                roi, nthreads);
}



ImageBuf
ImageBufAlgo::warp(const ImageBuf& src, M33fParam M, const Filter2D* filter,
                   bool recompute_roi, ImageBuf::WrapMode wrap, ROI roi,
                   int nthreads)
{
    ImageBuf result;
    bool ok = warp(result, src, M, filter, recompute_roi, wrap, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::warp() error");
    return result;
}



ImageBuf
ImageBufAlgo::warp(const ImageBuf& src, M33fParam M, string_view filtername,
                   float filterwidth, bool recompute_roi,
                   ImageBuf::WrapMode wrap, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = warp(result, src, M, filtername, filterwidth, recompute_roi, wrap,
                   roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::warp() error");
    return result;
}



bool
ImageBufAlgo::rotate(ImageBuf& dst, const ImageBuf& src, float angle,
                     float center_x, float center_y, Filter2D* filter,
                     bool recompute_roi, ROI roi, int nthreads)
{
    // Calculate the rotation matrix
    Imath::M33f M;
    M.translate(Imath::V2f(-center_x, -center_y));
    M.rotate(angle);
    M *= Imath::M33f().translate(Imath::V2f(center_x, center_y));
    return ImageBufAlgo::warp(dst, src, M, filter, recompute_roi,
                              ImageBuf::WrapBlack, roi, nthreads);
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
resize_(ImageBuf& dst, const ImageBuf& src, const Filter2D* filter, ROI roi,
        int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& srcspec(src.spec());
        const ImageSpec& dstspec(dst.spec());
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
        float xratio = float(dstspec.full_width)
                       / srcfw;  // 2 upsize, 0.5 downsize
        float yratio = float(dstspec.full_height) / srcfh;

        float dstfx          = float(dstspec.full_x);
        float dstfy          = float(dstspec.full_y);
        float dstfw          = float(dstspec.full_width);
        float dstfh          = float(dstspec.full_height);
        float dstpixelwidth  = 1.0f / dstfw;
        float dstpixelheight = 1.0f / dstfh;
        float filterrad      = filter->width() / 2.0f;

        // radi,radj is the filter radius, as an integer, in source pixels.  We
        // will filter the source over [x-radi, x+radi] X [y-radj,y+radj].
        int radi        = (int)ceilf(filterrad / xratio);
        int radj        = (int)ceilf(filterrad / yratio);
        int xtaps       = 2 * radi + 1;
        int ytaps       = 2 * radj + 1;
        bool separable  = filter->separable();
        float* yfiltval = OIIO_ALLOCA(float, ytaps);
        std::unique_ptr<float[]> xfiltval_all;
        if (separable) {
            // For separable filters, horizontal tap weights will be the same
            // for every column. So we precompute all the tap weights for every
            // x position we'll need. We do the same thing in y, but row by row
            // inside the loop (since we never revisit a y row). This
            // substantially speeds up resize.
            xfiltval_all.reset(new float[xtaps * roi.width()]);
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                float* xfiltval = xfiltval_all.get() + (x - roi.xbegin) * xtaps;
                float s         = (x - dstfx + 0.5f) * dstpixelwidth;
                float src_xf    = srcfx + s * srcfw;
                int src_x;
                float src_xf_frac   = floorfrac(src_xf, &src_x);
                float totalweight_x = 0.0f;
                for (int i = 0; i < xtaps; ++i) {
                    float w = filter->xfilt(
                        xratio * (i - radi - (src_xf_frac - 0.5f)));
                    xfiltval[i] = w;
                    totalweight_x += w;
                }
                if (totalweight_x != 0.0f)
                    for (int i = 0; i < xtaps; ++i)    // normalize x filter
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

        // Accumulate the weighted results in pel[]. We select a type big
        // enough to hold with required precision.
        typedef typename Accum_t<DSTTYPE>::type Acc_t;
        Acc_t* pel = OIIO_ALLOCA(Acc_t, nchannels);

#define USE_SPECIAL 0
#if USE_SPECIAL
        // Special case: src and dst are local memory, float buffers, and we're
        // operating on all channels, <= 4.
        bool special
            = ((std::is_same<DSTTYPE, float>::value
                || std::is_same<DSTTYPE, half>::value)
               && (std::is_same<SRCTYPE, float>::value
                   || std::is_same<SRCTYPE, half>::value)
               // && dst.localpixels() // has to be, because it's writable
               && src.localpixels()
               // && R.contains_roi(roi)  // has to be, because IBAPrep
               && src.contains_roi(roi) && roi.chbegin == 0
               && roi.chend == dst.nchannels() && roi.chend == src.nchannels()
               && roi.chend <= 4 && separable);
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
            ImageBuf::Iterator<DSTTYPE> out(dst, roi);
            ImageBuf::ConstIterator<SRCTYPE> srcpel(src, ImageBuf::WrapClamp);
            for (int y = roi.ybegin; y < roi.yend; ++y) {
                float t      = (y - dstfy + 0.5f) * dstpixelheight;
                float src_yf = srcfy + t * srcfh;
                int src_y;
                float src_yf_frac = floorfrac(src_yf, &src_y);
                // If using separable filters, our vertical set of filter tap
                // weights will be the same for the whole scanline we're on.  Just
                // compute and normalize them once.
                float totalweight_y = 0.0f;
                for (int j = 0; j < ytaps; ++j) {
                    float w = filter->yfilt(
                        yratio * (j - radj - (src_yf_frac - 0.5f)));
                    yfiltval[j] = w;
                    totalweight_y += w;
                }
                if (totalweight_y != 0.0f)
                    for (int i = 0; i < ytaps; ++i)
                        yfiltval[i] /= totalweight_y;

                for (int x = roi.xbegin; x < roi.xend; ++x, ++out) {
                    float s      = (x - dstfx + 0.5f) * dstpixelwidth;
                    float src_xf = srcfx + s * srcfw;
                    int src_x    = ifloor(src_xf);
                    for (int c = 0; c < nchannels; ++c)
                        pel[c] = 0.0f;
                    const float* xfiltval = xfiltval_all.get()
                                            + (x - roi.xbegin) * xtaps;
                    float totalweight_x = 0.0f;
                    for (int i = 0; i < xtaps; ++i)
                        totalweight_x += xfiltval[i];
                    if (totalweight_x != 0.0f) {
                        srcpel.rerange(src_x - radi, src_x + radi + 1,
                                       src_y - radj, src_y + radj + 1, 0, 1,
                                       ImageBuf::WrapClamp);
                        for (int j = -radj; j <= radj; ++j) {
                            float wy = yfiltval[j + radj];
                            if (wy == 0.0f) {
                                // 0 weight for this y tap -- move to next line
                                srcpel.pos(srcpel.x(), srcpel.y() + 1,
                                           srcpel.z());
                                continue;
                            }
                            for (int i = 0; i < xtaps; ++i, ++srcpel) {
                                float w = wy * xfiltval[i];
                                if (w)
                                    for (int c = 0; c < nchannels; ++c)
                                        pel[c] += w * srcpel[c];
                            }
                        }
                    }
                    // Copy the pixel value (already normalized) to the output.
                    OIIO_DASSERT(out.x() == x && out.y() == y);
                    if (totalweight_y == 0.0f) {
                        // zero it out
                        for (int c = 0; c < nchannels; ++c)
                            out[c] = 0.0f;
                    } else {
                        for (int c = 0; c < nchannels; ++c)
                            out[c] = pel[c];
                    }
                }
            }

        } else {
            // Non-separable filter
            ImageBuf::Iterator<DSTTYPE> out(dst, roi);
            ImageBuf::ConstIterator<SRCTYPE> srcpel(src, ImageBuf::WrapClamp);
            for (int y = roi.ybegin; y < roi.yend; ++y) {
                float t      = (y - dstfy + 0.5f) * dstpixelheight;
                float src_yf = srcfy + t * srcfh;
                int src_y;
                float src_yf_frac = floorfrac(src_yf, &src_y);
                for (int x = roi.xbegin; x < roi.xend; ++x, ++out) {
                    float s      = (x - dstfx + 0.5f) * dstpixelwidth;
                    float src_xf = srcfx + s * srcfw;
                    int src_x;
                    float src_xf_frac = floorfrac(src_xf, &src_x);
                    for (int c = 0; c < nchannels; ++c)
                        pel[c] = 0.0f;
                    float totalweight = 0.0f;
                    srcpel.rerange(src_x - radi, src_x + radi + 1, src_y - radi,
                                   src_y + radi + 1, 0, 1, ImageBuf::WrapClamp);
                    for (int j = -radj; j <= radj; ++j) {
                        for (int i = -radi; i <= radi; ++i, ++srcpel) {
                            OIIO_DASSERT(!srcpel.done());
                            float w
                                = (*filter)(xratio * (i - (src_xf_frac - 0.5f)),
                                            yratio
                                                * (j - (src_yf_frac - 0.5f)));
                            if (w) {
                                totalweight += w;
                                for (int c = 0; c < nchannels; ++c)
                                    pel[c] += w * srcpel[c];
                            }
                        }
                    }
                    OIIO_DASSERT(srcpel.done());
                    // Rescale pel to normalize the filter and write it to the
                    // output image.
                    OIIO_DASSERT(out.x() == x && out.y() == y);
                    if (totalweight == 0.0f) {
                        // zero it out
                        for (int c = 0; c < nchannels; ++c)
                            out[c] = 0.0f;
                    } else {
                        for (int c = 0; c < nchannels; ++c)
                            out[c] = pel[c] / totalweight;
                    }
                }
            }
        }
    });  // end of parallel_image
    return true;
}



static std::shared_ptr<Filter2D>
get_resize_filter(string_view filtername, float fwidth, ImageBuf& dst,
                  float wratio, float hratio)
{
    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filter;
    if (filtername.empty()) {
        // No filter name supplied -- pick a good default
        if (wratio > 1.0f || hratio > 1.0f)
            filtername = "blackman-harris";
        else
            filtername = "lanczos3";
    }
    for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
        FilterDesc fd;
        Filter2D::get_filterdesc(i, &fd);
        if (fd.name == filtername) {
            float w = fwidth > 0.0f ? fwidth
                                    : fd.width * std::max(1.0f, wratio);
            float h = fwidth > 0.0f ? fwidth
                                    : fd.width * std::max(1.0f, hratio);
            filter.reset(Filter2D::create(filtername, w, h));
            break;
        }
    }
    if (!filter) {
        dst.errorfmt("Filter \"{}\" not recognized", filtername);
    }
    return filter;
}



bool
ImageBufAlgo::resize(ImageBuf& dst, const ImageBuf& src, KWArgs options,
                     ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::resize");

    static const ustring recognized[] = {
        filtername_us,
        filterwidth_us,
        filterptr_us,
#if 0 /* Not currently recognized */
        wrap_us,
        edgeclamp_us,
        recompute_roi_us,
#endif
    };
    IBA_check_optional(options, recognized);

    if (!IBAprep(roi, &dst, &src,
                 IBAprep_NO_SUPPORT_VOLUME | IBAprep_NO_COPY_ROI_FULL))
        return false;
    const ImageSpec& srcspec(src.spec());
    const ImageSpec& dstspec(dst.spec());

    Filter2D::ref filterptr = get_filterptr_option(options);
    if (!filterptr) {
        // Resize ratios
        float wratio = float(dstspec.full_width) / float(srcspec.full_width);
        float hratio = float(dstspec.full_height) / float(srcspec.full_height);
        filterptr    = get_resize_filter(options.get_string(filtername_us),
                                         options.get_float(filterwidth_us), dst,
                                         wratio, hratio);
        if (!filterptr)
            return false;  // error issued in get_resize_filter
    }

#if 0 /* These aren't currently reconized */
    ImageBuf::WrapMode wrap = ImageBuf::WrapMode::WrapDefault;
    auto wrapparam          = options.find(wrap_us);
    if (wrapparam != options.end()) {
        if (wrapparam->type() == TypeString)
            wrap = ImageBuf::WrapMode_from_string(wrapparam->get_ustring());
        else
            wrap = (ImageBuf::WrapMode)wrapparam->get_int();
    }
    bool recompute_roi = options.get_int(recompute_roi_us, 0);
    bool edgeclamp     = options.get_int(edgeclamp_us, 0);
#endif

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "resize", resize_, dst.spec().format,
                                src.spec().format, dst, src, filterptr.get(),
                                roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::resize(const ImageBuf& src, KWArgs options, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = resize(result, src, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::resize() error");
    return result;
}


bool
ImageBufAlgo::fit(ImageBuf& dst, const ImageBuf& src, KWArgs options, ROI roi,
                  int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fit");

    static const ustring recognized[] = {
        filtername_us,
        filterwidth_us,
        filterptr_us,
        fillmode_us,
        exact_us,
#if 0 /* Not currently recognized */
        wrap_us,
        edgeclamp_us,
#endif
    };
    IBA_check_optional(options, recognized);
    // No time logging, it will be accounted in the underlying warp/resize
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_NO_SUPPORT_VOLUME | IBAprep_NO_COPY_ROI_FULL))
        return false;

    string_view fillmode = options.get_string(fillmode_us, "letterbox");
    int exact            = options.get_int(exact_us);

    const ImageSpec& srcspec(src.spec());

    // Compute scaling factors and use action_resize to do the heavy lifting
    int fit_full_width     = roi.width();
    int fit_full_height    = roi.height();
    int fit_full_x         = roi.xbegin;
    int fit_full_y         = roi.ybegin;
    float oldaspect        = float(srcspec.full_width) / srcspec.full_height;
    float newaspect        = float(fit_full_width) / fit_full_height;
    int resize_full_width  = fit_full_width;
    int resize_full_height = fit_full_height;
    int xoffset = 0, yoffset = 0;
    float xoff = 0.0f, yoff = 0.0f;
    float scale = 1.0f;

    if (fillmode != "height" && fillmode != "width") {
        // Unknown fill mode: default to "letterbox"
        fillmode = "letterbox";
    }
    if (fillmode == "letterbox") {
        if (newaspect >= oldaspect) {
            // same or wider than original, fill to height
            fillmode = "height";
        } else {
            // narrower than original, so fill to width
            fillmode = "width";
        }
    }
    if (fillmode == "height") {
        resize_full_width = int(resize_full_height * oldaspect + 0.5f);
        xoffset           = (fit_full_width - resize_full_width) / 2;
        scale             = float(fit_full_height) / float(srcspec.full_height);
        xoff = float(fit_full_width - scale * srcspec.full_width) / 2.0f;
    } else if (fillmode == "width") {
        resize_full_height = int(resize_full_width / oldaspect + 0.5f);
        yoffset            = (fit_full_height - resize_full_height) / 2;
        scale              = float(fit_full_width) / float(srcspec.full_width);
        yoff = float(fit_full_height - scale * srcspec.full_height) / 2.0f;
    }

    ROI newroi(fit_full_x, fit_full_x + fit_full_width, fit_full_y,
               fit_full_y + fit_full_height, 0, 1, 0, srcspec.nchannels);
    // std::cout << "  Fitting " << srcspec.roi()
    //           << " into " << newroi << "\n";
    // std::cout << "  Fit scale factor " << scale << "\n";

    Filter2D::ref filterptr = get_filterptr_option(options);
    if (!filterptr) {
        // If no filter was provided, punt and just linearly interpolate.
        float wratio = float(resize_full_width) / float(srcspec.full_width);
        float hratio = float(resize_full_height) / float(srcspec.full_height);
        filterptr    = get_resize_filter(options.get_string(filtername_us),
                                         options.get_float(filterwidth_us), dst,
                                         wratio, hratio);
        if (!filterptr)
            return false;  // error issued in get_resize_filter
    }

    bool ok = true;
    if (exact) {
        // Full partial-pixel filtered resize -- exactly preserves aspect
        // ratio and exactly centers the padded image, but might make the
        // edges of the resized area blurry because it's not a whole number
        // of pixels.
        Imath::M33f M(scale, 0.0f, 0.0f, 0.0f, scale, 0.0f, xoff, yoff, 1.0f);
        // std::cout << "   Fit performing warp with " << M << "\n";
        ImageSpec newspec = srcspec;
        newspec.set_roi(newroi);
        newspec.set_roi_full(newroi);
        dst.reset(newspec);
        ImageBuf::WrapMode wrap = ImageBuf::WrapMode_from_string("black");
        ok &= warp_impl(dst, src, M, filterptr.get(), /*recompute_roi*/ false,
                        wrap, /*edgeclamp*/ true, /*roi*/ {}, nthreads);
    } else {
        // Full pixel resize -- gives the sharpest result, but for odd-sized
        // destination resolution, may not be exactly centered and will only
        // preserve the aspect ratio to the nearest integer pixel size.
        if (resize_full_width != srcspec.full_width
            || resize_full_height != srcspec.full_height
            || fit_full_x != srcspec.full_x || fit_full_y != srcspec.full_y) {
            ROI resizeroi(fit_full_x, fit_full_x + resize_full_width,
                          fit_full_y, fit_full_y + resize_full_height, 0, 1, 0,
                          srcspec.nchannels);
            ImageSpec newspec = srcspec;
            newspec.set_roi(resizeroi);
            newspec.set_roi_full(resizeroi);
            dst.reset(newspec);
            logtime.stop();  // it will be picked up again by the next call...
            const Filter2D* filterraw = filterptr.get();
            ok &= ImageBufAlgo::resize(dst, src,
                                       { make_pv(filterptr_us, filterraw) },
                                       resizeroi, nthreads);
        } else {
            ok &= dst.copy(src);  // no resize is necessary
        }
        dst.specmod().full_width  = fit_full_width;
        dst.specmod().full_height = fit_full_height;
        dst.specmod().full_x      = fit_full_x;
        dst.specmod().full_y      = fit_full_y;
        dst.specmod().x           = xoffset;
        dst.specmod().y           = yoffset;
    }
    return ok;
}



ImageBuf
ImageBufAlgo::fit(const ImageBuf& src, KWArgs options, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fit(result, src, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::fit() error");
    return result;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
resample_(ImageBuf& dst, const ImageBuf& src, bool interpolate, ROI roi,
          int nthreads)
{
    OIIO_ASSERT(src.deep() == dst.deep());
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& srcspec(src.spec());
        const ImageSpec& dstspec(dst.spec());
        int nchannels = src.nchannels();
        bool deep     = src.deep();

        // Local copies of the source image window, converted to float
        float srcfx = srcspec.full_x;
        float srcfy = srcspec.full_y;
        float srcfw = srcspec.full_width;
        float srcfh = srcspec.full_height;

        float dstfx          = dstspec.full_x;
        float dstfy          = dstspec.full_y;
        float dstfw          = dstspec.full_width;
        float dstfh          = dstspec.full_height;
        float dstpixelwidth  = 1.0f / dstfw;
        float dstpixelheight = 1.0f / dstfh;
        span<float> pel      = OIIO_ALLOCA_SPAN(float, nchannels);

        ImageBuf::Iterator<DSTTYPE> out(dst, roi);
        ImageBuf::ConstIterator<SRCTYPE> srcpel(src);
        for (int y = roi.ybegin; y < roi.yend; ++y) {
            // s,t are NDC space
            float t = (y - dstfy + 0.5f) * dstpixelheight;
            // src_xf, src_xf are image space float coordinates
            float src_yf = srcfy + t * srcfh;
            // src_x, src_y are image space integer coordinates of the floor
            int src_y = ifloor(src_yf);
            for (int x = roi.xbegin; x < roi.xend; ++x, ++out) {
                float s      = (x - dstfx + 0.5f) * dstpixelwidth;
                float src_xf = srcfx + s * srcfw;
                int src_x    = ifloor(src_xf);
                if (deep) {
                    srcpel.pos(src_x, src_y, 0);
                    int nsamps = srcpel.deep_samples();
                    OIIO_DASSERT(nsamps == out.deep_samples());
                    if (!nsamps || nsamps != out.deep_samples())
                        continue;
                    for (int c = 0; c < nchannels; ++c) {
                        if (dstspec.channelformat(c) == TypeDesc::UINT32)
                            for (int samp = 0; samp < nsamps; ++samp)
                                out.set_deep_value(
                                    c, samp, srcpel.deep_value_uint(c, samp));
                        else
                            for (int samp = 0; samp < nsamps; ++samp)
                                out.set_deep_value(c, samp,
                                                   srcpel.deep_value(c, samp));
                    }
                } else if (interpolate) {
                    // Non-deep image, bilinearly interpolate
                    src.interppixel(src_xf, src_yf, pel, ImageBuf::WrapClamp);
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        out[c] = pel[c];
                } else {
                    // Non-deep image, just copy closest pixel
                    srcpel.pos(src_x, src_y, 0);
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        out[c] = srcpel[c];
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::resample(ImageBuf& dst, const ImageBuf& src, bool interpolate,
                       ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::resample");
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_NO_SUPPORT_VOLUME | IBAprep_NO_COPY_ROI_FULL
                     | IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        const ImageSpec& srcspec(src.spec());
        const ImageSpec& dstspec(dst.spec());
        float srcfx          = srcspec.full_x;
        float srcfy          = srcspec.full_y;
        float srcfw          = srcspec.full_width;
        float srcfh          = srcspec.full_height;
        float dstpixelwidth  = 1.0f / dstspec.full_width;
        float dstpixelheight = 1.0f / dstspec.full_height;
        ImageBuf::ConstIterator<float> srcpel(src, roi);
        ImageBuf::Iterator<float> dstpel(dst, roi);
        for (; !dstpel.done(); ++dstpel, ++srcpel) {
            float s   = (dstpel.x() - dstspec.full_x + 0.5f) * dstpixelwidth;
            float t   = (dstpel.y() - dstspec.full_y + 0.5f) * dstpixelheight;
            int src_y = ifloor(srcfy + t * srcfh);
            int src_x = ifloor(srcfx + s * srcfw);
            srcpel.pos(src_x, src_y, 0);
            dstpel.set_deep_samples(srcpel.deep_samples());
        }
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "resample", resample_, dst.spec().format,
                                src.spec().format, dst, src, interpolate, roi,
                                nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::resample(const ImageBuf& src, bool interpolate, ROI roi,
                       int nthreads)
{
    ImageBuf result;
    bool ok = resample(result, src, interpolate, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::resample() error");
    return result;
}



bool
ImageBufAlgo::rotate(ImageBuf& dst, const ImageBuf& src, float angle,
                     float center_x, float center_y, string_view filtername,
                     float filterwidth, bool recompute_roi, ROI roi,
                     int nthreads)
{
    // Calculate the rotation matrix
    Imath::M33f M;
    M.translate(Imath::V2f(-center_x, -center_y));
    M.rotate(angle);
    M *= Imath::M33f().translate(Imath::V2f(center_x, center_y));
    return ImageBufAlgo::warp(dst, src, M,
                              { { "filtername", filtername },
                                { "filterwidth", filterwidth },
                                { "recompute_roi", int(recompute_roi) },
                                { "wrap", "black" } },
                              roi, nthreads);
}



bool
ImageBufAlgo::rotate(ImageBuf& dst, const ImageBuf& src, float angle,
                     Filter2D* filter, bool recompute_roi, ROI roi,
                     int nthreads)
{
    ROI src_roi_full = src.roi_full();
    float center_x   = 0.5f * (src_roi_full.xbegin + src_roi_full.xend);
    float center_y   = 0.5f * (src_roi_full.ybegin + src_roi_full.yend);
    return ImageBufAlgo::rotate(dst, src, angle, center_x, center_y, filter,
                                recompute_roi, roi, nthreads);
}



bool
ImageBufAlgo::rotate(ImageBuf& dst, const ImageBuf& src, float angle,
                     string_view filtername, float filterwidth,
                     bool recompute_roi, ROI roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    float center_x   = 0.5f * (src_roi_full.xbegin + src_roi_full.xend);
    float center_y   = 0.5f * (src_roi_full.ybegin + src_roi_full.yend);
    return ImageBufAlgo::rotate(dst, src, angle, center_x, center_y, filtername,
                                filterwidth, recompute_roi, roi, nthreads);
}



ImageBuf
ImageBufAlgo::rotate(const ImageBuf& src, float angle, float center_x,
                     float center_y, Filter2D* filter, bool recompute_roi,
                     ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate(result, src, angle, center_x, center_y, filter,
                     recompute_roi, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate() error");
    return result;
}



ImageBuf
ImageBufAlgo::rotate(const ImageBuf& src, float angle, float center_x,
                     float center_y, string_view filtername, float filterwidth,
                     bool recompute_roi, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate(result, src, angle, center_x, center_y, filtername,
                     filterwidth, recompute_roi, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate() error");
    return result;
}



ImageBuf
ImageBufAlgo::rotate(const ImageBuf& src, float angle, Filter2D* filter,
                     bool recompute_roi, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate(result, src, angle, filter, recompute_roi, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate() error");
    return result;
}



ImageBuf
ImageBufAlgo::rotate(const ImageBuf& src, float angle, string_view filtername,
                     float filterwidth, bool recompute_roi, ROI roi,
                     int nthreads)
{
    ImageBuf result;
    bool ok = rotate(result, src, angle, filtername, filterwidth, recompute_roi,
                     roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate() error");
    return result;
}



template<typename DSTTYPE, typename SRCTYPE, typename STTYPE>
static bool
st_warp_(ImageBuf& dst, const ImageBuf& src, const ImageBuf& stbuf, int chan_s,
         int chan_t, bool flip_s, bool flip_t, const Filter2D* filter, ROI roi,
         int nthreads)
{
    OIIO_DASSERT(filter);
    OIIO_DASSERT(dst.spec().nchannels >= roi.chend);

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& srcspec(src.spec());
        const ImageSpec& dstspec(dst.spec());
        const int src_width  = srcspec.full_width;
        const int src_height = srcspec.full_height;

        const float xscale = float(dstspec.full_width) / src_width;
        const float yscale = float(dstspec.full_height) / src_height;

        const int xbegin = src.xbegin();
        const int xend   = src.xend();
        const int ybegin = src.ybegin();
        const int yend   = src.yend();

        // The horizontal and vertical filter radii, in source pixels.
        // We will sample and filter the source over
        //   [x-filterrad_x, x+filterrad_x] X [y-filterrad_y,y+filterrad_y].
        const int filterrad_x = (int)ceilf(filter->width() / 2.0f / xscale);
        const int filterrad_y = (int)ceilf(filter->height() / 2.0f / yscale);

        // Accumulation buffer for filter samples, typed to maintain the
        // necessary precision.
        typedef typename Accum_t<DSTTYPE>::type Acc_t;
        const int nchannels = roi.chend - roi.chbegin;
        Acc_t* sample_accum = OIIO_ALLOCA(Acc_t, nchannels);

        ImageBuf::ConstIterator<SRCTYPE, Acc_t> src_iter(src);
        ImageBuf::ConstIterator<STTYPE> st_iter(stbuf, roi);
        ImageBuf::Iterator<DSTTYPE, Acc_t> out_iter(dst, roi);

        // The ST buffer defines the output dimensions, and thus the bounds of
        // the outer loop.
        // XXX: Sampling of the source buffer can be entirely random, so there
        // are probably some opportunities for optimization in here...
        for (; !st_iter.done(); ++st_iter, ++out_iter) {
            // Look up source coordinates from ST channels.
            float src_s = st_iter[chan_s];
            float src_t = st_iter[chan_t];

            if (flip_s) {
                src_s = 1.0f - src_s;
            }
            if (flip_t) {
                src_t = 1.0f - src_t;
            }

            const float src_x = src_s * src_width;
            const float src_y = src_t * src_height;

            // Set up source iterator range
            const int x_min = clamp((int)floorf(src_x - filterrad_x), xbegin,
                                    xend);
            const int x_max = clamp((int)ceilf(src_x + filterrad_x), xbegin,
                                    xend);
            const int y_min = clamp((int)floorf(src_y - filterrad_y), ybegin,
                                    yend);
            const int y_max = clamp((int)ceilf(src_y + filterrad_y), ybegin,
                                    yend);

            src_iter.rerange(x_min, x_max + 1, y_min, y_max + 1, 0, 1);

            memset(sample_accum, 0, nchannels * sizeof(Acc_t));
            float total_weight = 0.0f;
            for (; !src_iter.done(); ++src_iter) {
                const float weight = (*filter)(src_iter.x() - src_x + 0.5f,
                                               src_iter.y() - src_y + 0.5f);
                total_weight += weight;
                for (int idx = 0, chan = roi.chbegin; chan < roi.chend;
                     ++chan, ++idx) {
                    sample_accum[idx] += src_iter[chan] * weight;
                }
            }

            if (total_weight > 0.0f) {
                for (int idx = 0, chan = roi.chbegin; chan < roi.chend;
                     ++chan, ++idx) {
                    out_iter[chan] = sample_accum[idx] / total_weight;
                }
            } else {
                for (int chan = roi.chbegin; chan < roi.chend; ++chan) {
                    out_iter[chan] = 0;
                }
            }
        }
    });  // end of parallel_image
    return true;
}



static bool
check_st_warp_args(ImageBuf& dst, const ImageBuf& src, const ImageBuf& stbuf,
                   int chan_s, int chan_t, ROI& roi)
{
    // Validate ST buffer
    if (!stbuf.initialized()) {
        dst.errorfmt("ImageBufAlgo::st_warp : Uninitialized ST buffer");
        return false;
    }

    const ImageSpec& stSpec = stbuf.spec();
    // XXX: Wanted to use `uint32_t` for channel indices, but I don't want to
    // break from the rest of the API and introduce a bunch of compile warnings.
    if (chan_s >= stSpec.nchannels) {
        dst.errorfmt("ImageBufAlgo::st_warp : Out-of-range S channel index: {}",
                     chan_s);
        return false;
    }
    if (chan_t >= stSpec.nchannels) {
        dst.errorfmt("ImageBufAlgo::st_warp : Out-of-range T channel index: {}",
                     chan_t);
        return false;
    }

    // Prep the output buffer, and then intersect the resulting ROI with the ST
    // buffer's ROI, since the ST warp is only defined for pixels in the latter.
    bool res
        = ImageBufAlgo::IBAprep(roi, &dst, &src,
                                ImageBufAlgo::IBAprep_NO_SUPPORT_VOLUME
                                    | ImageBufAlgo::IBAprep_NO_COPY_ROI_FULL);
    if (res) {
        const int chbegin = roi.chbegin;
        const int chend   = roi.chend;
        roi               = roi_intersection(roi, stSpec.roi());
        if (roi.npixels() <= 0) {
            dst.errorfmt("ImageBufAlgo::st_warp : Output ROI does not "
                         "intersect ST buffer.");
            return false;
        }
        // Make sure to preserve the channel range determined by `IBAprep`.
        roi.chbegin = chbegin;
        roi.chend   = chend;
    }
    return res;
}



bool
ImageBufAlgo::st_warp(ImageBuf& dst, const ImageBuf& src, const ImageBuf& stbuf,
                      const Filter2D* filter, int chan_s, int chan_t,
                      bool flip_s, bool flip_t, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::st_warp");

    if (!check_st_warp_args(dst, src, stbuf, chan_s, chan_t, roi)) {
        return false;
    }

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    std::shared_ptr<Filter2D> filterptr((Filter2D*)nullptr, Filter2D::destroy);
    if (!filter) {
        // If a null filter was provided, fall back to a reasonable default.
        filterptr.reset(Filter2D::create("lanczos3", 6.0f, 6.0f));
        filter = filterptr.get();
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3(ok, "st_warp", st_warp_, dst.spec().format,
                                src.spec().format, stbuf.spec().format, dst,
                                src, stbuf, chan_s, chan_t, flip_s, flip_t,
                                filter, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::st_warp(ImageBuf& dst, const ImageBuf& src, const ImageBuf& stbuf,
                      string_view filtername, float filterwidth, int chan_s,
                      int chan_t, bool flip_s, bool flip_t, ROI roi,
                      int nthreads)
{
    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    auto filter = get_warp_filter(filtername, filterwidth, dst);
    if (!filter) {
        return false;  // Error issued in `get_warp_filter`.
    }
    return st_warp(dst, src, stbuf, filter.get(), chan_s, chan_t, flip_s,
                   flip_t, roi, nthreads);
}



ImageBuf
ImageBufAlgo::st_warp(const ImageBuf& src, const ImageBuf& stbuf,
                      const Filter2D* filter, int chan_s, int chan_t,
                      bool flip_s, bool flip_t, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = st_warp(result, src, stbuf, filter, chan_s, chan_t, flip_s,
                      flip_t, roi, nthreads);
    if (!ok && !result.has_error()) {
        result.errorfmt("ImageBufAlgo::st_warp : Unknown error");
    }
    return result;
}



ImageBuf
ImageBufAlgo::st_warp(const ImageBuf& src, const ImageBuf& stbuf,
                      string_view filtername, float filterwidth, int chan_s,
                      int chan_t, bool flip_s, bool flip_t, ROI roi,
                      int nthreads)
{
    ImageBuf result;
    bool ok = st_warp(result, src, stbuf, filtername, filterwidth, chan_s,
                      chan_t, flip_s, flip_t, roi, nthreads);
    if (!ok && !result.has_error()) {
        result.errorfmt("ImageBufAlgo::st_warp : Unknown error");
    }
    return result;
}


OIIO_NAMESPACE_END
