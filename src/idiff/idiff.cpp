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

#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;



enum idiffErrors {
    ErrOK = 0,            ///< No errors, the images match exactly
    ErrWarn,              ///< Warning: the errors differ a little
    ErrFail,              ///< Failure: the errors differ a lot
    ErrDifferentSize,     ///< Images aren't even the same size
    ErrFileNotFound,      ///< Could not find or open input files, etc.
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
static float hardfail = std::numeric_limits<float>::max();
static std::vector<std::string> filenames;
static ImageIOFormatSpec inspec[2];
static int npels, nvals;
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



// Open the two input files.  Fail if either one cannot be found and
// opened.
//
// Compare the dimensions of the images.  Fail if they aren't the same
// resolution and number of channels.  No problem, though, if they
// aren't the same data type.
//
// Read the two input images, converting to float data for when we
// compare.  Once we do this, we no longer need the image readers.
static int
read_inputs ()
{
    ImageInput *in[2] = { NULL, NULL };

    for (int i = 0;  i < 2;  ++i) {
        in[i] = ImageInput::create (filenames[i].c_str(), "" /* searchpath */);
        if (! in[i]) {
            std::cerr << "idiff ERROR: Could not find an ImageIO plugin to read \"" 
                      << filenames[i] << "\" : " << OpenImageIO::error_message() << "\n";
            delete in[0];
            delete in[1];
            return ErrFileNotFound;
        }
        if (! in[i]->open (filenames[i].c_str(), inspec[i])) {
            std::cerr << "idiff ERROR: Could not open \"" << filenames[i]
                      << "\" : " << in[i]->error_message() << "\n";
            delete in[0];
            delete in[1];
            return ErrFileNotFound;
        }
    }

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
        for (int i = 0;  i < 2;  ++i) {
            in[i]->close ();
            delete in[i];
        }
        return ErrDifferentSize;
    }

    npels = inspec[0].width * inspec[0].height * inspec[0].depth;
    nvals = npels * inspec[0].nchannels;
    pixels0 = new float[nvals];
    pixels1 = new float[nvals];
    in[0]->read_image (PT_FLOAT, pixels0);
    in[1]->read_image (PT_FLOAT, pixels1);

    in[0]->close ();
    delete in[0];
    in[1]->close ();
    delete in[1];

    return ErrOK;
}



static void
write_diff_image ()
{
    ImageIOFormatSpec outspec = inspec[0];
    outspec.extra_attribs.clear();

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (diffimage.c_str());
    if (! out) {
        std::cerr 
            << "idiff ERROR: Could not find an ImageIO plugin to write \"" 
            << diffimage << "\" :" << OpenImageIO::error_message() << "\n";;
    } else {
        if (! out->open (diffimage.c_str(), outspec)) {
            std::cerr << "idiff ERROR: Could not open \"" << diffimage
                      << "\" : " << out->error_message() << "\n";
        } else {
            if (diffabs) {
                for (int i = 0;  i < nvals;  ++i)
                    pixels0[i] = fabs(pixels0[i]);
            }
            if (diffscale != 1) {
                for (int i = 0;  i < nvals;  ++i)
                    pixels0[i] *= diffscale;
            }
            out->write_image (PT_FLOAT, &(pixels0[0]));
            out->close ();
        }
        delete out;
    }
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    std::cout << "Comparing \"" << filenames[0] 
             << "\" and \"" << filenames[1] << "\"\n";

    int ret = read_inputs();
    if (ret)
        return ret;

    // Subtract the second image from the first.  At which time we no
    // longer need the second image, so free it.
    for (int i = 0;  i < nvals;  ++i)
        pixels0[i] -= pixels1[i];
    delete [] pixels1;
    pixels1 = NULL;

    // Compare the two images.
    //
    int nscanlines = inspec[0].height * inspec[0].depth;
    int scanlinevals = inspec[0].width * inspec[0].nchannels;
    double totalerror = 0;
    double maxerror;
    int maxx, maxy, maxz, maxc;
    int nfail = 0, nwarn = 0;
    float *p = &pixels0[0];
    for (int z = 0;  z < inspec[0].depth;  ++z) {
        for (int y = 0;  y < inspec[0].height;  ++y) {
            double scanlineerror = 0;
            for (int x = 0;  x < inspec[0].width;  ++x) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = 0;  c < inspec[0].nchannels;  ++c, ++p) {
                    double f = (*p);
                    scanlineerror += f;
                    f = fabs(f);
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

    // If the user requested that a difference image be output, do that.
    //
    if (diffimage.size() && (maxerror != 0 || !outdiffonly))
        write_diff_image ();

    if (nfail > (failpercent/100.0 * npels) || maxerror > hardfail) {
        std::cout << "FAILURE\n";
        return ErrFail;
    }
    if (nwarn > (warnpercent/100.0 * npels) || maxerror > hardwarn) {
        std::cout << "WARNING\n";
        return ErrWarn;
    }
    std::cout << "PASS\n";
    return ErrOK;
}
