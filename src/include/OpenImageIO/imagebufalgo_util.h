// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <functional>

#include <OpenImageIO/function_view.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/parallel.h>



OIIO_NAMESPACE_BEGIN

using std::bind;
using std::cref;
using std::ref;
using namespace std::placeholders;
using std::placeholders::_1;


namespace ImageBufAlgo {



/// Helper template for generalized multithreading for image processing
/// functions.  Some function/functor or lambda `f` is applied to every pixel
/// the region of interest roi, dividing the region into multiple threads if
/// threads != 1.  Note that threads == 0 indicates that the number of threads
/// should be as set by the global OIIO "threads" attribute.
///
/// The `opt.splitdir` determines along which axis the split will be made. The
/// default is SplitDir::Y (vertical splits), which generally seems the
/// fastest (due to cache layout issues?), but perhaps there are algorithms
/// where it's better to split in X, Z, or along the longest axis.
///
inline void
parallel_image(ROI roi, paropt opt, std::function<void(ROI)> f)
{
    opt.resolve();
    // Try not to assign a thread less than 16k pixels, or it's not worth
    // the thread startup/teardown cost.
    opt.maxthreads(
        std::min(opt.maxthreads(), 1 + int(roi.npixels() / opt.minitems())));
    if (opt.singlethread()) {
        // Just one thread, or a small image region, or if recursive use of
        // parallel_image is disallowed: use this thread only
        f(roi);
        return;
    }

    // If splitdir was not explicit, find the longest edge.
    paropt::SplitDir splitdir = opt.splitdir();
    if (splitdir == paropt::SplitDir::Biggest)
        splitdir = roi.width() > roi.height() ? paropt::SplitDir::X
                                              : paropt::SplitDir::Y;

    int64_t xchunk = 0, ychunk = 0;
    if (splitdir == paropt::SplitDir::Y) {
        xchunk = roi.width();
        // ychunk = std::max (64, minitems/xchunk);
    } else if (splitdir == paropt::SplitDir::X) {
        ychunk = roi.height();
        // ychunk = std::max (64, minitems/xchunk);
    } else if (splitdir == paropt::SplitDir::Tile) {
        int64_t n = std::min<imagesize_t>(opt.minitems(), roi.npixels());
        xchunk = ychunk = std::max(1, int(std::sqrt(n)) / 4);
    } else {
        xchunk = ychunk = std::max(int64_t(1),
                                   int64_t(std::sqrt(opt.maxthreads())) / 2);
    }

    auto task = [&](int64_t xbegin, int64_t xend, int64_t ybegin,
                    int64_t yend) {
        f(ROI(xbegin, xend, ybegin, yend, roi.zbegin, roi.zend, roi.chbegin,
              roi.chend));
    };
    parallel_for_chunked_2D(roi.xbegin, roi.xend, xchunk, roi.ybegin, roi.yend,
                            ychunk, task, opt);
}


inline void
parallel_image(ROI roi, std::function<void(ROI)> f)
{
    parallel_image(roi, paropt(), f);
}



/// Common preparation for IBA functions (or work-alikes): Given an ROI (which
/// may or may not be the default ROI::All()), destination image (which may or
/// may not yet be allocated), and optional input images (presented as a span
/// of pointers to ImageBufs), adjust `roi` if necessary and allocate pixels
/// for `dst` if necessary.  If `dst` is already initialized, it will keep its
/// "full" (aka display) window, otherwise its full/display window will be set
/// to the union of inputs' full/display windows.  If `dst` is uninitialized
/// and `force_spec` is not nullptr, use `*force_spec` as `dst`'s new spec
/// rather than using the first input image.  Also, if any inputs are
/// specified but not initialized or are broken, it's an error, so return
/// false. If all is ok, return true.
///
/// The `options` list contains optional ParamValue's that control the
/// behavior, including what input configurations are considered errors, and
/// policies for how an uninitialized output is constructed from knowledge of
/// the input images.  The following options are recognized:
///
///   - "require_alpha" : int (default: 0)
///
///     If nonzero, require all inputs and output to have an alpha channel.
///
///   - "require_z" : int (default: 0)
///
///     If nonzero, require all inputs and output to have a z channel.
///
///   - "require_same_nchannels" : int (default: 0)
///
///     If nonzero, require all inputs and output to have the same number of
///     channels.
///
///   - "copy_roi_full" : int (default: 1)
///
///     Copy the src's roi_full. This is the default behavior. Set to 0 to
///     disable copying roi_full from src to dst.
///
///   - "support_volume" : int (default: 1)
///
///     Support volumetric (3D) images. This is the default behavior. Set to 0
///     to disable support for 3D images.
///
///   - "copy_metadata" : string (default: "true")
///
///     If set to "true-like" value, copy most "safe" metadata from the first
///     input image to the destination image. If set to "all", copy all
///     metadata from the first input image to the destination image, even
///     dubious things. If set to a "false-like" value, do not copy any
///     metadata from the input images to the destination image.
///
///   - "clamp_mutual_nchannels" : int (default: 0)
///
///     If nonzero, clamp roi.chend to the minimum number of channels of any
///     of the input images.
///
///   - "support_deep" : string (default: "false")
///
///     If "false-like" (the default), deep images (having multiple depth
///     values per pixel) are not supported. If set to a true-like value
///     (e.g., "1", "on", "true", "yes"), deep images are allowed, but not
///     required, and if any input or output image is deep, they all must be
///     deep. If set to "mixed", any mixture of deep and non-deep images may
///     be supplied. If set to "required", all input and output images must be
///     deep.
///
///   - "dst_float_pixels" : int (default: 0)
///
///     If nonzero and dst is uninitialized, then initialize it to float
///     regardless of the pixel types of the input images.
///
///   - "dst_datatype" : string (default: "")
///
///     If not empty and dst is uninitialized, then initialize it to the data
///     type indicated by the string regardless of the pixel types of any
///     input images.
///
///   - "minimize_nchannels" : int (default: 0)
///
///     If nonzero and dst is uninitialized and the multiple input images do
///     not all have the same number of channels, initialize `dst` to have the
///     smallest number of channels of any input. (If 0, the default, an
///     uninitialized `dst` will be given the maximum of the number of
///     channels of all input images.)
///
///   - "require_matching_channels" : int (default: 0)
///
///     If nonzero, require all input images to have the same channel *names*,
///     in the same order.
///
///   - "merge_metadata" : int (default: 0)
///
///     If nonzero, merge all inputs' metadata into the `dst` image's
///     metadata.
///
///   - "fill_zero_alloc" : int (default: 0)
///
///     If nonzero and `dst` is uninitialized, fill `dst` with 0 values if we
///     allocate space for it.
///
bool
IBAprep(ROI& roi, ImageBuf& dst, cspan<const ImageBuf*> srcs = {},
        KWArgs options = {}, ImageSpec* force_spec = nullptr);


/// Common preparation for IBA functions: Given an ROI (which may or may not
/// be the default ROI::All()), destination image (which may or may not yet
/// be allocated), and optional input images, adjust roi if necessary and
/// allocate pixels for dst if necessary.  If dst is already initialized, it
/// will keep its "full" (aka display) window, otherwise its full/display
/// window will be set to the union of A's and B's full/display windows.  If
/// dst is uninitialized and  force_spec is not NULL, use *force_spec as
/// dst's new spec rather than using A's.  Also, if A or B inputs are
/// specified but not initialized or broken, it's an error so return false.
/// If all is ok, return true.  Some additional checks and behaviors may be
/// specified by the 'prepflags', which is a bit field defined by
/// IBAprep_flags.
bool OIIO_API
IBAprep(ROI& roi, ImageBuf* dst, const ImageBuf* A = NULL,
        const ImageBuf* B = NULL, const ImageBuf* C = NULL,
        ImageSpec* force_spec = NULL, int prepflags = 0);
inline bool
IBAprep(ROI& roi, ImageBuf* dst, const ImageBuf* A, const ImageBuf* B,
        ImageSpec* force_spec, int prepflags = 0)
{
    return IBAprep(roi, dst, A, B, NULL, force_spec, prepflags);
}
inline bool
IBAprep(ROI& roi, ImageBuf* dst, const ImageBuf* A, const ImageBuf* B,
        int prepflags)
{
    return IBAprep(roi, dst, A, B, NULL, NULL, prepflags);
}
inline bool
IBAprep(ROI& roi, ImageBuf* dst, const ImageBuf* A, int prepflags)
{
    return IBAprep(roi, dst, A, NULL, NULL, NULL, prepflags);
}
inline bool
IBAprep(ROI& roi, ImageBuf* dst, int prepflags)
{
    return IBAprep(roi, dst, nullptr, nullptr, nullptr, nullptr, prepflags);
}


// clang-format off


enum IBAprep_flags {
    IBAprep_DEFAULT = 0,
    IBAprep_REQUIRE_ALPHA = 1<<0,
    IBAprep_REQUIRE_Z = 1<<1,
    IBAprep_REQUIRE_SAME_NCHANNELS = 1<<2,
    IBAprep_NO_COPY_ROI_FULL = 1<<3,    // Don't copy the src's roi_full
    IBAprep_NO_SUPPORT_VOLUME = 1<<4,   // Don't know how to do volumes
    IBAprep_NO_COPY_METADATA = 1<<8,    // N.B. default copies all metadata
    IBAprep_COPY_ALL_METADATA = 1<<9,   // Even unsafe things
    IBAprep_CLAMP_MUTUAL_NCHANNELS = 1<<10, // Clamp roi.chend to min of inputs
    IBAprep_SUPPORT_DEEP = 1<<11,       // Operation allows deep images
    IBAprep_DEEP_MIXED = 1<<12,         // Allow deep & non-deep combinations
    IBAprep_DST_FLOAT_PIXELS = 1<<13,   // If dst is uninit, make it float
    IBAprep_MINIMIZE_NCHANNELS = 1<<14, // Multi-inputs get min(nchannels)
    IBAprep_REQUIRE_MATCHING_CHANNELS = 1<<15, // Channel names must match
    IBAprep_MERGE_METADATA = 1 << 16,   // Merge all inputs' metadata
    IBAprep_FILL_ZERO_ALLOC = 1 << 17,  // Fill with 0 if we alloc space
};



OIIO_DEPRECATED("Use TypeDesc::basetype_merge [2.3]")
inline TypeDesc::BASETYPE type_merge (TypeDesc::BASETYPE a, TypeDesc::BASETYPE b)
{
    return TypeDesc::basetype_merge(a, b);
}

OIIO_DEPRECATED("Use TypeDesc::basetype_merge [2.3]")
inline TypeDesc type_merge (TypeDesc a, TypeDesc b) {
    return TypeDesc::basetype_merge(a, b);
}

OIIO_DEPRECATED("Use TypeDesc::basetype_merge [2.3]")
inline TypeDesc type_merge (TypeDesc a, TypeDesc b, TypeDesc c)
{
    return TypeDesc::basetype_merge(TypeDesc::basetype_merge(a,b), c);
}


inline bool is_common_pixel_type(TypeDesc t)
{
    return t.basetype == TypeDesc::FLOAT || t.basetype == TypeDesc::UINT8 
        || t.basetype == TypeDesc::UINT16 || t.basetype == TypeDesc::HALF;
}



// Macro to call a type-specialized version func<type>(R,...)
// instantiated for EVERY pixel type we support.
#define OIIO_DISPATCH_TYPES(ret,name,func,type,R,...)                   \
    switch (type.basetype) {                                            \
    case TypeDesc::FLOAT :                                              \
        ret = func<float> (R, __VA_ARGS__); break;                      \
    case TypeDesc::UINT8 :                                              \
        ret = func<unsigned char> (R, __VA_ARGS__); break;              \
    case TypeDesc::HALF  :                                              \
        ret = func<half> (R, __VA_ARGS__); break;                       \
    case TypeDesc::UINT16:                                              \
        ret = func<unsigned short> (R, __VA_ARGS__); break;             \
    case TypeDesc::INT8  :                                              \
        ret = func<char> (R, __VA_ARGS__); break;                       \
    case TypeDesc::INT16 :                                              \
        ret = func<short> (R, __VA_ARGS__); break;                      \
    case TypeDesc::UINT  :                                              \
        ret = func<unsigned int> (R, __VA_ARGS__); break;               \
    case TypeDesc::INT   :                                              \
        ret = func<int> (R, __VA_ARGS__); break;                        \
    case TypeDesc::DOUBLE:                                              \
        ret = func<double> (R, __VA_ARGS__); break;                     \
    default:                                                            \
        (R).errorfmt("{}: Unsupported pixel data format '{}'", name, type); \
        ret = false;                                                    \
    }

// Helper, do not call from the outside world.
#define OIIO_DISPATCH_TYPES2_HELP(ret,name,func,Rtype,Atype,R,...)      \
    switch (Atype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<Rtype,float> (R, __VA_ARGS__); break;                \
    case TypeDesc::UINT8 :                                              \
        ret = func<Rtype,unsigned char> (R, __VA_ARGS__); break;        \
    case TypeDesc::HALF  :                                              \
        ret = func<Rtype,half> (R, __VA_ARGS__); break;                 \
    case TypeDesc::UINT16:                                              \
        ret = func<Rtype,unsigned short> (R, __VA_ARGS__); break;       \
    case TypeDesc::INT8 :                                               \
        ret = func<Rtype,char> (R, __VA_ARGS__); break;                 \
    case TypeDesc::INT16 :                                              \
        ret = func<Rtype,short> (R, __VA_ARGS__); break;                \
    case TypeDesc::UINT :                                               \
        ret = func<Rtype,unsigned int> (R, __VA_ARGS__); break;         \
    case TypeDesc::INT :                                                \
        ret = func<Rtype,int> (R, __VA_ARGS__); break;                  \
    case TypeDesc::DOUBLE :                                             \
        ret = func<Rtype,double> (R, __VA_ARGS__); break;               \
    default:                                                            \
        (R).errorfmt("{}: Unsupported pixel data format '{}'", name, Atype); \
        ret = false;                                                    \
    }

// Macro to call a type-specialized version func<Rtype,Atype>(R,...).
// instantiated for EVERY combination of pixel type we support.
// Note: The combinatorics here are brutal! Only do this if necessary.
#define OIIO_DISPATCH_TYPES2(ret,name,func,Rtype,Atype,R,...)           \
    switch (Rtype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,float,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT8 :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned char,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::HALF  :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,half,Atype,R,__VA_ARGS__);  \
        break;                                                          \
    case TypeDesc::UINT16:                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned short,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::INT8:                                                \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,char,Atype,R,__VA_ARGS__);  \
        break;                                                          \
    case TypeDesc::INT16:                                               \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,short,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT:                                                \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned int,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::INT:                                                 \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,int,Atype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::DOUBLE:                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,double,Atype,R,__VA_ARGS__);\
        break;                                                          \
    default:                                                            \
        (R).errorfmt("{}: Unsupported pixel data format '{}'", name, Rtype); \
        ret = false;                                                    \
    }


// Macro to call a type-specialized version func<type>(R,...) for
// the most common types, will auto-convert the rest to float.
#define OIIO_DISPATCH_COMMON_TYPES(ret,name,func,type,R,...)            \
    switch (type.basetype) {                                            \
    case TypeDesc::FLOAT :                                              \
        ret = func<float> (R, __VA_ARGS__); break;                      \
    case TypeDesc::UINT8 :                                              \
        ret = func<unsigned char> (R, __VA_ARGS__); break;              \
    case TypeDesc::HALF  :                                              \
        ret = func<half> (R, __VA_ARGS__); break;                       \
    case TypeDesc::UINT16:                                              \
        ret = func<unsigned short> (R, __VA_ARGS__); break;             \
    default: {                                                          \
        /* other types: punt and convert to float, then copy back */    \
        ImageBuf Rtmp;                                                  \
        if ((R).initialized())                                          \
            Rtmp.copy(R, TypeFloat);                                    \
        ret = func<float> (Rtmp, __VA_ARGS__);                          \
        if (ret)                                                        \
            (R).copy(Rtmp, (R).pixeltype());                            \
        else                                                            \
            (R).errorfmt("{}", Rtmp.geterror());                        \
        }                                                               \
    }

// Helper, do not call from the outside world.
#define OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,Rtype,Atype,R,A,...) \
    switch (Atype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<Rtype,float> (R, A, __VA_ARGS__); break;             \
    case TypeDesc::UINT8 :                                              \
        ret = func<Rtype,unsigned char> (R, A, __VA_ARGS__); break;     \
    case TypeDesc::HALF  :                                              \
        ret = func<Rtype,half> (R, A, __VA_ARGS__); break;              \
    case TypeDesc::UINT16:                                              \
        ret = func<Rtype,unsigned short> (R, A, __VA_ARGS__); break;    \
    default:                                                            \
        OIIO_DASSERT(0 && "bad type");                                  \
        ret = false;                                                    \
        break;                                                          \
    }

// Macro to call a type-specialized version func<Rtype,Atype>(R,A,...) for
// the most common types. It will auto-convert remaining rare cases to float.
#define OIIO_DISPATCH_COMMON_TYPES2_FULL(ret, name, func, Rtype_, Atype_, R, A, ...)\
    {                                                                          \
        OIIO_ASSERT((R).initialized());                                        \
        ret       = false;                                                     \
        auto Rptr = &R;                                                        \
        auto Aptr = &A;                                                        \
        ImageBuf Rtmp, Atmp;                                                   \
        TypeDesc Rtype = Rtype_, Atype = Atype_;                               \
        if (!ImageBufAlgo::is_common_pixel_type(Atype)) {                      \
            Atype = TypeDesc::FLOAT;                                           \
            Atmp.copy(A, Atype);                                               \
            Aptr = &Atmp;                                                      \
        }                                                                      \
        if (!ImageBufAlgo::is_common_pixel_type(Rtype)) {                      \
            Rtype = TypeDesc::FLOAT;                                           \
            Rtmp.copy(R, Rtype);                                               \
            Rptr = &Rtmp;                                                      \
        }                                                                      \
        if (Rtype == TypeFloat) {                                              \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret, name, func, float, Atype,    \
                                             *Rptr, *Aptr, __VA_ARGS__);       \
        } else if (Rtype == TypeUInt8) {                                       \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret, name, func, uint8_t, Atype,  \
                                             *Rptr, *Aptr, __VA_ARGS__);       \
        } else if (Rtype == TypeHalf) {                                        \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret, name, func, half, Atype,     \
                                             *Rptr, *Aptr, __VA_ARGS__);       \
        } else if (Rtype == TypeUInt16) {                                      \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret, name, func, uint16_t, Atype, \
                                             *Rptr, *Aptr, __VA_ARGS__);       \
        } else {                                                               \
            OIIO_DASSERT(0 && "bad type");                                     \
        }                                                                      \
        if (Rptr != &(R)) {                                                    \
            /* If we used Rtmp, copy pixels or errors to the real R */         \
            if (ret)                                                           \
                (R).copy(*Rptr, (R).pixeltype());                              \
            else                                                               \
                (R).errorfmt("{}", Rptr->geterror());                          \
        }                                                                      \
    }


// Macro to call a type-specialized version func<Rtype,Atype>(R,A,...) for
// the most common types. It will auto-convert remaining rare cases to float.
#define OIIO_DISPATCH_COMMON_TYPES2(ret, name, func, Rtype_, Atype_, R, A, ...)\
    {                                                                          \
        OIIO_ASSERT((R).initialized());                                        \
        ret       = false;                                                     \
        auto Rptr = &R;                                                        \
        auto Aptr = &A;                                                        \
        ImageBuf Rtmp, Atmp;                                                   \
        TypeDesc Rtype = Rtype_, Atype = Atype_;                               \
        if (!ImageBufAlgo::is_common_pixel_type(Atype)) {                      \
            Atype = TypeDesc::FLOAT;                                           \
            Atmp.copy(A, Atype);                                               \
            Aptr = &Atmp;                                                      \
        }                                                                      \
        if (!ImageBufAlgo::is_common_pixel_type(Rtype) || (Rtype != Atype && Rtype != TypeFloat)) {  \
            Rtype = TypeDesc::FLOAT;                                           \
            Rtmp.copy(R, Rtype);                                               \
            Rptr = &Rtmp;                                                      \
        }                                                                      \
        OIIO_ASSERT(Rtype == TypeFloat || Rtype == Atype);                     \
        if (Rtype == TypeFloat) {                                              \
            switch (Atype.basetype) {                                          \
            case TypeDesc::FLOAT :                                             \
                ret = func<float,float> (*Rptr, *Aptr, __VA_ARGS__); break;            \
            case TypeDesc::UINT8 :                                             \
                ret = func<float,unsigned char> (*Rptr, *Aptr, __VA_ARGS__); break;    \
            case TypeDesc::HALF  :                                             \
                ret = func<float,half> (*Rptr, *Aptr, __VA_ARGS__); break;             \
            case TypeDesc::UINT16:                                             \
                ret = func<float,unsigned short> (*Rptr, *Aptr, __VA_ARGS__); break;   \
            default:                                                           \
                OIIO_DASSERT(0 && "bad type");                                 \
                ret = false;                                                   \
                break;                                                         \
            }                                                                  \
        } else if (Rtype == TypeUInt8) {                                       \
            switch (Atype.basetype) {                                          \
            case TypeDesc::FLOAT :                                             \
                ret = func<uint8_t,float> (*Rptr, *Aptr, __VA_ARGS__); break;            \
            case TypeDesc::UINT8 :                                             \
                ret = func<uint8_t,unsigned char> (*Rptr, *Aptr, __VA_ARGS__); break;    \
            /* case TypeDesc::HALF  :*/                                           \
            /*     ret = func<uint8_t,half> (*Rptr, *Aptr, __VA_ARGS__); break;*/           \
            /* case TypeDesc::UINT16:*/                                           \
            /*     ret = func<uint8_t,unsigned short> (*Rptr, *Aptr, __VA_ARGS__); break;*/ \
            default:                                                           \
                OIIO_DASSERT(0 && "bad type");                                 \
                ret = false;                                                   \
                break;                                                         \
            }                                                                  \
        } else if (Rtype == TypeHalf) {                                        \
            switch (Atype.basetype) {                                          \
            case TypeDesc::FLOAT :                                             \
                ret = func<half,float> (*Rptr, *Aptr, __VA_ARGS__); break;            \
            /* case TypeDesc::UINT8 : */                                            \
            /*     ret = func<half,unsigned char> (*Rptr, *Aptr, __VA_ARGS__); break; */   \
            case TypeDesc::HALF  :                                             \
                ret = func<half,half> (*Rptr, *Aptr, __VA_ARGS__); break;             \
            /* case TypeDesc::UINT16: */                                            \
            /*     ret = func<half,unsigned short> (*Rptr, *Aptr, __VA_ARGS__); break; */  \
            default:                                                           \
                OIIO_DASSERT(0 && "bad type");                                 \
                ret = false;                                                   \
                break;                                                         \
            }                                                                  \
        } else if (Rtype == TypeUInt16) {                                      \
            switch (Atype.basetype) {                                          \
            case TypeDesc::FLOAT :                                             \
                ret = func<uint16_t,float> (*Rptr, *Aptr, __VA_ARGS__); break;            \
            /*case TypeDesc::UINT8 : */                                          \
            /*    ret = func<uint16_t,unsigned char> (*Rptr, *Aptr, __VA_ARGS__); break; */ \
            /*case TypeDesc::HALF  : */                                          \
            /*    ret = func<uint16_t,half> (*Rptr, *Aptr, __VA_ARGS__); break; */          \
            case TypeDesc::UINT16:                                             \
                ret = func<uint16_t,unsigned short> (*Rptr, *Aptr, __VA_ARGS__); break;   \
            default:                                                           \
                OIIO_DASSERT(0 && "bad type");                                 \
                ret = false;                                                   \
                break;                                                         \
            }                                                                  \
        } else {                                                               \
            OIIO_DASSERT(0 && "bad type");                                     \
        }                                                                      \
        if (Rptr != &(R)) {                                                    \
            /* If we used Rtmp, copy pixels or errors to the real R */         \
            if (ret)                                                           \
                (R).copy(*Rptr, (R).pixeltype());                              \
            else                                                               \
                (R).errorfmt("{}", Rptr->geterror());                          \
        }                                                                      \
    }


// Macro to call a type-specialized version func<Rtype,Atype>(R,A,...) for
// the most common types. It will auto-convert other cases to/from float.
// This is the case for when we don't actually write to the read-only R image.
#define OIIO_DISPATCH_COMMON_TYPES2_CONST(ret,name,func,Rtype_,Atype_,R,A,...)  \
    {                                                                   \
        OIIO_ASSERT((R).initialized());                                 \
        ret = false;                                                    \
        auto Rptr = &R;                                                 \
        auto Aptr = &A;                                                 \
        ImageBuf Rtmp, Atmp;                                            \
        TypeDesc Rtype = Rtype_, Atype = Atype_;                        \
        if (!ImageBufAlgo::is_common_pixel_type(Rtype)) {               \
            Rtype = TypeDesc::FLOAT;                                    \
            Rtmp.copy(R, Rtype);                                        \
            Rptr = &Rtmp;                                               \
        }                                                               \
        if (!ImageBufAlgo::is_common_pixel_type(Atype)) {               \
            Atype = TypeDesc::FLOAT;                                    \
            Atmp.copy(A, Atype);                                        \
            Aptr = &Atmp;                                               \
        }                                                               \
        switch (Rtype.basetype) {                                       \
        case TypeDesc::FLOAT :                                          \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,float,Atype,*Rptr,*Aptr, __VA_ARGS__); \
            break;                                                      \
        case TypeDesc::UINT8 :                                          \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,uint8_t,Atype,*Rptr,*Aptr, __VA_ARGS__); \
            break;                                                      \
        case TypeDesc::HALF  :                                          \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,half,Atype,*Rptr,*Aptr, __VA_ARGS__); \
            break;                                                      \
        case TypeDesc::UINT16:                                          \
            OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,uint16_t,Atype,*Rptr,*Aptr, __VA_ARGS__); \
            break;                                                      \
        default:                                                        \
            OIIO_DASSERT(0 && "bad type");                              \
            break;                                                      \
        }                                                               \
    }


// Helper, do not call from the outside world.
// This is the the case where we're switching based on the result. Be very
// stingy about the types directly supported here, for all others, write into
// a temp float buffer and copy back. Remember that if the result had the same
// type as both inputs, we wouldn't be here all, so this is already an
// inefficient case.
#define OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,Atype,Btype,R,A,B,...) \
    switch (Rtype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<float,Atype,Btype> (R,A,B,__VA_ARGS__); break;       \
    case TypeDesc::UINT8 :                                              \
        ret = func<unsigned char,Atype,Btype> (R,A,B,__VA_ARGS__); break;  \
    case TypeDesc::HALF  :                                              \
        ret = func<half,Atype,Btype> (R,A,B,__VA_ARGS__); break;        \
    case TypeDesc::UINT16:                                              \
        ret = func<unsigned short,Atype,Btype> (R,A,B,__VA_ARGS__); break;  \
    default:                                                            \
        OIIO_DASSERT(0 && "bad type");                                  \
        break;                                                          \
    }

// Macro to call a type-specialized version func<Rtype,Atype,Btype>(R,A,B,...)
// the most common type combination. Remaining rare cases will auto-convert to
// float. Specifically: anything not {float,half,uint8,uint16} is converted
// to float; if A and B's types don't match, they are converted to float; and
// if R doesn't either match A and B or is float, then A and B are also
#define OIIO_DISPATCH_COMMON_TYPES3(ret,name,func,Rtype_,Atype_,Btype_,R,A,B,...)  \
    {                                                                          \
        OIIO_ASSERT((R).initialized());                                        \
        ret       = false;                                                     \
        auto Rptr = &R;                                                        \
        auto Aptr = &A;                                                        \
        auto Bptr = &B;                                                        \
        ImageBuf Rtmp, Atmp, Btmp;                                             \
        TypeDesc Rtype = Rtype_, Atype = Atype_, Btype = Btype_;               \
        if (!ImageBufAlgo::is_common_pixel_type(Atype) || (Atype != Btype && Atype != TypeFloat)) {  \
            Atype = TypeDesc::FLOAT;                                           \
            Atmp.copy(A, Atype);                                               \
            Aptr = &Atmp;                                                      \
        }                                                                      \
        if (!ImageBufAlgo::is_common_pixel_type(Btype) || (Atype != Btype && Btype != TypeFloat)) {  \
            Btype = TypeDesc::FLOAT;                                           \
            Btmp.copy(B, Btype);                                               \
            Bptr = &Btmp;                                                      \
        }                                                                      \
        OIIO_ASSERT(Atype == Btype);                                           \
        if (!ImageBufAlgo::is_common_pixel_type(Rtype) || (Rtype != Atype && Rtype != TypeFloat)) {  \
            Rtype = TypeDesc::FLOAT;                                           \
            Rtmp.copy(R, Rtype);                                               \
            Rptr = &Rtmp;                                                      \
        }                                                                      \
        OIIO_ASSERT(Rtype == Atype || Rtype == TypeFloat);                     \
        switch (Atype.basetype) {                                       \
        case TypeDesc::FLOAT :                                          \
            switch (Rtype.basetype) {                                           \
            case TypeDesc::FLOAT :                                              \
                ret = func<float,float,float>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;       \
            /*case TypeDesc::UINT8 :*/                                              \
            /*    ret = func<unsigned char,float,float>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/  \
            /*case TypeDesc::HALF  :*/                                              \
            /*    ret = func<half,float,float>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/        \
            /*case TypeDesc::UINT16:*/                                              \
            /*    ret = func<unsigned short,float,float>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/  \
            default:                                                            \
                OIIO_ASSERT(0 && "bad type");                                  \
                break;                                                          \
            }                                                           \
            break;                                                      \
        case TypeDesc::UINT8 :                                          \
            switch (Rtype.basetype) {                                           \
            case TypeDesc::FLOAT :                                              \
                ret = func<float,uint8_t,uint8_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;       \
            case TypeDesc::UINT8 :                                              \
                ret = func<unsigned char,uint8_t,uint8_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;  \
            /*case TypeDesc::HALF  :*/                                              \
            /*    ret = func<half,uint8_t,uint8_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/        \
            /*case TypeDesc::UINT16:*/                                              \
            /*    ret = func<unsigned short,uint8_t,uint8_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/  \
            default:                                                            \
                OIIO_ASSERT(0 && "bad type");                                  \
                break;                                                          \
            }                                                           \
            break;                                                      \
        case TypeDesc::HALF  :                                          \
            switch (Rtype.basetype) {                                           \
            case TypeDesc::FLOAT :                                              \
                ret = func<float,half,half>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;       \
            /*case TypeDesc::UINT8 :*/                                              \
            /*    ret = func<unsigned char,half,half>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/  \
            case TypeDesc::HALF  :                                              \
                ret = func<half,half,half>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;        \
            /*case TypeDesc::UINT16: */                                             \
            /*    ret = func<unsigned short,half,half>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;*/  \
            default:                                                            \
                OIIO_ASSERT(0 && "bad type");                                  \
                break;                                                          \
            }                                                           \
            break;                                                      \
        case TypeDesc::UINT16:                                          \
            switch (Rtype.basetype) {                                           \
            case TypeDesc::FLOAT :                                              \
                ret = func<float,uint16_t,uint16_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;       \
            /*case TypeDesc::UINT8 :                                              \
                ret = func<unsigned char,uint16_t,uint16_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break; */ \
            /*case TypeDesc::HALF  :                                              \
                ret = func<half,uint16_t,uint16_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break; */ \
            case TypeDesc::UINT16:                                              \
                ret = func<unsigned short,uint16_t,uint16_t>(*Rptr,*Aptr,*Bptr,__VA_ARGS__); break;  \
            default:                                                            \
                OIIO_ASSERT(0 && "bad type");                                  \
                break;                                                          \
            }                                                           \
            break;                                                      \
        default:                                                        \
            OIIO_ASSERT(0 && "bad type");                              \
            break;                                                      \
        }                                                               \
        if (Rptr != &(R)) {                                                    \
            /* If we used Rtmp, copy pixels or errors to the real R */         \
            if (ret)                                                           \
                (R).copy(*Rptr, (R).pixeltype());                              \
            else                                                               \
                (R).errorfmt("{}", Rptr->geterror());                          \
        }                                                                      \
    }


// Utility: for span av, if it had fewer elements than len, alloca a new
// copy that's the right length. Use the `missing` value for missing entries
// (one or more supplied, but not all), and `zdef` default to use if there
// were no entries at all. This is used in many IBA functions that take
// constant per-channel values.
#define IBA_FIX_PERCHAN_LEN(av,len,missing,zdef)                        \
    if (std::ssize(av) < len) {                                          \
        int nc = len;                                                   \
        float *vals = OIIO_ALLOCA(float, nc);                           \
        for (int i = 0;  i < nc;  ++i)                                  \
            vals[i] = i < std::ssize(av) ? av[i] : (i ? vals[i-1] : zdef);  \
        av = cspan<float>(vals, span_size_t(nc));                       \
    }

// Default IBA_FIX_PERCHAN_LEN, with zdef=0.0 and missing = the last value
// that was supplied.
#define IBA_FIX_PERCHAN_LEN_DEF(av,len)                                 \
    IBA_FIX_PERCHAN_LEN (av, len, 0.0f, av.size() ? av.back() : 0.0f);


// clang-format on


/// Simple image per-pixel unary operation: Given a source image `src`, return
/// an image of the same dimensions (and same data type, unless `options`
/// includes the "dst_float_pixels" hint turned on, which will result in a
/// float pixel result image) where each pixel is the result of running the
/// caller-supplied function `op` on the corresponding pixel values of `src`.
/// The `op` function should take two `span<float>` arguments, the first
/// referencing a destination pixel, and the second being a reference to the
/// corresponding source pixel. The `op` function should return `true` if the
/// operation was successful, or `false` if there was an error.
///
/// The `perpixel_op` function is thread-safe and will parallelize the
/// operation across multiple threads if `nthreads` is not equal to 1
/// (following the usual ImageBufAlgo `nthreads` rules), and also takes care
/// of all the pixel loops and conversions to and from `float` values.
///
/// The `options` keyword/value list contains additional controls. It supports
/// all hints described by `IBAPrep()` as well as the following:
///
///   - "nthreads" : int (default: 0)
///
///     Controls the number of threads (0 signalling to use all available
///     threads in the pool.
///
/// An example (using the binary op version) of how to implement a simple
/// pixel-by-pixel `add()` operation that is the equivalent of
/// `ImageBufAlgo::add()`:
///
/// ```
///    // Assume ImageBuf A, B are the inputs, ImageBuf R is the output
///    R = ImageBufAlgo::perpixel_op(A, B,
///            [](span<float> r, cspan<float> a, cspan<float> b) {
///                for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
///                    r[c] = a[c] + b[c];
///                return true;
///            });
/// ```
///
/// Caveats:
/// * The operation must be one that can be applied independently to each
///   pixel.
/// * If the input image is not `float`-valued pixels, there may be some
///   inefficiency due to the need to convert the pixels to `float` and back,
///   since there is no type templating and thus no opportunity to supply a
///   version of the operation that allows specialization to any other pixel
///   data types
//
OIIO_NODISCARD OIIO_API ImageBuf
perpixel_op(const ImageBuf& src,
            function_view<bool(span<float>, cspan<float>)> op,
            KWArgs options = {});

/// A version of perpixel_op that performs a binary operation, taking two
/// source images and a 3-argument `op` function that receives a destination
/// and two source pixels.
OIIO_NODISCARD OIIO_API ImageBuf
perpixel_op(const ImageBuf& srcA, const ImageBuf& srcB,
            function_view<bool(span<float>, cspan<float>, cspan<float>)> op,
            KWArgs options = {});

}  // end namespace ImageBufAlgo


OIIO_NAMESPACE_END
