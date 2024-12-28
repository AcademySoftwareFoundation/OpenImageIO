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
    unsigned int maxlen;      // Maximum length (if nonzero)
};

static IIMtag iimtag[] = {
    { 3, "IPTC:ObjectTypeReference", NULL, false, 67 },
    { 4, "IPTC:ObjectAttributeReference", NULL, true, 68 },
    { 5, "IPTC:ObjectName", NULL, false, 64 },
    { 7, "IPTC:EditStatus", NULL, false, 64 },
    { 10, "IPTC:Urgency", NULL, false, 1 },  // deprecated by IPTC
    { 12, "IPTC:SubjectReference", NULL, true, 236 },
    { 15, "IPTC:Category", NULL, false, 3 },
    { 20, "IPTC:SupplementalCategories", NULL, true, 32 },  // deprecated by IPTC
    { 22, "IPTC:FixtureIdentifier", NULL, false, 32 },
    { 25, "IPTC:Keywords", NULL, true, 64 },
    { 26, "IPTC:ContentLocationCode", NULL, true, 3 },
    { 27, "IPTC:ContentLocationName", NULL, true, 64 },
    { 30, "IPTC:ReleaseDate", NULL, false, 8 },
    { 35, "IPTC:ReleaseTime", NULL, false, 11 },
    { 37, "IPTC:ExpirationDate", NULL, false, 8 },
    { 38, "IPTC:ExpirationTime", NULL, false, 11 },
    { 40, "IPTC:Instructions", NULL, false, 256 },
    { 45, "IPTC:ReferenceService", NULL, true, 10 },
    { 47, "IPTC:ReferenceDate", NULL, false, 8 },
    { 50, "IPTC:ReferenceNumber", NULL, true, 8 },
    { 55, "IPTC:DateCreated", NULL, false, 8 },
    { 60, "IPTC:TimeCreated", NULL, false, 11 },
    { 62, "IPTC:DigitalCreationDate", NULL, false, 8 },
    { 63, "IPTC:DigitalCreationTime", NULL, false, 11 },
    { 65, "IPTC:OriginatingProgram", "Software", false, 32 },
    { 70, "IPTC:ProgramVersion", NULL, false, 10 },
    { 80, "IPTC:Creator", "Artist", true, 32 },  // sometimes called "byline"
    { 85, "IPTC:AuthorsPosition", NULL, true, 32 },  // sometimes "byline title"
    { 90, "IPTC:City", NULL, false, 32 },
    { 92, "IPTC:Sublocation", NULL, false, 32 },
    { 95, "IPTC:State", NULL, false, 32 },  // sometimes "Province/State"
    { 100, "IPTC:CountryCode", NULL, false, 3 },
    { 101, "IPTC:Country", NULL, false, 64 },
    { 103, "IPTC:TransmissionReference", NULL, false, 32 },
    { 105, "IPTC:Headline", NULL, false, 256 },
    { 110, "IPTC:Provider", NULL, false, 32 },  // aka Credit
    { 115, "IPTC:Source", NULL, false, 32 },
    { 116, "IPTC:CopyrightNotice", "Copyright", false, 128 },
    { 118, "IPTC:Contact", NULL, false, 128 },
    { 120, "IPTC:Caption", "ImageDescription", false, 2000 },
    { 121, "IPTC:LocalCaption", NULL, false, 256 },
    { 122, "IPTC:CaptionWriter", NULL, false, 32 },  // aka Writer/Editor
    // Note: 150-154 is audio sampling stuff
    { 184, "IPTC:JobID", NULL, false, 64 },
    { 185, "IPTC:MasterDocumentID", NULL, false, 256 },
    { 186, "IPTC:ShortDocumentID", NULL, false, 64 },
    { 187, "IPTC:UniqueDocumentID", NULL, false, 128 },
    { 188, "IPTC:OwnerID", NULL, false, 128 },
    { 221, "IPTC:Prefs", NULL, false, 64 },
    { 225, "IPTC:ClassifyState", NULL, false, 64 },
    { 228, "IPTC:SimilarityIndex", NULL, false, 32 },
    { 230, "IPTC:DocumentNotes", NULL, false, 1024 },
    { 231, "IPTC:DocumentHistory", NULL, false, 256 },
    { -1, NULL, NULL, false, 0 }
};

// N.B. All "Date" fields are 8 digit strings: CCYYMMDD
// All "Time" fields are 11 digit strings (what format?)

// IPTC references:
//
// * https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata
// * https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata#iptc-core-schema-1-5-specifications
//   This is the one where you can find the length limits
// * ExifTool's documentation about IPTC tags (caveat: not a definitive
//   reference, could be outdated or incorrect):
//   https://exiftool.org/TagNames/IPTC.html

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
#if 0
                    // We are no longer confident about auto-translating IPTC
                    // data into allegedly equivalent metadata.
                    if (iimtag[i].anothername
                        && !spec.extra_attribs.contains(iimtag[i].anothername))
                        spec.attribute(iimtag[i].anothername, s);
#endif
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



bool
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
                    if (token.size()) {
                        if (iimtag[i].maxlen && iimtag[i].maxlen < token.size())
                            token = token.substr(0, iimtag[i].maxlen);
                        encode_iptc_iim_one_tag(iimtag[i].tag, token, iptc);
                    }
                }
            } else {
                // Regular, non-repeating
                std::string token = p->get_string(0);
                if (iimtag[i].maxlen && iimtag[i].maxlen < token.size())
                    token = token.substr(0, iimtag[i].maxlen);
                encode_iptc_iim_one_tag(iimtag[i].tag, token, iptc);
            }
        }
#if 0
        // We are no longer confident about auto-translating other metadata
        // into allegedly equivalent IPTC.
        if (iimtag[i].anothername) {
            if ((p = spec.find_attribute(iimtag[i].anothername)))
                encode_iptc_iim_one_tag(iimtag[i].tag, p->get_string(0), iptc);
        }
#endif
    }
    return iptc.size() != 0;
}


OIIO_NAMESPACE_END
