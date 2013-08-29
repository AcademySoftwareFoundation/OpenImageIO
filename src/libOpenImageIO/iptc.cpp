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

#include <boost/tokenizer.hpp>

#include "imageio.h"

#define DEBUG_IPTC_READ  0
#define DEBUG_IPTC_WRITE 0


OIIO_NAMESPACE_ENTER
{

namespace {

struct IIMtag {
    int tag;                  // IIM code
    const char *name;         // Attribute name we use
    const char *anothername;  // Optional second name
    bool repeatable;          // May repeat
};

static IIMtag iimtag [] = {
    {   3, "IPTC:ObjectTypeReference", NULL, false },
    {   4, "IPTC:ObjectAttributeReference", NULL, true },
    {   5, "IPTC:ObjectName", NULL, false },
    {   7, "IPTC:EditStatus", NULL, false },
    {  10, "IPTC:Urgency", NULL, false },  // deprecated by IPTC
    {  12, "IPTC:SubjectReference", NULL, true },
    {  15, "IPTC:Category", NULL, false },
    {  20, "IPTC:SupplementalCategories", NULL, true }, // deprecated by IPTC
    {  22, "IPTC:FixtureIdentifier", NULL, false },
    {  25, "Keywords", NULL, true },
    {  26, "IPTC:ContentLocationCode", NULL, true },
    {  27, "IPTC:ContentLocationName", NULL, true },
    {  30, "IPTC:ReleaseDate", NULL, false },
    {  35, "IPTC:ReleaseTime", NULL, false },
    {  37, "IPTC:ExpirationDate", NULL, false },
    {  38, "IPTC:ExpirationTime", NULL, false },
    {  40, "IPTC:Instructions", NULL, false },
    {  45, "IPTC:ReferenceService", NULL, true },
    {  47, "IPTC:ReferenceDate", NULL, false },
    {  50, "IPTC:ReferenceNumber", NULL, true },
    {  55, "IPTC:DateCreated", NULL, false },
    {  60, "IPTC:TimeCreated", NULL, false },
    {  62, "IPTC:DigitalCreationDate", NULL, false },
    {  63, "IPTC:DigitalCreationTime", NULL, false },
    {  65, "IPTC:OriginatingProgram", "Software", false },
    {  70, "IPTC:ProgramVersion", NULL, false },
    {  80, "IPTC:Creator", "Artist", true },  // sometimes called "byline"
    {  85, "IPTC:AuthorsPosition", NULL, true }, // sometimes "byline title"
    {  90, "IPTC:City", NULL, false },
    {  92, "IPTC:Sublocation", NULL, false },
    {  95, "IPTC:State", NULL, false },  // sometimes "Province/State"
    { 100, "IPTC:CountryCode", NULL, false },
    { 101, "IPTC:Country", NULL, false },
    { 103, "IPTC:TransmissionReference", NULL, false },
    { 105, "IPTC:Headline", NULL, false },
    { 110, "IPTC:Provider", NULL, false }, // aka Credit
    { 115, "IPTC:Source", NULL, false },
    { 116, "IPTC:CopyrightNotice", "Copyright", false },
    { 118, "IPTC:Contact", NULL, false },
    { 120, "IPTC:Caption", "ImageDescription", false},
    { 121, "IPTC:LocalCaption", NULL, false},
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

}   // anonymous namespace



bool
decode_iptc_iim (const void *iptc, int length, ImageSpec &spec)
{
    const unsigned char *buf = (const unsigned char *) iptc;

#if DEBUG_IPTC_READ
    std::cerr << "IPTC dump:\n";
    for (int i = 0;  i < 100;  ++i) {
        if (buf[i] >= ' ')
            std::cerr << (char)buf[i] << ' ';
        std::cerr << "(" << (int)(unsigned char)buf[i] << ") ";
    }
    std::cerr << "\n";
#endif

    // Now there are a series of data blocks.  Each one starts with 1C
    // 02, then a single byte indicating the tag type, then 2 byte (big
    // endian) giving the tag length, then the data itself.  This
    // repeats until we've used up the whole segment buffer, or I guess
    // until we don't find another 1C 02 tag start.  
    // N.B. I don't know why, but Picasa sometimes uses 1C 01 !
    while (length > 0 && buf[0] == 0x1c && (buf[1] == 0x02 || buf[1] == 0x01)) {
        int secondbyte = buf[1];
        int tagtype = buf[2];
        int tagsize = (buf[3] << 8) + buf[4];
        buf += 5;
        length -= 5;

#if DEBUG_IPTC_READ
        std::cerr << "iptc tag " << tagtype << ":\n";
        for (int i = 0;  i < tagsize;  ++i) {
            if (buf[i] >= ' ')
                std::cerr << (char)buf[i] << ' ';
            std::cerr << "(" << (int)(unsigned char)buf[i] << ") ";
        }
        std::cerr << "\n";
#endif

        if (secondbyte == 0x02) {
            std::string s ((const char *)buf, tagsize);

            for (int i = 0;  iimtag[i].name;  ++i) {
                if (tagtype == iimtag[i].tag) {
                    if (iimtag[i].repeatable) {
                        // For repeatable IIM tags, concatenate them
                        // together separated by semicolons
                        s = Strutil::strip (s);
                        std::string old = spec.get_string_attribute (iimtag[i].name);
                        if (old.size())
                            old += "; ";
                        spec.attribute (iimtag[i].name, old+s);
                    } else {
                        spec.attribute (iimtag[i].name, s);
                    }
                    if (iimtag[i].anothername)
                        spec.attribute (iimtag[i].anothername, s);
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
encode_iptc_iim_one_tag (int tag, const char *name, TypeDesc type,
                         const void *data, std::vector<char> &iptc)
{
    if (type == TypeDesc::STRING) {
        iptc.push_back ((char)0x1c);
        iptc.push_back ((char)0x02);
        iptc.push_back ((char)tag);
        const char *str = ((const char **)data)[0];
        int tagsize = strlen(str);
        tagsize = std::min (tagsize, 0xffff - 1); // Prevent 16 bit overflow
        iptc.push_back ((char)(tagsize >> 8));
        iptc.push_back ((char)(tagsize & 0xff));
        iptc.insert (iptc.end(), str, str+tagsize);
    }
}



void
encode_iptc_iim (const ImageSpec &spec, std::vector<char> &iptc)
{
    iptc.clear ();
    
    const ImageIOParameter *p;
    for (int i = 0;  iimtag[i].name;  ++i) {
        if ((p = spec.find_attribute (iimtag[i].name))) {
            if (iimtag[i].repeatable) {
                std::string allvals (*(const char **)p->data());
                std::vector<std::string> tokens;
                Strutil::split (allvals, tokens, ";");
                for (size_t t = 0, e = tokens.size();  t < e;  ++t) {
                    tokens[t] = Strutil::strip (tokens[t]);
                    if (tokens[t].size()) {
                        const char *tok = &tokens[t][0];
                        encode_iptc_iim_one_tag (iimtag[i].tag, iimtag[i].name,
                                                 p->type(), &tok, iptc);
                    }
                }
            } else {
                // Regular, non-repeating
                encode_iptc_iim_one_tag (iimtag[i].tag, iimtag[i].name,
                                         p->type(), p->data(), iptc);
            }
        }
        if (iimtag[i].anothername) {
            if ((p = spec.find_attribute (iimtag[i].anothername)))
                encode_iptc_iim_one_tag (iimtag[i].tag, iimtag[i].anothername,
                                         p->type(), p->data(), iptc);
        }
    }
}


}
OIIO_NAMESPACE_EXIT
