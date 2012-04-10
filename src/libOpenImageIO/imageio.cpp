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

#include <cstdio>
#include <cstdlib>

#include <OpenEXR/half.h>
#include <OpenEXR/ImathFun.h>

#include <boost/scoped_array.hpp>

#include "dassert.h"
#include "typedesc.h"
#include "strutil.h"
#include "fmath.h"

#include "imageio.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_ENTER
{

// Global private data
namespace pvt {
int oiio_threads = 1;
ustring plugin_searchpath;
};



using namespace pvt;

namespace {


// To avoid thread oddities, we have the storage area buffering error
// messages for error()/geterror() be thread-specific.
static thread_specific_ptr<std::string> thread_error_msg;

// Return a reference to the string for this thread's error messages,
// creating it if none exists for this thread thus far.
static std::string &
error_msg ()
{
    std::string *e = thread_error_msg.get();
    if (! e) {
        e = new std::string;
        thread_error_msg.reset (e);
    }
    return *e;
}

} // end anon namespace

recursive_mutex pvt::imageio_mutex;



int
openimageio_version ()
{
    return OIIO_VERSION;
}



/// Error reporting for the plugin implementation: call this with
/// printf-like arguments.
void
pvt::error (const char *message, ...)
{
    recursive_lock_guard lock (pvt::imageio_mutex);
    va_list ap;
    va_start (ap, message);
    error_msg() = Strutil::vformat (message, ap);
    va_end (ap);
}



std::string
geterror ()
{
    recursive_lock_guard lock (pvt::imageio_mutex);
    std::string e = error_msg();
    error_msg().clear ();
    return e;
}



namespace {

// Private global OIIO data.

static spin_mutex attrib_mutex;
static const int maxthreads = 64;   // reasonable maximum for sanity check

};



bool
attribute (const std::string &name, TypeDesc type, const void *val)
{
    spin_lock lock (attrib_mutex);
    if (name == "threads" && type == TypeDesc::TypeInt) {
        oiio_threads = Imath::clamp (*(const int *)val, 0, maxthreads);
        if (oiio_threads == 0) {
#if (BOOST_VERSION >= 103500)
            oiio_threads = boost::thread::hardware_concurrency();
#else
            oiio_threads = 1;
#endif
        }
        return true;
    }
    if (name == "plugin_searchpath" && type == TypeDesc::TypeString) {
        plugin_searchpath = ustring (*(const char **)val);
        return true;
    }
    return false;
}



bool
getattribute (const std::string &name, TypeDesc type, void *val)
{
    spin_lock lock (attrib_mutex);
    if (name == "threads" && type == TypeDesc::TypeInt) {
        *(int *)val = oiio_threads;
        return true;
    }
    if (name == "plugin_searchpath" && type == TypeDesc::TypeString) {
        *(ustring *)val = plugin_searchpath;
        return true;
    }
    return false;
}



int
quantize (float value, int quant_black, int quant_white,
                       int quant_min, int quant_max)
{
    value = Imath::lerp (quant_black, quant_white, value);
    return Imath::clamp ((int)(value + 0.5f), quant_min, quant_max);
}

namespace {

/// Type-independent template for turning potentially
/// non-contiguous-stride data (e.g. "RGB RGB ") into contiguous-stride
/// ("RGBRGB").  Caller must pass in a dst pointing to enough memory to
/// hold the contiguous rectangle.  Return a ptr to where the contiguous
/// data ended up, which is either dst or src (if the strides indicated
/// that data were already contiguous).
template<typename T>
const T *
_contiguize (const T *src, int nchannels, stride_t xstride, stride_t ystride, stride_t zstride, 
             T *dst, int width, int height, int depth)
{
    int datasize = sizeof(T);
    if (xstride == nchannels*datasize  &&  ystride == xstride*width  &&
            (zstride == ystride*height || !zstride))
        return src;

    if (depth < 1)     // Safeguard against volume-unaware clients
        depth = 1;
    T *dstsave = dst;
    for (int z = 0;  z < depth;  ++z, src = (const T *)((char *)src + zstride)) {
        const T *scanline = src;
        for (int y = 0;  y < height;  ++y, scanline = (const T *)((char *)scanline + ystride)) {
            const T *pixel = scanline;
            for (int x = 0;  x < width;  ++x, pixel = (const T *)((char *)pixel + xstride))
                for (int c = 0;  c < nchannels;  ++c)
                    *dst++ = pixel[c];
        }
    }
    return dstsave;
}

}

const void *
pvt::contiguize (const void *src, int nchannels,
                 stride_t xstride, stride_t ystride, stride_t zstride, 
                 void *dst, int width, int height, int depth,
                 TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::FLOAT :
        return _contiguize ((const float *)src, nchannels, 
                            xstride, ystride, zstride,
                            (float *)dst, width, height, depth);
    case TypeDesc::DOUBLE :
        return _contiguize ((const double *)src, nchannels, 
                            xstride, ystride, zstride,
                            (double *)dst, width, height, depth);
    case TypeDesc::INT8:
    case TypeDesc::UINT8 :
        return _contiguize ((const char *)src, nchannels, 
                            xstride, ystride, zstride,
                            (char *)dst, width, height, depth);
    case TypeDesc::HALF :
        DASSERT (sizeof(half) == sizeof(short));
    case TypeDesc::INT16 :
    case TypeDesc::UINT16 :
        return _contiguize ((const short *)src, nchannels, 
                            xstride, ystride, zstride,
                            (short *)dst, width, height, depth);
    case TypeDesc::INT :
    case TypeDesc::UINT :
        return _contiguize ((const int *)src, nchannels, 
                            xstride, ystride, zstride,
                            (int *)dst, width, height, depth);
    case TypeDesc::INT64 :
    case TypeDesc::UINT64 :
        return _contiguize ((const long long *)src, nchannels, 
                            xstride, ystride, zstride,
                            (long long *)dst, width, height, depth);
    default:
        ASSERT (0 && "OpenImageIO::contiguize : bad format");
        return NULL;
    }
}



const float *
pvt::convert_to_float (const void *src, float *dst, int nvals,
                       TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::FLOAT :
        return (float *)src;
    case TypeDesc::HALF :
        convert_type ((const half *)src, dst, nvals);
        break;
    case TypeDesc::DOUBLE :
        convert_type ((const double *)src, dst, nvals);
        break;
    case TypeDesc::INT8:
        convert_type ((const char *)src, dst, nvals);
        break;
    case TypeDesc::UINT8 :
        convert_type ((const unsigned char *)src, dst, nvals);
        break;
    case TypeDesc::INT16 :
        convert_type ((const short *)src, dst, nvals);
        break;
    case TypeDesc::UINT16 :
        convert_type ((const unsigned short *)src, dst, nvals);
        break;
    case TypeDesc::INT :
        convert_type ((const int *)src, dst, nvals);
        break;
    case TypeDesc::UINT :
        convert_type ((const unsigned int *)src, dst, nvals);
        break;
    case TypeDesc::INT64 :
        convert_type ((const long long *)src, dst, nvals);
        break;
    case TypeDesc::UINT64 :
        convert_type ((const unsigned long long *)src, dst, nvals);
        break;
    default:
        ASSERT (0 && "ERROR to_float: bad format");
        return NULL;
    }
    return dst;
}



template<typename T>
const void *
_from_float (const float *src, T *dst, size_t nvals,
            int quant_black, int quant_white, int quant_min, int quant_max)
{
    if (! src) {
        // If no source pixels, assume zeroes
        memset (dst, 0, nvals * sizeof(T));
        T z = (T) quantize (0, quant_black, quant_white,
                            quant_min, quant_max);
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = z;
    } else if (std::numeric_limits <T>::is_integer) {
        // Convert float to non-float native format, with quantization
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = (T) quantize (src[p], quant_black, quant_white,
                                   quant_min, quant_max);
    } else {
        // It's a floating-point type of some kind -- we don't apply 
        // quantization
        if (sizeof(T) == sizeof(float)) {
            // It's already float -- return the source itself
            return src;
        }
        // Otherwise, it's converting between two fp types
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = (T) src[p];
    }

    return dst;
}



const void *
pvt::convert_from_float (const float *src, void *dst, size_t nvals,
                         int quant_black, int quant_white,
                         int quant_min, int quant_max,
                         TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::FLOAT :
        return src;
    case TypeDesc::HALF :
        return _from_float<half> (src, (half *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::DOUBLE :
        return _from_float (src, (double *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::INT8:
        return _from_float (src, (char *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::UINT8 :
        return _from_float (src, (unsigned char *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::INT16 :
        return _from_float (src, (short *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::UINT16 :
        return _from_float (src, (unsigned short *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::INT :
        return _from_float (src, (int *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::UINT :
        return _from_float (src, (unsigned int *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::INT64 :
        return _from_float (src, (long long *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    case TypeDesc::UINT64 :
        return _from_float (src, (unsigned long long *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max);
    default:
        ASSERT (0 && "ERROR from_float: bad format");
        return NULL;
    }
}


bool
convert_types (TypeDesc src_type, const void *src, 
               TypeDesc dst_type, void *dst, int n,
               int alpha_channel, int z_channel)
{
    // If no conversion is necessary, just memcpy
    if ((src_type == dst_type || dst_type.basetype == TypeDesc::UNKNOWN)) {
        memcpy (dst, src, n * src_type.size());
        return true;
    }

    // Conversions are via a temporary float array
    bool use_tmp = false;
    boost::scoped_array<float> tmp;
    float *buf;
    if (src_type == TypeDesc::FLOAT) {
        buf = (float *) src;
    } else {
        tmp.reset (new float[n]);  // Will be freed when tmp exists its scope
        buf = tmp.get();
        use_tmp = true;
    }

    if (use_tmp) {
        // Convert from 'src_type' to float (or nothing, if already float)
        switch (src_type.basetype) {
        case TypeDesc::UINT8 : convert_type ((const unsigned char *)src, buf, n); break;
        case TypeDesc::UINT16 : convert_type ((const unsigned short *)src, buf, n); break;
        case TypeDesc::FLOAT : convert_type ((const float *)src, buf, n); break;
        case TypeDesc::HALF :  convert_type ((const half *)src, buf, n); break;
        case TypeDesc::DOUBLE : convert_type ((const double *)src, buf, n); break;
        case TypeDesc::INT8 :  convert_type ((const char *)src, buf, n);  break;
        case TypeDesc::INT16 : convert_type ((const short *)src, buf, n); break;
        case TypeDesc::INT :   convert_type ((const int *)src, buf, n); break;
        case TypeDesc::UINT :  convert_type ((const unsigned int *)src, buf, n);  break;
        case TypeDesc::INT64 : convert_type ((const long long *)src, buf, n); break;
        case TypeDesc::UINT64 : convert_type ((const unsigned long long *)src, buf, n);  break;
        default:         return false;  // unknown format
        }
    }

    // Convert float to 'dst_type' (just a copy if dst is float)
    switch (dst_type.basetype) {
    case TypeDesc::FLOAT :  memcpy (dst, buf, n * sizeof(float));       break;
    case TypeDesc::UINT8 :  convert_type (buf, (unsigned char *)dst, n);  break;
    case TypeDesc::UINT16 : convert_type (buf, (unsigned short *)dst, n); break;
    case TypeDesc::HALF :   convert_type (buf, (half *)dst, n);   break;
    case TypeDesc::INT8 :   convert_type (buf, (char *)dst, n);   break;
    case TypeDesc::INT16 :  convert_type (buf, (short *)dst, n);  break;
    case TypeDesc::INT :    convert_type (buf, (int *)dst, n);  break;
    case TypeDesc::UINT :   convert_type (buf, (unsigned int *)dst, n);  break;
    case TypeDesc::INT64 :  convert_type (buf, (long long *)dst, n);  break;
    case TypeDesc::UINT64 : convert_type (buf, (unsigned long long *)dst, n);  break;
    case TypeDesc::DOUBLE : convert_type (buf, (double *)dst, n); break;
        default:            return false;  // unknown format
    }

    return true;
}



bool
convert_image (int nchannels, int width, int height, int depth,
               const void *src, TypeDesc src_type,
               stride_t src_xstride, stride_t src_ystride,
               stride_t src_zstride,
               void *dst, TypeDesc dst_type,
               stride_t dst_xstride, stride_t dst_ystride,
               stride_t dst_zstride,
               int alpha_channel, int z_channel)
{
    // If no format conversion is taking place, use the simplified
    // copy_image.
    if (src_type == dst_type)
        return copy_image (nchannels, width, height, depth, src, 
                           src_type.size()*nchannels,
                           src_xstride, src_ystride, src_zstride,
                           dst, dst_xstride, dst_ystride, dst_zstride);

    ImageSpec::auto_stride (src_xstride, src_ystride, src_zstride,
                            src_type, nchannels, width, height);
    ImageSpec::auto_stride (dst_xstride, dst_ystride, dst_zstride,
                            dst_type, nchannels, width, height);
    bool result = true;
    bool contig = (src_xstride == dst_xstride &&
                   src_xstride == stride_t(nchannels*src_type.size()));
    for (int z = 0;  z < depth;  ++z) {
        for (int y = 0;  y < height;  ++y) {
            const char *f = (const char *)src + (z*src_zstride + y*src_ystride);
            char *t = (char *)dst + (z*dst_zstride + y*dst_ystride);
            if (contig) {
                // Special case: pixels within each row are contiguous
                // in both src and dst and we're copying all channels.
                // Be efficient by converting each scanline as a single
                // unit.  (Note that within convert_types, a memcpy will
                // be used if the formats are identical.)
                result &= convert_types (src_type, f, dst_type, t,
                                         nchannels*width,
                                         alpha_channel, z_channel);
            } else {
                // General case -- anything goes with strides.
                for (int x = 0;  x < width;  ++x) {
                    result &= convert_types (src_type, f, dst_type, t,
                                             nchannels,
                                             alpha_channel, z_channel);
                    f += src_xstride;
                    t += dst_xstride;
                }
            }
        }
    }
    return result;
}



bool
copy_image (int nchannels, int width, int height, int depth,
            const void *src, stride_t pixelsize, stride_t src_xstride,
            stride_t src_ystride, stride_t src_zstride, void *dst, 
            stride_t dst_xstride, stride_t dst_ystride, stride_t dst_zstride)
{
    stride_t channelsize = pixelsize / nchannels;
    ImageSpec::auto_stride (src_xstride, src_ystride, src_zstride,
                            channelsize, nchannels, width, height);
    ImageSpec::auto_stride (dst_xstride, dst_ystride, dst_zstride,
                            channelsize, nchannels, width, height);
    bool contig = (src_xstride == dst_xstride &&
                   src_xstride == (stride_t)pixelsize);
    for (int z = 0;  z < depth;  ++z) {
        for (int y = 0;  y < height;  ++y) {
            const char *f = (const char *)src + (z*src_zstride + y*src_ystride);
            char *t = (char *)dst + (z*dst_zstride + y*dst_ystride);
            if (contig) {
                // Special case: pixels within each row are contiguous
                // in both src and dst and we're copying all channels.
                // Be efficient by converting each scanline as a single
                // unit.
                memcpy (t, f, width*pixelsize);
            } else {
                // General case -- anything goes with strides.
                for (int x = 0;  x < width;  ++x) {
                    memcpy (t, f, pixelsize);
                    f += src_xstride;
                    t += dst_xstride;
                }
            }
        }
    }
    return true;
}

}
OIIO_NAMESPACE_EXIT
