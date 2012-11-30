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
#include <sstream>
#include <utility>
#include <ctype.h>

#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include "argparse.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "filesystem.h"
#include "filter.h"
#include "color.h"

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


void
Oiiotool::read (ImageRecRef img)
{
    // If the image is already elaborated, take an early out, both to
    // save time, but also because we only want to do the format and
    // tile adjustments below as images are read in fresh from disk.
    if (img->elaborated())
        return;

    // Cause the ImageRec to get read
    img->read ();

    // If this is the first tiled image we have come across, use it to
    // set our tile size (unless the user explicitly set a tile size, or
    // explicitly instructed scanline output).
    const ImageSpec &nspec ((*img)().nativespec());
    if (nspec.tile_width && ! output_tilewidth && ! ot.output_scanline) {
        output_tilewidth = nspec.tile_width;
        output_tileheight = nspec.tile_height;
    }
    // If we do not yet have an expected output format, set it based on
    // this image (presumably the first one read.
    if (output_dataformat == TypeDesc::UNKNOWN) {
        output_dataformat = nspec.format;
        if (! output_bitspersample)
            output_bitspersample = nspec.get_int_attribute ("oiio:BitsPerSample");
    }
}



bool
Oiiotool::postpone_callback (int required_images, CallbackFunction func,
                             int argc, const char *argv[])
{
    if (((curimg ? 1 : 0) + (int)image_stack.size()) < required_images) {
        // Not enough have inputs been specified so far, so put this
        // function on the "pending" list.
        m_pending_callback = func;
        m_pending_argc = argc;
        for (int i = 0;  i < argc;  ++i)
            m_pending_argv[i] = ustring(argv[i]).c_str();
        return true;
    }
    return false;
}



void
Oiiotool::process_pending ()
{
    // Process any pending command -- this is a case where the
    // command line had prefix 'oiiotool --action file1 file2'
    // instead of infix 'oiiotool file1 --action file2'.
    if (m_pending_callback) {
        int argc = m_pending_argc;
        const char *argv[4];
        for (int i = 0;  i < argc;  ++i)
            argv[i] = m_pending_argv[i];
        CallbackFunction callback = m_pending_callback;
        m_pending_callback = NULL;
        m_pending_argc = 0;
        (*callback) (argc, argv);
    }
}



void
Oiiotool::error (const std::string &command, const std::string &explanation)
{
    std::cerr << "ERROR: " << command;
    if (explanation.length())
        std::cerr << " (" << explanation << ")";
    std::cerr << "\n";
    exit (-1);
}



static int
set_threads (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    OIIO::attribute ("threads", atoi(argv[1]));
    return 0;
}



static int
input_file (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++) {
        int exists = 1;
        if (! ot.imagecache->get_image_info (ustring(argv[0]), 0, 0, 
                            ustring("exists"), TypeDesc::TypeInt, &exists)
            || !exists) {
            std::cerr << "oiiotool ERROR: Could not open file \"" << argv[0] << "\"\n";
            exit (1);
        }
        if (ot.verbose)
            std::cout << "Reading " << argv[0] << "\n";
        ot.push (ImageRecRef (new ImageRec (argv[i], ot.imagecache)));
        if (ot.printinfo || ot.printstats) {
            OiioTool::print_info_options pio;
            pio.verbose = ot.verbose;
            pio.subimages = ot.allsubimages;
            pio.compute_stats = ot.printstats;
            pio.compute_sha1 = ot.hash;
            long long totalsize = 0;
            std::string error;
            OiioTool::print_info (argv[i], pio, totalsize, error);
        }
        ot.process_pending ();
    }
    return 0;
}



static void
adjust_output_options (ImageSpec &spec, const Oiiotool &ot)
{
    if (ot.output_dataformat != TypeDesc::UNKNOWN) {
        spec.set_format (ot.output_dataformat);
        if (ot.output_bitspersample != 0)
            spec.attribute ("oiio:BitsPerSample", ot.output_bitspersample);
        else
            spec.erase_attribute ("oiio:BitsPerSample");
    }

//        spec.channelformats.clear ();   // FIXME: why?

    if (ot.output_scanline)
        spec.tile_width = spec.tile_height = 0;
    else if (ot.output_tilewidth) {
        spec.tile_width = ot.output_tilewidth;
        spec.tile_height = ot.output_tileheight;
        spec.tile_depth = 1;
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
    bool supports_displaywindow = out->supports ("displaywindow");
    ot.read ();
    ImageRecRef saveimg = ot.curimg;
    ImageRecRef ir (ot.curimg);

    if (! supports_displaywindow && ot.output_autocrop &&
        (ir->spec()->x != ir->spec()->full_x ||
         ir->spec()->y != ir->spec()->full_y ||
         ir->spec()->width != ir->spec()->full_width ||
         ir->spec()->height != ir->spec()->full_height)) {
        const char *argv[] = { "croptofull" };
        int action_croptofull (int argc, const char *argv[]); // forward decl
        action_croptofull (1, argv);
        ir = ot.curimg;
    }

    std::vector<ImageSpec> subimagespecs (ir->subimages());
    for (int s = 0;  s < ir->subimages();  ++s) {
        ImageSpec spec = *ir->spec(s,0);
        adjust_output_options (spec, ot);
        subimagespecs[s] = spec;
    }

    // Do the initial open
    ImageOutput::OpenMode mode = ImageOutput::Create;
    if (ir->subimages() > 1 && out->supports("multiimage")) {
        if (! out->open (filename, ir->subimages(), &subimagespecs[0])) {
            std::cerr << "oiiotool ERROR: " << out->geterror() << "\n";
            return 0;
        }
    } else {
        if (! out->open (filename, subimagespecs[0], mode)) {
            std::cerr << "oiiotool ERROR: " << out->geterror() << "\n";
            return 0;
        }
    }

    // Output all the subimages and MIP levels
    for (int s = 0, send = ir->subimages();  s < send;  ++s) {
        for (int m = 0, mend = ir->miplevels(s);  m < mend;  ++m) {
            ImageSpec spec = *ir->spec(s,m);
            adjust_output_options (spec, ot);
            if (s > 0 || m > 0) {  // already opened first subimage/level
                if (! out->open (filename, spec, mode)) {
                    std::cerr << "oiiotool ERROR: " << out->geterror() << "\n";
                    return 0;
                }
            }
            if (! (*ir)(s,m).write (out)) {
                std::cerr << "oiiotool ERROR: " << (*ir)(s,m).geterror() << "\n";
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
        std::string metadatatime = ir->spec(0,0)->get_string_attribute ("DateTime");
        std::time_t in_time = ir->time();
        if (! metadatatime.empty())
            DateTime_to_time_t (metadatatime.c_str(), in_time);
        Filesystem::last_write_time (filename, in_time);
    }

    ot.curimg = saveimg;
    return 0;
}



static int
set_dataformat (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    std::string s (argv[1]);
    ot.output_bitspersample = 0;  // use the default
    if (s == "uint8")
        ot.output_dataformat = TypeDesc::UINT8;
    else if (s == "int8")
        ot.output_dataformat = TypeDesc::INT8;
    else if (s == "uint10") {
        ot.output_dataformat = TypeDesc::UINT16;
        ot.output_bitspersample = 10;
    } 
    else if (s == "uint12") {
        ot.output_dataformat = TypeDesc::UINT16;
        ot.output_bitspersample = 12;
    }
    else if (s == "uint16")
        ot.output_dataformat = TypeDesc::UINT16;
    else if (s == "int16")
        ot.output_dataformat = TypeDesc::INT16;
    else if (s == "half")
        ot.output_dataformat = TypeDesc::HALF;
    else if (s == "float")
        ot.output_dataformat = TypeDesc::FLOAT;
    else if (s == "double")
        ot.output_dataformat = TypeDesc::DOUBLE;
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
    set_attribute (ot.curimg, argv[1], TypeDesc::TypeString, argv[2]);
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
    set_attribute (ot.curimg, argv[1], TypeDesc(TypeDesc::UNKNOWN), argv[2]);
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
OiioTool::adjust_geometry (int &w, int &h, int &x, int &y, const char *geom,
                           bool allow_scaling)
{
    size_t geomlen = strlen(geom);
    float scale = 1.0f;
    int ww = w, hh = h;
    int xx = x, yy = y;
    int xmax, ymax;
    if (sscanf (geom, "%d,%d,%d,%d", &xx, &yy, &xmax, &ymax) == 4) {
        x = xx;
        y = yy;
        w = std::max (0, xmax-xx+1);
        h = std::max (0, ymax-yy+1);
    } else if (sscanf (geom, "%dx%d%d%d", &ww, &hh, &xx, &yy) == 4) {
        w = ww;
        h = hh;
        x = xx;
        y = yy;
    } else if (sscanf (geom, "%dx%d", &ww, &hh) == 2) {
        w = ww;
        h = hh;
    } else if (sscanf (geom, "%d%d", &xx, &yy) == 2) {
        x = xx;
        y = yy;
    } else if (allow_scaling &&
               sscanf (geom, "%f", &scale) == 1 && geom[geomlen-1] == '%') {
        scale *= 0.01f;
        w = (int)(w * scale + 0.5f);
        h = (int)(h * scale + 0.5f);
    } else if (allow_scaling && sscanf (geom, "%f", &scale) == 1) {
        w = (int)(w * scale + 0.5f);
        h = (int)(h * scale + 0.5f);
    } else {
        std::cerr << "oiiotool ERROR: Unrecognized geometry \"" 
                  << geom << "\"\n";
        return false;
    }
//    printf ("geom %dx%d, %+d%+d\n", w, h, x, y);
    return true;
}



bool
OiioTool::set_attribute (ImageRecRef img, const std::string &attribname,
                         TypeDesc type, const std::string &value)
{
    ot.read (img);
    img->metadata_modified (true);
    if (! value.length()) {
        // If the value is the empty string, clear the attribute
        return apply_spec_mod (*img, do_erase_attribute,
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
        return apply_spec_mod (*img, do_set_any_attribute<int>,
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
        return apply_spec_mod (*img, do_set_any_attribute<float>,
                               std::pair<std::string,float>(attribname,f),
                               ot.allsubimages);
    }

    // Otherwise, set it as a string attribute
    return apply_spec_mod (*img, do_set_any_attribute<std::string>,
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
    return set_attribute (ot.curimg, argv[0], TypeDesc::INT, argv[1]);
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
set_origin (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_origin, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &spec (*A->spec(0,0));
    int x = spec.x, y = spec.y;
    int w = spec.width, h = spec.height;

    adjust_geometry (w, h, x, y, argv[1]);
    if (spec.width != w || spec.height != h)
        std::cerr << argv[0] << " can't be used to change the size, only the origin\n";
    if (spec.x != x || spec.y != y) {
        spec.x = x;
        spec.y = y;
        A->metadata_modified (true);
    }
    return 0;
}



static int
set_fullsize (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_fullsize, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &spec (*A->spec(0,0));
    int x = spec.full_x, y = spec.full_y;
    int w = spec.full_width, h = spec.full_height;

    adjust_geometry (w, h, x, y, argv[1]);
    if (spec.full_x != x || spec.full_y != y ||
          spec.full_width != w || spec.full_height != h) {
        spec.full_x = x;
        spec.full_y = y;
        spec.full_width = w;
        spec.full_height = h;
        A->metadata_modified (true);
    }
    return 0;
}



static int
set_full_to_pixels (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_full_to_pixels, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &spec (*A->spec(0,0));
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    A->metadata_modified (true);
    return 0;
}



static int
set_colorspace (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    const char *args[3] = { argv[0], "oiio:ColorSpace", argv[1] };
    return set_string_attribute (3, args);
}



static int
action_colorconvert (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (ot.postpone_callback (1, action_colorconvert, argc, argv))
        return 0;

    std::string fromspace = argv[1];
    std::string tospace = argv[2];

    ot.read ();
    bool need_transform = false;
    ImageRecRef A = ot.curimg;
    ot.read (A);

    for (int s = 0, send = A->subimages();  s < send;  ++s) {
        for (int m = 0, mend = A->miplevels(s);  m < mend;  ++m) {
            const ImageSpec *spec = A->spec(s,m);
            need_transform |=
                spec->get_string_attribute("oiio:ColorSpace") != tospace;
        }
    }

    if (! need_transform)
        return 1;    // no need to do anything

    ot.pop ();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));
    
    if (fromspace == "current")
        fromspace = A->spec(0,0)->get_string_attribute ("oiio:Colorspace", "Linear");

    ColorProcessor *processor =
        ot.colorconfig.createColorProcessor (fromspace.c_str(), tospace.c_str());
    if (! processor)
        return 1;

    for (int s = 0, send = A->subimages();  s < send;  ++s) {
        for (int m = 0, mend = A->miplevels(s);  m < mend;  ++m) {
            bool ok = ImageBufAlgo::colorconvert ((*ot.curimg)(s,m), (*A)(s,m), processor, false);
            if (! ok)
                ot.error (argv[0], (*ot.curimg)(s,m).geterror());
            ot.curimg->spec(s,m)->attribute ("oiio::Colorspace", tospace);
        }
    }

    ot.colorconfig.deleteColorProcessor (processor);

    return 1;
}



static int
action_tocolorspace (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    const char *args[3] = { argv[0], "current", argv[1] };
    return action_colorconvert (3, args);
}



static int
output_tiles (int /*argc*/, const char *argv[])
{
    // the ArgParse will have set the tile size, but we need this routine
    // to clear the scanline flag
    ot.output_scanline = false;
    return 0;
}



static int
action_unmip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_unmip, argc, argv))
        return 0;

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --unmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, 0, true, true));
    ot.curimg = newimg;
    return 0;
}



// For a given spec (which contains the channel names for an image), and
// a comma separated list of channels (e.g., "B,G,R,A"), compute the
// vector of integer indices for those channels (e.g., {2,1,0,3}).
// Return true for success, false for failure, including if any of the
// channels were not present in the image.
static bool
decode_channel_set (const ImageSpec &spec, std::string chanlist,
                    std::vector<std::string> &channelnames,
                    std::vector<int> &channels, std::vector<float> &values)
{
    channels.clear ();
    while (chanlist.length()) {
        // Extract the next channel name
        size_t pos = chanlist.find_first_of(",");
        std::string onechan (chanlist, 0, pos);
        onechan = Strutil::strip (onechan);
        if (pos == std::string::npos)
            chanlist.clear();
        else
            chanlist = chanlist.substr (pos+1, std::string::npos);

        // Find the index corresponding to that channel
        float value = 0.0f;
        int ch = -1;
        for (int i = 0;  i < spec.nchannels;  ++i)
            if (spec.channelnames[i] == onechan) {
                ch = i;  break;
            }
        if (ch < 0 && onechan.length() &&
                (isdigit(onechan[0]) || onechan[0] == '-'))
            ch = atoi (onechan.c_str());
        if (ch < 0 && onechan.length() && onechan[0] == '=')
            value = atof (onechan.c_str()+1);
        channelnames.push_back (onechan);
        channels.push_back (ch);
        values.push_back (value);
    }
    return true;
}



static int
action_channels (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_channels, argc, argv))
        return 0;

    ImageRecRef A (ot.pop());
    ot.read (A);

    std::string chanlist = argv[1];
    if (chanlist == "RGB")   // Fix common synonyms/mistakes
        chanlist = "R,G,B";
    else if (chanlist == "RGBA")
        chanlist = "R,G,B,A";

    // Decode the channel set, make the full list of ImageSpec's we'll
    // need to describe the new ImageRec with the altered channels.
    std::vector<int> allmiplevels;
    std::vector<ImageSpec> allspecs;
    for (int s = 0, subimages = ot.allsubimages ? A->subimages() : 1;
         s < subimages;  ++s) {
        std::vector<std::string> channelnames;
        std::vector<int> channels;
        std::vector<float> values;
        bool ok = decode_channel_set (*A->spec(s,0), chanlist,
                                      channelnames, channels, values);
        if (! ok) {
            ot.error (argv[0], Strutil::format("Invalid or unknown channel selection \"%s\"", chanlist));
            ot.push (A);
            return 0;
        }
        int miplevels = ot.allsubimages ? A->miplevels(s) : 1;
        allmiplevels.push_back (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageSpec spec = *A->spec(s,m);
            spec.nchannels = (int)channelnames.size();
            spec.default_channel_names ();
            spec.channelformats.clear();
            allspecs.push_back (spec);
        }
    }

    // Create the replacement ImageRec
    ImageRecRef R (new ImageRec(A->name(), (int)allmiplevels.size(),
                                &allmiplevels[0], &allspecs[0]));
    ot.push (R);

    // Subimage by subimage, MIP level by MIP level, copy/shuffle the
    // channels individually from the source image into the result.
    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        std::vector<std::string> channelnames;
        std::vector<int> channels;
        std::vector<float> values;
        decode_channel_set (*A->spec(s,0), chanlist, channelnames,
                            channels, values);
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            // Shuffle the indexed/named channels
            ImageBufAlgo::channels ((*R)(s,m), (*A)(s,m), (int)channels.size(),
                                    &channels[0], false);
            // Set channels that are literals
            for (int c = 0;  c < (int)channels.size();  ++c)
                if (channels[c] < 0) {
                    ROI roi = get_roi (*R->spec(s,m));
                    roi.chbegin = c;
                    roi.chend = c+1;
                    ImageBufAlgo::fill ((*R)(s,m), &values[c], roi);
                }
            // Tricky subtlety: IBA::channels changed the underlying IB,
            // we may need to update the IRR's copy of the spec.
            R->update_spec_from_imagebuf(s,m);
        }
    }

    return 0;
}



static int
action_selectmip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_unmip, argc, argv))
        return 0;

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --selectmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, atoi(argv[1]), true, true));
    ot.curimg = newimg;
    return 0;
}



static int
action_select_subimage (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_select_subimage, argc, argv))
        return 0;

    ot.read ();
    if (ot.curimg->subimages() == 1)
        return 0;    // --subimage on a single-image file is a no-op
    
    int subimage = std::min (atoi(argv[1]), ot.curimg->subimages());
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, subimage));
    return 0;
}



static int
action_diff (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_diff, argc, argv))
        return 0;

    int ret = do_action_diff (*ot.image_stack.back(), *ot.curimg, ot);
    if (ret != DiffErrOK && ret != DiffErrWarn)
        ot.return_value = EXIT_FAILURE;
    return 0;
}



static int
action_add (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_add, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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
            bool ok = ImageBufAlgo::add (Rib, Aib, Bib);
            if (! ok)
                ot.error (argv[0], Rib.geterror());
        }
    }
             
    return 0;
}



static int
action_sub (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_sub, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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
action_abs (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_abs, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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
action_flip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flip, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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
action_flop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flop, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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
action_flipflop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flipflop, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

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



static int
action_pop (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    ot.pop ();
    return 0;
}



static int
action_dup (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    ot.push (ot.curimg);
    return 0;
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
    adjust_geometry (spec.width, spec.height, spec.x, spec.y, argv[1]);
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_z = spec.z;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    ImageRecRef img (new ImageRec ("new", spec, ot.imagecache));
    bool ok = ImageBufAlgo::zero ((*img)());
    if (! ok)
        ot.error (argv[0], (*img)().geterror());
    if (ot.curimg)
        ot.image_stack.push_back (ot.curimg);
    ot.curimg = img;
    return 0;
}



static int
action_pattern (int argc, const char *argv[])
{
    ASSERT (argc == 4);
    int nchans = atoi (argv[3]);
    if (nchans < 1 || nchans > 1024) {
        std::cout << "Invalid number of channels: " << nchans << "\n";
        nchans = 3;
    }
    ImageSpec spec (64, 64, nchans, TypeDesc::FLOAT);
    adjust_geometry (spec.width, spec.height, spec.x, spec.y, argv[2]);
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_z = spec.z;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    ImageRecRef img (new ImageRec ("new", spec, ot.imagecache));
    ImageBuf &ib ((*img)());
    std::string pattern = argv[1];
    if (Strutil::iequals(pattern,"black")) {
        bool ok = ImageBufAlgo::zero (ib);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    } else if (Strutil::istarts_with(pattern,"constant")) {
        float *fill = ALLOCA (float, nchans);
        for (int c = 0;  c < nchans;  ++c)
            fill[c] = 1.0f;
        size_t pos;
        while ((pos = pattern.find_first_of(":")) != std::string::npos) {
            pattern = pattern.substr (pos+1, std::string::npos);
            if (Strutil::istarts_with(pattern,"color=")) {
                // Parse comma-separated color list
                size_t numpos = 6;
                for (int c = 0; c < nchans && numpos < pattern.size() && pattern[numpos] != ':'; ++c) {
                    fill[c] = atof (pattern.c_str()+numpos);
                    while (numpos < pattern.size() && pattern[numpos] != ':' && pattern[numpos] != ',')
                        ++numpos;
                    if (numpos < pattern.size())
                        ++numpos;
                }
            }
        }
        bool ok = ImageBufAlgo::fill (ib, fill);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    } else if (Strutil::istarts_with(pattern,"checker")) {
        int width = 8;
        size_t pos;
        while ((pos = pattern.find_first_of(":")) != std::string::npos) {
            pattern = pattern.substr (pos+1, std::string::npos);
            if (Strutil::istarts_with(pattern,"width="))
                width = atoi (pattern.substr(6, std::string::npos).c_str());
        }
        std::vector<float> color1 (nchans, 0.0f);
        std::vector<float> color2 (nchans, 1.0f);
        bool ok = ImageBufAlgo::checker (ib, width, &color1[0], &color2[0],
                                         ib.xbegin(), ib.xend(),
                                         ib.ybegin(), ib.yend(),
                                         ib.zbegin(), ib.zend());
        if (! ok)
            ot.error (argv[0], ib.geterror());
    } else {
        bool ok = ImageBufAlgo::zero (ib);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    }
    ot.push (img);
    return 0;
}



static int
action_capture (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    int camera = 0;

    std::string cmd = argv[0];
    size_t pos;
    while ((pos = cmd.find_first_of(":")) != std::string::npos) {
        cmd = cmd.substr (pos+1, std::string::npos);
        if (Strutil::istarts_with(cmd,"camera="))
            camera = atoi(cmd.c_str()+7);
    }

    ImageBuf ib;
    bool ok = ImageBufAlgo::capture_image (ib, camera, TypeDesc::FLOAT);
    if (! ok)
        ot.error (argv[0], ib.geterror());
    ImageRecRef img (new ImageRec ("capture", ib.spec(), ot.imagecache));
    (*img)().copy (ib);
    ot.push (img);
    return 0;
}



static int
action_crop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_crop, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &Aspec (*A->spec(0,0));
    ImageSpec newspec = Aspec;

    adjust_geometry (newspec.width, newspec.height,
                     newspec.x, newspec.y, argv[1]);
    if (newspec.width != Aspec.width || newspec.height != Aspec.height) {
        // resolution changed -- we need to do a full crop
        ot.pop();
        ot.push (new ImageRec (A->name(), newspec, ot.imagecache));
        const ImageBuf &Aib ((*A)(0,0));
        ImageBuf &Rib ((*ot.curimg)(0,0));
        bool ok = ImageBufAlgo::crop (Rib, Aib, newspec.x, newspec.x+newspec.width,
                                      newspec.y, newspec.y+newspec.height);
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    } else if (newspec.x != Aspec.x || newspec.y != Aspec.y) {
        // only offset changed; don't copy the image or crop, simply
        // adjust the origins.
        Aspec.x = newspec.x;
        Aspec.y = newspec.y;
        A->metadata_modified (true);
    }

    return 0;
}



int
action_croptofull (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_croptofull, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.curimg;
    const ImageSpec &Aspec (*A->spec(0,0));
    // Implement by calling action_crop with a geometry specifier built
    // from the current full image size.
    std::string size = Strutil::format ("%dx%d%+d%+d",
                                        Aspec.full_width, Aspec.full_height,
                                        Aspec.full_x, Aspec.full_y);
    const char *newargv[2] = { "crop", size.c_str() };
    return action_crop (2, newargv);
}



static int
action_resize (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_resize, argc, argv))
        return 0;

    std::string filtername;
    std::string cmd = argv[0];
    size_t pos;
    while ((pos = cmd.find_first_of(":")) != std::string::npos) {
        cmd = cmd.substr (pos+1, std::string::npos);
        if (! strncmp (cmd.c_str(), "filter=", 7)) {
            filtername = cmd.substr (7, std::string::npos);
        }
    }

    ot.read ();
    ImageRecRef A = ot.pop();
    const ImageSpec &Aspec (*A->spec(0,0));
    ImageSpec newspec = Aspec;

    adjust_geometry (newspec.width, newspec.height,
                     newspec.x, newspec.y, argv[1], true);
    if (newspec.width == Aspec.width && newspec.height == Aspec.height)
        return 0;  // nothing to do

    // Shrink-wrap full to match actual pixels; I'm not sure what else
    // is appropriate, need to think it over.
    newspec.full_x = newspec.x;
    newspec.full_y = newspec.y;
    newspec.full_width = newspec.width;
    newspec.full_height = newspec.height;

    ot.push (new ImageRec (A->name(), newspec, ot.imagecache));
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



int
action_fixnan (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fixnan, argc, argv))
        return 0;

    NonFiniteFixMode mode = NONFINITE_BOX3;
    if (!strcmp(argv[1], "black"))
        mode = NONFINITE_BLACK;
    else if (!strcmp(argv[1], "box3"))
        mode = NONFINITE_BOX3;
    else {
        std::cerr << "--fixnan argument \"" << argv[1] << "\" not recognized. Valid choices: black, box3\n";
    }
    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            bool ok = ImageBufAlgo::fixNonFinite (Rib, Aib, mode);
            if (! ok)
                ot.error (argv[0], Rib.geterror());
        }
    }
             
    return 0;
}



static int
action_over (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_over, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    const ImageBuf &Aib ((*A)());
    const ImageBuf &Bib ((*B)());
    const ImageSpec &specA = Aib.spec();
    const ImageSpec &specB = Bib.spec();

    // Create output image specification.
    ImageSpec specR = specA;
    set_roi (specR, roi_union (get_roi(specA), get_roi(specB)));
    specR.nchannels = std::max (specA.nchannels, specB.nchannels);
    if (specR.alpha_channel < 0 && specR.nchannels == 4)
        specR.alpha_channel = 3;

    ot.push (new ImageRec ("irec", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());

    bool ok = ImageBufAlgo::over (Rib, Aib, Bib);
    if (! ok)
        ot.error (argv[0], Rib.geterror());
    return 0;
}



static int
action_fill (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fill, argc, argv))
        return 0;

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.push (new ImageRec (*A, 0, 0, true, false));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    const ImageSpec &Rspec = Rib.spec();
    bool ok = ImageBufAlgo::zero (Rib);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    int w = Rib.spec().width, h = Rib.spec().height;
    int x = Rib.spec().x, y = Rib.spec().y;
    if (! adjust_geometry (w, h, x, y, argv[1], true)) {
        return 0;
    }

    float *color = ALLOCA (float, Rspec.nchannels);
    for (int c = 0;  c < Rspec.nchannels;  ++c)
        color[c] = 1.0f;

    // Parse optional arguments for overrides
    std::string command = argv[0];
    size_t pos;
    while ((pos = command.find_first_of(":")) != std::string::npos) {
        command = command.substr (pos+1, std::string::npos);
        if (Strutil::istarts_with(command,"color=")) {
            // Parse comma-separated color list
            size_t numpos = 6;
            for (int c = 0; c < Rspec.nchannels && numpos < command.size() && command[numpos] != ':'; ++c) {
                color[c] = atof (command.c_str()+numpos);
                while (numpos < command.size() && command[numpos] != ':' && command[numpos] != ',')
                    ++numpos;
                if (numpos < command.size())
                    ++numpos;
            }
        }
    }

    ok = ImageBufAlgo::fill (Rib, color, ROI(x, x+w, y, y+h));
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    return 0;
}



static int
action_text (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_text, argc, argv))
        return 0;

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.push (new ImageRec (*A, 0, 0, true, false));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    const ImageSpec &Rspec = Rib.spec();
    bool ok = ImageBufAlgo::zero (Rib);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    // Set up defaults for text placement, size, font, color
    int x = Rspec.x + Rspec.width/2;
    int y = Rspec.y + Rspec.height/2;
    int fontsize = 16;
    std::string font = "";
    float *textcolor = ALLOCA (float, Rspec.nchannels);
    for (int c = 0;  c < Rspec.nchannels;  ++c)
        textcolor[c] = 1.0f;

    // Parse optional arguments for overrides
    std::string command = argv[0];
    size_t pos;
    while ((pos = command.find_first_of(":")) != std::string::npos) {
        command = command.substr (pos+1, std::string::npos);
        if (Strutil::istarts_with(command,"x=")) {
            x = atoi (command.c_str()+2);
        } else if (Strutil::istarts_with(command,"y=")) {
            y = atoi (command.c_str()+2);
        } else if (Strutil::istarts_with(command,"size=")) {
            fontsize = atoi (command.c_str()+5);
        } else if (Strutil::istarts_with(command,"color=")) {
            // Parse comma-separated color list
            size_t numpos = 6;
            for (int c = 0; c < Rspec.nchannels && numpos < command.size() && command[numpos] != ':'; ++c) {
                textcolor[c] = atof (command.c_str()+numpos);
                while (numpos < command.size() && command[numpos] != ':' && command[numpos] != ',')
                    ++numpos;
                if (numpos < command.size())
                    ++numpos;
            }
        } else if (Strutil::istarts_with(command,"font=")) {
            font = "";
            size_t s = 5;
            bool quote = (command[s] == '\"');
            if (quote)
                ++s;
            for ( ; s < command.size() && command[s] != ':' && command[s] != '\"'; ++s)
                font += command[s];
        }
    }

    ok = ImageBufAlgo::render_text (Rib, x, y, argv[1] /* the text */,
                                    fontsize, font, textcolor);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    return 0;
}



/// action_histogram ---------------------------------------------------------
/// Usage:
///                   ./oiiotool in --histogram:cumulative=int 'bins'x'height'
///                   channel -o out
///
/// in              - Input image that contains the channel to be histogramed.
/// cumulative      - Optional argument that can take values 0 or 1. If 0,
///                   then each bin will contain the count of pixels having
///                   values in the range for that bin. If 1, then each bin
///                   will contain not only its count, but also the counts of
///                   all preceding bins.
/// 'bins'x'height' - Width and height of the histogram, where width equals
///                   the number of bins.
/// channel         - The channel in the input image to be histogramed.
/// out             - Output image.
///
/// Examples:
///                 - ./oiiotool in --histogram 256x256 0 -o out
///
///                   Save the non-cumulative histogram of channel 0 in image
///                   'in', as an image with size 256x256.
///
///                 - ./oiiotool in --histogram:cumulative=1 256x256 0 -o out
///
///                   Same as the previous example, but now a cumulative
///                   histogram is created, instead of a regular one.
/// --------------------------------------------------------------------------
static int
action_histogram (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (ot.postpone_callback (1, action_histogram, argc, argv))
        return 0;

    // Input image.
    ot.read ();
    ImageRecRef A (ot.pop());
    const ImageBuf &Aib ((*A)());

    // Get arguments from command line.
    const char *size = argv[1];
    int channel = atoi (argv[2]);

    int cumulative = 0;
    std::string cmd = argv[0];
    size_t pos;
    while ((pos = cmd.find_first_of(":")) != std::string::npos) {
        cmd = cmd.substr (pos+1, std::string::npos);
        if (Strutil::istarts_with(cmd,"cumulative="))
            cumulative = atoi(cmd.c_str()+11);
    }

    // Extract bins and height from size.
    int bins = 0, height = 0;
    if (sscanf (size, "%dx%d", &bins, &height) != 2) {
        std::cerr << "Invalid size" << "\n";
        return -1;
    }

    // Compute regular histogram.
    std::vector<imagesize_t> hist;
    bool ok = ImageBufAlgo::histogram (Aib, channel, hist, bins);
    if (! ok) {
        ot.error (argv[0], Aib.geterror());
        return 0;
    }

    // Compute cumulative histogram if specified.
    if (cumulative == 1)
        for (int i = 1; i < bins; i++)
            hist[i] += hist[i-1];

    // Output image.
    ImageSpec specR (bins, height, 1, TypeDesc::FLOAT);
    ot.push (new ImageRec ("irec", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());

    ok = ImageBufAlgo::histogram_draw (Rib, hist);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
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
                "--hash", &ot.hash, "Print SHA-1 hash of each input image",
//                "-u", &ot.updatemode, "Update mode: skip outputs when the file exists and is newer than all inputs",
                "--no-clobber", &ot.noclobber, "Do not overwrite existing files",
                "--noclobber", &ot.noclobber, "", // synonym
                "--threads %@ %d", set_threads, &ot.threads, "Number of threads (default 0 == #cores)",
                "<SEPARATOR>", "Commands that write images:",
                "-o %@ %s", output_file, NULL, "Output the current image to the named file",
                "<SEPARATOR>", "Options that affect subsequent image output:",
                "-d %@ %s", set_dataformat, NULL,
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
                "--noautocrop %!", &ot.output_autocrop, 
                    "Do not automatically crop images whose formats don't support separate pixel data and full/display windows",
                "<SEPARATOR>", "Options that change current image metadata (but not pixel values):",
                "--attrib %@ %s %s", set_any_attribute, NULL, NULL, "Sets metadata attribute (name, value)",
                "--sattrib %@ %s %s", set_string_attribute, NULL, NULL, "Sets string metadata attribute (name, value)",
                "--caption %@ %s", set_caption, NULL, "Sets caption (ImageDescription metadata)",
                "--keyword %@ %s", set_keyword, NULL, "Add a keyword",
                "--clear-keywords %@", clear_keywords, NULL, "Clear all keywords",
                "--orientation %@ %d", set_orientation, NULL, "Set the assumed orientation",
                "--rotcw %@", rotate_orientation, NULL, "Rotate orientation 90 deg clockwise",
                "--rotccw %@", rotate_orientation, NULL, "Rotate orientation 90 deg counter-clockwise",
                "--rot180 %@", rotate_orientation, NULL, "Rotate orientation 180 deg",
                "--origin %@ %s", set_origin, NULL,
                    "Set the pixel data window origin (e.g. +20+10)",
                "--fullsize %@ %s", set_fullsize, NULL, "Set the display window (e.g., 1920x1080, 1024x768+100+0, -20-30)",
                "--fullpixels %@", set_full_to_pixels, NULL, "Set the 'full' image range to be the pixel data window",
                "<SEPARATOR>", "Options that affect subsequent actions:",
                "--fail %g", &ot.diff_failthresh, "Failure threshold difference (0.000001)",
                "--failpercent %g", &ot.diff_failpercent, "Allow this percentage of failures in diff (0)",
                "--hardfail %g", &ot.diff_hardfail, "Fail diff if any one pixel exceeds this error (infinity)",
                "--warn %g", &ot.diff_warnthresh, "Warning threshold difference (0.00001)",
                "--warnpercent %g", &ot.diff_warnpercent, "Allow this percentage of warnings in diff (0)",
                "--hardwarn %g", &ot.diff_hardwarn, "Warn if any one pixel difference exceeds this error (infinity)",
                "<SEPARATOR>", "Actions:",
                "--create %@ %s %d", action_create, NULL, NULL,
                        "Create a blank image (args: geom, channels)",
                "--pattern %@ %s %s %d", action_pattern, NULL, NULL, NULL,
                        "Create a patterned image (args: pattern, geom, channels)",
                "--capture %@", action_capture, NULL,
                        "Capture an image (args: camera=%%d)",
                "--diff %@", action_diff, NULL, "Print report on the difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--add %@", action_add, NULL, "Add two images",
                "--sub %@", action_sub, NULL, "Subtract two images",
                "--abs %@", action_abs, NULL, "Take the absolute value of the image pixels",
                "--over %@", action_over, NULL, "'Over' composite of two images",
                "--histogram %@ %s %d", action_histogram, NULL, NULL, "Histogram one channel (args: cumulative=0)",
                "--flip %@", action_flip, NULL, "Flip the image vertically (top<->bottom)",
                "--flop %@", action_flop, NULL, "Flop the image horizontally (left<->right)",
                "--flipflop %@", action_flipflop, NULL, "Flip and flop the image (180 degree rotation)",
                "--crop %@ %s", action_crop, NULL, "Set pixel data resolution and offset, cropping or padding if necessary (WxH+X+Y or xmin,ymin,xmax,ymax)",
                "--croptofull %@", action_croptofull, NULL, "Crop or pad to make pixel data region match the \"full\" region",
                "--resize %@ %s", action_resize, NULL, "Resize (640x480, 50%)",
                "--fixnan %@ %s", action_fixnan, NULL, "Fix NaN/Inf values in the image (options: none, black, box3)",
                "--fill %@ %s", action_fill, NULL, "Fill a region (options: x=, y=, size=, color=)",
                "--text %@ %s", action_text, NULL,
                    "Render text into the current image (options: x=, y=, size=, color=)",
                "<SEPARATOR>", "Image stack manipulation:",
                "--ch %@ %s", action_channels, NULL,
                    "Select or shuffle channels (e.g., \"R,G,B\", \"B,G,R\", \"2,3,4\")",
                "--unmip %@", action_unmip, NULL, "Discard all but the top level of a MIPmap",
                "--selectmip %@ %d", action_selectmip, NULL,
                    "Select just one MIP level (0 = highest res)",
                "--subimage %@ %d", action_select_subimage, NULL, "Select just one subimage",
                "--pop %@", action_pop, NULL,
                    "Throw away the current image",
                "--dup %@", action_dup, NULL,
                    "Duplicate the current image (push a copy onto the stack)",
                "<SEPARATOR>", "Color management:",
                "--iscolorspace %@ %s", set_colorspace, NULL,
                    "Set the assumed color space (without altering pixels)",
                "--tocolorspace %@ %s", action_tocolorspace, NULL,
                    "Convert the current image's pixels to a named color space",
                "--colorconvert %@ %s %s", action_colorconvert, NULL, NULL,
                    "Convert pixels from 'src' to 'dst' color space (without regard to its previous interpretation)",
                NULL);

    if (ap.parse(argc, (const char**)argv) < 0) {
	std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help || argc <= 1) {
        ap.usage ();

        // debugging color space names
        std::stringstream s;
        s << "Known color spaces: ";
        const char *linear = ot.colorconfig.getColorSpaceNameByRole("linear");
        for (int i = 0, e = ot.colorconfig.getNumColorSpaces();  i < e;  ++i) {
            const char *n = ot.colorconfig.getColorSpaceNameByIndex(i);
            s << "\"" << n << "\"";
            if (linear && !Strutil::iequals(n,"linear") &&
                    Strutil::iequals (n, linear))
                s << " (linear)";
            if (i < e-1)
                s << ", ";
        }
        int columns = Sysutil::terminal_columns() - 2;
        std::cout << Strutil::wordwrap(s.str(), columns, 4) << "\n";

        exit (EXIT_FAILURE);
    }

}



int
main (int argc, char *argv[])
{
    ot.imagecache = ImageCache::create (false);
    ASSERT (ot.imagecache);
    ot.imagecache->attribute ("forcefloat", 1);
    ot.imagecache->attribute ("m_max_memory_MB", 4096.0);
//    ot.imagecache->attribute ("autotile", 1024);
#ifdef DEBUG
    ot.imagecache->attribute ("statistics:level", 2);
#endif

    getargs (argc, argv);
    ot.process_pending ();
    if (ot.pending_callback()) {
        std::cout << "oiiotool WARNING: pending '" << ot.pending_callback_name()
                  << "' command never executed.\n";
    }

    return ot.return_value;
}
