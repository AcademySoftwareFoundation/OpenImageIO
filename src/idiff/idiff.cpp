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

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/sysutil.h>


using namespace OIIO;
using OIIO::Strutil::print;


enum idiffErrors {
    ErrOK = 0,         ///< No errors, the images match exactly
    ErrWarn,           ///< Warning: the errors differ a little
    ErrFail,           ///< Failure: the errors differ a lot
    ErrDifferentSize,  ///< Images aren't even the same size
    ErrFile,           ///< Could not find or open input files, etc.
    ErrLast
};



static ArgParse
getargs(int argc, char* argv[])
{
    // clang-format off
    ArgParse ap;
    ap.intro("idiff -- compare two images\n"
             OIIO_INTRO_STRING)
      .usage("idiff [options] <image1> <image2 | directory>")
      .add_version(OIIO_VERSION_STRING)
      .print_defaults(true);

    ap.arg("filename")
      .hidden()
      .action(ArgParse::append());
    ap.arg("-v")
      .help("Verbose status messages");
    ap.arg("-q")
      .help("Quiet (minimal messages)");
    ap.arg("-a")
      .help("Compare all subimages/miplevels");

    ap.separator("Thresholding and comparison options");
    ap.arg("-fail")
      .help("Failure absolute difference threshold")
      .metavar("VAL")
      .defaultval(1.0e-6f);
    ap.arg("-failrelative")
      .help("Failure relative threshold")
      .metavar("VAL")
      .defaultval(0.0f);
    ap.arg("-failpercent")
      .help("Allow this percentage of failures")
      .metavar("PERCENT")
      .defaultval(0.0f);
    ap.arg("-hardfail")
      .help("Fail if any one pixel exceeds this error")
      .metavar("VAL")
      .defaultval(std::numeric_limits<float>::infinity());
    ap.arg("-allowfailures")
      .help("OK for this number of pixels to fail by any amount")
      .metavar("N")
      .defaultval(0);
    ap.arg("-warn")
      .help("Warning absolute difference threshold")
      .metavar("VAL")
      .defaultval(1.0e-6f);
    ap.arg("-warnrelative")
      .help("Warning relative threshold")
      .metavar("VAL")
      .defaultval(0.0f);
    ap.arg("-warnpercent")
      .help("Allow this percentage of warnings")
      .metavar("PERCENT")
      .defaultval(0.0f);
    ap.arg("-hardwarn")
      .help("Warn if any one pixel exceeds this error")
      .metavar("VAL")
      .defaultval(std::numeric_limits<float>::infinity());
    ap.arg("-p")
      .help("Perform perceptual (rather than numeric) comparison");

    ap.separator("Difference image options");
    ap.arg("-o")
      .help("Output difference image")
      .metavar("FILENAME");
    ap.arg("-od")
      .help("Output image only if nonzero difference");
    ap.arg("-abs")
      .help("Output image of absolute value, not signed difference");
    ap.arg("-scale")
      .help("Scale the output image by this factor")
      .defaultval(1.0f)
      .metavar("FACTOR");

    ap.parse(argc, (const char**)argv);

    return ap;
    // clang-format on
}



static bool
read_input(const std::string& filename, ImageBuf& img,
           std::shared_ptr<ImageCache> cache, int subimage = 0,
           int miplevel = 0)
{
    if (img.subimage() >= 0 && img.subimage() == subimage
        && img.miplevel() == miplevel)
        return true;

    img.reset(filename, 0, 0, cache);
    if (img.read(subimage, miplevel, false, TypeFloat))
        return true;

    print(stderr, "idiff ERROR: Could not read {}:\n\t{}\n", filename,
          img.geterror());
    return false;
}



// function that standardize printing NaN and Inf values on
// Windows (where they are in 1.#INF, 1.#NAN format) and all
// others platform
inline void
safe_double_print(double val)
{
    if (std::isnan(val))
        print("nan\n");
    else if (std::isinf(val))
        print("inf\n");
    else
        print("{:g}\n", val);
}



inline void
print_subimage(ImageBuf& img0, int subimage, int miplevel)
{
    if (img0.nsubimages() > 1)
        print("Subimage {} ", subimage);
    if (img0.nmiplevels() > 1)
        print(" MIP level {} ", miplevel);
    if (img0.nsubimages() > 1 || img0.nmiplevels() > 1)
        print(": ");
    print("{} x {}", img0.spec().width, img0.spec().height);
    if (img0.spec().depth > 1)
        print(" x {}", img0.spec().depth);
    print(", {} channels\n", img0.spec().nchannels);
}


// Append the filename from "first" when "second" is a directory.
// "second" is an output variable and modified in-place.
inline void
add_filename_to_directory(const std::string& first, std::string& second)
{
    if (Filesystem::is_directory(second)) {
        char last_byte = second.at(second.size() - 1);
        if (last_byte != '/' && last_byte != '\\') {
#if defined(_MSC_VER)
            second += '\\';
#else
            second += '/';
#endif
        }
        second += Filesystem::filename(first);
    }
}


int
main(int argc, char* argv[])
{
    // Helpful for debugging to make sure that any crashes dump a stack
    // trace.
    Sysutil::setup_crash_stacktrace("stdout");

    Filesystem::convert_native_arguments(argc, (const char**)argv);
    ArgParse ap = getargs(argc, argv);

    std::vector<std::string> filenames = ap["filename"].as_vec<std::string>();
    if (filenames.size() == 2) {
        add_filename_to_directory(filenames[0], filenames[1]);
    } else {
        print(stderr, "idiff: Must have two input filenames.\n");
        print(stderr, "> {}\n", Strutil::join(filenames, ", "));
        ap.usage();
        return EXIT_FAILURE;
    }
    bool verbose          = ap["v"].get<int>();
    bool quiet            = ap["q"].get<int>();
    bool compareall       = ap["a"].get<int>();
    bool outdiffonly      = ap["od"].get<int>();
    bool diffabs          = ap["abs"].get<int>();
    bool perceptual       = ap["p"].get<int>();
    std::string diffimage = ap["o"].get();
    float diffscale       = ap["scale"].get<float>();
    float failthresh      = ap["fail"].get<float>();
    float failrelative    = ap["failrelative"].get<float>();
    float failpercent     = ap["failpercent"].get<float>();
    float hardfail        = ap["hardfail"].get<float>();
    float warnthresh      = ap["warn"].get<float>();
    float warnrelative    = ap["warnrelative"].get<float>();
    float warnpercent     = ap["warnpercent"].get<float>();
    float hardwarn        = ap["hardwarn"].get<float>();
    int allowfailures     = ap["allowfailures"].get<int>();

    if (!quiet) {
        print("Comparing \"{}\" and \"{}\"\n", filenames[0], filenames[1]);
        fflush(stdout);
    }

    // Create a private ImageCache so we can customize its cache size
    // and instruct it store everything internally as floats.
    std::shared_ptr<ImageCache> imagecache = ImageCache::create(true);
    imagecache->attribute("forcefloat", 1);
    if (sizeof(void*) == 4)  // 32 bit or 64?
        imagecache->attribute("max_memory_MB", 512.0);
    else
        imagecache->attribute("max_memory_MB", 2048.0);
    imagecache->attribute("autotile", 256);
    // force a full diff, even for files tagged with the same
    // fingerprint, just in case some mistake has been made.
    imagecache->attribute("deduplicate", 0);

    ImageBuf img0, img1;
    if (!read_input(filenames[0], img0, imagecache)
        || !read_input(filenames[1], img1, imagecache))
        return ErrFile;
    //    ImageSpec spec0 = img0.spec();  // stash it

    int ret = ErrOK;
    for (int subimage = 0; subimage < img0.nsubimages(); ++subimage) {
        if (subimage > 0 && !compareall)
            break;
        if (subimage >= img1.nsubimages())
            break;

        if (!read_input(filenames[0], img0, imagecache, subimage)
            || !read_input(filenames[1], img1, imagecache, subimage)) {
            print(stderr, "Failed to read subimage {}\n", subimage);
            return ErrFile;
        }

        if (img0.nmiplevels() != img1.nmiplevels()) {
            if (!quiet)
                print("Files do not match in their number of MIPmap levels\n");
        }

        for (int m = 0; m < img0.nmiplevels(); ++m) {
            if (m > 0 && !compareall)
                break;
            if (m > 0 && img0.nmiplevels() != img1.nmiplevels()) {
                print(stderr,
                      "Files do not match in their number of MIPmap levels\n");
                ret = ErrDifferentSize;
                break;
            }

            if (!read_input(filenames[0], img0, imagecache, subimage, m)
                || !read_input(filenames[1], img1, imagecache, subimage, m))
                return ErrFile;

            if (img0.deep() != img1.deep()) {
                print(stderr,
                      "One image contains deep data, the other does not\n");
                ret = ErrDifferentSize;
                break;
            }

            int npels = img0.spec().width * img0.spec().height
                        * img0.spec().depth;
            if (npels == 0)
                npels = 1;  // Avoid divide by zero for 0x0 images
            OIIO_ASSERT(img0.spec().format == TypeFloat);

            // Compare the two images.
            //
            auto cr = ImageBufAlgo::compare(img0, img1, failthresh, warnthresh,
                                            failrelative, warnrelative);

            int yee_failures = 0;
            if (perceptual && !img0.deep()) {
                cr           = {};
                yee_failures = ImageBufAlgo::compare_Yee(img0, img1, cr);
            }

            if (cr.nfail <= imagesize_t(allowfailures)) {
                // Pass if users set allowfailures and we are within that
                // limit.
            } else if (cr.nfail > (failpercent / 100.0 * npels)
                       || cr.maxerror > hardfail
                       || yee_failures > (failpercent / 100.0 * npels)) {
                ret = ErrFail;
            } else if (cr.nwarn > (warnpercent / 100.0 * npels)
                       || cr.maxerror > hardwarn) {
                if (ret != ErrFail)
                    ret = ErrWarn;
            }

            // Print the report
            //
            if (verbose || (ret != ErrOK && !quiet)) {
                if (compareall)
                    print_subimage(img0, subimage, m);
                print("  Mean error = ");
                safe_double_print(cr.meanerror);
                print("  RMS error = ");
                safe_double_print(cr.rms_error);
                print("  Peak SNR = ");
                safe_double_print(cr.PSNR);
                print("  Max error  = {:g}", cr.maxerror);
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
                        ;
                        print(" vs ");
                        for (int c = 0; c < img1.spec().nchannels; ++c)
                            print("{}{}", (c ? ", " : ""),
                                  img1.getchannel(cr.maxx, cr.maxy, 0, c));
                        ;
                    }
                }
                print("\n");
#if OIIO_MSVS_BEFORE_2015
                // When older Visual Studio is used, float values in
                // scientific format are printed with three digit exponent.
                // We change this behaviour to fit Linux way.
                _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
                print("  {} pixels ({:1.3g}%) over {}\n", cr.nwarn,
                      (100.0 * cr.nwarn / npels), warnthresh);
                print("  {} pixels ({:1.3g}%) over {}\n", cr.nfail,
                      (100.0 * cr.nfail / npels), failthresh);
                if (perceptual)
                    print("  {} pixels ({:3g}%) failed the perceptual test\n",
                          yee_failures, (100.0 * yee_failures / npels));
            }

            // If the user requested that a difference image be output,
            // do that.  N.B. we only do this for the first subimage
            // right now, because ImageBuf doesn't really know how to
            // write subimages.
            if (diffimage.size() && (cr.maxerror != 0 || !outdiffonly)) {
                ImageBuf diff;
                if (diffabs)
                    ImageBufAlgo::absdiff(diff, img0, img1);
                else
                    ImageBufAlgo::sub(diff, img0, img1);
                if (diffscale != 1.0f)
                    ImageBufAlgo::mul(diff, diff, diffscale);
                diff.write(diffimage);

                // Clear diff image name so we only save the first
                // non-matching subimage.
                diffimage = "";
            }
        }
    }

    if (compareall && img0.nsubimages() != img1.nsubimages()) {
        if (!quiet)
            print(stderr,
                  "Images had differing numbers of subimages ({} vs {})\n",
                  img0.nsubimages(), img1.nsubimages());
        ret = ErrFail;
    }
    if (!compareall && (img0.nsubimages() > 1 || img1.nsubimages() > 1)) {
        if (!quiet)
            print(
                "Only compared the first subimage (of {} and {}, respectively)\n",
                img0.nsubimages(), img1.nsubimages());
    }

    if (ret == ErrOK) {
        if (!quiet)
            print("PASS\n");
    } else if (ret == ErrWarn) {
        if (!quiet)
            print("WARNING\n");
    } else if (ret) {
        if (quiet)
            print(stderr, "FAILURE\n");
        else
            print("FAILURE\n");
    }

    imagecache->invalidate_all(true);
    shutdown();
    return ret;
}
