// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Curve-family normalization for the transfer-function ("curve") named
// transforms an interop identities config may declare. Such a curve name
// carries a suffix naming its negative-axis policy: `_tx` for the
// pass-through variant, and a bare (suffixless) name for the mirror variant
// that shares the identical positive-axis curve. Older configs may still use
// the legacy `_scene` / `_display` spellings; both normalize to the same
// family. These helpers reduce a curve name to a family token/name so two
// spaces sharing a transfer curve compare equal regardless of the
// reference-space state their name encodes.
//
// Pure, stateless string functions with no OCIO dependency -- kept in their
// own translation unit so color_ocio.cpp doesn't have to grow to hold them.

#include "color_pvt.h"
#include "imageio_pvt.h"

#include <OpenImageIO/strutil.h>

OIIO_NAMESPACE_BEGIN

namespace pvt {

namespace {

    // State suffixes a curve name may carry, tried in order; at most one is
    // stripped. No suffix is a suffix of another, so the order is immaterial in
    // practice -- but the strip-once semantics are deliberate: a name like
    // `crv_g24_tx_display` (none exist) would strip only the trailing suffix.
    constexpr const char* kCurveStateSuffixes[] = { "_scene", "_display",
                                                    "_tx" };

    // True if `name` ends with `suffix` and is strictly longer than it, so a name
    // that *is* a bare suffix ("_tx") passes through untouched.
    inline bool has_curve_suffix(string_view name, string_view suffix)
    {
        return name.size() > suffix.size() && Strutil::ends_with(name, suffix);
    }

    // Strip at most one trailing state suffix (first match wins).
    string_view strip_curve_suffix(string_view name)
    {
        for (string_view suffix : kCurveStateSuffixes) {
            if (has_curve_suffix(name, suffix)) {
                name.remove_suffix(suffix.size());
                break;
            }
        }
        return name;
    }

}  // namespace


std::string
family_token(string_view name)
{
    if (Strutil::starts_with(name, "crv_"))
        name.remove_prefix(4);
    return std::string(strip_curve_suffix(name));
}


std::string
family_name(string_view name)
{
    return std::string(strip_curve_suffix(name));
}


bool
curve_is_passthrough(string_view name)
{
    return has_curve_suffix(name, "_scene") || has_curve_suffix(name, "_tx");
}


bool
curve_is_mirror(string_view name, cspan<std::string> catalog_names)
{
    if (has_curve_suffix(name, "_display"))
        return true;
    // v10 companion lookup: a suffixless mirror is identified by the presence
    // of its `_tx` pass-through twin in the same catalog. Self-adapts to
    // whichever config generation built the catalog.
    // Linear scan -- a hot matcher loop should pre-build a set of catalog
    // names, but no such caller exists yet.
    const std::string companion = std::string(name) + "_tx";
    for (const std::string& n : catalog_names)
        if (n == companion)
            return true;
    return false;
}

}  // namespace pvt

OIIO_NAMESPACE_END
