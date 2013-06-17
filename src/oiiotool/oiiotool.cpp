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
#include <map>

#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "argparse.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "filesystem.h"
#include "filter.h"
#include "color.h"
#include "timer.h"

#include "oiiotool.h"

OIIO_NAMESPACE_USING
using namespace OiioTool;
using namespace ImageBufAlgo;


static Oiiotool ot;



Oiiotool::Oiiotool ()
    : imagecache(NULL),
      return_value (EXIT_SUCCESS),
      total_readtime (false /*don't start timer*/),
      total_writetime (false /*don't start timer*/),
      total_imagecache_readtime (0.0),
      enable_function_timing(true)
{
    clear_options ();
}



void
Oiiotool::clear_options ()
{
    verbose = false;
    runstats = false;
    noclobber = false;
    allsubimages = false;
    printinfo = false;
    printstats = false;
    hash = false;
    updatemode = false;
    threads = 0;
    full_command_line.clear ();
    printinfo_metamatch.clear ();
    printinfo_nometamatch.clear ();
    output_dataformat = TypeDesc::UNKNOWN;
    output_channelformats.clear ();
    output_bitspersample = 0;
    output_scanline = false;
    output_tilewidth = 0;
    output_tileheight = 0;
    output_compression = "";
    output_quality = -1;
    output_planarconfig = "default";
    output_adjust_time = false;
    output_autocrop = true;
    diff_warnthresh = 1.0e-6f;
    diff_warnpercent = 0;
    diff_hardwarn = std::numeric_limits<float>::max();
    diff_failthresh = 1.0e-6f;
    diff_failpercent = 0;
    diff_hardfail = std::numeric_limits<float>::max();
    m_pending_callback = NULL;
    m_pending_argc = 0;
}



std::string
format_resolution (int w, int h, int x, int y)
{
#if 0
    // This should work...
    return Strutil::format ("%dx%d%+d%+d", w, h, x, y);
    // ... but tinyformat doesn't print the sign for '0' values!  It
    // appears to be a bug with iostream use of 'showpos' format flag,
    // specific to certain gcc libs, perhaps only on OSX.  Workaround:
#else
    return Strutil::format ("%dx%d%c%d%c%d", w, h,
                            x >= 0 ? '+' : '-', abs(x),
                            y >= 0 ? '+' : '-', abs(y));
#endif
}


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

    // Cause the ImageRec to get read.  Try to compute how long it took.
    // Subtract out ImageCache time, to avoid double-accounting it later.
    float pre_ic_time, post_ic_time;
    imagecache->getattribute ("stat:fileio_time", pre_ic_time);
    total_readtime.start ();
    img->read ();
    total_readtime.stop ();
    imagecache->getattribute ("stat:fileio_time", post_ic_time);
    total_imagecache_readtime += post_ic_time - pre_ic_time;

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
extract_options (std::map<std::string,std::string> &options,
                 std::string command)
{
    // std::cout << "extract_options '" << command << "'\n";
    int noptions = 0;
    size_t pos;
    while ((pos = command.find_first_of(":")) != std::string::npos) {
        command = command.substr (pos+1, std::string::npos);
        size_t e = command.find_first_of("=");
        if (e != std::string::npos) {
            std::string name = command.substr(0,e);
            std::string value = command.substr(e+1,command.find_first_of(":")-(e+1));
            options[name] = value;
            ++noptions;
            // std::cout << "'" << name << "' -> '" << value << "'\n";
        }
    }
    return noptions;
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
    Timer timer (ot.enable_function_timing);
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
            pio.metamatch = ot.printinfo_metamatch;
            pio.nometamatch = ot.printinfo_nometamatch;
            long long totalsize = 0;
            std::string error;
            bool ok = OiioTool::print_info (argv[i], pio, totalsize, error);
            if (! ok)
                std::cerr << "oiiotool ERROR: " << error << "\n";
        }
        ot.process_pending ();
    }
    ot.function_times["input"] += timer();
    return 0;
}



static void
string_to_dataformat (const std::string &s, TypeDesc &dataformat, int &bits)
{
    if (s == "uint8") {
        dataformat = TypeDesc::UINT8;   bits = 0;
    } else if (s == "int8") {
        dataformat = TypeDesc::INT8;    bits = 0;
    } else if (s == "uint10") {
        dataformat = TypeDesc::UINT16;  bits = 10;
    } else if (s == "uint12") {
        dataformat = TypeDesc::UINT16;  bits = 12;
    } else if (s == "uint16") {
        dataformat = TypeDesc::UINT16;  bits = 0;
    } else if (s == "int16") {
        dataformat = TypeDesc::INT16;   bits = 0;
    } else if (s == "half") {
        dataformat = TypeDesc::HALF;    bits = 0;
    } else if (s == "float") {
        dataformat = TypeDesc::FLOAT;   bits = 0;
    } else if (s == "double") {
        dataformat = TypeDesc::DOUBLE;  bits = 0;
    }
}



static void
adjust_output_options (ImageSpec &spec, const Oiiotool &ot,
                       bool format_supports_tiles)
{
    if (ot.output_dataformat != TypeDesc::UNKNOWN) {
        spec.set_format (ot.output_dataformat);
        if (ot.output_bitspersample != 0)
            spec.attribute ("oiio:BitsPerSample", ot.output_bitspersample);
        else
            spec.erase_attribute ("oiio:BitsPerSample");
    }
    if (ot.output_channelformats.size()) {
        spec.channelformats.clear ();
        spec.channelformats.resize (spec.nchannels, spec.format);
        bool allsame = true;
        for (int c = 0;  c < spec.nchannels;  ++c) {
            if (c >= (int)spec.channelnames.size())
                break;
            std::map<std::string,std::string>::const_iterator i = ot.output_channelformats.find (spec.channelnames[c]);
            if (i != ot.output_channelformats.end()) {
                int bits = 0;
                string_to_dataformat (i->second, spec.channelformats[c], bits);
            }
            if (c > 0)
                allsame &= (spec.channelformats[c] == spec.channelformats[0]);
        }
        if (allsame) {
            spec.format = spec.channelformats[0];
            spec.channelformats.clear();
        }
    } else {
        spec.channelformats.clear ();
    }

    // If we've had tiled input and scanline was not explicitly
    // requested, we'll try tiled output.
    if (ot.output_tilewidth && !ot.output_scanline && format_supports_tiles) {
        spec.tile_width = ot.output_tilewidth;
        spec.tile_height = ot.output_tileheight;
        spec.tile_depth = 1;
    } else {
        spec.tile_width = spec.tile_height = spec.tile_depth = 0;
    }

    if (! ot.output_compression.empty())
        spec.attribute ("compression", ot.output_compression);
    if (ot.output_quality > 0)
        spec.attribute ("CompressionQuality", ot.output_quality);
            
    if (ot.output_planarconfig == "contig" ||
        ot.output_planarconfig == "separate")
        spec.attribute ("planarconfig", ot.output_planarconfig);

    // Append command to image history
    std::string history = spec.get_string_attribute ("Exif:ImageHistory");
    if (! Strutil::iends_with (history, ot.full_command_line)) { // don't add twice
        if (history.length() && ! Strutil::iends_with (history, "\n"))
            history += std::string("\n");
        history += ot.full_command_line;
        spec.attribute ("Exif:ImageHistory", history);
    }

    std::string software = Strutil::format ("OpenImageIO %s : %s",
                                   OIIO_VERSION_STRING, ot.full_command_line);
    spec.attribute ("Software", software);
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

    Timer timer (ot.enable_function_timing);
    ot.total_writetime.start();
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
    bool supports_tiles = out->supports ("tiles");
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
        adjust_output_options (spec, ot, supports_tiles);
        // For deep files, must copy the native deep channelformats
        if (spec.deep)
            spec.channelformats = (*ir)(s,0).nativespec().channelformats;
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
            adjust_output_options (spec, ot, supports_tiles);
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
    ot.total_writetime.stop();
    ot.function_times["output"] += timer();
    return 0;
}



static int
set_dataformat (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    std::vector<std::string> chans;
    Strutil::split (argv[1], chans, ",");

    if (chans.size() == 0) {
        return 0;   // Nothing to do
    }

    if (chans.size() == 1 && !strchr(chans[0].c_str(),'=')) {
        // Of the form:   -d uint8    (for example)
        // Just one default format designated, apply to all channels
        ot.output_dataformat = TypeDesc::UNKNOWN;
        ot.output_bitspersample = 0;
        string_to_dataformat (chans[0], ot.output_dataformat,
                              ot.output_bitspersample);
        ot.output_channelformats.clear ();
        return 0;  // we're done
    }

    // If we make it here, the format designator was of the form
    //    name0=type0,name1=type1,...
    for (size_t i = 0;  i < chans.size();  ++i) {
        const char *eq = strchr(chans[i].c_str(),'=');
        if (eq) {
            std::string channame (chans[i], 0, eq - chans[i].c_str());
            ot.output_channelformats[channame] = std::string (eq+1);
        } else {
            ot.error (argv[0], Strutil::format ("Malformed format designator \"%s\"", chans[i]));
        }
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



static bool
do_set_keyword (ImageSpec &spec, const std::string &keyword)
{
    std::string oldkw = spec.get_string_attribute ("Keywords");
    std::vector<std::string> oldkwlist;
    if (! oldkw.empty())
        Strutil::split (oldkw, oldkwlist, ";");
    bool dup = false;
    BOOST_FOREACH (std::string &ok, oldkwlist) {
        ok = Strutil::strip (ok);
        dup |= (ok == keyword);
    }
    if (! dup) {
        oldkwlist.push_back (keyword);
        spec.attribute ("Keywords", Strutil::join (oldkwlist, "; "));
    }
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
    Timer timer (ot.enable_function_timing);

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
    
    ot.function_times["origin"] += timer();
    return 0;
}



static int
set_fullsize (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_fullsize, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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
    ot.function_times["fullsize"] += timer();
    return 0;
}



static int
set_full_to_pixels (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_full_to_pixels, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &spec (*A->spec(0,0));
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    A->metadata_modified (true);
    ot.function_times["fullpixels"] += timer();
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
    Timer timer (ot.enable_function_timing);

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
    if (! processor) {
        if (ot.colorconfig.error())
            ot.error ("colorconvert", ot.colorconfig.geterror());
        else
            ot.error ("colorconvert", "Could not construct the color transform");
        return 1;
    }

    for (int s = 0, send = A->subimages();  s < send;  ++s) {
        for (int m = 0, mend = A->miplevels(s);  m < mend;  ++m) {
            bool ok = ImageBufAlgo::colorconvert ((*ot.curimg)(s,m), (*A)(s,m), processor, false);
            if (! ok)
                ot.error (argv[0], (*ot.curimg)(s,m).geterror());
            ot.curimg->spec(s,m)->attribute ("oiio::Colorspace", tospace);
        }
    }

    ot.colorconfig.deleteColorProcessor (processor);

    ot.function_times["colorconvert"] += timer();
    return 1;
}



static int
action_tocolorspace (int argc, const char *argv[])
{
    // Don't time -- let it get accounted by colorconvert
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        std::cerr << "oiiotool ERROR: " << argv[0] << " had no current image.\n";
        return 0;
    }
    const char *args[3] = { argv[0], "current", argv[1] };
    return action_colorconvert (3, args);
}



static int
action_ociolook (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    if (ot.postpone_callback (1, action_ociolook, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::string lookname = argv[1];

    std::map<std::string,std::string> options;
    options["inverse"] = "0";
    options["from"] = "current";
    options["to"] = "current";
    options["key"] = "";
    options["value"] = "";
    extract_options (options, argv[0]);
    std::string fromspace = options["from"];
    std::string tospace = options["to"];
    std::string contextkey = options["key"];
    std::string contextvalue = options["value"];
    bool inverse = Strutil::from_string<int> (options["inverse"]);

    ImageRecRef A = ot.curimg;
    ot.read (A);
    ot.pop ();
    ot.push (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                           0, true, true|false));

    if (fromspace == "current" || fromspace == "")
        fromspace = A->spec(0,0)->get_string_attribute ("oiio:Colorspace", "Linear");
    if (tospace == "current" || tospace == "")
        tospace = A->spec(0,0)->get_string_attribute ("oiio:Colorspace", "Linear");

    ColorProcessor *processor = ot.colorconfig.createLookTransform (
            lookname.c_str(), fromspace.c_str(), tospace.c_str(),
            inverse, contextkey.c_str(), contextvalue.c_str());
    if (! processor) {
        if (ot.colorconfig.error())
            ot.error ("ociolook", ot.colorconfig.geterror());
        else
            ot.error ("ociolook", "Could not construct the look transform");
        return 1;
    }

    for (int s = 0, send = A->subimages();  s < send;  ++s) {
        for (int m = 0, mend = A->miplevels(s);  m < mend;  ++m) {
            bool ok = ImageBufAlgo::colorconvert ((*ot.curimg)(s,m), (*A)(s,m), processor, false);
            if (! ok)
                ot.error (argv[0], (*ot.curimg)(s,m).geterror());
            ot.curimg->spec(s,m)->attribute ("oiio::Colorspace", tospace);
        }
    }

    ot.colorconfig.deleteColorProcessor (processor);

    ot.function_times["ociolook"] += timer();
    return 1;
}



static int
action_unpremult (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_unpremult, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A = ot.pop();
    A->read ();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s)
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m)
            ImageBufAlgo::unpremult ((*R)(s,m));

    ot.function_times["unpremult"] += timer();
    return 0;
}



static int
action_premult (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_premult, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A = ot.pop();
    A->read ();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s)
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m)
            ImageBufAlgo::premult ((*R)(s,m));

    ot.function_times["premult"] += timer();
    return 0;
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
    Timer timer (ot.enable_function_timing);

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --unmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, 0, true, true));
    ot.curimg = newimg;
    ot.function_times["unmip"] += timer();
    return 0;
}



static int
set_channelnames (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_channelnames, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    ImageRecRef A = ot.curimg;
    ot.read (A);

    std::vector<std::string> newchannelnames;
    Strutil::split (argv[1], newchannelnames, ",");

    for (int s = 0; s < A->subimages(); ++s) {
        int miplevels = A->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageSpec *spec = A->spec(s,m);
            spec->channelnames.resize (spec->nchannels);
            for (int c = 0; c < spec->nchannels;  ++c) {
                if (c < (int)newchannelnames.size() &&
                      newchannelnames[c].size()) {
                    std::string name = newchannelnames[c];
                    spec->channelnames[c] = name;
                    if (Strutil::iequals(name,"A") ||
                        Strutil::iends_with(name,".A") ||
                        Strutil::iequals(name,"Alpha") ||
                        Strutil::iends_with(name,".Alpha"))
                        spec->alpha_channel = c;
                    if (Strutil::iequals(name,"Z") ||
                        Strutil::iends_with(name,".Z") ||
                        Strutil::iequals(name,"Depth") ||
                        Strutil::iends_with(name,".Depth"))
                        spec->z_channel = c;
                }
            }
        }
    }
    ot.function_times["chnames"] += timer();
    return 0;
}



// For a given spec (which contains the channel names for an image), and
// a comma separated list of channels (e.g., "B,G,R,A"), compute the
// vector of integer indices for those channels (e.g., {2,1,0,3}).
// A channel may be a literal assignment (e.g., "=0.5"), or a literal
// assignment with channel naming (e.g., "Z=0.5").
// Return true for success, false for failure, including if any of the
// channels were not present in the image.  Upon return, channels
// will be the indices of the source image channels to copy (-1 for
// channels that are not filled with source data), values will hold
// the value to fill un-sourced channels (defaulting to zero), and
// newchannelnames will be the name of renamed or non-default-named
// channels (defaulting to "" if no special name is needed).
static bool
decode_channel_set (const ImageSpec &spec, std::string chanlist,
                    std::vector<std::string> &newchannelnames,
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
        newchannelnames.push_back (std::string());
        float value = 0.0f;
        int ch = -1;
        for (int i = 0;  i < spec.nchannels;  ++i)
            if (spec.channelnames[i] == onechan) { // name of a known channel?
                ch = i;
                break;
            }
        if (ch < 0 && onechan.length() &&
                (isdigit(onechan[0]) || onechan[0] == '-'))
            ch = atoi (onechan.c_str());  // numeric channel index
        if (ch < 0 && onechan.length()) {
            // Look for Either =val or name=val
            size_t equal_pos = onechan.find ('=');
            if (equal_pos != std::string::npos) {
                value = (float) atof (onechan.c_str()+equal_pos+1);
                onechan.erase (equal_pos);
                newchannelnames.back() = onechan;
            }
        }
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
    Timer timer (ot.enable_function_timing);

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
        std::vector<std::string> newchannelnames;
        std::vector<int> channels;
        std::vector<float> values;
        bool ok = decode_channel_set (*A->spec(s,0), chanlist,
                                      newchannelnames, channels, values);
        if (! ok) {
            ot.error (argv[0], Strutil::format("Invalid or unknown channel selection \"%s\"", chanlist));
            ot.push (A);
            return 0;
        }
        int miplevels = ot.allsubimages ? A->miplevels(s) : 1;
        allmiplevels.push_back (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageSpec spec = *A->spec(s,m);
            spec.nchannels = (int)newchannelnames.size();
            spec.channelformats.clear();
            spec.default_channel_names ();
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
        std::vector<std::string> newchannelnames;
        std::vector<int> channels;
        std::vector<float> values;
        decode_channel_set (*A->spec(s,0), chanlist, newchannelnames,
                            channels, values);
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            // Shuffle the indexed/named channels
            bool ok = ImageBufAlgo::channels ((*R)(s,m), (*A)(s,m),
                                      (int)channels.size(), &channels[0],
                                      &values[0], &newchannelnames[0], false);
            if (! ok)
                ot.error ("channels", (*R)(s,m).geterror());
            // Tricky subtlety: IBA::channels changed the underlying IB,
            // we may need to update the IRR's copy of the spec.
            R->update_spec_from_imagebuf(s,m);
        }
    }

    ot.function_times["channels"] += timer();
    return 0;
}



static int
action_chappend (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_chappend, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);

    std::vector<int> allmiplevels;
    for (int s = 0, subimages = ot.allsubimages ? A->subimages() : 1;
         s < subimages;  ++s) {
        int miplevels = ot.allsubimages ? A->miplevels(s) : 1;
        allmiplevels.push_back (miplevels);
    }

    // Create the replacement ImageRec
    ImageRecRef R (new ImageRec(A->name(), (int)allmiplevels.size(),
                                &allmiplevels[0]));
    ot.push (R);

    // Subimage by subimage, MIP level by MIP level, channel_append the
    // two images.
    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            // Shuffle the indexed/named channels
            bool ok = ImageBufAlgo::channel_append ((*R)(s,m), (*A)(s,m), (*B)(s,m));
            if (! ok)
                ot.error ("chappend", (*R)(s,m).geterror());
            // Tricky subtlety: IBA::channels changed the underlying IB,
            // we may need to update the IRR's copy of the spec.
            R->update_spec_from_imagebuf(s,m);
        }
    }
    ot.function_times["chappend"] += timer();
    return 0;
}



static int
action_selectmip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_selectmip, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --selectmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, atoi(argv[1]), true, true));
    ot.curimg = newimg;
    ot.function_times["selectmip"] += timer();
    return 0;
}



static int
action_select_subimage (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_select_subimage, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    if (ot.curimg->subimages() == 1)
        return 0;    // --subimage on a single-image file is a no-op
    
    int subimage = std::min (atoi(argv[1]), ot.curimg->subimages());
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, subimage));
    ot.function_times["subimage"] += timer();
    return 0;
}



static int
action_colorcount (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_colorcount, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageBuf &Aib ((*ot.curimg)(0,0));
    int nchannels = Aib.nchannels();

    // We assume ';' to split, but for the sake of some command shells,
    // that use ';' as a command separator, also accept ":".
    std::vector<float> colorvalues;
    std::vector<std::string> colorstrings;
    if (strchr (argv[1], ':'))
        Strutil::split (argv[1], colorstrings, ":");
    else
        Strutil::split (argv[1], colorstrings, ";");
    int ncolors = (int) colorstrings.size();
    for (int col = 0; col < ncolors; ++col) {
        std::vector<float> color (nchannels, 0.0f);
        Strutil::extract_from_list_string (color, colorstrings[col], ",");
        for (int c = 0;  c < nchannels;  ++c)
            colorvalues.push_back (c < (int)color.size() ? color[c] : 0.0f);
    }

    std::vector<float> eps (nchannels, 0.001f);
    std::map<std::string,std::string> options;
    extract_options (options, argv[0]);
    Strutil::extract_from_list_string (eps, options["eps"]);

    imagesize_t *count = ALLOCA (imagesize_t, ncolors);
    bool ok = ImageBufAlgo::color_count ((*ot.curimg)(0,0), count,
                                         ncolors, &colorvalues[0], &eps[0]);
    if (ok) {
        for (int col = 0;  col < ncolors;  ++col)
            std::cout << Strutil::format("%8d  %s\n", count[col], colorstrings[col]);
    } else {
        ot.error ("colorcount", (*ot.curimg)(0,0).geterror());
    }

    ot.function_times["colorcount"] += timer();
    return 0;
}



static int
action_rangecheck (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_rangecheck, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageBuf &Aib ((*ot.curimg)(0,0));
    int nchannels = Aib.nchannels();

    std::vector<float> low(nchannels,0.0f), high(nchannels,1.0f);
    Strutil::extract_from_list_string (low, argv[1], ",");
    Strutil::extract_from_list_string (high, argv[2], ",");

    imagesize_t lowcount = 0, highcount = 0, inrangecount = 0;
    bool ok = ImageBufAlgo::color_range_check ((*ot.curimg)(0,0), &lowcount,
                                               &highcount, &inrangecount,
                                               &low[0], &high[0]);
    if (ok) {
        std::cout << Strutil::format("%8d  < %s\n", lowcount, argv[1]);
        std::cout << Strutil::format("%8d  > %s\n", highcount, argv[2]);
        std::cout << Strutil::format("%8d  within range\n", inrangecount);
    } else {
        ot.error ("rangecheck", (*ot.curimg)(0,0).geterror());
    }

    ot.function_times["rangecheck"] += timer();
    return 0;
}



static int
action_diff (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_diff, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    int ret = do_action_diff (*ot.image_stack.back(), *ot.curimg, ot);
    if (ret != DiffErrOK && ret != DiffErrWarn)
        ot.return_value = EXIT_FAILURE;

    if (ret != DiffErrOK && ret != DiffErrWarn && ret != DiffErrFail)
        ot.error ("Error doing --diff");

    ot.function_times["diff"] += timer();
    return 0;
}



static int
action_add (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_add, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ImageRecRef R (new ImageRec (*A, *B, ot.allsubimages ? -1 : 0,
                                 ImageRec::WinMergeUnion,
                                 ImageRec::WinMergeUnion, TypeDesc::FLOAT));
    ot.push (R);

    int subimages = R->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        const ImageBuf &Aib ((*A)(s));
        const ImageBuf &Bib ((*B)(s));
        bool ok = ImageBufAlgo::add (Rib, Aib, Bib);
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    }
             
    ot.function_times["add"] += timer();
    return 0;
}



static int
action_sub (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_sub, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ImageRecRef R (new ImageRec (*A, *B, ot.allsubimages ? -1 : 0,
                                 ImageRec::WinMergeUnion,
                                 ImageRec::WinMergeUnion, TypeDesc::FLOAT));
    ot.push (R);

    int subimages = R->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        const ImageBuf &Aib ((*A)(s));
        const ImageBuf &Bib ((*B)(s));
        bool ok = ImageBufAlgo::sub (Rib, Aib, Bib);
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    }
             
    ot.function_times["sub"] += timer();
    return 0;
}



static int
action_mul (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_mul, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.read (B);
    ImageRecRef R (new ImageRec (*A, *B, ot.allsubimages ? -1 : 0,
                                 ImageRec::WinMergeUnion,
                                 ImageRec::WinMergeUnion, TypeDesc::FLOAT));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        const ImageBuf &Aib ((*A)(s));
        const ImageBuf &Bib ((*B)(s));
        bool ok = ImageBufAlgo::mul (Rib, Aib, Bib);
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    }
             
    ot.function_times["mul"] += timer();
    return 0;
}



static int
action_abs (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_abs, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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
             
    ot.function_times["abs"] += timer();
    return 0;
}



static int
action_cmul (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_cmul, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::vector<std::string> scalestrings;
    Strutil::split (std::string(argv[1]), scalestrings, ",");
    if (scalestrings.size() < 1)
        return 0;   // Implicit multiplication by 1 if we can't figure it out

    ImageRecRef A = ot.pop();
    A->read ();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    std::vector<float> scale;
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int nchans = R->spec(s,0)->nchannels;
        scale.clear ();
        scale.resize (nchans, (float) atof(scalestrings[0].c_str()));
        if (scalestrings.size() > 1) {
            for (int c = 0;  c < nchans;  ++c) {
                if (c < (int)scalestrings.size())
                    scale[c] = (float) atof(scalestrings[c].c_str());
                else
                    scale[c] = 1.0f;
            }
        }
        for (int m = 0, miplevels = ot.curimg->miplevels(s); m < miplevels; ++m) {
            bool ok = ImageBufAlgo::mul ((*R)(s,m), &scale[0]);
            if (! ok)
                ot.error ("cmul", (*R)(s,m).geterror());
        }
    }

    ot.function_times["cmul"] += timer();
    return 0;
}



static int
action_cadd (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_cadd, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::vector<std::string> addstrings;
    Strutil::split (std::string(argv[1]), addstrings, ",");
    if (addstrings.size() < 1)
        return 0;   // Implicit addition by 0 if we can't figure it out

    ImageRecRef A = ot.pop();
    A->read ();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    std::vector<float> val;
    int subimages = ot.curimg->subimages();
    for (int s = 0;  s < subimages;  ++s) {
        int nchans = R->spec(s,0)->nchannels;
        val.clear ();
        val.resize (nchans, (float) atof(addstrings[0].c_str()));
        if (addstrings.size() > 1) {
            for (int c = 0;  c < nchans;  ++c) {
                if (c < (int)addstrings.size())
                    val[c] = (float) atof(addstrings[c].c_str());
                else
                    val[c] = 0.0f;
            }
        }
        for (int m = 0, miplevels = ot.curimg->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::add ((*R)(s,m), &val[0]);
            if (! ok)
                ot.error ("cadd", (*R)(s,m).geterror());
        }
    }

    ot.function_times["cadd"] += timer();
    return 0;
}



static int
action_chsum (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_chsum, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A (ot.pop());
    ot.read (A);
    ImageRecRef R (new ImageRec ("chsum", ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        std::vector<float> weight ((*A)(s).nchannels(), 1.0f);
        std::map<std::string,std::string> options;
        extract_options (options, argv[0]);
        Strutil::extract_from_list_string (weight, options["weight"]);

        ImageBuf &Rib ((*R)(s));
        const ImageBuf &Aib ((*A)(s));
        bool ok = ImageBufAlgo::channel_sum (Rib, Aib, &weight[0]);
        if (! ok)
            ot.error ("chsum", Rib.geterror());
        R->update_spec_from_imagebuf (s);
    }

    ot.function_times["chsum"] += timer();
    return 0;
}




static int
action_flip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flip, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0, true, false));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s)
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::flip ((*R)(s,m), (*A)(s,m));
            if (! ok)
                ot.error ("flip", (*R)(s,m).geterror());
        }
    ot.function_times["flip"] += timer();             
    return 0;
}



static int
action_flop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flop, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0, true, false));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s)
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::flop ((*R)(s,m), (*A)(s,m));
            if (! ok)
                ot.error ("flop", (*R)(s,m).geterror());
        }

    ot.function_times["flop"] += timer();
    return 0;
}



static int
action_flipflop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_flipflop, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0, true, false));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s)
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::flipflop ((*R)(s,m), (*A)(s,m));
            if (! ok)
                ot.error ("flipflop", (*R)(s,m).geterror());
        }

    ot.function_times["flipflop"] += timer();
    return 0;
}



static int
action_transpose (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_transpose, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A (ot.pop());
    ot.read (A);

    ImageRecRef R (new ImageRec ("transpose",
                                 ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        bool ok = ImageBufAlgo::transpose ((*R)(s), (*A)(s));
        if (! ok)
            ot.error ("transpose", (*R)(s).geterror());
        R->update_spec_from_imagebuf (s);
    }

    ot.function_times["transpose"] += timer();
    return 0;
}



static int
action_cshift (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_cshift, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    int x = 0, y = 0, z = 0;
    if (sscanf (argv[1], "%d%d%d", &x, &y, &z) < 2) {
        ot.error ("cshift", Strutil::format ("Invalid shift offset '%s'", argv[1]));
        return 0;
    }

    ImageRecRef A (ot.pop());
    ot.read (A);

    ImageRecRef R (new ImageRec ("cshift",
                                 ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        bool ok = ImageBufAlgo::circular_shift ((*R)(s), (*A)(s), x, y, z); 
        if (! ok)
            ot.error ("cshift", (*R)(s).geterror());
        R->update_spec_from_imagebuf (s);
    }

    ot.function_times["cshift"] += timer();
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
action_swap (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    if (ot.image_stack.size() < 1) {
        ot.error (argv[0], "requires at least two loaded images");
        return 0;
    }
    ImageRecRef B (ot.pop());
    ImageRecRef A (ot.pop());
    ot.push (B);
    ot.push (A);
    return 0;
}


static int
action_create (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    Timer timer (ot.enable_function_timing);
    int nchans = atoi (argv[2]);
    if (nchans < 1 || nchans > 1024) {
        std::cout << "Invalid number of channels: " << nchans << "\n";
        nchans = 3;
    }
    ImageSpec spec (64, 64, nchans, TypeDesc::FLOAT);
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
    ot.function_times["create"] += timer();
    return 0;
}



static int
action_pattern (int argc, const char *argv[])
{
    ASSERT (argc == 4);
    Timer timer (ot.enable_function_timing);
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
        std::vector<float> fill (nchans, 1.0f);
        std::map<std::string,std::string> options;
        extract_options (options, pattern);
        Strutil::extract_from_list_string (fill, options["color"]);
        bool ok = ImageBufAlgo::fill (ib, &fill[0]);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    } else if (Strutil::istarts_with(pattern,"checker")) {
        std::map<std::string,std::string> options;
        options["width"] = "8";
        options["height"] = "8";
        options["depth"] = "8";
        extract_options (options, pattern);
        int width = Strutil::from_string<int> (options["width"]);
        int height = Strutil::from_string<int> (options["height"]);
        int depth = Strutil::from_string<int> (options["depth"]);
        std::vector<float> color1 (nchans, 0.0f);
        std::vector<float> color2 (nchans, 1.0f);
        Strutil::extract_from_list_string (color1, options["color1"]);
        Strutil::extract_from_list_string (color2, options["color2"]);
        bool ok = ImageBufAlgo::checker (ib, width, height, depth,
                                         &color1[0], &color2[0], 0, 0, 0);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    } else {
        bool ok = ImageBufAlgo::zero (ib);
        if (! ok)
            ot.error (argv[0], ib.geterror());
    }
    ot.push (img);
    ot.function_times["pattern"] += timer();
    return 0;
}



static int
action_kernel (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    Timer timer (ot.enable_function_timing);
    int nchans = 1;
    if (nchans < 1 || nchans > 1024) {
        std::cout << "Invalid number of channels: " << nchans << "\n";
        nchans = 3;
    }

    float w = 1.0f, h = 1.0f;
    if (sscanf (argv[2], "%fx%f", &w, &h) != 2)
        ot.error ("kernel", Strutil::format ("Unknown size %s", argv[2]));

    ImageSpec spec (1, 1, nchans, TypeDesc::FLOAT);
    ImageRecRef img (new ImageRec ("kernel", spec, ot.imagecache));
    ImageBuf &ib ((*img)());
    int ok = ImageBufAlgo::make_kernel (ib, argv[1], w, h);
    if (! ok)
        ot.error (argv[0], ib.geterror());
    img->update_spec_from_imagebuf (0, 0);

    ot.push (img);
    ot.function_times["kernel"] += timer();
    return 0;
}



static int
action_capture (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    Timer timer (ot.enable_function_timing);
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
    ot.function_times["capture"] += timer();
    return 0;
}



static int
action_crop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_crop, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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
        bool ok = ImageBufAlgo::crop (Rib, Aib, get_roi(newspec));
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    } else if (newspec.x != Aspec.x || newspec.y != Aspec.y) {
        // only offset changed; don't copy the image or crop, simply
        // adjust the origins.
        Aspec.x = newspec.x;
        Aspec.y = newspec.y;
        A->metadata_modified (true);
    }

    ot.function_times["crop"] += timer();
    return 0;
}



int
action_croptofull (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_croptofull, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.curimg;
    const ImageSpec &Aspec (*A->spec(0,0));
    // Implement by calling action_crop with a geometry specifier built
    // from the current full image size.
    std::string size = format_resolution (Aspec.full_width, Aspec.full_height,
                                          Aspec.full_x, Aspec.full_y);
    const char *newargv[2] = { "crop", size.c_str() };
    bool old_enable_function_timing = ot.enable_function_timing;
    ot.enable_function_timing = false;
    int result = action_crop (2, newargv);
    ot.function_times["croptofull"] += timer();
    ot.enable_function_timing = old_enable_function_timing;
    return result;
}



static int
action_resample (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_resample, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ot.read ();
    ImageRecRef A = ot.pop();
    const ImageSpec &Aspec (*A->spec(0,0));
    ImageSpec newspec = Aspec;

    adjust_geometry (newspec.width, newspec.height,
                     newspec.x, newspec.y, argv[1], true);
    if (newspec.width == Aspec.width && newspec.height == Aspec.height) {
        ot.push (A);  // Restore the original image
        return 0;  // nothing to do
    }

    // Shrink-wrap full to match actual pixels; I'm not sure what else
    // is appropriate, need to think it over.
    newspec.full_x = newspec.x;
    newspec.full_y = newspec.y;
    newspec.full_width = newspec.width;
    newspec.full_height = newspec.height;

    ot.push (new ImageRec (A->name(), newspec, ot.imagecache));

    const ImageBuf &Aib ((*A)(0,0));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    bool ok = ImageBufAlgo::resample (Rib, Aib);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    ot.function_times["resample"] += timer();
    return 0;
}



static int
action_resize (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_resize, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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
    if (newspec.width == Aspec.width && newspec.height == Aspec.height) {
        ot.push (A);  // Restore the original image
        return 0;  // nothing to do
    }

    // Shrink-wrap full to match actual pixels; I'm not sure what else
    // is appropriate, need to think it over.
    newspec.full_x = newspec.x;
    newspec.full_y = newspec.y;
    newspec.full_width = newspec.width;
    newspec.full_height = newspec.height;

    ot.push (new ImageRec (A->name(), newspec, ot.imagecache));

    // Resize ratio
    float wratio = float(newspec.full_width) / float(Aspec.full_width);
    float hratio = float(newspec.full_height) / float(Aspec.full_height);

    if (filtername.empty()) {
        // No filter name supplied -- pick a good default
        if (wratio > 1.0f || hratio > 1.0f)
            filtername = "blackman-harris";
        else
            filtername = "lanczos3";
    }
    Filter2D *filter = NULL;
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc fd;
        Filter2D::get_filterdesc (i, &fd);
        if (fd.name == filtername) {
            float w = fd.width * std::max (1.0f, wratio);
            float h = fd.width * std::max (1.0f, hratio);
            filter = Filter2D::create (filtername, w, h);
            break;
        }
    }
    if (! filter) {
        ot.error (argv[0], Strutil::format("Filter \"%s\" not recognized",
                                           filtername));
        return false;
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
    bool ok = ImageBufAlgo::resize (Rib, Aib, filter, get_roi(Rib.spec()));
    if (filter)
        Filter2D::destroy (filter);
    ot.function_times["resize"] += timer();
    if (! ok)
        ot.error (argv[0], Rib.geterror());
    return 0;
}



static int
action_fit (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fit, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    bool old_enable_function_timing = ot.enable_function_timing;
    ot.enable_function_timing = false;

    // Examine the top of stack
    ImageRecRef A = ot.top();
    ot.read ();
    const ImageSpec *Aspec = A->spec(0,0);

    // Parse the user request for resolution to fit
    int fit_full_width = Aspec->full_width;
    int fit_full_height = Aspec->full_height;
    int fit_full_x = Aspec->full_x;
    int fit_full_y = Aspec->full_y;
    adjust_geometry (fit_full_width, fit_full_height, fit_full_x, fit_full_y,
                     argv[1], false);

    std::map<std::string,std::string> options;
    extract_options (options, argv[0]);
    std::string padopt = options["pad"];
    bool pad = padopt.size() && atoi(padopt.c_str());
    std::string filtername = options["filter"];
    
    // Compute scaling factors and use action_resize to do the heavy lifting
    float oldaspect = float(Aspec->full_width) / Aspec->full_height;
    float newaspect = float(fit_full_width) / fit_full_height;
    int resize_full_width = fit_full_width;
    int resize_full_height = fit_full_height;
    int xoffset = 0, yoffset = 0;

    if (newaspect >= oldaspect) {  // same or wider than original
        resize_full_width = int(resize_full_height * oldaspect + 0.5f);
        xoffset = (fit_full_width - resize_full_width) / 2;
    } else {  // narrower than original
        resize_full_height = int(resize_full_width / oldaspect + 0.5f);
        yoffset = (fit_full_height - resize_full_height) / 2;
    }

    if (ot.verbose) {
        std::cout << "Fitting " 
                  << format_resolution(Aspec->full_width, Aspec->full_height,
                                       Aspec->full_x, Aspec->full_y)
                  << " into "
                  << format_resolution(fit_full_width, fit_full_height,
                                       fit_full_x, fit_full_y) 
                  << "\n";
        std::cout << "  Resizing to "
                  << format_resolution(resize_full_width, resize_full_height,
                                       fit_full_x, fit_full_y) << "\n";
    }
    if (resize_full_width != Aspec->full_width ||
        resize_full_height != Aspec->full_height ||
        fit_full_x != Aspec->full_x || fit_full_y != Aspec->full_y) {
        std::string resize = format_resolution (resize_full_width,
                                                resize_full_height,
                                                0, 0);
        std::string command = "resize";
        if (filtername.size())
            command += Strutil::format (":filter=%s", filtername);
        const char *newargv[2] = { command.c_str(), resize.c_str() };
        action_resize (2, newargv);
        A = ot.top ();
        Aspec = A->spec(0,0);
        A->spec(0,0)->full_width = fit_full_width;
        A->spec(0,0)->full_height = fit_full_height;
        A->spec(0,0)->x = xoffset;
        A->spec(0,0)->y = yoffset;
        // Now A,Aspec are for the NEW resized top of stack
    }

    if (pad && (fit_full_width != Aspec->width ||
                fit_full_height != Aspec->height)) {
        // Needs padding
        ImageSpec newspec = *Aspec;
        newspec.width = newspec.full_width = fit_full_width;
        newspec.height = newspec.full_height = fit_full_height;
        newspec.x = newspec.full_x = fit_full_x;
        newspec.y = newspec.full_y = fit_full_y;
        newspec.set_format (TypeDesc::FLOAT);
        ImageRecRef B (new ImageRec (A->name(), newspec, ot.imagecache));
        ImageBuf &Rib ((*B)(0,0));
        ot.curimg = B;
        ImageBufAlgo::zero (Rib);
        bool ok;
        if (Aspec->full_width == fit_full_width)
            ok = ImageBufAlgo::paste (Rib, 0, (fit_full_height-Aspec->full_height)/2,
                                      0, 0, (*A)(0,0));
        else
            ok = ImageBufAlgo::paste (Rib, (fit_full_width-Aspec->full_height)/2, 0,
                                      0, 0, (*A)(0,0));
        if (! ok)
            ot.error (argv[0], Rib.geterror());
    }

    ot.function_times["fit"] += timer();
    ot.enable_function_timing = old_enable_function_timing;
    return 0;
}



static int
action_convolve (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_convolve, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef K = ot.pop();  // kernel
    ImageRecRef A = ot.pop();
    A->read();
    K->read();

    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0, 0,
                                 true /*writable*/, false /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        bool ok = ImageBufAlgo::convolve (Rib, (*A)(s), (*K)(0));
        if (! ok)
            ot.error ("convolve", Rib.geterror());
    }

    ot.function_times["convolve"] += timer();
    return 0;
}



static int
action_blur (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_blur, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::map<std::string,std::string> options;
    options["kernel"] = "gaussian";
    extract_options (options, argv[0]);
    std::string kernopt = options["kernel"];

    float w = 1.0f, h = 1.0f;
    if (sscanf (argv[1], "%fx%f", &w, &h) != 2)
        ot.error ("blur", Strutil::format ("Unknown size %s", argv[1]));
    ImageBuf Kernel ("kernel");
    if (! ImageBufAlgo::make_kernel (Kernel, kernopt.c_str(), w, h))
        ot.error ("blur", Kernel.geterror());

    ImageRecRef A = ot.pop();
    A->read();

    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0, 0,
                                 true /*writable*/, false /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        bool ok = ImageBufAlgo::convolve (Rib, (*A)(s), Kernel);
        if (! ok)
            ot.error ("blur", Rib.geterror());
    }

    ot.function_times["blur"] += timer();
    return 0;
}



static int
action_unsharp (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_unsharp, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::map<std::string,std::string> options;
    options["kernel"] = "gaussian";
    options["width"] = "3";
    options["contrast"] = "1";
    options["threshold"] = "0";
    extract_options (options, argv[0]);
    std::string kernel = options["kernel"];
    float width = (float) strtod (options["width"].c_str(), NULL);
    float contrast = (float) strtod (options["contrast"].c_str(), NULL);
    float threshold = (float) strtod (options["threshold"].c_str(), NULL);

    ImageRecRef A = ot.pop();
    A->read();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0, 0,
                                 true /*writable*/, false /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        bool ok = ImageBufAlgo::unsharp_mask (Rib, (*A)(s), kernel.c_str(),
                                              width, contrast, threshold);
        if (! ok)
            ot.error ("unsharp", Rib.geterror());
    }

    ot.function_times["unsharp"] += timer();
    return 0;
}



static int
action_fft (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fft, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A = ot.pop();
    A->read();
    ImageRecRef R (new ImageRec ("fft", ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        bool ok = ImageBufAlgo::fft (Rib, (*A)(s));
        R->update_spec_from_imagebuf (s);
        if (! ok)
            ot.error ("fft", Rib.geterror());
    }

    ot.function_times["fft"] += timer();
    return 0;
}



static int
action_ifft (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_ifft, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A = ot.pop();
    A->read();
    ImageRecRef R (new ImageRec ("ifft", ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        ImageBuf &Rib ((*R)(s));
        bool ok = ImageBufAlgo::ifft (Rib, (*A)(s));
        R->update_spec_from_imagebuf (s);
        if (! ok)
            ot.error ("ifft", Rib.geterror());
    }

    ot.function_times["ifft"] += timer();
    return 0;
}



int
action_fixnan (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fixnan, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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
             
    ot.function_times["fixnan"] += timer();
    return 0;
}



static int
action_fillholes (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fillholes, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ImageSpec spec = (*A)(0,0).spec();
    set_roi (spec, roi_union (get_roi(spec), get_roi_full(spec)));
    ImageRecRef B (new ImageRec("filled", spec, ot.imagecache));
    ot.push (B);
    ImageBuf &Rib ((*B)(0,0));
    bool ok = ImageBufAlgo::fillholes_pushpull (Rib, (*A)(0,0));
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    ot.function_times["fillholes"] += timer();
    return 0;
}



static int
action_paste (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_paste, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef BG (ot.pop());
    ImageRecRef FG (ot.pop());
    ot.read (BG);
    ot.read (FG);

    int x = 0, y = 0;
    if (sscanf (argv[1], "%d%d", &x, &y) != 2) {
        ot.error ("paste", Strutil::format ("Invalid offset '%s'", argv[1]));
        return 0;
    }

    ImageRecRef R (new ImageRec (*BG, 0, 0, true /* writable*/, true /* copy */));
    ot.push (R);

    bool ok = ImageBufAlgo::paste ((*R)(), x, y, 0, 0, (*FG)());
    if (! ok)
        ot.error (argv[0], (*R)().geterror());
    ot.function_times["paste"] += timer();
    return 0;
}



static int
action_mosaic (int argc, const char *argv[])
{
    // Mosaic is tricky. We have to parse the argument before we know
    // how many images it wants to pull off the stack.
    int ximages = 0, yimages = 0;
    if (sscanf (argv[1], "%dx%d", &ximages, &yimages) != 2 
          || ximages < 1 || yimages < 1) {
        ot.error ("mosaic", Strutil::format ("Invalid size '%s'", argv[1]));
        return 0;
    }
    int nimages = ximages * yimages;

    if (ot.postpone_callback (nimages, action_paste, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    int widest = 0, highest = 0, nchannels = 0;
    std::vector<ImageRecRef> images (nimages);
    for (int i = nimages-1;  i >= 0;  --i) {
        ImageRecRef img = ot.pop();
        images[i] = img;
        ot.read (img);
        widest = std::max (widest, img->spec()->full_width);
        highest = std::max (highest, img->spec()->full_height);
        nchannels = std::max (nchannels, img->spec()->nchannels);
    }

    std::map<std::string,std::string> options;
    options["pad"] = "0";
    extract_options (options, argv[0]);
    int pad = strtol (options["pad"].c_str(), NULL, 10);

    ImageSpec Rspec (ximages*widest + (ximages-1)*pad,
                     yimages*highest + (yimages-1)*pad,
                     nchannels, TypeDesc::FLOAT);
    ImageRecRef R (new ImageRec ("mosaic", Rspec, ot.imagecache));
    ot.push (R);

    ImageBufAlgo::zero ((*R)());
    for (int j = 0;  j < yimages;  ++j) {
        int y = j * (highest + pad);
        for (int i = 0;  i < ximages;  ++i) {
            int x = i * (widest + pad);
            bool ok = ImageBufAlgo::paste ((*R)(), x, y, 0, 0,
                                           (*images[j*ximages+i])(0));
            if (! ok)
                ot.error (argv[0], (*R)().geterror());
        }
    }

    ot.function_times["mosaic"] += timer();
    return 0;
}



static int
action_over (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_over, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

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

    ot.push (new ImageRec ("over", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());

    bool ok = ImageBufAlgo::over (Rib, Aib, Bib);
    if (! ok)
        ot.error (argv[0], Rib.geterror());
    ot.function_times["over"] += timer();
    return 0;
}



static int
action_zover (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_zover, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    // Get optional flags
    bool z_zeroisinf = false;
    std::string cmd = argv[0];
    size_t pos;
    while ((pos = cmd.find_first_of(":")) != std::string::npos) {
        cmd = cmd.substr (pos+1, std::string::npos);
        if (Strutil::istarts_with(cmd,"zeroisinf="))
            z_zeroisinf = (atoi(cmd.c_str()+10) != 0);
    }

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

    ot.push (new ImageRec ("zover", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());

    bool ok = ImageBufAlgo::zover (Rib, Aib, Bib, z_zeroisinf);
    if (! ok)
        ot.error (argv[0], Rib.geterror());
    ot.function_times["zover"] += timer();
    return 0;
}



static int
action_fill (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fill, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.push (new ImageRec (*A, 0, 0, true, true /*copy_pixels*/));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    const ImageSpec &Rspec = Rib.spec();

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
                color[c] = (float) atof (command.c_str()+numpos);
                while (numpos < command.size() && command[numpos] != ':' && command[numpos] != ',')
                    ++numpos;
                if (numpos < command.size())
                    ++numpos;
            }
        }
    }

    bool ok = ImageBufAlgo::fill (Rib, color, ROI(x, x+w, y, y+h));
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    ot.function_times["fill"] += timer();
    return 0;
}



static int
action_clamp (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_clamp, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    ImageRecRef A = ot.pop();
    A->read ();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writeable*/, true /*copy_pixels*/));
    ot.push (R);
    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        int nchans = (*R)(s,0).nchannels();
        const float big = std::numeric_limits<float>::max();
        std::vector<float> min (nchans, -big);
        std::vector<float> max (nchans, big);
        std::map<std::string,std::string> options;
        options["clampalpha"] = "0";  // initialize
        extract_options (options, argv[0]);
        Strutil::extract_from_list_string (min, options["min"]);
        Strutil::extract_from_list_string (max, options["max"]);
        bool clampalpha01 = strtol (options["clampalpha"].c_str(), NULL, 10) != 0;

        for (int m = 0, miplevels=R->miplevels(s);  m < miplevels;  ++m) {
            ImageBuf &Rib ((*R)(s,m));
            bool ok = ImageBufAlgo::clamp (Rib, &min[0], &max[0], clampalpha01);
            if (! ok)
                ot.error (argv[0], Rib.geterror());
        }
    }

    ot.function_times["clamp"] += timer();
    return 0;
}



static int
action_rangecompress (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_rangecompress, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::vector<std::string> addstrings;
    Strutil::split (std::string(argv[1]), addstrings, ",");
    if (addstrings.size() < 1)
        return 0;   // Implicit addition by 0 if we can't figure it out

    std::map<std::string,std::string> options;
    extract_options (options, argv[0]);
    std::string useluma_str = options["luma"];
    bool useluma = useluma_str.size() ? atoi(useluma_str.c_str()) != 0 : true;

    ImageRecRef A = ot.pop();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::rangecompress ((*R)(s,m), useluma);
            if (! ok)
                ot.error (argv[0], (*R)(s,m).geterror());
        }
    }

    ot.function_times["rangecompress"] += timer();
    return 0;
}



static int
action_rangeexpand (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_rangeexpand, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    std::vector<std::string> addstrings;
    Strutil::split (std::string(argv[1]), addstrings, ",");
    if (addstrings.size() < 1)
        return 0;   // Implicit addition by 0 if we can't figure it out

    std::map<std::string,std::string> options;
    extract_options (options, argv[0]);
    std::string useluma_str = options["luma"];
    bool useluma = useluma_str.size() ? atoi(useluma_str.c_str()) != 0 : true;

    ImageRecRef A = ot.pop();
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writable*/, true /*copy_pixels*/));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        for (int m = 0, miplevels = R->miplevels(s);  m < miplevels;  ++m) {
            bool ok = ImageBufAlgo::rangeexpand ((*R)(s,m), useluma);
            if (! ok)
                ot.error (argv[0], (*R)(s,m).geterror());
        }
    }

    ot.function_times["rangeexpand"] += timer();
    return 0;
}



static int
action_text (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_text, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.push (new ImageRec (*A, 0, 0, true, true /*copy_pixels*/));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    const ImageSpec &Rspec = Rib.spec();

    // Set up defaults for text placement, size, font, color
    std::map<std::string,std::string> options;
    extract_options (options, argv[0]);
    int x = options["x"].size() ? Strutil::from_string<int>(options["x"]) : (Rspec.x + Rspec.width/2);
    int y = options["y"].size() ? Strutil::from_string<int>(options["y"]) : (Rspec.y + Rspec.height/2);
    int fontsize = options["size"].size() ? Strutil::from_string<int>(options["size"]) : 16;
    std::string font = options["font"];
    std::vector<float> textcolor (Rspec.nchannels, 1.0f);
    Strutil::extract_from_list_string (textcolor, options["color"]);

    bool ok = ImageBufAlgo::render_text (Rib, x, y, argv[1] /* the text */,
                                         fontsize, font, &textcolor[0]);
    if (! ok)
        ot.error (argv[0], Rib.geterror());

    ot.function_times["text"] += timer();
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
    Timer timer (ot.enable_function_timing);

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

    ot.function_times["histogram"] += timer();
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap (argc, (const char **)argv);
    ot.full_command_line = ap.command_line();
    ap.options ("oiiotool -- simple image processing operations\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  oiiotool [filename,option,action]...\n",
                "%*", input_file, "",
                "<SEPARATOR>", "Options (general):",
                "--help", &help, "Print help message",
                "-v", &ot.verbose, "Verbose status messages",
                "-q %!", &ot.verbose, "Quiet mode (turn verbose off)",
                "--runstats", &ot.runstats, "Print runtime statistics",
                "-a", &ot.allsubimages, "Do operations on all subimages/miplevels",
                "--info", &ot.printinfo, "Print resolution and metadata on all inputs",
                "--metamatch %s", &ot.printinfo_metamatch,
                    "Regex: which metadata is printed with -info -v",
                "--no-metamatch %s", &ot.printinfo_nometamatch,
                    "Regex: which metadata is excluded with -info -v",
                "--stats", &ot.printstats, "Print pixel statistics on all inputs",
                "--hash", &ot.hash, "Print SHA-1 hash of each input image",
                "--colorcount %@ %s", action_colorcount, NULL,
                    "Count of how many pixels have the given color (argument: color;color;...) (optional args: eps=color)",
                "--rangecheck %@ %s %s", action_rangecheck, NULL, NULL,
                    "Count of how many pixels are outside the low and high color arguments (each is a comma-separated color value list)",
//                "-u", &ot.updatemode, "Update mode: skip outputs when the file exists and is newer than all inputs",
                "--no-clobber", &ot.noclobber, "Do not overwrite existing files",
                "--noclobber", &ot.noclobber, "", // synonym
                "--threads %@ %d", set_threads, &ot.threads, "Number of threads (default 0 == #cores)",
                "--frames %s", NULL, "Frame range for '#' wildcards",
                "--framepadding %d", NULL, "Frame number padding digits",
                "<SEPARATOR>", "Commands that write images:",
                "-o %@ %s", output_file, NULL, "Output the current image to the named file",
                "<SEPARATOR>", "Options that affect subsequent image output:",
                "-d %@ %s", set_dataformat, NULL,
                    "'-d TYPE' sets the output data format of all channels, "
                    "'-d CHAN=TYPE' overrides a single named channel (multiple -d args are allowed). "
                    "Data types include: uint8, sint8, uint10, uint12, uint16, sint16, half, float, double",
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
                "--chnames %@ %s", set_channelnames, NULL,
                    "Set the channel names (comma-separated)",
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
                "--kernel %@ %s %s", action_kernel, NULL, NULL,
                        "Create a centered convolution kernel (args: name, geom)",
                "--capture %@", action_capture, NULL,
                        "Capture an image (options: camera=%d)",
                "--diff %@", action_diff, NULL, "Print report on the difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--add %@", action_add, NULL, "Add two images",
                "--sub %@", action_sub, NULL, "Subtract two images",
                "--abs %@", action_abs, NULL, "Take the absolute value of the image pixels",
                "--mul %@", action_mul, NULL, "Multiply two images",
                "--cmul %s %@", action_cmul, NULL, "Multiply the image values by a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--cadd %s %@", action_cadd, NULL, "Add to all channels a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--chsum %@", action_chsum, NULL,
                    "Turn into 1-channel image by summing channels (options: weight=r,g,...)",
                "--paste %@ %s", action_paste, NULL, "Paste fg over bg at the given position (e.g., +100+50)",
                "--mosaic %@ %s", action_mosaic, NULL,
                        "Assemble images into a mosaic (arg: WxH; options: pad=0)",
                "--over %@", action_over, NULL, "'Over' composite of two images",
                "--zover %@", action_zover, NULL, "Depth composite two images with Z channels (options: zeroisinf=%d)",
                "--histogram %@ %s %d", action_histogram, NULL, NULL, "Histogram one channel (options: cumulative=0)",
                "--flip %@", action_flip, NULL, "Flip the image vertically (top<->bottom)",
                "--flop %@", action_flop, NULL, "Flop the image horizontally (left<->right)",
                "--flipflop %@", action_flipflop, NULL, "Flip and flop the image (180 degree rotation)",
                "--transpose %@", action_transpose, NULL, "Transpose the image",
                "--cshift %@ %s", action_cshift, NULL, "Circular shift the image (e.g.: +20-10)",
                "--crop %@ %s", action_crop, NULL, "Set pixel data resolution and offset, cropping or padding if necessary (WxH+X+Y or xmin,ymin,xmax,ymax)",
                "--croptofull %@", action_croptofull, NULL, "Crop or pad to make pixel data region match the \"full\" region",
                "--resample %@ %s", action_resample, NULL, "Resample (640x480, 50%)",
                "--resize %@ %s", action_resize, NULL, "Resize (640x480, 50%) (optional args: filter=%s)",
                "--fit %@ %s", action_fit, NULL, "Resize to fit within a window size (optional args: filter=%s, pad=%d)",
                "--convolve %@", action_convolve, NULL,
                    "Convolve with a kernel",
                "--blur %@ %s", action_blur, NULL,
                    "Blur the image (arg: WxH; options: kernel=name)",
                "--unsharp %@", action_unsharp, NULL,
                    "Unsharp mask (options: kernel=gaussian, width=3, contrast=1, threshold=0)",
                "--fft %@", action_fft, NULL,
                    "Take the FFT of the image",
                "--ifft %@", action_ifft, NULL,
                    "Take the inverse FFT of the image",
                "--fixnan %@ %s", action_fixnan, NULL, "Fix NaN/Inf values in the image (options: none, black, box3)",
                "--fillholes %@", action_fillholes, NULL,
                    "Fill in holes (where alpha is not 1)",
                "--fill %@ %s", action_fill, NULL, "Fill a region (options: color=)",
                "--clamp %@", action_clamp, NULL, "Clamp values (options: min=..., max=..., clampalpha=0)",
                "--rangecompress %@", action_rangecompress, NULL,
                    "Compress the range of pixel values > 1 with a log scale (options: luma=0|1)",
                "--rangeexpand %@", action_rangeexpand, NULL,
                    "Un-rangecompress pixel values > 1 (options: luma=0|1)",
                "--text %@ %s", action_text, NULL,
                    "Render text into the current image (options: x=, y=, size=, color=)",
                "<SEPARATOR>", "Image stack manipulation:",
                "--ch %@ %s", action_channels, NULL,
                    "Select or shuffle channels (e.g., \"R,G,B\", \"B,G,R\", \"2,3,4\")",
                "--chappend %@", action_chappend, NULL,
                    "Append the channels of the last two images",
                "--unmip %@", action_unmip, NULL, "Discard all but the top level of a MIPmap",
                "--selectmip %@ %d", action_selectmip, NULL,
                    "Select just one MIP level (0 = highest res)",
                "--subimage %@ %d", action_select_subimage, NULL, "Select just one subimage",
                "--pop %@", action_pop, NULL,
                    "Throw away the current image",
                "--dup %@", action_dup, NULL,
                    "Duplicate the current image (push a copy onto the stack)",
                "--swap %@", action_swap, NULL,
                    "Swap the top two images on the stack.",
                "<SEPARATOR>", "Color management:",
                "--iscolorspace %@ %s", set_colorspace, NULL,
                    "Set the assumed color space (without altering pixels)",
                "--tocolorspace %@ %s", action_tocolorspace, NULL,
                    "Convert the current image's pixels to a named color space",
                "--colorconvert %@ %s %s", action_colorconvert, NULL, NULL,
                    "Convert pixels from 'src' to 'dst' color space (without regard to its previous interpretation)",
                "--ociolook %@ %s", action_ociolook, NULL,
                    "Apply the named OCIO look (optional args: fromspace=, tospace=, inverse=, key=, value=)",
                "--unpremult %@", action_unpremult, NULL,
                    "Divide all color channels of the current image by the alpha to \"un-premultiply\"",
                "--premult %@", action_premult, NULL,
                    "Multiply all color channels of the current image by the alpha",
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

        int nlooks = ot.colorconfig.getNumLooks();
        if (nlooks) {
            std::stringstream s;
            s << "Known looks: ";
            for (int i = 0;  i < nlooks;  ++i) {
                const char *n = ot.colorconfig.getLookNameByIndex(i);
                s << "\"" << n << "\"";
                if (i < nlooks-1)
                    s << ", ";
            }
            std::cout << Strutil::wordwrap(s.str(), columns, 4) << "\n";
        }

        exit (EXIT_FAILURE);
    }

}



// Given a sequence description, generate numbers.
static void
generate_sequence (std::string seqdesc, int framepadding,
                   std::vector<std::string> &numbers)
{
    numbers.clear ();
    std::string format = Strutil::format ("%%0%dd", framepadding);

    // Split the sequence description into comma-separated subranges.
    std::vector<std::string> sequences;
    Strutil::split (seqdesc, sequences, ",");

    // For each subrange...
    BOOST_FOREACH (std::string s, sequences) {
        // It's START, START-END, or START-ENDxSTEP
        std::vector<std::string> range;
        Strutil::split (s, range, "-");
        int first = strtol (range[0].c_str(), NULL, 10);
        int last = first;
        int step = 1;
        if (range.size() > 1) {
            last = strtol (range[1].c_str(), NULL, 10);
            if (const char *x = strchr (range[1].c_str(), 'x'))
                step = std::max (1, (int) strtol (x+1, NULL, 10));
        }
        for (int i = first; i <= last; i += step)
            numbers.push_back (Strutil::format (format.c_str(), i));
    }
    // std::cout << "Sequence " << Strutil::join(numbers, " ") << "\n";
}



// Given a pattern (such as "foo.#.tif" or "bar.1-10#.exr"), produce a
// list of matching filenames.  Explicit ranges enumerate the range,
// whereas full numeric wildcards search for existing files.
static bool
deduce_sequence (std::string pattern, int framepadding,
                 std::vector<std::string> &filenames,
                 std::vector<std::string> &numbers)
{
    filenames.clear ();

    // Isolate the directory name (or '.' if none was specified)
    std::string directory = Filesystem::parent_path (pattern);
    if (directory.size() == 0) {
        directory = ".";
#ifdef _WIN32
        pattern = ".\\\\" + pattern;
#else
        pattern = "./" + pattern;
#endif
    }

    // The pattern is either a range (e.g., "1-15#"), or just a 
    // set of hash marks (e.g. "####").
    static boost::regex range_re ("([0-9]+)\\-([0-9]+)#+");
    static boost::regex hash_re ("#+");

    boost::match_results<std::string::const_iterator> range_match;
    if (boost::regex_search (pattern, range_match, range_re)) {
        // It's a range. Generate the names by iterating through the
        // numbers.  
        std::string prefix (range_match.prefix().first, range_match.prefix().second);
        std::string suffix (range_match.suffix().first, range_match.suffix().second);
        std::string r1 (range_match[1].first, range_match[1].second);
        std::string r2 (range_match[2].first, range_match[2].second);
        int rangefirst = (int) strtol (r1.c_str(), NULL, 10);
        int rangelast = (int) strtol (r2.c_str(), NULL, 10);
        // Only save the numbers if it's not already filled in.
        bool save_numbers = (numbers.size() == 0);

        // There are two cases: either the files exist, or they don't.
        // Check the first one and assume it's the same for all.
        for (int r = rangefirst; r <= rangelast; ++r) {
            // Try up to 4 leading zeroes
            std::string f, num;
            for (int i = 1; i <= framepadding; ++i) {
                std::string fmt = Strutil::format ("%%0%dd", i);
                std::string num = Strutil::format (fmt.c_str(), r);
                f = prefix + num + suffix;
                if (Filesystem::exists (f))
                    break;  // found it
            }
            // At this point, we either have an f that exists, or f is
            // the file with 4 digit number.
            filenames.push_back (f);
            if (save_numbers)
                numbers.push_back (f);
        }

    } else if (numbers.size()) {
        // Numeric wildcard, and an earlier argument has already
        // expanded into a specific series of numbers.  We MUST make
        // this wildcard expand to the same set of numbers.
        for (size_t i = 0; i < numbers.size(); ++i) {
            std::string f = boost::regex_replace (pattern, hash_re, numbers[i]);
            filenames.push_back (f);
        }

    } else {
        // Numeric wildcard, but we don't yet have a prescribed frame
        // range, so search the directories for matches.

        pattern = boost::regex_replace (pattern, hash_re, "([0-9]+)");
        pattern = "^" + pattern + "$";
        bool ok = Filesystem::get_directory_entries (directory, filenames,
                                                     false, pattern);
        if (! ok)
            return false;

        boost::regex pattern_re (pattern);
        for (size_t i = 0; i < filenames.size(); ++i) {
            boost::match_results<std::string::const_iterator> match;
            bool ok = boost::regex_search (filenames[i], match, pattern_re);
            ASSERT (ok);  // should have matched
            std::string num (match[1].first, match[1].second);
            numbers.push_back (num);
        }
    }


//    std::cout << "Matches: \n\t" << Strutil::join (filenames, "\n\t") << "\n";
    return true;
}



// Check if any of the command line arguments contains numeric ranges or
// wildcards.  If not, just return 'false'.  But if they do, the
// remainder of processing will happen here (and return 'true').
static bool 
handle_sequence (int argc, const char **argv)
{
    // First, scan the original command line arguments for '#'
    // characters.  Any found indicate that there are numeric rnage or
    // wildcards to deal with.  Also look for --frames and --framepadding
    // options.
    std::string framenumbers_string;
    int framepadding = 4;
    std::vector<int> sequence_args;  // Args with sequence numbers
    bool is_sequence = false;
    for (int a = 1;  a < argc;  ++a) {
        if (strchr (argv[a], '#')) {
            is_sequence = true;
            sequence_args.push_back (a);
        }
        if (! strcmp (argv[a], "--frames") && a < argc-1) {
            framenumbers_string = argv[++a];
        }
        else if (! strcmp (argv[a], "--framepadding") && a < argc-1) {
            int f = atoi (argv[++a]);
            if (f >= 1 && f < 10)
                framepadding = f;
        }
    }

    // No ranges or wildcards?
    if (! is_sequence)
        return false;

    std::vector<std::string> numbers;

    // If an explicit frame sequence was given, elaborate it.
    if (framenumbers_string.size())
        generate_sequence (framenumbers_string, framepadding, numbers);

    // For each of the arguments that contains a wildcard, use
    // deduce_sequence to fully elaborate all the filenames in the
    // sequence.  It's an error if the sequences are not all of the
    // same length.
    std::vector< std::vector<std::string> > filenames (argc+1);
    size_t nfilenames = 0;
    for (size_t i = 0;  i < sequence_args.size();  ++i) {
        int a = sequence_args[i];
        deduce_sequence (argv[a], framepadding, filenames[a], numbers);
        if (i == 0) {
            nfilenames = filenames[a].size();
        } else if (nfilenames != filenames[a].size()) {
            ot.error (Strutil::format("Not all sequence specifications matched: %s vs. %s",
                                      argv[sequence_args[0]], argv[a]), "");
            return true;
        }
    }

    // OK, now we just call getargs once for each item in the sequences,
    // substituting the i-th sequence entry for its respective argument
    // every time.
    std::vector<const char *> seq_argv (argv, argv+argc+1);
    for (size_t i = 0;  i < nfilenames;  ++i) {
        for (size_t j = 0;  j < sequence_args.size();  ++j) {
            size_t a = sequence_args[j];
            seq_argv[a] = filenames[a][i].c_str();
        }
        ot.clear_options (); // Careful to reset all command line options!
        getargs (argc, (char **)&seq_argv[0]);
        ot.process_pending ();
        if (ot.pending_callback()) {
            std::cout << "oiiotool WARNING: pending '" << ot.pending_callback_name()
                      << "' command never executed.\n";
        }
    }

    return true;
}



int
main (int argc, char *argv[])
{
// When Visual Studio is used float values in scientific format are printed 
// with three digit exponent. We change this behavior to fit the Linux way.
#ifdef _MSC_VER
    _set_output_format (_TWO_DIGIT_EXPONENT);
#endif

    Timer totaltime;

    ot.imagecache = ImageCache::create (false);
    ASSERT (ot.imagecache);
    ot.imagecache->attribute ("forcefloat", 1);
    ot.imagecache->attribute ("m_max_memory_MB", 4096.0);
//    ot.imagecache->attribute ("autotile", 1024);

    if (handle_sequence (argc, (const char **)argv)) {
        // Deal with sequence

    } else {
        // Not a sequence
        getargs (argc, argv);
        ot.process_pending ();
        if (ot.pending_callback()) {
            std::cout << "oiiotool WARNING: pending '" << ot.pending_callback_name()
                      << "' command never executed.\n";
        }
    }

    if (ot.runstats) {
        double total_time = totaltime();
        double unaccounted = total_time;
        std::cout << "\n";
        int threads = -1;
        OIIO::getattribute ("threads", threads);
        std::cout << "Threads: " << threads << "\n";
        std::cout << "oiiotool runtime statistics:\n";
        std::cout << "  Total time: " << Strutil::timeintervalformat(total_time,2) << "\n";
        static const char *timeformat = "      %-12s : %5.2f\n";
        for (Oiiotool::TimingMap::const_iterator func = ot.function_times.begin();
             func != ot.function_times.end();  ++func) {
            double t = func->second;
            std::cout << Strutil::format (timeformat, func->first, t);
            unaccounted -= t;
        }
        std::cout << Strutil::format (timeformat, "unaccounted", std::max(unaccounted, 0.0));
        std::cout << ot.imagecache->getstats() << "\n";
    }

    return ot.return_value;
}
