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
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagecache.h>


using namespace OIIO;


static std::string uninitialized = "uninitialized \001 HHRU dfvAS: efjl";
static std::string dataformatname = "";
static float gammaval = 1.0f;
//static bool depth = false;
static bool verbose = false;
static int nthreads = 0;    // default: use #cores threads if available
static std::vector<std::string> filenames;
static int tile[3] = { 0, 0, 1 };
static bool scanline = false;
//static bool zfile = false;
//static std::string channellist;
static std::string compression;
static bool no_copy_image = false;
static int quality = -1;
static bool adjust_time = false;
static std::string caption = uninitialized;
static std::vector<std::string> keywords;
static bool clear_keywords = false;
static std::vector<std::string> attribnames, attribvals;
static bool inplace = false;
static int orientation = 0;
static bool rotcw = false, rotccw = false, rot180 = false;
static bool sRGB = false;
static bool separate = false, contig = false;
static bool noclobber = false;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("iconvert -- copy images with format conversions and other alterations\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  iconvert [options] inputfile outputfile\n"
                "   or:  iconvert --inplace [options] file...\n",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose status messages",
                "--threads %d", &nthreads, "Number of threads (default 0 = #cores)",
                "-d %s", &dataformatname, "Set the output data format to one of:"
                        "uint8, sint8, uint10, uint12, uint16, sint16, half, float, double",
                "-g %f", &gammaval, "Set gamma correction (default = 1)",
                "--tile %d %d", &tile[0], &tile[1], "Output as a tiled image",
                "--scanline", &scanline, "Output as a scanline image",
                "--compression %s", &compression, "Set the compression method (default = same as input)",
                "--quality %d", &quality, "Set the compression quality, 1-100",
                "--no-copy-image", &no_copy_image, "Do not use ImageOutput copy_image functionality (dbg)",
                "--adjust-time", &adjust_time, "Adjust file times to match DateTime metadata",
                "--caption %s", &caption, "Set caption (ImageDescription)",
                "--keyword %L", &keywords, "Add a keyword",
                "--clear-keywords", &clear_keywords, "Clear keywords",
                "--attrib %L %L", &attribnames, &attribvals, "Set a string attribute (name, value)",
                "--orientation %d", &orientation, "Set the orientation",
                "--rotcw", &rotcw, "Rotate 90 deg clockwise",
                "--rotccw", &rotccw, "Rotate 90 deg counter-clockwise",
                "--rot180", &rot180, "Rotate 180 deg",
                "--inplace", &inplace, "Do operations in place on images",
                "--sRGB", &sRGB, "This file is in sRGB color space",
                "--separate", &separate, "Force planarconfig separate",
                "--contig", &contig, "Force planarconfig contig",
                "--no-clobber", &noclobber, "Do no overwrite existing files",
//FIXME         "-z", &zfile, "Treat input as a depth file",
//FIXME         "-c %s", &channellist, "Restrict/shuffle channels",
                NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
	std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() != 2 && ! inplace) {
        std::cerr << "iconvert: Must have both an input and output filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
    if (filenames.size() == 0 && inplace) {
        std::cerr << "iconvert: Must have at least one filename\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
    if (((int)rotcw + (int)rotccw + (int)rot180 + (orientation>0)) > 1) {
        std::cerr << "iconvert: more than one of --rotcw, --rotccw, --rot180, --orientation\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
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



// Adjust the output spec based on the command-line arguments.
// Return whether the specifics preclude using copy_image.
static bool
adjust_spec (ImageInput *in, ImageOutput *out,
             const ImageSpec &inspec, ImageSpec &outspec)
{
    bool nocopy = no_copy_image;

    // Copy the spec, with possible change in format
    outspec.format = inspec.format;
    if (inspec.channelformats.size()) {
        // Input file has mixed channels
        if (out->supports("channelformats")) {
            // Output supports mixed formats -- so request it
            outspec.format = TypeDesc::UNKNOWN;
        } else {
            // Input had mixed formats, output did not, so just use a fixed
            // format and forget the per-channel formats for output.
            outspec.channelformats.clear ();
        }
    }
    if (! dataformatname.empty()) {
        // make sure there isn't a stray BPS that will screw us up
        outspec.erase_attribute ("oiio:BitsPerSample");
        if (dataformatname == "uint8")
            outspec.set_format (TypeDesc::UINT8);
        else if (dataformatname == "int8")
            outspec.set_format (TypeDesc::INT8);
        else if (dataformatname == "uint10") {
            outspec.attribute ("oiio:BitsPerSample", 10);
            outspec.set_format (TypeDesc::UINT16);
        }
        else if (dataformatname == "uint12") {
            outspec.attribute ("oiio:BitsPerSample", 12);
            outspec.set_format (TypeDesc::UINT16);
        }
        else if (dataformatname == "uint16")
            outspec.set_format (TypeDesc::UINT16);
        else if (dataformatname == "int16")
            outspec.set_format (TypeDesc::INT16);
        else if (dataformatname == "uint32" || dataformatname == "uint")
            outspec.set_format (TypeDesc::UINT32);
        else if (dataformatname == "int32" || dataformatname == "int")
            outspec.set_format (TypeDesc::INT32);
        else if (dataformatname == "half")
            outspec.set_format (TypeDesc::HALF);
        else if (dataformatname == "float")
            outspec.set_format (TypeDesc::FLOAT);
        else if (dataformatname == "double")
            outspec.set_format (TypeDesc::DOUBLE);
        outspec.channelformats.clear ();
    }
    if (outspec.format != inspec.format || inspec.channelformats.size())
        nocopy = true;
    
    outspec.attribute ("oiio:Gamma", gammaval);
    if (sRGB) {
        outspec.attribute ("oiio:ColorSpace", "sRGB");
        if (!strcmp (in->format_name(), "jpeg") ||
                outspec.find_attribute ("Exif:ColorSpace"))
            outspec.attribute ("Exif:ColorSpace", 1);
    }

    if (tile[0]) {
        outspec.tile_width = tile[0];
        outspec.tile_height = tile[1];
        outspec.tile_depth = tile[2];
    }
    if (scanline) {
        outspec.tile_width = 0;
        outspec.tile_height = 0;
        outspec.tile_depth = 0;
    }
    if (outspec.tile_width != inspec.tile_width ||
            outspec.tile_height != inspec.tile_height ||
            outspec.tile_depth != inspec.tile_depth)
        nocopy = true;

    if (! compression.empty()) {
        outspec.attribute ("compression", compression);
        if (compression != inspec.get_string_attribute ("compression"))
            nocopy = true;
    }

    if (quality > 0) {
        outspec.attribute ("CompressionQuality", quality);
        if (quality != inspec.get_int_attribute ("CompressionQuality"))
            nocopy = true;
    }

    if (contig)
        outspec.attribute ("planarconfig", "contig");
    if (separate)
        outspec.attribute ("planarconfig", "separate");

    if (orientation >= 1)
        outspec.attribute ("Orientation", orientation);
    else {
        orientation = outspec.get_int_attribute ("Orientation", 1);
        if (orientation >= 1 && orientation <= 8) {
            static int cw[] = { 0, 6, 7, 8, 5, 2, 3, 4, 1 };
            if (rotcw || rotccw || rot180)
                orientation = cw[orientation];
            if (rotccw || rot180)
                orientation = cw[orientation];
            if (rotccw)
                orientation = cw[orientation];
            outspec.attribute ("Orientation", orientation);
        }
    }

    if (caption != uninitialized)
        outspec.attribute ("ImageDescription", caption);

    if (clear_keywords)
        outspec.attribute ("Keywords", "");
    if (keywords.size()) {
        std::string oldkw = outspec.get_string_attribute ("Keywords");
        std::vector<std::string> oldkwlist;
        if (! oldkw.empty()) {
            Strutil::split (oldkw, oldkwlist, ";");
            for (size_t i = 0; i < oldkwlist.size(); ++i)
                oldkwlist[i] = Strutil::strip (oldkwlist[i]);
        }
        for (auto&& nk : keywords) {
            bool dup = false;
            for (auto&& ok : oldkwlist)
                dup |= (ok == nk);
            if (! dup)
                oldkwlist.push_back (nk);
        }
        outspec.attribute ("Keywords", Strutil::join (oldkwlist, "; "));
    }

    for (size_t i = 0;  i < attribnames.size();  ++i) {
        outspec.attribute (attribnames[i].c_str(), attribvals[i].c_str());
    }

    return nocopy;
}



static bool
convert_file (const std::string &in_filename, const std::string &out_filename)
{
    if (noclobber && Filesystem::exists(out_filename)) {
        std::cerr << "iconvert ERROR: Output file already exists \""
                  << out_filename << "\"\n";
        return false;
    }

    if (verbose)
        std::cout << "Converting " << in_filename << " to " << out_filename << "\n";

    std::string tempname = out_filename;
    if (tempname == in_filename) {
        tempname = out_filename + ".tmp"
                    + Filesystem::extension (out_filename);
    }

    // Find an ImageIO plugin that can open the input file, and open it.
    ImageInput *in = ImageInput::open (in_filename.c_str());
    if (! in) {
        std::string err = geterror();
        std::cerr << "iconvert ERROR: " 
                  << (err.length() ? err : Strutil::format("Could not open \"%s\"", in_filename))
                  << "\n";
        delete in;
        return false;
    }
    ImageSpec inspec = in->spec();
    std::string metadatatime = inspec.get_string_attribute ("DateTime");

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (tempname.c_str());
    if (! out) {
        std::cerr 
            << "iconvert ERROR: Could not find an ImageIO plugin to write \"" 
            << out_filename << "\" :" << geterror() << "\n";
        delete in;
        return false;
    }

    // In order to deal with formats that support subimages, but not
    // subimage appending, we gather them all first.
    std::vector<ImageSpec> subimagespecs;
    if (out->supports("multiimage") && !out->supports("appendsubimage")) {
        ImageCache *imagecache = ImageCache::create ();
        int nsubimages = 0;
        ustring ufilename (in_filename);
        imagecache->get_image_info (ufilename, 0, 0, ustring("subimages"),
                                    TypeDesc::TypeInt, &nsubimages);
        if (nsubimages > 1) {
            subimagespecs.resize (nsubimages);
            for (int i = 0;  i < nsubimages;  ++i) {
                ImageSpec inspec = *imagecache->imagespec (ufilename, i, 0,
                                                           true /*native*/);
                subimagespecs[i] = inspec;
                adjust_spec (in, out, inspec, subimagespecs[i]);
            }
        }
        ImageCache::destroy (imagecache);
    }

    bool ok = true;
    bool mip_to_subimage_warning = false;
    for (int subimage = 0;
           ok && in->seek_subimage(subimage,0,inspec);
           ++subimage) {

        if (subimage > 0 &&  !out->supports ("multiimage")) {
            std::cerr << "iconvert WARNING: " << out->format_name()
                      << " does not support multiple subimages.\n";
            std::cerr << "\tOnly the first subimage has been copied.\n";
            break;  // we're done
        }

        int miplevel = 0;
        do {
            // Copy the spec, with possible change in format
            ImageSpec outspec = inspec;
            bool nocopy = adjust_spec (in, out, inspec, outspec);
        
            if (miplevel > 0) {
                // Moving to next MIP level
                ImageOutput::OpenMode mode;
                if (out->supports ("mipmap"))
                    mode = ImageOutput::AppendMIPLevel;
                else if (out->supports ("multiimage") &&
                         out->supports ("appendsubimage")) {
                    mode = ImageOutput::AppendSubimage; // use if we must
                    if (! mip_to_subimage_warning
                        && strcmp(out->format_name(),"tiff")) {
                        std::cerr << "iconvert WARNING: " << out->format_name()
                                  << " does not support MIPmaps.\n";
                        std::cerr << "\tStoring the MIPmap levels in subimages.\n";
                    }
                    mip_to_subimage_warning = true;
                } else {
                    std::cerr << "iconvert WARNING: " << out->format_name()
                              << " does not support MIPmaps.\n";
                    std::cerr << "\tOnly the first level has been copied.\n";
                    break;  // on to the next subimage
                }
                ok = out->open (tempname.c_str(), outspec, mode);
            } else if (subimage > 0) {
                // Moving to next subimage
                ok = out->open (tempname.c_str(), outspec,
                                ImageOutput::AppendSubimage);
            } else {
                // First time opening
                if (subimagespecs.size())
                    ok = out->open (tempname.c_str(), int(subimagespecs.size()),
                                    &subimagespecs[0]);
                else
                    ok = out->open (tempname.c_str(), outspec, ImageOutput::Create);
            }
            if (! ok) {
                std::string err = out->geterror();
                std::cerr << "iconvert ERROR: " 
                          << (err.length() ? err : Strutil::format("Could not open \"%s\"", out_filename))
                          << "\n";
                ok = false;
                break;
            }

            if (! nocopy) {
                ok = out->copy_image (in);
                if (! ok)
                    std::cerr << "iconvert ERROR copying \"" << in_filename 
                              << "\" to \"" << out_filename << "\" :\n\t" 
                              << out->geterror() << "\n";
            } else {
                // Need to do it by hand for some reason.  Future expansion in which
                // only a subset of channels are copied, or some such.
                std::vector<char> pixels ((size_t)outspec.image_bytes(true));
                ok = in->read_image (outspec.format, &pixels[0]);
                if (! ok) {
                    std::cerr << "iconvert ERROR reading \"" << in_filename 
                              << "\" : " << in->geterror() << "\n";
                } else {
                    ok = out->write_image (outspec.format, &pixels[0]);
                    if (! ok)
                        std::cerr << "iconvert ERROR writing \"" << out_filename 
                                  << "\" : " << out->geterror() << "\n";
                }
            }
        
            ++miplevel;
        } while (ok && in->seek_subimage(subimage,miplevel,inspec));
    }

    out->close ();
    delete out;
    in->close ();
    delete in;

    // Figure out a time for the input file -- either one supplied by
    // the metadata, or the actual time stamp of the input file.
    std::time_t in_time;
    if (metadatatime.empty() ||
           ! DateTime_to_time_t (metadatatime.c_str(), in_time))
        in_time = Filesystem::last_write_time (in_filename);

    if (out_filename != tempname) {
        if (ok) {
            Filesystem::remove (out_filename);
            Filesystem::rename (tempname, out_filename);
        }
        else
            Filesystem::remove (tempname);
    }

    // If user requested, try to adjust the file's modification time to
    // the creation time indicated by the file's DateTime metadata.
    if (ok && adjust_time)
        Filesystem::last_write_time (out_filename, in_time);

    return ok;
}



int
main (int argc, char *argv[])
{
    Filesystem::convert_native_arguments (argc, (const char **)argv);
    getargs (argc, argv);

    OIIO::attribute ("threads", nthreads);

    bool ok = true;

    if (inplace) {
        for (auto&& s : filenames)
            ok &= convert_file (s, s);
    } else {
        ok = convert_file (filenames[0], filenames[1]);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
