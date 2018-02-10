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
#include <algorithm>
#include <utility>
#include <cctype>
#include <map>

#include <OpenEXR/ImfTimeCode.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/timer.h>

#include "oiiotool.h"

#ifdef USE_BOOST_REGEX
# include <boost/regex.hpp>
  using boost::regex;
  using boost::regex_search;
  using boost::regex_replace;
  using boost::match_results;
#else
# include <regex>
  using std::regex;
  using std::regex_search;
  using std::regex_replace;
  using std::match_results;
#endif


using namespace OIIO;
using namespace OiioTool;
using namespace ImageBufAlgo;


static Oiiotool ot;


// Macro to fully set up the "action" function that straightforwardly
// calls a custom OiiotoolOp class.
#define OP_CUSTOMCLASS(name,opclass,ninputs)                           \
    static int action_##name (int argc, const char *argv[]) {          \
        if (ot.postpone_callback (ninputs, action_##name, argc, argv)) \
            return 0;                                                  \
        opclass op (ot, #name, argc, argv);                            \
        return op();                                                   \
    }


#define UNARY_IMAGE_OP(name,impl)                                      \
    static int action_##name (int argc, const char *argv[]) {          \
        const int nargs = 1, ninputs = 1;                              \
        if (ot.postpone_callback (ninputs, action_##name, argc, argv)) \
            return 0;                                                  \
        ASSERT (argc == nargs);                                        \
        OiiotoolSimpleUnaryOp<IBAunary> op (impl, ot, #name,           \
                                            argc, argv, ninputs);      \
        return op();                                                   \
    }


#define BINARY_IMAGE_OP(name,impl)                                     \
    static int action_##name (int argc, const char *argv[]) {          \
        const int nargs = 1, ninputs = 2;                              \
        if (ot.postpone_callback (ninputs, action_##name, argc, argv)) \
            return 0;                                                  \
        ASSERT (argc == nargs);                                        \
        OiiotoolSimpleBinaryOp<IBAbinary> op (impl, ot, #name,         \
                                              argc, argv, ninputs);    \
        return op();                                                   \
    }


#define BINARY_IMAGE_COLOR_OP(name,impl,defaultval)                    \
    static int action_##name (int argc, const char *argv[]) {          \
        const int nargs = 2, ninputs = 1;                              \
        if (ot.postpone_callback (ninputs, action_##name, argc, argv)) \
            return 0;                                                  \
        ASSERT (argc == nargs);                                        \
        OiiotoolImageColorOp<IBAbinary_img_col> op (impl, ot, #name,   \
                                              argc, argv, ninputs);    \
        return op();                                                   \
    }





Oiiotool::Oiiotool ()
{
    clear_options ();
}



void
Oiiotool::clear_options ()
{
    verbose = false;
    debug = false;
    dryrun = false;
    runstats = false;
    noclobber = false;
    allsubimages = false;
    printinfo = false;
    printstats = false;
    dumpdata = false;
    dumpdata_showempty = true;
    hash = false;
    updatemode = false;
    autoorient = false;
    autocc = false;
    nativeread = false;
    cachesize = 4096;
    autotile = 0;   // was: 4096
    // FIXME: Turned off autotile by default Jan 2018 after thinking that
    // it was possible to deadlock when doing certain parallel IBA functions
    // in combination with autotile. When the deadlock possibility is fixed,
    // maybe we'll turn it back to on by default.
    frame_padding = 0;
    full_command_line.clear ();
    printinfo_metamatch.clear ();
    printinfo_nometamatch.clear ();
    printinfo_verbose = false;
    input_config = ImageSpec();
    input_config_set = false;
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
    output_autotrim = false;
    output_dither = false;
    output_force_tiles = false;
    metadata_nosoftwareattrib = false;
    diff_warnthresh = 1.0e-6f;
    diff_warnpercent = 0;
    diff_hardwarn = std::numeric_limits<float>::max();
    diff_failthresh = 1.0e-6f;
    diff_failpercent = 0;
    diff_hardfail = std::numeric_limits<float>::max();
    m_pending_callback = NULL;
    m_pending_argc = 0;
    frame_number = 0;
    frame_padding = 0;
    first_input_dataformat = TypeUnknown;
    first_input_dataformat_bits = 0;
    first_input_channelformats.clear();
}



std::string
format_resolution (int w, int h, int x, int y)
{
    return Strutil::format ("%dx%d%+d%+d", w, h, x, y);
}



std::string
format_resolution (int w, int h, int d, int x, int y, int z)
{
    return Strutil::format ("%dx%dx%d%+d%+d%+d", w, h, d, x, y, z);
}


// FIXME -- lots of things we skimped on so far:
// FIXME: reject volume images?
// FIXME: do all ops respect -a (or lack thereof?)


bool
Oiiotool::read (ImageRecRef img, ReadPolicy readpolicy)
{
    // If the image is already elaborated, take an early out, both to
    // save time, but also because we only want to do the format and
    // tile adjustments below as images are read in fresh from disk.
    if (img->elaborated())
        return true;

    // Cause the ImageRec to get read.  Try to compute how long it took.
    // Subtract out ImageCache time, to avoid double-accounting it later.
    float pre_ic_time, post_ic_time;
    imagecache->getattribute ("stat:fileio_time", pre_ic_time);
    total_readtime.start ();
    if (ot.nativeread)
        readpolicy = ReadPolicy (readpolicy | ReadNative);
    bool ok = img->read (readpolicy);
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
    // Remember the first input format we encountered.
    if (first_input_dataformat == TypeUnknown) {
        first_input_dataformat = nspec.format;
        first_input_dataformat_bits = nspec.get_int_attribute ("oiio:BitsPerSample");
        if (nspec.channelformats.size()) {
            for (int c = 0; c < nspec.nchannels; ++c) {
                std::string chname = nspec.channelnames[c];
                first_input_channelformats[chname] = std::string(nspec.channelformat(c).c_str());
            }
        }
    }

    if (! ok) {
        error ("read "+img->name(), img->geterror());
    }
    return ok;
}



bool
Oiiotool::postpone_callback (int required_images, CallbackFunction func,
                             int argc, const char *argv[])
{
    if (image_stack_depth() < required_images) {
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
Oiiotool::error (string_view command, string_view explanation) const
{
    std::cerr << "oiiotool ERROR: " << command;
    if (explanation.length())
        std::cerr << " : " << explanation;
    std::cerr << "\n";
    // Repeat the command line, so if oiiotool is being called from a
    // script, it's easy to debug how the command was mangled.
    std::cerr << "Full command line was:\n> " << full_command_line << "\n";
    exit (-1);
}



void
Oiiotool::warning (string_view command, string_view explanation) const
{
    std::cerr << "oiiotool WARNING: " << command;
    if (explanation.length())
        std::cerr << " : " << explanation;
    std::cerr << "\n";
}



int
Oiiotool::extract_options (std::map<std::string,std::string> &options,
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
    int nthreads = atoi(argv[1]);
    OIIO::attribute ("threads", nthreads);
    OIIO::attribute ("exr_threads", nthreads);
    return 0;
}



static int
set_cachesize (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    ot.cachesize = atoi(argv[1]);
    ot.imagecache->attribute ("max_memory_MB", float(ot.cachesize));
    return 0;
}



static int
set_autotile (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    ot.autotile = atoi(argv[1]);
    ot.imagecache->attribute ("autotile", ot.autotile);
    ot.imagecache->attribute ("autoscanline", int(ot.autotile ? 1 : 0));
    return 0;
}



static int
set_native (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    ot.nativeread = true;
    ot.imagecache->attribute ("forcefloat", 0);
    return 0;
}



static int
set_dumpdata (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    string_view command = ot.express (argv[0]);
    ot.dumpdata = true;
    std::map<std::string,std::string> options;
    options["empty"] = "1";
    ot.extract_options (options, command);
    ot.dumpdata_showempty = Strutil::from_string<int> (options["empty"]);
    return 0;
}



static int
set_printinfo (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    string_view command = ot.express (argv[0]);
    ot.printinfo = true;
    std::map<std::string,std::string> options;
    ot.extract_options (options, command);
    ot.printinfo_format = options["format"];
    ot.printinfo_verbose = Strutil::from_string<int>(options["verbose"]);
    return 0;
}



static int
set_autopremult (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    ot.imagecache->attribute ("unassociatedalpha", 0);
    return 0;
}



static int
unset_autopremult (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    ot.imagecache->attribute ("unassociatedalpha", 1);
    return 0;
}



static int
action_label (int argc, const char *argv[])
{
    string_view labelname = ot.express(argv[1]);
    ot.image_labels[labelname] = ot.curimg;
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
    } else if (s == "uint32") {
        dataformat = TypeDesc::UINT32;  bits = 0;
    } else if (s == "int32") {
        dataformat = TypeDesc::INT32;   bits = 0;
    } else if (s == "half") {
        dataformat = TypeDesc::HALF;    bits = 0;
    } else if (s == "float") {
        dataformat = TypeDesc::FLOAT;   bits = 0;
    } else if (s == "double") {
        dataformat = TypeDesc::DOUBLE;  bits = 0;
    }
}




inline int
get_value_override (string_view localoption, int defaultval=0)
{
    return localoption.size() ? Strutil::from_string<int>(localoption)
                              : defaultval;
}


inline float
get_value_override (string_view localoption, float defaultval)
{
    return localoption.size() ? Strutil::from_string<float>(localoption)
                              : defaultval;
}


inline string_view
get_value_override (string_view localoption, string_view defaultval)
{
    return localoption.size() ? localoption : defaultval;
}



// Given a (potentially empty) overall data format, per-channel formats,
// and bit depth, modify the existing spec.
static void
set_output_dataformat (ImageSpec& spec, TypeDesc format,
                       const std::map<std::string,std::string>& channelformats,
                       int bitdepth)
{
    if (format != TypeUnknown)
        spec.format = format;
    if (bitdepth)
        spec.attribute ("oiio:BitsPerSample", bitdepth);
    else
        spec.erase_attribute ("oiio:BitsPerSample");
    if (channelformats.size()) {
        spec.channelformats.clear ();
        spec.channelformats.resize (spec.nchannels, spec.format);
        for (int c = 0;  c < spec.nchannels;  ++c) {
            if (c >= (int)spec.channelnames.size())
                break;
            auto i = channelformats.find (spec.channelnames[c]);
            if (i != channelformats.end() && i->second.size()) {
                int bits = 0;
                string_to_dataformat (i->second, spec.channelformats[c], bits);
            }
        }
        bool allsame = true;
        if (spec.channelnames.size())
            for (int c = 1;  c < spec.nchannels;  ++c)
                allsame &= (spec.channelformats[c] == spec.channelformats[0]);
        if (allsame) {
            spec.format = spec.channelformats[0];
            spec.channelformats.clear();
        }
    } else {
        spec.channelformats.clear ();
    }
}



static void
adjust_output_options (string_view filename,
                       ImageSpec &spec, const ImageSpec *nativespec,
                       const Oiiotool &ot,
                       bool format_supports_tiles,
                       std::map<std::string,std::string> &fileoptions,
                       bool was_direct_read = false)
{
    // What data format and bit depth should we use for the output? Here's
    // the logic:
    // * If a specific request was made on this command (e.g. -o:format=half)
    //   or globally (e.g., -d half), honor that, with a per-command request
    //   taking precedence.
    // * Otherwise, If the buffer is more or less a direct copy from an
    //   input image (as read, not the result of subsequent operations,
    //   which will tend to generate float output no matter what the
    //   inputs), write it out in the same format it was read from.
    // * Otherwise, output the same type as the FIRST file that was input
    //   (we are guessing that even if the operations made result buffers
    //   that were float, the user probably wanted to output it the same
    //   format as the input, or else she would have said so).
    // * Otherwise, just write the buffer's format, regardless of how it got
    //   that way.
    TypeDesc requested_output_dataformat = ot.output_dataformat;
    auto requested_output_channelformats = ot.output_channelformats;
    if (fileoptions["datatype"] != "") {
        requested_output_dataformat.fromstring (fileoptions["datatype"]);
        requested_output_channelformats.clear();
    }
    int requested_output_bits = get_value_override (fileoptions["bits"], ot.output_bitspersample);

    if (requested_output_dataformat != TypeUnknown) {
        // Requested an explicit override of datatype
        set_output_dataformat (spec, requested_output_dataformat,
                               requested_output_channelformats,
                               requested_output_bits);
    }
    else if (was_direct_read && nativespec) {
        // Do nothing -- use the file's native data format
        set_output_dataformat (spec, nativespec->format,
                               std::map<std::string,std::string>(),
                               nativespec->get_int_attribute("oiio:BitsPerSample"));
        spec.channelformats = nativespec->channelformats;
    }
    else if (ot.first_input_dataformat != TypeUnknown) {
        set_output_dataformat (spec, ot.first_input_dataformat,
                               ot.first_input_channelformats,
                               ot.first_input_dataformat_bits);
    }

    // Tiling strategy:
    // * If a specific request was made for tiled or scanline output, honor
    //   that (assuming the file format supports it).
    // * Otherwise, if the buffer is a direct copy from an input image, try
    //   to write it with the same tile/scanline choices as the input (if
    //   the file format supports it).
    // * Otherwise, just default to scanline.
    int requested_tilewidth = ot.output_tilewidth;
    int requested_tileheight = ot.output_tileheight;
    string_view tilesize = fileoptions["tile"];
    if (tilesize.size()) {
        int x, y;  // dummy vals for adjust_geometry
        ot.adjust_geometry ("-o", requested_tilewidth, requested_tileheight,
                            x, y, tilesize.c_str(), false);
    }
    bool requested_scanline = get_value_override (fileoptions["scanline"], ot.output_scanline);
    if (requested_tilewidth && !requested_scanline && format_supports_tiles) {
        // Explicit request to tile, honor it.
        spec.tile_width = requested_tilewidth;
        spec.tile_height = requested_tileheight ? requested_tileheight : requested_tilewidth;
        spec.tile_depth = 1;   // FIXME if we ever want volume support
    } else if (was_direct_read && nativespec &&
               nativespec->tile_width > 0 && nativespec->tile_height > 0 &&
               !requested_scanline && format_supports_tiles) {
        // No explicit request, but a direct read of a tiled input: keep the
        // input tiling.
        spec.tile_width = nativespec->tile_width;
        spec.tile_height = nativespec->tile_height;
        spec.tile_depth = nativespec->tile_depth;
    } else {
        // Otherwise, be safe and force scanline output.
        spec.tile_width = spec.tile_height = spec.tile_depth = 0;
    }

    if (! ot.output_compression.empty())
        spec.attribute ("compression", ot.output_compression);
    if (ot.output_quality > 0)
        spec.attribute ("CompressionQuality", ot.output_quality);

    if (get_value_override (fileoptions["separate"]))
        spec.attribute ("planarconfig", "separate");
    else if (get_value_override (fileoptions["contig"]))
        spec.attribute ("planarconfig", "contig");
    else if (ot.output_planarconfig == "contig" ||
        ot.output_planarconfig == "separate")
        spec.attribute ("planarconfig", ot.output_planarconfig);

    // Append command to image history.  Sometimes we may not want to recite the
    // entire command line (eg. when we have loaded it up with metadata attributes
    // that will make it into the header anyway).
    if (! ot.metadata_nosoftwareattrib) {
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

    int dither = get_value_override (fileoptions["dither"], ot.output_dither);
    if (dither) {
        int h = (int) Strutil::strhash(filename);
        if (!h)
            h = 1;
        spec.attribute ("oiio:dither", h);
    }

    // Make sure we kill any special hints that maketx adds and that will
    // no longer be valid after whatever oiiotool operations we've done.
    spec.erase_attribute ("oiio:SHA-1");
    spec.erase_attribute ("oiio:ConstantColor");
    spec.erase_attribute ("oiio:AverageColor");
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



// For a comma-separated list of channel names (e.g., "B,G,R,A"), compute
// the vector of integer indices for those channels as found in the spec
// (e.g., {2,1,0,3}), using -1 for any channels whose names were not found
// in the spec. Return true if all named channels were found, false if one
// or more were not found.
static bool
parse_channels (const ImageSpec &spec, string_view chanlist,
                std::vector<int> &channels)
{
    bool ok = true;
    channels.clear ();
    for (int c = 0; chanlist.length(); ++c) {
        int chan = -1;
        Strutil::skip_whitespace (chanlist);
        string_view name = Strutil::parse_until (chanlist, ",");
        if (name.size()) {
            for (int i = 0;  i < spec.nchannels;  ++i)
                if (spec.channelnames[i] == name) { // name of a known channel?
                    chan = i;
                    break;
                }
            if (chan < 0) { // Didn't find a match? Try case-insensitive.
                for (int i = 0;  i < spec.nchannels;  ++i)
                    if (Strutil::iequals (spec.channelnames[i], name)) {
                        chan = i;
                        break;
                    }
            }
            if (chan < 0)
                ok = false;
            channels.push_back (chan);
        }
        if (! Strutil::parse_char (chanlist, ','))
            break;
    }
    return ok;
}



static int
set_dataformat (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    string_view command = ot.express (argv[0]);
    std::vector<std::string> chans;
    Strutil::split (ot.express(argv[1]), chans, ",");

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
        if (ot.output_dataformat == TypeDesc::UNKNOWN)
            ot.error (command, Strutil::format ("Unknown data format \"%s\"", chans[0]));
        ot.output_channelformats.clear ();
        return 0;  // we're done
    }

    // If we make it here, the format designator was of the form
    //    name0=type0,name1=type1,...
    for (auto& chan : chans) {
        const char *eq = strchr(chan.c_str(),'=');
        if (eq) {
            std::string channame (chan, 0, eq - chan.c_str());
            ot.output_channelformats[channame] = std::string (eq+1);
        } else {
            ot.error (command, Strutil::format ("Malformed format designator \"%s\"", chan));
        }
    }

    return 0;
}



static int
set_string_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (! ot.curimg.get()) {
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }
    set_attribute (ot.curimg, argv[1], TypeString, argv[2],
                   ot.allsubimages);
    // N.B. set_attribute does expression expansion on its args
    return 0;
}



static int
set_any_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 3);
    if (! ot.curimg.get()) {
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }

    std::map<std::string,std::string> options;
    ot.extract_options (options, argv[0]);
    TypeDesc type (options["type"]);

    set_attribute (ot.curimg, argv[1], type, argv[2], ot.allsubimages);
    // N.B. set_attribute does expression expansion on its args
    return 0;
}



static bool
do_erase_attribute (ImageSpec &spec, string_view attribname)
{
    spec.erase_attribute (attribname);
    return true;
}



static int
erase_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }
    string_view pattern = ot.express (argv[1]);
    return apply_spec_mod (*ot.curimg, do_erase_attribute,
                           pattern, ot.allsubimages);
}



template<class T>
static bool
do_set_any_attribute (ImageSpec &spec, const std::pair<std::string,T> &x)
{
    spec.attribute (x.first, x.second);
    return true;
}



bool
Oiiotool::get_position (string_view command, string_view geom,
                        int &x, int &y)
{
    string_view orig_geom (geom);
    bool ok = Strutil::parse_int (geom, x)
           && Strutil::parse_char (geom, ',')
           && Strutil::parse_int (geom, y);
    if (! ok)
        error (command, Strutil::format ("Unrecognized position \"%s\"", orig_geom));
    return ok;
}



bool
Oiiotool::adjust_geometry (string_view command,
                           int &w, int &h, int &x, int &y, const char *geom,
                           bool allow_scaling) const
{
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    int ww = w, hh = h;
    int xx = x, yy = y;
    int xmax, ymax;
    if (sscanf (geom, "%d,%d,%d,%d", &xx, &yy, &xmax, &ymax) == 4) {
        x = xx;
        y = yy;
        w = std::max (0, xmax-xx+1);
        h = std::max (0, ymax-yy+1);
    } else if (sscanf (geom, "%dx%d%d%d", &ww, &hh, &xx, &yy) == 4 ||
               sscanf (geom, "%dx%d+%d+%d", &ww, &hh, &xx, &yy) == 4) {
        if (ww == 0 && h != 0)
            ww = int (hh * float(w)/float(h) + 0.5f);
        if (hh == 0 && w != 0)
            hh = int (ww * float(h)/float(w) + 0.5f);
        w = ww;
        h = hh;
        x = xx;
        y = yy;
    } else if (sscanf (geom, "%dx%d", &ww, &hh) == 2) {
        if (ww == 0 && h != 0)
            ww = int (hh * float(w)/float(h) + 0.5f);
        if (hh == 0 && w != 0)
            hh = int (ww * float(h)/float(w) + 0.5f);
        w = ww;
        h = hh;
    } else if (allow_scaling && sscanf (geom, "%f%%x%f%%", &scaleX, &scaleY) == 2) {
        scaleX = std::max(0.0f, scaleX*0.01f);
        scaleY = std::max(0.0f, scaleY*0.01f);
        if (scaleX == 0 && scaleY != 0)
            scaleX = scaleY;
        if (scaleY == 0 && scaleX != 0)
            scaleY = scaleX;
        w = (int)(w * scaleX + 0.5f);
        h = (int)(h * scaleY + 0.5f);
    } else if (sscanf (geom, "%d%d", &xx, &yy) == 2) {
        x = xx;
        y = yy;
    } else if (allow_scaling && sscanf (geom, "%f%%", &scaleX) == 1) {
        scaleX *= 0.01f;
        w = (int)(w * scaleX + 0.5f);
        h = (int)(h * scaleX + 0.5f);
    } else if (allow_scaling && sscanf (geom, "%f", &scaleX) == 1) {
        w = (int)(w * scaleX + 0.5f);
        h = (int)(h * scaleX + 0.5f);
    } else {
        error (command, Strutil::format ("Unrecognized geometry \"%s\"", geom));
        return false;
    }
    // printf ("geom %dx%d, %+d%+d\n", w, h, x, y);
    return true;
}



void
Oiiotool::express_error (const string_view expr, const string_view s, string_view explanation)
{
    int offset = expr.rfind(s) + 1;
    error("expression", Strutil::format ("%s at char %d of `%s'", explanation, offset, expr));
}



bool
Oiiotool::express_parse_atom(const string_view expr, string_view& s, std::string& result)
{
    // std::cout << " Entering express_parse_atom, s='" << s << "'\n";

    string_view orig = s;
    string_view stringval;
    float floatval;

    Strutil::skip_whitespace(s);

    // handle + or - prefixes
    bool negative = false;
    while (s.size()) {
        if (Strutil::parse_char (s, '-')) {
            negative = ! negative;
        } else if (Strutil::parse_char (s, '+')) {
            // no op
        } else {
            break;
        }
    }

    if (Strutil::parse_char (s, '(')) {
        // handle parentheses
        if (express_parse_summands (expr, s, result)) {
            if (! Strutil::parse_char (s, ')')) {
                express_error (expr, s, "missing `)'");
                result = orig;
                return false;
            }
        } else {
            result = orig;
            return false;
        }

    } else if (Strutil::starts_with (s,"TOP") || Strutil::starts_with (s, "IMG[")) {
        // metadata substitution
        ImageRecRef img;
        if (Strutil::parse_prefix (s, "TOP")) {
            img = curimg;
        } else if (Strutil::parse_prefix (s, "IMG[")) {
            int index = -1;
            if (Strutil::parse_int (s, index) && Strutil::parse_char (s, ']')
                  && index >= 0 && index <= (int)image_stack.size()) {
                if (index == 0)
                    img = curimg;
                else
                    img = image_stack[image_stack.size()-index];
            } else {
                string_view name = Strutil::parse_until (s, "]");
                std::map<std::string,ImageRecRef>::const_iterator found;
                found = ot.image_labels.find(name);
                if (found != ot.image_labels.end())
                    img = found->second;
                else
                    img = ImageRecRef (new ImageRec (name, ot.imagecache));
                Strutil::parse_char (s, ']');
            }
        }
        if (! img.get()) {
            express_error (expr, s, "not a valid image");
            result = orig;
            return false;
        }
        if (! Strutil::parse_char (s, '.')) {
            express_error (expr, s, "expected `.'");
            result = orig;
            return false;
        }
        string_view metadata = Strutil::parse_identifier (s, ":", true);
        if (metadata.size()) {
            read (img);
            ParamValue tmpparam;
            const ParamValue *p = img->spec(0,0)->find_attribute (metadata, tmpparam);
            if (p) {
                std::string val = ImageSpec::metadata_val (*p);
                if (p->type().basetype == TypeDesc::STRING) {
                    // metadata_val returns strings double quoted, strip
                    val.erase (0, 1);
                    val.erase (val.size()-1, 1);
                }
                result = val;
            }
            else if (metadata == "filename")
                result = img->name();
            else if (metadata == "file_extension")
                result = Filesystem::extension (img->name());
            else if (metadata == "file_noextension") {
                std::string filename = img->name();
                std::string ext = Filesystem::extension (img->name());
                result = filename.substr (0, filename.size()-ext.size());
            } else if (metadata == "MINCOLOR") {
                ImageBufAlgo::PixelStats pixstat;
                ImageBufAlgo::computePixelStats (pixstat, (*img)(0,0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.min.size(); ++i)
                    out << (i ? "," : "") << pixstat.min[i];
                result = out.str();
            } else if (metadata == "MAXCOLOR") {
                ImageBufAlgo::PixelStats pixstat;
                ImageBufAlgo::computePixelStats (pixstat, (*img)(0,0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.max.size(); ++i)
                    out << (i ? "," : "") << pixstat.max[i];
                result = out.str();
            } else if (metadata == "AVGCOLOR") {
                ImageBufAlgo::PixelStats pixstat;
                ImageBufAlgo::computePixelStats (pixstat, (*img)(0,0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.avg.size(); ++i)
                    out << (i ? "," : "") << pixstat.avg[i];
                result = out.str();
            } else {
                express_error (expr, s, Strutil::format ("unknown attribute name `%s'", metadata));
                result = orig;
                return false;
            }
        }
    } else if (Strutil::parse_float (s, floatval)) {
        result = Strutil::format ("%g", floatval);
    }
    // Test some special identifiers
    else if (Strutil::parse_identifier_if (s, "FRAME_NUMBER")) {
        result = Strutil::format ("%d", ot.frame_number);
    }
    else if (Strutil::parse_identifier_if (s, "FRAME_NUMBER_PAD")) {
        std::string fmt = ot.frame_padding == 0 ? std::string("%d")
                                : Strutil::format ("\"%%0%dd\"", ot.frame_padding);
        result = Strutil::format (fmt, ot.frame_number);
    }
    else {
        express_error (expr, s, "syntax error");
        result = orig;
        return false;
    }

    if (negative)
        result = "-" + result;

    // std::cout << " Exiting express_parse_atom, result='" << result << "'\n";

    return true;
}



bool
Oiiotool::express_parse_factors(const string_view expr, string_view& s, std::string& result)
{
    // std::cout << " Entering express_parse_factors, s='" << s << "'\n";

    string_view orig = s;
    std::string atom;
    float lval, rval;

    // parse the first factor
    if (! express_parse_atom (expr, s, atom)) {
        result = orig;
        return false;
    }

    if (atom.size() >= 2 && atom.front() == '\"' && atom.back() == '\"') {
        // Double quoted is string, return it
        result = atom;
    } else if (Strutil::string_is<float> (atom)) {
        // lval is a number
        lval = Strutil::from_string<float> (atom);
        while (s.size()) {
            char op;
            if (Strutil::parse_char (s, '*'))
                op = '*';
            else if (Strutil::parse_char (s, '/'))
                op = '/';
            else {
                // no more factors
                break;
            }

            // parse the next factor
            if (! express_parse_atom (expr, s, atom)) {
                result = orig;
                return false;
            }

            if (! Strutil::string_is<float> (atom)) {
                express_error (expr, s, Strutil::format ("expected number but got `%s'", atom));
                result = orig;
                return false;
            }

            // rval is a number, so we can math
            rval = Strutil::from_string<float>(atom);
            if (op == '*')
                lval *= rval;
            else // op == '/'
                lval /= rval;
        }

        result = Strutil::format ("%g", lval);

    } else {
        // atom is not a number, so we're done
        result = atom;
    }

    // std::cout << " Exiting express_parse_factors, result='" << result << "'\n";

    return true;
}



bool
Oiiotool::express_parse_summands(const string_view expr, string_view& s, std::string& result)
{
    // std::cout << " Entering express_parse_summands, s='" << s << "'\n";

    string_view orig = s;
    std::string atom;
    float lval, rval;

    // parse the first summand
    if (! express_parse_factors(expr, s, atom)) {
        result = orig;
        return false;
    }

    if (atom.size() >= 2 && atom.front() == '\"' && atom.back() == '\"') {
        // Double quoted is string, strip it
        result = atom.substr (1, atom.size()-2);
    } else if (Strutil::string_is<float> (atom)) {
        // lval is a number
        lval = Strutil::from_string<float> (atom);
        while (s.size()) {
            char op;
            if (Strutil::parse_char (s, '+'))
                op = '+';
            else if (Strutil::parse_char (s, '-'))
                op = '-';
            else {
                // no more summands
                break;
            }

            // parse the next summand
            if (! express_parse_factors(expr, s, atom)) {
                result = orig;
                return false;
            }

            if (! Strutil::string_is<float> (atom)) {
                express_error (expr, s, Strutil::format ("`%s' is not a number", atom));
                result = orig;
                return false;
            }

            // rval is also a number, we can math
            rval = Strutil::from_string<float>(atom);
            if (op == '+')
                lval += rval;
            else // op == '-'
                lval -= rval;
        }

        result = Strutil::format ("%g", lval);

    } else {
        // atom is not a number, so we're done
        result = atom;
    }

    // std::cout << " Exiting express_parse_summands, result='" << result << "'\n";

    return true;
}



// Expression evaluation and substitution for a single expression
std::string
Oiiotool::express_impl (string_view s)
{
    std::string result;
    string_view orig = s;
    if (! express_parse_summands(orig, s, result)) {
        result = orig;
    }
    return result;
}



// Perform expression evaluation and substitution on a string
string_view
Oiiotool::express (string_view str)
{
    string_view s = str;
    // eg. s="ab{cde}fg"
    size_t openbrace = s.find('{');
    if (openbrace == s.npos)
        return str;    // No open brace found -- no expresion substitution

    string_view prefix = s.substr (0, openbrace);
    s.remove_prefix (openbrace);
    // eg. s="{cde}fg", prefix="ab"
    string_view expr = Strutil::parse_nested (s);
    if (expr.empty())
        return str;     // No corresponding close brace found -- give up
    // eg. prefix="ab", expr="{cde}", s="fg", prefix="ab"
    ASSERT (expr.front() == '{' && expr.back() == '}');
    expr.remove_prefix(1);
    expr.remove_suffix(1);
    // eg. expr="cde"
    ustring result = ustring::format("%s%s%s", prefix, express_impl(expr), express(s));
    if (ot.debug)
        std::cout << "Expanding expression \"" << str << "\" -> \"" << result << "\"\n";
    return result;
}



static int
set_input_attribute (int argc, const char *argv[])
{
    ASSERT (argc == 3);

    std::map<std::string,std::string> options;
    ot.extract_options (options, argv[0]);
    TypeDesc type (options["type"]);
    string_view attribname = ot.express(argv[1]);
    string_view value = ot.express(argv[2]);

    if (! value.size()) {
        // If the value is the empty string, clear the attribute
        ot.input_config.erase_attribute (attribname);
        return 0;
    }

    ot.input_config_set = true;

    // First, handle the cases where we're told what to expect
    if (type.basetype == TypeDesc::FLOAT) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<float> vals (n, 0.0f);
        for (size_t i = 0; i < n && value.size(); ++i) {
            Strutil::parse_float (value, vals[i]);
            Strutil::parse_char (value, ',');
        }
        ot.input_config.attribute (attribname, type, &vals[0]);
        return 0;
    }
    if (type.basetype == TypeDesc::INT) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<int> vals (n, 0);
        for (size_t i = 0; i < n && value.size(); ++i) {
            Strutil::parse_int (value, vals[i]);
            Strutil::parse_char (value, ',');
        }
        ot.input_config.attribute (attribname, type, &vals[0]);
        return 0;
    }
    if (type.basetype == TypeDesc::STRING) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<ustring> vals (n, ustring());
        if (n == 1)
            vals[0] = ustring(value);
        else {
            for (size_t i = 0; i < n && value.size(); ++i) {
                string_view s;
                Strutil::parse_string (value, s);
                vals[i] = ustring(s);
                Strutil::parse_char (value, ',');
            }
        }
        ot.input_config.attribute (attribname, type, &vals[0]);
        return 0;
    }

    if (type == TypeInt ||
        (type == TypeUnknown && Strutil::string_is_int(value))) {
        // Does it seem to be an int, or did the caller explicitly request
        // that it be set as an int?
        ot.input_config.attribute (attribname, Strutil::stoi(value));
    } else if (type == TypeFloat ||
        (type == TypeUnknown && Strutil::string_is_float(value))) {
        // Does it seem to be a float, or did the caller explicitly request
        // that it be set as a float?
        ot.input_config.attribute (attribname, Strutil::stof(value));
    } else {
        // Otherwise, set it as a string attribute
        ot.input_config.attribute (attribname, value);
    }
    return 0;
}




bool
OiioTool::set_attribute (ImageRecRef img, string_view attribname,
                         TypeDesc type, string_view value,
                         bool allsubimages)
{
    // Expression substitution
    attribname = ot.express(attribname);
    value = ot.express(value);

    ot.read (img);
    img->metadata_modified (true);
    if (! value.size()) {
        // If the value is the empty string, clear the attribute
        return apply_spec_mod (*img, do_erase_attribute,
                               attribname, allsubimages);
    }

    // First, handle the cases where we're told what to expect
    if (type.basetype == TypeDesc::FLOAT) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<float> vals (n, 0.0f);
        for (size_t i = 0; i < n && value.size(); ++i) {
            Strutil::parse_float (value, vals[i]);
            Strutil::parse_char (value, ',');
        }
        for (int s = 0, send = img->subimages();  s < send;  ++s) {
            for (int m = 0, mend = img->miplevels(s);  m < mend;  ++m) {
                ((*img)(s,m).specmod()).attribute (attribname, type, &vals[0]);
                img->update_spec_from_imagebuf (s, m);
                if (! allsubimages)
                    break;
            }
            if (! allsubimages)
                break;
        }
        return true;
    }
    if (type == TypeTimeCode && value.find(':') != value.npos) {
        // Special case: They are specifying a TimeCode as a "HH:MM:SS:FF"
        // string, we need to re-encode as a uint32[2].
        int hour = 0, min = 0, sec = 0, frame = 0;
        sscanf (value.c_str(), "%d:%d:%d:%d", &hour, &min, &sec, &frame);
        Imf::TimeCode tc (hour, min, sec, frame);
        for (int s = 0, send = img->subimages();  s < send;  ++s) {
            for (int m = 0, mend = img->miplevels(s);  m < mend;  ++m) {
                ((*img)(s,m).specmod()).attribute (attribname, type, &tc);
                img->update_spec_from_imagebuf (s, m);
                if (! allsubimages)
                    break;
            }
            if (! allsubimages)
                break;
        }
        return true;
    }
    if (type == TypeRational && value.find('/') != value.npos) {
        // Special case: They are specifying a rational as "a/b", so we need
        // to re-encode as a int32[2].
        int v[2];
        Strutil::parse_int (value, v[0]);
        Strutil::parse_char (value, '/');
        Strutil::parse_int (value, v[1]);
        for (int s = 0, send = img->subimages();  s < send;  ++s) {
            for (int m = 0, mend = img->miplevels(s);  m < mend;  ++m) {
                ((*img)(s,m).specmod()).attribute (attribname, type, v);
                img->update_spec_from_imagebuf (s, m);
                if (! allsubimages)
                    break;
            }
            if (! allsubimages)
                break;
        }
        return true;
    }
    if (type.basetype == TypeDesc::INT) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<int> vals (n, 0);
        for (size_t i = 0; i < n && value.size(); ++i) {
            Strutil::parse_int (value, vals[i]);
            Strutil::parse_char (value, ',');
        }
        for (int s = 0, send = img->subimages();  s < send;  ++s) {
            for (int m = 0, mend = img->miplevels(s);  m < mend;  ++m) {
                ((*img)(s,m).specmod()).attribute (attribname, type, &vals[0]);
                img->update_spec_from_imagebuf (s, m);
                if (! allsubimages)
                    break;
            }
            if (! allsubimages)
                break;
        }
        return true;
    }
    if (type.basetype == TypeDesc::STRING) {
        size_t n = type.numelements() * type.aggregate;
        std::vector<ustring> vals (n, ustring());
        if (n == 1)
            vals[0] = ustring(value);
        else {
            for (size_t i = 0; i < n && value.size(); ++i) {
                string_view s;
                Strutil::parse_string (value, s);
                vals[i] = ustring(s);
                Strutil::parse_char (value, ',');
            }
        }
        for (int s = 0, send = img->subimages();  s < send;  ++s) {
            for (int m = 0, mend = img->miplevels(s);  m < mend;  ++m) {
                ((*img)(s,m).specmod()).attribute (attribname, type, &vals[0]);
                img->update_spec_from_imagebuf (s, m);
                if (! allsubimages)
                    break;
            }
            if (! allsubimages)
                break;
        }
        return true;
    }

    if (type == TypeInt ||
        (type == TypeUnknown && Strutil::string_is_int(value))) {
        // Does it seem to be an int, or did the caller explicitly request
        // that it be set as an int?
        int v = Strutil::stoi(value);
        return apply_spec_mod (*img, do_set_any_attribute<int>,
                               std::pair<std::string,int>(attribname,v),
                               allsubimages);
    } else if (type == TypeFloat ||
        (type == TypeUnknown && Strutil::string_is_float(value))) {
        // Does it seem to be a float, or did the caller explicitly request
        // that it be set as a float?
        float v = Strutil::stof(value);
        return apply_spec_mod (*img, do_set_any_attribute<float>,
                               std::pair<std::string,float>(attribname,v),
                               allsubimages);
    } else {
        // Otherwise, set it as a string attribute
        return apply_spec_mod (*img, do_set_any_attribute<std::string>,
                               std::pair<std::string,std::string>(attribname,value),
                               allsubimages);
    }
}



static int
set_caption (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    const char *newargs[3] = { argv[0], "ImageDescription", argv[1] };
    return set_string_attribute (3, newargs);
    // N.B. set_string_attribute does expression expansion on its args
}



static bool
do_set_keyword (ImageSpec &spec, const std::string &keyword)
{
    std::string oldkw = spec.get_string_attribute ("Keywords");
    std::vector<std::string> oldkwlist;
    if (! oldkw.empty())
        Strutil::split (oldkw, oldkwlist, ";");
    bool dup = false;
    for (std::string &ok : oldkwlist) {
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
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }

    std::string keyword (ot.express(argv[1]));
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
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }
    return set_attribute (ot.curimg, "Orientation", TypeDesc::INT, argv[1],
                          ot.allsubimages);
    // N.B. set_attribute does expression expansion on its args
}



static bool
do_rotate_orientation (ImageSpec &spec, string_view cmd)
{
    bool rotcw = (cmd == "--orientcw" || cmd == "-orientcw" ||
                  cmd == "--rotcw" || cmd == "-rotcw");
    bool rotccw = (cmd == "--orientccw" || cmd == "-orientccw" ||
                   cmd == "--rotccw" || cmd == "-rotccw");
    bool rot180 = (cmd == "--orient180" || cmd == "-orient180" ||
                   cmd == "--rot180" || cmd == "-rot180");
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
    string_view command = ot.express (argv[0]);
    if (! ot.curimg.get()) {
        ot.warning (command, "no current image available to modify");
        return 0;
    }
    apply_spec_mod (*ot.curimg, do_rotate_orientation, command,
                    ot.allsubimages);
    return 0;
}



static int
set_origin (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_origin, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    string_view origin  = ot.express (argv[1]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    for (int s = 0; s < A->subimages(); ++s) {
        ImageSpec &spec (*A->spec(s));
        int x = spec.x, y = spec.y, z = spec.z;
        int w = spec.width, h = spec.height, d = spec.depth;
        ot.adjust_geometry (command, w, h, x, y, origin.c_str());
        if (spec.width != w || spec.height != h || spec.depth != d)
            ot.warning (command, "can't be used to change the size, only the origin");
        if (spec.x != x || spec.y != y) {
            ImageBuf &ib = (*A)(s);
            if (ib.storage() == ImageBuf::IMAGECACHE) {
                // If the image is cached, we will totally screw up the IB/IC
                // operations if we try to change the origin in place, so in
                // that case force a full read to convert to a local buffer,
                // which is safe to diddle the origin.
                ib.read (0, 0, true /*force*/, spec.format);
            }
            spec.x = x;
            spec.y = y;
            spec.z = z;
            // That updated the private spec of the ImageRec. In this case
            // we really need to update the underlying IB as well.
            ImageSpec &ibspec = ib.specmod();
            ibspec.x = x;
            ibspec.y = y;
            ibspec.z = z;
            A->metadata_modified (true);
        }
    }
    ot.function_times[command] += timer();
    return 0;
}



static int
set_fullsize (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_fullsize, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    ImageSpec &spec (*A->spec(0,0));
    int x = spec.full_x, y = spec.full_y;
    int w = spec.full_width, h = spec.full_height;

    ot.adjust_geometry (argv[0], w, h, x, y, size.c_str());
    if (spec.full_x != x || spec.full_y != y ||
          spec.full_width != w || spec.full_height != h) {
        spec.full_x = x;
        spec.full_y = y;
        spec.full_width = w;
        spec.full_height = h;
        // That updated the private spec of the ImageRec. In this case
        // we really need to update the underlying IB as well.
        ImageSpec &ibspec = (*A)(0,0).specmod();
        ibspec.full_x = x;
        ibspec.full_y = y;
        ibspec.full_width = w;
        ibspec.full_height = h;
        A->metadata_modified (true);
    }
    ot.function_times[command] += timer();
    return 0;
}



static int
set_full_to_pixels (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_full_to_pixels, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    for (int s = 0, send = A->subimages();  s < send;  ++s) {
        for (int m = 0, mend = A->miplevels(s);  m < mend;  ++m) {
            ImageSpec &spec = *A->spec(s,m);
            spec.full_x = spec.x;
            spec.full_y = spec.y;
            spec.full_z = spec.z;
            spec.full_width = spec.width;
            spec.full_height = spec.height;
            spec.full_depth = spec.depth;
            // That updated the private spec of the ImageRec. In this case
            // we really need to update the underlying IB as well.
            ImageSpec &ibspec = (*A)(s,m).specmod();
            ibspec.full_x = spec.x;
            ibspec.full_y = spec.y;
            ibspec.full_z = spec.z;
            ibspec.full_width = spec.width;
            ibspec.full_height = spec.height;
            ibspec.full_depth = spec.depth;
        }
    }
    A->metadata_modified (true);
    ot.function_times[command] += timer();
    return 0;
}



static int
set_colorconfig (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    ot.colorconfig.reset (argv[1]);
    return 0;
}



static int
set_colorspace (int argc, const char *argv[])
{
    ASSERT (argc == 2);
    const char *args[3] = { argv[0], "oiio:ColorSpace", argv[1] };
    return set_string_attribute (3, args);
    // N.B. set_string_attribute does expression expansion on its args
}



class OpColorConvert : public OiiotoolOp {
public:
    OpColorConvert (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {
            fromspace = args[1];  tospace = args[2];
        }
    virtual void option_defaults () {
        options["strict"] = "1";
        options["unpremult"] = "0";
    }
    virtual bool setup () {
        if (fromspace == tospace) {
            // The whole thing is a no-op. Get rid of the empty result we
            // pushed on the stack, replace it with the original image, and
            // signal that we're done.
            ot.pop ();
            ot.push (ir[1]);
            return false;
        }
        return true;
    }
    virtual int impl (ImageBuf **img) {
        string_view contextkey = options["key"];
        string_view contextvalue = options["value"];
        bool strict = Strutil::from_string<int>(options["strict"]);
        bool unpremult = Strutil::from_string<int>(options["unpremult"]);
        if (unpremult && img[1]->spec().get_int_attribute("oiio:UnassociatedAlpha") && img[1]->spec().alpha_channel >= 0) {
            ot.warning (opname(), "Image appears to already be unassociated alpha (un-premultiplied color), beware double unpremult. Don't use --unpremult and also --colorconvert:unpremult=1.");
        }
        bool ok = ImageBufAlgo::colorconvert (*img[0], *img[1],
                                              fromspace, tospace, unpremult,
                                              contextkey, contextvalue,
                                              &ot.colorconfig);
        if (!ok && !strict) {
            // The color transform failed, but we were told not to be
            // strict, so ignore the error and just copy destination to
            // source.
            std::string err = img[0]->geterror();
            ot.warning (opname(), err);
            // ok = ImageBufAlgo::copy (*img[0], *img[1], TypeDesc);
            ok = img[0]->copy (*img[1]);
        }
        return ok;
    }
private:
    string_view fromspace, tospace;
};

OP_CUSTOMCLASS (colorconvert, OpColorConvert, 1);



static int
action_tocolorspace (int argc, const char *argv[])
{
    // Don't time -- let it get accounted by colorconvert
    ASSERT (argc == 2);
    if (! ot.curimg.get()) {
        ot.warning (argv[0], "no current image available to modify");
        return 0;
    }
    const char *args[3] = { argv[0], "current", argv[1] };
    return action_colorconvert (3, args);
}



class OpOcioLook : public OiiotoolOp {
public:
    OpOcioLook (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual void option_defaults () {
        options["from"] = "current";
        options["to"] = "current";
        options["unpremult"] = "0";
    }
    virtual int impl (ImageBuf **img) {
        string_view lookname = args[1];
        string_view fromspace = options["from"];
        string_view tospace = options["to"];
        string_view contextkey = options["key"];
        string_view contextvalue = options["value"];
        bool inverse = Strutil::from_string<int> (options["inverse"]);
        bool unpremult = Strutil::from_string<int>(options["unpremult"]);
        if (fromspace == "current" || fromspace == "")
            fromspace = img[1]->spec().get_string_attribute ("oiio:Colorspace", "Linear");
        if (tospace == "current" || tospace == "")
            tospace = img[1]->spec().get_string_attribute ("oiio:Colorspace", "Linear");
        return ImageBufAlgo::ociolook (*img[0], *img[1], lookname,
                                       fromspace, tospace, unpremult, inverse,
                                       contextkey, contextvalue,
                                       &ot.colorconfig);
    }
};

OP_CUSTOMCLASS (ociolook, OpOcioLook, 1);



class OpOcioDisplay : public OiiotoolOp {
public:
    OpOcioDisplay (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual void option_defaults () {
        options["from"] = "current";
        options["unpremult"] = "0";
    }
    virtual int impl (ImageBuf **img) {
        string_view displayname  = args[1];
        string_view viewname     = args[2];
        string_view fromspace    = options["from"];
        string_view contextkey   = options["key"];
        string_view contextvalue = options["value"];
        bool override_looks = options.find("looks") != options.end();
        bool unpremult = Strutil::from_string<int>(options["unpremult"]);
        if (fromspace == "current" || fromspace == "")
            fromspace = img[1]->spec().get_string_attribute ("oiio:Colorspace", "Linear");
        return ImageBufAlgo::ociodisplay (*img[0], *img[1], displayname,
                             viewname, fromspace,
                             override_looks ? options["looks"] : std::string(""),
                             unpremult, contextkey, contextvalue, &ot.colorconfig);
    }
};

OP_CUSTOMCLASS (ociodisplay, OpOcioDisplay, 1);



class OpOcioFileTransform : public OiiotoolOp {
public:
    OpOcioFileTransform (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual void option_defaults () {
        options["unpremult"] = "0";
    }
    virtual int impl (ImageBuf **img) {
        string_view name = args[1];
        bool inverse = Strutil::from_string<int> (options["inverse"]);
        bool unpremult = Strutil::from_string<int>(options["unpremult"]);
        return ImageBufAlgo::ociofiletransform (*img[0], *img[1], name,
                                                inverse, unpremult, &ot.colorconfig);
    }
};

OP_CUSTOMCLASS (ociofiletransform, OpOcioFileTransform, 1);



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
    string_view command = ot.express (argv[0]);

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --unmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, 0, true, true));
    ot.curimg = newimg;
    ot.function_times[command] += timer();
    return 0;
}



static int
set_channelnames (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, set_channelnames, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command    = ot.express (argv[0]);
    string_view channelarg = ot.express (argv[1]);

    ImageRecRef A = ot.curimg;
    ot.read (A);

    std::vector<std::string> newchannelnames;
    Strutil::split (channelarg, newchannelnames, ",");

    for (int s = 0; s < A->subimages(); ++s) {
        int miplevels = A->miplevels(s);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageSpec *spec = &(*A)(s,m).specmod();
            spec->channelnames.resize (spec->nchannels);
            for (int c = 0; c < spec->nchannels;  ++c) {
                if (c < (int)newchannelnames.size() &&
                      newchannelnames[c].size()) {
                    std::string name = newchannelnames[c];
                    ot.output_channelformats[name] = ot.output_channelformats[spec->channelnames[c]];
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
           A->update_spec_from_imagebuf(s,m);
        }
    }
    ot.function_times[command] += timer();
    return 0;
}



// For a given spec (which contains the channel names for an image), and
// a comma separated list of channels (e.g., "B,G,R,A"), compute the
// vector of integer indices for those channels (e.g., {2,1,0,3}).
// A channel may be a literal assignment (e.g., "=0.5"), or a literal
// assignment with channel naming (e.g., "Z=0.5"), the name of a channel
// ("A"), or the name of a channel with a new name reassigned ("R=G").
// Return true for success, false for failure, including if any of the
// channels were not present in the image.  Upon return, channels
// will be the indices of the source image channels to copy (-1 for
// channels that are not filled with source data), values will hold
// the value to fill un-sourced channels (defaulting to zero), and
// newchannelnames will be the name of renamed or non-default-named
// channels (defaulting to "" if no special name is needed).
bool
OiioTool::decode_channel_set (const ImageSpec &spec, string_view chanlist,
                    std::vector<std::string> &newchannelnames,
                    std::vector<int> &channels, std::vector<float> &values)
{
    // std::cout << "Decode_channel_set '" << chanlist << "'\n";
    channels.clear ();
    for (int c = 0; chanlist.length(); ++c) {
        // It looks like:
        //     <int>                (put old channel here, by numeric index)
        //     oldname              (put old named channel here)
        //     newname=oldname      (put old channel here, with new name)
        //     newname=<float>      (put constant value here, with a name)
        //     =<float>             (put constant value here, default name)
        std::string newname;
        int chan = -1;
        float val = 0.0f;
        Strutil::skip_whitespace (chanlist);
        if (chanlist.empty())
            break;
        if (Strutil::parse_int (chanlist, chan) && chan >= 0
                                                && chan < spec.nchannels) {
            // case: <int>
            newname = spec.channelnames[chan];
        } else if (Strutil::parse_char (chanlist, '=')) {
            // case: =<float>
            Strutil::parse_float (chanlist, val);
        } else {
            string_view n = Strutil::parse_until (chanlist, "=,");
            string_view oldname;
            if (Strutil::parse_char (chanlist, '=')) {
                if (Strutil::parse_float (chanlist, val)) {
                    // case: newname=float
                    newname = n;
                } else {
                    // case: newname=oldname
                    newname = n;
                    oldname = Strutil::parse_until (chanlist, ",");
                }
            } else {
                // case: oldname
                oldname = n;
            }
            if (oldname.size()) {
                for (int i = 0;  i < spec.nchannels;  ++i)
                    if (spec.channelnames[i] == oldname) { // name of a known channel?
                        chan = i;
                        break;
                    }
                if (chan < 0) { // Didn't find a match? Try case-insensitive.
                    for (int i = 0;  i < spec.nchannels;  ++i)
                        if (Strutil::iequals (spec.channelnames[i], oldname)) {
                            chan = i;
                            break;
                        }
                }
                if (newname.empty() && chan >= 0)
                    newname = spec.channelnames[chan];
            }
        }

        if (! newname.size()) {
            const char *RGBAZ[] = { "R", "G", "B", "A", "Z" };
            if (c <= 4)
                newname = std::string(RGBAZ[c]);
            else
                newname = Strutil::format ("channel%d", c);
        }

        // std::cout << "  Chan " << c << ": " << newname << ' ' << chan << ' ' << val << "\n";
        newchannelnames.push_back (newname);
        channels.push_back (chan);
        values.push_back (val);

        if (! Strutil::parse_char (chanlist, ','))
            break;
    }
    return true;
}



int
action_channels (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_channels, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);
    string_view chanlist = ot.express (argv[1]);

    ImageRecRef A (ot.pop());
    ot.read (A);

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
            ot.error (command, Strutil::format("Invalid or unknown channel selection \"%s\"", chanlist));
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
                ot.error (command, (*R)(s,m).geterror());
            // Tricky subtlety: IBA::channels changed the underlying IB,
            // we may need to update the IR's copy of the spec.
            R->update_spec_from_imagebuf(s,m);
        }
    }

    ot.function_times[command] += timer();
    return 0;
}



static int
action_chappend (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_chappend, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);

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
                ot.error (command, (*R)(s,m).geterror());
            // Tricky subtlety: IBA::channels changed the underlying IB,
            // we may need to update the IRR's copy of the spec.
            R->update_spec_from_imagebuf(s,m);
        }
    }
    ot.function_times[command] += timer();
    return 0;
}



static int
action_selectmip (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_selectmip, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    int miplevel = Strutil::from_string<int> (ot.express(argv[1]));

    ot.read ();
    bool mipmapped = false;
    for (int s = 0, send = ot.curimg->subimages();  s < send;  ++s)
        mipmapped |= (ot.curimg->miplevels(s) > 1);
    if (! mipmapped) {
        return 0;    // --selectmip on an unmipped image is a no-op
    }

    ImageRecRef newimg (new ImageRec (*ot.curimg, -1, miplevel, true, true));
    ot.curimg = newimg;
    ot.function_times[command] += timer();
    return 0;
}



static int
action_select_subimage (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_select_subimage, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    ot.read ();

    string_view command = ot.express (argv[0]);
    int subimage = 0;
    std::string whichsubimage = ot.express(argv[1]);
    string_view w (whichsubimage);
    if (Strutil::parse_int (w, subimage) && w.empty()) {
        // Subimage specification was an integer: treat as an index
        if (subimage < 0 || subimage >= ot.curimg->subimages()) {
            ot.error (command,
                     Strutil::format ("Invalid -subimage (%d): %s has %d subimage%s",
                                      subimage, ot.curimg->name(), ot.curimg->subimages(),
                                      ot.curimg->subimages() == 1 ? "" : "s"));
            return 0;
        }
    } else {
        // The subimage specification wasn't an integer. Assume it's a name.
        subimage = -1;
        for (int i = 0, n = ot.curimg->subimages(); i < n; ++i) {
            string_view siname = ot.curimg->spec(i)->get_string_attribute("oiio:subimagename");
            if (siname == whichsubimage) {
                subimage = i;
                break;
            }
        }
        if (subimage < 0) {
            ot.error (command,
                     Strutil::format ("Invalid -subimage (%s): named subimage not found",
                                      whichsubimage));
            return 0;
        }
    }

    if (ot.curimg->subimages() == 1 && subimage == 0)
        return 0;    // asking for the only subimage is a no-op
    
    ImageRecRef A = ot.pop();
    ot.push (new ImageRec (*A, subimage));
    ot.function_times[command] += timer();
    return 0;
}



static int
action_subimage_split (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_subimage_split, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ImageRecRef A = ot.pop();
    ot.read (A);

    // Push the individual subimages onto the stack
    for (int subimage = 0;  subimage <  A->subimages();  ++subimage)
        ot.push (new ImageRec (*A, subimage));

    ot.function_times[command] += timer();
    return 0;
}



static void
action_subimage_append_n (int n, string_view command)
{
    std::vector<ImageRecRef> images (n);
    for (int i = n-1; i >= 0; --i) {
        images[i] = ot.pop();
        ot.read (images[i]);   // necessary?
    }

    // Find the MIP levels in all the subimages of both A and B
    std::vector<int> allmiplevels;
    for (int i = 0; i < n; ++i) {
        ImageRecRef A = images[i];
        for (int s = 0;  s < A->subimages();  ++s) {
            int miplevels = ot.allsubimages ? A->miplevels(s) : 1;
            allmiplevels.push_back (miplevels);
        }
    }

    // Create the replacement ImageRec
    ImageRecRef R (new ImageRec(images[0]->name(), (int)allmiplevels.size(),
                                &allmiplevels[0]));
    ot.push (R);

    // Subimage by subimage, MIP level by MIP level, copy
    int sub = 0;
    for (int i = 0; i < n; ++i) {
        ImageRecRef A = images[i];
        for (int s = 0;  s <  A->subimages();  ++s, ++sub) {
            for (int m = 0;  m < A->miplevels(s);  ++m) {
                bool ok = (*R)(sub,m).copy ((*A)(s,m));
                if (! ok)
                    ot.error (command, (*R)(sub,m).geterror());
                // Update the IR's copy of the spec.
                R->update_spec_from_imagebuf(sub,m);
            }
            // For subimage append, preserve the notion of whether the
            // format is exactly as read from disk -- this is one of the few
            // operations for which it's true, since we are just appending
            // subimage, not modifying data or data format.
            (*R)[sub].was_direct_read ((*A)[s].was_direct_read());
        }
    }
}



static int
action_subimage_append (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_subimage_append, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    action_subimage_append_n (2, command);

    ot.function_times[command] += timer();
    return 0;
}



static int
action_subimage_append_all (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_subimage_append_all, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    action_subimage_append_n (int(ot.image_stack.size()+1), command);

    ot.function_times[command] += timer();
    return 0;
}



static int
action_colorcount (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_colorcount, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);
    string_view colorarg = ot.express (argv[1]);

    ot.read ();
    ImageBuf &Aib ((*ot.curimg)(0,0));
    int nchannels = Aib.nchannels();

    // We assume ';' to split, but for the sake of some command shells,
    // that use ';' as a command separator, also accept ":".
    std::vector<float> colorvalues;
    std::vector<std::string> colorstrings;
    if (colorarg.find(':') != colorarg.npos)
        Strutil::split (colorarg, colorstrings, ":");
    else
        Strutil::split (colorarg, colorstrings, ";");
    int ncolors = (int) colorstrings.size();
    for (int col = 0; col < ncolors; ++col) {
        std::vector<float> color (nchannels, 0.0f);
        Strutil::extract_from_list_string (color, colorstrings[col], ",");
        for (int c = 0;  c < nchannels;  ++c)
            colorvalues.push_back (c < (int)color.size() ? color[c] : 0.0f);
    }

    std::vector<float> eps (nchannels, 0.001f);
    std::map<std::string,std::string> options;
    ot.extract_options (options, command);
    Strutil::extract_from_list_string (eps, options["eps"]);

    imagesize_t *count = ALLOCA (imagesize_t, ncolors);
    bool ok = ImageBufAlgo::color_count ((*ot.curimg)(0,0), count,
                                         ncolors, &colorvalues[0], &eps[0]);
    if (ok) {
        for (int col = 0;  col < ncolors;  ++col)
            std::cout << Strutil::format("%8d  %s\n", count[col], colorstrings[col]);
    } else {
        ot.error (command, (*ot.curimg)(0,0).geterror());
    }

    ot.function_times[command] += timer();
    return 0;
}



static int
action_rangecheck (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_rangecheck, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);
    string_view lowarg   = ot.express (argv[1]);
    string_view higharg  = ot.express (argv[2]);

    ot.read ();
    ImageBuf &Aib ((*ot.curimg)(0,0));
    int nchannels = Aib.nchannels();

    std::vector<float> low(nchannels,0.0f), high(nchannels,1.0f);
    Strutil::extract_from_list_string (low, lowarg, ",");
    Strutil::extract_from_list_string (high, higharg, ",");

    imagesize_t lowcount = 0, highcount = 0, inrangecount = 0;
    bool ok = ImageBufAlgo::color_range_check ((*ot.curimg)(0,0), &lowcount,
                                               &highcount, &inrangecount,
                                               &low[0], &high[0]);
    if (ok) {
        std::cout << Strutil::format("%8d  < %s\n", lowcount, lowarg);
        std::cout << Strutil::format("%8d  > %s\n", highcount, higharg);
        std::cout << Strutil::format("%8d  within range\n", inrangecount);
    } else {
        ot.error (command, (*ot.curimg)(0,0).geterror());
    }

    ot.function_times[command] += timer();
    return 0;
}



static int
action_diff (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_diff, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    int ret = do_action_diff (*ot.image_stack.back(), *ot.curimg, ot);
    if (ret != DiffErrOK && ret != DiffErrWarn)
        ot.return_value = EXIT_FAILURE;

    if (ret != DiffErrOK && ret != DiffErrWarn && ret != DiffErrFail)
        ot.error (command);

    ot.printed_info = true; // because taking the diff has output
    ot.function_times[command] += timer();
    return 0;
}



static int
action_pdiff (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_pdiff, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    int ret = do_action_diff (*ot.image_stack.back(), *ot.curimg, ot, 1);
    if (ret != DiffErrOK && ret != DiffErrWarn)
        ot.return_value = EXIT_FAILURE;

    if (ret != DiffErrOK && ret != DiffErrWarn && ret != DiffErrFail)
        ot.error (command);

    ot.function_times[command] += timer();
    return 0;
}



BINARY_IMAGE_OP (add, ImageBufAlgo::add);
BINARY_IMAGE_OP (sub, ImageBufAlgo::sub);
BINARY_IMAGE_OP (mul, ImageBufAlgo::mul);
BINARY_IMAGE_OP (div, ImageBufAlgo::div);
BINARY_IMAGE_OP (absdiff, ImageBufAlgo::absdiff);

BINARY_IMAGE_COLOR_OP (addc, ImageBufAlgo::add, 0);
BINARY_IMAGE_COLOR_OP (subc, ImageBufAlgo::sub, 0);
BINARY_IMAGE_COLOR_OP (mulc, ImageBufAlgo::mul, 1);
BINARY_IMAGE_COLOR_OP (divc, ImageBufAlgo::div, 1);
BINARY_IMAGE_COLOR_OP (absdiffc, ImageBufAlgo::absdiff, 0);
BINARY_IMAGE_COLOR_OP (powc, ImageBufAlgo::pow, 1.0f);

UNARY_IMAGE_OP (abs, ImageBufAlgo::abs);



class OpPremult : public OiiotoolOp {
public:
    OpPremult (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::premult (*img[0], *img[1]);
    }
};
OP_CUSTOMCLASS (premult, OpPremult, 1);



class OpUnpremult : public OiiotoolOp {
public:
    OpUnpremult (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        if (img[1]->spec().get_int_attribute("oiio:UnassociatedAlpha") && img[1]->spec().alpha_channel >= 0) {
            ot.warning (opname(), "Image appears to already be unassociated alpha (un-premultiplied color), beware double unpremult.");
        }
        return ImageBufAlgo::unpremult (*img[0], *img[1]);
    }
};
OP_CUSTOMCLASS (unpremult, OpUnpremult, 1);




class OpMad : public OiiotoolOp {
public:
    OpMad (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 3) {}
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::mad (*img[0], *img[1], *img[2], *img[3]);
    }
};

OP_CUSTOMCLASS (mad, OpMad, 3);



class OpInvert : public OiiotoolOp {
public:
    OpInvert (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        // invert the first three channels only, spare alpha
        ROI roi = img[1]->roi();
        roi.chend = std::min (3, roi.chend);
        return ImageBufAlgo::invert (*img[0], *img[1], roi, 0);
    }
};

OP_CUSTOMCLASS (invert, OpInvert, 1);




class OpNoise : public OiiotoolOp {
public:
    OpNoise (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual void option_defaults () {
        options["type"] = "gaussian";
        options["min"] = "0";
        options["max"] = "0.1";
        options["mean"] = "0";
        options["stddev"] = "0.1";
        options["portion"] = "0.01";
        options["value"] = "0";
        options["mono"] = "0";
        options["seed"] = "0";
        options["nchannels"] = "10000";
    }
    virtual int impl (ImageBuf **img) {
        img[0]->copy (*img[1]);
        string_view type (options["type"]);
        float A = 0.0f, B = 0.1f;
        if (type == "gaussian") {
            A = Strutil::from_string<float> (options["mean"]);
            B = Strutil::from_string<float> (options["stddev"]);
        } else if (type == "uniform") {
            A = Strutil::from_string<float> (options["min"]);
            B = Strutil::from_string<float> (options["max"]);
        } else if (type == "salt") {
            A = Strutil::from_string<float> (options["value"]);
            B = Strutil::from_string<float> (options["portion"]);
        } else {
            ot.error (opname(), Strutil::format ("Unknown noise type \"%s\"", type));
            return 0;
        }
        bool mono = Strutil::from_string<int> (options["mono"]);
        int seed = Strutil::from_string<int> (options["seed"]);
        int nchannels = Strutil::from_string<int> (options["nchannels"]);
        ROI roi = img[0]->roi();
        roi.chend = std::min (roi.chend, nchannels);
        return ImageBufAlgo::noise (*img[0], type, A, B, mono, seed, roi);
    }
};

OP_CUSTOMCLASS (noise, OpNoise, 1);



static int
action_chsum (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_chsum, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ImageRecRef A (ot.pop());
    ot.read (A);
    ImageRecRef R (new ImageRec ("chsum", ot.allsubimages ? A->subimages() : 1));
    ot.push (R);

    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        std::vector<float> weight ((*A)(s).nchannels(), 1.0f);
        std::map<std::string,std::string> options;
        ot.extract_options (options, command);
        Strutil::extract_from_list_string (weight, options["weight"]);

        ImageBuf &Rib ((*R)(s));
        const ImageBuf &Aib ((*A)(s));
        bool ok = ImageBufAlgo::channel_sum (Rib, Aib, &weight[0]);
        if (! ok)
            ot.error (command, Rib.geterror());
        R->update_spec_from_imagebuf (s);
    }

    ot.function_times[command] += timer();
    return 0;
}



class OpColormap : public OiiotoolOp {
public:
    OpColormap (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        if (isalpha(args[1][0])) {
            // Named color map
            return ImageBufAlgo::color_map (*img[0], *img[1], -1, args[1],
                                            img[1]->roi(), 0);
        } else {
            // Values
            std::vector<float> knots;
            int n = Strutil::extract_from_list_string (knots, args[1]);
            return ImageBufAlgo::color_map (*img[0], *img[1], -1,
                                            n/3, 3, knots,
                                            img[1]->roi(), 0);
        }
    }
};

OP_CUSTOMCLASS (colormap, OpColormap, 1);





UNARY_IMAGE_OP (flip, ImageBufAlgo::flip);
UNARY_IMAGE_OP (flop, ImageBufAlgo::flop);
UNARY_IMAGE_OP (rotate180, ImageBufAlgo::rotate180);
UNARY_IMAGE_OP (rotate90, ImageBufAlgo::rotate90);
UNARY_IMAGE_OP (rotate270, ImageBufAlgo::rotate270);
UNARY_IMAGE_OP (transpose, ImageBufAlgo::transpose);



int
action_reorient (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_reorient, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    // Make sure time in the rotate functions is charged to reorient
    bool old_enable_function_timing = ot.enable_function_timing;
    ot.enable_function_timing = false;

    ImageRecRef A = ot.pop();
    ot.read (A);

    // See if any subimages need to be reoriented
    bool needs_reorient = false;
    for (int s = 0, subimages = A->subimages();  s < subimages;  ++s) {
        int orientation = (*A)(s).orientation();
        needs_reorient |= (orientation != 1);
    }

    if (needs_reorient) {
        ImageRecRef R (new ImageRec ("reorient", ot.allsubimages ? A->subimages() : 1));
        ot.push (R);
        for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
            ImageBufAlgo::reorient ((*R)(s), (*A)(s));
            R->update_spec_from_imagebuf (s);
        }
    } else {
        // No subimages need modification, just leave the whole thing in
        // place.
        ot.push (A);
    }

    ot.function_times[command] += timer();
    ot.enable_function_timing = old_enable_function_timing;
    return 0;
}



class OpRotate : public OiiotoolOp {
public:
    OpRotate (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        float angle = Strutil::from_string<float> (args[1]);
        std::string filtername = options["filter"];
        bool recompute_roi = Strutil::from_string<int>(options["recompute_roi"]);
        string_view center (options["center"]);
        float center_x = 0.0f, center_y = 0.0f;
        float cx, cy;
        if (center.size() && Strutil::parse_float(center, center_x) &&
            Strutil::parse_char(center, ',') && Strutil::parse_float(center, center_y)) {
            // center supplied
            cx = center_x;
            cy = center_y;
        } else {
            ROI src_roi_full = img[1]->roi_full();
            cx = 0.5f * (src_roi_full.xbegin + src_roi_full.xend);
            cy = 0.5f * (src_roi_full.ybegin + src_roi_full.yend);
        }
        return ImageBufAlgo::rotate (*img[0], *img[1],
                                     angle*float(M_PI/180.0), cx, cy,
                                     filtername, 0.0f, recompute_roi);
    }
};

OP_CUSTOMCLASS (rotate, OpRotate, 1);



class OpWarp : public OiiotoolOp {
public:
    OpWarp (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        std::string filtername = options["filter"];
        bool recompute_roi = Strutil::from_string<int>(options["recompute_roi"]);
        std::vector<float> M (9);
        if (Strutil::extract_from_list_string (M, args[1]) != 9) {
            ot.error (opname(), "expected 9 comma-separatd floats to form a 3x3 matrix");
            return 0;
        }
        return ImageBufAlgo::warp (*img[0], *img[1],
                                   *(Imath::M33f *)&M[0], filtername, 0.0f,
                                   recompute_roi, ImageBuf::WrapDefault);
    }
};

OP_CUSTOMCLASS (warp, OpWarp, 1);



class OpCshift : public OiiotoolOp {
public:
    OpCshift (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        int x = 0, y = 0, z = 0;
        if (sscanf (args[1].c_str(), "%d%d%d", &x, &y, &z) < 2) {
            ot.error (opname(), Strutil::format ("Invalid shift offset '%s'", args[1]));
            return 0;
        }
        return ImageBufAlgo::circular_shift (*img[0], *img[1], x, y, z);
    }
};

OP_CUSTOMCLASS (cshift, OpCshift, 1);



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
    string_view command = ot.express (argv[0]);
    if (ot.image_stack.size() < 1) {
        ot.error (command, "requires at least two loaded images");
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
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);
    int nchans = Strutil::from_string<int> (ot.express(argv[2]));
    if (nchans < 1 || nchans > 1024) {
        ot.warning (argv[0], Strutil::format ("Invalid number of channels: %d", nchans));
        nchans = 3;
    }
    ImageSpec spec (64, 64, nchans, TypeDesc::FLOAT);
    ot.adjust_geometry (argv[0], spec.width, spec.height, spec.x, spec.y,
                        size.c_str());
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_z = spec.z;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    ImageRecRef img (new ImageRec ("new", spec, ot.imagecache));
    bool ok = ImageBufAlgo::zero ((*img)());
    if (! ok)
        ot.error (command, (*img)().geterror());
    if (ot.curimg)
        ot.image_stack.push_back (ot.curimg);
    ot.curimg = img;
    ot.function_times[command] += timer();
    return 0;
}



static int
action_pattern (int argc, const char *argv[])
{
    ASSERT (argc == 4);
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    std::string pattern = ot.express (argv[1]);
    std::string size    = ot.express (argv[2]);
    int nchans = Strutil::from_string<int> (ot.express(argv[3]));
    if (nchans < 1 || nchans > 1024) {
        ot.warning (argv[0], Strutil::format ("Invalid number of channels: %d", nchans));
        nchans = 3;
    }
    ImageSpec spec (64, 64, nchans, TypeDesc::FLOAT);
    ot.adjust_geometry (argv[0], spec.width, spec.height, spec.x, spec.y,
                        size.c_str());
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_z = spec.z;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    ImageRecRef img (new ImageRec ("new", spec, ot.imagecache));
    ot.push (img);
    ImageBuf &ib ((*img)());
    bool ok = true;
    if (Strutil::iequals(pattern,"black")) {
        ok = ImageBufAlgo::zero (ib);
    } else if (Strutil::istarts_with(pattern,"constant")) {
        std::vector<float> fill (nchans, 1.0f);
        std::map<std::string,std::string> options;
        ot.extract_options (options, pattern);
        Strutil::extract_from_list_string (fill, options["color"]);
        ok = ImageBufAlgo::fill (ib, &fill[0]);
    } else if (Strutil::istarts_with(pattern,"fill")) {
        std::vector<float> topleft (nchans, 1.0f);
        std::vector<float> topright (nchans, 1.0f);
        std::vector<float> bottomleft (nchans, 1.0f);
        std::vector<float> bottomright (nchans, 1.0f);
        std::map<std::string,std::string> options;
        ot.extract_options (options, pattern);
        if (Strutil::extract_from_list_string (topleft,     options["topleft"]) &&
            Strutil::extract_from_list_string (topright,    options["topright"]) &&
            Strutil::extract_from_list_string (bottomleft,  options["bottomleft"]) &&
            Strutil::extract_from_list_string (bottomright, options["bottomright"])) {
            ok = ImageBufAlgo::fill (ib, &topleft[0], &topright[0],
                                     &bottomleft[0], &bottomright[0]);
        }
        else if (Strutil::extract_from_list_string (topleft,    options["top"]) &&
                 Strutil::extract_from_list_string (bottomleft, options["bottom"])) {
            ok = ImageBufAlgo::fill (ib, &topleft[0], &bottomleft[0]);
        }
        else if (Strutil::extract_from_list_string (topleft,  options["left"]) &&
                 Strutil::extract_from_list_string (topright, options["right"])) {
            ok = ImageBufAlgo::fill (ib, &topleft[0], &topright[0],
                                     &topleft[0], &topright[0]);
        }
        else if (Strutil::extract_from_list_string (topleft, options["color"])) {
            ok = ImageBufAlgo::fill (ib, &topleft[0]);
        }
    } else if (Strutil::istarts_with(pattern,"checker")) {
        std::map<std::string,std::string> options;
        options["width"] = "8";
        options["height"] = "8";
        options["depth"] = "8";
        ot.extract_options (options, pattern);
        int width = Strutil::from_string<int> (options["width"]);
        int height = Strutil::from_string<int> (options["height"]);
        int depth = Strutil::from_string<int> (options["depth"]);
        std::vector<float> color1 (nchans, 0.0f);
        std::vector<float> color2 (nchans, 1.0f);
        Strutil::extract_from_list_string (color1, options["color1"]);
        Strutil::extract_from_list_string (color2, options["color2"]);
        ok = ImageBufAlgo::checker (ib, width, height, depth,
                                    &color1[0], &color2[0], 0, 0, 0);
    } else if (Strutil::istarts_with(pattern, "noise")) {
        std::map<std::string,std::string> options;
        options["type"] = "gaussian";
        options["min"] = "0.5";
        options["max"] = "1";
        options["mean"] = "0.5";
        options["stddev"] = "0.1";
        options["portion"] = "0.01";
        options["value"] = "0";
        options["mono"] = "0";
        options["seed"] = "0";
        ot.extract_options (options, pattern);
        std::string type = options["type"];
        float A = 0, B = 1;
        if (type == "gaussian") {
            A = Strutil::from_string<float> (options["mean"]);
            B = Strutil::from_string<float> (options["stddev"]);
        } else if (type == "uniform") {
            A = Strutil::from_string<float> (options["min"]);
            B = Strutil::from_string<float> (options["max"]);
        } else if (type == "salt") {
            A = Strutil::from_string<float> (options["value"]);
            B = Strutil::from_string<float> (options["portion"]);
        } else {
            ot.error (command, Strutil::format ("Unknown noise type \"%s\"", type));
            ok = false;
        }
        bool mono = Strutil::from_string<int> (options["mono"]);
        int seed = Strutil::from_string<int> (options["seed"]);
        ImageBufAlgo::zero (ib);
        if (ok)
            ok = ImageBufAlgo::noise (ib, type, A, B, mono, seed);
    } else {
        ok = ImageBufAlgo::zero (ib);
        ot.warning (command, Strutil::format("Unknown pattern \"%s\"", pattern));
    }
    if (! ok)
        ot.error (command, ib.geterror());
    ot.function_times[command] += timer();
    return 0;
}



class OpKernel : public OiiotoolOp {
public:
    OpKernel (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 0) { }
    virtual int impl (ImageBuf **img) {
        string_view kernelname (args[1]);
        string_view kernelsize (args[2]);
        float w = 1.0f, h = 1.0f;
        if (sscanf (kernelsize.c_str(), "%fx%f", &w, &h) != 2)
            ot.error (opname(), Strutil::format ("Unknown size %s", kernelsize));
        return ImageBufAlgo::make_kernel (*img[0], kernelname, w, h);
    }
};

OP_CUSTOMCLASS (kernel, OpKernel, 0);



static int
action_capture (int argc, const char *argv[])
{
    ASSERT (argc == 1);
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    std::map<std::string,std::string> options;
    ot.extract_options (options, command);
    int camera = Strutil::from_string<int> (options["camera"]);

    ImageBuf ib;
    bool ok = ImageBufAlgo::capture_image (ib, camera, TypeDesc::FLOAT);
    if (! ok)
        ot.error (command, ib.geterror());
    ImageRecRef img (new ImageRec ("capture", ib.spec(), ot.imagecache));
    (*img)().copy (ib);
    ot.push (img);
    ot.function_times[command] += timer();
    return 0;
}



int
action_crop (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_crop, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);

    std::map<std::string,std::string> options;
    options["allsubimages"] = std::to_string(ot.allsubimages);
    ot.extract_options (options, command);
    int crop_all_subimages = Strutil::from_string<int>(options["allsubimages"]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    bool crops_needed = false;
    int subimages = crop_all_subimages ? A->subimages() : 1;
    for (int s = 0; s < subimages; ++s) {
        ImageSpec &spec (*A->spec(s,0));
        int w = spec.width, h = spec.height, d = spec.depth;
        int x = spec.x, y = spec.y, z = spec.z;
        ot.adjust_geometry (argv[0], w, h, x, y, size.c_str());
        crops_needed |=
            (w != spec.width || h != spec.height || d != spec.depth ||
             x != spec.x || y != spec.y || z != spec.z);
    }

    if (crops_needed) {
        ot.pop ();
        ImageRecRef R (new ImageRec (A->name(), subimages, 0));
        ot.push (R);
        for (int s = 0; s < subimages; ++s) {
            ImageSpec &spec (*A->spec(s,0));
            int w = spec.width, h = spec.height, d = spec.depth;
            int x = spec.x, y = spec.y, z = spec.z;
            ot.adjust_geometry (argv[0], w, h, x, y, size.c_str());
            const ImageBuf &Aib ((*A)(s,0));
            ImageBuf &Rib ((*R)(s,0));
            ROI roi = Aib.roi();
            if (w != spec.width || h != spec.height || d != spec.depth ||
                    x != spec.x || y != spec.y || z != spec.z) {
                roi = ROI (x, x+w, y, y+h, z, z+d);
            }
            bool ok = ImageBufAlgo::crop (Rib, Aib, roi);
            if (! ok)
                ot.error (command, Rib.geterror());
            R->update_spec_from_imagebuf (s, 0);
        }
    }

    ot.function_times[command] += timer();
    return 0;
}



int
action_croptofull (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_croptofull, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    bool crops_needed = false;
    for (int s = 0; s < A->subimages(); ++s) {
        crops_needed |= ((*A)(s).roi() != (*A)(s).roi_full());
    }

    if (crops_needed) {
        ot.pop ();
        ImageRecRef R (new ImageRec (A->name(), A->subimages(), 0));
        ot.push (R);
        for (int s = 0; s < A->subimages(); ++s) {
            const ImageBuf &Aib ((*A)(s,0));
            ImageBuf &Rib ((*R)(s,0));
            ROI roi = (Aib.roi() != Aib.roi_full()) ? Aib.roi_full() : Aib.roi();
            bool ok = ImageBufAlgo::crop (Rib, Aib, roi);
            if (! ok)
                ot.error (command, Rib.geterror());
            R->update_spec_from_imagebuf (s, 0);
        }
    }
    ot.function_times[command] += timer();
    return 0;
}



int
action_trim (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_trim, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ot.read ();
    ImageRecRef A = ot.curimg;
    
    // First, figure out shared nonzero region
    ROI nonzero_region;
    for (int s = 0; s < A->subimages(); ++s) {
        ROI roi = ImageBufAlgo::nonzero_region ((*A)(s));
        if (roi.npixels() == 0) {
            // Special case -- all zero; but doctor to make it 1 zero pixel
            roi = (*A)(s).roi();
            roi.xend = roi.xbegin+1;
            roi.yend = roi.ybegin+1;
            roi.zend = roi.zbegin+1;
        }
        nonzero_region = roi_union (nonzero_region, roi);
    }
    
    // Now see if any subimges need cropping
    bool crops_needed = false;
    for (int s = 0; s < A->subimages(); ++s) {
        crops_needed |= (nonzero_region != (*A)(s).roi());
    }
    if (crops_needed) {
        ot.pop ();
        ImageRecRef R (new ImageRec (A->name(), A->subimages(), 0));
        ot.push (R);
        for (int s = 0; s < A->subimages(); ++s) {
            const ImageBuf &Aib ((*A)(s,0));
            ImageBuf &Rib ((*R)(s,0));
            bool ok = ImageBufAlgo::crop (Rib, Aib, nonzero_region);
            if (! ok)
                ot.error (command, Rib.geterror());
            R->update_spec_from_imagebuf (s, 0);
        }
    }
    ot.function_times[command] += timer();
    return 0;
}



int
action_cut (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_cut, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);

    ot.read ();
    ImageRecRef A = ot.pop();
    ImageSpec &Aspec (*A->spec(0,0));
    ImageSpec newspec = Aspec;

    ot.adjust_geometry (argv[0], newspec.width, newspec.height,
                        newspec.x, newspec.y, size.c_str());

    ImageRecRef R (new ImageRec (A->name(), newspec, ot.imagecache));
    const ImageBuf &Aib ((*A)(0,0));
    ImageBuf &Rib ((*R)(0,0));
    ImageBufAlgo::cut (Rib, Aib, get_roi(newspec));

    ImageSpec &spec (*R->spec(0,0));
    set_roi (spec, Rib.roi());
    set_roi_full (spec, Rib.roi());
    A->metadata_modified (true);

    ot.push (R);

    ot.function_times[command] += timer();
    return 0;
}



class OpResample : public OiiotoolOp {
public:
    OpResample (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int compute_subimages () { return 1; } // just the first one
    virtual void option_defaults () {
        options["interp"] = "1";
    }
    virtual bool setup () {
        // The size argument will be the resulting display (full) window.
        const ImageSpec &Aspec (*ir[1]->spec(0,0));
        ImageSpec newspec = Aspec;
        ot.adjust_geometry (args[0], newspec.full_width, newspec.full_height,
                            newspec.full_x, newspec.full_y,
                            args[1].c_str() /*size*/, true);
        if (newspec.full_width == Aspec.full_width &&
            newspec.full_height == Aspec.full_height) {
            // No change -- pop the temp result and restore the original
            ot.pop ();
            ot.push (ir[1]);
            return false;   // nothing more to do
        }
        // Compute corresponding data window.
        float wratio = float(newspec.full_width) / float(Aspec.full_width);
        float hratio = float(newspec.full_height) / float(Aspec.full_height);
        newspec.x = newspec.full_x + int(floorf ((Aspec.x - Aspec.full_x) * wratio));
        newspec.y = newspec.full_y + int(floorf ((Aspec.y - Aspec.full_y) * hratio));
        newspec.width = int(ceilf (Aspec.width * wratio));
        newspec.height = int(ceilf (Aspec.height * hratio));
        (*ir[0])(0,0).reset (newspec);
        return true;
    }
    virtual int impl (ImageBuf **img) {
        bool interp = (bool) Strutil::from_string<int>(options["interp"]);
        return ImageBufAlgo::resample (*img[0], *img[1], interp);
    }
};

OP_CUSTOMCLASS (resample, OpResample, 1);



class OpResize : public OiiotoolOp {
public:
    OpResize (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int compute_subimages () { return 1; } // just the first one
    virtual bool setup () {
        // The size argument will be the resulting display (full) window.
        const ImageSpec &Aspec (*ir[1]->spec(0,0));
        ImageSpec newspec = Aspec;
        ot.adjust_geometry (args[0], newspec.full_width, newspec.full_height,
                            newspec.full_x, newspec.full_y,
                            args[1].c_str() /*size*/, true);
        if (newspec.full_width == Aspec.full_width &&
            newspec.full_height == Aspec.full_height) {
            // No change -- pop the temp result and restore the original
            ot.pop ();
            ot.push (ir[1]);
            return false;   // nothing more to do
        }
        // Compute corresponding data window.
        float wratio = float(newspec.full_width) / float(Aspec.full_width);
        float hratio = float(newspec.full_height) / float(Aspec.full_height);
        newspec.x = newspec.full_x + int(floorf ((Aspec.x - Aspec.full_x) * wratio));
        newspec.y = newspec.full_y + int(floorf ((Aspec.y - Aspec.full_y) * hratio));
        newspec.width = int(ceilf (Aspec.width * wratio));
        newspec.height = int(ceilf (Aspec.height * hratio));
        (*ir[0])(0,0).reset (newspec);
        return true;
    }
    virtual int impl (ImageBuf **img) {
        string_view filtername = options["filter"];
        if (ot.debug) {
            const ImageSpec &newspec (img[0]->spec());
            const ImageSpec &Aspec (img[1]->spec());
            std::cout << "  Resizing " << Aspec.width << "x" << Aspec.height
                      << " to " << newspec.width << "x" << newspec.height
                      << " using "
                      << (filtername.size() ? filtername.c_str() : "default")
                      << " filter\n";
        }
        return ImageBufAlgo::resize (*img[0], *img[1], filtername,
                                     0.0f, img[0]->roi());
    }
};

OP_CUSTOMCLASS (resize, OpResize, 1);



static int
action_fit (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fit, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    bool old_enable_function_timing = ot.enable_function_timing;
    ot.enable_function_timing = false;
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);

    // Examine the top of stack
    ImageRecRef A = ot.top();
    ot.read ();
    const ImageSpec *Aspec = A->spec(0,0);

    // Parse the user request for resolution to fit
    int fit_full_width = Aspec->full_width;
    int fit_full_height = Aspec->full_height;
    int fit_full_x = Aspec->full_x;
    int fit_full_y = Aspec->full_y;
    ot.adjust_geometry (argv[0], fit_full_width, fit_full_height,
                        fit_full_x, fit_full_y, size.c_str(), false);

    std::map<std::string,std::string> options;
    options["wrap"] = "black";
    options["allsubimages"] = std::to_string(ot.allsubimages);
    ot.extract_options (options, command);
    bool pad = Strutil::from_string<int>(options["pad"]);
    string_view filtername = options["filter"];
    bool exact = Strutil::from_string<int>(options["exact"]);
    ImageBuf::WrapMode wrap = ImageBuf::WrapMode_from_string (options["wrap"]);
    bool allsubimages = Strutil::from_string<int>(options["allsubimages"]);

    // Compute scaling factors and use action_resize to do the heavy lifting
    float oldaspect = float(Aspec->full_width) / Aspec->full_height;
    float newaspect = float(fit_full_width) / fit_full_height;
    int resize_full_width = fit_full_width;
    int resize_full_height = fit_full_height;
    int xoffset = 0, yoffset = 0;
    float xoff = 0.0f, yoff = 0.0f;
    float scale = 1.0f;

    if (newaspect >= oldaspect) {  // same or wider than original
        resize_full_width = int(resize_full_height * oldaspect + 0.5f);
        xoffset = (fit_full_width - resize_full_width) / 2;
        scale = float(fit_full_height) / float(Aspec->full_height);
        xoff = float(fit_full_width - scale * Aspec->full_width) / 2.0f;
    } else {  // narrower than original
        resize_full_height = int(resize_full_width / oldaspect + 0.5f);
        yoffset = (fit_full_height - resize_full_height) / 2;
        scale = float(fit_full_width) / float(Aspec->full_width);
        yoff = float(fit_full_height - scale * Aspec->full_height) / 2.0f;
    }

    if (ot.debug) {
        std::cout << "  Fitting "
                  << format_resolution(Aspec->full_width, Aspec->full_height,
                                       Aspec->full_x, Aspec->full_y)
                  << " into "
                  << format_resolution(fit_full_width, fit_full_height,
                                       fit_full_x, fit_full_y) << "\n";
        std::cout << "  Fit scale factor " << scale << "\n";
    }

    if (exact) {
        // Full partial-pixel filtered resize -- exactly preserves aspect
        // ratio and exactly centers the padded image, but might make the
        // edges of the resized area blurry because it's not a whole number
        // of pixels.
        Imath::M33f M (scale, 0.0f,  0.0f,
                       0.0f,  scale, 0.0f,
                       xoff,  yoff,  1.0f);
        if (ot.debug)
            std::cout << "   Fit performing warp with " << M << "\n";
        int subimages = allsubimages ? A->subimages() : 1;
        ImageRecRef R (new ImageRec (A->name(), subimages));
        for (int s = 0; s < subimages; ++s) {
            ImageSpec newspec = (*A)(s,0).spec();
            newspec.width = newspec.full_width = fit_full_width;
            newspec.height = newspec.full_height = fit_full_height;
            newspec.x = newspec.full_x = fit_full_x;
            newspec.y = newspec.full_y = fit_full_y;
            (*R)(s,0).reset (newspec);
            ImageBufAlgo::warp ((*R)(s,0), (*A)(s,0), M, filtername, 0.0f,
                                false, wrap);
            R->update_spec_from_imagebuf (s, 0);
        }
        ot.pop();
        ot.push (R);
        A = ot.top ();
        Aspec = A->spec(0,0);
    } else {
        // Full pixel resize -- gives the sharpest result, but for odd-sized
        // destination resolution, may not be exactly centered and will only
        // preserve the aspect ratio to the nearest integer pixel size.
        if (resize_full_width != Aspec->full_width ||
            resize_full_height != Aspec->full_height ||
            fit_full_x != Aspec->full_x || fit_full_y != Aspec->full_y) {
            std::string resize = format_resolution (resize_full_width,
                                                    resize_full_height,
                                                    0, 0);
            if (ot.debug)
                std::cout << "    Resizing to " << resize << "\n";
            std::string command = "resize";
            if (filtername.size())
                command += Strutil::format (":filter=%s", filtername);
            command += Strutil::format (":allsubimages=%d", allsubimages);
            const char *newargv[2] = { command.c_str(), resize.c_str() };
            action_resize (2, newargv);
            A = ot.top ();
            Aspec = A->spec(0,0);
            // Now A,Aspec are for the NEW resized top of stack
        } else {
            if (ot.debug)
                std::cout << "   no need to do a resize\n";
        }
        A->spec(0,0)->full_width = (*A)(0,0).specmod().full_width = fit_full_width;
        A->spec(0,0)->full_height = (*A)(0,0).specmod().full_height = fit_full_height;
        A->spec(0,0)->full_x = (*A)(0,0).specmod().full_x = fit_full_x;
        A->spec(0,0)->full_y = (*A)(0,0).specmod().full_y = fit_full_y;
        A->spec(0,0)->x = (*A)(0,0).specmod().x = xoffset;
        A->spec(0,0)->y = (*A)(0,0).specmod().y = yoffset;
    }

    if (pad && (fit_full_width != Aspec->width ||
                fit_full_height != Aspec->height)) {
        // Needs padding
        if (ot.debug)
            std::cout << "   performing a croptofull\n";
        const char *argv[] = { "croptofull" };
        action_croptofull (1, argv);
    }

    ot.function_times[command] += timer();
    ot.enable_function_timing = old_enable_function_timing;
    return 0;
}



static int
action_pixelaspect (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_pixelaspect, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    bool old_enable_function_timing = ot.enable_function_timing;
    ot.enable_function_timing = false;
    string_view command = ot.express (argv[0]);

    float new_paspect = Strutil::from_string<float> (ot.express (argv[1]));
    if (new_paspect <= 0.0f) {
        ot.error (command, Strutil::format ("Invalid pixel aspect ratio '%g'", new_paspect));
        return 0;
    }

    // Examine the top of stack
    ImageRecRef A = ot.top();
    ot.read ();
    const ImageSpec *Aspec = A->spec(0,0);

    // Get the current pixel aspect ratio
    float paspect = Aspec->get_float_attribute ("PixelAspectRatio", 1.0);
    if (paspect <= 0.0f) {
        ot.error (command, Strutil::format ("Invalid pixel aspect ratio '%g' in source", paspect));
        return 0;
    }

    // Get the current (if any) XResolution/YResolution attributes
    float xres = Aspec->get_float_attribute( "XResolution", 0.0);
    float yres = Aspec->get_float_attribute( "YResolution", 0.0);

    // Compute scaling factors and use action_resize to do the heavy lifting
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    float factor = paspect / new_paspect;
    if (factor > 1.0)
        scaleX = factor;
    else if (factor < 1.0)
        scaleY = 1.0/factor;

    int scale_full_width = (int)(Aspec->full_width * scaleX + 0.5f);
    int scale_full_height = (int)(Aspec->full_height * scaleY + 0.5f);

    float scale_xres = xres * scaleX;
    float scale_yres = yres * scaleY;

    std::map<std::string,std::string> options;
    ot.extract_options (options, command);
    string_view filtername = options["filter"];

    if (ot.debug) {
        std::cout << "  Scaling "
                  << format_resolution(Aspec->full_width, Aspec->full_height,
                                       Aspec->full_x, Aspec->full_y)
                  << " with a pixel aspect ratio of " << paspect
                  << " to "
                  << format_resolution(scale_full_width, scale_full_height,
                                       Aspec->full_x, Aspec->full_y)
                  << "\n";
    }
    if (scale_full_width != Aspec->full_width ||
        scale_full_height != Aspec->full_height) {
        std::string resize = format_resolution(scale_full_width,
                                               scale_full_height,
                                               0, 0);
        std::string command = "resize";
        if (filtername.size())
            command += Strutil::format (":filter=%s", filtername);
        const char *newargv[2] = { command.c_str(), resize.c_str() };
        action_resize (2, newargv);
        A = ot.top ();
        A->spec(0,0)->full_width = (*A)(0,0).specmod().full_width = scale_full_width;
        A->spec(0,0)->full_height = (*A)(0,0).specmod().full_height = scale_full_height;
        A->spec(0,0)->attribute ("PixelAspectRatio", new_paspect);
        if (xres)
            A->spec(0,0)->attribute ("XResolution", scale_xres);
        if (yres)
            A->spec(0,0)->attribute ("YResolution", scale_yres);
        // Now A,Aspec are for the NEW resized top of stack
    }

    ot.function_times[command] += timer();
    ot.enable_function_timing = old_enable_function_timing;
    return 0;
}



class OpConvolve : public OiiotoolOp {
public:
    OpConvolve (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 2) {}
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::convolve (*img[0], *img[1], *img[2]);
    }
};

OP_CUSTOMCLASS (convolve, OpConvolve, 2);




class OpBlur : public OiiotoolOp {
public:
    OpBlur (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual void option_defaults () {
        options["kernel"] = "gaussian";
    };
    virtual int impl (ImageBuf **img) {
        string_view kernopt = options["kernel"];
        float w = 1.0f, h = 1.0f;
        if (sscanf (args[1].c_str(), "%fx%f", &w, &h) != 2)
            ot.error (opname(), Strutil::format ("Unknown size %s", args[1]));
        ImageBuf Kernel;
        if (! ImageBufAlgo::make_kernel (Kernel, kernopt, w, h))
            ot.error (opname(), Kernel.geterror());
        return ImageBufAlgo::convolve (*img[0], *img[1], Kernel);
    }
};

OP_CUSTOMCLASS (blur, OpBlur, 1);




class OpMedian : public OiiotoolOp {
public:
    OpMedian (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int impl (ImageBuf **img) {
        string_view size (args[1]);
        int w = 3, h = 3;
        if (sscanf (size.c_str(), "%dx%d", &w, &h) != 2)
            ot.error (opname(), Strutil::format ("Unknown size %s", size));
        return ImageBufAlgo::median_filter (*img[0], *img[1], w, h);
    }
};

OP_CUSTOMCLASS (median, OpMedian, 1);



class OpDilate : public OiiotoolOp {
public:
    OpDilate (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int impl (ImageBuf **img) {
        string_view size (args[1]);
        int w = 3, h = 3;
        if (sscanf (size.c_str(), "%dx%d", &w, &h) != 2)
            ot.error (opname(), Strutil::format ("Unknown size %s", size));
        return ImageBufAlgo::dilate (*img[0], *img[1], w, h);
    }
};

OP_CUSTOMCLASS (dilate, OpDilate, 1);



class OpErode : public OiiotoolOp {
public:
    OpErode (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int impl (ImageBuf **img) {
        string_view size (args[1]);
        int w = 3, h = 3;
        if (sscanf (size.c_str(), "%dx%d", &w, &h) != 2)
            ot.error (opname(), Strutil::format ("Unknown size %s", size));
        return ImageBufAlgo::erode (*img[0], *img[1], w, h);
    }
};

OP_CUSTOMCLASS (erode, OpErode, 1);



class OpUnsharp : public OiiotoolOp {
public:
    OpUnsharp (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual void option_defaults () {
        options["kernel"] = "gaussian";
        options["width"] = "3";
        options["contrast"] = "1";
        options["threshold"] = "0";
    }
    virtual int impl (ImageBuf **img) {
        string_view kernel = options["kernel"];
        float width = Strutil::from_string<float> (options["width"]);
        float contrast = Strutil::from_string<float> (options["contrast"]);
        float threshold = Strutil::from_string<float> (options["threshold"]);
        return ImageBufAlgo::unsharp_mask (*img[0], *img[1], kernel,
                                           width, contrast, threshold);
    }
};

OP_CUSTOMCLASS (unsharp, OpUnsharp, 1);


class OpLaplacian : public OiiotoolOp {
public:
    OpLaplacian (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) { }
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::laplacian (*img[0], *img[1]);
    }
};

OP_CUSTOMCLASS (laplacian, OpLaplacian, 1);




UNARY_IMAGE_OP (fft, ImageBufAlgo::fft);
UNARY_IMAGE_OP (ifft, ImageBufAlgo::ifft);
UNARY_IMAGE_OP (polar, ImageBufAlgo::complex_to_polar);
UNARY_IMAGE_OP (unpolar, ImageBufAlgo::polar_to_complex);



int
action_fixnan (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fixnan, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);
    string_view modename = ot.express (argv[1]);

    NonFiniteFixMode mode = NONFINITE_BOX3;
    if (modename == "black")
        mode = NONFINITE_BLACK;
    else if (modename == "box3")
        mode = NONFINITE_BOX3;
    else if (modename == "error")
        mode = NONFINITE_ERROR;
    else {
        ot.warning (argv[0], Strutil::format ("\"%s\" not recognized. Valid choices: black, box3, error", modename));
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
                ot.error (command, Rib.geterror());
        }
    }
             
    ot.function_times[command] += timer();
    return 0;
}



static int
action_fillholes (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fillholes, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

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
        ot.error (command, Rib.geterror());

    ot.function_times[command] += timer();
    return 0;
}



static int
action_paste (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_paste, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command  = ot.express (argv[0]);
    string_view position = ot.express (argv[1]);

    ImageRecRef BG (ot.pop());
    ImageRecRef FG (ot.pop());
    ot.read (BG);
    ot.read (FG);

    int x = 0, y = 0;
    if (sscanf (position.c_str(), "%d%d", &x, &y) != 2) {
        ot.error (command, Strutil::format ("Invalid offset '%s'", position));
        return 0;
    }

    ImageRecRef R (new ImageRec (*BG, 0, 0, true /* writable*/, true /* copy */));
    ot.push (R);

    bool ok = ImageBufAlgo::paste ((*R)(), x, y, 0, 0, (*FG)());
    if (! ok)
        ot.error (command, (*R)().geterror());
    ot.function_times[command] += timer();
    return 0;
}



static int
action_mosaic (int argc, const char *argv[])
{
    Timer timer (ot.enable_function_timing);

    // Mosaic is tricky. We have to parse the argument before we know
    // how many images it wants to pull off the stack.
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);
    int ximages = 0, yimages = 0;
    if (sscanf (size.c_str(), "%dx%d", &ximages, &yimages) != 2 
          || ximages < 1 || yimages < 1) {
        ot.error (command, Strutil::format ("Invalid size '%s'", size));
        return 0;
    }
    int nimages = ximages * yimages;

    // Make the matrix complete with placeholder images
    ImageRecRef blank_img;
    while (ot.image_stack_depth() < nimages) {
        if (! blank_img) {
            ImageSpec blankspec (1, 1, 1, TypeDesc::UINT8);
            blank_img.reset (new ImageRec ("blank", blankspec, ot.imagecache));
            ImageBufAlgo::zero ((*blank_img)());
        }
        ot.push (blank_img);
    }

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
    ot.extract_options (options, command);
    int pad = Strutil::stoi (options["pad"]);

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
                ot.error (command, (*R)().geterror());
        }
    }

    ot.function_times[command] += timer();
    return 0;
}



BINARY_IMAGE_OP (over, ImageBufAlgo::over);


#if 0
// Don't enable this until we are sure we have a zover test in testsuite.

class OpZover : public OiiotoolOp {
public:
    OpZover (Oiiotool &ot, string_view opname,
                             int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        bool zeroisinf = Strutil::from_string<int>(options["zeroisinf"]);
        return ImageBufAlgo::zover (*img[0], *img[1], zeroisinf, ROI(), 0);
    }
};

OP_CUSTOMCLASS (zover, OpZover, 1);

#else

static int
action_zover (int argc, const char *argv[])
{
    if (ot.postpone_callback (2, action_zover, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

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
    set_roi_full (specR, roi_union (get_roi_full(specA), get_roi_full(specB)));

    ot.push (new ImageRec ("zover", specR, ot.imagecache));
    ImageBuf &Rib ((*ot.curimg)());

    bool ok = ImageBufAlgo::zover (Rib, Aib, Bib, z_zeroisinf);
    if (! ok)
        ot.error (command, Rib.geterror());
    ot.function_times[command] += timer();
    return 0;
}
#endif



class OpDeepMerge : public OiiotoolOp {
public:
    OpDeepMerge (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 2) {}
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::deep_merge (*img[0], *img[1], *img[2]);
    }
};

OP_CUSTOMCLASS (deepmerge, OpDeepMerge, 2);



class OpDeepHoldout : public OiiotoolOp {
public:
    OpDeepHoldout (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 2) {}
    virtual int impl (ImageBuf **img) {
        return ImageBufAlgo::deep_holdout (*img[0], *img[1], *img[2]);
    }
};

OP_CUSTOMCLASS (deepholdout, OpDeepHoldout, 2);



class OpDeepen : public OiiotoolOp {
public:
    OpDeepen (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual void option_defaults () { options["z"] = 1.0; }
    virtual int impl (ImageBuf **img) {
        float z = Strutil::from_string<float>(options["z"]);
        return ImageBufAlgo::deepen (*img[0], *img[1], z);
    }
};

OP_CUSTOMCLASS (deepen, OpDeepen, 1);



UNARY_IMAGE_OP (flatten, ImageBufAlgo::flatten);



static int
action_fill (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_fill, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);
    string_view size    = ot.express (argv[1]);

    // Read and copy the top-of-stack image
    ImageRecRef A (ot.pop());
    ot.read (A);
    ot.push (new ImageRec (*A, 0, 0, true, true /*copy_pixels*/));
    ImageBuf &Rib ((*ot.curimg)(0,0));
    const ImageSpec &Rspec = Rib.spec();

    int w = Rib.spec().width, h = Rib.spec().height;
    int x = Rib.spec().x, y = Rib.spec().y;
    if (! ot.adjust_geometry (argv[0], w, h, x, y, size.c_str(), true)) {
        return 0;
    }

    std::vector<float> topleft (Rspec.nchannels, 1.0f);
    std::vector<float> topright (Rspec.nchannels, 1.0f);
    std::vector<float> bottomleft (Rspec.nchannels, 1.0f);
    std::vector<float> bottomright (Rspec.nchannels, 1.0f);

    std::map<std::string,std::string> options;
    ot.extract_options (options, command);

    bool ok = true;
    if (Strutil::extract_from_list_string (topleft,     options["topleft"]) &&
        Strutil::extract_from_list_string (topright,    options["topright"]) &&
        Strutil::extract_from_list_string (bottomleft,  options["bottomleft"]) &&
        Strutil::extract_from_list_string (bottomright, options["bottomright"])) {
        ok = ImageBufAlgo::fill (Rib, &topleft[0], &topright[0],
                                 &bottomleft[0], &bottomright[0],
                                 ROI(x, x+w, y, y+h));
    }
    else if (Strutil::extract_from_list_string (topleft,    options["top"]) &&
             Strutil::extract_from_list_string (bottomleft, options["bottom"])) {
        ok = ImageBufAlgo::fill (Rib, &topleft[0], &bottomleft[0],
                                 ROI(x, x+w, y, y+h));
    }
    else if (Strutil::extract_from_list_string (topleft,  options["left"]) &&
             Strutil::extract_from_list_string (topright, options["right"])) {
        ok = ImageBufAlgo::fill (Rib, &topleft[0], &topright[0],
                                 &topleft[0], &topright[0],
                                 ROI(x, x+w, y, y+h));
    }
    else if (Strutil::extract_from_list_string (topleft, options["color"])) {
        ok = ImageBufAlgo::fill (Rib, &topleft[0], ROI(x, x+w, y, y+h));
    }
    else {
        ot.warning (command, "No recognized fill parameters: filling with white.");
        ok = ImageBufAlgo::fill (Rib, &topleft[0], ROI(x, x+w, y, y+h));
    }
    if (! ok)
        ot.error (command, Rib.geterror());

    ot.function_times[command] += timer();
    return 0;
}



static int
action_clamp (int argc, const char *argv[])
{
    if (ot.postpone_callback (1, action_clamp, argc, argv))
        return 0;
    Timer timer (ot.enable_function_timing);
    string_view command = ot.express (argv[0]);

    ImageRecRef A = ot.pop();
    ot.read (A);
    ImageRecRef R (new ImageRec (*A, ot.allsubimages ? -1 : 0,
                                 ot.allsubimages ? -1 : 0,
                                 true /*writeable*/, false /*copy_pixels*/));
    ot.push (R);
    for (int s = 0, subimages = R->subimages();  s < subimages;  ++s) {
        int nchans = (*R)(s,0).nchannels();
        const float big = std::numeric_limits<float>::max();
        std::vector<float> min (nchans, -big);
        std::vector<float> max (nchans, big);
        std::map<std::string,std::string> options;
        options["clampalpha"] = "0";  // initialize
        ot.extract_options (options, command);
        Strutil::extract_from_list_string (min, options["min"]);
        Strutil::extract_from_list_string (max, options["max"]);
        bool clampalpha01 = Strutil::stoi (options["clampalpha"]);

        for (int m = 0, miplevels=R->miplevels(s);  m < miplevels;  ++m) {
            ImageBuf &Rib ((*R)(s,m));
            ImageBuf &Aib ((*A)(s,m));
            bool ok = ImageBufAlgo::clamp (Rib, Aib, &min[0], &max[0], clampalpha01);
            if (! ok)
                ot.error (command, Rib.geterror());
        }
    }

    ot.function_times[command] += timer();
    return 0;
}



class OpRangeCompress : public OiiotoolOp {
public:
    OpRangeCompress (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        bool useluma = Strutil::from_string<int>(options["luma"]);
        return ImageBufAlgo::rangecompress (*img[0], *img[1], useluma);
    }
};

OP_CUSTOMCLASS (rangecompress, OpRangeCompress, 1);



class OpRangeExpand : public OiiotoolOp {
public:
    OpRangeExpand (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        bool useluma = Strutil::from_string<int>(options["luma"]);
        return ImageBufAlgo::rangeexpand (*img[0], *img[1], useluma);
    }
};

OP_CUSTOMCLASS (rangeexpand, OpRangeExpand, 1);




class OpBox : public OiiotoolOp {
public:
    OpBox (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        img[0]->copy (*img[1]);
        const ImageSpec &Rspec (img[0]->spec());
        int x1, y1, x2, y2;
        string_view s (args[1]);
        if (Strutil::parse_int (s, x1) && Strutil::parse_char (s, ',') &&
            Strutil::parse_int (s, y1) && Strutil::parse_char (s, ',') &&
            Strutil::parse_int (s, x2) && Strutil::parse_char (s, ',') &&
            Strutil::parse_int (s, y2)) {
            std::vector<float> color (Rspec.nchannels+1, 1.0f);
            Strutil::extract_from_list_string (color, options["color"]);
            bool fill = Strutil::from_string<int>(options["fill"]);
            return ImageBufAlgo::render_box (*img[0], x1, y1, x2, y2,
                                             color, fill);
        }
        return false;
    }
};

OP_CUSTOMCLASS (box, OpBox, 1);



class OpLine : public OiiotoolOp {
public:
    OpLine (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        img[0]->copy (*img[1]);
        const ImageSpec &Rspec (img[0]->spec());
        std::vector<int> points;
        Strutil::extract_from_list_string (points, args[1]);
        std::vector<float> color (Rspec.nchannels+1, 1.0f);
        Strutil::extract_from_list_string (color, options["color"]);
        bool closed = (points.size() > 4 &&
                       points[0] == points[points.size()-2] &&
                       points[1] == points[points.size()-1]);
        for (size_t i = 0, e = points.size()-2; i < e; i += 2)
            ImageBufAlgo::render_line (*img[0], points[i+0], points[i+1],
                                       points[i+2], points[i+3], color,
                                       closed || i>0 /*skip_first_point*/);
        return true;
    }
};

OP_CUSTOMCLASS (line, OpLine, 1);



class OpText : public OiiotoolOp {
public:
    OpText (Oiiotool &ot, string_view opname, int argc, const char *argv[])
        : OiiotoolOp (ot, opname, argc, argv, 1) {}
    virtual int impl (ImageBuf **img) {
        img[0]->copy (*img[1]);
        const ImageSpec &Rspec (img[0]->spec());
        int x = options["x"].size() ? Strutil::from_string<int>(options["x"]) : (Rspec.x + Rspec.width/2);
        int y = options["y"].size() ? Strutil::from_string<int>(options["y"]) : (Rspec.y + Rspec.height/2);
        int fontsize = options["size"].size() ? Strutil::from_string<int>(options["size"]) : 16;
        std::string font = options["font"];
        std::vector<float> textcolor (Rspec.nchannels+1, 1.0f);
        Strutil::extract_from_list_string (textcolor, options["color"]);
        std::string ax = options["xalign"];
        std::string ay = options["yalign"];
        TextAlignX alignx (TextAlignX::Left);
        TextAlignY aligny (TextAlignY::Baseline);
        if (Strutil::iequals(ax, "right") || Strutil::iequals(ax, "r"))
            alignx = TextAlignX::Right;
        if (Strutil::iequals(ax, "center") || Strutil::iequals(ax, "c"))
            alignx = TextAlignX::Center;
        if (Strutil::iequals(ay, "top") || Strutil::iequals(ay, "t"))
            aligny = TextAlignY::Top;
        if (Strutil::iequals(ay, "bottom") || Strutil::iequals(ay, "b"))
            aligny = TextAlignY::Bottom;
        if (Strutil::iequals(ay, "center") || Strutil::iequals(ay, "c"))
            aligny = TextAlignY::Center;
        int shadow = Strutil::from_string<int>(options["shadow"]);
        return ImageBufAlgo::render_text (*img[0], x, y, args[1],
                                          fontsize, font, textcolor,
                                          alignx, aligny, shadow);
    }
};

OP_CUSTOMCLASS (text, OpText, 1);




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
    string_view command = ot.express (argv[0]);
    string_view size = ot.express(argv[1]);
    int channel = Strutil::from_string<int> (ot.express(argv[2]));
    std::map<std::string,std::string> options;
    ot.extract_options (options, command);
    int cumulative = Strutil::from_string<int> (options["cumulative"]);

    // Input image.
    ot.read ();
    ImageRecRef A (ot.pop());
    const ImageBuf &Aib ((*A)());

    // Extract bins and height from size.
    int bins = 0, height = 0;
    if (sscanf (size.c_str(), "%dx%d", &bins, &height) != 2) {
        ot.error (command, Strutil::format ("Invalid size: %s", size));
        return -1;
    }

    // Compute regular histogram.
    std::vector<imagesize_t> hist;
    bool ok = ImageBufAlgo::histogram (Aib, channel, hist, bins);
    if (! ok) {
        ot.error (command, Aib.geterror());
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
        ot.error (command, Rib.geterror());

    ot.function_times[command] += timer();
    return 0;
}



static int
input_file (int argc, const char *argv[])
{
    ot.total_readtime.start();
    string_view command = ot.express (argv[0]);
    if (argc > 1 && Strutil::starts_with(command, "-i")) {
        --argc;
        ++argv;
    } else {
        command = "-i";
    }
    std::map<std::string,std::string> fileoptions;
    ot.extract_options (fileoptions, command);
    int printinfo = get_value_override (fileoptions["info"], int(ot.printinfo));
    bool readnow = get_value_override (fileoptions["now"], int(0));
    bool autocc = get_value_override (fileoptions["autocc"], int(ot.autocc));
    std::string infoformat = get_value_override (fileoptions["infoformat"],
                                                 ot.printinfo_format);
    TypeDesc input_dataformat (fileoptions["type"]);
    std::string channel_set = fileoptions["ch"];

    for (int i = 0;  i < argc;  i++) { // FIXME: this loop is pointless
        string_view filename = ot.express(argv[i]);
        std::map<std::string,ImageRecRef>::const_iterator found;
        found = ot.image_labels.find(filename);
        if (found != ot.image_labels.end()) {
            if (ot.debug)
                std::cout << "Referencing labeled image " << filename << "\n";
            ot.push (found->second);
            ot.process_pending ();
            break;
        }
        Timer timer (ot.enable_function_timing);
        int exists = 1;
        if (ot.input_config_set) {
            // User has set some input configuration, so seed the cache with
            // that information.
            ustring fn (filename);
            ot.imagecache->invalidate (fn);
            bool ok = ot.imagecache->add_file (fn, NULL, &ot.input_config);
            if (!ok) {
                std::string err = ot.imagecache->geterror();
                ot.error ("read", err.size() ? err : "(unknown error)");
                exit (1);
            }
        }
        if (! ot.imagecache->get_image_info (ustring(filename), 0, 0,
                            ustring("exists"), TypeInt, &exists)
            || !exists) {
            // Try to get a more precise error message to report
            ImageInput *input = ImageInput::create (filename);
            bool procedural = input ? input->supports ("procedural") : false;
            ImageInput::destroy (input);
            if (! Filesystem::exists(filename) && !procedural)
                ot.error ("read", Strutil::format ("File does not exist: \"%s\"", filename));
            else {
                std::string err;
                ImageInput *in = ImageInput::open (filename);
                if (in) {
                    err = in->geterror();
                    in->close ();
                    delete in;
                } else {
                    err = OIIO::geterror();
                }
                ot.error ("read", err.size() ? err : "(unknown error)");
            }
            exit (1);
        }

        if (channel_set.size()) {
            ot.input_channel_set = channel_set;
            readnow = true;
        }

        if (ot.debug || ot.verbose)
            std::cout << "Reading " << filename << "\n";
        ot.push (ImageRecRef (new ImageRec (filename, ot.imagecache)));
        ot.curimg->configspec (ot.input_config);
        ot.curimg->input_dataformat (input_dataformat);
        if (readnow) {
            ot.curimg->read (ReadNoCache, channel_set);
            // If we do not yet have an expected output format, set it based on
            // this image (presumably the first one read.
            if (ot.output_dataformat == TypeDesc::UNKNOWN) {
                const ImageSpec &nspec ((*ot.curimg)(0,0).nativespec());
                ot.output_dataformat = nspec.format;
                if (! ot.output_bitspersample)
                    ot.output_bitspersample = nspec.get_int_attribute ("oiio:BitsPerSample");
                if (nspec.channelformats.size()) {
                    for (int c = 0; c < nspec.nchannels; ++c) {
                        std::string chname = nspec.channelnames[c];
                        ot.output_channelformats[chname] = std::string(nspec.channelformat(c).c_str());
                    }
                }
            }
        }
        if (printinfo || ot.printstats || ot.dumpdata || ot.hash) {
            OiioTool::print_info_options pio;
            pio.verbose = ot.verbose || printinfo > 1 || ot.printinfo_verbose;
            pio.subimages = ot.allsubimages || printinfo > 1;
            pio.compute_stats = ot.printstats;
            pio.dumpdata = ot.dumpdata;
            pio.dumpdata_showempty = ot.dumpdata_showempty;
            pio.compute_sha1 = ot.hash;
            pio.metamatch = ot.printinfo_metamatch;
            pio.nometamatch = ot.printinfo_nometamatch;
            pio.infoformat = infoformat;
            long long totalsize = 0;
            std::string error;
            bool ok = OiioTool::print_info (ot, filename, pio, totalsize, error);
            if (! ok)
                ot.error ("read", error);
            ot.printed_info = true;
        }
        ot.function_times["input"] += timer();
        if (ot.autoorient) {
            int action_reorient (int argc, const char *argv[]);
            const char *argv[] = { "--reorient" };
            action_reorient (1, argv);
        }

        if (autocc) {
            // Try to deduce the color space it's in
            string_view colorspace (ot.colorconfig.parseColorSpaceFromString(filename));
            if (colorspace.size() && ot.debug)
                std::cout << "  From " << filename << ", we deduce color space \""
                          << colorspace << "\"\n";
            if (colorspace.empty()) {
                ot.read ();
                colorspace = ot.curimg->spec()->get_string_attribute ("oiio:ColorSpace");
                if (ot.debug)
                    std::cout << "  Metadata of " << filename << " indicates color space \""
                              << colorspace << "\"\n";
            }
            string_view linearspace = ot.colorconfig.getColorSpaceNameByRole("linear");
            if (linearspace.empty())
                linearspace = string_view("Linear");
            if (colorspace.size() && !Strutil::iequals(colorspace,linearspace)) {
                const char *argv[] = { "colorconvert:strict=0", colorspace.c_str(),
                                       linearspace.c_str() };
                if (ot.debug)
                    std::cout << "  Converting " << filename << " from "
                              << colorspace << " to " << linearspace << "\n";
                action_colorconvert (3, argv);
            }
        }

        ot.process_pending ();
    }

    if (ot.input_config_set) {
        ot.input_config = ImageSpec();
        ot.input_config_set = false;
    }
    ot.input_channel_set.clear ();
    ot.check_peak_memory ();
    ot.total_readtime.stop();
    return 0;
}



static void
prep_texture_config (ImageSpec &configspec,
                     std::map<std::string,std::string> &fileoptions)
{
    configspec.tile_width  = ot.output_tilewidth  ? ot.output_tilewidth  : 64;
    configspec.tile_height = ot.output_tileheight ? ot.output_tileheight : 64;
    configspec.tile_depth  = 1;
    string_view wrap = get_value_override (fileoptions["wrap"], "black");
    string_view swrap = get_value_override (fileoptions["swrap"], wrap);
    string_view twrap = get_value_override (fileoptions["twrap"], wrap);
    configspec.attribute ("wrapmodes", Strutil::format("%s,%s", swrap, twrap));
    configspec.attribute ("maketx:verbose", ot.verbose);
    configspec.attribute ("maketx:runstats", ot.runstats);
    configspec.attribute ("maketx:resize",
                          get_value_override(fileoptions["resize"], 0));
    configspec.attribute ("maketx:nomipmap",
                          get_value_override(fileoptions["nomipmap"], 0));
    configspec.attribute ("maketx:updatemode",
                          get_value_override(fileoptions["updatemode"], 0));
    configspec.attribute ("maketx:constant_color_detect",
                          get_value_override(fileoptions["constant_color_detect"], 0));
    configspec.attribute ("maketx:monochrome_detect",
                          get_value_override(fileoptions["monochrome_detect"], 0));
    configspec.attribute ("maketx:opaque_detect",
                          get_value_override(fileoptions["opaque_detect"], 0));
    configspec.attribute ("maketx:compute_average",
                          get_value_override(fileoptions["compute_average"], 1));
    configspec.attribute ("maketx:unpremult",
                          get_value_override(fileoptions["unpremult"], 0));
    configspec.attribute ("maketx:incolorspace",
                          get_value_override(fileoptions["incolorspace"], ""));
    configspec.attribute ("maketx:outcolorspace",
                          get_value_override(fileoptions["outcolorspace"], ""));
    configspec.attribute ("maketx:highlightcomp",
                          get_value_override(fileoptions["highlightcomp"],
                            get_value_override(fileoptions["hilightcomp"],
                              get_value_override(fileoptions["hicomp"], 0))));
    configspec.attribute ("maketx:sharpen",
                          get_value_override(fileoptions["sharpen"], 0.0f));
    if (fileoptions["filter"].size() || fileoptions["filtername"].size())
        configspec.attribute ("maketx:filtername",
                              get_value_override(fileoptions["filtername"],
                                                 get_value_override(fileoptions["filter"], "")));
    if (fileoptions["fileformatname"].size())
        configspec.attribute ("maketx:fileformatname",
                              get_value_override(fileoptions["fileformatname"], ""));
    configspec.attribute ("maketx:prman_metadata",
                          get_value_override(fileoptions["prman_metadata"], 0));
    configspec.attribute ("maketx:oiio_options",
                          get_value_override(fileoptions["oiio_options"],
                                             get_value_override(fileoptions["oiio"])));
    configspec.attribute ("maketx:prman_options",
                          get_value_override(fileoptions["prman_options"],
                                             get_value_override(fileoptions["prman"])));
    // if (mipimages.size())
    //     configspec.attribute ("maketx:mipimages", Strutil::join(mipimages,";"));

    std::string software = configspec.get_string_attribute ("Software");
    if (software.size())
        configspec.attribute ("maketx:full_command_line", software);
}



static int
output_file (int argc, const char *argv[])
{
    Timer timer (ot.enable_function_timing);
    ot.total_writetime.start();
    string_view command = ot.express (argv[0]);
    string_view filename = ot.express (argv[1]);

    std::map<std::string,std::string> fileoptions;
    ot.extract_options (fileoptions, command);

    string_view stripped_command = command;
    Strutil::parse_char (stripped_command, '-');
    Strutil::parse_char (stripped_command, '-');    
    bool do_tex = Strutil::starts_with (stripped_command, "otex");
    bool do_latlong = Strutil::starts_with (stripped_command, "oenv") ||
                      Strutil::starts_with (stripped_command, "olatlong");
    bool do_shad = Strutil::starts_with (stripped_command, "oshad");
    bool do_bumpslopes = Strutil::starts_with (stripped_command, "obump");

    if (ot.debug)
        std::cout << "Output: " << filename << "\n";
    if (! ot.curimg.get()) {
        ot.warning (command, Strutil::format("%s did not have any current image to output.", filename));
        return 0;
    }

    if (fileoptions["all"].size()) {
        // Special case: if they requested outputting all images on the
        // stack, handle it recursively. The filename, then, is the pattern,
        // presumed to have a %d in it somewhere, which we will substitute
        // with the image index.
        int startnumber = Strutil::from_string<int>(fileoptions["all"]);
        int nimages = 1 /*curimg*/ + int(ot.image_stack.size());
        const char *new_argv[2];
        // Git rid of the ":all=" part of the command so we don't infinitely
        // recurse.
        std::string newcmd = regex_replace (command.str(),
                                                   regex(":all=[0-9]+"), "");
        new_argv[0] = newcmd.c_str();;
        ImageRecRef saved_curimg = ot.curimg; // because we'll overwrite it
        for (int i = 0; i < nimages; ++i) {
            if (i < nimages-1)
                ot.curimg = ot.image_stack[i];
            else
                ot.curimg = saved_curimg;  // note: last iteration also restores it!
            // Use the filename as a pattern, format with the frame number
            new_argv[1] = ustring::format(filename.c_str(), i+startnumber).c_str();
            // recurse for this file
            output_file (2, new_argv);
        }
        return 0;
    }

    if (ot.noclobber && Filesystem::exists(filename)) {
        ot.warning (command, Strutil::format("%s already exists, not overwriting.", filename));
        return 0;
    }
    string_view formatname = fileoptions["fileformatname"];
    if (formatname.empty())
        formatname = filename;
    std::unique_ptr<ImageOutput> out (ImageOutput::create (formatname));
    if (! out) {
        std::string err = OIIO::geterror();
        ot.error (command, err.size() ? err.c_str() : "unknown error creating an ImageOutput");
        return 0;
    }
    bool supports_displaywindow = out->supports ("displaywindow");
    bool supports_negativeorigin = out->supports ("negativeorigin");
    bool supports_tiles = out->supports ("tiles") || ot.output_force_tiles;
    ot.read ();
    ImageRecRef saveimg = ot.curimg;
    ImageRecRef ir (ot.curimg);
    TypeDesc saved_output_dataformat = ot.output_dataformat;
    int saved_bitspersample = ot.output_bitspersample;

    timer.stop();   // resume after all these auto-transforms

    // Automatically drop channels we can't support in output
    if ((ir->spec()->nchannels > 4 && ! out->supports("nchannels")) ||
        (ir->spec()->nchannels > 3 && ! out->supports("alpha"))) {
        bool alpha = (ir->spec()->nchannels > 3 && out->supports("alpha"));
        string_view chanlist = alpha ? "R,G,B,A" : "R,G,B";
        std::vector<int> channels;
        bool found = parse_channels (*ir->spec(), chanlist, channels);
        if (! found)
            chanlist = alpha ? "0,1,2,3" : "0,1,2";
        const char *argv[] = { "channels", chanlist.c_str() };
        int action_channels (int argc, const char *argv[]); // forward decl
        action_channels (2, argv);
        ot.warning (command, Strutil::format("Can't save %d channels to %f... saving only %s",
                                              ir->spec()->nchannels, out->format_name(), chanlist.c_str()));
        ir = ot.curimg;
    }

    // Handle --autotrim
    int autotrim = get_value_override (fileoptions["autotrim"], ot.output_autotrim);
    if (supports_displaywindow && autotrim) {
        ROI origroi = get_roi(*ir->spec(0,0));
        ROI roi = ImageBufAlgo::nonzero_region ((*ir)(0,0), origroi);
        if (roi.npixels() == 0) {
            // Special case -- all zero; but doctor to make it 1 zero pixel
            roi = origroi;
            roi.xend = roi.xbegin+1;
            roi.yend = roi.ybegin+1;
            roi.zend = roi.zbegin+1;
        }
        std::string crop = (ir->spec(0,0)->depth == 1)
            ? format_resolution (roi.width(), roi.height(),
                                 roi.xbegin, roi.ybegin)
            : format_resolution (roi.width(), roi.height(), roi.depth(),
                                 roi.xbegin, roi.ybegin, roi.zbegin);
        const char *argv[] = { "crop", crop.c_str() };
        int action_crop (int argc, const char *argv[]); // forward decl
        action_crop (2, argv);
        ir = ot.curimg;
    }

    // Automatically crop/pad if outputting to a format that doesn't
    // support display windows, unless autocrop is disabled.
    int autocrop = get_value_override (fileoptions["autocrop"], ot.output_autocrop);
    if (! supports_displaywindow && autocrop &&
        (ir->spec()->x != ir->spec()->full_x ||
         ir->spec()->y != ir->spec()->full_y ||
         ir->spec()->width != ir->spec()->full_width ||
         ir->spec()->height != ir->spec()->full_height)) {
        const char *argv[] = { "croptofull" };
        int action_croptofull (int argc, const char *argv[]); // forward decl
        action_croptofull (1, argv);
        ir = ot.curimg;
    }

    // See if the filename appears to contain a color space name embedded.
    // Automatically color convert if --autocc is used and the current
    // color space doesn't match that implied by the filename, and
    // automatically set -d based on the name if --autod is used.
    int autocc = get_value_override (fileoptions["autocc"], ot.autocc);
    string_view outcolorspace = ot.colorconfig.parseColorSpaceFromString(filename);
    if (autocc && outcolorspace.size()) {
        TypeDesc type;
        int bits;
        type = ot.colorconfig.getColorSpaceDataType(outcolorspace,&bits);
        if (type.basetype != TypeDesc::UNKNOWN) {
            if (ot.debug)
                std::cout << "  Deduced data type " << type << " (" << bits
                          << "bits) for output to " << filename << "\n";
            if ((ot.output_dataformat && ot.output_dataformat != type)
                || (bits && ot.output_bitspersample && ot.output_bitspersample != bits)) {
                std::string msg = Strutil::format (
                    "Output filename colorspace \"%s\" implies %s (%d bits), overriding prior request for %s.",
                    outcolorspace, type, bits, ot.output_dataformat);
                ot.warning (command, msg);
            }
            ot.output_dataformat = type;
            ot.output_bitspersample = bits;
        }
    }
    if (autocc) {
        string_view linearspace = ot.colorconfig.getColorSpaceNameByRole("linear");
        if (linearspace.empty())
            linearspace = string_view("Linear");
        string_view currentspace = ir->spec()->get_string_attribute ("oiio:ColorSpace", linearspace);
        // Special cases where we know formats should be particular color
        // spaces
        if (outcolorspace.empty() && (Strutil::iends_with (filename, ".jpg") ||
                                      Strutil::iends_with (filename, ".jpeg") ||
                                      Strutil::iends_with (filename, ".gif")))
            outcolorspace = string_view("sRGB");
        if (outcolorspace.size() && currentspace != outcolorspace) {
            if (ot.debug)
                std::cout << "  Converting from " << currentspace << " to "
                          << outcolorspace << " for output to " << filename << "\n";
            const char *argv[] = { "colorconvert:strict=0",
                                   currentspace.c_str(), outcolorspace.c_str() };
            action_colorconvert (3, argv);
            ir = ot.curimg;
        }
    }

    // Automatically crop out the negative areas if outputting to a format
    // that doesn't support negative origins.
    if (! supports_negativeorigin && autocrop &&
        (ir->spec()->x < 0 || ir->spec()->y < 0 || ir->spec()->z < 0)) {
        ROI roi = get_roi (*ir->spec(0,0));
        roi.xbegin = std::max (0, roi.xbegin);
        roi.ybegin = std::max (0, roi.ybegin);
        roi.zbegin = std::max (0, roi.zbegin);
        std::string crop = (ir->spec(0,0)->depth == 1)
            ? format_resolution (roi.width(), roi.height(),
                                 roi.xbegin, roi.ybegin)
            : format_resolution (roi.width(), roi.height(), roi.depth(),
                                 roi.xbegin, roi.ybegin, roi.zbegin);
        const char *argv[] = { "crop", crop.c_str() };
        int action_crop (int argc, const char *argv[]); // forward decl
        action_crop (2, argv);
        ir = ot.curimg;
    }

    if (ot.dryrun) {
        ot.curimg = saveimg;
        ot.output_dataformat = saved_output_dataformat;
        ot.output_bitspersample = saved_bitspersample;
        return 0;
    }

    timer.start();
    if (ot.debug || ot.verbose)
        std::cout << "Writing " << filename << "\n";

    // FIXME -- the various automatic transformations above neglect to handle
    // MIPmaps or subimages with full generality.

    bool ok = true;
    if (do_tex || do_latlong || do_bumpslopes) {
        ImageSpec configspec;
        adjust_output_options (filename, configspec, nullptr,
                               ot, supports_tiles, fileoptions);
        prep_texture_config (configspec, fileoptions);
        ImageBufAlgo::MakeTextureMode mode = ImageBufAlgo::MakeTxTexture;
        if (do_shad)
            mode = ImageBufAlgo::MakeTxShadow;
        if (do_latlong)
            mode = ImageBufAlgo::MakeTxEnvLatl;
        if(do_bumpslopes)
            mode = ImageBufAlgo::MakeTxBumpWithSlopes;
        // if (lightprobemode)
        //     mode = ImageBufAlgo::MakeTxEnvLatlFromLightProbe;
        ok = ImageBufAlgo::make_texture (mode, (*ir)(0,0), filename,
                                         configspec, &std::cout);
        if (!ok)
            ot.error (command, "Could not make texture");
        // N.B. make_texture already internally writes to a temp file and
        // then atomically moves it to the final destination, so we don't
        // need to explicitly do that here.
    } else {
        // Non-texture case
        std::vector<ImageSpec> subimagespecs (ir->subimages());
        for (int s = 0;  s < ir->subimages();  ++s) {
            ImageSpec spec = *ir->spec(s,0);
            adjust_output_options (filename, spec, ir->nativespec(s),
                                   ot, supports_tiles,
                                   fileoptions, (*ir)[s].was_direct_read());
            // For deep files, must copy the native deep channelformats
            if (spec.deep)
                spec.channelformats = (*ir)(s,0).nativespec().channelformats;
            // If it's not tiled and MIP-mapped, remove any "textureformat"
            if (! spec.tile_pixels() || ir->miplevels(s) <= 1)
                spec.erase_attribute ("textureformat");
            subimagespecs[s] = spec;
        }

        // Write the output to a temp file first, then rename it to the
        // final destination (same directory). This improves robustness.
        // There is less chance a crash during execution will leave behind a
        // partially formed file, and it also protects us against corrupting
        // an input if they are "oiiotooling in place" (especially
        // problematic for large files that are ImageCache-based and so only
        // partially read at the point that we open the file. We also force
        // a unique filename to protect against multiple processes running
        // at the same time on the same file.
        std::string extension = Filesystem::extension(filename);
        std::string tmpfilename = Filesystem::replace_extension (filename, ".%%%%%%%%.temp"+extension);
        tmpfilename = Filesystem::unique_path(tmpfilename);

        // Do the initial open
        ImageOutput::OpenMode mode = ImageOutput::Create;
        if (ir->subimages() > 1 && out->supports("multiimage")) {
            if (! out->open (tmpfilename, ir->subimages(), &subimagespecs[0])) {
                std::string err = out->geterror();
                ot.error (command, err.size() ? err.c_str() : "unknown error");
                return 0;
            }
        } else {
            if (! out->open (tmpfilename, subimagespecs[0], mode)) {
                std::string err = out->geterror();
                ot.error (command, err.size() ? err.c_str() : "unknown error");
                return 0;
            }
        }

        // Output all the subimages and MIP levels
        for (int s = 0, send = ir->subimages();  s < send;  ++s) {
            for (int m = 0, mend = ir->miplevels(s);  m < mend && ok;  ++m) {
                ImageSpec spec = *ir->spec(s,m);
                adjust_output_options (filename, spec, ir->nativespec(s,m),
                                       ot, supports_tiles,
                                       fileoptions, (*ir)[s].was_direct_read());
                if (s > 0 || m > 0) {  // already opened first subimage/level
                    if (! out->open (tmpfilename, spec, mode)) {
                        std::string err = out->geterror();
                        ot.error (command, err.size() ? err.c_str() : "unknown error");
                        ok = false;
                        break;
                    }
                }
                if (! (*ir)(s,m).write (out.get())) {
                    ot.error (command, (*ir)(s,m).geterror());
                    ok = false;
                    break;
                }
                ot.check_peak_memory();
                if (mend > 1) {
                    if (out->supports("mipmap")) {
                        mode = ImageOutput::AppendMIPLevel;  // for next level
                    } else if (out->supports("multiimage")) {
                        mode = ImageOutput::AppendSubimage;
                    } else {
                        ot.warning (command, Strutil::format ("%s does not support MIP-maps for %s",
                                                               out->format_name(), filename));
                        break;
                    }
                }
            }
            mode = ImageOutput::AppendSubimage;  // for next subimage
            if (send > 1 && ! out->supports("multiimage")) {
                ot.warning (command, Strutil::format ("%s does not support multiple subimages for %s",
                                                       out->format_name(), filename));
                break;
            }
        }

        out->close ();
        out.reset ();    // make extra sure it's cleaned up

        // We wrote to a temporary file, so now atomically move it to the
        // original desired location.
        if (ok) {
            std::string err;
            ok = Filesystem::rename (tmpfilename, filename, err);
            if (! ok)
                ot.error (command, Strutil::format("oiiotool ERROR: could not move temp file %s to %s: %s",
                                                   tmpfilename, filename, err));
        }
        if (! ok)
            Filesystem::remove (tmpfilename);
    }

    // Make sure to invalidate any IC entries that think they are the
    // file we just wrote.
    ot.imagecache->invalidate (ustring(filename));

    if (ot.output_adjust_time && ok) {
        std::string metadatatime = ir->spec(0,0)->get_string_attribute ("DateTime");
        std::time_t in_time = ir->time();
        if (! metadatatime.empty())
            DateTime_to_time_t (metadatatime.c_str(), in_time);
        Filesystem::last_write_time (filename, in_time);
    }

    ot.check_peak_memory();
    ot.curimg = saveimg;
    ot.output_dataformat = saved_output_dataformat;
    ot.output_bitspersample = saved_bitspersample;
    ot.curimg->was_output (true);
    ot.total_writetime.stop();
    double optime = timer();
    ot.function_times[command] += optime;
    ot.num_outputs += 1;

    if (ot.debug)
        Strutil::printf ("    output took %s  (total time %s, mem %s)\n",
                         Strutil::timeintervalformat(optime,2),
                         Strutil::timeintervalformat(ot.total_runtime(),2),
                         Strutil::memformat(Sysutil::memory_used()));
    return 0;
}



static int
do_echo (int argc, const char *argv[])
{
    ASSERT (argc == 2);

    string_view command = ot.express (argv[0]);
    string_view message = ot.express (argv[1]);

    std::map<std::string,std::string> options;
    options["newline"] = "1";
    ot.extract_options (options, command);
    int newline = Strutil::from_string<int>(options["newline"]);

    std::cout << message;
    for (int i = 0; i < newline; ++i)
        std::cout << '\n';
    std::cout.flush();
    ot.printed_info = true;
    return 0;
}



// Concatenate the command line into one string, optionally filtering out
// verbose attribute commands.
static std::string
command_line_string (int argc, char * argv[], bool sansattrib)
{
    std::string s;
    for (int i = 0;  i < argc;  ++i) {
        if (sansattrib) {
            // skip any filtered attributes
            if (Strutil::starts_with(argv[i], "--attrib") || Strutil::starts_with(argv[i], "-attrib") ||
                Strutil::starts_with(argv[i], "--sattrib") || Strutil::starts_with(argv[i], "-sattrib")) {
                i += 2;  // also skip the following arguments
                continue;
            }
            if (Strutil::starts_with(argv[i], "--sansattrib") || Strutil::starts_with(argv[i], "-sansattrib")) {
                continue;
            }
        }
        if (strchr (argv[i], ' ')) {  // double quote args with spaces
            s += '\"';
            s += argv[i];
            s += '\"';
        } else {
            s += argv[i];
        }
        if (i < argc-1)
            s += ' ';
    }
    return s;
}



static std::string
formatted_format_list (string_view format_typename, string_view attr)
{
    int columns = Sysutil::terminal_columns() - 2;
    std::stringstream s;
    s << format_typename << " formats supported: ";
    std::string format_list;
    OIIO::getattribute (attr, format_list);
    std::vector<string_view> formats;
    Strutil::split (format_list, formats, ",");
    std::sort (formats.begin(), formats.end());
    format_list = Strutil::join (formats, ", ");
    s << format_list;
    return Strutil::wordwrap(s.str(), columns, 4);
}



static void
print_usage_tips (const ArgParse &ap, std::ostream &out)
{
    int columns = Sysutil::terminal_columns() - 2;

    out <<
      "Important usage tips:\n"
      << Strutil::wordwrap(
          "  * The oiiotool command line is processed in order, LEFT to RIGHT.\n",
          columns, 4)
      << Strutil::wordwrap(
          "  * The command line consists of image NAMES ('image.tif') and "
          "COMMANDS ('--over'). Commands start with dashes (one or two dashes "
          "are equivalent). Some commands have required arguments which "
          "must follow on the command line. For example, the '-o' command is "
          "followed by a filename.\n",
          columns, 4)
      << Strutil::wordwrap(
          "  * oiiotool is STACK-based: naming an image pushes it on the stack, and "
          "most commands pop the top image (or sometimes more than one image), "
          "perform a calculation, and push the result image back on the stack. "
          "For example, the '--over' command pops the top two images off the "
          "stack, composites them, then pushes the result back onto the stack.\n",
          columns, 4)
      << Strutil::wordwrap(
          "  * Some commands allow one or more optional MODIFIERS in the form "
          "'name=value', which are appended directly to the command itself "
          "(no spaces), separated by colons ':'. For example,\n",
          columns, 4)
      <<  "       oiiotool in.tif --text:x=100:y=200:color=1,0,0 \"Hello\" -o out.tif\n"
      << Strutil::wordwrap(
          "  * Using numerical wildcards will run the whole command line on each of "
          "several sequentially-named files, for example:\n",
          columns, 4)
      << "       oiiotool fg.#.tif bg.#.tif -over -o comp.#.tif\n"
      << "   See the manual for info about subranges, number of digits, etc.\n"
      << "\n";
}



static void
print_help_end (const ArgParse &ap, std::ostream &out)
{
    out << "\n";
    int columns = Sysutil::terminal_columns() - 2;

    out << formatted_format_list ("Input", "input_format_list") << "\n";
    out << formatted_format_list ("Output", "output_format_list") << "\n";

    // debugging color space names
    std::stringstream s;
    s << "Color configuration: " << ot.colorconfig.configname() << "\n";
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
    out << Strutil::wordwrap(s.str(), columns, 4) << "\n";

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
        out << Strutil::wordwrap(s.str(), columns, 4) << "\n";
    }

    const char *default_display = ot.colorconfig.getDefaultDisplayName();
    int ndisplays = ot.colorconfig.getNumDisplays();
    if (ndisplays) {
        std::stringstream s;
        s << "Known displays: ";
        for (int i = 0; i < ndisplays; ++i) {
            const char *d = ot.colorconfig.getDisplayNameByIndex(i);
            s << "\"" << d << "\"";
            if (! strcmp(d, default_display))
                s << "*";
            const char *default_view = ot.colorconfig.getDefaultViewName(d);
            int nviews = ot.colorconfig.getNumViews(d);
            if (nviews) {
                s << " (views: ";
                for (int i = 0; i < nviews; ++i) {
                    const char *v = ot.colorconfig.getViewNameByIndex(d, i);
                    s << "\"" << v << "\"";
                    if (! strcmp(v, default_view))
                        s << "*";
                    if (i < nviews-1)
                        s << ", ";
                }
                s << ")";
            }
            if (i < ndisplays-1)
                s << ", ";
        }
        s << " (* = default)";
        out << Strutil::wordwrap(s.str(), columns, 4) << "\n";
    }
    if (! ot.colorconfig.supportsOpenColorIO())
        out << "No OpenColorIO support was enabled at build time.\n";
    std::string libs = OIIO::get_string_attribute("library_list");
    if (libs.size()) {
        std::vector<string_view> libvec;
        Strutil::split (libs, libvec, ";");
        for (auto& lib : libvec) {
            size_t pos = lib.find(':');
            lib.remove_prefix (pos+1);
        }
        out << "Dependent libraries:\n    "
            << Strutil::wordwrap(Strutil::join (libvec, ", "), columns, 4)
            << std::endl;
    }

    // Print the path to the docs. If found, use the one installed in the
    // same area is this executable, otherwise just point to the copy on
    // GitHub corresponding to our version of the softare.
    out << "Full OIIO documentation can be found at\n";
    std::string path = Sysutil::this_program_path();
    path = Filesystem::parent_path (path);
    path = Filesystem::parent_path (path);
    path += "/share/doc/OpenImageIO/openimageio.pdf";
    if (Filesystem::exists(path))
        out << "    " << path << "\n";
    else {
        std::string branch;
        if (Strutil::ends_with (OIIO_VERSION_STRING, "dev"))
            branch = "master";
        else
            branch = Strutil::format ("RB-%d.%d", OIIO_VERSION_MAJOR, OIIO_VERSION_MINOR);
        std::string docsurl = Strutil::format("https://github.com/OpenImageIO/oiio/blob/%s/src/doc/openimageio.pdf",
                                              branch);
        out << "    " << docsurl << "\n";
    }
}



static void
print_help (ArgParse &ap)
{
    ap.set_preoption_help (print_usage_tips);
    ap.set_postoption_help (print_help_end);

    ap.usage ();
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;

    bool sansattrib = false;
    for (int i = 0; i < argc; ++i)
        if (!strcmp(argv[i],"--sansattrib") || !strcmp(argv[i],"-sansattrib"))
            sansattrib = true;
    ot.full_command_line = command_line_string (argc, argv, sansattrib);

    ArgParse ap (argc, (const char **)argv);
    ap.options ("oiiotool -- simple image processing operations\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  oiiotool [filename|command]...\n",
                "%*", input_file, "",
                "<SEPARATOR>", "Options (general):",
                "--help", &help, "Print help message",
                "-v", &ot.verbose, "Verbose status messages",
                "-q %!", &ot.verbose, "Quiet mode (turn verbose off)",
                "-n", &ot.dryrun, "No saved output (dry run)",
                "-a", &ot.allsubimages, "Do operations on all subimages/miplevels",
                "--debug", &ot.debug, "Debug mode",
                "--runstats", &ot.runstats, "Print runtime statistics",
                "--info %@", set_printinfo, NULL, "Print resolution and basic info on all inputs, detailed metadata if -v is also used (options: format=xml:verbose=1)",
                "--echo %@ %s", do_echo, NULL, "Echo message to console (options: newline=0)",
                "--metamatch %s", &ot.printinfo_metamatch,
                    "Regex: which metadata is printed with -info -v",
                "--no-metamatch %s", &ot.printinfo_nometamatch,
                    "Regex: which metadata is excluded with -info -v",
                "--stats", &ot.printstats, "Print pixel statistics on all inputs",
                "--dumpdata %@", set_dumpdata, NULL, "Print all pixel data values (options: empty=0)",
                "--hash", &ot.hash, "Print SHA-1 hash of each input image",
                "--colorcount %@ %s", action_colorcount, NULL,
                    "Count of how many pixels have the given color (argument: color;color;...) (options: eps=color)",
                "--rangecheck %@ %s %s", action_rangecheck, NULL, NULL,
                    "Count of how many pixels are outside the low and high color arguments (each is a comma-separated color value list)",
//                "-u", &ot.updatemode, "Update mode: skip outputs when the file exists and is newer than all inputs",
                "--no-clobber", &ot.noclobber, "Do not overwrite existing files",
                "--noclobber", &ot.noclobber, "", // synonym
                "--threads %@ %d", set_threads, NULL, "Number of threads (default 0 == #cores)",
                "--frames %s", NULL, "Frame range for '#' or printf-style wildcards",
                "--framepadding %d", &ot.frame_padding, "Frame number padding digits (ignored when using printf-style wildcards)",
                "--views %s", NULL, "Views for %V/%v wildcards (comma-separated, defaults to left,right)",
                "--wildcardoff", NULL, "Disable numeric wildcard expansion for subsequent command line arguments",
                "--wildcardon", NULL, "Enable numeric wildcard expansion for subsequent command line arguments",
                "--no-autopremult %@", unset_autopremult, NULL, "Turn off automatic premultiplication of images with unassociated alpha",
                "--autopremult %@", set_autopremult, NULL, "Turn on automatic premultiplication of images with unassociated alpha",
                "--autoorient", &ot.autoorient, "Automatically --reorient all images upon input",
                "--auto-orient", &ot.autoorient, "", // symonym for --autoorient
                "--autocc", &ot.autocc, "Automatically color convert based on filename",
                "--noautocc %!", &ot.autocc, "Turn off automatic color conversion",
                "--native %@", set_native, &ot.nativeread, "Keep native pixel data type (bypass cache if necessary)",
                "--cache %@ %d", set_cachesize, &ot.cachesize, "ImageCache size (in MB: default=4096)",
                "--autotile %@ %d", set_autotile, &ot.autotile, "Autotile size for cached images (default=4096)",
                "<SEPARATOR>", "Commands that read images:",
                "-i %@ %s", input_file, NULL, "Input file (argument: filename) (options: now=, printinfo=, autocc=, type=, ch=)",
                "--iconfig %@ %s %s", set_input_attribute, NULL, NULL, "Sets input config attribute (name, value) (options: type=...)",
                "<SEPARATOR>", "Commands that write images:",
                "-o %@ %s", output_file, NULL, "Output the current image to the named file",
                "-otex %@ %s", output_file, NULL, "Output the current image as a texture",
                "-oenv %@ %s", output_file, NULL, "Output the current image as a latlong env map",
                "-obump %@ %s", output_file, NULL, "Output the current normal or height texture map as a 6 channels bump texture including the first and second moment of slopes",
                "<SEPARATOR>", "Options that affect subsequent image output:",
                "-d %@ %s", set_dataformat, NULL,
                    "'-d TYPE' sets the output data format of all channels, "
                    "'-d CHAN=TYPE' overrides a single named channel (multiple -d args are allowed). "
                    "Data types include: uint8, sint8, uint10, uint12, uint16, sint16, uint32, sint32, half, float, double",
                "--scanline", &ot.output_scanline, "Output scanline images",
                "--tile %@ %d %d", output_tiles, &ot.output_tilewidth, &ot.output_tileheight,
                    "Output tiled images (tilewidth, tileheight)",
                "--force-tiles", &ot.output_force_tiles, "", // undocumented
                "--compression %s", &ot.output_compression, "Set the compression method",
                "--quality %d", &ot.output_quality, "Set the compression quality, 1-100",
                "--dither", &ot.output_dither, "Add dither to 8-bit output",
                "--planarconfig %s", &ot.output_planarconfig,
                    "Force planarconfig (contig, separate, default)",
                "--adjust-time", &ot.output_adjust_time,
                    "Adjust file times to match DateTime metadata",
                "--noautocrop %!", &ot.output_autocrop, 
                    "Do not automatically crop images whose formats don't support separate pixel data and full/display windows",
                "--autotrim", &ot.output_autotrim, 
                    "Automatically trim black borders upon output to file formats that support separate pixel data and full/display windows",
                "<SEPARATOR>", "Options that change current image metadata (but not pixel values):",
                "--attrib %@ %s %s", set_any_attribute, NULL, NULL, "Sets metadata attribute (name, value) (options: type=...)",
                "--sattrib %@ %s %s", set_string_attribute, NULL, NULL, "Sets string metadata attribute (name, value)",
                "--eraseattrib %@ %s", erase_attribute, NULL, "Erase attributes matching regex",
                "--caption %@ %s", set_caption, NULL, "Sets caption (ImageDescription metadata)",
                "--keyword %@ %s", set_keyword, NULL, "Add a keyword",
                "--clear-keywords %@", clear_keywords, NULL, "Clear all keywords",
                "--nosoftwareattrib", &ot.metadata_nosoftwareattrib, "Do not write command line into Exif:ImageHistory, Software metadata attributes",
                "--sansattrib", &sansattrib, "Write command line into Software & ImageHistory but remove --sattrib and --attrib options",
                "--orientation %@ %d", set_orientation, NULL, "Set the assumed orientation",
                "--orientcw %@", rotate_orientation, NULL, "Rotate orientation metadata 90 deg clockwise",
                "--orientccw %@", rotate_orientation, NULL, "Rotate orientation metadata 90 deg counter-clockwise",
                "--orient180 %@", rotate_orientation, NULL, "Rotate orientation metadata 180 deg",
                "--rotcw %@", rotate_orientation, NULL, "", // DEPRECATED(1.5), back compatibility
                "--rotccw %@", rotate_orientation, NULL, "", // DEPRECATED(1.5), back compatibility
                "--rot180 %@", rotate_orientation, NULL, "", // DEPRECATED(1.5), back compatibility
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
                        "Create a patterned image (args: pattern, geom, channels). Patterns: black, fill, checker, noise",
                "--kernel %@ %s %s", action_kernel, NULL, NULL,
                        "Create a centered convolution kernel (args: name, geom)",
                "--capture %@", action_capture, NULL,
                        "Capture an image (options: camera=%d)",
                "--diff %@", action_diff, NULL, "Print report on the difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--pdiff %@", action_pdiff, NULL, "Print report on the perceptual difference of two images (modified by --fail, --failpercent, --hardfail, --warn, --warnpercent --hardwarn)",
                "--add %@", action_add, NULL, "Add two images",
                "--addc %s %@", action_addc, NULL, "Add to all channels a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--cadd %s %@", action_addc, NULL, "", // Deprecated synonym
                "--sub %@", action_sub, NULL, "Subtract two images",
                "--subc %s %@", action_subc, NULL, "Subtract from all channels a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--csub %s %@", action_subc, NULL, "", // Deprecated synonym
                "--mul %@", action_mul, NULL, "Multiply two images",
                "--mulc %s %@", action_mulc, NULL, "Multiply the image values by a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--cmul %s %@", action_mulc, NULL, "", // Deprecated synonym
                "--div %@", action_div, NULL, "Divide first image by second image",
                "--divc %s %@", action_divc, NULL, "Divide the image values by a scalar or per-channel constants (e.g.: 0.5 or 1,1.25,0.5)",
                "--mad %@", action_mad, NULL, "Multiply two images, add a third",
                "--invert %@", action_invert, NULL, "Take the color inverse (subtract from 1)",
                "--abs %@", action_abs, NULL, "Take the absolute value of the image pixels",
                "--absdiff %@", action_absdiff, NULL, "Absolute difference between two images",
                "--absdiffc %s %@", action_absdiffc, NULL, "Absolute difference versus a scalar or per-channel constant (e.g.: 0.5 or 1,1.25,0.5)",
                "--powc %s %@", action_powc, NULL, "Raise the image values to a scalar or per-channel power (e.g.: 2.2 or 2.2,2.2,2.2,1.0)",
                "--cpow %s %@", action_powc, NULL, "", // Depcrcated synonym
                "--noise %@", action_noise, NULL, "Add noise to an image (options: type=gaussian:mean=0:stddev=0.1, type=uniform:min=0:max=0.1, type=salt:value=0:portion=0.1, seed=0",
                "--chsum %@", action_chsum, NULL,
                    "Turn into 1-channel image by summing channels (options: weight=r,g,...)",
                "--colormap %s %@", action_colormap, NULL, "Color map based on channel 0 (arg: \"inferno\", \"viridis\", \"magma\", \"plasma\", \"blue-red\", \"spectrum\", \"heat\", or comma-separated list of RGB triples)",
                "--crop %@ %s", action_crop, NULL, "Set pixel data resolution and offset, cropping or padding if necessary (WxH+X+Y or xmin,ymin,xmax,ymax)",
                "--croptofull %@", action_croptofull, NULL, "Crop or pad to make pixel data region match the \"full\" region",
                "--trim %@", action_trim, NULL, "Crop to the minimal ROI containing nonzero pixel values",
                "--cut %@ %s", action_cut, NULL, "Cut out the ROI and reposition to the origin (WxH+X+Y or xmin,ymin,xmax,ymax)",
                "--paste %@ %s", action_paste, NULL, "Paste fg over bg at the given position (e.g., +100+50)",
                "--mosaic %@ %s", action_mosaic, NULL,
                        "Assemble images into a mosaic (arg: WxH; options: pad=0)",
                "--over %@", action_over, NULL, "'Over' composite of two images",
                "--zover %@", action_zover, NULL, "Depth composite two images with Z channels (options: zeroisinf=%d)",
                "--deepmerge %@", action_deepmerge, NULL, "Merge/composite two deep images",
                "--deepholdout %@", action_deepholdout, NULL, "Hold out one deep image by another",
                "--histogram %@ %s %d", action_histogram, NULL, NULL, "Histogram one channel (options: cumulative=0)",
                "--rotate90 %@", action_rotate90, NULL, "Rotate the image 90 degrees clockwise",
                "--rotate180 %@", action_rotate180, NULL, "Rotate the image 180 degrees",
                "--flipflop %@", action_rotate180, NULL, "", // Deprecated synonym for --rotate180
                "--rotate270 %@", action_rotate270, NULL, "Rotate the image 270 degrees clockwise (or 90 degrees CCW)",
                "--flip %@", action_flip, NULL, "Flip the image vertically (top<->bottom)",
                "--flop %@", action_flop, NULL, "Flop the image horizontally (left<->right)",
                "--reorient %@", action_reorient, NULL, "Rotate and/or flop the image to transform the pixels to match the Orientation metadata",
                "--transpose %@", action_transpose, NULL, "Transpose the image",
                "--cshift %@ %s", action_cshift, NULL, "Circular shift the image (e.g.: +20-10)",
                "--resample %@ %s", action_resample, NULL, "Resample (640x480, 50%) (options: interp=0)",
                "--resize %@ %s", action_resize, NULL, "Resize (640x480, 50%) (options: filter=%s)",
                "--fit %@ %s", action_fit, NULL, "Resize to fit within a window size (options: filter=%s, pad=%d, exact=%d)",
                "--pixelaspect %@ %g", action_pixelaspect, NULL, "Scale up the image's width or height to match the given pixel aspect ratio (options: filter=%s)",
                "--rotate %@ %g", action_rotate, NULL, "Rotate pixels (argument is degrees clockwise) around the center of the display window (options: filter=%s, center=%f,%f, recompute_roi=%d",
                "--warp %@ %s", action_warp, NULL, "Warp pixels (argument is a 3x3 matrix, separated by commas) (options: filter=%s, recompute_roi=%d)",
                "--convolve %@", action_convolve, NULL,
                    "Convolve with a kernel",
                "--blur %@ %s", action_blur, NULL,
                    "Blur the image (arg: WxH; options: kernel=name)",
                "--median %@ %s", action_median, NULL,
                    "Median filter the image (arg: WxH)",
                "--dilate %@ %s", action_dilate, NULL,
                    "Dilate (area maximum) the image (arg: WxH)",
                "--erode %@ %s", action_erode, NULL,
                    "Erode (area minimum) the image (arg: WxH)",
                "--unsharp %@", action_unsharp, NULL,
                    "Unsharp mask (options: kernel=gaussian, width=3, contrast=1, threshold=0)",
                "--laplacian %@", action_laplacian, NULL,
                    "Laplacian filter the image",
                "--fft %@", action_fft, NULL,
                    "Take the FFT of the image",
                "--ifft %@", action_ifft, NULL,
                    "Take the inverse FFT of the image",
                "--polar %@", action_polar, NULL,
                    "Convert complex (real,imag) to polar (amplitude,phase)",
                "--unpolar %@", action_unpolar, NULL,
                    "Convert polar (amplitude,phase) to complex (real,imag)",
                "--fixnan %@ %s", action_fixnan, NULL, "Fix NaN/Inf values in the image (options: none, black, box3, error)",
                "--fillholes %@", action_fillholes, NULL,
                    "Fill in holes (where alpha is not 1)",
                "--clamp %@", action_clamp, NULL, "Clamp values (options: min=..., max=..., clampalpha=0)",
                "--rangecompress %@", action_rangecompress, NULL,
                    "Compress the range of pixel values with a log scale (options: luma=0|1)",
                "--rangeexpand %@", action_rangeexpand, NULL,
                    "Un-rangecompress pixel values back to a linear scale (options: luma=0|1)",
                "--line %@ %s", action_line, NULL,
                    "Render a poly-line (args: x1,y1,x2,y2... ; options: color=)",
                "--box %@ %s", action_box, NULL,
                    "Render a box (args: x1,y1,x2,y2 ; options: color=)",
                "--fill %@ %s", action_fill, NULL, "Fill a region (options: color=)",
                "--text %@ %s", action_text, NULL,
                    "Render text into the current image (options: x=, y=, size=, color=)",
                // "--noise_uniform %@", action_noise_uniform, NULL, "Add uniform noise to the image (options: min=, max=)",
                // "--noise_gaussian %@", action_noise_gaussian, NULL, "Add Gaussian noise to the image (options: mean=, stddev=)",
                // "--noise_salt %@", action_noise_saltpepp, NULL, "Add 'salt & pepper' noise to the image (options: min=, max=)",
                "<SEPARATOR>", "Manipulating channels or subimages:",
                "--ch %@ %s", action_channels, NULL,
                    "Select or shuffle channels (e.g., \"R,G,B\", \"B,G,R\", \"2,3,4\")",
                "--chappend %@", action_chappend, NULL,
                    "Append the channels of the last two images",
                "--unmip %@", action_unmip, NULL, "Discard all but the top level of a MIPmap",
                "--selectmip %@ %d", action_selectmip, NULL,
                    "Select just one MIP level (0 = highest res)",
                "--subimage %@ %s", action_select_subimage, NULL, "Select just one subimage (by index or name)",
                "--sisplit %@", action_subimage_split, NULL,
                    "Split the top image's subimges into separate images",
                "--siappend %@", action_subimage_append, NULL,
                    "Append the last two images into one multi-subimage image",
                "--siappendall %@", action_subimage_append_all, NULL,
                    "Append all images on the stack into a single multi-subimage image",
                "--deepen %@", action_deepen, NULL, "Deepen normal 2D image to deep",
                "--flatten %@", action_flatten, NULL, "Flatten deep image to non-deep",
                "<SEPARATOR>", "Image stack manipulation:",
                "--dup %@", action_dup, NULL,
                    "Duplicate the current image (push a copy onto the stack)",
                "--swap %@", action_swap, NULL,
                    "Swap the top two images on the stack.",
                "--pop %@", action_pop, NULL,
                    "Throw away the current image",
                "--label %@ %s", action_label, NULL,
                    "Label the top image",
                "<SEPARATOR>", "Color management:",
                "--colorconfig %@ %s", set_colorconfig, NULL,
                    "Explicitly specify an OCIO configuration file",
                "--iscolorspace %@ %s", set_colorspace, NULL,
                    "Set the assumed color space (without altering pixels)",
                "--tocolorspace %@ %s", action_tocolorspace, NULL,
                    "Convert the current image's pixels to a named color space",
                "--colorconvert %@ %s %s", action_colorconvert, NULL, NULL,
                    "Convert pixels from 'src' to 'dst' color space (options: key=, value=, unpremult=)",
                "--ociolook %@ %s", action_ociolook, NULL,
                    "Apply the named OCIO look (options: from=, to=, inverse=, key=, value=, unpremult=)",
                "--ociodisplay %@ %s %s", action_ociodisplay, NULL, NULL,
                    "Apply the named OCIO display and view (options: from=, looks=, key=, value=, unpremult=)",
                "--ociofiletransform %@ %s", action_ociofiletransform, NULL,
                    "Apply the named OCIO filetransform (options: inverse=, unpremult=)",
                "--unpremult %@", action_unpremult, NULL,
                    "Divide all color channels of the current image by the alpha to \"un-premultiply\"",
                "--premult %@", action_premult, NULL,
                    "Multiply all color channels of the current image by the alpha",
                NULL);

    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        print_help (ap);
        // Repeat the command line, so if oiiotool is being called from a
        // script, it's easy to debug how the command was mangled.
        std::cerr << "\nFull command line was:\n> " << ot.full_command_line << "\n";
        exit (EXIT_FAILURE);
    }
    if (help) {
        print_help (ap);
        exit (EXIT_SUCCESS);
    }
    if (argc <= 1) {
        ap.briefusage ();
        std::cout << "\nFor detailed help: oiiotool --help\n";
        exit (EXIT_SUCCESS);
    }
}



// Check if any of the command line arguments contains numeric ranges or
// wildcards.  If not, just return 'false'.  But if they do, the
// remainder of processing will happen here (and return 'true').
static bool 
handle_sequence (int argc, const char **argv)
{
    // First, scan the original command line arguments for '#', '@', '%0Nd',
    // '%v' or '%V' characters.  Any found indicate that there are numeric
    // range or wildcards to deal with.  Also look for --frames,
    // --framepadding and --views options.
#define ONERANGE_SPEC "[0-9]+(-[0-9]+((x|y)-?[0-9]+)?)?"
#define MANYRANGE_SPEC ONERANGE_SPEC "(," ONERANGE_SPEC ")*"
#define VIEW_SPEC "%[Vv]"
#define SEQUENCE_SPEC "((" MANYRANGE_SPEC ")?" "((#|@)+|(%[0-9]*d)))" "|" "(" VIEW_SPEC ")"
    static regex sequence_re (SEQUENCE_SPEC);
    std::string framespec = "";

    static const char *default_views = "left,right";
    std::vector<string_view> views;
    Strutil::split (default_views, views, ",");

    int framepadding = 0;
    std::vector<int> sequence_args;  // Args with sequence numbers
    std::vector<bool> sequence_is_output;
    bool is_sequence = false;
    bool wildcard_on = true;
    for (int a = 1;  a < argc;  ++a) {
        bool is_output = false;
        bool is_output_all = false;
        if (Strutil::starts_with (argv[a], "-o") && a < argc-1) {
            is_output = true;
            if (Strutil::contains (argv[a], ":all=")) {
                // skip wildcard expansion for -o:all, because the name
                // will be a pattern for expansion of the subimage number.
                is_output_all = true;
            }
            a++;
        }
        std::string strarg (argv[a]);
        match_results<std::string::const_iterator> range_match;
        if (strarg == "--debug" || strarg == "-debug")
            ot.debug = true;
        else if ((strarg == "--frames" || strarg == "-frames") && a < argc-1) {
            framespec = argv[++a];
        }
        else if ((strarg == "--framepadding" || strarg == "-framepadding")
                 && a < argc-1) {
            int f = atoi (argv[++a]);
            if (f >= 1 && f < 10)
                framepadding = f;
        }
        else if ((strarg == "--views" || strarg == "-views") && a < argc-1) {
            Strutil::split (argv[++a], views, ",");
        }
        else if (strarg == "--wildcardoff" || strarg == "-wildcardoff") {
            wildcard_on = false;
        }
        else if (strarg == "--wildcardon" || strarg == "-wildcardon") {
            wildcard_on = true;
        }
        else if (wildcard_on && !is_output_all &&
                 regex_search (strarg, range_match, sequence_re)) {
            is_sequence = true;
            sequence_args.push_back (a);
            sequence_is_output.push_back (is_output);
        }
    }

    // No ranges or wildcards?
    if (! is_sequence)
        return false;

    // For each of the arguments that contains a wildcard, get a normalized
    // pattern in printf style (e.g. "foo.%04d.exr"). Next, either expand the
    // frame pattern to a list of frame numbers and use enumerate_file_sequence
    // to fully elaborate all the filenames in the sequence, or if no frame
    // range was specified, scan the filesystem for matching frames. Output
    // sequences without explicit frame ranges inherit the frame numbers of
    // the first input sequence. It's an error if the sequences are not all
    // of the same length.
    std::vector< std::vector<std::string> > filenames (argc+1);
    std::vector< std::vector<int> > frame_numbers (argc+1);
    std::vector< std::vector<string_view> > frame_views (argc+1);
    std::string normalized_pattern, sequence_framespec;
    size_t nfilenames = 0;
    bool result;
    for (size_t i = 0; i < sequence_args.size(); ++i) {
        int a = sequence_args[i];
        result = Filesystem::parse_pattern (argv[a],
                                            framepadding,
                                            normalized_pattern,
                                            sequence_framespec);
        if (! result) {
            ot.error (Strutil::format("Could not parse pattern: %s",
                                      argv[a]), "");
            return true;
        }

        if (sequence_framespec.empty())
            sequence_framespec = framespec;
        if (! sequence_framespec.empty()) {
            Filesystem::enumerate_sequence (sequence_framespec.c_str(),
                                            frame_numbers[a]);
            Filesystem::enumerate_file_sequence (normalized_pattern,
                                                 frame_numbers[a],
                                                 frame_views[a],
                                                 filenames[a]);
        } else if (sequence_is_output[i]) {
            // use frame numbers from first sequence
            Filesystem::enumerate_file_sequence (normalized_pattern,
                                                 frame_numbers[sequence_args[0]],
                                                 frame_views[sequence_args[0]],
                                                 filenames[a]);
        } else if (! sequence_is_output[i]) {
            result = Filesystem::scan_for_matching_filenames (normalized_pattern,
                                                              views,
                                                              frame_numbers[a],
                                                              frame_views[a],
                                                              filenames[a]);
            if (! result) {
                ot.error (Strutil::format("No filenames found matching pattern: \"%s\" (did you intend to use --wildcardoff?)",
                                          argv[a]), "");
                return true;
            }
        }

        if (i == 0) {
            nfilenames = filenames[a].size();
        } else if (nfilenames != filenames[a].size()) {
            ot.error (Strutil::format("Not all sequence specifications matched: %s (%d frames) vs. %s (%d frames)",
                                      argv[sequence_args[0]], nfilenames, argv[a], filenames[a].size()), "");
            return true;
        }
    }

    // OK, now we just call getargs once for each item in the sequences,
    // substituting the i-th sequence entry for its respective argument
    // every time.
    // Note: nfilenames really means, number of frame number iterations.
    std::vector<const char *> seq_argv (argv, argv+argc+1);
    for (size_t i = 0;  i < nfilenames;  ++i) {
        if (ot.debug)
            std::cout << "SEQUENCE " << i << "\n";
        for (size_t a : sequence_args) {
            seq_argv[a] = filenames[a][i].c_str();
            if (ot.debug)
                std::cout << "  " << argv[a] << " -> " << seq_argv[a] << "\n";
        }

        ot.clear_options (); // Careful to reset all command line options!
        ot.frame_number = frame_numbers[sequence_args[0]][i];
        getargs (argc, (char **)&seq_argv[0]);

        ot.process_pending ();
        if (ot.pending_callback())
            ot.warning (Strutil::format ("pending '%s' command never executed", ot.pending_callback_name()));
        // Clear the stack at the end of each iteration
        ot.curimg.reset ();
        ot.image_stack.clear();

        if (ot.runstats)
            std::cout << "End iteration " << i << ": "
                    << Strutil::timeintervalformat(ot.total_runtime(),2) << "  "
                    << Strutil::memformat(Sysutil::memory_used()) << "\n";
        if (ot.debug)
            std::cout << "\n";
    }

    return true;
}



int
main (int argc, char *argv[])
{
#if OIIO_MSVS_BEFORE_2015
     // When older Visual Studio is used, float values in scientific foramt
     // are printed with three digit exponent. We change this behaviour to
     // fit Linux way.
    _set_output_format (_TWO_DIGIT_EXPONENT);
#endif

    // Globally force classic "C" locale, and turn off all formatting
    // internationalization, for the entire oiiotool application.
    std::locale::global (std::locale::classic());

    ot.imagecache = ImageCache::create (false);
    ASSERT (ot.imagecache);
    ot.imagecache->attribute ("forcefloat", 1);
    ot.imagecache->attribute ("max_memory_MB", float(ot.cachesize));
    ot.imagecache->attribute ("autotile", ot.autotile);
    ot.imagecache->attribute ("autoscanline", int(ot.autotile ? 1 : 0));

    Filesystem::convert_native_arguments (argc, (const char **)argv);
    if (handle_sequence (argc, (const char **)argv)) {
        // Deal with sequence

    } else {
        // Not a sequence
        getargs (argc, argv);
        ot.process_pending ();
        if (ot.pending_callback())
            ot.warning (Strutil::format ("pending '%s' command never executed", ot.pending_callback_name()));
    }

    if (!ot.printinfo && !ot.printstats && !ot.dumpdata && !ot.dryrun && !ot.printed_info) {
        if (ot.curimg && !ot.curimg->was_output() &&
            (ot.curimg->metadata_modified() || ot.curimg->pixels_modified()))
            ot.warning ("modified images without outputting them. Did you forget -o?");
        else if (ot.num_outputs == 0)
            ot.warning ("oiiotool produced no output. Did you forget -o?");
    }

    if (ot.runstats) {
        double total_time = ot.total_runtime();
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
        ot.check_peak_memory ();
        std::cout << "  Peak memory:    " << Strutil::memformat(ot.peak_memory) << "\n";
        std::cout << "  Current memory: " << Strutil::memformat(Sysutil::memory_used()) << "\n";
        std::cout << "\n" << ot.imagecache->getstats(2) << "\n";
    }

    return ot.return_value;
}
