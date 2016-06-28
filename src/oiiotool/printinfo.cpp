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
#include <iterator>
#if defined(_MSC_VER)
#include <io.h>
#endif

#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <OpenEXR/half.h>
#include <OpenEXR/ImathVec.h>

#include "OpenImageIO/argparse.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/sysutil.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/deepdata.h"
#include "OpenImageIO/hash.h"
#include "OpenImageIO/fmath.h"
#include "OpenImageIO/array_view.h"
#include "oiiotool.h"

OIIO_NAMESPACE_USING;
using namespace OiioTool;
using namespace ImageBufAlgo;





static void
print_sha1 (ImageInput *input)
{
    SHA1 sha;
    const ImageSpec &spec (input->spec());
    if (spec.deep) {
        // Special handling of deep data
        DeepData dd;
        if (! input->read_native_deep_image (dd)) {
            printf ("    SHA-1: unable to compute, could not read image\n");
            return;
        }
        // Hash both the sample counts and the data block
        sha.append (dd.all_samples());
        sha.append (dd.all_data());
    } else {
        imagesize_t size = input->spec().image_bytes (true /*native*/);
        if (size >= std::numeric_limits<size_t>::max()) {
            printf ("    SHA-1: unable to compute, image is too big\n");
            return;
        }
        else if (size != 0) {
            boost::scoped_array<char> buf (new char [size]);
            if (! input->read_image (TypeDesc::UNKNOWN /*native*/, &buf[0])) {
                printf ("    SHA-1: unable to compute, could not read image\n");
                return;
            }
            sha.append (&buf[0], size);
        }
    }

    printf ("    SHA-1: %s\n", sha.digest().c_str());
}



static void
dump_data (ImageInput *input, const print_info_options &opt)
{
    const ImageSpec &spec (input->spec());
    if (spec.deep) {
        // Special handling of deep data
        DeepData dd;
        if (! input->read_native_deep_image (dd)) {
            printf ("    dump data: could not read image\n");
            return;
        }
        int nc = spec.nchannels;
        for (int z = 0, pixel = 0;  z < spec.depth;  ++z) {
            for (int y = 0;  y < spec.height;  ++y) {
                for (int x = 0;  x < spec.width;  ++x, ++pixel) {
                    int nsamples = dd.samples(pixel);
                    if (nsamples == 0 && ! opt.dumpdata_showempty)
                        continue;
                    std::cout << "    Pixel (";
                    if (spec.depth > 1 || spec.z != 0)
                        std::cout << Strutil::format("%d, %d, %d",
                                             x+spec.x, y+spec.y, z+spec.z);
                    else
                        std::cout << Strutil::format("%d, %d",
                                                     x+spec.x, y+spec.y);
                    std::cout << "): " << nsamples << " samples" 
                              << (nsamples ? ":" : "");
                    for (int s = 0;  s < nsamples;  ++s) {
                        if (s)
                            std::cout << " / ";
                        for (int c = 0;  c < nc;  ++c) {
                            std::cout << " " << spec.channelnames[c] << "=";
                            if (dd.channeltype(c) == TypeDesc::UINT)
                                std::cout << dd.deep_value_uint(pixel, c, s);
                            else
                                std::cout << dd.deep_value (pixel, c, s);
                        }
                    }
                    std::cout << "\n";
                }
            }
        }

    } else {
        std::vector<float> buf(spec.image_pixels() * spec.nchannels);
        if (! input->read_image (TypeDesc::FLOAT, &buf[0])) {
            printf ("    dump data: could not read image\n");
            return;
        }
        const float *ptr = &buf[0];
        for (int z = 0;  z < spec.depth;  ++z) {
            for (int y = 0;  y < spec.height;  ++y) {
                for (int x = 0;  x < spec.width;  ++x) {
                    if (! opt.dumpdata_showempty) {
                        bool allzero = true;
                        for (int c = 0; c < spec.nchannels && allzero; ++c)
                            allzero &= (ptr[c] == 0.0f);
                        if (allzero) {
                            ptr += spec.nchannels;
                            continue;
                        }
                    }
                    if (spec.depth > 1 || spec.z != 0)
                        std::cout << Strutil::format("    Pixel (%d, %d, %d):",
                                             x+spec.x, y+spec.y, z+spec.z);
                    else
                        std::cout << Strutil::format("    Pixel (%d, %d):",
                                             x+spec.x, y+spec.y);
                    for (int c = 0;  c < spec.nchannels;  ++c, ++ptr) {
                        std::cout << ' ' << (*ptr);
                    }
                    std::cout << "\n";
                }
            }
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
// Stats

static bool
read_input (const std::string &filename, ImageBuf &img,
            int subimage=0, int miplevel=0)
{
    if (img.subimage() >= 0 && img.subimage() == subimage)
        return true;

    if (img.init_spec (filename, subimage, miplevel)) {
        // Force a read now for reasonable-sized first images in the
        // file. This can greatly speed up the multithread case for
        // tiled images by not having multiple threads working on the
        // same image lock against each other on the file handle.
        // We guess that "reasonable size" is 200 MB, that's enough to
        // hold a 4k RGBA float image.  Larger things will 
        // simply fall back on ImageCache.
        bool forceread = (img.spec().image_bytes() < 200*1024*1024);
        return img.read (subimage, miplevel, forceread, TypeDesc::FLOAT);
    }

    return false;
}



static void
print_stats_num (float val, int maxval, bool round)
{
    // Ensure uniform printing of NaN and Inf on all platforms
    if (isnan(val))
        printf ("nan");
    else if (isinf(val))
        printf ("inf");
    else if (maxval == 0) {
        printf("%f",val);
    } else {
        float fval = val * static_cast<float>(maxval);
        if (round) {
            int v = static_cast<int>(roundf (fval));
            printf ("%d", v);
        } else {
            printf ("%0.2f", fval);
        }
    }
}


// First check oiio:BitsPerSample int attribute.  If not set,
// fall back on the TypeDesc. return 0 for float types
// or those that exceed the int range (long long, etc)
static unsigned long long
get_intsample_maxval (const ImageSpec &spec)
{
    TypeDesc type = spec.format;
    int bits = spec.get_int_attribute ("oiio:BitsPerSample");
    if (bits > 0) {
        if (type.basetype == TypeDesc::UINT8 ||
              type.basetype == TypeDesc::UINT16 ||
              type.basetype == TypeDesc::UINT32)
            return ((1LL) << bits) - 1;
        if (type.basetype == TypeDesc::INT8 ||
              type.basetype == TypeDesc::INT16 ||
              type.basetype == TypeDesc::INT32)
            return ((1LL) << (bits-1)) - 1;
    }
    
    // These correspond to all the int enums in typedesc.h <= int
    if (type.basetype == TypeDesc::UCHAR)        return 0xff;
    if (type.basetype == TypeDesc::CHAR)         return 0x7f;
    if (type.basetype == TypeDesc::USHORT)     return 0xffff;
    if (type.basetype == TypeDesc::SHORT)      return 0x7fff;
    if (type.basetype == TypeDesc::UINT)   return 0xffffffff;
    if (type.basetype == TypeDesc::INT)    return 0x7fffffff;
    
    return 0;
}


static void
print_stats_footer (unsigned int maxval)
{
    if (maxval==0)
        printf ("(float)");
    else
        printf ("(of %u)", maxval);
}


static void
print_stats (Oiiotool &ot,
             const std::string &filename,
             const ImageSpec &originalspec,
             int subimage=0, int miplevel=0, bool indentmip=false)
{
    const char *indent = indentmip ? "      " : "    ";
    ImageBuf input;
    
    if (! read_input (filename, input, subimage, miplevel)) {
        ot.error ("stats", input.geterror());
        return;
    }
    PixelStats stats;
    if (! computePixelStats (stats, input)) {
        std::string err = input.geterror();
        ot.error ("stats", Strutil::format ("unable to compute: %s",
                                            err.empty() ? "unspecified error" : err.c_str()));
        return;
    }
    
    // The original spec is used, otherwise the bit depth will
    // be reported incorrectly (as FLOAT)
    unsigned int maxval = (unsigned int)get_intsample_maxval (originalspec);
    
    printf ("%sStats Min: ", indent);
    for (unsigned int i=0; i<stats.min.size(); ++i) {
        print_stats_num (stats.min[i], maxval, true);
        printf (" ");
    }
    print_stats_footer (maxval);
    printf ("\n");
    
    printf ("%sStats Max: ", indent);
    for (unsigned int i=0; i<stats.max.size(); ++i) {
        print_stats_num (stats.max[i], maxval, true);
        printf (" ");
    }
    print_stats_footer (maxval);
    printf ("\n");
    
    printf ("%sStats Avg: ", indent);
    for (unsigned int i=0; i<stats.avg.size(); ++i) {
        print_stats_num (stats.avg[i], maxval, false);
        printf (" ");
    }
    print_stats_footer (maxval);
    printf ("\n");
    
    printf ("%sStats StdDev: ", indent);
    for (unsigned int i=0; i<stats.stddev.size(); ++i) {
        print_stats_num (stats.stddev[i], maxval, false);
        printf (" ");
    }
    print_stats_footer (maxval);
    printf ("\n");
    
    printf ("%sStats NanCount: ", indent);
    for (unsigned int i=0; i<stats.nancount.size(); ++i) {
        printf ("%llu ", (unsigned long long)stats.nancount[i]);
    }
    printf ("\n");
    
    printf ("%sStats InfCount: ", indent);
    for (unsigned int i=0; i<stats.infcount.size(); ++i) {
        printf ("%llu ", (unsigned long long)stats.infcount[i]);
    }
    printf ("\n");
    
    printf ("%sStats FiniteCount: ", indent);
    for (unsigned int i=0; i<stats.finitecount.size(); ++i) {
        printf ("%llu ", (unsigned long long)stats.finitecount[i]);
    }
    printf ("\n");
    
    if (input.deep()) {
        const DeepData *dd (input.deepdata());
        size_t npixels = dd->pixels();
        size_t totalsamples = 0, emptypixels = 0;
        size_t maxsamples = 0, minsamples = std::numeric_limits<size_t>::max();
        size_t maxsamples_npixels = 0;
        float mindepth = std::numeric_limits<float>::max();
        float maxdepth = -std::numeric_limits<float>::max();
        Imath::V3i maxsamples_pixel(-1,-1,-1), minsamples_pixel(-1,-1,-1);
        Imath::V3i mindepth_pixel(-1,-1,-1), maxdepth_pixel(-1,-1,-1);
        Imath::V3i nonfinite_pixel(-1,-1,-1);
        int nonfinite_pixel_samp(-1), nonfinite_pixel_chan(-1);
        size_t sampoffset = 0;
        int nchannels = dd->channels();
        int depthchannel = -1;
        long long nonfinites = 0;
        for (int c = 0; c < nchannels; ++c)
            if (Strutil::iequals (originalspec.channelnames[c], "Z"))
                depthchannel = c;
        int xend = originalspec.x + originalspec.width;
        int yend = originalspec.y + originalspec.height;
        int zend = originalspec.z + originalspec.depth;
        size_t p = 0;
        std::vector<size_t> nsamples_histogram;
        for (int z = originalspec.z; z < zend; ++z) {
            for (int y = originalspec.y; y < yend; ++y) {
                for (int x = originalspec.x; x < xend; ++x, ++p) {
                    size_t samples = input.deep_samples (x, y, z);
                    totalsamples += samples;
                    if (samples == maxsamples)
                        ++maxsamples_npixels;
                    if (samples > maxsamples) {
                        maxsamples = samples;
                        maxsamples_pixel.setValue (x, y, z);
                        maxsamples_npixels = 1;
                    }
                    if (samples < minsamples)
                        minsamples = samples;
                    if (samples == 0)
                        ++emptypixels;
                    if (samples >= nsamples_histogram.size())
                        nsamples_histogram.resize (samples+1, 0);
                    nsamples_histogram[samples] += 1;
                    for (unsigned int s = 0;  s < samples;  ++s) {
                        for (int c = 0;  c < nchannels; ++c) {
                            float d = input.deep_value (x, y, z, c, s);
                            if (! isfinite(d)) {
                                if (nonfinites++ == 0) {
                                    nonfinite_pixel.setValue (x, y, z);
                                    nonfinite_pixel_samp = s;
                                    nonfinite_pixel_chan = c;
                                }
                            }
                            if (depthchannel == c) {
                                if (d < mindepth) {
                                    mindepth = d;
                                    mindepth_pixel.setValue (x, y, z);
                                }
                                if (d > maxdepth) {
                                    maxdepth = d;
                                    maxdepth_pixel.setValue (x, y, z);
                                }
                            }
                        }
                    }
                    sampoffset += samples;
                }
            }
        }
        printf ("%sMin deep samples in any pixel : %llu\n", indent, (unsigned long long)minsamples);
        printf ("%sMax deep samples in any pixel : %llu\n", indent, (unsigned long long)maxsamples);
        printf ("%s%llu pixel%s had the max of %llu samples, including (x=%d, y=%d)\n",
                indent, (unsigned long long)maxsamples_npixels,
                maxsamples_npixels > 1 ? "s" : "",
                (unsigned long long)maxsamples,
                maxsamples_pixel.x, maxsamples_pixel.y);
        printf ("%sAverage deep samples per pixel: %.2f\n", indent, double(totalsamples)/double(npixels));
        printf ("%sTotal deep samples in all pixels: %llu\n", indent, (unsigned long long)totalsamples);
        printf ("%sPixels with deep samples   : %llu\n", indent, (unsigned long long)(npixels-emptypixels));
        printf ("%sPixels with no deep samples: %llu\n", indent, (unsigned long long)emptypixels);
        printf ("%sSamples/pixel histogram:\n", indent);
        size_t grandtotal = 0;
        for (size_t i = 0, e = nsamples_histogram.size();  i < e;  ++i)
            grandtotal += nsamples_histogram[i];
        size_t binstart = 0, bintotal = 0;
        for (size_t i = 0, e = nsamples_histogram.size();  i < e;  ++i) {
            bintotal += nsamples_histogram[i];
            if (i < 8 || i == (e-1) || OIIO::ispow2(i+1)) {
                // batch by powers of 2, unless it's a small number
                if (i == binstart)
                    printf ("%s  %3lld    ", indent, (long long)i);
                else
                    printf ("%s  %3lld-%3lld", indent,
                            (long long)binstart, (long long)i);
                printf (" : %8lld (%4.1f%%)\n", (long long)bintotal,
                        (100.0*bintotal)/grandtotal);
                binstart = i+1;
                bintotal = 0;
            }
        }
        if (depthchannel >= 0) {
            printf ("%sMinimum depth was %g at (%d, %d)\n", indent, mindepth,
                    mindepth_pixel.x, mindepth_pixel.y);
            printf ("%sMaximum depth was %g at (%d, %d)\n", indent, maxdepth,
                    maxdepth_pixel.x, maxdepth_pixel.y);
        }
        if (nonfinites > 0) {
            printf ("%sNonfinite values: %lld, including (x=%d, y=%d, chan=%s, samp=%d)\n",
                    indent, nonfinites, nonfinite_pixel.x, nonfinite_pixel.y,
                    input.spec().channelnames[nonfinite_pixel_chan].c_str(),
                    nonfinite_pixel_samp);
        }
    } else {
        std::vector<float> constantValues(input.spec().nchannels);
        if (isConstantColor(input, &constantValues[0])) {
            printf ("%sConstant: Yes\n", indent);
            printf ("%sConstant Color: ", indent);
            for (unsigned int i=0; i<constantValues.size(); ++i) {
                print_stats_num (constantValues[i], maxval, false);
                printf (" ");
            }
            print_stats_footer (maxval);
            printf ("\n");
        }
        else {
            printf ("%sConstant: No\n", indent);
        }
    
        if( isMonochrome(input)) {
            printf ("%sMonochrome: Yes\n", indent);
        } else {
            printf ("%sMonochrome: No\n", indent);
        }
    }
}



static void
print_metadata (const ImageSpec &spec, const std::string &filename,
                const print_info_options &opt,
                boost::regex &field_re, boost::regex &field_exclude_re)
{
    bool printed = false;
    if (opt.metamatch.empty() ||
          boost::regex_search ("channels", field_re) ||
          boost::regex_search ("channel list", field_re)) {
        if (opt.filenameprefix)
            printf ("%s : ", filename.c_str());
        printf ("    channel list: ");
        for (int i = 0;  i < spec.nchannels;  ++i) {
            if (i < (int)spec.channelnames.size())
                printf ("%s", spec.channelnames[i].c_str());
            else
                printf ("unknown");
            if (i < (int)spec.channelformats.size())
                printf (" (%s)", spec.channelformats[i].c_str());
            if (i < spec.nchannels-1)
                printf (", ");
        }
        printf ("\n");
        printed = true;
    }
    if (spec.x || spec.y || spec.z) {
        if (opt.metamatch.empty() ||
            boost::regex_search ("pixel data origin", field_re)) {
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    pixel data origin: x=%d, y=%d", spec.x, spec.y);
            if (spec.depth > 1)
                printf (", z=%d", spec.z);
            printf ("\n");
            printed = true;
        }
    }
    if (spec.full_x || spec.full_y || spec.full_z ||
          (spec.full_width != spec.width && spec.full_width != 0) || 
          (spec.full_height != spec.height && spec.full_height != 0) ||
          (spec.full_depth != spec.depth && spec.full_depth != 0)) {
        if (opt.metamatch.empty() ||
              boost::regex_search ("full/display size", field_re)) {
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    full/display size: %d x %d",
                    spec.full_width, spec.full_height);
            if (spec.depth > 1)
                printf (" x %d", spec.full_depth);
            printf ("\n");
            printed = true;
        }
        if (opt.metamatch.empty() ||
            boost::regex_search ("full/display origin", field_re)) {
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    full/display origin: %d, %d",
                    spec.full_x, spec.full_y);
            if (spec.depth > 1)
                printf (", %d", spec.full_z);
            printf ("\n");
            printed = true;
        }
    }
    if (spec.tile_width) {
        if (opt.metamatch.empty() ||
            boost::regex_search ("tile", field_re)) {
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    tile size: %d x %d",
                    spec.tile_width, spec.tile_height);
            if (spec.depth > 1)
                printf (" x %d", spec.tile_depth);
            printf ("\n");
            printed = true;
        }
    }
    
    BOOST_FOREACH (const ImageIOParameter &p, spec.extra_attribs) {
        if (! opt.metamatch.empty() &&
            ! boost::regex_search (p.name().c_str(), field_re))
            continue;
        if (! opt.nometamatch.empty() &&
            boost::regex_search (p.name().c_str(), field_exclude_re))
            continue;
        std::string s = spec.metadata_val (p, true);
        if (opt.filenameprefix)
            printf ("%s : ", filename.c_str());
        printf ("    %s: ", p.name().c_str());
        if (! strcmp (s.c_str(), "1.#INF"))
            printf ("inf");
        else
            printf ("%s", s.c_str());
        printf ("\n");
        printed = true;
    }

    if (! printed && !opt.metamatch.empty()) {
        if (opt.filenameprefix)
            printf ("%s : ", filename.c_str());
        printf ("    %s: <unknown>\n", opt.metamatch.c_str());
    }
}



static const char *
extended_format_name (TypeDesc type, int bits)
{
    if (bits && bits < (int)type.size()*8) {
        // The "oiio:BitsPerSample" betrays a different bit depth in the
        // file than the data type we are passing.
        if (type == TypeDesc::UINT8 || type == TypeDesc::UINT16 ||
            type == TypeDesc::UINT32 || type == TypeDesc::UINT64)
            return ustring::format("uint%d", bits).c_str();
        if (type == TypeDesc::INT8 || type == TypeDesc::INT16 ||
            type == TypeDesc::INT32 || type == TypeDesc::INT64)
            return ustring::format("int%d", bits).c_str();
    }
    return type.c_str();  // use the name implied by type
}



static const char *
brief_format_name (TypeDesc type, int bits=0)
{
    if (! bits)
        bits = (int)type.size()*8;
    if (type.is_floating_point()) {
        if (type.basetype == TypeDesc::FLOAT)
            return "f";
        if (type.basetype == TypeDesc::HALF)
            return "h";
        return ustring::format("f%d", bits).c_str();
    } else if (type.is_signed()) {
        return ustring::format("i%d", bits).c_str();
    } else {
        return ustring::format("u%d", bits).c_str();
    }
    return type.c_str();  // use the name implied by type
}



// prints basic info (resolution, width, height, depth, channels, data format,
// and format name) about given subimage.
static void
print_info_subimage (Oiiotool &ot,
                     int current_subimage, int max_subimages, ImageSpec &spec,
                     ImageInput *input, const std::string &filename,
                     const print_info_options &opt,
                     boost::regex &field_re, boost::regex &field_exclude_re)
{
    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;

    int nmip = 1;

    bool printres = opt.verbose && (opt.metamatch.empty() ||
                                boost::regex_search ("resolution, width, height, depth, channels", field_re));
    if (printres && max_subimages > 1 && opt.subimages) {
        printf (" subimage %2d: ", current_subimage);
        printf ("%4d x %4d", spec.width, spec.height);
        if (spec.depth > 1)
            printf (" x %4d", spec.depth);
        printf (", %d channel, %s%s", spec.nchannels,
                spec.deep ? "deep " : "",
                spec.depth > 1 ? "volume " : "");
        if (spec.channelformats.size()) {
            for (size_t c = 0;  c < spec.channelformats.size();  ++c)
                printf ("%s%s", c ? "/" : "",
                        spec.channelformats[c].c_str());
        } else {
            int bits = spec.get_int_attribute ("oiio:BitsPerSample", 0);
            printf ("%s", extended_format_name(spec.format, bits));
        }
        printf (" %s", input->format_name());
        printf ("\n");
    }
    // Count MIP levels
    ImageSpec mipspec;
    while (input->seek_subimage (current_subimage, nmip, mipspec)) {
        if (printres) {
            if (nmip == 1)
                printf ("    MIP-map levels: %dx%d", spec.width, spec.height);
            printf (" %dx%d", mipspec.width, mipspec.height);
        }
        ++nmip;
    }
    if (printres && nmip > 1)
        printf ("\n");

    if (opt.compute_sha1 && (opt.metamatch.empty() ||
                         boost::regex_search ("sha-1", field_re))) {
        if (opt.filenameprefix)
            printf ("%s : ", filename.c_str());
        // Before sha-1, be sure to point back to the highest-res MIP level
        ImageSpec tmpspec;
        input->seek_subimage (current_subimage, 0, tmpspec);
        print_sha1 (input);
    }

    if (opt.verbose)
        print_metadata (spec, filename, opt, field_re, field_exclude_re);

    if (opt.dumpdata) {
        ImageSpec tmp;
        input->seek_subimage (current_subimage, 0, tmp);
        dump_data (input, opt);
    }

    if (opt.compute_stats && (opt.metamatch.empty() ||
                          boost::regex_search ("stats", field_re))) {
        for (int m = 0;  m < nmip;  ++m) {
            ImageSpec mipspec;
            input->seek_subimage (current_subimage, m, mipspec);
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            if (nmip > 1) {
                printf ("    MIP %d of %d (%d x %d):\n",
                        m, nmip, mipspec.width, mipspec.height);
            }
            print_stats (ot, filename, spec, current_subimage, m, nmip>1);
        }
    }

    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;
}



bool
OiioTool::print_info (Oiiotool &ot,
                      const std::string &filename, 
                      const print_info_options &opt,
                      long long &totalsize,
                      std::string &error)
{
    error.clear();
    ImageInput *input = ImageInput::open (filename.c_str());
    if (! input) {
        error = geterror();
        if (error.empty())
            error = Strutil::format ("Could not open \"%s\"", filename.c_str());
        return false;
    }
    ImageSpec spec = input->spec();

    boost::regex field_re;
    boost::regex field_exclude_re;
    if (! opt.metamatch.empty()) {
        try {
            field_re.assign (opt.metamatch,
                         boost::regex::extended | boost::regex_constants::icase);
        } catch (const std::exception &e) {
            error = Strutil::format ("Regex error '%s' on metamatch regex \"%s\"",
                                     e.what(), opt.metamatch);
            return false;
        }
    }
    if (! opt.nometamatch.empty()) {
        try {
            field_exclude_re.assign (opt.nometamatch,
                         boost::regex::extended | boost::regex_constants::icase);
        } catch (const std::exception &e) {
            error = Strutil::format ("Regex error '%s' on metamatch regex \"%s\"",
                                     e.what(), opt.nometamatch);
            return false;
        }
    }

    int padlen = std::max (0, (int)opt.namefieldlength - (int)filename.length());
    std::string padding (padlen, ' ');

    // checking how many subimages and mipmap levels are stored in the file
    int num_of_subimages = 1;
    bool any_mipmapping = false;
    std::vector<int> num_of_miplevels;
    {
        int nmip = 1;
        while (input->seek_subimage (input->current_subimage(), nmip, spec)) {
            ++nmip;
            any_mipmapping = true;
        }
        num_of_miplevels.push_back (nmip);
    }
    while (input->seek_subimage (num_of_subimages, 0, spec)) {
        // maybe we should do this more gently?
        ++num_of_subimages;
        int nmip = 1;
        while (input->seek_subimage (input->current_subimage(), nmip, spec)) {
            ++nmip;
            any_mipmapping = true;
        }
        num_of_miplevels.push_back (nmip);
    }
    input->seek_subimage (0, 0, spec);  // re-seek to the first

    if (opt.metamatch.empty() ||
        boost::regex_search ("resolution, width, height, depth, channels", field_re)) {
        printf ("%s%s : %4d x %4d", filename.c_str(), padding.c_str(),
                spec.width, spec.height);
        if (spec.depth > 1)
            printf (" x %4d", spec.depth);
        printf (", %d channel, %s%s", spec.nchannels,
                spec.deep ? "deep " : "",
                spec.depth > 1 ? "volume " : "");
        if (spec.channelformats.size()) {
            for (size_t c = 0;  c < spec.channelformats.size();  ++c)
                printf ("%s%s", c ? "/" : "",
                        spec.channelformats[c].c_str());
        } else {
            int bits = spec.get_int_attribute ("oiio:BitsPerSample", 0);
            printf ("%s", extended_format_name(spec.format, bits));
        }
        printf (" %s", input->format_name());
        if (opt.sum) {
            imagesize_t imagebytes = spec.image_bytes (true);
            totalsize += imagebytes;
            printf (" (%.2f MB)", (float)imagebytes / (1024.0*1024.0));
        }
        // we print info about how many subimages are stored in file
        // only when we have more then one subimage
        if ( ! opt.verbose && num_of_subimages != 1)
            printf (" (%d subimages%s)", num_of_subimages,
                    any_mipmapping ? " +mipmap)" : "");
        if (! opt.verbose && num_of_subimages == 1 && any_mipmapping)
            printf (" (+mipmap)");
        printf ("\n");
    }

    int movie = spec.get_int_attribute ("oiio:Movie");
    if (opt.verbose && num_of_subimages != 1) {
        // info about num of subimages and their resolutions
        printf ("    %d subimages: ", num_of_subimages);
        for (int i = 0; i < num_of_subimages; ++i) {
            input->seek_subimage (i, 0, spec);
            int bits = spec.get_int_attribute ("oiio:BitsPerSample",
                                               spec.format.size()*8);
            if (i)
                printf (", ");
            if (spec.depth > 1)
                printf ("%dx%dx%d ", spec.width, spec.height, spec.depth);
            else
                printf ("%dx%d ", spec.width, spec.height);
            // printf ("[");
            for (int c = 0; c < spec.nchannels; ++c)
                printf ("%c%s", c ? ',' : '[',
                        brief_format_name(spec.channelformat(c), bits));
            printf ("]");
            if (movie)
                break;
        }
        printf ("\n");
    }

    // if the '-a' flag is not set we print info
    // about first subimage only
    if ( ! opt.subimages)
        num_of_subimages = 1;
    for (int i = 0; i < num_of_subimages; ++i) {
        print_info_subimage (ot, i, num_of_subimages, spec, input,
                             filename, opt, field_re, field_exclude_re);
    }

    input->close ();
    delete input;
    return true;
}
