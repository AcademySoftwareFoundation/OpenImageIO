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

/* This header have to be included before boost/regex.hpp header
   If it is included after, there is an error
   "undefined reference to CSHA1::Update (unsigned char const*, unsigned long)"
*/
#include "SHA1.h"

#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "argparse.h"
#include "strutil.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "oiiotool.h"

OIIO_NAMESPACE_USING;
using namespace OiioTool;
using namespace ImageBufAlgo;





static void
print_sha1 (ImageInput *input)
{
    imagesize_t size = input->spec().image_bytes ();
    if (size >= std::numeric_limits<size_t>::max()) {
        printf ("    SHA1 digest: (unable to compute, image is too big)\n");
        return;
    }
    boost::scoped_array<unsigned char> buf(new unsigned char[(unsigned int)size]);
    input->read_image (input->spec().format, &buf[0]);
    CSHA1 sha;
    sha.Update ((const unsigned char *)&buf[0], (unsigned int) size);
    sha.Final ();
    std::string digest;
    sha.ReportHashStl (digest, CSHA1::REPORT_HEX_SHORT);
    printf ("    SHA-1: %s\n", digest.c_str());
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
    const char *indent = indentmip ? "      " : "    ";
    ImageBuf input;
    
    if (! read_input (filename, input, subimage, miplevel)) {
        std::cerr << "Stats: read error: " << input.geterror() << "\n";
        return;
    }
    
    PixelStats stats;
    if (! computePixelStats (stats, input)) {
        printf ("%sStats: (unable to compute)\n", indent);
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
    
    std::vector<float> constantValues(input.spec().nchannels);
    if(isConstantColor(input, &constantValues[0])) {
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
    
    if(isMonochrome(input)) {
        printf ("%sMonochrome: Yes\n", indent);
    } else {
        printf ("%sMonochrome: No\n", indent);
    }
}



static void
print_metadata (const ImageSpec &spec, const std::string &filename,
                const print_info_options &opt,
                boost::regex &field_re)
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



// prints basic info (resolution, width, height, depth, channels, data format,
// and format name) about given subimage.
static void
print_info_subimage (int current_subimage, int max_subimages, ImageSpec &spec,
                     ImageInput *input, const std::string &filename,
                     const print_info_options &opt,
                     boost::regex &field_re)
{
    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;

    if (! opt.metamatch.empty() &&
        ! boost::regex_search ("resolution, width, height, depth, channels, sha-1, stats", field_re)) {
        // nothing to do here
        return;
    }

    int nmip = 1;

    bool printres = opt.verbose && (opt.metamatch.empty() ||
                                boost::regex_search ("resolution, width, height, depth, channels", field_re));
    if (printres && max_subimages > 1 && opt.subimages) {
        printf (" subimage %2d: ", current_subimage);
        printf ("%4d x %4d", spec.width, spec.height);
        if (spec.depth > 1)
            printf (" x %4d", spec.depth);
        printf (", %d channel, %s%s", spec.nchannels, spec.format.c_str(),
                spec.depth > 1 ? " volume" : "");
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
        print_sha1 (input);
    }

    if (opt.verbose)
        print_metadata (spec, filename, opt, field_re);

    if (opt.compute_stats && (opt.metamatch.empty() ||
                          boost::regex_search ("stats", field_re))) {
        for (int m = 0;  m < nmip;  ++m) {
            ImageSpec mipspec;
            input->seek_subimage (current_subimage, m, mipspec);
            if (opt.filenameprefix)
                printf ("%s : ", filename.c_str());
            if (nmip > 1 && (opt.subimages || m == 0)) {
                printf ("    MIP %d of %d (%d x %d):\n",
                        m, nmip, mipspec.width, mipspec.height);
            }
            print_stats (filename, spec, current_subimage, m, nmip>1);
        }
    }

    if ( ! input->seek_subimage (current_subimage, 0, spec) )
        return;
}



bool
OiioTool::print_info (const std::string &filename, 
                      const print_info_options &opt,
                      long long &totalsize,
                      std::string &error)
{
    error.clear();
    ImageInput *input = ImageInput::create (filename.c_str(), "" /* searchpath */);
    if (! input) {
        error = geterror();
        if (error.empty())
            error = Strutil::format ("Could not open \"%s\"", filename.c_str());
        return false;
    }
    ImageSpec spec;
    if (! input->open (filename.c_str(), spec)) {
        error = input->geterror();
        if (error.empty())
            error = Strutil::format ("Could not open \"%s\"", filename.c_str());
        delete input;
        return false;
    }


    boost::regex field_re;
    if (! opt.metamatch.empty())
        field_re.assign (opt.metamatch,
                         boost::regex::extended | boost::regex_constants::icase);

    bool printed = false;
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
        printf (", %d channel, ", spec.nchannels);
        if (spec.channelformats.size()) {
            for (size_t c = 0;  c < spec.channelformats.size();  ++c)
                printf ("%s%s", c ? "/" : "",
                        spec.channelformats[c].c_str());
        } else {
            printf ("%s", spec.format.c_str());
        }
        if (spec.depth > 1)
            printf (" volume");
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
        printed = true;
    }

    if (opt.verbose && num_of_subimages != 1) {
        // info about num of subimages and their resolutions
        printf ("    %d subimages: ", num_of_subimages);
        for (int i = 0; i < num_of_subimages; ++i) {
            input->seek_subimage (i, 0, spec);
            if (spec.depth > 1)
                printf ("%dx%dx%d ", spec.width, spec.height, spec.depth);
            else
                printf ("%dx%d ", spec.width, spec.height);
        }
        printf ("\n");
    }

    // if the '-a' flag is not set we print info
    // about first subimage only
    if ( ! opt.subimages)
        num_of_subimages = 1;
    for (int i = 0; i < num_of_subimages; ++i) {
        print_info_subimage (i, num_of_subimages, spec, input,
                             filename, opt, field_re);
    }

    input->close ();
    delete input;
    return true;
}
