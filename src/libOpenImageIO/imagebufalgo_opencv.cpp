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
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#endif

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/sysutil.h>



OIIO_NAMESPACE_BEGIN



bool
ImageBufAlgo::from_IplImage (ImageBuf &dst, const IplImage *ipl,
                             TypeDesc convert)
{
    if (! ipl) {
        DASSERT (0 && "ImageBufAlgo::fromIplImage called with NULL ipl");
        dst.error ("Passed NULL source IplImage");
        return false;
    }
#ifdef USE_OPENCV
    TypeDesc srcformat;
    switch (ipl->depth) {
    case int(IPL_DEPTH_8U) :
        srcformat = TypeDesc::UINT8;  break;
    case int(IPL_DEPTH_8S) :
        srcformat = TypeDesc::INT8;  break;
    case int(IPL_DEPTH_16U) :
        srcformat = TypeDesc::UINT16;  break;
    case int(IPL_DEPTH_16S) :
        srcformat = TypeDesc::INT16;  break;
    case int(IPL_DEPTH_32F) :
        srcformat = TypeDesc::FLOAT;  break;
    case int(IPL_DEPTH_64F) :
        srcformat = TypeDesc::DOUBLE;  break;
    default:
        DASSERT (0 && "unknown IplImage type");
        dst.error ("Unsupported IplImage depth %d", (int)ipl->depth);
        return false;
    }

    TypeDesc dstformat = (convert != TypeDesc::UNKNOWN) ? convert : srcformat;
    ImageSpec spec (ipl->width, ipl->height, ipl->nChannels, dstformat);
    // N.B. The OpenCV headers say that ipl->alphaChannel,
    // ipl->colorModel, and ipl->channelSeq are ignored by OpenCV.

    if (ipl->dataOrder != IPL_DATA_ORDER_PIXEL) {
        // We don't handle separate color channels, and OpenCV doesn't either
        dst.error ("Unsupported IplImage data order %d", (int)ipl->dataOrder);
        return false;
    }

    dst.reset (dst.name(), spec);
    size_t pixelsize = srcformat.size()*spec.nchannels;
    // Account for the origin in the line step size, to end up with the
    // standard OIIO origin-at-upper-left:
    size_t linestep = ipl->origin ? -ipl->widthStep : ipl->widthStep;
    // Block copy and convert
    convert_image (spec.nchannels, spec.width, spec.height, 1,
                   ipl->imageData, srcformat,
                   pixelsize, linestep, 0,
                   dst.pixeladdr(0,0), dstformat,
                   spec.pixel_bytes(), spec.scanline_bytes(), 0);
    // FIXME - honor dataOrder.  I'm not sure if it is ever used by
    // OpenCV.  Fix when it becomes a problem.

    // OpenCV uses BGR ordering
    // FIXME: what do they do with alpha?
    if (spec.nchannels >= 3) {
        float pixel[4];
        for (int y = 0;  y < spec.height;  ++y) {
            for (int x = 0;  x < spec.width;  ++x) {
                dst.getpixel (x, y, pixel, 4);
                float tmp = pixel[0];  pixel[0] = pixel[2]; pixel[2] = tmp;
                dst.setpixel (x, y, pixel, 4);
            }
        }
    }
    // FIXME -- the copy and channel swap should happen all as one loop,
    // probably templated by type.

    return true;
#else
    dst.error ("fromIplImage not supported -- no OpenCV support at compile time");
    return false;
#endif
}



IplImage *
ImageBufAlgo::to_IplImage (const ImageBuf &src)
{
#ifdef USE_OPENCV
    ImageBuf tmp = src;
    ImageSpec spec = tmp.spec();

    // Make sure the image buffer is initialized.
    if (!tmp.initialized() && !tmp.read(tmp.subimage(), tmp.miplevel(), true)) {
        DASSERT (0 && "Could not initialize ImageBuf.");
        return NULL;
    }

    int dstFormat;
    TypeDesc dstSpecFormat;
    if (spec.format == TypeDesc(TypeDesc::UINT8)) {
        dstFormat = IPL_DEPTH_8U;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::INT8)) {
        dstFormat = IPL_DEPTH_8S;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::UINT16)) {
        dstFormat = IPL_DEPTH_16U;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::INT16)) {
        dstFormat = IPL_DEPTH_16S;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::HALF)) {
        dstFormat = IPL_DEPTH_32F;
        // OpenCV does not support half types. Switch to float instead.
        dstSpecFormat = TypeDesc(TypeDesc::FLOAT);
    } else if (spec.format == TypeDesc(TypeDesc::FLOAT)) {
        dstFormat = IPL_DEPTH_32F;
        dstSpecFormat = spec.format;
    } else if (spec.format == TypeDesc(TypeDesc::DOUBLE)) {
        dstFormat = IPL_DEPTH_64F;
        dstSpecFormat = spec.format;
    } else {
        DASSERT (0 && "Unknown data format in ImageBuf.");
        return NULL;
    }
    IplImage *ipl = cvCreateImage(cvSize(spec.width, spec.height), dstFormat, spec.nchannels);
    if (!ipl) {
        DASSERT (0 && "Unable to create IplImage.");
        return NULL;
    }

    size_t pixelsize = dstSpecFormat.size() * spec.nchannels;
    // Account for the origin in the line step size, to end up with the
    // standard OIIO origin-at-upper-left:
    size_t linestep = ipl->origin ? -ipl->widthStep : ipl->widthStep;

    bool converted = convert_image(spec.nchannels, spec.width, spec.height, 1,
                                   tmp.localpixels(), spec.format,
                                   spec.pixel_bytes(), spec.scanline_bytes(), 0,
                                   ipl->imageData, dstSpecFormat,
                                   pixelsize, linestep, 0);

    if (!converted) {
        DASSERT (0 && "convert_image failed.");
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



namespace {

#ifdef USE_OPENCV
static mutex opencv_mutex;

class CameraHolder {
public:
    CameraHolder () { }
    // Destructor frees all cameras
    ~CameraHolder () {
        for (camera_map::iterator i = m_cvcaps.begin();  i != m_cvcaps.end();  ++i)
            cvReleaseCapture (&(i->second));
    }
    // Get the capture device, creating a new one if necessary.
    CvCapture * operator[] (int cameranum) {
        camera_map::iterator i = m_cvcaps.find (cameranum);
        if (i != m_cvcaps.end())
            return i->second;
        CvCapture *cvcam = cvCreateCameraCapture (cameranum);
        m_cvcaps[cameranum] = cvcam;
        return cvcam;
    }
private:
    typedef std::map<int,CvCapture*> camera_map;
    camera_map m_cvcaps;
};

static CameraHolder cameras;
#endif

}


bool
ImageBufAlgo::capture_image (ImageBuf &dst, int cameranum, TypeDesc convert)
{
#ifdef USE_OPENCV
    IplImage *frame = NULL;
    {
        // This block is mutex-protected
        lock_guard lock (opencv_mutex);
        CvCapture *cvcam = cameras[cameranum];
        if (! cvcam) {
            dst.error ("Could not create a capture camera (OpenCV error)");
            return false;  // failed somehow
        }
        frame = cvQueryFrame (cvcam);
        if (! frame) {
            dst.error ("Could not cvQueryFrame (OpenCV error)");
            return false;  // failed somehow
        }
    }

    time_t now;
    time (&now);
    struct tm tmtime;
    Sysutil::get_local_time (&now, &tmtime);
    std::string datetime = Strutil::format ("%4d:%02d:%02d %02d:%02d:%02d",
                                   tmtime.tm_year+1900, tmtime.tm_mon+1,
                                   tmtime.tm_mday, tmtime.tm_hour,
                                   tmtime.tm_min, tmtime.tm_sec);

    bool ok = ImageBufAlgo::from_IplImage (dst, frame, convert);
    // cvReleaseImage (&frame);   // unnecessary?
    if (ok)
        dst.specmod().attribute ("DateTime", datetime);

    return ok;
#else
    dst.error ("capture_image not supported -- no OpenCV support at compile time");
    return false;
#endif
}


OIIO_NAMESPACE_END
