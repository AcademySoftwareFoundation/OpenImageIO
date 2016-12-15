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
#include <memory>
#if defined(_MSC_VER)
#include <io.h>
#endif

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





static std::string
compute_sha1 (Oiiotool &ot, ImageInput *input)
{
    SHA1 sha;
    const ImageSpec &spec (input->spec());
    if (spec.deep) {
        // Special handling of deep data
        DeepData dd;
        if (! input->read_native_deep_image (dd)) {
            ot.error ("    SHA-1: unable to compute, could not read image\n");
            return std::string();
        }
        // Hash both the sample counts and the data block
        sha.append (dd.all_samples());
        sha.append (dd.all_data());
    } else {
        imagesize_t size = input->spec().image_bytes (true /*native*/);
        if (size >= std::numeric_limits<size_t>::max()) {
            ot.error ("    SHA-1: unable to compute, image is too big\n");
            return std::string();
        }
        else if (size != 0) {
            std::unique_ptr<char[]> buf (new char [size]);
            if (! input->read_image (TypeDesc::UNKNOWN /*native*/, &buf[0])) {
                ot.error ("    SHA-1: unable to compute, could not read image\n");
                return std::string();
            }
            sha.append (&buf[0], size);
        }
    }

    return sha.digest().c_str();
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



static void
print_info_subimage (Oiiotool &ot,
                     int current_subimage, int num_of_subimages, int nmip,
                     ImageSpec &spec,
                     ImageInput *input, const std::string &filename,
                     const print_info_options &opt,
                     boost::regex &field_re, boost::regex &field_exclude_re,
                     ImageSpec::SerialFormat serformat,
                     ImageSpec::SerialVerbose verbose)
{
    using Strutil::format;

    int padlen = std::max (0, (int)opt.namefieldlength - (int)filename.length());
    std::string padding (padlen, ' ');
    bool printres = opt.verbose && (opt.metamatch.empty() ||
                                boost::regex_search ("resolution, width, height, depth, channels", field_re));

    // int nmip = num_of_miplevels[current_subimage];

    std::string ser = spec.serialize (serformat, verbose);

    std::vector<std::string> lines;
    Strutil::split (spec.serialize(serformat, verbose), lines, "\n");

    if (opt.compute_sha1 && (opt.metamatch.empty() ||
                             boost::regex_search ("sha-1", field_re))) {
        // Before sha-1, be sure to point back to the highest-res MIP level
        ImageSpec tmpspec;
        std::string sha = compute_sha1 (ot, input);
        if (serformat == ImageSpec::SerialText)
            lines.insert (lines.begin()+1, format("    SHA-1: %s", sha));
        else if (serformat == ImageSpec::SerialText)
            lines.insert (lines.begin()+1, format("<SHA1>%s</SHA1>", sha));
    }

    // Count MIP levels
    if (printres && nmip > 1) {
        ImageSpec mipspec;
        std::string mipdesc = format ("    MIP-map levels: %dx%d", spec.width, spec.height);
        for (int m = 1; input->seek_subimage (current_subimage, m, mipspec); ++m)
            mipdesc += format (" %dx%d", mipspec.width, mipspec.height);
        lines.insert (lines.begin()+1, mipdesc);
    }

    if (serformat == ImageSpec::SerialText) {
        // Requested a subset of metadata but not res, etc.? Kill first line.
        if (opt.metamatch.empty() ||
            boost::regex_search ("resolution, width, height, depth, channels", field_re)) {
            std::string orig_line0 = lines[0];
            if (current_subimage == 0)
                lines[0] = Strutil::format ("%s%s : ", filename, padding) + lines[0];
            else
                lines[0] = Strutil::format (" subimage %2d: ", current_subimage) + lines[0];
            if (opt.sum) {
                imagesize_t imagebytes = spec.image_bytes (true);
                // totalsize += imagebytes;
                lines[0] += format (" (%.2f MB)", (float)imagebytes / (1024.0*1024.0));
            }
            lines[0] += format (" %s", input->format_name());
            // we print info about how many subimages are stored in file
            // only when we have more then one subimage
            if ( ! opt.verbose && num_of_subimages != 1)
                lines[0] += format (" (%d subimages%s)", num_of_subimages,
                                    (nmip > 1) ? " +mipmap)" : "");
            if (! opt.verbose && num_of_subimages == 1 && (nmip > 1))
                lines[0] += " (+mipmap)";
            if (num_of_subimages > 1 && current_subimage == 0 && opt.subimages)
                lines.insert (lines.begin()+1, format (" subimage  0: %s %s",
                                                       orig_line0, input->format_name()));
        } else {
            lines.erase (lines.begin());
        }
    } else if (serformat == ImageSpec::SerialXML) {
        if (nmip > 1)
            lines.insert (lines.begin()+1,
                          format ("<miplevels>%d</miplevels>", nmip));
        if (num_of_subimages > 1)
            lines.insert (lines.begin()+1,
                          format ("<subimages>%d</subimages>", num_of_subimages));
    }

    if (current_subimage == 0 && opt.verbose && num_of_subimages != 1
        && serformat == ImageSpec::SerialText) {
        // info about num of subimages and their resolutions
        int movie = spec.get_int_attribute ("oiio:Movie");
        std::string s = format ("    %d subimages: ", num_of_subimages);
        for (int i = 0; i < num_of_subimages; ++i) {
            input->seek_subimage (i, 0, spec);
            int bits = spec.get_int_attribute ("oiio:BitsPerSample",
                                               spec.format.size()*8);
            if (i)
                s += ", ";
            if (spec.depth > 1)
                s += format ("%dx%dx%d ", spec.width, spec.height, spec.depth);
            else
                s += format ("%dx%d ", spec.width, spec.height);
            for (int c = 0; c < spec.nchannels; ++c)
                s += format ("%c%s", c ? ',' : '[',
                        brief_format_name(spec.channelformat(c), bits));
            s += "]";
            if (movie)
                break;
        }
        lines.insert (lines.begin()+1, s);
    }

    if (! opt.metamatch.empty() || ! opt.nometamatch.empty()) {
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string s = lines[i].substr (0, lines[i].find(": "));
            if ((! opt.nometamatch.empty() && boost::regex_search (s, field_exclude_re)) ||
                (! opt.metamatch.empty() && ! boost::regex_search (s, field_re))) {
                lines.erase (lines.begin()+i);
                --i;
            }
        }
    }

    // Unescape the strings if we're printing for human consumption
    for (size_t i = 0; i < lines.size(); ++i) {
        lines[i] = Strutil::unescape_chars (lines[i]);
    }

    ser = Strutil::join (lines, "\n");
    if (ser[ser.size()-1] != '\n')
        ser += '\n';
    std::cout << ser;

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
                std::cout << format ("%s : ", filename);
            if (nmip > 1) {
                std::cout << format ("    MIP %d of %d (%d x %d):\n",
                                     m, nmip, mipspec.width, mipspec.height);
            }
            print_stats (ot, filename, spec, current_subimage, m, nmip>1);
        }
    }
}



bool
OiioTool::print_info (Oiiotool &ot,
                      const std::string &filename,
                      const print_info_options &opt,
                      long long &totalsize,
                      std::string &error)
{
    using Strutil::format;
    error.clear();
    ImageSpec *config = ot.input_config_set ? &ot.input_config : NULL;
    ImageInput *input = ImageInput::open (filename.c_str(), config);
    if (! input) {
        error = geterror();
        if (error.empty())
            error = Strutil::format ("Could not open \"%s\"", filename);
        return false;
    }
    ImageSpec spec = input->spec();

    ImageSpec::SerialFormat serformat = ImageSpec::SerialText;
    if (Strutil::iequals (opt.infoformat, "xml"))
        serformat = ImageSpec::SerialXML;
    ImageSpec::SerialVerbose verbose = opt.verbose ? ImageSpec::SerialDetailedHuman : ImageSpec::SerialBrief;

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

    // checking how many subimages and mipmap levels are stored in the file
    std::vector<int> num_of_miplevels;
    int num_of_subimages;
    for (num_of_subimages = 0; input->seek_subimage (num_of_subimages, 0, spec); ++num_of_subimages) {
        int nmip = 1;
        while (input->seek_subimage (num_of_subimages, nmip, spec))
            ++nmip;
        num_of_miplevels.push_back (nmip);
    }

    for (int current_subimage = 0; current_subimage < num_of_subimages; ++current_subimage) {
        if ( ! input->seek_subimage (current_subimage, 0, spec) )
            break;
        print_info_subimage (ot, current_subimage, num_of_subimages,
                             num_of_miplevels[current_subimage],
                             spec, input, filename, opt,
                             field_re, field_exclude_re, serformat, verbose);
        // if the '-a' flag is not set we print info
        // about first subimage only
        if ( ! opt.subimages)
            break;
    }

    input->close ();
    delete input;
    return true;
}
