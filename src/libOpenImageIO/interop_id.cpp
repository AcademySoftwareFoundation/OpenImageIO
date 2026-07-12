// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// The ID grammar and sanitization rules from the CIF recommendation
// "An ID for Color Interop", Annex B (interop ID syntax) and Annex C
// (sanitizing name strings for interop ID usage):
// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki
//
// Pure, stateless functions with no OCIO dependency -- kept in their own
// translation unit so color_ocio.cpp doesn't have to grow to hold them.

#include "imageio_pvt.h"

#include <array>

OIIO_NAMESPACE_BEGIN

namespace pvt {

namespace {

// Annex B id-char set: lowercase a-z, 0-9, and the punctuation below.
// Built once at compile time into a 128-entry table keyed by ASCII byte,
// so the per-character check is a single indexed load (no hashing, no
// first-call guard needed since the table is constexpr-built). Shared by
// both sanitize_id_token's "already legal" fast path and
// parse_interop_id's token-validity check (which must NOT lowercase or
// map -- just accept/reject).
constexpr std::array<bool, 128>
make_allowed_id_char_table()
{
    std::array<bool, 128> table {};
    for (char c = 'a'; c <= 'z'; ++c)
        table[static_cast<unsigned char>(c)] = true;
    for (char c = '0'; c <= '9'; ++c)
        table[static_cast<unsigned char>(c)] = true;
    for (char c :
         { '.', '-', '_', '~', '/', '*', '#', '%', '^', '+', '(', ')', '[',
           ']', '|' })
        table[static_cast<unsigned char>(c)] = true;
    return table;
}

bool
is_allowed_id_char(char c)
{
    static constexpr std::array<bool, 128> kAllowed
        = make_allowed_id_char_table();
    const auto byte = static_cast<unsigned char>(c);
    return byte < 0x80u && kAllowed[byte];
}

// True if every byte of `s` is ASCII and allowed per is_allowed_id_char.
// Empty strings are NOT valid tokens (grammar requires 1*id-char).
bool
is_valid_token(const std::string& s)
{
    if (s.empty())
        return false;
    for (unsigned char c : s) {
        if (c >= 0x80 || !is_allowed_id_char(static_cast<char>(c)))
            return false;
    }
    return true;
}

// Number of bytes (including the lead byte) in the UTF-8 sequence that
// starts with `lead`, per the standard lead-byte bit patterns. Returns 1
// for ASCII or for a stray/invalid lead byte (defensive fallback).
int
utf8_sequence_length(unsigned char lead)
{
    if ((lead & 0xE0u) == 0xC0u)
        return 2;
    if ((lead & 0xF0u) == 0xE0u)
        return 3;
    if ((lead & 0xF8u) == 0xF0u)
        return 4;
    return 1;
}

bool
is_utf8_continuation(unsigned char b)
{
    return (b & 0xC0u) == 0x80u;
}

}  // namespace



std::string
sanitize_id_token(const std::string& token)
{
    std::string result;
    result.reserve(token.size());

    std::size_t i    = 0;
    const std::size_t n = token.size();
    while (i < n) {
        const unsigned char byte = static_cast<unsigned char>(token[i]);

        if (byte < 0x80u) {
            const char c = static_cast<char>(byte);
            switch (c) {
            case ' ':
            case '\t':
            case '\n':
            case '\r': result.push_back('_'); break;
            case '{':
            case '<': result.push_back('('); break;
            case '}':
            case '>': result.push_back(')'); break;
            case ',': result.push_back('.'); break;
            case ';':
            case ':': result.push_back('|'); break;
            case '\'':
            case '"': result.push_back('#'); break;
            case '\\': result.push_back('/'); break;
            default:
                if (is_allowed_id_char(c))
                    result.push_back(c);
                else if (c >= 'A' && c <= 'Z')
                    result.push_back(static_cast<char>(c - 'A' + 'a'));
                else
                    result.push_back('*');
                break;
            }
            ++i;
            continue;
        }

        // Non-ASCII: collapse the whole UTF-8 code point to a single '^',
        // no matter how many bytes it takes.
        int seqLen           = utf8_sequence_length(byte);
        std::size_t consumed = 1;
        for (int k = 1; k < seqLen; ++k) {
            if (i + static_cast<std::size_t>(k) >= n
                || !is_utf8_continuation(static_cast<unsigned char>(
                    token[i + static_cast<std::size_t>(k)])))
                break;
            consumed = static_cast<std::size_t>(k) + 1;
        }
        result.push_back('^');
        i += consumed;
    }

    return result;
}



InteropIdParts
parse_interop_id(const std::string& id)
{
    InteropIdParts parts;

    if (id.empty())
        return parts;  // InteropIdForm::INVALID

    // Locate up to the first two colons.
    const std::size_t firstColon = id.find(':');
    if (firstColon == std::string::npos) {
        // 0 colons: the whole string is the base candidate.
        if (is_valid_token(id)) {
            parts.form = InteropIdForm::BASE;
            parts.base = id;
        }
        return parts;
    }

    const std::size_t secondColon = id.find(':', firstColon + 1);
    if (secondColon == std::string::npos) {
        // 1 colon: A:B
        std::string a = id.substr(0, firstColon);
        std::string b = id.substr(firstColon + 1);
        if (is_valid_token(a) && is_valid_token(b)) {
            parts.form  = InteropIdForm::INNER_BASE;
            parts.inner = a;
            parts.base  = b;
        }
        return parts;
    }

    // 3+ colons is unconditionally invalid.
    if (id.find(':', secondColon + 1) != std::string::npos)
        return parts;

    // 2 colons: A:B:C
    std::string a = id.substr(0, firstColon);
    std::string b = id.substr(firstColon + 1, secondColon - firstColon - 1);
    std::string c = id.substr(secondColon + 1);

    if (!is_valid_token(a) || !is_valid_token(c))
        return parts;

    if (b.empty()) {
        parts.form  = InteropIdForm::OUTER_BLANK_BASE;
        parts.outer = a;
        parts.base  = c;
        return parts;
    }

    if (is_valid_token(b)) {
        parts.form  = InteropIdForm::OUTER_INNER_BASE;
        parts.outer = a;
        parts.inner = b;
        parts.base  = c;
        return parts;
    }

    return parts;  // inner present but contains disallowed characters
}



bool
is_valid_interop_id(const std::string& id)
{
    return parse_interop_id(id).form != InteropIdForm::INVALID;
}



std::string
strip_leftmost_namespace(const std::string& id)
{
    const std::size_t firstColon = id.find(':');
    if (firstColon == std::string::npos)
        return id;
    return id.substr(firstColon + 1);
}



bool
is_utility_interop_id(const std::string& id)
{
    return id == "data" || id == "unknown" || id == "bypass";
}

}  // namespace pvt

OIIO_NAMESPACE_END
