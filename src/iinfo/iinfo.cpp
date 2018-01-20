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

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/filesystem.h>

#ifdef USE_BOOST_REGEX
# include <boost/regex.hpp>
  using boost::regex;
  using boost::regex_search;
#else
# include <regex>
  using std::regex;
  using std::regex_search;
#endif

using namespace OIIO;

using namespace ImageBufAlgo;


static bool verbose = false;
static bool sum = false;
static bool help = false;
static std::vector<std::string> filenames;
static std::string metamatch;
static bool filenameprefix = false;
static regex field_re;
static bool subimages = false;
static bool compute_sha1 = false;
static bool compute_stats = false;



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
        // Hash both the sample counds and the data block
        sha.append (dd.all_samples());
        sha.append (dd.all_data());
    } else {
        imagesize_t size = input->spec().image_bytes (true /*native*/);
        if (size >= std::numeric_limits<size_t>::max()) {
            printf ("    SHA-1: unable to compute, image is too big\n");
            return;
        }
        std::unique_ptr<char[]> buf (new char [size]);
        if (! input->read_image (TypeDesc::UNKNOWN /*native*/, &buf[0])) {
            printf ("    SHA-1: unable to compute, could not read image\n");
            return;
        }
        sha.append (&buf[0], size);
    }

    printf ("    SHA-1: %s\n", sha.digest().c_str());
}



///////////////////////////////////////////////////////////////////////////////
// Stats

static bool
read_input (const std::string &filename, ImageBuf &img,
            int subimage=0, int miplevel=0)
{
    if (img.subimage() >= 0 && img.subimage() == subimage)
        return true;

    if (img.init_spec (filename, subimage, miplevel) && 
        img.read (subimage, miplevel, false, TypeDesc::FLOAT))
        return true;

    std::cerr << "iinfo ERROR: Could not read " << filename << ":\n\t"
              << img.geterror() << "\n";
    return false;
}



static void
print_stats_num (float val, int maxval, bool round)
{
    if (maxval == 0) {
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
print_stats (const std::string &filename,
             const ImageSpec &originalspec,
             int subimage=0, int miplevel=0, bool indentmip=false)
{
    PixelStats stats;
    const char *indent = indentmip ? "      " : "    ";

    ImageBuf input;
    if (! read_input (filename, input, subimage, miplevel)) {
        std::cerr << "Stats: read error: " << input.geterror() << "\n";
        return;
    }
    
    if (! computePixelStats (stats, input)) {
        printf ("%sStats: (unable to compute)\n", indent);
        if (input.has_error())
            std::cerr << "Error: " << input.geterror() << "\n";
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
        for (size_t p = 0;  p < npixels;  ++p) {
            size_t c = dd->samples(p);
            totalsamples += c;
            if (c > maxsamples)
                maxsamples = c;
            if (c < minsamples)
                minsamples = c;
            if (c == 0)
                ++emptypixels;
        }
        printf ("%sMin deep samples in any pixel : %llu\n", indent, (unsigned long long)minsamples);
        printf ("%sMax deep samples in any pixel : %llu\n", indent, (unsigned long long)maxsamples);
        printf ("%sAverage deep samples per pixel: %.2f\n", indent, double(totalsamples)/double(npixels));
        printf ("%sTotal deep samples in all pixels: %llu\n", indent, (unsigned long long)totalsamples);
        printf ("%sPixels with deep samples   : %llu\n", indent, (unsigned long long)(npixels-emptypixels));
        printf ("%sPixels with no deep samples: %llu\n", indent, (unsigned long long)emptypixels);
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
    
        if (isMonochrome(input)) {
            printf ("%sMonochrome: Yes\n", indent);
        } else {
            printf ("%sMonochrome: No\n", indent);
        }
    }
}



static void
print_metadata (const ImageSpec &spec, const std::string &filename)
{
    bool printed = false;
    if (metamatch.empty() ||
          regex_search ("channels", field_re) ||
          regex_search ("channel list", field_re)) {
        if (filenameprefix)
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
        if (metamatch.empty() ||
            regex_search ("pixel data origin", field_re)) {
            if (filenameprefix)
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
        if (metamatch.empty() ||
              regex_search ("full/display size", field_re)) {
            if (filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    full/display size: %d x %d",
                    spec.full_width, spec.full_height);
            if (spec.depth > 1)
                printf (" x %d", spec.full_depth);
            printf ("\n");
            printed = true;
        }
        if (metamatch.empty() ||
            regex_search ("full/display origin", field_re)) {
            if (filenameprefix)
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
        if (metamatch.empty() ||
            regex_search ("tile", field_re)) {
            if (filenameprefix)
                printf ("%s : ", filename.c_str());
            printf ("    tile size: %d x %d",
                    spec.tile_width, spec.tile_height);
            if (spec.depth > 1)
                printf (" x %d", spec.tile_depth);
            printf ("\n");
            printed = true;
        }
    }

    for (auto&& p : spec.extra_attribs) {
        if (! metamatch.empty() &&
            ! regex_search (p.name().c_str(), field_re))
            continue;
        std::string s = spec.metadata_val (p, true);
        if (filenameprefix)
            printf ("%s : ", filename.c_str());
        printf ("    %s: ", p.name().c_str());
        if (! strcmp (s.c_str(), "1.#INF"))
            printf ("inf");
        else
            printf ("%s", s.c_str());
        printf ("\n");
        printed = true;
    }

    if (! printed && !metamatch.empty()) {
        if (filenameprefix)
            printf ("%s : ", filename.c_str());
        printf ("    %s: <unknown>\n", metamatch.c_str());
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
print_info_subimage (int current_subimage, int max_subimages, ImageSpec &spec,
                     ImageInput *input, const std::string &filename)
{
    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;

    if (! metamatch.empty() &&
        ! regex_search ("resolution, width, height, depth, channels, sha-1, stats", field_re)) {
        // nothing to do here
        return;
    }

    int nmip = 1;

    bool printres = verbose && (metamatch.empty() ||
                                regex_search ("resolution, width, height, depth, channels", field_re));
    if (printres && max_subimages > 1 && subimages) {
        printf (" subimage %2d: ", current_subimage);
        printf ("%4d x %4d", spec.width, spec.height);
        if (spec.depth > 1)
            printf (" x %4d", spec.depth);
        int bits = spec.get_int_attribute ("oiio:BitsPerSample", 0);
        printf (", %d channel, %s%s%s", spec.nchannels,
                spec.deep ? "deep " : "",
                spec.depth > 1 ? "volume " : "",
                extended_format_name(spec.format, bits));
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

    if (compute_sha1 && (metamatch.empty() ||
                         regex_search ("sha-1", field_re))) {
        if (filenameprefix)
            printf ("%s : ", filename.c_str());
        // Before sha-1, be sure to point back to the highest-res MIP level
        ImageSpec tmpspec;
        input->seek_subimage (current_subimage, 0, tmpspec);
        print_sha1 (input);
    }

    if (verbose)
        print_metadata (spec, filename);

    if (compute_stats && (metamatch.empty() ||
                          regex_search ("stats", field_re))) {
        for (int m = 0;  m < nmip;  ++m) {
            ImageSpec mipspec;
            input->seek_subimage (current_subimage, m, mipspec);
            if (filenameprefix)
                printf ("%s : ", filename.c_str());
            if (nmip > 1 && (subimages || m == 0)) {
                printf ("    MIP %d of %d (%d x %d):\n",
                        m, nmip, mipspec.width, mipspec.height);
            }
            print_stats (filename, spec, current_subimage, m, nmip>1);
        }
    }

    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;
}



static void
print_info (const std::string &filename, size_t namefieldlength, 
            ImageInput *input, ImageSpec &spec,
            bool verbose, bool sum, long long &totalsize)
{
    int padlen = std::max (0, (int)namefieldlength - (int)filename.length());
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

    if (metamatch.empty() ||
        regex_search ("resolution, width, height, depth, channels", field_re)) {
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
        if (sum) {
            imagesize_t imagebytes = spec.image_bytes (true);
            totalsize += imagebytes;
            printf (" (%.2f MB)", (float)imagebytes / (1024.0*1024.0));
        }
        // we print info about how many subimages are stored in file
        // only when we have more then one subimage
        if ( ! verbose && num_of_subimages != 1)
            printf (" (%d subimages%s)", num_of_subimages,
                    any_mipmapping ? " +mipmap)" : "");
        if (! verbose && num_of_subimages == 1 && any_mipmapping)
            printf (" (+mipmap)");
        printf ("\n");
    }

    int movie = spec.get_int_attribute ("oiio:Movie");
    if (verbose && num_of_subimages != 1) {
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
    if ( ! subimages)
        num_of_subimages = 1;
    for (int i = 0; i < num_of_subimages; ++i) {
        print_info_subimage (i, num_of_subimages, spec, input, filename);
    }
}



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



int
main (int argc, const char *argv[])
{
    Filesystem::convert_native_arguments (argc, (const char **)argv);
    ArgParse ap;
    ap.options ("iinfo -- print information about images\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  iinfo [options] filename...",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose output",
                "-m %s", &metamatch, "Metadata names to print (default: all)",
                "-f", &filenameprefix, "Prefix each line with the filename",
                "-s", &sum, "Sum the image sizes",
                "-a", &subimages, "Print info about all subimages",
                "--hash", &compute_sha1, "Print SHA-1 hash of pixel values",
                "--stats", &compute_stats, "Print image pixel statistics (data window)",
                NULL);
    if (ap.parse(argc, argv) < 0 || filenames.empty()) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (! metamatch.empty()) {
#if USE_BOOST_REGEX
        field_re.assign (metamatch,
                     boost::regex::extended | boost::regex_constants::icase);
#else
        field_re.assign (metamatch,
                     std::regex_constants::extended | std::regex_constants::icase);
#endif
    }

    // Find the longest filename
    size_t longestname = 0;
    for (auto&& s : filenames)
        longestname = std::max (longestname, s.length());
    longestname = std::min (longestname, (size_t)40);

    long long totalsize = 0;
    for (auto&& s : filenames) {
        ImageInput *in = ImageInput::open (s.c_str());
        if (! in) {
            std::string err = geterror();
            if (err.empty())
                err = Strutil::format ("Could not open \"%s\"", s);
            std::cerr << "iinfo ERROR: " << err << "\n";
            continue;
        }
        ImageSpec spec = in->spec();
        print_info (s, longestname, in, spec, verbose, sum, totalsize);
        in->close ();
        delete in;
    }

    if (sum) {
        double t = (double)totalsize / (1024.0*1024.0);
        if (t > 1024.0)
            printf ("Total size: %.2f GB\n", t/1024.0);
        else
            printf ("Total size: %.2f MB\n", t);
    }

    return 0;
}
