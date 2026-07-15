// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Pure, config-free primitives for color-space search by characterization:
// the per-term grammar parse and the three-valued (match / known-different /
// unknown) axis combination. These are the crown of the search that can be
// exercised without a live OCIO config; the hint resolution, candidate
// probing, and the bounded-exhaustive walk that call them live with the
// config in color_ocio.cpp.
//
// The three-valued table this encodes (per term):
//
//   term mode        property = match   = known-different   = unknown
//   include (plain)  select             --                  miss
//   ~ inverse        --                 select              reject
//   - exclude        reject             --                  preserve
//
// and the axis rules: an empty axis accepts everything; include ∪ inverse
// select; an exclusion-only axis starts from the full universe; exclusion
// always wins last.

#include "imageio_pvt.h"

#include <OpenImageIO/strutil.h>

#include <algorithm>
#include <stdexcept>

OIIO_NAMESPACE_BEGIN

namespace pvt {

std::pair<SearchTermMode, std::string>
parse_search_term(string_view raw)
{
    if (raw.empty())
        return { SearchTermMode::include, {} };

    SearchTermMode mode = SearchTermMode::include;
    if (raw.front() == '-') {
        mode = SearchTermMode::exclude;
        raw.remove_prefix(1);
    } else if (raw.front() == '~') {
        mode = SearchTermMode::inverse;
        raw.remove_prefix(1);
    }
    if (raw.empty())
        throw std::invalid_argument(
            "color-space search hint may not be a bare operator");

    std::string value(raw);
    // A backslash escapes exactly one operator character ('-', '~', '\'),
    // which then becomes part of the name. Everything else is invalid.
    if (value.front() == '\\') {
        if (value.size() == 1)
            throw std::invalid_argument(
                "color-space search hint has a dangling escape: "
                + std::string(raw));
        if (value[1] == '-' || value[1] == '~' || value[1] == '\\')
            value.erase(value.begin());
        else
            throw std::invalid_argument(
                "color-space search hint has an invalid escape: "
                + std::string(raw));
    }
    return { mode, std::move(value) };
}


bool
three_valued_axis(cspan<SearchTermMode> modes, cspan<unsigned char> term_matches,
                  bool property_known)
{
    OIIO_DASSERT(modes.size() == term_matches.size());
    if (modes.empty())
        return true;  // an empty axis is unconstrained

    // An exclusion-only axis starts from the full candidate universe; any
    // include/inverse selector flips the start to "must be positively
    // selected".
    const bool has_selector
        = std::any_of(modes.begin(), modes.end(), [](SearchTermMode m) {
              return m != SearchTermMode::exclude;
          });
    bool selected = !has_selector;
    for (size_t i = 0, e = modes.size(); i < e; ++i) {
        const bool matches = term_matches[i] != 0;
        if (modes[i] == SearchTermMode::include && matches)
            selected = true;
        else if (modes[i] == SearchTermMode::inverse && property_known
                 && !matches)
            selected = true;  // inverse selects only *known* differences
    }
    // Exclusion always wins last -- a proven match on any exclude term drops
    // the candidate, but an unknown property is preserved (never matches).
    for (size_t i = 0, e = modes.size(); i < e; ++i)
        if (modes[i] == SearchTermMode::exclude && term_matches[i] != 0)
            return false;
    return selected;
}

}  // namespace pvt

OIIO_NAMESPACE_END
