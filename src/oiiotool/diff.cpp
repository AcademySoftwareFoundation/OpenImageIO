// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "oiiotool.h"

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

using namespace OIIO;
using namespace OiioTool;
using namespace ImageBufAlgo;



inline void
print_subimage(ImageRec& img0, int subimage, int miplevel)
{
    if (img0.subimages() > 1)
        print("Subimage {} ", subimage);
    if (img0.miplevels(subimage) > 1)
        print(" MIP level {} ", miplevel);
    if (img0.subimages() > 1 || img0.miplevels(subimage) > 1)
        print(": ");
    const ImageSpec& spec(*img0.spec(subimage));
    print("{} x {}", spec.width, spec.height);
    if (spec.depth > 1)
        print(" x {}", spec.depth);
    print(", {} channel\n", spec.nchannels);
}



int
Oiiotool::do_action_diff(ImageRecRef ir0, ImageRecRef ir1, Oiiotool& ot,
                         int perceptual)
{
    print("Computing {}diff of \"{}\" vs \"{}\"\n",
          perceptual ? "perceptual " : "", ir0->name(), ir1->name());
    read(ir0);
    read(ir1);

    int ret = DiffErrOK;
    for (int subimage = 0; subimage < ir0->subimages(); ++subimage) {
        if (subimage > 0 && !ot.allsubimages)
            break;
        if (subimage >= ir1->subimages())
            break;

        for (int m = 0; m < ir0->miplevels(subimage); ++m) {
            if (m > 0 && !ot.allsubimages)
                break;
            if (m > 0 && ir0->miplevels(subimage) != ir1->miplevels(subimage)) {
                print("Files do not match in their number of MIPmap levels\n");
                ret = DiffErrDifferentSize;
                break;
            }

            ImageBuf& img0((*ir0)(subimage, m));
            ImageBuf& img1((*ir1)(subimage, m));
            int npels = img0.spec().width * img0.spec().height
                        * img0.spec().depth;
            if (npels == 0)
                npels = 1;  // Avoid divide by zero for 0x0 images
            OIIO_ASSERT(img0.spec().format == TypeDesc::FLOAT);

            // Compare the two images.
            //
            ImageBufAlgo::CompareResults cr;
            int yee_failures = 0;
            switch (perceptual) {
            case 1:
                yee_failures = ImageBufAlgo::compare_Yee(img0, img1, cr);
                break;
            default:
                cr = ImageBufAlgo::compare(img0, img1, ot.diff_failthresh,
                                           ot.diff_warnthresh);
                break;
            }

            if (cr.nfail > (ot.diff_failpercent / 100.0 * npels)
                || cr.maxerror > ot.diff_hardfail
                || yee_failures > (ot.diff_failpercent / 100.0 * npels)) {
                ret = DiffErrFail;
            } else if (cr.nwarn > (ot.diff_warnpercent / 100.0 * npels)
                       || cr.maxerror > ot.diff_hardwarn) {
                if (ret != DiffErrFail)
                    ret = DiffErrWarn;
            }

            // Print the report
            //
            if (ot.verbose || ot.debug || ret != DiffErrOK) {
                if (ot.allsubimages)
                    print_subimage(*ir0, subimage, m);
                if (!perceptual) {
                    print("  Mean error = {:.6g}\n", cr.meanerror);
                    print("  RMS error = {:.6g}\n", cr.rms_error);
                    print("  Peak SNR = {:.6g}\n", cr.PSNR);
                }
                print("  Max error  = {}", cr.maxerror);
                if (cr.maxerror != 0) {
                    print(" @ ({}, {}", cr.maxx, cr.maxy);
                    if (img0.spec().depth > 1)
                        print(", {}", cr.maxz);
                    if (cr.maxc < (int)img0.spec().channelnames.size())
                        print(", {})", img0.spec().channelnames[cr.maxc]);
                    else if (cr.maxc < (int)img1.spec().channelnames.size())
                        print(", {})", img1.spec().channelnames[cr.maxc]);
                    else
                        print(", channel {})", cr.maxc);
                    if (!img0.deep()) {
                        print("  values are ");
                        for (int c = 0; c < img0.spec().nchannels; ++c)
                            print("{}{}", (c ? ", " : ""),
                                  img0.getchannel(cr.maxx, cr.maxy, 0, c));
                        print(" vs ");
                        for (int c = 0; c < img1.spec().nchannels; ++c)
                            print("{}{}", (c ? ", " : ""),
                                  img1.getchannel(cr.maxx, cr.maxy, 0, c));
                    }
                }
                print("\n");
                if (perceptual == 1) {
                    print("  {} pixels ({:.3g}%) failed the perceptual test\n",
                          yee_failures, 100.0 * yee_failures / npels);
                } else {
                    print("  {} pixels ({:.3g}%) over {}\n", cr.nwarn,
                          (100.0 * cr.nwarn / npels), ot.diff_warnthresh);
                    print("  {} pixels ({:.3g}%) over {}\n", cr.nfail,
                          (100.0 * cr.nfail / npels), ot.diff_failthresh);
                }
            }
        }
    }

    if (ot.allsubimages && ir0->subimages() != ir1->subimages()) {
        print("Images had differing numbers of subimages ({} vs {})\n",
              ir0->subimages(), ir1->subimages());
        ret = DiffErrFail;
    }
    if (!ot.allsubimages && (ir0->subimages() > 1 || ir1->subimages() > 1)) {
        print("Only compared the first subimage (of {} and {}, respectively)\n",
              ir0->subimages(), ir1->subimages());
    }

    if (ret == DiffErrOK)
        print("PASS\n");
    else if (ret == DiffErrWarn)
        print("WARNING\n");
    else {
        print("FAILURE\n");
        ot.return_value = ret;
    }
    fflush(stdout);
    return ret;
}
