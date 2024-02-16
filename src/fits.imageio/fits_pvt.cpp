// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "fits_pvt.h"
#include <OpenImageIO/strutil.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace fits_pvt {



std::string
num2str(float val)
{
    std::stringstream out;
    out << val;
    std::string result(20 - out.str().size(), ' ');
    result += out.str();
    return result;
}



std::string
create_card(std::string keyname, std::string value)
{
    Strutil::to_upper(keyname);

    if (keyname.substr(0, 7) == "COMMENT" || keyname.substr(0, 7) == "HISTORY")
        keyname = keyname.substr(0, 7) + " ";
    else if (keyname.substr(0, 8) == "HIERARCH")
        keyname = "HIERARCH";
    else {
        // other keynames are separated from values by "= "
        keyname.resize(8, ' ');
        keyname += "= ";
    }

    std::string card = keyname;
    // boolean values are placed on byte 30 of the card
    // (20 of the value field)
    if (value.size() == 1) {
        std::string tmp(19, ' ');
        value = tmp + value;
    }
    card += value;
    card.resize(80, ' ');
    return card;
}



void
unpack_card(const std::string& card, std::string& keyname, std::string& value)
{
    keyname.clear();
    value.clear();

    // extracting keyname - first 8 bytes of the keyword (always)
    // we strip spaces that are placed after keyword name
    keyname = Strutil::strip(card.substr(0, 8));

    // the value starts at 10 byte of the card if "=" is present at 8 byte
    // or at 8 byte otherwise
    int start = 10;
    if (card[8] != '=')
        start = 8;
    // copy of the card with keyword name stripped (only value and comment)
    std::string card_cpy = card.substr(start, card.size());
    card_cpy             = Strutil::strip(card_cpy);

    // retrieving value and get rid of the comment
    size_t begin = 0, end = std::string::npos;
    if (card_cpy[0] == '\'') {
        begin = 1;
        end   = card_cpy.find("'", 1);
    } else {
        end = card_cpy.find("/", 1);
    }

    // after creating substring we strip NULL chars from the end
    // without this some strings are broken: see HISTORY keywords
    // in ftt4b/file003.fits test image for example
    value = Strutil::strip(card_cpy.substr(begin, end - begin).c_str());
}

}  // namespace fits_pvt

OIIO_PLUGIN_NAMESPACE_END
