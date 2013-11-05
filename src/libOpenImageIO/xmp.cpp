/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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


#include <iostream>

#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

#include "thread.h"
#include "strutil.h"
#include "fmath.h"
#include "imageio.h"
#include <pugixml.hpp>

#define DEBUG_XMP_READ  0
#define DEBUG_XMP_WRITE 0

#define MY_ENCODING "ISO-8859-1"

OIIO_NAMESPACE_ENTER
{

namespace {  // anonymous


// Define special processing flags -- they're individual bits so can be
// combined with '|'
enum XMPspecial {
    NothingSpecial = 0,
    Rational = 1,         // It needs to be expressed as A/B
    DateConversion = 2,   // It's a date, may need conversion to canonical form
    TiffRedundant = 4,    // It's something that's part of normal TIFF tags
    ExifRedundant = 8,    // It's something included in Exif
    Suppress = 16,        // Explicitly suppress it from XMP
    IsList = 32,          // Make a semicolon-separated list out of it
    IsSeq = 64,           // Like List, but order matters
    IsBool = 128          // Should be output as True/False
};

struct XMPtag {
    const char *xmpname;      // XMP name
    const char *oiioname;     // Attribute name we use
    TypeDesc oiiotype;        // Type we use
    int special;              // Special handling
};

static XMPtag xmptag [] = {
    { "photoshop:AuthorsPosition", "IPTC:AuthorsPosition", TypeDesc::STRING, 0 },
    { "photoshop:CaptionWriter", "IPTC:CaptionWriter", TypeDesc::STRING, 0 },
    { "photoshop:Category", "IPTC:Category", TypeDesc::STRING, 0 },
    { "photoshop:City", "IPTC:City", TypeDesc::STRING, 0 },
    { "photoshop:Country", "IPTC:Country", TypeDesc::STRING, 0 },
    { "photoshop:Credit", "IPTC:Provider", TypeDesc::STRING, 0 },
    { "photoshop:DateCreated", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
    { "photoshop:Headline", "IPTC:Headline", TypeDesc::STRING, 0 },
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

    { "exif:ColorSpace", "Exif:ColorSpace", TypeDesc::INT, ExifRedundant },
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

    { "rdf:li", "" },  // ignore these strays
    { NULL, NULL }
};



// Utility: add an attribute to the spec with the given xml name and
// value.  Search for it in xmptag, and if found that will tell us what
// the type is supposed to be, as well as any special handling.  If not
// found in the table, add it as a string and hope for the best.
static void
add_attrib (ImageSpec &spec, const char *xmlname, const char *xmlvalue)
{
#if DEBUG_XMP_READ
    std::cerr << "add_attrib " << xmlname << ": '" << xmlvalue << "'\n";
#endif
    for (int i = 0;  xmptag[i].xmpname;  ++i) {
        if (Strutil::iequals (xmptag[i].xmpname, xmlname)) {
            if (! xmptag[i].oiioname || ! xmptag[i].oiioname[0])
                return;   // ignore it purposefully
            if (xmptag[i].oiiotype == TypeDesc::STRING) {
                std::string val;
                if (xmptag[i].special & (IsList|IsSeq)) {
                    // Special case -- append it to a list
                    std::vector<std::string> items;
                    ImageIOParameter *p = spec.find_attribute (xmptag[i].oiioname, TypeDesc::STRING); 
                    bool dup = false;
                    if (p) {
                        Strutil::split (*(const char **)p->data(), items, ";");
                        for (size_t item = 0;  item < items.size();  ++item) {
                            items[item] = Strutil::strip (items[item]);
                            dup |= (items[item] == xmlvalue);
                        }
                        dup |= (xmlvalue == std::string(*(const char **)p->data()));
                    }
                    if (! dup)
                        items.push_back (xmlvalue);
                    val = Strutil::join (items, "; ");
                } else {
                    val = xmlvalue;
                }
                spec.attribute (xmptag[i].oiioname, val);
                return;
            } else if (xmptag[i].oiiotype == TypeDesc::INT) {
                if (xmptag[i].special & IsBool)
                    spec.attribute (xmptag[i].oiioname, (int)Strutil::iequals(xmlvalue,"true"));
                else  // ordinary int
                    spec.attribute (xmptag[i].oiioname, (int)atoi(xmlvalue));
                return;
            } else if (xmptag[i].oiiotype == TypeDesc::FLOAT) {
                float f = atoi (xmlvalue);
                const char *slash = strchr (xmlvalue, '/');
                if (slash)  // It's rational!
                    f /= (float) atoi (slash+1);
                spec.attribute (xmptag[i].oiioname, f);
                return;
            }
#if (!defined(NDEBUG) || DEBUG_XMP_READ)
            else {
                std::cerr << "iptc xml add_attrib unknown type " << xmlname 
                          << ' ' << xmptag[i].oiiotype.c_str() << "\n";
            }
#endif
            return;
        }
    }
    // Catch-all for unrecognized things -- just add them!
    spec.attribute (xmlname, xmlvalue);
}



// Utility: Search str for the first substring in str (starting from
// position pos) that starts with startmarker and ends with endmarker.
// If not found, return false.  If found, return true, store the
// beginning and ending indices in startpos and endpos.
static bool
extract_middle (const std::string &str, size_t pos, 
                const char *startmarker, const char *endmarker,
                size_t &startpos, size_t &endpos)
{
    startpos = str.find (startmarker, pos);
    if (startpos == std::string::npos)
        return false;   // start marker not found
    endpos = str.find (endmarker, startpos);
    if (endpos == std::string::npos)
        return false;   // end marker not found
    endpos += strlen (endmarker);
    return true;
}


static void
decode_xmp_node (pugi::xml_node node, ImageSpec &spec,
                 int level=1, const char *parentname=NULL, bool isList=false)
{
    std::string mylist;  // will accumulate for list items
    for ( ;  node;  node = node.next_sibling()) {
#if DEBUG_XMP_READ
        std::cerr << "Level " << level << " " << node.name() << " = " << node.value() << "\n";
#endif
        // First, encode all attributes of this node
        for (pugi::xml_attribute attr = node.first_attribute();
             attr; attr = attr.next_attribute()) {
#if DEBUG_XMP_READ
            std::cerr << "   level " << level
                      << " parent " << (parentname?parentname:"-")
                      << " attr " << attr.name() << ' ' << attr.value() << "\n";
#endif
            if (Strutil::istarts_with(attr.name(), "xml:") ||
                Strutil::istarts_with(attr.name(), "xmlns:"))
                continue;   // xml attributes aren't image metadata
            if (attr.name()[0] && attr.value()[0])
                add_attrib (spec, attr.name(), attr.value());
        }
        if (Strutil::iequals(node.name(), "xmpMM::History")) {
            // FIXME -- image history is complicated. Come back to it.
            continue;
        }
        if (Strutil::iequals(node.name(), "rdf:Bag") ||
            Strutil::iequals(node.name(), "rdf:Seq") ||
            Strutil::iequals(node.name(), "rdf:Alt") ||
            Strutil::iequals(node.name(), "rdf:li")) {
            // Various kinds of lists.  Recuse, pass the parent name
            // down, and let the child know it's part of a list.
            decode_xmp_node (node.first_child(), spec, level+1, parentname, true);
        } else {
            // Not a list, but it's got children.  Recurse.
            decode_xmp_node (node.first_child(), spec, level+1, node.name());
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
        add_attrib (spec, parentname, mylist.c_str());
    }
}


}   // anonymous namespace




bool
decode_xmp (const std::string &xml, ImageSpec &spec)
{
#if DEBUG_XMP_READ
    std::cerr << "XMP dump:\n---\n" << xml << "\n---\n";
#endif
    if (! xml.length())
        return true;
    for (size_t startpos = 0, endpos = 0;
         extract_middle (xml, endpos, "<rdf:Description", "</rdf:Description>", startpos, endpos);  ) {
        // Turn that middle section into an XML document
        std::string rdf (xml, startpos, endpos-startpos);  // scooch in
#if DEBUG_XMP_READ
        std::cerr << "RDF is:\n---\n" << rdf << "\n---\n";
#endif
        pugi::xml_document doc;
        pugi::xml_parse_result parse_result = doc.load_buffer (&rdf[0], rdf.size());
        if (! parse_result) {
#if DEBUG_XMP_READ
            std::cerr << "Error parsing XML\n";
#endif
            return true;
        }
        // Decode the contents of the XML document (it will recurse)
        decode_xmp_node (doc.first_child(), spec);
    }

    return true;
}



// Turn one ImageIOParameter (whose xmp info we know) into a properly
// serialized xmp string.
static std::string
stringize (const ImageIOParameterList::const_iterator &p,
           const XMPtag &xmptag)
{
    if (p->type() == TypeDesc::STRING) {
        if (xmptag.special & DateConversion) {
            // FIXME -- convert to yyyy-mm-ddThh:mm:ss.sTZD
            // return std::string();
        }
        return std::string(*(const char **)p->data());
    } else if (p->type() == TypeDesc::INT) {
        if (xmptag.special & IsBool)
            return *(const int *)p->data() ? "True" : "False";
        else // ordinary int
            return Strutil::format ("%d", *(const int *)p->data());
    } else if (p->type() == TypeDesc::FLOAT) {
        if (xmptag.special & Rational) {
            unsigned int num, den;
            float_to_rational (*(const float *)p->data(), num, den);
            return Strutil::format ("%d/%d", num, den);
        } else  {
            return Strutil::format ("%g", *(const float *)p->data());
        }
    }
    return std::string();
}



static void
gather_xmp_attribs (const ImageSpec &spec,
                    std::vector<std::pair<int,std::string> > &list)
{
    // Loop over all params...
    for (ImageIOParameterList::const_iterator p = spec.extra_attribs.begin();
         p != spec.extra_attribs.end();  ++p) {
        // For this param, see if there's a table entry with a matching
        // name, where the xmp name is in the right category.
        for (int i = 0;  xmptag[i].xmpname;  ++i) {
            if (! Strutil::iequals (p->name().c_str(), xmptag[i].oiioname))
                continue;   // Name doesn't match
            if (xmptag[i].special & Suppress) {
                break;   // Purposely suppressing
            }
            std::string s = stringize (p,xmptag[i]);
            if (s.size()) {
                list.push_back (std::pair<int,std::string>(i, s));
                //std::cerr << "  " << xmptag[i].xmpname << " = " << s << "\n"; 
            }
        }
    }
}



enum XmpControl { XMP_suppress, XMP_nodes, XMP_attribs,
                  XMP_SeqList, // sequential list
                  XMP_BagList, // unordered list
                  XMP_AltList  // alternate list, WTF is that?
};


// Turn an entire category of XMP items into a properly serialized 
// xml fragment.
static std::string
encode_xmp_category (std::vector<std::pair<int,std::string> > &list,
                     const char *xmlnamespace, const char *pattern,
                     const char *exclude_pattern,
                     const char *nodename, const char *url,
                     bool minimal, XmpControl control)
{
    std::string category = std::string(xmlnamespace) + ':';
    std::string xmp;
    std::string xmp_minimal;

#if DEBUG_XMP_WRITE
    std::cerr << "Category " << xmlnamespace << ", pattern '" << pattern << "'\n";
#endif
    // Loop over all params...
    bool found = false;
    for (size_t li = 0;  li < list.size();  ++li) {
        // For this param, see if there's a table entry with a matching
        // name, where the xmp name is in the right category.
        int i = list[li].first;
        const std::string &val (list[li].second);
        const char *xmpname (xmptag[i].xmpname);
        if (control == XMP_attribs && (xmptag[i].special & (IsList|IsSeq)))
            continue;   // Skip lists for attrib output
        if (exclude_pattern && exclude_pattern[0] &&
            Strutil::istarts_with (xmpname, exclude_pattern)) {
            continue;
        }
        if (Strutil::istarts_with (xmpname, pattern)) {
            std::string x;
            if (control == XMP_attribs)
                x = Strutil::format ("%s=\"%s\"", xmpname, val);
            else if (control == XMP_AltList || control == XMP_BagList) {
                std::vector<std::string> vals;
                Strutil::split (val, vals, ";");
                for (size_t i = 0;  i < vals.size();  ++i) {
                    vals[i] = Strutil::strip (vals[i]);
                    x += Strutil::format ("<rdf:li>%s</rdf:li>", vals[i]);
                }
            }
            else
                x = Strutil::format ("<%s>%s</%s>", xmpname, val, xmpname);
            if (! x.empty() && control != XMP_suppress) {
                if (! found) {
//                    if (nodename && nodename[0]) {
//                       x = Strutil::format("<%s ", nodename);
//                    }
                }
                if (minimal && (xmptag[i].special & (TiffRedundant|ExifRedundant))) {
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
            else std::cerr << "  NOT going to output '" << x << "'\n";
#endif
            list.erase (list.begin()+li);
            --li;
        }
    }

    if (xmp.length() && xmp_minimal.length())
        xmp += ' ' + xmp_minimal;

#if 1
    if (xmp.length()) {
        if (control == XMP_BagList)
            xmp = Strutil::format ("<%s><rdf:Bag> %s </rdf:Bag></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
        else if (control == XMP_SeqList)
            xmp = Strutil::format ("<%s><rdf:Seq> %s </rdf:Seq></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
        else if (control == XMP_AltList)
            xmp = Strutil::format ("<%s><rdf:Alt> %s </rdf:Alt></%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
#if 0
        else if (control == XMP_nodes)
            xmp = Strutil::format("<%s>%s</%s>",
                                   nodename ? nodename : xmlnamespace, xmp,
                                   nodename ? nodename : xmlnamespace);
 nodename);
#endif

        std::string r;
        r += Strutil::format ("<rdf:Description rdf:about=\"\" "
                              "xmlns:%s=\"%s\"%s", xmlnamespace, url,
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
encode_xmp (const ImageSpec &spec, bool minimal)
{
    std::vector<std::pair<int,std::string> > list;
    gather_xmp_attribs (spec, list);

    std::string xmp;

#if 1
    // This stuff seems to work
    xmp += encode_xmp_category (list, "photoshop", "photoshop:", NULL, NULL,
                                "http://ns.adobe.com/photoshop/1.0/", minimal, XMP_attribs);
    xmp += encode_xmp_category (list, "xmp", "xmp:Rating", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/", minimal, XMP_attribs);
    xmp += encode_xmp_category (list, "xmp", "xmp:CreateDate", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/", false, XMP_attribs);
    xmp += encode_xmp_category (list, "xmp", "xmp:ModifyDate", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/", false, XMP_attribs);
    xmp += encode_xmp_category (list, "xmp", "xmp:MetadataDate", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/", false, XMP_attribs);
    xmp += encode_xmp_category (list, "xmpRights", "xmpRights:UsageTerms", NULL, "xmpRights:UsageTerms",
                                "http://ns.adobe.com/xap/1.0/rights/", minimal, XMP_AltList);
    xmp += encode_xmp_category (list, "xmpRights", "xmpRights:", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/rights/", minimal, XMP_attribs);
    xmp += encode_xmp_category (list, "dc", "dc:subject", NULL, "dc:subject",
                                "http://purl.org/dc/elements/1.1/", minimal, XMP_BagList);
    xmp += encode_xmp_category (list, "Iptc4xmpCore", "Iptc4xmpCore:SubjectCode",
                                NULL, "Iptc4xmpCore:SubjectCode",
                                "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                                false, XMP_BagList);
    xmp += encode_xmp_category (list, "Iptc4xmpCore", "Iptc4xmpCore:",
                                "Iptc4xmpCore:Ci", NULL,
                                "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                                minimal, XMP_attribs);
    xmp += encode_xmp_category (list, "Iptc4xmpCore", "Iptc4xmpCore:Ci", NULL,
                                "Iptc4xmpCore:CreatorContactInfo",
                                "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                                minimal, XMP_attribs);
    xmp += encode_xmp_category (list, "Iptc4xmpCore", "Iptc4xmpCore:Scene", NULL,
                                "Iptc4xmpCore:Scene",
                                "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                                minimal, XMP_BagList);

    xmp += encode_xmp_category (list, "xmpMM", "xmpMM:", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/mm/", minimal, XMP_attribs);
#endif

    xmp += encode_xmp_category (list, "xmp", "xmp:", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/", minimal, XMP_nodes);

    xmp += encode_xmp_category (list, "tiff", "tiff:", NULL, NULL,
                                "http://ns.adobe.com/tiff/1.0/", minimal, XMP_attribs);
#if 0
    // Doesn't work yet
    xmp += encode_xmp_category (list, "xapRights", "xapRights:", NULL, NULL,
                                "http://ns.adobe.com/xap/1.0/rights/", minimal, XMP_attribs);
//    xmp += encode_xmp_category (list, "dc", "dc:", NULL, NULL,
//                                "http://purl.org/dc/elements/1.1/", minimal, XMP_attribs);

#endif

// FIXME exif xmp stRef stVer stJob xmpDM 

  if (! xmp.empty()) {
      std::string head (
            "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?> "
            "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Adobe XMP Core 5.5-c002 1.148022, 2012/07/15-18:06:45        \"> <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"> "
            );
        std::string foot (" </rdf:RDF> </x:xmpmeta> <?xpacket end=\"w\"?>");
        xmp = head + xmp 
            + foot;
    }


#if DEBUG_XMP_WRITE
    std::cerr << "xmp to write = \n---\n" << xmp << "\n---\n";
    std::cerr << "\n\nHere's what I still haven't output:\n";
    for (size_t i = 0; i < list.size(); ++i)
        std::cerr << xmptag[list[i].first].xmpname << "\n";
#endif

    return xmp;
}


}
OIIO_NAMESPACE_EXIT

