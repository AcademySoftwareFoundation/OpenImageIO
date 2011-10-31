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
#include "colortransfer.h"

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



/// Apply a transfer function to the pixel values.
///
bool DLLPUBLIC colortransfer (ImageBuf &dst, const ImageBuf &src,
                              ColorTransfer *tfunc);


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


};  // end namespace ImageBufAlgo


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H
