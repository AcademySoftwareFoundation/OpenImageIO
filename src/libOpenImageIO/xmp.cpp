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
    Supress = 16,         // Explicitly supress it from XMP
    IsList = 32           // Make a semicolon-separated list out of it
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
    { "photoshop:SupplementalCategories", "IPTC:SupplementalCategories", TypeDesc::STRING, 0 },
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
    { "xap:CreatorTool", "Software", TypeDesc::STRING, TiffRedundant },
    { "xmp:CreatorTool", "Software", TypeDesc::STRING, TiffRedundant },
    { "xap:CreateDate", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
    { "xmp:CreateDate", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
//    { "xap:ModifyDate", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
//    { "xmp:ModifyDate", "DateTime", TypeDesc::STRING, DateConversion|TiffRedundant },
    { "dc:format", "", TypeDesc::STRING, TiffRedundant|Supress },
    { "dc:Description", "ImageDescription", TypeDesc::STRING, TiffRedundant },
    { "dc:Creator", "Artist", TypeDesc::STRING, TiffRedundant },
    { "dc:Rights", "Copyright", TypeDesc::STRING, TiffRedundant },
    { "dc:title", "IPTC:ObjectName", TypeDesc::STRING, 0 },
    { "dc:subject", "Keywords", TypeDesc::STRING, IsList },
    { "Iptc4xmpCore:IntellectualGenre", "IPTC:IntellectualGenre", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CreatorContactInfo", "IPTC:CreatorContactInfo", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:ContactInfoDetails", "IPTC:Contact", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrExtadr", "IPTC:ContactInfoAddress", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrCity", "IPTC:ContactInfoCity", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAddrRegion", "IPTC:ContactInfoState", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiAdrCtry", "IPTC:ContactInfoCountry", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiPcode", "IPTC:ContactInfoPostalCode", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiEmailWork", "IPTC:ContactInfoEmail", TypeDesc::STRING, 0 },

    { "Iptc4xmpCore:CiTelWork", "IPTC:ContactInfoPhone", TypeDesc::STRING, 0 },
    { "Iptc4xmpCore:CiUrlWork", "IPTC:ContactInfoURL", TypeDesc::STRING, 0 },

#if 0
    // Not handled: photoshop:SupplementalCategories
#endif
    { "rdf:li", "" },  // ignore these strays
    { NULL, NULL }
};



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
        while (t.size() && t[0] == ' ')
            t.erase (t.begin());
        if (t.size())
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



// Utility: add an attribute to the spec with the given xml name and
// value.  Search for it in xmptag, and if found that will tell us what
// the type is supposed to be, as well as any special handling.  If not
// found in the table, add it as a string and hope for the best.
static void
add_attrib (ImageSpec &spec, const char *xmlname, const char *xmlvalue)
{
    for (int i = 0;  xmptag[i].xmpname;  ++i) {
        if (Strutil::iequals (xmptag[i].xmpname, xmlname)) {
            if (! xmptag[i].oiioname || ! xmptag[i].oiioname[0])
                return;   // ignore it purposefully
            if (xmptag[i].oiiotype == TypeDesc::STRING) {
                std::string val;
                if (xmptag[i].special & IsList) {
                    // Special case -- append it to a list
                    std::vector<std::string> items;
                    ImageIOParameter *p = spec.find_attribute (xmptag[i].oiioname, TypeDesc::STRING); 
                    bool dup = false;
                    if (p) {
                        split_list (*(const char **)p->data(), items);
                        for (size_t i = 0;  i < items.size();  ++i)
                            dup |= (items[i] == xmlvalue);
                        dup |= (xmlvalue == std::string(*(const char **)p->data()));
                    }
                    if (! dup)
                        items.push_back (xmlvalue);
                    val = join_list (items);
                } else {
                    val = xmlvalue;
                }
                spec.attribute (xmptag[i].oiioname, val);
                return;
            } else if (xmptag[i].oiiotype == TypeDesc::INT) {
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
#if (defined(DEBUG) || DEBUG_XMP_READ)
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


};   // anonymous namespace




bool
decode_xmp (const std::string &xml, ImageSpec &spec)
{
#if DEBUG_XMP_READ
    std::cerr << "XMP dump:\n---\n" << xml << "\n---\n";
#endif

    // FIXME: we should replace this awkward regex matching with actual
    // XML parsing with pugixml.

    try {
    // Instead of doing a true parse of the XML, we can get away with
    // some simple pattern matching.
    boost::regex xml_item_pattern ("<(\\w+:\\w+)>(.*)</(\\1)>", boost::regex::perl);
    boost::regex xml_nested_pattern ("<(\\w+:\\w+)>.*<(\\w+:\\w+).*>.*</\\2>.*</(\\1)>", boost::regex::perl);

    // Search in turn for all "<rdf:Description ... </rdf:Description>" blocks.
    for (size_t startpos = 0, endpos = 0;
         extract_middle (xml, endpos, "<rdf:Description", "</rdf:Description>", startpos, endpos);  ) {
        std::string rdf (xml, startpos+1, endpos-startpos-2);  // scooch in
        // We have an rdf block, actually we scooched in by one char in each
        // direction so that we don't accidentally match it looking for
        // items inside it.
        while (1) {
            // Search for a simple "<ATTRIB...>VALUE</ATTRIB>" block
            // within the RDF.
            boost::match_results<std::string::const_iterator> what;
            if (boost::regex_search (rdf, what, xml_item_pattern)) {
                std::string attrib = what[1], value = what[2];
                // OK, there are two cases.  It may be a simple value, or
                // it may be nested XML items inside the value, which 
                // happens for list items.
                boost::match_results<std::string::const_iterator> nestwhat;
                if (boost::regex_search (value, nestwhat, xml_nested_pattern) &&
                      (nestwhat[1] == "rdf:Seq" || nestwhat[1] == "rdf:Alt") &&
                      nestwhat[2] == "rdf:li") {
                    // It's a list.  Look at each "<rdf:li...</rdf:li>"
                    std::string list = nestwhat[0];
                    std::string v;
                    for (size_t s = 0, e = 0;
                         extract_middle (list, e, "<rdf:li", "</rdf:li>", s, e); ) {
                        std::string item (list, s, e-s);
                        boost::match_results<std::string::const_iterator> w;
                        if (boost::regex_search (item, w, xml_item_pattern)) {
                            // OK, we're down to a single list item.
                            if (v.length() && w.length())
                                v += "; ";
                            v += w[2];
                        }
                    }
                    if (v.size())
                        add_attrib (spec, attrib.c_str(), v.c_str());
                } else {
                    // Not a list -- just a straight-up attribute, add to spec
                    add_attrib (spec, attrib.c_str(), value.c_str());
                }
                rdf = what.suffix();
            } else {
                // std::cerr << "NO MATCH ->" << rdf << "<-\n";
                break;
            }
        }
    }
    } /* end of try */
    catch (const std::exception &e) {
#ifdef DEBUG
        std::cerr << "ERROR! '" << e.what() << "'\n";
#endif
        return false;
    }

    return true;
}



// Convert from a single semicolon-separated string into an XMP list.
static std::string
encode_xmp_list_item (const std::string &val)
{
    bool anyfound = false;
    std::string xml = "<rdf:Seq>";
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(";");
    tokenizer tokens (val, sep);
    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end(); ++tok_iter) {
        std::string t = *tok_iter;
        while (t.size() && t[0] == ' ')
            t.erase (t.begin());
        if (t.size()) {
            xml += Strutil::format ("<rdf:li>%s</rdf:li>", t.c_str());
            anyfound = true;
        }
    }
    xml += "</rdf:Seq>";
    return anyfound ? xml : std::string();
}



// Turn one ImageIOParameter (whose xmp info we know) into a properly
// serialized xmp string.
static std::string
encode_xmp_oneitem (const ImageIOParameterList::const_iterator &p,
                    const XMPtag &xmptag)
{
    if (p->type() == TypeDesc::STRING) {
        if (xmptag.special & DateConversion) {
            // FIXME -- convert to yyyy-mm-ddThh:mm:ss.sTZD
            return std::string();
        }
        if (xmptag.special & IsList)
            return Strutil::format ("  <%s>%s</%s>\n", xmptag.xmpname, 
                                    encode_xmp_list_item (*(const char **)p->data()).c_str(),
                                    xmptag.xmpname);
        else 
            return Strutil::format ("  <%s>%s</%s>\n", xmptag.xmpname, 
                                    *(const char **)p->data(), xmptag.xmpname);
    } else if (p->type() == TypeDesc::INT) {
        return Strutil::format ("  <%s>%d</%s>\n", xmptag.xmpname, 
                                *(const int *)p->data(), xmptag.xmpname);
    } else if (p->type() == TypeDesc::FLOAT) {
        if (xmptag.special & Rational) {
            unsigned int num, den;
            float_to_rational (*(const float *)p->data(), num, den);
            return Strutil::format ("  <%s>%d/%d</%s>\n", xmptag.xmpname, 
                                    num, den, xmptag.xmpname);
        } else {
            return Strutil::format ("  <%s>%g</%s>\n", xmptag.xmpname,
                                    *(const float *)p->data(), xmptag.xmpname);
        }
    }
    return std::string();
}



// Turn an entire category of XMP items into a properly serialized 
// xml fragment.
static std::string
encode_xmp_category (const ImageSpec &spec, const char *xmlnamespace,
                     const char *url, bool minimal=false)
{
    std::string category = std::string(xmlnamespace) + ':';
    std::string xmp;
    std::string xmp_minimal;
    // Loop over all params...
    for (ImageIOParameterList::const_iterator p = spec.extra_attribs.begin();
         p != spec.extra_attribs.end();  ++p) {
        // For this param, see if there's a table entry with a matching
        // name, where the xmp name is in the right category.
        bool found = false;
        for (int i = 0;  xmptag[i].xmpname;  ++i) {
            if (! Strutil::iequals (p->name().c_str(), xmptag[i].oiioname))
                continue;   // Name doesn't match
            if (strncmp (xmptag[i].xmpname, category.c_str(), category.length()))
                continue;   // Category doesn't match
            if (xmptag[i].special & Supress) {
                found = true;
                break;   // Purposely supressing
            }

            std::string x = encode_xmp_oneitem (p, xmptag[i]);
            if (! x.empty()) {
                found = true;
                if (minimal && (xmptag[i].special & (TiffRedundant|ExifRedundant))) {
                    xmp_minimal += x;
                } else {
                    xmp += x;
                }
            } else {
#if (defined(DEBUG) || DEBUG_XMP_WRITE)
                std::cerr << "encode_xmp_category: not sure about " << p->type().c_str() << " " << xmptag[i].oiioname << "\n";
#endif
            }
        }
        if (! found) {
            // We have an attrib that wasn't in the table.  But if it is
            // prefixed by the category name, go ahead and output it anyway.
            if (! strncmp (p->name().c_str(), category.c_str(), category.length())) {
                XMPtag dummytag;
                dummytag.xmpname = p->name().c_str();
                dummytag.oiioname = p->name().c_str();
                dummytag.oiiotype = p->type();
                dummytag.special = NothingSpecial;
                xmp += encode_xmp_oneitem (p, dummytag);
            }
        }
    }

    if (xmp.length())
        xmp += xmp_minimal;

    if (xmp.length()) {
        xmp = Strutil::format (" <rdf:Description rdf:about=\"\" "
                               "xmlns:%s=\"%s\">\n", xmlnamespace, url)
            + xmp
            + " </rdf:Description>\n\n";
    }

    return xmp;
}



std::string 
encode_xmp (const ImageSpec &spec, bool minimal)
{
    std::string head ("<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?> \n"
                      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"> \n"
                      "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n\n");
    std::string foot ("</rdf:RDF>\n"
                      "</x:xmpmeta>\n"
                      "<?xpacket end=\"w\"?>");

    std::string xmp;
    xmp += encode_xmp_category (spec, "photoshop",
                                "http://ns.adobe.com/photoshop/1.0/", minimal);
    xmp += encode_xmp_category (spec, "tiff",
                                "http://ns.adobe.com/tiff/1.0/", minimal);
    xmp += encode_xmp_category (spec, "xap",
                                "http://ns.adobe.com/xap/1.0/", minimal);
    xmp += encode_xmp_category (spec, "xapRights",
                                "http://ns.adobe.com/xap/1.0/rights/", minimal);
    xmp += encode_xmp_category (spec, "xapMM",
                                "http://ns.adobe.com/xap/1.0/mm/", minimal);
    xmp += encode_xmp_category (spec, "dc",
                                "http://purl.org/dc/elements/1.1/", minimal);
    xmp += encode_xmp_category (spec, "Iptc4xmpCore",
                                "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                                minimal);
// FIXME exif xmp stRef stVer stJob xmpDM 

    if (! xmp.empty())
        xmp = head + xmp + foot;

#if DEBUG_XMP_WRITE
    std::cerr << "xmp = \n---\n" << xmp << "\n---\n";
#endif

    return xmp;
}


}
OIIO_NAMESPACE_EXIT

