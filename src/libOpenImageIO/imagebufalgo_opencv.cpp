/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
/// Implementation of ImageBufAlgo algorithms related to OpenCV.
/// These are nonfunctional if OpenCV is not found at build time.

#ifdef USE_OPENCV
#    include <opencv2/core/version.hpp>
#    ifdef CV_VERSION_EPOCH
#        define OIIO_OPENCV_VERSION                                            \
            (10000 * CV_VERSION_EPOCH + 100 * CV_VERSION_MAJOR                 \
             + CV_VERSION_MINOR)
#    else
#        define OIIO_OPENCV_VERSION                                            \
            (10000 * CV_VERSION_MAJOR + 100 * CV_VERSION_MINOR                 \
             + CV_VERSION_REVISION)
#    endif
#    include <opencv2/opencv.hpp>
#    if OIIO_OPENCV_VERSION >= 40000
#        include <opencv2/core/core_c.h>
#        include <opencv2/imgproc/imgproc_c.h>
#    endif
#endif

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

#include <OpenEXR/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

#include "imageio_pvt.h"

// using namespace cv;

OIIO_NAMESPACE_BEGIN



// Note: DEPRECATED(2.0)
ImageBuf
ImageBufAlgo::from_IplImage(const IplImage* ipl, TypeDesc convert)
{
    pvt::LoggedTimer logtime("IBA::from_IplImage");
    ImageBuf dst;
    if (!ipl) {
        dst.error("Passed NULL source IplImage");
        return dst;
    }
#ifdef USE_OPENCV
    TypeDesc srcformat;
    switch (ipl->depth) {
    case int(IPL_DEPTH_8U): srcformat = TypeDesc::UINT8; break;
    case int(IPL_DEPTH_8S): srcformat = TypeDesc::INT8; break;
    case int(IPL_DEPTH_16U): srcformat = TypeDesc::UINT16; break;
    case int(IPL_DEPTH_16S): srcformat = TypeDesc::INT16; break;
    case int(IPL_DEPTH_32F): srcformat = TypeDesc::FLOAT; break;
    case int(IPL_DEPTH_64F): srcformat = TypeDesc::DOUBLE; break;
    default:
        dst.error("Unsupported IplImage depth %d", (int)ipl->depth);
        return dst;
    }

    TypeDesc dstformat = (convert != TypeDesc::UNKNOWN) ? convert : srcformat;
    ImageSpec spec(ipl->width, ipl->height, ipl->nChannels, dstformat);
    // N.B. The OpenCV headers say that ipl->alphaChannel,
    // ipl->colorModel, and ipl->channelSeq are ignored by OpenCV.

    if (ipl->dataOrder != IPL_DATA_ORDER_PIXEL) {
        // We don't handle separate color channels, and OpenCV doesn't either
        dst.error("Unsupported IplImage data order %d", (int)ipl->dataOrder);
        return dst;
    }

    dst.reset(dst.name(), spec);
    size_t pixelsize = srcformat.size() * spec.nchannels;
    // Account for the origin in the line step size, to end up with the
    // standard OIIO origin-at-upper-left:
    size_t linestep = ipl->origin ? -ipl->widthStep : ipl->widthStep;
    // Block copy and convert
    convert_image(spec.nchannels, spec.width, spec.height, 1, ipl->imageData,
                  srcformat, pixelsize, linestep, 0, dst.pixeladdr(0, 0),
                  dstformat, spec.pixel_bytes(), spec.scanline_bytes(), 0);
    // FIXME - honor dataOrder.  I'm not sure if it is ever used by
    // OpenCV.  Fix when it becomes a problem.

    // OpenCV uses BGR ordering
    // FIXME: what do they do with alpha?
    if (spec.nchannels >= 3) {
        float pixel[4];
        for (int y = 0; y < spec.height; ++y) {
            for (int x = 0; x < spec.width; ++x) {
                dst.getpixel(x, y, pixel, 4);
                float tmp = pixel[0];
                pixel[0]  = pixel[2];
                pixel[2]  = tmp;
                dst.setpixel(x, y, pixel, 4);
            }
        }
    }
    // FIXME -- the copy and channel swap should happen all as one loop,
    // probably templated by type.

#else
    dst.error(
        "fromIplImage not supported -- no OpenCV support at compile time");
#endif

    return dst;
}



// Note: DEPRECATED(2.0)
IplImage*
ImageBufAlgo::to_IplImage(const ImageBuf& src)
{
    pvt::LoggedTimer logtime("IBA::to_IplImage");
#ifdef USE_OPENCV
    ImageBuf tmp   = src;
    ImageSpec spec = tmp.spec();

    // Make sure the image buffer is initialized.
    if (!tmp.initialized() && !tmp.read(tmp.subimage(), tmp.miplevel(), true)) {
        DASSERT(0 && "Could not initialize ImageBuf.");
        return NULL;
    }

    int dstFormat;
    TypeDesc dstSpecFormat;
    if (spec.format == TypeDesc(TypeDesc::UINT8)) {
        dstFormat     = IPL_DEPTH_8U;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::INT8)) {
        dstFormat     = IPL_DEPTH_8S;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::UINT16)) {
        dstFormat     = IPL_DEPTH_16U;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::INT16)) {
        dstFormat     = IPL_DEPTH_16S;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::HALF)) {
        dstFormat = IPL_DEPTH_32F;
        // OpenCV does not support half types. Switch to float instead.
        dstSpecFormat = TypeDesc(TypeDesc::FLOAT);
    } else if (spec.format == TypeDesc(TypeDesc::FLOAT)) {
        dstFormat     = IPL_DEPTH_32F;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::DOUBLE)) {
        dstFormat     = IPL_DEPTH_64F;
        dstSpecFormat = spec.format;
    } else {
        DASSERT(0 && "Unknown data format in ImageBuf.");
        return NULL;
    }
    IplImage* ipl = cvCreateImage(cvSize(spec.width, spec.height), dstFormat,
                                  spec.nchannels);
    if (!ipl) {
        DASSERT(0 && "Unable to create IplImage.");
        return NULL;
    }

    size_t pixelsize = dstSpecFormat.size() * spec.nchannels;
    // Account for the origin in the line step size, to end up with the
    // standard OIIO origin-at-upper-left:
    size_t linestep = ipl->origin ? -ipl->widthStep : ipl->widthStep;

    bool converted = convert_image(spec.nchannels, spec.width, spec.height, 1,
                                   tmp.localpixels(), spec.format,
                                   spec.pixel_bytes(), spec.scanline_bytes(), 0,
                                   ipl->imageData, dstSpecFormat, pixelsize,
                                   linestep, 0);

    if (!converted) {
        DASSERT(0 && "convert_image failed.");
        cvReleaseImage(&ipl);
        return NULL;
    }

    // OpenCV uses BGR ordering
    if (spec.nchannels == 3) {
        cvCvtColor(ipl, ipl, CV_RGB2BGR);
    } else if (spec.nchannels == 4) {
        cvCvtColor(ipl, ipl, CV_RGBA2BGRA);
    }

    return ipl;
#else
    return NULL;
#endif
}



// Templated fast swap of R and B channels.
template<class Rtype>
static bool
RBswap(ImageBuf& R, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (ImageBuf::Iterator<Rtype, Rtype> r(R, roi); !r.done(); ++r)
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                Rtype tmp = r[0];
                r[0]      = Rtype(r[2]);
                r[2]      = tmp;
            }
    });
    return true;
}



ImageBuf
ImageBufAlgo::from_OpenCV(const cv::Mat& mat, TypeDesc convert, ROI roi,
                          int nthreads)
{
    pvt::LoggedTimer logtime("IBA::from_OpenCV");
    ImageBuf dst;
#ifdef USE_OPENCV
    TypeDesc srcformat;
    switch (mat.depth()) {
    case CV_8U: srcformat = TypeDesc::UINT8; break;
    case CV_8S: srcformat = TypeDesc::INT8; break;
    case CV_16U: srcformat = TypeDesc::UINT16; break;
    case CV_16S: srcformat = TypeDesc::INT16; break;
    case CV_32F: srcformat = TypeDesc::FLOAT; break;
    case CV_64F: srcformat = TypeDesc::DOUBLE; break;
    default:
        dst.errorf("Unsupported OpenCV data type, depth=%d", mat.depth());
        return dst;
    }

    TypeDesc dstformat = (convert != TypeDesc::UNKNOWN) ? convert : srcformat;
    ROI matroi(0, mat.cols, 0, mat.rows, 0, 1, 0, mat.channels());
    roi = roi_intersection(roi, matroi);
    ImageSpec spec(roi, dstformat);
    dst.reset(dst.name(), spec);
    size_t pixelsize = srcformat.size() * spec.nchannels;
    size_t linestep  = mat.step[0];
    // Block copy and convert
    parallel_convert_image(spec.nchannels, spec.width, spec.height, 1,
                           mat.ptr(), srcformat, pixelsize, linestep, 0,
                           dst.pixeladdr(roi.xbegin, roi.ybegin), dstformat,
                           spec.pixel_bytes(), spec.scanline_bytes(), 0, -1, -1,
                           nthreads);

    // OpenCV uses BGR ordering
    if (spec.nchannels >= 3) {
        OIIO_MAYBE_UNUSED bool ok = true;
        OIIO_DISPATCH_TYPES(ok, "from_OpenCV R/B swap", RBswap, dstformat, dst,
                            roi, nthreads);
    }

#else
    dst.error(
        "from_OpenCV() not supported -- no OpenCV support at compile time");
#endif

    return dst;
}



bool
ImageBufAlgo::to_OpenCV(cv::Mat& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    pvt::LoggedTimer logtime("IBA::to_OpenCV");
#ifdef USE_OPENCV
    if (!roi.defined())
        roi = src.roi();
    roi.chend              = std::min(roi.chend, src.nchannels());
    const ImageSpec& spec  = src.spec();
    int chans              = std::min(4, roi.nchannels());
    int dstFormat          = 0;
    TypeDesc dstSpecFormat = spec.format;
    if (spec.format == TypeDesc(TypeDesc::UINT8)) {
        dstFormat = CV_MAKETYPE(CV_8U, chans);
    } else if (spec.format == TypeDesc(TypeDesc::INT8)) {
        dstFormat = CV_MAKETYPE(CV_8S, chans);
    } else if (spec.format == TypeDesc(TypeDesc::UINT16)) {
        dstFormat = CV_MAKETYPE(CV_16U, chans);
    } else if (spec.format == TypeDesc(TypeDesc::INT16)) {
        dstFormat = CV_MAKETYPE(CV_16S, chans);
    } else if (spec.format == TypeDesc(TypeDesc::UINT32)) {
        dstFormat     = CV_MAKETYPE(CV_16U, chans);
        dstSpecFormat = TypeUInt16;
    } else if (spec.format == TypeDesc(TypeDesc::INT32)) {
        dstFormat     = CV_MAKETYPE(CV_16S, chans);
        dstSpecFormat = TypeInt16;
    } else if (spec.format == TypeDesc(TypeDesc::HALF)) {
        dstFormat     = CV_MAKETYPE(CV_32F, chans);
        dstSpecFormat = TypeFloat;
    } else if (spec.format == TypeDesc(TypeDesc::FLOAT)) {
        dstFormat = CV_MAKETYPE(CV_32F, chans);
    } else if (spec.format == TypeDesc(TypeDesc::DOUBLE)) {
        dstFormat = CV_MAKETYPE(CV_64F, chans);
    } else {
        DASSERT(0 && "Unknown data format in ImageBuf.");
        return false;
    }
    cv::Mat mat(roi.height(), roi.width(), dstFormat);
    if (mat.empty()) {
        DASSERT(0 && "Unable to create cv::Mat.");
        return false;
    }

    size_t pixelsize = dstSpecFormat.size() * chans;
    size_t linestep  = pixelsize * roi.width();
    bool converted   = parallel_convert_image(
        chans, roi.width(), roi.height(), 1,
        src.pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
        spec.format, spec.pixel_bytes(), spec.scanline_bytes(), 0, mat.ptr(),
        dstSpecFormat, pixelsize, linestep, 0, -1, -1, nthreads);

    if (!converted) {
        DASSERT(0 && "convert_image failed.");
        return false;
    }

    // OpenCV uses BGR ordering
    if (chans == 3) {
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    } else if (chans == 4) {
        cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);
    }

    dst = std::move(mat);
    return true;
#else
    return false;
#endif
}



namespace {

#ifdef USE_OPENCV
static mutex opencv_mutex;

class CameraHolder {
public:
    CameraHolder() {}
    // Destructor frees all cameras
    ~CameraHolder() {}
    // Get the capture device, creating a new one if necessary.
    cv::VideoCapture* operator[](int cameranum)
    {
        auto i = m_cvcaps.find(cameranum);
        if (i != m_cvcaps.end())
            return i->second.get();
        auto cvcam = new cv::VideoCapture(cameranum);
        m_cvcaps[cameranum].reset(cvcam);
        return cvcam;
    }

private:
    std::map<int, std::unique_ptr<cv::VideoCapture>> m_cvcaps;
};

static CameraHolder cameras;
#endif

}  // namespace



ImageBuf
ImageBufAlgo::capture_image(int cameranum, TypeDesc convert)
{
    pvt::LoggedTimer logtime("IBA::capture_image");
    ImageBuf dst;
#ifdef USE_OPENCV
    cv::Mat frame;
    {
        // This block is mutex-protected
        lock_guard lock(opencv_mutex);
        auto cvcam = cameras[cameranum];
        if (!cvcam) {
            dst.error("Could not create a capture camera (OpenCV error)");
            return dst;  // failed somehow
        }
        (*cvcam) >> frame;
        if (frame.empty()) {
            dst.error("Could not cvQueryFrame (OpenCV error)");
            return dst;  // failed somehow
        }
    }

    logtime.stop();
    dst = from_OpenCV(frame, convert);
    logtime.start();
    if (!dst.has_error()) {
        time_t now;
        time(&now);
        struct tm tmtime;
        Sysutil::get_local_time(&now, &tmtime);
        std::string datetime = Strutil::sprintf("%4d:%02d:%02d %02d:%02d:%02d",
                                                tmtime.tm_year + 1900,
                                                tmtime.tm_mon + 1,
                                                tmtime.tm_mday, tmtime.tm_hour,
                                                tmtime.tm_min, tmtime.tm_sec);
        dst.specmod().attribute("DateTime", datetime);
    }
#else
    dst.error(
        "capture_image not supported -- no OpenCV support at compile time");
#endif
    return dst;
}


OIIO_NAMESPACE_END
