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
/// Implementation of ImageBufAlgo algorithms that analize or compare
/// images.

/* This header has to be included before boost/regex.hpp header
   If it is included after, there is an error
   "undefined reference to CSHA1::Update (unsigned char const*, unsigned long)"
*/
#include "SHA1.h"

#include <boost/bind.hpp>

#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
#include "dassert.h"
#include "thread.h"

#ifdef USE_OPENSSL
#ifdef __APPLE__
// Newer OSX releaes mark OpenSSL functions as deprecated, in favor of
// CDSA.  Make the warnings stop.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" 
#endif
#include <openssl/sha.h>
#endif



OIIO_NAMESPACE_ENTER
{


inline void
reset (ImageBufAlgo::PixelStats &p, int nchannels)
{
    const float inf = std::numeric_limits<float>::infinity();
    p.min.clear ();          p.min.resize (nchannels, inf);
    p.max.clear ();          p.max.resize (nchannels, -inf);
    p.avg.clear ();          p.avg.resize (nchannels);
    p.stddev.clear ();       p.stddev.resize (nchannels);
    p.nancount.clear ();     p.nancount.resize (nchannels, 0);
    p.infcount.clear ();     p.infcount.resize (nchannels, 0);
    p.finitecount.clear ();  p.finitecount.resize (nchannels, 0);
    p.sum.clear ();          p.sum.resize (nchannels, 0.0);
    p.sum2.clear ();         p.sum2.resize (nchannels, 0.0);
}


inline void
merge (ImageBufAlgo::PixelStats &sum, const ImageBufAlgo::PixelStats &p)
{
    ASSERT (sum.min.size() == p.min.size());
    for (size_t c = 0, e = sum.min.size(); c < e;  ++c) {
        sum.min[c] = std::min (sum.min[c], p.min[c]);
        sum.max[c] = std::max (sum.max[c], p.max[c]);
        sum.nancount[c] += p.nancount[c];
        sum.infcount[c] += p.infcount[c];
        sum.finitecount[c] += p.finitecount[c];
        sum.sum[c] += p.sum[c];
        sum.sum2[c] += p.sum2[c];
    }
}


inline void
val (ImageBufAlgo::PixelStats &p, int c, float value)
{
    if (isnan (value)) {
        ++p.nancount[c];
        return;
    }
    if (isinf (value)) {
        ++p.infcount[c];
        return;
    }
    ++p.finitecount[c];
    p.sum[c] += value;
    p.sum2[c] += value*value;
    p.min[c] = std::min (value, p.min[c]);
    p.max[c] = std::max (value, p.max[c]);
}



inline void
finalize (ImageBufAlgo::PixelStats &p)
{
    for (size_t c = 0, e = p.min.size();  c < e;  ++c) {
        if (p.finitecount[c] == 0) {
            p.min[c] = 0.0;
            p.max[c] = 0.0;
            p.avg[c] = 0.0;
            p.stddev[c] = 0.0;
        } else {
            double Count = static_cast<double>(p.finitecount[c]);
            double davg = p.sum[c] / Count;
            p.avg[c] = static_cast<float>(davg);
            p.stddev[c] = static_cast<float>(sqrt(p.sum2[c]/Count - davg*davg));
        }
    }
}



template <class T>
static bool
computePixelStats_ (const ImageBuf &src, ImageBufAlgo::PixelStats &stats,
                    ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());
    else
        roi.chend = std::min (roi.chend, src.nchannels());

    int nchannels = src.spec().nchannels;

    // Use local storage for smaller batches, then merge the batches
    // into the final results.  This preserves precision for large
    // images, where the running total may be too big to incorporate the
    // contributions of individual pixel values without losing
    // precision.
    //
    // This approach works best when the batch size is the sqrt of
    // numpixels, which makes the num batches roughly equal to the
    // number of pixels / batch.
    ImageBufAlgo::PixelStats tmp;
    reset (tmp, nchannels);
    reset (stats, nchannels);
    
    int PIXELS_PER_BATCH = std::max (1024,
            static_cast<int>(sqrt((double)src.spec().image_pixels())));
    
    if (src.deep()) {
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<T> s(src, roi); ! s.done();  ++s) {
            int samples = s.deep_samples();
            if (! samples)
                continue;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                for (int i = 0;  i < samples;  ++i) {
                    float value = s.deep_value (c, i);
                    val (tmp, c, value);
                    if ((tmp.finitecount[c] % PIXELS_PER_BATCH) == 0) {
                        merge (stats, tmp);
                        reset (tmp, nchannels);
                    }
                }
            }
        }
    } else {  // Non-deep case
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<T> s(src, roi); ! s.done();  ++s) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                float value = s[c];
                val (tmp, c, value);
                if ((tmp.finitecount[c] % PIXELS_PER_BATCH) == 0) {
                    merge (stats, tmp);
                    reset (tmp, nchannels);
                }
            }
        }
    }

    // Merge anything left over
    merge (stats, tmp);

    // Compute final results
    finalize (stats);
    
    return true;
};



bool
ImageBufAlgo::computePixelStats (PixelStats &stats, const ImageBuf &src,
                                 ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());
    else
        roi.chend = std::min (roi.chend, src.nchannels());
    int nchannels = src.spec().nchannels;
    if (nchannels == 0) {
        src.error ("%d-channel images not supported", nchannels);
        return false;
    }

    OIIO_DISPATCH_TYPES ("computePixelStats", computePixelStats_,
                         src.spec().format, src, stats, roi, nthreads);
    return false;
}



template<class BUFT>
inline void
compare_value (ImageBuf::ConstIterator<BUFT,float> &a, int chan,
               float aval, float bval, ImageBufAlgo::CompareResults &result,
               float &maxval, double &batcherror, double &batch_sqrerror,
               bool &failed, bool &warned, float failthresh, float warnthresh)
{
    maxval = std::max (maxval, std::max (aval, bval));
    double f = fabs (aval - bval);
    batcherror += f;
    batch_sqrerror += f*f;
    if (f > result.maxerror) {
        result.maxerror = f;
        result.maxx = a.x();
        result.maxy = a.y();
        result.maxz = a.z();
        result.maxc = chan;
    }
    if (! warned && f > warnthresh) {
        ++result.nwarn;
        warned = true;
    }
    if (! failed && f > failthresh) {
        ++result.nfail;
        failed = true;
    }
}



template <class Atype, class Btype>
static bool
compare_ (const ImageBuf &A, const ImageBuf &B,
          float failthresh, float warnthresh,
          ImageBufAlgo::CompareResults &result,
          ROI roi, int nthreads)
{
    imagesize_t npels = roi.npixels();
    imagesize_t nvals = npels * roi.nchannels();
    int Achannels = A.nchannels(), Bchannels = B.nchannels();

    // Compare the two images.
    //
    double totalerror = 0;
    double totalsqrerror = 0;
    result.maxerror = 0;
    result.maxx=0, result.maxy=0, result.maxz=0, result.maxc=0;
    result.nfail = 0, result.nwarn = 0;
    float maxval = 1.0;  // max possible value

    ImageBuf::ConstIterator<Atype> a (A, roi, ImageBuf::WrapBlack);
    ImageBuf::ConstIterator<Btype> b (B, roi, ImageBuf::WrapBlack);
    bool deep = A.deep();
    // Break up into batches to reduce cancelation errors as the error
    // sums become too much larger than the error for individual pixels.
    const int batchsize = 4096;   // As good a guess as any
    for ( ;  ! a.done();  ) {
        double batcherror = 0;
        double batch_sqrerror = 0;
        if (deep) {
            for (int i = 0;  i < batchsize && !a.done();  ++i, ++a, ++b) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    for (int s = 0, e = a.deep_samples(); s < e;  ++s) {
                        compare_value (a, c, a.deep_value(c,s),
                                       b.deep_value(c,s), result, maxval,
                                       batcherror, batch_sqrerror,
                                       failed, warned, failthresh, warnthresh);
                    }
            }
        } else {  // non-deep
            for (int i = 0;  i < batchsize && !a.done();  ++i, ++a, ++b) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    compare_value (a, c, c < Achannels ? a[c] : 0.0f,
                                   c < Bchannels ? b[c] : 0.0f,
                                   result, maxval, batcherror, batch_sqrerror,
                                   failed, warned, failthresh, warnthresh);
            }
        }
        totalerror += batcherror;
        totalsqrerror += batch_sqrerror;
    }
    result.meanerror = totalerror / nvals;
    result.rms_error = sqrt (totalsqrerror / nvals);
    result.PSNR = 20.0 * log10 (maxval / result.rms_error);
    return result.nfail == 0;
}



bool
ImageBufAlgo::compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       ImageBufAlgo::CompareResults &result,
                       ROI roi, int nthreads)
{
    // If no ROI is defined, use the union of the data windows of the two
    // images.
    if (! roi.defined())
        roi = roi_union (get_roi(A.spec()), get_roi(B.spec()));
    roi.chend = std::min (roi.chend, std::max(A.nchannels(), B.nchannels()));

    // Deep and non-deep images cannot be compared
    if (B.deep() != A.deep())
        return false;

    OIIO_DISPATCH_TYPES2 ("compare", compare_,
                          A.spec().format, B.spec().format,
                          A, B, failthresh, warnthresh, result,
                          roi, nthreads);
    // FIXME - The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
    return false;
}



template<typename T>
static inline bool
isConstantColor_ (const ImageBuf &src, float *color,
                  ROI roi, int nthreads)
{
    // Iterate using the native typing (for speed).
    std::vector<T> constval (roi.nchannels());
    ImageBuf::ConstIterator<T,T> s (src, roi);
    for (int c = roi.chbegin;  c < roi.chend;  ++c)
        constval[c] = s[c];

    // Loop over all pixels ...
    for ( ; ! s.done();  ++s) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (constval[c] != s[c])
                return false;
    }
    
    if (color) {
        ImageBuf::ConstIterator<T,float> s (src, roi);
        for (int c = 0;  c < roi.chbegin; ++c)
            color[c] = 0.0f;
        for (int c = roi.chbegin; c < roi.chend; ++c)
            color[c] = s[c];
        for (int c = roi.chend;  c < src.nchannels(); ++c)
            color[c] = 0.0f;
    }

    return true;
}



bool
ImageBufAlgo::isConstantColor (const ImageBuf &src, float *color,
                               ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    if (roi.nchannels() == 0)
        return true;
    
    OIIO_DISPATCH_TYPES ("isConstantColor", isConstantColor_,
                         src.spec().format, src, color, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
};



template<typename T>
static inline bool
isConstantChannel_ (const ImageBuf &src, int channel, float val,
                    ROI roi, int nthreads)
{

    T v = convert_type<float,T> (val);
    for (ImageBuf::ConstIterator<T,T> s(src, roi);  !s.done();  ++s)
        if (s[channel] != v)
            return false;
    return true;
}


bool
ImageBufAlgo::isConstantChannel (const ImageBuf &src, int channel, float val,
                                 ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());

    if (channel < 0 || channel >= src.nchannels())
        return false;  // that channel doesn't exist in the image

    OIIO_DISPATCH_TYPES ("isConstantChannel", isConstantChannel_,
                         src.spec().format, src, channel, val, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
};



template<typename T>
static inline bool
isMonochrome_ (const ImageBuf &src, ROI roi, int nthreads)
{
    int nchannels = src.nchannels();
    if (nchannels < 2) return true;
    
    // Loop over all pixels ...
    for (ImageBuf::ConstIterator<T,T> s(src, roi);  ! s.done();  ++s) {
        T constvalue = s[roi.chbegin];
        for (int c = roi.chbegin+1;  c < roi.chend;  ++c)
            if (s[c] != constvalue)
                return false;
    }
    return true;
}



bool
ImageBufAlgo::isMonochrome (const ImageBuf &src, ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());
    if (roi.nchannels() < 2)
        return true;  // 1 or fewer channels are always "monochrome"

    OIIO_DISPATCH_TYPES ("isMonochrome", isMonochrome_, src.spec().format,
                         src, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
};



template<typename T>
static bool
color_count_ (const ImageBuf &src, atomic_ll *count,
              int ncolors, const float *color, const float *eps,
              ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(color_count_<T>, boost::ref(src),
                        count, ncolors, color, eps,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    int nchannels = src.nchannels();
    long long *n = ALLOCA (long long, ncolors);
    for (int col = 0;  col < ncolors;  ++col)
        n[col] = 0;
    for (ImageBuf::ConstIterator<T> p (src, roi);  !p.done();  ++p) {
        int coloffset = 0;
        for (int col = 0;  col < ncolors;  ++col, coloffset += nchannels) {
            int match = 1;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                if (fabsf(p[c] - color[coloffset+c]) > eps[c]) {
                    match = 0;
                    break;
                }
            }
            n[col] += match;
        }
    }

    for (int col = 0;  col < ncolors;  ++col)
        count[col] += n[col];
    return true;
}



bool
ImageBufAlgo::color_count (const ImageBuf &src, imagesize_t *count,
                           int ncolors, const float *color,
                           const float *eps,
                           ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    if (! eps) {
        float *localeps = ALLOCA (float, roi.chend);
        for (int c = 0;  c < roi.chend;  ++c)
            localeps[c] = 0.001f;
        eps = localeps;
    }

    for (int col = 0;  col < ncolors;  ++col)
        count[col] = 0;
    OIIO_DISPATCH_TYPES ("color_count", color_count_, src.spec().format,
                         src, (atomic_ll *)count, ncolors, color, eps,
                         roi, nthreads);
}



template<typename T>
static bool
color_range_check_ (const ImageBuf &src, atomic_ll *lowcount,
                    atomic_ll *highcount, atomic_ll *inrangecount,
                    const float *low, const float *high,
                    ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(color_range_check_<T>, boost::ref(src),
                        lowcount, highcount, inrangecount, low, high,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    long long lc = 0, hc = 0, inrange = 0;
    for (ImageBuf::ConstIterator<T> p (src, roi);  !p.done();  ++p) {
        bool lowval = false, highval = false;
        for (int c = roi.chbegin;  c < roi.chend;  ++c) {
            float f = p[c];
            lowval |= (f < low[c]);
            highval |= (f > high[c]);
        }
        if (lowval)
            ++lc;
        if (highval)
            ++hc;
        if (!lowval && !highval)
            ++inrange;
    }

    if (lowcount)
        *lowcount += lc;
    if (highcount)
        *highcount += hc;
    if (inrangecount)
        *inrangecount += inrange;
    return true;
}



bool
ImageBufAlgo::color_range_check (const ImageBuf &src, imagesize_t *lowcount,
                                 imagesize_t *highcount,
                                 imagesize_t *inrangecount,
                                 const float *low, const float *high,
                                 ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    if (lowcount)
        *lowcount = 0;
    if (highcount)
        *highcount = 0;
    if (inrangecount)
        *inrangecount = 0;
    OIIO_DISPATCH_TYPES ("color_range_check", color_range_check_,
                         src.spec().format,
                         src, (atomic_ll *)lowcount, 
                         (atomic_ll *)highcount, (atomic_ll *)inrangecount,
                         low, high, roi, nthreads);
}



namespace {

std::string
simplePixelHashSHA1 (const ImageBuf &src,
                     const std::string & extrainfo, ROI roi)
{
    if (! roi.defined())
        roi = get_roi (src.spec());

    bool localpixels = src.localpixels();
    imagesize_t scanline_bytes = roi.width() * src.spec().pixel_bytes();
    ASSERT (scanline_bytes < std::numeric_limits<unsigned int>::max());
    // Do it a few scanlines at a time
    int chunk = std::max (1, int(16*1024*1024/scanline_bytes));

    std::vector<unsigned char> tmp;
    if (! localpixels)
        tmp.resize (chunk*scanline_bytes);

#ifdef USE_OPENSSL
    // If OpenSSL was available at build time, use its SHA-1
    // implementation, which is about 20% faster than CSHA1.
    SHA_CTX sha;
    SHA1_Init (&sha);

    for (int z = roi.zbegin, zend=roi.zend;  z < zend;  ++z) {
        for (int y = roi.ybegin, yend=roi.yend;  y < yend;  y += chunk) {
            int y1 = std::min (y+chunk, yend);
            if (localpixels) {
                SHA1_Update (&sha, src.pixeladdr (roi.xbegin, y, z),
                            (unsigned int) scanline_bytes*(y1-y));
            } else {
                src.get_pixels (roi.xbegin, roi.xend, y, y1, z, z+1,
                                src.spec().format, &tmp[0]);
                SHA1_Update (&sha, &tmp[0], (unsigned int) scanline_bytes*(y1-y));
            }
        }
    }
    
    // If extra info is specified, also include it in the sha computation
    if (!extrainfo.empty())
        SHA1_Update (&sha, extrainfo.c_str(), extrainfo.size());

    unsigned char md[SHA_DIGEST_LENGTH];
    char hash_digest[2*SHA_DIGEST_LENGTH+1];
    SHA1_Final (md, &sha);
    for (int i = 0;  i < SHA_DIGEST_LENGTH;  ++i)
        sprintf (hash_digest+2*i, "%02X", (int)md[i]);
    hash_digest[2*SHA_DIGEST_LENGTH] = 0;
    return std::string (hash_digest);
    
#else
    // Fall back on CSHA1 if OpenSSL was not available or if 
    CSHA1 sha;
    sha.Reset ();
    
    for (int z = roi.zbegin, zend=roi.zend;  z < zend;  ++z) {
        for (int y = roi.ybegin, yend=roi.yend;  y < yend;  y += chunk) {
            int y1 = std::min (y+chunk, yend);
            if (localpixels) {
                sha.Update ((const unsigned char *)src.pixeladdr (roi.xbegin, y, z),
                            (unsigned int) scanline_bytes*(y1-y));
            } else {
                src.get_pixels (roi.xbegin, roi.xend, y, y1, z, z+1,
                                src.spec().format, &tmp[0]);
                sha.Update (&tmp[0], (unsigned int) scanline_bytes*(y1-y));
            }
        }
    }
    
    // If extra info is specified, also include it in the sha computation
    if (!extrainfo.empty()) {
        sha.Update ((const unsigned char*) extrainfo.c_str(), extrainfo.size());
    }
    
    sha.Final ();
    std::string hash_digest;
    sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);

    return hash_digest;
#endif
}



// Wrapper to single-threadedly SHA1 hash a region in blocks and store
// the results in a designated place.
static void
sha1_hasher (const ImageBuf *src, ROI roi, int blocksize,
             std::string *results, int firstresult)
{
    ROI broi = roi;
    for (int y = roi.ybegin; y < roi.yend; y += blocksize) {
        broi.ybegin = y;
        broi.yend = std::min (y+blocksize, roi.yend);
        std::string s = simplePixelHashSHA1 (*src, "", broi);
        results[firstresult++] = s;
    }
}

} // anon namespace



std::string
ImageBufAlgo::computePixelHashSHA1 (const ImageBuf &src,
                                    const std::string & extrainfo,
                                    ROI roi, int blocksize, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());

    // Fall back to whole-image hash for only one block
    if (blocksize <= 0 || blocksize >= roi.height())
        return simplePixelHashSHA1 (src, extrainfo, roi);

    // Request for 0 threads means "use the OIIO global thread count"
    if (nthreads <= 0)
        OIIO::getattribute ("threads", nthreads);

    int nblocks = (roi.height()+blocksize-1) / blocksize;
    std::vector<std::string> results (nblocks);
    if (nthreads <= 1) {
        sha1_hasher (&src, roi, blocksize, &results[0], 0);
    } else {
        // parallel case
        boost::thread_group threads;
        int blocks_per_thread = (nblocks+nthreads-1) / nthreads;
        ROI broi = roi;
        for (int b = 0, t = 0;  b < nblocks;  b += blocks_per_thread, ++t) {
            int y = roi.ybegin + b*blocksize;
            if (y >= roi.yend)
                break;
            broi.ybegin = y;
            broi.yend = std::min (y+blocksize*blocks_per_thread, roi.yend);
            threads.add_thread (new boost::thread (sha1_hasher, &src, broi,
                                                   blocksize, &results[0], b));
        }
        threads.join_all ();
    }

#ifdef USE_OPENSSL
    // If OpenSSL was available at build time, use its SHA-1
    // implementation, which is about 20% faster than CSHA1.
    SHA_CTX sha;
    SHA1_Init (&sha);
    for (int b = 0;  b < nblocks;  ++b)
        SHA1_Update (&sha, results[b].c_str(), results[b].size());
    if (extrainfo.size())
        SHA1_Update (&sha, extrainfo.c_str(), extrainfo.size());
    unsigned char md[SHA_DIGEST_LENGTH];
    char hash_digest[2*SHA_DIGEST_LENGTH+1];
    SHA1_Final (md, &sha);
    for (int i = 0;  i < SHA_DIGEST_LENGTH;  ++i)
        sprintf (hash_digest+2*i, "%02X", (int)md[i]);
    hash_digest[2*SHA_DIGEST_LENGTH] = 0;
    return std::string (hash_digest);
#else
    // Fall back on CSHA1 if OpenSSL was not available or if 
    CSHA1 sha;
    sha.Reset ();
    for (int b = 0;  b < nblocks;  ++b)
        sha.Update ((const unsigned char *)results[b].c_str(), results[b].size());
    if (extrainfo.size())
        sha.Update ((const unsigned char *)extrainfo.c_str(), extrainfo.size());
    sha.Final ();
    std::string hash_digest;
    sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);
    return hash_digest;
#endif
}




/// histogram_impl -----------------------------------------------------------
/// Fully type-specialized version of histogram.
///
/// Pixel values in min->max range are mapped to 0->(bins-1) range, so that
/// each value is placed in the appropriate bin. The formula used is:
/// y = (x-min) * bins/(max-min), where y is the value in the 0->(bins-1)
/// range and x is the value in the min->max range. There is one special
/// case x==max for which the formula is not used and x is assigned to the
/// last bin at position (bins-1) in the vector histogram.
/// --------------------------------------------------------------------------
template<class Atype>
static bool
histogram_impl (const ImageBuf &A, int channel,
                std::vector<imagesize_t> &histogram, int bins,
                float min, float max, imagesize_t *submin,
                imagesize_t *supermax, ROI roi)
{
    // Double check A's type.
    if (A.spec().format != BaseTypeFromC<Atype>::value) {
        A.error ("Unsupported pixel data format '%s'", A.spec().format);
        return false;
    }

    // Initialize.
    ImageBuf::ConstIterator<Atype, float> a (A, roi);
    float ratio = bins / (max-min);
    int bins_minus_1 = bins-1;
    bool submin_ok = submin != NULL;
    bool supermax_ok = supermax != NULL;
    if (submin_ok)
        *submin = 0;
    if (supermax_ok)
        *supermax = 0;
    histogram.assign(bins, 0);

    // Compute histogram.
    for ( ; ! a.done(); a++) {
        float c = a[channel];
        if (c >= min && c < max) {
            // Map range min->max to 0->(bins-1).
            histogram[ (int) ((c-min) * ratio) ]++;
        } else if (c == max) {
            histogram[bins_minus_1]++;
        } else {
            if (submin_ok && c < min)
                (*submin)++;
            else if (supermax_ok)
                (*supermax)++;
        }
    }
    return true;
}



bool
ImageBufAlgo::histogram (const ImageBuf &A, int channel,
                         std::vector<imagesize_t> &histogram, int bins,
                         float min, float max, imagesize_t *submin,
                         imagesize_t *supermax, ROI roi)
{
    if (A.spec().format != TypeDesc::TypeFloat) {
        A.error ("Unsupported pixel data format '%s'", A.spec().format);
        return false;
    }

    if (A.nchannels() == 0) {
        A.error ("Input image must have at least 1 channel");
        return false;
    }

    if (channel < 0 || channel >= A.nchannels()) {
        A.error ("Invalid channel %d for input image with channels 0 to %d",
                  channel, A.nchannels()-1);
        return false;
    }

    if (bins < 1) {
        A.error ("The number of bins must be at least 1");
        return false;
    }

    if (max <= min) {
        A.error ("Invalid range, min must be strictly smaller than max");
        return false;
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from A.
    if (! roi.defined())
        roi = get_roi (A.spec());

    histogram_impl<float> (A, channel, histogram, bins, min, max,
                                  submin, supermax, roi);

    return ! A.has_error();
}



bool
ImageBufAlgo::histogram_draw (ImageBuf &R,
                              const std::vector<imagesize_t> &histogram)
{
    // Fail if there are no bins to draw.
    int bins = histogram.size();
    if (bins == 0) {
        R.error ("There are no bins to draw, the histogram is empty");
        return false;
    }

    // Check R and modify it if needed.
    int height = R.spec().height;
    if (R.spec().format != TypeDesc::TypeFloat || R.nchannels() != 1 ||
        R.spec().width != bins) {
        ImageSpec newspec = ImageSpec (bins, height, 1, TypeDesc::FLOAT);
        R.reset ("dummy", newspec);
    }

    // Fill output image R with white color.
    ImageBuf::Iterator<float, float> r (R);
    for ( ; ! r.done(); ++r)
        r[0] = 1;

    // Draw histogram left->right, bottom->up.
    imagesize_t max = *std::max_element (histogram.begin(), histogram.end());
    for (int b = 0; b < bins; b++) {
        int bin_height = (int) ((float)histogram[b]/(float)max*height + 0.5f);
        if (bin_height != 0) {
            // Draw one bin at column b.
            for (int j = 1; j <= bin_height; j++) {
                int row = height - j;
                r.pos (b, row);
                r[0] = 0;
            }
        }
    }
    return true;
}



}
OIIO_NAMESPACE_EXIT
