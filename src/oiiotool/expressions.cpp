// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>

#include "oiiotool.h"

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>


using namespace OIIO;
using namespace OiioTool;



void
Oiiotool::express_error(const string_view expr, const string_view s,
                        string_view explanation)
{
    int offset = expr.rfind(s) + 1;
    errorfmt("expression", "{} at char {} of '{}'", explanation, offset, expr);
}



// If str starts with what looks like a function call "name(" (allowing for
// whitespace before the paren), eat those chars from str and return true.
// Otherwise return false and leave str unchanged.
inline bool
parse_function_start_if(string_view& str, string_view name)
{
    string_view s = str;
    if (Strutil::parse_identifier_if(s, name) && Strutil::parse_char(s, '(')) {
        str = s;
        return true;
    }
    return false;
}



bool
Oiiotool::express_parse_atom(const string_view expr, string_view& s,
                             std::string& result)
{
    // print(" Entering express_parse_atom, s='{}'\n", s);

    string_view orig = s;
    float floatval;

    Strutil::skip_whitespace(s);

    // handle + - ! prefixes
    bool negative = false;
    bool invert   = false;
    while (s.size()) {
        if (Strutil::parse_char(s, '-')) {
            negative = !negative;
        } else if (Strutil::parse_char(s, '+')) {
            // no op
        } else if (Strutil::parse_char(s, '!')) {
            invert = !invert;
        } else {
            break;
        }
    }

    if (Strutil::parse_char(s, '(')) {
        // handle parentheses
        if (express_parse_summands(expr, s, result)) {
            if (!Strutil::parse_char(s, ')')) {
                express_error(expr, s, "missing `)'");
                result = orig;
                return false;
            }
        } else {
            result = orig;
            return false;
        }

    } else if (parse_function_start_if(s, "getattribute")) {
        // "{getattribute(name)}" retrieves global attribute `name`
        bool ok = true;
        Strutil::skip_whitespace(s);
        string_view name;
        if (s.size() && (s.front() == '\"' || s.front() == '\''))
            ok = Strutil::parse_string(s, name);
        else {
            name = Strutil::parse_until(s, ")");
        }
        if (name.size()) {
            std::string rs;
            int ri;
            float rf;
            if (OIIO::getattribute(name, rs))
                result = rs;
            else if (OIIO::getattribute(name, ri))
                result = Strutil::to_string(ri);
            else if (OIIO::getattribute(name, rf))
                result = Strutil::to_string(rf);
            else
                ok = false;
        }
        return Strutil::parse_char(s, ')') && ok;
    } else if (parse_function_start_if(s, "var")) {
        // "{var(name)}" retrieves user variable `name`
        bool ok = true;
        Strutil::skip_whitespace(s);
        string_view name;
        if (s.size() && (s.front() == '\"' || s.front() == '\''))
            ok = Strutil::parse_string(s, name);
        else {
            name = Strutil::parse_until(s, ")");
        }
        if (name.size()) {
            result = uservars[name];
        }
        return Strutil::parse_char(s, ')') && ok;
    } else if (parse_function_start_if(s, "eq")) {
        std::string left, right;
        bool ok = express_parse_atom(s, s, left) && Strutil::parse_char(s, ',');
        ok &= express_parse_atom(s, s, right) && Strutil::parse_char(s, ')');
        result = left == right ? "1" : "0";
        // Strutil::print("eq: left='{}', right='{}' ok={} result={}\n", left,
        //                right, ok, result);
        if (!ok)
            return false;
    } else if (parse_function_start_if(s, "neq")) {
        std::string left, right;
        bool ok = express_parse_atom(s, s, left) && Strutil::parse_char(s, ',');
        ok &= express_parse_atom(s, s, right) && Strutil::parse_char(s, ')');
        result = left != right ? "1" : "0";
        // Strutil::print("neq: left='{}', right='{}' ok={} result={}\n", left,
        //                right, ok, result);
        if (!ok)
            return false;
    } else if (parse_function_start_if(s, "not")) {
        std::string val;
        bool ok = express_parse_summands(s, s, val)
                  && Strutil::parse_char(s, ')');
        result = Strutil::eval_as_bool(val) ? "0" : "1";
        if (!ok)
            return false;

    } else if (Strutil::starts_with(s, "TOP")
               || Strutil::starts_with(s, "BOTTOM")
               || Strutil::starts_with(s, "IMG[")) {
        // metadata substitution
        ImageRecRef img;
        if (Strutil::parse_prefix(s, "TOP")) {
            img = curimg;
        } else if (Strutil::parse_prefix(s, "BOTTOM")) {
            img = (image_stack.size() <= 1) ? curimg : image_stack[0];
        } else if (Strutil::parse_prefix(s, "IMG[")) {
            std::string until_bracket = Strutil::parse_until(s, "]");
            if (until_bracket.empty() || !Strutil::parse_char(s, ']')) {
                express_error(expr, until_bracket,
                              "malformed IMG[] specification");
                result = orig;
                return false;
            }
            auto labelfound = image_labels.find(until_bracket);
            if (labelfound != image_labels.end()) {
                // Found an image label
                img = labelfound->second;
            } else if (Strutil::string_is_int(until_bracket)) {
                // It's an integer... don't process more quite yet
            } else if (Filesystem::exists(until_bracket)) {
                // It's the name of an image file
                img = ImageRecRef(new ImageRec(until_bracket, imagecache));
            }
            if (!img) {
                // Not a label, int, or file. Evaluate it as an expression.
                // Evaluate it as an expression and hope it's an integer or
                // the name of an image?
                until_bracket = express_impl(until_bracket);
                if (Strutil::string_is_int(until_bracket)) {
                    // Between brackets (including an expanded variable) is an
                    // integer -- it's an index into the image stack (error if out
                    // of range).
                    int index = Strutil::stoi(until_bracket);
                    if (index >= 0 && index <= (int)image_stack.size()) {
                        img = (index == 0)
                                  ? curimg
                                  : image_stack[image_stack.size() - index];
                    } else {
                        express_error(expr, until_bracket,
                                      "out-of-range IMG[] index");
                        result = orig;
                        return false;
                    }
                } else if (Filesystem::exists(until_bracket)) {
                    // It's the name of an image file
                    img = ImageRecRef(new ImageRec(until_bracket, imagecache));
                }
            }
            if (!img || img->has_error()) {
                express_error(expr, until_bracket, "not a valid image");
                result = orig;
                return false;
            }
        }
        if (!img || img->has_error()) {
            express_error(expr, s, "not a valid image");
            result = orig;
            return false;
        }
        OIIO_ASSERT(img);
        img->read();
        bool using_bracket = false;
        if (Strutil::parse_char(s, '[')) {
            using_bracket = true;
        } else if (!Strutil::parse_char(s, '.')) {
            express_error(expr, s, "expected `.` or `[`");
            result = orig;
            return false;
        }
        string_view metadata;
        char quote             = s.size() ? s.front() : ' ';
        bool metadata_in_quote = quote == '\"' || quote == '\'';
        if (metadata_in_quote)
            Strutil::parse_string(s, metadata);
        else
            metadata = Strutil::parse_identifier(s, ":");
        if (using_bracket) {
            if (!Strutil::parse_char(s, ']')) {
                express_error(expr, s, "expected `]`");
                result = orig;
                return false;
            }
        }
        if (metadata.size()) {
            read(img);
            ParamValue tmpparam;
            if (metadata == "nativeformat") {
                result = img->nativespec(0, 0)->format.c_str();
            } else if (auto p = img->spec(0, 0)->find_attribute(metadata,
                                                                tmpparam)) {
                std::string val = ImageSpec::metadata_val(*p);
                if (p->type().basetype == TypeDesc::STRING) {
                    // metadata_val returns strings double quoted, strip
                    val.erase(0, 1);
                    val.erase(val.size() - 1, 1);
                }
                result = val;
            } else if (metadata == "filename")
                result = img->name();
            else if (metadata == "file_extension")
                result = Filesystem::extension(img->name());
            else if (metadata == "file_noextension") {
                std::string filename = img->name();
                std::string ext      = Filesystem::extension(img->name());
                result = filename.substr(0, filename.size() - ext.size());
            } else if (metadata == "MINCOLOR") {
                auto pixstat = ImageBufAlgo::computePixelStats((*img)(0, 0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.min.size(); ++i)
                    out << (i ? "," : "") << pixstat.min[i];
                result = out.str();
            } else if (metadata == "MAXCOLOR") {
                auto pixstat = ImageBufAlgo::computePixelStats((*img)(0, 0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.max.size(); ++i)
                    out << (i ? "," : "") << pixstat.max[i];
                result = out.str();
            } else if (metadata == "AVGCOLOR") {
                auto pixstat = ImageBufAlgo::computePixelStats((*img)(0, 0));
                std::stringstream out;
                for (size_t i = 0; i < pixstat.avg.size(); ++i)
                    out << (i ? "," : "") << pixstat.avg[i];
                result = out.str();
            } else if (metadata == "NONFINITE_COUNT") {
                auto pixstat    = ImageBufAlgo::computePixelStats((*img)(0, 0));
                imagesize_t sum = std::accumulate(pixstat.nancount.begin(),
                                                  pixstat.nancount.end(), 0)
                                  + std::accumulate(pixstat.infcount.begin(),
                                                    pixstat.infcount.end(), 0);
                result = Strutil::to_string(sum);
            } else if (metadata == "META" || metadata == "METANATIVE") {
                std::stringstream out;
                print_info_options opt;
                opt.verbose   = true;
                opt.subimages = true;
                opt.native    = (metadata == "METANATIVE");
                std::string error;
                OiioTool::print_info(out, *this, img.get(), opt, error);
                result = out.str();
                if (result.size() && result.back() == '\n')
                    result.pop_back();
            } else if (metadata == "METABRIEF"
                       || metadata == "METANATIVEBRIEF") {
                std::stringstream out;
                print_info_options opt;
                opt.verbose   = false;
                opt.subimages = false;
                opt.native    = (metadata == "METANATIVEBRIEF");
                std::string error;
                OiioTool::print_info(out, *this, img.get(), opt, error);
                result = out.str();
                if (result.size() && result.back() == '\n')
                    result.pop_back();
            } else if (metadata == "STATS") {
                std::stringstream out;
                // OiioTool::print_stats(out, *this, (*img)());

                std::string err;
                if (!pvt::print_stats(out, "", (*img)(), (*img)().nativespec(),
                                      ROI(), err))
                    errorfmt("stats", "unable to compute: {}", err);

                result = out.str();
                if (result.size() && result.back() == '\n')
                    result.pop_back();
            } else if (metadata == "IS_CONSTANT") {
                std::vector<float> color((*img)(0, 0).nchannels());
                if (ImageBufAlgo::isConstantColor((*img)(0, 0), 0.0f, color)) {
                    result = "1";
                } else {
                    result = "0";
                }
            } else if (metadata == "IS_BLACK") {
                std::vector<float> color((*img)(0, 0).nchannels());
                // Check constant first to guard against false positive average of 0 with negative values i.e. -2, 1, 1
                if (ImageBufAlgo::isConstantColor((*img)(0, 0), 0.0f, color)) {
                    // trusting that the constantcolor check means all channels have the same value, so we only check the first channel
                    if (color[0] == 0.0f) {
                        result = "1";
                    } else {
                        result = "0";
                    }
                } else {
                    // Not even constant color case -> We don't want those to count as black frames.
                    result = "0";
                }

            } else if (using_bracket) {
                // For the TOP[meta] syntax, if the metadata doesn't exist,
                // return the empty string, and do not make an error.
                result = "";
            } else {
                express_error(expr, s,
                              Strutil::fmt::format("unknown attribute name '{}'",
                                                   metadata));
                result = orig;
                return false;
            }
        }
    } else if (Strutil::parse_float(s, floatval)) {
        result = Strutil::fmt::format("{:g}", floatval);
    } else if (Strutil::parse_char(s, '\"', true, false)
               || Strutil::parse_char(s, '\'', true, false)) {
        string_view r;
        Strutil::parse_string(s, r);
        result = r;
    }
    // Test some special identifiers
    else if (Strutil::parse_identifier_if(s, "FRAME_NUMBER")) {
        result = Strutil::to_string(frame_number);
    } else if (Strutil::parse_identifier_if(s, "FRAME_NUMBER_PAD")) {
        std::string fmt = frame_padding == 0
                              ? std::string("{}")
                              : Strutil::fmt::format("\"{{:0{}d}}\"",
                                                     frame_padding);
        result          = Strutil::fmt::format(fmt, frame_number);
    } else if (Strutil::parse_identifier_if(s, "NIMAGES")) {
        result = Strutil::to_string(image_stack_depth());
    } else {
        string_view id = Strutil::parse_identifier(s, false);
        if (id.size() && uservars.contains(id)) {
            result = uservars[id];
            Strutil::parse_identifier(s, true);  // eat the id
        } else {
            express_error(expr, s, "syntax error");
            result = orig;
            return false;
        }
    }

    if (negative)
        result = "-" + result;
    if (invert)
        result = Strutil::eval_as_bool(result) ? "0" : "1";

    // print(" Exiting express_parse_atom, result='{}'\n", result);

    return true;
}



bool
Oiiotool::express_parse_factors(const string_view expr, string_view& s,
                                std::string& result)
{
    // print(" Entering express_parse_factrors, s='{}'\n", s);

    string_view orig = s;
    std::string atom;
    float lval, rval;

    // parse the first factor
    if (!express_parse_atom(expr, s, atom)) {
        result = orig;
        return false;
    }

    if (atom.size() >= 2 && atom.front() == '\"' && atom.back() == '\"') {
        // Double quoted is string, return it
        result = atom;
    } else if (Strutil::string_is<float>(atom)) {
        // lval is a number
        lval = Strutil::from_string<float>(atom);
        while (s.size()) {
            enum class Ops { mul, div, idiv, imod };
            Ops op;
            if (Strutil::parse_char(s, '*'))
                op = Ops::mul;
            else if (Strutil::parse_prefix(s, "//"))
                op = Ops::idiv;
            else if (Strutil::parse_char(s, '/'))
                op = Ops::div;
            else if (Strutil::parse_char(s, '%'))
                op = Ops::imod;
            else {
                // no more factors
                break;
            }

            // parse the next factor
            if (!express_parse_atom(expr, s, atom)) {
                result = orig;
                return false;
            }

            if (!Strutil::string_is<float>(atom)) {
                express_error(
                    expr, s,
                    Strutil::fmt::format("expected number but got '{}'", atom));
                result = orig;
                return false;
            }

            // rval is a number, so we can math
            rval = Strutil::from_string<float>(atom);
            if (op == Ops::mul)
                lval *= rval;
            else if (op == Ops::div)
                lval /= rval;
            else if (op == Ops::idiv) {
                int ilval(lval), irval(rval);
                lval = float(rval ? ilval / irval : 0);
            } else if (op == Ops::imod) {
                int ilval(lval), irval(rval);
                lval = float(rval ? ilval % irval : 0);
            }
        }

        result = Strutil::fmt::format("{:g}", lval);

    } else {
        // atom is not a number, so we're done
        result = atom;
    }

    // print(" Exiting express_parse_factors, result='{}'\n", result);

    return true;
}



bool
Oiiotool::express_parse_summands(const string_view expr, string_view& s,
                                 std::string& result)
{
    // print(" Entering express_parse_summands, s='{}'\n", s);

    string_view orig = s;
    std::string atom;

    // parse the first summand
    if (!express_parse_factors(expr, s, atom)) {
        result = orig;
        return false;
    }

    if (atom.size() >= 2 && atom.front() == '\"' && atom.back() == '\"') {
        // Double quoted is string, strip it
        result = atom.substr(1, atom.size() - 2);
    } else if (Strutil::string_is<float>(atom)) {
        // lval is a number
        float lval = Strutil::from_string<float>(atom);
        while (s.size()) {
            Strutil::skip_whitespace(s);
            string_view op = Strutil::parse_while(s, "+-<=>!&|");
            if (op == "") {
                // no more summands
                break;
            }

            // parse the next summand
            if (!express_parse_factors(expr, s, atom)) {
                result = orig;
                return false;
            }

            if (!Strutil::string_is<float>(atom)) {
                express_error(expr, s,
                              Strutil::fmt::format("'{}' is not a number",
                                                   atom));
                result = orig;
                return false;
            }

            // rval is also a number, we can math
            float rval = Strutil::from_string<float>(atom);
            if (op == "+") {
                lval += rval;
            } else if (op == "-") {
                lval -= rval;
            } else if (op == "<") {
                lval = (lval < rval) ? 1 : 0;
            } else if (op == ">") {
                lval = (lval > rval) ? 1 : 0;
            } else if (op == "<=") {
                lval = (lval <= rval) ? 1 : 0;
            } else if (op == ">=") {
                lval = (lval >= rval) ? 1 : 0;
            } else if (op == "==") {
                lval = (lval == rval) ? 1 : 0;
            } else if (op == "!=") {
                lval = (lval != rval) ? 1 : 0;
            } else if (op == "<=>") {
                lval = (lval < rval) ? -1 : (lval > rval ? 1 : 0);
            } else if (op == "&&" || op == "&") {
                lval = (lval != 0.0f && rval != 0.0f) ? 1 : 0;
            } else if (op == "||" || op == "|") {
                lval = (lval != 0.0f || rval != 0.0f) ? 1 : 0;
            }
        }

        result = Strutil::fmt::format("{:g}", lval);

    } else {
        // atom is not a number, so we're done
        result = atom;
    }

    // print(" Exiting express_parse_summands, result='{}'\n", result);

    return true;
}



// Expression evaluation and substitution for a single expression
std::string
Oiiotool::express_impl(string_view s)
{
    std::string result;
    string_view orig = s;
    if (!express_parse_summands(orig, s, result)) {
        result = orig;
    }
    return result;
}



// Perform expression evaluation and substitution on a string
string_view
Oiiotool::express(string_view str)
{
    if (!eval_enable)
        return str;  // Expression evaluation disabled

    string_view s = str;
    // eg. s="ab{cde}fg"
    size_t openbrace = s.find('{');
    if (openbrace == s.npos)
        return str;  // No open brace found -- no expression substitution

    string_view prefix = s.substr(0, openbrace);
    s.remove_prefix(openbrace);
    // eg. s="{cde}fg", prefix="ab"
    string_view expr = Strutil::parse_nested(s);
    if (expr.empty())
        return str;  // No corresponding close brace found -- give up
    // eg. prefix="ab", expr="{cde}", s="fg", prefix="ab"
    OIIO_ASSERT(expr.front() == '{' && expr.back() == '}');
    expr.remove_prefix(1);
    expr.remove_suffix(1);
    // eg. expr="cde"
    ustring result = ustring::fmtformat("{}{}{}", prefix, express_impl(expr),
                                        express(s));
    if (debug)
        print("Expanding expression \"{}\" -> \"{}\"\n", str, result);
    return result;
}
