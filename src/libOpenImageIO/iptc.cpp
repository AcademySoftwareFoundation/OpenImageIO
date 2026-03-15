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

OIIO_NAMESPACE_END


OIIO_NAMESPACE_3_1_BEGIN

bool
decode_iptc_iim(const void* iptc, int length, ImageSpec& spec)
{
    if (!iptc || length <= 0)
        return false;
    return decode_iptc_iim(string_view(reinterpret_cast<const char*>(iptc),
                                       static_cast<size_t>(length)),
                           spec);
}



bool
decode_iptc_iim(string_view iptc, ImageSpec& spec)
{
#if DEBUG_IPTC_READ
    print(stderr, "IPTC dump (len={}):\n", iptc.size());
    for (size_t i = 0; i < std::min(iptc.size(), size_t(1000)); ++i) {
        if (iptc[i] >= ' ' && iptc[i] < 128)
            print(stderr, "{:c} ", iptc[i]);
        print(stderr, "({:d}) ", uint32_t(static_cast<unsigned char>(iptc[i])));
    }
    print(stderr, "\n");
#endif

    // Now there are a series of data blocks.  Each one starts with 1C
    // 02, then a single byte indicating the tag type, then 2 byte (big
    // endian) giving the tag length, then the data itself.  This
    // repeats until we've used up the whole segment buffer, or I guess
    // until we don't find another 1C 02 tag start.
    // N.B. I don't know why, but Picasa sometimes uses 1C 01 !
    while (iptc.size() >= 5 && iptc[0] == 0x1c
           && (iptc[1] == 0x02 || iptc[1] == 0x01)) {
        int secondbyte = static_cast<unsigned char>(iptc[1]);
        int tagtype    = static_cast<unsigned char>(iptc[2]);
        size_t tagsize = (static_cast<unsigned char>(iptc[3]) << 8)
                         + static_cast<unsigned char>(iptc[4]);
        iptc.remove_prefix(5);
        tagsize = std::min(tagsize, iptc.size());

#if DEBUG_IPTC_READ
        print(stderr, "iptc tag {}, size={}:\n", tagtype, tagsize);
        for (size_t i = 0; i < tagsize; ++i) {
            if (iptc[i] >= ' ')
                print(stderr, "{:c} ", iptc[i]);
            print(stderr, "({:d}) ", static_cast<unsigned char>(iptc[i]));
        }
        print(stderr, "\n");
#endif

        if (secondbyte == 0x02) {
            std::string s = iptc.substr(0, tagsize);
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

        iptc.remove_prefix(tagsize);
    }

    return true;
}



static void
encode_iptc_iim_one_tag(int tag, string_view data, std::vector<char>& iptc)
{
    if (data.size() == 0)
        return;
    data = data.substr(0, 0xffff);  // Truncate to prevent 16 bit overflow
    size_t tagsize = data.size();
    iptc.push_back((char)0x1c);
    iptc.push_back((char)0x02);
    iptc.push_back((char)tag);
    iptc.push_back((char)(tagsize >> 8));
    iptc.push_back((char)(tagsize & 0xff));
    OIIO_PRAGMA_WARNING_PUSH
    OIIO_GCC_ONLY_PRAGMA(GCC diagnostic ignored "-Wstringop-overflow")
    // Suppress what I'm sure is a false positive warning when
    // _GLIBCXX_ASSERTIONS is enabled.
    iptc.insert(iptc.end(), data.begin(), data.end());
    OIIO_PRAGMA_WARNING_POP
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
                for (auto token : tokens) {
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


OIIO_NAMESPACE_3_1_END
