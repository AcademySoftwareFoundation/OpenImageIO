/*
  Copyright 2008-2009 Larry Gritz and the other authors and contributors.
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

#include "fits_pvt.h"
#include <OpenImageIO/strutil.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace fits_pvt {



std::string num2str (float val)
{
    std::stringstream out ;
    out << val;
    std::string result (20 - out.str().size(), ' ');
    result += out.str ();
    return result;
}



std::string create_card (std::string keyname, std::string value)
{
    Strutil::to_upper (keyname);

    if (keyname.substr (0, 7) == "COMMENT" || keyname.substr (0, 7) == "HISTORY")
        keyname = keyname.substr (0, 7) + " ";
    else if (keyname.substr (0, 8) == "HIERARCH")
        keyname = "HIERARCH";
    else {
        // other keynames are separated from values by "= "
        keyname.resize (8, ' ');
        keyname += "= ";
    }

    std::string card = keyname;
    // boolean values are placed on byte 30 of the card
    // (20 of the value field)
    if (value.size () == 1) {
        std::string tmp (19, ' ');
        value = tmp + value;
    }
    card += value;
    card.resize (80, ' ');
    return card;
}



void
unpack_card (const std::string &card, std::string &keyname, std::string &value)
{
    keyname.clear ();
    value.clear ();

    // extracting keyname - first 8 bytes of the keyword (always)
    // we strip spaces that are placed after keyword name
    keyname = Strutil::strip (card.substr(0,8));

    // the value starts at 10 byte of the card if "=" is present at 8 byte
    // or at 8 byte otherwise
    int start = 10;
    if (card[8] != '=')
        start = 8;
    // copy of the card with keyword name stripped (only value and comment)
    std::string card_cpy = card.substr (start, card.size ());
    card_cpy = Strutil::strip (card_cpy);

    // retrieving value and get rid of the comment
    size_t begin = 0, end = std::string::npos;
    if (card_cpy[0] == '\'') {
        begin = 1;
        end = card_cpy.find ("'", 1);
    } else {
        end = card_cpy.find ("/", 1);
    }

    // after creating substring we strip NULL chars from the end
    // without this some strings are broken: see HISTORY keywords
    // in ftt4b/file003.fits test image for example
    value = Strutil::strip (card_cpy.substr (begin, end-begin).c_str());
}

} // namespace fits_pvt

OIIO_PLUGIN_NAMESPACE_END

