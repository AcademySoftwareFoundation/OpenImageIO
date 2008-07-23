/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


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

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;



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
static ImageIOFormatSpec inspec[2];
static float *pixels0 = NULL, *pixels1 = NULL;


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
    ArgParse ap (argc, (const char **)argv);
    if (ap.parse ("Usage:  idiff [options] image1 image2",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &diffimage, "Output difference image",
                  "-od", &outdiffonly, "Output image only if nonzero difference",
                  "-scale %g", &diffscale, "Scale the output image by this factor",
                  "-abs", &diffabs, "Output image of absolute value, not signed difference",
                  "-warn %g", &warnthresh, "Warning threshold difference",
                  "-warnpercent %g", &warnpercent, "Allow this percentage of warnings",
                  "-hardwarn %g", &hardwarn, "Warn if any one pixel exceeds this error",
                  "-fail %g", &failthresh, "Failure threshold difference",
                  "-failpercent %g", &failpercent, "Allow this percentage of failures",
                  "-hardfail %g", &hardfail, "Fail if any one pixel exceeds this error",
                  "-p", &perceptual, "Perform perceptual (rather than numeric) comparison",
                  NULL) < 0) {
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
read_input (const std::string &filename, ImageIOFormatSpec &inspec,
            float * &pixels)
{
    ImageInput *in = ImageInput::create (filename.c_str(), "" /* searchpath */);
    if (! in) {
        std::cerr << "idiff ERROR: Could not find an ImageIO plugin to read \"" 
                  << filename << "\" : " << OpenImageIO::error_message() << "\n";
        delete in;
        return false;
    }
    if (! in->open (filename.c_str(), inspec)) {
        std::cerr << "idiff ERROR: Could not open \"" << filename
                  << "\" : " << in->error_message() << "\n";
        delete in;
        return false;
    }

    int npels = inspec.width * inspec.height * inspec.depth;
    int nvals = npels * inspec.nchannels;
    pixels = new float[nvals];
    bool ret = true;
    if (! in->read_image (PT_FLOAT, pixels)) {
        ret = false;
        delete [] pixels;
        pixels = NULL;
    }
    in->close ();
    delete in;
    return ret;
}



static void
write_diff_image (const std::string &filename, const ImageIOFormatSpec &spec,
                  float *pixels)
{
    ImageIOFormatSpec outspec = spec;
    outspec.extra_attribs.clear();

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (filename.c_str());
    if (! out) {
        std::cerr 
            << "idiff ERROR: Could not find an ImageIO plugin to write \"" 
            << filename << "\" :" << OpenImageIO::error_message() << "\n";;
    } else {
        if (! out->open (filename.c_str(), outspec)) {
            std::cerr << "idiff ERROR: Could not open \"" << filename
                      << "\" : " << out->error_message() << "\n";
        } else {
            int nvals = spec.width * spec.height * spec.depth * spec.nchannels;
            if (diffabs) {
                for (int i = 0;  i < nvals;  ++i)
                    pixels[i] = fabs(pixels[i]);
            }
            if (diffscale != 1) {
                for (int i = 0;  i < nvals;  ++i)
                    pixels[i] *= diffscale;
            }
            out->write_image (PT_FLOAT, &(pixels[0]));
            out->close ();
        }
        delete out;
    }
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



inline void
AdobeRGBToXYZ (float r, float g, float b, float &x, float &y, float &z)
{
    // matrix is from http://www.brucelindbloom.com/
    x = r * 0.576700f + g * 0.185556f + b * 0.188212f;
    y = r * 0.297361f + g * 0.627355f + b * 0.0752847f;
    z = r * 0.0270328f + g * 0.0706879f + b * 0.991248f;
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



static void
XYZToLAB (float x, float y, float z, float &L, float &A, float &B)
{
    // Reference white point
    const float xw = 0.576700f + 0.185556f + 0.188212f;
    const float yw = 0.297361f + 0.627355f + 0.0752847f;
    const float zw = 0.0270328f + 0.0706879f + 0.991248f;
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    float f[3];
    float r[3];
    r[0] = x / xw;
    r[1] = y / yw;
    r[2] = z / zw;
    for (int i = 0; i < 3; i++) {
        if (r[i] > epsilon) {
            f[i] = powf (r[i], 1.0f / 3.0f);
        } else {
            f[i] = (kappa * r[i] + 16.0f) / 116.0f;
        }
    }
    L = 116.0f * f[1] - 16.0f;
    A = 500.0f * (f[0] - f[1]);
    B = 200.0f * (f[1] - f[2]);
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
                        int nx = x+i;
                        int ny = y+j;
                        if (nx < 0)
                            nx = -nx;
                        if (ny < 0)
                            ny = -ny;
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
Yee_Compare (const ImageIOFormatSpec &spec,
             float *pixels0, float *pixels1,
             float luminance = 100,
             float fov = 45)
{
    int nscanlines = spec.height * spec.depth;
    int npels = nscanlines * spec.width;
    int nvals = npels * spec.nchannels;

    bool luminanceOnly = false;

    // assuming colorspaces are in Adobe RGB (1998), convert to LAB
    boost::scoped_array<Color3f> aLAB (new Color3f[npels]);
    boost::scoped_array<Color3f> bLAB (new Color3f[npels]);
    boost::scoped_array<float> aLum (new float[npels]);
    boost::scoped_array<float> bLum (new float[npels]);
    for (int i = 0;  i < npels;  ++i) {
        Color3f RGB, XYZ;
        RGB = * (Color3f *)(&pixels0[i*spec.nchannels]);
        XYZ = AdobeRGBToXYZ (RGB);
        aLAB[i] = XYZToLAB (XYZ);
        aLum[i] = XYZ[1] * luminance;

        RGB = * (Color3f *)(&pixels1[i*spec.nchannels]);
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



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    std::cout << "Comparing \"" << filenames[0] 
             << "\" and \"" << filenames[1] << "\"\n";

    if (! read_input (filenames[0], inspec[0], pixels0) ||
        ! read_input (filenames[1], inspec[1], pixels1))
        return ErrFile;

    // Compare the dimensions of the images.  Fail if they aren't the
    // same resolution and number of channels.  No problem, though, if
    // they aren't the same data type.
    if (inspec[0].width != inspec[1].width ||
        inspec[0].height != inspec[1].height ||
        inspec[0].depth != inspec[1].depth ||
        inspec[0].nchannels != inspec[1].nchannels) {
        std::cout << "Images do not match in size: ";
        std::cout << "(" << inspec[0].width << "x" << inspec[0].height;
        if (inspec[0].depth > 1)
            std::cout << "x" << inspec[0].depth;
        std::cout << "x" << inspec[0].nchannels;
        std::cout << ")";
        std::cout << " versus ";
        std::cout << "(" << inspec[1].width << "x" << inspec[1].height;
        if (inspec[1].depth > 1)
            std::cout << "x" << inspec[1].depth;
        std::cout << "x" << inspec[1].nchannels;
        std::cout << ")\n";
        delete [] pixels0;
        pixels0 = NULL;
        delete [] pixels1;
        pixels1 = NULL;
        return ErrDifferentSize;
    }

    int npels = inspec[0].width * inspec[0].height * inspec[0].depth;
    int nvals = npels * inspec[0].nchannels;

    // Compare the two images.
    //
    int nscanlines = inspec[0].height * inspec[0].depth;
    int scanlinevals = inspec[0].width * inspec[0].nchannels;
    double totalerror = 0;
    double maxerror;
    int maxx, maxy, maxz, maxc;
    int nfail = 0, nwarn = 0;
    float *p = &pixels0[0];
    float *q = &pixels1[0];
    for (int z = 0;  z < inspec[0].depth;  ++z) {
        for (int y = 0;  y < inspec[0].height;  ++y) {
            double scanlineerror = 0;
            for (int x = 0;  x < inspec[0].width;  ++x) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = 0;  c < inspec[0].nchannels;  ++c, ++p, ++q) {
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
        yee_failures = Yee_Compare (inspec[0], pixels0, pixels1);

    // Print the report
    //
    std::cout << "  Mean error = " << totalerror << '\n';
    std::cout << "  Max error  = " << maxerror;
    if (maxerror != 0) {
        std::cout << " @ (" << maxx << ", " << maxy;
        if (inspec[0].depth > 1)
            std::cout << ", " << maxz;
        std::cout << ", " << inspec[0].channelnames[maxc] << ')';
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
    if (nfail > (failpercent/100.0 * npels) || maxerror > hardfail ||
            yee_failures > (failpercent/100.0 * npels)) {
        std::cout << "FAILURE\n";
        return ErrFail;
    }
    if (nwarn > (warnpercent/100.0 * npels) || maxerror > hardwarn) {
        std::cout << "WARNING\n";
        return ErrWarn;
    }
    std::cout << "PASS\n";

    // If the user requested that a difference image be output, do that.
    //
    if (diffimage.size() && (maxerror != 0 || !outdiffonly)) {
        // Subtract the second image from the first.  At which time we no
        // longer need the second image, so free it.
        for (int i = 0;  i < nvals;  ++i)
            pixels0[i] -= pixels1[i];
        write_diff_image (diffimage, inspec[0], pixels0);
    }

    return ErrOK;
}
