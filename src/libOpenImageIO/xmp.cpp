// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <iostream>

#include <boost/container/flat_map.hpp>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/tiffutils.h>

extern "C" {
#include "tiff.h"
}

#if USE_EXTERNAL_PUGIXML
#    include <pugixml.hpp>
#else
#    include <OpenImageIO/detail/pugixml/pugixml.hpp>
#endif

#define DEBUG_XMP_READ 0
#define DEBUG_XMP_WRITE 0

#define MY_ENCODING "ISO-8859-1"

OIIO_NAMESPACE_BEGIN

namespace {  // anonymous


// Define special processing flags -- they're individual bits so can be
// combined with '|'
enum XMPspecial {
    NothingSpecial = 0,
    Rational       = 1,   // It needs to be expressed as A/B
    DateConversion = 2,   // It's a date, may need conversion to canonical form
    TiffRedundant  = 4,   // It's something that's part of normal TIFF tags
    ExifRedundant  = 8,   // It's something included in Exif
    Suppress       = 16,  // Explicitly suppress it from XMP
    IsList         = 32,  // Make a semicolon-separated list out of it
    IsSeq          = 64,  // Like List, but order matters
    IsBool         = 128  // Should be output as True/False
};

struct XMPtag {
    const char* xmpname;   // XMP name
    const char* oiioname;  // Attribute name we use
    TypeDesc oiiotype;     // Type we use
    int special;           // Special handling

    XMPtag(const char* xname, const char* oname, TypeDesc type = TypeUnknown,
           int spec = 0)
        : xmpname(xname)
        , oiioname(oname)
        , oiiotype(type)
        , special(spec)
    {
    }
};

static XMPtag xmptag[] = {
    // clang-format off
    { "photoshop:AuthorsPosition", "IPTC:AuthorsPosition", TypeDesc::STRING, 0 },
    { "photoshop:CaptionWriter", "IPTC:CaptionWriter", TypeDesc::STRING, 0 },
    { "photoshop:Category", "IPTC:Category", TypeDesc::STRING, 0 },
    { "photoshop:City", "IPTC:City", TypeDesc::STRING, 0 },
    { "photoshop:Country", "IPTC:Country", TypeDesc::STRING, 0 },
    { "photoshop:Credit", "IPTC:Provider", TypeDesc::STRING, 0 },
    { "photoshop:DateCreated", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
    { "photoshop:Headline", "IPTC:Headline", TypeDesc::STRING, 0 },
    { "photoshop:History", "ImageHistory", TypeDesc::STRING, 0 },
    { "photoshop:Instructions", "IPTC:Instructions", TypeDesc::STRING, 0 },
    { "photoshop:Source", "IPTC:Source", TypeDesc::STRING, 0 },
    { "photoshop:State", "IPTC:State", TypeDesc::STRING, 0 },
    { "photoshop:SupplementalCategories", "IPTC:SupplementalCategories", TypeDesc::STRING, IsList|Suppress },  // FIXME -- un-suppress when we have it working
    { "photoshop:TransmissionReference", "IPTC:TransmissionReference", TypeDesc::STRING, 0 },
    { "photoshop:Urgency", "photoshop:Urgency", TypeDesc::INT, 0 },

    { "tiff:Compression", "tiff:Compression", TypeDesc::INT, TiffRedundant },
    { "tiff:PlanarConfiguration", "tiff:PlanarConfiguration", TypeDesc::INT, TiffRedundant },
    { "tiff:PhotometricInterpretation", "tiff:PhotometricInterpretation", TypeDesc::INT, TiffRedundant },
    { "tiff:subfiletype", "tiff:subfiletype", TypeDesc::INT, TiffRedundant },
    { "tiff:Orientation", "Orientation", TypeDesc::INT, TiffRedundant },
    { "tiff:XResolution", "XResolution", TypeDesc::FLOAT, Rational|TiffRedundant },
    { "tiff:YResolution", "YResolution", TypeDesc::FLOAT, Rational|TiffRedundant },
    { "tiff:ResolutionUnit", "ResolutionUnit", TypeDesc::INT, TiffRedundant },
    { "tiff:Artist", "Artist", TypeDesc::STRING, 0 },
    { "tiff:Copyright", "Copyright", TypeDesc::STRING, 0 },
    { "tiff:DateTime", "DateTime", TypeDesc::STRING, DateConversion },
    { "tiff:ImageDescription", "ImageDescription", TypeDesc::STRING, 0 },
    { "tiff:Make", "Make", TypeDesc::STRING, 0 },
    { "tiff:Model", "Model", TypeDesc::STRING, 0 },
    { "tiff:Software", "Software", TypeDesc::STRING, TiffRedundant },

    { "exif:ColorSpace", "Exif:ColorSpace", TypeDesc::INT, ExifRedundant },
    { "exif:PixelXDimension", "", TypeDesc::INT, ExifRedundant|TiffRedundant},
    { "exif:PixelYDimension", "", TypeDesc::INT, ExifRedundant|TiffRedundant },
    { "exifEX:PhotographicSensitivity", "Exif:ISOSpeedRatings", TypeDesc::INT, ExifRedundant },

    { "xmp:CreateDate", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
    { "xmp:CreatorTool", "Software", TypeDesc::STRING, TiffRedundant },
    { "xmp:Label", "IPTC:Label", TypeDesc::STRING, 0 },
    { "xmp:MetadataDate", "IPTC:MetadataDate", TypeDesc::STRING, DateConversion },
    { "xmp:ModifyDate", "IPTC:ModifyDate", TypeDesc::STRING, DateConversion },
    { "xmp:Rating", "IPTC:Rating", TypeDesc::INT, 0 },

    { "xmpMM:DocumentID", "IPTC:DocumentID", TypeDesc::STRING, 0 },
    { "xmpMM:History", "ImageHistory", TypeDesc::STRING, IsSeq|Suppress },
    { "xmpMM:InstanceID", "IPTC:InstanceID", TypeDesc::STRING, 0 },
    { "xmpMM:OriginalDocumentID", "IPTC:OriginalDocumentID", TypeDesc::STRING, 0 },

    { "xmpRights:Marked", "IPTC:CopyrightStatus", TypeDesc::INT, IsBool },
    { "xmpRights:WebStatement", "IPTC:CopyrightInfoURL", TypeDesc::STRING, 0 },
    { "xmpRights:UsageTerms", "IPTC:RightsUsageTerms", TypeDesc::STRING, 0 },

    { "dc:format", "", TypeDesc::STRING, TiffRedundant|Suppress },
    { "dc:Description", "ImageDescription", TypeDesc::STRING, TiffRedundant },
    { "dc:Creator", "Artist", TypeDesc::STRING, TiffRedundant },
    { "dc:Rights", "Copyright", TypeDesc::STRING, TiffRedundant },
    { "dc:title", "IPTC:ObjectName", TypeDesc::STRING, 0 },
    { "dc:subject", "Keywords", TypeDesc::STRING, IsList },
    { "dc:keywords", "Keywords", TypeDesc::STRING, IsList },

    { "Iptc4xmpCore:IntellectualGenre", "IPTC:IntellectualGenre", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CountryCode", "IPTC:CountryCode", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CreatorContactInfo", "IPTC:CreatorContactInfo", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:ContactInfoDetails", "IPTC:Contact", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrExtadr", "IPTC:ContactInfoAddress", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrCity", "IPTC:ContactInfoCity", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrRegion", "IPTC:ContactInfoState", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrPcode", "IPTC:ContactInfoPostalCode", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrCtry", "IPTC:ContactInfoCountry", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiEmailWork", "IPTC:ContactInfoEmail", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiTelWork", "IPTC:ContactInfoPhone", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiUrlWork", "IPTC:ContactInfoURL", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:Location", "IPTC:Sublocation", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:SubjectCode", "IPTC:SubjectCode", TypeDesc::STRING, IsList },
    { "Iptc4xmpCore:Scene", "IPTC:SceneCode", TypeDesc::STRING, IsList },
    { "Iptc4xmpExt:PersonInImage", "IPTC:PersonInImage", TypeDesc::STRING, IsList },

    { "aux::Firmware", "aux:Firmware", TypeDesc::STRING, 0},

    { "crs:AutoBrightness", "crs:AutoBrightness"  , TypeDesc::INT, IsBool },
    { "crs:AutoContrast", "crs:AutoContrast"    , TypeDesc::INT, IsBool },
    { "crs:AutoExposure", "crs:AutoExposure"    , TypeDesc::INT, IsBool },
    { "crs:AutoShadows", "crs:AutoShadows"     , TypeDesc::INT, IsBool },
    { "crs:BlueHue", "crs:BlueHue"         , TypeDesc::INT, 0 },
    { "crs:BlueSaturation", "crs:BlueSaturation"  , TypeDesc::INT, 0 },
    { "crs:Brightness", "crs:Brightness"      , TypeDesc::INT, 0 },
    { "crs:CameraProfile", "crs:CameraProfile"   , TypeDesc::STRING, 0 },
    { "crs:ChromaticAberrationB", "crs:ChromaticAberrationB"    , TypeDesc::INT, 0 },
    { "crs:ChromaticAberrationR", "crs:ChromaticAberrationR"    , TypeDesc::INT, 0 },
    { "crs:ColorNoiseReduction", "crs:ColorNoiseReduction" , TypeDesc::INT, 0 },
    { "crs:Contrast", "crs:Contrast", TypeDesc::INT, 0 },
    { "crs:CropTop", "crs:CropTop", TypeDesc::FLOAT, 0 },
    { "crs:CropLeft", "crs:CropLeft", TypeDesc::FLOAT, 0 },
    { "crs:CropBottom", "crs:CropBottom", TypeDesc::FLOAT, 0 },
    { "crs:CropRight", "crs:CropRight", TypeDesc::FLOAT, 0 },
    { "crs:CropAngle", "crs:CropAngle", TypeDesc::FLOAT, 0 },
    { "crs:CropWidth", "crs:CropWidth", TypeDesc::FLOAT, 0 },
    { "crs:CropHeight", "crs:CropHeight", TypeDesc::FLOAT, 0 },
    { "crs:CropUnits", "crs:CropUnits", TypeDesc::INT, 0 },
    { "crs:Exposure", "crs:Exposure", TypeDesc::FLOAT, 0 },
    { "crs:GreenHue", "crs:GreenHue", TypeDesc::INT, 0 },
    { "crs:GreenSaturation", "crs:GreenSaturation", TypeDesc::INT, 0 },
    { "crs:HasCrop", "crs:HasCrop", TypeDesc::INT, IsBool },
    { "crs:HasSettings", "crs:HasSettings", TypeDesc::INT, IsBool },
    { "crs:LuminanceSmoothing", "crs:LuminanceSmoothing", TypeDesc::INT, 0 },
    { "crs:RawFileName", "crs:RawFileName", TypeDesc::STRING, 0 },
    { "crs:RedHue", "crs:RedHue", TypeDesc::INT, 0 },
    { "crs:RedSaturation", "crs:RedSaturation", TypeDesc::INT, 0 },
    { "crs:Saturation", "crs:Saturation", TypeDesc::INT, 0 },
    { "crs:Shadows", "crs:Shadows", TypeDesc::INT, 0 },
    { "crs:ShadowTint", "crs:ShadowTint", TypeDesc::INT, 0 },
    { "crs:Sharpness", "crs:Sharpness", TypeDesc::INT, 0 },
    { "crs:Temperature", "crs:Temperature", TypeDesc::INT, 0 },
    { "crs:Tint", "crs:Tint", TypeDesc::INT, 0 },
    { "crs:ToneCurve", "crs:ToneCurve", TypeDesc::STRING, 0 },
    { "crs:ToneCurveName", "crs:ToneCurveName", TypeDesc::STRING, 0 },
    { "crs:Version", "crs:Version", TypeDesc::STRING, 0 },
    { "crs:VignetteAmount", "crs:VignetteAmount", TypeDesc::INT, 0 },
    { "crs:VignetteMidpoint", "crs:VignetteMidpoint", TypeDesc::INT, 0 },
    { "crs:WhiteBalance", "crs:WhiteBalance", TypeDesc::STRING, 0 },

    { "GPano:UsePanoramaViewer", "GPano:UsePanoramaViewer", TypeDesc::INT, IsBool },
    { "GPano:CaptureSoftware", "GPano:CaptureSoftware", TypeDesc::STRING, 0 },
    { "GPano:StitchingSoftware", "GPano:StitchingSoftware", TypeDesc::STRING, 0 },
    { "GPano:ProjectionType", "GPano:ProjectionType", TypeDesc::STRING, 0 },
    { "GPano:PoseHeadingDegrees", "GPano:PoseHeadingDegrees", TypeDesc::FLOAT, 0 },
    { "GPano:PosePitchDegrees", "GPano:PosePitchDegrees", TypeDesc::FLOAT, 0 },
    { "GPano:PoseRollDegrees", "GPano:PoseRollDegrees", TypeDesc::FLOAT, 0 },
    { "GPano:InitialViewHeadingDegrees", "GPano:InitialViewHeadingDegrees", TypeDesc::INT, 0 },
    { "GPano:InitialViewPitchDegrees", "GPano:InitialViewPitchDegrees", TypeDesc::INT, 0 },
    { "GPano:InitialViewRollDegrees", "GPano:InitialViewRollDegrees", TypeDesc::INT, 0 },
    { "GPano:InitialHorizontalFOVDegrees", "GPano:InitialHorizontalFOVDegrees", TypeDesc::FLOAT, 0 },
    { "GPano:FirstPhotoDate", "GPano:FirstPhotoDate", TypeDesc::STRING, DateConversion },
    { "GPano:LastPhotoDate", "GPano:LastPhotoDate", TypeDesc::STRING, DateConversion },
    { "GPano:SourcePhotosCount", "GPano:SourcePhotosCount", TypeDesc::INT, 0 },
    { "GPano:ExposureLockUsed", "GPano:ExposureLockUsed", TypeDesc::INT, IsBool },
    { "GPano:CroppedAreaImageWidthPixels", "GPano:CroppedAreaImageWidthPixels", TypeDesc::INT, 0 },
    { "GPano:CroppedAreaImageHeightPixels", "GPano:CroppedAreaImageHeightPixels", TypeDesc::INT, 0 },
    { "GPano:FullPanoWidthPixels", "GPano:FullPanoWidthPixels", TypeDesc::INT, 0 },
    { "GPano:FullPanoHeightPixels", "GPano:FullPanoHeightPixels", TypeDesc::INT, 0 },
    { "GPano:CroppedAreaLeftPixels", "GPano:CroppedAreaLeftPixels", TypeDesc::INT, 0 },
    { "GPano:CroppedAreaTopPixels", "GPano:CroppedAreaTopPixels", TypeDesc::INT, 0 },
    { "GPano:InitialCameraDolly", "GPano:InitialCameraDolly", TypeDesc::FLOAT, 0 },
    { "GPano:LargestValidInteriorRectWidth", "GPano:LargestValidInteriorRectWidth", TypeDesc::INT, 0 },
    { "GPano:LargestValidInteriorRectHeight", "GPano:LargestValidInteriorRectHeight", TypeDesc::INT, 0 },
    { "GPano:LargestValidInteriorRectTop", "GPano:LargestValidInteriorRectTop", TypeDesc::INT, 0 },
    { "GPano:LargestValidInteriorRectLeft", "GPano:LargestValidInteriorRectLeft", TypeDesc::INT, 0 },

    { "rdf:li", "" },  // ignore these strays
    { nullptr, nullptr }
    // clang-format on
};



class XMPtagMap {
    typedef boost::container::flat_map<std::string, const XMPtag*> tagmap_t;
    // Key is lower case so it's effectively case-insensitive
public:
    XMPtagMap(const XMPtag* tag_table)
    {
        for (const XMPtag* t = &tag_table[0]; t->xmpname; ++t) {
            std::string lower(t->xmpname);
            Strutil::to_lower(lower);
            m_tagmap[lower] = t;
        }
    }

    const XMPtag* find(string_view name) const
    {
        std::string lower = name;
        Strutil::to_lower(lower);
        tagmap_t::const_iterator i = m_tagmap.find(lower);
        return i == m_tagmap.end() ? nullptr : i->second;
    }

private:
    tagmap_t m_tagmap;
};

static XMPtagMap&
xmp_tagmap_ref()
{
    static XMPtagMap T(xmptag);
    return T;
}



// Utility: add an attribute to the spec with the given xml name and
// value.  Search for it in xmptag, and if found that will tell us what
// the type is supposed to be, as well as any special handling.  If not
// found in the table, add it as a string and hope for the best.
static void
add_attrib(ImageSpec& spec, const char* xmlname, const char* xmlvalue)
{
#if DEBUG_XMP_READ
    std::cerr << "add_attrib " << xmlname << ": '" << xmlvalue << "'\n";
#endif
    std::string oiioname = xmlname;
    TypeDesc oiiotype;
    int special = NothingSpecial;

    // See if it's in the xmp table, which will tell us something about the
    // proper type (everything in the xml itself just looks like a string).
    if (const XMPtag* xt = xmp_tagmap_ref().find(xmlname)) {
        if (!xt->oiioname || !xt->oiioname[0])
            return;  // ignore it purposefully
        // Found
        oiioname = xt->oiioname;
        oiiotype = xt->oiiotype;
        special  = xt->special;
    }

    // Also try looking it up to see if it's a known exif tag.
    int tag = -1, tifftype = -1, count = 0;
    if (Strutil::istarts_with(xmlname, "Exif:")
        && (exif_tag_lookup(xmlname, tag, tifftype, count)
            || exif_tag_lookup(xmlname + 5, tag, tifftype, count))) {
        // It's a known Exif name
        if (tifftype == TIFF_SHORT && count == 1)
            oiiotype = TypeDesc::UINT;
        else if (tifftype == TIFF_LONG && count == 1)
            oiiotype = TypeDesc::UINT;
        else if ((tifftype == TIFF_RATIONAL || tifftype == TIFF_SRATIONAL)
                 && count == 1) {
            oiiotype = TypeDesc::FLOAT;
            special  = Rational;
        } else if (tifftype == TIFF_ASCII)
            oiiotype = TypeDesc::STRING;
        else if (tifftype == TIFF_BYTE && count == 1)
            oiiotype = TypeDesc::INT;
        else if (tifftype == TIFF_NOTYPE)
            return;  // skip
    }

    if (oiiotype == TypeDesc::STRING) {
        std::string val;
        if (special & (IsList | IsSeq)) {
            // Special case -- append it to a list
            std::vector<std::string> items;
            ParamValue* p = spec.find_attribute(oiioname, TypeDesc::STRING);
            bool dup      = false;
            if (p) {
                Strutil::split(*(const char**)p->data(), items, ";");
                for (auto& item : items) {
                    item = Strutil::strip(item);
                    dup |= (item == xmlvalue);
                }
                dup |= (xmlvalue == std::string(*(const char**)p->data()));
            }
            if (!dup)
                items.emplace_back(xmlvalue);
            val = Strutil::join(items, "; ");
        } else {
            val = xmlvalue;
        }
        spec.attribute(oiioname, val);
        return;
    } else if (oiiotype == TypeDesc::INT) {
        if (special & IsBool)
            spec.attribute(oiioname, (int)Strutil::iequals(xmlvalue, "true"));
        else  // ordinary int
            spec.attribute(oiioname, (int)Strutil::stoi(xmlvalue));
        return;
    } else if (oiiotype == TypeDesc::UINT) {
        spec.attribute(oiioname, Strutil::from_string<unsigned int>(xmlvalue));
        return;
    } else if (oiiotype == TypeDesc::FLOAT) {
        float f           = Strutil::stoi(xmlvalue);
        const char* slash = strchr(xmlvalue, '/');
        if (slash)  // It's rational!
            f /= (float)Strutil::stoi(slash + 1);
        spec.attribute(oiioname, f);
        return;
    }
#if (!defined(NDEBUG) || DEBUG_XMP_READ)
    else {
        std::cerr << "iptc xml add_attrib unknown type " << xmlname << ' '
                  << oiiotype.c_str() << "\n";
    }
#endif

#if 0
    // Guess that if it's exactly an integer, it's an integer.
    string_view intstring (xmlvalue);
    int intval;
    if (intstring.size() && intstring[0] != ' ' &&
          Strutil::parse_int(intstring, intval, true) &&
          intstring.size() == 0) {
        spec.attribute (xmlname, intval);
        return;
    }

    // If it's not exactly an int, but is exactly a float, guess that it's
    // a float.
    string_view floatstring (xmlvalue);
    float floatval;
    if (floatstring.size() && floatstring[0] != ' ' &&
          Strutil::parse_float(floatstring, floatval, true) &&
          floatstring.size() == 0) {
        spec.attribute (xmlname, floatval);
        return;
    }
#endif

    // Catch-all for unrecognized things -- just add them as a string!
    spec.attribute(xmlname, xmlvalue);
}



// Utility: Search str for the first substring in str (starting from
// position pos) that starts with startmarker and ends with endmarker.
// If not found, return false.  If found, return true, store the
// beginning and ending indices in startpos and endpos.
static bool
extract_middle(string_view str, size_t pos, string_view startmarker,
               string_view endmarker, size_t& startpos, size_t& endpos)
{
    startpos = str.find(startmarker, pos);
    if (startpos == std::string::npos)
        return false;  // start marker not found
    endpos = str.find(endmarker, startpos);
    if (endpos == std::string::npos)
        return false;  // end marker not found
    endpos += endmarker.size();
    return true;
}


static void
decode_xmp_node(pugi::xml_node node, ImageSpec& spec, int level = 1,
                const char* parentname = NULL, bool /*isList*/ = false)
{
    std::string mylist;  // will accumulate for list items
    for (; node; node = node.next_sibling()) {
#if DEBUG_XMP_READ
        std::cerr << "Level " << level << " " << node.name() << " = "
                  << node.value() << "\n";
#endif
        // First, encode all attributes of this node
        for (pugi::xml_attribute attr = node.first_attribute(); attr;
             attr                     = attr.next_attribute()) {
#if DEBUG_XMP_READ
            std::cerr << "   level " << level << " parent "
                      << (parentname ? parentname : "-") << " attr "
                      << attr.name() << ' ' << attr.value() << "\n";
#endif
            if (Strutil::istarts_with(attr.name(), "xml:")
                || Strutil::istarts_with(attr.name(), "xmlns:"))
                continue;  // xml attributes aren't image metadata
            if (attr.name()[0] && attr.value()[0])
                add_attrib(spec, attr.name(), attr.value());
        }
        if (Strutil::iequals(node.name(), "xmpMM::History")) {
            // FIXME -- image history is complicated. Come back to it.
            continue;
        }
        if (Strutil::iequals(node.name(), "rdf:Bag")
            || Strutil::iequals(node.name(), "rdf:Seq")
            || Strutil::iequals(node.name(), "rdf:Alt")
            || Strutil::iequals(node.name(), "rdf:li")) {
            // Various kinds of lists.  Recuse, pass the parent name
            // down, and let the child know it's part of a list.
            decode_xmp_node(node.first_child(), spec, level + 1, parentname,
                            true);
        } else {
            // Not a list, but it's got children.  Recurse.
            decode_xmp_node(node.first_child(), spec, level + 1, node.name());
        }

        // If this node has a value but no name, it's definitely part
        // of a list.  Accumulate the list items, separated by semicolons.
        if (parentname && !node.name()[0] && node.value()[0]) {
            if (mylist.size())
                mylist += ";";
            mylist += node.value();
        }
    }

    // If we have accumulated a list, turn it into an attribute
    if (parentname && mylist.size()) {
        add_attrib(spec, parentname, mylist.c_str());
    }
}


}  // anonymous namespace



// DEPRECATED(2.1)
bool
decode_xmp(const std::string& xml, ImageSpec& spec)
{
    return decode_xmp(string_view(xml), spec);
}



// DEPRECATED(2.1)
bool
decode_xmp(const char* xml, ImageSpec& spec)
{
    return decode_xmp(string_view(xml), spec);
}



bool
decode_xmp(cspan<uint8_t> xml, ImageSpec& spec)
{
    return decode_xmp(string_view((const char*)xml.data(), xml.size()), spec);
}



bool
decode_xmp(string_view xml, ImageSpec& spec)
{
#if DEBUG_XMP_READ
    std::cerr << "XMP dump:\n---\n" << xml << "\n---\n";
#endif
    if (!xml.length())
        return true;
    for (size_t startpos = 0, endpos = 0;
         extract_middle(xml, endpos, "<rdf:Description", "</rdf:Description>",
                        startpos, endpos);) {
        // Turn that middle section into an XML document
        string_view rdf = xml.substr(startpos, endpos - startpos);  // scooch in
#if DEBUG_XMP_READ
        std::cerr << "RDF is:\n---\n" << rdf << "\n---\n";
#endif
        pugi::xml_document doc;
        pugi::xml_parse_result parse_result
            = doc.load_buffer(rdf.data(), rdf.size(),
                              pugi::parse_default | pugi::parse_fragment);
        if (!parse_result) {
#if DEBUG_XMP_READ
            std::cerr << "Error parsing XML @" << parse_result.offset << ": "
                      << parse_result.description() << "\n";
#endif
            // Instead of returning early here if there were errors parsing
            // the XML -- I have noticed that very minor XML malformations
            // are common in XMP found in files -- hope for the best and
            // go ahead and assume that maybe it managed to put something
            // useful in the resulting document.
#if 0
            return true;
#endif
        }
        // Decode the contents of the XML document (it will recurse)
        decode_xmp_node(doc.first_child(), spec);
    }

    return true;
}



// Turn one ParamValue (whose xmp info we know) into a properly
// serialized xmp string.
static std::string
stringize(const ParamValueList::const_iterator& p, const XMPtag& xmptag)
{
    if (p->type() == TypeDesc::STRING) {
        if (xmptag.special & DateConversion) {
            // FIXME -- convert to yyyy-mm-ddThh:mm:ss.sTZD
            // return std::string();
        }
        return std::string(*(const char**)p->data());
    } else if (p->type() == TypeDesc::INT) {
        if (xmptag.special & IsBool)
            return *(const int*)p->data() ? "True" : "False";
        else  // ordinary int
            return Strutil::sprintf("%d", *(const int*)p->data());
    } else if (p->type() == TypeDesc::FLOAT) {
        if (xmptag.special & Rational) {
            unsigned int num, den;
            float_to_rational(*(const float*)p->data(), num, den);
            return Strutil::sprintf("%d/%d", num, den);
        } else {
            return Strutil::sprintf("%g", *(const float*)p->data());
        }
    }
    return std::string();
}



static void
gather_xmp_attribs(const ImageSpec& spec,
                   std::vector<std::pair<const XMPtag*, std::string>>& list)
{
    // Loop over all params...
    for (ParamValueList::const_iterator p = spec.extra_attribs.begin();
         p != spec.extra_attribs.end(); ++p) {
        // For this param, see if there's a table entry with a matching
        // name, where the xmp name is in the right category.
        const XMPtag* tag = xmp_tagmap_ref().find(p->name());
        if (tag) {
            if (!Strutil::iequals(p->name(), tag->oiioname))
                continue;  // Name doesn't match
            if (tag->special & Suppress) {
                break;  // Purposely suppressing
            }
            std::string s = stringize(p, *tag);
            if (s.size()) {
                list.emplace_back(tag, s);
                //std::cerr << "  " << tag->xmpname << " = " << s << "\n";
            }
        }
    }
}



enum XmpControl {
    XMP_suppress,
    XMP_nodes,
    XMP_attribs,
    XMP_SeqList,  // sequential list
    XMP_BagList,  // unordered list
    XMP_AltList   // alternate list, WTF is that?
};


// Turn an entire category of XMP items into a properly serialized
// xml fragment.
static std::string
encode_xmp_category(std::vector<std::pair<const XMPtag*, std::string>>& list,
                    const char* xmlnamespace, const char* pattern,
                    const char* exclude_pattern, const char* nodename,
                    const char* url, bool minimal, XmpControl control)
{
    std::string category = std::string(xmlnamespace) + ':';
    std::string xmp;
    std::string xmp_minimal;

#if DEBUG_XMP_WRITE
    std::cerr << "Category " << xmlnamespace << ", pattern '" << pattern
              << "'\n";
#endif
    // Loop over all params...
    bool found = false;
    for (size_t li = 0; li < list.size(); ++li) {
        // For this param, see if there's a table entry with a matching
        // name, where the xmp name is in the right category.
        const XMPtag* tag = list[li].first;
        const std::string& val(list[li].second);
        const char* xmpname(tag->xmpname);
        if (control == XMP_attribs && (tag->special & (IsList | IsSeq)))
            continue;  // Skip lists for attrib output
        if (exclude_pattern && exclude_pattern[0]
            && Strutil::istarts_with(xmpname, exclude_pattern)) {
            continue;
        }
        if (Strutil::istarts_with(xmpname, pattern)) {
            std::string x;
            if (control == XMP_attribs)
                x = Strutil::sprintf("%s=\"%s\"", xmpname, val);
            else if (control == XMP_AltList || control == XMP_BagList) {
                std::vector<std::string> vals;
                Strutil::split(val, vals, ";");
                for (auto& val : vals) {
                    val = Strutil::strip(val);
                    x += Strutil::sprintf("<rdf:li>%s</rdf:li>", val);
                }
            } else
                x = Strutil::sprintf("<%s>%s</%s>", xmpname, val, xmpname);
            if (!x.empty() && control != XMP_suppress) {
                if (!found) {
                    // if (nodename && nodename[0]) {
                    //    x = Strutil::sprintf("<%s ", nodename);
                    // }
                }
                if (minimal
                    && (tag->special & (TiffRedundant | ExifRedundant))) {
                    if (xmp_minimal.size())
                        xmp_minimal += ' ';
                    xmp_minimal += x;
                } else {
                    if (xmp.size())
                        xmp += ' ';
                    xmp += x;
                }
                found = true;
#if DEBUG_XMP_WRITE
                std::cerr << "  going to output '" << x << "'\n";
#endif
            }
#if DEBUG_XMP_WRITE
            else
                std::cerr << "  NOT going to output '" << x << "'\n";
#endif
            list.erase(list.begin() + li);
            --li;
        }
    }

    if (xmp.length() && xmp_minimal.length())
        xmp += ' ' + xmp_minimal;

#if 1
    if (xmp.length()) {
        if (control == XMP_BagList)
            xmp = Strutil::sprintf("<%s><rdf:Bag> %s </rdf:Bag></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
        else if (control == XMP_SeqList)
            xmp = Strutil::sprintf("<%s><rdf:Seq> %s </rdf:Seq></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
        else if (control == XMP_AltList)
            xmp = Strutil::sprintf("<%s><rdf:Alt> %s </rdf:Alt></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
#    if 0
        else if (control == XMP_nodes)
            xmp = Strutil::sprintf("<%s>%s</%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
#    endif

        std::string r;
        r += Strutil::sprintf("<rdf:Description rdf:about=\"\" "
                              "xmlns:%s=\"%s\"%s",
                              xmlnamespace, url,
                              (control == XMP_attribs) ? " " : ">");
        r += xmp;
        if (control == XMP_attribs)
            r += "/> ";  // end the <rdf:Description...
        else
            r += " </rdf:Description>";
        return r;
    }
#endif

#if DEBUG_XMP_WRITE
    std::cerr << "  Nothing to output\n";
#endif
    return std::string();
}



std::string
encode_xmp(const ImageSpec& spec, bool minimal)
{
    std::vector<std::pair<const XMPtag*, std::string>> list;
    gather_xmp_attribs(spec, list);

    std::string xmp;

#if 1
    // This stuff seems to work
    xmp += encode_xmp_category(list, "photoshop", "photoshop:", NULL, NULL,
                               "http://ns.adobe.com/photoshop/1.0/", minimal,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "xmp", "xmp:Rating", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/", minimal,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "xmp", "xmp:CreateDate", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/", false,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "xmp", "xmp:ModifyDate", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/", false,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "xmp", "xmp:MetadataDate", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/", false,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "xmpRights", "xmpRights:UsageTerms", NULL,
                               "xmpRights:UsageTerms",
                               "http://ns.adobe.com/xap/1.0/rights/", minimal,
                               XMP_AltList);
    xmp += encode_xmp_category(list, "xmpRights", "xmpRights:", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/rights/", minimal,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "dc", "dc:subject", NULL, "dc:subject",
                               "http://purl.org/dc/elements/1.1/", minimal,
                               XMP_BagList);
    xmp += encode_xmp_category(list, "Iptc4xmpCore", "Iptc4xmpCore:SubjectCode",
                               NULL, "Iptc4xmpCore:SubjectCode",
                               "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                               false, XMP_BagList);
    xmp += encode_xmp_category(list, "Iptc4xmpCore",
                               "Iptc4xmpCore:", "Iptc4xmpCore:Ci", NULL,
                               "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                               minimal, XMP_attribs);
    xmp += encode_xmp_category(list, "Iptc4xmpCore", "Iptc4xmpCore:Ci", NULL,
                               "Iptc4xmpCore:CreatorContactInfo",
                               "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                               minimal, XMP_attribs);
    xmp += encode_xmp_category(list, "Iptc4xmpCore", "Iptc4xmpCore:Scene", NULL,
                               "Iptc4xmpCore:Scene",
                               "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                               minimal, XMP_BagList);

    xmp += encode_xmp_category(list, "xmpMM", "xmpMM:", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/mm/", minimal,
                               XMP_attribs);
    xmp += encode_xmp_category(list, "GPano", "GPano:", NULL, NULL,
                               "http://ns.google.com/photos/1.0/panorama/",
                               minimal, XMP_attribs);
    xmp += encode_xmp_category(list, "crs", "crs:", NULL, NULL,
                               "http://ns.adobe.com/camera-raw-settings/1.0/",
                               minimal, XMP_attribs);
#endif

    xmp += encode_xmp_category(list, "xmp", "xmp:", NULL, NULL,
                               "http://ns.adobe.com/xap/1.0/", minimal,
                               XMP_nodes);

    xmp += encode_xmp_category(list, "tiff", "tiff:", NULL, NULL,
                               "http://ns.adobe.com/tiff/1.0/", minimal,
                               XMP_attribs);
#if 0
    // Doesn't work yet
    xmp += encode_xmp_category (list, "xapRights", "xapRights:", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/rights/", minimal, XMP_attribs);
//    xmp += encode_xmp_category (list, "dc", "dc:", NULL, NULL,
//                                "http://purl.org/dc/elements/1.1/", minimal, XMP_attribs);

#endif

    // FIXME exif xmp stRef stVer stJob xmpDM

    if (!xmp.empty()) {
        std::string head(
            "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?> "
            "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Adobe XMP Core 5.5-c002 1.148022, 2012/07/15-18:06:45        \"> <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"> ");
        std::string foot(" </rdf:RDF> </x:xmpmeta> <?xpacket end=\"w\"?>");
        xmp = head + xmp + foot;
    }


#if DEBUG_XMP_WRITE
    std::cerr << "xmp to write = \n---\n" << xmp << "\n---\n";
    std::cerr << "\n\nHere's what I still haven't output:\n";
    for (size_t i = 0; i < list.size(); ++i)
        std::cerr << list[i].first->xmpname << "\n";
#endif

    return xmp;
}


OIIO_NAMESPACE_END
