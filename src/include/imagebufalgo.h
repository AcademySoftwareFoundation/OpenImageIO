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

/// Zero out (set to 0, black) the image region.  If the optional ROI is
/// not specified, it will set all channels of all image pixels to 0.0.
/// Return true on success, false on failure.
bool OIIO_API zero (ImageBuf &dst, ROI roi=ROI());

/// Fill the image with a given channel values.  If the optional ROI is
/// not specified, it will fill all channels of all image pixels.  Note
/// that values[0] corresponds to channel roi.chanbegin.  Return true on
/// success, false on failure.
bool OIIO_API fill (ImageBuf &dst, const float *values, ROI roi=ROI());

/// Fill a subregion of the volume with the given pixel value.  The
/// subregion is bounded by [xbegin,xend) X [ybegin,yend) X [zbegin,zend).
/// return true on success.
inline bool OIIO_API fill (ImageBuf &dst, const float *pixel,
                     int xbegin, int xend, int ybegin, int yend,
                     int zbegin=0, int zend=1) {
    return fill (dst, pixel, ROI(xbegin,xend,ybegin,yend,zbegin,zend));
}

/// Fill a subregion of the volume with a checkerboard.  The subregion
/// is bounded by [xbegin,xend) X [ybegin,yend) X [zbegin,zend).  return
/// true on success.
bool OIIO_API checker (ImageBuf &dst,
                        int width,
                        const float *color1,
                        const float *color2,
                        int xbegin, int xend,
                        int ybegin, int yend,
                        int zbegin=0, int zend=1);

/// Enum describing options to be passed to transform

enum OIIO_API AlignedTransform
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

bool OIIO_API transform (ImageBuf &dst, const ImageBuf &src, AlignedTransform t);


/// Change the number of channels in the specified imagebuf.
/// This is done by either dropping them, or synthesizing additional ones.
/// If channels are added, they are cleared to a value of 0.0.
/// Does not support in-place operation.
/// return true on success.
/// DEPRECATED -- you should instead use the more general
/// ImageBufAlgo::channels (dst, src, numChannels, NULL, true);
bool OIIO_API setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels);


/// Generic channel shuffling -- copy src to dst, but with channels in
/// the order channelorder[0..nchannels-1].  Does not support in-place
/// operation.  For any channel in which channelorder[i] < 0, it will
/// just make dst channel i a constant color -- set to channelvalues[i]
/// (if channelvalues != NULL) or 0.0 (if channelvalues == NULL).
//
/// If channelorder is NULL, it will be interpreted as
/// {0, 1, ..., nchannels-1} (meaning that it's only renaming channels,
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
bool OIIO_API channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues=NULL,
                        const std::string *newchannelnames=NULL,
                        bool shuffle_channel_names=false);

/// DEPRECATED -- for back-compatibility
bool OIIO_API channels (ImageBuf &dst, const ImageBuf &src,
                         int nchannels, const int *channelorder,
                         bool shuffle_channel_names);

/// Make dst be a cropped copy of src, but with the new pixel data
/// window range [xbegin..xend) x [ybegin..yend).  Source pixel data
/// falling outside this range will not be transferred to dst.  If
/// the new pixel range extends beyond that of the source image, those
/// new pixels will get the color specified by bordercolor[0..nchans-1],
/// or with black/zero values if bordercolor is NULL.
bool OIIO_API crop (ImageBuf &dst, const ImageBuf &src,
                     int xbegin, int xend, int ybegin, int yend,
                     const float *bordercolor=NULL);


/// Copy into dst, beginning at (xbegin,ybegin,zbegin), the pixels of
/// src described by srcroi.  If srcroi is ROI(), the entirety of src
/// will be used.  It will copy into channels [chbegin...], as many
/// channels as are described by srcroi.
bool OIIO_API paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin,
                     const ImageBuf &src, ROI srcroi=ROI());


/// Add the pixels of two images A and B, putting the sum in dst.
/// The 'options' flag controls behaviors, particular of what happens
/// when A, B, and dst have differing data windows.  Note that dst must
/// not be the same image as A or B, and all three images must have the
/// same number of channels.  A and B *must* be float images.

bool OIIO_API add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B, int options=0);

/// Enum describing options to be passed to ImageBufAlgo::add.
/// Multiple options are allowed simultaneously by "or'ing" together.
enum OIIO_API AddOptions
{
    ADD_DEFAULT = 0,
    ADD_RETAIN_DST = 1,     ///< Retain dst pixels outside the region
    ADD_CLEAR_DST = 0,      ///< Default: clear all the dst pixels first
    ADD_RETAIN_WINDOWS = 2, ///< Honor the existing windows
    ADD_ALIGN_WINDOWS = 0,  ///< Default: align the windows before adding
};


/// For all pixels of R within region roi (defaulting to all the defined
/// pixels in R), multiply their value by 'val'.  Use the given number
/// of threads.
bool OIIO_API mul (ImageBuf &R, float val, ROI roi=ROI(), int threads=0);

/// For all pixels of R within region roi (defaulting to all the defined
/// pixels in R), multiply their value by val[0..nchans-1]. Use the
/// given number of threads.
bool OIIO_API mul (ImageBuf &R, const float *val, ROI roi=ROI(), int threads=0);



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


bool OIIO_API colorconvert (ImageBuf &dst, const ImageBuf &src,
    const ColorProcessor * processor,
    bool unpremult);

bool OIIO_API colorconvert (float * color, int nchannels,
    const ColorProcessor * processor,
    bool unpremult);


struct OIIO_API PixelStats {
    std::vector<float> min;
    std::vector<float> max;
    std::vector<float> avg;
    std::vector<float> stddev;
    std::vector<imagesize_t> nancount;
    std::vector<imagesize_t> infcount;
    std::vector<imagesize_t> finitecount;
};


/// Compute statistics on the specified image (over all pixels in the
/// data window of the current subimage and MIPmap level). Upon success,
/// the returned vectors will have size == numchannels.  A FLOAT
/// ImageBuf is required.
bool OIIO_API computePixelStats (PixelStats &stats, const ImageBuf &src);

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

/// Numerically compare two images.  The images must be the same size
/// and number of channels, and must both be FLOAT data.  The difference
/// threshold (for any individual color channel in any pixel) for a
/// "failure" is failthresh, and for a "warning" is warnthresh.  The
/// results are stored in result.
bool OIIO_API compare (const ImageBuf &A, const ImageBuf &B,
                        float failthresh, float warnthresh,
                        CompareResults &result);

/// Compare two images using Hector Yee's perceptual metric, returning
/// the number of pixels that fail the comparison.  The images must be
/// the same size, FLOAT, and in a linear color space.  Only the first
/// three channels are compared.  Free parameters are the ambient
/// luminance in the room and the field of view of the image display;
/// our defaults are probably reasonable guesses for an office
/// environment.
int OIIO_API compare_Yee (const ImageBuf &img0, const ImageBuf &img1,
                           float luminance = 100, float fov = 45);

/// Do all pixels for the entire image have the same channel values?  If
/// color is not NULL, that constant value will be stored in
/// color[0..nchannels-1].
bool OIIO_API isConstantColor (const ImageBuf &src, float *color = NULL);

/// Does the requested channel have a given value over the entire image?
///
bool OIIO_API isConstantChannel (const ImageBuf &src, int channel, float val);

/// Is the image monochrome? (i.e., are all channels the same value?)
/// zero and one channel images always return true
/// (current subimage, and current mipmap level)
bool OIIO_API isMonochrome(const ImageBuf &src);

/// Compute the SHA-1 byte hash for all the pixels in the specifed
/// region of the image.  If blocksize > 0, the function will compute
/// separate SHA-1 hashes of each 'blocksize' batch of scanlines, then
/// return a hash of the individual hashes.  This is just as strong a
/// hash, but will NOT match a single hash of the entire image
/// (blocksize==0).  But by breaking up the hash into independent
/// blocks, we can parallelize across multiple threads, given by
/// nthreads (if nthreads is 0, it will use the global OIIO thread
/// count).
std::string OIIO_API computePixelHashSHA1 (const ImageBuf &src,
                                           const std::string &extrainfo = "",
                                           ROI roi = ROI::All(),
                                           int blocksize = 0, int nthreads=0);



/// Set dst, over the pixel range [xbegin,xend) x [ybegin,yend), to be a
/// resized version of src (mapping such that the "full" image window of
/// each correspond to each other, regardless of resolution).  The
/// caller may explicitly pass a reconstruction filter, or resize() will
/// choose a reasonable default if NULL is passed.  The dst buffer must
/// be of type FLOAT.
bool OIIO_API resize (ImageBuf &dst, const ImageBuf &src,
                       int xbegin, int xend, int ybegin, int yend,
                       Filter2D *filter=NULL);


enum OIIO_API NonFiniteFixMode
{
    NONFINITE_NONE = 0,     ///< Do nothing
    NONFINITE_BLACK = 1,    ///< Replace nonfinite pixels with black
    NONFINITE_BOX3 = 2,     ///< Replace nonfinite pixels with 3x3 finite average
};

/// Fix all non-finite pixels (nan/inf) using the specified approach
bool OIIO_API fixNonFinite(ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode=NONFINITE_BOX3,
                            int * pixelsFixed=NULL);


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



/// Set R to the composite of A over B using the Porter/Duff definition
/// of "over", returning true upon success and false for any of a
/// variety of failures (as described below).  All three buffers must
/// have 'float' pixel data type.
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
bool OIIO_API over (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     ROI roi = ROI(), int threads = 0);


/// Just like ImageBufAlgo::over(), but inputs A and B must have
/// designated 'z' channels, and on a pixel-by-pixel basis, the z values
/// will determine which of A or B will be considered the foreground or
/// background (lower z is foreground).  If z_zeroisinf is true, then
/// z=0 values will be treated as if they are infinitely far away.
bool OIIO_API zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf = false,
                     ROI roi = ROI(), int threads = 0);

/// DEPRECATED -- preserved for link compatibility, but will be removed.
bool OIIO_API zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int threads = 0);


/// Render a text string into image R, essentially doing an "over" of
/// the character into the existing pixel data.  The baseline of the
/// first character will start at position (x,y).  The font is given by
/// fontname as a full pathname to the font file (defaulting to some
/// reasonable system font if not supplied at all), and with a nominal
/// height of fontheight (in pixels).  The characters will be drawn in
/// opaque white (1.0,1.0,...) in all channels, unless textcolor is
/// supplied (and is expected to point to a float array of length at
/// least equal to R.spec().nchannels).
bool OIIO_API render_text (ImageBuf &R, int x, int y,
                            const std::string &text,
                            int fontsize=16, const std::string &fontname="",
                            const float *textcolor = NULL);


/// ImageBufAlgo::histogram --------------------------------------------------
/// Parameters:
/// A           - Input image that contains the one channel to be histogramed.
///               A must contain float pixel data and have at least 1 channel,
///               but it can have more.
/// channel     - Only this channel in A will be histogramed. It must satisfy
///               0 <= channel < A.nchannels().
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
bool OIIO_API histogram (const ImageBuf &A, int channel,
                          std::vector<imagesize_t> &histogram, int bins=256,
                          float min=0, float max=1, imagesize_t *submin=NULL,
                          imagesize_t *supermax=NULL, ROI roi=ROI());



/// ImageBufAlgo::histogram_draw ---------------------------------------------
/// Parameters:
/// R           - The histogram will be drawn in the output image R. R must
///               have only 1 channel with float pixel data, and width equal
///               to the number of bins, that is elements in histogram.
/// histogram   - The histogram to be drawn, must have at least 1 bin.
/// --------------------------------------------------------------------------
bool OIIO_API histogram_draw (ImageBuf &R,
                               const std::vector<imagesize_t> &histogram);



enum OIIO_API MakeTextureMode {
    MakeTxTexture, MakeTxShadow, MakeTxEnvLatl, _MakeTxLast
};

/// Turn an image file (filename) into a tiled, MIP-mapped, texture file
/// (outputfilename).  The 'mode' describes what type of texture file we
/// are creating.  If the outstream pointer is not NULL, it should point
/// to a stream (for example, &std::out, or a pointer to a local 
/// std::stringstream to capture output), which is where console output
/// and error messages will be deposited.
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
///                           filter to use when resampling for MIPmap levels.
///                           Default: "", use simple bilinear resampling.
///    maketx:nchannels (int) If nonzero, will specify how many channels
///                              the output texture should have, padding with
///                              0 values or dropping channels, if it doesn't
///                              the number of channels in the input.
///                              (default: 0, meaning keep all input channels)
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
//                                a reasonable threshold (e.g. 1 GB). (0)
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

/// Version of make_texture that starts with an ImageBuf, rather than
/// reading the input image from disk.
bool OIIO_API make_texture (MakeTextureMode mode,
                            const ImageBuf &input,
                            const std::string &outputfilename,
                            const ImageSpec &config,
                            std::ostream *outstream = NULL);




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
///     ROI roi = get_roi (R.spec());
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
            if (roi.ybegin >= roi.yend)
                break;   // no more work to dole out
            threads.add_thread (new boost::thread (f, roi));
        }
        threads.join_all ();
    }
}



// Macro to call a type-specialzed version func<type>(R,...)
#define OIIO_DISPATCH_TYPES(name,func,type,R,...)                       \
    switch (type.basetype) {                                            \
    case TypeDesc::FLOAT :                                              \
        return func<float> (R, __VA_ARGS__); break;                     \
    case TypeDesc::UINT8 :                                              \
        return func<unsigned char> (R, __VA_ARGS__); break;             \
    case TypeDesc::HALF  :                                              \
        return func<half> (R, __VA_ARGS__); break;                      \
    case TypeDesc::UINT16:                                              \
        return func<unsigned short> (R, __VA_ARGS__); break;            \
    case TypeDesc::INT8  :                                              \
        return func<char> (R, __VA_ARGS__); break;                      \
    case TypeDesc::INT16 :                                              \
        return func<short> (R, __VA_ARGS__); break;                     \
    case TypeDesc::UINT  :                                              \
        return func<unsigned int> (R, __VA_ARGS__); break;              \
    case TypeDesc::INT   :                                              \
        return func<int> (R, __VA_ARGS__); break;                       \
    case TypeDesc::UINT64:                                              \
        return func<unsigned long long> (R, __VA_ARGS__); break;        \
    case TypeDesc::INT64 :                                              \
        return func<long long> (R, __VA_ARGS__); break;                 \
    case TypeDesc::DOUBLE:                                              \
        return func<double> (R, __VA_ARGS__); break;                    \
    default:                                                            \
        (R).error ("%s: Unsupported pixel data format '%s'", name, type); \
        return false;                                                   \
    }



}  // end namespace ImageBufAlgo


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H
