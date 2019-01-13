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


// clang-format off

#pragma once
#define OPENIMAGEIO_IMAGEBUFALGO_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::vector<T> members of PixelStats below.
#  pragma warning (disable : 4251)
#endif

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/span.h>

#include <OpenEXR/ImathMatrix.h>       /* because we need M33f */

#include <limits>

#if !defined(__OPENCV_CORE_TYPES_H__) && !defined(OPENCV_CORE_TYPES_H)
struct IplImage;  // Forward declaration; used by Intel Image lib & OpenCV
namespace cv {
    class Mat;
}
#endif



OIIO_NAMESPACE_BEGIN

class Filter2D;  // forward declaration



/// Some generalities about ImageBufAlgo functions: (READ ME!)
///
/// This section explains the general rules common to all ImageBufAlgo
/// functions. Only exceptions to these rules will be explained in the
/// subsequent listings of all the individual ImageBufAlgo functions.
///
///
/// ### Return values and error messages
///
/// Most ImageBufAlgo functions that produce image data come in two forms:
///
/// 1. Return an ImageBuf.
///
///    The return value is a new ImageBuf containing the result image. In
///    this case, an entirely new image will be created to hold the result.
///    In case of error, the result image returned can have any error
///    conditions checked with `has_error()` and `geterror()`.
///
///        // Method 1: Return an image result
///        ImageBuf fg ("fg.exr"), bg ("bg.exr");
///        ImageBuf dst = ImageBufAlgo::over (fg, bg);
///        if (dst.has_error())
///            std::cout << "error: " << dst.geterror() << "\n";
///
/// 2. Pass a destination ImageBuf reference as the first parameter.
///
///    The function is passed a *destination* ImageBuf where the results
///    will be stored, and the return value is a `bool` that is `true` if
///    the function succeeds or `false` if the function fails. Upon failure,
///    the destination ImageBuf (the one that is being altered) will have an
///    error message set.
///
///        // Method 2: Write into an existing image
///        ImageBuf fg ("fg.exr"), bg ("bg.exr");
///        ImageBuf dst;   // will be the output image
///        bool ok = ImageBufAlgo::over (dst, fg, bg);
///        if (! ok)
///            std::cout << "error: " << dst.geterror() << "\n";
///
/// The first option (return an ImageBuf) is a more compact and intuitive
/// notation that is natural for most simple uses. But the second option
/// (pass an ImageBuf& referring to an existing destination) offers
/// additional flexibility, including more careful control over allocations,
/// the ability to partially overwrite regions of an existing image, and the
/// ability for the destination image to also be one of the input images
/// (for example, add(A,A,B) adds B into existing image A, with no third
/// image allocated at all).
///
/// ### Region of interest
///
/// Most ImageBufAlgo functions take an optional ROI parameter that
/// restricts the operation to a range in x, y, z, and channels. The default
/// ROI (also known as `ROI::All()) means no region restriction.
///
/// For ImageBufAlgo functions that write into a destination ImageBuf
/// parameter and it is already initialized (i.e. allocated with a
/// particular size and data type), the operation will be performed on the
/// pixels in the destination that overlap the ROI, leaving pixels in the
/// destination which are outside the ROI unaltered.
///
/// For ImageBufAlgo functions that return an ImageBuf directly, or their
/// `dst` parameter that is an uninitialized ImageBuf, the ROI (if set)
/// determines the size of the result image. If the ROI is the default
/// `All`, the result image size will be the union of the pixel data windows
/// of the input images and have a data type determind by the data types of
/// the input images.
///
/// Most ImageBufAlgo functions also respect the `chbegin` and `chend`
/// members of the ROI, thus restricting the channel range on which the
/// operation is performed.  The default ROI constructor sets up the ROI
/// to specify that the operation should be performed on all channels of
/// the input image(s).
///
/// ### Constant and per-channel values
///
/// Many ImageBufAlgo functions take per-channel constant-valued arguments
/// (for example, a fill color). These parameters are passed as
/// `cspan<float>`. These are generally expected to have length equal to the
/// number of channels. But you may also pass a single float which will be
/// used as the value for all channels. (More generally, what is happening
/// is that the last value supplied is replicated for any missing channel.)
///
/// Some ImageBufAlgo functions have parameters of type `Image_or_Const`,
/// which may take either an ImageBuf reference, or a per-channel constant,
/// or a single constant to be used for all channels.
///
/// ### Multithreading
///
/// All ImageBufAlgo functions take an optional `nthreads` parameter that
/// signifies the maximum number of threads to use to parallelize the
/// operation.  The default value for `nthreads` is 0, which signifies that
/// the number of thread should be the OIIO global default set by
/// `OIIO::attribute()`, which itself defaults to be the detected level of
/// hardware concurrency (number of cores available).
///
/// Generally you can ignore this parameter (or pass 0), meaning to use all
/// the cores available in order to perform the computation as quickly as
/// possible.  The main reason to explicitly pass a different number
/// (generally 1) is if the application is multithreaded at a high level,
/// and the thread calling the ImageBufAlgo function just wants to continue
/// doing the computation without spawning additional threads, which might
/// tend to crowd out the other application threads.
///



/// Image_or_Const: Parameter-passing helper that is a non-owning reference
/// to either an ImageBuf&, ImageBuf*, per-channel float constant, or a
/// single float constant. This lets us tame the combinatorics of functions
/// where each of several input parameters may be either images or constant
/// values.
class Image_or_Const {
public:
    struct None {};
    Image_or_Const (None) : m_type(NONE) {}
    Image_or_Const (const ImageBuf &img) : m_type(IMG), m_img(&img) {}
    Image_or_Const (const ImageBuf *img) : m_type(IMG), m_img(img) {}
    Image_or_Const (cspan<float> val) : m_type(VAL), m_val(val) {}
    Image_or_Const (const float& val) : m_type(VAL), m_val(val) {}
    Image_or_Const (const std::vector<float>& val) : m_type(VAL), m_val(val) {}
    Image_or_Const (const float *v, size_t s) : m_type(VAL), m_val(v,s) {}
    Image_or_Const (const float *v, int s) : m_type(VAL), m_val(v,s) {}

    bool is_img () const { return m_type == IMG; }
    bool is_val () const { return m_type == VAL; }
    bool is_empty () const { return m_type == NONE; }
    const ImageBuf& img () const { return *m_img; }
    const ImageBuf* imgptr () const { return m_img; }
    cspan<float> val () const { return m_val; }

    void swap (Image_or_Const &other) {
        std::swap (m_type, other.m_type);
        std::swap (m_img, other.m_img);
        std::swap (m_val, other.m_val);
    }
private:
    enum Contents { NONE, VAL, IMG };
    Contents m_type;
    const ImageBuf * m_img = nullptr;
    cspan<float> m_val;
};




namespace ImageBufAlgo {

// old name (DEPRECATED 1.9)
typedef parallel_options parallel_image_options;



/// Create an all-black `float` image of size and channels as described by
/// the ROI. If dst is passed and is alredy initialized, keep its shape and
/// data type, and just zero out the pixels in the ROI.
ImageBuf OIIO_API zero (ROI roi, int nthreads=0);
bool OIIO_API zero (ImageBuf &dst, ROI roi={}, int nthreads=0);


/// Fill an image region with given channel values, either returning a new
/// image or altering the existing dst image within the ROI.  Note that the
/// values arrays start with channel 0, even if the ROI indicates that a
/// later channel is the first to be changed.
///
/// Three varieties of fill() exist: (a) a single set of channel values that
/// will apply to the whole ROI, (b) two sets of values that will create a
/// linearly interpolated gradient from top to bottom of the ROI, (c) four
/// sets of values that will be bilnearly interpolated across all four
/// corners of the ROI.
ImageBuf OIIO_API fill (cspan<float> values, ROI roi, int nthreads=0);
ImageBuf OIIO_API fill (cspan<float> top, cspan<float> bottom,
                        ROI roi, int nthreads=0);
ImageBuf OIIO_API fill (cspan<float> topleft, cspan<float> topright,
                        cspan<float> bottomleft, cspan<float> bottomright,
                        ROI roi, int nthreads=0);
bool OIIO_API fill (ImageBuf &dst, cspan<float> values,
                    ROI roi={}, int nthreads=0);
bool OIIO_API fill (ImageBuf &dst, cspan<float> top, cspan<float> bottom,
                    ROI roi={}, int nthreads=0);
bool OIIO_API fill (ImageBuf &dst, cspan<float> topleft, cspan<float> topright,
                    cspan<float> bottomleft, cspan<float> bottomright,
                    ROI roi={}, int nthreads=0);


/// Create a new image, or set the pixels in the destination image within
/// the ROI, to a checkerboard pattern with origin (xoffset,yoffset,zoffset)
/// and that alternates between color1[] and color2[] every width pixels in
/// x, every height pixels in y, and every depth pixels in z.  The pattern
/// is definied in abstract "image space" independently of the pixel data
/// window of dst or the ROI.
ImageBuf OIIO_API checker (int width, int height, int depth,
                           cspan<float> color1, cspan<float> color2,
                           int xoffset, int yoffset, int zoffset,
                           ROI roi, int nthreads=0);
bool OIIO_API checker (ImageBuf &dst, int width, int height, int depth,
                       cspan<float> color1, cspan<float> color2,
                       int xoffset=0, int yoffset=0, int zoffset=0,
                       ROI roi={}, int nthreads=0);


/// Return an image of "noise", or add pseudorandom noise to existring
/// image dst in every pixel and channel specified by the roi (defaulting
/// to all pixels, all channels). There are several noise types to choose
/// from, and each behaves differently and has a different interpretation of
/// the A and B parameters:
///   "gaussian"   adds Gaussian (normal distribution) noise values with
///                   mean value A and standard deviation B.
///   "uniform"    adds noise values uninformly distributed on range [A,B).
///   "salt"       changes to value A a portion of pixels given by B.
/// If the 'mono' flag is true, a single noise value will be applied to all
/// channels specified by roi, but if 'mono' is false, a separate noise
/// value will be computed for each channel in the region.
///
/// The random number generator is actually driven by a hash on the "image
/// space" coordinates and channel, independently of the pixel data window
/// of dst or the ROI. Choosing different seed values will result in a
/// different pattern, but for the same seed value, the noise at a  given
/// pixel coordinate (x,y,z) channel c will is completely deterministic and
/// repeatable.
ImageBuf OIIO_API noise (string_view noisetype,
                         float A = 0.0f, float B = 0.1f, bool mono = false,
                         int seed = 0, ROI roi={}, int nthreads=0);
bool OIIO_API noise (ImageBuf &dst, string_view noisetype,
                     float A = 0.0f, float B = 0.1f, bool mono = false,
                     int seed = 0, ROI roi={}, int nthreads=0);


/// Generic channel shuffling: return (or store in dst) a copy of src, but
/// with channels in the order channelorder[0..nchannels-1].  For any
/// channel in which channelorder[i] < 0, it will just make dst channel i a
/// constant color -- set to channelvalues[i] (if channelvalues is not
/// empty) or 0.0 (if channelvalues is empty).  In-place operation is
/// allowed (i.e., dst and src the same image, but an extra copy will
/// occur).
///
/// If channelorder is empty, it will be interpreted as
/// {0, 1, ..., nchannels-1}, meaning that it's only renaming channels,
/// not reordering them.
///
/// If newchannelnames is not empty, it contains an array of new channel
/// names.  Channels for which newchannelnames[i] is the empty string (or
/// all channels, if newchannelnames is empty) will be named as follows:
/// If shuffle_channel_names is false, the resulting dst image will have
/// default channel names in the usual order ("R", "G", etc.), but if
/// shuffle_channel_names is true, the names will be taken from the
/// corresponding channels of the source image -- be careful with this,
/// shuffling both channel ordering and their names could result in no
/// semantic change at all, if you catch the drift.
///
/// N.B. If you are merely interested in extending the number of channels
/// (with zero values for added channels) or truncating channels at the end
/// (but leaving the other channels intact), then you should call this as:
///    channels (dst, src, nchannels, {}, {}, {}, true);
ImageBuf OIIO_API channels (const ImageBuf &src,
                        int nchannels, cspan<int> channelorder,
                        cspan<float> channelvalues={},
                        cspan<std::string> newchannelnames={},
                        bool shuffle_channel_names=false, int nthreads=0);
bool OIIO_API channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, cspan<int> channelorder,
                        cspan<float> channelvalues={},
                        cspan<std::string> newchannelnames={},
                        bool shuffle_channel_names=false, int nthreads=0);


/// Append the channels of A and B together into dst over the region of
/// interest.  If the region passed is uninitialized (the default), it
/// will be interpreted as being the union of the pixel windows of A and
/// B (and all channels of both images).  If dst is not already
/// initialized, it will be resized to be big enough for the region.
ImageBuf OIIO_API channel_append (const ImageBuf &A, const ImageBuf &B,
                                  ROI roi={}, int nthreads=0);
bool OIIO_API channel_append (ImageBuf &dst, const ImageBuf &A,
                              const ImageBuf &B, ROI roi={}, int nthreads=0);


/// Return (or copy into dst at the corresponding locations) the specified
/// region of pixels of src.  If dst is not already initialized, it will be
/// set to the same size as roi (defaulting to all of src), optionally with
/// the pixel type overridden by convert (if it is not UNKNOWN).
ImageBuf OIIO_API copy (const ImageBuf &src, TypeDesc convert=TypeUnknown,
                        ROI roi={}, int nthreads=0);
bool OIIO_API copy (ImageBuf &dst, const ImageBuf &src, TypeDesc convert=TypeUnknown,
                    ROI roi={}, int nthreads=0);

/// Return (or store into dst) the specified region of src, without
/// altering its position in the image plane.
ImageBuf OIIO_API crop (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API crop (ImageBuf &dst, const ImageBuf &src, ROI roi={}, int nthreads=0);

/// Return (or store into dst) the designated region of src, but shifted to
/// be at the (0,0) origin, and with the full/display resolution set to be
/// identical to the data region.
ImageBuf OIIO_API cut (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API cut (ImageBuf &dst, const ImageBuf &src, ROI roi={}, int nthreads=0);

/// Copy into dst, beginning at (xbegin,ybegin,zbegin), the pixels of
/// src described by srcroi.  If srcroi is ROI::All(), the entirety of src
/// will be used.  It will copy into channels [chbegin...], as many
/// channels as are described by srcroi. Pixels or channels of dst outside
/// the range of roi will not be altered.
bool OIIO_API paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin, const ImageBuf &src,
                     ROI srcroi={}, int nthreads=0);


/// rotateN: Return (or copy into dst) a rotated copy of the image pixels of
/// src, in 90 degree increments. Pictorially:
///
///  rotate90             rotate180            rotate270
/// -----------          -----------          -----------
/// AB  -->  CA          AB  -->  DC          AB  -->  BD
/// CD       DB          CD       BA          CD       AC
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
ImageBuf OIIO_API rotate90  (const ImageBuf &src, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rotate180 (const ImageBuf &src, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rotate270 (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API rotate90  (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);
bool OIIO_API rotate180 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);
bool OIIO_API rotate270 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);


/// flip()/flop()/transpose(): Return (or copy into dst) a subregion of src,
/// but with the scanlines exchanged vertically (flip), or columns exchanged
/// horizontally (flop), or transposed across the diagonal by swapping rows
/// for columns (transpose) within the display/full window. In other words,
///
///    flip                 flop               transpose
/// -----------          -----------          -----------
/// AB  -->  CD          AB  -->  BA          AB  -->  AC
/// CD       AB          CD       DC          CD       BD
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
ImageBuf OIIO_API flip (const ImageBuf &src, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API flop (const ImageBuf &src, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API transpose (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API flip (ImageBuf &dst, const ImageBuf &src,
                    ROI roi={}, int nthreads=0);
bool OIIO_API flop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi={}, int nthreads=0);
bool OIIO_API transpose (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);


/// Return (or store into dst) a copy of src, but with whatever seties of
/// rotations, flips, or flops are necessary to transform the pixels into
/// the configuration suggested by the Orientation metadata of the image
/// (and the Orientation metadata is then set to 1, ordinary orientation).
ImageBuf OIIO_API reorient (const ImageBuf &src, int nthreads=0);
bool OIIO_API reorient (ImageBuf &dst, const ImageBuf &src, int nthreads=0);


/// Return (or store into dst) a subregion of src, but circularly
/// shifting by the given amount.  To clarify, the circular shift
/// of [0,1,2,3,4,5] by +2 is [4,5,0,1,2,3].
///
/// Only the pixels (and channels) of src that are specified by roi will
/// be copied to dst; the default roi is to alter all the pixels in dst.
/// If dst is uninitialized, it will be resized to be an ImageBuf large
/// enough to hold the region specified by the transposed roi.  It is an
/// error to pass both an uninitialied dst and an undefined roi.
ImageBuf OIIO_API circular_shift (const ImageBuf &src,
                                  int xshift, int yshift, int zshift=0,
                                  ROI roi={}, int nthreads=0);
bool OIIO_API circular_shift (ImageBuf &dst, const ImageBuf &src,
                              int xshift, int yshift, int zshift=0,
                              ROI roi={}, int nthreads=0);


/// Return (or copy into dst) pixels of src with pixel values clamped as
/// follows:
/// * min specifies the minimum clamp value for each channel
///   (if min is empty, no minimum clamping is performed).
/// * max specifies the maximum clamp value for each channel
///   (if max is empty, no maximum clamping is performed).
/// * If clampalpha01 is true, then additionally any alpha channel is
///   clamped to the 0-1 range.
ImageBuf OIIO_API clamp (const ImageBuf &src,
                         cspan<float> min=-std::numeric_limits<float>::max(),
                         cspan<float> max=std::numeric_limits<float>::max(),
                         bool clampalpha01 = false, ROI roi={}, int nthreads=0);
bool OIIO_API clamp (ImageBuf &dst, const ImageBuf &src,
                     cspan<float> min=-std::numeric_limits<float>::max(),
                     cspan<float> max=std::numeric_limits<float>::max(),
                     bool clampalpha01 = false, ROI roi={}, int nthreads=0);

/// Compute per-pixel sum A + B, returning the result image or storing
/// the result into existing image dst.
///
/// A and B may each either be an ImageBuf&, or a cspan<float> giving a per-
/// channel constant, or a single constant used for all channels. (But at
/// least one must be an image.)
ImageBuf OIIO_API add (Image_or_Const A, Image_or_Const B,
                       ROI roi={}, int nthreads=0);
bool OIIO_API add (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                   ROI roi={}, int nthreads=0);


/// Compute per-pixel signed difference A - B, returning the result image or
/// storing the result into existing image dst.
///
/// A and B may each either be an ImageBuf&, or a cspan<float> giving a per-
/// channel constant, or a single constant used for all channels. (But at
/// least one must be an image.)
ImageBuf OIIO_API sub (Image_or_Const A, Image_or_Const B,
                       ROI roi={}, int nthreads=0);
bool OIIO_API sub (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                   ROI roi={}, int nthreads=0);


/// Compute per-pixel absolute difference abs(A - B), returning the result
/// image or storing the result into existing image dst.
///
/// A and B may each either be an ImageBuf&, or a cspan<float> giving a per-
/// channel constant, or a single constant used for all channels. (But at
/// least one must be an image.)
ImageBuf OIIO_API absdiff (Image_or_Const A, Image_or_Const B,
                           ROI roi={}, int nthreads=0);
bool OIIO_API absdiff (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                       ROI roi={}, int nthreads=0);


/// Compute per-pixel absolute value abs(A), returning the result image or
/// storing the result into existing image dst.
ImageBuf OIIO_API abs (const ImageBuf &A, ROI roi={}, int nthreads=0);
bool OIIO_API abs (ImageBuf &dst, const ImageBuf &A, ROI roi={}, int nthreads=0);


/// Compute per-pixel product A * B, returning the result image or
/// storing the result into existing image dst.
///
/// Either both A and B are images, or one is an image and the other is an
/// cspan<float> giving a per-channel constant or a single constant
/// used for all channels.
ImageBuf OIIO_API mul (Image_or_Const A, Image_or_Const B,
                       ROI roi={}, int nthreads=0);
bool OIIO_API mul (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                   ROI roi={}, int nthreads=0);


/// div(): Compute per-pixel division A / B, returning the result image or
/// storing the result into existing image dst.
///
/// We define division-by-zero to result in 0.
///
/// A is always an image, and B is either an image or an cspan<float>
/// giving a per-channel constant or a single constant used for all
/// channels.
ImageBuf OIIO_API div (Image_or_Const A, Image_or_Const B,
                       ROI roi={}, int nthreads=0);
bool OIIO_API div (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                   ROI roi={}, int nthreads=0);


/// mad(): Compute per-pixel multiply-and-add A * B + C, returning the result
/// image or storing the result into existing image dst.
///
/// A, B, and C are each either an image, or an cspan<float> giving a
/// per-channel constant or a single constant used for all channels. (Note:
/// at least one must be an image.)
ImageBuf OIIO_API mad (Image_or_Const A, Image_or_Const B,
                       Image_or_Const C, ROI roi={}, int nthreads=0);
bool OIIO_API mad (ImageBuf &dst, Image_or_Const A, Image_or_Const B,
                   Image_or_Const C, ROI roi={}, int nthreads=0);



/// Compute per-pixel inverse 1.0 - A. returning the result image or storing
/// the result into existing image dst.
///
/// Tips for callers: (1) You probably want to set roi to restrict the
/// operation to only the color channels, and not accidentally include
/// alpha, z, or others. (2) There may be situations where you want to
/// unpremult() before the inverst, then premult() the result, so that you
/// are computing the inverse of the unmasked color.
ImageBuf OIIO_API invert (const ImageBuf &A, ROI roi={}, int nthreads=0);
bool OIIO_API invert (ImageBuf &dst, const ImageBuf &A, ROI roi={}, int nthreads=0);


/// Compute per-pixel power A ^ B. returning the result image or storing
/// the result into existing image dst. It is permitted for dst and A to be
/// the same image.
///
/// A is always an image, and B is either an image or an cspan<float>
/// giving a per-channel constant or a single constant used for all
/// channels.
ImageBuf OIIO_API pow (const ImageBuf &A, cspan<float> B,
                       ROI roi={}, int nthreads=0);
bool OIIO_API pow (ImageBuf &dst, const ImageBuf &A, cspan<float> B,
                   ROI roi={}, int nthreads=0);


/// Converts a multi-channel image into a 1-channel image via a weighted sum
/// of channels: (channel[0]*weight[0] + channel[1]*weight[1] + ...),
/// returning the resulting 1-channel image or storing the result into
/// channel 0 of existing image dst. The weights, if not supplied, default
/// to { 1, 1, 1, ... }).
ImageBuf OIIO_API channel_sum (const ImageBuf &src, cspan<float> weights=1.0f,
                               ROI roi={}, int nthreads=0);
bool OIIO_API channel_sum (ImageBuf &dst, const ImageBuf &src,
                           cspan<float> weights=1.0f,
                           ROI roi={}, int nthreads=0);



/// rangecompress() returns (or copy into dst) all pixels and color channels
/// of src within region roi (defaulting to all the defined pixels of dst),
/// rescaling their range with a logarithmic transformation. Alpha and z
/// channels are not transformed.
///
/// rangeexpand() performs the inverse transformation (logarithmic back
/// into linear).
///
/// If useluma is true, the luma of channels [roi.chbegin..roi.chbegin+2]
/// (presumed to be R, G, and B) are used to compute a single scale
/// factor for all color channels, rather than scaling all channels
/// individually (which could result in a color shift).
///
/// The purpose of these function is as follows: Some image operations (such
/// as resizing with a "good" filter that contains negative lobes) can have
/// objectionable artifacts when applied to images with very high-contrast
/// regions involving extra bright pixels (such as highlights in HDR
/// captured or rendered images).  By compressing the range pixel values,
/// then performing the operation, then expanding the range of the result
/// again, the result can be much more pleasing (even if not exactly
/// correct).
ImageBuf OIIO_API rangecompress (const ImageBuf &src, bool useluma = false,
                                 ROI roi={}, int nthreads=0);
bool OIIO_API rangecompress (ImageBuf &dst, const ImageBuf &src,
                             bool useluma = false, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rangeexpand (const ImageBuf &src, bool useluma = false,
                               ROI roi={}, int nthreads=0);
bool OIIO_API rangeexpand (ImageBuf &dst, const ImageBuf &src,
                           bool useluma = false, ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the pixels of src within the ROI, applying a
/// color transform. In-place operations (dst == src) are supported.
///
/// If unpremult is true, divide the RGB channels by alpha (if it exists and
/// is nonzero) before color conversion, then re-multiply by alpha after the
/// after the color conversion. Passing unpremult=false skips this step,
/// which may be desirable if you know that the image is "unassociated alpha"
/// (a.k.a. "not pre-multiplied colors").
ImageBuf OIIO_API colorconvert (const ImageBuf &src,
                      string_view fromspace, string_view tospace, bool unpremult=true,
                      string_view context_key="", string_view context_value="",
                      ColorConfig *colorconfig=nullptr,
                      ROI roi={}, int nthreads=0);
bool OIIO_API colorconvert (ImageBuf &dst, const ImageBuf &src,
                  string_view fromspace, string_view tospace, bool unpremult=true,
                  string_view context_key="", string_view context_value="",
                  ColorConfig *colorconfig=nullptr,
                  ROI roi={}, int nthreads=0);

/// Return (or copy into dst) the pixels of src within the ROI, applying a
/// color transform. In-place operations (dst == src) are supported.
///
/// If unpremult is true, divide the RGB channels by alpha (if it exists and
/// is nonzero) before color conversion, then re-multiply by alpha after the
/// after the color conversion. Passing unpremult=false skips this step,
/// which may be desirable if you know that the image is "unassociated alpha"
/// (a.k.a. "not pre-multiplied colors").
ImageBuf OIIO_API colorconvert (const ImageBuf &src,
                                const ColorProcessor *processor,
                                bool unpremult, ROI roi={}, int nthreads=0);
bool OIIO_API colorconvert (ImageBuf &dst, const ImageBuf &src,
                            const ColorProcessor *processor,
                            bool unpremult, ROI roi={}, int nthreads=0);

/// Apply a color transform in-place to just one color:
/// color[0..nchannels-1].  nchannels should either be 3 or 4 (if 4, the
/// last channel is alpha).
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You'll may want to use this
/// flag if your image contains an alpha channel.
bool OIIO_API colorconvert (span<float> color,
                            const ColorProcessor *processor, bool unpremult);

// Less safe version with raw pointer and length.
inline bool colorconvert (float *color, int nchannels,
                          const ColorProcessor *processor, bool unpremult) {
    return colorconvert ({color,nchannels}, processor, unpremult);
}


/// Return (or copy into dst) the pixels of src within the ROI, applying an
/// OpenColorIO "look" transform. In-place operations (dst == src) are
/// supported.
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You may want to use this
/// flag if your image contains an alpha channel. If inverse is true, it
/// will reverse the color transformation.
ImageBuf OIIO_API ociolook (const ImageBuf &src, string_view looks,
                            string_view fromspace, string_view tospace,
                            bool unpremult=true, bool inverse=false,
                            string_view key="", string_view value="",
                            ColorConfig *colorconfig=nullptr,
                            ROI roi={}, int nthreads=0);
bool OIIO_API ociolook (ImageBuf &dst, const ImageBuf &src, string_view looks,
                        string_view fromspace, string_view tospace,
                        bool unpremult=true, bool inverse=false,
                        string_view key="", string_view value="",
                        ColorConfig *colorconfig=nullptr,
                        ROI roi={}, int nthreads=0);

/// Return (or copy into dst) the pixels of src within the ROI, applying an
/// OpenColorIO "look" transform.  If from or looks are nullptr, it will not
/// override the look or source color space (subtly different than
/// passing "", the empty string, which means to use no look or source
/// space). If inverse is true, it will reverse the color transformation.
///
/// If unpremult is true, divide the RGB channels by alpha (if it exists and
/// is nonzero) before color conversion, then re-multiply by alpha after the
/// after the color conversion. Passing unpremult=false skips this step,
/// which may be desirable if you know that the image is "unassociated alpha"
/// (a.k.a. "not pre-multiplied colors").
ImageBuf OIIO_API ociodisplay (const ImageBuf &src,
                               string_view display, string_view view,
                               string_view fromspace="", string_view looks="",
                               bool unpremult=true,
                               string_view key="", string_view value="",
                               ColorConfig *colorconfig=nullptr,
                               ROI roi={}, int nthreads=0);
bool OIIO_API ociodisplay (ImageBuf &dst, const ImageBuf &src,
                           string_view display, string_view view,
                           string_view fromspace="", string_view looks="",
                           bool unpremult=true,
                           string_view key="", string_view value="",
                           ColorConfig *colorconfig=nullptr,
                           ROI roi={}, int nthreads=0);

/// Return (or copy into dst) the pixels of src within the ROI, applying an
/// OpenColorIO "file" transform. If inverse is true, it will reverse the
/// color transformation. In-place operations (dst == src) are supported.
///
/// If unpremult is true, divide the RGB channels by alpha (if it exists and
/// is nonzero) before color conversion, then re-multiply by alpha after the
/// after the color conversion. Passing unpremult=false skips this step,
/// which may be desirable if you know that the image is "unassociated alpha"
/// (a.k.a. "not pre-multiplied colors").
ImageBuf OIIO_API ociofiletransform (const ImageBuf &src,
                                     string_view name,
                                     bool unpremult=true, bool inverse=false,
                                     ColorConfig *colorconfig=nullptr,
                                     ROI roi={}, int nthreads=0);
bool OIIO_API ociofiletransform (ImageBuf &dst, const ImageBuf &src,
                                 string_view name,
                                 bool unpremult=true, bool inverse=false,
                                 ColorConfig *colorconfig=nullptr,
                                 ROI roi={}, int nthreads=0);

/// Return (or copy into dst) pixels from src, and in the process divide all
/// color channels (those not alpha or z) by the alpha value, to "un-
/// premultiply" them.  This presumes that the image starts of as
/// "associated alpha" a.k.a. "premultipled."  The alterations are
/// restricted to the pixels and channels of the supplied ROI (which
/// defaults to all of src).  Pixels in which the alpha channel is 0 will
/// not be modified (since the operation is undefined in that case).  This
/// is just a copy if there is no identified alpha channel (and a no-op if
/// dst and src are the same image).
ImageBuf OIIO_API unpremult (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API unpremult (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);

/// Return (or copy into dst) pixels from src, and in the process multiply
/// all color channels (those not alpha or z) by the alpha value, to
/// "premultiply" them.  This presumes that the image starts off as
/// "unassociated alpha" a.k.a. "non-premultiplied."  The alterations are
/// restricted to the pixels and channels of the supplied ROI (which
/// defaults to all of src). Pixels in which the alpha channel is 0 will not
/// be modified (since the operation is undefined in that case).  This is
/// just a copy if there is no identified alpha channel (and a no-op if dst
/// and src are the same image).
ImageBuf OIIO_API premult (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API premult (ImageBuf &dst, const ImageBuf &src,
                       ROI roi={}, int nthreads=0);


/// Return (or copy into dst) pixel values determined by looking up a color
/// map using values of the source image, using either the channel specified
/// by srcchannel, or the luminance of src's RGB if srcchannel is -1. This
/// happens for all pixels within the  ROI (which defaults to all of src),
/// and if dst is not already initialized, it will be initialized to the ROI
/// and with color channels equal to channels.
///
/// The knots of the interpolated map are given by knots[nknots*channels].
/// An input value of 0.0 corresponds to knots[0..channels-1], an input
/// value of 1.0 corresponds ot knots[(nknots-1)*channels..knots.size()-1].
ImageBuf OIIO_API color_map (const ImageBuf &src, int srcchannel,
                             int nknots, int channels, cspan<float> knots,
                             ROI roi={}, int nthreads=0);
bool OIIO_API color_map (ImageBuf &dst, const ImageBuf &src, int srcchannel,
                         int nknots, int channels, cspan<float> knots,
                         ROI roi={}, int nthreads=0);

/// Return (or copy into dst) pixel values determined by looking up a color
/// map using values of the source image, using either the channel specified
/// by srcchannel, or the luminance of src's RGB if srcchannel is -1. This
/// happens for all pixels within the  ROI (which defaults to all of src),
/// and if dst is not already initialized, it will be initialized to the ROI
/// and with 3 color channels.
///
/// The mapname may be one of: "inferno", "viridis", "magma", or "plasma",
/// all of which are perceptually uniform, strictly increasing in luminance,
/// look good when converted to grayscale, and work for people with all
/// types of colorblindness. Also supported are the following color maps
/// that do not having those desirable qualities (and are thus not
/// recommended): "blue-red", "spectrum", "heat".
///
/// Return true on successs, false on error (with an appropriate error
/// message set in dst).
ImageBuf OIIO_API color_map (const ImageBuf &src, int srcchannel,
                             string_view mapname, ROI roi={}, int nthreads=0);
bool OIIO_API color_map (ImageBuf &dst, const ImageBuf &src, int srcchannel,
                         string_view mapname, ROI roi={}, int nthreads=0);



/// Remap contrast by transforming values [black, white] to new range
/// [min, max], either linearly or with optional application of a smooth
/// sigmoidal remapping (if scontrast != 1.0).
///
/// The following steps are performed, in order:
/// 1. Linearly rescale values [black, white] to [0, 1].
/// 2. If scontrast != 1, apply a sigmoidal remapping where a larger
///    scontrast value makes a steeper slope, and the steepest part is at
///    value sthresh (relative to the new remapped value after steps 1 & 2;
///    the default is 0.5).
/// 3. Rescale the range of that result: 0.0 -> min and 1.0 -> max.
///
/// Values outside of the [black,white] range will be extrapolated to
/// outside [min,max], so it may be prudent to apply a clamp() to the
/// results.
///
/// The black, white, min, max, scontrast, sthresh parameters may each
/// either be a single float value for all channels, or a span giving
/// per-channel values.
///
/// You can use this function for a simple linear contrast remapping of
/// [black, white] to [min, max] if you use the default values for sthresh.
/// Or just a simple sigmoidal contrast stretch within the [0,1] range if
/// you leave all other parameters at their defaults, or a combination of
/// these effects. Note that if black == white, the result will be a simple
/// binary thresholding where values < black map to min and values >= bkack
/// map to max.
OIIO_API ImageBuf contrast_remap (const ImageBuf &src,
                    cspan<float> black=0.0f, cspan<float> white=1.0f,
                    cspan<float> min=0.0f, cspan<float> max=1.0f,
                    cspan<float> scontrast=1.0f, cspan<float> sthresh=0.5f,
                    ROI={}, int nthreads=0);
OIIO_API bool contrast_remap (ImageBuf &dst, const ImageBuf &src,
                    cspan<float> black=0.0f, cspan<float> white=1.0f,
                    cspan<float> min=0.0f, cspan<float> max=1.0f,
                    cspan<float> scontrast=1.0f, cspan<float> sthresh=0.5f,
                    ROI={}, int nthreads=0);




struct OIIO_API PixelStats {
    std::vector<float> min;
    std::vector<float> max;
    std::vector<float> avg;
    std::vector<float> stddev;
    std::vector<imagesize_t> nancount;
    std::vector<imagesize_t> infcount;
    std::vector<imagesize_t> finitecount;
    std::vector<double> sum, sum2;  // for intermediate calculation

    PixelStats () {}
    PixelStats (PixelStats&& other) = default;
    PixelStats (int nchannels) { reset(nchannels); }
    void reset (int nchannels);
    void merge (const PixelStats &p);
    const PixelStats& operator= (PixelStats&& other);  // Move assignment
};


/// Compute statistics about the ROI of the src image, returning a
/// PixelStats structure. Upon success, the returned vectors in the result
/// structure will have size == src.nchannels(). If there is a failure, the
/// vector sizes will be 0 and an error will be set in src.
PixelStats OIIO_API computePixelStats (const ImageBuf &src,
                                       ROI roi={}, int nthreads=0);

// DEPRECATED(1.9): with C++11 move semantics, there's no reason why
// stats needs to be passed as a parameter instead of returned.
bool OIIO_API computePixelStats (PixelStats &stats, const ImageBuf &src,
                                 ROI roi={}, int nthreads=0);


/// Struct holding all the results computed by ImageBufAlgo::compare().
/// (maxx,maxy,maxz,maxc) gives the pixel coordintes (x,y,z) and color
/// channel of the pixel that differed maximally between the two images.
/// nwarn and nfail are the number of "warnings" and "failures",
/// respectively.
struct CompareResults {
    double meanerror, rms_error, PSNR, maxerror;
    int maxx, maxy, maxz, maxc;
    imagesize_t nwarn, nfail;
    bool error;
};

/// Numerically compare two images.  The difference threshold (for any
/// individual color channel in any pixel) for a "failure" is
/// failthresh, and for a "warning" is warnthresh.  The results are
/// stored in result.  If roi is defined, pixels will be compared for
/// the pixel and channel range that is specified.  If roi is not
/// defined, the comparison will be for all channels, on the union of
/// the defined pixel windows of the two images (for either image,
/// undefined pixels will be assumed to be black).
CompareResults OIIO_API compare (const ImageBuf &A, const ImageBuf &B,
                                 float failthresh, float warnthresh,
                                 ROI roi={}, int nthreads=0);

// DEPRECATED(1.9): with C++11 move semantics, there's no reason why
// result needs to be passed as a parameter instead of returned.
bool OIIO_API compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       CompareResults &result, ROI roi={}, int nthreads=0);

/// Compare two images using Hector Yee's perceptual metric, returning
/// the number of pixels that fail the comparison.  Only the first three
/// channels (or first three channels specified by roi) are compared.
/// Free parameters are the ambient luminance in the room and the field
/// of view of the image display; our defaults are probably reasonable
/// guesses for an office environment.  The 'result' structure will
/// store the maxerror, and the maxx, maxy, maxz of the pixel that
/// failed most severely.  (The other fields of the CompareResults
/// are not used for Yee comparison.)
///
/// Works for all pixel types.  But it's basically meaningless if the
/// first three channels aren't RGB in a linear color space that sort
/// of resembles AdobeRGB.
///
/// Return true on success, false on error.
int OIIO_API compare_Yee (const ImageBuf &A, const ImageBuf &B,
                          CompareResults &result,
                          float luminance = 100, float fov = 45,
                          ROI roi={}, int nthreads=0);


/// Do all pixels within the ROI have the same values for channels
/// [roi.chbegin..roi.chend-1], within a tolerance of +/- threshold?  If so,
/// return true and store that color in color[chbegin...chend-1] (if color
/// is not empty); otherwise return false.  If roi is not defined (the
/// default), it will be understood to be all of the defined pixels and
/// channels of source.
OIIO_API bool isConstantColor (const ImageBuf &src, float threshold=0.0f,
                               span<float> color = {},
                               ROI roi={}, int nthreads=0);
inline bool isConstantColor (const ImageBuf &src, span<float> color,
                             ROI roi={}, int nthreads=0) {
    return isConstantColor (src, 0.0f, color, roi, nthreads);
}

/// Does the requested channel have a given value (within a tolerance of +/-
/// threshold) for every channel within the ROI?  (For this function, the
/// ROI's chbegin/chend are ignored.)  Return true if so, otherwise return
/// false.  If roi is not defined (the default), it will be understood to be
/// all of the defined pixels and channels of source.
OIIO_API bool isConstantChannel (const ImageBuf &src, int channel,
                                 float val, float threshold=0.0f,
                                 ROI roi={}, int nthreads=0);
inline bool isConstantChannel (const ImageBuf &src, int channel, float val,
                               ROI roi, int nthreads=0) {
    return isConstantChannel (src, channel, val, 0.0f, roi, nthreads);
}

/// Is the image monochrome within the ROI, i.e., for every pixel within the
/// region, do all channels [roi.chbegin, roi.chend) have the same value
/// (within a tolerance of +/- threshold)?  If roi is not defined (the
/// default), it will be understood to be all of the defined pixels and
/// channels of source.
OIIO_API bool isMonochrome (const ImageBuf &src, float threshold=0.0f,
                            ROI roi={}, int nthreads=0);
inline bool isMonochrome (const ImageBuf &src, ROI roi, int nthreads=0) {
    return isMonochrome (src, 0.0f, roi, nthreads);
}

/// Count how many pixels in the ROI match a list of colors.
///
/// The colors to match are in colors[0..nchans-1],
/// colors[nchans..2*nchans-1], and so on, a total of ncolors
/// consecutive colors of nchans each.
///
/// eps[0..nchans-1] are the error tolerances for a match, for each channel.
/// Setting eps[c] = numeric_limits<float>::max() will effectively make it
/// ignore the channel.  The default eps is 0.001 for all channels (requires
/// exact matches for 8 bit images, but allows a wee bit of imprecision for
/// float images.
///
/// Upon success, return true and store the number of pixels that matched
/// each color count[..ncolors-1].  If there is an error, returns false and
/// sets an appropriate error message set in src.
bool OIIO_API color_count (const ImageBuf &src, imagesize_t *count,
                           int ncolors, cspan<float> color,
                           cspan<float> eps = 0.001f,
                           ROI roi={}, int nthreads=0);

/// Count how many pixels in the ROI are outside the value range.
/// low[0..nchans-1] and high[0..nchans-1] are the low and high
/// acceptable values for each color channel.
///
/// The number of pixels containing values that fall below the lower
/// bound will be stored in *lowcount, the number of pixels containing
/// values that fall above the upper bound will be stored in *highcount,
/// and the number of pixels for which all channels fell within the
/// bounds will be stored in *inrangecount. Any of these may be NULL,
/// which simply means that the counts need not be collected or stored.
///
/// Return true if the operation can be performed, false if there is
/// some sort of error (and sets an appropriate error message in src).
bool OIIO_API color_range_check (const ImageBuf &src,
                                 imagesize_t *lowcount,
                                 imagesize_t *highcount,
                                 imagesize_t *inrangecount,
                                 cspan<float> low, cspan<float> high,
                                 ROI roi={}, int nthreads=0);

/// Find the minimal rectangular region within roi (which defaults to
/// the entire pixel data window of src) that consists of nonzero pixel
/// values.  In other words, gives the region that is a "shrink-wraps"
/// of src to exclude black border pixels.  Note that if the entire
/// image was black, the ROI returned will contain no pixels.
///
/// For "deep" images, this function returns the smallest ROI that contains
/// all pixels that contain depth samples, and excludes the border pixels
/// that contain no depth samples at all.
OIIO_API ROI nonzero_region (const ImageBuf &src, ROI roi={}, int nthreads=0);

/// Compute the SHA-1 byte hash for all the pixels in the specifed
/// region of the image.  If blocksize > 0, the function will compute
/// separate SHA-1 hashes of each 'blocksize' batch of scanlines, then
/// return a hash of the individual hashes.  This is just as strong a
/// hash, but will NOT match a single hash of the entire image
/// (blocksize==0).  But by breaking up the hash into independent
/// blocks, we can parallelize across multiple threads, given by
/// nthreads (if nthreads is 0, it will use the global OIIO thread
/// count).  The 'extrainfo' provides additional text that will be
/// incorporated into the hash.
std::string OIIO_API computePixelHashSHA1 (const ImageBuf &src,
                                           string_view extrainfo = "",
                                           ROI roi={},
                                           int blocksize = 0, int nthreads=0);


/// Warp the src image using the supplied 3x3 transformation matrix.
///
/// Only the pixels (and channels) of dst that are specified by roi will be
/// copied from the warped src; the default roi is to alter all the pixels
/// in dst. If dst is uninitialized, it will be sized to be an ImageBuf
/// large enough to hold the warped image if recompute_roi is true, or
/// will have the same ROI as src if recompute_roi is false. It is an error
/// to pass both an uninitialied dst and an undefined roi.
///
/// The filter is used to weight the src pixels falling underneath it for
/// each dst pixel.  The caller may specify a reconstruction filter by name
/// and width (expressed in pixels unts of the dst image), or rotate() will
/// choose a reasonable default high-quality default filter (lanczos3) if
/// the empty string is passed, and a reasonable filter width if filterwidth
/// is 0. (Note that some filter choices only make sense with particular
/// width, in which case this filterwidth parameter may be ignored.)
///
ImageBuf OIIO_API warp (const ImageBuf &src, const Imath::M33f &M,
                        string_view filtername = string_view(),
                        float filterwidth = 0.0f, bool recompute_roi = false,
                        ImageBuf::WrapMode wrap = ImageBuf::WrapDefault,
                        ROI roi={}, int nthreads=0);
ImageBuf OIIO_API warp (const ImageBuf &src, const Imath::M33f &M,
                        const Filter2D *filter, bool recompute_roi = false,
                        ImageBuf::WrapMode wrap = ImageBuf::WrapDefault,
                        ROI roi = {}, int nthreads=0);
bool OIIO_API warp (ImageBuf &dst, const ImageBuf &src, const Imath::M33f &M,
                    string_view filtername = string_view(),
                    float filterwidth = 0.0f, bool recompute_roi = false,
                    ImageBuf::WrapMode wrap = ImageBuf::WrapDefault,
                    ROI roi={}, int nthreads=0);
bool OIIO_API warp (ImageBuf &dst, const ImageBuf &src, const Imath::M33f &M,
                    const Filter2D *filter, bool recompute_roi = false,
                    ImageBuf::WrapMode wrap = ImageBuf::WrapDefault,
                    ROI roi = {}, int nthreads=0);


/// Rotate the src image by the angle (in radians, with positive angles
/// clockwise). When center_x and center_y are supplied, they denote the
/// center of rotation; in their absence, the rotation will be about the
/// center of the image's display window.
///
/// Only the pixels (and channels) of dst that are specified by roi will be
/// copied from the rotated src; the default roi is to alter all the pixels
/// in dst. If dst is uninitialized, it will be resized to be an ImageBuf
/// large enough to hold the rotated image if recompute_roi is true, or will
/// have the same ROI as src if recompute_roi is false. It is an error to
/// pass both an uninitialied dst and an undefined roi.
///
/// The filter is used to weight the src pixels falling underneath it for
/// each dst pixel.  The caller may specify a reconstruction filter by name
/// and width (expressed in pixels unts of the dst image), or rotate() will
/// choose a reasonable default high-quality default filter (lanczos3) if
/// the empty string is passed, and a reasonable filter width if filterwidth
/// is 0. (Note that some filter choices only make sense with particular
/// width, in which case this filterwidth parameter may be ignored.)
ImageBuf OIIO_API rotate (const ImageBuf &src, float angle,
                          string_view filtername = string_view(),
                          float filterwidth = 0.0f, bool recompute_roi = false,
                          ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rotate (const ImageBuf &src, float angle,
                          Filter2D *filter, bool recompute_roi = false,
                          ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rotate (const ImageBuf &src,
                          float angle, float center_x, float center_y,
                          string_view filtername = string_view(),
                          float filterwidth = 0.0f, bool recompute_roi = false,
                          ROI roi={}, int nthreads=0);
ImageBuf OIIO_API rotate (const ImageBuf &src,
                          float angle, float center_x, float center_y,
                          Filter2D *filter, bool recompute_roi = false,
                          ROI roi={}, int nthreads=0);
bool OIIO_API rotate (ImageBuf &dst, const ImageBuf &src, float angle,
                      string_view filtername = string_view(),
                      float filterwidth = 0.0f, bool recompute_roi = false,
                      ROI roi={}, int nthreads=0);
bool OIIO_API rotate (ImageBuf &dst, const ImageBuf &src, float angle,
                      Filter2D *filter, bool recompute_roi = false,
                      ROI roi={}, int nthreads=0);
bool OIIO_API rotate (ImageBuf &dst, const ImageBuf &src,
                      float angle, float center_x, float center_y,
                      string_view filtername = string_view(),
                      float filterwidth = 0.0f, bool recompute_roi = false,
                      ROI roi={}, int nthreads=0);
bool OIIO_API rotate (ImageBuf &dst, const ImageBuf &src,
                      float angle, float center_x, float center_y,
                      Filter2D *filter, bool recompute_roi = false,
                      ROI roi={}, int nthreads=0);


/// Set dst, over the region of interest, to be a resized version of the
/// corresponding portion of src (mapping such that the "full" image
/// window of each correspond to each other, regardless of resolution).
///
/// The filter is used to weight the src pixels falling underneath it for
/// each dst pixel.  The caller may specify a reconstruction filter
/// explicitly by passing a `Filter2D*`, or by name and width (expressed in
/// pixels unts of the dst image), or resize() will choose a reasonable
/// default high-quality default filter (blackman-harris when upsizing,
/// lanczos3 when downsizing) if the empty string is passed or if
/// filterwidth is 0.
///
ImageBuf OIIO_API resize (const ImageBuf &src,
                          string_view filtername = "", float filterwidth=0.0f,
                          ROI roi={}, int nthreads=0);
bool OIIO_API resize (ImageBuf &dst, const ImageBuf &src,
                      string_view filtername = "", float filterwidth=0.0f,
                      ROI roi={}, int nthreads=0);
ImageBuf OIIO_API resize (const ImageBuf &src, Filter2D *filter,
                          ROI roi={}, int nthreads=0);
bool OIIO_API resize (ImageBuf &dst, const ImageBuf &src, Filter2D *filter,
                      ROI roi={}, int nthreads=0);


/// Set dst, over the region of interest, to be a resampled version of the
/// corresponding portion of src (mapping such that the "full" image
/// window of each correspond to each other, regardless of resolution).
///
/// Unlike ImageBufAlgo::resize(), resample does not take a filter; it
/// just samples either with a bilinear interpolation (if interpolate is
/// true, the default) or uses the single "closest" pixel (if
/// interpolate is false).  This makes it a lot faster than a proper
/// resize(), though obviously with lower quality (aliasing when
/// downsizing, pixel replication when upsizing).
ImageBuf OIIO_API resample (const ImageBuf &src, bool interpolate = true,
                        ROI roi={}, int nthreads=0);
bool OIIO_API resample (ImageBuf &dst, const ImageBuf &src,
                        bool interpolate = true, ROI roi={}, int nthreads=0);


/// Fit src into dst (to a size specified by roi, if dst is not
/// initialized), resizing but preserving its original aspect ratio. Thus,
/// it will resize so be the largest size with the same aspect ratio that
/// can fix inside the region, but will not stretch to completely fill it in
/// both dimensions.
///
/// If `exact` is true, will result in an exact match on aspect ratio and
/// centering (partial pixel shift if necessary), whereas exact=false
/// will only preserve aspect ratio and centering to the precision of a
/// whole pixel.
///
/// The filter is used to weight the src pixels falling underneath it for
/// each dst pixel.  The caller may specify a reconstruction filter
/// explicitly by passing a `Filter2D*`, or by name and width (expressed in
/// pixels unts of the dst image), or resize() will choose a reasonable
/// default high-quality default filter (blackman-harris when upsizing,
/// lanczos3 when downsizing) if the empty string is passed or if
/// filterwidth is 0.
///
ImageBuf OIIO_API fit (const ImageBuf &src,
                       string_view filtername = "", float filterwidth=0.0f,
                       bool exact=false, ROI roi={}, int nthreads=0);
bool OIIO_API fit (ImageBuf &dst, const ImageBuf &src,
                   string_view filtername = "", float filterwidth=0.0f,
                   bool exact=false, ROI roi={}, int nthreads=0);
ImageBuf OIIO_API fit (const ImageBuf &src, Filter2D *filter,
                       bool exact=false, ROI roi={}, int nthreads=0);
bool OIIO_API fit (ImageBuf &dst, const ImageBuf &src, Filter2D *filter,
                   bool exact=false, ROI roi={}, int nthreads=0);

/// Return (or store into the ROI of dst) the convolution of src and a
/// kernel. If roi is not defined, it defaults to the full size of dst (or
/// src, if dst was uninitialized).  If dst is uninitialized, it will be
/// allocated to be the size specified by roi.  If  normalized is true, the
/// kernel will be normalized for the  convolution, otherwise the original
/// values will be used.
ImageBuf OIIO_API convolve (const ImageBuf &src, const ImageBuf &kernel,
                            bool normalize = true, ROI roi={}, int nthreads=0);
bool OIIO_API convolve (ImageBuf &dst, const ImageBuf &src, const ImageBuf &kernel,
                        bool normalize = true, ROI roi={}, int nthreads=0);

/// Make a 1-channel FLOAT image of the named kernel. The size of the image
/// will be big enough to contain the kernel given its size (width x height)
/// and rounded up to odd resolution so that the center of the kernel can be
/// at the center of the middle pixel.  The kernel image will be offset so
/// that its center is at the (0,0) coordinate.  If normalize is true, the
/// values will be normalized so that they sum to 1.0. If depth > 1, a
/// volumetric kernel will be created.  Use with caution!
///
/// Kernel names can be: "gaussian", "sharp-gaussian", "box",
/// "triangle", "blackman-harris", "mitchell", "b-spline", "catmull-rom",
/// "lanczos3", "disk", "binomial", "laplacian".
///
/// Note that "catmull-rom" and "lanczos3" are fixed-size kernels that
/// don't scale with the width, and are therefore probably less useful
/// in most cases.
///
ImageBuf OIIO_API make_kernel (string_view name, float width, float height,
                               float depth = 1.0f, bool normalize = true);
// DEPRECATED(1.9):
inline bool make_kernel (ImageBuf &dst, string_view name,
                         float width, float height, float depth = 1.0f,
                         bool normalize = true) {
    dst = make_kernel (name, width, height, depth, normalize);
    return ! dst.has_error();
}


/// Return (or copy into dst) a sharpened version of the corresponding
/// region of src using the ``unsharp mask'' technique. Unsharp masking
/// basically works by first blurring the image (low pass filter),
/// subtracting this from the original image, then adding the residual back
/// to the original to emphasize the edges. Roughly speaking,
///
///      dst = src + contrast * thresh(src - blur(src))
///
/// The specific blur can be selected by kernel name and width.  The
/// contrast is a multiplier on the overall sharpening effect.  The
/// thresholding step causes all differences less than 'threshold' to be
/// squashed to zero, which can be useful for suppressing sharpening of
/// low-contrast details (like noise) but allow sharpening of
/// higher-contrast edges.
ImageBuf OIIO_API unsharp_mask (const ImageBuf &src,
                            string_view kernel="gaussian", float width = 3.0f,
                            float contrast = 1.0f, float threshold = 0.0f,
                            ROI roi={}, int nthreads=0);
bool OIIO_API unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                            string_view kernel="gaussian", float width = 3.0f,
                            float contrast = 1.0f, float threshold = 0.0f,
                            ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the Laplacian of the corresponding region of
/// src.  The Laplacian is the generalized second derivative of the image
/// $$\frac{\partial^2 s}{\partial x^2} + \frac{\partial^2 s}{\partial y^2}$$
/// which is approximated by convolving the image with a discrete 3x3
/// Laplacian kernel,
///                     [ 0  1  0 ]
///                     [ 1 -4  1 ]
///                     [ 0  1  0 ]
///
ImageBuf OIIO_API laplacian (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API laplacian (ImageBuf &dst, const ImageBuf &src,
                         ROI roi={}, int nthreads=0);


/// Return (or copy into dst) a median-filtered version of the corresponding
/// region of src. The size of the window over which the median is computed
/// is given by width and height (if height is <= 0, it will be set to
/// width, making a square filter).
///
/// Median filters are good for removing high-frequency detail smaller than
/// the window size (including noise), without blurring edges that are
/// larger than the window size.
ImageBuf OIIO_API median_filter (const ImageBuf &src,
                                 int width = 3, int height = -1,
                                 ROI roi={}, int nthreads=0);
bool OIIO_API median_filter (ImageBuf &dst, const ImageBuf &src,
                             int width = 3, int height = -1,
                             ROI roi={}, int nthreads=0);


/// Return (or copy into dst) a dilated version of the corresponding region
/// of src. Dilation is defined as the maximum value of all pixels under
/// nonzero values of the structuring element (which is taken to be a width
/// x height square). If height is not set, it will default to be the same
/// as width. Dilation makes bright features wider and more prominent, dark
/// features thinner, and removes small isolated dark spots.
ImageBuf OIIO_API dilate (const ImageBuf &src, int width=3, int height=-1,
                          ROI roi={}, int nthreads=0);
bool OIIO_API dilate (ImageBuf &dst, const ImageBuf &src,
                      int width=3, int height=-1, ROI roi={}, int nthreads=0);


/// Return (or copy into dst) an eroded version of the corresponding region
/// of src. Erosion is defined as the minimum value of all pixels under
/// nonzero values of the structuring element (which is taken to be a width
/// x height square). If height is not set, it will default to be the same
/// as width. Erosion makes dark features wider, bright features thinner,
/// and removes small isolated bright spots.
ImageBuf OIIO_API erode (const ImageBuf &src, int width=3, int height=-1,
                         ROI roi={}, int nthreads=0);
bool OIIO_API erode (ImageBuf &dst, const ImageBuf &src,
                     int width=3, int height=-1, ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the discrete Fourier transform (DFT) of the
/// section of src denoted by roi,  If roi is not defined, it will be all of
/// src's pixels.  Only one channel of src may be FFT'd at a time, so it
/// will be the first channel described by roi (or, again, channel 0 if roi
/// is undefined).  If not already in the correct format, dst will be re-
/// allocated to be a 2-channel float buffer of size width x height, with
/// channel 0 being the "real" part and channel 1 being the the "imaginary"
/// part.  The values returned are actually the unitary DFT, meaning that it
/// is scaled by 1/sqrt(npixels).
///
/// Works on all pixel data type for src; dst will always be reallocated
/// as FLOAT.
ImageBuf OIIO_API fft (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API fft (ImageBuf &dst, const ImageBuf &src, ROI roi={}, int nthreads=0);

/// Return (or copy into dst) the inverse discrete Fourier transform of the
/// section of src denoted by roi.  If roi is not defined, it will be all of
/// src's pixels.
///
/// Src MUST be a 2-channel float image, and is assumed to be a complex
/// frequency-domain signal with the "real" component in channel 0 and
/// the "imaginary" component in channel 1.  Dst will end up being a
/// float image of one channel (the real component is kept, the
/// imaginary component of the spatial-domain will be discarded).
/// Just as with fft(), the ifft() function is dealing with the unitary
/// DFT, so it is scaled by 1/sqrt(npixels).
///
ImageBuf OIIO_API ifft (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API ifft (ImageBuf &dst, const ImageBuf &src, ROI roi={}, int nthreads=0);


/// Convert a 2-channel image with "polar" values (amplitude, phase)
/// into a 2-channel image with complex values (real, imaginary).
///
/// The transformation between the two representations are:
///     real = amplitude * cos(phase);
///     imag = amplitude * sin(phase);
///
ImageBuf OIIO_API polar_to_complex (const ImageBuf &src,
                                    ROI roi={}, int nthreads=0);
bool OIIO_API polar_to_complex (ImageBuf &dst, const ImageBuf &src,
                                ROI roi={}, int nthreads=0);

/// Convert a 2-channel image with complex values (real, imaginary) into a
/// 2-channel image with "polar" values (amplitude, phase).
///
/// The transformation between the two representations are:
///     amplitude = hypot (real, imag);
///     phase = atan2 (imag, real);
///
ImageBuf OIIO_API complex_to_polar (const ImageBuf &src,
                                    ROI roi={}, int nthreads=0);
bool OIIO_API complex_to_polar (ImageBuf &dst, const ImageBuf &src,
                                ROI roi={}, int nthreads=0);


enum OIIO_API NonFiniteFixMode
{
    NONFINITE_NONE = 0,     ///< Do nothing
    NONFINITE_BLACK = 1,    ///< Replace nonfinite pixels with black
    NONFINITE_BOX3 = 2,     ///< Replace nonfinite pixels with 3x3 finite average
    NONFINITE_ERROR = 100,  ///< Error if any nonfinite pixels are found
};

/// Return (or copy into dst) a the values of src (within the ROI), while
/// repairing  any non-finite (NaN/Inf) pixels. If pixelsFixed is not
/// nullptr, store in it the number of pixels that contained non-finite
/// value.  It is permissible to operate in-place (with src and dst
/// referring to the same image).
///
/// How the non-finite values are repaired is specified by one of the
/// following modes:
///   NONFINITE_NONE   do not alter the pixels (but do count the number
///                       of nonfinite pixels in *pixelsFixed, if non-null).
///   NONFINITE_BLACK  change non-finite values to 0.
///   NONFINITE_BOX3   replace non-finite values by the average of any
///                       finite pixels within a 3x3 window.
///   NONFINITE_ERROR  return false (error), but don't change any values,
///                       if any nonfinite values are found.
///
/// Works on all pixel data types, though it's just a copy for images with
/// pixel data types that cannot represent NaN or Inf values.
///
ImageBuf OIIO_API fixNonFinite (const ImageBuf &src,
                                NonFiniteFixMode mode=NONFINITE_BOX3,
                                int *pixelsFixed = nullptr,
                                ROI roi={}, int nthreads=0);
bool OIIO_API fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode=NONFINITE_BOX3,
                            int *pixelsFixed = nullptr,
                            ROI roi={}, int nthreads=0);


/// Fill the holes using a push-pull technique.  The src image must have
/// an alpha channel.  The dst image will end up with a copy of src, but
/// will have an alpha of 1.0 everywhere, and any place where the alpha
/// of src was < 1, dst will have a pixel color that is a plausible
/// "filling" of the original alpha hole.
ImageBuf OIIO_API fillholes_pushpull (const ImageBuf &src,
                                      ROI roi={}, int nthreads=0);
bool OIIO_API fillholes_pushpull (ImageBuf &dst, const ImageBuf &src,
                                  ROI roi={}, int nthreads=0);


/// Convert an OpenCV cv::Mat into an ImageBuf, copying the pixels
/// (optionally converting to the pixel data type specified by `convert`, if
/// not UNKNOWN, which means to preserve the original data type if
/// possible).  Return true if ok, false if it couldn't figure out how to
/// make the conversion from Mat to ImageBuf. If OpenImageIO was compiled
/// without OpenCV support, this function will return an empty image with
/// error message set.
OIIO_API ImageBuf
from_OpenCV (const cv::Mat& mat, TypeDesc convert = TypeUnknown,
             ROI roi={}, int nthreads=0);

/// Construct an OpenCV cv::Mat containing the contents of ImageBuf src, and
/// return true. If it is not possible, or if OpenImageIO was compiled
/// without OpenCV support, then return false. Note that OpenCV only
/// supports up to 4 channels, so >4 channel images will be truncated in the
/// conversion.
OIIO_API bool to_OpenCV (cv::Mat& dst, const ImageBuf& src,
                         ROI roi={}, int nthreads=0);

/// Capture a still image from a designated camera.  If able to do so,
/// store the image in dst and return true.  If there is no such device,
/// or support for camera capture is not available (such as if OpenCV
/// support was not enabled at compile time), return false and do not
/// alter dst.
ImageBuf OIIO_API capture_image (int cameranum = 0,
                                 TypeDesc convert=TypeUnknown);
// DEPRECATED(1.9):
inline bool capture_image (ImageBuf &dst, int cameranum = 0,
                           TypeDesc convert=TypeUnknown) {
    dst = capture_image (cameranum, convert);
    return !dst.has_error();
}

/// Convert an IplImage, used by OpenCV and Intel's Image Libray, into an
/// ImageBuf (copying the pixels).  If convert is not set to UNKNOWN,
/// convert the IplImage to that data type. Return true if ok, false if it
/// couldn't figure out how to make the conversion from IplImage to
/// ImageBuf.  If OpenImageIO was compiled without OpenCV support, this
/// function will return an empty image with error message set.
///
/// DEPRECATED(2.0). The OpenCV 1.x era IplImage-based functions should be
/// avoided, giving preference to from_OpenCV.
ImageBuf OIIO_API from_IplImage (const IplImage *ipl,
                                 TypeDesc convert=TypeUnknown);
// DEPRECATED(1.9):
inline bool from_IplImage (ImageBuf &dst, const IplImage *ipl,
                           TypeDesc convert=TypeUnknown) {
    dst = from_IplImage (ipl, convert);
    return ! dst.has_error();
}

/// Construct an IplImage*, used by OpenCV and Intel's Image Library, that
/// is equivalent to the ImageBuf src.  If it is not possible, or if
/// OpenImageIO was compiled without OpenCV support, then return nullptr.
/// The ownership of the IplImage is fully transferred to the calling
/// application.
///
/// DEPRECATED(2.0). The OpenCV 1.x era IplImage-based functions should be
/// avoided, giving preference to from_OpenCV.
OIIO_API IplImage* to_IplImage (const ImageBuf &src);




/// Return (or copy into dst) the composite of A over B using the
/// Porter/Duff definition of "over", returning true upon success and false
/// for any of a variety of failures (as described below).
///
/// A and B (and dst, if already defined/allocated) must have valid alpha
/// channels identified by their ImageSpec alpha_channel field.  If A or
/// B do not have alpha channels (as determined by those rules) or if
/// the number of non-alpha channels do not match between A and B,
/// over() will fail, returning false.
///
/// If dst is not already an initialized ImageBuf, it will be sized to
/// encompass the minimal rectangular pixel region containing the union
/// of the defined pixels of A and B, and with a number of channels
/// equal to the number of non-alpha channels of A and B, plus an alpha
/// channel.  However, if dst is already initialized, it will not be
/// resized, and the "over" operation will apply to its existing pixel
/// data window.  In this case, dst must have an alpha channel designated
/// and must have the same number of non-alpha channels as A and B,
/// otherwise it will fail, returning false.
///
/// A, B, and dst need not perfectly overlap in their pixel data windows;
/// pixel values of A or B that are outside their respective pixel data
/// window will be treated as having "zero" (0,0,0...) value.
ImageBuf OIIO_API over (const ImageBuf &A, const ImageBuf &B,
                        ROI roi={}, int nthreads=0);
bool OIIO_API over (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi={}, int nthreads=0);


/// Just like ImageBufAlgo::over(), but inputs A and B must have
/// designated 'z' channels, and on a pixel-by-pixel basis, the z values
/// will determine which of A or B will be considered the foreground or
/// background (lower z is foreground).  If z_zeroisinf is true, then
/// z=0 values will be treated as if they are infinitely far away.
ImageBuf OIIO_API zover (const ImageBuf &A, const ImageBuf &B,
                         bool z_zeroisinf=false, ROI roi={}, int nthreads=0);
bool OIIO_API zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf=false, ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the "deep" version of "flat" input src.
/// Turning a flat image into a deep one means:
///
/// If the src image has a "Z" channel: if the source pixel's Z channel
/// value is not infinite, the corresponding pixel of dst will get a single
/// depth sample that copies the data from the soruce pixel; otherwise, dst
/// will get an empty pixel. In other words, infinitely far pixels will not
/// turn into deep samples.
///
/// If the src image lacks a "Z" channel: if any of the source pixel's
/// channel values are nonzero, the corresponding pixel of dst will get a
/// single depth sample that copies the data from the source pixel and uses
/// the zvalue parameter for the depth; otherwise, if all source channels in
/// that pixel are zero, the destination pixel will get no depth samples.
///
/// If src is already a deep image, it will just copy pixel values from src
/// to dst.
ImageBuf OIIO_API deepen (const ImageBuf &src, float zvalue = 1.0f,
                          ROI roi={}, int nthreads=0);
bool OIIO_API deepen (ImageBuf &dst, const ImageBuf &src, float zvalue = 1.0f,
                      ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the ``flattened'' composite of deep image src.
/// That is, it converts a deep image to a simple flat image by front-to-
/// back compositing the samples within each pixel.  If src is already a
/// non-deep/flat image, it will just copy pixel values from src to dst. If
/// dst is not already an initialized ImageBuf, it will be sized to match
/// src (but made non-deep).
ImageBuf OIIO_API flatten (const ImageBuf &src, ROI roi={}, int nthreads=0);
bool OIIO_API flatten (ImageBuf &dst, const ImageBuf &src,
                       ROI roi={}, int nthreads=0);


/// Return (or copy into dst) the deep merge of the samples of deep images A
/// and B, overwriting any existing samples of dst in the ROI. If
/// occlusion_cull is true, any samples occluded by an opaque sample will be
/// deleted.
ImageBuf OIIO_API deep_merge (const ImageBuf &A, const ImageBuf &B,
                              bool occlusion_cull = true,
                              ROI roi={}, int nthreads=0);
bool OIIO_API deep_merge (ImageBuf &dst, const ImageBuf &A,
                          const ImageBuf &B, bool occlusion_cull = true,
                          ROI roi={}, int nthreads=0);

/// Return (or copy into dst) the samples of deep image src that are closer
/// than the opaque frontier of deep image holdout, returning true upon
/// success and false for any failures. Samples of src that are farther than
/// the first opaque sample of holdout (for the corresponding pixel)will not
/// be copied to dst. Image holdout is only used as the depth threshold; no
/// sample values from holdout are themselves copied to dst.
ImageBuf OIIO_API deep_holdout (const ImageBuf &src, const ImageBuf &holdout,
                            ROI roi={}, int nthreads=0);
bool OIIO_API deep_holdout (ImageBuf &dst, const ImageBuf &src,
                            const ImageBuf &holdout,
                            ROI roi={}, int nthreads=0);



/// Render a single point at (x,y) of the given color "over" the existing
/// image dst. If there is no alpha channel, the color will be written
/// unconditionally (as if the alpha is 1.0).
bool OIIO_API render_point (ImageBuf &dst, int x, int y,
                            cspan<float> color=1.0f,
                            ROI roi={}, int nthreads=0);

/// Render a line from (x1,y1) to (x2,y2) of the given color "over" the
/// existing image dst. If there is no alpha channel, the color will be
/// written unconditionally (as if the alpha is 1.0).  If skip_first_point
/// is true, the very first point (x1,y1) will not be rendered; this can be
/// useful for rendering segments of poly-lines to avoid double-rendering
/// the vertex positions.
bool OIIO_API render_line (ImageBuf &dst, int x1, int y1, int x2, int y2,
                           cspan<float> color=1.0f,
                           bool skip_first_point = false,
                           ROI roi={}, int nthreads=0);

/// Render an a filled or unfilled box with corners (x1,y1) and (x2,y2) of
/// the given color "over" the existing image dst. If there is no alpha
/// channel, the color will be written unconditionally (as if the alpha is
/// 1.0).
bool OIIO_API render_box (ImageBuf &dst, int x1, int y1, int x2, int y2,
                          cspan<float> color=1.0f, bool fill = false,
                          ROI roi={}, int nthreads=0);


enum class TextAlignX { Left, Right, Center };
enum class TextAlignY { Baseline, Top, Bottom, Center };

/// Render a text string (encoded as UTF-8) into image dst. If the dst image
/// is not yet initiailzed, it will be initialized to be a black background
/// exactly large enought to contain the rasterized text.  If dst is already
/// initialized, the text will be rendered into the existing image by
/// essentially doing an "over" of the character into the existing pixel
/// data.
///
/// The font is given by fontname (if not a full pathname to a font file, it
/// will search for a matching font, defaulting to some reasonable system
/// font if not supplied at all), and with a nominal height of fontsize (in
/// pixels).
///
/// The position is given by coordinates (x,y), with the default behavior
/// to align the left edge of the character baseline to (x,y). Optionally,
/// alignx and aligny can override the alignment behavior, with horizontal
/// alignment choices of TextAlignX::Left, Right, and Center, and vertical
/// alignment choices of TextAlginY::Baseline, Top, Bottom, or Center.
///
/// The characters will be drawn in opaque white (1.0,1.0,...) in all
/// channels, unless textcolor is supplied (and is expected to point to a
/// float array of length at least equal to R.spec().nchannels, or defaults
/// will be chosen for you). If shadow is nonzero, a "drop shadow" of that
/// radius will be used to make the text look more clear by dilating the
/// alpha channel of the composite (makes a black halo around the
/// characters).
bool OIIO_API render_text (ImageBuf &dst, int x, int y, string_view text,
                           int fontsize=16, string_view fontname="",
                           cspan<float> textcolor = 1.0f,
                           TextAlignX alignx = TextAlignX::Left,
                           TextAlignY aligny = TextAlignY::Baseline,
                           int shadow = 0, ROI roi={}, int nthreads=0);


/// Helper function: how big is the text that would be drawn by render_text?
/// Returns the extent as an ROI (relative to the left edge of the baseline
/// of the first character). The size is an ROI, but only the x and y
/// dimensions are used. Failures can be detected by testing the ROI's
/// defined() property.
ROI OIIO_API text_size (string_view text, int fontsize=16,
                        string_view fontname="");



/// Compute a histogram of src, for the given channel and ROI. Return a
/// vector of length `bins` that contains the counts of how many pixel
/// values were in each of `bins` equally spaced bins covering the range of
/// values [min,max]. Values < min count for bin 0, values > max count for
/// bin `nbins-1`. If ignore_empty is true, no counts will be incremented
/// for any pixels that are 0 in all channels.
OIIO_API
std::vector<imagesize_t> histogram (const ImageBuf &src, int channel=0,
                                    int bins=256, float min=0.0f, float max=1.0f,
                                    bool ignore_empty=false,
                                    ROI roi={}, int nthreads=0);


/// DEPRECATED(1.9)
bool OIIO_API histogram (const ImageBuf &src, int channel,
                         std::vector<imagesize_t> &histogram, int bins=256,
                         float min=0, float max=1, imagesize_t *submin=nullptr,
                         imagesize_t *supermax=nullptr, ROI roi={});

// DEPRECATED(1.9): never liked this.
bool OIIO_API histogram_draw (ImageBuf &dst,
                              const std::vector<imagesize_t> &histogram);



enum OIIO_API MakeTextureMode {
    MakeTxTexture, MakeTxShadow, MakeTxEnvLatl,
    MakeTxEnvLatlFromLightProbe,
    MakeTxBumpWithSlopes,
    _MakeTxLast
};

/// Turn an image into a tiled, MIP-mapped, texture file and write it to
/// disk (outputfilename).  The 'mode' describes what type of texture
/// file we are creating and may be one of:
///    MakeTxTexture    Ordinary 2D texture
///    MakeTxEnvLatl    Latitude-longitude environment map
///    MakeTxEnvLatlFromLightProbe   Latitude-longitude environment map
///                     constructed from a "light probe" image.
///
/// If the outstream pointer is not NULL, it should point to a stream
/// (for example, &std::out, or a pointer to a local std::stringstream
/// to capture output), which is where console output and error messages
/// will be deposited.
///
/// The 'config' is an ImageSpec that contains all the information and
/// special instructions for making the texture.  Anything set in config
/// (format, tile size, or named metadata) will take precedence over
/// whatever is specified by the input file itself.  Additionally, named
/// metadata that starts with "maketx:" will not be output to the file
/// itself, but may contain instructions controlling how the texture is
/// created.  The full list of supported configuration options is:
///
/// Named fields:
///    format         Data format of the texture file (default: UNKNOWN =
///                     same format as the input)
///    tile_width     Preferred tile size (default: 64x64x1)
///    tile_height    
///    tile_depth     
/// Metadata in config.extra_attribs:
///    compression (string)   Default: "zip"
///    fovcot (float)         Default: aspect ratio of the image resolution
///    planarconfig (string)  Default: "separate"
///    worldtocamera (matrix) World-to-camera matrix of the view.
///    worldtoscreen (matrix) World-to-screen space matrix of the view.
///    wrapmodes (string)     Default: "black,black"
///    maketx:verbose (int)   How much detail should go to outstream (0).
///    maketx:runstats (int)  If nonzero, print run stats to outstream (0).
///    maketx:resize (int)    If nonzero, resize to power of 2. (0)
///    maketx:nomipmap (int)  If nonzero, only output the top MIP level (0).
///    maketx:updatemode (int) If nonzero, write new output only if the
///                              output file doesn't already exist, or is
///                              older than the input file, or was created
///                              with different command-line arguments. (0)
///    maketx:constant_color_detect (int)
///                           If nonzero, detect images that are entirely
///                             one color, and change them to be low
///                             resolution (default: 0).
///    maketx:monochrome_detect (int)
///                           If nonzero, change RGB images which have 
///                              R==G==B everywhere to single-channel 
///                              grayscale (default: 0).
///    maketx:opaque_detect (int)
///                           If nonzero, drop the alpha channel if alpha
///                              is 1.0 in all pixels (default: 0).
///    maketx:compute_average (int)
///                           If nonzero, compute and store the average
///                              color of the texture (default: 1).
///    maketx:unpremult (int) If nonzero, unpremultiply color by alpha before
///                              color conversion, then multiply by alpha
///                              after color conversion (default: 0).
///    maketx:incolorspace (string)
///    maketx:outcolorspace (string) 
///                           These two together will apply a color conversion
///                               (with OpenColorIO, if compiled). Default: ""
///    maketx:colorconfig (string)
///                           Specifies a custom OpenColorIO color config
///                           file. Default: ""
///    maketx:checknan (int)  If nonzero, will consider it an error if the
///                               input image has any NaN pixels. (0)
///    maketx:fixnan (string) If set to "black" or "box3", will attempt
///                               to repair any NaN pixels found in the
///                               input image (default: "none").
///    maketx:set_full_to_pixels (int)
///                           If nonzero, doctors the full/display window
///                               of the texture to be identical to the
///                               pixel/data window and reset the origin
///                               to 0,0 (default: 0).
///    maketx:filtername (string)
///                           If set, will specify the name of a high-quality
///                              filter to use when resampling for MIPmap
///                              levels. Default: "", use bilinear resampling.
///    maketx:highlightcomp (int)
///                           If nonzero, performs highlight compensation --
///                              range compression and expansion around
///                              the resize, plus clamping negative plxel
///                              values to zero. This reduces ringing when
///                              using filters with negative lobes on HDR
///                              images.
///    maketx:sharpen (float) If nonzero, sharpens details when creating
///                              MIPmap levels. The amount is the contrast
///                              matric. The default is 0, meaning no
///                              sharpening.
///    maketx:nchannels (int) If nonzero, will specify how many channels
///                              the output texture should have, padding with
///                              0 values or dropping channels, if it doesn't
///                              the number of channels in the input.
///                              (default: 0, meaning keep all input channels)
///    maketx:channelnames (string)
///                           If set, overrides the channel names of the
///                              output image (comma-separated).
///    maketx:fileformatname (string)
///                           If set, will specify the output file format.
///                               (default: "", meaning infer the format from
///                               the output filename)
///    maketx:prman_metadata (int)
///                           If set, output some metadata that PRMan will
///                               need for its textures. (0)
///    maketx:oiio_options (int)
///                           (Deprecated; all are handled by default)
///    maketx:prman_options (int)
///                           If nonzero, override a whole bunch of settings
///                               as needed to make textures that are
///                               compatible with PRMan. (0)
///    maketx:mipimages (string)
///                           Semicolon-separated list of alternate images
///                               to be used for individual MIPmap levels,
///                               rather than simply downsizing. (default: "")
///    maketx:full_command_line (string)
///                           The command or program used to generate this
///                               call, will be embedded in the metadata.
///                               (default: "")
///    maketx:ignore_unassoc (int)
///                           If nonzero, will disbelieve any evidence that
///                               the input image is unassociated alpha. (0)
///    maketx:read_local_MB (int)
///                           If nonzero, will read the full input file locally
///                               if it is smaller than this threshold. Zero
///                               causes the system to make a good guess at
///                               a reasonable threshold (e.g. 1 GB). (0)
///    maketx:forcefloat (int)
///                           Forces a conversion through float data for
///                               the sake of ImageBuf math. (1)
///    maketx:hash (int)
///                           Compute the sha1 hash of the file in parallel. (1)
///    maketx:allow_pixel_shift (int)
///                           Allow up to a half pixel shift per mipmap level.
///                               The fastest path may result in a slight shift
///                               in the image, accumulated for each mip level
///                               with an odd resolution. (0)
///    maketx:bumpformat (string)
///                           For the MakeTxBumpWithSlopes mode, chooses
///                               whether to assume the map is a height map
///                               ("height"), a normal map ("normal"), or
///                               automatically determine it from the number
///                               of channels ("auto", the default).
///
bool OIIO_API make_texture (MakeTextureMode mode,
                            const ImageBuf &input,
                            string_view outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = nullptr);

/// Version of make_texture that starts with a filename and reads the input
/// from that file, rather than being given an ImageBuf directly.
bool OIIO_API make_texture (MakeTextureMode mode,
                            string_view filename,
                            string_view outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = nullptr);

/// Version of make_texture that takes multiple filenames (reserved for
/// future expansion, such as assembling several faces into a cube map).
bool OIIO_API make_texture (MakeTextureMode mode,
                            const std::vector<std::string> &filenames,
                            string_view outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = nullptr);


///////////////////////////////////////////////////////////////////////
// DEPRECATED(1.9): These are all functions that take raw pointers,
// which we are deprecating as of 1.9, replaced by new versions that
// take span<> for length safety.

inline bool fill (ImageBuf &dst, const float *values,
                  ROI roi={}, int nthreads=0) {
    int nc (roi.defined() ? roi.nchannels() : dst.nchannels());
    return fill (dst, {values, nc}, roi, nthreads);
}
inline bool fill (ImageBuf &dst, const float *top, const float *bottom,
                  ROI roi={}, int nthreads=0) {
    int nc (roi.defined() ? roi.nchannels() : dst.nchannels());
    return fill (dst, {top, nc}, {bottom, nc}, roi, nthreads);
}
inline bool fill (ImageBuf &dst, const float *topleft, const float *topright,
                  const float *bottomleft, const float *bottomright,
                  ROI roi={}, int nthreads=0) {
    int nc (roi.defined() ? roi.nchannels() : dst.nchannels());
    return fill (dst, {topleft, nc}, {topright, nc}, {bottomleft, nc},
                 {bottomright, nc}, roi, nthreads);
}

inline bool checker (ImageBuf &dst, int width, int height, int depth,
                     const float *color1, const float *color2,
                     int xoffset=0, int yoffset=0, int zoffset=0,
                     ROI roi={}, int nthreads=0) {
    int nc (roi.defined() ? roi.nchannels() : dst.nchannels());
    return checker (dst, width, height, depth, {color1,nc}, {color2,nc},
                    xoffset, yoffset, zoffset, roi, nthreads);
}

inline bool add (ImageBuf &dst, const ImageBuf &A, const float *B,
                 ROI roi={}, int nthreads=0) {
    return add (dst, A, {B,A.nchannels()}, roi, nthreads);
}
inline bool sub (ImageBuf &dst, const ImageBuf &A, const float *B,
                 ROI roi={}, int nthreads=0) {
    return sub (dst, A, {B,A.nchannels()}, roi, nthreads);
}
inline bool absdiff (ImageBuf &dst, const ImageBuf &A, const float *B,
                     ROI roi={}, int nthreads=0) {
    return absdiff (dst, A, cspan<float>(B,A.nchannels()), roi, nthreads);
}
inline bool mul (ImageBuf &dst, const ImageBuf &A, const float *B,
                 ROI roi={}, int nthreads=0) {
    return mul (dst, A, {B, A.nchannels()}, roi, nthreads);
}
inline bool div (ImageBuf &dst, const ImageBuf &A, const float *B,
                 ROI roi={}, int nthreads=0) {
    return div (dst, A, {B, A.nchannels()}, roi, nthreads);
}
inline bool mad (ImageBuf &dst, const ImageBuf &A, const float *B,
                 const ImageBuf &C, ROI roi={}, int nthreads=0) {
    return mad (dst, A, {B, A.nchannels()}, C, roi, nthreads);
}
inline bool mad (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                 const float *C, ROI roi={}, int nthreads=0) {
    return mad (dst, A, C, B, roi, nthreads);
}
inline bool mad (ImageBuf &dst, const ImageBuf &A, const float *B,
                 const float *C, ROI roi={}, int nthreads=0) {
    return mad (dst, A, {B, A.nchannels()}, {C, A.nchannels()}, roi, nthreads);
}

inline bool pow (ImageBuf &dst, const ImageBuf &A, const float *B,
                 ROI roi={}, int nthreads=0) {
    return pow (dst, A, {B, A.nchannels()}, roi, nthreads);
}

inline bool channel_sum (ImageBuf &dst, const ImageBuf &src,
                         const float *weights=nullptr, ROI roi={},
                         int nthreads=0) {
    return channel_sum (dst, src, {weights, src.nchannels()},
                        roi, nthreads);
}

inline bool channels (ImageBuf &dst, const ImageBuf &src,
                      int nchannels, const int *channelorder,
                      const float *channelvalues=nullptr,
                      const std::string *newchannelnames=nullptr,
                      bool shuffle_channel_names=false, int nthreads=0) {
    return channels (dst, src, nchannels,
                     { channelorder, channelorder?nchannels:0 },
                     { channelvalues, channelvalues?nchannels:0 },
                     { newchannelnames, newchannelnames?nchannels:0},
                     shuffle_channel_names, nthreads);
}

inline bool clamp (ImageBuf &dst, const ImageBuf &src,
                   const float *min=nullptr, const float *max=nullptr,
                   bool clampalpha01 = false,
                   ROI roi={}, int nthreads=0) {
    return clamp (dst, src, { min, min ? src.nchannels() : 0 },
                  { max, max ? src.nchannels() : 0 }, clampalpha01,
                  roi, nthreads);
}

inline bool isConstantColor (const ImageBuf &src, float *color,
                             ROI roi={}, int nthreads=0) {
    int nc = roi.defined() ? std::min(roi.chend,src.nchannels()) : src.nchannels();
    return isConstantColor (src, {color, color ? nc : 0}, roi, nthreads);
}

inline bool color_count (const ImageBuf &src, imagesize_t *count,
                         int ncolors, const float *color,
                         const float *eps=nullptr,
                         ROI roi={}, int nthreads=0) {
    return color_count (src, count, ncolors,
                        { color, ncolors*src.nchannels() },
                        eps ? cspan<float>(eps,src.nchannels()) : cspan<float>(),
                        roi, nthreads);
}

inline bool color_range_check (const ImageBuf &src, imagesize_t *lowcount,
                               imagesize_t *highcount, imagesize_t *inrangecount,
                               const float *low, const float *high,
                               ROI roi={}, int nthreads=0) {
    return color_range_check (src, lowcount, highcount, inrangecount,
                              {low,src.nchannels()}, {high,src.nchannels()},
                              roi, nthreads);
}

inline bool render_text (ImageBuf &dst, int x, int y, string_view text,
                         int fontsize, string_view fontname,
                         const float *textcolor) {
    return render_text (dst, x, y, text, fontsize, fontname,
                        {textcolor, textcolor?dst.nchannels():0});
}

///////////////////////////////////////////////////////////////////////

}  // end namespace ImageBufAlgo

OIIO_NAMESPACE_END
