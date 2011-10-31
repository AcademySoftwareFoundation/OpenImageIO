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
#include <utility>

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
#include "filesystem.h"
#include "filter.h"

#include "oiiotool.h"

OIIO_NAMESPACE_USING
using namespace OiioTool;
using namespace ImageBufAlgo;


static Oiiotool ot;




// FIXME -- lots of things we skimped on so far:
// FIXME: check binary ops for compatible image dimensions
// FIXME: handle missing image
// FIXME: reject volume images?
// FIXME: do all ops respect -a (or lack thereof?)




static void
process_pending ()
{
    // Process any pending command -- this is a case where the
    // command line had prefix 'oiiotool --action file1 file2'
    // instead of infix 'oiiotool file1 --action file2'.
    if (ot.pending_callback) {
        int argc = ot.pending_argc;
        const char *argv[4];
        for (int i = 0;  i < argc;  ++i)
            argv[i] = ot.pending_argv[i];
        CallbackFunction callback = ot.pending_callback;
        ot.pending_callback = NULL;
        ot.pending_argc = 0;
        (*callback) (argc, argv);
    }
}



static int
input_file (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++) {
        if (ot.verbose)
            std::cout << "Reading " << argv[0] << "\n";
        if (ot.curimg.get() != NULL) {
            // Already a current image -- push it on the stack
            ot.image_stack.push_back (ot.curimg);
        }
        ot.curimg.reset (new ImageRec (argv[i], ot.imagecache));
        if (ot.printinfo || ot.printstats) {
            OiioTool::print_info_options pio;
            pio.verbose = ot.verbose;
            pio.subimages = ot.allsubimages;
            pio.compute_stats = ot.printstats;
            long long totalsize = 0;
            std::string error;
            OiioTool::print_info (argv[i], pio, totalsize, error);
        }
        process_pending ();
    }
    return 0;
}



static void
adjust_output_options (ImageSpec &spec, const Oiiotool &ot)
{
    if (! ot.output_dataformatname.empty()) {
        if (ot.output_dataformatname == "uint8")
            spec.set_format (TypeDesc::UINT8);
        else if (ot.output_dataformatname == "int8")
            spec.set_format (TypeDesc::INT8);
        else if (ot.output_dataformatname == "uint10") {
            spec.attribute ("oiio:BitsPerSample", 10);
            spec.set_format (TypeDesc::UINT16);
        }
        else if (ot.output_dataformatname == "uint12") {
            spec.attribute ("oiio:BitsPerSample", 12);
            spec.set_format (TypeDesc::UINT16);
        }
        else if (ot.output_dataformatname == "uint16")
            spec.set_format (TypeDesc::UINT16);
        else if (ot.output_dataformatname == "int16")
            spec.set_format (TypeDesc::INT16);
        else if (ot.output_dataformatname == "half")
            spec.set_format (TypeDesc::HALF);
        else if (ot.output_dataformatname == "float")
            spec.set_format (TypeDesc::FLOAT);
        else if (ot.output_dataformatname == "double")
            spec.set_format (TypeDesc::DOUBLE);
#if 0
        // FIXME -- eventually restore this for "copy" functionality
//        if (spec.format != inspec.format || inspec.channelformats.size())
//            nocopy = true;
#endif
        spec.channelformats.clear ();
    }

    if (ot.output_scanline)
        spec.tile_width = spec.tile_height = 0;
    else if (ot.output_tilewidth) {
        spec.tile_width = ot.output_tilewidth;
        spec.tile_height = ot.output_tileheight;
    }

    if (! ot.output_compression.empty())
        spec.attribute ("compression", ot.output_compression);
    if (ot.output_quality > 0)
        spec.attribute ("CompressionQuality", ot.output_quality);
            
    if (ot.output_planarconfig == "contig" ||
        ot.output_planarconfig == "separate")
        spec.attribute ("planarconfig", ot.output_planarconfig);
}



static bool
DateTime_to_time_t (const char *datetime, time_t &timet)
{
    int year, month, day, hour, min, sec;
    int r = sscanf (datetime, "%d:%d:%d %d:%d:%d",
                    &year, &month, &day, &hour, &min, &sec);
    // printf ("%d  %d:%d:%d %d:%d:%d\n", r, year, month, day, hour, min, sec);
    if (r != 6)
        return false;
    struct tm tmtime;
    time_t now;
    Sysutil::get_local_time (&now, &tmtime); // fill in defaults
    tmtime.tm_sec = sec;
    tmtime.tm_min = min;
    tmtime.tm_hour = hour;
    tmtime.tm_mday = day;
    tmtime.tm_mon = month-1;
    tmtime.tm_year = year-1900;
    timet = mktime (&tmtime);
    return true;
}



static int
output_file (int argc, const char *argv[])
{
    ASSERT (argc == 2 && !strcmp(argv[0],"-o"));
    std::string filename = argv[1];
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: -o " << filename << " did not have any current image to output.\n";
        return 0;
    }
    if (ot.noclobber && Filesystem::exists(filename)) {
        std::cerr << "oiiotool ERROR: Output file \"" << filename 
                  << "\" already exists, not overwriting.\n";
        return 0;
    }
    if (ot.verbose)
        std::cout << "Writing " << argv[1] << "\n";
    ImageOutput *out = ImageOutput::create (filename.c_str());
    if (! out) {
        std::cerr << "oiiotool ERROR: " << geterror() << "\n";
        return 0;
    }
    ot.curimg->read ();
    ImageRec &ir (*ot.curimg);
    ImageOutput::OpenMode mode = ImageOutput::Create;  // initial open
    for (int s = 0, send = ir.subimages();  s < send;  ++s) {
        for (int m = 0, mend = ir.miplevels(s);  m < mend;  ++m) {
            ImageSpec spec = ir(s,m).nativespec();
            adjust_output_options (spec, ot);
            if (! out->open (filename, spec, mode)) {
                std::cerr << "oiiotool ERROR: " << out->geterror() << "\n";
                return 0;
            }
            if (! ir(s,m).write (out)) {
                std::cerr << "oiiotool ERROR: " << ir(s,m).geterror() << "\n";
                return 0;
            }
            if (mend > 1) {
                if (out->supports("mipmap")) {
                    mode = ImageOutput::AppendMIPLevel;  // for next level
                } else if (out->supports("multiimage")) {
                    mode = ImageOutput::AppendSubimage;
                } else {
                    std::cout << "oiiotool WARNING: " << out->format_name() 
                              << " does not support MIP-maps for " 
                              << filename << "\n";
                    break;
                }
            }
        }
        mode = ImageOutput::AppendSubimage;  // for next subimage
        if (send > 1 && ! out->supports("multiimage")) {
            std::cout << "oiiotool WARNING: " << out->format_name() 
                      << " does not support multiple subimages for " 
                      << filename << "\n";
            break;
        }
    }

    out->close ();
    delete out;

    if (ot.output_adjust_time) {
        std::string metadatatime = ir.spec(0,0)->get_string_attribute ("DateTime");
        std::time_t in_time = ir.time();
        if (! metadatatime.empty())
            DateTime_to_time_t (metadatatime.c_str(), in_time);
        boost::filesystem::last_write_time (filename, in_time);
    }

    return 0;
}



static int
set_string_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    set_attribute (*ot.curimg, argv[1], TypeDesc::TypeString, argv[2]);
    return 0;
}



static int
set_any_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    set_attribute (*ot.curimg, argv[1], TypeDesc(TypeDesc::UNKNOWN), argv[2]);
    return 0;
}



static bool
do_erase_attribute (ImageSpec &spec, const std::string &attribname)
{
    spec.erase_attribute (attribname);
    return true;
}


template<class T>
static bool
do_set_any_attribute (ImageSpec &spec, const std::pair<std::string,T> &x)
{
    spec.attribute (x.first, x.second);
    return true;
}



bool
OiioTool::set_attribute (ImageRec &img, const std::string &attribname,
                         TypeDesc type, const std::string &value)
{
    img.read ();
    img.metadata_modified (true);
    if (! value.length()) {
        // If the value is the empty string, clear the attribute
        return apply_spec_mod (img, do_erase_attribute,
                               attribname, ot.allsubimages);
    }

    // Does it seem to be an int, or did the caller explicitly request
    // that it be set as an int?
    char *p = NULL;
    int i = strtol (value.c_str(), &p, 10);
    while (*p && isspace(*p))
        ++p;
    if ((! *p && type == TypeDesc::UNKNOWN) || type == TypeDesc::INT) {
        // int conversion succeeded and accounted for the whole string --
        // so set an int attribute.
        return apply_spec_mod (img, do_set_any_attribute<int>,
                               std::pair<std::string,int>(attribname,i),
                               ot.allsubimages);
    }

    // Does it seem to be a float, or did the caller explicitly request
    // that it be set as a float?
    p = NULL;
    float f = (float)strtod (value.c_str(), &p);
    while (*p && isspace(*p))
        ++p;
    if ((! *p && type == TypeDesc::UNKNOWN) || type == TypeDesc::FLOAT) {
        // float conversion succeeded and accounted for the whole string --
        // so set a float attribute.
        return apply_spec_mod (img, do_set_any_attribute<float>,
                               std::pair<std::string,float>(attribname,f),
                               ot.allsubimages);
    }

    // Otherwise, set it as a string attribute
    return apply_spec_mod (img, do_set_any_attribute<std::string>,
                           std::pair<std::string,std::string>(attribname,value),
                           ot.allsubimages);
}



static int
set_caption (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    const char *newargs[3];
    newargs[0] = argv[0];
    newargs[1] = "ImageDescription";
    newargs[2] = argv[1];
    return set_string_attribute (3, newargs);
}



// Utility: split semicolon-separated list
static void
split_list (const std::string &list, std::vector<std::string> &items)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(";");
    tokenizer tokens (list, sep);
    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end(); ++tok_iter) {
        std::string t = *tok_iter;
        while (t.length() && t[0] == ' ')
            t.erase (t.begin());
        if (t.length())
            items.push_back (t);
    }
}



// Utility: join list into a single semicolon-separated string
static std::string
join_list (const std::vector<std::string> &items)
{
    std::string s;
    for (size_t i = 0;  i < items.size();  ++i) {
        if (i > 0)
            s += "; ";
        s += items[i];
    }
    return s;
}



static bool
do_set_keyword (ImageSpec &spec, const std::string &keyword)
{
    std::string oldkw = spec.get_string_attribute ("Keywords");
    std::vector<std::string> oldkwlist;
    if (! oldkw.empty())
        split_list (oldkw, oldkwlist);
    bool dup = false;
    BOOST_FOREACH (const std::string &ok, oldkwlist)
        dup |= (ok == keyword);
    if (! dup)
        oldkwlist.push_back (keyword);
    spec.attribute ("Keywords", join_list (oldkwlist));
    return true;
}



static int
set_keyword (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }

    std::string keyword (argv[1]);
    if (keyword.size())
        apply_spec_mod (*ot.curimg, do_set_keyword, keyword, ot.allsubimages);

    return 0;
}



static int
clear_keywords (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    const char *newargs[3];
    newargs[0] = argv[0];
    newargs[1] = "Keywords";
    newargs[2] = "";
    return set_string_attribute (3, newargs);
}



static int
set_orientation (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    return set_attribute (*ot.curimg, argv[0], TypeDesc::INT, argv[1]);
}



static bool
do_rotate_orientation (ImageSpec &spec, const std::string &cmd)
{
    bool rotcw = cmd == "--rotcw" || cmd == "-rotcw";
    bool rotccw = cmd == "--rotccw" || cmd == "-rotccw";
    bool rot180 = cmd == "--rot180" || cmd == "-rot180";
    int orientation = spec.get_int_attribute ("Orientation", 1);
    if (orientation >= 1 && orientation <= 8) {
        static int cw[] = { 0, 6, 7, 8, 5, 2, 3, 4, 1 };
        if (rotcw || rotccw || rot180)
            orientation = cw[orientation];
        if (rotccw || rot180)
            orientation = cw[orientation];
        if (rotccw)
            orientation = cw[orientation];
        spec.attribute ("Orientation", orientation);
    }
    return true;
}



static int
rotate_orientation (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    apply_spec_mod (*ot.curimg, do_rotate_orientation, std::string(argv[0]),
                    ot.allsubimages);
    return 0;
}



static int
output_tiles (int, const char *[])
{
    // the ArgParse will have set the tile size, but we need this routine
    // to clear the scanline flag
    ot.output_scanline = false;
    return 0;
}



static int
action_unmip (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // No image has been specified so far, maybe the argument will
        // come next?  Put it on the "pending" list.
        ot.pending_callback = action_unmip;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ot.curimg->read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --unmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, false, true, true));
    ot.curimg = newimg;
    return 0;
}



static int
action_select_subimage (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // No image has been specified so far, maybe the argument will
        // come next?  Put it on the "pending" list.
        ot.pending_callback = action_select_subimage;
        ot.pending_argv[0] = argv[0];
        ot.pending_argv[1] = argv[1];
        ot.pending_argc = 2;
        return 0;
    }

    ot.curimg->read ();
    if (ot.curimg->subimages() == 1)
        return 0;    // --subimage on a single-image file is a no-op
    
    int subimage = std::min (atoi(argv[1]), ot.curimg->subimages());
    ot.curimg.reset (new ImageRec (*ot.curimg, subimage, true, true, true));
    return 0;
}



static int
action_diff (int, const char *argv[])
{
    if (! ot.curimg.get() || ot.image_stack.size() == 0) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_diff;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    int ret = do_action_diff (*ot.image_stack.back(), *ot.curimg, ot);
    if (ret != DiffErrOK && ret != DiffErrWarn)
        ot.return_value = EXIT_FAILURE;
    return 0;
}



static int
action_add (int, const char *argv[])
{
    if (! ot.curimg.get() || ot.image_stack.size() == 0) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_add;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.image_stack.back();
    ot.image_stack.resize (ot.image_stack.size()-1);
    ImageRecRef B = ot.curimg;
    A->read ();
    B->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                   ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            const ImageBuf &Bib ((*B)(s,m));
            if (! same_size (Aib, Bib)) {
                // FIXME: some day, there should be options of combining
                // differing images somehow.
                std::cerr << "oiiotool: " << argv[0] << " could not combine images of differing sizes\n";
                continue;
            }
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBufAlgo::add (Rib, Aib, Bib);
        }
    }
             
    return 0;
}



static int
action_sub (int, const char *argv[])
{
    if (! ot.curimg.get() || ot.image_stack.size() == 0) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_sub;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.image_stack.back();
    ot.image_stack.resize (ot.image_stack.size()-1);
    ImageRecRef B = ot.curimg;
    A->read ();
    B->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            const ImageBuf &Bib ((*B)(s,m));
            if (! same_size (Aib, Bib)) {
                // FIXME: some day, there should be options of combining
                // differing images somehow.
                std::cerr << "oiiotool: " << argv[0] << " could not combine images of differing sizes\n";
                continue;
            }
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBuf::ConstIterator<float> a (Aib);
            ImageBuf::ConstIterator<float> b (Bib);
            ImageBuf::Iterator<float> r (Rib);
            int nchans = Rib.nchannels();
            for ( ; ! r.done(); ++r) {
                a.pos (r.x(), r.y());
                b.pos (r.x(), r.y());
                for (int c = 0;  c < nchans;  ++c)
                    r[c] = a[c] - b[c];
            }
        }
    }
             
    return 0;
}



static int
action_abs (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_abs;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.curimg;
    A->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBuf::ConstIterator<float> a (Aib);
            ImageBuf::Iterator<float> r (Rib);
            int nchans = Rib.nchannels();
            for ( ; ! r.done(); ++r) {
                a.pos (r.x(), r.y());
                for (int c = 0;  c < nchans;  ++c)
                    r[c] = fabsf(a[c]);
            }
        }
    }
             
    return 0;
}



static int
action_flip (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_abs;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.curimg;
    A->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBuf::ConstIterator<float> a (Aib);
            ImageBuf::Iterator<float> r (Rib);
            int nchans = Rib.nchannels();
            int firstscanline = Rib.ymin();
            int lastscanline = Rib.ymax();
            for ( ; ! r.done(); ++r) {
                a.pos (r.x(), lastscanline - (r.y() - firstscanline));
                for (int c = 0;  c < nchans;  ++c)
                    r[c] = a[c];
            }
        }
    }
             
    return 0;
}



static int
action_flop (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_abs;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.curimg;
    A->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBuf::ConstIterator<float> a (Aib);
            ImageBuf::Iterator<float> r (Rib);
            int nchans = Rib.nchannels();
            int firstcolumn = Rib.xmin();
            int lastcolumn = Rib.xmax();
            for ( ; ! r.done(); ++r) {
                a.pos (lastcolumn - (r.x() - firstcolumn), r.y());
                for (int c = 0;  c < nchans;  ++c)
                    r[c] = a[c];
            }
        }
    }
             
    return 0;
}



static int
action_flipflop (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_abs;
        ot.pending_argv[0] = argv[0];
        ot.pending_argc = 1;
        return 0;
    }

    ImageRecRef A = ot.curimg;
    A->read ();
    ot.curimg.reset (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                ot.allsubimages, true, false));

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            ImageBuf::ConstIterator<float> a (Aib);
            ImageBuf::Iterator<float> r (Rib);
            int nchans = Rib.nchannels();
            int firstscanline = Rib.ymin();
            int lastscanline = Rib.ymax();
            int firstcolumn = Rib.xmin();
            int lastcolumn = Rib.xmax();
            for ( ; ! r.done(); ++r) {
                a.pos (lastcolumn - (r.x() - firstcolumn),
                       lastscanline - (r.y() - firstscanline));
                for (int c = 0;  c < nchans;  ++c)
                    r[c] = a[c];
            }
        }
    }
             
    return 0;
}



// Modify the resolution of the spec according to what's in geom.  Valid
// resolutions are 640x480 (an exact resolution), 50% (a percentage
// change, will be rounded to the nearest pixel), 1.2 (a scale amount).
static void
adjust_spec_resolution (ImageSpec &spec, const char *geom)
{
    size_t geomlen = strlen(geom);
    int x = 0, y = 0;
    float scale = 1.0f;
    if (sscanf (geom, "%dx%d", &x, &y) == 2) {
        // printf ("geom %d x %d\n", x, y);
    } else if (sscanf (geom, "%f", &scale) == 1 && geom[geomlen-1] == '%') {
        scale *= 0.01f;
        x = (int)(spec.width * scale + 0.5f);
        y = (int)(spec.height * scale + 0.5f);
    } else if (sscanf (geom, "%f", &scale) == 1) {
        x = (int)(spec.width * scale + 0.5f);
        y = (int)(spec.height * scale + 0.5f);
    } else {
        std::cout << "Unrecognized size '" << geom << "'\n";
        return;
    }
    if (spec.width != x) {
        spec.width = x;
        // Punt on display window -- just set to data window for now, and
        // also set the origin to 0.  Is there a better strategy when you
        // resize?
        spec.x = 0;
        spec.full_x = 0;
        spec.full_width = x;
    }
    if (spec.height != y) {
        spec.height = y;
        // Punt on display window -- just set to data window for now, and
        // also set the origin to 0.  Is there a better strategy when you
        // resize?
        spec.y = 0;
        spec.full_y = 0;
        spec.full_height = y;
    }
}



static int
action_create (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    int nchans = atoi (argv[2]);
    if (nchans < 1 || nchans > 1024) {
        std::cout << "Invalid number of channels: " << nchans << "\n";
        nchans = 3;
    }
    ImageSpec spec (64, 64, nchans);
    adjust_spec_resolution (spec, argv[1]);
    ImageRecRef img (new ImageRec ("new", spec, ot.imagecache));
    ImageBufAlgo::zero ((*img)());
    if (ot.curimg)
        ot.image_stack.push_back (ot.curimg);
    ot.curimg = img;
    return 0;
}



static int
action_resize (int, const char *argv[])
{
    if (! ot.curimg.get()) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        ot.pending_callback = action_resize;
        ot.pending_argv[0] = argv[0];
        ot.pending_argv[1] = argv[1];
        ot.pending_argc = 2;
        return 0;
    }

    std::string filtername;
    std::string cmd = argv[0];
    size_t pos;
    while ((pos = cmd.find_first_of(":")) != std::string::npos) {
        cmd = cmd.substr (pos+1, std::string::npos);
        if (! strncmp (cmd.c_str(), "filter=", 7)) {
            filtername = cmd.substr (7, std::string::npos);
        }
    }

    ImageRecRef A = ot.curimg;
    A->read ();
    const ImageSpec &Aspec (*A->spec(0,0));
    ImageSpec newspec = Aspec;

    adjust_spec_resolution (newspec, argv[1]);
    if (newspec.width == Aspec.width && newspec.height == Aspec.height)
        return 0;  // nothing to do

    ot.curimg.reset (new ImageRec (A->name(), newspec, ot.imagecache));
    Filter2D *filter = NULL;
    if (! filtername.empty()) {
        // If there's a matching filter, use it (and its recommended width)
        for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
            FilterDesc fd;
            Filter2D::get_filterdesc (i, &fd);
            if (fd.name == filtername) {
                filter = Filter2D::create (filtername, fd.width, fd.width);
                break;
            }
        }
        if (!filter)
            std::cout << "Filter \"" << filtername << "\" not recognized\n";
    } else  if (newspec.width > Aspec.width && newspec.height > Aspec.height) {
        // default maximizing filter: blackman-harris 3x3
        filter = Filter2D::create ("blackman-harris", 3.0f, 3.0f);
    } else if (newspec.width < Aspec.width && newspec.height < Aspec.height) {
        // defualt minimizing filter: lanczos 3 lobe 6x6
        filter = Filter2D::create ("lanczos3", 6.0f, 6.0f);
    }

    if (ot.verbose) {
        std::cout << "Resizing " << Aspec.width << "x" << Aspec.height
                  << " to " << newspec.width << "x" << newspec.height 
                  << " using ";
        if (filter) {
            std::cout << filter->name();
        } else {
            std::cout << "default";
        }
        std::cout << " filter\n";
    }
    const ImageBuf &Aib ((*A)(0,0));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    resize (Rib, Aib, Rib.xbegin(), Rib.xend(), Rib.ybegin(), Rib.yend(),
            filter);

    if (filter)
        Filter2D::destroy (filter);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    bool dummybool;
    int dummyint;
    std::string dummystr;
    ap.options ("oiiotool -- simple image processing operations\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  oiiotool [filename,option,action]...\n",
                "%*", input_file, "",
                "<SEPARATOR>", "Options (general):",
                "--help", &help, "Print help message",
                "-v", &ot.verbose, "Verbose status messages",
                "-q %!", &ot.verbose, "Quiet mode (turn verbose off)",
                "-a", &ot.allsubimages, "Do operations on all subimages/miplevels",
                "--info", &ot.printinfo, "Print resolution and metadata on all inputs",
                "--stats", &ot.printstats, "Print pixel statistics on all inputs",
//                "-u", &ot.updatemode, "Update mode: skip outputs when the file exists and is newer than all inputs",
                "--no-clobber", &ot.noclobber, "Do not overwrite existing files",
                "--noclobber", &ot.noclobber, "", // synonym
                "--threads %d", &ot.threads, "Number of threads to use (0 == #cores)",
                "<SEPARATOR>", "Commands that write images:",
                "-o %@ %s", output_file, &dummystr, "Output the current image to the named file",
                "<SEPARATOR>", "Options that affect subsequent image output:",
                "-d %s", &ot.output_dataformatname,
                    "Set the output data format to one of: "
                    "uint8, sint8, uint10, uint12, uint16, sint16, half, float, double",
                "--scanline", &ot.output_scanline, "Output scanline images",
                "--tile %@ %d %d", output_tiles, &ot.output_tilewidth, &ot.output_tileheight,
                    "Output tiled images (tilewidth, tileheight)",
                "--compression %s", &ot.output_compression, "Set the compression method",
                "--quality %d", &ot.output_quality, "Set the compression quality, 1-100",
                "--planarconfig %s", &ot.output_planarconfig,
                    "Force planarconfig (contig, separate, default)",
                "--adjust-time", &ot.output_adjust_time,
                    "Adjust file times to match DateTime metadata",
                "<SEPARATOR>", "Options that change current image metadata (but not pixel values):",
                "--attrib %@ %s %s", set_any_attribute, &dummystr, &dummystr, "Sets metadata attribute (name, value)",
                "--sattrib %@ %s %s", set_string_attribute, &dummystr, &dummystr, "Sets string metadata attribute (name, value)",
                "--caption %@ %s", set_caption, &dummystr, "Sets caption (ImageDescription metadata)",
                "--keyword %@ %s", set_keyword, &dummystr, "Add a keyword",
                "--clear-keywords %@", clear_keywords, &dummybool, "Clear all keywords",
                "--orientation %@ %d", set_orientation, &dummyint, "Set the assumed orientation",
                "--rotcw %@", rotate_orientation, &dummybool, "Rotate orientation 90 deg clockwise",
                "--rotccw %@", rotate_orientation, &dummybool, "Rotate orientation 90 deg counter-clockwise",
                "--rot180 %@", rotate_orientation, &dummybool, "Rotate orientation 180 deg",
                "<SEPARATOR>", "Options that affect subsequent actions:",
                "--fail %g", &ot.diff_failthresh, "",
                "--failpercent %g", &ot.diff_failpercent, "",
                "--hardfail %g", &ot.diff_hardfail, "",
                "--warn %g", &ot.diff_warnthresh, "",
                "--warnpercent %g", &ot.diff_warnpercent, "",
                "--hardwarn %g", &ot.diff_hardwarn, "",
                "<SEPARATOR>", "Actions:",
                "--create %@ %s %d", action_create, &dummystr, &dummyint,
                        "Create a blank image (args: geom, channels)",
                "--unmip %@", action_unmip, &dummybool, "Discard all but the top level of a MIPmap",
                "--subimage %@ %d", action_select_subimage, &dummyint, "Select just one subimage",
                "--diff %@", action_diff, &dummybool, "Print report on the difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--add %@", action_add, &dummybool, "Add two images",
                "--sub %@", action_sub, &dummybool, "Subtract two images",
                "--abs %@", action_abs, &dummybool, "Take the absolute value of the image pixels",
                "--flip %@", action_flip, &dummybool, "Flip the image vertically (top<->bottom)",
                "--flop %@", action_flop, &dummybool, "Flop the image horizontally (left<->right)",
                "--flipflop %@", action_flipflop, &dummybool, "Flip and flop the image (180 degree rotation)",
                "--resize %@ %s", action_resize, &dummystr, "Resize (640x480, 50%)",
                NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
	std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help || argc <= 1) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

}



int
main (int argc, char *argv[])
{
    ot.imagecache = ImageCache::create (false);
    ASSERT (ot.imagecache);
    ot.imagecache->attribute ("forcefloat", 1);
    ot.imagecache->attribute ("m_max_memory_MB", 2048.0);
    ot.imagecache->attribute ("autotile", 1024);
#ifdef DEBUG
    ot.imagecache->attribute ("statistics:level", 2);
#endif

    getargs (argc, argv);
    process_pending ();
    if (ot.pending_callback) {
        std::cout << "oiiotool WARNING: pending '" << ot.pending_argv[0]
                  << "' command never executed.\n";
    }

    return ot.return_value;
}
