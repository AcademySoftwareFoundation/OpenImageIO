// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#if defined(_MSC_VER)
#    include <io.h>
#endif

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN
using namespace ImageBufAlgo;
namespace pvt {


std::string
compute_sha1(ImageInput* input, int subimage, int miplevel, std::string& err)
{
    SHA1 sha;
    ImageSpec spec = input->spec_dimensions(subimage);
    if (spec.deep) {
        // Special handling of deep data
        DeepData dd;
        if (!input->read_native_deep_image(subimage, 0, dd)) {
            err = input->geterror();
            if (err.empty())
                err = "could not read image";
            return std::string();
        }
        // Hash both the sample counts and the data block
        sha.append(dd.all_samples());
        sha.append(dd.all_data());
    } else {
        imagesize_t size = spec.image_bytes(true /*native*/);
        if (size >= std::numeric_limits<size_t>::max()) {
            err = std::string("unable to compute, image is too big");
            return std::string();
        } else if (size != 0) {
            std::unique_ptr<char[]> buf(new char[size]);
            if (!input->read_image(subimage, miplevel, 0, spec.nchannels,
                                   TypeDesc::UNKNOWN /*native*/, &buf[0])) {
                err = input->geterror();
                if (err.empty())
                    err = "could not read image";
                return std::string();
            }
            sha.append(&buf[0], size);
        }
    }

    return sha.digest();
}



static std::string
stats_num(float val, int maxval, bool round)
{
    // Ensure uniform printing of NaN and Inf on all platforms
    using Strutil::fmt::format;
    std::string result;
    if (isnan(val))
        result = "nan";
    else if (isinf(val))
        result = "inf";
    else if (maxval == 0) {
        result = format("{:f}", val);
    } else {
        float fval = val * static_cast<float>(maxval);
        if (round) {
            int v  = static_cast<int>(roundf(fval));
            result = format("{}", v);
        } else {
            result = format("{:0.2f}", fval);
        }
    }
    return result;
}



// First check oiio:BitsPerSample int attribute.  If not set,
// fall back on the TypeDesc. return 0 for float types
// or those that exceed the int range (long long, etc)
static unsigned long long
get_intsample_maxval(const ImageSpec& spec)
{
    TypeDesc type = spec.format;
    int bits      = spec.get_int_attribute("oiio:BitsPerSample");
    if (bits > 0) {
        if (type.basetype == TypeDesc::UINT8
            || type.basetype == TypeDesc::UINT16
            || type.basetype == TypeDesc::UINT32)
            return ((1LL) << bits) - 1;
        if (type.basetype == TypeDesc::INT8 || type.basetype == TypeDesc::INT16
            || type.basetype == TypeDesc::INT32)
            return ((1LL) << (bits - 1)) - 1;
    }

    // These correspond to all the int enums in typedesc.h <= int
    if (type.basetype == TypeDesc::UCHAR)
        return 0xff;
    if (type.basetype == TypeDesc::CHAR)
        return 0x7f;
    if (type.basetype == TypeDesc::USHORT)
        return 0xffff;
    if (type.basetype == TypeDesc::SHORT)
        return 0x7fff;
    if (type.basetype == TypeDesc::UINT)
        return 0xffffffff;
    if (type.basetype == TypeDesc::INT)
        return 0x7fffffff;

    return 0;
}


std::string
stats_footer(unsigned int maxval)
{
    if (maxval == 0)
        return "(float)";
    else
        return Strutil::fmt::format("(of {})", maxval);
}



static void
print_stats_summary(std::ostream& out, string_view indent,
                    const ImageBufAlgo::PixelStats& stats,
                    const ImageSpec& spec)
{
    unsigned int maxval = (unsigned int)get_intsample_maxval(spec);

    print(out, "{}Stats Min: ", indent);
    for (unsigned int i = 0; i < stats.min.size(); ++i) {
        Strutil::print(out, "{} ", stats_num(stats.min[i], maxval, true));
    }
    Strutil::print(out, "{}\n", stats_footer(maxval));

    print(out, "{}Stats Max: ", indent);
    for (unsigned int i = 0; i < stats.max.size(); ++i) {
        Strutil::print(out, "{} ", stats_num(stats.max[i], maxval, true));
    }
    print(out, "{}\n", stats_footer(maxval));

    print(out, "{}Stats Avg: ", indent);
    for (unsigned int i = 0; i < stats.avg.size(); ++i) {
        print(out, "{} ", stats_num(stats.avg[i], maxval, false));
    }
    print(out, "{}\n", stats_footer(maxval));

    print(out, "{}Stats StdDev: ", indent);
    for (unsigned int i = 0; i < stats.stddev.size(); ++i) {
        print(out, "{} ", stats_num(stats.stddev[i], maxval, false));
    }
    print(out, "{}\n", stats_footer(maxval));

    print(out, "{}Stats NanCount: ", indent);
    for (unsigned int i = 0; i < stats.nancount.size(); ++i) {
        print(out, "{} ", (unsigned long long)stats.nancount[i]);
    }
    print(out, "\n");

    print(out, "{}Stats InfCount: ", indent);
    for (unsigned int i = 0; i < stats.infcount.size(); ++i) {
        print(out, "{} ", (unsigned long long)stats.infcount[i]);
    }
    print(out, "\n");

    print(out, "{}Stats FiniteCount: ", indent);
    for (unsigned int i = 0; i < stats.finitecount.size(); ++i) {
        print(out, "{} ", (unsigned long long)stats.finitecount[i]);
    }
    print(out, "\n");
}



static void
print_deep_stats(std::ostream& out, string_view indent, const ImageBuf& input,
                 const ImageSpec& spec)
{
    const DeepData* dd(input.deepdata());
    size_t npixels      = dd->pixels();
    size_t totalsamples = 0, emptypixels = 0;
    size_t maxsamples = 0, minsamples = std::numeric_limits<size_t>::max();
    size_t maxsamples_npixels = 0;
    float mindepth            = std::numeric_limits<float>::max();
    float maxdepth            = -std::numeric_limits<float>::max();
    Imath::V3i maxsamples_pixel(-1, -1, -1);
    Imath::V3i mindepth_pixel(-1, -1, -1), maxdepth_pixel(-1, -1, -1);
    Imath::V3i nonfinite_pixel(-1, -1, -1);
    int nonfinite_pixel_samp(-1), nonfinite_pixel_chan(-1);
    int nchannels        = dd->channels();
    int depthchannel     = -1;
    long long nonfinites = 0;
    for (int c = 0; c < nchannels; ++c)
        if (Strutil::iequals(spec.channelnames[c], "Z"))
            depthchannel = c;
    int xend = spec.x + spec.width;
    int yend = spec.y + spec.height;
    int zend = spec.z + spec.depth;
    size_t p = 0;
    std::vector<size_t> nsamples_histogram;
    for (int z = spec.z; z < zend; ++z) {
        for (int y = spec.y; y < yend; ++y) {
            for (int x = spec.x; x < xend; ++x, ++p) {
                size_t samples = input.deep_samples(x, y, z);
                totalsamples += samples;
                if (samples == maxsamples)
                    ++maxsamples_npixels;
                if (samples > maxsamples) {
                    maxsamples = samples;
                    maxsamples_pixel.setValue(x, y, z);
                    maxsamples_npixels = 1;
                }
                if (samples < minsamples)
                    minsamples = samples;
                if (samples == 0)
                    ++emptypixels;
                if (samples >= nsamples_histogram.size())
                    nsamples_histogram.resize(samples + 1, 0);
                nsamples_histogram[samples] += 1;
                for (unsigned int s = 0; s < samples; ++s) {
                    for (int c = 0; c < nchannels; ++c) {
                        float d = input.deep_value(x, y, z, c, s);
                        if (!isfinite(d)) {
                            if (nonfinites++ == 0) {
                                nonfinite_pixel.setValue(x, y, z);
                                nonfinite_pixel_samp = s;
                                nonfinite_pixel_chan = c;
                            }
                        }
                        if (depthchannel == c) {
                            if (d < mindepth) {
                                mindepth = d;
                                mindepth_pixel.setValue(x, y, z);
                            }
                            if (d > maxdepth) {
                                maxdepth = d;
                                maxdepth_pixel.setValue(x, y, z);
                            }
                        }
                    }
                }
            }
        }
    }
    print(out, "{}Min deep samples in any pixel : {}\n", indent, minsamples);
    print(out, "{}Max deep samples in any pixel : {}\n", indent, maxsamples);
    print(out,
          "{}{} pixel{} had the max of {} samples, including (x={}, y={})\n",
          indent, maxsamples_npixels, maxsamples_npixels > 1 ? "s" : "",
          maxsamples, maxsamples_pixel.x, maxsamples_pixel.y);
    print(out, "{}Average deep samples per pixel: {:.2f}\n", indent,
          double(totalsamples) / double(npixels));
    print(out, "{}Total deep samples in all pixels: {}\n", indent,
          totalsamples);
    print(out, "{}Pixels with deep samples   : {}\n", indent,
          (npixels - emptypixels));
    print(out, "{}Pixels with no deep samples: {}\n", indent, emptypixels);
    print(out, "{}Samples/pixel histogram:\n", indent);
    size_t grandtotal = 0;
    for (size_t i = 0, e = nsamples_histogram.size(); i < e; ++i)
        grandtotal += nsamples_histogram[i];
    size_t binstart = 0, bintotal = 0;
    for (size_t i = 0, e = nsamples_histogram.size(); i < e; ++i) {
        bintotal += nsamples_histogram[i];
        if (i < 8 || i == (e - 1) || OIIO::ispow2(i + 1)) {
            // batch by powers of 2, unless it's a small number
            if (i == binstart)
                print(out, "{}  {:3}    ", indent, i);
            else
                print(out, "{}  {:3}-{:3}", indent, binstart, i);
            print(out, " : {:8} ({:4.1f}%)\n", bintotal,
                  (100.0 * bintotal) / grandtotal);
            binstart = i + 1;
            bintotal = 0;
        }
    }
    if (depthchannel >= 0) {
        print(out, "{}Minimum depth was {:g} at ({}, {})\n", indent, mindepth,
              mindepth_pixel.x, mindepth_pixel.y);
        print(out, "{}Maximum depth was {:g} at ({}, {})\n", indent, maxdepth,
              maxdepth_pixel.x, maxdepth_pixel.y);
    }
    if (nonfinites > 0) {
        print(
            out,
            "{}Nonfinite values: {}, including (x={}, y={}, chan={}, samp={})\n",
            indent, nonfinites, nonfinite_pixel.x, nonfinite_pixel.y,
            input.spec().channelnames[nonfinite_pixel_chan],
            nonfinite_pixel_samp);
    }
}



bool
print_stats(std::ostream& out, string_view indent, const ImageBuf& input,
            const ImageSpec& spec, ROI roi, std::string& err)
{
    PixelStats stats = computePixelStats(input, roi);
    if (!stats.min.size()) {
        err = input.geterror();
        if (err.empty())
            err = std::string("unspecified error");
        return false;
    }

    // The original spec is used, otherwise the bit depth will
    // be reported incorrectly (as FLOAT)
    // const ImageSpec& originalspec(input.nativespec());
    unsigned int maxval = (unsigned int)get_intsample_maxval(spec);

    print_stats_summary(out, indent, stats, spec);

    if (input.deep()) {
        print_deep_stats(out, indent, input, spec);
    } else {
        std::vector<float> constantValues(input.spec().nchannels);
        if (isConstantColor(input, 0.0f, constantValues)) {
            print(out, "{}Constant: Yes\n", indent);
            print(out, "{}Constant Color: ", indent);
            for (unsigned int i = 0; i < constantValues.size(); ++i) {
                print(out, "{} ", stats_num(constantValues[i], maxval, false));
            }
            print(out, "{}\n", stats_footer(maxval));
        } else {
            print(out, "{}Constant: No\n", indent);
        }

        if (isMonochrome(input)) {
            print(out, "{}Monochrome: Yes\n", indent);
        } else {
            print(out, "{}Monochrome: No\n", indent);
        }
    }
    return true;
}


}  // namespace pvt
OIIO_NAMESPACE_END
