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

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>


using namespace OIIO;


enum idiffErrors {
    ErrOK = 0,            ///< No errors, the images match exactly
    ErrWarn,              ///< Warning: the errors differ a little
    ErrFail,              ///< Failure: the errors differ a lot
    ErrDifferentSize,     ///< Images aren't even the same size
    ErrFile,              ///< Could not find or open input files, etc.
    ErrLast
};



static bool verbose = false;
static bool quiet = false;
static bool outdiffonly = false;
static std::string diffimage;
static float diffscale = 1.0;
static bool diffabs = false;
static float warnthresh = 1.0e-6f;
static float warnpercent = 0;
static float hardwarn = std::numeric_limits<float>::max();
static float failthresh = 1.0e-6f;
static float failpercent = 0;
static bool perceptual = false;
static float hardfail = std::numeric_limits<float>::max();
static std::vector<std::string> filenames;
//static bool comparemeta = false;
static bool compareall = false;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("idiff -- compare two images\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  idiff [options] image1 image2",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-q", &quiet, "Quiet (minimal messages)",
                  "-a", &compareall, "Compare all subimages/miplevels",
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
//                  "-meta", &comparemeta, "Compare metadata",
                  NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
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
read_input (const std::string &filename, ImageBuf &img, 
            ImageCache *cache, int subimage=0, int miplevel=0)
{
    if (img.subimage() >= 0 && 
            img.subimage() == subimage && img.miplevel() == miplevel)
        return true;

    img.reset (filename, cache);
    if (img.read (subimage, miplevel, false, TypeFloat))
        return true;

    std::cerr << "idiff ERROR: Could not read " << filename << ":\n\t"
              << img.geterror() << "\n";
    return false;
}



// function that standarize printing NaN and Inf values on
// Windows (where they are in 1.#INF, 1.#NAN format) and all
// others platform
inline void
safe_double_print (double val)
{
    if (OIIO::isnan (val))
        std::cout << "nan";
    else if (OIIO::isinf (val))
        std::cout << "inf";
    else
        std::cout << val;
    std::cout << '\n';
}



inline void
print_subimage (ImageBuf &img0, int subimage, int miplevel)
{
    if (img0.nsubimages() > 1)
        std::cout << "Subimage " << subimage << ' ';
    if (img0.nmiplevels() > 1)
        std::cout << " MIP level " << miplevel << ' ';
    if (img0.nsubimages() > 1 || img0.nmiplevels() > 1)
        std::cout << ": ";
    std::cout << img0.spec().width << " x " << img0.spec().height;
    if (img0.spec().depth > 1)
        std::cout << " x " << img0.spec().depth;
    std::cout << ", " << img0.spec().nchannels << " channel\n";
}



int
main (int argc, char *argv[])
{
    Filesystem::convert_native_arguments (argc, (const char **)argv);
    getargs (argc, argv);

    if (! quiet)
        std::cout << "Comparing \"" << filenames[0]
                  << "\" and \"" << filenames[1] << "\"\n";

    // Create a private ImageCache so we can customize its cache size
    // and instruct it store everything internally as floats.
    ImageCache *imagecache = ImageCache::create (true);
    imagecache->attribute ("forcefloat", 1);
    if (sizeof(void *) == 4)  // 32 bit or 64?
        imagecache->attribute ("max_memory_MB", 512.0);
    else
        imagecache->attribute ("max_memory_MB", 2048.0);
    imagecache->attribute ("autotile", 256);
    // force a full diff, even for files tagged with the same
    // fingerprint, just in case some mistake has been made.
    imagecache->attribute ("deduplicate", 0);

    ImageBuf img0, img1;
    if (! read_input (filenames[0], img0, imagecache) ||
        ! read_input (filenames[1], img1, imagecache))
        return ErrFile;
//    ImageSpec spec0 = img0.spec();  // stash it

    int ret = ErrOK;
    for (int subimage = 0;  subimage < img0.nsubimages();  ++subimage) {
        if (subimage > 0 && !compareall)
            break;
        if (subimage >= img1.nsubimages())
            break;

        if (! read_input (filenames[0], img0, imagecache, subimage) ||
            ! read_input (filenames[1], img1, imagecache, subimage)) {
            std::cerr << "Failed to read subimage " << subimage << "\n";
            return ErrFile;
        }

        if (img0.nmiplevels() != img1.nmiplevels()) {
            if (! quiet)
                std::cout << "Files do not match in their number of MIPmap levels\n";
        }

        for (int m = 0;  m < img0.nmiplevels();  ++m) {
            if (m > 0 && !compareall)
                break;
            if (m > 0 && img0.nmiplevels() != img1.nmiplevels()) {
                std::cerr << "Files do not match in their number of MIPmap levels\n";
                ret = ErrDifferentSize;
                break;
            }

            if (! read_input (filenames[0], img0, imagecache, subimage, m) ||
                ! read_input (filenames[1], img1, imagecache, subimage, m))
                return ErrFile;

            if (img0.deep() != img1.deep()) {
                std::cerr << "One image contains deep data, the other does not\n";
                ret = ErrDifferentSize;
                break;
            }

            int npels = img0.spec().width * img0.spec().height * img0.spec().depth;
            if (npels == 0)
                npels = 1;    // Avoid divide by zero for 0x0 images
            ASSERT (img0.spec().format == TypeFloat);

            // Compare the two images.
            //
            ImageBufAlgo::CompareResults cr;
            ImageBufAlgo::compare (img0, img1, failthresh, warnthresh, cr);

            int yee_failures = 0;
            if (perceptual && ! img0.deep()) {
                ImageBufAlgo::CompareResults cr;
                yee_failures = ImageBufAlgo::compare_Yee (img0, img1, cr);
            }

            if (cr.nfail > (failpercent/100.0 * npels) || cr.maxerror > hardfail ||
                yee_failures > (failpercent/100.0 * npels)) {
                ret = ErrFail;
            } else if (cr.nwarn > (warnpercent/100.0 * npels) || cr.maxerror > hardwarn) {
                if (ret != ErrFail)
                    ret = ErrWarn;
            }

            // Print the report
            //
            if (verbose || (ret != ErrOK && !quiet)) {
                if (compareall)
                    print_subimage (img0, subimage, m);
                std::cout << "  Mean error = ";
                safe_double_print (cr.meanerror);
                std::cout << "  RMS error = ";
                safe_double_print (cr.rms_error);
                std::cout << "  Peak SNR = ";
                safe_double_print (cr.PSNR);
                std::cout << "  Max error  = " << cr.maxerror;
                if (cr.maxerror != 0) {
                    std::cout << " @ (" << cr.maxx << ", " << cr.maxy;
                    if (img0.spec().depth > 1)
                        std::cout << ", " << cr.maxz;
                    if (cr.maxc < (int)img0.spec().channelnames.size())
                        std::cout << ", " << img0.spec().channelnames[cr.maxc] << ')';
                    else if (cr.maxc < (int)img1.spec().channelnames.size())
                        std::cout << ", " << img1.spec().channelnames[cr.maxc] << ')';
                    else
                        std::cout << ", channel " << cr.maxc << ')';
                    if (! img0.deep()) {
                        std::cout << "  values are ";
                        for (int c = 0; c < img0.spec().nchannels; ++c)
                            std::cout << (c ? ", " : "") << img0.getchannel(cr.maxx, cr.maxy, 0, c);
                        std::cout << " vs ";
                        for (int c = 0; c < img1.spec().nchannels; ++c)
                            std::cout << (c ? ", " : "") << img1.getchannel(cr.maxx, cr.maxy, 0, c);
                    }
                }
                std::cout << "\n";
#if OIIO_MSVS_BEFORE_2015
                // When older Visual Studio is used, float values in
                // scientific foramt are printed with three digit exponent.
                // We change this behaviour to fit Linux way.
                _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
                std::streamsize precis = std::cout.precision();
                std::cout << "  " << cr.nwarn << " pixels (" 
                          << std::setprecision(3) << (100.0*cr.nwarn / npels) 
                          << std::setprecision(precis) << "%) over " << warnthresh << "\n";
                std::cout << "  " << cr.nfail << " pixels (" 
                          << std::setprecision(3) << (100.0*cr.nfail / npels) 
                          << std::setprecision(precis) << "%) over " << failthresh << "\n";
                if (perceptual)
                    std::cout << "  " << yee_failures << " pixels ("
                              << std::setprecision(3) << (100.0*yee_failures / npels) 
                              << std::setprecision(precis)
                              << "%) failed the perceptual test\n";
            }

            // If the user requested that a difference image be output,
            // do that.  N.B. we only do this for the first subimage
            // right now, because ImageBuf doesn't really know how to
            // write subimages.
            if (diffimage.size() && (cr.maxerror != 0 || !outdiffonly)) {
                ImageBuf diff;
                if (diffabs)
                    ImageBufAlgo::absdiff (diff, img0, img1);
                else
                    ImageBufAlgo::sub (diff, img0, img1);
                if (diffscale != 1.0f)
                    ImageBufAlgo::mul (diff, diff, diffscale);
                diff.write (diffimage);

                // Clear diff image name so we only save the first
                // non-matching subimage.
                diffimage = "";
            }
        }
    }

    if (compareall && img0.nsubimages() != img1.nsubimages()) {
        if (! quiet)
            std::cerr << "Images had differing numbers of subimages ("
                      << img0.nsubimages() << " vs " << img1.nsubimages() << ")\n";
        ret = ErrFail;
    }
    if (!compareall && (img0.nsubimages() > 1 || img1.nsubimages() > 1)) {
        if (! quiet)
            std::cout << "Only compared the first subimage (of "
                      << img0.nsubimages() << " and " << img1.nsubimages()
                      << ", respectively)\n";
    }

    if (ret == ErrOK) {
        if (! quiet)
            std::cout << "PASS\n";
    }
    else if (ret == ErrWarn) {
        if (! quiet)
            std::cout << "WARNING\n";
    }
    else if (ret) {
        if (quiet)
            std::cerr << "FAILURE\n";
        else
            std::cout << "FAILURE\n";
    }

    imagecache->invalidate_all (true);
    ImageCache::destroy (imagecache);
    return ret;
}
