// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <iostream>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#define DEBUG_IPTC_READ 0
#define DEBUG_IPTC_WRITE 0


OIIO_NAMESPACE_BEGIN

namespace {

struct IIMtag {
    int tag;                  // IIM code
    const char* name;         // Attribute name we use
    const char* anothername;  // Optional second name
    bool repeatable;          // May repeat
};

static IIMtag iimtag[] = {
    { 3, "IPTC:ObjectTypeReference", NULL, false },
    { 4, "IPTC:ObjectAttributeReference", NULL, true },
    { 5, "IPTC:ObjectName", NULL, false },
    { 7, "IPTC:EditStatus", NULL, false },
    { 10, "IPTC:Urgency", NULL, false },  // deprecated by IPTC
    { 12, "IPTC:SubjectReference", NULL, true },
    { 15, "IPTC:Category", NULL, false },
    { 20, "IPTC:SupplementalCategories", NULL, true },  // deprecated by IPTC
    { 22, "IPTC:FixtureIdentifier", NULL, false },
    { 25, "Keywords", NULL, true },
    { 26, "IPTC:ContentLocationCode", NULL, true },
    { 27, "IPTC:ContentLocationName", NULL, true },
    { 30, "IPTC:ReleaseDate", NULL, false },
    { 35, "IPTC:ReleaseTime", NULL, false },
    { 37, "IPTC:ExpirationDate", NULL, false },
    { 38, "IPTC:ExpirationTime", NULL, false },
    { 40, "IPTC:Instructions", NULL, false },
    { 45, "IPTC:ReferenceService", NULL, true },
    { 47, "IPTC:ReferenceDate", NULL, false },
    { 50, "IPTC:ReferenceNumber", NULL, true },
    { 55, "IPTC:DateCreated", NULL, false },
    { 60, "IPTC:TimeCreated", NULL, false },
    { 62, "IPTC:DigitalCreationDate", NULL, false },
    { 63, "IPTC:DigitalCreationTime", NULL, false },
    { 65, "IPTC:OriginatingProgram", "Software", false },
    { 70, "IPTC:ProgramVersion", NULL, false },
    { 80, "IPTC:Creator", "Artist", true },      // sometimes called "byline"
    { 85, "IPTC:AuthorsPosition", NULL, true },  // sometimes "byline title"
    { 90, "IPTC:City", NULL, false },
    { 92, "IPTC:Sublocation", NULL, false },
    { 95, "IPTC:State", NULL, false },  // sometimes "Province/State"
    { 100, "IPTC:CountryCode", NULL, false },
    { 101, "IPTC:Country", NULL, false },
    { 103, "IPTC:TransmissionReference", NULL, false },
    { 105, "IPTC:Headline", NULL, false },
    { 110, "IPTC:Provider", NULL, false },  // aka Credit
    { 115, "IPTC:Source", NULL, false },
    { 116, "IPTC:CopyrightNotice", "Copyright", false },
    { 118, "IPTC:Contact", NULL, false },
    { 120, "IPTC:Caption", "ImageDescription", false },
    { 121, "IPTC:LocalCaption", NULL, false },
    { 122, "IPTC:CaptionWriter", NULL, false },  // aka Writer/Editor
    // Note: 150-154 is audio sampling stuff
    { 184, "IPTC:JobID", NULL, false },
    { 185, "IPTC:MasterDocumentID", NULL, false },
    { 186, "IPTC:ShortDocumentID", NULL, false },
    { 187, "IPTC:UniqueDocumentID", NULL, false },
    { 188, "IPTC:OwnerID", NULL, false },
    { 221, "IPTC:Prefs", NULL, false },
    { 225, "IPTC:ClassifyState", NULL, false },
    { 228, "IPTC:SimilarityIndex", NULL, false },
    { 230, "IPTC:DocumentNotes", NULL, false },
    { 231, "IPTC:DocumentHistory", NULL, false },
    { -1, NULL, NULL, false }
};

// N.B. All "Date" fields are 8 digit strings: CCYYMMDD
// All "Time" fields are 11 digit strings (what format?)

}  // anonymous namespace



bool
decode_iptc_iim(const void* iptc, int length, ImageSpec& spec)
{
    const unsigned char* buf = (const unsigned char*)iptc;

#if DEBUG_IPTC_READ
    std::cerr << "IPTC dump:\n";
    for (int i = 0; i < std::min(length, 100); ++i) {
        if (buf[i] >= ' ' && buf[i] < 128)
            std::cerr << (char)buf[i] << ' ';
        std::cerr << "(" << int(buf[i]) << ") ";
    }
    std::cerr << "\n";
#endif

    // Now there are a series of data blocks.  Each one starts with 1C
    // 02, then a single byte indicating the tag type, then 2 byte (big
    // endian) giving the tag length, then the data itself.  This
    // repeats until we've used up the whole segment buffer, or I guess
    // until we don't find another 1C 02 tag start.
    // N.B. I don't know why, but Picasa sometimes uses 1C 01 !
    while (length >= 5 && buf[0] == 0x1c
           && (buf[1] == 0x02 || buf[1] == 0x01)) {
        int secondbyte = buf[1];
        int tagtype    = buf[2];
        int tagsize    = (buf[3] << 8) + buf[4];
        buf += 5;
        length -= 5;
        tagsize = std::min(tagsize, length);

#if DEBUG_IPTC_READ
        std::cerr << "iptc tag " << tagtype << ", size=" << tagsize << ":\n";
        for (int i = 0; i < tagsize; ++i) {
            if (buf[i] >= ' ')
                std::cerr << (char)buf[i] << ' ';
            std::cerr << "(" << (int)(unsigned char)buf[i] << ") ";
        }
        std::cerr << "\n";
#endif

        if (secondbyte == 0x02) {
            std::string s((const char*)buf, tagsize);

            for (int i = 0; iimtag[i].name; ++i) {
                if (tagtype == iimtag[i].tag) {
                    if (iimtag[i].repeatable) {
                        // For repeatable IIM tags, concatenate them
                        // together separated by semicolons
                        s               = Strutil::strip(s);
                        std::string old = spec.get_string_attribute(
                            iimtag[i].name);
                        if (old.size())
                            old += "; ";
                        spec.attribute(iimtag[i].name, old + s);
                    } else {
                        spec.attribute(iimtag[i].name, s);
                    }
                    if (iimtag[i].anothername
                        && !spec.extra_attribs.contains(iimtag[i].anothername))
                        spec.attribute(iimtag[i].anothername, s);
                    break;
                }
            }
        }

        buf += tagsize;
        length -= tagsize;
    }

    return true;
}



static void
encode_iptc_iim_one_tag(int tag, string_view data, std::vector<char>& iptc)
{
    OIIO_DASSERT(data != nullptr);
    iptc.push_back((char)0x1c);
    iptc.push_back((char)0x02);
    iptc.push_back((char)tag);
    if (data.size()) {
        int tagsize = std::min(int(data.size()),
                               0xffff - 1);  // Prevent 16 bit overflow
        iptc.push_back((char)(tagsize >> 8));
        iptc.push_back((char)(tagsize & 0xff));
        iptc.insert(iptc.end(), data.data(), data.data() + tagsize);
    }
}



void
encode_iptc_iim(const ImageSpec& spec, std::vector<char>& iptc)
{
    iptc.clear();

    const ParamValue* p;
    for (int i = 0; iimtag[i].name; ++i) {
        if ((p = spec.find_attribute(iimtag[i].name))) {
            if (iimtag[i].repeatable) {
                std::string allvals = p->get_string(0);
                std::vector<std::string> tokens;
                Strutil::split(allvals, tokens, ";");
                for (auto& token : tokens) {
                    token = Strutil::strip(token);
                    if (token.size())
                        encode_iptc_iim_one_tag(iimtag[i].tag, token, iptc);
                }
            } else {
                // Regular, non-repeating
                encode_iptc_iim_one_tag(iimtag[i].tag, p->get_string(0), iptc);
            }
        }
        if (iimtag[i].anothername) {
            if ((p = spec.find_attribute(iimtag[i].anothername)))
                encode_iptc_iim_one_tag(iimtag[i].tag, p->get_string(0), iptc);
        }
    }
}


OIIO_NAMESPACE_END
