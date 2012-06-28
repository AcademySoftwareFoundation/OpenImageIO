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


#ifndef OPENIMAGEIO_IMAGEBUFALGO_H
#define OPENIMAGEIO_IMAGEBUFALGO_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::vector<T> members of PixelStats below.
#  pragma warning (disable : 4251)
#endif

#include "imageio.h"
#include "imagebuf.h"
#include "fmath.h"
#include "color.h"
#include "thread.h"


#ifndef __OPENCV_CORE_TYPES_H__
struct IplImage;  // Forward declaration; used by Intel Image lib & OpenCV
#endif



OIIO_NAMESPACE_ENTER
{

class Filter2D;  // forward declaration

namespace ImageBufAlgo {

/// Zero out (set to 0, black) the entire image.
/// return true on success.
bool DLLPUBLIC zero (ImageBuf &dst);


/// Fill the entire image with the given pixel value.
/// return true on success.
bool DLLPUBLIC fill (ImageBuf &dst,
                     const float *pixel);

/// Fill a subregion of the image with the given pixel value.  The
/// subregion is bounded by [xbegin..xend) X [ybegin..yend).
/// return true on success.
bool DLLPUBLIC fill (ImageBuf &dst,
                     const float *pixel,
                     int xbegin, int xend,
                     int ybegin, int yend);

/// Fill a subregion of the volume with the given pixel value.  The
/// subregion is bounded by [xbegin,xend) X [ybegin,yend) X [zbegin,zend).
/// return true on success.
bool DLLPUBLIC fill (ImageBuf &dst,
                     const float *pixel,
                     int xbegin, int xend,
                     int ybegin, int yend,
                     int zbegin, int zend);

/// Fill a subregion of the volume with a checkerboard.  The subregion
/// is bounded by [xbegin,xend) X [ybegin,yend) X [zbegin,zend).  return
/// true on success.
bool DLLPUBLIC checker (ImageBuf &dst,
                        int width,
                        const float *color1,
                        const float *color2,
                        int xbegin, int xend,
                        int ybegin, int yend,
                        int zbegin=0, int zend=1);

/// Enum describing options to be passed to transform

enum DLLPUBLIC AlignedTransform
{
    TRANSFORM_NONE = 0,
    TRANSFORM_FLIP,        // Upside-down
    TRANSFORM_FLOP,        // Left/Right Mirrored
    TRANSFORM_FLIPFLOP,    // Upside-down + Mirrored (Same as 180 degree rotation)
//  TRANSFORM_ROT90,       // Rotate 90 degrees clockwise. Image remains in positive quadrant.
//  TRANSFORM_ROT180,      // Rotate 180 degrees clockwise. Image remains in positive quadrant. (Same as FlipFlop)
//  TRANSFORM_ROT270,      // Rotate 270 degrees clockwise. Image remains in positive quadrant.
};

/// Transform the image, as specified in the options. All transforms are done
/// with respect the display winow (full_size / full_origin), though data
/// outside this area (overscan) is preserved.  This operation does not
/// filter pixel values; all operations are pixel aligned. In-place operation
/// (dst == src) is not supported.
/// return true on success.

bool DLLPUBLIC transform (ImageBuf &dst, const ImageBuf &src, AlignedTransform t);


/// Change the number of channels in the specified imagebuf.
/// This is done by either dropping them, or synthesizing additional ones.
/// If channels are added, they are cleared to a value of 0.0.
/// Does not support in-place operation.
/// return true on success.

bool DLLPUBLIC setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels);


/// Make dst be a cropped copy of src, but with the new pixel data
/// window range [xbegin..xend) x [ybegin..yend).  Source pixel data
/// falling outside this range will not be transferred to dst.  If
/// the new pixel range extends beyond that of the source image, those
/// new pixels will get the color specified by bordercolor[0..nchans-1],
/// or with black/zero values if bordercolor is NULL.
bool DLLPUBLIC crop (ImageBuf &dst, const ImageBuf &src,
                     int xbegin, int xend, int ybegin, int yend,
                     const float *bordercolor=NULL);



/// Add the pixels of two images A and B, putting the sum in dst.
/// The 'options' flag controls behaviors, particular of what happens
/// when A, B, and dst have differing data windows.  Note that dst must
/// not be the same image as A or B, and all three images must have the
/// same number of channels.  A and B *must* be float images.

bool DLLPUBLIC add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B, int options=0);

/// Enum describing options to be passed to ImageBufAlgo::add.
/// Multiple options are allowed simultaneously by "or'ing" together.
enum DLLPUBLIC AddOptions
{
    ADD_DEFAULT = 0,
    ADD_RETAIN_DST = 1,     ///< Retain dst pixels outside the region
    ADD_CLEAR_DST = 0,      ///< Default: clear all the dst pixels first
    ADD_RETAIN_WINDOWS = 2, ///< Honor the existing windows
    ADD_ALIGN_WINDOWS = 0,  ///< Default: align the windows before adding
};


/// Apply a color transform to the pixel values
///
/// In-place operations (dst == src) are supported
/// If unpremult is specified, unpremultiply before color conversion,
/// then premultiply after the color conversion.  You'll may want to use this
/// flag if your image contains an alpha channel
///
/// Note: the dst image does not need to equal the src image, either in buffers
///       or bit depths. (For example, it is common for the src buffer to be a
///       lower bit depth image and the output image to be float).
/// If the output buffer is less than floating-point, results may be quantized /
/// clamped
/// return true on success, false on failure


bool DLLPUBLIC colorconvert (ImageBuf &dst, const ImageBuf &src,
    const ColorProcessor * processor,
    bool unpremult);

bool DLLPUBLIC colorconvert (float * color, int nchannels,
    const ColorProcessor * processor,
    bool unpremult);


struct DLLPUBLIC PixelStats {
    std::vector<float> min;
    std::vector<float> max;
    std::vector<float> avg;
    std::vector<float> stddev;
    std::vector<imagesize_t> nancount;
    std::vector<imagesize_t> infcount;
    std::vector<imagesize_t> finitecount;
};


/// Compute statistics on the specified image (over all pixels in the data
/// window). Upon success, the returned vectors will have size == numchannels.
/// A FLOAT ImageBuf is required.
/// (current subimage, and current mipmap level)
bool DLLPUBLIC computePixelStats (PixelStats &stats, const ImageBuf &src);

/// Struct holding all the results computed by ImageBufAlgo::compare().
/// (maxx,maxy,maxz,maxc) gives the pixel coordintes (x,y,z) and color
/// channel of the pixel that differed maximally between the two images.
/// nwarn and nfail are the number of "warnings" and "failures",
/// respectively.
struct CompareResults {
    double meanerror, rms_error, PSNR, maxerror;
    int maxx, maxy, maxz, maxc;
    int nwarn, nfail;
};

/// Numerically compare two images.  The images must be the same size
/// and number of channels, and must both be FLOAT data.  The difference
/// threshold (for any individual color channel in any pixel) for a
/// "failure" is failthresh, and for a "warning" is warnthresh.  The
/// results are stored in result.
bool DLLPUBLIC compare (const ImageBuf &A, const ImageBuf &B,
                        float failthresh, float warnthresh,
                        CompareResults &result);

/// Compare two images using Hector Yee's perceptual metric, returning
/// the number of pixels that fail the comparison.  The images must be
/// the same size, FLOAT, and in a linear color space.  Only the first
/// three channels are compared.  Free parameters are the ambient
/// luminance in the room and the field of view of the image display;
/// our defaults are probably reasonable guesses for an office
/// environment.
int DLLPUBLIC compare_Yee (const ImageBuf &img0, const ImageBuf &img1,
                           float luminance = 100, float fov = 45);

/// Do all pixels for the entire image have the same channel values?  If
/// color is not NULL, that constant value will be stored in
/// color[0..nchannels-1].
bool DLLPUBLIC isConstantColor (const ImageBuf &src, float *color = NULL);

/// Does the requested channel have a given value over the entire image?
///
bool DLLPUBLIC isConstantChannel (const ImageBuf &src, int channel, float val);

/// Is the image monochrome? (i.e., are all channels the same value?)
/// zero and one channel images always return true
/// (current subimage, and current mipmap level)
bool DLLPUBLIC isMonochrome(const ImageBuf &src);

/// Compute the sha1 byte hash for all the pixels in the image.
/// (current subimage, and current mipmap level)
std::string DLLPUBLIC computePixelHashSHA1(const ImageBuf &src);

/// Compute the sha1 byte hash for all the pixels in the image.
/// (current subimage, and current mipmap level)
std::string DLLPUBLIC computePixelHashSHA1(const ImageBuf &src,
                                           const std::string & extrainfo);



/// Set dst, over the pixel range [xbegin,xend) x [ybegin,yend), to be a
/// resized version of src (mapping such that the "full" image window of
/// each correspond to each other, regardless of resolution).  The
/// caller may explicitly pass a reconstruction filter, or resize() will
/// choose a reasonable default if NULL is passed.  The dst buffer must
/// be of type FLOAT.
bool DLLPUBLIC resize (ImageBuf &dst, const ImageBuf &src,
                       int xbegin, int xend, int ybegin, int yend,
                       Filter2D *filter=NULL);


enum DLLPUBLIC NonFiniteFixMode
{
    NONFINITE_NONE = 0,     ///< Do nothing
    NONFINITE_BLACK = 1,    ///< Replace nonfinite pixels with black
    NONFINITE_BOX3 = 2,     ///< Replace nonfinite pixels with 3x3 finite average
};

/// Fix all non-finite pixels (nan/inf) using the specified approach
bool DLLPUBLIC fixNonFinite(ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode=NONFINITE_BOX3,
                            int * pixelsFixed=NULL);


/// Convert an IplImage, used by OpenCV and Intel's Image Libray, and
/// set ImageBuf dst to be the same image (copying the pixels).  If
/// convert is not set to UNKNOWN, try to establish dst as holding that
/// data type and convert the IplImage data.  Return true if ok, false
/// if it couldn't figure out how to make the conversion from IplImage
/// to ImageBuf.  If OpenImageIO was compiled without OpenCV support,
/// this function will return false without modifying dst.
bool DLLPUBLIC from_IplImage (ImageBuf &dst, const IplImage *ipl,
                              TypeDesc convert=TypeDesc::UNKNOWN);

/// Construct an IplImage*, used by OpenCV and Intel's Image Library,
/// that is equivalent to the ImageBuf src.  If it is not possible, or
/// if OpenImageIO was compiled without OpenCV support, then return
/// NULL.  The ownership of the IplImage is fully transferred to the
/// calling application.
DLLPUBLIC IplImage* to_IplImage (const ImageBuf &src);

/// Capture a still image from a designated camera.  If able to do so,
/// store the image in dst and return true.  If there is no such device,
/// or support for camera capture is not available (such as if OpenCV
/// support was not enabled at compile time), return false and do not
/// alter dst.
bool DLLPUBLIC capture_image (ImageBuf &dst, int cameranum = 0,
                              TypeDesc convert=TypeDesc::UNKNOWN);



/// Set R to the composite of A over B using the Porter/Duff definition
/// of "over", returning true upon success and false for any of a
/// variety of failures (as described below).
///
/// A and B must have valid alpha channels identified by their ImageSpec
/// alpha_channel field, with the following two exceptions: (a) a
/// 3-channel image with no identified alpha will be assumed to be RGB,
/// alpha == 1.0; (b) a 4-channel image with no identified alpha will be
/// assumed to be RGBA with alpha in channel [3].  If A or B do not have
/// alpha channels (as determined by those rules) or if the number of
/// non-alpha channels do not match between A and B, over() will fail,
/// returning false.
///
/// R is not already an initialized ImageBuf, it will be sized to
/// encompass the minimal rectangular pixel region containing the union
/// of the defined pixels of A and B, and with a number of channels
/// equal to the number of non-alpha channels of A and B, plus an alpha
/// channel.  However, if R is already initialized, it will not be
/// resized, and the "over" operation will apply to its existing pixel
/// data window.  In this case, R must have an alpha channel designated
/// and must have the same number of non-alpha channels as A and B,
/// otherwise it will fail, returning false.
///
/// 'roi' specifies the region of R's pixels which will be computed;
/// existing pixels outside this range will not be altered.  If not
/// specified, the default ROI value will be interpreted as a request to
/// apply "A over B" to the entire region of R's pixel data.
///
/// A, B, and R need not perfectly overlap in their pixel data windows;
/// pixel values of A or B that are outside their respective pixel data
/// window will be treated as having "zero" (0,0,0...) value.
///
/// threads == 0, the default, indicates that over() should use as many
/// CPU threads as are specified by the global OIIO "threads" attribute.
/// Note that this is not a guarantee, for example, the implementation
/// may choose to spawn fewer threads for images too small to make a
/// large number of threads to be worthwhile.  Values of threads > 0 are
/// a request for that specific number of threads, with threads == 1
/// guaranteed to not spawn additional threads (this is especially
/// useful if over() is being called from one thread of an
/// already-multithreaded program).
bool DLLPUBLIC over (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     ROI roi = ROI(), int threads = 0);




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
///     ROI roi = get_roi (R);
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
            threads.add_thread (new boost::thread (f, roi));
        }
        threads.join_all ();
    }
}


};  // end namespace ImageBufAlgo


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H
