// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

#include "imageio_pvt.h"

using namespace OIIO;
using namespace ImageBufAlgo;


static bool verbose = false;
static bool sum     = false;
static bool help    = false;
static std::vector<std::string> filenames;
static std::string metamatch;
static bool filenameprefix = false;
static std::regex field_re;
static bool subimages     = false;
static bool compute_sha1  = false;
static bool compute_stats = false;

using OIIO::print;



static void
print_sha1(ImageInput* input, int subimage, int miplevel)
{
    std::string err;
    std::string s1 = pvt::compute_sha1(input, subimage, miplevel, err);
    print("    SHA-1: {}\n", err.size() ? err : s1);
}



///////////////////////////////////////////////////////////////////////////////
// Stats

static bool
read_input(const std::string& filename, ImageBuf& img, int subimage = 0,
           int miplevel = 0)
{
    if (img.read(subimage, miplevel, false, TypeDesc::FLOAT))
        return true;

    std::cerr << "iinfo ERROR: Could not read " << filename << ":\n\t"
              << img.geterror() << "\n";
    return false;
}



static void
print_stats(const std::string& filename, const ImageSpec& originalspec,
            int subimage = 0, int miplevel = 0, bool indentmip = false)
{
    const char* indent = indentmip ? "      " : "    ";

    ImageBuf input(filename);
    if (!read_input(filename, input, subimage, miplevel)) {
        // Note: read_input prints an error message if one occurs
        return;
    }

    std::string err;
    if (!pvt::print_stats(std::cout, indent, input, originalspec, ROI(), err)) {
        print("{}Stats: (unable to compute)\n", indent);
        if (err.size())
            std::cerr << "Error: " << err << "\n";
        return;
    }
}



static void
print_metadata(const ImageSpec& spec, const std::string& filename)
{
    bool printed = false;
    if (metamatch.empty() || std::regex_search("channels", field_re)
        || std::regex_search("channel list", field_re)) {
        if (filenameprefix)
            print("{} : ", filename);
        print("    channel list: ");
        for (int i = 0; i < spec.nchannels; ++i) {
            if (i < (int)spec.channelnames.size())
                print("{}", spec.channelnames[i]);
            else
                print("unknown");
            if (i < (int)spec.channelformats.size())
                print(" ({})", spec.channelformats[i]);
            if (i < spec.nchannels - 1)
                print(", ");
        }
        print("\n");
        printed = true;
    }
    if (spec.x || spec.y || spec.z) {
        if (metamatch.empty()
            || std::regex_search("pixel data origin", field_re)) {
            if (filenameprefix)
                print("{} : ", filename);
            print("    pixel data origin: x={}, y={}", spec.x, spec.y);
            if (spec.depth > 1)
                print(", z={}", spec.z);
            print("\n");
            printed = true;
        }
    }
    if (spec.full_x || spec.full_y || spec.full_z
        || (spec.full_width != spec.width && spec.full_width != 0)
        || (spec.full_height != spec.height && spec.full_height != 0)
        || (spec.full_depth != spec.depth && spec.full_depth != 0)) {
        if (metamatch.empty()
            || std::regex_search("full/display size", field_re)) {
            if (filenameprefix)
                print("{} : ", filename);
            print("    full/display size: {} x {}", spec.full_width,
                  spec.full_height);
            if (spec.depth > 1)
                print(" x {}", spec.full_depth);
            print("\n");
            printed = true;
        }
        if (metamatch.empty()
            || std::regex_search("full/display origin", field_re)) {
            if (filenameprefix)
                print("{} : ", filename);
            print("    full/display origin: {}, {}", spec.full_x, spec.full_y);
            if (spec.depth > 1)
                print(", {}", spec.full_z);
            print("\n");
            printed = true;
        }
    }
    if (spec.tile_width) {
        if (metamatch.empty() || std::regex_search("tile", field_re)) {
            if (filenameprefix)
                print("{} : ", filename);
            print("    tile size: {} x {}", spec.tile_width, spec.tile_height);
            if (spec.depth > 1)
                print(" x {}", spec.tile_depth);
            print("\n");
            printed = true;
        }
    }

    // Sort the metadata alphabetically, case-insensitive, but making
    // sure that all non-namespaced attribs appear before namespaced
    // attribs.
    ParamValueList attribs = spec.extra_attribs;
    attribs.sort(false /* sort case-insensitively */);
    for (auto&& p : attribs) {
        if (!metamatch.empty()
            && !std::regex_search(p.name().c_str(), field_re))
            continue;
        std::string s = spec.metadata_val(p, true);
        if (filenameprefix)
            print("{} : ", filename);
        print("    {}: ", p.name());
        if (s == "1.#INF")
            print("inf");
        else
            print("{}", s);
        print("\n");
        printed = true;
    }

    if (!printed && !metamatch.empty()) {
        if (filenameprefix)
            print("{} : ", filename);
        print("    {}: <unknown>\n", metamatch);
    }
}



static const char*
extended_format_name(TypeDesc type, int bits)
{
    if (bits && bits < (int)type.size() * 8) {
        // The "oiio:BitsPerSample" betrays a different bit depth in the
        // file than the data type we are passing.
        if (type == TypeDesc::UINT8 || type == TypeDesc::UINT16
            || type == TypeDesc::UINT32 || type == TypeDesc::UINT64)
            return ustring::fmtformat("uint{}", bits).c_str();
        if (type == TypeDesc::INT8 || type == TypeDesc::INT16
            || type == TypeDesc::INT32 || type == TypeDesc::INT64)
            return ustring::fmtformat("int{}", bits).c_str();
    }
    return type.c_str();  // use the name implied by type
}



static const char*
brief_format_name(TypeDesc type, int bits = 0)
{
    if (!bits)
        bits = (int)type.size() * 8;
    if (type.is_floating_point()) {
        if (type.basetype == TypeDesc::FLOAT)
            return "f";
        if (type.basetype == TypeDesc::HALF)
            return "h";
        return ustring::fmtformat("f{}", bits).c_str();
    } else if (type.is_signed()) {
        return ustring::fmtformat("i{}", bits).c_str();
    } else {
        return ustring::fmtformat("u{}", bits).c_str();
    }
    return type.c_str();  // use the name implied by type
}



// prints basic info (resolution, width, height, depth, channels, data format,
// and format name) about given subimage.
static void
print_info_subimage(int current_subimage, int max_subimages, ImageSpec& spec,
                    ImageInput* input, const std::string& filename)
{
    if (!input->seek_subimage(current_subimage, 0))
        return;
    spec = input->spec(current_subimage);

    if (!metamatch.empty()
        && !std::regex_search(
            "resolution, width, height, depth, channels, sha-1, stats",
            field_re)) {
        // nothing to do here
        return;
    }

    int nmip = 1;

    bool printres
        = verbose
          && (metamatch.empty()
              || std::regex_search("resolution, width, height, depth, channels",
                                   field_re));
    if (printres && max_subimages > 1 && subimages) {
        print(" subimage {:2}: ", current_subimage);
        print("{:4} x {:4}", spec.width, spec.height);
        if (spec.depth > 1)
            print(" x {:4}", spec.depth);
        int bits = spec.get_int_attribute("oiio:BitsPerSample", 0);
        print(", {} channel, {}{}{}", spec.nchannels, spec.deep ? "deep " : "",
              spec.depth > 1 ? "volume " : "",
              extended_format_name(spec.format, bits));
        print(" {}", input->format_name());
        print("\n");
    }
    // Count MIP levels
    while (input->seek_subimage(current_subimage, nmip)) {
        if (printres) {
            ImageSpec mipspec = input->spec_dimensions(current_subimage, nmip);
            if (nmip == 1)
                print("    MIP-map levels: {}x{}", spec.width, spec.height);
            print(" {}x{}", mipspec.width, mipspec.height);
        }
        ++nmip;
    }
    if (printres && nmip > 1)
        print("\n");

    if (compute_sha1
        && (metamatch.empty() || std::regex_search("sha-1", field_re))) {
        if (filenameprefix)
            print("{} : ", filename);
        // Before sha-1, be sure to point back to the highest-res MIP level
        input->seek_subimage(current_subimage, 0);
        print_sha1(input, current_subimage, 0);
    }

    if (verbose)
        print_metadata(spec, filename);

    if (compute_stats
        && (metamatch.empty() || std::regex_search("stats", field_re))) {
        for (int m = 0; m < nmip; ++m) {
            ImageSpec mipspec = input->spec_dimensions(current_subimage, m);
            if (filenameprefix)
                print("{} : ", filename);
            if (nmip > 1 && (subimages || m == 0)) {
                print("    MIP {} of {} ({} x {}):\n", m, nmip, mipspec.width,
                      mipspec.height);
            }
            print_stats(filename, spec, current_subimage, m, nmip > 1);
        }
    }

    if (!input->seek_subimage(current_subimage, 0))
        return;
}



static void
print_info(const std::string& filename, size_t namefieldlength,
           ImageInput* input, ImageSpec& spec, bool verbose, bool sum,
           long long& totalsize)
{
    int padlen = std::max(0, (int)namefieldlength - (int)filename.length());
    std::string padding(padlen, ' ');

    // checking how many subimages and mipmap levels are stored in the file
    int num_of_subimages = 1;
    bool any_mipmapping  = false;
    std::vector<int> num_of_miplevels;
    {
        int nmip = 1;
        while (input->seek_subimage(input->current_subimage(), nmip)) {
            ++nmip;
            any_mipmapping = true;
        }
        num_of_miplevels.push_back(nmip);
    }
    while (input->seek_subimage(num_of_subimages, 0)) {
        // maybe we should do this more gently?
        ++num_of_subimages;
        int nmip = 1;
        while (input->seek_subimage(input->current_subimage(), nmip)) {
            ++nmip;
            any_mipmapping = true;
        }
        num_of_miplevels.push_back(nmip);
    }
    // input->seek_subimage(0, 0);  // re-seek to the first
    spec = input->spec(0, 0);

    if (metamatch.empty()
        || std::regex_search("resolution, width, height, depth, channels",
                             field_re)) {
        print("{}{} : {:4} x {:4}", filename, padding, spec.width, spec.height);
        if (spec.depth > 1)
            print(" x {:4}", spec.depth);
        print(", {} channel, {}{}", spec.nchannels, spec.deep ? "deep " : "",
              spec.depth > 1 ? "volume " : "");
        if (spec.channelformats.size()) {
            for (size_t c = 0; c < spec.channelformats.size(); ++c)
                print("{}{}", c ? "/" : "", spec.channelformat(c));
        } else {
            int bits = spec.get_int_attribute("oiio:BitsPerSample", 0);
            print("{}", extended_format_name(spec.format, bits));
        }
        print(" {}", input->format_name());
        if (sum) {
            imagesize_t imagebytes = spec.image_bytes(true);
            totalsize += imagebytes;
            print(" ({:.2f} MB)", (float)imagebytes / (1024.0 * 1024.0));
        }
        // we print info about how many subimages are stored in file
        // only when we have more then one subimage
        if (!verbose && num_of_subimages != 1)
            print(" ({} subimages{})", num_of_subimages,
                  any_mipmapping ? " +mipmap)" : "");
        if (!verbose && num_of_subimages == 1 && any_mipmapping)
            print(" (+mipmap)");
        print("\n");
    }

    int movie = spec.get_int_attribute("oiio:Movie");
    if (verbose && num_of_subimages != 1) {
        // info about num of subimages and their resolutions
        print("    {} subimages: ", num_of_subimages);
        for (int i = 0; i < num_of_subimages; ++i) {
            spec     = input->spec(i, 0);
            int bits = spec.get_int_attribute("oiio:BitsPerSample",
                                              spec.format.size() * 8);
            if (i)
                print(", ");
            if (spec.depth > 1)
                print("{}x{}x{} ", spec.width, spec.height, spec.depth);
            else
                print("{}x{} ", spec.width, spec.height);
            // print("[");
            for (int c = 0; c < spec.nchannels; ++c)
                print("{:c}{}", c ? ',' : '[',
                      brief_format_name(spec.channelformat(c), bits));
            print("]");
            if (movie)
                break;
        }
        print("\n");
    }

    // if the '-a' flag is not set we print info
    // about first subimage only
    if (!subimages)
        num_of_subimages = 1;
    for (int i = 0; i < num_of_subimages; ++i) {
        print_info_subimage(i, num_of_subimages, spec, input, filename);
    }
}



int
main(int argc, const char* argv[])
{
    // Helpful for debugging to make sure that any crashes dump a stack
    // trace.
    Sysutil::setup_crash_stacktrace("stdout");

    Filesystem::convert_native_arguments(argc, (const char**)argv);
    ArgParse ap;
    // clang-format off
    ap.intro("iinfo -- print information about images\n" OIIO_INTRO_STRING)
      .usage("iinfo [options] filename...")
      .add_version(OIIO_VERSION_STRING);
    ap.arg("filename")
      .hidden()
      .action([&](cspan<const char*> argv){ filenames.emplace_back(argv[0]); });
    ap.arg("-v", &verbose)
      .help("Verbose output");
    ap.arg("-m %s:NAMES", &metamatch)
      .help("Metadata names to print (default: all)");
    ap.arg("-f", &filenameprefix)
      .help("Prefix each line with the filename");
    ap.arg("-s", &sum)
      .help("Sum the image sizes");
    ap.arg("-a", &subimages)
      .help("Print info about all subimages")
      .action(ArgParse::store_true());
    ap.arg("--hash", &compute_sha1)
      .help("Print SHA-1 hash of pixel values")
      .action(ArgParse::store_true());
    ap.arg("--stats", &compute_stats)
      .help("Print image pixel statistics (data window)");
    // clang-format on
    if (ap.parse(argc, argv) < 0 || filenames.empty()) {
        std::cerr << ap.geterror() << std::endl;
        ap.print_help();
        return help ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (!metamatch.empty()) {
        field_re.assign(metamatch, std::regex_constants::extended
                                       | std::regex_constants::icase);
    }

    // Find the longest filename
    size_t longestname = 0;
    for (auto&& s : filenames)
        longestname = std::max(longestname, s.length());
    longestname = std::min(longestname, (size_t)40);

    int returncode      = EXIT_SUCCESS;
    long long totalsize = 0;
    for (auto&& s : filenames) {
        auto in = ImageInput::open(s);
        if (!in) {
            std::string err = geterror();
            print(std::cerr, "iinfo ERROR: \"{}\" : {}\n", s,
                  err.size() ? err : std::string("Could not open file."));
            returncode = EXIT_FAILURE;
            continue;
        }
        ImageSpec spec = in->spec();
        print_info(s, longestname, in.get(), spec, verbose, sum, totalsize);
    }

    if (sum)
        print("Total size: {}\n", Strutil::memformat(totalsize));

    shutdown();
    return returncode;
}
