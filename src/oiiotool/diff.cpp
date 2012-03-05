/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
#include <iomanip>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

using boost::algorithm::iequals;


#include "argparse.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "filter.h"

#include "oiiotool.h"

OIIO_NAMESPACE_USING
using namespace OiioTool;


using namespace ImageBufAlgo;




// function that standarize printing NaN and Inf values on
// Windows (where they are in 1.#INF, 1.#NAN format) and all
// others platform
inline void
safe_double_print (double val)
{
    if (isnan (val))
        std::cout << "nan";
    else if (isinf (val))
        std::cout << "inf";
    else
        std::cout << val;
    std::cout << '\n';
}



inline void
print_subimage (ImageRec &img0, int subimage, int miplevel)
{
    if (img0.subimages() > 1)
        std::cout << "Subimage " << subimage << ' ';
    if (img0.miplevels(subimage) > 1)
        std::cout << " MIP level " << miplevel << ' ';
    if (img0.subimages() > 1 || img0.miplevels(subimage) > 1)
        std::cout << ": ";
    const ImageSpec &spec (*img0.spec(subimage));
    std::cout << spec.width << " x " << spec.height;
    if (spec.depth > 1)
        std::cout << " x " << spec.depth;
    std::cout << ", " << spec.nchannels << " channel\n";
}



int
OiioTool::do_action_diff (ImageRec &ir0, ImageRec &ir1,
                          Oiiotool &ot)
{
    std::cout << "Computing diff of \"" << ir0.name() << "\" vs \""
              << ir1.name() << "\"\n";
    ir0.read ();
    ir1.read ();

    int ret = DiffErrOK;
    for (int subimage = 0;  subimage < ir0.subimages();  ++subimage) {
        if (subimage > 0 && !ot.allsubimages)
            break;
        if (subimage >= ir1.subimages())
            break;

        if (ir0.miplevels(subimage) != ir1.miplevels(subimage)) {
            std::cout << "Files do not match in their number of MIPmap levels\n";
        }

        for (int m = 0;  m < ir0.miplevels(subimage);  ++m) {
            if (m > 0 && !ot.allsubimages)
                break;
            if (m > 0 && ir0.miplevels(subimage) != ir1.miplevels(subimage)) {
                std::cout << "Files do not match in their number of MIPmap levels\n";
                ret = DiffErrDifferentSize;
                break;
            }

            ImageBuf &img0 (ir0(subimage,m));
            ImageBuf &img1 (ir1(subimage,m));

            // Compare the dimensions of the images.  Fail if they
            // aren't the same resolution and number of channels.  No
            // problem, though, if they aren't the same data type.
            if (! same_size (img0, img1)) {
                print_subimage (ir0, subimage, m);
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
                ret = DiffErrDifferentSize;
                break;
            }

            int npels = img0.spec().width * img0.spec().height * img0.spec().depth;
            if (npels == 0)
                npels = 1;    // Avoid divide by zero for 0x0 images
            ASSERT (img0.spec().format == TypeDesc::FLOAT);

            // Compare the two images.
            //
            ImageBufAlgo::CompareResults cr;
            ImageBufAlgo::compare (img0, img1, ot.diff_failthresh, ot.diff_warnthresh, cr);

            int yee_failures = 0;
#if 0
            if (perceptual)
                yee_failures = ImageBufAlgo::compare_Yee (img0, img1);
#endif
            if (cr.nfail > (ot.diff_failpercent/100.0 * npels) || cr.maxerror > ot.diff_hardfail ||
                yee_failures > (ot.diff_failpercent/100.0 * npels)) {
                ret = DiffErrFail;
            } else if (cr.nwarn > (ot.diff_warnpercent/100.0 * npels) || cr.maxerror > ot.diff_hardwarn) {
                if (ret != DiffErrFail)
                    ret = DiffErrWarn;
            }

            // Print the report
            //
            if (ot.verbose || ret != DiffErrOK) {
                if (ot.allsubimages)
                    print_subimage (ir0, subimage, m);
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
                    std::cout << ", " << img0.spec().channelnames[cr.maxc] << ')';
                }
                std::cout << "\n";
// when Visual Studio is used float values in scientific foramt are 
// printed with three digit exponent. We change this behaviour to fit
// Linux way
#ifdef _MSC_VER
                _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
                std::streamsize precis = std::cout.precision();
                std::cout << "  " << cr.nwarn << " pixels (" 
                          << std::setprecision(3) << (100.0*cr.nwarn / npels) 
                          << std::setprecision(precis) << "%) over " << ot.diff_warnthresh << "\n";
                std::cout << "  " << cr.nfail << " pixels (" 
                          << std::setprecision(3) << (100.0*cr.nfail / npels) 
                          << std::setprecision(precis) << "%) over " << ot.diff_failthresh << "\n";
#if 0
                if (perceptual)
                    std::cout << "  " << yee_failures << " pixels ("
                              << std::setprecision(3) << (100.0*yee_failures / npels) 
                              << std::setprecision(precis)
                              << "%) failed the perceptual test\n";
#endif
            }

#if 0
            // If the user requested that a difference image be output,
            // do that.  N.B. we only do this for the first subimage
            // right now, because ImageBuf doesn't really know how to
            // write subimages.
            if (diffimage.size() && (cr.maxerror != 0 || !outdiffonly)) {
                ImageBuf diff (diffimage, img0.spec());
                ImageBuf::ConstIterator<float,float> pix0 (img0);
                ImageBuf::ConstIterator<float,float> pix1 (img1);
                ImageBuf::Iterator<float,float> pixdiff (diff);
                // Subtract the second image from the first.  At which
                // time we no longer need the second image, so free it.
                if (diffabs) {
                    for (  ;  pix0.valid();  ++pix0) {
                        pix1.pos (pix0.x(), pix0.y());  // ensure alignment
                        pixdiff.pos (pix0.x(), pix0.y());
                        for (int c = 0;  c < img0.nchannels();  ++c)
                            pixdiff[c] = diffscale * fabsf (pix0[c] - pix1[c]);
                    }
                } else {
                    for (  ;  pix0.valid();  ++pix0) {
                        pix1.pos (pix0.x(), pix0.y());  // ensure alignment
                        pixdiff.pos (pix0.x(), pix0.y());
                        for (int c = 0;  c < img0.spec().nchannels;  ++c)
                            pixdiff[c] = diffscale * (pix0[c] - pix1[c]);
                    }
                }

                diff.save (diffimage);

                // Clear diff image name so we only save the first
                // non-matching subimage.
                diffimage = "";
            }
#endif
        }
    }

    if (ot.allsubimages && ir0.subimages() != ir1.subimages()) {
        std::cout << "Images had differing numbers of subimages ("
                  << ir0.subimages() << " vs " << ir1.subimages() << ")\n";
        ret = DiffErrFail;
    }
    if (!ot.allsubimages && (ir0.subimages() > 1 || ir1.subimages() > 1)) {
        std::cout << "Only compared the first subimage (of "
                  << ir0.subimages() << " and " << ir1.subimages() 
                  << ", respectively)\n";
    }

    if (ret == DiffErrOK)
        std::cout << "PASS\n";
    else if (ret == DiffErrWarn)
        std::cout << "WARNING\n";
    else {
        std::cout << "FAILURE\n";
    }
    return ret;
}
