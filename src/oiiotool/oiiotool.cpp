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
#include "fmath.h"

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



static int
set_threads (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    OIIO_NAMESPACE::attribute ("threads", atoi(argv[0]));
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

    ImageOutput::OpenMode mode = ImageOutput::Create;  // initial open
    for (int s = 0, send = ir->subimages();  s < send;  ++s) {
        for (int m = 0, mend = ir->miplevels(s);  m < mend;  ++m) {
            ImageSpec spec = *ir->spec(s,m);
            adjust_output_options (spec, ot);
            if (! out->open (filename, spec, mode)) {
                std::cerr << "oiiotool ERROR: " << out->geterror() << "\n";
                return 0;
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
        boost::filesystem::last_write_time (filename, in_time);
    }

    ot.curimg = saveimg;
    return 0;
}



static int
set_dataformat (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    std::string s (argv[1]);
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
            ImageBufAlgo::colorconvert ((*ot.curimg)(s,m), (*A)(s,m), processor, false);
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
            ImageBufAlgo::add (Rib, Aib, Bib);
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
    ImageBufAlgo::zero ((*img)());
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
        ImageBufAlgo::zero (ib);
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
        ImageBufAlgo::checker (ib, width, &color1[0], &color2[0],
                               ib.xbegin(), ib.xend(),
                               ib.ybegin(), ib.yend(),
                               ib.zbegin(), ib.zend());
    } else {
        ImageBufAlgo::zero (ib);
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
    ImageBufAlgo::capture_image (ib, camera, TypeDesc::FLOAT);
    ImageRecRef img (new ImageRec ("capture", ib.spec(), ot.imagecache));
    (*img)() = ib;
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
        ImageBufAlgo::crop (Rib, Aib, newspec.x, newspec.x+newspec.width,
                            newspec.y, newspec.y+newspec.height);
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
                     newspec.x, newspec.y, argv[1]);
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
            ImageBufAlgo::fixNonFinite (Rib, Aib, mode);
        }
    }
             
    return 0;
}



int*
ip_histogram_calculate (const ImageBuf &Aib) 
{
    ImageBuf::ConstIterator<float> a (Aib);   

    int* histogram = new int[256];
    for (int i = 0; i < 256; i++) {
        histogram[i] = 0;
    }
    
    int v;
    for ( ; ! a.done(); ++a) {
        a.pos (a.x(), a.y());
        v = (int) (a[0] * 255); // map 0-1 range to 0-255
        histogram[v]++;        
    }

    return histogram;
}



ImageBuf*
ip_histogram_to_image (int* histogram, int xres, int yres) 
{
    // Create new ImageBuf
    ImageSpec spec (256, 256, 1, TypeDesc::FLOAT);
    ImageBuf Rib ("", spec);
    ImageBuf::Iterator<float> r (Rib);

    // Init image to white
    for ( ; ! r.done(); ++r) {
        r[0] = 1.0f;                
    }
    r.pos (0, 0);
     
    // Get max value in histogram
    int max = 0;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > max) {
            max = histogram[i];
        }
    }    

    // Draw histogram column by column, down -> up, by filling pixels.
    // factor - how much we need to multiply the max histogram value so that it is drawn at the top of the image                                          
    float maxf = ((float)max / (float)(xres * yres)) * 255;       // map maxf in range 0-255
    float factor = 255.0 / maxf;                                  // maxf * factor = 255 => factor = 255 / maxf                                    
    for (int i = 0; i < 256; i++) {        
        float a = ((float)histogram[i] / (float)(xres * yres)) *  255;    // map in range 0-255
        int aa = a * factor;         
        for (int j = 0; j < aa; j++) {
            int row = 255 - j;
            r.pos (i, row);
            r[0] = 0;
        }
    }

    ImageBuf* result = new ImageBuf(Rib);
    return result;
}



static int
action_ip_histogram (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_histogram, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageSpec specR (256, 256, 1, TypeDesc::FLOAT);
    ot.push (new ImageRec ("irec", specR, ot.imagecache));

    // Get arguments from command line
    // no arguments

    // Calculate histogram just for the first miplevel of the first subimage
    const ImageBuf &Aib ((*A)());
    ImageBuf &Rib ((*ot.curimg)());          
    
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;    
    int* histogram = ip_histogram_calculate (Aib);
    Rib = *ip_histogram_to_image (histogram, xres, yres);    
             
    return 0;
}



float
rgb_to_grayscale (float* rgb) 
{    
    float wr = 0.2125, wg = 0.7154, wb = 0.072;
    return wr * rgb[0] + wg * rgb[1] + wb * rgb[2];
}



int*
ip_histogram_rgb_luma (const ImageBuf &Aib)
{
    int* histogram = new int[256];
    for (int i = 0; i < 256; i++) {
        histogram[i] = 0;
    }

    ImageBuf::ConstIterator<float> a (Aib);    
    for ( ; ! a.done(); ++a) {
        a.pos (a.x(), a.y());
        float rgb[3] = { a[0], a[1], a[2]}; 
        float luma = rgb_to_grayscale (rgb);
        int index = luma * 255; // map to 0-255 range
        histogram[index]++;
    }  
    
    return histogram;
}



static int
action_ip_histogram_rgb_luma (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_histogram_rgb_luma, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageSpec specR (256, 256, 1, TypeDesc::FLOAT);
    ot.push (new ImageRec ("irec", specR, ot.imagecache));

    // Get arguments from command line
    // no arguments

    // Calculate histogram just for the first miplevel of the first subimage
    const ImageBuf &Aib ((*A)());
    ImageBuf &Rib ((*ot.curimg)());          
    
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;    
    int* histogram_luma = ip_histogram_rgb_luma (Aib);
    Rib = *ip_histogram_to_image (histogram_luma, xres, yres);    
             
    return 0;
}



void
ip_histogram_rgb_components (const ImageBuf &Aib, int r[256], int g[256], int b[256]) 
{     
    ImageBuf::ConstIterator<float> a (Aib);
    
    // Calculate R histogram.
    for (int i = 0; i < 256; i++) {
        r[i] = 0;
    }    
    for ( ; ! a.done(); ++a) {
        a.pos (a.x(), a.y());
        int v = (int) (a[0] * 255); // map 0-1 range to 0-255
        r[v]++;        
    }
    a.pos (0, 0);

    // Calculate G histogram.
    for (int i = 0; i < 256; i++) {
        g[i] = 0;
    }    
    for ( ; ! a.done(); ++a) {
        a.pos (a.x(), a.y());
        int v = (int) (a[1] * 255); // map 0-1 range to 0-255
        g[v]++;        
    }
    a.pos (0, 0);

    // Calculate B histogram.
    for (int i = 0; i < 256; i++) {
        b[i] = 0;
    }    
    for ( ; ! a.done(); ++a) {
        a.pos (a.x(), a.y());
        int v = (int) (a[2] * 255); // map 0-1 range to 0-255
        b[v]++;        
    }    
}



static int
action_ip_histogram_rgb_components (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_histogram_rgb_components, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();

    // Calculate histogram just for the first miplevel of the first subimage
    const ImageBuf &Aib ((*A)());  
        
    // Calculate histograms for R, G and B channels
    int *r = new int[256];
    int *g = new int[256];
    int *b = new int[256];
    ip_histogram_rgb_components (Aib, r, g, b);

    // Get histograms as images  
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;   
    ImageBuf* histogram_R = ip_histogram_to_image (r, xres, yres);
    ImageBuf* histogram_G = ip_histogram_to_image (g, xres, yres);
    ImageBuf* histogram_B = ip_histogram_to_image (b, xres, yres);
    
    // Create paths
    std::string filename_prefix = argv[1];   // the images will be named prefix_R, prefix_G and prefix_B
    std::string directory = argv[2];         // dir where the three images will be saved
    std::string path = directory + filename_prefix + "_";
    std::string path_R = path + "R.tif";
    std::string path_G = path + "G.tif";
    std::string path_B = path + "B.tif";

    ImageSpec specR (256, 256, 1, TypeDesc::FLOAT);
    ot.push (new ImageRec ("irec", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());
    Rib = *histogram_G;    

    // Write images    
    histogram_R->save (path_R);
    histogram_G->save (path_G);
    histogram_B->save (path_B);
             
    return 0;
}



int*
ip_cumulative_histogram (int histogram[256]) 
{
    int* result = new int[256];
    for (int i = 0; i < 256; i++) {
        result[i] = 0;
    }

    result[0] = histogram[0];
    for (int i = 1; i < 256; i++)
    {
        result[i] = result[i-1]  + histogram[i];
    }

    return result;
}



static int
action_ip_cumulative_histogram (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_cumulative_histogram, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageSpec specR (256, 256, 1, TypeDesc::FLOAT);
    ot.push (new ImageRec ("irec", specR, ot.imagecache));

    // Get arguments from command line
    // no arguments

    // Calculate histogram just for the first miplevel of the first subimage
    const ImageBuf &Aib ((*A)());
    ImageBuf &Rib ((*ot.curimg)());
  
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;
    int* histogram = ip_histogram_calculate (Aib);
    int* histogram_cum = ip_cumulative_histogram (histogram);
    Rib = *ip_histogram_to_image (histogram_cum, xres, yres);    
             
    return 0;
}



void
ip_contrast (const ImageBuf &Aib, ImageBuf &Rib, float contrast) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        r[0] = clamp (a[0] * contrast, 0.0f, 1.0f);
    }
}



static int
action_ip_contrast (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_contrast, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float contrast = (float) atof(argv[1]);
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_contrast (Aib, Rib, contrast);
        }
    }
             
    return 0;
}



void
ip_brightness (const ImageBuf &Aib, ImageBuf &Rib, float brightness) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        r[0] = clamp (a[0] + brightness, 0.0f, 1.0f);
    }
}



static int
action_ip_brightness (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_brightness, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float brightness = (float) atof(argv[1]);
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_brightness (Aib, Rib, brightness);
        }
    }
             
    return 0;
}



void
ip_invert (const ImageBuf &Aib, ImageBuf &Rib) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        r[0] = 1.0 - a[0];
    } 
}



static int
action_ip_invert (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_invert, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments    
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_invert (Aib, Rib);
        }
    }
             
    return 0;
}



void
ip_threshold (const ImageBuf &Aib, ImageBuf &Rib, float threshold, float low, float high) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        if (a[0] < threshold) {            
            r[0] = low;
        } else {
            r[0] = high;
        }
    }
}



static int
action_ip_threshold (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_threshold, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float threshold = (float) atof(argv[1]);
    float low = (float) atof(argv[2]);
    float high = (float) atof(argv[3]);

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_threshold (Aib, Rib, threshold, low, high);
        }
    }
             
    return 0;
}



void
ip_auto_contrast (const ImageBuf &Aib, ImageBuf &Rib) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    float min = 0.0, max = 1.0, lowest = 1.0, highest = 0.0;
    // Get lowest and highest pixel values in the image
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        if (a[0] < lowest) { lowest = a[0]; }
        if (a[0] > highest) { highest = a[0]; }
    }

    r.pos (0, 0);
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        r[0] = (a[0] - lowest) * (max / (highest - lowest));
    }
}



static int
action_ip_auto_contrast (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_auto_contrast, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_auto_contrast (Aib, Rib);
        }
    }
             
    return 0;
}



void
ip_modified_auto_contrast (const ImageBuf &Aib, ImageBuf &Rib, float slow, float shigh) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;

    // Get histogram and cumulative histogram.
    int* histogram = ip_histogram_calculate (Aib);
    int* histogram_cum = ip_cumulative_histogram (histogram);
  
    // Find alow.
    int temp1 = xres * yres * slow;
    int alow = 255;
    for (int i = 0; i < 256; i++) {
        if (histogram_cum[i] >= temp1 && i < alow) {
            alow = i;
        }
    }

    // Find ahigh.
    int temp2 = xres * yres * (1 - shigh);
    int ahigh = 0;
    for (int i = 0; i < 256; i++) {
        if (histogram_cum[i] <= temp2 && i > ahigh) {
            ahigh = i;
        }
    }    
    // std::cout << alow << ", " << ahigh << "\n";

    // Modify pixels.
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        int ai = (int) (a[0] * 255.0f); // map to range 0-255
        if (ai <= alow) {
            r[0] = 0;
        } else if (ai >= ahigh) {
            r[0] = 1;
        } else {
            r[0] = (float)(ai - alow) / (float)(ahigh - alow);
        }
    }
}



static int
action_ip_modified_auto_contrast (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_modified_auto_contrast, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float slow = (float) atof(argv[1]);
    float shigh = (float) atof(argv[2]);

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_modified_auto_contrast (Aib, Rib, slow, shigh);
        }
    }
             
    return 0;
}



void
ip_histogram_equalization (const ImageBuf &Aib, ImageBuf &Rib) 
{
    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);
    const ImageSpec& spec = Aib.spec();
    int xres = spec.width;
    int yres = spec.height;

    // Get histogram and cumulative histogram.
    int* histogram = ip_histogram_calculate (Aib);
    int* histogram_cum = ip_cumulative_histogram (histogram);

    float factor = 255.0 / (float)(xres * yres);
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        int val = (int) (a[0] * 255); // map to range 0-255
        int result = floor (histogram_cum[val] * factor);
        result = clamp (result, 0, 255);
        r[0] = (float)result / 255.0f;    // map back to 0-1
    }
}



static int
action_ip_histogram_equalization (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_histogram_equalization, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_histogram_equalization (Aib, Rib);
        }
    }
             
    return 0;
}



float* 
cdf (int* histogram, int xres, int yres) 
{
    float* result = new float[256];    
    int* histogram_cum = ip_cumulative_histogram (histogram);
    for (int i = 0; i < 256; i++) {
        result[i] = (float)histogram_cum[i] / (float)(xres * yres);
    }
    return result;
}



float*
piecewise_linear_distribution_build (float* cdf) 
{
    // 256 / 32 = 8
    int samples = 32 + 1;
    int step = 8;
    
    float* result = new float[33 * 2];    
    result[32 * 2 + 0] = 255; 
    result[32 * 2 + 1] = 1;
    for (int i = 0; i < samples - 1; i++) {
        int a = i * step;
        result[i * 2 + 0] = a;
        result[i * 2 + 1] = cdf[a];
    }
    return result;
}



float
piecewise_linear_distribution_evaluate (float* pld, int i) 
{
    // Find m.
    int samples = 32;
    int m = -1;
    for (int j = 0; j < samples - 1; i++) {
        float aj = pld[j * 2 + 0];
        if (aj <= i and j > m) {
            m = j;
        }
    }
    
    float am = pld[m * 2 + 0];
    float qm = pld[m * 2 + 1];
    float am_plus1 = pld[(m + 1) * 2 + 0];
    float qm_plus1 = pld[(m + 1) * 2 + 1];

    float result = qm + (i - am) * ((qm_plus1 - qm) / (am_plus1 - am));
    return result;
}



void
ip_histogram_specification_piecewise_linear (const ImageBuf &Aib, const ImageBuf &Bib, ImageBuf &Rib) 
{    
    // Iterators
    ImageBuf::ConstIterator<float> a (Aib);    
    ImageBuf::Iterator<float> r (Rib);

    // A
    const ImageSpec& specA = Aib.spec();    
    int xresA = specA.width;
    int yresA = specA.height;
    int* histogramA = ip_histogram_calculate (Aib);    
    float* cdfA = cdf (histogramA, xresA, yresA);    

    // Create distribution_to_match from Bib
    const ImageSpec& specB = Bib.spec();
    int xresB = specB.width;
    int yresB = specB.height;
    int* histogramB = ip_histogram_calculate (Bib);
    float* cdfB = cdf (histogramB, xresB, yresB);
    float* distribution_to_match = piecewise_linear_distribution_build (cdfB);

    // Calculate mapping.
    int N = 32; // number of samples
    float q0 = distribution_to_match[0 * 2 + 1];
    unsigned char* map = new unsigned char[256];
    for (int a = 0; a < 256; a++) {
        float b = cdfA[a];        
        if (b <= q0) {
            map[a] = 0;
        }
        else if (b >= 1) {
            map[a] = 255;
        }
        else {
            int n = N - 1;            
            while (n >= 0 && distribution_to_match[n * 2 + 1] > b) {
                n--;
            }   
            float an = distribution_to_match[n * 2 + 0];
            float an_plus1 = distribution_to_match[(n + 1) * 2 + 0];
            float qn = distribution_to_match[n * 2 + 1];
            float qn_plus1 = distribution_to_match[(n + 1) * 2 + 1];         
            map[a] = an + (b - qn) * ((an_plus1 - an) / (qn_plus1 - qn));
        }
    }
    
    // Map pixels
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        int val = (int) (a[0] * 255); // map to range 0-255
        r[0] = (float)map[val] / 255.0f;    // map back to 0-1
    }
}



static int
action_ip_histogram_specification_piecewise_linear (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_ip_histogram_specification_piecewise_linear, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            const ImageBuf &Bib ((*B)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_histogram_specification_piecewise_linear (Aib, Bib, Rib);
        }
    }
             
    return 0;
}



void
ip_histogram_specification_moving_bars (const ImageBuf &Aib, const ImageBuf &Bib, ImageBuf &Rib) 
{    
    // Iterators
    ImageBuf::ConstIterator<float> a (Aib);    
    ImageBuf::Iterator<float> r (Rib);

    // A
    const ImageSpec& specA = Aib.spec();    
    int xresA = specA.width;
    int yresA = specA.height;
    int* histogramA = ip_histogram_calculate (Aib);    
    float* cdfA = cdf (histogramA, xresA, yresA);    

    // B
    const ImageSpec& specB = Bib.spec();
    int xresB = specB.width;
    int yresB = specB.height;
    int* histogramB = ip_histogram_calculate (Bib);
    float* cdfB = cdf (histogramB, xresB, yresB);
    
    // Calculate mapping.
    unsigned char* map = new unsigned char[256];
    for (int a = 0; a < 256; a++) {
        int j = 255;
        do {
            map[a] = j--;
        } while (j >= 0 && cdfA[a] <= cdfB[j]);
    }
    
    // Map pixels
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        int val = (int) (a[0] * 255); // map to range 0-255
        r[0] = (float)map[val] / 255.0f;    // map back to 0-1
    }
}



static int
action_ip_histogram_specification_moving_bars (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_ip_histogram_specification_moving_bars, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            const ImageBuf &Bib ((*B)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_histogram_specification_moving_bars (Aib, Bib, Rib);
        }
    }
             
    return 0;
}



void
ip_gamma_correction (const ImageBuf &Aib, ImageBuf &Rib, float gammaValue)
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;
    ImageBuf::ConstIterator<float> a (Aib);    
    ImageBuf::Iterator<float> r (Rib);
    
    // Calculate mapping.
    unsigned char* map = new unsigned char[256];
    for (int a = 0; a < 256; a++) {
        float aa = (float)a / 255.0f;
        aa = pow(aa, gammaValue);
        int b = (int) (aa * 255); 
        map[a] = b;
    }

    // Map pixels
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        int val = (int) (a[0] * 255); // map to range 0-255
        r[0] = (float)map[val] / 255.0f;    // map back to 0-1
    }
}



static int
action_ip_gamma_correction (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_gamma_correction, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float gammaValue = (float) atof(argv[1]);
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_gamma_correction (Aib, Rib, gammaValue);
        }
    }
             
    return 0;
}



void
ip_alpha_blend (const ImageBuf &Aib, const ImageBuf &Bib, ImageBuf &Rib, float alpha)
{
    ImageBuf::ConstIterator<float> a (Aib); 
    ImageBuf::ConstIterator<float> b (Bib);   
    ImageBuf::Iterator<float> r (Rib);
    
    // A
    const ImageSpec& specA = Aib.spec();    
    int xresA = specA.width;
    int yresA = specA.height;
    
    // B
    const ImageSpec& specB = Bib.spec();
    int xresB = specB.width;
    int yresB = specB.height;
          
    for ( ; ! r.done(); ++r) {
        a.pos (r.x(), r.y());
        b.pos (r.x(), r.y());
        r[0] = alpha * a[0] + (1 - alpha) * b[0];
    }
}



static int
action_ip_alpha_blend (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_ip_alpha_blend, argc, argv))
        return 0;

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float alpha = (float) atof(argv[1]);

    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            const ImageBuf &Bib ((*B)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_alpha_blend (Aib, Bib, Rib, alpha);
        }
    }
}



void
ip_filter_box (const ImageBuf &Aib, ImageBuf &Rib)
{
    ImageBuf::ConstIterator<float> a (Aib);       
    ImageBuf::Iterator<float> r (Rib);
    
    // A
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;
    
    for (int v = 1; v <= yres - 2; v++) {
        for (int u = 1; u <= xres - 2; u ++) {
            r.pos (u, v);
            float sum = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    a.pos (u + i, v + j);                    
                    sum += a[0];
                }
            }            
            r[0] = sum / 9.0;
        }
    }
}



static int
action_ip_filter_box (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_filter_box, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_filter_box (Aib, Rib);
        }
    }
             
    return 0;
}    



float* 
ip_filter_linear (const ImageBuf &Aib, float* filter, int filterX, int filterY) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;
    ImageBuf::ConstIterator<float> a (Aib);

    float* result = new float[xres * yres];
    int K = filterX / 2;
    int L = filterY / 2;

    for (int v = L; v <= yres - L - 1; v++) {
        for (int u = K; u <= xres - K - 1; u ++) {
            float sum = 0;
            for (int j = -L; j <= L; j++) {
                for (int i = -K; i <= K; i++) {
                    a.pos (u + i, v + j);                           
                    float c = filter[(j + L) * filterX + (i + K)];
                    sum += c * a[0];
                }
            }           
            result[v * xres + u] = sum;
        }
    }

    return result;
}



float* 
ip_filter_linear_float (float* image, int xres, int yres, float* filter, int filterX, int filterY) 
{  
    float* result = new float[xres * yres];
    int K = filterX / 2;
    int L = filterY / 2;

    for (int v = L; v <= yres - L - 1; v++) {
        for (int u = K; u <= xres - K - 1; u ++) {
            float sum = 0;
            for (int j = -L; j <= L; j++) {
                for (int i = -K; i <= K; i++) {
                    float p = image[(v + j) * xres + (u + i)];
                    float c = filter[(j + L) * filterX + (i + K)];
                    sum += c * p;
                }
            }           
            result[v * xres + u] = sum;
        }
    }

    return result;
}



void 
ip_filter_min (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for (int v = 1; v <= yres - 2; v++) {
        for (int u = 1; u <= xres - 2; u ++) {
            r.pos (u, v);
            float min = 1;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    a.pos (u + i, v + j);                                
                    if (a[0] < min) {
                        min = a[0];
                    }
                }
            }
            r[0] = min;
        }
    }    
}



static int
action_ip_filter_min (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_filter_min, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_filter_min (Aib, Rib);
        }
    }
             
    return 0;
} 



void 
ip_filter_max (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    for (int v = 1; v <= yres - 2; v++) {
        for (int u = 1; u <= xres - 2; u ++) {
            r.pos (u, v);
            float max = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    a.pos (u + i, v + j);                                
                    if (a[0] > max) {
                        max = a[0];
                    }
                }
            }
            r[0] = max;
        }
    }    
}



static int
action_ip_filter_max (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_filter_max, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_filter_max (Aib, Rib);
        }
    }
             
    return 0;
} 



template<typename T>
T* sort_array (T* arr, int n) 
{
    std::vector<T> myvector (arr, arr + n);    
    std::sort (myvector.begin(), myvector.end()); 
    T* array = new T[myvector.size()];
    for(int i = 0; i < myvector.size(); i++) {
        array[i] = myvector[i];
    }
    return array;
}



void 
ip_filter_median_3x3 (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    float* p = new float[9];
    for (int v = 1; v <= yres - 2; v++) {
        for (int u = 1; u <= xres - 2; u ++) {
            r.pos (u, v);
            int k = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    a.pos (u + j, v + i);
                    p[k++] = a[0];
                }
            }
            p = sort_array<float> (p, 9);
            r[0] = p[4];
        }
    }    
}



static int
action_ip_filter_median_3x3 (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_filter_median_3x3, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_filter_median_3x3 (Aib, Rib);
        }
    }
             
    return 0;
}



float*
normalize_filter (float* filter, int x, int y)
{
    float* result = new float[x * y];    
    float sum = 0.0;
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            sum += abs (filter[j * x + i]);
        }
    }    
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            result[j * x + i] = filter[j * x + i] / sum;
        }
    }
    return result;
}



void 
ip_edge_detector_sobel (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    // Get Dx and Dy.    
    float* sobel_filter_x = new float[9]{-1, 0, 1, -2, 0, 2, -1, 0, 1};
    float* sobel_filter_y = new float[9]{-1, -2, -1, 0, 0, 0, 1, 2, 1};    
    float* Dx = ip_filter_linear (Aib, sobel_filter_x, 3, 3); 
    float* Dy = ip_filter_linear (Aib, sobel_filter_y, 3, 3);

    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            // Set pixels to gradient magnitude.            
            r.pos (i, j);         
            r[0] = sqrt(pow(Dx[j * xres + i], 2) + pow(Dy[j * xres + i], 2));            
        }
    }
}



static int
action_ip_edge_detector_sobel (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_edge_detector_sobel, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_edge_detector_sobel (Aib, Rib);
        }
    }
             
    return 0;
}



void 
ip_edge_detector_prewitt (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    // Get Dx and Dy.    
    float* prewitt_filter_x = new float[9]{-1, 0, 1, -1, 0, 1, -1, 0, 1};
    float* prewitt_filter_y = new float[9]{-1, -1, -1, 0, 0, 0, 1, 1, 1}; 
    float* Dx = ip_filter_linear (Aib, prewitt_filter_x, 3, 3); 
    float* Dy = ip_filter_linear (Aib, prewitt_filter_y, 3, 3);

    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            // Set pixels to gradient magnitude.            
            r.pos (i, j);         
            r[0] = sqrt(pow(Dx[j * xres + i], 2) + pow(Dy[j * xres + i], 2));            
        }
    }
}



static int
action_ip_edge_detector_prewitt (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_edge_detector_prewitt, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_edge_detector_prewitt (Aib, Rib);
        }
    }
             
    return 0;
}



void 
ip_edge_sharpen_laplace (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    float* laplace_filter = new float[9]{0, 1, 0, 1, -4, 1, 0, 1, 0};
    float* temp =  ip_filter_linear (Aib, laplace_filter, 3, 3);

    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            a.pos (i, j);
            r.pos (i, j);    
            r[0] = a[0] - temp[j * xres + i];
        }
    }
}



static int
action_ip_edge_sharpen_laplace (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_edge_sharpen_laplace, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_edge_sharpen_laplace (Aib, Rib);
        }
    }
             
    return 0;
}



void 
ip_edge_unsharp_mask (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    float p = 1.0;    
    float* gaussian = new float[9]{0.075, 0.125, 0.075, 0.125, 0.2, 0.125, 0.075, 0.125, 0.075};
    float* blurred = ip_filter_linear (Aib, gaussian, 3, 3);

    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) { 
            a.pos(i, j);
            r.pos(i, j);
            float val = (1 + p) * a[0];
            val -= p * blurred[j * xres + i];
            val = clamp(val, 0.0f, 1.0f);
            r[0] = val;            
        }
    }
}



static int
action_ip_edge_unsharp_mask (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_edge_unsharp_mask, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_edge_unsharp_mask (Aib, Rib);
        }
    }
             
    return 0;
}



void
ip_edge_detector_canny (const ImageBuf &Aib, ImageBuf &Rib, float low, float high) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;    

    ImageBuf::ConstIterator<float> a (Aib);
    ImageBuf::Iterator<float> r (Rib);

    // Map Aib values to 0-255
    float* AA = new float[xres * yres];
    for (int i = 0; i < xres; i++) {
        for (int j = 0; j < yres; j++) {
            a.pos (i, j);
            AA[j * xres + i] = a[0] * 255; 
        }
    }
    
    // Step 1: Blur
    float* gaussian = new float[9]{0.075, 0.125, 0.075, 0.125, 0.2, 0.125, 0.075, 0.125, 0.075};
    float* blurred = ip_filter_linear_float (AA, xres, yres, gaussian, 3, 3);

    // Step 2: Gradient
    float* sobel_filter_x = new float[9]{-1, 0, 1, -2, 0, 2, -1, 0, 1};
    float* sobel_filter_y = new float[9]{-1, -2, -1, 0, 0, 0, 1, 2, 1};    
    float* Dx = ip_filter_linear_float (blurred, xres, yres, sobel_filter_x, 3, 3);
    float* Dy = ip_filter_linear_float (blurred, xres, yres, sobel_filter_y, 3, 3);

    float* gradient = new float[xres * yres];
    float* gradient_direction = new float[xres * yres];
    int* gradient_direction_quantized = new int[xres * yres];

    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i++) {    
            // magnitude 
            float value = sqrt(pow(Dx[j * xres + i], 2) + pow(Dy[j * xres + i], 2));
            gradient[j * xres + i] = value;

            // direction            
            gradient_direction[j * xres + i] = atan2(Dy[j * xres + i], Dx[j * xres + i]) * (180.0 / 3.1415926);
            if (gradient_direction[j * xres + i] < 0) {                
                gradient_direction[j * xres + i] += 360;
            }
            float angle = gradient_direction[j * xres + i];
            gradient_direction_quantized[j * xres + i] = 0;

            if ((angle >= 0 && angle < 22.5) || (angle >= 157.5 && angle < 202.5) || (angle >= 337.5 && angle < 360)) {
                gradient_direction_quantized[j * xres + i] = 0;                
            }
            else if ((angle >= 22.5 && angle < 67.5) || (angle >= 202.5 && angle < 247.5)) {
                gradient_direction_quantized[j * xres + i] = 45;
            }
            else if ((angle >= 67.5 && angle < 112.5) || (angle >= 247.5 && angle < 292.5)) {
                gradient_direction_quantized[j * xres + i] = 90;
            }
            else if ((angle >= 112.5 && angle < 157.5) || (angle >= 292.5 && angle < 337.5)) {                               
                gradient_direction_quantized[j * xres + i] = 135;
            }
        }
    }

    // Step 3: Non maximum suppression
    float* non_max = new float[xres * yres];
    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            non_max[j * xres + i] = gradient[j * xres + i];
        }
    }
    for (int j = 1; j < yres - 1; j++) {
        for (int i = 1; i < xres - 1; i ++) {
            if (gradient_direction_quantized[j * xres + i] == 0) {
                if (gradient[j * xres + i] < gradient[j * xres + (i - 1)] || gradient[j * xres + i] < gradient[j * xres + (i + 1)]) {
                    non_max[j * xres + i] = 0;
                }
            }
            if (gradient_direction_quantized[j * xres + i] == 45) {
                if (gradient[j * xres + i] < gradient[(j + 1) * xres + (i - 1)] || gradient[j * xres + i] < gradient[(j - 1) * xres + (i + 1)]) {
                    non_max[j * xres + i] = 0;
                }
            }
            if (gradient_direction_quantized[j * xres + i] == 90) {
                if (gradient[j * xres + i] < gradient[(j - 1) * xres + i] || gradient[j * xres + i] < gradient[(j + 1) * xres + i]) {
                    non_max[j * xres + i] = 0;
                }
            }
            if (gradient_direction_quantized[j * xres + i] == 135) {
                if (gradient[j * xres + i] < gradient[(j - 1) * xres + (i - 1)] || gradient[j * xres + i] < gradient[(j + 1) * xres + (i + 1)]) {
                    non_max[j * xres + i] = 0;
                }
            }
        }
    }

    // Step 4: Hysteresis
    float* hysteresis = new float[xres * yres];  
    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            hysteresis[j * xres + i] = non_max[j * xres + i];
        }
    }
    // Mark as an edge, maybe an edge, or definitely not an edge
    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            if (hysteresis[j * xres + i] >= high) { hysteresis[j * xres + i] = 255; }
            else if (hysteresis[j * xres + i] < high && hysteresis[j * xres + i] >= low) { hysteresis[j * xres + i] = 2; }
            else if (hysteresis[j * xres + i] < low) { hysteresis[j * xres + i] = 0; }
        }
    }

    // Reconsider case 2
    for (int j = 1; j < yres - 1; j++) {
        for (int i = 1; i < xres - 1; i ++) {
            // Survive only if there is an edge pixel in the 8-pixel neighborhood
            if (hysteresis[j * xres + (i - 1)] == 1 || hysteresis[j * xres + (i + 1)] == 1 || hysteresis[(j - 1) * xres + (i - 1)] == 1
            || hysteresis[(j - 1) * xres + i] == 1 || hysteresis[(j - 1) * xres + (i + 1)] == 1 || hysteresis[(j + 1) * xres + (i - 1)] == 1
            || hysteresis[(j + 1) * xres + i] == 1 || hysteresis[(j + 1) * xres + (i + 1)] == 1) {
                hysteresis[j * xres + i] = 255;        
            }
        }
    }
    // Suppress case 2 pixels
    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i ++) {
            if (hysteresis[j * xres + i] == 2) {
                hysteresis[j * xres + i] = 0;
            }
        }
    }  

    // Copy result to Rib
    for (int i = 0; i < xres; i++) {
        for (int j = 0; j < yres; j++) {
            r.pos (i, j);
            r[0] = hysteresis[j * xres + i] / 255.0f;   // map back to 0-1
        }
    }

    /*
    // Create rgb image that shows directions in different colors, for debug purposes.
    unsigned char* directions = new unsigned char[xres * yres * 3];
    for (int j = 0; j < yres; j++) {
        for (int i = 0; i < xres; i++) {
            // initially set to black color
            directions[j * xres * 3 + i * 3 + 0] = 0;
            directions[j * xres * 3 + i * 3 + 1] = 0;
            directions[j * xres * 3 + i * 3 + 2] = 0;
            if (hysteresis[j * xres + i] == 255) {
                if (gradient_direction_quantized[j * xres + i] == 0) {
                    directions[j * xres * 3 + i * 3 + 0] = 255;
                    directions[j * xres * 3 + i * 3 + 1] = 0;
                    directions[j * xres * 3 + i * 3 + 2] = 0;
                }
                if (gradient_direction_quantized[j * xres + i] == 45) {
                    directions[j * xres * 3 + i * 3 + 0] = 0;
                    directions[j * xres * 3 + i * 3 + 1] = 255;
                    directions[j * xres * 3 + i * 3 + 2] = 0;
                }
                if (gradient_direction_quantized[j * xres + i] == 90) {
                    directions[j * xres * 3 + i * 3 + 0] = 0;
                    directions[j * xres * 3 + i * 3 + 1] = 0;
                    directions[j * xres * 3 + i * 3 + 2] = 255;
                }
                if (gradient_direction_quantized[j * xres + i] == 135) {

                    directions[j * xres * 3 + i * 3 + 0] = 255;
                    directions[j * xres * 3 + i * 3 + 1] = 255;
                    directions[j * xres * 3 + i * 3 + 2] = 0;
                }
            }
        }
    }
    // Draw directions
    */ 
}



static int
action_ip_edge_detector_canny (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_edge_detector_canny, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    float low = (float) atof(argv[1]) * 255;    // map to 0-255 range     
    float high = (float) atof(argv[2]) * 255;    
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_edge_detector_canny (Aib, Rib, low, high);
        }
    }
             
    return 0;
}



float*
matrix_elements_pow (float* matrix, int xres, int yres, float degree) 
{
    float* result = new float[xres * yres];
    for (int i = 0; i < yres; i++) {
        for (int j = 0; j < xres; j++) {                                    
            result[i * xres + j] = pow (matrix[i * xres + j], degree);
        }
    }
    return result;
}



float*
matrix_elements_mult (float* matrixA, float* matrixB, int xres, int yres) 
{
    float* result = new float[xres * yres];
    for (int i = 0; i < yres; i++) {
        for (int j = 0; j < xres; j++) {            
            result[i * xres + j] = matrixA[i * xres + j] * matrixB[i * xres + j];
        }
    }
    return result;
}



float*
matrix_elements_abs (float* matrix, int xres, int yres) 
{
    float* result = new float[xres * yres];
    for (int i = 0; i < yres; i++) {
        for (int j = 0; j < xres; j++) {            
            result[i * xres + j] = abs(matrix[i * xres + j]);
        }
    }
    return result;
}



float*
matrix_elements_clamp (float* matrix, int xres, int yres, float min, float max) 
{
    float* result = new float[xres * yres];
    for (int i = 0; i < yres; i++) {
        for (int j = 0; j < xres; j++) {
            matrix[i * xres + j] = clamp (matrix[i * xres + j], min, max);
        }
    }
    return result;
}



void
harris_make_derivatives (const ImageBuf &Aib, float** A, float** B, float** C )
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height;

    // Map Aib values to 0-255 range
    ImageBuf::ConstIterator<float> a (Aib);
    float* AA = new float[xres * yres];
    for (int i = 0; i < xres; i++) {
        for (int j = 0; j < yres; j++) {
            a.pos (i, j);
            AA[j * xres + i] = a[0] * 255;
        }
    }

    // Blur the original image
    float pfilt[3] = { 0.223755, 0.552490, 0.223755 };
    float* Ix = ip_filter_linear_float (AA, xres, yres, pfilt, 3, 1);      
    float* Iy = ip_filter_linear_float (AA, xres, yres, pfilt, 1, 3);     

    // Horizontal and vertical derivatives
    float dfilt[3] = { 0.453014, 0, -0.453014 };    
    Ix = ip_filter_linear_float (Ix, xres, yres, dfilt, 3, 1);    
    Iy = ip_filter_linear_float (Iy, xres, yres, dfilt, 1, 3);

    // Compute A, B and C matrices   
    *A = matrix_elements_pow (Ix, xres, yres, 2);
    *B = matrix_elements_pow (Iy, xres, yres, 2);
    *C = matrix_elements_mult (Ix, Iy, xres, yres);
    
    // Convolve with 1D gaussians
    float bfilt[7] = { 0.01563, 0.09375, 0.234375, 0.3125, 0.234375, 0.09375, 0.01563 };
    *A = ip_filter_linear_float (*A, xres, yres, bfilt, 7, 1);
    *A = ip_filter_linear_float (*A, xres, yres, bfilt, 1, 7);
    *B = ip_filter_linear_float (*B, xres, yres, bfilt, 7, 1);
    *B = ip_filter_linear_float (*B, xres, yres, bfilt, 1, 7);
    *C = ip_filter_linear_float (*C, xres, yres, bfilt, 7, 1);
    *C = ip_filter_linear_float (*C, xres, yres, bfilt, 1, 7);
}



float*
harris_make_crf (float* A, float* B, float* C, int xres, int yres, float alpha) 
{
    float* Q = new float[xres * yres];
    for (int i = 0; i < yres; i++) {
        for (int j = 0; j < xres; j++) {            
            int t = i * xres + j;
            float a = A[t], b = B[t], c = C[t];
            float det = (a * b) - (c * c);
            float trace = a + b;
            Q[t] = det - alpha * (trace * trace);
        }
    } 
    return Q;
}



bool
harris_is_local_max (float* Q, int xres, int yres, int x, int y) 
{    
    if (x <= 0 || x >= xres - 1 || y <=0 || y >= yres - 1) {
        return false;
    }
    else {
        // check 8 neighbors        
        float current = Q[y * xres + x];
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i != 0 && j != 0) {
                    if (Q[(y + j) * xres + (x + i)] > current) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}



class Corner {
public:
    int x;
    int y;
    float q;

    Corner () {
        x = 0;
        y = 0;
        q = 0;
    } 
   
    Corner (int xx, int yy, int qq) {
        x = xx;
        y = yy;
        q = qq;
    }
};



bool compare_corners (Corner* i, Corner* j) { return (i->q > j->q); }



std::vector<Corner*>
harris_collect_corners (float* Q, int xres, int yres, int border, int threshold) 
{
    std::vector<Corner*> corner_list;
    for (int j = border; j < yres - border; j++) {
        for (int i = border; i < xres - border; i++) {
            float q = Q[j * xres + i];
            if (q > threshold && harris_is_local_max(Q, xres, yres, i, j)) {
                Corner* c = new Corner(i, j, q);
                corner_list.push_back (c);
            }
        }
    }
    sort (corner_list.begin(), corner_list.end(), compare_corners);
    return corner_list;
}



int 
harris_corner_dist (Corner c1, Corner c2) 
{
    int dx = c1.x - c2.x;
    int dy = c1.y - c2.y;
    return dx * dx + dy * dy; 
}



std::vector<Corner*>
harris_cleanup_corners (std::vector<Corner*> corners, float dmin) 
{
    // Vector to array    
    Corner** corner_array = new Corner*[corners.size()];
    for (int i = 0; i < corners.size(); i++) {
        corner_array[i] = corners[i];
    }
    
    std::vector<Corner*> good_corners;
    for (int i = 0; i < corners.size(); i++) {
        if (corner_array[i] != NULL) {
            good_corners.push_back(corner_array[i]);               
            // delete corners close to this one
            for (int j = i + 1; j < corners.size(); j++) {
                if (corner_array[j] != NULL) {
                    if (harris_corner_dist (*corner_array[i], *corner_array[j]) < dmin * dmin) {
                        corner_array[j] = NULL; // delete corner
                    }
                }
            }
        } 
    }
    
    return good_corners;    
}



void
harris_draw_corner (ImageBuf &Rib, int xres, int yres, Corner corner) 
{
    int x = corner.x;
    int y = corner.y;
    
    ImageBuf::Iterator<float> r (Rib);
    r.pos (x, y);
    r[0] = 0;
    for (int i = -10; i <= 10; i++) { 
        r.pos (x, y + i);       
        r[0] = 0;        
    }
    for (int i = -10; i <= 10; i++) {        
        r.pos (x + i, y); 
        r[0] = 0;      
    }
}



void
ip_corner_detector_harris (const ImageBuf &Aib, ImageBuf &Rib) 
{
    const ImageSpec& spec = Aib.spec();    
    int xres = spec.width;
    int yres = spec.height; 

    // Make derivatives
    float* A = new float[xres * yres];
    float* B = new float[xres * yres];
    float* C = new float[xres * yres];
    harris_make_derivatives (Aib, &A, &B, &C);

    // Make crf
    float* Q = harris_make_crf (A, B, C, xres, yres, 0.05);
    
    // Collect corners
    std::vector<Corner*> corners = harris_collect_corners (Q, xres, yres, 20, 20000);    

    // Cleanup corners
    corners = harris_cleanup_corners (corners, 10);
            
    // Draw corners over the original image
    Rib = Aib;
    for (int i = 0; i < corners.size(); i++) {
        harris_draw_corner (Rib, xres, yres, *corners[i]);
    }
}



static int
action_ip_corner_detector_harris (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ip_corner_detector_harris, argc, argv))
        return 0;

    ot.read ();
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           ot.allsubimages ? -1 : 0, true, false));

    // Get arguments from command line
    // no arguments
    
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = ot.curimg->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            const ImageBuf &Aib ((*A)(s,m));
            ImageBuf &Rib ((*ot.curimg)(s,m));
            
            // For each subimage and mipmap
            ip_corner_detector_harris (Aib, Rib);
        }
    }
             
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    bool dummybool;
    int dummyint;
    float dummyfloat;
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
                "--hash", &ot.hash, "Print SHA-1 hash of each input image",
//                "-u", &ot.updatemode, "Update mode: skip outputs when the file exists and is newer than all inputs",
                "--no-clobber", &ot.noclobber, "Do not overwrite existing files",
                "--noclobber", &ot.noclobber, "", // synonym
                "--threads %@ %d", set_threads, &ot.threads, "Number of threads (default 0 == #cores)",
                "<SEPARATOR>", "Commands that write images:",
                "-o %@ %s", output_file, &dummystr, "Output the current image to the named file",
                "<SEPARATOR>", "Options that affect subsequent image output:",
                "-d %@ %s", set_dataformat, &dummystr,
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
                "--attrib %@ %s %s", set_any_attribute, &dummystr, &dummystr, "Sets metadata attribute (name, value)",
                "--sattrib %@ %s %s", set_string_attribute, &dummystr, &dummystr, "Sets string metadata attribute (name, value)",
                "--caption %@ %s", set_caption, &dummystr, "Sets caption (ImageDescription metadata)",
                "--keyword %@ %s", set_keyword, &dummystr, "Add a keyword",
                "--clear-keywords %@", clear_keywords, &dummybool, "Clear all keywords",
                "--orientation %@ %d", set_orientation, &dummyint, "Set the assumed orientation",
                "--rotcw %@", rotate_orientation, &dummybool, "Rotate orientation 90 deg clockwise",
                "--rotccw %@", rotate_orientation, &dummybool, "Rotate orientation 90 deg counter-clockwise",
                "--rot180 %@", rotate_orientation, &dummybool, "Rotate orientation 180 deg",
                "--origin %@ %s", set_origin, &dummystr,
                    "Set the pixel data window origin (e.g. +20+10)",
                "--fullsize %@ %s", set_fullsize, &dummystr, "Set the display window (e.g., 1920x1080, 1024x768+100+0, -20-30)",
                "--fullpixels %@", set_full_to_pixels, &dummybool, "Set the 'full' image range to be the pixel data window",
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
                "--pattern %@ %s %s %d", action_pattern, NULL, NULL, NULL,
                        "Create a patterned image (args: pattern, geom, channels)",
                "--capture %@", action_capture, NULL,
                        "Capture an image (args: camera=%%d)",
                "--unmip %@", action_unmip, &dummybool, "Discard all but the top level of a MIPmap",
                "--selectmip %@ %d", action_selectmip, &dummyint,
                    "Select just one MIP level (0 = highest res)",
                "--subimage %@ %d", action_select_subimage, &dummyint, "Select just one subimage",
                "--diff %@", action_diff, &dummybool, "Print report on the difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--add %@", action_add, &dummybool, "Add two images",
                "--sub %@", action_sub, &dummybool, "Subtract two images",

                "--threshold %@ %g %g %g", action_ip_threshold, &dummyfloat, &dummyfloat, &dummyfloat, "Threshold",
                "--contrast %@ %g", action_ip_contrast, &dummyfloat, "Contrast",
                "--brightness %@ %g", action_ip_brightness, &dummyfloat, "Brightness",
                "--invert %@", action_ip_invert, &dummybool, "Invert",
                "--autocontrast %@", action_ip_auto_contrast, &dummybool, "Auto-contrast",
                "--autocontrast_modified %@ %g %g", action_ip_modified_auto_contrast, &dummyfloat, &dummyfloat, "Modified auto-contrast",
                "--histogram %@", action_ip_histogram, &dummybool, "Histogram",
                "--histogram_luma %@", action_ip_histogram_rgb_luma, &dummybool, "Luminance histogram for RGB image",
                "--histogram_rgb_components %@ %s %s", action_ip_histogram_rgb_components, &dummystr, &dummystr, "Histograms per channel for RGB image",                               
                "--histogram_cumulative %@", action_ip_cumulative_histogram, &dummybool, "Cumulative histogram",
                "--histogram_equalization %@", action_ip_histogram_equalization, &dummybool, "Histogram",
                "--histogram_specification_pl %@", action_ip_histogram_specification_piecewise_linear, &dummybool, "Histogram specification with piecewise linear distribution method",
                "--histogram_specification_mb %@", action_ip_histogram_specification_moving_bars, &dummybool, "Histogram specification with moving bars method",
                "--gamma_correction %@ %g", action_ip_gamma_correction, &dummyfloat, "Gamma correction",
                "--alpha_blend %@ %g", action_ip_alpha_blend, &dummyfloat, "Alpha blend two images",
                "--filter_box %@", action_ip_filter_box, &dummybool, "Box 3x3 filter",
                "--filter_min %@", action_ip_filter_min, &dummybool, "Min filter",
                "--filter_max %@", action_ip_filter_max, &dummybool, "Max filter",
                "--filter_median %@", action_ip_filter_median_3x3, &dummybool, "Median filter",
                "--edges_sobel %@", action_ip_edge_detector_sobel, &dummybool, "Sobel edge detector",
                "--edges_prewitt %@", action_ip_edge_detector_prewitt, &dummybool, "Prewitt edge detector",
                "--edges_sharpen_laplace %@", action_ip_edge_sharpen_laplace, &dummybool, "Laplace edge sharpening",
                "--edges_unsharp_mask %@", action_ip_edge_unsharp_mask, &dummybool, "Unsharp mask",
                "--edges_canny %@ %g %g", action_ip_edge_detector_canny, &dummyfloat, &dummyfloat, "Canny edge detector",
                "--corners_harris %@", action_ip_corner_detector_harris, &dummybool, "Harris corner detector",

                "--abs %@", action_abs, &dummybool, "Take the absolute value of the image pixels",
                "--flip %@", action_flip, &dummybool, "Flip the image vertically (top<->bottom)",
                "--flop %@", action_flop, &dummybool, "Flop the image horizontally (left<->right)",
                "--flipflop %@", action_flipflop, &dummybool, "Flip and flop the image (180 degree rotation)",
                "--crop %@ %s", action_crop, &dummystr, "Set pixel data resolution and offset, cropping or padding if necessary (WxH+X+Y or xmin,ymin,xmax,ymax)",
                "--croptofull %@", action_croptofull, &dummybool, "Crop or pad to make pixel data region match the \"full\" region",
                "--resize %@ %s", action_resize, &dummystr, "Resize (640x480, 50%)",
                "--fixnan %@ %s", action_fixnan, NULL, "Fix NaN/Inf values in the image (options: none, black, box3)",
                "--pop %@", action_pop, &dummybool,
                    "Throw away the current image",
                "--dup %@", action_dup, &dummybool,
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
