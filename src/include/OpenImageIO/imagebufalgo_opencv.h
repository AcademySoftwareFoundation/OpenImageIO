// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// @file
///
/// This file contains ImageBufAlgo functions that involve interoperability
/// with OpenCV. Please read these guidelines carefully:
///
/// * Only `#include <OpenImageIO/imagebufalgo_opencv.h>` AFTER including
///   the necessary OpenCV headers. These functions use the cv::Mat type
///   from OpenCV.
/// * These functions are inline, in order to make it unnecessary for
///   libOpenImageIO itself to link against OpenCV.
/// * However, since the implementation of the functions in this header make
///   calls to OpenCV, it is necessary for any application calling these
///   functions, the application is responsible for finding OpenCV and
///   linking against the OpenCV libraries.
///


#pragma once
#define OPENIMAGEIO_IMAGEBUFALGO_OPENCV_H

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>

#if !__has_include(<opencv2/core/version.hpp>)
#    error "This header requires OpenCV"
#endif

#include <opencv2/core/version.hpp>
#ifdef CV_VERSION_EPOCH
#    define OIIO_OPENCV_VERSION \
        (10000 * CV_VERSION_EPOCH + 100 * CV_VERSION_MAJOR + CV_VERSION_MINOR)
#else
#    define OIIO_OPENCV_VERSION                            \
        (10000 * CV_VERSION_MAJOR + 100 * CV_VERSION_MINOR \
         + CV_VERSION_REVISION)
#endif
OIIO_PRAGMA_WARNING_PUSH
#if OIIO_GNUC_VERSION >= 110000 || OIIO_CLANG_VERSION >= 120000 \
    || OIIO_APPLE_CLANG_VERSION >= 120000
#    pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
// #    pragma GCC diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
// #    pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include <opencv2/opencv.hpp>
#if OIIO_OPENCV_VERSION >= 40000
#    include <opencv2/core/core_c.h>
#    include <opencv2/imgproc/imgproc_c.h>
#else
#    error "OpenCV 4.0 is the minimum supported version"
#endif
OIIO_PRAGMA_WARNING_POP



OIIO_NAMESPACE_BEGIN

namespace ImageBufAlgo {

/// Convert an OpenCV cv::Mat into an ImageBuf, copying the pixels (optionally
/// converting to the pixel data type specified by `convert`, if not UNKNOWN,
/// which means to preserve the original data type if possible).  Return true
/// if ok, false if it was not able to make the conversion from Mat to
/// ImageBuf. Any error messages can be retrieved by calling `geterror()` on
/// the returned ImageBuf. If OpenImageIO was compiled without OpenCV support,
/// this function will return false.
inline ImageBuf
from_OpenCV(const cv::Mat& mat, TypeDesc convert = TypeUnknown, ROI roi = {},
            int nthreads = 0);

/// Construct an OpenCV cv::Mat containing the contents of ImageBuf src, and
/// return true. If it is not possible, or if OpenImageIO was compiled without
/// OpenCV support, then return false. Any error messages can be retrieved by
/// calling OIIO::geterror(). Note that OpenCV only supports up to 4 channels,
/// so >4 channel images will be truncated in the conversion.
inline bool
to_OpenCV(cv::Mat& dst, const ImageBuf& src, ROI roi = {}, int nthreads = 0);

/// Capture a still image from a designated camera.  If able to do so,
/// store the image in dst and return true.  If there is no such device,
/// or support for camera capture is not available (such as if OpenCV
/// support was not enabled at compile time), return false and do not
/// alter dst.
inline ImageBuf
capture_image(int cameranum = 0, TypeDesc convert = TypeUnknown);

}  // namespace ImageBufAlgo



//////////////////////////////////////////////////////////////////////////
//
// Implementation details follow.
//
// ^^^ All declarations and documentation is above ^^^
//
// vvv Below is the implementation.
//
//////////////////////////////////////////////////////////////////////////

namespace pvt {

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

}  // namespace pvt



inline ImageBuf
ImageBufAlgo::from_OpenCV(const cv::Mat& mat, TypeDesc convert, ROI roi,
                          int nthreads)
{
    Timer timer;
    ImageBuf dst;
    TypeDesc srcformat;
    switch (mat.depth()) {
    case CV_8U: srcformat = TypeDesc::UINT8; break;
    case CV_8S: srcformat = TypeDesc::INT8; break;
    case CV_16U: srcformat = TypeDesc::UINT16; break;
    case CV_16S: srcformat = TypeDesc::INT16; break;
    case CV_32F: srcformat = TypeDesc::FLOAT; break;
    case CV_64F: srcformat = TypeDesc::DOUBLE; break;
    default:
        dst.errorfmt("Unsupported OpenCV data type, depth={}", mat.depth());
        return dst;
    }

    TypeDesc dstformat = (convert.is_unknown()) ? srcformat : convert;
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
                           spec.pixel_bytes(), spec.scanline_bytes(), 0,
                           nthreads);

    // OpenCV uses BGR ordering
    if (spec.nchannels >= 3) {
        OIIO_MAYBE_UNUSED bool ok = true;
        OIIO_DISPATCH_TYPES(ok, "from_OpenCV R/B swap", pvt::RBswap, dstformat,
                            dst, roi, nthreads);
    }

    log_time("IBA::from_OpenCV", timer);
    return dst;
}



inline bool
ImageBufAlgo::to_OpenCV(cv::Mat& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    Timer timer;
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
        dstFormat = CV_MAKETYPE(CV_16F, chans);
    } else if (spec.format == TypeDesc(TypeDesc::FLOAT)) {
        dstFormat = CV_MAKETYPE(CV_32F, chans);
    } else if (spec.format == TypeDesc(TypeDesc::DOUBLE)) {
        dstFormat = CV_MAKETYPE(CV_64F, chans);
    } else {
        // Punt, make 8 bit
        dstFormat = CV_MAKETYPE(CV_8S, chans);
    }
    dst.create(roi.height(), roi.width(), dstFormat);
    if (dst.empty()) {
        OIIO::errorfmt("to_OpenCV() was unable to create cv::Mat of {}x{} {}",
                       roi.width(), roi.height(), dstSpecFormat);
        log_time("IBA::to_OpenCV", timer);
        return false;
    }

    size_t pixelsize = dstSpecFormat.size() * chans;
    size_t linestep  = pixelsize * roi.width();
    size_t bufsize   = linestep * roi.height();
    // Make an IB that wraps the OpenCV buffer, then IBA:: copy to it
    ImageBuf cvib(ImageSpec(roi.width(), roi.height(), chans, dstSpecFormat),
                  make_span((std::byte*)dst.ptr(), bufsize), nullptr, pixelsize,
                  linestep);
    bool converted = ImageBufAlgo::copy(cvib, src);
    if (!converted) {
        OIIO::errorfmt(
            "to_OpenCV() was unable to convert source {} to cv::Mat of {}",
            spec.format, dstSpecFormat);
        log_time("IBA::to_OpenCV", timer);
        return false;
    }

    // OpenCV uses BGR ordering
    if (chans == 3) {
        cv::cvtColor(dst, dst, cv::COLOR_RGB2BGR);
    } else if (chans == 4) {
        cv::cvtColor(dst, dst, cv::COLOR_RGBA2BGRA);
    }

    log_time("IBA::to_OpenCV", timer);
    return true;
}



inline ImageBuf
ImageBufAlgo::capture_image(int cameranum, TypeDesc convert)
{
    Timer timer;
    ImageBuf dst;
    cv::Mat frame;
    {
        // This block is mutex-protected
        static std::map<int, std::unique_ptr<cv::VideoCapture>> cameras;
        static mutex opencv_mutex;
        lock_guard lock(opencv_mutex);
        auto& cvcam = cameras[cameranum];
        if (!cvcam) {
            cvcam.reset(new cv::VideoCapture(cameranum));
            if (!cvcam) {
                dst.errorfmt(
                    "Could not create a capture camera (OpenCV error)");
                log_time("IBA::capture_image", timer);
                return dst;  // failed somehow
            }
        }
        (*cvcam) >> frame;
        if (frame.empty()) {
            dst.errorfmt("Could not cvQueryFrame (OpenCV error)");
            log_time("IBA::capture_image", timer);
            return dst;  // failed somehow
        }
    }

    // logtime.stop();
    dst = from_OpenCV(frame, convert);
    // logtime.start();
    if (!dst.has_error()) {
        time_t now;
        time(&now);
        struct tm tmtime;
        Sysutil::get_local_time(&now, &tmtime);
        std::string datetime
            = Strutil::fmt::format("{:4d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                                   tmtime.tm_year + 1900, tmtime.tm_mon + 1,
                                   tmtime.tm_mday, tmtime.tm_hour,
                                   tmtime.tm_min, tmtime.tm_sec);
        dst.specmod().attribute("DateTime", datetime);
    }
    log_time("IBA::capture_image", timer);
    return dst;
}


OIIO_NAMESPACE_END
