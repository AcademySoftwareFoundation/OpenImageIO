// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// The built-in interop identities config (the compiled-in registry of
// CIF-published interop identities) and the process-global registry
// fingerprint index built from it. Split out of color_ocio.cpp; see
// color_ocio_pvt.h for the shared internal declarations.

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/strutil.h>

#include "color_ocio_pvt.h"

#include "interop_identities_config.h"



// The built-in interop identities config and the interoperability
// assertion/bootstrap machinery below touch ColorConfig::Impl, which lives in
// the ABI-versioned v3_1 namespace -- so they must too. The OIIO_API pvt
// shims that expose them are declared (by color_pvt.h) in the library's
// "current" namespace and are defined further down in a separate
// OIIO_NAMESPACE_BEGIN block; those reach back here with explicit v3_1::
// qualification.
OIIO_NAMESPACE_3_1_BEGIN

// Return OIIO's built-in interop identities config: a config that defines
// color spaces for the CIF-published interop identities OIIO knows how to
// reliably recognize and relate in other OCIO configs. Built once per
// process and reused for the life of the process.
//
// With a linked OCIO that predates native interop ID support, this is the
// small config OIIO ships compiled in (see interop_identities_config.h),
// parsed as-is. With OCIO >= 2.5 -- which ships builtin studio configs that
// already carry the CIF interop identities natively (every color space has
// getInteropID() set) -- it is OCIO's latest builtin studio config with only
// the identities that config doesn't already provide layered on top of a
// mutable copy.
OCIO::ConstConfigRcPtr
build_interop_identities_config()
{
    // Build once and reuse for all ColorConfig instances -- function-local
    // static initialization is thread-safe (C++11 magic statics), so no
    // extra mutex is needed here.
    static OCIO::ConstConfigRcPtr s_interop_identities_config =
        []() -> OCIO::ConstConfigRcPtr {
        try {
            std::istringstream iss(kInteropIdentitiesConfig);
            OCIO::ConstConfigRcPtr embedded = OCIO::Config::CreateFromStream(
                iss);
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
            // Start from OCIO's own latest builtin studio config and layer in
            // only the embedded identities it doesn't already carry. Each
            // embedded entry's color space name equals its interop_id by
            // construction, so the prefix and lookup below key on the name:
            //   - skip "ocio:"-namespaced identities: the studio config
            //     defines the OCIO namespace itself;
            //   - add "oiio:"-namespaced identities: OIIO-only additions the
            //     studio config never carries;
            //   - add bare CIF identities only when the studio config doesn't
            //     already resolve the name, so its own (superior) definition
            //     always wins where present.
            OCIO::ConfigRcPtr config = OCIO::Config::CreateFromBuiltinConfig(
                                           "ocio://studio-config-latest")
                                           ->createEditableCopy();
            for (int i = 0, n = embedded->getNumColorSpaces(); i < n; ++i) {
                const char* name = embedded->getColorSpaceNameByIndex(i);
                if (Strutil::starts_with(name, "ocio:"))
                    continue;
                if (config->getColorSpace(name))
                    continue;
                config->addColorSpace(embedded->getColorSpace(name));
            }
            // For now the overlay adds the few bare CIF identities this build's
            // OCIO >= 2.5 studio config turns out not to carry (mostly display
            // identities); most bare identities are already present as
            // aliases. Reconfirm which the studio config carries before OCIO
            // >= 2.5 becomes OIIO's minimum, when the embedded config can
            // shrink to just the "oiio:" delta.
            //
            // The embedded registry's cinema encoding tags override the
            // studio config's, even where the studio definition supplies the
            // (superior) transforms: sdr-cinema (gamma-2.6 theatrical family,
            // 48 cd/m² calibration white) and hdr-cinema (PQ cinema masters)
            // are OIIO-side classifications that OCIO's builtin config tags
            // plain sdr-video / hdr-video.
            for (int i = 0, n = embedded->getNumColorSpaces(); i < n; ++i) {
                const char* name = embedded->getColorSpaceNameByIndex(i);
                auto ecs         = embedded->getColorSpace(name);
                const char* enc  = ecs ? ecs->getEncoding() : nullptr;
                if (!enc
                    || (!Strutil::iequals(enc, "sdr-cinema")
                        && !Strutil::iequals(enc, "hdr-cinema")))
                    continue;
                auto existing = config->getColorSpace(name);
                if (!existing
                    || Strutil::iequals(existing->getEncoding()
                                            ? existing->getEncoding()
                                            : "",
                                        enc))
                    continue;
                auto retagged = existing->createEditableCopy();
                retagged->setEncoding(enc);
                config->addColorSpace(retagged);  // replaces by name
            }
            return config;
#else
            return embedded;
#endif
        } catch (OCIO::Exception&) {
            return {};
        }
    }();
    return s_interop_identities_config;
}



std::string
interop_registry_data_version()
{
    // Line-scan the embedded registry YAML for its config-level `name:`
    // header (the first `name:` line in the file), which carries the
    // registry DATA version -- no config build, independent of the linked
    // OCIO version (whose >= 2.5 composite reports a different name).
    string_view yaml(kInteropIdentitiesConfig);
    for (string_view line : Strutil::splitsv(yaml, "\n")) {
        line = Strutil::strip(line);
        if (Strutil::parse_prefix(line, "name:"))
            return std::string(Strutil::strip(line));
    }
    return "(unknown)";
}


//////////////////////////////////////////////////////////////////////////
//
// Registry fingerprint index: the one genuinely new primitive the read-side
// registry-equivalence tier (and the write-side derivation that shares this
// file) needs. It fingerprints the built-in interop identities config's own
// simple color spaces so a query config's spaces can be matched against them by
// value. Everything else it uses -- the registry config, the interopified probe
// copy, the probe protocol, the fingerprint compute and match -- is reused from
// the foundation above.

// Build (once, lazily) and return the process-global registry fingerprint
// index. Nothing here runs until the first resolve() query reaches the
// registry-equivalence tier; ColorConfig construction never touches it. The
// index is immutable for the life of the process -- the registry config is a
// process-global constant, so entries are content-addressed and never
// invalidated or evicted. The C++11 magic-static guard makes the one-time build
// thread-safe (same idiom as build_interop_identities_config()). The write-side
// derivation fingerprints the same registry the same way and reuses this exact
// builder.
const RegistryFingerprintIndex&
registry_fingerprint_index()
{
    static const RegistryFingerprintIndex s_index =
        []() -> RegistryFingerprintIndex {
        RegistryFingerprintIndex idx;
        OCIO::ConstConfigRcPtr registry = build_interop_identities_config();
        if (!registry)
            return idx;
        idx.config = interopify_config(registry);
        if (!idx.config)
            return idx;
        try {
            OCIO::ConstContextRcPtr context = idx.config->getCurrentContext();
            ProbeValues probes = initialize_probe_values(idx.config, context);
            for (const auto& name : get_simple_color_spaces(idx.config)) {
                auto cs = idx.config->getColorSpace(name.c_str());
                auto fp = compute_fingerprint(idx.config, cs, context, probes);
                if (fp)
                    idx.entries.emplace_back(name, std::move(*fp));
            }
        } catch (...) {
            idx.entries.clear();
        }
        std::sort(idx.entries.begin(), idx.entries.end(),
                  [](const auto& a, const auto& b) {
                      return a.first < b.first;
                  });
        return idx;
    }();
    return s_index;
}

// Map a query interop id to its registry entry's fingerprint. Resolves the id
// against the registry by name or alias (utility tokens name no registry space
// and so miss here) to the canonical registry space, then finds that space's
// fingerprint in the sorted index. On a hit, `canonical_id_out` receives the
// registry space's own interop id (its interop_id attribute on OCIO >= 2.5,
// else its name) for the cheap direct-id compare on the query side. Returns
// null when the id resolves to no registry space, or to one with no fingerprint
// (e.g. a non-simple registry space).
const OIIO::pvt::ColorSpaceFingerprint*
registry_fingerprint_for_id(const RegistryFingerprintIndex& index,
                            string_view id, std::string& canonical_id_out)
{
    canonical_id_out.clear();
    if (!index.config || id.empty())
        return nullptr;
    OCIO::ConstColorSpaceRcPtr cs;
    try {
        cs = index.config->getColorSpace(std::string(id).c_str());
    } catch (...) {
        return nullptr;
    }
    if (!cs)
        return nullptr;
    const std::string name = cs->getName();
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    if (const char* iid = cs->getInteropID(); iid && *iid)
        canonical_id_out = iid;
#endif
    if (canonical_id_out.empty())
        canonical_id_out = name;
    auto it = std::lower_bound(index.entries.begin(), index.entries.end(), name,
                               [](const auto& e, const std::string& key) {
                                   return e.first < key;
                               });
    if (it != index.entries.end() && it->first == name)
        return &it->second;
    return nullptr;
}

// Reverse of registry_fingerprint_for_id (the write-side direction): given a
// query space's fingerprint, return the built-in registry identity whose
// fingerprint matches it within tolerance, walking the index's sorted
// deterministic order so first-match is stable. The returned view is the
// registry identity's own interop id -- its interop_id attribute (OCIO >= 2.5,
// where the registry is the studio config whose space names, e.g. "ACEScg",
// differ from the CIF ids, e.g. "lin_ap1_scene"), else its name (the embedded
// config, where name == interop_id). This mirrors registry_fingerprint_for_id's
// canonicalization. Both strings are owned by the process-global registry
// config/index, so the view is stable for the life of the process. Empty when
// nothing matches.
string_view
registry_id_for_fingerprint(const RegistryFingerprintIndex& index,
                            const OIIO::pvt::ColorSpaceFingerprint& query_fp)
{
    for (const auto& entry : index.entries) {
        if (!fingerprints_match(query_fp, entry.second))
            continue;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
        if (index.config) {
            try {
                if (auto cs = index.config->getColorSpace(entry.first.c_str()))
                    if (const char* iid = cs->getInteropID(); iid && *iid)
                        return iid;
            } catch (...) {
            }
        }
#endif
        return entry.first;
    }
    return {};
}



OIIO_NAMESPACE_END



// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


int
interop_identities_config_size()
{
    auto config = v3_1::build_interop_identities_config();
    return config ? config->getNumColorSpaces() : 0;
}

bool
interop_identities_config_resolves(string_view interop_id)
{
    auto config = v3_1::build_interop_identities_config();
    if (!config || interop_id.empty())
        return false;
    try {
        // getColorSpace resolves by color space name or alias, which is how
        // every CIF identity in this config is reachable (see the config's
        // per-entry name/alias scheme).
        return bool(config->getColorSpace(std::string(interop_id).c_str()));
    } catch (OCIO::Exception&) {
        return false;
    }
}

std::vector<std::string>
interop_identities_config_names()
{
    std::vector<std::string> names;
    auto config = v3_1::build_interop_identities_config();
    if (!config)
        return names;
    int n = config->getNumColorSpaces();
    names.reserve(n);
    for (int i = 0; i < n; ++i)
        names.emplace_back(config->getColorSpaceNameByIndex(i));
    return names;
}


std::vector<std::string>
embedded_interop_identities_ids()
{
    // Line-scan the embedded registry YAML for `interop_id: <token>`: the
    // canonical CIID set, independent of the linked OCIO version (the
    // parsed composite config's declared names diverge from the canonical
    // id set with OCIO >= 2.5's studio-config overlay).
    std::set<std::string> ids;
    string_view yaml(kInteropIdentitiesConfig);  // array is NUL-terminated
    for (string_view line : Strutil::splitsv(yaml, "\n")) {
        line = Strutil::strip(line);
        if (Strutil::parse_prefix(line, "interop_id:"))
            ids.emplace(Strutil::strip(line));
    }
    return { ids.begin(), ids.end() };
}

}  // namespace pvt

OIIO_NAMESPACE_END
