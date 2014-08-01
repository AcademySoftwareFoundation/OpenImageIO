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

#include <limits>

#ifndef __OPENCV_CORE_TYPES_H__
struct IplImage;  // Forward declaration; used by Intel Image lib & OpenCV
#endif



OIIO_NAMESPACE_ENTER
{

class Filter2D;  // forward declaration



/// Some generalities about ImageBufAlgo functions:
///
/// All IBA functions take a ROI.  Only the pixels (and channels) in dst
/// that are specified by the ROI will be altered; the default ROI is to
/// alter all the pixels in dst.  Exceptions will be noted, including 
/// functions that do not honor their channel range.
///
/// In general, IBA functions that are passed an initialized 'dst' or
/// 'result' image do not reallocate it or alter its existing pixels
/// that lie outside the ROI (exceptions will be noted). If passed an
/// uninitialized result image, it will be reallocatd to be the size of
/// the ROI (and with float pixels).  If the result image passed is
/// uninitialized and also the ROI is undefined, the ROI will be the
/// union of the pixel data regions of any input images.  (A small
/// number of IBA functions, such as fill(), have only a result image
/// and no input image; in such cases, it's an error to have both an
/// uninitiailized result image and an undefined ROI.)
///
/// IBA functions that have an 'nthreads' parameter use it to specify
/// how many threads (potentially) may be used, but it's not a
/// guarantee.  If nthreads == 0, it will use the global OIIO attribute
/// "nthreads".  If nthreads == 1, it guarantees that it will not launch
/// any new threads.
///
/// All IBA functions return true on success, false on error (with an
/// appropriate error message set in dst).



namespace ImageBufAlgo {

/// Zero out (set to 0, black) the image region.
///
/// Only the pixels (and channels) in dst that are specified by roi will
/// be altered; the default roi is to alter all the pixels in dst.
///
/// If dst is uninitialized, it will be resized to be a float ImageBuf
/// large enough to hold the region specified by roi.  It is an error to
/// pass both an uninitialied dst and an undefined roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API zero (ImageBuf &dst, ROI roi=ROI::All(), int nthreads=0);


/// Fill the image region with given channel values.  Note that the
/// values pointer starts with channel 0, even if the ROI indicates that
/// a later channel is the first to be changed.
///
/// Only the pixels (and channels) in dst that are specified by roi will
/// be altered; the default roi is to alter all the pixels in dst.
///
/// If dst is uninitialized, it will be resized to be a float ImageBuf
/// large enough to hold the region specified by roi.  It is an error to
/// pass both an uninitialied dst and an undefined roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API fill (ImageBuf &dst, const float *values,
                    ROI roi=ROI::All(), int nthreads=0);



/// Fill a subregion of the volume with a checkerboard with origin
/// (xoffset,yoffset,zoffset) and that alternates between color1[] and
/// color2[] every width pixels in x, every height pixels in y, and
/// every depth pixels in z.  The pattern is definied in abstract "image
/// space" independently of the pixel data window of dst or the ROI.
///
/// Only the pixels (and channels) in dst that are specified by roi will
/// be altered; the default roi is to alter all the pixels in dst.
///
/// If dst is uninitialized, it will be resized to be a float ImageBuf
/// large enough to hold the region specified by roi.  It is an error
/// to pass both an uninitialied dst and an undefined roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API checker (ImageBuf &dst, int width, int height, int depth,
                       const float *color1, const float *color2,
                       int xoffset=0, int yoffset=0, int zoffset=0,
                       ROI roi=ROI::All(), int nthreads=0);



/// Generic channel shuffling -- copy src to dst, but with channels in
/// the order channelorder[0..nchannels-1].  Does not support in-place
/// operation.  For any channel in which channelorder[i] < 0, it will
/// just make dst channel i a constant color -- set to channelvalues[i]
/// (if channelvalues != NULL) or 0.0 (if channelvalues == NULL).
///
/// If channelorder is NULL, it will be interpreted as
/// {0, 1, ..., nchannels-1}, meaning that it's only renaming channels,
/// not reordering them.
///
/// If newchannelnames is not NULL, it points to an array of new channel
/// names.  Channels for which newchannelnames[i] is the empty string (or
/// all channels, if newchannelnames == NULL) will be named as follows:
/// If shuffle_channel_names is false, the resulting dst image will have
/// default channel names in the usual order ("R", "G", etc.), but if
/// shuffle_channel_names is true, the names will be taken from the
/// corresponding channels of the source image -- be careful with this,
/// shuffling both channel ordering and their names could result in no
/// semantic change at all, if you catch the drift.
///
/// N.B. If you are merely interested in extending the number of channels
/// or truncating channels at the end (but leaving the other channels
/// intact), then you should call this as:
///    channels (dst, src, nchannels, NULL, NULL, NULL, true);
bool OIIO_API channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues=NULL,
                        const std::string *newchannelnames=NULL,
                        bool shuffle_channel_names=false);


/// Append the channels of A and B together into dst over the region of
/// interest.  If the region passed is uninitialized (the default), it
/// will be interpreted as being the union of the pixel windows of A and
/// B (and all channels of both images).  If dst is not already
/// initialized, it will be resized to be big enough for the region.
bool OIIO_API channel_append (ImageBuf &dst, const ImageBuf &A,
                              const ImageBuf &B, ROI roi=ROI::All(),
                              int nthreads=0);


/// Set dst to the ``flattened'' composite of deep image src.  That is, it
/// converts a deep image to a simple flat image by front-to-back
/// compositing the samples within each pixel.  If src is already a non-
/// deep/flat image, it will just copy pixel values from src to dst. If dst
/// is not already an initialized ImageBuf, it will be sized to match src
/// (but made non-deep).
///
/// 'roi' specifies the region of dst's pixels which will be computed;
/// existing pixels outside this range will not be altered.  If not
/// specified, the default ROI value will be the pixel data window of src.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
bool OIIO_API flatten (ImageBuf &dst, const ImageBuf &src,
                       ROI roi = ROI::All(), int nthreads = 0);


/// Reset dst to be the specified region of src.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API crop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi = ROI::All(), int nthreads = 0);


/// Assign to dst the designated region of src, but shifted to be at the
/// (0,0) origin, and with the full/display resolution set to be identical
/// to the data region.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API cut (ImageBuf &dst, const ImageBuf &src,
                   ROI roi=ROI::All(), int nthreads = 0);


/// Copy into dst, beginning at (xbegin,ybegin,zbegin), the pixels of
/// src described by srcroi.  If srcroi is ROI::All(), the entirety of src
/// will be used.  It will copy into channels [chbegin...], as many
/// channels as are described by srcroi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin,
                     const ImageBuf &src, ROI srcroi=ROI::All(),
                     int nthreads = 0);


/// Copy src to dst, but with the image pixels rotated 90 degrees
/// clockwise. In other words,
///     AB  -->  CA
///     CD       DB
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API rotate90 (ImageBuf &dst, const ImageBuf &src,
                        ROI roi=ROI::All(), int nthreads=0);

/// Copy src to dst, but with the image pixels rotated 180 degrees.
/// In other words,
///     AB  -->  DC
///     CD       BA
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API rotate180 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi=ROI::All(), int nthreads=0);
/// DEPRECATED(1.5) synonym for rotate180.
bool OIIO_API flipflop (ImageBuf &dst, const ImageBuf &src,
                        ROI roi=ROI::All(), int nthreads=0);

/// Copy src to dst, but with the image pixels rotated 90 degrees
/// clockwise. In other words,
///     AB  -->  BD
///     CD       AC
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API rotate270 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi=ROI::All(), int nthreads=0);

/// Copy src to dst, but with the scanlines exchanged vertically within
/// the display/full window. In other words,
///     AB  -->   CD
///     CD        AB
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API flip (ImageBuf &dst, const ImageBuf &src,
                    ROI roi=ROI::All(), int nthreads=0);

/// Copy src to dst, but with the columns exchanged horizontally within
/// the display/full window. In other words,
///     AB  -->  BA
///     CD       DC
///
/// Only the pixels (and channels) in src that are specified by roi will be
/// copied to their corresponding positions in dst; the default roi is to
/// copy the whole data region of src. If dst is uninitialized, it will be
/// resized to be a float ImageBuf large enough to hold the region specified
/// by roi. It is an error to pass both an uninitialied dst and an undefined
/// roi.
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API flop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi=ROI::All(), int nthreads=0);

/// Copy src to dst, but with whatever seties of rotations, flips, or flops
/// are necessary to transform the pixels into the configuration suggested
/// by the Orientation metadata of the image (and the Orientation metadata
/// is then set to 1, ordinary orientation).
///
/// The nthreads parameter specifies how many threads (potentially) may be
/// used, but it's not a guarantee.  If nthreads == 0, it will use the
/// global OIIO attribute "nthreads".  If nthreads == 1, it guarantees that
/// it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API reorient (ImageBuf &dst, const ImageBuf &src,
                        int nthreads=0);

/// Copy a subregion of src to the corresponding transposed (x<->y)
/// pixels of dst.  In other words, for all (x,y) within the ROI, set
/// dst[y,x] = src[x,y].
///     AB  -->  AC
///     CD       BD
///
/// Only the pixels (and channels) of src that are specified by roi will
/// be copied to dst; the default roi is to alter all the pixels in dst.
/// If dst is uninitialized, it will be resized to be an ImageBuf large
/// enough to hold the region specified by the transposed roi.  It is an
/// error to pass both an uninitialied dst and an undefined roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API transpose (ImageBuf &dst, const ImageBuf &src,
                         ROI roi=ROI::All(), int nthreads=0);


/// Copy a subregion of src to the pixels of dst, but circularly
/// shifting by the given amount.  To clarify, the circular shift
/// of [0,1,2,3,4,5] by +2 is [4,5,0,1,2,3].
///
/// Only the pixels (and channels) of src that are specified by roi will
/// be copied to dst; the default roi is to alter all the pixels in dst.
/// If dst is uninitialized, it will be resized to be an ImageBuf large
/// enough to hold the region specified by the transposed roi.  It is an
/// error to pass both an uninitialied dst and an undefined roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API circular_shift (ImageBuf &dst, const ImageBuf &src,
                              int xshift, int yshift, int zshift=0,
                              ROI roi=ROI::All(), int nthreads=0);


/// Copy pixels from src to dst (within the ROI), clamping the values
/// as follows:
/// min[0..nchans-1] specifies the minimum clamp value for each channel
/// (if min is NULL, no minimum clamping is performed).
/// max[0..nchans-1] specifies the maximum clamp value for each channel
/// (if max is NULL, no maximum clamping is performed).
/// If clampalpha01 is true, then additionally any alpha channel is
/// clamped to the 0-1 range.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API clamp (ImageBuf &dst, const ImageBuf &src,
                     const float *min=NULL, const float *max=NULL,
                     bool clampalpha01 = false,
                     ROI roi = ROI::All(), int nthreads = 0);

/// Copy pixels from src to dst (within the ROI), clamping the values of
/// as follows:
/// All channels are clamped to [min,max].
/// If clampalpha01 is true, then additionally any alpha channel is
/// clamped to the 0-1 range.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API clamp (ImageBuf &dst, const ImageBuf &src,
                     float min=-std::numeric_limits<float>::max(),
                     float max=std::numeric_limits<float>::max(),
                     bool clampalpha01 = false,
                     ROI roi = ROI::All(), int nthreads = 0);

/// DEPRECATED (1.3) in-place version
bool OIIO_API clamp (ImageBuf &dst, 
                     const float *min=NULL, const float *max=NULL,
                     bool clampalpha01 = false,
                     ROI roi = ROI::All(), int nthreads = 0);

/// DEPRECATED (1.3) in-place version
bool OIIO_API clamp (ImageBuf &dst, 
                     float min=-std::numeric_limits<float>::max(),
                     float max=std::numeric_limits<float>::max(),
                     bool clampalpha01 = false,
                     ROI roi = ROI::All(), int nthreads = 0);

/// For all pixels within the designated region, set dst = A + B.
/// All three images must have the same number of channels.
///
/// If roi is not initialized, it will be set to the union of the pixel
/// regions of A and B.  If dst is not initialized, it will be sized
/// based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works only for pixel types float, half, uint8, uint16.
/// It is permitted for dst and A to be the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within the designated region, set 
/// dst = A + B.  (B must point to nchannels floats.)
///
/// If roi is not initialized, it will be set to the pixel region of A.
/// If dst is not initialized, it will be sized based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types. It is permitted for dst and A to be the
/// same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API add (ImageBuf &dst, const ImageBuf &A, const float *B,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within the designated region, set 
/// dst = A + B.  (B is a single float that is added to all channels.)
///
/// If roi is not initialized, it will be set to the pixel region of A.
/// If dst is not initialized, it will be sized based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types. It is permitted for dst and A to be the
/// same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API add (ImageBuf &dst, const ImageBuf &A, float B,
                   ROI roi=ROI::All(), int nthreads=0);

/// DEPRECATED as of 1.3 -- in-place add
bool OIIO_API add (ImageBuf &dst, float val,
                   ROI roi=ROI::All(), int nthreads=0);
/// DEPRECATED as of 1.3 -- in-place add
bool OIIO_API add (ImageBuf &dst, const float *val,
                   ROI roi=ROI::All(), int nthreads=0);


/// For all pixels within the designated ROI, compute dst = A - B.
/// All three images must have the same number of channels.
///
/// If roi is not initialized, it will be set to the union of the pixel
/// regions of A and B.  If dst is not initialized, it will be sized
/// based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works only for pixel types float, half, uint8, uint16.
/// It is permitted for dst and A to be the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API sub (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within the designated region, set 
/// dst = A - B.  (B must point to nchannels floats.)
///
/// If roi is not initialized, it will be set to the pixel region of A.
/// If dst is not initialized, it will be sized based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types. It is permitted for dst and A to be the
/// same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API sub (ImageBuf &dst, const ImageBuf &A, const float *B,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within the designated region, set 
/// dst = A - B.  (B is a single float that is added to all channels.)
///
/// If roi is not initialized, it will be set to the pixel region of A.
/// If dst is not initialized, it will be sized based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types. It is permitted for dst and A to be the
/// same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API sub (ImageBuf &dst, const ImageBuf &A, float B,
                   ROI roi=ROI::All(), int nthreads=0);


/// For all pixels within the designated ROI, compute dst = A * B.
/// All three images must have the same number of channels.
///
/// If roi is not initialized, it will be set to the union of the pixel
/// regions of A and B.  If dst is not initialized, it will be sized
/// based on roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works only for pixel types float, half, uint8, uint16.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API mul (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi=ROI::All(), int nthreads=0);


/// For all pixels and channels of dst within region roi (defaulting to
/// all the defined pixels of dst), set dst = A * B.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.  It is permissible for dst and A to be
/// the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API mul (ImageBuf &dst, const ImageBuf &A, float B,
                   ROI roi=ROI::All(), int nthreads=0);

/// DEPRECATED in-place version. (1.3)
bool OIIO_API mul (ImageBuf &dst, float val,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within region roi (defaulting to
/// all the defined pixels of dst), set dst = A * B.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.  It is permissible for dst and A to be
/// the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API mul (ImageBuf &dst, const ImageBuf &A, const float *B,
                   ROI roi=ROI::All(), int nthreads=0);

/// DEPRECATED in-place version. (1.3)
bool OIIO_API mul (ImageBuf &dst, const float *val,
                   ROI roi=ROI::All(), int nthreads=0);


/// For all pixels and channels of dst within region roi (defaulting to
/// all the defined pixels of dst), set dst = A ^ b. (raise to power)
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.  It is permissible for dst and A to be
/// the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API pow (ImageBuf &dst, const ImageBuf &A, float B,
                   ROI roi=ROI::All(), int nthreads=0);

/// For all pixels and channels of dst within region roi (defaulting to
/// all the defined pixels of R), set R = A ^ b. (raise to power)
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.  It is permissible for dst and A to be
/// the same image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API pow (ImageBuf &dst, const ImageBuf &A, const float *B,
                   ROI roi=ROI::All(), int nthreads=0);


/// Converts a multi-channel image into a 1-channel image via a weighted
/// sum of channels.  For each pixel of src within the designated ROI
/// (defaulting to all of src, if not defined), sum the channels
/// designated by roi and store the result in channel 0 of dst.  If
/// weights is not NULL, weight[i] will provide a per-channel weight
/// (rather than defaulting to 1.0 for each channel).
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API channel_sum (ImageBuf &dst, const ImageBuf &src,
                           const float *weights=NULL, ROI roi=ROI::All(),
                           int nthreads=0);

/// For all pixels and color channels within region roi (defaulting to all
/// the defined pixels of dst), copy pixels from src to dst, rescaling their
/// range with a logarithmic transformation. Alpha and z channels are not
/// transformed.  If dst is not already defined and allocated, it will be
/// sized based on src and roi.
///
/// If useluma is true, the luma of channels [roi.chbegin..roi.chbegin+2]
/// (presumed to be R, G, and B) are used to compute a single scale
/// factor for all color channels, rather than scaling all channels
/// individually (which could result in a color shift).
///
/// Some image operations (such as resizing with a "good" filter that
/// contains negative lobes) can have objectionable artifacts when applied
/// to images with very high-contrast regions involving extra bright pixels
/// (such as highlights in HDR captured or rendered images).  By compressing
/// the range pixel values, then performing the operation, then expanding
/// the range of the result again, the result can be much more pleasing
/// (even if not exactly correct).
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API rangecompress (ImageBuf &dst, const ImageBuf &src,
                             bool useluma = false,
                             ROI roi = ROI::All(), int nthreads=0);

/// rangeexpand is the opposite operation of rangecompress -- rescales
/// the logarithmic color channel values back to a linear response.
bool OIIO_API rangeexpand (ImageBuf &dst, const ImageBuf &src,
                           bool useluma = false,
                           ROI roi = ROI::All(), int nthreads=0);

/// DEPRECATED in-place version (1.3)
bool OIIO_API rangecompress (ImageBuf &dst, bool useluma = false,
                             ROI roi = ROI::All(), int nthreads=0);

/// DEPRECATED in-place version (1.3)
bool OIIO_API rangeexpand (ImageBuf &dst, bool useluma = false,
                           ROI roi = ROI::All(), int nthreads=0);


/// Copy pixels within the ROI from src to dst, applying a color transform.
///
/// If dst is not yet initialized, it will be allocated to the same
/// size as specified by roi.  If roi is not defined it will be all
/// of dst, if dst is defined, or all of src, if dst is not yet defined.
///
/// In-place operations (dst == src) are supported.
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You may want to use this
/// flag if your image contains an alpha channel.
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API colorconvert (ImageBuf &dst, const ImageBuf &src,
                            const char *from, const char *to,
                            bool unpremult=false,
                            ROI roi=ROI::All(), int nthreads=0);

/// Copy pixels within the ROI from src to dst, applying an OpenColorIO
/// "look" transform.
///
/// If dst is not yet initialized, it will be allocated to the same
/// size as specified by roi.  If roi is not defined it will be all
/// of dst, if dst is defined, or all of src, if dst is not yet defined.
///
/// In-place operations (dst == src) are supported.
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You may want to use this
/// flag if your image contains an alpha channel.
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API ociolook (ImageBuf &dst, const ImageBuf &src,
                        const char *looks, const char *from, const char *to,
                        bool unpremult=false, bool inverse=false,
                        const char *key=NULL, const char *value=NULL,
                        ROI roi=ROI::All(), int nthreads=0);

/// Copy pixels within the ROI from src to dst, applying an OpenColorIO
/// "display" transform.  If from or looks are NULL, it will not
/// override the look or source color space (subtly different than
/// passing "", the empty string, which means to use no look or source
/// space).
///
/// If dst is not yet initialized, it will be allocated to the same
/// size as specified by roi.  If roi is not defined it will be all
/// of dst, if dst is defined, or all of src, if dst is not yet defined.
/// In-place operations (dst == src) are supported.
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You may want to use this
/// flag if your image contains an alpha channel.
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API ociodisplay (ImageBuf &dst, const ImageBuf &src,
                        const char *display, const char *view,
                        const char *from=NULL, const char *looks=NULL,
                        bool unpremult=false,
                        const char *key=NULL, const char *value=NULL,
                        ROI roi=ROI::All(), int nthreads=0);

/// Copy pixels within the ROI from src to dst, applying a color transform.
///
/// If dst is not yet initialized, it will be allocated to the same
/// size as specified by roi.  If roi is not defined it will be all
/// of dst, if dst is defined, or all of src, if dst is not yet defined.
///
/// In-place operations (dst == src) are supported.
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You may want to use this
/// flag if your image contains an alpha channel.
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API colorconvert (ImageBuf &dst, const ImageBuf &src,
                            const ColorProcessor *processor,
                            bool unpremult,
                            ROI roi=ROI::All(), int nthreads=0);

/// Apply a color transform in-place to just one color:
/// color[0..nchannels-1].  nchannels should either be 3 or 4 (if 4, the
/// last channel is alpha).
///
/// If unpremult is true, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You'll may want to use this
/// flag if your image contains an alpha channel.
bool OIIO_API colorconvert (float *color, int nchannels,
                            const ColorProcessor *processor, bool unpremult);


/// Copy pixels from dst to src, and in the process divide all color
/// channels (those not alpha or z) by the alpha value, to "un-premultiply"
/// them.  This presumes that the image starts of as "associated alpha"
/// a.k.a. "premultipled."  The alterations are restricted to the pixels and
/// channels of the supplied ROI (which defaults to all of src).  Pixels in
/// which the alpha channel is 0 will not be modified (since the operation
/// is undefined in that case).  This is just a copy if there is no
/// identified alpha channel (and a no-op if dst and src are the same
/// image).
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API unpremult (ImageBuf &dst, const ImageBuf &src,
                         ROI roi = ROI::All(), int nthreads = 0);

/// DEPRECATED (1.3) in-place version
bool OIIO_API unpremult (ImageBuf &dst, ROI roi = ROI::All(), int nthreads = 0);

/// Copy pixels from dst to src, and in the process multiply all color
/// channels (those not alpha or z) by the alpha value, to "-premultiply"
/// them.  This presumes that the image starts off as "unassociated alpha"
/// a.k.a. "non-premultiplied."  The alterations are restricted to the
/// pixels and channels of the supplied ROI (which defaults to all of src).
/// Pixels in which the alpha channel is 0 will not be modified (since the
/// operation is undefined in that case).  This is just a copy if there is
/// no identified alpha channel (and a no-op if dst and src are the same
/// image).
///
/// For all dst pixels and channels within the ROI, divide all color
/// channels (those not alpha or z) by the alpha, to "un-premultiply"
/// them.
///
/// Works with all data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API premult (ImageBuf &dst, const ImageBuf &src,
                       ROI roi = ROI::All(), int nthreads = 0);

/// DEPRECATED (1.3) in-place version
bool OIIO_API premult (ImageBuf &dst, ROI roi = ROI::All(), int nthreads = 0);



struct OIIO_API PixelStats {
    std::vector<float> min;
    std::vector<float> max;
    std::vector<float> avg;
    std::vector<float> stddev;
    std::vector<imagesize_t> nancount;
    std::vector<imagesize_t> infcount;
    std::vector<imagesize_t> finitecount;
    std::vector<double> sum, sum2;  // for intermediate calculation
};


/// Compute statistics about the ROI of the specified image. Upon success,
/// the returned vectors will have size == src.nchannels().
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API computePixelStats (PixelStats &stats, const ImageBuf &src,
                                 ROI roi=ROI::All(), int nthreads=0);


/// Struct holding all the results computed by ImageBufAlgo::compare().
/// (maxx,maxy,maxz,maxc) gives the pixel coordintes (x,y,z) and color
/// channel of the pixel that differed maximally between the two images.
/// nwarn and nfail are the number of "warnings" and "failures",
/// respectively.
struct CompareResults {
    double meanerror, rms_error, PSNR, maxerror;
    int maxx, maxy, maxz, maxc;
    imagesize_t nwarn, nfail;
};

/// Numerically compare two images.  The difference threshold (for any
/// individual color channel in any pixel) for a "failure" is
/// failthresh, and for a "warning" is warnthresh.  The results are
/// stored in result.  If roi is defined, pixels will be compared for
/// the pixel and channel range that is specified.  If roi is not
/// defined, the comparison will be for all channels, on the union of
/// the defined pixel windows of the two images (for either image,
/// undefined pixels will be assumed to be black).
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
///
/// Return true on success, false on error.
bool OIIO_API compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       CompareResults &result,
                       ROI roi = ROI::All(), int nthreads=0);

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
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.  But it's basically meaningless if the
/// first three channels aren't RGB in a linear color space that sort
/// of resembles AdobeRGB.
///
/// Return true on success, false on error.
int OIIO_API compare_Yee (const ImageBuf &A, const ImageBuf &B,
                          CompareResults &result,
                          float luminance = 100, float fov = 45,
                          ROI roi = ROI::All(), int nthreads = 0);


/// Do all pixels within the ROI have the same values for channels
/// [roi.chbegin..roi.chend-1]?  If so, return true and store that color
/// in color[chbegin...chend-1] (if color != NULL); otherwise return
/// false.  If roi is not defined (the default), it will be understood
/// to be all of the defined pixels and channels of source.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
bool OIIO_API isConstantColor (const ImageBuf &src, float *color = NULL,
                               ROI roi = ROI::All(), int nthreads=0);

/// Does the requested channel have a given value over the ROI?  (For
/// this function, the ROI's chbegin/chend are ignored.)  Return true if
/// so, otherwise return false.  If roi is not defined (the default), it
/// will be understood to be all of the defined pixels and channels of
/// source.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
bool OIIO_API isConstantChannel (const ImageBuf &src, int channel, float val,
                                 ROI roi = ROI::All(), int nthreads = 0);

/// Is the image monochrome within the ROI, i.e., for all pixels within
/// the region, do all channels [roi.chbegin, roi.chend) have the same
/// value?  If roi is not defined (the default), it will be understood
/// to be all of the defined pixels and channels of source.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
bool OIIO_API isMonochrome (const ImageBuf &src,
                            ROI roi = ROI::All(), int nthreads = 0);

/// Count how many pixels in the ROI match a list of colors.
///
/// The colors to match are in colors[0..nchans-1],
/// colors[nchans..2*nchans-1], and so on, a total of ncolors
/// consecutive colors of nchans each.
///
/// eps[0..nchans-1] are the error tolerances for a match, for each
/// channel.  Setting eps[c] = numeric_limits<float>::max() will
/// effectively make it ignore the channel.  Passing eps == NULL will be
/// interpreted as a tolerance of 0.001 for all channels (requires exact
/// matches for 8 bit images, but allows a wee bit of imprecision for
/// float images.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
///
/// Upon success, return true and store the number of pixels that
/// matched each color count[..ncolors-1].  If there is an error,
/// returns false and sets an appropriate error message set in src.
bool OIIO_API color_count (const ImageBuf &src,
                           imagesize_t *count,
                           int ncolors, const float *color,
                           const float *eps=NULL,
                           ROI roi=ROI::All(), int nthreads=0);

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
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
///
/// Return true if the operation can be performed, false if there is
/// some sort of error (and sets an appropriate error message in src).
bool OIIO_API color_range_check (const ImageBuf &src,
                                 imagesize_t *lowcount,
                                 imagesize_t *highcount,
                                 imagesize_t *inrangecount,
                                 const float *low, const float *high,
                                 ROI roi=ROI::All(), int nthreads=0);

/// Find the minimal rectangular region within roi (which defaults to
/// the entire pixel data window of src) that consists of nonzero pixel
/// values.  In other words, gives the region that is a "shrink-wraps"
/// of src to exclude black border pixels.  Note that if the entire
/// image was black, the ROI returned will contain no pixels.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works for all pixel types.
OIIO_API ROI nonzero_region (const ImageBuf &src,
                             ROI roi=ROI::All(), int nthreads=0);

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
                                           const std::string &extrainfo = "",
                                           ROI roi = ROI::All(),
                                           int blocksize = 0, int nthreads=0);



/// Set dst, over the region of interest, to be a resized version of the
/// corresponding portion of src (mapping such that the "full" image
/// window of each correspond to each other, regardless of resolution).
///
/// The filter is used to weight the src pixels falling underneath it for
/// each dst pixel.  The caller may specify a reconstruction filter by name
/// and width (expressed  in pixels unts of the dst image), or resize() will
/// choose a reasonable default high-quality default filter (blackman-harris
/// when upsizing, lanczos3 when downsizing) if the empty string is passed
/// or if filterwidth is 0.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API resize (ImageBuf &dst, const ImageBuf &src,
                      const std::string &filtername = "",
                      float filterwidth = 0.0f,
                      ROI roi = ROI::All(), int nthreads = 0);

/// Set dst, over the region of interest, to be a resized version of the
/// corresponding portion of src (mapping such that the "full" image
/// window of each correspond to each other, regardless of resolution).
///
/// The caller may explicitly pass a reconstruction filter, or resize()
/// will choose a reasonable default if NULL is passed.  The filter is
/// used to weight the src pixels falling underneath it for each dst
/// pixel; the filter's size is expressed in pixel units of the dst
/// image.  If no filter is supplied, a default medium-quality
/// (triangle) filter will be used.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API resize (ImageBuf &dst, const ImageBuf &src,
                      Filter2D *filter,
                      ROI roi = ROI::All(), int nthreads = 0);


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
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API resample (ImageBuf &dst, const ImageBuf &src,
                        bool interpolate = true,
                        ROI roi = ROI::All(), int nthreads = 0);

/// Replace the given ROI of dst with the convolution of src and
/// a kernel.  If roi is not defined, it defaults to the full size
/// of dst (or src, if dst was uninitialized).  If dst is uninitialized,
/// it will be allocated to be the size specified by roi.  If 
/// normalized is true, the kernel will be normalized for the 
/// convolution, otherwise the original values will be used.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on any pixel data type for dst and src, but kernel MUST be
/// a float image.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API convolve (ImageBuf &dst, const ImageBuf &src,
                        const ImageBuf &kernel, bool normalize = true,
                        ROI roi = ROI::All(), int nthreads = 0);

/// Initialize dst to be a 1-channel FLOAT image of the named kernel.
/// The size of the dst image will be big enough to contain the kernel
/// given its size (width x height) and rounded up to odd resolution so
/// that the center of the kernel can be at the center of the middle
/// pixel.  The kernel image will be offset so that its center is at the
/// (0,0) coordinate.  If normalize is true, the values will be
/// normalized so that they sum to 1.0.
///
/// If depth > 1, a volumetric kernel will be created.  Use with caution!
///
/// Kernel names can be: "gaussian", "sharp-gaussian", "box",
/// "triangle", "blackman-harris", "mitchell", "b-spline", "catmull-rom",
/// "lanczos3", "disk", "binomial."
/// 
/// Note that "catmull-rom" and "lanczos3" are fixed-size kernels that
/// don't scale with the width, and are therefore probably less useful
/// in most cases.
/// 
bool OIIO_API make_kernel (ImageBuf &dst, const char *name,
                           float width, float height, float depth = 1.0f,
                           bool normalize = true);

/// Replace the given ROI of dst with a sharpened version of the
/// corresponding region of src using the ``unsharp mask'' technique.
/// Unsharp masking basically works by first blurring the image (low
/// pass filter), subtracting this from the original image, then
/// adding the residual back to the original to emphasize the edges.
/// Roughly speaking,
///      dst = src + contrast * thresh(src - blur(src))
///
/// The specific blur can be selected by kernel name and width.  The
/// contrast is a multiplier on the overall sharpening effect.  The
/// thresholding step causes all differences less than 'threshold' to be
/// squashed to zero, which can be useful for suppressing sharpening of
/// low-contrast details (like noise) but allow sharpening of
/// higher-contrast edges.
///
/// If roi is not defined, it defaults to the full size of dst (or src,
/// if dst was undefined).  If dst is uninitialized, it will be
/// allocated to be the size specified by roi.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                            const char *kernel="gaussian", float width = 3.0f,
                            float contrast = 1.0f, float threshold = 0.0f,
                            ROI roi = ROI::All(), int nthreads = 0);


/// Take the discrete Fourier transform (DFT) of the section of src
/// denoted by roi, store it in dst.  If roi is not defined, it will be
/// all of src's pixels.  Only one channel of src may be FFT'd at a
/// time, so it will be the first channel described by roi (or, again,
/// channel 0 if roi is undefined).  If not already in the correct
/// format, dst will be re-allocated to be a 2-channel float buffer of
/// size width x height, with channel 0 being the "real" part and
/// channel 1 being the the "imaginary" part.  The values returned are
/// actually the unitary DFT, meaning that it is scaled by 1/sqrt(npixels).
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data type for src; dst will always be reallocated 
/// as FLOAT.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API fft (ImageBuf &dst, const ImageBuf &src,
                   ROI roi = ROI::All(), int nthreads = 0);

/// Take the inverse discrete Fourier transform of the section of src
/// denoted by roi, store it in dst.  If roi is not defined, it will be
/// all of src's pixels.
///
/// Src MUST be a 2-channel float image, and is assumed to be a complex
/// frequency-domain signal with the "real" component in channel 0 and
/// the "imaginary" component in channel 1.  Dst will end up being a
/// float image of one channel (the real component is kept, the
/// imaginary component of the spatial-domain will be discarded).
/// Just as with fft(), the ifft() function is dealing with the unitary
/// DFT, so it is scaled by 1/sqrt(npixels).
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API ifft (ImageBuf &dst, const ImageBuf &src,
                    ROI roi = ROI::All(), int nthreads = 0);


/// Convert a 2-channel image with "polar" values (amplitude, phase)
/// into a 2-channel image with complex values (real, imaginary).
///
/// The transformation between the two representations are:
///     real = amplitude * cos(phase);
///     imag = amplitude * sin(phase);
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API polar_to_complex (ImageBuf &dst, const ImageBuf &src,
                                ROI roi = ROI::All(), int nthreads = 0);

/// Convert a 2-channel image with complex values (real, imaginary) into a
/// 2-channel image with "polar" values (amplitude, phase).
///
/// The transformation between the two representations are:
///     amplitude = hypot (real, imag);
///     phase = atan2 (imag, real);
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API complex_to_polar (ImageBuf &dst, const ImageBuf &src,
                                ROI roi = ROI::All(), int nthreads = 0);


enum OIIO_API NonFiniteFixMode
{
    NONFINITE_NONE = 0,     ///< Do nothing
    NONFINITE_BLACK = 1,    ///< Replace nonfinite pixels with black
    NONFINITE_BOX3 = 2,     ///< Replace nonfinite pixels with 3x3 finite average
};

/// Copy the values of src (within the ROI) to dst, while repairing  any
/// non-finite (NaN/Inf) pixels. If pixelsFound is not NULL, store in it the
/// number of pixels that contained non-finite value.  It is permissible
/// to operate in-place (with src and dst referring to the same image).
///
/// How the non-finite values are repaired is specified by one of the
/// following modes:
///   NONFINITE_NONE   do not alter the pixels (but do count the number
///                       of nonfinite pixels in *pixelsFixed, if non-NULL).
///   NONFINITE_BLACK  change non-finite values to 0.
///   NONFINITE_BOX3   replace non-finite values by the average of any
///                       finite pixels within a 3x3 window.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types, though it's just a copy for images with
/// pixel data types that cannot represent NaN or Inf values.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode=NONFINITE_BOX3,
                            int *pixelsFixed = NULL,
                            ROI roi = ROI::All(), int nthreads = 0);

/// DEPRECATED (1.3) in-place version
bool OIIO_API fixNonFinite (ImageBuf &dst, NonFiniteFixMode mode=NONFINITE_BOX3,
                            int *pixelsFixed = NULL,
                            ROI roi = ROI::All(), int nthreads = 0);


/// Fill the holes using a push-pull technique.  The src image must have
/// an alpha channel.  The dst image will end up with a copy of src, but
/// will have an alpha of 1.0 everywhere, and any place where the alpha
/// of src was < 1, dst will have a pixel color that is a plausible
/// "filling" of the original alpha hole.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
///
/// Return true on success, false on error (with an appropriate error
/// message set in dst).
bool OIIO_API fillholes_pushpull (ImageBuf &dst, const ImageBuf &src,
                                  ROI roi = ROI::All(), int nthreads = 0);


/// Convert an IplImage, used by OpenCV and Intel's Image Libray, and
/// set ImageBuf dst to be the same image (copying the pixels).  If
/// convert is not set to UNKNOWN, try to establish dst as holding that
/// data type and convert the IplImage data.  Return true if ok, false
/// if it couldn't figure out how to make the conversion from IplImage
/// to ImageBuf.  If OpenImageIO was compiled without OpenCV support,
/// this function will return false without modifying dst.
bool OIIO_API from_IplImage (ImageBuf &dst, const IplImage *ipl,
                              TypeDesc convert=TypeDesc::UNKNOWN);

/// Construct an IplImage*, used by OpenCV and Intel's Image Library,
/// that is equivalent to the ImageBuf src.  If it is not possible, or
/// if OpenImageIO was compiled without OpenCV support, then return
/// NULL.  The ownership of the IplImage is fully transferred to the
/// calling application.
OIIO_API IplImage* to_IplImage (const ImageBuf &src);

/// Capture a still image from a designated camera.  If able to do so,
/// store the image in dst and return true.  If there is no such device,
/// or support for camera capture is not available (such as if OpenCV
/// support was not enabled at compile time), return false and do not
/// alter dst.
bool OIIO_API capture_image (ImageBuf &dst, int cameranum = 0,
                              TypeDesc convert=TypeDesc::UNKNOWN);



/// Set dst to the composite of A over B using the Porter/Duff definition
/// of "over", returning true upon success and false for any of a
/// variety of failures (as described below).
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
/// 'roi' specifies the region of dst's pixels which will be computed;
/// existing pixels outside this range will not be altered.  If not
/// specified, the default ROI value will be interpreted as a request to
/// apply "A over B" to the entire region of dst's pixel data.
///
/// A, B, and dst need not perfectly overlap in their pixel data windows;
/// pixel values of A or B that are outside their respective pixel data
/// window will be treated as having "zero" (0,0,0...) value.
///
/// The nthreads parameter specifies how many threads (potentially) may
/// be used, but it's not a guarantee.  If nthreads == 0, it will use
/// the global OIIO attribute "nthreads".  If nthreads == 1, it
/// guarantees that it will not launch any new threads.
///
/// Works on all pixel data types.
bool OIIO_API over (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi = ROI::All(), int nthreads = 0);


/// Just like ImageBufAlgo::over(), but inputs A and B must have
/// designated 'z' channels, and on a pixel-by-pixel basis, the z values
/// will determine which of A or B will be considered the foreground or
/// background (lower z is foreground).  If z_zeroisinf is true, then
/// z=0 values will be treated as if they are infinitely far away.
bool OIIO_API zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf = false,
                     ROI roi = ROI::All(), int nthreads = 0);



/// Render a text string into image dst, essentially doing an "over" of
/// the character into the existing pixel data.  The baseline of the
/// first character will start at position (x,y).  The font is given by
/// fontname as a full pathname to the font file (defaulting to some
/// reasonable system font if not supplied at all), and with a nominal
/// height of fontsize (in pixels).  The characters will be drawn in
/// opaque white (1.0,1.0,...) in all channels, unless textcolor is
/// supplied (and is expected to point to a float array of length at
/// least equal to R.spec().nchannels).
bool OIIO_API render_text (ImageBuf &dst, int x, int y,
                           const std::string &text,
                           int fontsize=16, const std::string &fontname="",
                           const float *textcolor = NULL);


/// ImageBufAlgo::histogram --------------------------------------------------
/// Parameters:
/// src         - Input image that contains the one channel to be histogramed.
///               src must contain float pixel data and have at least 1 channel,
///               but it can have more.
/// channel     - Only this channel in src will be histogramed. It must satisfy
///               0 <= channel < src.nchannels().
/// histogram   - Clear old content and store the histogram here.
/// bins        - Number of bins must be at least 1.
/// min, max    - Pixel values outside of the min->max range are not used for
///               computing the histogram. If min<max then the range is valid.
/// submin      - Store number of pixel values < min.
/// supermax    - Store number of pixel values > max.
/// roi         - Only pixels in this region of the image are histogramed. If
///               roi is not defined then the full size image will be
///               histogramed.
/// --------------------------------------------------------------------------
bool OIIO_API histogram (const ImageBuf &src, int channel,
                         std::vector<imagesize_t> &histogram, int bins=256,
                         float min=0, float max=1, imagesize_t *submin=NULL,
                         imagesize_t *supermax=NULL, ROI roi=ROI::All());



/// ImageBufAlgo::histogram_draw ---------------------------------------------
/// Parameters:
/// dst         - The histogram will be drawn in the image dst. which must
///               have only 1 channel with float pixel data, and width equal
///               to the number of bins, that is elements in histogram.
/// histogram   - The histogram to be drawn, must have at least 1 bin.
/// --------------------------------------------------------------------------
bool OIIO_API histogram_draw (ImageBuf &dst,
                              const std::vector<imagesize_t> &histogram);



enum OIIO_API MakeTextureMode {
    MakeTxTexture, MakeTxShadow, MakeTxEnvLatl,
    MakeTxEnvLatlFromLightProbe,
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
///    maketx:stats (int)     If nonzero, print stats to outstream (0).
///    maketx:resize (int)    If nonzero, resize to power of 2. (0)
///    maketx:nomipmap (int)  If nonzero, only output the top MIP level (0).
///    maketx:updatemode (int) If nonzero, write new output only if the
///                              output file doesn't already exist, or is
///                              older than the input file. (0)
///    maketx:constant_color_detect (int)
///                           If nonzero, detect images that are entirely
///                             one color, and change them to be low
///                             resolution (default: 0).
///    maketx:monochrome_detect (int)
///                           If nonzero, change RGB images which have 
///                              R==G==B everywhere to single-channel 
///                              grayscale (default: 0).
///    maketx:opaquedetect (int)
///                           If nonzero, drop the alpha channel if alpha
///                              is 1.0 in all pixels (default: 0).
///    maketx:unpremult (int) If nonzero, unpremultiply color by alpha before
///                              color conversion, then multiply by alpha
///                              after color conversion (default: 0).
///    maketx:incolorspace (string)
///    maketx:outcolorspace (string) 
///                           These two together will apply a color conversion
///                               (with OpenColorIO, if compiled). Default: ""
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
///
bool OIIO_API make_texture (MakeTextureMode mode,
                            const ImageBuf &input,
                            const std::string &outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = NULL);

/// Version of make_texture that starts with a filename and reads the input
/// from that file, rather than being given an ImageBuf directly.
bool OIIO_API make_texture (MakeTextureMode mode,
                            const std::string &filename,
                            const std::string &outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = NULL);

/// Version of make_texture that takes multiple filenames (reserved for
/// future expansion, such as assembling several faces into a cube map).
bool OIIO_API make_texture (MakeTextureMode mode,
                            const std::vector<std::string> &filenames,
                            const std::string &outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = NULL);




}  // end namespace ImageBufAlgo

}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUFALGO_H
