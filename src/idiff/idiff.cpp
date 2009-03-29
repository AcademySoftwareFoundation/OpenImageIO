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


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <iterator>

#include <boost/scoped_array.hpp>

#include <ImathColor.h>
using Imath::Color3f;
#include <ImathFun.h>

#include "dassert.h"
#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;
#include "imagebuf.h"



enum idiffErrors {
    ErrOK = 0,            ///< No errors, the images match exactly
    ErrWarn,              ///< Warning: the errors differ a little
    ErrFail,              ///< Failure: the errors differ a lot
    ErrDifferentSize,     ///< Images aren't even the same size
    ErrFile,              ///< Could not find or open input files, etc.
    ErrLast
};



static bool verbose = false;
static bool outdiffonly = false;
static std::string diffimage;
static float diffscale = 1.0;
static bool diffabs = false;
static float warnthresh = 1.0e-6;
static float warnpercent = 0;
static float hardwarn = std::numeric_limits<float>::max();
static float failthresh = 1.0e-6;
static float failpercent = 0;
static bool perceptual = false;
static float hardfail = std::numeric_limits<float>::max();
static std::vector<std::string> filenames;
static ImageBuf img0, img1;
static bool comparemeta = false;
static bool comparemip = false;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("Usage:  idiff [options] image1 image2",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "<SEPARATOR>", "Thresholding and comparison options",
                  "-fail %g", &failthresh, "Failure threshold difference (0.000001)",
                  "-failpercent %g", &failpercent, "Allow this percentage of failures (0)",
                  "-hardfail %g", &hardfail, "Fail if any one pixel exceeds this error (infinity)",
                  "-warn %g", &warnthresh, "Warning threshold difference (0.00001)",
                  "-warnpercent %g", &warnpercent, "Allow this percentage of warnings (0)",
                  "-hardwarn %g", &hardwarn, "Warn if any one pixel exceeds this error (infinity)",
                  "-p", &perceptual, "Perform perceptual (rather than numeric) comparison",
                  "<SEPARATOR>", "Difference image options",
                  "-o %s", &diffimage, "Output difference image",
                  "-od", &outdiffonly, "Output image only if nonzero difference",
                  "-abs", &diffabs, "Output image of absolute value, not signed difference",
                  "-scale %g", &diffscale, "Scale the output image by this factor",
//                  "-mip", &comparemip, "Compare all MIP-map levels or subimages",
//                  "-meta", &comparemeta, "Compare metadata",
                  NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() != 2) {
        std::cerr << "idiff: Must have two input filenames.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
}



static bool
read_input (const std::string &filename, ImageBuf &img)
{
    if (img.init_spec (filename) && 
        img.read (0, false, TypeDesc::FLOAT))
        return true;

    std::cerr << "idiff ERROR: Could not read " << filename << ":\n\t"
              << img.error_message() << "\n";
    return false;
}



// Adobe RGB (1998) with reference white D65 -> XYZ
// matrix is from http://www.brucelindbloom.com/
inline Color3f
AdobeRGBToXYZ (const Color3f &rgb)
{
    return Color3f (rgb[0] * 0.576700f  + rgb[1] * 0.185556f  + rgb[2] * 0.188212f,
                    rgb[0] * 0.297361f  + rgb[1] * 0.627355f  + rgb[2] * 0.0752847f,
                    rgb[0] * 0.0270328f + rgb[1] * 0.0706879f + rgb[2] * 0.991248f);
}



template<class T>
Imath::Vec3<T>
powf (const Imath::Vec3<T> &x, float y)
{
    return Imath::Vec3<T> (powf (x[0], y), powf (x[1], y), powf (x[2], y));
}



/// Convert a color in XYZ space to LAB space.
///
static Color3f
XYZToLAB (const Color3f xyz)
{
    // Reference white point
    static const Color3f white (0.576700f + 0.185556f + 0.188212f,
                                0.297361f + 0.627355f + 0.0752847f,
                                0.0270328f + 0.0706879f + 0.991248f);
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;

    Color3f r = xyz / white;
    Color3f f;
    for (int i = 0; i < 3; i++) {
        if (r[i] > epsilon)
            f[i] = powf (r[i], 1.0f / 3.0f);
        else
            f[i] = (kappa * r[i] + 16.0f) / 116.0f;
    }
    return Color3f (116.0f * f[1] - 16.0f,    // L
                    500.0f * (f[0] - f[1]),   // A
                    200.0f * (f[1] - f[2]));  // B
}



#define LAPLACIAN_MAX_LEVELS 8


class LaplacianPyramid
{
public:
    LaplacianPyramid (float *image, int _width, int _height) 
        : w(_width), h(_height)
    {
        level[0].insert (level[0].begin(), image, image+w*h);
        for (int i = 1;  i < LAPLACIAN_MAX_LEVELS;  ++i)
            convolve (level[i], level[i-1]);
    }

    ~LaplacianPyramid () { }

    float value (int x, int y, int lev) const {
	return level[std::min (lev, LAPLACIAN_MAX_LEVELS)][y*w + x];
    }

private:
    int w, h;
    std::vector<float> level[LAPLACIAN_MAX_LEVELS];

    // convolve image b with the kernel and store it in a
    void convolve (std::vector<float> &a, const std::vector<float> &b) {
        const float kernel[] = {0.05f, 0.25f, 0.4f, 0.25f, 0.05f};
        a.resize (b.size());
        for (int y = 0, index = 0;  y < h;  ++y) {
            for (int x = 0;  x < w;  ++x, ++index) {
                a[index] = 0.0f;
                for (int i = -2;  i <= 2;  ++i) {
                    for (int j = -2;  j<= 2;  ++j) {
                        int nx = abs(x+i);
                        int ny = abs(y+j);
                        if (nx >= w)
                            nx=2*w-nx-1;
                        if (ny >= h)
                            ny=2*h-ny-1;
                        a[index] += kernel[i+2] * kernel[j+2] * b[ny * w + nx];
                    } 
                }
            }
        }
    }
};



// Contrast sensitivity function (Barten SPIE 1989)
static float
contrast_sensitivity (float cyclesperdegree, float luminance)
{
    float a = 440.0f * powf ((1.0f + 0.7f / luminance), -0.2f);
    float b = 0.3f * powf ((1.0f + 100.0f / luminance), 0.15f);
    return a * cyclesperdegree * expf(-b * cyclesperdegree) 
             * sqrtf(1.0f + 0.06f * expf(b * cyclesperdegree)); 
}



// Visual Masking Function from Daly 1993
inline float
mask (float contrast)
{
    float a = powf (392.498f * contrast, 0.7f);
    float b = powf (0.0153f * a, 4.0f);
    return powf (1.0f + b, 0.25f); 
} 



// Given the adaptation luminance, this function returns the
// threshold of visibility in cd per m^2
// TVI means Threshold vs Intensity function
// This version comes from Ward Larson Siggraph 1997
static float
tvi (float adaptation_luminance)
{
    // returns the threshold luminance given the adaptation luminance
    // units are candelas per meter squared
    float r;
    float log_a = log10f(adaptation_luminance);
    if (log_a < -3.94f)
        r = -2.86f;
    else if (log_a < -1.44f)
        r = powf(0.405f * log_a + 1.6f , 2.18f) - 2.86f;
    else if (log_a < -0.0184f)
        r = log_a - 0.395f;
    else if (log_a < 1.9f)
        r = powf(0.249f * log_a + 0.65f, 2.7f) - 0.72f;
    else
        r = log_a - 1.255f;
    return powf (10.0f, r); 
} 



/// Use Hector Yee's perceptual metric.  Return the number of pixels that
// fail the comparison.
/// N.B. - assume pixels are already in linear color space.
int
Yee_Compare (const ImageBuf &img0, const ImageBuf &img1,
             float luminance = 100, float fov = 45)
{
    const ImageSpec &spec (img0.spec());
    ASSERT (spec.format == TypeDesc::FLOAT);
    int nscanlines = spec.height * spec.depth;
    int npels = nscanlines * spec.width;

    bool luminanceOnly = false;

    // assuming colorspaces are in Adobe RGB (1998), convert to LAB
    boost::scoped_array<Color3f> aLAB (new Color3f[npels]);
    boost::scoped_array<Color3f> bLAB (new Color3f[npels]);
    boost::scoped_array<float> aLum (new float[npels]);
    boost::scoped_array<float> bLum (new float[npels]);
    for (int i = 0;  i < npels;  ++i) {
        Color3f RGB, XYZ;
        img0.getpixel (i, RGB.getValue(), 3);
        XYZ = AdobeRGBToXYZ (RGB);
        aLAB[i] = XYZToLAB (XYZ);
        aLum[i] = XYZ[1] * luminance;

        img1.getpixel (i, RGB.getValue(), 3);
        XYZ = AdobeRGBToXYZ (RGB);
        bLAB[i] = XYZToLAB (XYZ);
        bLum[i] = XYZ[1] * luminance;
    }

    // Construct Laplacian pyramids
    LaplacianPyramid la (&aLum[0], spec.width, nscanlines);
    LaplacianPyramid lb (&bLum[0], spec.width, nscanlines);

    float num_one_degree_pixels = (float) (2 * tan(fov * 0.5 * M_PI / 180) * 180 / M_PI);
    float pixels_per_degree = spec.width / num_one_degree_pixels;
	
    unsigned int adaptation_level = 0;
    for (int i = 0, npixels = 1;
             i < LAPLACIAN_MAX_LEVELS && npixels <= num_one_degree_pixels;
             ++i, npixels *= 2) 
        adaptation_level = i;

    float cpd[LAPLACIAN_MAX_LEVELS];
    cpd[0] = 0.5f * pixels_per_degree;
    for (int i = 1;  i < LAPLACIAN_MAX_LEVELS;  ++i)
        cpd[i] = 0.5f * cpd[i - 1];
    float csf_max = contrast_sensitivity (3.248f, 100.0f);
    
    float F_freq[LAPLACIAN_MAX_LEVELS - 2];
    for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2;  ++i)
        F_freq[i] = csf_max / contrast_sensitivity (cpd[i], 100.0f);
	
    unsigned int pixels_failed = 0;
    for (int y = 0, index = 0; y < nscanlines;  ++y) {
        for (int x = 0;  x < spec.width;  ++x, ++index) {
            float contrast[LAPLACIAN_MAX_LEVELS - 2];
            float sum_contrast = 0;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++) {
                float n1 = fabsf (la.value(x,y,i) - la.value(x,y,i+1));
                float n2 = fabsf (lb.value(x,y,i) - lb.value(x,y,i+1));
                float numerator = std::max (n1, n2);
                float d1 = fabsf (la.value(x,y,i+2));
                float d2 = fabsf (lb.value(x,y,i+2));
                float denominator = std::max (std::max (d1, d2), 1.0e-5f);
                contrast[i] = numerator / denominator;
                sum_contrast += contrast[i];
            }
            if (sum_contrast < 1e-5)
                sum_contrast = 1e-5f;
            float F_mask[LAPLACIAN_MAX_LEVELS - 2];
            float adapt = la.value(x,y,adaptation_level) + lb.value(x,y,adaptation_level);
            adapt *= 0.5f;
            if (adapt < 1e-5)
                adapt = 1e-5f;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++)
                F_mask[i] = mask(contrast[i] * contrast_sensitivity(cpd[i], adapt)); 
            float factor = 0;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++)
                factor += contrast[i] * F_freq[i] * F_mask[i] / sum_contrast;
            factor = Imath::clamp (factor, 1.0f, 10.0f);
            float delta = fabsf (la.value(x,y,0) - lb.value(x,y,0));
            bool pass = true;
            // pure luminance test
            if (delta > factor * tvi(adapt)) {
                pass = false;
            } else if (! luminanceOnly) {
                // CIE delta E test with modifications
                float color_scale = 1.0f;
                // ramp down the color test in scotopic regions
                if (adapt < 10.0f) {
                    color_scale = 1.0f - (10.0f - color_scale) / 10.0f;
                    color_scale = color_scale * color_scale;
                }
                float da = aLAB[index][1] - bLAB[index][1];  // diff in A
                float db = aLAB[index][2] - bLAB[index][2];  // diff in B
                da = da * da;
                db = db * db;
                float delta_e = (da + db) * color_scale;
                if (delta_e > factor)
                    pass = false;
            }
            if (!pass)
                ++pixels_failed;
        }
    }
//    std::cout << "Perceptual diff shows " << pixels_failed << " failures\n";

    return pixels_failed;
}



static bool
same_size (const ImageBuf &A, const ImageBuf &B)
{
    const ImageSpec &a (A.spec()), &b (B.spec());
    return (a.width == b.width && a.height == b.height &&
            a.depth == b.depth && a.nchannels == b.nchannels);
}
 


int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    std::cout << "Comparing \"" << filenames[0] 
             << "\" and \"" << filenames[1] << "\"\n";

    if (! read_input (filenames[0], img0) ||
        ! read_input (filenames[1], img1))
        return ErrFile;

    // Compare the dimensions of the images.  Fail if they aren't the
    // same resolution and number of channels.  No problem, though, if
    // they aren't the same data type.
    if (! same_size (img0, img1)) {
        std::cout << "Images do not match in size: ";
        std::cout << "(" << img0.spec().width << "x" << img0.spec().height;
        if (img0.spec().depth > 1)
            std::cout << "x" << img0.spec().depth;
        std::cout << "x" << img0.spec().nchannels << ")";
        std::cout << " versus ";
        std::cout << "(" << img1.spec().width << "x" << img1.spec().height;
        if (img1.spec().depth > 1)
            std::cout << "x" << img1.spec().depth;
        std::cout << "x" << img1.spec().nchannels << ")\n";
        return ErrDifferentSize;
    }

    int npels = img0.spec().width * img0.spec().height * img0.spec().depth;
    int nvals = npels * img0.spec().nchannels;

    // Compare the two images.
    //
    double totalerror = 0;
    double maxerror = 0;
    int maxx=0, maxy=0, maxz=0, maxc=0;
    int nfail = 0, nwarn = 0;
    ASSERT (img0.spec().format == TypeDesc::FLOAT);
    float *pixels0 = (float *) img0.pixeladdr (img0.spec().x, img0.spec().y);
    float *pixels1 = (float *) img1.pixeladdr (img0.spec().x, img0.spec().y);
    float *p = pixels0;
    float *q = pixels1;
    for (int z = 0;  z < img0.spec().depth;  ++z) {
        for (int y = 0;  y < img0.spec().height;  ++y) {
            double scanlineerror = 0;
            for (int x = 0;  x < img0.spec().width;  ++x) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = 0;  c < img0.spec().nchannels;  ++c, ++p, ++q) {
                    double f = fabs (*p - *q);
                    scanlineerror += f;
                    if (f > maxerror) {
                        maxerror = f;
                        maxx = x;
                        maxy = y;
                        maxz = z;
                        maxc = c;
                    }
                    if (! warned && f > warnthresh) {
                        ++nwarn;
                        warned = true;
                    }
                    if (! failed && f > failthresh) {
                        ++nfail;
                        failed = true;
                    }
                }
            }
            totalerror += scanlineerror;
        }
    }
    totalerror /= nvals;

    int yee_failures = 0;
    if (perceptual)
        yee_failures = Yee_Compare (img0, img1);

    // Print the report
    //
    std::cout << "  Mean error = " << totalerror << '\n';
    std::cout << "  Max error  = " << maxerror;
    if (maxerror != 0) {
        std::cout << " @ (" << maxx << ", " << maxy;
        if (img0.spec().depth > 1)
            std::cout << ", " << maxz;
        std::cout << ", " << img0.spec().channelnames[maxc] << ')';
    }
    std::cout << "\n";
    int precis = std::cout.precision();
    std::cout << "  " << nwarn << " pixels (" 
              << std::setprecision(3) << (100.0*nwarn / npels) 
              << std::setprecision(precis)
              << "%) over " << warnthresh << "\n";
    std::cout << "  " << nfail << " pixels (" 
              << std::setprecision(3) << (100.0*nfail / npels) 
              << std::setprecision(precis)
              << "%) over " << failthresh << "\n";
    if (perceptual)
        std::cout << "  " << yee_failures << " pixels ("
                  << std::setprecision(3) << (100.0*yee_failures / npels) 
                  << std::setprecision(precis)
                  << "%) failed the perceptual test\n";

    int ret = ErrOK;
    if (nfail > (failpercent/100.0 * npels) || maxerror > hardfail ||
            yee_failures > (failpercent/100.0 * npels)) {
        std::cout << "FAILURE\n";
        ret = ErrFail;
    } else if (nwarn > (warnpercent/100.0 * npels) || maxerror > hardwarn) {
        std::cout << "WARNING\n";
        ret = ErrWarn;
    } else {
        std::cout << "PASS\n";
    }

    // If the user requested that a difference image be output, do that.
    //
    if (diffimage.size() && (maxerror != 0 || !outdiffonly)) {
        // Subtract the second image from the first.  At which time we no
        // longer need the second image, so free it.
        if (diffabs)
            for (int i = 0;  i < nvals;  ++i)
                pixels0[i] = fabsf (pixels0[i] - pixels1[i]);
        else
            for (int i = 0;  i < nvals;  ++i)
                pixels0[i] = (pixels0[i] - pixels1[i]);
        if (diffscale != 1) {
            for (int i = 0;  i < nvals;  ++i)
                pixels0[i] *= diffscale;
        }

        img0.save (diffimage);
    }

    return ret;
}
