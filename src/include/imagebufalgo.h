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

#include "imageio.h"
#include "imagebuf.h"
#include "fmath.h"
#include "colortransfer.h"
#include "filter.h"
#include "transformation.h"

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
///.filter pixel values; all operations are pixel aligned. In-place operation
/// (dst == src) is not supported.
/// return true on success.

bool DLLPUBLIC transform (ImageBuf &dst, const ImageBuf &src, AlignedTransform t);


/// Change the number of channels in the specified imagebuf.
/// This is done by either dropping them, or synthesizing additional ones.
/// If channels are added, they are cleared to a value of 0.0.
/// Does not support in-place operation.
/// return true on success.

bool DLLPUBLIC setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels);

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



/// Copy a crop window of src to dst.  The crop region is bounded by
/// [xbegin..xend) X [ybegin..yend), with the pixels affected including
/// begin but not including the end pixel (just like STL ranges).  The
/// cropping can be done one of several ways, specified by the options
/// parameter, one of: CROP_CUT, CROP_WINDOW, CROP_BLACK, CROP_WHITE,
/// CROP_TRANS.
bool DLLPUBLIC crop (ImageBuf &dst, const ImageBuf &src,
           int xbegin, int xend, int ybegin, int yend, int options);

enum DLLPUBLIC CropOptions 
{
    CROP_CUT, 	  ///< cut out a pixel region to make a new image at the origin
    CROP_WINDOW,  ///< reduce the pixel data window, keep in the same position
    CROP_BLACK,	  ///< color to black all the pixels outside of the bounds
    CROP_WHITE,	  ///< color to white all the pixels outside of the bounds
    CROP_TRANS	  ///< make all pixels out of bounds transparent (zero)
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

/// You can optionally query the constantvalue'd color
/// (current subimage, and current mipmap level)
bool DLLPUBLIC isConstantColor (const ImageBuf &src, float *color = NULL);

/// Is the image monochrome? (i.e., are all channels the same value?)
/// zero and one channel images always return true
/// (current subimage, and current mipmap level)
bool DLLPUBLIC isMonochrome(const ImageBuf &src);

/// Compute the sha1 byte hash for all the pixels in the image.
/// (current subimage, and current mipmap level)
std::string DLLPUBLIC computePixelHashSHA1(const ImageBuf &src);


/// Set dst, over the pixel range [xbegin,xend) x [ybegin,yend), to be a
/// resized version of src (mapping such that the "full" image window of
/// each correspond to each other, regardless of resolution).  The
/// caller may explicitly pass a reconstruction filter, or resize() will
/// choose a reasonable default if NULL is passed.  The dst buffer must
/// be of type FLOAT.
bool DLLPUBLIC resize (ImageBuf &dst, const ImageBuf &src,
                       int xbegin, int xend, int ybegin, int yend,
                       Filter2D *filter=NULL, float filterwidth=1.0);

template <typename TRANSTYPE>
bool DLLPUBLIC transform(ImageBuf &dst, const ImageBuf &src,
                        Filter2D *filter, float filterwidth,
                        TRANSTYPE *trans
                        );


};  // end namespace ImageBufAlgo


}
OIIO_NAMESPACE_EXIT



#endif // OPENIMAGEIO_IMAGEBUF_H
