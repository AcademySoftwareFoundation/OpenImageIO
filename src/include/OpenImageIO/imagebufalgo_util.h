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


#ifndef OPENIMAGEIO_IMAGEBUFALGO_UTIL_H
#define OPENIMAGEIO_IMAGEBUFALGO_UTIL_H

#include "imagebufalgo.h"


OIIO_NAMESPACE_ENTER
{




namespace ImageBufAlgo {

/// Helper template for generalized multithreading for image processing
/// functions.  Some function/functor f is applied to every pixel the
/// region of interest roi, dividing the region into multiple threads if
/// threads != 1.  Note that threads == 0 indicates that the number of
/// threads should be as set by the global OIIO "threads" attribute.
///
/// Most image operations will require additional arguments, including
/// additional input and output images or other parameters.  The
/// parallel_image template can still be used by employing the
/// boost::bind (or std::bind, for C++11).  For example, suppose you
/// have an image operation defined as:
///     void my_image_op (ImageBuf &out, const ImageBuf &in,
///                       float scale, ROI roi);
/// Then you can parallelize it as follows:
///     ImageBuf R /*result*/, A /*input*/;
///     ROI roi = get_roi (R.spec());
///     parallel_image (boost::bind(my_image_op,boost::ref(R),
///                                 boost::cref(A),3.14,_1), roi);
///
template <class Func>
void
parallel_image (Func f, ROI roi, int nthreads=0)
{
    // Special case: threads <= 0 means to use the "threads" attribute
    if (nthreads <= 0)
        OIIO::getattribute ("threads", nthreads);

    if (nthreads <= 1 || roi.npixels() < 1000) {
        // Just one thread, or a small image region: use this thread only
        f (roi);
    } else {
        // Spawn threads by dividing the region into y bands.
        boost::thread_group threads;
        int blocksize = std::max (1, (roi.height() + nthreads - 1) / nthreads);
        int roi_ybegin = roi.ybegin;
        int roi_yend = roi.yend;
        for (int i = 0;  i < nthreads;  i++) {
            roi.ybegin = roi_ybegin + i * blocksize;
            roi.yend = std::min (roi.ybegin + blocksize, roi_yend);
            if (roi.ybegin >= roi.yend)
                break;   // no more work to dole out
            threads.add_thread (new boost::thread (f, roi));
        }
        threads.join_all ();
    }
}



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
bool OIIO_API IBAprep (ROI &roi, ImageBuf *dst,
                       const ImageBuf *A=NULL, const ImageBuf *B=NULL,
                       ImageSpec *force_spec=NULL, int prepflags=0);
inline bool IBAprep (ROI &roi, ImageBuf *dst,
                     const ImageBuf *A, int prepflags) {
    return IBAprep (roi, dst, A, NULL, NULL, prepflags);
}

enum IBAprep_flags {
    IBAprep_DEFAULT = 0,
    IBAprep_REQUIRE_ALPHA = 1,
    IBAprep_REQUIRE_Z = 2,
    IBAprep_REQUIRE_SAME_NCHANNELS = 4,
    IBAprep_NO_COPY_ROI_FULL = 8,       // Don't copy the src's roi_full
    IBAprep_NO_COPY_METADATA = 256,     // N.B. default copies all metadata
    IBAprep_COPY_ALL_METADATA = 512     // Even unsafe things
};



// Macro to call a type-specialzed version func<type>(R,...)
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
        (R).error ("%s: Unsupported pixel data format '%s'", name, type); \
        ret = false;                                                    \
    }

// Helper, do not call from the outside world.
#define OIIO_DISPATCH_TYPES2_HELP(ret,name,func,Atype,Btype,R,...)      \
    switch (Btype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<Atype,float> (R, __VA_ARGS__); break;                \
    case TypeDesc::UINT8 :                                              \
        ret = func<Atype,unsigned char> (R, __VA_ARGS__); break;        \
    case TypeDesc::HALF  :                                              \
        ret = func<Atype,half> (R, __VA_ARGS__); break;                 \
    case TypeDesc::UINT16:                                              \
        ret = func<Atype,unsigned short> (R, __VA_ARGS__); break;       \
    case TypeDesc::INT8 :                                               \
        ret = func<Atype,char> (R, __VA_ARGS__); break;                 \
    case TypeDesc::INT16 :                                              \
        ret = func<Atype,short> (R, __VA_ARGS__); break;                \
    case TypeDesc::UINT :                                               \
        ret = func<Atype,unsigned int> (R, __VA_ARGS__); break;         \
    case TypeDesc::INT :                                                \
        ret = func<Atype,int> (R, __VA_ARGS__); break;                  \
    case TypeDesc::DOUBLE :                                             \
        ret = func<Atype,double> (R, __VA_ARGS__); break;               \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Btype); \
        ret = false;                                                    \
    }

// Macro to call a type-specialzed version func<Atype,Btype>(R,...).
#define OIIO_DISPATCH_TYPES2(ret,name,func,Atype,Btype,R,...)           \
    switch (Atype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,float,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT8 :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned char,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::HALF  :                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,half,Btype,R,__VA_ARGS__);  \
        break;                                                          \
    case TypeDesc::UINT16:                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned short,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::INT8:                                                \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,char,Btype,R,__VA_ARGS__);  \
        break;                                                          \
    case TypeDesc::INT16:                                               \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,short,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT:                                                \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,unsigned int,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::INT:                                                 \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,int,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::DOUBLE:                                              \
        OIIO_DISPATCH_TYPES2_HELP(ret,name,func,double,Btype,R,__VA_ARGS__);\
        break;                                                          \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Atype); \
        ret = false;                                                    \
    }


// Macro to call a type-specialzed version func<type>(R,...) for
// the most common types, fail for anything else.
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
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, type); \
        ret = false;                                                    \
    }

// Helper, do not call from the outside world.
#define OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,Atype,Btype,R,...) \
    switch (Btype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<Atype,float> (R, __VA_ARGS__); break;                \
    case TypeDesc::UINT8 :                                              \
        ret = func<Atype,unsigned char> (R, __VA_ARGS__); break;        \
    case TypeDesc::HALF  :                                              \
        ret = func<Atype,half> (R, __VA_ARGS__); break;                 \
    case TypeDesc::UINT16:                                              \
        ret = func<Atype,unsigned short> (R, __VA_ARGS__); break;       \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Btype); \
        ret = false;                                                    \
    }

// Macro to call a type-specialzed version func<Atype,Btype>(R,...) for
// the most common types, fail for anything else.
#define OIIO_DISPATCH_COMMON_TYPES2(ret,name,func,Atype,Btype,R,...)    \
    switch (Atype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,float,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT8 :                                              \
        OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,unsigned char,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::HALF  :                                              \
        OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,half,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT16:                                              \
        OIIO_DISPATCH_COMMON_TYPES2_HELP(ret,name,func,unsigned short,Btype,R,__VA_ARGS__); \
        break;                                                          \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Atype); \
        ret = false;                                                    \
    }


// Helper, do not call from the outside world.
#define OIIO_DISPATCH_COMMON_TYPES3_HELP2(ret,name,func,Rtype,Atype,Btype,R,...) \
    switch (Rtype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        ret = func<float,Atype,Btype> (R, __VA_ARGS__); break;          \
    case TypeDesc::UINT8 :                                              \
        ret = func<unsigned char,Atype,Btype> (R, __VA_ARGS__); break;  \
    case TypeDesc::HALF  :                                              \
        ret = func<half,Atype,Btype> (R, __VA_ARGS__); break;           \
    case TypeDesc::UINT16:                                              \
        ret = func<unsigned short,Atype,Btype> (R, __VA_ARGS__); break;  \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Rtype); \
        ret = false;                                                    \
    }

// Helper, do not call from the outside world.
#define OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,Atype,Btype,R,...) \
    switch (Btype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP2(ret,name,func,Rtype,Atype,float,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT8 :                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP2(ret,name,func,Rtype,Atype,unsigned char,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::HALF :                                               \
        OIIO_DISPATCH_COMMON_TYPES3_HELP2(ret,name,func,Rtype,Atype,half,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT16 :                                             \
        OIIO_DISPATCH_COMMON_TYPES3_HELP2(ret,name,func,Rtype,Atype,unsigned short,R,__VA_ARGS__); \
        break;                                                          \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Btype); \
        ret = false;                                                    \
    }

// Macro to call a type-specialzed version func<Rtype,Atype,Btype>(R,...)
// for the most common types, fail for anything else.
#define OIIO_DISPATCH_COMMON_TYPES3(ret,name,func,Rtype,Atype,Btype,R,...)  \
    switch (Atype.basetype) {                                           \
    case TypeDesc::FLOAT :                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,float,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT8 :                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,unsigned char,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::HALF  :                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,half,Btype,R,__VA_ARGS__); \
        break;                                                          \
    case TypeDesc::UINT16:                                              \
        OIIO_DISPATCH_COMMON_TYPES3_HELP(ret,name,func,Rtype,unsigned short,Btype,R,__VA_ARGS__); \
        break;                                                          \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, Atype); \
        ret = false;                                                    \
    }



}  // end namespace ImageBufAlgo


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUFALGO_UTIL_H
