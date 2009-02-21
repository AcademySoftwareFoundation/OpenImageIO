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
using namespace OpenImageIO;

#define DEBUG_IPTC_READ  0
#define DEBUG_IPTC_WRITE 0


namespace OpenImageIO {


namespace {

struct IIMtag {
    int tag;                  // IIM code
    const char *name;         // Attribute name we use
    const char *anothername;  // Optional second name
};

static IIMtag iimtag [] = {
    {   5, "IPTC:ObjectName", NULL },
    {  15, "IPTC:Category", NULL },
//    {  25, "Keywords", NULL },
    {  40, "IPTC:Instructions", NULL },
    {  65, "IPTC:OriginatingProgram", "Software" },
    {  80, "IPTC:Creator", "Artist" },   // N.B. in theory, repeatable
    {  85, "IPTC:AuthorsPosition", NULL },  // N.B. in theory, repeatable
    {  90, "IPTC:City", NULL },
    {  92, "IPTC:Sublocation", NULL },
    {  95, "IPTC:State", NULL },
    { 100, "IPTC:CountryCode", NULL },
    { 101, "IPTC:Country", NULL },
    { 103, "IPTC:TransmissionReference", NULL },
    { 105, "IPTC:Headline", NULL },
    { 110, "IPTC:Provider", NULL }, // aka Credit
    { 115, "IPTC:Source", NULL },
    { 116, "IPTC:CopyrightNotice", "Copyright" },
    { 118, "IPTC:Contact", NULL },
    { 120, "IPTC:Caption", "ImageDescription"},
    { 122, "IPTC:CaptionWriter", NULL },  // should it be called Writer?
    { -1, NULL, NULL }
};

// FIXME? others:
// 20 SupplementalCategories (repeatable) [ deprecated by IPTC ]
// 30 ReleaseDate
// 35 ReleaseTime
// 37 ExpirationDate
// 38 ExpirationTime
// 45 ReferenceService
// 47 ReferenceDate
// 50 ReferenceNumber
// 55 DateCreated (CCYYMMDD, 00 for unknown parts)
// 60 TimeCreated [11 digs]
// 62 DigitalCreationDate [8 digs]
// 63 DigitalCreationTime [11 digs]
// 70 ProgramVersion
// 121 LocalCaption
// 150-154 audio stuff
// 184 JobID
// 185 MasterDocumentID
// 186 ShortDocumentID
// 187 UniqueDocumentID
// 188 OwnerID
// 221 Prefs
// 225 ClassifyState
// 228 SimilarityIndex
// 230 DocumentNotes
// 231 DocumentHistory

};   // anonymous namespace



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

    std::string keywords;

    // Now there are a series of data blocks.  Each one starts with 1C
    // 02, then a single byte indicating the tag type, then 2 byte (big
    // endian) giving the tag length, then the data itself.  This
    // repeats until we've used up the whole segment buffer, or I guess
    // until we don't find another 1C 02 tag start.  
    // N.B. I don't know why, but Picasa sometimes uses 1C 01 !
    while (length > 0 && buf[0] == 0x1c && (buf[1] == 0x02 || buf[1] == 0x01)) {
        int firstbyte = buf[0], secondbyte = buf[1];
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
                    spec.attribute (iimtag[i].name, s);
                    if (iimtag[i].anothername)
                        spec.attribute (iimtag[i].anothername, s);
                }
            }

            // Special case for keywords
            if (tagtype == 25) {
                if (keywords.length())
                    keywords.append (std::string("; "));
                keywords.append (s);
            }
        }

        buf += tagsize;
        length -= tagsize;
    }

    if (keywords.length())
        spec.attribute ("Keywords", keywords);
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
        if (p = spec.find_attribute (iimtag[i].name))
            encode_iptc_iim_one_tag (iimtag[i].tag, iimtag[i].name,
                                     p->type(), p->data(), iptc);
        if (iimtag[i].anothername) {
            if (p = spec.find_attribute (iimtag[i].anothername))
                encode_iptc_iim_one_tag (iimtag[i].tag, iimtag[i].anothername,
                                         p->type(), p->data(), iptc);
        }
    }

    // Special case: Keywords
    if (p = spec.find_attribute ("Keywords", TypeDesc::STRING)) {
        std::string allkeywords (*(const char **)p->data());
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
        boost::char_separator<char> sep(";");
        tokenizer tokens (allkeywords, sep);
        for (tokenizer::iterator tok_iter = tokens.begin();
                 tok_iter != tokens.end(); ++tok_iter) {
            std::string t = *tok_iter;
            while (t.size() && t[0] == ' ')
                t.erase (t.begin());
            if (t.size()) {
                const char *tptr = &t[0];
                encode_iptc_iim_one_tag (25 /* tag number */, "Keywords",
                                         TypeDesc::STRING, &tptr, iptc);
            }
        }
    }
}


};  // namespace OpenImageIO

