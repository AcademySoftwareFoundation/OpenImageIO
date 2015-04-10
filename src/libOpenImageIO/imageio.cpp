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

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/fmath.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/hash.h"
#include "OpenImageIO/imageio.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_ENTER
{

// Global private data
namespace pvt {
recursive_mutex imageio_mutex;
atomic_int oiio_threads (boost::thread::hardware_concurrency());
atomic_int oiio_read_chunk (256);
ustring plugin_searchpath (OIIO_DEFAULT_PLUGIN_SEARCHPATH);
std::string format_list;   // comma-separated list of all formats
std::string extension_list;   // list of all extensions for all formats
}



using namespace pvt;

namespace {


// To avoid thread oddities, we have the storage area buffering error
// messages for seterror()/geterror() be thread-specific.
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




int
openimageio_version ()
{
    return OIIO_VERSION;
}



/// Error reporting for the plugin implementation: call this with
/// printf-like arguments.
void
pvt::seterror (const std::string& message)
{
    recursive_lock_guard lock (pvt::imageio_mutex);
    error_msg() = message;
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
attribute (string_view name, TypeDesc type, const void *val)
{
    if (name == "threads" && type == TypeDesc::TypeInt) {
        int ot = Imath::clamp (*(const int *)val, 0, maxthreads);
        if (ot == 0)
            ot = boost::thread::hardware_concurrency();
        oiio_threads = ot;
        return true;
    }
    spin_lock lock (attrib_mutex);
    if (name == "read_chunk" && type == TypeDesc::TypeInt) {
        oiio_read_chunk = *(const int *)val;
        return true;
    }
    if (name == "plugin_searchpath" && type == TypeDesc::TypeString) {
        plugin_searchpath = ustring (*(const char **)val);
        return true;
    }
    return false;
}



bool
getattribute (string_view name, TypeDesc type, void *val)
{
    if (name == "threads" && type == TypeDesc::TypeInt) {
        *(int *)val = oiio_threads;
        return true;
    }
    spin_lock lock (attrib_mutex);
    if (name == "read_chunk" && type == TypeDesc::TypeInt) {
        *(int *)val = oiio_read_chunk;
        return true;
    }
    if (name == "plugin_searchpath" && type == TypeDesc::TypeString) {
        *(ustring *)val = plugin_searchpath;
        return true;
    }
    if (name == "format_list" && type == TypeDesc::TypeString) {
        if (format_list.empty())
            pvt::catalog_all_plugins (plugin_searchpath.string());
        *(ustring *)val = ustring(format_list);
        return true;
    }
    if (name == "extension_list" && type == TypeDesc::TypeString) {
        if (extension_list.empty())
            pvt::catalog_all_plugins (plugin_searchpath.string());
        *(ustring *)val = ustring(extension_list);
        return true;
    }
    return false;
}


inline long long
quantize (float value, long long quant_min, long long quant_max)
{
    value = value * quant_max;
    return Imath::clamp ((long long)(value + 0.5f), quant_min, quant_max);
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
    if (xstride == nchannels*datasize) {
        // Optimize for contiguous scanlines, but not from scanline to scanline
        for (int z = 0;  z < depth;  ++z, src = (const T *)((char *)src + zstride)) {
            const T *scanline = src;
            for (int y = 0;  y < height;  ++y, dst += nchannels*width,
                 scanline = (const T *)((char *)scanline + ystride))
                memcpy(dst, scanline, xstride * width);
        }
    } else {
        for (int z = 0;  z < depth;  ++z, src = (const T *)((char *)src + zstride)) {
            const T *scanline = src;
            for (int y = 0;  y < height;  ++y, scanline = (const T *)((char *)scanline + ystride)) {
                const T *pixel = scanline;
                for (int x = 0;  x < width;  ++x, pixel = (const T *)((char *)pixel + xstride))
                    for (int c = 0;  c < nchannels;  ++c)
                        *dst++ = pixel[c];
            }
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
    case TypeDesc::DOUBLE :
        return _contiguize ((const double *)src, nchannels, 
                            xstride, ystride, zstride,
                            (double *)dst, width, height, depth);
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
    case TypeDesc::UINT8 :
        convert_type ((const unsigned char *)src, dst, nvals);
        break;
    case TypeDesc::HALF :
        convert_type ((const half *)src, dst, nvals);
        break;
    case TypeDesc::UINT16 :
        convert_type ((const unsigned short *)src, dst, nvals);
        break;
    case TypeDesc::INT8:
        convert_type ((const char *)src, dst, nvals);
        break;
    case TypeDesc::INT16 :
        convert_type ((const short *)src, dst, nvals);
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
    case TypeDesc::DOUBLE :
        convert_type ((const double *)src, dst, nvals);
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
             long long quant_min, long long quant_max)
{
    if (! src) {
        // If no source pixels, assume zeroes
        T z = T(0);
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = z;
    } else if (std::numeric_limits <T>::is_integer) {
        // Convert float to non-float native format, with quantization
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = (T) quantize (src[p], quant_min, quant_max);
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
                         long long quant_min, long long quant_max, TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::FLOAT :
        return src;
    case TypeDesc::HALF :
        return _from_float<half> (src, (half *)dst, nvals,
                                  quant_min, quant_max);
    case TypeDesc::DOUBLE :
        return _from_float (src, (double *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::INT8:
        return _from_float (src, (char *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::UINT8 :
        return _from_float (src, (unsigned char *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::INT16 :
        return _from_float (src, (short *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::UINT16 :
        return _from_float (src, (unsigned short *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::INT :
        return _from_float (src, (int *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::UINT :
        return _from_float (src, (unsigned int *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::INT64 :
        return _from_float (src, (long long *)dst, nvals,
                            quant_min, quant_max);
    case TypeDesc::UINT64 :
        return _from_float (src, (unsigned long long *)dst, nvals,
                            quant_min, quant_max);
    default:
        ASSERT (0 && "ERROR from_float: bad format");
        return NULL;
    }
}


// DEPRECATED (1.4)
const void *
pvt::convert_from_float (const float *src, void *dst, size_t nvals,
                         long long quant_black, long long quant_white,
                         long long quant_min, long long quant_max,
                         TypeDesc format)
{
    return convert_from_float (src, dst, nvals, quant_min, quant_max, format);
}



const void *
pvt::parallel_convert_from_float (const float *src, void *dst, size_t nvals,
                                  TypeDesc format, int nthreads)
{
    if (format.basetype == TypeDesc::FLOAT)
        return src;

    const size_t quanta = 30000;
    if (nvals < quanta)
        nthreads = 1;

    if (nthreads <= 0)
        nthreads = oiio_threads;

    long long quant_min, quant_max;
    get_default_quantize (format, quant_min, quant_max);

    if (nthreads <= 1)
        return convert_from_float (src, dst, nvals, quant_min, quant_max, format);

    boost::thread_group threads;
    size_t blocksize = std::max (quanta, size_t((nvals + nthreads - 1) / nthreads));
    for (size_t i = 0;  i < size_t(nthreads);  i++) {
        size_t begin = i * blocksize;
        if (begin >= nvals)
            break;  // no more work to divvy up
        size_t end = std::min (begin + blocksize, nvals);
        threads.add_thread (new boost::thread (
                                boost::bind (convert_from_float, src+begin,
                                             (char *)dst+begin*format.size(),
                                             end-begin, quant_min, quant_max, format)));
    }
    threads.join_all ();
    return dst;
}



// DEPRECATED (1.4)
const void *
pvt::parallel_convert_from_float (const float *src, void *dst,
                                  size_t nvals,
                                  long long quant_black, long long quant_white,
                                  long long quant_min, long long quant_max,
                                  TypeDesc format, int nthreads)
{
    return parallel_convert_from_float (src, dst, nvals, format, nthreads);
}



bool
convert_types (TypeDesc src_type, const void *src, 
               TypeDesc dst_type, void *dst, int n)
{
    // If no conversion is necessary, just memcpy
    if ((src_type == dst_type || dst_type.basetype == TypeDesc::UNKNOWN)) {
        memcpy (dst, src, n * src_type.size());
        return true;
    }

    if (dst_type == TypeDesc::TypeFloat) {
        // Special case -- converting non-float to float
        pvt::convert_to_float (src, (float *)dst, n, src_type);
        return true;
    }

    // Conversion is to a non-float type

    boost::scoped_array<float> tmp;   // In case we need a lot of temp space
    float *buf = (float *)src;
    if (src_type != TypeDesc::TypeFloat) {
        // If src is also not float, convert through an intermediate buffer
        if (n <= 4096)  // If < 16k, use the stack
            buf = ALLOCA (float, n);
        else {
            tmp.reset (new float[n]);  // Freed when tmp exists its scope
            buf = tmp.get();
        }
        pvt::convert_to_float (src, buf, n, src_type);
    }

    // Convert float to 'dst_type'
    switch (dst_type.basetype) {
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



// Deprecated version -- keep for link compatibiity
bool
convert_types (TypeDesc src_type, const void *src, 
               TypeDesc dst_type, void *dst, int n,
               int alpha_channel, int z_channel)
{
    return convert_types (src_type, src, dst_type, dst, n);
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



namespace {
// This nonsense is just to get around the 10-arg limits of boost::bind
// for compilers that don't have variadic templates.
struct convert_image_wrapper {
    convert_image_wrapper (int nchannels, int width, int height, int depth,
              const void *src, TypeDesc src_type,
              stride_t src_xstride, stride_t src_ystride, stride_t src_zstride,
              void *dst, TypeDesc dst_type,
              stride_t dst_xstride, stride_t dst_ystride, stride_t dst_zstride,
              int alpha_channel, int z_channel)
        : nchannels(nchannels), width(width), height(height), depth(depth),
          src(src), src_type(src_type), src_xstride(src_xstride),
          src_ystride(src_ystride), src_zstride(src_zstride), dst(dst),
          dst_type(dst_type), dst_xstride(dst_xstride),
          dst_ystride(dst_ystride), dst_zstride(dst_zstride),
          alpha_channel(alpha_channel), z_channel(z_channel)
    { }
        
    void operator() () {
        convert_image (nchannels, width, height, depth,
                       src, src_type, src_xstride, src_ystride, src_zstride, 
                       dst, dst_type, dst_xstride, dst_ystride, dst_zstride,
                       alpha_channel, z_channel);
    }
private:
    int nchannels, width, height, depth;
    const void *src;
    TypeDesc src_type;
    stride_t src_xstride, src_ystride, src_zstride;
    void *dst;
    TypeDesc dst_type;
    stride_t dst_xstride, dst_ystride, dst_zstride;
    int alpha_channel, z_channel;
};
}  // anon namespace



bool
parallel_convert_image (int nchannels, int width, int height, int depth,
               const void *src, TypeDesc src_type,
               stride_t src_xstride, stride_t src_ystride,
               stride_t src_zstride,
               void *dst, TypeDesc dst_type,
               stride_t dst_xstride, stride_t dst_ystride,
               stride_t dst_zstride,
               int alpha_channel, int z_channel, int nthreads)
{
    if (imagesize_t(width)*height*depth*nchannels < 30000)
        nthreads = 1;

    if (nthreads <= 0)
        nthreads = oiio_threads;

    if (nthreads <= 1)
        return convert_image (nchannels, width, height, depth,
                        src, src_type, src_xstride, src_ystride, src_zstride, 
                        dst, dst_type, dst_xstride, dst_ystride, dst_zstride,
                        alpha_channel, z_channel);

    ImageSpec::auto_stride (src_xstride, src_ystride, src_zstride,
                            src_type, nchannels, width, height);
    ImageSpec::auto_stride (dst_xstride, dst_ystride, dst_zstride,
                            dst_type, nchannels, width, height);

    boost::thread_group threads;
    int blocksize = std::max (1, (height + nthreads - 1) / nthreads);
    for (int i = 0;  i < nthreads;  i++) {
        int ybegin = i * blocksize;
        if (ybegin >= height)
            break;  // no more work to divvy up
        int yend = std::min (ybegin + blocksize, height);
        convert_image_wrapper ciw (nchannels, width, yend-ybegin, depth,
                                   (const char *)src+src_ystride*ybegin,
                                   src_type, src_xstride, src_ystride, src_zstride,
                                   (char *)dst+dst_ystride*ybegin,
                                   dst_type, dst_xstride, dst_ystride, dst_zstride,
                                   alpha_channel, z_channel);
        threads.add_thread (new boost::thread (ciw));
    }
    threads.join_all ();
    return true;
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



void
add_dither (int nchannels, int width, int height, int depth,
            float *data, stride_t xstride, stride_t ystride, stride_t zstride,
            float ditheramplitude,
            int alpha_channel, int z_channel, unsigned int ditherseed,
            int chorigin, int xorigin, int yorigin, int zorigin)
{
    ImageSpec::auto_stride (xstride, ystride, zstride,
                            sizeof(float), nchannels, width, height);
    char *plane = (char *)data;
    for (int z = 0;  z < depth;  ++z, plane += zstride) {
        char *scanline = plane;
        for (int y = 0;  y < height;  ++y, scanline += ystride) {
            char *pixel = scanline;
            uint32_t ba = (z+zorigin)*1311 + yorigin+y;
            uint32_t bb = ditherseed + (chorigin<<24);
            uint32_t bc = xorigin;
            for (int x = 0;  x < width;  ++x, pixel += xstride) {
                float *val = (float *)pixel;
                for (int c = 0;  c < nchannels;  ++c, ++val, ++bc) {
                    bjhash::bjmix (ba, bb, bc);
                    int channel = c+chorigin;
                    if (channel == alpha_channel || channel == z_channel)
                        continue;
                    float dither = bc / float(std::numeric_limits<uint32_t>::max());
                    *val += ditheramplitude * (dither - 0.5f);
                }
            }
        }
    }
}



template<typename T>
static void
premult_impl (int nchannels, int width, int height, int depth,
              int chbegin, int chend,
              T *data, stride_t xstride, stride_t ystride, stride_t zstride,
              int alpha_channel, int z_channel)
{
    char *plane = (char *)data;
    for (int z = 0;  z < depth;  ++z, plane += zstride) {
        char *scanline = plane;
        for (int y = 0;  y < height;  ++y, scanline += ystride) {
            char *pixel = scanline;
            for (int x = 0;  x < width;  ++x, pixel += xstride) {
                DataArrayProxy<T,float> val ((T*)pixel);
                float alpha = val[alpha_channel];
                for (int c = chbegin;  c < chend;  ++c) {
                    if (c == alpha_channel || c == z_channel)
                        continue;
                    val[c] = alpha * val[c];
                }
            }
        }
    }
}



void
premult (int nchannels, int width, int height, int depth,
         int chbegin, int chend,
         TypeDesc datatype, void *data,
         stride_t xstride, stride_t ystride, stride_t zstride,
         int alpha_channel, int z_channel)
{
    if (alpha_channel < 0 || alpha_channel > nchannels)
        return;  // nothing to do
    ImageSpec::auto_stride (xstride, ystride, zstride,
                            datatype.size(), nchannels, width, height);
    switch (datatype.basetype) {
    case TypeDesc::FLOAT :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (float*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::UINT8 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (unsigned char*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::UINT16 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (unsigned short*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::HALF :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (half*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::INT8 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (char*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::INT16 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (short*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::INT :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (int*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::UINT :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (unsigned int*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::INT64 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (int64_t*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::UINT64 :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (uint64_t*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    case TypeDesc::DOUBLE :
        premult_impl (nchannels, width, height, depth, chbegin, chend,
                      (double*)data, xstride, ystride, zstride,
                      alpha_channel, z_channel);
        break;
    default: break;
    }
}



void
DeepData::init (int npix, int nchan,
                const TypeDesc *chbegin, const TypeDesc *chend)
{
    clear ();
    npixels = npix;
    nchannels = nchan;
    channeltypes.assign (chbegin, chend);
    nsamples.resize (npixels, 0);
    pointers.resize (size_t(npixels)*size_t(nchannels), NULL);
}



void
DeepData::alloc ()
{
    // Calculate the total size we need, align each channel to 4 byte boundary
    size_t totalsamples = 0, totalbytes = 0;
    for (int i = 0;  i < npixels;  ++i) {
        if (int s = nsamples[i]) {
            totalsamples += s;
            for (int c = 0;  c < nchannels;  ++c)
                totalbytes += round_to_multiple (channeltypes[c].size() * s, 4);
        }
    }

    // Set all the data pointers to the right offsets within the
    // data block.  Leave the pointes NULL for pixels with no samples.
    data.resize (totalbytes);
    char *p = &data[0];
    for (int i = 0;  i < npixels;  ++i) {
        if (int s = nsamples[i]) {
            for (int c = 0;  c < nchannels;  ++c) {
                pointers[i*nchannels+c] = p;
                p += round_to_multiple (channeltypes[c].size()*s, 4);
            }
        }
    }
}



void
DeepData::clear ()
{
    npixels = 0;
    nchannels = 0;
    channeltypes.clear();
    nsamples.clear();
    pointers.clear();
    data.clear();
}



void
DeepData::free ()
{
    std::vector<unsigned int>().swap (nsamples);
    std::vector<void *>().swap (pointers);
    std::vector<char>().swap (data);
}



void *
DeepData::channel_ptr (int pixel, int channel) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return NULL;
    return pointers[pixel*nchannels + channel];
}



float
DeepData::deep_value (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return 0.0f;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return 0.0f;
    switch (channeltypes[channel].basetype) {
    case TypeDesc::FLOAT :
        return ((const float *)ptr)[sample];
    case TypeDesc::HALF  :
        return ((const half *)ptr)[sample];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,float>((const unsigned char *)ptr)[sample];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,float>((const char *)ptr)[sample];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,float>((const unsigned short *)ptr)[sample];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,float>((const short *)ptr)[sample];
    case TypeDesc::UINT  :
        return ConstDataArrayProxy<unsigned int,float>((const unsigned int *)ptr)[sample];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,float>((const int *)ptr)[sample];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,float>((const unsigned long long *)ptr)[sample];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,float>((const long long *)ptr)[sample];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



uint32_t
DeepData::deep_value_uint (int pixel, int channel, int sample) const
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return 0.0f;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return 0.0f;
    const void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return 0.0f;
    switch (channeltypes[channel].basetype) {
    case TypeDesc::FLOAT :
        return ConstDataArrayProxy<float,uint32_t>((const float *)ptr)[sample];
    case TypeDesc::HALF  :
        return ConstDataArrayProxy<half,uint32_t>((const half *)ptr)[sample];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,uint32_t>((const unsigned char *)ptr)[sample];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,uint32_t>((const char *)ptr)[sample];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,uint32_t>((const unsigned short *)ptr)[sample];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,uint32_t>((const short *)ptr)[sample];
    case TypeDesc::UINT  :
        return ((const unsigned int *)ptr)[sample];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,uint32_t>((const int *)ptr)[sample];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,uint32_t>((const unsigned long long *)ptr)[sample];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,uint32_t>((const long long *)ptr)[sample];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



void
DeepData::set_deep_value (int pixel, int channel, int sample, float value)
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return;
    switch (channeltypes[channel].basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,float>((float *)ptr)[sample] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,float>((half *)ptr)[sample] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,float>((unsigned char *)ptr)[sample] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,float>((char *)ptr)[sample] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,float>((unsigned short *)ptr)[sample] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,float>((short *)ptr)[sample] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,float>((uint32_t *)ptr)[sample] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,float>((int *)ptr)[sample] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,float>((uint64_t *)ptr)[sample] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,float>((int64_t *)ptr)[sample] = value; break;
    default:
        ASSERT (0);
    }
}



void
DeepData::set_deep_value_uint (int pixel, int channel, int sample, uint32_t value)
{
    if (pixel < 0 || pixel >= npixels || channel < 0 || channel >= nchannels)
        return;
    int nsamps = nsamples[pixel];
    if (nsamps == 0 || sample < 0 || sample >= nsamps)
        return;
    void *ptr = pointers[pixel*nchannels + channel];
    if (! ptr)
        return;
    switch (channeltypes[channel].basetype) {
    case TypeDesc::FLOAT :
        DataArrayProxy<float,uint32_t>((float *)ptr)[sample] = value; break;
    case TypeDesc::HALF  :
        DataArrayProxy<half,uint32_t>((half *)ptr)[sample] = value; break;
    case TypeDesc::UINT8 :
        DataArrayProxy<unsigned char,uint32_t>((unsigned char *)ptr)[sample] = value; break;
    case TypeDesc::INT8  :
        DataArrayProxy<char,uint32_t>((char *)ptr)[sample] = value; break;
    case TypeDesc::UINT16:
        DataArrayProxy<unsigned short,uint32_t>((unsigned short *)ptr)[sample] = value; break;
    case TypeDesc::INT16 :
        DataArrayProxy<short,uint32_t>((short *)ptr)[sample] = value; break;
    case TypeDesc::UINT  :
        DataArrayProxy<uint32_t,uint32_t>((uint32_t *)ptr)[sample] = value; break;
    case TypeDesc::INT   :
        DataArrayProxy<int,uint32_t>((int *)ptr)[sample] = value; break;
    case TypeDesc::UINT64:
        DataArrayProxy<uint64_t,uint32_t>((uint64_t *)ptr)[sample] = value; break;
    case TypeDesc::INT64 :
        DataArrayProxy<int64_t,uint32_t>((int64_t *)ptr)[sample] = value; break;
    default:
        ASSERT (0);
    }
}



bool
wrap_black (int &coord, int origin, int width)
{
    return (coord >= origin && coord < (width+origin));
}


bool
wrap_clamp (int &coord, int origin, int width)
{
    if (coord < origin)
        coord = origin;
    else if (coord >= origin+width)
        coord = origin+width-1;
    return true;
}


bool
wrap_periodic (int &coord, int origin, int width)
{
    coord -= origin;
    coord %= width;
    if (coord < 0)       // Fix negative values
        coord += width;
    coord += origin;
    return true;
}


bool
wrap_periodic_pow2 (int &coord, int origin, int width)
{
    DASSERT (ispow2(width));
    coord -= origin;
    coord &= (width - 1); // Shortcut periodic if we're sure it's a pow of 2
    coord += origin;
    return true;
}


bool
wrap_mirror (int &coord, int origin, int width)
{
    coord -= origin;
    if (coord < 0)
        coord = -coord - 1;
    int iter = coord / width;    // Which iteration of the pattern?
    coord -= iter * width;
    if (iter & 1)  // Odd iterations -- flip the sense
        coord = width - 1 - coord;
    DASSERT_MSG (coord >= 0 && coord < width,
                 "width=%d, origin=%d, result=%d", width, origin, coord);
    coord += origin;
    return true;
}


}
OIIO_NAMESPACE_EXIT
