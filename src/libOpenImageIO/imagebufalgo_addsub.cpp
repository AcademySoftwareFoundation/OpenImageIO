// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo algorithms that do math on
/// single pixels at a time.

#include <cmath>
#include <iostream>
#include <limits>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
#    include "imagebufalgo_hwy_pvt.h"
#endif

#include "imageio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
add_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



template<class Rtype, class Atype>
static bool
add_impl_scalar(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



#if OIIO_USE_HWY
// Native integer add using SaturatedAdd (scale-invariant, no float conversion)
template<class T>
static bool
add_impl_hwy_native_int(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
                        ROI roi, int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<T>(Rv, nchannels)
                            && ChannelsContiguous<T>(Av, nchannels)
                            && ChannelsContiguous<T>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            T* r_row       = RoiRowPtr<T>(Rv, y, roi);
            const T* a_row = RoiRowPtr<T>(Av, y, roi);
            const T* b_row = RoiRowPtr<T>(Bv, y, roi);

            if (contig) {
                // Native integer saturated add - much faster than float conversion!
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyBinaryNativeInt<T>(r_row, a_row, b_row, n,
                                         [](auto d, auto a, auto b) {
                                             return hn::SaturatedAdd(a, b);
                                         });
            } else {
                // Scalar fallback
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    T* r_ptr       = ChannelPtr<T>(Rv, x, y, roi.chbegin);
                    const T* a_ptr = ChannelPtr<T>(Av, x, y, roi.chbegin);
                    const T* b_ptr = ChannelPtr<T>(Bv, x, y, roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        // Saturating add in scalar
                        int64_t sum = (int64_t)a_ptr[c] + (int64_t)b_ptr[c];
                        if constexpr (std::is_unsigned_v<T>) {
                            r_ptr[c] = (sum > std::numeric_limits<T>::max())
                                           ? std::numeric_limits<T>::max()
                                           : (T)sum;
                        } else {
                            r_ptr[c] = (sum > std::numeric_limits<T>::max())
                                           ? std::numeric_limits<T>::max()
                                       : (sum < std::numeric_limits<T>::min())
                                           ? std::numeric_limits<T>::min()
                                           : (T)sum;
                        }
                    }
                }
            }
        }
    });
    return true;
}

template<class Rtype, class Atype, class Btype>
static bool
add_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi);
            const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);
            const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi);

            if (contig) {
                // Process whole line as one vector stream
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyCmd<Rtype, Atype, Btype>(r_row, a_row, b_row, n,
                                               [](auto d, auto a, auto b) {
                                                   return hn::Add(a, b);
                                               });
            } else {
                // Process pixel by pixel (scalar fallback for strided channels)
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const Atype* a_ptr = ChannelPtr<Atype>(Av, x, y,
                                                           roi.chbegin);
                    const Btype* b_ptr = ChannelPtr<Btype>(Bv, x, y,
                                                           roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        r_ptr[c] = static_cast<Rtype>(
                            static_cast<float>(a_ptr[c])
                            + static_cast<float>(b_ptr[c]));
                    }
                }
            }
        }
    });
    return true;
}

template<class Rtype, class Atype>
static bool
add_impl_hwy(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
             int nthreads)
{
    using SimdType
        = std::conditional_t<std::is_same_v<Rtype, double>, double, float>;
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (int y = roi.ybegin; y < roi.yend; ++y) {
            std::byte* r_row       = PixelBase(Rv, roi.xbegin, y);
            const std::byte* a_row = PixelBase(Av, roi.xbegin, y);
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                const size_t xoff = static_cast<size_t>(x - roi.xbegin);
                Rtype* r_ptr      = reinterpret_cast<Rtype*>(
                    r_row + xoff * Rv.pixel_bytes);
                const Atype* a_ptr = reinterpret_cast<const Atype*>(
                    a_row + xoff * Av.pixel_bytes);
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    r_ptr[c] = (Rtype)((SimdType)a_ptr[c] + (SimdType)b[c]);
                }
            }
        }
    });
    return true;
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class Atype, class Btype>
static bool
add_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels()) {
        // Use native integer path for scale-invariant add when all types match
        // and are integer types (much faster: 6-12x vs 3-5x with float conversion)
        constexpr bool all_same = std::is_same_v<Rtype, Atype>
                                  && std::is_same_v<Atype, Btype>;
        constexpr bool is_integer = std::is_integral_v<Rtype>;
        if constexpr (all_same && is_integer) {
            return add_impl_hwy_native_int<Rtype>(R, A, B, roi, nthreads);
        }
        return add_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
    }
#endif
    return add_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
add_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels())
        return add_impl_hwy<Rtype, Atype>(R, A, b, roi, nthreads);
#endif
    return add_impl_scalar<Rtype, Atype>(R, A, b, roi, nthreads);
}

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
// Native integer sub using SaturatedSub (scale-invariant, no float conversion)
template<class T>
static bool
sub_impl_hwy_native_int(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
                        ROI roi, int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<T>(Rv, nchannels)
                            && ChannelsContiguous<T>(Av, nchannels)
                            && ChannelsContiguous<T>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            T* r_row       = RoiRowPtr<T>(Rv, y, roi);
            const T* a_row = RoiRowPtr<T>(Av, y, roi);
            const T* b_row = RoiRowPtr<T>(Bv, y, roi);

            if (contig) {
                // Native integer saturated sub - much faster than float conversion!
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyBinaryNativeInt<T>(r_row, a_row, b_row, n,
                                         [](auto d, auto a, auto b) {
                                             return hn::SaturatedSub(a, b);
                                         });
            } else {
                // Scalar fallback
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    T* r_ptr       = ChannelPtr<T>(Rv, x, y, roi.chbegin);
                    const T* a_ptr = ChannelPtr<T>(Av, x, y, roi.chbegin);
                    const T* b_ptr = ChannelPtr<T>(Bv, x, y, roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        // Saturating sub in scalar
                        if constexpr (std::is_unsigned_v<T>) {
                            r_ptr[c] = (a_ptr[c] > b_ptr[c])
                                           ? (a_ptr[c] - b_ptr[c])
                                           : T(0);
                        } else {
                            int64_t diff = (int64_t)a_ptr[c]
                                           - (int64_t)b_ptr[c];
                            r_ptr[c] = (diff > std::numeric_limits<T>::max())
                                           ? std::numeric_limits<T>::max()
                                       : (diff < std::numeric_limits<T>::min())
                                           ? std::numeric_limits<T>::min()
                                           : (T)diff;
                        }
                    }
                }
            }
        }
    });
    return true;
}

template<class Rtype, class Atype, class Btype>
static bool
sub_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi);
            const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);
            const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyCmd<Rtype, Atype, Btype>(r_row, a_row, b_row, n,
                                               [](auto d, auto a, auto b) {
                                                   return hn::Sub(a, b);
                                               });
            } else {
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const Atype* a_ptr = ChannelPtr<Atype>(Av, x, y,
                                                           roi.chbegin);
                    const Btype* b_ptr = ChannelPtr<Btype>(Bv, x, y,
                                                           roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        r_ptr[c] = static_cast<Rtype>(
                            static_cast<float>(a_ptr[c])
                            - static_cast<float>(b_ptr[c]));
                    }
                }
            }
        }
    });
    return true;
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class Atype, class Btype>
static bool
sub_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels()) {
        // Use native integer path for scale-invariant sub when all types match
        // and are integer types (much faster: 6-12x vs 3-5x with float conversion)
        constexpr bool all_same = std::is_same_v<Rtype, Atype>
                                  && std::is_same_v<Atype, Btype>;
        constexpr bool is_integer = std::is_integral_v<Rtype>;
        if constexpr (all_same && is_integer) {
            return sub_impl_hwy_native_int<Rtype>(R, A, B, roi, nthreads);
        }
        return sub_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
    }
#endif
    return sub_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

static bool
add_impl_deep(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
              int nthreads)
{
    OIIO_ASSERT(R.deep());
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        cspan<TypeDesc> channeltypes(R.deepdata()->all_channeltypes());
        ImageBuf::Iterator<float> r(R, roi);
        ImageBuf::ConstIterator<float> a(A, roi);
        for (; !r.done(); ++r, ++a) {
            for (int samp = 0, samples = r.deep_samples(); samp < samples;
                 ++samp) {
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    if (channeltypes[c].basetype == TypeDesc::UINT32)
                        r.set_deep_value(c, samp, a.deep_value_uint(c, samp));
                    else
                        r.set_deep_value(c, samp, a.deep_value(c, samp) + b[c]);
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::add(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::add");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "add", add_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
        if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
            // Edge case: A and B differed in nchannels, we allocated dst to be
            // the bigger of them, but adjusted roi to be the lesser. Now handle
            // the channels that got left out because they were not common to
            // all the inputs.
            OIIO_ASSERT(roi.chend <= dst.nchannels());
            roi.chbegin = roi.chend;
            roi.chend   = origroi.chend;
            if (A.nchannels() > B.nchannels()) {  // A exists
                copy(dst, A, dst.spec().format, roi, nthreads);
            } else {  // B exists
                copy(dst, B, dst.spec().format, roi, nthreads);
            }
        }
        return ok;
    }
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap(B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf& A(A_.img());
        cspan<float> b = B_.val();
        if (!IBAprep(roi, &dst, &A,
                     IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples(A.deepdata()->all_samples());
            return add_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "add", add_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::add(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::add(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = add(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::add() error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
sub_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] - b[c];
    });
    return true;
}



bool
ImageBufAlgo::sub(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::sub");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "sub", sub_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
        if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
            // Edge case: A and B differed in nchannels, we allocated dst to be
            // the bigger of them, but adjusted roi to be the lesser. Now handle
            // the channels that got left out because they were not common to
            // all the inputs.
            OIIO_ASSERT(roi.chend <= dst.nchannels());
            roi.chbegin = roi.chend;
            roi.chend   = origroi.chend;
            if (A.nchannels() > B.nchannels()) {  // A exists
                copy(dst, A, dst.spec().format, roi, nthreads);
            } else {  // B exists
                copy(dst, B, dst.spec().format, roi, nthreads);
            }
        }
        return ok;
    }
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap(B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf& A(A_.img());
        cspan<float> b = B_.val();
        if (!IBAprep(roi, &dst, &A,
                     IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        // Negate b (into a copy)
        int nc      = A.nchannels();
        float* vals = OIIO_ALLOCA(float, nc);
        for (int c = 0; c < nc; ++c)
            vals[c] = -b[c];
        b = cspan<float>(vals, nc);
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples(A.deepdata()->all_samples());
            return add_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "sub", add_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::sub(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::sub(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = sub(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::sub() error");
    return result;
}


OIIO_NAMESPACE_3_1_END
