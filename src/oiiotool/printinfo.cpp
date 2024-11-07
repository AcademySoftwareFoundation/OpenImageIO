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
#if defined(_MSC_VER)
#    include <io.h>
#endif

#include <OpenImageIO/detail/fmt.h>
#include <OpenImageIO/half.h>

#include "oiiotool.h"

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;
using namespace OIIO::pvt;
using namespace OiioTool;
using namespace ImageBufAlgo;


// clang-format off
#if defined(FMT_VERSION) && !defined(OIIO_HALF_FORMATTER)
#if FMT_VERSION >= 100000
#define OIIO_HALF_FORMATTER
FMT_BEGIN_NAMESPACE
template<> struct formatter<half> : ostream_formatter { };
FMT_END_NAMESPACE
#endif
#endif
// clang-format on


template<typename T>
static void
print_nums(std::ostream& out, int n, const T* val, string_view sep = " ",
           bool C_formatting = false)
{
    if (std::is_floating_point<T>::value || std::is_same<T, half>::value) {
        // Ensure uniform printing of NaN and Inf on all platforms
        for (int i = 0; i < n; ++i) {
            if (i)
                Strutil::print(out, "{}", sep);
            float v = float(val[i]);
            if (isnan(v))
                Strutil::print(out, "nan");
            else if (isinf(v))
                Strutil::print(out, "inf");
            else
                Strutil::print(out, "{:.9f}", v);
        }
    } else {
        // not floating point -- print the int values, then float equivalents
        for (int i = 0; i < n; ++i)
            Strutil::print(out, "{}{}", i ? sep : "", val[i]);
        Strutil::print(out, " {}(", C_formatting ? "/* " : "");
        for (int i = 0; i < n; ++i)
            Strutil::print(out, "{}{}", i ? sep : "",
                           convert_type<T, float>(val[i]));
        Strutil::print(out, "){}", C_formatting ? " */" : "");
    }
}



template<typename T>
static bool
dump_flat_data(std::ostream& out, ImageInput* input,
               const pvt::print_info_options& opt, int subimage)
{
    ImageSpec spec = input->spec_dimensions(subimage);
    std::vector<T> buf(spec.image_pixels() * spec.nchannels);
    if (!input->read_image(subimage, 0, 0, spec.nchannels,
                           BaseTypeFromC<T>::value, &buf[0])) {
        Strutil::print(out, "    dump data Error: could not read image: {}\n",
                       input->geterror());
        return false;
    }
    if (opt.dumpdata_C) {
        if (spec.depth == 1 && spec.z == 0)
            Strutil::print(out, "{}{} {}[{}][{}][{}] =\n{{\n", spec.format,
                           spec.format.is_floating_point() ? "" : "_t",
                           opt.dumpdata_C_name, spec.height, spec.width,
                           spec.nchannels);
        else
            Strutil::print(out, "{}{} {}[{}][{}][{}][{}] =\n{{\n", spec.format,
                           spec.format.is_floating_point() ? "" : "_t",
                           opt.dumpdata_C_name, spec.depth, spec.height,
                           spec.width, spec.nchannels);
    }
    const T* ptr = &buf[0];
    for (int z = 0; z < spec.depth; ++z) {
        if (opt.dumpdata_C && (spec.depth > 1 || spec.z != 0) && z == 0)
            Strutil::print(out, " }} /* slice {} */\n", z);
        for (int y = 0; y < spec.height; ++y) {
            for (int x = 0; x < spec.width; ++x, ptr += spec.nchannels) {
                if (!opt.dumpdata_showempty) {
                    bool allzero = true;
                    for (int c = 0; c < spec.nchannels && allzero; ++c)
                        allzero &= (ptr[c] == T(0));
                    if (allzero)
                        continue;
                }
                if (spec.depth > 1 || spec.z != 0)
                    Strutil::print(out, "  {}{} ({}, {}, {}): {}",
                                   opt.dumpdata_C && x == 0 ? "{ " : "  ",
                                   opt.dumpdata_C ? "/*" : "Pixel", x + spec.x,
                                   y + spec.y, z + spec.z,
                                   opt.dumpdata_C ? "*/ " : "");
                else
                    Strutil::print(out, "  {}{} ({}, {}): {}",
                                   opt.dumpdata_C && x == 0 ? "{ " : "  ",
                                   opt.dumpdata_C ? "/*" : "Pixel", x + spec.x,
                                   y + spec.y, opt.dumpdata_C ? "*/ { " : "");
                print_nums(out, spec.nchannels, ptr,
                           opt.dumpdata_C ? ", " : " ", opt.dumpdata_C);
                Strutil::print(out, "{}{}\n",
                               opt.dumpdata_C && x == (spec.width - 1) ? " }"
                                                                       : "",
                               opt.dumpdata_C ? " }," : "");
            }
        }
        if (opt.dumpdata_C && (spec.depth > 1 || spec.z != 0)
            && z == spec.depth - 1)
            Strutil::print(out, " }}{}\n", z < spec.depth - 1 ? "," : "");
    }
    if (opt.dumpdata_C) {
        Strutil::print(out, "}};\n");
    }
    return true;
}



// Macro to call a type-specialzed version func<type>(R,...)
#define PRINTINFO_DISPATCH_TYPES(ret, name, func, type, R, ...)               \
    switch (type.basetype) {                                                  \
    case TypeDesc::FLOAT: ret = func<float>(R, __VA_ARGS__); break;           \
    case TypeDesc::UINT8: ret = func<unsigned char>(R, __VA_ARGS__); break;   \
    case TypeDesc::HALF: ret = func<half>(R, __VA_ARGS__); break;             \
    case TypeDesc::UINT16: ret = func<unsigned short>(R, __VA_ARGS__); break; \
    case TypeDesc::INT8: ret = func<char>(R, __VA_ARGS__); break;             \
    case TypeDesc::INT16: ret = func<short>(R, __VA_ARGS__); break;           \
    case TypeDesc::UINT: ret = func<unsigned int>(R, __VA_ARGS__); break;     \
    case TypeDesc::INT: ret = func<int>(R, __VA_ARGS__); break;               \
    case TypeDesc::DOUBLE: ret = func<double>(R, __VA_ARGS__); break;         \
    default: ret = false;                                                     \
    }



static void
dump_data(std::ostream& out, ImageInput* input,
          const pvt::print_info_options& opt, int subimage)
{
    ImageSpec spec = input->spec(subimage);
    if (spec.deep) {
        // Special handling of deep data
        DeepData dd;
        if (!input->read_native_deep_image(subimage, 0, dd)) {
            print(out, "    dump data: could not read image\n");
            return;
        }
        int nc = spec.nchannels;
        for (int z = 0, pixel = 0; z < spec.depth; ++z) {
            for (int y = 0; y < spec.height; ++y) {
                for (int x = 0; x < spec.width; ++x, ++pixel) {
                    int nsamples = dd.samples(pixel);
                    if (nsamples == 0 && !opt.dumpdata_showempty)
                        continue;
                    print(out, "    Pixel (");
                    if (spec.depth > 1 || spec.z != 0)
                        print(out, "{}, {}, {}", x + spec.x, y + spec.y,
                              z + spec.z);
                    else
                        print(out, "{}, {}", x + spec.x, y + spec.y);
                    print(out, "): {} samples {}", nsamples,
                          nsamples ? ":" : "");
                    for (int s = 0; s < nsamples; ++s) {
                        if (s)
                            print(out, " / ");
                        for (int c = 0; c < nc; ++c) {
                            print(out, " {}=", spec.channel_name(c));
                            if (dd.channeltype(c) == TypeDesc::UINT)
                                print(out, "{}",
                                      dd.deep_value_uint(pixel, c, s));
                            else
                                print(out, "{}", dd.deep_value(pixel, c, s));
                        }
                    }
                    print(out, "\n");
                }
            }
        }

    } else {
        OIIO_MAYBE_UNUSED bool ok = true;
        PRINTINFO_DISPATCH_TYPES(ok, "dump_flat_data", dump_flat_data,
                                 spec.format, out, input, opt, subimage);
    }
}



///////////////////////////////////////////////////////////////////////////////
// Stats

static bool
read_input(Oiiotool& ot, const std::string& filename, ImageBuf& img,
           int subimage = 0, int miplevel = 0)
{
    img.reset(filename, subimage, miplevel, nullptr, &ot.input_config);
    if (img.init_spec(filename, subimage, miplevel)) {
        // Force a read now for reasonable-sized first images in the
        // file. This can greatly speed up the multithread case for
        // tiled images by not having multiple threads working on the
        // same image lock against each other on the file handle.
        // We guess that "reasonable size" is 200 MB, that's enough to
        // hold a 4k RGBA float image.  Larger things will
        // simply fall back on ImageCache.
        bool forceread = (img.spec().image_bytes() < 200 * 1024 * 1024);
        return img.read(subimage, miplevel, forceread);
    }

    return false;
}



void
OiioTool::print_stats(std::ostream& out, Oiiotool& ot,
                      const std::string& filename, int subimage, int miplevel,
                      string_view indent, ROI roi)
{
    ImageBuf input;
    if (!read_input(ot, filename, input, subimage, miplevel)) {
        ot.error("stats", input.geterror());
        return;
    }
    std::string err;
    if (!pvt::print_stats(out, indent, input, input.nativespec(), roi, err))
        ot.errorfmt("stats", "unable to compute: {}", err);
}



static std::string
brief_format_name(TypeDesc type, int bits = 0)
{
    if (!bits)
        bits = (int)type.size() * 8;
    if (type.is_floating_point()) {
        if (type.basetype == TypeDesc::FLOAT)
            return "f";
        if (type.basetype == TypeDesc::HALF)
            return "h";
        return Strutil::fmt::format("f{}", bits);
    } else if (type.is_signed()) {
        return Strutil::fmt::format("i{}", bits);
    } else {
        return Strutil::fmt::format("u{}", bits);
    }
    return type.c_str();  // use the name implied by type
}



static void
print_info_subimage(std::ostream& out, Oiiotool& ot, int current_subimage,
                    int num_of_subimages, int nmip, const ImageSpec& spec,
                    ImageRec* img, ImageInput* input,
                    const std::string& filename,
                    const pvt::print_info_options& opt, std::regex& field_re,
                    std::regex& field_exclude_re,
                    ImageSpec::SerialFormat serformat,
                    ImageSpec::SerialVerbose verbose)
{
    using Strutil::fmt::format;

    int padlen = std::max(0, (int)opt.namefieldlength - (int)filename.length());
    std::string padding(padlen, ' ');
    bool printres
        = opt.verbose
          && (opt.metamatch.empty()
              || std::regex_search("resolution, width, height, depth, channels",
                                   field_re));

    std::vector<std::string> lines;
    Strutil::split(spec.serialize(serformat, verbose), lines, "\n");

    if (input && opt.compute_sha1
        && (opt.metamatch.empty() || std::regex_search("sha-1", field_re))) {
        // Before sha-1, be sure to point back to the highest-res MIP level
        ImageSpec tmpspec;
        std::string err;
        std::string sha = compute_sha1(input, current_subimage, 0, err);
        if (!err.empty())
            ot.errorfmt("-info", "SHA-1: {}", err);
        else if (serformat == ImageSpec::SerialText)
            lines.insert(lines.begin() + 1, format("    SHA-1: {}", sha));
        else if (serformat == ImageSpec::SerialXML)
            lines.insert(lines.begin() + 1, format("<SHA1>{}</SHA1>", sha));
    }

    // Count MIP levels
    if (printres && nmip > 1) {
        std::string mipdesc = format("    MIP-map levels: {}x{}", spec.width,
                                     spec.height);
        if (img) {
            for (int m = 1; m < nmip; ++m) {
                ImageSpec* mipspec = img->spec(current_subimage, m);
                mipdesc += format(" {}x{}", mipspec->width, mipspec->height);
            }
        } else if (input) {
            for (int m = 1; input->seek_subimage(current_subimage, m); ++m) {
                ImageSpec mipspec = input->spec_dimensions(current_subimage, m);
                mipdesc += format(" {}x{}", mipspec.width, mipspec.height);
            }
        }
        lines.insert(lines.begin() + 1, mipdesc);
    }

    if (serformat == ImageSpec::SerialText) {
        // Requested a subset of metadata but not res, etc.? Kill first line.
        if (opt.metamatch.empty()
            || std::regex_search("resolution, width, height, depth, channels",
                                 field_re)) {
            std::string orig_line0 = lines[0];
            if (current_subimage == 0) {
                if (filename.size())
                    lines[0] = format("{}{}{} : {}",
                                      opt.dumpdata_C ? "// " : "", filename,
                                      padding, lines[0]);
            } else
                lines[0] = format(" subimage {:2}: {}", current_subimage,
                                  lines[0]);
            if (opt.sum) {
                imagesize_t imagebytes = spec.image_bytes(true);
                // totalsize += imagebytes;
                lines[0] += format(" ({:.2f} MB)",
                                   (float)imagebytes / (1024.0 * 1024.0));
            }
            std::string file_format_name;
            if (img)
                file_format_name = (*img)(current_subimage).file_format_name();
            else if (input)
                file_format_name = input->format_name();
            lines[0] += format(" {}", file_format_name);
            // we print info about how many subimages are stored in file
            // only when we have more then one subimage
            if (!opt.verbose && num_of_subimages != 1)
                lines[0] += format(" ({} subimages{})", num_of_subimages,
                                   (nmip > 1) ? " +mipmap)" : "");
            if (!opt.verbose && num_of_subimages == 1 && (nmip > 1))
                lines[0] += " (+mipmap)";
            if (num_of_subimages > 1 && current_subimage == 0 && opt.subimages)
                lines.insert(lines.begin() + 1,
                             format(" subimage  0: {} {}", orig_line0,
                                    file_format_name));
        } else {
            lines.erase(lines.begin());
        }
    } else if (serformat == ImageSpec::SerialXML) {
        if (nmip > 1)
            lines.insert(lines.begin() + 1,
                         format("<miplevels>{}</miplevels>", nmip));
        if (num_of_subimages > 1)
            lines.insert(lines.begin() + 1,
                         format("<subimages>{}</subimages>", num_of_subimages));
    }

    if (current_subimage == 0 && opt.verbose && num_of_subimages != 1
        && serformat == ImageSpec::SerialText) {
        // info about num of subimages and their resolutions
        int movie     = spec.get_int_attribute("oiio:Movie");
        std::string s = format("    {} subimages: ", num_of_subimages);
        for (int i = 0; i < num_of_subimages; ++i) {
            ImageSpec spec;
            if (img)
                spec = *img->nativespec(i);
            if (input)
                spec = input->spec(i, 0);
            int bits = spec.get_int_attribute("oiio:BitsPerSample",
                                              spec.format.size() * 8);
            if (i)
                s += ", ";
            if (spec.depth > 1)
                s += format("{}x{}x{} ", spec.width, spec.height, spec.depth);
            else
                s += format("{}x{} ", spec.width, spec.height);
            for (int c = 0; c < spec.nchannels; ++c)
                s += format("{}{}", c ? ',' : '[',
                            brief_format_name(spec.channelformat(c), bits));
            s += "]";
            if (movie)
                break;
        }
        lines.insert(lines.begin() + 1, s);
    }

    if (!opt.metamatch.empty() || !opt.nometamatch.empty()) {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i == 0 && serformat == ImageSpec::SerialText && printres) {
                // Special case for first line in serialized text case.
                continue;
            }
            std::string s = lines[i].substr(0, lines[i].find(": "));
            if ((!opt.nometamatch.empty()
                 && std::regex_search(s, field_exclude_re))
                || (!opt.metamatch.empty() && !std::regex_search(s, field_re))) {
                lines.erase(lines.begin() + i);
                --i;
            }
        }
    }

    // Unescape the strings if we're printing for human consumption, except for
    // the first line which corresponds to the filename and on windows might
    // contain backslashes as path separators.
    for (size_t i = 1; i < lines.size(); ++i) {
        lines[i] = Strutil::unescape_chars(lines[i]);
    }

    std::string ser = Strutil::join(lines, "\n");
    if (ser[ser.size() - 1] != '\n')
        ser += '\n';
    out << ser;

    if (input && opt.dumpdata) {
        dump_data(out, input, opt, current_subimage);
    }

    if (opt.compute_stats
        && (opt.metamatch.empty() || std::regex_search("stats", field_re))) {
        for (int m = 0; m < nmip; ++m) {
            ImageSpec mipspec;
            if (input)
                mipspec = input->spec(current_subimage, m);
            else if (img)
                mipspec = *img->spec(current_subimage, m);
            if (opt.filenameprefix)
                Strutil::print(out, "{}{} : ", opt.dumpdata_C ? "// " : "",
                               filename);
            if (nmip > 1 && opt.subimages) {
                Strutil::print(out, "{}    MIP {} of {} ({} x {}):\n",
                               opt.dumpdata_C ? "// " : "", m, nmip,
                               mipspec.width, mipspec.height);
            }
            if (input)
                print_stats(out, ot, filename, /*spec,*/ current_subimage, m,
                            nmip > 1 ? "      " : "    ", opt.roi);
            else if (img) {
                std::string err;
                if (!pvt::print_stats(out, nmip > 1 ? "      " : "    ",
                                      (*img)(current_subimage, m),
                                      (*img)(current_subimage, m).nativespec(),
                                      opt.roi, err))
                    ot.errorfmt("stats", "unable to compute: {}", err);
            }
            if (!opt.subimages)
                break;
        }
    }
}



bool
OiioTool::print_info(std::ostream& out, Oiiotool& ot, ImageRec* img,
                     const pvt::print_info_options& opt, std::string& error)
{
    using Strutil::fmt::format;
    error.clear();
    if (!img) {
        error = "No image";
        return false;
    }

    ImageSpec::SerialFormat serformat = ImageSpec::SerialText;
    if (Strutil::iequals(opt.infoformat, "xml"))
        serformat = ImageSpec::SerialXML;
    ImageSpec::SerialVerbose verbose = opt.verbose
                                           ? ImageSpec::SerialDetailedHuman
                                           : ImageSpec::SerialBrief;

    std::regex field_re;
    std::regex field_exclude_re;
    if (!opt.metamatch.empty()) {
        try {
            field_re.assign(opt.metamatch, std::regex_constants::extended
                                               | std::regex_constants::icase);
        } catch (const std::exception& e) {
            error = format("Regex error '{}' on metamatch regex \"{}\"",
                           e.what(), opt.metamatch);
            return false;
        }
    }
    if (!opt.nometamatch.empty()) {
        try {
            field_exclude_re.assign(opt.nometamatch,
                                    std::regex_constants::extended
                                        | std::regex_constants::icase);
        } catch (const std::exception& e) {
            error = format("Regex error '{}' on metamatch regex \"{}\"",
                           e.what(), opt.nometamatch);
            return false;
        }
    }

    for (int s = 0, nsubimages = img->subimages(); s < nsubimages; ++s) {
        const ImageSpec* spec = opt.native ? img->nativespec(s) : img->spec(s);
        OIIO_DASSERT(spec != nullptr);
        print_info_subimage(out, ot, s, nsubimages, img->miplevels(s), *spec,
                            img, nullptr, "", opt, field_re, field_exclude_re,
                            serformat, verbose);
        // If opt.subimages is not set, we print info about first subimage
        // only.
        if (!opt.subimages)
            break;
    }

    return true;
}



bool
OiioTool::print_info(std::ostream& out, Oiiotool& ot,
                     const std::string& filename,
                     const pvt::print_info_options& opt, std::string& error)
{
    using Strutil::fmt::format;
    error.clear();
    auto input = ImageInput::open(filename, &ot.input_config);
    if (!input) {
        error = geterror();
        if (error.empty())
            error = format("Could not open \"{}\"", filename);
        return false;
    }

    ImageSpec::SerialFormat serformat = ImageSpec::SerialText;
    if (Strutil::iequals(opt.infoformat, "xml"))
        serformat = ImageSpec::SerialXML;
    ImageSpec::SerialVerbose verbose = opt.verbose
                                           ? ImageSpec::SerialDetailedHuman
                                           : ImageSpec::SerialBrief;

    std::regex field_re;
    std::regex field_exclude_re;
    if (!opt.metamatch.empty()) {
        try {
            field_re.assign(opt.metamatch, std::regex_constants::extended
                                               | std::regex_constants::icase);
        } catch (const std::exception& e) {
            error = format("Regex error '{}' on metamatch regex \"{}\"",
                           e.what(), opt.metamatch);
            return false;
        }
    }
    if (!opt.nometamatch.empty()) {
        try {
            field_exclude_re.assign(opt.nometamatch,
                                    std::regex_constants::extended
                                        | std::regex_constants::icase);
        } catch (const std::exception& e) {
            error = format("Regex error '{}' on metamatch regex \"{}\"",
                           e.what(), opt.nometamatch);
            return false;
        }
    }

    // checking how many subimages and mipmap levels are stored in the file
    std::vector<int> num_of_miplevels;
    int num_of_subimages;
    for (num_of_subimages = 0; input->seek_subimage(num_of_subimages, 0);
         ++num_of_subimages) {
        int nmip = 1;
        while (input->seek_subimage(num_of_subimages, nmip))
            ++nmip;
        num_of_miplevels.push_back(nmip);
    }

    for (int current_subimage = 0; current_subimage < num_of_subimages;
         ++current_subimage) {
        if (!input->seek_subimage(current_subimage, 0))
            break;
        print_info_subimage(out, ot, current_subimage, num_of_subimages,
                            num_of_miplevels[current_subimage], input->spec(),
                            nullptr, input.get(), filename, opt, field_re,
                            field_exclude_re, serformat, verbose);
        // if the '-a' flag is not set we print info
        // about first subimage only
        if (!opt.subimages)
            break;
    }

    return true;
}
